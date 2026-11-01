/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Ronin @feature 20/09/2026 DX9: TAA step 1 — the jitter. See R3DTaa.h for why this is the only thing step 1 does.

#include "WW3D2/dx8todx9.h"
#include "WWLib/always.h"
#include "Lib/BaseType.h"
#include "W3DDevice/GameClient/R3DTaa.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/formconv.h"		// Ronin @feature 17/10/2026 DX9: the finish pass copies the frame in its own format
#include "WW3D2/camera.h"
#include "WW3D2/mesh.h"		// Ronin @feature 24/09/2026 DX9: mesh motion vectors (§14)
#include "WW3D2/meshmdl.h"
#include "WW3D2/matinfo.h"		// Ronin @feature 24/09/2026 DX9: reactive mask - time-variant mappers
#include <d3dx9math.h>		// Ronin @bugfix 20/09/2026 DX9: D3DX matrix maths, to match the shadow map's convention
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/R3DSsao.h"
#include "Common/GlobalData.h"		// Ronin @feature 26/09/2026 DX9: Options.ini DX9TAA keys
#include "W3DDevice/GameClient/R3DShadowMap.h"	// Ronin @bugfix 27/09/2026 DX9: the sun matrix, for the moving-shadow mask
#include "GameClient/Drawable.h"				// Ronin @bugfix 27/09/2026 DX9: idle infantry - which object a mesh belongs to
#include "GameClient/DrawableInfo.h"
#include "W3DDevice/GameClient/R3DLocalLights.h"	// Ronin @feature 09/10/2026 DX9: which meshes have their beam polygons hidden
#include "Common/KindOf.h"
#include "GameLogic/PolygonTrigger.h"			// Ronin @bugfix 29/09/2026 DX9: the water's polygons, for its reactive mask
#include "GameLogic/TerrainLogic.h"			// Ronin @bugfix 26/10/2026 DX9: the ground under a moving decal's mark
#include "W3DDevice/GameClient/W3DWater.h"

Bool R3DTaa::s_enabled = FALSE;

static Int s_phase = 0;

// Ronin @feature 20/09/2026 DX9: TAA step 2b. Declared up here because beginFrame/setEnabled below touch them:
// toggling TAA mid-game must not blend the first new frame into whatever the buffer held from the last time it ran,
// and s_ownsDepth records that TAA — not SSAO — started the INTZ buffer, so TAA is the one that ends it.
static Bool s_historyValid = FALSE;
static Bool s_active       = FALSE;	// Ronin @feature 26/09/2026 DX9: enabled AND MSAA off, decided once per frame in beginFrame
static Bool s_ownsDepth    = FALSE;

// Ronin @diagnostic 20/09/2026 DX9: runtime knobs, driven by the `taa` panel command. Defaults are the shipping values.
static float s_weight      = 0.9f;		// ~8 frames of memory, matching the jitter sequence length
static Int   s_debugMode   = 0;
// Ronin @feature 26/09/2026 DX9: `taa sharpen` 0..1 = CAS strength, applied in the screen copy (pass 2). The unsharp
// mask it replaces ran in pass 1, so it was written into the history and compounded: +35% edge wobble at 0.35.
static float s_sharpen     = 0.0f;
// Ronin @feature 26/09/2026 DX9: `taa mipbias` - texture mip LOD bias while TAA runs; negative = sharper mips, whose extra
// aliasing the accumulation resolves. Nothing else in the engine sets D3DSAMP_MIPMAPLODBIAS, so TAA owns it.
static float s_mipBias     = -1.0f;	// Ronin @feature 26/09/2026 DX9: -1, user-tested; Options.ini DX9TAAMipBias overrides
static float s_lastMipBias = 0.0f;

// Ronin @feature 22/09/2026 DX9: last frame's LINEAR depth, packed RGBA8. The history is invalidated on a depth
// discontinuity - evidence the pixel shows DIFFERENT GEOMETRY - never on colour disagreement.
static TextureClass        *s_depthHist[2] = { NULL, NULL };
static IDirect3DPixelShader9 *s_depthStorePS = NULL;
// Ronin @feature 18/10/2026 DX9: per history buffer, for the screen-space reflections: the s_frameNo it was written in (0 = not yet),
// whether a depth and a camera came with it, that camera's clean view-projection and where its 3D view sat in it (c5).
static Bool        s_depthStoredNow = FALSE;
static UnsignedInt s_histFrame[2]   = { 0, 0 };
static Bool        s_histWhole[2]   = { FALSE, FALSE };
static D3DXMATRIX  s_histViewProj[2];
static float       s_histView[2][4] = { { 0.0f } };
// Ronin @feature 18/10/2026 DX9: the reflections' change mask (`taa reflmark`): a pane marks where its reflection changed. A visible
// change under LEAST is the history's own drift; from LEAST + 1 / GAIN on it marks fully.
static Int         s_reflMark       = 2;		// 0 off, 1 clamp there, 2 and restart the pixel where a unit moved
static const float TAA_REFLMARK_LEAST = 0.02f;
static const float TAA_REFLMARK_GAIN  = 20.0f;
static DWORD       s_reflMarkWrite  = 0;		// target 1's write mask before beginReflectMask
static Bool        s_reflMarkBound  = FALSE;
static DWORD       s_lightMaskWrite = 0;		// Ronin @bugfix 19/10/2026 DX9: target 1's write mask before beginLightMask
static Bool        s_lightMaskSet   = FALSE;
// Ronin @feature 19/10/2026 DX9: screen-space reflections WITHOUT TAA - last frame kept by the frame itself (storeLastFrame): its
// picture, its linear depth packed as TaaDepthStore_ps packs it, the s_frameNo it was kept in (0 = none), its camera and 3D view.
static Bool         s_lastFrameWanted = FALSE;	// R3DLocalLights, every frame: the screen-space reflections are on
static TextureClass *s_reflColour     = NULL;
static TextureClass *s_reflDepth      = NULL;
static UnsignedInt  s_reflW = 0, s_reflH = 0;
static D3DFORMAT    s_reflFormat      = D3DFMT_UNKNOWN;
static UnsignedInt  s_reflFrame       = 0;
static D3DXMATRIX   s_reflViewProj;
static float        s_reflView[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
static float        s_reflFar         = 1000.0f;
// Frame stamp for the mesh tracking table. Slots untouched for TAA_STALE_FRAMES may be reused.
enum { TAA_STALE_FRAMES = 8 };
static UnsignedInt  s_moverFrame = 0;
static Bool     s_velocityOn = TRUE;
static TextureClass          *s_velTex = NULL;
// Ronin @feature 24/09/2026 DX9: LAST frame's velocity target, swapped with s_velTex at the end of each frame. Its alpha
// says where a mover WAS; a pixel covered by a mover last frame and not this one is DISOCCLUDED ground whose history
// still holds the mover - the trail behind a moving unit. The depth test cannot see it for a low vehicle: its depth
// differs from the ground's by less than the tolerance. `taa disocc` switches it.
static TextureClass          *s_velPrevTex = NULL;
static Bool                   s_prevVelOK  = FALSE;
static Bool                   s_olderVelOK = FALSE;	// Ronin @feature 19/10/2026 DX9: s_velTex until postRender redraws it (reflections)
static Int                    s_disocc     = 2;	// `taa disocc` - 0 off, 1 this pixel, 2 this pixel or a neighbour
static UnsignedInt s_velW = 0, s_velH = 0;
static Bool s_lastVelOK = FALSE;

// Ronin @feature 24/09/2026 DX9: MESH motion vectors, §14. Every mesh drawn in the main pass reports itself through
// MeshClass::s_TaaMeshNote; TAA keeps its previous WORLD TRANSFORM, and a mesh whose transform changed is re-drawn into
// the velocity target from its own vertex array. No classification: turrets, dishes and animated rigid parts qualify
// just by moving. Keyed by pointer, identity only; a pointer reused after a delete costs at most one wrong frame.
enum { TAA_MESH_BITS = 12, TAA_MESH_SLOTS = 1 << TAA_MESH_BITS, TAA_MAX_MESH_DRAWS = 2048 };	// Ronin @bugfix 09/10/2026 DX9: 1024 -> 2048: a selected army flashes
// Ronin @feature 26/09/2026 DX9: §14 stage 2 - a SKIN keeps its deformed WORLD positions for two frames in skin[]
// (skinCur = this frame's). W3D deforms skins on the CPU, so there is no transform to diff.
struct TaaMeshSlot { const void *key; Matrix3D prev; UnsignedInt stamp; UnsignedInt movedStamp;
                     Vector3 *skin[2]; Int skinCap; Int skinCount; Int skinCur;
                     Matrix3D prevRoot; UnsignedInt rootStamp;	// Ronin @bugfix 27/09/2026 DX9: its object's transform
                     float boxC[3]; float boxE[3]; Bool boxOK; };	// Ronin @bugfix 09/10/2026 DX9: its world box when last drawn, reset if it vanishes
struct TaaMeshDraw { MeshClass *mesh; Matrix3D cur; Matrix3D prev; Int kind;
                     const Vector3 *skinCur; const Vector3 *skinPrev;	// skinCur NULL = rigid
                     float flash[4]; };	// Ronin @bugfix 09/10/2026 DX9: its drawable's selection flash, w = 1 while it flashes
// Ronin @feature 24/09/2026 DX9: REACTIVE MASK. What a motion vector cannot describe: a texture scrolling on a still
// surface (treads, conveyors - the engine's own time-variant mappers) and a MOVING translucent layer (a rotor), which
// writes no depth. Flagged in the velocity alpha: 0 none, 0.4 reactive, 0.7 velocity + reactive, 1.0 velocity.
// "a > 0.5 = velocity" still holds for every existing reader.
enum { TAA_KIND_VEL = 0, TAA_KIND_VEL_REACT, TAA_KIND_REACT, TAA_KIND_TRANSLUCENT, TAA_KIND_STILL, TAA_KIND_GHOST,
       TAA_KIND_VEL_IDLE };	// STILL: alpha 0.15
// Ronin @bugfix 27/09/2026 DX9: VEL_IDLE (alpha 0.92) = a soldier standing still whose mesh only breathes. debug 15 measured it:
// his head moves as far per frame as a flag near its pole, so distance cannot tell them apart; what he IS can. The resolve
// keeps `taa disoccv` for him alone (resetting every breath popped his edges) and the shadow mask leaves him out.
// Ronin @bugfix 27/09/2026 DX9: GHOST (alpha 0.3) = a MOVING runtime-translucent preview, or the placement bib. The resolve
// keeps no history under it and resets where it just was; REACTIVE's single clamp let busy grass admit its grey edges.
enum { TAA_MODEL_BITS = 10, TAA_MODEL_SLOTS = 1 << TAA_MODEL_BITS };
enum { TAA_MF_DONE = 1, TAA_MF_TIMEVAR = 2, TAA_MF_TRANSL = 4 };
struct TaaModelSlot { const void *key; Int verts; Int polys; UnsignedByte flags; };
static TaaModelSlot s_modelSlots[TAA_MODEL_SLOTS];
static float s_reactiveW  = 0.5f;	// `taa reactive` - 0 off, else the history weight cap there (1 = clamp only)
// Ronin @feature 26/09/2026 DX9: AUTO-REACTIVE, FSR2's generated mask. The frame is copied just before particles and
// sorted translucency draw; where the final frame differs from the copy an effect drew, and the history is clamped
// there. Particles have no velocity, so nothing else clamps them. `taa autoreact` = the threshold, 0 = off.
static float         s_autoReact   = 0.03f;
// Ronin @bugfix 08/10/2026 DX9: `taa autofull` - the difference at which an effect is FULLY reactive; proportional in between.
// On/off left a hard-edged field with no AA round thin smoke. At or below `autoreact` it is the old on/off again.
static float         s_autoFull    = 0.20f;
static float autoReactSlope(void) { const float span = s_autoFull - s_autoReact; return (span > 0.001f) ? 1.0f / span : 1000.0f; }
// Ronin @feature 12/10/2026 DX9: HDR stage 1b: a raw texture in the scene target's own format (it is compared with the scene; 8
// bits would clip what the sun's lift raised). Not the texture manager's: let go in releaseResources before a reset.
static IDirect3DTexture9 *s_opaqueTex = NULL;
static D3DFORMAT     s_opaqueFormat = D3DFMT_UNKNOWN;
static UnsignedInt   s_opaqueW = 0, s_opaqueH = 0;
static Bool          s_opaqueOK    = FALSE;
static void releaseOpaque(void)
{
	if (s_opaqueTex != NULL)
		s_opaqueTex->Release();
	s_opaqueTex    = NULL;
	s_opaqueFormat = D3DFMT_UNKNOWN;
	s_opaqueW      = 0;
	s_opaqueH      = 0;
	s_opaqueOK     = FALSE;
}
static UnsignedInt   s_opaqueStamp = 0;
// Ronin @bugfix 26/09/2026 DX9: the build-placement bib (red while the spot is illegal). Terrain geometry that follows the
// cursor: no velocity, no depth change, so the pixels it left kept it in their history - the red trail. Its world corners
// come from W3DTerrainVisual::addFactionBibDrawable, refreshed every other frame; flagged reactive while fresh.
static Vector3      s_bibCorners[4];
static UnsignedInt  s_bibMovedFrame = 0;	// s_moverFrame when its corners last changed; 0 = never
// Ronin @bugfix 27/09/2026 DX9: flagged only while it MOVES (3 frames: InGameUI refreshes it every other frame). A bib
// parked on an illegal spot keeps normal TAA; one that just vanished is reset for those frames, clearing its red.
static inline Bool  cursorBibLive(void) { return s_bibMovedFrame != 0 && (s_moverFrame - s_bibMovedFrame) <= 3; }
// Ronin @bugfix 26/10/2026 DX9: decals that move - the ability rings on the cursor, a unit's decal. Terrain geometry with no velocity,
// as the bib: marked for 3 frames after each move, on a grid at the ground's height (a ring is wider than the ground is flat).
// Ronin @bugfix 27/10/2026 DX9: only where the decal's own texture shows (TaaDecalMark_vs/_ps): its whole square reset half the screen.
// Ronin @bugfix 27/10/2026 DX9: with their own motion (last frame's corners), and a free decal - the cursor's ring - every frame it
// shows, so the moving-shadow mask leaves it alone; a unit's decal while it moves.
enum { TAA_DECAL_MARKS = 64, TAA_DECAL_GRID = 16 };
struct TaaDecalMark
{
	const void *key; Vector3 corner[4]; Vector3 prevCorner[4]; UnsignedInt movedFrame; UnsignedInt seenFrame;
	TextureClass *texture; Bool additive; Bool attached;
};
static IDirect3DVertexShader9 *s_decalMarkVS = NULL;
static IDirect3DPixelShader9  *s_decalMarkPS = NULL;
static TaaDecalMark s_decalMarks[TAA_DECAL_MARKS];
static Int          s_decalMarkCount = 0;
static inline Bool  decalMarkMoving(const TaaDecalMark &m) { return m.movedFrame != 0 && (s_moverFrame - m.movedFrame) <= 3; }
static inline Bool  decalMarkDrawn(const TaaDecalMark &m)  { return m.seenFrame == s_moverFrame && (!m.attached || decalMarkMoving(m)); }
// Ronin @bugfix 29/09/2026 DX9: the water. It animates every frame but the mesh velocity pass never sees it, so it kept
// full history and smeared: its polygons are flagged reactive in the velocity pass. `taa water`.
enum { TAA_WATER_MAX_VERTS = 3 * 4096 };
static Vector3 s_waterVerts[TAA_WATER_MAX_VERTS];
static Int     s_waterVertCount = 0;
static Bool    s_waterMask = TRUE;
// Ronin @bugfix 03/10/2026 DX9: the water marks its OWN pixels - WaterSea_ps writes COLOR1 into this target (render target
// 1 around each water draw) and the velocity pass copies the marks in. The flat polygons above are the fallback.
static TextureClass          *s_waterMaskTex   = NULL;
static UnsignedInt            s_waterMaskW     = 0;
static UnsignedInt            s_waterMaskH     = 0;
static IDirect3DPixelShader9 *s_waterMaskPS    = NULL;	// TaaWaterMask.pso
static Bool                   s_waterMaskReady = FALSE;	// this frame's target is cleared and may be bound
static Bool                   s_waterMaskDrawn = FALSE;	// a water draw bound it this frame
static Bool                   s_waterMaskLast  = FALSE;	// the frame before's, for the panel
static Int                    s_waterMaskCaps  = -1;		// -1 not asked yet; 1 = two render targets, blended
// Ronin @bugfix 09/10/2026 DX9: the local lights' change mask (beginLightMask). A still pixel under a light that changed had no
// evidence of change, so its history lagged while every mover's shadow footprint, clamped, showed the light as it is.
static TextureClass          *s_lightMaskTex   = NULL;
static UnsignedInt            s_lightMaskW = 0, s_lightMaskH = 0;
static UnsignedInt            s_lightMaskStamp = 0;		// s_moverFrame it was drawn in
static Int                    s_lightMaskCaps  = -1;		// -1 not asked yet; 1 = two render targets, blended
// Ronin @bugfix 09/10/2026 DX9: the selection flash is drawn AFTER the resolve. In the history a few-frame tint lagged,
// and every way of hurrying the history cost its anti-aliasing (AntiAliasing_Work.md 22b). See ownsSelectionFlash.
static UnsignedInt            s_flashAsked     = 0;		// s_moverFrame the scene last asked - proof it skips the flash itself
static TextureClass          *s_flashTex       = NULL;	// rgb = the flash colour of the mesh at this pixel, a = 1 where one flashes
static UnsignedInt            s_flashW = 0, s_flashH = 0;
static UnsignedInt            s_flashStamp     = 0;		// s_moverFrame the mask was drawn in
static const float            TAA_FLASH_GAIN   = 2.0f;	// the scene added the tint to the ambient AND to every light
// Ronin @bugfix 09/10/2026 DX9: a mesh drawn last frame and not this one leaves its last box, drawn as a GHOST: reset.
// More than TAA_MAX_VANISH in one frame is a cut or a fast scroll, where the whole picture is clamped anyway.
enum { TAA_MAX_VANISH = 64 };
static float                  s_vanishBox[TAA_MAX_VANISH][6];	// centre xyz, extent xyz
static Int                    s_vanishCount    = 0;
// Ronin @bugfix 01/11/2026 DX9: the fog of war's change mask (updateFogMask): a = 255 at the shroud texels that changed this
// frame or last; the resolve reads it at each pixel's ground position (s12, c25..c29).
static TextureClass          *s_fogMaskTex     = NULL;	// MANAGED, the shroud texture's size
static unsigned char         *s_fogPrev        = NULL;	// last frame's shroud texels, raw
static unsigned char         *s_fogAge         = NULL;	// frames each texel stays marked
static UnsignedInt            s_fogW = 0, s_fogH = 0, s_fogBpp = 0;
static Bool                   s_fogLitLast     = FALSE;	// marked last frame: the mask must be rewritten
static Bool                   s_fogReady       = FALSE;	// marked this frame: the resolve binds it
static float                  s_toFogT[16];				// screen clip -> fog texture uv, transposed
static const float            TAA_JUMP_DIST2   = 16.0f;	// Ronin @bugfix 09/10/2026 DX9: a still mesh that moved further than 4 units in one frame jumped
static float s_disoccV    = 0.5f;	// Ronin @bugfix 26/09/2026 DX9: `taa disoccv` - px a mover must have moved to vacate a pixel
static Int   s_reactCount = 0;
// Ronin @diagnostic 25/09/2026 DX9: meshes that found no slot this frame, so no velocity at all. Should read 0.
static Int   s_trkFull = 0;
static TaaMeshSlot s_meshSlots[TAA_MESH_SLOTS];
static TaaMeshDraw s_meshDraws[TAA_MAX_MESH_DRAWS];
static Int  s_meshDrawCount = 0;
static Int  s_meshSeen      = 0;
static Int  s_skinDrawCount = 0;
static float *s_skinScratch    = NULL;	// interleaved { cur.xyz, prev.xyz } per vertex for the skin draw
static Int    s_skinScratchCap = 0;
// Ronin @feature 24/09/2026 DX9: the clean view and projection CameraClass::Apply sent for the 3D camera, captured
// inside Apply - the view-projection the velocity pass and the reprojection use.
static D3DXMATRIX s_appliedView, s_appliedProj;
static Bool  s_appliedValid = FALSE;
// Ronin @bugfix 24/09/2026 DX9: the viewport the SCENE drew with, read at the first noteMesh of the frame. s_viewport
// is taken in preRender, before the scene's Apply sets it: 1600x900 there, 1600x720 here (no command bar).
static D3DVIEWPORT9 s_meshVp;
static Bool       s_meshVpValid = FALSE;
static IDirect3DVertexShader9 *s_velMeshVS = NULL;
static IDirect3DVertexShader9 *s_velSkinVS = NULL;	// Ronin @feature 26/09/2026 DX9: §14 stage 2
static IDirect3DPixelShader9  *s_velMeshPS = NULL;

// Ronin @bugfix 27/09/2026 DX9: MOVING-SHADOW MASK (`taa shadowmask`). A mesh that moved casts a shadow that moved, onto still
// ground with no motion vector and no depth change: the trail. Every mesh that moved this frame is drawn from the sun's view
// (the shadow map's far cascade covers the whole view): its depth where it is now into RG, where it was last frame into BA.
// The resolve clamps still pixels in or near that shadow, now or last frame. Not a threshold on colour.
// Ronin @bugfix 27/09/2026 DX9: 2048 and 4096 were tried and changed nothing visible.
enum { MOVER_MAP_RES = 1024 };
// Ronin @bugfix 27/09/2026 DX9: world units round a moving shadow that are still treated; 4 and 8 were tried, no change.
static const float MOVER_MARGIN_WORLD = 1.5f;
static const float MOVER_BIAS_WORLD   = 1.0f;	// world units along the sun between a caster and what it shades
static Bool            s_moverOn     = TRUE;		// `taa shadowmask 0|1`
static Bool            s_moverCap    = TRUE;		// Ronin @bugfix 27/09/2026 DX9: `taa maskcap 0|1` - half weight where it changed
static Bool            s_moverFailed = FALSE;	// targets could not be made; stop retrying
static TextureClass   *s_moverTex    = NULL;
static ZTextureClass  *s_moverZ      = NULL;
static IDirect3DPixelShader9 *s_moverPS = NULL;
static Bool            s_moverOK     = FALSE;	// drawn this frame
static Int             s_moverDraws  = 0;		// meshes drawn into it this frame
static float           s_moverToSunT[16];		// resolve c12..c15: screen clip -> sun clip, transposed
static float           s_moverParams[4];		// resolve c16: x = bound, y = half texel, z = dilation (uv), w = depth bias

// Ronin @diagnostic 27/09/2026 DX9: GPU TIME of postRender and of the mask pass, from timestamp queries read GPU_RING frames
// later so nothing waits. The game is frame-capped, so fps cannot show what TAA costs. Shown on the [TAA] row.
enum { GPU_RING = 4, GPU_MARKS = 4 };	// marks: 0 postRender start, 1 mask start, 2 mask end, 3 postRender end
struct TaaGpuFrame { IDirect3DQuery9 *disjoint; IDirect3DQuery9 *freq; IDirect3DQuery9 *t[GPU_MARKS]; Bool issued; };
static TaaGpuFrame s_gpu[GPU_RING];
static Int   s_gpuSlot    = 0;
static Bool  s_gpuOK      = FALSE;
static float s_gpuTotalMs = -1.0f;
static float s_gpuMaskMs  = -1.0f;
static Bool  s_gpuRemake  = FALSE;	// Ronin @bugfix 07/10/2026 DX9: a device reset dropped them; postRender makes them again

static void gpuRelease(void)
{
	for (Int i = 0; i < GPU_RING; ++i)
	{
		TaaGpuFrame &f = s_gpu[i];
		if (f.disjoint != NULL) { f.disjoint->Release(); f.disjoint = NULL; }
		if (f.freq     != NULL) { f.freq->Release();     f.freq     = NULL; }
		for (Int m = 0; m < GPU_MARKS; ++m)
			if (f.t[m] != NULL) { f.t[m]->Release(); f.t[m] = NULL; }
		f.issued = FALSE;
	}
	s_gpuOK = FALSE;
}

static void gpuCreate(IDirect3DDevice9 *dev)
{
	gpuRelease();
	Bool ok = TRUE;
	for (Int i = 0; i < GPU_RING && ok; ++i)
	{
		TaaGpuFrame &f = s_gpu[i];
		ok = SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &f.disjoint)) &&
			 SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &f.freq));
		for (Int m = 0; m < GPU_MARKS && ok; ++m)
			ok = SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &f.t[m]));
	}
	if (ok)
		s_gpuOK = TRUE;
	else
		gpuRelease();		// no timestamp support: the row shows n/a
}

static void gpuMark(Int m)
{
	if (s_gpuOK)
		s_gpu[s_gpuSlot].t[m]->Issue(D3DISSUE_END);
}

// Reads this slot's previous frame, if the GPU has finished it, then opens it for this frame.
static void gpuBegin(void)
{
	if (!s_gpuOK)
		return;
	TaaGpuFrame &f = s_gpu[s_gpuSlot];
	if (f.issued)
	{
		BOOL   disjoint = TRUE;
		UINT64 freq = 0, t[GPU_MARKS] = { 0, 0, 0, 0 };
		Bool   ready = (f.disjoint->GetData(&disjoint, sizeof(disjoint), 0) == S_OK) &&
					   (f.freq->GetData(&freq, sizeof(freq), 0) == S_OK);
		for (Int m = 0; m < GPU_MARKS && ready; ++m)
			ready = (f.t[m]->GetData(&t[m], sizeof(UINT64), 0) == S_OK);
		if (ready && !disjoint && freq > 0 && t[3] >= t[0] && t[2] >= t[1])
		{
			const float total = (float)((double)(t[3] - t[0]) * 1000.0 / (double)freq);
			const float mask  = (float)((double)(t[2] - t[1]) * 1000.0 / (double)freq);
			s_gpuTotalMs = (s_gpuTotalMs < 0.0f) ? total : (s_gpuTotalMs * 0.9f + total * 0.1f);
			s_gpuMaskMs  = (s_gpuMaskMs  < 0.0f) ? mask  : (s_gpuMaskMs  * 0.9f + mask  * 0.1f);
		}
		f.issued = FALSE;
	}
	f.disjoint->Issue(D3DISSUE_BEGIN);
	f.t[0]->Issue(D3DISSUE_END);
}

static void gpuEnd(void)
{
	if (!s_gpuOK)
		return;
	TaaGpuFrame &f = s_gpu[s_gpuSlot];
	f.t[3]->Issue(D3DISSUE_END);
	f.freq->Issue(D3DISSUE_END);
	f.disjoint->Issue(D3DISSUE_END);
	f.issued  = TRUE;
	s_gpuSlot = (s_gpuSlot + 1) % GPU_RING;
}


// Ronin @bugfix 23/09/2026 DX9: the 3D camera, cached in preRender. The view-projection is built from THIS, not
// read off the device - see the note at the capture site.
static const CameraClass *s_camera = NULL;
// Ronin @diagnostic 22/09/2026 DX9: `taa clamp` - clamp strength where there is evidence of change; 0 = off.
static float s_clamp     = 1.0f;

// Ronin @bugfix 27/09/2026 DX9: every float knob lands here, from Options.ini and the console alike. A NaN fails every
// comparison and would pass a plain clamp, so it is replaced by the default first.
static inline float taaKnob(float v, float lo, float hi, float ifNaN)
{
	if (!(v == v))
		return ifNaN;
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}
void  R3DTaa::setSharpen(float s)     { s_sharpen = taaKnob(s, 0.0f, 1.0f, 0.0f); }
void  R3DTaa::setMipBias(float b)     { s_mipBias = taaKnob(b, -2.0f, 0.0f, -1.0f); }
float R3DTaa::getMipBias(void)        { return s_mipBias; }
float R3DTaa::getSharpen(void)        { return s_sharpen; }

// Ronin @feature 12/10/2026 DX9: HDR STAGE 1A - the scene's target as 16-bit float (`hdr`). Nothing else changes yet: the resolve
// still writes 8-bit history and the 8-bit frame. A reset gives the engine's texture its old format; beginFrame asks again.
static Bool s_floatScene   = FALSE;
static Bool s_floatRefused = FALSE;	// the device would not make it: not asked again every frame
// Ronin @feature 12/10/2026 DX9: HDR stage 1b. This frame the scene IS drawn into a float target; and the look's three knobs.
static Bool  s_floatNow    = FALSE;
static Bool  s_floatReady  = FALSE;	// Ronin @feature 17/10/2026 DX9: the float target stands and MSAA is off - TAA or not
static float s_sunGain[4]  = { 2.0f, 1.8f, 2.0f, 1.0f };	// `hdr sun`: morning, afternoon, evening, night
static float s_sunWarm     = 0.35f;						// `hdr warm`
static float s_white       = 1.5f;						// `hdr white`

static void keepSceneFormat(void)
{
	s_floatReady = FALSE;
	IDirect3DTexture9 *tex = W3DShaderManager::getRenderTexture();
	D3DSURFACE_DESC d;
	if (tex == NULL || FAILED(tex->GetLevelDesc(0, &d)))
		return;
	Bool isFloat = (d.Format == D3DFMT_A16B16G16R16F) ? TRUE : FALSE;
	const Bool wantFloat = (s_floatScene && R3DLocalLights::isEnabled()) ? TRUE : FALSE;	// Ronin @feature 23/10/2026 DX9: HDR needs the master
	if (isFloat != wantFloat && !(wantFloat && s_floatRefused))
	{
		if (W3DShaderManager::remakeRenderTexture(wantFloat ? D3DFMT_A16B16G16R16F : D3DFMT_UNKNOWN))
			isFloat = wantFloat;
		else if (wantFloat)
			s_floatRefused = TRUE;
	}
	s_floatReady = (isFloat && DX8Wrapper::Get_Anti_Aliasing_Level() == 0) ? TRUE : FALSE;	// preRender makes it s_floatNow
}

static Int sunGainSlot(void)
{
	const Int slot = (TheGlobalData != NULL) ? (Int)TheGlobalData->m_timeOfDay - 1 : 1;
	return (slot < 0 || slot > 3) ? 1 : slot;
}

void  R3DTaa::setSunGain(float v)	{ s_sunGain[sunGainSlot()] = (v < 1.0f) ? 1.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DTaa::getSunGain(void)		{ return s_sunGain[sunGainSlot()]; }
// Ronin @feature 21/10/2026 DX9: one time of day's gain by its slot (0 morning .. 3 night), for DX9Lights.ini.
void  R3DTaa::setSunGainAt(Int slot, float v)	{ if (slot >= 0 && slot < 4) s_sunGain[slot] = (v < 1.0f) ? 1.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DTaa::getSunGainAt(Int slot)			{ return (slot >= 0 && slot < 4) ? s_sunGain[slot] : 1.0f; }
// Ronin @feature 21/10/2026 DX9: the camera preRender took for this frame's 3D view - SSAO's `aosun` runs the shade pass with it.
const CameraClass *R3DTaa::getSceneCamera(void)	{ return s_camera; }
void  R3DTaa::setSunWarm(float v)	{ s_sunWarm = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DTaa::getSunWarm(void)		{ return s_sunWarm; }
void  R3DTaa::setWhite(float v)		{ s_white = (v < 1.0f) ? 1.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getWhite(void)		{ return s_white; }

// Ronin @feature 12/10/2026 DX9: what the lift adds to a wholly sunlit pixel, as a share of it: gain - 1, less in green and
// blue by `warm` (to 0.90 and 0.72 of it).
Bool R3DTaa::getSunLift(float rgb[3])
{
	const float more = s_sunGain[sunGainSlot()] - 1.0f;
	if (!s_floatNow || more <= 0.0f)
		return FALSE;
	rgb[0] = more;
	rgb[1] = more * (1.0f - 0.10f * s_sunWarm);
	rgb[2] = more * (1.0f - 0.28f * s_sunWarm);
	return TRUE;
}

// Ronin @feature 15/10/2026 DX9: THE NIGHT LOOK (`night`; docs/Lighting_Work.md 4.4c). Decided on Dark Night: tint .55, lo .15,
// hi .60; the floor keeps team colours readable in the dark.
static Bool  s_nightOn    = TRUE;
static float s_nightTint  = 0.55f;
static float s_nightLo    = 0.15f;
static float s_nightHi    = 0.60f;
static float s_nightFloor = 0.30f;
static float s_nightLift  = 0.9f;		// `night lift`: .9 by his word 16/10 (.75 washed the meshes' shading out)
static float s_nightFull  = 0.65f;	// `night full`: Dark Night's light (saturation 0.65) gets all of it, GameData's night (0.33) a third

// Ronin @feature 15/10/2026 DX9: the map's light on level ground now, as the terrain takes it (BaseHeightMap::doTheLight: light 1's
// ambient + each light's diffuse x how far it points down).
static void mapLevelLight(float out[3])
{
	out[0] = out[1] = out[2] = 0.0f;
	if (TheGlobalData == NULL)
		return;
	const GlobalData::TerrainLighting *tl = TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay];
	out[0] = tl[0].ambient.red;
	out[1] = tl[0].ambient.green;
	out[2] = tl[0].ambient.blue;
	for (Int i = 0; i < MAX_GLOBAL_LIGHTS; ++i)
	{
		const float len = sqrtf(tl[i].lightPos.x * tl[i].lightPos.x + tl[i].lightPos.y * tl[i].lightPos.y + tl[i].lightPos.z * tl[i].lightPos.z);
		const float down = (len > 0.0001f && tl[i].lightPos.z < 0.0f) ? (-tl[i].lightPos.z / len) : 0.0f;
		out[0] += tl[i].diffuse.red * down;
		out[1] += tl[i].diffuse.green * down;
		out[2] += tl[i].diffuse.blue * down;
	}
}

// Ronin @feature 15/10/2026 DX9: c17 = the dim's tint - the map light's cast (its colour over its luminance) to the power `tint`,
// back to luminance 1 - and w = on; c18 = lo, hi, floor.
static void nightConstants(float c[8], float levelOut[3], Bool lampMask)
{
	mapLevelLight(levelOut);
	const float *e = levelOut;
	const float l = 0.299f * e[0] + 0.587f * e[1] + 0.114f * e[2];
	float tint[3] = { 1.0f, 1.0f, 1.0f };
	if (l > 0.001f)
	{
		for (Int k = 0; k < 3; ++k)
			tint[k] = powf(((e[k] > 0.0001f) ? e[k] : 0.0001f) / l, s_nightTint);
		const float tl = 0.299f * tint[0] + 0.587f * tint[1] + 0.114f * tint[2];
		for (Int k = 0; k < 3; ++k)
			tint[k] /= tl;
	}
	const Bool on = (s_nightOn && R3DLocalLights::isEnabled() && TheGlobalData != NULL && TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) ? TRUE : FALSE;
	// Ronin @feature 15/10/2026 DX9: the strength follows how coloured the map's light is: the look counters a saturated light.
	const float hi = (e[0] > e[1]) ? ((e[0] > e[2]) ? e[0] : e[2]) : ((e[1] > e[2]) ? e[1] : e[2]);
	const float lo = (e[0] < e[1]) ? ((e[0] < e[2]) ? e[0] : e[2]) : ((e[1] < e[2]) ? e[1] : e[2]);
	const float sat = (hi > 0.0001f) ? ((hi - lo) / hi) : 0.0f;
	const float span = (s_nightFull > 0.16f) ? (s_nightFull - 0.15f) : 0.01f;
	float t = (sat - 0.15f) / span;
	t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
	c[0] = tint[0];
	c[1] = tint[1];
	c[2] = tint[2];
	c[3] = on ? (t * t * (3.0f - 2.0f * t)) : 0.0f;
	c[4] = s_nightLo;
	c[5] = (s_nightHi > s_nightLo + 0.01f) ? s_nightHi : (s_nightLo + 0.01f);
	c[6] = s_nightFloor;
	c[7] = lampMask ? 1.0f : 0.0f;
}

// Ronin @feature 17/10/2026 DX9: THE FINISH PASS - with TAA off or MSAA on, the night look and the dark lift run in a pass of
// their own after the scene (finishScene): the frame is copied and drawn back through the resolve shader's screen copy.
static UnsignedInt   s_frameNo        = 0;		// every frame, TAA or not (beginFrame)
static UnsignedInt   s_lightMaskFrame = 0;		// the s_frameNo the light mask was drawn in
static TextureClass *s_finishCopy     = NULL;	// the frame at its own size and format: StretchRect resolves MSAA into it
static UnsignedInt   s_finishW = 0, s_finishH = 0;
static D3DFORMAT     s_finishFormat   = D3DFMT_UNKNOWN;
// Ronin @feature 21/10/2026 DX9: THE GRADE (`grade`) - the daytime colour, from the "Zero Hour: Enhanced" shots measured: a warm tint
// (luminance kept), more saturation, darker mid-tones. Off by default (DX9Lights.ini sets it); never at night - the night look's.
static Bool  s_gradeOn   = FALSE;
static float s_gradeWarm = 1.0f;		// `grade warm`: 0 = none, 1 = their tint, 1 : 0.88 : 0.72
static float s_gradeSat  = 1.2f;		// `grade sat`: 1 = none; 1.2 his pick 21/10 (the measured 1.6 was too much)
static float s_gradeMids = 1.34f;		// `grade mids`: the mid-tones' power, 1 = none, above 1 darker (their mid grey 0.2, ours 0.3)
static Bool gradeNow(void)
{
	return (s_gradeOn && R3DLocalLights::isEnabled() && TheGlobalData != NULL && TheGlobalData->m_timeOfDay != TIME_OF_DAY_NIGHT) ? TRUE : FALSE;
}

// Ronin @feature 21/10/2026 DX9: the grade into a screen copy of the resolve shader: c20 = (on, saturation, mid-tones' power), c21 = the
// warm tint. Every screen copy sets it, so none reads a stale one.
static void bindGrade(IDirect3DDevice9 *dev)
{
	const float c20[8] = { gradeNow() ? 1.0f : 0.0f, s_gradeSat, s_gradeMids, 0.0f,
						   1.0f, 1.0f - 0.12f * s_gradeWarm, 1.0f - 0.28f * s_gradeWarm, 0.0f };
	dev->SetPixelShaderConstantF(20, c20, 2);
}

void  R3DTaa::setGrade(Bool on)			{ s_gradeOn = on; }
Bool  R3DTaa::getGrade(void)			{ return s_gradeOn; }
void  R3DTaa::setGradeWarm(float v)		{ s_gradeWarm = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getGradeWarm(void)		{ return s_gradeWarm; }
void  R3DTaa::setGradeSat(float v)		{ s_gradeSat = (v < 0.0f) ? 0.0f : ((v > 2.5f) ? 2.5f : v); }
float R3DTaa::getGradeSat(void)			{ return s_gradeSat; }
void  R3DTaa::setGradeMids(float v)		{ s_gradeMids = (v < 0.5f) ? 0.5f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getGradeMids(void)		{ return s_gradeMids; }

// Ronin @feature 22/10/2026 DX9: VIBRANCE and LOCAL CONTRAST (`vibrance`, `localcontrast`), on by default; at night their own pair
// (`nightvibrance`, `nightcontrast`, 23/10). 0 turns each off, and so does `lights 0`.
static float s_vibrance      = 0.8f;
static float s_localContrast = 0.1f;
static float s_nightVibrance = 0.8f;
static float s_nightContrast = 0.4f;
static Bool vividNight(void)	{ return (TheGlobalData != NULL && TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) ? TRUE : FALSE; }
static float vibranceNow(void)	{ return vividNight() ? s_nightVibrance : s_vibrance; }
static float contrastNow(void)	{ return vividNight() ? s_nightContrast : s_localContrast; }
static Bool vividNow(void)
{
	return ((vibranceNow() > 0.0f || contrastNow() > 0.0f) && R3DLocalLights::isEnabled() && TheGlobalData != NULL) ? TRUE : FALSE;
}
void  R3DTaa::setVibrance(float v)		{ s_vibrance = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getVibrance(void)			{ return s_vibrance; }
void  R3DTaa::setLocalContrast(float v)	{ s_localContrast = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getLocalContrast(void)	{ return s_localContrast; }
void  R3DTaa::setNightVibrance(float v)	{ s_nightVibrance = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getNightVibrance(void)	{ return s_nightVibrance; }
void  R3DTaa::setNightContrast(float v)	{ s_nightContrast = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DTaa::getNightContrast(void)	{ return s_nightContrast; }
static void releaseSurround(void);		// local contrast's surroundings, beside drawFinish

static Bool finishWanted(void)
{
	if (s_active)
		return FALSE;
	if (gradeNow())
		return TRUE;		// Ronin @feature 21/10/2026 DX9: the grade is drawn by the finish pass
	if (vividNow())
		return TRUE;		// Ronin @feature 22/10/2026 DX9: so are vibrance and local contrast
	if (!s_nightOn || !R3DLocalLights::isEnabled() || TheGlobalData == NULL || TheGlobalData->m_timeOfDay != TIME_OF_DAY_NIGHT)
		return FALSE;
	if (s_nightLift < 0.999f)
		return TRUE;
	float c[8], e[3];
	nightConstants(c, e, FALSE);
	return (c[3] > 0.0f) ? TRUE : FALSE;
}

static float clamp01(float v)	{ return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
void  R3DTaa::setNight(Bool on)			{ s_nightOn = on; }
Bool  R3DTaa::getNight(void)			{ return s_nightOn; }
void  R3DTaa::setNightTint(float v)		{ s_nightTint = clamp01(v); }
float R3DTaa::getNightTint(void)		{ return s_nightTint; }
void  R3DTaa::setNightLo(float v)		{ s_nightLo = clamp01(v); }
float R3DTaa::getNightLo(void)			{ return s_nightLo; }
void  R3DTaa::setNightHi(float v)		{ s_nightHi = clamp01(v); }
float R3DTaa::getNightHi(void)			{ return s_nightHi; }
void  R3DTaa::setNightFloor(float v)	{ s_nightFloor = clamp01(v); }
float R3DTaa::getNightFloor(void)		{ return s_nightFloor; }
void  R3DTaa::setNightLift(float v)		{ s_nightLift = (v < 0.4f) ? 0.4f : ((v > 1.0f) ? 1.0f : v); }
float R3DTaa::getNightLift(void)		{ return s_nightLift; }
void  R3DTaa::setNightFull(float v)		{ s_nightFull = (v < 0.2f) ? 0.2f : ((v > 1.0f) ? 1.0f : v); }
float R3DTaa::getNightFull(void)		{ return s_nightFull; }

void R3DTaa::describeNight(char *out, Int size)
{
	if (out == NULL || size <= 0)
		return;
	float c[8], e[3];
	nightConstants(c, e, FALSE);
	const Bool night = (TheGlobalData != NULL && TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) ? TRUE : FALSE;
	_snprintf(out, size, "night: %d (%s)  lift %.2f  strength %.2f (full at %.2f)  tint %.2f  lo %.2f  hi %.2f  floor %.2f;  the map's light now %.2f %.2f %.2f (blue %.1f x red) -> the dim's tint %.2f %.2f %.2f",
			  s_nightOn ? 1 : 0, !night ? "waits for night" : (s_active ? "showing" : ((DX8Wrapper::Get_Anti_Aliasing_Level() != 0) ?
			  "showing in its own pass - MSAA: lamps keep their colour by brightness only" : "showing in its own pass (TAA off)")), s_nightLift, c[3], s_nightFull,
			  s_nightTint, s_nightLo, c[5], s_nightFloor, e[0], e[1], e[2], (e[0] > 0.0001f) ? (e[2] / e[0]) : 0.0f, c[0], c[1], c[2]);
	out[size - 1] = 0;
}

void R3DTaa::setFloatScene(Bool on)
{
	s_floatScene   = on;
	s_floatRefused = FALSE;
	keepSceneFormat();		// the panel runs between frames: never while the scene is redirected
}

Bool R3DTaa::getFloatScene(void)	{ return s_floatScene; }

void R3DTaa::describeScene(char *out, Int size)
{
	if (out == NULL || size <= 0)
		return;
	IDirect3DTexture9 *tex = W3DShaderManager::getRenderTexture();
	D3DSURFACE_DESC d;
	const char *name = "none";
	UnsignedInt w = 0, h = 0;
	if (tex != NULL && SUCCEEDED(tex->GetLevelDesc(0, &d)))
	{
		name = (d.Format == D3DFMT_A16B16G16R16F) ? "A16B16G16R16F (16-bit float)"
			 : ((d.Format == D3DFMT_A8R8G8B8) ? "A8R8G8B8 (8-bit)" : ((d.Format == D3DFMT_X8R8G8B8) ? "X8R8G8B8 (8-bit)" : "another format"));
		w = d.Width;
		h = d.Height;
	}
	_snprintf(out, size, "hdr: %d (needs MSAA off)%s  scene target: %s, %u x %u. The scene %s  sun %.2f (this time of day; all four %.2f %.2f %.2f %.2f)  warm %.2f  white %.2f",
			  s_floatScene ? 1 : 0, s_floatRefused ? " (REFUSED by the device)" : "", name, w, h,
			  s_active ? "is drawn into it (TAA runs)." : ((DX8Wrapper::Get_Anti_Aliasing_Level() != 0) ?
			  "is NOT drawn into it: MSAA is on." : (s_floatReady ? "is drawn into it (TAA off: the tone curve alone brings it back)." :
			  "is NOT drawn into it: TAA and hdr are off - it goes straight to the frame buffer.")),
			  s_sunGain[sunGainSlot()], s_sunGain[0], s_sunGain[1], s_sunGain[2], s_sunGain[3], s_sunWarm, s_white);
	out[size - 1] = 0;
}
void  R3DTaa::setClamp(float v)       { s_clamp = taaKnob(v, 0.0f, 1.0f, 1.0f); }
float R3DTaa::getClamp(void)          { return s_clamp; }
void  R3DTaa::setShadowMask(Bool on)  { s_moverOn = on; }
Bool  R3DTaa::getShadowMask(void)     { return s_moverOn; }
void  R3DTaa::setMaskCap(Bool on)     { s_moverCap = on; }
Bool  R3DTaa::getMaskCap(void)        { return s_moverCap; }
Int   R3DTaa::getShadowMaskDraws(void) { return s_moverOK ? s_moverDraws : -1; }
void  R3DTaa::getGpuMs(float *total, float *mask)
{
	if (total != NULL) *total = s_gpuOK ? s_gpuTotalMs : -1.0f;
	if (mask  != NULL) *mask  = s_gpuOK ? s_gpuMaskMs  : -1.0f;
}


void  R3DTaa::setWeight(float w)    { s_weight = taaKnob(w, 0.0f, 0.99f, 0.9f); s_historyValid = FALSE; }
float R3DTaa::getWeight(void)       { return s_weight; }
void  R3DTaa::setDebug(Int mode)    { s_debugMode = mode; }
Int   R3DTaa::getDebug(void)        { return s_debugMode; }


// Ronin @feature 20/09/2026 DX9: Halton(2,3). A low-discrepancy sequence beats random offsets because 8 consecutive
// samples are guaranteed to spread evenly over the pixel — random ones clump, and a clump is a frame that adds no new
// information. Centred on 0 so the average offset over the sequence is zero and the image does not drift.
static float halton(Int index, Int base)
{
	float result = 0.0f;
	float f      = 1.0f / (float)base;
	Int   i      = index;
	while (i > 0)
	{
		result += f * (float)(i % base);
		i      /= base;
		f      /= (float)base;
	}
	return result;
}

static void currentJitterPixels(float *outX, float *outY)
{
	if (!s_active)
	{
		*outX = 0.0f;
		*outY = 0.0f;
		return;
	}
	// Halton is 1-based here: index 0 returns 0 for every base, which would waste a sample on the pixel centre.
	*outX = halton(s_phase + 1, 2) - 0.5f;
	*outY = halton(s_phase + 1, 3) - 0.5f;
}

void R3DTaa::beginFrame(void)
{
	++s_frameNo;		// Ronin @feature 17/10/2026 DX9: the finish pass's frame stamp
	// Ronin @feature 26/09/2026 DX9: Options.ini `DX9TAA`, `DX9TAASharpness`, `DX9TAAMipBias` apply at the first frame; the console
	// still overrides them for the session.
	static Bool s_iniApplied = FALSE;
	if (!s_iniApplied && TheGlobalData != NULL)
	{
		s_iniApplied = TRUE;
		setEnabled(TheGlobalData->m_taaEnabled);
		setSharpen(TheGlobalData->m_taaSharpness);	// Options.ini DX9TAASharpness, 0..1
		setMipBias(TheGlobalData->m_taaMipBias);		// Options.ini DX9TAAMipBias, -2..0
		setFloatScene(TheGlobalData->m_hdr);			// Ronin @feature 21/10/2026 DX9: Options.ini DX9HDR, the player's
	}
	// Ronin @feature 26/09/2026 DX9: never under MSAA - INTZ has no multisampled form, so there would be no reprojection
	// and no velocity, only jitter. Checked every frame: the options menu can switch MSAA while the game runs.
	const Bool active = (s_enabled && DX8Wrapper::Get_Anti_Aliasing_Level() == 0) ? TRUE : FALSE;
	if (active && !s_active)
		s_historyValid = FALSE;		// the history is from before the pause
	s_active = active;
	keepSceneFormat();		// Ronin @feature 12/10/2026 DX9: HDR stage 1a: `hdr`, again after a device reset

	// Ronin @feature 26/09/2026 DX9: the mip bias, on every sampler, only while TAA runs. Written every frame while non-zero
	// so a device reset (which restores 0) cannot drop it; once back at 0 it is left alone - zero cost with TAA off.
	const float wantBias = s_active ? s_mipBias : 0.0f;
	if (wantBias != 0.0f || s_lastMipBias != 0.0f)
	{
		IDirect3DDevice9 *biasDev = DX8Wrapper::_Get_D3D_Device8();
		if (biasDev != NULL)
			for (DWORD st = 0; st < 16; ++st)
				biasDev->SetSamplerState(st, D3DSAMP_MIPMAPLODBIAS, *(const DWORD *)&wantBias);
	}
	s_lastMipBias = wantBias;

	if (!s_active)
	{
		s_phase = 0;			// settle back to the start, so enabling always begins at the same sample
		return;
	}
	s_phase = (s_phase + 1) % SAMPLE_COUNT;

	// Ronin @feature 20/09/2026 DX9: reprojection needs the INTZ scene depth. SSAO begins it when its quality is above
	// Off and it runs BEFORE us in W3DDisplay::draw, so its isActive() already answers "did anyone take it". Only start
	// our own when nobody did — Begin_Scene_Depth refuses a second start, and two owners would fight over the end.
	s_ownsDepth = FALSE;
	if (!R3DSsao::isActive())
		s_ownsDepth = DX8Wrapper::Begin_Scene_Depth() ? TRUE : FALSE;
}

void R3DTaa::endFrame(void)
{
	if (!s_ownsDepth)
		return;					// SSAO started it, SSAO ends it
	s_ownsDepth = FALSE;
	DX8Wrapper::End_Scene_Depth();
}

Bool R3DTaa::isActive(void) { return s_active; }
Bool R3DTaa::ownsDepth(void) { return s_ownsDepth; }

void R3DTaa::setEnabled(Bool on)
{
	if (s_enabled == on)
		return;
	s_enabled      = on;
	s_phase        = 0;
	s_historyValid = FALSE;		// the accumulated image is meaningless across a toggle
}


// Ronin @bugfix 29/09/2026 DX9: TRUE between preRender and postRender - the main scene is drawing into the TAA target.
static Bool s_redirected = FALSE;

// Ronin @feature 20/09/2026 DX9: pixels -> NDC. A shift of one pixel is 2/width in NDC because NDC spans -1..1, and the
// projection's off-centre terms (matrix4.h Init_Perspective: Row[0][2], Row[1][2]) are in exactly those units.
// The sign is NEGATED because this port's frustum is column-vector with w = -z: adding j to Row[0][2] contributes
// j*z to clip.x, and clip.x/w = j*z/(-z) = -j. Y flips again because NDC +Y is up while screen +Y is down.
void R3DTaa::getJitterNDC(float *outX, float *outY)
{
	if (outX != NULL) *outX = 0.0f;
	if (outY != NULL) *outY = 0.0f;
	// Ronin @bugfix 29/09/2026 DX9: only the main scene jitters. The water mirror reuses the main camera BEFORE the redirect,
	// into its own target: jittered, the reflection jumped every frame.
	if (!s_active || !s_redirected)
		return;

	Int width = 0, height = 0, bits = 0;
	bool windowed = false;
	WW3D::Get_Render_Target_Resolution(width, height, bits, windowed);
	if (width <= 0 || height <= 0)
		return;

	float jx = 0.0f, jy = 0.0f;
	currentJitterPixels(&jx, &jy);

	if (outX != NULL) *outX = -(2.0f * jx / (float)width);
	if (outY != NULL) *outY =  (2.0f * jy / (float)height);
}

// ---------------------------------------------------------------------------------------------------------------
// Ronin @feature 20/09/2026 DX9: TAA step 2a — the render-to-texture wrap and an IDENTITY resolve.
// Nothing here blends yet. The point is to prove the scene can be redirected into a texture and drawn back through
// our own shader WITHOUT changing a single pixel. Step 2b replaces the resolve with the real history blend.
// ---------------------------------------------------------------------------------------------------------------

static IDirect3DPixelShader9  *s_resolvePS    = NULL;
static IDirect3DVertexShader9 *s_quadVS       = NULL;
static Bool                    s_shadersTried = FALSE;
// Ronin @bugfix 29/09/2026 DX9: s_redirected moved above getJitterNDC, which reads it.

// Ronin @feature 20/09/2026 DX9: TAA step 2b-i. History ping-pong. POOL_DEFAULT render targets through TextureClass so
// DX8TextureManagerClass recreates them across a device Reset — the same rule SSAO's targets follow.
// s_historyValid is FALSE until a frame has actually been written: the resolve must never blend into an unwritten buffer.
static TextureClass *s_history[2]   = { NULL, NULL };
// Ronin @bugfix 09/10/2026 DX9: the history's low bits, a second 8-bit pair written as render target 1. On a still
// pixel 0.9 x history + 0.1 x frame rounds back to the history once they are under 5/255 apart: ghosts never left.
static TextureClass *s_historyLo[2] = { NULL, NULL };
static UnsignedInt   s_historyLoW = 0, s_historyLoH = 0;
static UnsignedInt   s_lowStamp   = 0;		// s_moverFrame the low bits were last written in
static UnsignedInt   s_historyW     = 0;
static UnsignedInt   s_historyH     = 0;
static Int           s_historyIndex  = 0;

Int  R3DTaa::getMoverCount(void)   { return s_meshDrawCount; }
Int  R3DTaa::getMeshSeen(void)     { return s_meshSeen; }
void  R3DTaa::setReactive(float w) { s_reactiveW = taaKnob(w, 0.0f, 1.0f, 0.5f); }
float R3DTaa::getReactive(void)    { return s_reactiveW; }
void  R3DTaa::setDisoccV(float px) { s_disoccV = taaKnob(px, 0.0f, 8.0f, 0.5f); }
float R3DTaa::getDisoccV(void)     { return s_disoccV; }
void  R3DTaa::setAutoReact(float t) { s_autoReact = taaKnob(t, 0.0f, 1.0f, 0.03f); }
float R3DTaa::getAutoReact(void)    { return s_autoReact; }
void  R3DTaa::setAutoFull(float t)  { s_autoFull = taaKnob(t, 0.0f, 1.0f, 0.20f); }
float R3DTaa::getAutoFull(void)     { return s_autoFull; }
Bool  R3DTaa::getOpaqueOK(void)     { return s_opaqueOK; }
void  R3DTaa::noteCursorBib(const Vector3 *corners)
{
	if (!s_active || corners == NULL)
		return;
	Bool moved = FALSE;
	for (Int i = 0; i < 4; ++i)
	{
		if (fabsf(corners[i].X - s_bibCorners[i].X) > 0.01f || fabsf(corners[i].Y - s_bibCorners[i].Y) > 0.01f)
			moved = TRUE;
		s_bibCorners[i] = corners[i];
	}
	if (moved)
		s_bibMovedFrame = (s_moverFrame != 0) ? s_moverFrame : 1;
}

void  R3DTaa::noteMovingDecal(const void *key, const Vector3 &centre, const Vector3 *corners, TextureClass *texture, Bool additive,
							  Bool attached)
{
	if (!s_active || corners == NULL || texture == NULL)
		return;
	Vector3 world[4];
	for (Int i = 0; i < 4; ++i)
		world[i].Set(centre.X + corners[i].X, centre.Y + corners[i].Y, 0.0f);
	const UnsignedInt now = (s_moverFrame != 0) ? s_moverFrame : 1;
	TaaDecalMark *mark = NULL;
	TaaDecalMark *oldest = NULL;
	for (Int m = 0; m < s_decalMarkCount; ++m)
	{
		if (s_decalMarks[m].key == key)
		{
			mark = &s_decalMarks[m];
			break;
		}
		if (oldest == NULL || s_decalMarks[m].seenFrame < oldest->seenFrame)
			oldest = &s_decalMarks[m];
	}
	if (mark == NULL)
	{
		// One that just appeared has no trail yet. A full table gives up the one unseen longest.
		if (s_decalMarkCount < TAA_DECAL_MARKS)
		{
			mark = &s_decalMarks[s_decalMarkCount++];
			mark->texture = NULL;
		}
		else
			mark = oldest;
		mark->key = key;
		mark->movedFrame = 0;
		mark->seenFrame = 0;
		for (Int i = 0; i < 4; ++i)
			mark->corner[i] = mark->prevCorner[i] = world[i];
	}
	// Last frame's corners, once a frame (the mirror draws the decals too).
	if (mark->seenFrame != now)
		for (Int i = 0; i < 4; ++i)
			mark->prevCorner[i] = mark->corner[i];
	mark->attached = attached;
	// Held, not borrowed: a mark outlives its decal by up to 3 frames.
	if (mark->texture != texture)
	{
		texture->Add_Ref();
		REF_PTR_RELEASE(mark->texture);
		mark->texture = texture;
	}
	mark->additive = additive;
	Bool moved = FALSE;
	for (Int i = 0; i < 4; ++i)
		if (fabsf(world[i].X - mark->corner[i].X) > 0.01f || fabsf(world[i].Y - mark->corner[i].Y) > 0.01f)
			moved = TRUE;
	if (moved)
	{
		for (Int i = 0; i < 4; ++i)
			mark->corner[i] = world[i];
		mark->movedFrame = now;
	}
	mark->seenFrame = now;
}

void R3DTaa::setWaterMask(Bool on)   { s_waterMask = on; }

// Ronin @bugfix 09/10/2026 DX9: 1 / the map's own light on open ground, per channel, so frame x this ~ the surface's colour - what the
// selection flash is added to. Terrain values at the map's time of day: ambient plus each light by how steeply it comes down.
void R3DTaa::getMapLightFactor(float out[3])
{
	static const float LIGHT_MIN = 0.25f, LIGHT_MAX = 1.0f;	// bounds on very dark or very bright maps
	float sum[3] = { 0.0f, 0.0f, 0.0f };
	if (TheGlobalData != NULL)
	{
		for (Int i = 0; i < MAX_GLOBAL_LIGHTS; ++i)
		{
			const GlobalData::TerrainLighting &tl = TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][i];
			const float len = sqrtf(tl.lightPos.x * tl.lightPos.x + tl.lightPos.y * tl.lightPos.y + tl.lightPos.z * tl.lightPos.z);
			float down = (len > 0.0001f) ? (-tl.lightPos.z / len) : 0.0f;
			if (down < 0.0f)
				down = 0.0f;
			sum[0] += tl.ambient.red   + tl.diffuse.red   * down;
			sum[1] += tl.ambient.green + tl.diffuse.green * down;
			sum[2] += tl.ambient.blue  + tl.diffuse.blue  * down;
		}
	}
	for (Int c = 0; c < 3; ++c)
	{
		float v = sum[c];
		if (v < LIGHT_MIN) v = LIGHT_MIN;
		if (v > LIGHT_MAX) v = LIGHT_MAX;
		out[c] = 1.0f / v;
	}
}

// Ronin @bugfix 09/10/2026 DX9: RTS3DScene::renderOneObject asks before it adds a drawable's selection flash to its lights. TRUE = leave
// it out: TAA draws it after the resolve. Only inside the redirected 3D render; the mirror and the shadow passes keep it.
Bool R3DTaa::ownsSelectionFlash(void)
{
	if (!s_redirected || !s_velocityOn || s_velMeshVS == NULL || s_velMeshPS == NULL || s_resolvePS == NULL)
		return FALSE;
	s_flashAsked = s_moverFrame;
	return TRUE;
}
Bool R3DTaa::getWaterMask(void)      { return s_waterMask; }
Int  R3DTaa::getWaterMaskTris(void)  { return s_waterVertCount / 3; }

static inline void addWaterTri(const ICoord3D &a, const ICoord3D &b, const ICoord3D &c)
{
	if (s_waterVertCount + 3 > TAA_WATER_MAX_VERTS)
		return;
	s_waterVerts[s_waterVertCount++].Set((Real)a.x, (Real)a.y, (Real)a.z);
	s_waterVerts[s_waterVertCount++].Set((Real)b.x, (Real)b.y, (Real)b.z);
	s_waterVerts[s_waterVertCount++].Set((Real)c.x, (Real)c.y, (Real)c.z);
}

// Ronin @bugfix 29/09/2026 DX9: this frame's water triangles from the map's water polygons, in the shapes W3DWater.cpp
// draws (quads fanned from point 0; river strips bank to bank). Nothing when the flat water is switched off.
static void buildWaterMask(void)
{
	s_waterVertCount = 0;
	if (!s_waterMask || TheWaterRenderObj == NULL || TheWaterDebug.skipFlat)
		return;
	for (PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig != NULL; pTrig = pTrig->getNext())
	{
		const Int n = pTrig->getNumPoints();
		if (!pTrig->isWaterArea() || n <= 2)
			continue;
		if (pTrig->isRiver())
		{
			Int innerNdx = pTrig->getRiverStart();
			Int outerNdx = innerNdx + 1;
			if (innerNdx >= n - 1)
				continue;	// drawRiverWater draws nothing either
			ICoord3D prevA, prevB;
			for (Int i = 0; i < n / 2; ++i)
			{
				const ICoord3D a = *pTrig->getPoint(outerNdx);	// drawRiverWater's vertex 2i
				const ICoord3D b = *pTrig->getPoint(innerNdx);	// and 2i+1
				outerNdx++;
				innerNdx--;
				if (innerNdx < 0)
					innerNdx = n - 1;
				if (outerNdx >= n)
					outerNdx = 0;
				if (i > 0)
				{
					addWaterTri(prevA, prevB, b);	// its (2i, 2i+1, 2i+3) and (2i, 2i+3, 2i+2), one step back
					addWaterTri(prevA, b, a);
				}
				prevA = a;
				prevB = b;
			}
		}
		else
		{
			const ICoord3D p3 = *pTrig->getPoint(0);
			for (Int k = 1; k < n - 1; k += 2)
			{
				const ICoord3D p2 = *pTrig->getPoint(k);
				const ICoord3D p1 = *pTrig->getPoint(k + 1);
				const ICoord3D p0 = (k + 2 < n) ? *pTrig->getPoint(k + 2) : p1;
				addWaterTri(p0, p1, p2);
				addWaterTri(p0, p2, p3);
			}
		}
	}
}

// Ronin @feature 26/09/2026 DX9: called by RTS3DScene::Flush in the main pass, just before particles and the sorted
// translucent flush. The first call of the frame is the one: later flushes draw on top of it.
void R3DTaa::noteOpaqueDone(void)
{
	// Ronin @feature 23/10/2026 DX9: the shafts' dust, after the copy below - on every path, TAA or not: to the resolve it is an effect.
	struct DustAfter { ~DustAfter() { R3DLocalLights::drawShaftDust(); } } dustAfter;
	if (!s_redirected || s_autoReact <= 0.0f || s_opaqueStamp == s_moverFrame)
		return;
	s_opaqueStamp = s_moverFrame;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL)
		return;
	IDirect3DSurface9 *rt = NULL;
	if (FAILED(dev->GetRenderTarget(0, &rt)) || rt == NULL)
		return;
	D3DSURFACE_DESC desc;
	if (SUCCEEDED(rt->GetDesc(&desc)))
	{
		if (s_opaqueTex == NULL || s_opaqueW != desc.Width || s_opaqueH != desc.Height || s_opaqueFormat != desc.Format)
		{
			releaseOpaque();
			if (FAILED(dev->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT,
										  &s_opaqueTex, NULL)))
			{
				s_opaqueTex = NULL;
			}
			else
			{
				s_opaqueW = desc.Width;
				s_opaqueH = desc.Height;
				s_opaqueFormat = desc.Format;
			}
		}
		if (s_opaqueTex != NULL)
		{
			IDirect3DSurface9 *dst = NULL;
			if (SUCCEEDED(s_opaqueTex->GetSurfaceLevel(0, &dst)) && dst != NULL)
			{
				s_opaqueOK = SUCCEEDED(dev->StretchRect(rt, NULL, dst, NULL, D3DTEXF_NONE)) ? TRUE : FALSE;
				dst->Release();
			}
		}
	}
	rt->Release();
}

// The snapshot is usable only if it was taken this frame, of a target the size of the one being resolved, and last
// frame's depth history exists to carry the flag forward - an unbound stage reads white, which would flag everything.
static Bool autoReactUsable(Int w, Int h)
{
	return (s_autoReact > 0.0f && s_opaqueOK && s_opaqueTex != NULL &&
			s_opaqueW == (UnsignedInt)w && s_opaqueH == (UnsignedInt)h &&
			s_depthHist[0] != NULL && s_depthHist[1] != NULL) ? TRUE : FALSE;
}
Int   R3DTaa::getReactiveCount(void) { return s_reactCount; }
Int   R3DTaa::getTrackMisses(void) { return s_trkFull; }
Int   R3DTaa::getSkinDrawCount(void) { return s_skinDrawCount; }
void  R3DTaa::setDisocc(Int m)     { s_disocc = (m < 0) ? 0 : ((m > 2) ? 2 : m); }
Int   R3DTaa::getDisocc(void)      { return s_disocc; }

// Ronin @feature 24/09/2026 DX9: called from CameraClass::Apply for the TAA camera, BEFORE the jitter is added, with
// exactly the projection and view it is about to send. The last call before postRender is the scene's.
void R3DTaa::noteCameraApplied(const Matrix4x4 &d3dProjClean, const Matrix3D &view)
{
	s_appliedProj  = D3DXMATRIX(To_D3DMATRIX(d3dProjClean));
	s_appliedView  = D3DXMATRIX(To_D3DMATRIX(view));
	s_appliedValid = TRUE;
}

// Ronin @bugfix 26/09/2026 DX9: Fibonacci hashing takes the HIGH bits of the product. The low bits depend only on the
// key's low bits, which pool-allocated meshes share at a fixed stride - chains clustered and overflowed (trk=13).
static inline UnsignedInt taaHashPtr(const void *key, Int bits)
{
	return ((UnsignedInt)((size_t)key >> 3) * 2654435761u) >> (32 - bits);
}

// Ronin @bugfix 26/09/2026 DX9: search the WHOLE chain for the key before claiming a free slot. Taking the first freed
// slot met could leave the mesh's real slot further on, throwing away its previous transform.
static TaaMeshSlot *findMeshSlot(const void *key)
{
	const UnsignedInt h = taaHashPtr(key, TAA_MESH_BITS);
	TaaMeshSlot *freeSlot = NULL;
	for (Int probe = 0; probe < 32; ++probe)
	{
		TaaMeshSlot &sl = s_meshSlots[(h + probe) & (TAA_MESH_SLOTS - 1)];
		if (sl.key == key)
			return &sl;
		if (freeSlot == NULL && (sl.key == NULL || (s_moverFrame - sl.stamp) > TAA_STALE_FRAMES))
			freeSlot = &sl;
		if (sl.key == NULL)
			break;		// never used: nothing was ever placed past it
	}
	if (freeSlot == NULL)
	{
		++s_trkFull;
		return NULL;		// locally full; this mesh gets no velocity this frame
	}
	freeSlot->key        = key;
	freeSlot->stamp      = 0;		// 0 = no previous transform yet
	freeSlot->movedStamp = 0;
	freeSlot->skinCount  = 0;		// buffers kept for reuse, contents invalid
	freeSlot->rootStamp  = 0;
	freeSlot->boxOK      = FALSE;
	return freeSlot;
}

static inline Bool taaShaderTranslucent(const ShaderClass &sh)
{
	return (sh.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO || sh.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_DISABLE);
}

// Ronin @feature 24/09/2026 DX9: classified once per model. Translucent = pass 0 blends or writes no depth, on most of
// its polygons; a later additive pass does not count. The counts guard against a freed model's address being reused.
static UnsignedByte classifyModel(MeshClass *mesh, MeshModelClass *mdl)
{
	const Int nv = mdl->Get_Vertex_Count();
	const Int np = mdl->Get_Polygon_Count();
	const UnsignedInt h = taaHashPtr(mdl, TAA_MODEL_BITS);
	TaaModelSlot *sl = NULL;
	for (Int probe = 0; probe < 16; ++probe)
	{
		TaaModelSlot &m = s_modelSlots[(h + probe) & (TAA_MODEL_SLOTS - 1)];
		if (m.key == mdl && m.verts == nv && m.polys == np)
			return m.flags;
		if (m.key == NULL || m.key == mdl)
		{
			sl = &m;
			break;
		}
	}
	if (sl == NULL)
		sl = &s_modelSlots[h];		// locally full: evict

	UnsignedByte f = TAA_MF_DONE;
	MaterialInfoClass *mi = mesh->Get_Material_Info();
	if (mi != NULL)
	{
		if (mi->Has_Time_Variant_Texture_Mappers())
			f |= TAA_MF_TIMEVAR;
		mi->Release_Ref();
	}
	if (mdl->Get_Pass_Count() > 0)
	{
		Int trans = 0, total = 1;
		if (mdl->Has_Shader_Array(0))
		{
			total = np;
			for (Int i = 0; i < np; ++i)
				if (taaShaderTranslucent(mdl->Get_Shader(i, 0)))
					++trans;
		}
		else if (taaShaderTranslucent(mdl->Get_Single_Shader(0)))
			trans = 1;
		if (trans * 2 > total)
			f |= TAA_MF_TRANSL;
	}
	sl->key   = mdl;
	sl->verts = nv;
	sl->polys = np;
	sl->flags = f;
	return f;
}

// Ronin @feature 24/09/2026 DX9: MeshClass::Render calls this for every mesh drawn outside the shadow depth pass. Only
// meshes inside the TAA-redirected 3D render count, and each is taken once a frame - the water reflection redraws the
// scene, and a second note would read back the transform the first one just stored.
void R3DTaa::noteMesh(MeshClass *mesh)
{
	if (!s_redirected || !s_velocityOn || mesh == NULL)
		return;
	++s_meshSeen;
	if (s_meshSeen == 1)
	{
		IDirect3DDevice9 *d = DX8Wrapper::_Get_D3D_Device8();
		s_meshVpValid = (d != NULL && SUCCEEDED(d->GetViewport(&s_meshVp))) ? TRUE : FALSE;
	}
	MeshModelClass *mdl = mesh->Peek_Model();
	if (mdl == NULL)
		return;
	// Ronin @feature 26/09/2026 DX9: §14 stage 2 - skins too. Get_Deformed_Vertices needs the container's HTree.
	const Bool isSkin = mdl->Get_Flag(MeshGeometryClass::SKIN) ? TRUE : FALSE;
	if (isSkin && (mesh->Get_Container() == NULL || mesh->Get_Container()->Get_HTree() == NULL))
		return;
	TaaMeshSlot *sl = findMeshSlot(mesh);
	if (sl == NULL || sl->stamp == s_moverFrame)
		return;

	Matrix3D cur  = mesh->Get_Transform();
	Matrix3D prev = sl->prev;
	// The previous transform must be LAST frame's - a mesh off screen for a while would otherwise get a velocity
	// spanning all the frames it was missing.
	const Bool hadPrev = (sl->stamp != 0) && (s_moverFrame - sl->stamp == 1);
	const Bool movedLast = (sl->movedStamp != 0) && (s_moverFrame - sl->movedStamp == 1);
	sl->prev  = cur;
	sl->stamp = s_moverFrame;
	// Ronin @bugfix 09/10/2026 DX9: where it is now - its world box - for the frame it is no longer drawn (postRender).
	{
		const AABoxClass &wb = mesh->Get_Bounding_Box();
		sl->boxC[0] = wb.Center.X; sl->boxC[1] = wb.Center.Y; sl->boxC[2] = wb.Center.Z;
		sl->boxE[0] = wb.Extent.X; sl->boxE[1] = wb.Extent.Y; sl->boxE[2] = wb.Extent.Z;
		sl->boxOK   = TRUE;
	}

	// Ronin @bugfix 09/10/2026 DX9: SELECTION FLASH. Its drawable's flash colour, when the scene left it out this frame (it asked:
	// s_flashAsked). The velocity pass marks this mesh's pixels with it and the screen copy adds it after the resolve.
	RenderObjClass *top = mesh;
	while (top->Get_Container() != NULL)
		top = top->Get_Container();
	const DrawableInfo *topInfo = (const DrawableInfo *)top->Get_User_Data();
	float flash[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if (s_flashAsked == s_moverFrame && topInfo != NULL && topInfo->m_drawable != NULL)
	{
		const Vector3 *sel = topInfo->m_drawable->getSelectionColor();
		if (sel != NULL && (sel->X > 0.004f || sel->Y > 0.004f || sel->Z > 0.004f))
		{
			flash[0] = sel->X;
			flash[1] = sel->Y;
			flash[2] = sel->Z;
			flash[3] = 1.0f;
		}
	}

	Bool moved = FALSE;
	const Vector3 *skinCur = NULL, *skinPrev = NULL;
	if (isSkin)
	{
		// Deformed this frame into the buffer that is NOT last frame's; compared vertex by vertex, since an idle
		// infantryman's transform never changes while his mesh breathes.
		const Int nv = mdl->Get_Vertex_Count();
		if (nv <= 0 || nv > 65535)
			return;
		if (sl->skinCap < nv)
		{
			delete [] sl->skin[0];
			delete [] sl->skin[1];
			sl->skin[0]   = W3DNEWARRAY Vector3[nv];
			sl->skin[1]   = W3DNEWARRAY Vector3[nv];
			sl->skinCap   = nv;
			sl->skinCount = 0;
		}
		const Int ci = sl->skinCur ^ 1;
		mesh->Get_Deformed_Vertices(sl->skin[ci]);
		const Bool prevOK = hadPrev && (sl->skinCount == nv);
		if (prevOK)
		{
			const Vector3 *a = sl->skin[ci];
			const Vector3 *b = sl->skin[ci ^ 1];
			for (Int v = 0; v < nv && !moved; ++v)
				if (fabsf(a[v].X - b[v].X) > 1.0e-4f || fabsf(a[v].Y - b[v].Y) > 1.0e-4f || fabsf(a[v].Z - b[v].Z) > 1.0e-4f)
					moved = TRUE;
		}
		sl->skinCur   = ci;
		sl->skinCount = nv;
		skinCur  = sl->skin[ci];
		skinPrev = prevOK ? sl->skin[ci ^ 1] : sl->skin[ci];
	}
	else if (hadPrev)
		for (Int r = 0; r < 3 && !moved; ++r)
			for (Int c = 0; c < 4; ++c)
				if (fabsf(cur[r][c] - prev[r][c]) > 1.0e-4f) { moved = TRUE; break; }
	if (moved)
		sl->movedStamp = s_moverFrame;
	const Bool vel = hadPrev && moved;

	// Ronin @feature 24/09/2026 DX9: reactive meshes are drawn whether they moved or not - a conveyor never moves.
	// A STATIC translucent mesh is left alone: over still ground its history is valid.
	UnsignedByte f = (s_reactiveW > 0.0f) ? classifyModel(mesh, mdl) : 0;
	// Ronin @feature 09/10/2026 DX9: its scrolling, see-through polygons are a beam that is not drawn (a spot light stands in): what is
	// left of the mesh is the lamp's housing, and marking the beam's footprint reactive would cost that area its AA.
	if (R3DLocalLights::beamsHiddenIn(mesh))
		f &= (UnsignedByte)~(TAA_MF_TIMEVAR | TAA_MF_TRANSL);
	if (f & TAA_MF_TRANSL)
		sl->boxOK = FALSE;		// Ronin @bugfix 09/10/2026 DX9: a translucent mesh that goes (a muzzle flash) is the reactive mask's, not a box reset
	// Ronin @bugfix 26/09/2026 DX9: translucent at RUNTIME - an opacity override draws an opaque model through the alpha shader
	// (the build-placement preview, fading units); the model's shaders cannot say so. Moving, it is a GHOST (27/09).
	// Get_Alpha_Override is last frame's: Render sets it from rinfo after this note - one frame late at a change, no more.
	const Bool ghost = (s_reactiveW > 0.0f && mesh->Get_Alpha_Override() < 1.0f) ? TRUE : FALSE;
	Int kind = -1;
	if (ghost)
	{
		if (vel)
			kind = TAA_KIND_GHOST;
	}
	else if (f & TAA_MF_TRANSL)
	{
		if (vel || (f & TAA_MF_TIMEVAR))
			kind = TAA_KIND_TRANSLUCENT;
	}
	else if (vel)
		kind = (f & TAA_MF_TIMEVAR) ? TAA_KIND_VEL_REACT : TAA_KIND_VEL;
	else if (f & TAA_MF_TIMEVAR)
		kind = TAA_KIND_REACT;
	// Ronin @bugfix 25/09/2026 DX9: drawn ONE more frame after it stops, so the disocclusion test sees it is still here.
	// Without it every pixel of a unit that just stopped read as vacated ground and reset: a second of shimmer.
	else if (movedLast)
		kind = TAA_KIND_STILL;
	// Ronin @bugfix 27/09/2026 DX9: IDLE INFANTRY - the mesh moved but its object did not: the top render object (the one the
	// scene holds, carrying the DrawableInfo) kept last frame's transform. Last frame's must be known, else it counts as moving.
	if (kind == TAA_KIND_VEL)
	{
		const Matrix3D root = top->Get_Transform();
		Bool rootStill = (sl->rootStamp != 0 && s_moverFrame - sl->rootStamp == 1) ? TRUE : FALSE;
		for (Int r = 0; r < 3 && rootStill; ++r)
			for (Int c = 0; c < 4; ++c)
				if (fabsf(root[r][c] - sl->prevRoot[r][c]) > 1.0e-4f) { rootStill = FALSE; break; }
		sl->prevRoot  = root;
		sl->rootStamp = s_moverFrame;
		if (rootStill && topInfo != NULL && topInfo->m_drawable != NULL && topInfo->m_drawable->isKindOf(KINDOF_INFANTRY))
			kind = TAA_KIND_VEL_IDLE;
	}
	if (isSkin && kind == TAA_KIND_TRANSLUCENT)
		kind = -1;		// skins draw after the translucent pass; none are translucent in practice
	// Ronin @bugfix 09/10/2026 DX9: a flashing mesh with nothing else to say is drawn as STILL (present, no mover) - only so the
	// velocity pass reaches it and can mark its pixels for the flash.
	if (flash[3] > 0.5f && kind < 0)
		kind = TAA_KIND_STILL;
	// Ronin @bugfix 09/10/2026 DX9: a still mesh that JUMPS (the rally flag re-placed) was in no velocity target last frame, so nothing
	// marked the place it left and it faded there for a second. Its old footprint, drawn as a GHOST, is reset.
	if (vel && !movedLast && !isSkin && s_meshDrawCount < TAA_MAX_MESH_DRAWS &&
		(cur.Get_Translation() - prev.Get_Translation()).Length2() > TAA_JUMP_DIST2)
	{
		TaaMeshDraw &g = s_meshDraws[s_meshDrawCount++];
		g.mesh = mesh;
		g.cur  = prev;
		g.prev = prev;
		g.kind = TAA_KIND_GHOST;
		g.skinCur  = NULL;
		g.skinPrev = NULL;
		g.flash[0] = g.flash[1] = g.flash[2] = g.flash[3] = 0.0f;
		++s_reactCount;
	}
	if (kind < 0 || s_meshDrawCount >= TAA_MAX_MESH_DRAWS)
		return;

	TaaMeshDraw &d = s_meshDraws[s_meshDrawCount++];
	d.mesh = mesh;
	d.cur  = cur;
	d.prev = hadPrev ? prev : cur;
	d.kind = kind;
	d.skinCur  = skinCur;
	d.skinPrev = skinPrev;
	d.flash[0] = flash[0];
	d.flash[1] = flash[1];
	d.flash[2] = flash[2];
	d.flash[3] = flash[3];
	if (isSkin)
		++s_skinDrawCount;
	if (kind != TAA_KIND_VEL && kind != TAA_KIND_STILL && kind != TAA_KIND_VEL_IDLE)
		++s_reactCount;
}
void R3DTaa::setVelocity(Bool on)  { s_velocityOn = on; }
Bool R3DTaa::getVelocity(void)     { return s_velocityOn; }



// Ronin @feature 20/09/2026 DX9: TAA step 2b-ii. This frame's and last frame's UNJITTERED view-projection, for the
// reprojection.
// Ronin @bugfix 20/09/2026 DX9: these are D3D-convention matrices read straight off the DEVICE, not built from
// Matrix4x4. Matrix4x4 is column-vector and D3D is row-vector, so building here and reasoning about HLSL's matrix
// packing on top gave two chances to get the handedness wrong — and it got it wrong. R3DShadowMap.cpp:1509-1518 is the
// convention that demonstrably works in this codebase: GetTransform, multiply view*proj, transpose, mul(rowVec, M).
// Both of these are JITTER-FREE. The device's projection carries this frame's subpixel offset; it is stripped before
// either is built, so a still camera produces an exactly zero reprojection delta. See the note at the D3DXMatrixInverse
// call for why cancelling the jitter here would destroy the accumulation rather than stabilise it.
static D3DXMATRIX s_curViewProjClean;	// this frame, jitter removed — also becomes next frame's s_prevViewProj
static D3DXMATRIX s_prevViewProj;		// last frame's, likewise clean
static Bool       s_curViewProjValid  = FALSE;
static Bool       s_prevViewProjValid = FALSE;

// Ronin @bugfix 20/09/2026 DX9: the 3D viewport, captured in preRender BEFORE the render target changes.
// SetRenderTarget resets the viewport to the whole target — this codebase says so in three places — so asking the
// device for it after endRenderToTexture returns the FULL FRAME BUFFER, not the 3D sub-rect. Reading it there made the
// shader map NDC across the whole screen: every reprojection delta wrong by the ratio between the two, scaling with
// distance from centre, which is exactly what the radial smear was.
static D3DVIEWPORT9 s_viewport  = { 0, 0, 0, 0, 0.0f, 1.0f };

static Bool         s_viewportOK = FALSE;
static float        s_zNear      = 1.0f;
static float        s_zFar       = 1000.0f;



// Ronin @feature 23/09/2026 DX9: the velocity target and last frame's, ping-ponged. Full resolution: per pixel and exact.
static Bool ensureVelocity(UnsignedInt width, UnsignedInt height)
{
	const UnsignedInt vw = (width  > 0) ? width  : 1;
	const UnsignedInt vh = (height > 0) ? height : 1;
	if (s_velTex != NULL && s_velPrevTex != NULL && s_velW == vw && s_velH == vh)
		return TRUE;

	REF_PTR_RELEASE(s_velTex);
	REF_PTR_RELEASE(s_velPrevTex);
	s_prevVelOK = FALSE;
	s_velW = 0;
	s_velH = 0;
	s_velTex     = NEW_REF(TextureClass, (vw, vh, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
										  TextureClass::POOL_DEFAULT, true));
	s_velPrevTex = NEW_REF(TextureClass, (vw, vh, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
										  TextureClass::POOL_DEFAULT, true));
	if (s_velTex->Peek_D3D_Base_Texture() == NULL || s_velPrevTex->Peek_D3D_Base_Texture() == NULL)
	{
		REF_PTR_RELEASE(s_velTex);
		REF_PTR_RELEASE(s_velPrevTex);
		return FALSE;
	}
	s_velW = vw;
	s_velH = vh;
	return TRUE;
}

// Ronin @bugfix 03/10/2026 DX9: the water's own mask. preRender: make the target the frame buffer's size and clear it.
static void prepareWaterMask(IDirect3DDevice9 *dev)
{
	if (!s_waterMask || s_waterMaskPS == NULL || s_quadVS == NULL)
		return;
	if (s_waterMaskCaps < 0)
	{
		D3DCAPS9 caps;
		s_waterMaskCaps = (SUCCEEDED(dev->GetDeviceCaps(&caps)) && caps.NumSimultaneousRTs >= 2 &&
						   (caps.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING) != 0) ? 1 : 0;
	}
	if (s_waterMaskCaps != 1)
		return;
	IDirect3DSurface9 *rt0 = NULL;
	if (FAILED(dev->GetRenderTarget(0, &rt0)) || rt0 == NULL)
		return;
	D3DSURFACE_DESC desc;
	if (SUCCEEDED(rt0->GetDesc(&desc)))
	{
		if (s_waterMaskTex == NULL || s_waterMaskW != desc.Width || s_waterMaskH != desc.Height)
		{
			REF_PTR_RELEASE(s_waterMaskTex);
			s_waterMaskTex = NEW_REF(TextureClass, (desc.Width, desc.Height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
													TextureClass::POOL_DEFAULT, true));
			if (s_waterMaskTex->Peek_D3D_Base_Texture() == NULL)
				REF_PTR_RELEASE(s_waterMaskTex);
			s_waterMaskW = desc.Width;
			s_waterMaskH = desc.Height;
		}
		IDirect3DSurface9 *surf = (s_waterMaskTex != NULL) ? s_waterMaskTex->Get_D3D_Surface_Level() : NULL;
		if (surf != NULL)
		{
			if (SUCCEEDED(dev->SetRenderTarget(0, surf)))
			{
				dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
				s_waterMaskReady = TRUE;
			}
			dev->SetRenderTarget(0, rt0);
			if (s_viewportOK)
				dev->SetViewport(&s_viewport);	// SetRenderTarget reset it to the whole target
			surf->Release();
		}
	}
	rt0->Release();
}

// Around a water draw (W3DWater.cpp). TRUE = render target 1 is the mask; call endWaterMask after the draw. Only inside
// the TAA redirect and over a target the mask's size.
Bool R3DTaa::beginWaterMask(void)
{
	if (!s_redirected || !s_waterMaskReady || s_waterMaskTex == NULL)
		return FALSE;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface9 *rt0 = NULL;
	if (dev == NULL || FAILED(dev->GetRenderTarget(0, &rt0)) || rt0 == NULL)
		return FALSE;
	D3DSURFACE_DESC desc;
	const Bool same = (SUCCEEDED(rt0->GetDesc(&desc)) && desc.Width == s_waterMaskW && desc.Height == s_waterMaskH &&
					   desc.MultiSampleType == D3DMULTISAMPLE_NONE) ? TRUE : FALSE;
	rt0->Release();
	if (!same)
		return FALSE;
	IDirect3DSurface9 *surf = s_waterMaskTex->Get_D3D_Surface_Level();
	if (surf == NULL)
		return FALSE;
	const Bool ok = SUCCEEDED(dev->SetRenderTarget(1, surf)) ? TRUE : FALSE;
	surf->Release();
	if (ok)
	{
		dev->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
													 D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		s_waterMaskDrawn = TRUE;
	}
	return ok;
}

void R3DTaa::endWaterMask(void)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev != NULL)
		dev->SetRenderTarget(1, NULL);
}

Bool R3DTaa::getWaterMaskDrawn(void) { return s_waterMaskLast; }

// Ronin @feature 18/10/2026 DX9: screen-space reflections - read during this frame's scene, before postRender overwrites the older
// buffer: the history and depth last written, with their camera; the one before them for the change mask (NULL: every hit marks).
Bool R3DTaa::getLastFrame(IDirect3DTexture9 **colour, IDirect3DTexture9 **depth, float c[24], IDirect3DTexture9 **older,
						  float cOlder[24])
{
	const Int last = s_historyIndex ^ 1;
	const Int prev = s_historyIndex;
	if (!s_active)
	{
		// Ronin @feature 19/10/2026 DX9: without TAA, the frame storeLastFrame kept - last frame's only. No change mask: nothing
		// accumulates, so nothing ghosts.
		if (s_reflFrame == 0 || s_reflFrame + 1 != s_frameNo || s_reflColour == NULL || s_reflDepth == NULL)
			return FALSE;
		*colour = s_reflColour->Peek_D3D_Texture();
		*depth  = s_reflDepth->Peek_D3D_Texture();
		if (*colour == NULL || *depth == NULL)
			return FALSE;
		D3DXMATRIX keptT;
		D3DXMatrixTranspose(&keptT, &s_reflViewProj);
		memcpy(c, (const float *)&keptT, sizeof(float) * 16);
		for (Int k = 0; k < 4; ++k)
			c[16 + k] = s_reflView[k];
		c[20] = 1.0f;
		c[21] = s_reflFar;
		c[22] = 0.0f;
		c[23] = 0.0f;
		*older = NULL;
		memset(cOlder, 0, sizeof(float) * 24);
		return TRUE;
	}
	if (!s_historyValid || !s_histWhole[last] || s_history[last] == NULL || s_depthHist[last] == NULL)
		return FALSE;
	*colour = s_history[last]->Peek_D3D_Texture();
	*depth  = s_depthHist[last]->Peek_D3D_Texture();
	if (*colour == NULL || *depth == NULL)
		return FALSE;
	D3DXMATRIX vpT;
	D3DXMatrixTranspose(&vpT, &s_histViewProj[last]);
	memcpy(c, (const float *)&vpT, sizeof(float) * 16);
	for (Int k = 0; k < 4; ++k)
		c[16 + k] = s_histView[last][k];
	c[20] = 1.0f;
	c[21] = s_zFar;		// last frame's: preRender sets this frame's
	c[22] = 0.0f;		// the caller's: how far a ray runs, how thick a hit may be
	c[23] = 0.0f;
	*older = NULL;
	memset(cOlder, 0, sizeof(float) * 24);
	cOlder[22] = TAA_REFLMARK_LEAST;
	cOlder[23] = TAA_REFLMARK_GAIN;
	if (s_histFrame[prev] != 0 && s_histWhole[prev] && s_history[prev] != NULL && s_histFrame[last] - s_histFrame[prev] <= 4)
	{
		*older = s_history[prev]->Peek_D3D_Texture();
		if (*older != NULL)
		{
			D3DXMatrixTranspose(&vpT, &s_histViewProj[prev]);
			memcpy(cOlder, (const float *)&vpT, sizeof(float) * 16);
			for (Int k = 0; k < 4; ++k)
				cOlder[16 + k] = s_histView[prev][k];
			cOlder[21] = 1.0f;		// y: the older picture is there (x is the draw's: the mask is bound)
		}
	}
	return TRUE;
}

// Ronin @feature 18/10/2026 DX9: the light-change mask at target 0's size, cleared once a frame: the reflective meshes mark it in the
// scene, the lights after them. NULL when it cannot pair with target 0 (MSAA); sceneOnly: nor off the TAA frame's size.
static IDirect3DSurface9 *lightMaskSurface(IDirect3DDevice9 *dev, Bool sceneOnly)
{
	if (s_lightMaskCaps < 0)
	{
		D3DCAPS9 caps;
		s_lightMaskCaps = (SUCCEEDED(dev->GetDeviceCaps(&caps)) && caps.NumSimultaneousRTs >= 2 &&
						   (caps.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING) != 0) ? 1 : 0;
	}
	if (s_lightMaskCaps != 1)
		return NULL;
	IDirect3DSurface9 *rt0 = NULL;
	if (FAILED(dev->GetRenderTarget(0, &rt0)) || rt0 == NULL)
		return NULL;
	D3DSURFACE_DESC desc;
	const Bool plain = (SUCCEEDED(rt0->GetDesc(&desc)) && desc.MultiSampleType == D3DMULTISAMPLE_NONE) ? TRUE : FALSE;
	rt0->Release();
	if (!plain || (sceneOnly && (desc.Width != s_historyW || desc.Height != s_historyH)))
		return NULL;
	if (s_lightMaskTex == NULL || s_lightMaskW != desc.Width || s_lightMaskH != desc.Height)
	{
		REF_PTR_RELEASE(s_lightMaskTex);
		s_lightMaskW = 0;
		s_lightMaskH = 0;
		s_lightMaskFrame = 0;		// a new one is cleared below
		s_lightMaskTex = NEW_REF(TextureClass, (desc.Width, desc.Height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
												 TextureClass::POOL_DEFAULT, true));
		if (s_lightMaskTex->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_lightMaskTex);	// a macro with its own if - keep the braces
			return NULL;
		}
		s_lightMaskW = desc.Width;
		s_lightMaskH = desc.Height;
	}
	IDirect3DSurface9 *surf = s_lightMaskTex->Get_D3D_Surface_Level();
	if (surf == NULL)
		return NULL;
	if (s_lightMaskFrame != s_frameNo || s_lightMaskStamp != s_moverFrame)
	{
		if (FAILED(dev->ColorFill(surf, NULL, D3DCOLOR_ARGB(0, 0, 0, 0))))	// not Clear: that wipes every bound target
		{
			surf->Release();
			return NULL;
		}
		s_lightMaskStamp = s_moverFrame;
		s_lightMaskFrame = s_frameNo;
	}
	return surf;
}

// Ronin @bugfix 09/10/2026 DX9: around R3DLocalLights' draws. TRUE = render target 1 is this frame's light-change mask, cleared;
// call endLightMask after the draws. Only inside the TAA redirect - there is no MSAA then, so two targets can pair.
Bool R3DTaa::beginLightMask(void)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	// Ronin @feature 17/10/2026 DX9: or for the finish pass - it keeps a lamp-lit pixel's colour by it. Not under MSAA (lightMaskSurface).
	if ((!s_redirected && !finishWanted()) || dev == NULL)
		return FALSE;
	IDirect3DSurface9 *surf = lightMaskSurface(dev, FALSE);
	if (surf == NULL)
		return FALSE;
	const Bool ok = SUCCEEDED(dev->SetRenderTarget(1, surf)) ? TRUE : FALSE;
	surf->Release();
	if (ok)		// Ronin @feature 19/10/2026 DX9: the lights' change in green, lamp light in alpha: red is the reflections'
	{
		dev->GetRenderState(D3DRS_COLORWRITEENABLE1, &s_lightMaskWrite);
		dev->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_ALPHA);
	}
	s_lightMaskSet = ok;
	return ok;
}

void R3DTaa::endLightMask(void)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL)
		return;
	dev->SetRenderTarget(1, NULL);
	// Ronin @bugfix 19/10/2026 DX9: the write mask back: the sun's and the trees' shade records bind target 1 and keep whatever it is
	// (green + alpha left their red and blue unwritten - magenta meshes at night without TAA).
	if (s_lightMaskSet)
		dev->SetRenderState(D3DRS_COLORWRITEENABLE1, s_lightMaskWrite);
	s_lightMaskSet = FALSE;
}

// Ronin @feature 18/10/2026 DX9: the reflections' change mask - the light mask's red for ReflectiveRigid_ps COLOR1, where a pane's
// reflection changed: the resolve clamps it as a light's change. Only over the TAA frame, never over another pass's target 1.
Bool R3DTaa::beginReflectMask(void)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (!s_redirected || s_reflMark <= 0 || s_reflMarkBound || dev == NULL)
		return FALSE;
	IDirect3DSurface9 *other = NULL;
	if (SUCCEEDED(dev->GetRenderTarget(1, &other)) && other != NULL)
	{
		other->Release();
		return FALSE;
	}
	IDirect3DSurface9 *surf = lightMaskSurface(dev, TRUE);
	if (surf == NULL)
		return FALSE;
	s_reflMarkBound = SUCCEEDED(dev->SetRenderTarget(1, surf)) ? TRUE : FALSE;
	surf->Release();
	if (s_reflMarkBound)
	{
		dev->GetRenderState(D3DRS_COLORWRITEENABLE1, &s_reflMarkWrite);
		// Ronin @feature 19/10/2026 DX9: red = the reflection changed, blue = a unit was at its hit (green and alpha: the lights')
		dev->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_BLUE);
		// Ronin @feature 19/10/2026 DX9: the mover mark's inputs - last frame's velocity and the frame before's, as the history pair
		// (alpha = what drew there), on s11 / s12; c75 = (both there, one texel in uv).
		const Bool vel = (s_prevVelOK && s_olderVelOK && s_velPrevTex != NULL && s_velTex != NULL && s_velW == s_historyW &&
						  s_velH == s_historyH) ? TRUE : FALSE;
		dev->SetTexture(11, vel ? s_velPrevTex->Peek_D3D_Texture() : NULL);
		dev->SetTexture(12, vel ? s_velTex->Peek_D3D_Texture() : NULL);
		for (DWORD st = 11; st <= 12; ++st)
		{
			dev->SetSamplerState(st, D3DSAMP_MINFILTER, D3DTEXF_POINT);		// packed: never blended
			dev->SetSamplerState(st, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			dev->SetSamplerState(st, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			dev->SetSamplerState(st, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			dev->SetSamplerState(st, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		}
		const float c75[4] = { vel ? 1.0f : 0.0f, 1.0f / (float)((s_velW > 0) ? s_velW : 1), 1.0f / (float)((s_velH > 0) ? s_velH : 1), 0.0f };
		dev->SetPixelShaderConstantF(75, c75, 1);
	}
	return s_reflMarkBound;
}

void R3DTaa::endReflectMask(void)
{
	if (!s_reflMarkBound)
		return;
	s_reflMarkBound = FALSE;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL)
		return;
	dev->SetRenderTarget(1, NULL);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE1, s_reflMarkWrite);
	dev->SetTexture(11, NULL);		// Ronin @feature 19/10/2026 DX9: the velocity pair goes back to postRender
	dev->SetTexture(12, NULL);
}

void R3DTaa::setReflMark(Int mode)	{ s_reflMark = (mode < 0) ? 0 : ((mode > 2) ? 2 : mode); }
void R3DTaa::setLastFrameWanted(Bool on)	{ s_lastFrameWanted = on; }
Int  R3DTaa::getReflMark(void)		{ return s_reflMark; }

// Ronin @bugfix 09/10/2026 DX9: the selection-flash mask, made the frame's size, cleared and bound as render target 1. ColorFill,
// not Clear - that wipes every bound target. A TextureClass target, so a device reset is the texture manager's.
static Bool bindFlashMask(IDirect3DDevice9 *dev, UnsignedInt width, UnsignedInt height)
{
	if (s_flashTex == NULL || s_flashW != width || s_flashH != height)
	{
		REF_PTR_RELEASE(s_flashTex);
		s_flashW = 0;
		s_flashH = 0;
		s_flashTex = NEW_REF(TextureClass, (width, height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
		if (s_flashTex->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_flashTex);	// a macro with its own if - keep the braces
			return FALSE;
		}
		s_flashW = width;
		s_flashH = height;
	}
	IDirect3DSurface9 *surf = s_flashTex->Get_D3D_Surface_Level();
	if (surf == NULL)
		return FALSE;
	const Bool ok = (SUCCEEDED(dev->ColorFill(surf, NULL, D3DCOLOR_ARGB(0, 0, 0, 0))) &&
					 SUCCEEDED(dev->SetRenderTarget(1, surf))) ? TRUE : FALSE;
	surf->Release();
	if (ok)
	{
		dev->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
													 D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		s_flashStamp = s_moverFrame;
	}
	return ok;
}

// Ronin @bugfix 09/10/2026 DX9: the low-bits pair, the size of the history. TextureClass targets, so a device reset is the texture
// manager's. FALSE = not makeable: the history stays 8-bit.
static Bool ensureHistoryLow(UnsignedInt width, UnsignedInt height)
{
	if (s_historyLo[0] != NULL && s_historyLo[1] != NULL && s_historyLoW == width && s_historyLoH == height)
		return TRUE;
	REF_PTR_RELEASE(s_historyLo[0]);
	REF_PTR_RELEASE(s_historyLo[1]);
	s_historyLoW = 0;
	s_historyLoH = 0;
	s_lowStamp   = 0;
	for (Int i = 0; i < 2; ++i)
	{
		s_historyLo[i] = NEW_REF(TextureClass, (width, height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
												 TextureClass::POOL_DEFAULT, true));
		if (s_historyLo[i]->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_historyLo[0]);
			REF_PTR_RELEASE(s_historyLo[1]);
			return FALSE;
		}
	}
	s_historyLoW = width;
	s_historyLoH = height;
	return TRUE;
}

static Bool ensureHistory(UnsignedInt width, UnsignedInt height)
{
	if (s_history[0] != NULL && s_history[1] != NULL && s_historyW == width && s_historyH == height)
		return TRUE;

	REF_PTR_RELEASE(s_history[0]);
	REF_PTR_RELEASE(s_history[1]);
	s_historyW     = 0;
	s_historyH     = 0;
	s_historyValid = FALSE;		// a resize throws the accumulated image away; it no longer lines up
	s_histFrame[0] = 0;			// Ronin @feature 18/10/2026 DX9: and neither buffer holds a frame (the reflections)
	s_histFrame[1] = 0;

	REF_PTR_RELEASE(s_depthHist[0]);
	REF_PTR_RELEASE(s_depthHist[1]);
	for (Int i = 0; i < 2; ++i)
	{
		s_depthHist[i] = NEW_REF(TextureClass, (width, height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
												TextureClass::POOL_DEFAULT, true));
		s_history[i] = NEW_REF(TextureClass, (width, height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
											  TextureClass::POOL_DEFAULT, true));
		if (s_history[i]->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_history[0]);
			REF_PTR_RELEASE(s_history[1]);
			return FALSE;
		}
	}
	// Ronin @bugfix 21/09/2026 DX9: CLEAR them. A fresh POOL_DEFAULT target holds whatever was in that memory, and
	// alpha is now the motion memory — an uncleared buffer would read back as "everything moved" and reject history
	// across the whole screen for the first several frames after a load or a device reset.
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface9 *oldRT = NULL;
	if (dev != NULL && SUCCEEDED(dev->GetRenderTarget(0, &oldRT)) && oldRT != NULL)
	{
		for (Int i = 0; i < 2; ++i)
		{
			IDirect3DSurface9 *surf = s_history[i]->Get_D3D_Surface_Level();
			if (surf != NULL)
			{
				if (SUCCEEDED(dev->SetRenderTarget(0, surf)))
					dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
				surf->Release();
			}
		}
		dev->SetRenderTarget(0, oldRT);
		oldRT->Release();
	}

	s_historyW = width;
	s_historyH = height;

	return TRUE;
}

// Same blob reader R3DSsao and R3DShadowMap use; file-static in each so no header has to carry it.
static DWORD *readShaderBlob(const char *path)
{
	HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;
	const DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == 0 || fileSize == INVALID_FILE_SIZE || fileSize < sizeof(DWORD))
	{
		CloseHandle(hFile);
		return NULL;
	}
	DWORD *blob = (DWORD *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize);
	if (blob == NULL)
	{
		CloseHandle(hFile);
		return NULL;
	}
	DWORD bytesRead = 0;
	const BOOL ok = ReadFile(hFile, blob, fileSize, &bytesRead, NULL);
	CloseHandle(hFile);
	if (!ok || bytesRead != fileSize)
	{
		HeapFree(GetProcessHeap(), 0, blob);
		return NULL;
	}
	return blob;
}

static void ensureShaders(IDirect3DDevice9 *dev)
{
	if (s_shadersTried)
		return;
	s_shadersTried = TRUE;

	DWORD *blob = readShaderBlob("shaders\\ScreenQuad.vso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreateVertexShader(blob, &s_quadVS)))
			s_quadVS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaResolve.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_resolvePS)))
			s_resolvePS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaVelMesh.vso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreateVertexShader(blob, &s_velMeshVS)))
			s_velMeshVS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaVelSkin.vso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreateVertexShader(blob, &s_velSkinVS)))
			s_velSkinVS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaVelMesh.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_velMeshPS)))
			s_velMeshPS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	// Ronin @bugfix 27/10/2026 DX9: the moving decals' marks. Missing = no marks (the old trail), never the whole square.
	blob = readShaderBlob("shaders\\TaaDecalMark.vso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreateVertexShader(blob, &s_decalMarkVS)))
			s_decalMarkVS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaDecalMark.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_decalMarkPS)))
			s_decalMarkPS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaDepthStore.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_depthStorePS)))
			s_depthStorePS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	// Ronin @bugfix 27/09/2026 DX9: the moving-shadow mask's pixel shader, and the GPU timer's queries.
	blob = readShaderBlob("shaders\\TaaMoverLight.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_moverPS)))
			s_moverPS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	// Ronin @bugfix 03/10/2026 DX9: the water's own mask, copied into the velocity target. Missing = the flat polygons.
	blob = readShaderBlob("shaders\\TaaWaterMask.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_waterMaskPS)))
			s_waterMaskPS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	gpuCreate(dev);
}

// Ronin @bugfix 20/09/2026 DX9: the filter is per stage and the difference matters.
//   POINT  — the scene and the depth. Both are read 1:1 at this pixel, and interpolating depth across an edge would
//            invent geometry that is not there.
//   LINEAR — the REPROJECTED HISTORY, and it must be. Its lookup lands between texels whenever the camera moves by a
//            fraction of a pixel; point sampling snaps that to the nearest texel, so slow panning quantises the
//            reprojection to whole pixels and the accumulated image ripples like water. Bilinear is what makes a
//            subpixel reprojection mean anything.
static void setResolveSampler(IDirect3DDevice9 *dev, DWORD stage, Bool linearFilter = FALSE)
{
	const DWORD filter = linearFilter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
	dev->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
	dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
	dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	dev->SetSamplerState(stage, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
	dev->SetSamplerState(stage, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
}

// Ronin @bugfix 01/11/2026 DX9: the fog of war's change mask - the shroud texture against last frame's copy, texel by texel. A
// changed texel stays marked this frame and the next ("reactive last frame"). Once a frame, before the resolve.
static void updateFogMask(void)
{
	s_fogReady  = FALSE;
	TextureClass *shroud = W3DShaderManager::getShroudTexture();
	IDirect3DTexture9 *st = (shroud != NULL) ? shroud->Peek_D3D_Texture() : NULL;
	float ox = 0.0f, oy = 0.0f, sx = 0.0f, sy = 0.0f;
	D3DSURFACE_DESC desc;
	if (st == NULL || !W3DShaderManager::getShroudMapState(&ox, &oy, &sx, &sy) || FAILED(st->GetLevelDesc(0, &desc)))
		return;
	const UnsignedInt bpp = (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8) ? 4 : 2;
	Bool fresh = FALSE;
	if (s_fogMaskTex == NULL || s_fogW != desc.Width || s_fogH != desc.Height || s_fogBpp != bpp)
	{
		REF_PTR_RELEASE(s_fogMaskTex);
		delete [] s_fogPrev;
		delete [] s_fogAge;
		s_fogW = desc.Width; s_fogH = desc.Height; s_fogBpp = bpp;
		s_fogPrev = new unsigned char[s_fogW * s_fogH * bpp];
		s_fogAge  = new unsigned char[s_fogW * s_fogH];
		memset(s_fogAge, 0, s_fogW * s_fogH);
		s_fogMaskTex = NEW_REF(TextureClass, (s_fogW, s_fogH, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_MANAGED));
		s_fogLitLast = TRUE;	// the new mask is written clear once
		fresh = TRUE;			// the first copy marks nothing
	}
	IDirect3DTexture9 *mt = (s_fogMaskTex != NULL) ? s_fogMaskTex->Peek_D3D_Texture() : NULL;
	D3DLOCKED_RECT src;
	if (mt == NULL || FAILED(st->LockRect(0, &src, NULL, D3DLOCK_READONLY)))
		return;
	const UnsignedInt rowBytes = s_fogW * bpp;
	for (UnsignedInt y = 0; y < s_fogH; ++y)
	{
		const unsigned char *row  = (const unsigned char *)src.pBits + y * src.Pitch;
		unsigned char       *prev = s_fogPrev + y * rowBytes;
		if (!fresh && memcmp(row, prev, rowBytes) != 0)
		{
			unsigned char *age = s_fogAge + y * s_fogW;
			for (UnsignedInt x = 0; x < s_fogW; ++x)
				if (memcmp(row + x * bpp, prev + x * bpp, bpp) != 0)
					age[x] = 2;
		}
		memcpy(prev, row, rowBytes);
	}
	st->UnlockRect(0);

	const UnsignedInt count = s_fogW * s_fogH;
	Int marked = 0;
	for (UnsignedInt i = 0; i < count; ++i)
		if (s_fogAge[i] > 0)
			++marked;
	const Bool litNow = (marked > 0) ? TRUE : FALSE;
	D3DLOCKED_RECT dst;
	if ((litNow || s_fogLitLast) && SUCCEEDED(mt->LockRect(0, &dst, NULL, 0)))
	{
		for (UnsignedInt y = 0; y < s_fogH; ++y)
		{
			DWORD *out = (DWORD *)((unsigned char *)dst.pBits + y * dst.Pitch);
			const unsigned char *age = s_fogAge + y * s_fogW;
			for (UnsignedInt x = 0; x < s_fogW; ++x)
				out[x] = (age[x] > 0) ? 0xFF000000 : 0;
		}
		mt->UnlockRect(0);
	}
	s_fogLitLast = litNow;
	for (UnsignedInt i = 0; i < count; ++i)
		if (s_fogAge[i] > 0)
			--s_fogAge[i];

	// screen clip -> world (this frame, jitter removed) -> fog texture uv, as the terrain maps it: (world + offset) * scale
	D3DXMATRIX inv, toWorldUV, toFog, toFogT;
	if (!litNow || D3DXMatrixInverse(&inv, NULL, &s_curViewProjClean) == NULL)
		return;
	D3DXMatrixIdentity(&toWorldUV);
	toWorldUV._11 = sx;			toWorldUV._22 = sy;			toWorldUV._33 = 0.0f;
	toWorldUV._41 = ox * sx;	toWorldUV._42 = oy * sy;
	D3DXMatrixMultiply(&toFog, &inv, &toWorldUV);
	D3DXMatrixTranspose(&toFogT, &toFog);
	memcpy(s_toFogT, &toFogT, sizeof(s_toFogT));
	s_fogReady = TRUE;
}

// Ronin @bugfix 27/09/2026 DX9: POOL_DEFAULT through TextureClass/ZTextureClass, so DX8TextureManagerClass recreates both across
// a device reset, like the shadow map's own pair.
static Bool ensureMoverTargets(void)
{
	if (s_moverTex != NULL && s_moverZ != NULL)
		return TRUE;
	if (s_moverFailed)
		return FALSE;
	s_moverTex = NEW_REF(TextureClass, (MOVER_MAP_RES, MOVER_MAP_RES, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
										TextureClass::POOL_DEFAULT, true));
	s_moverZ   = NEW_REF(ZTextureClass, (MOVER_MAP_RES, MOVER_MAP_RES, WW3D_ZFORMAT_D24S8, MIP_LEVELS_1,
										 TextureClass::POOL_DEFAULT));
	if (s_moverTex->Peek_D3D_Base_Texture() == NULL || s_moverZ->Peek_D3D_Base_Texture() == NULL)
	{
		REF_PTR_RELEASE(s_moverTex);
		REF_PTR_RELEASE(s_moverZ);
		s_moverFailed = TRUE;
		return FALSE;
	}
	return TRUE;
}

// Ronin @bugfix 27/09/2026 DX9: draws the moving-shadow mask (see s_moverOn) and fills the resolve's c12..c16 for it. Only meshes
// that MOVED: velocity, velocity + reactive and a moving ghost (the placement preview). Leaves the resolve's state as it was.
static void renderMoverMask(IDirect3DDevice9 *dev, const D3DVIEWPORT9 &vp, Bool haveDepth)
{
	s_moverOK    = FALSE;
	s_moverDraws = 0;
	if (!s_moverOn || !haveDepth || !TheUseShadowMaps || !R3DShadowMap::isAvailable() || R3DShadowMap::getSplitCount() < 1 ||
		s_meshDrawCount <= 0 || s_velMeshVS == NULL || s_moverPS == NULL || !s_curViewProjValid)
		return;
	Int movers = 0;
	for (Int k = 0; k < s_meshDrawCount; ++k)
	{
		const Int kind = s_meshDraws[k].kind;
		if (kind == TAA_KIND_VEL || kind == TAA_KIND_VEL_REACT || kind == TAA_KIND_GHOST)
			++movers;
	}
	if (movers == 0 || !ensureMoverTargets())
		return;

	// Stored TRANSPOSED and ready to upload (R3DShadowMap.cpp, the capture after each split's render); back to row-vector.
	const Matrix4x4 &sunStored = R3DShadowMap::getLightViewProj(R3DShadowMap::getSplitCount() - 1);
	D3DXMATRIX sunT((const float *)&sunStored), sun;
	D3DXMatrixTranspose(&sun, &sunT);

	IDirect3DSurface9 *cSurf  = s_moverTex->Get_D3D_Surface_Level();
	IDirect3DSurface9 *zSurf  = s_moverZ->Get_D3D_Surface_Level();
	IDirect3DSurface9 *prevRT = NULL;
	IDirect3DSurface9 *prevDS = NULL;
	if (cSurf != NULL && zSurf != NULL && SUCCEEDED(dev->GetRenderTarget(0, &prevRT)) && prevRT != NULL)
	{
		dev->GetDepthStencilSurface(&prevDS);		// may be NULL
		if (SUCCEEDED(dev->SetRenderTarget(0, cSurf)) && SUCCEEDED(dev->SetDepthStencilSurface(zSurf)))
		{
			// White = depth 1 on both pairs = nothing between the sun and anything.
			dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0);
			dev->SetRenderState(D3DRS_ZENABLE,      D3DZB_TRUE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
			dev->SetRenderState(D3DRS_ZFUNC,        D3DCMP_LESSEQUAL);
			DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ, "R3DTaa::moverMask");
			dev->SetVertexShader(s_velMeshVS);		// AFTER the layout bind - it clears the VS
			dev->SetPixelShader(s_moverPS);
			for (Int pass = 0; pass < 2; ++pass)		// 0 = where it is now (RG), 1 = where it was (BA)
			{
				if (pass == 1)
					dev->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
				dev->SetRenderState(D3DRS_COLORWRITEENABLE, (pass == 0)
					? (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN)
					: (D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA));
				for (Int k = 0; k < s_meshDrawCount; ++k)
				{
					TaaMeshDraw &md = s_meshDraws[k];
					if (md.kind != TAA_KIND_VEL && md.kind != TAA_KIND_VEL_REACT && md.kind != TAA_KIND_GHOST)
						continue;
					MeshModelClass *mdl = md.mesh->Peek_Model();
					if (mdl == NULL)
						continue;
					const Int vcount = mdl->Get_Vertex_Count();
					const Int pcount = mdl->Get_Polygon_Count();
					const TriIndex *polys = mdl->Get_Polygon_Array();
					if (vcount <= 0 || pcount <= 0 || vcount > 65535 || polys == NULL)	// 16-bit indices
						continue;
					const Vector3 *verts = NULL;
					D3DXMATRIX m;
					if (md.skinCur != NULL)
					{
						verts = (pass == 0) ? md.skinCur : md.skinPrev;	// skins are already in world space
						m     = sun;
					}
					else
					{
						verts = mdl->Get_Vertex_Array();
						D3DXMATRIX w(To_D3DMATRIX((pass == 0) ? md.cur : md.prev));
						D3DXMatrixMultiply(&m, &w, &sun);
					}
					if (verts == NULL)
						continue;
					D3DXMATRIX mT;
					D3DXMatrixTranspose(&mT, &m);
					dev->SetVertexShaderConstantF(0, (const float *)&mT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&mT, 4);	// TaaVelMesh.vso's second output, unused
					dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)vcount, (UINT)pcount,
												polys, D3DFMT_INDEX16, verts, sizeof(Vector3));
					if (pass == 0)
						++s_moverDraws;
				}
			}
		}
		dev->SetRenderTarget(0, prevRT);
		dev->SetDepthStencilSurface(prevDS);
		dev->SetRenderState(D3DRS_ZENABLE,      FALSE);
		dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
													D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		dev->SetViewport(&vp);		// SetRenderTarget reset it to the whole target
		DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::postRender");
		dev->SetVertexShader(s_quadVS);
		dev->SetPixelShader(s_resolvePS);
	}
	if (prevRT != NULL) prevRT->Release();
	if (prevDS != NULL) prevDS->Release();
	if (cSurf  != NULL) cSurf->Release();
	if (zSurf  != NULL) zSurf->Release();

	// The resolve rebuilds each pixel's position from the scene depth: screen clip -> world -> sun clip, in one matrix.
	D3DXMATRIX inv, toSun, toSunT;
	if (s_moverDraws > 0 && D3DXMatrixInverse(&inv, NULL, &s_curViewProjClean) != NULL)
	{
		D3DXMatrixMultiply(&toSun, &inv, &sun);
		D3DXMatrixTranspose(&toSunT, &toSun);
		memcpy(s_moverToSunT, &toSunT, sizeof(s_moverToSunT));
		// Sun clip units per world unit, across (x) and along the light (z) - ortho, so constant everywhere.
		const float sx = sqrtf(sun._11 * sun._11 + sun._21 * sun._21 + sun._31 * sun._31);
		const float sz = sqrtf(sun._13 * sun._13 + sun._23 * sun._23 + sun._33 * sun._33);
		s_moverParams[0] = s_moverCap ? 2.0f : 1.0f;	// 2 = also cap the weight where the shadow changed
		s_moverParams[1] = 0.5f / (float)MOVER_MAP_RES;
		s_moverParams[2] = MOVER_MARGIN_WORLD * sx * 0.5f;		// clip spans 2 across the map, uv spans 1
		s_moverParams[3] = MOVER_BIAS_WORLD * sz;
		s_moverOK = TRUE;
	}
}

// Ronin @bugfix 07/10/2026 DX9: a device reset fails while any query is alive - `msaa 8` froze the game on it. The
// targets are TextureClass, which the texture manager releases and remakes itself; the queries were the one thing left.
void R3DTaa::releaseResources(void)
{
	s_gpuRemake = (s_gpuRemake || s_gpuOK) ? TRUE : FALSE;
	gpuRelease();
	releaseOpaque();		// Ronin @feature 12/10/2026 DX9: a raw default-pool texture: a reset fails while it lives
	R3DLocalLights::releaseQueries();	// Ronin @diagnostic 20/10/2026 DX9: the lights' GPU timer, likewise
	R3DLocalLights::releaseShaftBuffers();	// Ronin @feature 22/10/2026 DX9: the sun shafts' raw default-pool buffers
}

void R3DTaa::shutdown(void)
{
	s_camera = NULL;
	releaseSurround();		// Ronin @feature 22/10/2026 DX9: local contrast's surroundings
	if (s_resolvePS != NULL) { s_resolvePS->Release(); s_resolvePS = NULL; }
	if (s_quadVS    != NULL) { s_quadVS->Release();    s_quadVS    = NULL; }
	if (s_depthStorePS != NULL) { s_depthStorePS->Release(); s_depthStorePS = NULL; }
	if (s_velMeshVS != NULL) { s_velMeshVS->Release(); s_velMeshVS = NULL; }
	if (s_velSkinVS != NULL) { s_velSkinVS->Release(); s_velSkinVS = NULL; }
	releaseOpaque();
	for (Int i = 0; i < TAA_MESH_SLOTS; ++i)
	{
		delete [] s_meshSlots[i].skin[0];
		delete [] s_meshSlots[i].skin[1];
		s_meshSlots[i].skin[0]   = NULL;
		s_meshSlots[i].skin[1]   = NULL;
		s_meshSlots[i].skinCap   = 0;
		s_meshSlots[i].skinCount = 0;
	}
	delete [] s_skinScratch;
	s_skinScratch    = NULL;
	s_skinScratchCap = 0;
	if (s_velMeshPS != NULL) { s_velMeshPS->Release(); s_velMeshPS = NULL; }
	// Ronin @bugfix 27/10/2026 DX9: the moving decals' marks, and the textures they hold.
	if (s_decalMarkVS != NULL) { s_decalMarkVS->Release(); s_decalMarkVS = NULL; }
	if (s_decalMarkPS != NULL) { s_decalMarkPS->Release(); s_decalMarkPS = NULL; }
	for (Int d = 0; d < s_decalMarkCount; ++d)
		REF_PTR_RELEASE(s_decalMarks[d].texture);
	s_decalMarkCount = 0;
	// Ronin @bugfix 27/09/2026 DX9: the moving-shadow mask and the GPU timer.
	if (s_moverPS != NULL) { s_moverPS->Release(); s_moverPS = NULL; }
	REF_PTR_RELEASE(s_moverTex);
	REF_PTR_RELEASE(s_moverZ);
	s_moverOK     = FALSE;
	s_moverFailed = FALSE;
	gpuRelease();
	MeshClass::Set_Taa_Mesh_Note_Func(NULL);
	s_shadersTried = FALSE;

	REF_PTR_RELEASE(s_history[0]);
	REF_PTR_RELEASE(s_history[1]);
	s_historyW     = 0;
	s_historyH     = 0;
	s_historyValid = FALSE;
	REF_PTR_RELEASE(s_flashTex);		// Ronin @bugfix 09/10/2026 DX9: the selection-flash mask
	s_flashW     = 0;
	s_flashH     = 0;
	s_flashStamp = 0;
	REF_PTR_RELEASE(s_historyLo[0]);	// Ronin @bugfix 09/10/2026 DX9: the history's low bits
	REF_PTR_RELEASE(s_historyLo[1]);
	s_historyLoW = 0;
	s_historyLoH = 0;
	s_lowStamp   = 0;
	REF_PTR_RELEASE(s_depthHist[0]);
	REF_PTR_RELEASE(s_depthHist[1]);
	REF_PTR_RELEASE(s_velTex);
	REF_PTR_RELEASE(s_velPrevTex);
	s_prevVelOK = FALSE;
	s_velW = 0;
	s_velH = 0;
	// Ronin @bugfix 03/10/2026 DX9: the water's own mask.
	if (s_waterMaskPS != NULL) { s_waterMaskPS->Release(); s_waterMaskPS = NULL; }
	REF_PTR_RELEASE(s_waterMaskTex);
	REF_PTR_RELEASE(s_lightMaskTex);	// Ronin @bugfix 09/10/2026 DX9: the local lights' change mask
	REF_PTR_RELEASE(s_fogMaskTex);		// Ronin @bugfix 01/11/2026 DX9: the fog's change mask and its copies
	delete [] s_fogPrev;	s_fogPrev = NULL;
	delete [] s_fogAge;		s_fogAge  = NULL;
	s_fogW = s_fogH = s_fogBpp = 0;
	s_lightMaskW     = 0;
	s_lightMaskH     = 0;
	s_lightMaskStamp = 0;
	REF_PTR_RELEASE(s_finishCopy);		// Ronin @feature 17/10/2026 DX9: the finish pass's copy
	REF_PTR_RELEASE(s_reflColour);		// Ronin @feature 19/10/2026 DX9: last frame, kept without TAA
	REF_PTR_RELEASE(s_reflDepth);
	s_reflW     = 0;
	s_reflH     = 0;
	s_reflFrame = 0;
	s_finishW        = 0;
	s_finishH        = 0;
	s_waterMaskW     = 0;
	s_waterMaskH     = 0;
	s_waterMaskReady = FALSE;
	s_waterMaskDrawn = FALSE;
}

// Ronin @feature 22/10/2026 DX9: LOCAL CONTRAST's surroundings - the source's luminance as shown, at quarter size, blurred across and down
// (sigma 5 texels at 900 rows, scaled with the view) by the resolve shader at c0.x below 0. Default-pool TextureClass: the reset remakes it.
static TextureClass *s_surround[2]   = { NULL, NULL };
static UnsignedInt   s_surroundW     = 0;
static UnsignedInt   s_surroundH     = 0;
static Bool          s_surroundReady = FALSE;		// built for the screen copy about to draw

static void releaseSurround(void)
{
	REF_PTR_RELEASE(s_surround[0]);
	REF_PTR_RELEASE(s_surround[1]);
	s_surroundW = 0;
	s_surroundH = 0;
	s_surroundReady = FALSE;
}

static void surroundQuad(IDirect3DDevice9 *dev, UnsignedInt width, UnsignedInt height)
{
	const float w = (float)width, h = (float)height;
	const float l = (-0.5f / w) * 2.0f - 1.0f, r = ((w - 0.5f) / w) * 2.0f - 1.0f;		// half a pixel: the UVs land on texel centres
	const float t = 1.0f - (-0.5f / h) * 2.0f, b = 1.0f - ((h - 0.5f) / h) * 2.0f;
	struct SurroundVertex { float x, y, z; float u, v; };
	const SurroundVertex q[4] = { { l, t, 0.0f, 0.0f, 0.0f }, { r, t, 0.0f, 1.0f, 0.0f }, { l, b, 0.0f, 0.0f, 1.0f }, { r, b, 0.0f, 1.0f, 1.0f } };
	dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(SurroundVertex));
}

// Ronin @feature 22/10/2026 DX9: `src` (texW x texH) with its 3D view at `rect` (0..1 of it); the frame's targets, depth and states put back.
static Bool buildSurround(IDirect3DDevice9 *dev, IDirect3DBaseTexture9 *src, UnsignedInt texW, UnsignedInt texH, const float rect[4],
						  Bool floatScene)
{
	s_surroundReady = FALSE;
	if (!vividNow() || contrastNow() <= 0.0f || dev == NULL || src == NULL || s_resolvePS == NULL || s_quadVS == NULL || texW == 0 ||
		texH == 0)
		return FALSE;
	const UnsignedInt vpW = (UnsignedInt)(rect[2] * (float)texW + 0.5f);
	const UnsignedInt vpH = (UnsignedInt)(rect[3] * (float)texH + 0.5f);
	const UnsignedInt w = (vpW + 3) / 4, h = (vpH + 3) / 4;
	if (w == 0 || h == 0)
		return FALSE;
	if (s_surround[0] == NULL || s_surround[1] == NULL || s_surroundW != w || s_surroundH != h)
	{
		releaseSurround();
		for (Int i = 0; i < 2; ++i)
		{
			s_surround[i] = NEW_REF(TextureClass, (w, h, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
			if (s_surround[i]->Peek_D3D_Base_Texture() == NULL)
			{
				releaseSurround();
				return FALSE;
			}
		}
		s_surroundW = w;
		s_surroundH = h;
	}
	IDirect3DSurface9 *oldRT = NULL, *oldRT1 = NULL, *oldDS = NULL;
	if (FAILED(dev->GetRenderTarget(0, &oldRT)) || oldRT == NULL)
		return FALSE;
	if (SUCCEEDED(dev->GetRenderTarget(1, &oldRT1)) && oldRT1 != NULL)
		dev->SetRenderTarget(1, NULL);
	dev->GetDepthStencilSurface(&oldDS);
	dev->SetDepthStencilSurface(NULL);		// the frame's may be multisampled, and is larger
	D3DVIEWPORT9 oldVp;
	const Bool haveOldVp = SUCCEEDED(dev->GetViewport(&oldVp)) ? TRUE : FALSE;
	DWORD oldZ = 0, oldZW = 0, oldAB = 0, oldAT = 0, oldCull = 0, oldCW = 0;
	dev->GetRenderState(D3DRS_ZENABLE,          &oldZ);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &oldZW);
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAB);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &oldAT);
	dev->GetRenderState(D3DRS_CULLMODE,         &oldCull);
	dev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldCW);
	dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  FALSE);
	dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE |
												D3DCOLORWRITEENABLE_ALPHA);
	DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::buildSurround");
	dev->SetVertexShader(s_quadVS);		// after the layout bind - BindLayoutFVF clears the VS
	dev->SetPixelShader(s_resolvePS);
	const float c9[4] = { s_white, floatScene ? 1.0f : 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(9, c9, 1);
	dev->SetPixelShaderConstantF(24, rect, 1);
	// Ronin @feature 23/10/2026 DX9: the dark lift, so the night's surroundings are as shown (c19 is uploaded after this).
	const float nightLiftC[4] = { s_nightLift, (s_nightOn && R3DLocalLights::isEnabled() && vividNight()) ? 1.0f : 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(19, nightLiftC, 1);
	const float step = (float)vpH / 900.0f;
	Bool ok = TRUE;
	for (Int pass = 0; pass < 3 && ok; ++pass)
	{
		IDirect3DSurface9 *surf = s_surround[(pass == 1) ? 1 : 0]->Get_D3D_Surface_Level();
		ok = (surf != NULL && SUCCEEDED(dev->SetRenderTarget(0, surf))) ? TRUE : FALSE;
		if (surf != NULL)
			surf->Release();
		if (!ok)
			break;
		D3DVIEWPORT9 svp;
		svp.X = 0; svp.Y = 0; svp.Width = w; svp.Height = h; svp.MinZ = 0.0f; svp.MaxZ = 1.0f;
		dev->SetViewport(&svp);
		const float c0[4] = { -1.0f - (float)pass, (pass == 0) ? 1.0f / (float)texW : 1.0f / (float)w,
							  (pass == 0) ? 1.0f / (float)texH : 1.0f / (float)h, step };
		dev->SetPixelShaderConstantF(0, c0, 1);
		dev->SetTexture(0, (pass == 0) ? src : s_surround[(pass == 1) ? 0 : 1]->Peek_D3D_Base_Texture());
		setResolveSampler(dev, 0, TRUE);
		surroundQuad(dev, w, h);
	}
	dev->SetTexture(0, NULL);
	dev->SetRenderTarget(0, oldRT);
	if (oldRT1 != NULL)
	{
		dev->SetRenderTarget(1, oldRT1);
		oldRT1->Release();
	}
	dev->SetDepthStencilSurface(oldDS);
	if (oldDS != NULL)
		oldDS->Release();
	oldRT->Release();
	if (haveOldVp)
		dev->SetViewport(&oldVp);
	dev->SetRenderState(D3DRS_ZENABLE,          oldZ);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     oldZW);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAB);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  oldAT);
	dev->SetRenderState(D3DRS_CULLMODE,         oldCull);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, oldCW);
	s_surroundReady = ok;
	return ok;
}

// Ronin @feature 22/10/2026 DX9: vibrance and local contrast into a screen copy: c22 and s11. Every screen copy sets it.
static void bindVivid(IDirect3DDevice9 *dev)
{
	const float c22[4] = { vividNow() ? 1.0f : 0.0f, vibranceNow(), s_surroundReady ? contrastNow() : 0.0f, vividNight() ? 1.0f : 0.0f };
	dev->SetPixelShaderConstantF(22, c22, 1);
	if (s_surroundReady)
	{
		dev->SetTexture(11, s_surround[0]->Peek_D3D_Base_Texture());
		setResolveSampler(dev, 11, TRUE);
	}
}

// Ronin @feature 17/10/2026 DX9: the screen copy without TAA - `src` over `vp` through the resolve shader at weight 0: the tone curve
// for the float scene, then at night the dark lift and the night look. No history, CAS or flash (the scene drew its own).
static void drawFinish(IDirect3DDevice9 *dev, IDirect3DTexture9 *src, const D3DVIEWPORT9 &vp, UnsignedInt texW, UnsignedInt texH,
					   Bool floatScene)
{
	// Ronin @feature 22/10/2026 DX9: local contrast's surroundings, from this source, before anything below is set.
	const float surroundRect[4] = { (float)vp.X / (float)texW, (float)vp.Y / (float)texH, (float)vp.Width / (float)texW,
								   (float)vp.Height / (float)texH };
	buildSurround(dev, src, texW, texH, surroundRect, floatScene);
	D3DVIEWPORT9 oldVp;
	const Bool haveOldVp = SUCCEEDED(dev->GetViewport(&oldVp)) ? TRUE : FALSE;
	dev->SetViewport(&vp);
	DWORD oldZ = 0, oldZW = 0, oldAB = 0, oldAT = 0, oldCull = 0, oldCW = 0;
	dev->GetRenderState(D3DRS_ZENABLE,          &oldZ);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &oldZW);
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAB);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &oldAT);
	dev->GetRenderState(D3DRS_CULLMODE,         &oldCull);
	dev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldCW);
	dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  FALSE);
	dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

	// Ronin @feature 17/10/2026 DX9: as postRender's screen copy - the quad spans the viewport, its UVs that sub-rect of `src`.
	const float u0 = (float)vp.X / (float)texW;
	const float v0 = (float)vp.Y / (float)texH;
	const float u1 = (float)(vp.X + vp.Width)  / (float)texW;
	const float v1 = (float)(vp.Y + vp.Height) / (float)texH;
	const float hx = 1.0f / (float)vp.Width;
	const float hy = 1.0f / (float)vp.Height;
	struct QuadVertex { float x, y, z; float u, v; } q[4];
	q[0].x = -1.0f - hx; q[0].y =  1.0f + hy; q[0].u = u0; q[0].v = v0;
	q[1].x =  1.0f - hx; q[1].y =  1.0f + hy; q[1].u = u1; q[1].v = v0;
	q[2].x = -1.0f - hx; q[2].y = -1.0f + hy; q[2].u = u0; q[2].v = v1;
	q[3].x =  1.0f - hx; q[3].y = -1.0f + hy; q[3].u = u1; q[3].v = v1;
	for (int i = 0; i < 4; ++i)
		q[i].z = 0.0f;

	DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::drawFinish");
	dev->SetVertexShader(s_quadVS);		// after the layout bind - BindLayoutFVF clears the VS
	dev->SetPixelShader(s_resolvePS);
	const float c0[4] = { 0.0f, 1.0f / (float)texW, 1.0f / (float)texH, 0.0f };
	dev->SetPixelShaderConstantF(0, c0, 1);
	const float c5[4] = { u0, v0, u1 - u0, v1 - v0 };
	dev->SetPixelShaderConstantF(5, c5, 1);
	const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(6, zero4, 1);		// no debug view
	dev->SetPixelShaderConstantF(7, zero4, 1);		// no CAS: it is TAA's
	const float c8[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
	dev->SetPixelShaderConstantF(8, c8, 1);			// no flash: without TAA the scene draws its own
	const float c9[4] = { s_white, floatScene ? 1.0f : 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(9, c9, 1);			// the tone curve bends a float scene only
	const Bool nightMask = (s_lightMaskTex != NULL && s_lightMaskFrame == s_frameNo && s_lightMaskW == texW &&
							s_lightMaskH == texH) ? TRUE : FALSE;
	if (nightMask)
	{
		dev->SetTexture(8, s_lightMaskTex->Peek_D3D_Texture());
		setResolveSampler(dev, 8);
	}
	float nightC[8], nightLevel[3];
	nightConstants(nightC, nightLevel, nightMask);
	dev->SetPixelShaderConstantF(17, nightC, 2);
	const Bool night = (s_nightOn && R3DLocalLights::isEnabled() && TheGlobalData != NULL && TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) ? TRUE : FALSE;
	const float nightLiftC[4] = { s_nightLift, night ? 1.0f : 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(19, nightLiftC, 1);
	bindGrade(dev);		// Ronin @feature 21/10/2026 DX9: the daytime grade
	bindVivid(dev);
	dev->SetTexture(0, src);
	setResolveSampler(dev, 0);
	dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadVertex));

	dev->SetTexture(11, NULL);
	dev->SetTexture(8, NULL);
	dev->SetTexture(0, NULL);
	dev->SetVertexShader(NULL);
	dev->SetPixelShader(NULL);
	dev->SetRenderState(D3DRS_ZENABLE,          oldZ);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     oldZW);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAB);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  oldAT);
	dev->SetRenderState(D3DRS_CULLMODE,         oldCull);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, oldCW);
	if (haveOldVp)
		dev->SetViewport(&oldVp);
	ShaderClass::Invalidate();
	DX8Wrapper::Invalidate_Cached_Render_States();
	DX8Wrapper::Invalidate_Texture_State(0, 1);
}

// Ronin @feature 19/10/2026 DX9: screen-space reflections without TAA - `src` (the frame, or HDR's float scene) copied, the scene
// depth stored beside it, linear, and their camera, for next frame's getLastFrame. vp = the 3D view in `src`.
static void storeLastFrame(IDirect3DDevice9 *dev, IDirect3DSurface9 *src, IDirect3DTexture9 *depthTex, const CameraClass *camera,
						   const D3DVIEWPORT9 &vp)
{
	s_reflFrame = 0;
	D3DSURFACE_DESC sd, dd;
	if (src == NULL || depthTex == NULL || camera == NULL || FAILED(src->GetDesc(&sd)) || FAILED(depthTex->GetLevelDesc(0, &dd)) ||
		dd.Width != sd.Width || dd.Height != sd.Height || sd.Width == 0 || sd.Height == 0)
		return;
	ensureShaders(dev);
	if (s_quadVS == NULL || s_depthStorePS == NULL)
		return;
	if (s_reflColour == NULL || s_reflDepth == NULL || s_reflW != sd.Width || s_reflH != sd.Height || s_reflFormat != sd.Format)
	{
		REF_PTR_RELEASE(s_reflColour);
		REF_PTR_RELEASE(s_reflDepth);
		s_reflW = 0;
		s_reflH = 0;
		WW3DFormat wwFormat = D3DFormat_To_WW3DFormat(sd.Format);
		if (wwFormat == WW3D_FORMAT_UNKNOWN)
			wwFormat = WW3D_FORMAT_A8R8G8B8;
		s_reflColour = NEW_REF(TextureClass, (sd.Width, sd.Height, wwFormat, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
		s_reflDepth  = NEW_REF(TextureClass, (sd.Width, sd.Height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
		if (s_reflColour->Peek_D3D_Base_Texture() == NULL || s_reflDepth->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_reflColour);	// macros with their own if - keep the braces
			REF_PTR_RELEASE(s_reflDepth);
			return;
		}
		s_reflW      = sd.Width;
		s_reflH      = sd.Height;
		s_reflFormat = sd.Format;
	}
	IDirect3DSurface9 *colourSurf = s_reflColour->Get_D3D_Surface_Level();
	IDirect3DSurface9 *depthSurf  = s_reflDepth->Get_D3D_Surface_Level();
	IDirect3DSurface9 *oldRT = NULL, *oldDS = NULL;
	// Ronin @feature 19/10/2026 DX9: StretchRect resolves a multisampled frame.
	if (colourSurf != NULL && depthSurf != NULL && SUCCEEDED(dev->StretchRect(src, NULL, colourSurf, NULL, D3DTEXF_NONE)) &&
		SUCCEEDED(dev->GetRenderTarget(0, &oldRT)) && oldRT != NULL)
	{
		D3DVIEWPORT9 oldVp;
		const Bool haveOldVp = SUCCEEDED(dev->GetViewport(&oldVp)) ? TRUE : FALSE;
		dev->GetDepthStencilSurface(&oldDS);		// none while the store draws: the frame's may be multisampled
		dev->SetDepthStencilSurface(NULL);
		if (SUCCEEDED(dev->SetRenderTarget(0, depthSurf)))
		{
			DWORD oldZ = 0, oldZW = 0, oldAB = 0, oldAT = 0, oldCull = 0, oldCW = 0;
			dev->GetRenderState(D3DRS_ZENABLE,          &oldZ);
			dev->GetRenderState(D3DRS_ZWRITEENABLE,     &oldZW);
			dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAB);
			dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &oldAT);
			dev->GetRenderState(D3DRS_CULLMODE,         &oldCull);
			dev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldCW);
			dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			dev->SetRenderState(D3DRS_ALPHATESTENABLE,  FALSE);
			dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
			dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE |
														D3DCOLORWRITEENABLE_ALPHA);
			CameraClass &cam = const_cast<CameraClass &>(*camera);
			float zn = 1.0f, zf = 1000.0f;
			cam.Get_Clip_Planes(zn, zf);
			// Ronin @feature 19/10/2026 DX9: the whole target, texel for texel with the depth
			const float hx = 1.0f / (float)sd.Width;
			const float hy = 1.0f / (float)sd.Height;
			struct QuadVertex { float x, y, z; float u, v; } q[4];
			q[0].x = -1.0f - hx; q[0].y =  1.0f + hy; q[0].u = 0.0f; q[0].v = 0.0f;
			q[1].x =  1.0f - hx; q[1].y =  1.0f + hy; q[1].u = 1.0f; q[1].v = 0.0f;
			q[2].x = -1.0f - hx; q[2].y = -1.0f + hy; q[2].u = 0.0f; q[2].v = 1.0f;
			q[3].x =  1.0f - hx; q[3].y = -1.0f + hy; q[3].u = 1.0f; q[3].v = 1.0f;
			for (int i = 0; i < 4; ++i)
				q[i].z = 0.0f;
			DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::storeLastFrame");
			dev->SetVertexShader(s_quadVS);		// after the layout bind - BindLayoutFVF clears the VS
			dev->SetPixelShader(s_depthStorePS);
			const float dcfg[4] = { 0.0f, zn, zf, 0.0f };		// w = 0: no auto-reactive flag
			dev->SetPixelShaderConstantF(0, dcfg, 1);
			dev->SetTexture(2, depthTex);
			setResolveSampler(dev, 2);
			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadVertex));
			dev->SetTexture(2, NULL);
			dev->SetVertexShader(NULL);
			dev->SetPixelShader(NULL);
			dev->SetRenderState(D3DRS_ZENABLE,          oldZ);
			dev->SetRenderState(D3DRS_ZWRITEENABLE,     oldZW);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAB);
			dev->SetRenderState(D3DRS_ALPHATESTENABLE,  oldAT);
			dev->SetRenderState(D3DRS_CULLMODE,         oldCull);
			dev->SetRenderState(D3DRS_COLORWRITEENABLE, oldCW);
			// Ronin @feature 19/10/2026 DX9: the camera, D3D convention as postRender builds it; no jitter without TAA
			Matrix4x4 projMtx;
			cam.Get_D3D_Projection_Matrix(&projMtx);
			const Matrix4x4 viewMtx(cam.Get_View_Matrix());
			const D3DMATRIX dv = To_D3DMATRIX(viewMtx);
			const D3DMATRIX dp = To_D3DMATRIX(projMtx);
			D3DXMATRIX dxView(dv), dxProj(dp);
			D3DXMatrixMultiply(&s_reflViewProj, &dxView, &dxProj);
			s_reflView[0] = (float)vp.X / (float)sd.Width;
			s_reflView[1] = (float)vp.Y / (float)sd.Height;
			s_reflView[2] = (float)vp.Width / (float)sd.Width;
			s_reflView[3] = (float)vp.Height / (float)sd.Height;
			s_reflFar   = zf;
			s_reflFrame = s_frameNo;
		}
		dev->SetRenderTarget(0, oldRT);
		dev->SetDepthStencilSurface(oldDS);
		if (haveOldVp)
			dev->SetViewport(&oldVp);
		ShaderClass::Invalidate();
		DX8Wrapper::Invalidate_Cached_Render_States();
		DX8Wrapper::Invalidate_Texture_State(2, 1);
	}
	if (oldDS != NULL)
		oldDS->Release();
	if (oldRT != NULL)
		oldRT->Release();
	if (colourSurf != NULL)
		colourSurf->Release();
	if (depthSurf != NULL)
		depthSurf->Release();
}

// Ronin @feature 17/10/2026 DX9: HDR WITHOUT TAA (MSAA off) - the scene still goes to the float target; postRender brings it back
// through drawFinish. TAA's own passes key off s_redirected, which stays FALSE: none of them runs.
static Bool s_hdrAlone = FALSE;
static Bool beginHdrAlone(void)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (!s_floatReady || dev == NULL)
		return FALSE;
	ensureShaders(dev);
	s_viewportOK = SUCCEEDED(dev->GetViewport(&s_viewport)) ? TRUE : FALSE;
	if (s_quadVS == NULL || s_resolvePS == NULL || !s_viewportOK || !W3DShaderManager::canRenderToTexture() ||
		W3DShaderManager::isRenderingToTexture())
		return FALSE;
	W3DShaderManager::startRenderToTexture();
	s_hdrAlone = W3DShaderManager::isRenderingToTexture() ? TRUE : FALSE;
	s_floatNow = s_hdrAlone;
	return s_hdrAlone;
}

static void endHdrAlone(void)
{
	s_hdrAlone = FALSE;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DTexture9 *sceneTex = W3DShaderManager::endRenderToTexture();
	if (dev == NULL || sceneTex == NULL)
		return;
	// Ronin @feature 17/10/2026 DX9: as postRender - endRenderToTexture rebound the frame buffer's own depth; suspend and resume
	// bind the scene depth again for the rest of the frame.
	const Bool depthSuspended = DX8Wrapper::Suspend_Scene_Depth() ? TRUE : FALSE;
	Int fbW = 0, fbH = 0, bits = 0;
	bool windowed = false;
	WW3D::Get_Render_Target_Resolution(fbW, fbH, bits, windowed);
	// Ronin @feature 19/10/2026 DX9: screen-space reflections without TAA read this float scene next frame, before its tone curve.
	if (s_lastFrameWanted && depthSuspended && s_viewportOK)
	{
		IDirect3DSurface9 *sceneSurf = NULL;
		if (SUCCEEDED(sceneTex->GetSurfaceLevel(0, &sceneSurf)) && sceneSurf != NULL)
		{
			storeLastFrame(dev, sceneSurf, DX8Wrapper::Peek_Scene_Depth_Texture(), s_camera, s_viewport);
			sceneSurf->Release();
		}
	}
	if (fbW > 0 && fbH > 0 && s_viewportOK)
		drawFinish(dev, sceneTex, s_viewport, (UnsignedInt)fbW, (UnsignedInt)fbH, TRUE);
	if (depthSuspended)
		DX8Wrapper::Resume_Scene_Depth();
}

Bool R3DTaa::preRender(const CameraClass &camera)
{
	s_redirected = FALSE;
	s_floatNow   = FALSE;		// Ronin @feature 17/10/2026 DX9: float only once a redirect is taken
	s_waterMaskReady = FALSE;	// Ronin @bugfix 03/10/2026 DX9: set below, once this frame's target is cleared
	s_waterMaskDrawn = FALSE;
	if (!s_active)
	{
		s_camera = &camera;		// Ronin @feature 19/10/2026 DX9: for storeLastFrame (screen-space reflections without TAA)
		return beginHdrAlone();		// Ronin @feature 17/10/2026 DX9: HDR without TAA
	}
	s_camera = &camera;
	s_meshDrawCount = 0;
	s_meshSeen      = 0;
	s_reactCount    = 0;
	s_trkFull       = 0;
	s_skinDrawCount = 0;
	s_appliedValid  = FALSE;	// Apply runs during the scene render and sets it again
	s_meshVpValid   = FALSE;
	s_opaqueOK      = FALSE;
	MeshClass::Set_Taa_Mesh_Note_Func(&R3DTaa::noteMesh);	// §14; noteMesh itself checks the redirect window
	++s_moverFrame;
	if (s_moverFrame == 0) ++s_moverFrame;	// 0 is the "no previous position" marker

	{
		CameraClass &cam = const_cast<CameraClass &>(camera);
		cam.Get_Clip_Planes(s_zNear, s_zFar);
	}

	// The 3D viewport, while it is still the 3D viewport — see the note on s_viewport.
	{
		IDirect3DDevice9 *vpDev = DX8Wrapper::_Get_D3D_Device8();
		s_viewportOK = (vpDev != NULL && SUCCEEDED(vpDev->GetViewport(&s_viewport))) ? TRUE : FALSE;
	}

	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL)
		return FALSE;
	ensureShaders(dev);
	if (s_quadVS == NULL || s_resolvePS == NULL)
		return FALSE;

	// A script's view filter (black & white, motion blur, crossfade) already redirects the scene and owns the target.
	// Those are cinematic and rare: stand down for the frame rather than fight over it.
	if (!W3DShaderManager::canRenderToTexture() || W3DShaderManager::isRenderingToTexture())
		return FALSE;

	prepareWaterMask(dev);	// Ronin @bugfix 03/10/2026 DX9: before the redirect - the target under it is still the frame buffer

	W3DShaderManager::startRenderToTexture();
	if (!W3DShaderManager::isRenderingToTexture())
		return FALSE;

	s_redirected = TRUE;
	s_floatNow   = s_floatReady;
	return TRUE;
}

// Ronin @feature 17/10/2026 DX9: THE FINISH PASS (finishWanted): the frame copied, then drawFinish over the camera's viewport.
void R3DTaa::finishScene(const CameraClass &camera)
{
	// Ronin @feature 19/10/2026 DX9: or for screen-space reflections without TAA - they read last frame from storeLastFrame.
	const Bool finish = finishWanted();
	const Bool store  = (s_lastFrameWanted && !s_active) ? TRUE : FALSE;
	if (!finish && !store)
		return;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL || W3DShaderManager::isRenderingToTexture())
		return;		// a script's view filter holds the frame
	ensureShaders(dev);
	if (s_quadVS == NULL || s_resolvePS == NULL)
		return;
	IDirect3DSurface9 *rt = NULL;
	if (FAILED(dev->GetRenderTarget(0, &rt)) || rt == NULL)
		return;
	D3DSURFACE_DESC rd;
	const Bool descOK = (SUCCEEDED(rt->GetDesc(&rd)) && rd.Width > 0 && rd.Height > 0) ? TRUE : FALSE;
	Bool ready = (descOK && finish) ? TRUE : FALSE;		// Ronin @feature 19/10/2026 DX9: the finish pass's own copy
	if (ready && (s_finishCopy == NULL || s_finishW != rd.Width || s_finishH != rd.Height || s_finishFormat != rd.Format))
	{
		REF_PTR_RELEASE(s_finishCopy);
		s_finishW = 0;
		s_finishH = 0;
		WW3DFormat wwFormat = D3DFormat_To_WW3DFormat(rd.Format);
		if (wwFormat == WW3D_FORMAT_UNKNOWN)
			wwFormat = WW3D_FORMAT_A8R8G8B8;
		s_finishCopy = NEW_REF(TextureClass, (rd.Width, rd.Height, wwFormat, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
		if (s_finishCopy->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_finishCopy);	// a macro with its own if - keep the braces
			ready = FALSE;
		}
		else
		{
			s_finishW = rd.Width;
			s_finishH = rd.Height;
			s_finishFormat = rd.Format;
		}
	}
	if (ready)
	{
		IDirect3DSurface9 *copySurf = s_finishCopy->Get_D3D_Surface_Level();
		ready = (copySurf != NULL && SUCCEEDED(dev->StretchRect(rt, NULL, copySurf, NULL, D3DTEXF_NONE))) ? TRUE : FALSE;
		if (copySurf != NULL)
			copySurf->Release();
	}
	if (!descOK)
	{
		rt->Release();
		return;
	}

	Vector2 vpMin, vpMax;
	camera.Get_Viewport(vpMin, vpMax);
	D3DVIEWPORT9 vp;
	vp.X      = (DWORD)(vpMin.X * (float)rd.Width + 0.5f);
	vp.Y      = (DWORD)(vpMin.Y * (float)rd.Height + 0.5f);
	vp.Width  = (DWORD)((vpMax.X - vpMin.X) * (float)rd.Width + 0.5f);
	vp.Height = (DWORD)((vpMax.Y - vpMin.Y) * (float)rd.Height + 0.5f);
	vp.MinZ   = 0.0f;
	vp.MaxZ   = 1.0f;
	const Bool vpOK = (vp.Width > 0 && vp.Height > 0 && vp.X + vp.Width <= rd.Width && vp.Y + vp.Height <= rd.Height) ? TRUE : FALSE;
	// Ronin @feature 19/10/2026 DX9: the frame as the scene left it, before the night look draws over it - as TAA's history.
	if (store && vpOK && DX8Wrapper::Suspend_Scene_Depth())
	{
		storeLastFrame(dev, rt, DX8Wrapper::Peek_Scene_Depth_Texture(), &camera, vp);
		DX8Wrapper::Resume_Scene_Depth();
	}
	rt->Release();
	if (ready && vpOK)
		drawFinish(dev, s_finishCopy->Peek_D3D_Texture(), vp, rd.Width, rd.Height, FALSE);
}

void R3DTaa::postRender(void)
{
	if (s_hdrAlone)
	{
		endHdrAlone();		// Ronin @feature 17/10/2026 DX9: HDR without TAA
		return;
	}
	if (!s_redirected)
		return;
	s_redirected = FALSE;
	s_depthStoredNow = FALSE;		// Ronin @feature 18/10/2026 DX9: screen-space reflections: set where the depth store draws

	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DTexture9 *sceneTex = W3DShaderManager::endRenderToTexture();
	if (dev == NULL || sceneTex == NULL || s_quadVS == NULL || s_resolvePS == NULL)
		return;

	// Ronin @feature 20/09/2026 DX9: take the INTZ buffer off the device so it can be SAMPLED. This must come AFTER
	// endRenderToTexture: that call rebinds the frame buffer's own depth, and Peek_Scene_Depth_Texture only hands the
	// texture over while ours is suspended. Resumed at the bottom — the rest of the frame keeps testing against it.
	const Bool depthSuspended = DX8Wrapper::Suspend_Scene_Depth() ? TRUE : FALSE;
	IDirect3DTexture9 *depthTex = depthSuspended ? DX8Wrapper::Peek_Scene_Depth_Texture() : NULL;
	// Ronin @bugfix 23/09/2026 DX9: build the view-projection from the CAMERA, not off the device. R3DShadowMap
	// records the hazard at its own capture site: "reading the device cold returned whatever drew last (particles set
	// VIEW to identity)". TAA had exactly that exposure - this runs after the scene, and Apply_Render_State_Changes
	// pushes whatever the wrapper last cached, which need not be the 3D camera. It looked fine only because the check
	// was `debug 1` flat grey AT REST, and a consistently wrong matrix also gives a zero delta at rest.
	// Get_D3D_Projection_Matrix returns ProjectionTransform, which is CLEAN - CameraClass::Apply jitters a local copy
	// only (camera.cpp:752-754) - so no jitter strip is needed here either.
	s_prevViewProj      = s_curViewProjClean;
	s_prevViewProjValid = s_curViewProjValid;
	if (s_camera != NULL)
	{
		CameraClass &cam = const_cast<CameraClass &>(*s_camera);
		Matrix4x4 projMtx;
		cam.Get_D3D_Projection_Matrix(&projMtx);
		const Matrix4x4 viewMtx(cam.Get_View_Matrix());

		const D3DMATRIX dv = To_D3DMATRIX(viewMtx);
		const D3DMATRIX dp = To_D3DMATRIX(projMtx);
		D3DXMATRIX dxView(dv), dxProj(dp);
		D3DXMatrixMultiply(&s_curViewProjClean, &dxView, &dxProj);	// row-vector: clip = pos * (view*proj)
		s_curViewProjValid = TRUE;

		// Ronin @feature 24/09/2026 DX9: prefer what Apply actually sent during the scene.
		if (s_appliedValid)
			D3DXMatrixMultiply(&s_curViewProjClean, &s_appliedView, &s_appliedProj);
	}
	else
	{
		s_curViewProjValid = FALSE;
	}

	const Bool canReproject = (depthTex != NULL) && s_prevViewProjValid && s_curViewProjValid;

	// The 3D viewport as it was BEFORE the render target changed. Asking the device now would give the whole frame
	// buffer, because endRenderToTexture's SetRenderTarget reset it — see the note on s_viewport.
	if (!s_viewportOK)
		return;
	const D3DVIEWPORT9 vp = s_viewport;
	dev->SetViewport(&vp);

	Int fbW = 0, fbH = 0, bits = 0;
	bool windowed = false;
	WW3D::Get_Render_Target_Resolution(fbW, fbH, bits, windowed);
	if (fbW <= 0 || fbH <= 0 || vp.Width == 0 || vp.Height == 0)
		return;
	if (s_gpuRemake)
	{
		s_gpuRemake = FALSE;
		gpuCreate(dev);
	}
	gpuBegin();		// Ronin @diagnostic 27/09/2026 DX9: every return below this point is past gpuEnd

	DWORD oldZ = 0, oldZW = 0, oldAB = 0, oldAT = 0, oldCull = 0, oldCW = 0;
	dev->GetRenderState(D3DRS_ZENABLE,          &oldZ);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &oldZW);
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAB);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &oldAT);
	dev->GetRenderState(D3DRS_CULLMODE,         &oldCull);
	dev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldCW);

	dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  FALSE);
	dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
	// Ronin @bugfix 21/09/2026 DX9: ALPHA MUST BE WRITABLE. This engine's standing convention is COLORWRITEENABLE =
	// RGB only — the shoreline pass and W3DScene both restore it that way — so without this the resolve's alpha is
	// masked off. The motion memory lives in that alpha, which means it was never stored: histAlpha read back as zero
	// every frame and the "remember motion for a few frames" mechanism, the one taken from the shadow system, has
	// been dead since it was written. The `moving` debug view being pure black is what exposed it.
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
												D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

	// The scene texture is the size of the WHOLE frame buffer and the viewport is a sub-rect of it (the command bar
	// takes the rest), so every quad below covers the viewport while its UVs address that same sub-rect — not 0..1.
	// The history targets are frame-buffer sized for exactly this reason: one UV convention for both passes.
	const float u0 = (float)vp.X / (float)fbW;
	const float v0 = (float)vp.Y / (float)fbH;
	const float u1 = (float)(vp.X + vp.Width)  / (float)fbW;
	const float v1 = (float)(vp.Y + vp.Height) / (float)fbH;

	// Clip space spans the VIEWPORT, so the quad is the full -1..1 rect, and the UVs address the viewport's sub-rect
	// of a frame-buffer-sized texture.
	//
	// Ronin @diagnostic 22/09/2026 DX9: this shift is CORRECT and it is now MEASURED, not argued. `taa debug 7` paints
	// the fractional sample offset: with the shift in, every component reads 0 or 1 - the same point modulo the texel
	// grid - and nothing reads 0.5, so uv lands exactly on texel centres and Catmull-Rom is an identity (f=0 gives
	// w1=1, f=1 gives w2=1 on the next texel over: the same tap). Removing it was tried on a derivation twice, 21/09
	// and 22/09, and blurred the image both times. The derivation is wrong; do not run it a third time.
	const float hx = 1.0f / (float)vp.Width;
	const float hy = 1.0f / (float)vp.Height;
	struct QuadVertex { float x, y, z; float u, v; } q[4];
	q[0].x = -1.0f - hx; q[0].y =  1.0f + hy; q[0].u = u0; q[0].v = v0;
	q[1].x =  1.0f - hx; q[1].y =  1.0f + hy; q[1].u = u1; q[1].v = v0;
	q[2].x = -1.0f - hx; q[2].y = -1.0f + hy; q[2].u = u0; q[2].v = v1;
	q[3].x =  1.0f - hx; q[3].y = -1.0f + hy; q[3].u = u1; q[3].v = v1;
	for (int i = 0; i < 4; ++i)
		q[i].z = 0.0f;

	DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::postRender");
	dev->SetVertexShader(s_quadVS);		// AFTER the layout bind — BindLayoutFVF clears the VS
	dev->SetPixelShader(s_resolvePS);

	const Bool haveHistory = ensureHistory((UnsignedInt)fbW, (UnsignedInt)fbH);

	// Ronin @feature 20/09/2026 DX9: current clip -> previous clip, in one matrix. A pixel's clip position goes back
	// through this frame's inverse view-projection to the world, then forward through last frame's, which is where it
	// sat on screen a frame ago. Transposed on upload, the convention every matrix in this project's shaders uses.
	// ALWAYS upload the viewport rect. The shader tests the history UV against it to reject pixels that were off
	// screen last frame, and that test runs whether or not reprojection is on — leaving c5 stale with reprojection
	// disabled rejected EVERY pixel, so no history was ever used and the jitter came back raw.
	// Ronin @bugfix 24/09/2026 DX9: the resolve rebuilds each pixel's NDC through c5, so it must be the viewport the SCENE
	// drew with. Measured: vp 1600x900 from preRender, the scene 1600x720 - the command bar is excluded.
	// Mapped over 900 rows, every pixel's NDC was wrong: harmless at rest (the same wrong mapping on both frames
	// cancels, which is why debug 1 read flat grey) but wrong while panning, and it was the offset under every mask,
	// rect and mesh silhouette since 22/09.
	const D3DVIEWPORT9 svp = s_meshVpValid ? s_meshVp : vp;
	const float c5[4] = { (float)svp.X / (float)fbW, (float)svp.Y / (float)fbH,
						  (float)svp.Width / (float)fbW, (float)svp.Height / (float)fbH };
	dev->SetPixelShaderConstantF(5, c5, 1);

	if (canReproject)
	{
		// Row-vector convention throughout: clip = pos * VP, so pos = clip * VP^-1 and prevClip = clip * VP^-1 * prevVP.
		// Then transpose and consume with mul(rowVec, M), the same pairing RigidInstance_ps uses for g_LightViewProj.
		// BOTH ends use the CLEAN matrix, and that is not a simplification — it is the point. With a still camera the
		// two matrices are then identical, the delta is exactly zero, and the history is sampled at this very pixel.
		// Using the jittered matrix for the inverse instead leaves a residual delta of -jitter that changes every
		// frame: the image wobbles while still, and keeps wobbling for about a second after the camera stops while the
		// EMA bleeds the error out. You must NOT cancel the jitter when sampling history — the history holds the
		// resolved colour at each pixel CENTRE, and the current frame's offset sample is the only new information the
		// frame carries. Averaging those offset samples IS the supersampling.
		D3DXMATRIX curInv, reproj, reprojT;
		if (D3DXMatrixInverse(&curInv, NULL, &s_curViewProjClean) != NULL)
		{
			D3DXMatrixMultiply(&reproj, &curInv, &s_prevViewProj);
			D3DXMatrixTranspose(&reprojT, &reproj);
			dev->SetPixelShaderConstantF(1, (const float *)&reprojT, 4);

			dev->SetTexture(2, depthTex);
			setResolveSampler(dev, 2);	// POINT: a depth texel must never be interpolated across an edge
		}
	}

	s_lastVelOK = FALSE;

	// ---- velocity drawn FROM THE MESHES, §14 ---------------------------------------------------------------------
	// Each mesh that moved this frame is re-drawn from its own vertex and triangle arrays, through its current and
	// previous world-view-projection. The pixel shader discards anything behind the scene depth, so only the visible
	// surface writes - the ownership the rectangles never had. Depth test off, blend off, cull none: all already set
	// for the resolve above.
	// Ronin @bugfix 03/10/2026 DX9: the water marked its own pixels this frame (beginWaterMask): no flat polygons then.
	s_waterMaskLast = s_waterMaskDrawn;
	if (s_waterMaskDrawn)
		s_waterVertCount = 0;
	else
		buildWaterMask();	// Ronin @bugfix 29/09/2026 DX9: the water, flagged reactive below
	// Ronin @bugfix 09/10/2026 DX9: VANISHED. A mesh that reported last frame and not this one: a deselected rally flag, a unit that
	// died. It was in no velocity target, so nothing reset where it stood and it faded for a second. Its box, below.
	s_vanishCount = 0;
	if (s_velocityOn)
	{
		Int found = 0;
		for (Int vi = 0; vi < TAA_MESH_SLOTS; ++vi)
		{
			const TaaMeshSlot &vs = s_meshSlots[vi];
			if (vs.key == NULL || !vs.boxOK || vs.stamp == 0 || s_moverFrame - vs.stamp != 1)
				continue;
			if (found < TAA_MAX_VANISH)
			{
				for (Int c = 0; c < 3; ++c)
				{
					s_vanishBox[found][c]     = vs.boxC[c];
					s_vanishBox[found][c + 3] = vs.boxE[c];
				}
			}
			++found;
		}
		s_vanishCount = (found <= TAA_MAX_VANISH) ? found : 0;
	}
	// Ronin @bugfix 27/10/2026 DX9: a decal's mark is reason enough too - with nothing moving (the fog) the pass never ran.
	Bool anyDecalMark = FALSE;
	for (Int d = 0; d < s_decalMarkCount && !anyDecalMark; ++d)
		anyDecalMark = decalMarkDrawn(s_decalMarks[d]);
	if (s_velocityOn && (s_meshDrawCount > 0 || cursorBibLive() || s_waterVertCount > 0 || s_waterMaskDrawn || s_vanishCount > 0 || anyDecalMark) && s_velMeshVS != NULL && s_velMeshPS != NULL &&
		depthTex != NULL && s_curViewProjValid && s_prevViewProjValid && ensureVelocity((UnsignedInt)fbW, (UnsignedInt)fbH))
	{
		IDirect3DSurface9 *vSurf  = s_velTex->Get_D3D_Surface_Level();
		IDirect3DSurface9 *prevRT = NULL;
		if (vSurf != NULL && SUCCEEDED(dev->GetRenderTarget(0, &prevRT)) && prevRT != NULL)
		{
			if (SUCCEEDED(dev->SetRenderTarget(0, vSurf)))
			{
				dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);	// A 0 = not written
				// Ronin @bugfix 24/09/2026 DX9: rasterise with the viewport the SCENE drew with, so each
				// velocity texel lines up with the scene and depth texels it describes.
				const D3DVIEWPORT9 vvp = s_meshVpValid ? s_meshVp : vp;
				dev->SetViewport(&vvp);		// SetRenderTarget reset it to the whole target
				// Ronin @bugfix 03/10/2026 DX9: the water's own marks, first - as the flat polygons were, so a ship on the
				// water still overwrites them with its velocity. A 1:1 copy over the scene's viewport.
				if (s_waterMaskDrawn && s_waterMaskTex != NULL && s_waterMaskPS != NULL)
				{
					const float mu0 = (float)vvp.X / (float)fbW;
					const float mv0 = (float)vvp.Y / (float)fbH;
					const float mu1 = (float)(vvp.X + vvp.Width)  / (float)fbW;
					const float mv1 = (float)(vvp.Y + vvp.Height) / (float)fbH;
					const float mhx = 1.0f / (float)vvp.Width;		// half a pixel in clip units, as the resolve's quad
					const float mhy = 1.0f / (float)vvp.Height;
					QuadVertex m[4];
					m[0].x = -1.0f - mhx; m[0].y =  1.0f + mhy; m[0].u = mu0; m[0].v = mv0;
					m[1].x =  1.0f - mhx; m[1].y =  1.0f + mhy; m[1].u = mu1; m[1].v = mv0;
					m[2].x = -1.0f - mhx; m[2].y = -1.0f + mhy; m[2].u = mu0; m[2].v = mv1;
					m[3].x =  1.0f - mhx; m[3].y = -1.0f + mhy; m[3].u = mu1; m[3].v = mv1;
					for (int mi = 0; mi < 4; ++mi)
						m[mi].z = 0.0f;
					DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::waterMask");
					dev->SetVertexShader(s_quadVS);		// AFTER the layout bind
					dev->SetPixelShader(s_waterMaskPS);
					dev->SetTexture(0, s_waterMaskTex->Peek_D3D_Texture());
					setResolveSampler(dev, 0);		// POINT
					dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, m, sizeof(QuadVertex));
					dev->SetTexture(0, NULL);
				}
				DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ, "R3DTaa::velMesh");
				dev->SetVertexShader(s_velMeshVS);	// AFTER the layout bind - BindLayoutFVF clears the VS
				dev->SetPixelShader(s_velMeshPS);
				dev->SetTexture(2, depthTex);
				setResolveSampler(dev, 2);		// POINT
				const float pc0[4] = { (float)vvp.Width, (float)vvp.Height, s_zNear, s_zFar };
				const float pc1[4] = { (float)vvp.X / (float)fbW, (float)vvp.Y / (float)fbH,
									   (float)vvp.Width / (float)fbW, (float)vvp.Height / (float)fbH };
				dev->SetPixelShaderConstantF(10, pc0, 1);	// c10/c11 - c0..c9 belong to the resolve, c1..c4 is g_Reproject
				dev->SetPixelShaderConstantF(11, pc1, 1);
				s_lastVelOK = TRUE;

				// Ronin @feature 24/09/2026 DX9: translucent FIRST, so an opaque mover under a rotor overwrites it with its
				// velocity (which already forces the clamp). c12 = { alpha out, one-sided depth test, zero velocity }.
				// Ronin @bugfix 26/09/2026 DX9: the placement bib FIRST, so anything moving over it overwrites it. c12.y = 2: no
				// depth test - the corners are a flat quad at the preview's height and the terrain under it need not be flat.
				if (cursorBibLive())
				{
					static const float bibC12[4] = { 0.3f, 2.0f, 1.0f, 0.0f };	// GHOST, no depth test, zero velocity
					static const UnsignedShort bibIdx[6] = { 0, 1, 2, 0, 2, 3 };
					dev->SetPixelShaderConstantF(12, bibC12, 1);
					D3DXMATRIX bcT, bpT;
					D3DXMatrixTranspose(&bcT, &s_curViewProjClean);
					D3DXMatrixTranspose(&bpT, &s_prevViewProj);
					dev->SetVertexShaderConstantF(0, (const float *)&bcT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&bpT, 4);
					dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, bibIdx, D3DFMT_INDEX16,
												s_bibCorners, sizeof(Vector3));
				}

				// Ronin @bugfix 27/10/2026 DX9: the decals, in the velocity target with their own motion, only where their texture shows and the
				// ground under them is what is seen (TaaDecalMark_vs/_ps). A ring keeps its history as it moves; the shadow mask leaves it alone.
				for (Int d = 0; d < s_decalMarkCount; )
				{
					if ((s_moverFrame - s_decalMarks[d].seenFrame) > 3)
					{
						REF_PTR_RELEASE(s_decalMarks[d].texture);	// long unseen: its decal is gone
						s_decalMarks[d] = s_decalMarks[--s_decalMarkCount];
						s_decalMarks[s_decalMarkCount].texture = NULL;
						continue;
					}
					++d;
				}
				Bool decalMarked = FALSE;
				for (Int d = 0; d < s_decalMarkCount && s_decalMarkVS != NULL && s_decalMarkPS != NULL; ++d)
				{
					const TaaDecalMark &dm = s_decalMarks[d];
					if (!decalMarkDrawn(dm) || dm.texture == NULL || dm.texture->Peek_D3D_Texture() == NULL)
						continue;
					enum { G = TAA_DECAL_GRID, GV = (TAA_DECAL_GRID + 1) * (TAA_DECAL_GRID + 1) };
					static UnsignedShort gridIdx[G * G * 6];
					static Bool gridIdxReady = FALSE;
					if (!gridIdxReady)
					{
						Int k = 0;
						for (Int gy = 0; gy < G; ++gy)
							for (Int gx = 0; gx < G; ++gx)
							{
								const UnsignedShort a = (UnsignedShort)(gy * (G + 1) + gx);
								const UnsignedShort b = (UnsignedShort)(a + G + 1);
								gridIdx[k++] = a; gridIdx[k++] = (UnsignedShort)(a + 1); gridIdx[k++] = (UnsignedShort)(b + 1);
								gridIdx[k++] = a; gridIdx[k++] = (UnsignedShort)(b + 1); gridIdx[k++] = b;
							}
						gridIdxReady = TRUE;
					}
					struct MarkVertex { float x, y, z, u, v, px, py, pz; };
					static MarkVertex grid[GV];
					for (Int gy = 0; gy <= G; ++gy)
					{
						const Real t = (Real)gy / (Real)G;
						for (Int gx = 0; gx <= G; ++gx)
						{
							// Corners: 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left - queueDecal's order, UV 0..1 across.
							const Real s = (Real)gx / (Real)G;
							const Vector3 p = (dm.corner[0] + (dm.corner[1] - dm.corner[0]) * s) * (1.0f - t) +
											  (dm.corner[3] + (dm.corner[2] - dm.corner[3]) * s) * t;
							const Vector3 q = (dm.prevCorner[0] + (dm.prevCorner[1] - dm.prevCorner[0]) * s) * (1.0f - t) +
											  (dm.prevCorner[3] + (dm.prevCorner[2] - dm.prevCorner[3]) * s) * t;
							MarkVertex &mv = grid[gy * (G + 1) + gx];
							// A unit up off the ground the grid lies on: the shader's depth test has room for the ground between points.
							mv.x = p.X;  mv.y = p.Y;  mv.z = ((TheTerrainLogic != NULL) ? TheTerrainLogic->getGroundHeight(p.X, p.Y) : 0.0f) + 1.0f;
							mv.px = q.X; mv.py = q.Y; mv.pz = ((TheTerrainLogic != NULL) ? TheTerrainLogic->getGroundHeight(q.X, q.Y) : 0.0f) + 1.0f;
							mv.u = s;
							mv.v = t;
						}
					}
					if (!decalMarked)
					{
						decalMarked = TRUE;
						DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE3(1), "R3DTaa::decalMark");
						dev->SetVertexShader(s_decalMarkVS);	// AFTER the layout bind - BindLayoutFVF clears the VS
						dev->SetPixelShader(s_decalMarkPS);
						D3DXMATRIX dcT, dpT;
						D3DXMatrixTranspose(&dcT, &s_curViewProjClean);
						D3DXMatrixTranspose(&dpT, &s_prevViewProj);
						dev->SetVertexShaderConstantF(0, (const float *)&dcT, 4);
						dev->SetVertexShaderConstantF(4, (const float *)&dpT, 4);
						dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
						dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
						dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
						dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
						dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
					}
					// 1 = a mover (moved in the last 3 frames); 0.15 = present, still. Both keep the shadow mask off (it takes alpha < 0.1).
					const float decalC12[4] = { decalMarkMoving(dm) ? 1.0f : 0.15f, 0.0f, 0.0f, dm.additive ? 1.0f : 0.0f };
					dev->SetPixelShaderConstantF(12, decalC12, 1);
					dev->SetTexture(0, dm.texture->Peek_D3D_Texture());
					dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, GV, G * G * 2, gridIdx, D3DFMT_INDEX16, grid, sizeof(MarkVertex));
				}
				if (decalMarked)
				{
					// Back to the velocity pass's own layout and shaders, for the water and the meshes after.
					dev->SetTexture(0, NULL);
					DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ, "R3DTaa::velMesh");
					dev->SetVertexShader(s_velMeshVS);
					dev->SetPixelShader(s_velMeshPS);
				}

				// Ronin @bugfix 29/09/2026 DX9: the water - reactive translucent (one-sided: it writes no depth), zero velocity. Before
				// the meshes, so a ship on it overwrites it with its own velocity.
				if (s_waterVertCount > 0)
				{
					static const float waterC12[4] = { 0.4f, 1.0f, 1.0f, 0.0f };
					dev->SetPixelShaderConstantF(12, waterC12, 1);
					D3DXMATRIX wcT, wpT;
					D3DXMatrixTranspose(&wcT, &s_curViewProjClean);
					D3DXMatrixTranspose(&wpT, &s_prevViewProj);
					dev->SetVertexShaderConstantF(0, (const float *)&wcT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&wpT, 4);
					dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(s_waterVertCount / 3), s_waterVerts, sizeof(Vector3));
				}

				// Ronin @bugfix 09/10/2026 DX9: the vanished meshes' boxes, one-sided, zero velocity, this frame and the next. Before the meshes, so
				// anything still standing there overwrites it. Ronin @bugfix 01/11/2026 DX9: 0.45, the resolve clamps the box and restarts
				// only the mesh's own pixels (its depth is gone); the whole box reset as a GHOST (0.3) flashed.
				if (s_vanishCount > 0)
				{
					static const float ghostC12[4] = { 0.45f, 1.0f, 1.0f, 0.0f };
					static const unsigned short boxIdx[36] = { 0,1,2, 2,1,3,  4,6,5, 5,6,7,  0,4,1, 1,4,5,
															  2,3,6, 6,3,7,  0,2,4, 4,2,6,  1,5,3, 3,5,7 };
					D3DXMATRIX vpT;
					D3DXMatrixTranspose(&vpT, &s_curViewProjClean);
					dev->SetPixelShaderConstantF(12, ghostC12, 1);
					dev->SetVertexShaderConstantF(0, (const float *)&vpT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&vpT, 4);
					for (Int vb = 0; vb < s_vanishCount; ++vb)
					{
						const float *b = s_vanishBox[vb];
						Vector3 corner[8];
						for (Int ci = 0; ci < 8; ++ci)
							corner[ci].Set(b[0] + ((ci & 1) ? b[3] : -b[3]), b[1] + ((ci & 2) ? b[4] : -b[4]), b[2] + ((ci & 4) ? b[5] : -b[5]));
						dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 8, 12, boxIdx, D3DFMT_INDEX16, corner, sizeof(Vector3));
					}
				}

				// Ronin @bugfix 09/10/2026 DX9: the selection-flash mask as render target 1 while the meshes draw (TaaVelMesh_ps COLOR1 = c13).
				// Only when one flashes. A mesh in front that does not flash writes zero there - the visible surface owns the pixel.
				Bool flashBound = FALSE;
				for (Int fk = 0; fk < s_meshDrawCount; ++fk)
				{
					if (s_meshDraws[fk].flash[3] > 0.5f)
					{
						flashBound = bindFlashMask(dev, (UnsignedInt)fbW, (UnsignedInt)fbH);
						break;
					}
				}

				Int boundKind = -1;
				for (Int k = 0; k < 2 * s_meshDrawCount; ++k)
				{
					TaaMeshDraw &md = s_meshDraws[k % s_meshDrawCount];
					const Bool firstGroup = (md.kind == TAA_KIND_TRANSLUCENT || md.kind == TAA_KIND_GHOST) ? TRUE : FALSE;
					if (firstGroup != (k < s_meshDrawCount))
						continue;
					if (md.skinCur != NULL)
						continue;		// skins: their own layout, below
					MeshModelClass *mdl = md.mesh->Peek_Model();
					if (mdl == NULL)
						continue;
					if (md.kind != boundKind)
					{
						static const float kindC12[7][4] = {
							{ 1.0f, 0.0f, 0.0f, 0.0f },		// velocity
							{ 0.7f, 0.0f, 0.0f, 0.0f },		// velocity + reactive
							{ 0.4f, 0.0f, 1.0f, 0.0f },		// reactive, opaque
							{ 0.4f, 1.0f, 1.0f, 0.0f },		// reactive, translucent: writes no depth, so one-sided
							{ 0.15f, 0.0f, 1.0f, 0.0f },	// still: stopped this frame, present but no mover
							{ 0.3f,  1.0f, 1.0f, 0.0f },		// ghost: moving runtime-translucent, one-sided, reset by the resolve
							{ 0.92f, 0.0f, 0.0f, 0.0f } };	// Ronin @bugfix 27/09/2026 DX9: idle infantry - velocity, flagged
						dev->SetPixelShaderConstantF(12, kindC12[md.kind], 1);
						boundKind = md.kind;
					}
					const Int vcount = mdl->Get_Vertex_Count();
					const Int pcount = mdl->Get_Polygon_Count();
					if (vcount <= 0 || pcount <= 0 || vcount > 65535)	// 16-bit indices
						continue;
					const Vector3  *verts = mdl->Get_Vertex_Array();
					const TriIndex *polys = mdl->Get_Polygon_Array();
					if (verts == NULL || polys == NULL)
						continue;

					// World in the D3D row-vector form Set_Transform(D3DTS_WORLD) uses, then x view-projection, then
					// transposed for mul(rowVec, M). The previous one pairs last frame's world with last frame's camera.
					D3DXMATRIX wCur(To_D3DMATRIX(md.cur)), wPrev(To_D3DMATRIX(md.prev));
					D3DXMATRIX cWVP, pWVP, cT, pT;
					D3DXMatrixMultiply(&cWVP, &wCur,  &s_curViewProjClean);
					D3DXMatrixMultiply(&pWVP, &wPrev, &s_prevViewProj);
					D3DXMatrixTranspose(&cT, &cWVP);
					D3DXMatrixTranspose(&pT, &pWVP);
					dev->SetVertexShaderConstantF(0, (const float *)&cT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&pT, 4);
					if (flashBound)
						dev->SetPixelShaderConstantF(13, md.flash, 1);
					dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)vcount, (UINT)pcount,
												polys, D3DFMT_INDEX16, verts, sizeof(Vector3));
				}

				// Ronin @feature 26/09/2026 DX9: §14 stage 2 - SKINS. Already in world space, so only the view-projections;
				// each vertex carries this frame's and last frame's position. Same pixel shader as the rigid draws.
				if (s_skinDrawCount > 0 && s_velSkinVS != NULL)
				{
					DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0), "R3DTaa::velSkin");
					dev->SetVertexShader(s_velSkinVS);	// AFTER the layout bind - it clears the VS
					setResolveSampler(dev, 2);			// POINT, re-set after the rebind
					D3DXMATRIX cT, pT;
					D3DXMatrixTranspose(&cT, &s_curViewProjClean);
					D3DXMatrixTranspose(&pT, &s_prevViewProj);
					dev->SetVertexShaderConstantF(0, (const float *)&cT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&pT, 4);
					for (Int k = 0; k < s_meshDrawCount; ++k)
					{
						TaaMeshDraw &md = s_meshDraws[k];
						if (md.skinCur == NULL)
							continue;
						MeshModelClass *mdl = md.mesh->Peek_Model();
						if (mdl == NULL)
							continue;
						const Int vcount = mdl->Get_Vertex_Count();
						const Int pcount = mdl->Get_Polygon_Count();
						const TriIndex *polys = mdl->Get_Polygon_Array();
						if (vcount <= 0 || pcount <= 0 || vcount > 65535 || polys == NULL)
							continue;
						if (md.kind != boundKind)
						{
							static const float skinC12[7][4] = {
								{ 1.0f, 0.0f, 0.0f, 0.0f }, { 0.7f, 0.0f, 0.0f, 0.0f }, { 0.4f, 0.0f, 1.0f, 0.0f },
								{ 0.4f, 1.0f, 1.0f, 0.0f }, { 0.15f, 0.0f, 1.0f, 0.0f }, { 0.3f, 1.0f, 1.0f, 0.0f },
								{ 0.92f, 0.0f, 0.0f, 0.0f } };	// as kindC12 above
							dev->SetPixelShaderConstantF(12, skinC12[md.kind], 1);
							boundKind = md.kind;
						}
						if (s_skinScratchCap < vcount)
						{
							delete [] s_skinScratch;
							s_skinScratch    = W3DNEWARRAY float[vcount * 6];
							s_skinScratchCap = vcount;
						}
						for (Int v = 0; v < vcount; ++v)
						{
							float *o = s_skinScratch + v * 6;
							o[0] = md.skinCur[v].X;  o[1] = md.skinCur[v].Y;  o[2] = md.skinCur[v].Z;
							o[3] = md.skinPrev[v].X; o[4] = md.skinPrev[v].Y; o[5] = md.skinPrev[v].Z;
						}
						if (flashBound)
							dev->SetPixelShaderConstantF(13, md.flash, 1);
						dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)vcount, (UINT)pcount,
													polys, D3DFMT_INDEX16, s_skinScratch, sizeof(float) * 6);
					}
				}
				if (flashBound)
					dev->SetRenderTarget(1, NULL);	// Ronin @bugfix 09/10/2026 DX9: before target 0 goes back
			}
			dev->SetRenderTarget(0, prevRT);
			prevRT->Release();
		}
		if (vSurf != NULL)
			vSurf->Release();
		dev->SetViewport(&vp);

		// The resolve passes below assume their layout and shaders are still bound.
		DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DTaa::postRender");
		dev->SetVertexShader(s_quadVS);
		dev->SetPixelShader(s_resolvePS);
	}

	// Ronin @bugfix 27/09/2026 DX9: the moving-shadow mask - after the velocity pass (it reads the same mover list), before
	// the resolve reads it. Timed on its own.
	gpuMark(1);
	renderMoverMask(dev, vp, (depthTex != NULL) ? TRUE : FALSE);
	gpuMark(2);

	dev->SetTexture(0, sceneTex);
	setResolveSampler(dev, 0);

	// ---- pass 1: scene + previous history -> this frame's history ---------------------------------------------
	// Blending into the history target rather than straight to screen is what makes the accumulation compound: next
	// frame reads what we write here, not the one-frame-old back buffer.
	IDirect3DSurface9 *oldRT = NULL;
	Bool wroteHistory = FALSE;
	if (haveHistory && SUCCEEDED(dev->GetRenderTarget(0, &oldRT)) && oldRT != NULL)
	{
		TextureClass *cur  = s_history[s_historyIndex];
		TextureClass *prev = s_history[s_historyIndex ^ 1];
		IDirect3DSurface9 *curSurf = cur->Get_D3D_Surface_Level();
		if (curSurf != NULL)
		{
			if (SUCCEEDED(dev->SetRenderTarget(0, curSurf)))
			{
				// SetRenderTarget resets the viewport to the whole target; the 3D view is a sub-rect of it.
				dev->SetViewport(&vp);

				// Ronin @bugfix 09/10/2026 DX9: the history's low bits: written as render target 1, last frame's read at s9 (c6.w) when it
				// was written in the frame just before this one. Blending is off here, so two targets need no blend cap.
				IDirect3DSurface9 *lowSurf = ensureHistoryLow(s_historyW, s_historyH)
											 ? s_historyLo[s_historyIndex]->Get_D3D_Surface_Level() : NULL;
				const Bool writeLow = (lowSurf != NULL && SUCCEEDED(dev->SetRenderTarget(1, lowSurf))) ? TRUE : FALSE;
				if (lowSurf != NULL)
					lowSurf->Release();
				const Bool readLow = (writeLow && s_historyValid && s_lowStamp != 0 && s_moverFrame - s_lowStamp == 1) ? TRUE : FALSE;
				if (writeLow)
					dev->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
																 D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
				if (readLow)
				{
					dev->SetTexture(9, s_historyLo[s_historyIndex ^ 1]->Peek_D3D_Texture());
					setResolveSampler(dev, 9, TRUE);	// LINEAR, as the history itself
				}

				// Only trust the history once a frame has actually been written into it. c0.w turns reprojection on:
				// without depth or a previous matrix the shader keeps the history where it is, which is 2b-i's
				// behaviour — softer while panning, but never wrong.
				const float weight = s_historyValid ? s_weight : 0.0f;
				const float c0[4] = { weight, 1.0f / (float)fbW, 1.0f / (float)fbH,
									  canReproject ? 1.0f : 0.0f };
				dev->SetPixelShaderConstantF(0, c0, 1);
				static const float vividOff[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				dev->SetPixelShaderConstantF(22, vividOff, 1);		// Ronin @feature 22/10/2026 DX9: the history never takes vibrance

				// Debug views draw in this pass so they show what the RESOLVE sees, not a re-derived picture.
				// yz carry the clip planes: raw D3D depth sits above 0.99 for almost the whole scene, so the depth
				// debug view has to LINEARISE or it is a white screen that tells you nothing. (It told me nothing.)
				const float c6[4] = { (float)s_debugMode, s_zNear, s_zFar, readLow ? 1.0f : 0.0f };
				static const float flashOff[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				dev->SetPixelShaderConstantF(8, flashOff, 1);	// Ronin @bugfix 09/10/2026 DX9: c8.w = 0: the flash never enters the history
				const float c7[4] = { 0.0f, (s_lastVelOK && s_velTex != NULL) ? 1.0f : 0.0f, (float)s_reflMark, s_clamp };	// x = 0: no CAS into the history
				dev->SetPixelShaderConstantF(6, c6, 1);
				dev->SetPixelShaderConstantF(7, c7, 1);
				// Ronin @feature 12/10/2026 DX9: HDR stage 1b: c9 = the tone curve - x = `hdr white`, y = 1: stage 0 and the copy before
				// particles hold the float scene and are bent as they are read. The history stays 8-bit, already bent.
				const float c9[4] = { s_white, s_floatNow ? 1.0f : 0.0f, 0.0f, 0.0f };
				dev->SetPixelShaderConstantF(9, c9, 1);

				// Ronin @bugfix 01/11/2026 DX9: the fog of war's change mask at s12, screen clip -> its uv at c25..c28, c29.x = bound.
				updateFogMask();
				if (s_fogReady)
				{
					dev->SetTexture(12, s_fogMaskTex->Peek_D3D_Texture());
					setResolveSampler(dev, 12, TRUE);	// LINEAR - the shroud is read filtered, so a texel's change reaches as far
					dev->SetPixelShaderConstantF(25, s_toFogT, 4);
				}
				const float c29[4] = { s_fogReady ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
				dev->SetPixelShaderConstantF(29, c29, 1);
				const Bool usePrevVel = (s_prevVelOK && s_velPrevTex != NULL);	// disocc AND last frame's reactive flags
				if (usePrevVel)
				{
					dev->SetTexture(6, s_velPrevTex->Peek_D3D_Texture());
					setResolveSampler(dev, 6);		// POINT - the alpha is a flag, not a value to blend
				}
				// Ronin @bugfix 09/10/2026 DX9: the local lights' change mask at s8 (c10.w), only when R3DLocalLights drew it this frame.
				const Bool useLightMask = (s_lightMaskTex != NULL && s_lightMaskStamp == s_moverFrame &&
										   s_lightMaskW == (UnsignedInt)fbW && s_lightMaskH == (UnsignedInt)fbH) ? TRUE : FALSE;
				if (useLightMask)
				{
					dev->SetTexture(8, s_lightMaskTex->Peek_D3D_Texture());
					setResolveSampler(dev, 8);		// POINT - 1:1 with the scene
				}
				const float c10[4] = { usePrevVel ? 1.0f : 0.0f, (float)s_disocc, s_reactiveW, useLightMask ? 1.0f : 0.0f };
				dev->SetPixelShaderConstantF(10, c10, 1);	// uploaded HERE, after the velocity pass used c10/c11
				const Bool useAuto = autoReactUsable(fbW, fbH);
				if (useAuto)
				{
					dev->SetTexture(7, s_opaqueTex);
					setResolveSampler(dev, 7);		// POINT - compared 1:1 with the scene
				}
				const float c11[4] = { 1.0f, autoReactSlope(), s_disoccV, useAuto ? s_autoReact : 0.0f };	// Ronin @bugfix 08/10/2026 DX9: x = an effect that left resets, y = the mask's slope
				dev->SetPixelShaderConstantF(11, c11, 1);	// likewise after the velocity pass
				// Ronin @bugfix 27/09/2026 DX9: the moving-shadow mask at s3, screen clip -> sun clip at c12..c15, its params at
				// c16 (x = 0: not drawn this frame, the resolve skips it). After the velocity pass, which used c12.
				static const float moverOff[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				if (s_moverOK)
				{
					dev->SetTexture(3, s_moverTex->Peek_D3D_Texture());
					setResolveSampler(dev, 3);		// POINT - packed depths
					dev->SetPixelShaderConstantF(12, s_moverToSunT, 4);
				}
				dev->SetPixelShaderConstantF(16, s_moverOK ? s_moverParams : moverOff, 1);

				if (s_lastVelOK && s_velTex != NULL)
				{
					dev->SetTexture(5, s_velTex->Peek_D3D_Texture());
					// POINT: packed velocity and kind flags must never be interpolated.
					setResolveSampler(dev, 5);
				}
				dev->SetTexture(1, prev->Peek_D3D_Texture());
				setResolveSampler(dev, 1, TRUE);	// LINEAR — the reprojected lookup lands between texels
				// POINT: a packed depth must never be interpolated - blending two encodings gives a nonsense value.
				if (s_depthHist[s_historyIndex ^ 1] != NULL)
				{
					dev->SetTexture(4, s_depthHist[s_historyIndex ^ 1]->Peek_D3D_Texture());
					setResolveSampler(dev, 4);
				}
				dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadVertex));
				if (writeLow)
				{
					dev->SetRenderTarget(1, NULL);	// Ronin @bugfix 09/10/2026 DX9: before the depth store takes target 0
					s_lowStamp = s_moverFrame;
				}
				dev->SetTexture(9, NULL);
				dev->SetTexture(1, NULL);
				dev->SetTexture(4, NULL);
				dev->SetTexture(5, NULL);
				dev->SetTexture(6, NULL);
				dev->SetTexture(7, NULL);
				dev->SetTexture(3, NULL);
				dev->SetTexture(8, NULL);
				dev->SetTexture(12, NULL);	// Ronin @bugfix 01/11/2026 DX9: the fog's change mask
				wroteHistory = TRUE;
			}
			curSurf->Release();
		}
		// ---- depth store: this frame's linear depth -> depthHist[cur], for next frame's validation ------------
		IDirect3DSurface9 *dSurf = (wroteHistory && depthTex != NULL && s_depthStorePS != NULL && s_depthHist[s_historyIndex] != NULL)
								   ? s_depthHist[s_historyIndex]->Get_D3D_Surface_Level() : NULL;
		if (dSurf != NULL)
		{
			if (SUCCEEDED(dev->SetRenderTarget(0, dSurf)))
			{
				dev->SetViewport(&vp);
				dev->SetPixelShader(s_depthStorePS);
				// Ronin @feature 26/09/2026 DX9: c0.w = the auto-reactive threshold; the flag goes in alpha.
				const Bool useAutoD = autoReactUsable(fbW, fbH);
				const float dcfg[4] = { autoReactSlope(), s_zNear, s_zFar, useAutoD ? s_autoReact : 0.0f };
				dev->SetPixelShaderConstantF(0, dcfg, 1);
				dev->SetTexture(2, depthTex);
				setResolveSampler(dev, 2);
				dev->SetTexture(0, sceneTex);
				setResolveSampler(dev, 0);
				if (useAutoD)
				{
					dev->SetTexture(7, s_opaqueTex);
					setResolveSampler(dev, 7);
				}
				dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadVertex));
				dev->SetTexture(7, NULL);
				dev->SetPixelShader(s_resolvePS);
				s_depthStoredNow = TRUE;		// Ronin @feature 18/10/2026 DX9: next frame's screen-space reflections may read it
			}
			dSurf->Release();
		}

		dev->SetRenderTarget(0, oldRT);
		dev->SetViewport(&vp);
		oldRT->Release();
	}

	// ---- pass 2: the resolved image -> the frame buffer ---------------------------------------------------------
	// Weight 0 makes the shader a straight copy of stage 0. If pass 1 could not run we fall back to copying the raw
	// scene, so a missing history target costs quality, never a broken frame.
	{
		// Ronin @feature 22/10/2026 DX9: local contrast's surroundings, from what this copy shows, before its constants are set.
		buildSurround(dev, wroteHistory ? (IDirect3DBaseTexture9 *)s_history[s_historyIndex]->Peek_D3D_Texture() : (IDirect3DBaseTexture9 *)sceneTex,
					  (UnsignedInt)fbW, (UnsignedInt)fbH, c5, (s_floatNow && !wroteHistory) ? TRUE : FALSE);
		// RGB only for the screen copy. Alpha had to be writable for the history above, but the frame buffer's alpha
		// belongs to the soft water edge (Windowednew §31) and nothing here should be writing it.
		dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
													D3DCOLORWRITEENABLE_BLUE);
		const float c0[4] = { 0.0f, 1.0f / (float)fbW, 1.0f / (float)fbH, 0.0f };
		dev->SetPixelShaderConstantF(0, c0, 1);
		// Debug 3 is the ONE view that must run in this pass: it reads back the motion memory the resolve stored in
		// alpha. Running it in the resolve pass instead would write the debug colour into the history and corrupt the
		// very signal being measured. Every other view stays off here.
		const float dbg = (s_debugMode == 3) ? 3.0f : 0.0f;
		const float c6off[4] = { dbg, 0.0f, 0.0f, 0.0f };
		dev->SetPixelShaderConstantF(6, c6off, 1);
		const float c7cas[4] = { s_sharpen, 0.0f, 0.0f, 0.0f };	// Ronin @feature 26/09/2026 DX9: CAS here, never in pass 1
		dev->SetPixelShaderConstantF(7, c7cas, 1);
		// Ronin @feature 12/10/2026 DX9: HDR stage 1b: stage 0 is the bent history here - or, without one, the raw float scene.
		const float c9copy[4] = { s_white, (s_floatNow && !wroteHistory) ? 1.0f : 0.0f, 0.0f, 0.0f };
		dev->SetPixelShaderConstantF(9, c9copy, 1);
		// Ronin @feature 15/10/2026 DX9: THE NIGHT LOOK (`night`): c17 = the dim's tint, w = on; c18 = lo, hi, floor.
		// Ronin @feature 15/10/2026 DX9: the light mask at s8 too when it is this frame's: its alpha keeps a lamp-lit pixel's colour.
		const Bool nightMask = (s_lightMaskTex != NULL && s_lightMaskStamp == s_moverFrame && s_lightMaskW == (UnsignedInt)fbW &&
								s_lightMaskH == (UnsignedInt)fbH) ? TRUE : FALSE;
		if (nightMask)
		{
			dev->SetTexture(8, s_lightMaskTex->Peek_D3D_Texture());
			setResolveSampler(dev, 8);
		}
		float nightC[8], nightLevel[3];
		nightConstants(nightC, nightLevel, nightMask);
		dev->SetPixelShaderConstantF(17, nightC, 2);
		// Ronin @feature 15/10/2026 DX9: the dark lift (`night lift`), c19: at night, with the look switched on.
		const float nightLiftC[4] = { s_nightLift, (s_nightOn && R3DLocalLights::isEnabled() && TheGlobalData != NULL && TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) ? 1.0f : 0.0f,
									  0.0f, 0.0f };
		dev->SetPixelShaderConstantF(19, nightLiftC, 1);
		// Ronin @bugfix 09/10/2026 DX9: the selection flash, added here - after the resolve, never in the history. c8 = 1 / the map's light
		// (frame -> surface colour, as the local lights use) and the strength; s10 = the mask the velocity pass drew.
		const Bool useFlash = (s_flashTex != NULL && s_flashStamp == s_moverFrame && s_flashW == (UnsignedInt)fbW &&
							   s_flashH == (UnsignedInt)fbH) ? TRUE : FALSE;
		float c8[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
		if (useFlash)
		{
			getMapLightFactor(c8);
			c8[3] = TAA_FLASH_GAIN;
			dev->SetTexture(10, s_flashTex->Peek_D3D_Texture());
			setResolveSampler(dev, 10);		// POINT - 1:1 with the frame
		}
		dev->SetPixelShaderConstantF(8, c8, 1);
		dev->SetTexture(0, wroteHistory ? s_history[s_historyIndex]->Peek_D3D_Texture()
										: (IDirect3DBaseTexture9 *)sceneTex);
		setResolveSampler(dev, 0);
		bindGrade(dev);		// Ronin @feature 21/10/2026 DX9: the daytime grade, on the screen copy only - never the history
		bindVivid(dev);
		dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadVertex));
		dev->SetTexture(11, NULL);
		dev->SetTexture(10, NULL);
		dev->SetTexture(8, NULL);	// Ronin @feature 15/10/2026 DX9: the night look's light mask
	}

	if (wroteHistory)
	{
		s_historyValid = TRUE;
		// Ronin @feature 18/10/2026 DX9: what the screen-space reflections read of it: when, its camera, where its 3D view sits.
		s_histFrame[s_historyIndex]    = s_frameNo;
		s_histWhole[s_historyIndex]    = (s_depthStoredNow && s_curViewProjValid) ? TRUE : FALSE;
		s_histViewProj[s_historyIndex] = s_curViewProjClean;
		for (Int k = 0; k < 4; ++k)
			s_histView[s_historyIndex][k] = c5[k];
		s_historyIndex ^= 1;		// what we just wrote becomes next frame's "previous"

		// Ronin @feature 24/09/2026 DX9: this frame's velocity becomes next frame's "where movers were".
		TextureClass *t = s_velPrevTex;
		s_velPrevTex = s_velTex;
		s_velTex     = t;
		s_olderVelOK = s_prevVelOK;		// Ronin @feature 19/10/2026 DX9: the frame before, for the reflections' mover mark
		s_prevVelOK  = s_lastVelOK;
	}

	gpuEnd();		// Ronin @diagnostic 27/09/2026 DX9: closes gpuBegin's frame

	dev->SetVertexShader(NULL);
	dev->SetPixelShader(NULL);
	dev->SetTexture(0, NULL);
	dev->SetTexture(2, NULL);
	if (depthSuspended)
		DX8Wrapper::Resume_Scene_Depth();
	dev->SetRenderState(D3DRS_ZENABLE,          oldZ);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     oldZW);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAB);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  oldAT);
	dev->SetRenderState(D3DRS_CULLMODE,         oldCull);
	// Back to whatever the frame had — the soft water edge depends on alpha writes being OFF (Windowednew §31).
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, oldCW);

	// Our raw SetRenderState / SetTexture calls desynced the wrapper's caches (§12a) — force a resync, same as SSAO.
	ShaderClass::Invalidate();
	DX8Wrapper::Invalidate_Cached_Render_States();
	DX8Wrapper::Invalidate_Texture_State(0, 1);
}
