# Render Backend Phase 3E — Partial migration of stencil/projected shadows

**Branch:** `bobtista/refactor/phase3e-shadows`
**Base:** `bobtista/refactor/phase3d-mouse` (Phase 3D)
**Status:** partial migration complete, pending Windows build verification

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the multi-phase plan and [PHASE3.md](PHASE3.md) - [PHASE3D.md](PHASE3D.md) for previous Phase 3 sessions.

## Goal — and an honest scope reduction

`W3DVolumetricShadow.cpp` and `W3DProjectedShadow.cpp` were on my Phase 3D handoff list as the highest-leverage next target. The framing was: "needs a stencil state extension group of ~5-7 methods, then ~140 stencil sites migrate, two related subsystems unlocked."

**That framing was wrong.** Investigation in Phase 3E shows the shadow files are deeply coupled in a way that goes well beyond stencil state. Both files use a pattern that completely bypasses `DX8Wrapper`:

```cpp
LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();
m_pDev->SetIndices(...);
m_pDev->SetStreamSource(...);
m_pDev->SetVertexShader(SHADOW_DYNAMIC_VOLUME_FVF);
m_pDev->SetTransform(D3DTS_WORLD, ...);
m_pDev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
m_pDev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
// ... 8 more SetRenderState calls
m_pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
// ... 5 more SetTextureStageState calls
m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, numVerts, ...);
```

Counted by raw call type:

| File | High-level `DX8Wrapper::*` calls | Raw `m_pDev->X()` calls |
|---|---:|---:|
| W3DVolumetricShadow.cpp (per copy) | 15 | 81 |
| W3DProjectedShadow.cpp (per copy) | 22 | 61 |

The 142 raw `m_pDev->` calls are not just state setters — they include `SetIndices`, `SetStreamSource`, `SetVertexShader` (FVF binding), `DrawIndexedPrimitive`, `DrawPrimitiveUP`, `CreateIndexBuffer`, `CreateVertexBuffer`. The shadow code has its own custom render loop that bypasses W3D's `DX8VertexBufferClass` / `DX8IndexBufferClass` for performance reasons (presumably to avoid per-call wrapper overhead in the inner stencil-volume rendering loops).

To migrate the raw blocks cleanly we'd need either:

- **Option A**: Rewrite the shadow rendering to go through W3D's `VertexBufferClass` / `IndexBufferClass` and standard `Draw_Triangles` paths. Substantial refactor of the shadow rendering loop, possible perf regression, requires retesting shadow visuals across many scenarios.
- **Option B**: Add IRenderBackend methods that essentially mirror the raw D3D8 device API (`Set_Stream_Source`, `Set_Indices`, `Draw_Indexed_Primitive`, etc.). This kind of defeats the purpose of the abstraction — it's a 1:1 wrapper of the device interface, not a backend-neutral one.

Both options are significantly more work than the other Phase 3 sessions and neither is the right "single session" effort. The honest call is to **partial-migrate** in Phase 3E: do the easy wins, leave the hard parts marked with clear TODOs, and revisit the deeply-coupled rendering loops in a dedicated future phase (or as part of Phase 4's cutover).

## Phase 3E scope (the easy wins)

What I'll migrate:

1. **The 37 high-level `DX8Wrapper::*` calls per file pair** (15 in volumetric, 22 in projected) that already use the wrapper. These are calls like:
   - `DX8Wrapper::Set_Material(vmat)`
   - `DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader)`
   - `DX8Wrapper::Set_Texture(0, nullptr)`
   - `DX8Wrapper::Apply_Render_State_Changes()`
   - `DX8Wrapper::Invalidate_Cached_Render_States()`
   - `DX8Wrapper::Set_Index_Buffer(ib_access, 0)`
   - `DX8Wrapper::Set_Vertex_Buffer(vb_access)`
   - `DX8Wrapper::Set_Transform(D3DTS_WORLD, tm)`
   - `DX8Wrapper::Draw_Triangles(0, 2, 0, 4)`
   - `DX8Wrapper::Has_Stencil()`
   - `DX8Wrapper::Create_Render_Target(...)`

2. **A few `Set_DX8_Render_State(D3DRS_SRCBLEND/DESTBLEND/ALPHABLENDENABLE/COLORWRITEENABLE...)` calls** that go through DX8Wrapper's facade (not the raw `m_pDev->SetRenderState` ones — those are in the deeply-coupled blocks). These can use the Phase 3B blend extension methods plus the new `Set_Alpha_Blend_Enable` from this phase.

3. **One small interface extension**: `Set_Alpha_Blend_Enable(bool)` — the natural complement to the existing `Set_Blend_Op` and `Set_Blend_Factors` methods. Two files in this session use it, plus future phases will need it.

What I'll **not** migrate:

- The raw `m_pDev->X()` blocks that hold the inner shadow rendering loops. These get a TODO marker comment block at the top of each block explaining they're deferred to Phase 4 (or a dedicated Phase 3F if we tackle them sooner).
- The `IDirect3DDevice8::CreateIndexBuffer` / `CreateVertexBuffer` calls in `W3DProjectedShadow::initRenderTargets`. These allocate device-pool buffers that stay alive across frames and need a substantial allocation abstraction we don't have yet.
- The `D3DRS_STENCIL*` calls — they live inside the raw `m_pDev->` blocks and would need a stencil state extension group that's only useful when the rest of the block migrates.
- The `D3DTSS_*` (texture stage state) calls — same reasoning. The texture-stage-state API is a Phase 3F+ design exercise.

## Why partial migration is the right call

Three reasons:

1. **Avoids overreach**. The Phase 3 sessions have all been small focused commits that produce verifiable improvements. Trying to migrate the shadow inner loops in one session would be a multi-day effort with high regression risk and would require performance retesting on Windows. Splitting it makes both the easy and hard parts tractable.

2. **The TODO markers are themselves valuable**. Future-me (or anyone else picking up the work) wants to know exactly which blocks need the heavyweight migration. A clear comment that says "this block needs Phase 4 raw rendering abstraction, see PHASE3E.md" is much better than discovering the coupling by surprise.

3. **The Phase 3B `Set_Color_Write_Enable` and Phase 3D `Show_Hardware_Cursor` regression caught me missing** declarations and implementations. Tackling 100+ raw call sites in one session is exactly the kind of thing where I'd miss something subtle. Better to do 37 mechanical migrations cleanly than 142 with bugs.

## Interface extension this session: `Set_Alpha_Blend_Enable`

```cpp
// In IRenderBackend.h, next to the existing blend methods:
virtual void Set_Alpha_Blend_Enable(bool enable) = 0;
```

DX8Backend forwards to `DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE)`. Bgfx and Diligent stub no-ops. Three header decls + three .cpp impls + one IRenderBackend.h line — same shape as every other Phase 3 extension.

## In scope

- `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h` — add `Set_Alpha_Blend_Enable`
- `Core/Libraries/Source/WWVegas/WW3D2/DX8Backend.{h,cpp}` — implement
- `Core/Libraries/Source/WWVegas/WW3D2/BgfxBackend.{h,cpp}` — stub
- `Core/Libraries/Source/WWVegas/WW3D2/DiligentBackend.{h,cpp}` — stub
- `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp` — migrate 15 high-level call sites
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp` — same
- `Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp` — migrate 22 high-level call sites
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp` — same

## Out of scope

- The 142 raw `m_pDev->X()` calls. These get TODO marker comments and stay as-is.
- The `D3DRS_STENCIL*` state setters that live inside the raw blocks.
- The `D3DTSS_*` texture stage state setters.
- The `IDirect3DDevice8::CreateIndexBuffer` / `CreateVertexBuffer` calls.
- The `D3DSTREAMSOURCE` / `SetIndices` / `DrawIndexedPrimitive` raw rendering paths.
- Any new abstraction for "raw rendering" — Phase 4 design work.

## Task list

- [x] **3E.0** Write this document
- [x] **3E.1** Add `Set_Alpha_Blend_Enable` to all 4 backend implementations
- [x] **3E.2** Migrate W3DVolumetricShadow high-level calls (both copies)
- [x] **3E.3** Migrate W3DProjectedShadow high-level calls (both copies)
- [x] **3E.4** Document completion + handoff

## What landed

### Interface extension (commit `5311b671e`)

One new virtual method on all four backends:

```cpp
virtual void Set_Alpha_Blend_Enable(bool enable) = 0;
```

DX8Backend forwards to `DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE)`. BgfxBackend and DiligentBackend stub no-ops.

### W3DVolumetricShadow partial migration (commit `2fc0727d2`)

Both Generals and GeneralsMD copies. **12 of 15 high-level call sites migrated per file**, leaving 3 that stay on `DX8Wrapper::` for documented reasons:

- ✅ Migrated: `Set_Material` (×2), `Set_Shader` (×2), `Set_Texture` (×3), `Apply_Render_State_Changes` (×2), `Invalidate_Cached_Render_States` (×2), `Has_Stencil`
- ⏸️ Left as-is with notes:
  - `DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps()` — caps query, not in interface
  - `DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0)` and the matching restore — paired with a raw `m_pDev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable)` save call. Round-tripping a DWORD bitmask through the boolean `Set_Color_Write_Enable(r,g,b,a)` API would smuggle in a behavior change.

The ~81 raw `m_pDev->X()` calls are unchanged. Each containing function gets a clear migration-comment block at the top explaining the Phase 4 dependency.

### W3DProjectedShadow partial migration (commit `aad68e09b`)

Both Generals and GeneralsMD copies. **18 of 22 high-level sites migrated per file** (the 4 unchanged are commented-out debug breadcrumbs):

- ✅ Migrated: `Create_Render_Target` (×2), `Set_Index_Buffer`, `Set_Vertex_Buffer` (added explicit `,0` stream arg), `Set_Texture` (×3), `Set_Material` (×2), `Set_Shader` (×4), `Set_Transform` (with `D3DTS_WORLD` → `RB_TRANSFORM_WORLD`), `Apply_Render_State_Changes` (×2), `Draw_Triangles`. Plus the blend trio: `Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA)` + `Set_Alpha_Blend_Enable(true)` replacing 3 raw `Set_DX8_Render_State` calls.

The ~61 raw `m_pDev->X()` calls remain unchanged with the same migration-comment marker pattern.

### Commits

```
aad68e09b refactor(ww3d): partially route W3DProjectedShadow high-level calls through g_renderBackend
2fc0727d2 refactor(ww3d): partially route W3DVolumetricShadow high-level calls through g_renderBackend
5311b671e feat(ww3d2): add Set_Alpha_Blend_Enable to IRenderBackend
9b9f83f96 docs(ww3d2): add Phase 3E plan with realistic partial-migration scope
```

### Statistics

- 2 subsystems partially migrated
- 6 files touched (2 .cpp × 2 copies + 4 backend files via the extension commit + this doc)
- ~60 high-level call sites replaced (12+18 per file × 2 copies)
- ~142 raw `m_pDev->X()` calls remain — clearly marked Phase 4 TODO
- 1 new IRenderBackend method
- 4 commits

## Cumulative phase 3 progress

Across Phase 3 + 3B + 3C + 3D + 3E, **12 subsystems are now routed through `g_renderBackend`** (10 fully + 2 partially):

1. W3DBibBuffer (3) — full
2. W3DDebugIcons (3) — full
3. W3DInGameUI (3) — full
4. W3DTerrainTracks (3) — full
5. W3DBridgeBuffer (3) — full
6. W3DRoadBuffer (3B) — full
7. W3DStatusCircle (1 + 3B) — full
8. FlatHeightMap (3B) — full
9. W3DShroud (3C) — full
10. W3DMouse (3D) — full
11. **W3DVolumetricShadow (3E) — partial** (12/15 high-level migrated, ~81 raw device calls remain)
12. **W3DProjectedShadow (3E) — partial** (18/22 high-level migrated, ~61 raw device calls remain)

`IRenderBackend` has grown by **7 methods + 2 enums** beyond Phase 1 baseline.

## What this changes at runtime

Same as every Phase 3 migration: **nothing changes under any backend.**

- `=dx8`: high-level calls go `g_renderBackend->X` → `DX8Backend::X` → `DX8Wrapper::X` → D3D8. Raw `m_pDev->X` calls go directly to D3D8 as before.
- `=bgfx` / `=diligent`: the migrated high-level calls silently no-op through stubs. The raw `m_pDev->` calls still drive DX8 directly because no Phase 3 work has touched device ownership. Visual result under non-DX8 backends is unpredictable for shadows specifically (state defaults that the migrated calls would have set go missing) but stencil draws still happen via the raw paths.

## Windows build verification

Test plan: build `=dx8`, start a skirmish, verify stencil shadows on units render correctly, verify projected decal shadows on terrain render correctly, alt-tab and back, confirm shadows survive a device reset.

`=bgfx` and `=diligent` builds should compile cleanly. Shadow visual correctness under non-DX8 backends is **not** expected — that's the whole point of the partial migration deferring the inner-loop work.

## Phase 3F preview

Logical next targets ordered by leverage:

1. **W3DScene** (99 calls, mostly mechanical) — biggest call count of any clean target. May need 1-2 more interface methods but most calls covered already.
2. **The deferred shadow inner-loop blocks** — would need a "raw rendering" abstraction or refactor to W3D buffer classes.
3. **W3DDisplay / W3DGameClient** — main display + device lifecycle, closer to the metal.
4. **W3DWater** — biggest single piece of remaining work, needs texture-stage-state API designed.

**Alternative**: Pause Phase 3 here and start designing Phase 4 (the cutover). With 12 subsystems migrated we have meaningful coverage to start exercising bgfx/Diligent backends in real frames. The remaining subsystems are progressively harder and progressively less leveraged.

## Exit criterion

Both shadow files have their high-level `DX8Wrapper::*` calls routed through `g_renderBackend` (and the Phase 3B blend methods + new `Set_Alpha_Blend_Enable`). The raw `m_pDev->X()` blocks remain unchanged but have a clear TODO marker comment block above each one explaining the Phase 4 dependency.

After Phase 3E:
- ~74 call sites migrated (37 per file × 2 copies)
- ~142 raw device sites remain, marked as Phase 4 TODO
- 1 new IRenderBackend method (`Set_Alpha_Blend_Enable`)
- Cumulative: 12 subsystems migrated (10 from prior phases + 2 from this session, even though both shadow subsystems are partial migrations)
