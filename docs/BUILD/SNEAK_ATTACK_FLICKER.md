# GLA Sneak Attack Wing Flicker — Investigation Handoff

## TL;DR

The GLA Sneak Attack Tunnel Network's open-state wing tops render with
intermittent dark/red "slat" stripes on the bgfx Metal backend. Symptom
flips between adjacent render frames at the same camera position. Sister
issue to the (now fixed) Chinook rotor problem, but on a different code
path — wing draws are opaque on view 1, not sorted alpha on view 2.

This document captures everything tried, what's been ruled out, and the
most promising remaining avenues.

Branch with the chinook fix and these notes: `bobtista/feat/phase4-bgfx-cutover`
(also merged into `bobtista/integration/macos-sdl3-bgfx`).
Save used for repro: `00000044.sav` ("sneak" — bombardment beach map).

## Reference vs. observed

Reference (DX8 path, what the user expects):
- Wing top is medium grey-green metal
- Visible rivets, ~3 horizontal slat grooves
- Green tip on outer edge of each wing
- Reference frames: `.context/sneak-attack/ref_v2_a.png`, `ref_v2_b.png`, `ref_v2_c.png`

Observed on bgfx Metal (the bug):
- Wing top dark base; intermittent RED slat stripes appear/disappear
  between adjacent render frames at the same camera position
- Sometimes a darker stripe band in the top third of the wing
- Glitch state most reproducibly visible at frame ~132 of `00000044.sav`
  loaded with `-quickstart -win` (clean frames at 129-131, red slat at 132)
- The transition correlates with the wing-opening animation completing

## What's verified about the wing draws

Captured via `GGC_BGFX_EFFECT_SUBMIT_DIAG=1` and
`GGC_BGFX_SORTED_DECAL_DIAG=1`:

- Six opaque submeshes draw on `kBgfxEngineView` (view 1) per frame:
  - `ubsnkatak_0.tga`: 60, 4, 8, 12 polys (4 submeshes — wings + tips + small detail)
  - `ubsnkatak_01.tga`: 75, 32 polys (2 submeshes — dirt mound + other)
- Plus a tiny 2-poly `ubsnkatak_01.tga` sorted alpha decal on view 2.
- All wing submeshes share: `state=0x500121202f`,
  `wz=1` (writes Z), `tssOps0=(3,3,0,0)` (modulate color/alpha),
  no second texture stage, no team-color or shroud overlay.
- `BLEND_FUNC(ONE, ZERO)` — effectively opaque.
- Cull is `CULL_CW`. Depth test `LEQUAL`.

Texture atlas `ubsnkatak_0.dds` is 256×256 DXT1 (BC1) with 9 authored mips:
- Top-left quadrant: red GLA scorpion on dark metal
- Top-right: sand
- Bottom-left: grey metal plate with rivets and slat grooves — this is
  what the wing TOP samples
- Bottom-right: wood planks

Extracted PNG of the atlas: `.context/sneak-attack/ubsnkatak_0.png`.

## Submesh 60-poly mystery

The 60-poly `_0` submesh appears to be hidden / back-facing — applying
positive z-bias even at 0.01 NDC (huge) to it produced **zero visible
change**. So the visible wing-top surface is split across the 4/8/12-poly
`_0` submeshes plus likely some `_01` submeshes.

Skipping individual small `_0` submeshes also produced no obvious visible
diff. Skipping ALL `ubsnkatak_0` removes the wing entirely (and the
green tips). So the green tips are part of `_0`, not `_01`.

## Things tried and ruled out (every one was a no-op or made it worse)

1. **mip-1-only upload** (skip mip 1+ in `BgfxBackendTextures.cpp`) —
   wing still flickers at frame 132 with the exact same red slat pattern.
2. **mip-2-only / mip-3-only** caps — wings turned uniformly black
   (mip 0 alone aliases the metal-plate grooves; capping forces the GPU
   to use only that one mip even at distance).
3. **`BGFX_SAMPLER_MIP_POINT` at bind** for `ubsnkatak` textures.
4. **Force `BGFX_SAMPLER_MIN/MAG_ANISOTROPIC` at bind** (pass
   `UINT32_MAX` for sampler flags so texture creation flags propagate).
5. **z-bias forward on 60-poly `_0`** submesh — no visible change
   (proves 60-poly is hidden geometry).
6. **z-bias forward on 8-poly `_0`** submesh — no change.
7. **z-bias forward on 12-poly `_0`** submesh — no change.
8. **z-bias forward on 4-poly `_0`** submesh — removed green tips
   (4-poly = green tips), still no help on slat flicker.
9. **LEQUAL → LESS** depth test for opaque `ubsnkatak` draws.
10. **Force unlit** for `ubsnkatak` (set `u_lightingEnabled = 0`).
11. **Force `u_matDiffuse = (1,1,1)` and `u_matAmbient = (1,1,1)`**
    for `ubsnkatak` draws to bypass the dark green-tinted lit math.
12. **Capture-time `state.world` read from `RenderStateCache`** scoped
    to `ubsnkatak` (mirrored from the chinook fix) — no change for the
    opaque view-1 draws (the rotor fix matters because rotor goes
    through the sorted view-2 replay; wings don't).

White-texture override (`GGC_SNEAK_WHITE_TEX=1`) confirmed that with
texture replaced by 1×1 white, the wings render dark green — so the
green tint is coming from vertex color / lit material, not the texture.
That should mean overrides 10 + 11 above ought to neutralize it, but
they visibly didn't, which is where the trail goes cold.

## What we know about the flicker timing

Captured frame-by-frame around the transition (interval 1):
- Frames 100-131: wings render clean dark
- Frame 132: red slats appear on left wing
- Frames 133+: continues with intermittent slats and dark
- The transition coincides with the entrance unfold animation finishing

This is *not* mip-selection flicker (proven by mip-only-mip-0 test still
showing it). It is *not* z-fighting between coplanar submeshes (proven
by aggressive z-bias on every submesh having no effect).

The transition feels like an animation/state event: one frame the wing
is "in motion" with one matrix, the next frame it has "settled" into a
slightly different transform with the same texture. The settled UVs
happen to expose the texture in a way the in-motion UVs didn't.

## Two leading hypotheses

### H1: Metal-specific bgfx backend behavior

The Metal backend may sample BC1 textures differently from D3D11/12 (BC1
decompression quality, sub-block interpolation, anisotropic sample
direction). The reference DX8 renders pixel-perfect; our bgfx-on-Metal
build does not for this specific atlas.

**To test:** Load `00000044.sav` on a Windows machine with the same
integration-branch binary. If the flicker is absent there, this is the
hypothesis to chase — likely involves either modifying sampler flags at
texture creation in `BgfxBackendTextures.cpp`, or pre-decompressing
BC1 → RGBA8 for this specific texture.

### H2: Sub-pixel matrix jitter from physics tick

Game logic ticks at 30 Hz, render runs faster. Each game tick the
sneak-attack object's transform may be recomputed and end up with a
slightly different floating-point value (even when "settled"). The
camera's view matrix also gets per-tick noise. Sub-pixel UV jitter
crossing texel boundaries in the atlas could expose dark/red pixels
that the previous frame's snapped position averaged away.

**To test:** Add a per-frame snapshot of the wing draw's
`state.world` and `bgfx::setViewTransform` arguments. Diff the snapshots
between frames 131 → 132 and look for changed bits. Look at:
- `Core/Libraries/Source/WWVegas/WW3D2/BgfxBackend.cpp` around
  `CaptureSortedBatchTransformsForBgfx` (line ~4318) and
  `SubmitEngineDraw`'s `setViewTransform` call (line ~7818) for
  capture points.

## Useful repro tooling

- `.context/sneak_capture.sh <label> <sav> <start> <interval> <secs>`
  — deterministic screenshot capture. Crops `.context/snk-capture/<label>/f.*_z.png`
  to the entrance area.
- `.context/sneak_motion.sh` — adds osascript-driven camera keystrokes
  (Page Up/Down for zoom, arrows for pan) during capture. Note: the
  savegame already drifts the camera after a couple seconds, so motion
  scripting may be unnecessary.
- The user's hand-captured frames showing both states are in
  `.context/attachments/Screenshot 2026-05-17 at 10.44.*.png` and
  `Screenshot 2026-05-17 at 11.10.24 AM.png`.

## What changed and what didn't

Single commit landed during this session:

- `0063b7e30 fix(bgfx): rotate chinook rotor blur using live world transform`

That fix is unrelated to the sneak-attack flicker mechanically (rotor
goes through sorted view 2; wings are opaque view 1) but is the same
*kind* of root cause — bgfx wasn't applying the real per-mesh world
transform to a draw because `FixedFunctionState`'s world is identity on
bgfx. Worth reviewing as a template for the sneak attack if H2 turns
out to involve a matrix-not-flowing-through problem.

## Suggested next steps for GPT/whoever picks this up

1. **Windows side-by-side test first** — cheapest signal. Load the same
   save and integration build on Windows; observe whether the wing
   flicker reproduces there.
2. **If Metal-specific:** experiment with `BGFX_SAMPLER_BORDER_COLOR`
   on the atlas to prevent cross-quadrant bleed, or pre-decompress
   `ubsnkatak_0` to RGBA8 with manually-generated per-quadrant mips
   (analogous to the existing `UploadTerrainAtlasMips` path in
   `Core/Libraries/Source/WWVegas/WW3D2/BgfxBackendTextures.cpp`).
3. **If shared with Windows:** add per-frame matrix logging at the
   wing-draw submit site, diff frame 131 vs 132, isolate which value
   actually changed.
4. **Either way:** consider modifying the W3D mesh's UV mapping to
   avoid the top edge of the `_0` bottom-left quadrant (where the
   weathered area / boundary with the top-left quadrant is). That's
   an asset-side fix that bypasses backend differences entirely.
