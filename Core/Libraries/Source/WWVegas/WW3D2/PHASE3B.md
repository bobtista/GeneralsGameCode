# Render Backend Phase 3B — Migrate W3DRoadBuffer + first interface extension

**Branch:** `bobtista/refactor/phase3b-migrate-callsites`
**Base:** `bobtista/refactor/phase3-migrate-callsites` (Phase 3 first batch)
**Status:** in progress

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the multi-phase plan and [PHASE3.md](PHASE3.md) for the Phase 3 scope decision (decoupling-only, no backend runtime changes).

## Goal

Continue Phase 3 subsystem decoupling with:

1. One big clean target: **W3DRoadBuffer** (~3300 LOC, 12 clean calls)
2. First minor **`IRenderBackend` extension**: add typed methods for blend ops, blend factors, and color write mask — enough to unblock W3DStatusCircle's fade effects and FlatHeightMap's shroud-rendering trick
3. Complete the partial Phase 1 migration of **W3DStatusCircle** (remove the 4 remaining `DX8Wrapper::Set_DX8_Render_State` blend-op calls)
4. Migrate **FlatHeightMap** (23 high-level calls + 1 `D3DRS_COLORWRITEENABLE` call, now expressible through the new interface)

## Non-goals

- **Not migrating W3DShroud.** Investigation found it uses `_Create_DX8_Surface`, `_Copy_DX8_Rects`, and `_Get_D3D_Device8()->TestCooperativeLevel()` — this is deeply coupled raw-D3D8 surface management, not simple state setters. Deferred to Phase 3C with a proper surface/blit abstraction.
- **Not touching deeply coupled subsystems** (W3DWater, shadows, W3DShaderManager, W3DDisplay, W3DScene, W3DMouse).
- **Not making bgfx/Diligent actually render.** Same scope decision as Phase 3 (see PHASE3.md's swapchain-ownership explanation).

## Interface extension design

The extension adds three method groups with typed POD enums, following the same style as Phase 1's `TransformKind`:

### Blend op (D3DRS_BLENDOP)

```cpp
enum BlendOp
{
    RB_BLEND_OP_ADD          = 1, // D3DBLENDOP_ADD
    RB_BLEND_OP_SUBTRACT     = 2, // D3DBLENDOP_SUBTRACT
    RB_BLEND_OP_REV_SUBTRACT = 3, // D3DBLENDOP_REVSUBTRACT
    RB_BLEND_OP_MIN          = 4, // D3DBLENDOP_MIN
    RB_BLEND_OP_MAX          = 5  // D3DBLENDOP_MAX
};

virtual void Set_Blend_Op(BlendOp op) = 0;
```

### Blend factors (D3DRS_SRCBLEND / D3DRS_DESTBLEND)

```cpp
enum BlendFactor
{
    RB_BLEND_ZERO            = 1,  // D3DBLEND_ZERO
    RB_BLEND_ONE             = 2,  // D3DBLEND_ONE
    RB_BLEND_SRC_COLOR       = 3,  // D3DBLEND_SRCCOLOR
    RB_BLEND_INV_SRC_COLOR   = 4,  // D3DBLEND_INVSRCCOLOR
    RB_BLEND_SRC_ALPHA       = 5,  // D3DBLEND_SRCALPHA
    RB_BLEND_INV_SRC_ALPHA   = 6,  // D3DBLEND_INVSRCALPHA
    RB_BLEND_DEST_ALPHA      = 7,  // D3DBLEND_DESTALPHA
    RB_BLEND_INV_DEST_ALPHA  = 8,  // D3DBLEND_INVDESTALPHA
    RB_BLEND_DEST_COLOR      = 9,  // D3DBLEND_DESTCOLOR
    RB_BLEND_INV_DEST_COLOR  = 10, // D3DBLEND_INVDESTCOLOR
    RB_BLEND_SRC_ALPHA_SAT   = 11  // D3DBLEND_SRCALPHASAT
};

virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest) = 0;
```

Values intentionally match `D3DBLEND_*` enum values 1-11 so `DX8Backend` can cast directly without a switch. bgfx/Diligent backends translate via their own switch.

### Color write mask (D3DRS_COLORWRITEENABLE)

```cpp
virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha) = 0;
```

Booleans instead of a bitmask enum for clarity at call sites — FlatHeightMap's call site is `R|G|B` only and booleans read cleaner than `RB_COLOR_WRITE_R|RB_COLOR_WRITE_G|RB_COLOR_WRITE_B`.

### Why these three, and why now

The goal of Phase 3B's interface extension is **not** to fully abstract the low-level D3D8 state API — that's a much bigger design exercise. It's to unblock specifically the call sites that have a small, well-understood set of low-level state changes, without growing the interface beyond what's needed.

Three methods, all blend-related, all used exactly once each in the files we're migrating. Minimal growth, maximum value.

### Backend implementations

- **DX8Backend**: straight forwarders to `DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, op)` etc. The enum values match D3D directly so it's a trivial cast.
- **BgfxBackend / DiligentBackend**: Phase 2 stubs (no-ops). The stubs are updated to accept the new methods but don't do anything with them — same approach as every other Phase 2 stub. When Phase 4 cutover happens, these will get real implementations.

## Task list

- [ ] **3B.0** Write this document
- [x] **3B.1** Investigate low-level state needs (W3DShroud is deferred; scope finalized)
- [ ] **3B.2** Migrate `W3DRoadBuffer` (both copies, clean) — subagent
- [ ] **3B.3** Extend `IRenderBackend` with blend + color-write state
- [ ] **3B.4** Complete `W3DStatusCircle` migration using the extension
- [ ] **3B.5** Migrate `FlatHeightMap` using the extension — subagent
- [ ] **3B.6** Document completion + Phase 3C preview

## Exit criterion

Phase 3B ends with:

- 3 additional subsystems migrated (W3DRoadBuffer, W3DStatusCircle fully, FlatHeightMap)
- `IRenderBackend` extended by 3 new virtual methods + 2 new enums
- All three backend implementations updated (DX8 real, bgfx/Diligent stubs)
- Default build (`=dx8`) compiles and runs identically to Phase 3
- `=bgfx` and `=diligent` builds compile; migrated subsystems silently no-op
