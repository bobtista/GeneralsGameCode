# Render Backend Phase 3C — Migrate W3DShroud via existing SurfaceClass abstraction

**Branch:** `bobtista/refactor/phase3c-shroud`
**Base:** `bobtista/refactor/phase3b-migrate-callsites` (Phase 3B)
**Status:** W3DShroud migration complete, pending Windows build verification

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the multi-phase plan, [PHASE3.md](PHASE3.md) and [PHASE3B.md](PHASE3B.md) for previous phases.

## Goal

Migrate `W3DShroud.cpp` (Generals + GeneralsMD copies) off raw `IDirect3DSurface8*` and `_Copy_DX8_Rects` / `_Create_DX8_Surface` / `TestCooperativeLevel` calls onto the existing `SurfaceClass` abstraction.

## The surprising finding that simplified this phase

When I deferred W3DShroud at the end of Phase 3B I expected this phase to need a substantial new `IRenderBackend` extension covering surface creation, blit operations, and device-cooperative-level checks. Investigation in Phase 3C found that almost none of that is necessary because **`SurfaceClass` already exists and already provides the abstraction we need**.

`SurfaceClass` (in `Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.h`) is a refcounted wrapper around `IDirect3DSurface8` with the API:

- Constructor `SurfaceClass(width, height, format)` — wraps `_Create_DX8_Surface` internally
- `Lock(int *pitch)` / `Unlock()` — wraps `LockRect` / `UnlockRect`
- `Copy(dstx, dsty, srcx, srcy, w, h, const SurfaceClass *src)` — wraps `_Copy_DX8_Rects` for full-format-match cases and falls back to `D3DXLoadSurfaceFromSurface` for format/size mismatches
- `Get_Description(SurfaceDescription &out)` — width/height/format
- `Get_Bytes_Per_Pixel()`

This is already a backend-neutral abstraction. The problem with `W3DShroud` isn't that `IRenderBackend` lacks the right methods — it's that `W3DShroud` was written before `SurfaceClass` existed (or independently of it) and uses the raw `IDirect3DSurface8*` API directly. The fix is to **refactor `W3DShroud` to use `SurfaceClass` internally**, not to extend `IRenderBackend`.

For the device-cooperative-level check (`DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel() != D3D_OK`), the existing `IRenderBackend::Is_Device_Lost()` method covers the same use case: "skip rendering this frame if the device isn't ready." I'll use that with inverted polarity.

**Net result for Phase 3C: zero new `IRenderBackend` methods, one substantial subsystem refactor.**

## Refactor plan

### Header change

`W3DShroud.h` currently has:

```cpp
IDirect3DSurface8 *m_pSrcTexture;  // stores sysmem copy of visible shroud.
```

Change to:

```cpp
SurfaceClass *m_pSrcTexture;  // stores sysmem copy of visible shroud.
```

Add a forward declaration `class SurfaceClass;` if it's not already present.

### Surface allocation

```cpp
m_pSrcTexture = DX8Wrapper::_Create_DX8_Surface(srcWidth, srcHeight, WW3D_FORMAT_R5G6B5);
```

becomes:

```cpp
m_pSrcTexture = new SurfaceClass(srcWidth, srcHeight, WW3D_FORMAT_R5G6B5);
```

`SurfaceClass`'s constructor that takes width/height/format calls `_Create_DX8_Surface` internally, so this is functionally identical.

### Lock/Unlock pattern

```cpp
D3DLOCKED_RECT rect;
HRESULT res = m_pSrcTexture->LockRect(&rect, nullptr, D3DLOCK_NO_DIRTY_UPDATE);
m_pSrcTexture->UnlockRect();
m_srcTextureData = rect.pBits;
m_srcTexturePitch = rect.Pitch;
```

becomes:

```cpp
int pitch = 0;
m_srcTextureData = m_pSrcTexture->Lock(&pitch);
m_pSrcTexture->Unlock();
m_srcTexturePitch = static_cast<UnsignedInt>(pitch);
```

The "lock-then-unlock-then-keep-pointer" pattern is preserved exactly. The existing code relies on `_Create_DX8_Surface` allocating system-memory surfaces whose pixel buffers stay alive after unlock. `SurfaceClass`'s constructor uses the same `_Create_DX8_Surface` underneath, so the same trick works.

(This pattern is dubious but it's what the existing code does. Phase 4 or later may switch to a CPU-backed pixel buffer that doesn't depend on this DX8 quirk. Out of scope for Phase 3C.)

### Release pattern

```cpp
if (m_pSrcTexture)
{
    m_pSrcTexture->Release();
    m_pSrcTexture = nullptr;
}
```

becomes:

```cpp
REF_PTR_RELEASE(m_pSrcTexture);
```

`SurfaceClass` is a refcounted class (extends `RefCountClass`); the project's standard release macro handles both nullification and the refcount drop.

### CopyRects calls

```cpp
DX8Wrapper::_Copy_DX8_Rects(
    m_pSrcTexture,
    &srcRect,
    1,
    pDestSurface->Peek_D3D_Surface(),
    &dstPoint);
```

becomes:

```cpp
pDestSurface->Copy(
    dstPoint.x, dstPoint.y,
    srcRect.left, srcRect.top,
    srcRect.right - srcRect.left,
    srcRect.bottom - srcRect.top,
    m_pSrcTexture);
```

`SurfaceClass::Copy(dstx, dsty, srcx, srcy, w, h, src)` is the direct equivalent. There are 3 of these call sites per file (in `fillBorderShroudData()` and `render()`). I verified `SurfaceClass::Copy` doesn't internally Lock/Unlock either surface, so it doesn't conflict with the persistent lock we hold on `m_pSrcTexture`.

### Device-cooperative-level check

```cpp
if (DX8Wrapper::_Get_D3D_Device8() && (DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel()) != D3D_OK)
    return; // device not ready to render anything
```

becomes:

```cpp
if (g_renderBackend->Is_Device_Lost())
    return; // device not ready to render anything
```

The semantics are slightly different. The existing code calls `TestCooperativeLevel()` directly which returns `D3DERR_DEVICELOST` or `D3DERR_DEVICENOTRESET` on a lost device. `Is_Device_Lost()` checks the cached `IsDeviceLost` flag inside `DX8Wrapper`. In practice the flag is updated by `DX8Wrapper`'s frame loop during normal operation, so by the time `W3DShroud::render()` runs the flag should reflect reality. If this turns out to cause issues during device-loss recovery, we can add an explicit `Is_Device_Cooperative()` method to `IRenderBackend` later — but `Is_Device_Lost()` covers the use case we care about (skip rendering when the device isn't ready).

## In scope

- `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DShroud.{h,cpp}`
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DShroud.{h,cpp}`

## Out of scope

- Refactoring `m_srcTextureData` to be a proper CPU buffer (would eliminate the lock-trick dependency but is a much larger change)
- Adding new `IRenderBackend` methods (none needed)
- Other shroud-adjacent files
- Touching the rendering of the shroud-as-blended-texture in the terrain/object passes (those are downstream of `m_pDstTexture`, which is already a `TextureClass`)

## Task list

- [x] **3C.0** Write this document
- [x] **3C.1** Refactor `W3DShroud.h` (both copies) — change `m_pSrcTexture` type
- [x] **3C.2** Refactor `W3DShroud.cpp` (both copies) — surface management migration
- [x] **3C.3** Document completion + cumulative update

## What landed

Four files touched, all in two commits:

| File | Change |
|---|---|
| `Generals/.../W3DShroud.h` | `IDirect3DSurface8*` → `SurfaceClass*`, added `class SurfaceClass` forward declaration |
| `GeneralsMD/.../W3DShroud.h` | Same |
| `Generals/.../W3DShroud.cpp` | Surface management refactor: 5 sites |
| `GeneralsMD/.../W3DShroud.cpp` | Same |

Per-file refactor sites in `W3DShroud.cpp`:

1. **Includes**: added `#include "WW3D2/RenderBackend.h"` and `#include "WW3D2/surfaceclass.h"`
2. **Allocation** in `init()`: `_Create_DX8_Surface(w, h, fmt)` → `new SurfaceClass(w, h, fmt)`
3. **Lock pattern** in `init()`: `LockRect(&rect, nullptr, D3DLOCK_NO_DIRTY_UPDATE); UnlockRect();` → `Lock(&pitch); Unlock();` via SurfaceClass API. The "lock-then-unlock-then-keep-pointer" trick is preserved exactly because `SurfaceClass::Lock` is a thin wrapper over `LockRect` and the underlying system-memory backing stays alive after unlock.
4. **Destructor** + `reset()` + `ReleaseResources()`: `if (m_pSrcTexture) m_pSrcTexture->Release()` → `REF_PTR_RELEASE(m_pSrcTexture)`
5. **CopyRects** in `fillBorderShroudData()` (2 calls) and `render()` (1 call): `DX8Wrapper::_Copy_DX8_Rects(src, &srcRect, 1, dest->Peek_D3D_Surface(), &dstPoint)` → `pDestSurface->Copy(dstx, dsty, srcx, srcy, w, h, m_pSrcTexture)`. The `SurfaceClass::Copy` API takes the same source rect + destination point that `_Copy_DX8_Rects` did, just with a different argument order.
6. **Device-cooperative-level check** in `render()`: `DX8Wrapper::_Get_D3D_Device8() && DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel() != D3D_OK` → `g_renderBackend->Is_Device_Lost()`. The semantic match is "skip rendering if the device isn't ready right now."

After the refactor, `W3DShroud.cpp` has **zero functional references to `DX8Wrapper::`, `_Create_DX8_Surface`, `_Copy_DX8_Rects`, `TestCooperativeLevel`, `_Get_D3D_Device8`, or `IDirect3DSurface8`**. The 4 remaining matches per file are all inside doc-comment text that mentions those names by reference.

### Commits

```
25e7650a6 refactor(ww3d): migrate W3DShroud surface management to SurfaceClass
4d5252bfd refactor(ww3d): change W3DShroud m_pSrcTexture type to SurfaceClass*
7afcf3d71 docs(ww3d2): add Phase 3C plan for W3DShroud SurfaceClass refactor
```

### Statistics

- 1 subsystem migrated (W3DShroud, both Generals and GeneralsMD copies)
- 4 files touched (2 .h, 2 .cpp)
- ~12 raw D3D8 sites replaced (3 surface allocations, 3 surface releases, 3 CopyRects, 1 Lock/Unlock pair, 1 cooperative-level check, multiplied across the two copies)
- 0 new `IRenderBackend` methods (the existing `SurfaceClass` already provided everything except the device-lost check, which `Is_Device_Lost()` covered)
- 3 commits

## Cumulative phase 3 progress

Across Phase 3, 3B, and 3C combined, **9 subsystems are now routed off direct `DX8Wrapper::` static calls**:

1. W3DBibBuffer (Phase 3)
2. W3DDebugIcons (Phase 3)
3. W3DInGameUI (Phase 3)
4. W3DTerrainTracks (Phase 3)
5. W3DBridgeBuffer (Phase 3)
6. W3DRoadBuffer (Phase 3B)
7. W3DStatusCircle (Phase 1 + Phase 3B)
8. FlatHeightMap (Phase 3B)
9. W3DShroud (Phase 3C)

`IRenderBackend` has grown from its Phase 1 baseline by 3 methods + 2 enums (Phase 3B's blend state extension). No new methods were needed for Phase 3C — `SurfaceClass` and `Is_Device_Lost()` were already enough.

## What's still to migrate

**Mid-tier (need modest interface extensions):**

- **W3DWater.cpp** — 47 raw `D3DRS_*` / `D3DTSS_*` calls. The biggest single piece of remaining work. Needs the texture-stage-state API abstracted as a small group of methods (or as a single `Set_Texture_Stage_State_Bundle` method that takes a struct).
- **W3DVolumetricShadow.cpp / W3DProjectedShadow.cpp** — stencil volume shadows with raw `D3DRS_STENCIL*`. Needs a `StencilState` group on `IRenderBackend`. Probably 5-7 new methods.

**Heavy-tier (significant interface and code work):**

- **W3DShaderManager.cpp** — central shader + post-processing manager, touches raw `IDirect3DDevice8` for render-to-texture setup. Large file, critical to multiple subsystems.
- **W3DDisplay.cpp** — main display context manager, device lifecycle management. Probably the last subsystem to migrate before Phase 4's cutover.
- **W3DScene.cpp** — scene graph orchestrator, 99 calls. Mostly mechanical but big.
- **W3DMouse.cpp** — uses `_Get_D3D_Device8` directly for `IDirect3DDevice8::ShowCursor` / `SetCursorProperties`. Needs a cursor API on `IRenderBackend`.

## What this changes at runtime

Same pattern as every other Phase 3 migration: **nothing changes under any backend.**

- Under `=dx8`: `SurfaceClass(w,h,fmt)` calls `_Create_DX8_Surface` internally; `SurfaceClass::Copy` calls `_Copy_DX8_Rects` internally; `Is_Device_Lost()` returns the same flag the existing `TestCooperativeLevel()` check would have set. Zero behavior change.
- Under `=bgfx` / `=diligent`: SurfaceClass is still wrapping a real DX8 surface (because `Init_Render_Backend` doesn't matter — `DX8Wrapper::Init` is what allocates the device and surface). The shroud system continues to work exactly as before because the underlying calls still go through `_Create_DX8_Surface` etc, just by a slightly more abstract path.

This is a clean architectural improvement that doesn't unblock Phase 4 by itself — the swapchain ownership problem still applies — but it does eliminate one of the most coupled raw-D3D8 subsystems from the engine, which makes the eventual Phase 4 cutover cleaner.

## Windows build verification needed

This branch is unbuilt. Risks specific to Phase 3C:

1. **The lock-then-unlock-then-keep-pointer trick.** The whole shroud system relies on the assumption that the sysmem surface backing stays alive after Unlock. This is true for DX8's `_Create_DX8_Surface` (the surface is system memory and the pixel buffer is owned by the surface, not the lock), and `SurfaceClass::Lock/Unlock` is a thin wrapper, so the trick should still work. But it's a sketchy enough pattern that it's worth verifying on first Windows run by checking that the fog-of-war and shroud actually render correctly during a skirmish match.
2. **`SurfaceClass::Copy` for full-format-match cases.** I verified by reading `surfaceclass.cpp:436-479` that when src/dest formats match and src is the same size as dst, it calls `_Copy_DX8_Rects` directly. When formats or sizes differ it falls back to `D3DXLoadSurfaceFromSurface` which is a different API path and might have edge cases. The shroud copies shouldn't hit the fallback path because the surface formats match between src and dest, but it's worth confirming visually.
3. **`Is_Device_Lost()` semantics.** The original code called `TestCooperativeLevel()` directly, which forces the driver to report current state. `Is_Device_Lost()` returns a cached flag updated by `DX8Wrapper`'s frame loop. There may be a one-frame delay during device-loss recovery where the new check returns the wrong answer. Worst case: one frame of bad shroud rendering during alt-tab. Acceptable trade-off, easy to revisit if it causes visible flicker.
4. **Pre-existing divergence between Generals and GeneralsMD.** The diff between the two files was 40 lines before my changes (the existing pre-Phase-3C divergence in dispatchEvent and similar bits). It should still be 40 lines after my changes since I applied identical edits to both. If the diff grew, I missed something in one copy.

Test plan for Windows:

1. Default `=dx8` build, start a skirmish, verify shroud is rendering correctly (you should see fog-of-war over unexplored terrain). Unit movement should reveal terrain. This is the regression test.
2. Toggle fog-of-war via `-DRTS_DEBUG=ON` and the in-game cheat — should still work.
3. Alt-tab during gameplay and back. Shroud should still render after alt-tab returns.
4. `=bgfx` and `=diligent` builds should compile and link cleanly. At runtime, the shroud will appear because DX8 is still doing all the rendering — Phase 3C doesn't change that.

## Phase 3D preview

The natural next target depends on what level of interface extension you want to invest in:

**Option A: W3DMouse** — only 3 low-level calls (`_Get_D3D_Device8` → `IDirect3DDevice8::ShowCursor` and `SetCursorProperties`). Adding `Set_Mouse_Cursor(image, hotspot)` and `Show_Mouse_Cursor(bool)` to `IRenderBackend` would cover them. Small but moderately interesting. ~600 LOC file.

**Option B: Volumetric / Projected shadows** — needs a `StencilState` extension group of ~5-7 methods, then ~140 stencil-touching call sites migrate. More interface work, more migration value, two related subsystems migrated together.

**Option C: W3DWater** — the biggest single prize but the most expensive. Needs a `TextureStageState` extension that's a real design exercise (D3DTSS has 30+ states). Probably 1-2 sessions of work just for the interface design before any migration starts.

My recommendation: **W3DMouse next** because it's small, isolated, and unblocks one of the more annoying coupling sites (`_Get_D3D_Device8` access from outside WW3D2). After that, the shadows are the highest leverage target — same interface design unlocks two large subsystems.


## Exit criterion

`W3DShroud.cpp` in both copies has zero direct references to `IDirect3DSurface8`, `_Create_DX8_Surface`, `_Copy_DX8_Rects`, `_Get_D3D_Device8`, or `TestCooperativeLevel`. The shroud system uses only `SurfaceClass` (already in WW3D2) and `g_renderBackend` (the new abstraction). Zero new `IRenderBackend` methods.
