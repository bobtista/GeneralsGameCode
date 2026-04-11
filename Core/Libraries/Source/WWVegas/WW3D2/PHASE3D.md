# Render Backend Phase 3D — Migrate W3DMouse via cursor API extension

**Branch:** `bobtista/refactor/phase3d-mouse`
**Base:** `bobtista/refactor/phase3c-shroud` (Phase 3C)
**Status:** in progress

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

- [ ] **3D.0** Write this document
- [ ] **3D.1** Extend `IRenderBackend` with cursor API + implementations in all 3 backends
- [ ] **3D.2** Migrate `W3DMouse.cpp`
- [ ] **3D.3** Document completion + cumulative update

## Exit criterion

`W3DMouse.cpp` has zero functional references to `_Get_D3D_Device8`, `LPDIRECT3DDEVICE8`, or `IDirect3DDevice8`. The cursor system uses only the new `IRenderBackend` cursor methods and the existing `SurfaceClass` for surface storage. The 4 raw device sites become 4 calls through `g_renderBackend->`.

Default `=dx8` build still renders the cursor identically to today's behavior. `=bgfx` and `=diligent` builds compile, but the cursor silently doesn't render (stubs no-op) — same as every other Phase 3 migration's behavior under non-DX8 backends.
