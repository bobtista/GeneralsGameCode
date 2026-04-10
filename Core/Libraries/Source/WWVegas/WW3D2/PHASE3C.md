# Render Backend Phase 3C — Migrate W3DShroud via existing SurfaceClass abstraction

**Branch:** `bobtista/refactor/phase3c-shroud`
**Base:** `bobtista/refactor/phase3b-migrate-callsites` (Phase 3B)
**Status:** in progress

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

- [ ] **3C.0** Write this document
- [ ] **3C.1** Refactor `W3DShroud.h` (both copies) — change `m_pSrcTexture` type
- [ ] **3C.2** Refactor `W3DShroud.cpp` (both copies) — surface management migration
- [ ] **3C.3** Document completion + cumulative update

## Exit criterion

`W3DShroud.cpp` in both copies has zero direct references to `IDirect3DSurface8`, `_Create_DX8_Surface`, `_Copy_DX8_Rects`, `_Get_D3D_Device8`, or `TestCooperativeLevel`. The shroud system uses only `SurfaceClass` (already in WW3D2) and `g_renderBackend` (the new abstraction). Zero new `IRenderBackend` methods.
