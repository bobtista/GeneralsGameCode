# Render Backend Phase 3B — Migrate W3DRoadBuffer + first interface extension

**Branch:** `bobtista/refactor/phase3b-migrate-callsites`
**Base:** `bobtista/refactor/phase3-migrate-callsites` (Phase 3 first batch)
**Status:** migrations complete, pending Windows build verification

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

- [x] **3B.0** Write this document
- [x] **3B.1** Investigate low-level state needs (W3DShroud is deferred; scope finalized)
- [x] **3B.2** Migrate `W3DRoadBuffer` (both copies, clean) — subagent
- [x] **3B.3** Extend `IRenderBackend` with blend + color-write state
- [x] **3B.4** Complete `W3DStatusCircle` migration using the extension
- [x] **3B.5** Migrate `FlatHeightMap` using the extension — subagent
- [x] **3B.6** Document completion + Phase 3C preview

## What landed in Phase 3B

### Subsystems migrated

| Subsystem | Files | DX8Wrapper calls before/after | Notes |
|---|---:|---|---|
| W3DRoadBuffer | 2 (Generals + ZH) | 12/11 → 1/1 | Road network rendering. The remaining reference in each file is a commented-out debug line inside `#ifdef RTS_DEBUG`. |
| W3DStatusCircle (complete) | 2 (Generals + ZH) | 4/4 → 0/0 | Phase 1 started this migration; Phase 3B finished it by routing the 4 fade-effect `D3DRS_BLENDOP` / `D3DRS_SRCBLEND` / `D3DRS_DESTBLEND` calls through the new blend API. Zero `DX8Wrapper::` references now. |
| FlatHeightMap | 1 (Core, shared) | 23 → 1 | Flat heightmap terrain rendering. The remaining reference is `DX8Wrapper::stats.m_disableTerrain`, a static debug-stats field access inside a runtime debug branch — not a method call, correctly left unchanged. The `D3DRS_COLORWRITEENABLE` call was migrated to `Set_Color_Write_Enable`, and the previously-stale `DX8Wrapper::getBackBufferFormat()` call was also updated to `g_renderBackend->Get_Back_Buffer_Format()` as a bonus. |

### IRenderBackend extension

Three new virtual methods + two new POD enums in `IRenderBackend.h`:

- `enum BlendOp` with values `RB_BLEND_OP_{ADD, SUBTRACT, REV_SUBTRACT, MIN, MAX}`, numerically matching `D3DBLENDOP_*` 1-5.
- `enum BlendFactor` with 11 values matching `D3DBLEND_*` 1-11.
- `virtual void Set_Blend_Op(BlendOp op) = 0;`
- `virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest) = 0;`
- `virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha) = 0;`

**Backend implementations:**

- `DX8Backend` — real forwarders to `DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP / _SRCBLEND / _DESTBLEND / _COLORWRITEENABLE, ...)`. The enum values cast directly to D3D constants since they're numerically identical.
- `BgfxBackend` — no-op stubs, same pattern as every other Phase 2 stub.
- `DiligentBackend` — no-op stubs.

### Commits on this branch

```
965910d4c refactor(ww3d): route FlatHeightMap render calls through g_renderBackend
9b2cf8ba5 docs(ww3d): update W3DStatusCircle Phase 1 comment now that Phase 3B completed it
962926dd1 refactor(ww3d): complete W3DStatusCircle migration via new blend state API
71fc5cf7d feat(ww3d2): extend IRenderBackend with blend op/factor and color write state
cbd1ea814 refactor(ww3d): route W3DRoadBuffer render calls through g_renderBackend
7760895c6 docs(ww3d2): add Phase 3B plan + interface extension design
```

## Session statistics

- 3 subsystems migrated (W3DRoadBuffer, W3DStatusCircle completed, FlatHeightMap)
- 5 files touched
- 40 call sites replaced across the migrations (12 + 11 + 4 + 4 + 23 − 2 for the expected remainders)
- 1 interface extension with 3 virtual methods and 2 enums
- 4 backend files updated (IRenderBackend header, DX8Backend, BgfxBackend, DiligentBackend — all consistent)
- 6 independent commits

## Cumulative Phase 3 progress

Across Phase 3 + Phase 3B, these subsystems are now routed through `g_renderBackend`:

1. W3DBibBuffer (Phase 3) — building bibs
2. W3DDebugIcons (Phase 3) — debug visualization icons
3. W3DInGameUI (Phase 3) — debug hint overlay (`RTS_DEBUG` only)
4. W3DTerrainTracks (Phase 3) — vehicle track marks
5. W3DBridgeBuffer (Phase 3) — bridge rendering
6. W3DRoadBuffer (Phase 3B) — road network
7. W3DStatusCircle (Phase 1 + Phase 3B) — team-dot marker and fade overlays
8. FlatHeightMap (Phase 3B) — flat terrain rendering

Plus the partial Phase 1 migration of `W3DStatusCircle` which was completed in Phase 3B.

**8 subsystems, ~80 call sites migrated total across all sessions.** The engine still has many more `DX8Wrapper::*` statics in unmigrated files, but the pattern and infrastructure are proven and the remaining migrations are mechanical.

## Remaining migration candidates for Phase 3C

**Need interface extensions before migration can begin:**

- `W3DShroud.cpp` (Generals + ZH) — uses `_Create_DX8_Surface`, `_Copy_DX8_Rects`, `_Get_D3D_Device8()->TestCooperativeLevel()`. Needs a **surface/blit/device-lost** abstraction. Candidate new methods: `Create_Offscreen_Surface(w, h, fmt)`, `Blit_Surface(src, src_rect, dst, dst_point)`, `Is_Device_Cooperative()`. This is meaningful but tractable work — estimate a session of its own (Phase 3C).

**Deeply coupled, need substantial low-level abstraction first:**

- `W3DWater.cpp` — 47 raw D3DRS_* / D3DTSS_* calls. The heaviest single system. Needs the texture-stage-state API abstracted.
- `W3DVolumetricShadow.cpp`, `W3DProjectedShadow.cpp` — stencil volume shadows, raw `D3DRS_STENCIL*`. Needs a **stencil state** abstraction group on `IRenderBackend`.
- `W3DShaderManager.cpp` — central shader + post-processing manager, touches raw `IDirect3DDevice8` methods for render-to-texture setups. Large.
- `W3DDisplay.cpp` — main display context manager, device lifecycle. Close to the metal.
- `W3DScene.cpp` — scene graph orchestrator, 99 calls.
- `W3DMouse.cpp` — uses `_Get_D3D_Device8` directly for `IDirect3DDevice8::ShowCursor` / `SetCursorProperties`.

Recommended Phase 3C focus: **W3DShroud** (the clearest next target with well-scoped interface extension work), plus small cleanups for any file that has a handful of stray low-level calls we can cover with the existing or newly-added interface methods.

## Windows build verification needed

Same as previous phases — this branch is unbuilt. Expected behavior:

1. **`-DGGC_RENDER_BACKEND=dx8`** (default) — should compile identically to Phase 3 + the 3 new migrations + 1 interface extension. Game should render identically. The newly-added DX8Backend forwarding methods translate enum values by cast, which the commit's values were chosen specifically to allow.
2. **`-DGGC_RENDER_BACKEND=bgfx`** — should still compile and link. The bgfx backend now has 3 new stub methods that accept but ignore their arguments. At runtime the migrated subsystems (W3DRoadBuffer, W3DStatusCircle, FlatHeightMap fade effects) silently no-op through the stubs. Total visible impact: road network, status circle, and FlatHeightMap's alpha-channel writes disappear under `=bgfx`. This is correct Phase 3B behavior.
3. **`-DGGC_RENDER_BACKEND=diligent`** — same as bgfx.

**Likely compile errors to watch for:**

- **"`D3DCOLORWRITEENABLE_RED` undeclared"** in `DX8Backend.cpp` — if the compilation unit doesn't transitively include `d3d8.h`. Should be fine because `DX8Backend.cpp` includes `dx8wrapper.h` which includes `d3d8.h`, but if the order matters, add an explicit include.
- **"no matching function for call to `Set_Color_Write_Enable`"** — if the subagent migrated the wrong number of booleans. Verify by grep.

None expected to be hard fixes.


## Exit criterion

Phase 3B ends with:

- 3 additional subsystems migrated (W3DRoadBuffer, W3DStatusCircle fully, FlatHeightMap)
- `IRenderBackend` extended by 3 new virtual methods + 2 new enums
- All three backend implementations updated (DX8 real, bgfx/Diligent stubs)
- Default build (`=dx8`) compiles and runs identically to Phase 3
- `=bgfx` and `=diligent` builds compile; migrated subsystems silently no-op
