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
- Legacy sea-water bump-map textures and the D3D bump-map conversion helper are
  excluded from standalone bgfx. The disabled MD old-water loader block was
  also removed; runtime water still uses the active bgfx grid/sea paths.
- `W3DWater.h` no longer forward-declares D3D interfaces in standalone bgfx;
  those typedefs are now only visible when the legacy DX8 sea-water members are
  compiled.
- Water's active flat-water setup no longer carries a dead alternate D3D state
  branch beside it; this removes stale direct-DX8 fallback text without changing
  the runtime path.
- Snow point-sprite-only D3D declarations are isolated from standalone bgfx.
  Standalone bgfx continues to use the existing snow quad path; a real
  point-sprite backend API remains optional future work.
- Snow point-sprite recursion and D3D dynamic-buffer fields are now also
  excluded from standalone bgfx. The active bgfx path remains the camera-facing
  quad renderer.
- Snow now queries point-sprite support through `IRenderBackend`; the legacy
  D3D point-sprite renderer still exists only for non-standalone builds, while
  bgfx explicitly reports no point-sprite support and uses the quad path.
- The legacy shader Voodoo3 stage-2 compatibility path now writes through
  `IRenderBackend` instead of issuing raw `SetTextureStageState`/`SetTexture`
  calls directly.
- `CameraShakeSystem.cpp` no longer includes D3DX or `dx8wrapper.h`; it did not
  use DX8 symbols and only pulled D3D headers into standalone bgfx builds.
- `W3DShroud.h` no longer includes `dx8wrapper.h`; the header now forward
  declares `TextureClass`, and the `.cpp` also dropped its unused wrapper
  include.
- Several leaf terrain/UI buffer sources no longer include `dx8wrapper.h` when
  they only submit through existing renderer/backend abstractions.
- Several WW3D2 leaf sources also dropped unused `dx8wrapper.h` includes where
  they already use renderer/backend-neutral APIs or do not touch render state.
- The device-reset cleanup hook now has its own small
  `RenderDeviceCleanupHook` interface instead of being declared by
  `dx8wrapper.h`; terrain cleanup users no longer include the whole DX8
  wrapper for that hook.
- Legacy buffer type constants now live in `RenderBufferTypes.h`, allowing
  dynamic/static buffer users to name the transitional buffer categories
  without including `dx8wrapper.h`.
- `DX8PolygonRendererClass` now routes strip draws through
  `IRenderBackend::Draw_Strip`, matching the existing triangle path and
  removing its active wrapper dependency.
- `TextureFilterClass` now stores backend sampler-filter enums directly rather
  than storing `D3DTEXF_*` constants and converting them during apply.
- DX8 vertex-buffer copy helpers now use `WW3DColor` for diffuse color packing
  instead of routing that scalar conversion through `DX8Wrapper`.
- `BaseHeightMap` now registers its device-reset cleanup hook through
  `IRenderBackend`; DX8 delegates to `DX8Wrapper::SetCleanupHook`, while bgfx
  keeps the default no-op.
- Stale `dx8wrapper.h` includes were removed from height-map, water-track, and
  GeneralsMD projected/volumetric shadow sources that already submit through
  backend or buffer abstractions.
- The legacy snow point-sprite path is isolated behind a private
  `W3DSnowPointSpriteRenderer`, so `W3DSnow.h` no longer exposes an
  `IDirect3DVertexBuffer8` member. Standalone bgfx continues to use the quad
  snow path.
- The texture reset/recreate tracker contract now lives in neutral
  `TextureResourceManager` files. `dx8texman` only owns the DX8-specific
  tracker subclasses that recreate D3D texture objects, while general shutdown
  and device-reset code calls `TextureResourceManagerClass`.

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
`IRenderBackend`, moving fixed-function state storage from `DX8Wrapper` and
`RenderStateCache` to `FixedFunctionState`, and routing the remaining
game-facing texture-stage state writes through semantic backend APIs:

- `raw_device`: 54 hits in 8 files
- `dx8wrapper_low_level`: 77 hits in 8 files
- `dx8wrapper_high_level`: 20 hits in 2 files
- `d3d_public_type`: 1807 hits in 48 files
- `bgfx_dx8backend_base_call`: 0 hits
- `bgfx_peek_dx8_state`: 0 hits
- total categorized hits: 1958

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
- Legacy sea-water bump-map texture arrays and `initBumpMap` are guarded out of
  standalone bgfx too. The old MD bump-water texture loading block was already
  disabled; removing it eliminates dead D3D/D3DX references without changing
  active water rendering.
- Water's D3D interface forward declarations now share the same
  non-standalone boundary as the fields that use them, reducing standalone bgfx
  header exposure without changing legacy DX8 compilation.
- `W3DSnow` no longer declares its D3D point-vertex format or point-sprite
  vertex struct in standalone bgfx. This keeps current bgfx behavior unchanged:
  snow uses camera-facing quads, while the old DX8 point-sprite path remains
  compiled only for non-standalone builds.
- The rest of `W3DSnow`'s point-sprite-only recursive renderer and dynamic D3D
  buffer bookkeeping is now behind that same non-standalone boundary. Shared
  snow setup and the backend dynamic-quad renderer still compile for bgfx.
- Water-track wave quads and tree buffer rebuilds now pass backend-neutral
  `RB_LOCK_*` flags into the shared vertex/index lock helpers instead of raw
  `D3DLOCK_*` constants. The helpers still translate to the native lock mode in
  the active backend.
- The procedural tree atlas refreshes its CPU texture snapshot after direct
  legacy-surface writes and mip filtering. This keeps bgfx texture uploads keyed
  to the completed atlas revision instead of the constructor-time empty texture.
- Removed obsolete `#if 0` terrain light/cloud texture setup blocks that still
  referenced direct DX8 transform and D3DX matrix calls. Runtime behavior is
  unchanged; the live path was already just `TextureClass::Apply`.
- The shader manager's legacy Voodoo3 stage-2 workaround now uses
  `IRenderBackend::Set_Texture_Stage_State` and
  `Bind_Texture_Immediate`. This keeps the path available for DX8 while
  removing the direct `DX8CALL` dependency from GeneralsMD shader setup.
- Removed unused D3DX/DX8 includes from the camera shake system. This is a
  header hygiene step: camera shake math is backend-neutral and does not need
  DX8 declarations in standalone bgfx.
- Removed unused `dx8wrapper.h` includes from shroud header/source. Shroud
  resource updates already flow through `SurfaceClass`, `TextureClass`, and
  backend texture notification rather than direct DX8 device calls.
- Removed unused `dx8wrapper.h` includes from prop, terrain-background, bib,
  bridge, custom-edging, debug-icon, road, status-circle, browser, and waypoint
  buffer sources. This does not alter draw behavior; it only reduces accidental
  D3D header exposure in standalone bgfx compiles.
- Removed unused wrapper includes from sentence rendering, segmented line,
  streak, texture filter, asset manager, mapper, matrix mapper, particle buffer,
  and scene sources. These files still include their actual renderer/backend
  dependencies directly where needed.
- Removed a stale Win-only D3DX include from the GeneralsMD web-browser bridge.
- Terrain-track, terrain-background, tree-buffer, and GeneralsMD shadow-buffer
  headers now forward-declare the legacy DX8 buffer classes instead of pulling
  D3D buffer headers into every include consumer. Their implementation files
  include the concrete buffer definitions where allocation and locks occur.
- `IRenderBackend::Supports_Texture_Format` now owns hardware/backend texture
  format checks. Radar texture format selection and GeneralsMD video-buffer
  selection no longer reach through `DX8Wrapper::Get_Current_Caps()`.
- `Get_Valid_Texture_Format` now uses the same backend texture-format query for
  compression and fallback decisions. It still uses `DX8Wrapper` for legacy
  color packing helpers, but no longer needs DX8 caps for format validation.
- `IRenderBackend::Get_Max_Texture_Stages` now owns the max texture-stage query
  used by material installation and sorted-state texture comparison/ref release.
  `matpass.cpp` no longer includes `dx8wrapper.h`.
- `IRenderBackend::Supports_Z_Bias` now owns decal Z-bias support checks.
  Decal mesh generation no longer includes `dx8wrapper.h`/`dx8caps.h` just to
  decide whether it must physically offset decal polygons.
- `IRenderBackend::Supports_Compressed_Textures`,
  `Supports_Bump_Envmap`, and `Supports_Bump_Envmap_Luminance` now own
  texture compression/bump-map capability checks used by texture creation and
  loading. Common texture code no longer reaches into `DX8Caps` for those
  decisions.
- `IRenderBackend::Get_Texture_Limits` now owns max texture width/height,
  volume extent, and aspect-ratio limits. `textureloader.cpp` no longer reads
  raw `D3DCAPS8` during size validation.
- `IRenderBackend::Supports_Texture_Filter` now owns linear/mip/anisotropic
  filter capability checks. `texturefilter.cpp` no longer reads
  `D3DCAPS8::TextureFilterCaps`.
- `IRenderBackend::Supports_Texture_Op`, `Supports_Fog`, and
  `Is_Legacy_Voodoo3` now own shader texture-op/fog/legacy-device capability
  checks. `shader.cpp` no longer includes `dx8caps.h` or reads raw
  `TextureOpCaps`.
- `IRenderBackend::Supports_NPatches` now owns N-Patch capability checks used
  by common material-normal/debug-statistics code and by legacy DX8-named
  mesh/buffer allocation paths.
- `IRenderBackend::Supports_Hardware_Transform_And_Lighting` now owns the
  legacy software-processing fallback decision in DX8 vertex/index buffers.
- `IRenderBackend::Supports_Point_Sprites` now owns the snow point-sprite
  capability decision. The actual raw D3D point-sprite draw path remains
  isolated behind the non-standalone build guard.
- `W3DShaderManager::LoadAndCreateLegacyShader` now names the shader-loading
  helper by what it does: load legacy shader blobs and ask the active backend
  to create native shader handles. The public helper no longer advertises a
  D3D-only operation.
- `RenderDeviceDescClass` no longer exposes raw `D3DCAPS8` or
  `D3DADAPTER_IDENTIFIER8` accessors to standalone bgfx builds. The private
  fields remain for `DX8Wrapper`'s legacy enumeration scaffolding until that
  wrapper work is split from the bgfx target.
- `IRenderBackend::Set_Texture_Address_Mode` and
  `Set_Texture_Sample_Filter` now provide a semantic sampler-state API. Smudge
  distortion setup uses those methods instead of writing raw address/filter
  texture-stage states directly.
- `TextureFilterClass::Apply` now pushes sampler address/filter state through
  that semantic API too. Its legacy filter tables still store D3D-compatible
  constants for now, but applying a texture filter no longer exposes those
  constants to the backend call site.
- Terrain alpha/scorch texture setup now uses the semantic sampler API for its
  terrain-quality filter selection and clamp/wrap address modes, leaving only
  fixed-function combine and texcoord state in those local terrain paths.
- Deleted the remaining obsolete `#if 0` terrain texture apply blocks that
  still contained dead DX8 sampler-state examples.
- Shader-manager terrain setup now routes repeated base terrain sampler blocks
  and 2D clamp/wrap address writes through backend-neutral sampler helpers. The
  backend sampler API now also has explicit partial min/mag and mip setters, so
  shader-manager noise, road, flat terrain, and screen-fade paths can preserve
  the legacy behavior of updating only the intended sampler components.
- River, trapezoid, reflection, and bump-map water paths now use the same
  backend sampler helpers for clamp/wrap address mode, min/mag filtering, and
  mip filtering. The remaining water texture-stage writes are fixed-function
  combine and bump-env constants.
- Active river and trapezoid water setup now selects mesh UVs and projected
  camera-space water-noise UVs through local backend semantic helpers.
- Sea-water setup and cleanup now express mesh UV selection and transform-reset
  state through the same water helpers.
- The disabled legacy water clip-plane block now also uses water texcoord and
  transform helpers, including its camera-space stage-1 UV-index variant.
- Water bump-env setup now uses backend semantic matrix and luminance APIs
  instead of writing raw bump-map texture-stage constants directly.
- `IRenderBackend` now exposes narrow texture-combine operation/argument
  setters. Water setup, cleanup, clipping, grid, and additive-alpha paths use
  those instead of direct `D3DTSS_COLOR*` / `D3DTSS_ALPHA*` writes.
- Smudge distortion rendering now also uses the texture-combine operation API
  for its alpha override and post-draw color/alpha restore.
- Water track and splash rendering uses the same texture-combine API when it
  modulates track quads with the shroud texture.
- `ShaderClass::Apply` routes its generated primary/detail texture-combine
  state through backend operation/argument setters instead of direct
  `D3DTSS_COLOR*` / `D3DTSS_ALPHA*` writes.
- The tactical-view black-and-white DOT3 screen filter now applies its color
  and alpha combiner overrides through backend operation/argument setters.
- Crossfade and motion-blur screen filters now use backend texture-combine
  setters for their mask and captured-view alpha overrides.
- Flat shroud texture setup now uses backend texture-combine setters for its
  projected shroud modulation state.
- The HeightMap wireframe extra-pass texture-factor override now writes through
  the backend texture-combine argument API.
- Projected cloud texture shader setup/reset now uses backend texture-combine
  operation/argument setters.
- `W3DShaderManager::setShroudTex` now applies its projected shroud
  texture-combine state through backend operation/argument setters.
- `TerrainShader2Stage` now applies base, blend, and projected noise/cloud
  texture-combine state through backend operation/argument setters.
- `RoadShader2Stage` now applies road base/blend/noise texture-combine state
  through backend operation/argument setters.
- `FlatTerrainShader2Stage` now applies flat terrain base, shroud, and
  projected noise/cloud texture-combine state through backend setters.
- `TerrainShader8Stage` now applies its base terrain multi-stage combine
  setup and cleanup through backend texture-combine setters.
- `AlphaTerrainTextureClass::Apply` now routes alpha terrain blend and legacy
  8-stage terrain texture-combine setup through backend setters.
- `CloudMapTerrainTextureClass::restore` now restores terrain texture-combine
  cleanup state through backend setters.
- `ScorchTextureClass::Apply` now configures scorch decal texture-combine state
  through backend setters.
- `SurfaceClass::Copy` now handles same-format surface copies with explicit
  surface locks and row copies instead of routing through `DX8Wrapper::CopyRects`.
- `MissingTexture::_Create_Missing_Surface` now fills its image surface through
  a lock instead of copying the generated texture with `CopyRects`.
- `WW3DColor` now owns backend-neutral ARGB/vector conversion helpers; the
  first leaf migration removes `ww3dformat.cpp`'s dependency on `DX8Wrapper`
  for color packing.
- Simple box and line render objects now also use `WW3DColor` for vertex color
  packing instead of depending on `DX8Wrapper`.
- Line-group geometry now packs diffuse vertex colors through `WW3DColor`.
- Segment-line effects now pack generated line colors through `WW3DColor`.
- Point-group particles now pack clamped diffuse colors through `WW3DColor`.
- Streak particles now pack clamped diffuse colors through `WW3DColor`.
- Sphere debug/utility geometry now packs alpha-vector colors through
  `WW3DColor`.
- Ring geometry now packs vertex colors through `WW3DColor`.
- Dynamic mesh helpers now pack clamped vertex colors through `WW3DColor`.
- Color-space recoloring now converts ARGB values through `WW3DColor`.
- Shatter mesh interpolation and DIG/DCG color math now use `WW3DColor`
  instead of relying on a transitive `DX8Wrapper` include.
- Mesh material DIG/DCG bake-down color math now uses `WW3DColor`.
- Mesh W3D load-time color conversion now uses `WW3DColor`.
- Dazzle rendering now uses `WW3DColor` for flare/halo colors and queries
  render-to-texture state through `IRenderBackend`.
- Shader blend/cull/fog scalar state now uses backend-neutral enums and
  integer colors instead of D3D blend/cull/color typedefs.
- Shader texture-stage operation selection now uses backend-neutral texture
  op/argument enums without D3D casts.
- `shader.cpp` no longer includes `dx8wrapper.h`; its remaining scalar state
  uses native C++ types plus backend enums.
- Game-client asset recoloring and occluded-player tint generation now use
  backend-neutral `WW3DColor` packing instead of `DX8Wrapper::Convert_Color`.
- Extended render debug toggles now live in `RenderDebugStats`; the DX8
  wrapper keeps a compatibility reference, but game-facing stats code no
  longer names `DX8Wrapper::stats`.
- Waypoint and status-circle public headers no longer export DX8 buffer
  definitions; status-circle keeps the concrete buffer includes in its
  implementation file until the buffer owner is migrated.
- Common Bezier curve evaluation now uses explicit cubic coefficients instead
  of D3DX matrix/vector helpers, removing a renderer-era dependency from
  projectile path math.
- Point-group ground-oriented quad expansion now computes its Z rotation
  directly instead of routing through `D3DXMatrixRotationZ`.
- Bib, bridge, custom-edging, road, and mirror public headers no longer export
  concrete DX8 buffer definitions when they only store buffer pointers.
- The shadow coordinator source no longer includes unused DX8 wrapper or D3DX
  math headers; the concrete shadow implementations keep their own includes
  until their buffer/math paths are migrated.
- `W3DView.cpp` and the projected-shadow source no longer include D3DX math
  when they do not use any D3DX symbols.
- Texture-filter anisotropy now uses a named backend API instead of passing
  `D3DTSS_MAXANISOTROPY` through the generic texture-stage state escape hatch.
- Texture-filter default tables now use `RenderBackendTextureSampleFilter`
  values directly. `texturefilter.cpp` no longer includes `dx8wrapper.h` just
  to name D3D sampler constants.
- DX8 vertex-buffer copy helpers now pack diffuse colors through
  `WW3DColor::To_ARGB`; the file still includes the DX8 wrapper for actual
  legacy buffer allocation, but color conversion no longer depends on it.
- Active river/trapezoid water noise texture transforms now build their
  matrices with `Matrix4x4` and backend transform APIs instead of D3DX helper
  calls.
- Water and screen-filter shader constants that were plain four-float vectors
  no longer use `D3DXVECTOR4` as storage.
- Repeated projected shroud texture-transform setup now goes through one
  `Matrix4x4` helper in `W3DShaderManager.cpp`; the bgfx shroud parameter path
  remains explicit.
- Terrain texture apply/restore paths now route mesh-UV selection and texture
  transform disable operations through semantic backend APIs instead of writing
  `D3DTSS_TEXCOORDINDEX` / `D3DTSS_TEXTURETRANSFORMFLAGS` directly.
- Shoreline destination-alpha passes now select mesh UV0 through
  `IRenderBackend::Set_Texture_Coord_Source`, removing the last raw texcoord
  stage writes from `BaseHeightMap.cpp`.
- Tree rendering now selects mesh UV0/UV1 and disables the shroud-stage
  texture transform through backend semantic APIs instead of direct
  `D3DTSS_*` writes.
- Shroud and screen-mask shader setup/reset now use local shader-manager
  helpers for camera-space projected UV2 and mesh-UV reset semantics.
- Terrain shader-manager variants now use the same helpers for base/detail
  mesh UV selection, projected noise/cloud stages, and terrain reset cleanup.
- Cloud and road shader-manager variants now also express projected noise/cloud
  stages and reset state through backend semantic texcoord helpers.
- Flat-terrain shader-manager variants now express shroud, noise, base mesh UV,
  and reset texcoord state through the same backend semantic helpers.
- Screen crossfade mask setup now selects its stage-1 mesh UV through the
  backend texcoord API, clearing the last raw texcoord state write from
  `W3DShaderManager.cpp`.
- `ShaderClass::Apply` now reads fog color through `IRenderBackend`, removing
  another direct `DX8Wrapper` query from the shared shader path.
- Tree WVP upload now builds its matrix with `Matrix4x4` and backend shader
  constants instead of round-tripping through `D3DXMATRIX`.
- `dx8webbrowser.h`, the height-map sources, and GeneralsMD `assetmgr.cpp` /
  `dx8vertexbuffer.cpp` no longer include stale D3D/D3DX headers where they
  do not use any of those symbols.
- The legacy Voodoo3 stage-2 compatibility path in `ShaderClass::Apply` now
  expresses its pass-through UV0 selection through the backend texcoord API.
- `SortingRendererClass::Flush` now saves/restores triangle draw enable through
  `IRenderBackend`. DX8 delegates to the legacy wrapper flag; bgfx owns its
  own equivalent flag and honors it in triangle/strip submits.
- Projected and volumetric shadow draw paths now also read triangle draw enable
  through `IRenderBackend`, leaving the remaining wrapper checks inside the
  legacy DX8 implementation/facade.
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

- Replace remaining raw device call sites outside `dx8wrapper.cpp`. Snow point
  sprites are now isolated in a private legacy helper and still selected only
  through a backend capability query.
- Introduce a generated-texture write/update API around `TextureClass`, then
  migrate the `TerrainTex.cpp` atlas-generation paths away from direct
  `IDirect3DTexture8`/surface locks.
- Move the remaining DX8 texture tracker subclasses behind a backend resource
  creation API once generated/procedural texture writes have a neutral upload
  path.
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
