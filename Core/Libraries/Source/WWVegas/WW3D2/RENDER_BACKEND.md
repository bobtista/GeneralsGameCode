# WW3D2 Render Backend Abstraction — Phase 1

**Branch:** `bobtista/refactor/render-backend-interface`
**Base:** `superhackers/main` at `df31c7eae` (post-unification)
**Status:** Phase 1 structural work complete pending Windows build validation

## Purpose

The goal of the render-backend refactor is to decouple WW3D2 from its hard dependency on DirectX 8 so the game can eventually run on a modern renderer (bgfx or Diligent Engine) targeting DirectX 11 / Vulkan / Metal / OpenGL and thereby run natively on Linux and macOS — not just through Wine/Proton translation.

The refactor is multi-phase. This directory's `RENDER_BACKEND.md` tracks the phase currently in progress.

### Migration phases (full picture)

| Phase | Description | State |
|---|---|---|
| 0 | Unify Generals and Zero Hour WW3D2 trees (`dx8wrapper`, `dx8caps`, `shdlib`) | **Done** (#2499) |
| 1 | Introduce `IRenderBackend` abstract interface. Make `DX8Wrapper` internals reachable through it. Zero behavior change. | **Done** (branch `bobtista/refactor/render-backend-interface`, pending Windows build validation) |
| 2 | Add parallel `BgfxBackend` and `DiligentBackend` behind a compile-time flag. DX8 path remains the default. Stubs only — real implementations come in Phase 3. | **Scaffolding done** (branch `bobtista/feat/phase2-render-backends`, see [PHASE2.md](PHASE2.md)) |
| 3 | Port rendering subsystems to use the interface, in order of isolation: 2D UI, debug geometry, heat haze, static meshes, terrain, shadows, water, particles. **Scope revision**: this phase is now a pure decoupling pass (no backend runtime changes). Filling in real backend implementations is deferred to Phase 4 because of the DX8 swapchain ownership problem — see [PHASE3.md](PHASE3.md) for details. | **8 subsystems done** (5 in Phase 3 on `bobtista/refactor/phase3-migrate-callsites`, 3 more + interface extension in Phase 3B on `bobtista/refactor/phase3b-migrate-callsites`, see [PHASE3.md](PHASE3.md) / [PHASE3B.md](PHASE3B.md)) |
| 4 | **The cutover.** Add `IRenderBackend::Init(hwnd, w, h)`, extend the interface with abstractions for remaining low-level D3D8 calls (blend ops, stencil ops, texture stage states), tear down `DX8Wrapper::Create_Device()` under non-DX8 backends, flip swapchain ownership to the new backend, fill in all backend stub methods with real rendering code. First point where the game actually renders through bgfx or Diligent. Then add Linux / macOS native builds. | Not started |
| 5 | Decide: retire the DX8 path once subsystem parity is reached, or keep it as a permanent reference implementation per xezon's request. | Not started |

## Phase 1 scope

The goal of Phase 1 is to get an `IRenderBackend` interface in place that the rest of the engine can talk through, without changing any rendering behavior. At the end of Phase 1 the game still runs on DirectX 8 identically to before. The only visible difference is a new abstract interface class and a concrete `DX8Backend` adapter that forwards to the existing `DX8Wrapper` static facade.

### In scope

- A new abstract class `IRenderBackend` in `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h`, covering the W3D-facing subset of `DX8Wrapper`'s public API.
- A new concrete class `DX8Backend` in `Core/Libraries/Source/WWVegas/WW3D2/DX8Backend.{h,cpp}` that inherits from `IRenderBackend` and forwards every virtual method to the existing `DX8Wrapper::` static functions. Zero new rendering logic — pure adaptation.
- A global `IRenderBackend* g_renderBackend` pointer exposed through `RenderBackend.h`, initialized to `new DX8Backend()` during `DX8Wrapper::Init()` and freed in `DX8Wrapper::Shutdown()`.
- One small proof-of-concept call site migration, picked to be safely isolated (e.g. `W3DStatusCircle`). Demonstrates the plumbing works end-to-end without risking the main render path.
- This document, updated as tasks complete.

### Out of scope (deferred to later phases)

- Any migration of `DX8Wrapper`'s **low-level** methods (the ones that take raw D3D8 types — `Set_DX8_Render_State(D3DRENDERSTATETYPE, ...)`, `Set_DX8_Texture_Stage_State`, `_Create_DX8_Texture`, etc.). These require introducing backend-neutral enums and handle types, which is a much larger design exercise. Phase 1 keeps them as `DX8Backend`-specific escape hatches that only the DX8 backend implements.
- Migration of the 33 files outside `WW3D2` that currently hold raw `IDirect3DTexture8*`/`IDirect3DSurface8*` pointers. These need case-by-case remediation and will happen during Phase 3 (subsystem porting).
- The actual bgfx or Diligent backend implementations. Those are Phase 2.
- Any performance optimization. A virtual call through `g_renderBackend->Set_Shader(...)` adds one indirection per call site compared to the current `DX8Wrapper::Set_Shader(...)`. This is negligible on modern hardware and we'll measure only if it's a problem.
- Renaming existing functions. We preserve the current WW3D naming convention (`Set_Shader`, not `SetShader`) to minimize diff and keep migration mechanical.

### Explicit non-goals

- **This phase does not add cross-platform support yet.** That's Phase 4. The DX8 path remains Windows-only.
- **This phase does not replace `DX8Wrapper`.** `DX8Wrapper` stays as a static facade exactly as it is today. `DX8Backend` is an *additional* class that wraps the same underlying DX8 device.
- **This phase does not touch the tools.** WorldBuilder, W3DView, etc. keep linking `z_ww3d2` and calling `DX8Wrapper::` statics. Nothing changes for them.

## Design

### High-level vs low-level API split

`DX8Wrapper` has approximately 90-110 public static functions. Looking at them, they split into two groups:

1. **High-level (W3D-facing):** Take and return types from the WW3D2 layer — `ShaderClass`, `TextureBaseClass`, `VertexMaterialClass`, `Matrix4x4`, `Vector3`, etc. These are the methods the rest of the engine actually calls. Examples: `Set_Shader`, `Set_Texture`, `Set_Material`, `Set_Transform`, `Draw_Triangles`, `Clear`, `Begin_Scene`, `End_Scene`.

2. **Low-level (D3D8-facing):** Take or return raw DirectX 8 types — `D3DRENDERSTATETYPE`, `D3DLIGHT8*`, `IDirect3DTexture8*`, `D3DTRANSFORMSTATETYPE`, etc. These are mostly called from inside `dx8wrapper.cpp` itself and from the 33 files that leak D3D8 types. Examples: `Set_DX8_Render_State`, `Set_DX8_Light`, `_Create_DX8_Texture`, `_Copy_DX8_Rects`.

**Phase 1 only abstracts the high-level API.** The low-level API stays on `DX8Wrapper` as-is and is only reachable from code that already depends on DX8. This is the right cut line because:

- The high-level API is mostly backend-neutral already (it takes W3D types, not D3D types)
- The low-level API requires a backend-neutral enum/handle design that's a significant undertaking
- Only `DX8Backend` will ever need the low-level API — new backends will have their own equivalent internal calls

### Interface naming and style

Interface methods use the **existing `DX8Wrapper` names verbatim** (`Set_Shader`, `Draw_Triangles`, etc.) to keep migration mechanical. The alternative — renaming to modern camelCase — would force a touch on every call site in the game and inflate the diff significantly.

The header uses C++98-compatible syntax because it must be includable from VC6-compiled translation units (tools and the DX8 reference path):

- Pure virtual functions are fine (C++98 feature)
- No `override` keyword in the header (use comments `// IRenderBackend impl` instead)
- No `= default`, `= delete`, `nullptr`, `auto`
- No STL types in signatures (no `std::string`, `std::vector`)
- POD structs for parameter bundles
- Forward-declare W3D types rather than including their headers where possible

The concrete implementation (`DX8Backend.cpp`) can use whatever C++ features the project's main build allows (currently C++20).

### Why not a C vtable?

C++ virtual dispatch is simpler, plays well with C++98 at the header level, and lets us use W3D class types (`ShaderClass&`, `TextureBaseClass*`) directly in signatures. A C vtable would force us to unwrap those to void* and re-cast in every implementation, which is worse than the "one vtable indirection per call" cost of virtual dispatch.

### Global backend pointer

Rather than threading a backend reference through every call, we use a single global `IRenderBackend* g_renderBackend` initialized at device-init time. This matches the existing static-facade pattern and requires zero changes to callers beyond `DX8Wrapper::Set_X(...)` → `g_renderBackend->Set_X(...)`.

The global is exposed by a small header `RenderBackend.h`:

```cpp
#pragma once
#include "IRenderBackend.h"
extern IRenderBackend* g_renderBackend;
```

`DX8Wrapper::Init()` does `g_renderBackend = new DX8Backend();` after device creation.
`DX8Wrapper::Shutdown()` does `delete g_renderBackend; g_renderBackend = NULL;` before device teardown.

## Phase 1 task list

- [x] **1.0** Write this document
- [x] **1.1** Write `IRenderBackend.h` — the abstract interface
- [x] **1.2** Write `DX8Backend.{h,cpp}` — the concrete adapter
- [x] **1.3** Add `RenderBackend.h` + `g_renderBackend` global + init/shutdown wiring
- [x] **1.4** Migrate one isolated call site as proof (`W3DStatusCircle::Render`)
- [x] **1.5** Update this document with completion status and Phase 2 handoff notes

## What landed in Phase 1

Files added:

- `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h` — abstract virtual interface, C++98-compatible, ~60 virtual methods covering the W3D-facing subset of `DX8Wrapper`'s public API. Takes and returns W3D types (`ShaderClass`, `TextureBaseClass`, `Matrix4x4`, `Vector3`, etc.); does not expose raw D3D8 types.
- `Core/Libraries/Source/WWVegas/WW3D2/DX8Backend.{h,cpp}` — concrete implementation of `IRenderBackend` that forwards every virtual method to the existing `DX8Wrapper::` static functions. Every forwarder is a single line. Zero new rendering logic.
- `Core/Libraries/Source/WWVegas/WW3D2/RenderBackend.h` — backend-agnostic public entry point exposing `extern IRenderBackend * g_renderBackend;` and `Init_Render_Backend()` / `Shutdown_Render_Backend()` free functions.
- `Core/Libraries/Source/WWVegas/WW3D2/RenderBackend.cpp` — defines `g_renderBackend` and implements the factory functions. Phase 1 always constructs a `DX8Backend`.

Files modified:

- `Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt` — adds the four new files to `WW3D2_SRC` so they are picked up by `corei_ww3d2` and compiled into both `z_ww3d2` (GeneralsMD) and `z_ww3d2` (Generals) static libraries.
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp` — `#include "RenderBackend.h"` added. `Init_Render_Backend()` called at the end of `Do_Onetime_Device_Dependent_Inits()`. `Shutdown_Render_Backend()` called at the start of `Do_Onetime_Device_Dependent_Shutdowns()`.
- `Generals/Code/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp` — identical change mirrored from the GeneralsMD copy (the two files stay byte-identical modulo the copyright line).
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DStatusCircle.cpp` — first call site migrated. `W3DStatusCircle::Render()` now routes high-level DX8Wrapper calls through `g_renderBackend->`. Low-level `DX8Wrapper::Set_DX8_Render_State` calls are explicitly left on the static facade because `IRenderBackend` does not expose raw `D3DRENDERSTATETYPE` in Phase 1.
- `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DStatusCircle.cpp` — identical change.

Commits on this branch (`bobtista/refactor/render-backend-interface`, based on `superhackers/main@df31c7eae`):

```
42c0df63c refactor(ww3d): route W3DStatusCircle high-level calls through g_renderBackend
77e5502ca feat(ww3d2): add global g_renderBackend pointer and lifecycle wiring
99b936b9d feat(ww3d2): add DX8Backend adapter forwarding to DX8Wrapper statics
3e0dbf4e7 feat(ww3d2): add IRenderBackend abstract interface
d2dff981b docs(ww3d2): add render backend Phase 1 plan
```

Runtime behavior is unchanged: the new `g_renderBackend->X(...)` calls in `W3DStatusCircle::Render()` go through the virtual dispatch to `DX8Backend::X(...)` which immediately forwards to `DX8Wrapper::X(...)`. The only observable difference versus `superhackers/main` is one extra vtable indirection per rendering call site in `W3DStatusCircle::Render()`.

## What's NOT in Phase 1

- **33 files outside `WW3D2` that hold raw `IDirect3DTexture8*` / `IDirect3DSurface8*` / `D3DMATRIX` types.** These still compile against DX8 and will remain DX8-only until Phase 3 ports them subsystem by subsystem.
- **The low-level D3D8 API on `DX8Wrapper`.** Calls like `Set_DX8_Render_State`, `Set_DX8_Texture_Stage_State`, `_Create_DX8_Texture`, `_Get_D3D_Device8`, etc. stay on `DX8Wrapper` as DX8-only methods. Adding backend-neutral equivalents is a design exercise for a later phase.
- **Migration of the other ~200 call sites** that use `DX8Wrapper::X(...)` syntax. Only `W3DStatusCircle` has been migrated in Phase 1 as a proof of concept. Phase 3 migrates the remaining subsystems.
- **Any bgfx or Diligent code.** That's Phase 2.
- **A CMake flag to select the backend.** Phase 1 hardcodes `new DX8Backend()` in `Init_Render_Backend()`. Phase 2 will introduce `GGC_RENDER_BACKEND` to pick between backends at compile time.
- **Native Linux or macOS build targets.** Still Windows-only. Phase 4 adds cross-platform.

## Windows build verification needed

**This branch has not been built on Windows.** Development happens on macOS where DirectX 8 headers are not available, so clang LSP can't see `DX8Wrapper::` methods and complains about undefined identifiers in every new file. Those errors are LSP-only and will disappear on a real MSVC 2022 build.

Concrete verification checklist for the first Windows build:

1. **Does the normal build still compile?** `cmake -S . -B build && cmake --build build --config Release --target z_generals z_generalszh`. This exercises every file in the refactor, including the new `DX8Backend.cpp` and `RenderBackend.cpp` which are compiled as part of `z_ww3d2`.
2. **Does the VC6 build still compile?** If the project's VC6 toolchain build is still active, it must successfully compile `IRenderBackend.h`, `DX8Backend.h`, `RenderBackend.h`, and the two modified `dx8wrapper.cpp` files. The interface headers are written in a C++98-compatible style specifically for this.
3. **Does the game still launch and render correctly?** Start a skirmish match, toggle the status circle on via the "show team dot" option, verify it still renders. This exercises the migrated call site in `W3DStatusCircle::Render()`. Alt-tab out and back in to exercise device loss / recovery and confirm `Shutdown_Render_Backend()` and `Init_Render_Backend()` lifecycle hooks fire correctly during device reset.
4. **Any visible difference in any subsystem?** There should not be — `W3DStatusCircle` is the only migrated caller, and its calls go through a trivial forwarding adapter. If anything looks different, that's a bug in `DX8Backend`'s forwarding layer that must be fixed before Phase 2.

Likely compile errors on first Windows build and how to fix them:

- **"`DX8Wrapper::X` is private"** — the interface header uses some method name I assumed was public but is actually protected or private. Either make it public in `dx8wrapper.h` or remove it from `IRenderBackend.h` if it shouldn't be part of the public surface.
- **"no matching function for call to `Set_Viewport`"** — the `D3DVIEWPORT8` conversion in `DX8Backend::Set_Viewport` may need tweaking if the `DWORD` field types don't match the `unsigned int` in `RenderBackendViewport`.
- **"`D3DTRANSFORMSTATETYPE` is not convertible from `TransformKind`"** — the `static_cast<D3DTRANSFORMSTATETYPE>` in `DX8Backend::Set_Transform` may fail on strict compilers if the enum base types don't line up. Fix by picking matching values or using an explicit switch.
- **Any "undeclared identifier `g_renderBackend`"** in `W3DStatusCircle.cpp` — confirm `RenderBackend.h` is reachable through the normal `#include "WW3D2/..."` path.

None of these should be hard to resolve. Each one is a 1-5 line fix.

## Phase 2 preview

Once Phase 1 is validated on Windows, Phase 2 adds parallel modern backends:

1. **Introduce `GGC_RENDER_BACKEND` CMake option** — values `dx8` (default), `bgfx`, `diligent`. Selected at configure time.
2. **Add `BgfxBackend.{h,cpp}`** under a new subdirectory like `Core/.../WW3D2/backends/bgfx/` (or similar). Implements `IRenderBackend` against bgfx, which we'll use in its DX11 backend on Windows and Metal/Vulkan on Linux/Mac. Pulls in the `bgfx.cmake` submodule.
3. **Add `DiligentBackend.{h,cpp}`** similarly. Pulls in `DiligentCore`.
4. **Modify `RenderBackend.cpp`** to pick the concrete backend based on `GGC_RENDER_BACKEND` at compile time:
   ```cpp
   #if GGC_RENDER_BACKEND == GGC_BACKEND_BGFX
       g_renderBackend = new BgfxBackend();
   #elif GGC_RENDER_BACKEND == GGC_BACKEND_DILIGENT
       g_renderBackend = new DiligentBackend();
   #else
       g_renderBackend = new DX8Backend();
   #endif
   ```
5. **Phase 2 exit criterion:** the game builds with `-DGGC_RENDER_BACKEND=bgfx`, starts, and renders *something* (probably just a clear color at first, or whatever the one migrated call site in `W3DStatusCircle` can draw). It won't render the whole scene — only the migrated subsystems come through.

See the heat-haze spike (`bobtista/spike/heat-haze-renderer-backends` branch) for hands-on familiarity with both bgfx and Diligent APIs. The spike validated both libraries against a standalone Win32 test app and can inform the Phase 2 backend implementations directly.

## Phase 3+ preview (for planning)

Phase 3 migrates the remaining subsystems from `DX8Wrapper::` statics to `g_renderBackend->` calls, in order of isolation:

1. 2D UI / `render2d`
2. Debug geometry
3. Static W3D meshes via `DX8TextureCategoryClass`
4. Heat haze (`W3DSmudge`)
5. Terrain
6. Shadows (projected + stencil)
7. Water
8. Particles

Each subsystem migration is a standalone commit that doesn't affect the others. Low-level D3D8 uses that a subsystem depends on must be replaced with backend-neutral alternatives before the subsystem can be fully migrated — which may involve growing `IRenderBackend` with new abstracted methods, or rewriting the subsystem to use higher-level primitives.

Phase 4 adds Linux and macOS native build targets once the migrated subsystems cover enough of the render pipeline to produce a playable game.

Phase 5 decides whether to retire the DX8 path or keep it permanently as a reference implementation.


## Why this phase matters

Phase 1 is the single biggest invisible refactor in the whole renderer modernization effort. It's invisible because nothing changes for the player or the rest of the engine; it's the biggest because it establishes the seam that every subsequent phase depends on. Getting the seam right saves months of rework later.

Specifically, Phase 1 enables:

- **Phase 2** can add `BgfxBackend` and `DiligentBackend` in parallel, behind a compile-time flag, without touching any existing code
- **Phase 3** can port subsystems one at a time, in any order, without risk to the rest of the engine
- **Phase 4** can add Linux and macOS targets knowing that the renderer-specific coupling is already contained to one well-defined interface
- **Phase 5** can retire the DX8 path at any point (or keep it forever) without touching any engine code

A sloppy Phase 1 forces a second refactor later; a careful Phase 1 lets every subsequent phase proceed in parallel with minimal coordination cost.
