# bgfx DX8 Removal Plan

## Current Status

The standalone bgfx build no longer links the real Direct3D 8 or D3DX8 runtime.
With `GGC_RENDER_BACKEND=bgfx` and `GGC_BGFX_STANDALONE=ON`, CMake strips the
real `d3d8`/`d3dx8` link libraries from `d3d8lib` and compiles the in-tree
compatibility files:

- `Core/Libraries/Source/WWVegas/WW3D2/StubD3D8Device.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/D3DXStandaloneStubs.cpp`

That is enough for macOS/Metal to run without a real Direct3D device. It is not
the same as removing DX8 from the bgfx build. The bgfx renderer still depends on
the legacy DX8-shaped state and resource model.

Recent progress on the DX8-removal stack:

- `BgfxBackend` now inherits `IRenderBackend` directly instead of `DX8Backend`.
- `BgfxBackend.cpp` no longer forwards through `DX8Backend` or peeks
  `DX8Wrapper` state directly.
- The bgfx CMake target no longer compiles `DX8Backend.cpp` or
  `dx8webbrowser.cpp`.
- Static vertex/index buffers and texture uploads are sourced from CPU-side
  snapshots instead of bgfx locking D3D mirror resources directly.
- `RenderStateStruct`, the current fixed-function render-state snapshot, and
  the dirty/change mask now live in `FixedFunctionState` instead of
  `DX8Wrapper`. `DX8Wrapper` still has compatibility accessors and still owns
  the DX8 apply facade, but it is no longer the storage owner for that state.
- The raw D3D texture-stage binding cache formerly stored as
  `DX8Wrapper::Textures` also lives in `FixedFunctionState`. It remains
  separate from `RenderStateStruct::Textures`: one tracks actual immediate D3D
  bindings, the other tracks deferred `TextureBaseClass` render state.
- `RenderStateCache` is now a compatibility facade over `FixedFunctionState`.
  The cached D3D render-state array, texture-stage-state array, and transform
  array no longer live in `RenderStateCache`; callers still use the old API
  while later phases replace the D3D-shaped layout with semantic backend state.
- `BgfxBackend::Set_Transform` updates bgfx frame matrices and the
  `FixedFunctionState` transform cache directly. It no longer calls
  `DX8Wrapper::Set_Transform` just to keep the legacy cache warm.
- Shader, material, texture, and changed-mask flag ownership now has explicit
  `FixedFunctionState` mutators. `DX8Wrapper` delegates its compatibility
  setters to those helpers, and `BgfxBackend` uses them directly instead of
  calling `DX8Wrapper::Set_Shader`, `Set_Material`, or `Set_Texture`.
- World/view identity helpers now also live in `FixedFunctionState`.
  `DX8Wrapper` keeps compatibility entry points, while `BgfxBackend` updates
  identity state directly instead of routing through the DX8 wrapper.
- Vertex/index buffer binding state now has `FixedFunctionState` mutators.
  `DX8Wrapper` remains the legacy facade, but bgfx no longer calls through it
  to keep current VB/IB state synchronized.
- `BgfxBackend.cpp` no longer has active `DX8Wrapper::...` calls. Remaining
  mentions in that file are comments describing legacy call paths.
- `W3DSmudge::copyRect` now reads through
  `IRenderBackend::Capture_Back_Buffer_Surface` and `SurfaceClass::Lock`
  instead of grabbing `IDirect3DDevice8` and using `CopyRects` directly.
- `W3DShaderManager::getChipset` now queries
  `IRenderBackend::Get_Device_Identity` instead of reaching through
  `DX8Wrapper` for adapter IDs and shader caps.
- `WaterRenderObjClass::renderWaterMesh` now writes dynamic grid vertices
  through `DynamicVBAccessClass` and draws the grid through `IRenderBackend`
  instead of locking, binding, and drawing raw D3D vertex/index buffers.
- Dynamic water-grid vertex-buffer allocation no longer creates a raw D3D
  buffer. The static sea-water patch still uses the legacy D3D buffer path and
  remains a separate migration target.
- Dynamic water-grid index generation now creates only the backend index buffer.
  The raw D3D index mirror is kept only for the static sea-water patch path.
- Texture-coordinate UV wrapping now has a real `DX8Backend` implementation,
  allowing legacy sea-water setup to call `IRenderBackend::Set_Texture_UV_Wrap`
  instead of writing `D3DRS_WRAP0` directly.
- The legacy D3D shader sea-water function is now compiled only outside
  `GGC_BGFX_STANDALONE`. Standalone bgfx already dispatches to
  `drawSeaBatch`; the guard reduces bgfx compile exposure to raw D3D sea code
  without changing DX8 runtime behavior.
- Legacy sea-water D3D members and static-buffer mirror requests are now also
  excluded from standalone bgfx. Backend water grid rendering remains active;
  raw D3D sea state is only compiled where the legacy DX8 sea path can run.

## Why DX8 Cannot Be Deleted Yet

`BgfxBackend` is detached from `DX8Backend`, and the current shader/material/
texture/light/VB/IB fixed-function snapshot has moved to `FixedFunctionState`.
Bgfx-compiled engine code still uses `DX8Wrapper` for compatibility entry
points and DX8-shaped helper APIs, and the broader renderer state is still
partly DX8-shaped. The current compatibility layer continues to maintain:

- current shader, material, textures, vertex buffers, and index buffer
- world/view/projection transforms
- light and fog state
- render states and texture-stage states
- D3D-shaped mirror resources for transitional texture/buffer ownership

Separately, several game and WW3D2 systems still call `DX8Wrapper` or raw
`IDirect3DDevice8` APIs directly. The highest-priority areas are:

- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/mapper.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/matrixmapper.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/scene.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/sortingrenderer.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/shader.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/vertmaterial.cpp`
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp`

The resource classes also still expose D3D-shaped storage:

- `TextureBaseClass` owns/returns `IDirect3DBaseTexture8 *`
- `TextureClass` returns `IDirect3DTexture8 *`
- `SurfaceClass` returns `IDirect3DSurface8 *`
- vertex/index buffer classes still expose `Get_DX8_*_Buffer()`

## Migration Phases

1. **Keep standalone bgfx stable.**
   Do not remove the current stubs while gameplay rendering is still being
   validated. The stubs are compatibility plumbing, not a rendering backend.

2. **Add backend-neutral state APIs for remaining raw state writes.**
   The current `IRenderBackend` has many typed state methods, but not enough to
   replace all raw texture-stage, texture-transform, light, and shader-manager
   paths. Add narrow APIs as real call sites need them. Avoid re-exposing a
   generic `SetRenderState(D3DRS_...)` facade unless the value has no useful
   backend-neutral name.

3. **Migrate direct game/WW3D2 call sites to `g_renderBackend`.**
   Start with call sites outside `dx8wrapper.cpp`, `DX8Backend.cpp`, and the
   stubs. Each migrated call site should keep the DX8 build behavior identical
   through `DX8Backend` while giving bgfx explicit state instead of relying on
   the stub device side effects.

4. **Move `DX8Wrapper` render-state tracking into neutral owners.**
   In progress. `DX8Wrapper::render_state`, `render_state_changed`, and the
   raw D3D texture binding slots formerly named `DX8Wrapper::Textures` have
   moved to `FixedFunctionState`, with `DX8Wrapper` delegating capture,
   restore, release, read-only peek, and raw texture cache compatibility calls.
   The `RenderStateCache` storage arrays (`RenderStates`,
   `TextureStageStates`, and `DX8Transforms`) have also moved to
   `FixedFunctionState`; `RenderStateCache` remains as a compatibility facade.
   The remaining work is replacing the D3D-shaped state layout with semantic
   backend state, not moving more storage out of `DX8Wrapper`.

5. **Make `BgfxBackend` inherit `IRenderBackend` directly.**
   Completed on `bobtista/remove-dx8-bgfx`. `BgfxBackend` now owns its cached
   state and no longer uses `DX8Backend` as a forwarding base.

6. **Split resources from D3D object ownership.**
   Texture/surface/VB/IB classes should store backend-neutral handles as the
   primary resource. D3D pointers should exist only in the DX8 backend path.
   After this, `StubD3D8Device.cpp` and `D3DXStandaloneStubs.cpp` can be
   removed from the standalone bgfx source list.

7. **Stop fetching min-DX8 for bgfx-only builds.**
   Once no bgfx-compiled headers expose `IDirect3D*8`, `D3D*`, or `D3DX*`
   types, `cmake/dx8.cmake` can be made conditional on the DX8 backend/tools.

## Audit Command

Use this after each phase:

```sh
python3 scripts/cpp/audit_bgfx_dx8_dependencies.py
```

The useful trend is not zero immediately. The useful trend is fewer direct raw
device calls outside the legacy DX8 implementation, fewer `DX8Backend::...`
base calls from `BgfxBackend`, and fewer D3D-shaped public resource APIs.

Current measured state after detaching `BgfxBackend`, removing `DX8Backend.cpp`
from the bgfx target, moving the first resource uploads to CPU snapshots,
routing shader/view-capture/sorted-state capture APIs through
`IRenderBackend`, and moving fixed-function state storage from `DX8Wrapper` and
`RenderStateCache` to `FixedFunctionState`:

- `raw_device`: 57 hits in 8 files
- `dx8wrapper_low_level`: 87 hits in 9 files
- `dx8wrapper_high_level`: 22 hits in 3 files
- `d3d_public_type`: 2872 hits in 56 files
- `bgfx_dx8backend_base_call`: 0 hits
- `bgfx_peek_dx8_state`: 0 hits
- total categorized hits: 3038

Completed low-risk migrations:

- `IRenderBackend::Bind_Texture_Immediate` for code that intentionally needs an
  immediate texture bind before a draw.
- `IRenderBackend::Clear_Light` for explicit light-slot clearing.
- Backend-owned legacy shader handle lifetime and shader bind routing for
  terrain/filter paths.
- Backend-owned tactical view capture and screen/captured-view quad submission.
  The bgfx backend reports the old D3D render-target texture filter path as
  unsupported; native bgfx filters should be implemented as scene-composite
  passes instead of fake D3D textures.
- `SortingRendererClass` captures its translucent replay state through
  `IRenderBackend::Capture_Legacy_Render_State_For_Sorted_Draw` instead of
  calling `DX8Wrapper` directly. The method is transitional: bgfx still sources
  the snapshot from the legacy state cache until phase 4 moves that state into
  a neutral owner.
- `SortingRendererClass` releases direct-draw replay state through
  `IRenderBackend::Release_Legacy_Render_State_For_Sorted_Draw` and now uses
  `Matrix4x4` for local sort-depth math. Its remaining D3D-shaped references
  are the saved legacy state layout and the D3DLIGHT conversion used to build
  `RenderBackendLight`.
- `RenderStateStruct` ownership moved from `DX8Wrapper` to
  `FixedFunctionState`. The DX8 path still applies that state through
  `DX8Wrapper::Apply_Render_State_Changes`; bgfx sorted/effect replay still
  snapshots it through backend hooks while later phases replace the
  D3D-shaped layout.
- The raw D3D texture binding cache moved from `DX8Wrapper::Textures` to
  `FixedFunctionState`. This preserves immediate-bind COM ref ownership while
  removing another static state array from `DX8Wrapper`.
- `RenderStateCache` storage moved to `FixedFunctionState`. This preserves the
  old invalidation, bounds-check, and transform-cache semantics while making
  `RenderStateCache` a transition facade instead of a storage owner.
- `BgfxBackend::Set_Transform` no longer calls `DX8Wrapper::Set_Transform`.
  The bgfx path now keeps transform state through `FixedFunctionState` and its
  own frame matrices, leaving DX8 device programming to `DX8Backend`.
- `FixedFunctionState` owns shader/material/texture mutation helpers and
  changed-mask constants. `DX8Wrapper` keeps the legacy inline API as a
  compatibility facade, while `BgfxBackend` updates this state directly.
- `FixedFunctionState` owns world/view identity helpers. This preserves the
  legacy changed-mask semantics while removing another pair of bgfx calls
  through `DX8Wrapper`.
- `FixedFunctionState` owns vertex/index buffer binding mutation helpers,
  including dynamic VB/IB offsets and dirty-mask updates. Dynamic access
  classes expose narrow read-only accessors so the state owner no longer needs
  `DX8Wrapper` friendship.
- `BgfxBackend::Set_Texture` uses `FixedFunctionState::Set_Texture`'s own
  texture-stage bound check instead of querying the legacy `DX8Caps` object
  through `DX8Wrapper`.
- The smudge hardware test no longer performs raw D3D render-target readback.
  DX8 still provides the captured surface through `DX8Backend`, while bgfx can
  keep standalone smudge support on its native scene-color path.
- `IRenderBackend::Get_Device_Identity` captures the small capability surface
  the shader manager needs for chipset classification. DX8 fills it from
  `DX8Caps` and adapter identifiers; bgfx reports a generic modern shader-cap
  profile so terrain/water shader selection stays on the high-quality path.
- `WaterRenderObjClass::renderWaterMesh` uses the existing dynamic vertex
  buffer abstraction and `IRenderBackend::Draw_Strip` path for water grid
  draws. This removes the old raw D3D dynamic-lock/draw path while keeping sea
  patch and bump-texture migration as separate, higher-risk follow-up work.
- The dynamic water grid no longer allocates a raw D3D vertex buffer or tracks
  a D3D lock offset. `generateVertexBuffer(..., doStatic=false)` is now a
  no-op because the grid vertices are produced into backend dynamic buffers at
  draw time. Static sea-water vertex-buffer allocation is intentionally
  unchanged.
- Water-grid index generation now opts out of the raw D3D mirror. The same
  helper still creates the mirror for the static sea-water path, which has not
  been migrated yet.
- `IRenderBackend::Set_Texture_UV_Wrap` is implemented in `DX8Backend` and used
  by legacy sea-water setup. The bgfx implementation already existed for its
  shader uniform path; this closes the DX8 half of that deliberate API.
- `drawSea` remains the original DX8 sea-water implementation, but it is now
  excluded from standalone bgfx builds. This is containment, not a full
  replacement: the real follow-up is a deliberate sea-water backend API or a
  native bgfx sea-water program if we want parity with the old D3D bump/
  reflection path.
- The legacy sea-water members (`m_pDev`, raw sea VB/IB, and legacy wave shader
  handles) are guarded with the same standalone-bgfx boundary. Attempts to
  request a raw static D3D mirror in standalone bgfx now fail explicitly; the
  bgfx sea path uses `drawSeaBatch` and transient backend buffers instead.
- Lighting enable, texture factor, decal Z-bias, shader blend/depth/cull state,
  alpha-test state, multiply-mode blend override, and normalize-normals state
  now flow through backend methods instead of direct
  `DX8Wrapper::Set_DX8_Render_State` calls.
- `BgfxBackend` render-state, transform, material/light, clear, stencil-shadow,
  shader texture, buffer, and texture paths no longer depend on
  `DX8Backend`.
- The bgfx target does not compile `DX8Backend.cpp` or `dx8webbrowser.cpp`.
- `BgfxBackend::Invalidate_Cached_Render_States` intentionally remains the
  default no-op for now. The DX8 invalidation model writes sentinel values into
  a cache while the real D3D device continues to own live state; bgfx currently
  uses this cache as live authoritative state, so blindly invalidating it can
  corrupt subsequent draws. A future bgfx invalidation path needs semantic
  default-state rehydration rather than `RenderStateCache::Invalidate()`.
- Sorted bgfx particles/effects still snapshot D3D-shaped fixed-function state
  through `FixedFunctionState`. That coupling is intentional for now: the
  storage owner is neutral, but the data layout still mirrors the legacy
  render-state cache. Until a semantic sorted-state layout replaces it, bgfx
  buffer binds must continue to mirror into `FixedFunctionState` so sorted
  replay records valid VB/IB state.

Next migration focus:

- Replace remaining raw device call sites outside `dx8wrapper.cpp`, especially
  water, snow, smudge/profiler capture, and the remaining shader-manager legacy
  filter snippets.
- Add a backend sea-water mesh submission path or convert sea water to existing
  `VertexBufferClass` / `IndexBufferClass` abstractions.
- Add a backend readback/profiler API, or make profiler capture an explicit
  unsupported capability in standalone bgfx.
- Continue splitting remaining `DX8Wrapper` state into backend-neutral owners:
  the storage owner has moved, but the D3D-shaped render-state,
  texture-stage-state, and transform layout still needs semantic replacement.
- Design a bgfx-safe state invalidation/default-reapply path before making
  `g_renderBackend->Invalidate_Cached_Render_States()` active for bgfx.
- Replace transitional raw texture-stage APIs with semantic descriptors for
  shader, mapper, terrain, and water call sites.
- Split texture/surface/VB/IB primary ownership from `IDirect3D*8` objects so
  the bgfx build can eventually drop `StubD3D8Device.cpp` and the min-DX8
  headers entirely.
