# WW3D2 Render Backend Abstraction — Phase 1

**Branch:** `bobtista/refactor/render-backend-interface`
**Base:** `superhackers/main` at `df31c7eae` (post-unification)
**Status:** in progress

## Purpose

The goal of the render-backend refactor is to decouple WW3D2 from its hard dependency on DirectX 8 so the game can eventually run on a modern renderer (bgfx or Diligent Engine) targeting DirectX 11 / Vulkan / Metal / OpenGL and thereby run natively on Linux and macOS — not just through Wine/Proton translation.

The refactor is multi-phase. This directory's `RENDER_BACKEND.md` tracks the phase currently in progress.

### Migration phases (full picture)

| Phase | Description | State |
|---|---|---|
| 0 | Unify Generals and Zero Hour WW3D2 trees (`dx8wrapper`, `dx8caps`, `shdlib`) | **Done** (#2499) |
| **1** | **Introduce `IRenderBackend` abstract interface. Make `DX8Wrapper` internals reachable through it. Zero behavior change.** | **In progress (this branch)** |
| 2 | Add parallel `BgfxBackend` (and possibly `DiligentBackend`) behind a compile-time flag. DX8 path remains the default. | Not started |
| 3 | Port rendering subsystems to use the interface, in order of isolation: 2D UI, debug geometry, heat haze, static meshes, terrain, shadows, water, particles. Each subsystem opts in individually. | Not started |
| 4 | Add Linux / macOS native build targets. Proton compatibility improves for free. | Not started |
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

- [ ] **1.0** Write this document (you are here)
- [ ] **1.1** Write `IRenderBackend.h` — the abstract interface
- [ ] **1.2** Write `DX8Backend.{h,cpp}` — the concrete adapter
- [ ] **1.3** Add `RenderBackend.h` + `g_renderBackend` global + init/shutdown wiring
- [ ] **1.4** Migrate one isolated call site as proof (e.g. `W3DStatusCircle`)
- [ ] **1.5** Update this document with completion status and Phase 2 handoff notes

## Why this phase matters

Phase 1 is the single biggest invisible refactor in the whole renderer modernization effort. It's invisible because nothing changes for the player or the rest of the engine; it's the biggest because it establishes the seam that every subsequent phase depends on. Getting the seam right saves months of rework later.

Specifically, Phase 1 enables:

- **Phase 2** can add `BgfxBackend` and `DiligentBackend` in parallel, behind a compile-time flag, without touching any existing code
- **Phase 3** can port subsystems one at a time, in any order, without risk to the rest of the engine
- **Phase 4** can add Linux and macOS targets knowing that the renderer-specific coupling is already contained to one well-defined interface
- **Phase 5** can retire the DX8 path at any point (or keep it forever) without touching any engine code

A sloppy Phase 1 forces a second refactor later; a careful Phase 1 lets every subsequent phase proceed in parallel with minimal coordination cost.
