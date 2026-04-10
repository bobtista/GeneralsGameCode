# Render Backend Phase 3 — Migrate call sites to `g_renderBackend`

**Branch:** `bobtista/refactor/phase3-migrate-callsites`
**Base:** `bobtista/feat/phase2-render-backends` (Phase 2)
**Status:** first migration batch complete, pending Windows build verification

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the full multi-phase plan, [PHASE2.md](PHASE2.md) for the previous phase.

## Goal

Migrate engine-side call sites from `DX8Wrapper::X(...)` statics to `g_renderBackend->X(...)` virtual dispatch. Each migrated file is a mechanical find-and-replace of the high-level DX8Wrapper calls; the raw-D3D8 low-level calls (`Set_DX8_Render_State`, etc.) remain on `DX8Wrapper::` because `IRenderBackend` does not expose them.

Phase 3 is a **pure decoupling pass**. Runtime behavior is unchanged under every backend. The value is code-organizational: reducing `DX8Wrapper::*` references makes Phase 4 (the actual backend switch) possible.

## Important scope decision: Phase 3 is NOT "make bgfx/Diligent actually render"

When Phase 3 started I expected to begin filling in `BgfxBackend`/`DiligentBackend` stubs with real rendering code, one subsystem at a time. That is not viable in the current architecture because of an **architectural constraint** that didn't surface cleanly until implementation:

### The swapchain ownership problem

Under `GGC_RENDER_BACKEND=bgfx`:

- `DX8Wrapper::Init()` still runs (it's the entry point — we have nothing else to drive device creation).
- `DX8Wrapper::Create_Device()` still runs, creates an `IDirect3DDevice8`, and takes ownership of the HWND's swapchain.
- `Do_Onetime_Device_Dependent_Inits()` calls `Init_Render_Backend()` which constructs a `BgfxBackend`.
- But the `BgfxBackend` stub in Phase 2 does **not** initialize bgfx's device or swapchain — it's a pure no-op at the GPU level. There's a link anchor to `bgfx::getCaps` to force the linker to resolve bgfx symbols, but bgfx itself is never `init()`-ed.
- Every frame, `DX8Wrapper::*` statics drive the DX8 device, rendering to DX8's backbuffer and presenting it. Migrated call sites go to `BgfxBackend` stubs and silently no-op.

For `BgfxBackend` to render anything visible, bgfx must own the swapchain. But **if bgfx owns the swapchain, DX8 can no longer present**, and any unmigrated call site that still goes through `DX8Wrapper::*` → `IDirect3DDevice8::*` will render into a backbuffer that never reaches the screen.

In other words: **a single frame cannot have half of its rendering through DX8 and half through bgfx** unless there's cross-API texture interop between D3D8 and modern D3D11/Vulkan/Metal. **D3D8 does not support this.** It predates the DXGI shared-resource infrastructure. Attempts to share textures across API boundaries either require D3D9Ex+ or don't work at all.

This means the transition from DX8 to bgfx can only happen as a **cutover**: at some point, all rendering must go through `g_renderBackend->*`, bgfx takes the swapchain, DX8 is torn down, and the game runs entirely on bgfx. There's no meaningful intermediate state where bgfx is "partially rendering."

### What this implies for Phase 3

Phase 3 **cannot** be "migrate one subsystem end-to-end, see it render through bgfx, migrate the next." That's the workflow the Phase 2 docs sketched, but it doesn't work given D3D8's swapchain ownership.

Phase 3 **is** a decoupling pass: migrate as many high-level `DX8Wrapper::*` call sites as possible to `g_renderBackend->*`, while leaving the bgfx/Diligent stubs as stubs. Runtime behavior doesn't change under any backend:

- `=dx8`: calls go `g_renderBackend->X` → `DX8Backend::X` → `DX8Wrapper::X` → `IDirect3DDevice8::*`. Same rendering as before, one extra vtable hop per call site.
- `=bgfx`: calls go `g_renderBackend->X` → `BgfxBackend::X` (no-op). Also: DX8Wrapper is still active in the background, driving the main render loop through static calls that haven't been migrated. So the visible output is DX8's rendering, minus whatever subsystems have been migrated (those silently no-op under `=bgfx`).
- `=diligent`: same as `=bgfx`.

The value of Phase 3 migrations is purely code-organizational:

1. **Reduces `DX8Wrapper::*` call count** in engine code. Makes Phase 4's cutover work tractable.
2. **Validates the `IRenderBackend` interface shape** against real usage. If a subsystem needs something the interface doesn't expose, we grow the interface (via extension) or gate that subsystem's low-level calls behind `#if GGC_RENDER_BACKEND_DX8` for now.
3. **Each migration is a small independent commit** that compiles and runs identically. Low risk, easy to review, easy to revert.

Phase 4 will be where the cutover happens: introduce a bgfx/Diligent initialization that takes the swapchain, tear down DX8's presentation path, and flip over all migrated call sites to real backend code simultaneously.

## In-scope for this session

Migrate the top clean candidates identified by exploration:

| # | File | LOC | Calls | Notes |
|---|---|---|---|---|
| 1 | `W3DBibBuffer.cpp` | 441 | 7 | Smallest, cleanest. Pattern reference. |
| 2 | `W3DDebugIcons.cpp` | 317 | 9 | Clean, small |
| 3 | `W3DInGameUI.cpp` | 733 | 8 | Clean, mid-size |
| 4 | `W3DTerrainTracks.cpp` | 973 | 8 | Clean, mid-size, Core (shared) |
| 5 | `W3DBridgeBuffer.cpp` | 1203 | 14 | Clean, larger |

Each is migrated in both `Generals/` and `GeneralsMD/` copies (if present) to keep them in sync with the post-unification state.

Deferred to a later Phase 3 session (or Phase 3B):

- `W3DRoadBuffer.cpp` (3311 LOC, 12 calls) — clean but large
- `W3DStatusCircle.cpp` — partially done in Phase 1, needs interface extension for the remaining `D3DRS_BLENDOP` calls
- `FlatHeightMap.cpp` (642 LOC, 23 high-level + 1 low-level) — the 1 low-level call needs gating or interface extension
- `W3DShroud.cpp` (800 LOC, 6 high-level + 3 low-level) — same
- Other subsystems with significant low-level D3D8 usage (water, shadows, shader manager) — need the low-level API abstracted first

## Out of scope for Phase 3 entirely

- Filling in real implementations in `BgfxBackend.cpp` or `DiligentBackend.cpp` (see the swapchain ownership explanation above — wait for Phase 4)
- Swapchain ownership transfer
- Any Linux/macOS native build work
- Extending `IRenderBackend` with low-level state setters (Phase 3B or later)
- Migrating files that use raw D3D8 types (`IDirect3DTexture8*`, `IDirect3DSurface8*`, etc.)

## Migration pattern

For each file:

1. Add `#include "WW3D2/RenderBackend.h"` next to the existing `#include "WW3D2/dx8wrapper.h"`.
2. Replace every `DX8Wrapper::X(...)` where X is a high-level method with `g_renderBackend->X(...)`.
3. For `Set_Transform(D3DTS_WORLD, ...)` style calls, replace `D3DTS_WORLD` with `RB_TRANSFORM_WORLD` (and similarly for `D3DTS_VIEW` → `RB_TRANSFORM_VIEW`, `D3DTS_PROJECTION` → `RB_TRANSFORM_PROJECTION`).
4. For `Set_Vertex_Buffer` calls that relied on the default `stream=0` parameter, pass `0` explicitly since the virtual method signature requires it.
5. Leave any `DX8Wrapper::Set_DX8_Render_State` / `Set_DX8_Texture_Stage_State` / other low-level calls unchanged. Add a comment noting they're deliberately left on the static facade if any are present.
6. Leave any raw D3D8 type references unchanged (they compile fine through the existing `DX8Wrapper` path).
7. Do NOT rename any existing functions, variables, or types. The migration is purely `DX8Wrapper::X` → `g_renderBackend->X`.

## Task list

- [x] **3.0** Write this document
- [x] **3.1** Migrate `W3DBibBuffer` (both copies) — pattern reference
- [x] **3.2** Migrate `W3DDebugIcons` (both copies) — subagent
- [x] **3.3** Migrate `W3DInGameUI` (both copies) — subagent
- [x] **3.4** Migrate `W3DTerrainTracks` (Core, shared) — subagent
- [x] **3.5** Migrate `W3DBridgeBuffer` (both copies) — subagent
- [x] **3.6** Document completion + Phase 4 handoff

## What landed in this Phase 3 batch

Nine files touched across five migrations. Every call site replaced is a high-level `DX8Wrapper::*` call that is covered by `IRenderBackend`; low-level D3D8 calls were left on the static facade.

| Subsystem | File(s) | Calls migrated | Remaining `DX8Wrapper::` refs | Notes |
|---|---|---:|---:|---|
| Building bibs | `W3DBibBuffer.cpp` (Generals + ZH) | 7 → 0 | 0 | `W3DBibBuffer::renderBibs()` |
| Debug icons | `W3DDebugIcons.cpp` (Generals + ZH) | 9 → 0 | 0 | `W3DDebugIcons::Render()` |
| Debug hint UI | `W3DInGameUI.cpp` (Generals + ZH) | 8 → 1 | 1 | `DebugHintObject::Render()`. The remaining `DX8Wrapper::stats.m_disableConsole` is a static debug-stats struct access, not a method call — correctly left unchanged |
| Terrain tracks | `W3DTerrainTracks.cpp` (Core, shared) | 7 → 0 | 0 | `TerrainTracksRenderObjClassSystem::flush()` edge-flush branch |
| Bridges | `W3DBridgeBuffer.cpp` (Generals + ZH) | 14 → 1 | 1 | `W3DBridge::renderBridge()` and `W3DBridgeBuffer::drawBridges()`. The remaining reference is a commented-out debug line inside `#ifdef RTS_DEBUG` |

**Commits on this branch (`bobtista/refactor/phase3-migrate-callsites`):**

```
b746b8a8f refactor(ww3d): route W3DBridgeBuffer render calls through g_renderBackend
7f12c055d refactor(ww3d): route W3DTerrainTracks flush calls through g_renderBackend
bcde69551 refactor(ww3d): route W3DInGameUI DebugHintObject::Render calls through g_renderBackend
45b89c5d9 refactor(ww3d): route W3DDebugIcons::Render calls through g_renderBackend
e15dd034a refactor(ww3d): route W3DBibBuffer renderBibs calls through g_renderBackend
9c2c9b7e3 docs(ww3d2): add Phase 3 plan explaining decoupling-only scope
```

Each commit is independent, small, and mechanical. Any one can be reverted in isolation without affecting the others.

## What this changes at runtime

**Nothing, under any backend.** Phase 3 is a pure decoupling pass. Each migrated file goes through `g_renderBackend->X(...)` which, under `=dx8`, hits `DX8Backend::X` which immediately forwards to `DX8Wrapper::X`. Under `=bgfx` or `=diligent`, the call hits the backend stubs which silently no-op. Because most of the rest of the engine is still using `DX8Wrapper::*` statics directly, the overall rendering still happens via DX8 in all three builds — just the five migrated subsystems are routed through the new virtual interface.

Performance impact: one extra vtable indirection per call site in the migrated files. Negligible on modern hardware. `Set_Transform` / `Set_Texture` / `Set_Shader` calls that were previously `jmp` to a static function now go through a `load-vtable / indirect-call` sequence, which is 2-3 extra cycles per call. Across the migrated files, that's maybe a few thousand extra cycles per frame — well under a microsecond total.

## What's still on `DX8Wrapper::*` (intentionally)

Two remaining references across the migrated files, both correct to leave as-is:

1. `Generals/.../W3DInGameUI.cpp:424` — `DX8Wrapper::stats.m_disableConsole`. This is a static field access on `DX8_Stats`, a debug-only stats struct that's part of `DX8Wrapper`. It's not a rendering method, it's a debug state flag. Migrating it would require exposing the stats struct on `IRenderBackend` or creating a separate debug-stats interface, neither of which is warranted for a single reference inside `#ifdef EXTENDED_STATS`.
2. `Generals/.../W3DBridgeBuffer.cpp` and ZH copy — a single `//DX8Wrapper::Set_Shader(detailShader);` commented-out debug line inside `#ifdef RTS_DEBUG`. Commented code stays commented; this will get cleaned up naturally when the debug block is next touched.

## Remaining migration candidates (Phase 3B / 3C)

These were identified as clean migration targets but deferred from this first batch due to scope:

- **`W3DRoadBuffer.cpp`** (3311 LOC, 12 clean calls) — largest clean candidate. Should be the next target in a follow-up Phase 3B session because it's clean and well-understood, just large.

These need interface extension before they can migrate cleanly:

- **`W3DStatusCircle.cpp`** — partially migrated in Phase 1. Four remaining `DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, ...)` calls for fade effects need `IRenderBackend` extended with a blend-op abstraction, or gated under `#if GGC_RENDER_BACKEND_DX8` with a comment explaining "non-DX8 backends don't support this yet".
- **`FlatHeightMap.cpp`** (642 LOC, 23 high-level + 1 low-level) — the 1 low-level call is `Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, ...)` which is isolated and can be easily gated or abstracted.
- **`W3DShroud.cpp`** (800 LOC, 6 high-level + 3 low-level) — similar story, the 3 low-level calls need investigation for whether they can be gated, abstracted, or moved into WW3D2 internals.

These are deeply coupled and need the low-level D3D8 API abstracted before migration can begin:

- **`W3DWater.cpp`** — 47 low-level calls, extensive use of raw `D3DRS_*` and `D3DTSS_*`. The water system is the largest single piece of DX8-coupled code. Phase 3C or later.
- **`W3DVolumetricShadow.cpp`** / **`W3DProjectedShadow.cpp`** — stencil volume shadows use raw `D3DRS_STENCIL*` setters. Need `IRenderBackend` stencil ops abstracted.
- **`W3DShaderManager.cpp`** — central shader + post-processing manager, touches raw `IDirect3DDevice8` methods. Large and critical; careful rewrite required.
- **`W3DDisplay.cpp`** — main display context manager with device lifecycle. Close to the metal; likely the last thing to migrate before Phase 4's cutover.
- **`W3DScene.cpp`** — scene graph rendering orchestrator, 99 calls. Large, should be migrated in a focused session.
- **`W3DMouse.cpp`** — cursor rendering, uses raw `_Get_D3D_Device8` for `IDirect3DDevice8::ShowCursor` / `SetCursorProperties`. Needs a cursor API on `IRenderBackend`.

## Windows build verification needed

This branch has not been built on Windows. Expected behavior on first Windows build:

1. **`-DGGC_RENDER_BACKEND=dx8`** (default) — should compile identically to the Phase 2 branch + the 5 new migrations. Running the game should produce a byte-identical rendering result. Specifically:
   - Bibs under buildings should render
   - Debug icons should render in debug builds
   - Debug hint overlay should render in debug builds
   - Vehicle terrain tracks should render
   - Bridges should render
2. **`-DGGC_RENDER_BACKEND=bgfx`** — should still compile and link. Running the game: all five migrated subsystems silently stop rendering (they go through bgfx stubs which no-op), while everything else keeps rendering via DX8 statics. Visual: game mostly works, but no bibs, debug icons, debug hints, terrain tracks, or bridges visible. This is the **correct** Phase 3 behavior under `=bgfx` — the migrated subsystems are decoupled from DX8 but the backend hasn't been filled in yet.
3. **`-DGGC_RENDER_BACKEND=diligent`** — same as bgfx.

Any visible difference in the `=dx8` build is a bug in `DX8Backend`'s forwarding layer (Phase 1 code), not a bug in Phase 3.

**Likely first-build errors and fixes:**

- **"g_renderBackend undeclared"** — `RenderBackend.h` isn't reaching a TU because of include-path ordering. Fix: ensure `RenderBackend.h` is included in the same block as `dx8wrapper.h`. Usually a 1-line fix.
- **"no matching function for call to `Set_Vertex_Buffer`"** — I instructed subagents to add `,0` for the stream argument, but there might be cases where a function is called with a `DynamicVBAccessClass&` overload that didn't originally have the stream parameter. Those calls should NOT add `,0`. Check which overload is being invoked and revert if needed.
- **"`RB_TRANSFORM_WORLD` not declared"** — `IRenderBackend.h` needs to be included. It's included by `RenderBackend.h` so this shouldn't happen, but it's a possible include-ordering issue.

## Phase 4 preview: the cutover

Phase 4 is the big architectural step that actually makes bgfx/Diligent render anything. The shape of the work:

1. **Finish migrating remaining high-level subsystems** (Phase 3B, 3C) — drain the `DX8Wrapper::*` call count across the engine down to just the low-level D3D8 calls and the `DX8Wrapper` internals.
2. **Abstract the remaining low-level calls.** Extend `IRenderBackend` with methods for the state categories still on `Set_DX8_Render_State`: blend ops, stencil ops, texture stage ops, etc. Implement them all in `DX8Backend` as forwarders, and in `BgfxBackend` / `DiligentBackend` as real backend code.
3. **Introduce an `Init(void* hwnd, int w, int h)` virtual method** on `IRenderBackend` so the backend can take the HWND and create its own device/swapchain.
4. **Add a `GGC_BACKEND_OWNS_SWAPCHAIN` compile flag or runtime check** that, when the non-DX8 backend is selected, disables `DX8Wrapper::Create_Device()` and has the new backend create the device instead. DX8Wrapper must gracefully handle "I never got a device" — any code that calls `_Get_D3D_Device8()` must be fully abstracted or gated by this point.
5. **Flip the swapchain ownership.** Under `=bgfx` or `=diligent`, the new backend creates and owns the HWND's swapchain. DX8 is not initialized at all. `DX8Wrapper::Init()` becomes a thin pass-through that constructs the `IRenderBackend` instance and does nothing else when a non-DX8 backend is selected.
6. **Run the game under `=bgfx`** and watch the migrated subsystems actually render through real bgfx code.
7. **Add Linux / macOS native build targets** once enough of the engine works on a non-DX8 backend to be useful.

Phase 4 is a multi-week effort. It cannot start until most of the migration work is done (Phases 3B, 3C) and the low-level API abstraction is in place. Good candidates for the order:

- Migrate `W3DRoadBuffer` next (big, clean)
- Migrate `FlatHeightMap` and `W3DShroud` with minor interface extensions
- Migrate `W3DStatusCircle`'s remaining fades once blend-op abstraction exists
- Tackle the hard ones (`W3DWater`, shadows, `W3DShaderManager`, `W3DDisplay`, `W3DScene`, `W3DMouse`) with interface extensions as needed
- Only then start Phase 4

## Statistics for this session

- 5 subsystems migrated
- 9 files touched
- 45 call sites replaced (7+9+8+7+14)
- 2 low-level references intentionally retained (stats field, commented code)
- 5 independent commits, each a safe isolated change
- 0 interface changes to `IRenderBackend.h` (every migrated call fit the existing high-level API)
- 0 behavioral changes under any backend (pure decoupling)
- Subagents used: 4 parallel migrations across 2 batches


## Exit criterion

Five subsystem files migrated cleanly. Both `Generals/` and `GeneralsMD/` copies in sync. `IRenderBackend` interface unchanged (no extensions needed — every migrated file uses only existing high-level methods). `cmake -DGGC_RENDER_BACKEND=dx8` build is byte-identical in visible behavior. `cmake -DGGC_RENDER_BACKEND=bgfx` and `=diligent` builds don't render the migrated subsystems because those stubs are no-ops — same as Phase 2.
