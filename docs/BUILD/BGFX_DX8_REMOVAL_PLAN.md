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

Recent progress on `bobtista/remove-dx8-bgfx`:

- `BgfxBackend` now inherits `IRenderBackend` directly instead of `DX8Backend`.
- `BgfxBackend.cpp` no longer forwards through `DX8Backend` or peeks
  `DX8Wrapper` state directly.
- The bgfx CMake target no longer compiles `DX8Backend.cpp` or
  `dx8webbrowser.cpp`.
- Static vertex/index buffers and texture uploads are sourced from CPU-side
  snapshots instead of bgfx locking D3D mirror resources directly.

## Why DX8 Cannot Be Deleted Yet

`BgfxBackend` is detached from `DX8Backend`, but bgfx-compiled engine code still
uses `DX8Wrapper` as the fixed-function state and resource compatibility layer.
`DX8Wrapper` continues to maintain:

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

4. **Move `DX8Wrapper` render-state tracking into a neutral owner.**
   This is the main cutover. The state currently stored in
   `DX8Wrapper::render_state`, `RenderStates`, `TextureStageStates`,
   `Textures`, and `DX8Transforms` needs a backend-neutral home. The DX8 build
   can then become one consumer of that state instead of the owner.

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
from the bgfx target, moving the first resource uploads to CPU snapshots, and
routing shader/view-capture APIs through `IRenderBackend`:

- `raw_device`: 66 hits in 10 files
- `dx8wrapper_low_level`: 87 hits in 9 files
- `dx8wrapper_high_level`: 29 hits in 4 files
- `d3d_public_type`: 2898 hits in 54 files
- `bgfx_dx8backend_base_call`: 0 hits
- `bgfx_peek_dx8_state`: 0 hits
- total categorized hits: 3080

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
- Lighting enable, texture factor, decal Z-bias, shader blend/depth/cull state,
  alpha-test state, multiply-mode blend override, and normalize-normals state
  now flow through backend methods instead of direct
  `DX8Wrapper::Set_DX8_Render_State` calls.
- `BgfxBackend` render-state, transform, material/light, clear, stencil-shadow,
  shader texture, buffer, and texture paths no longer depend on
  `DX8Backend`.
- The bgfx target does not compile `DX8Backend.cpp` or `dx8webbrowser.cpp`.
- Sorted bgfx particles/effects still snapshot legacy fixed-function state
  through `DX8Wrapper::render_state`. That coupling is intentional for now:
  removing it requires phase 4, moving render-state ownership into a neutral
  owner. Until then, bgfx buffer binds must continue to mirror into the legacy
  render-state cache so sorted replay records valid VB/IB state.

Next migration focus:

- Replace remaining raw device call sites outside `dx8wrapper.cpp`, especially
  water, snow, smudge/profiler capture, and the remaining shader-manager legacy
  filter snippets.
- Add a backend sea-water mesh submission path or convert sea water to existing
  `VertexBufferClass` / `IndexBufferClass` abstractions.
- Add a backend readback/profiler API, or make profiler capture an explicit
  unsupported capability in standalone bgfx.
- Move `DX8Wrapper`'s render-state arrays into a backend-neutral owner so
  call sites can keep fixed-function semantics without requiring a D3D-shaped
  wrapper.
- Replace transitional raw texture-stage APIs with semantic descriptors for
  shader, mapper, terrain, and water call sites.
- Split texture/surface/VB/IB primary ownership from `IDirect3D*8` objects so
  the bgfx build can eventually drop `StubD3D8Device.cpp` and the min-DX8
  headers entirely.
