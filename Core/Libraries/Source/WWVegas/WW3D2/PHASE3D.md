# Render Backend Phase 3D — Migrate W3DMouse via cursor API extension

**Branch:** `bobtista/refactor/phase3d-mouse`
**Base:** `bobtista/refactor/phase3c-shroud` (Phase 3C)
**Status:** W3DMouse migration complete, pending Windows build verification

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the multi-phase plan and [PHASE3.md](PHASE3.md), [PHASE3B.md](PHASE3B.md), [PHASE3C.md](PHASE3C.md) for previous Phase 3 sessions.

## Goal

Migrate `W3DMouse.cpp` (single Core file, no Generals/GeneralsMD mirror) off direct `IDirect3DDevice8` cursor API calls onto a small new `IRenderBackend` cursor extension.

## Why W3DMouse is the right next target

It's the smallest "leaky" file remaining in the engine. Only ~600 LOC, only 4 raw `_Get_D3D_Device8()` access sites, all of them clustered around the same 3 D3D8 cursor APIs:

- `IDirect3DDevice8::ShowCursor(BOOL)` — show or hide the hardware cursor
- `IDirect3DDevice8::SetCursorProperties(hsx, hsy, IDirect3DSurface8*)` — set cursor image + hot-spot
- `IDirect3DDevice8::SetCursorPosition(x, y, flags)` — move the cursor (used in fullscreen mode where Windows doesn't track the cursor itself)

These three D3D8 methods correspond to one logical concept: "the hardware cursor." A clean abstraction adds three matching methods to `IRenderBackend`. No texture stage state, no render state, no stencil ops — just three small focused methods.

W3DMouse already uses `SurfaceClass*` for its cursor surface storage (verified at `W3DMouse.h:79`), so the surface side is already abstracted. The only raw-D3D8 leak is the device call to consume those surfaces. Phase 3D closes that leak.

## Interface extension design

Three new virtual methods added to `IRenderBackend`:

```cpp
// Show or hide the hardware cursor managed by the underlying device.
virtual void Show_Hardware_Cursor(bool show) = 0;

// Set the hardware cursor's image and hot spot. The surface must remain
// valid for as long as the cursor is shown — the backend may reference it
// rather than copy it.
virtual void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, SurfaceClass * surface) = 0;

// Manually set the hardware cursor position in client window coordinates.
// Only needed in fullscreen mode where Windows does not track the cursor.
// In windowed mode, Windows owns cursor positioning and this is a no-op.
virtual void Set_Hardware_Cursor_Position(int x, int y) = 0;
```

`SurfaceClass` is forward-declared in `IRenderBackend.h` already (added in Phase 1) so no new include is needed in the public header.

### Backend implementations

- **DX8Backend**:
  - `Show_Hardware_Cursor(show)` → `DX8Wrapper::_Get_D3D_Device8()->ShowCursor(show ? TRUE : FALSE)`
  - `Set_Hardware_Cursor_Image(hsx, hsy, surface)` → `DX8Wrapper::_Get_D3D_Device8()->SetCursorProperties(hsx, hsy, surface->Peek_D3D_Surface())`
  - `Set_Hardware_Cursor_Position(x, y)` → `DX8Wrapper::_Get_D3D_Device8()->SetCursorPosition(x, y, D3DCURSOR_IMMEDIATE_UPDATE)`
  - All three guard against null device pointers (the existing W3DMouse code does this).
- **BgfxBackend / DiligentBackend**: stub no-ops, same pattern as every other Phase 2 stub. Hardware cursor support is a Phase 4+ concern for non-DX8 backends.

## Migration sites in W3DMouse.cpp

5 raw `_Get_D3D_Device8()` access sites + 4 `m_pDev->X` calls cluster across 3 functions:

| Site | Function | Original | After |
|---|---|---|---|
| ~111 | `~W3DMouse()` | `m_pDev->ShowCursor(FALSE)` | `g_renderBackend->Show_Hardware_Cursor(false)` |
| ~392, 397 | `setCursor(RM_DX8 branch)` | `m_pDev->ShowCursor(FALSE)` | same pattern |
| ~417, 418 | same | `m_pDev->SetCursorProperties(...); m_pDev->ShowCursor(TRUE);` | `g_renderBackend->Set_Hardware_Cursor_Image(...); g_renderBackend->Show_Hardware_Cursor(true);` |
| ~488, 490 | `draw(RM_DX8 branch)` | `m_pDev->ShowCursor(TRUE)` | same pattern |
| ~498 | same | `m_pDev->SetCursorPosition(x, y, D3DCURSOR_IMMEDIATE_UPDATE)` | `g_renderBackend->Set_Hardware_Cursor_Position(x, y)` |
| ~511 | same | `m_pDev->SetCursorProperties(...)` | same pattern |

The local `LPDIRECT3DDEVICE8 m_pDev = DX8Wrapper::_Get_D3D_Device8();` declarations get removed entirely. The null-check guards in the original code (`if (m_pDev != nullptr)`) become guards on `g_renderBackend != nullptr` — except `g_renderBackend` is always non-null between `Init_Render_Backend` and `Shutdown_Render_Backend`, so the guard can be dropped or kept defensively. I'll keep it as a defensive null check matching the original style.

Also: `D3DCURSOR_IMMEDIATE_UPDATE` is the only flag the original code passed and it's the only sensible choice. Phase 3D's `Set_Hardware_Cursor_Position` doesn't expose a flags parameter — it always uses immediate update. If a backend needs deferred update later we can extend the API.

## In scope

- `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h` — add 3 virtual methods
- `Core/Libraries/Source/WWVegas/WW3D2/DX8Backend.{h,cpp}` — real forwarders
- `Core/Libraries/Source/WWVegas/WW3D2/BgfxBackend.{h,cpp}` — stub no-ops
- `Core/Libraries/Source/WWVegas/WW3D2/DiligentBackend.{h,cpp}` — stub no-ops
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DMouse.cpp` — migrate the 4 sites

## Out of scope

- Generals/GeneralsMD mirrors of W3DMouse — there aren't any. It's only in the shared Core tree.
- Any other cursor-related files (`Win32Mouse`, `Mouse`, etc.). They handle Windows-side cursor logic and don't touch D3D directly.
- Deferred-update cursor positioning (always immediate).
- Other `_Get_D3D_Device8` callers in the engine. Those are addressed in their own phases.

## Task list

- [x] **3D.0** Write this document
- [x] **3D.1** Extend `IRenderBackend` with cursor API + implementations in all 3 backends
- [x] **3D.2** Migrate `W3DMouse.cpp`
- [x] **3D.3** Document completion + cumulative update

## What landed

### Interface extension (commit `d94d046a4`)

Three new virtual methods on `IRenderBackend`:

```cpp
virtual void Show_Hardware_Cursor(bool show) = 0;
virtual void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, SurfaceClass * surface) = 0;
virtual void Set_Hardware_Cursor_Position(int x, int y) = 0;
```

`SurfaceClass` was already forward-declared in `IRenderBackend.h` from Phase 1, so no new include was needed in the public header.

Backend implementations:

- **DX8Backend**: real forwarders to `IDirect3DDevice8::ShowCursor / SetCursorProperties / SetCursorPosition`. Each guards against null device pointer (defensive against device-loss windows). `Set_Hardware_Cursor_Position` always passes `D3DCURSOR_IMMEDIATE_UPDATE` since that's the only flag the original code used.
- **BgfxBackend / DiligentBackend**: stub no-ops, same pattern as every other Phase 2/3 stub. Hardware cursor support for non-DX8 backends is a Phase 4+ concern.

### Bonus fix: missing Phase 3B header declarations

While adding the cursor methods to `DX8Backend.h` I discovered that the Phase 3B blend extension methods (`Set_Blend_Op`, `Set_Blend_Factors`, `Set_Color_Write_Enable`) had been added to the **base class** `IRenderBackend` and to the **`.cpp` implementation** of `DX8Backend`, but **not to `DX8Backend.h`**. This is a Phase 3B regression I missed in earlier review.

Without those declarations in `DX8Backend.h`, `DX8Backend` would technically remain abstract because it didn't override the pure virtual methods, and `new DX8Backend()` in `Init_Render_Backend` would fail to compile under MSVC with "cannot instantiate abstract class." This is exactly the kind of bug Windows build verification catches that macOS clang LSP misses.

I rolled the fix into the same Phase 3D.1 commit (`d94d046a4`). DX8Backend.h now correctly declares all 3 Phase 3B blend methods AND the 3 new Phase 3D cursor methods.

`BgfxBackend.h` and `DiligentBackend.h` both already had the Phase 3B blend methods declared correctly — only `DX8Backend.h` was missing them. Lesson learned: when adding a new method to the base, double-check that all subclasses received both the header AND .cpp updates.

### W3DMouse migration (commit `9db2647aa`)

Single Core file, no Generals/MD mirrors. Three functions migrated:

1. **`~W3DMouse()` (~line 111)** — `m_pDev->ShowCursor(FALSE)` → `g_renderBackend->Show_Hardware_Cursor(false)`. Added defensive `g_renderBackend != nullptr` guard against unusual destruction orderings (in normal shutdown the backend is still alive when `~W3DMouse` runs, but defensive doesn't hurt).
2. **`setCursor()` (~line 391)** — the RM_DX8 branch had `LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8()` followed by 3 device method calls. Replaced with `g_renderBackend` cursor calls. The local `m_pDev` variable is gone.
3. **`draw()` (~line 491)** — same pattern, RM_DX8 branch with cursor positioning + animation. Replaced with `g_renderBackend->Set_Hardware_Cursor_Position` and `Set_Hardware_Cursor_Image`. Local `m_pDev` removed.

Verification:

```bash
$ grep -n "DX8Wrapper::\|_Get_D3D_Device8\|LPDIRECT3DDEVICE8\|IDirect3DDevice8" W3DMouse.cpp
114:	// of touching IDirect3DDevice8 directly. See PHASE3D.md. Null guard
397:		// previous code grabbed IDirect3DDevice8 directly via DX8Wrapper.
```

Both remaining matches are in the new migration comment text — **zero functional references**.

### Commits

```
9db2647aa refactor(ww3d): route W3DMouse hardware cursor calls through g_renderBackend
d94d046a4 feat(ww3d2): extend IRenderBackend with hardware cursor API and add missing Phase 3B header decls
cf7f495a6 docs(ww3d2): add Phase 3D plan for W3DMouse cursor API migration
```

### Statistics

- 1 subsystem migrated (W3DMouse, single Core file)
- 7 files touched (1 W3DMouse.cpp + IRenderBackend.h + 3 backend headers + 3 backend cpps)
- 4 raw IDirect3DDevice8 device calls replaced with cursor abstraction calls
- 3 new virtual methods on IRenderBackend (cursor extension)
- 3 missing Phase 3B header declarations recovered (DX8Backend.h regression fix)
- 3 commits

## Cumulative phase 3 progress

Across Phase 3, 3B, 3C, and 3D combined, **10 subsystems are now routed off direct `DX8Wrapper::*` static calls**:

1. W3DBibBuffer (Phase 3)
2. W3DDebugIcons (Phase 3)
3. W3DInGameUI (Phase 3)
4. W3DTerrainTracks (Phase 3)
5. W3DBridgeBuffer (Phase 3)
6. W3DRoadBuffer (Phase 3B)
7. W3DStatusCircle (Phase 1 + 3B)
8. FlatHeightMap (Phase 3B)
9. W3DShroud (Phase 3C)
10. **W3DMouse (Phase 3D)**

`IRenderBackend` has grown by 6 methods + 2 enums beyond Phase 1 baseline:
- Phase 3B: `Set_Blend_Op`, `Set_Blend_Factors`, `Set_Color_Write_Enable` + `BlendOp` and `BlendFactor` enums
- Phase 3D: `Show_Hardware_Cursor`, `Set_Hardware_Cursor_Image`, `Set_Hardware_Cursor_Position`

## What's left

**Mid-tier (need modest interface extensions):**

- **W3DVolumetricShadow / W3DProjectedShadow** — stencil volume shadows with raw `D3DRS_STENCIL*`. Single `StencilState` extension group of ~5-7 methods unlocks both. ~140 stencil sites total.

**Heavy-tier:**

- **W3DWater** — 47 raw `D3DRS_*` / `D3DTSS_*` calls. Needs the texture-stage-state API designed.
- **W3DShaderManager** — central shader + post-processing manager.
- **W3DDisplay** — main display context, device lifecycle.
- **W3DScene** — scene graph orchestrator, 99 calls.

## What this changes at runtime

Same pattern as every Phase 3 migration: **nothing changes under any backend**. Under `=dx8` the cursor renders identically. Under `=bgfx` and `=diligent` the cursor silently doesn't appear because the stub methods no-op — that's expected, and is the correct intermediate behavior until Phase 4's cutover.

## Windows build verification needed

This is the second consecutive phase that's exposed a likely Windows-only issue:

1. **The `DX8Backend.h` regression from Phase 3B** is now fixed. Without this Phase 3D commit, the Phase 3B branch would not compile under MSVC because `DX8Backend` would be abstract. This means **Phase 3B's Windows build was broken** and we just didn't know it. Anyone building Phase 3B (or anything based on it) on Windows would have hit "cannot instantiate abstract class DX8Backend" — blocking all phases. Phase 3D fixes that as a side-effect.
2. **W3DMouse cursor rendering**. The migration is mechanical and the DX8Backend forwarders are 1:1 translations of the original code, but it's worth visually confirming on Windows: launch the game, verify the mouse cursor renders correctly in windowed mode AND in fullscreen mode, verify it animates (some Generals cursors are animated), verify it changes when you hover over different game objects.

Phase 3D test plan:

1. Default `=dx8` build, launch game, verify cursor renders in skirmish.
2. Toggle fullscreen and verify cursor still tracks mouse position correctly (the `SetCursorPosition` path only runs in fullscreen).
3. Hover over various unit types to trigger cursor changes (animated cursors).
4. Make sure quitting the game doesn't crash on `~W3DMouse()`.
5. `=bgfx` and `=diligent` builds should compile cleanly. The cursor will silently not render under those backends — same as every other Phase 3 migration's behavior.

## Phase 3E preview

Logical next targets:

- **W3DVolumetricShadow + W3DProjectedShadow** — needs stencil state abstraction. Two related subsystems migrated together. Probably the highest-leverage next phase given that one extension group unlocks ~140 call sites across two files.
- **W3DWater** — biggest single piece of remaining work. Would need a careful design exercise for the texture-stage-state API.

My recommendation: **shadows next**. The stencil state design is well-bounded (D3DRS_STENCILENABLE, STENCILFUNC, STENCILREF, STENCILMASK, STENCILWRITEMASK, STENCILZFAIL, STENCILFAIL, STENCILPASS, plus the ZBIAS-related ones), and migrating two subsystems with one extension is good leverage.


## Exit criterion

`W3DMouse.cpp` has zero functional references to `_Get_D3D_Device8`, `LPDIRECT3DDEVICE8`, or `IDirect3DDevice8`. The cursor system uses only the new `IRenderBackend` cursor methods and the existing `SurfaceClass` for surface storage. The 4 raw device sites become 4 calls through `g_renderBackend->`.

Default `=dx8` build still renders the cursor identically to today's behavior. `=bgfx` and `=diligent` builds compile, but the cursor silently doesn't render (stubs no-op) — same as every other Phase 3 migration's behavior under non-DX8 backends.
