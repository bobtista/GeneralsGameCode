# Render Backend Phase 3F — W3DScene partial migration + stencil state group

**Branch:** `bobtista/refactor/phase3f-scene`
**Base:** `bobtista/refactor/phase3e-shadows` (Phase 3E)
**Status:** in progress

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the multi-phase plan and [PHASE3.md](PHASE3.md) - [PHASE3E.md](PHASE3E.md) for previous Phase 3 sessions.

## Goal

Migrate `W3DScene.cpp` (Generals + GeneralsMD copies) — the scene graph orchestrator. Largest single call-site count of any remaining clean target. Adds a substantial new `IRenderBackend` extension: the **stencil state group**.

## What's in W3DScene

99 `DX8Wrapper::*` calls per file (95 in Generals, 99 in MD), broken down by category:

| Category | Count | What |
|---|---:|---|
| `Set_DX8_Render_State(D3DRS_STENCIL*)` | ~36 | Stencil ops (ENABLE, FUNC, REF, MASK, WRITEMASK, PASS, FAIL, ZFAIL) |
| `Set_DX8_Render_State(D3DRS_ZBIAS)` | 8 | Polygon offset / depth bias |
| `Set_DX8_Render_State(D3DRS_COLORWRITEENABLE)` | 8 | Color channel write masks |
| `Set_DX8_Render_State(D3DRS_FILLMODE)` | 5 | Wireframe / solid toggle |
| `Set_DX8_Render_State(D3DRS_ZENABLE/ZFUNC)` | 6 | Z-buffer test enable + comparison |
| `Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE/SRCBLEND/DESTBLEND)` | 9 | Already covered by Phase 3B+3E methods |
| `Set_DX8_Render_State(D3DRS_AMBIENT)` | 3 | Already covered by `Set_Ambient` (takes Vector3) |
| **High-level calls** | **12** | `Has_Stencil`, `Convert_Color`, `Clear`, `Set_Shader`, `Set_Material`, `Set_Fog`, `Apply_Render_State_Changes`, `Get_Current_Caps`, `_Get_D3D_Device8`, `_Is_Triangle_Draw_Enabled`, `stats` |
| Total | ~99 | |

The stencil cluster is by far the largest — ~36 sites for 7 distinct `D3DRS_STENCIL*` enums. Adding a stencil state group as a Phase 3F extension unlocks all of those plus future Phase 3G/4 work that touches the deferred shadow stencil sites.

## Interface extension: stencil state group

Two new POD enums + 8 new virtual methods.

### Enums

```cpp
enum CompareFunc
{
    // Values match D3DCMP_* 1..8 directly so DX8Backend can cast.
    RB_CMP_NEVER         = 1,
    RB_CMP_LESS          = 2,
    RB_CMP_EQUAL         = 3,
    RB_CMP_LESS_EQUAL    = 4,
    RB_CMP_GREATER       = 5,
    RB_CMP_NOT_EQUAL     = 6,
    RB_CMP_GREATER_EQUAL = 7,
    RB_CMP_ALWAYS        = 8,
};

enum StencilOp
{
    // Values match D3DSTENCILOP_* 1..8.
    RB_STENCIL_OP_KEEP     = 1,
    RB_STENCIL_OP_ZERO     = 2,
    RB_STENCIL_OP_REPLACE  = 3,
    RB_STENCIL_OP_INCR_SAT = 4,
    RB_STENCIL_OP_DECR_SAT = 5,
    RB_STENCIL_OP_INVERT   = 6,
    RB_STENCIL_OP_INCR     = 7,
    RB_STENCIL_OP_DECR     = 8,
};
```

`CompareFunc` is generic on purpose — it's used for stencil comparison now and could also be used for depth comparison in a future phase (replacing the `D3DRS_ZFUNC` calls). Reusable design.

### Methods

```cpp
virtual void Set_Stencil_Enable(bool enable) = 0;
virtual void Set_Stencil_Func(CompareFunc func) = 0;
virtual void Set_Stencil_Ref(unsigned int ref) = 0;
virtual void Set_Stencil_Mask(unsigned int mask) = 0;
virtual void Set_Stencil_Write_Mask(unsigned int mask) = 0;
virtual void Set_Stencil_Pass_Op(StencilOp op) = 0;
virtual void Set_Stencil_Fail_Op(StencilOp op) = 0;
virtual void Set_Stencil_ZFail_Op(StencilOp op) = 0;
```

DX8Backend forwards each to `Set_DX8_Render_State(D3DRS_STENCIL*, ...)` with a direct cast. BgfxBackend and DiligentBackend stub no-ops.

The split into 8 methods rather than one bundle method (`Set_Stencil_State(struct StencilState)`) matches how the call sites use them: each `Set_DX8_Render_State` call sets one D3DRS at a time, and the Generals shadow code is interleaved with logic that conditionally sets only some of the stencil fields. A bundle struct would force the call sites to read state back, modify it, and re-write — more code change, more regression risk. Stay close to the original 1:1 mapping.

## What I'll migrate in W3DScene

✅ The 12 high-level calls — `Has_Stencil`, `Clear`, `Set_Shader`, `Set_Material`, `Set_Fog`, `Apply_Render_State_Changes`. (Skipping `Convert_Color` since it's a static utility, `Get_Current_Caps` since caps queries aren't in the interface, `_Get_D3D_Device8` and `_Is_Triangle_Draw_Enabled` and `stats` since they're explicitly low-level access.)

✅ The ~36 stencil state calls via the new stencil state group.

✅ The ~9 `D3DRS_ALPHABLENDENABLE`/`SRCBLEND`/`DESTBLEND` calls via the existing Phase 3B+3E blend methods.

✅ The 3 `D3DRS_AMBIENT` calls via the existing `Set_Ambient` (need to convert from `D3DCOLOR` to `Vector3` — requires unpack helper, may bail to leaving these as low-level if it's awkward).

## What I'll leave for a future phase (with TODO markers)

- ⏸️ The 8 `D3DRS_ZBIAS` calls — needs a `Set_Z_Bias(int)` method. Easy 1-method extension but not worth doing in this session if it's only one file using it.
- ⏸️ The 8 `D3DRS_COLORWRITEENABLE` calls — these likely pass DWORD bitmasks (like the W3DVolumetricShadow sites), not individual booleans. Same save/restore concern as Phase 3E.
- ⏸️ The 5 `D3DRS_FILLMODE` calls — needs a `Set_Fill_Mode(FillMode)` enum. Single file uses it.
- ⏸️ The 6 `D3DRS_ZENABLE/ZFUNC` calls — needs `Set_Depth_Test_Enable(bool)` + `Set_Depth_Func(CompareFunc)`. Reuses the new CompareFunc enum from this phase. Could fit, will decide during implementation.

The total non-migrated count after Phase 3F should be ~30 calls per file, down from 99.

## Estimated migration count

Per file: ~12 high-level + ~36 stencil + ~9 blend + ~3 ambient = **~60 sites migrated, ~30 left as TODO**.

Across both copies: ~120 migrated, ~60 TODO.

## Out of scope

- The 4 raw `m_pDev->X()` / `_Get_D3D_Device8` calls — they're scattered debug/conditional checks, leave them.
- W3DCustomScene if it exists — not part of W3DScene.cpp scope.
- Any Z-bias or fill-mode extensions — defer for a future phase if 1-2 files need them.

## Task list

- [ ] **3F.0** Write this document
- [ ] **3F.1** Add stencil state group to all 4 backend implementations
- [ ] **3F.2** Migrate W3DScene high-level + stencil + blend + ambient calls
- [ ] **3F.3** Document completion + handoff

## Exit criterion

W3DScene.cpp in both copies has its high-level calls, stencil state, blend state, and ambient calls routed through `g_renderBackend`. The remaining low-level calls (ZBIAS, FILLMODE, COLORWRITEENABLE bitmasks, ZENABLE/ZFUNC) get a clear migration-comment block at the top of each containing function explaining the deferred work.

After Phase 3F:
- ~120 call sites migrated across both copies
- ~60 sites remain as Phase 4 TODO with markers
- 8 new IRenderBackend methods + 2 new enums
- Cumulative: 13 subsystems migrated
