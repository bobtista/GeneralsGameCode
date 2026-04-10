# Render Backend Phase 3 — Migrate call sites to `g_renderBackend`

**Branch:** `bobtista/refactor/phase3-migrate-callsites`
**Base:** `bobtista/feat/phase2-render-backends` (Phase 2)
**Status:** in progress

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

- [ ] **3.0** Write this document
- [ ] **3.1** Migrate `W3DBibBuffer` (both copies) — pattern reference
- [ ] **3.2** Migrate `W3DDebugIcons` (both copies) — subagent
- [ ] **3.3** Migrate `W3DInGameUI` (both copies) — subagent
- [ ] **3.4** Migrate `W3DTerrainTracks` (Core, shared) — subagent
- [ ] **3.5** Migrate `W3DBridgeBuffer` (both copies) — subagent
- [ ] **3.6** Document completion + Phase 4 handoff

## Exit criterion

Five subsystem files migrated cleanly. Both `Generals/` and `GeneralsMD/` copies in sync. `IRenderBackend` interface unchanged (no extensions needed — every migrated file uses only existing high-level methods). `cmake -DGGC_RENDER_BACKEND=dx8` build is byte-identical in visible behavior. `cmake -DGGC_RENDER_BACKEND=bgfx` and `=diligent` builds don't render the migrated subsystems because those stubs are no-ops — same as Phase 2.
