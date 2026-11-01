/*
**	Command & Conquer Generals Zero Hour(tm)
**	DX9 port: binds the reflective water's shader inputs.
*/

// Ronin @feature 29/09/2026 DX9: binds the WaterSea textures, samplers and constants for one reflective water draw.
// W3DWater.cpp fills WaterSeaInputs; the look is TheWaterSeaProfiles and WaterSea_vs/_ps.hlsl. docs/Water_Work.md.
#pragma once

#include <cmath>
#include <d3d9.h>
#include "d3dx9math.h"
#include "WW3D2/dx8wrapper.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"		// Ronin @bugfix 29/09/2026 DX9: MAP_XY_FACTOR, the height texel size
#include "GameClient/Water.h"		// Ronin @feature 29/09/2026 DX9: TheWaterTransparency, the map's shore settings
#include "W3DDevice/GameClient/R3DWaterSeaTuning.h"
#include "W3DDevice/GameClient/R3DShadowMapState.h"	// Ronin @feature 29/09/2026 DX9: shadows on the water - TheTerrainShadowPass

enum { WATERSEA_STAGES = 7 };	///< s0 caust, s1 reflection, s2 water, s3 edge, s4 normals (+ foam in alpha), s5 heights, s6 frame copy

struct WaterSeaInputs
{
	IDirect3DBaseTexture9 *bump;		///< s0: this frame's caust frame (V8U8)
	IDirect3DBaseTexture9 *reflection;	///< s1: renderMirror's target
	IDirect3DBaseTexture9 *water;		///< s2: the standing-water texture; white for the sea
	IDirect3DBaseTexture9 *edge;		///< s3: a river's edge fade; white elsewhere
	IDirect3DBaseTexture9 *normals;		///< s4: WaterNormal.tga
	Real bumpScale;						///< texbem matrix diagonal (m_fBumpScale)
	Real bumpTilesPerUnit;				///< caust tiling, the sea's 3 per 40 world units
	Real edgeScaleU;					///< rivers 1/HEIGHT_TO_USE (their uv2 is uv1 with u doubled), else 0
	Real clockSeconds;					///< the water clock, logic seconds
	// Ronin @feature 29/09/2026 DX9: phase 1 - the terrain heights, one L8 texel per terrain vertex.
	IDirect3DBaseTexture9 *heights;		///< s5, null = no depth
	Real heightScaleU, heightScaleV;	///< world xy -> uv: 1 / (MAP_XY_FACTOR x extent)
	Real heightOffsetU, heightOffsetV;	///< (border + 0.5) / extent
	Real heightUnit;					///< world z per sampled 0..1 value: 255 x MAP_HEIGHT_SCALE
	// Ronin @feature 29/09/2026 DX9: phase 2 - the swell.
	Bool heightsVTF;					///< `heights` is R32F and the vertex shader may sample it
	Bool swell;							///< this draw takes the swell: the flat polygons, not rivers or the sea plane
	Real swellCell;						///< Ronin @bugfix 03/10/2026 DX9: its grid's real cell, world units (0 = the profile's `swellcell`)
	// Ronin @bugfix 03/10/2026 DX9: the fog of war, sampled by the PS at the displaced surface (s9, c53). White and a zero
	// scale = none here: no shroud, or a draw whose own pass applies it (rivers, the sea plane).
	IDirect3DBaseTexture9 *shroud;
	Real shroudOffsetX, shroudOffsetY;	///< uv = (world xy + offset) x scale (W3DShaderManager::getShroudMapState)
	Real shroudScaleX, shroudScaleY;
	// Ronin @feature 29/09/2026 DX9: phase 3 - refraction.
	IDirect3DBaseTexture9 *scene;		///< s6: the frame just before the water, null = the plain alpha blend
	Real sceneInvW, sceneInvH;			///< 1 / its size, for the pixel's own texel (VPOS)
	Real mirrorInvW, mirrorInvH;		///< Ronin @feature 08/10/2026 DX9: 1 / the mirror's size, for `mirrorsoft`
	// Ronin @bugfix 30/09/2026 DX9: the TAA jitter this frame's projection carries (R3DTaa::getJitterNDC), 0 outside TAA.
	Real jitterX, jitterY;
	// Ronin @feature 02/10/2026 DX9: phase 4 - the WaterKind whose profile this draw takes (sea: a standing polygon reaching
	// the map's edge, and the sea plane; lake: any other standing polygon; river).
	Int kind;
	// Ronin @feature 03/10/2026 DX9: phase 5 - this frame's wake texture (PS s10, VS s1), null = no wake in view or not
	// this draw's (rivers, the sea plane). uv = world xy x scale + offset.
	IDirect3DBaseTexture9 *wake;
	Real wakeScale, wakeOffsetU, wakeOffsetV;
	Bool wakeVTF;						///< the vertex shader may sample it: the wake moves the mesh
};

// Ronin @feature 29/09/2026 DX9: phase 2 - the Gerstner wave table: turn from the wind (degrees), wavelength and height
// (world units), each x the `swell*` knobs. The PS doubles the slopes (WATERSEA_SWELL_NORMAL).
struct WaterSeaWave { Real turn, length, height; };
enum { WATERSEA_WAVE_COUNT = 4 };
// Ronin @feature 03/10/2026 DX9: phase 5 - the sea's waves: short and steep for ZH's ~5-unit seas (slopes kA ~0.08, heights
// 3.95 in all), within 31 degrees of the wind so they roll in lines.
static const WaterSeaWave WATERSEA_WAVE_TABLE[WATERSEA_WAVE_COUNT] =
{
	{   0.0f, 120.0f, 1.6f  },
	{  14.0f,  85.0f, 1.1f  },
	{ -17.0f,  60.0f, 0.75f },
	{  31.0f,  42.0f, 0.5f  },
};
// Ronin @feature 03/10/2026 DX9: phase 2's table, slopes kA ~0.04: the lake keeps it - its look was settled on it (02/10).
static const WaterSeaWave WATERSEA_WAVE_TABLE_CALM[WATERSEA_WAVE_COUNT] =
{
	{   0.0f, 210.0f, 1.4f  },
	{  28.0f, 130.0f, 0.85f },
	{ -37.0f,  80.0f, 0.5f  },
	{  71.0f,  55.0f, 0.35f },
};
// Ronin @feature 03/10/2026 DX9: phase 5 - the sea table's total height: the crest tint and the whitecaps measure a wave
// against it, so a low swell tints little and never breaks. Whitecap trails: seconds behind the crest, and their weights.
static const Real WATERSEA_SWELL_REF    = 3.95f;
static const Real WATERSEA_CAP_LAG[2]   = { 0.3f, 0.6f };
static const Real WATERSEA_CAP_TRAIL[2] = { 0.7f, 0.4f };
static const Real WATERSEA_CAP_SOFT     = 0.15f;	///< the foam's edge, in the same height units as `whitecapat`
static const Real WATERSEA_CAP_NOISE    = 1.0f;		///< how far the ripple maps' slopes fray that edge
static const Real WATERSEA_GRAVITY      = 32.0f;	///< world units / s^2 (a unit is about a foot)
static const Real WATERSEA_SWELL_NORMAL = 2.0f;		///< the PS's normal tilt, x the true slope
// Ronin @feature 02/10/2026 DX9: phase 4 - a river's flow map: seconds per cycle. Longer = more stretch before the cross-fade
// hides the jump back; shorter = the fade pulses.
static const Real WATERSEA_FLOW_CYCLE   = 3.0f;
// Ronin @feature 03/10/2026 DX9: phase 5 - wakes. A moving unit drops a trail point every STEP units; the trail is drawn
// as a ribbon into a WAKE_SIZE texture over the view (WaterWake_ps.hlsl), and the water reads it.
enum { WATERSEA_WAKE_SIZE = 1024 };					///< the wake texture, texels a side (RGBA16F: 8 MB)
static const Real WATERSEA_WAKE_WINDOW  = 768.0f;	///< the smallest world span it covers; doubles until the view fits
static const Real WATERSEA_WAKE_LIFE    = 7.0f;		///< seconds a trail point lasts
static const Real WATERSEA_WAKE_STEP    = 6.0f;		///< world units between trail points
// Ronin @bugfix 04/10/2026 DX9: turns - a point per 6 degrees of heading (and 1 unit run); each side of the ribbon stops at
// FOLD of the way to a neighbour's cross-section, opens by OPEN per unit of trail, fades over RIM; NOSE ahead of the bow.
static const Real WATERSEA_WAKE_STEP_TURN = 1.0f;
static const Real WATERSEA_WAKE_TURN_COS  = 0.99452f;	///< cos(6 degrees)
static const Real WATERSEA_WAKE_FOLD    = 0.9f;
static const Real WATERSEA_WAKE_OPEN    = 1.0f;
static const Real WATERSEA_WAKE_RIM     = 9.0f;
static const Real WATERSEA_WAKE_NOSE    = 18.0f;
static const Real WATERSEA_WAKE_SPEED   = 40.0f;	///< the speed of a full wake, world units a second (BasicBoatLocomotor)
static const Real WATERSEA_WAKE_BEAM    = 12.0f;	///< the half beam of a full wake (the PT boat); wider hulls make more
static const Real WATERSEA_WAKE_APEX    = 2.0f;		///< the V's half-width at the bow
static const Real WATERSEA_WAKE_ARM     = 0.30f;	///< its half-width gained per unit astern: tan(19.47 deg) x 0.85
static const Real WATERSEA_WAKE_FINE[2] = { 2.6f, 0.45f };	///< the arm's sharp ridge: half-width, height (the PS shades it)
// Ronin @bugfix 04/10/2026 DX9: 18 wide (was 12): what the 10-unit swell grid carries smoothly (py: mesh error 2%)
static const Real WATERSEA_WAKE_BROAD[2] = { 18.0f, 1.0f };	///< the arm's swell: half-width, height (the mesh carries it)
static const Real WATERSEA_WAKE_WASH[4] = { 0.004f, 1.3f, 0.9f, 150.0f };	///< the strip astern: widening per unit, its foam; the arms' foam, its length
static const Real WATERSEA_WAKE_FEATHER[3] = { 0.3f, 0.26f, 0.37f };	///< the ridge's wavelets: depth, phase per unit astern, per unit out
static const Real WATERSEA_WAKE_FINE_IN = 30.0f;	///< Ronin @bugfix 04/10/2026 DX9: the ridge and the arms' foam come in over these units astern of the hull
// Ronin @feature 04/10/2026 DX9: rings round a hull at rest - a train leaving its outline: two close ripples that beat (the
// shading) and one long wave the swell grid can carry (the mesh) - and the surge ahead of one that stops.
static const Real WATERSEA_RING_SPEED   = 14.0f;	///< world units a second
static const Real WATERSEA_RING_REACH   = 130.0f;	///< how far from the hull they run
static const Real WATERSEA_RING_FINE[2] = { 13.0f, 17.0f };	///< the two ripples' lengths: a crest a second, in pulses
static const Real WATERSEA_RING_LONG    = 50.0f;	///< the mesh's wave
static const Real WATERSEA_RING_HEIGHT[2] = { 0.18f, 0.5f };	///< the ripples', the long wave's
static const Real WATERSEA_RING_REST    = 0.3f;		///< rings only under this share of a boat's full speed
static const Real WATERSEA_SURGE_WIDTH  = 16.0f;	///< what the swell grid carries (py: mesh error 5-8%; 79% worst at 10)
static const Real WATERSEA_SURGE_LIFE   = 3.0f;
static const Real WATERSEA_SURGE_SPEED  = 24.0f;	///< x the speed the unit had, as a share of a boat's full speed
static const Real WATERSEA_SURGE_HEIGHT = 1.4f;		///< x the same share
static const Real WATERSEA_SURGE_FOAM   = 0.6f;
static const Real WATERSEA_SURGE_FROM   = 0.5f;		///< a surge leaves a unit that was at least this fast ...
static const Real WATERSEA_SURGE_DROP   = 0.35f;	///< ... once it is down to this share of that
// Ronin @feature 04/10/2026 DX9: the hollow along the track astern, x the swell's height (py: 0.9 puts the water a hull
// length astern from 0..+19% brightness to -14..+19% at `wakeheight` 4; the mesh's error is unchanged).
static const Real WATERSEA_WAKE_HOLLOW  = 0.8f;

// Ronin @feature 03/10/2026 DX9: phase 5 - one wave as this frame's knobs make it: for the shaders' constants (WaterSea_Bind)
// and for the CPU's copy of the surface (WaterRenderObjClass::swellHeight) - one source, so a boat rides what the VS draws.
struct WaterSeaWaveNow
{
	Real dx, dy;	///< the direction it travels
	Real k, omega;	///< 2 pi / length; radians a second
	Real a;			///< height
	Real qa;		///< sideways travel (`swellsharp`)
	Bool moves;		///< at least 4 grid cells long: the VS moves the surface by it (shorter: shading only)
};

// Fills `out` for a water kind; returns the heights' total. The lake keeps the calm table.
inline Real WaterSea_Waves(Int kind, Real gridCell, WaterSeaWaveNow out[WATERSEA_WAVE_COUNT])
{
	const WaterSeaTuning &t = WaterSea_Profile(kind);
	const WaterSeaWave *const table = (kind == WATER_KIND_LAKE) ? WATERSEA_WAVE_TABLE_CALM : WATERSEA_WAVE_TABLE;
	Real totalHeight = 0.0f;
	for (Int w = 0; w < WATERSEA_WAVE_COUNT; w++)
	{
		WaterSeaWaveNow &o = out[w];
		const Real angle  = (t.swellDir + table[w].turn) * (PI / 180.0f);
		const Real length = table[w].length * t.swellSize;
		o.dx    = cosf(angle);
		o.dy    = sinf(angle);
		o.k     = 2.0f * PI / length;
		o.omega = sqrtf(WATERSEA_GRAVITY * o.k) * t.swellSpeed;
		o.a     = table[w].height * t.swellHeight;
		// `swellsharp`: 0 = circular orbits (phase 2's rule, round crests), 1 = the sideways travel that brings a crest to a
		// point (1 / (k N): never looping). Never over 4 x the height: no sliding without a wave.
		const Real loop  = 1.0f / (o.k * (Real)WATERSEA_WAVE_COUNT);
		const Real orbit = (o.a < loop) ? o.a : loop;
		o.qa    = orbit + (loop - orbit) * t.swellSharp;
		if (o.qa > 4.0f * o.a)
			o.qa = 4.0f * o.a;
		o.moves = (length >= 4.0f * gridCell) ? TRUE : FALSE;
		totalHeight += o.a;
	}
	return totalHeight;
}

// Ronin @feature 29/09/2026 DX9: vertex c0-c24, pixel c10-c55, samplers s0-s10 (the map is in WaterSea_vs/_ps.hlsl). Pixel
// constants stay at c10 and up: TAA keeps g_Reproject in c1-c4. World = identity; drawSea overwrites c4-c7 per patch.
inline void WaterSea_Bind(const WaterSeaInputs &in)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	const WaterSeaTuning &t = WaterSea_Profile(in.kind);	// Ronin @feature 02/10/2026 DX9: phase 4 - this kind's look
	const WaterSeaGlobal &g = TheWaterSeaGlobal;

	// textures: the reflection, the heights and the frame copy clamp and have one level, the rest tile; the caust frames keep
	// point mips
	IDirect3DBaseTexture9 *const tex[WATERSEA_STAGES] = { in.bump, in.reflection, in.water, in.edge, in.normals, in.heights, in.scene };
	const DWORD mip[WATERSEA_STAGES] = { D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_NONE };
	for (Int s = 0; s < WATERSEA_STAGES; s++)
	{
		const DWORD address = (s == 1 || s == 5 || s == 6) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
		DX8Wrapper::Set_DX8_Texture(s, tex[s]);
		DX8Wrapper::Set_DX8_Sampler_State(s, D3DSAMP_ADDRESSU, address);
		DX8Wrapper::Set_DX8_Sampler_State(s, D3DSAMP_ADDRESSV, address);
		DX8Wrapper::Set_DX8_Sampler_State(s, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_State(s, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_State(s, D3DSAMP_MIPFILTER, mip[s]);
	}

	// vertex: view-projection, world, caust and edge uv
	D3DXMATRIX view, proj, viewProj, world, invView;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, view);
	DX8Wrapper::_Get_DX8_Transform(D3DTS_PROJECTION, proj);
	D3DXMatrixMultiply(&viewProj, &view, &proj);
	D3DXMatrixTranspose(&viewProj, &viewProj);	// one dp4 per clip component
	D3DXMatrixIdentity(&world);
	dev->SetVertexShaderConstantF(0, (const float*)&viewProj, 4);
	dev->SetVertexShaderConstantF(4, (const float*)&world, 4);
	const float uvScale[4] = { in.bumpTilesPerUnit, in.bumpTilesPerUnit, in.edgeScaleU, (in.edgeScaleU != 0.0f) ? 1.0f : 0.0f };
	dev->SetVertexShaderConstantF(8, uvScale, 1);

	// vertex: two normal layers crossing at `cross` degrees. Offsets stay inside -1..1, so the uv keeps its precision.
	const Real tileA = 1.0f / (190.0f * t.tile);
	const Real tileB = 1.0f / (110.0f * t.tile);
	const Real sec = in.clockSeconds * t.speed;
	// Ronin @feature 01/11/2026 DX9: the sea's ripple map has its wind along u: both layers turn to `swelldir`, so the ripples'
	// crests run with the swell's; the lake and river keep 0. Layer B turns `cross` from A; a river's drift turns with them.
	const Real windA  = (in.kind == WATER_KIND_SEA) ? t.swellDir * 0.017453293f : 0.0f;
	const Real windB  = windA - t.cross * 0.017453293f;
	const Real aC = cosf(windA), aS = sinf(windA), bC = cosf(windB), bS = sinf(windB);
	float normalA[4]    = { aC * tileA, aS * tileA, -aS * tileA, aC * tileA };
	float normalBRot[4] = { bC * tileB, bS * tileB, -bS * tileB, bC * tileB };
	float normalBOff[4] = { fmodf(sec * -0.006f, 1.0f), fmodf(sec * 0.014f, 1.0f), fmodf(sec * 0.011f, 1.0f), fmodf(sec * 0.004f, 1.0f) };
	// Ronin @feature 02/10/2026 DX9: phase 4 - a river's layers stand still in world space: its flow map moves them
	// (c22 here, c46 below; the drift per vertex comes from drawRiverWater). `speed` is its current, world units a second.
	if (in.kind == WATER_KIND_RIVER)
	{
		normalBOff[2] = 0.0f;	normalBOff[3] = 0.0f;
		normalBOff[0] = 0.37f;	normalBOff[1] = 0.61f;
	}
	// Ronin @bugfix 03/10/2026 DX9: z - the swell grid's real cell: the VS fades the swell by the lowest ground that far around
	const float flowVS[4] = { t.speed, WATERSEA_FLOW_CYCLE, (in.swellCell > 0.0f) ? in.swellCell : t.swellCell, 0.0f };
	dev->SetVertexShaderConstantF(9, normalA, 1);
	dev->SetVertexShaderConstantF(10, normalBRot, 1);
	dev->SetVertexShaderConstantF(11, normalBOff, 1);
	dev->SetVertexShaderConstantF(22, flowVS, 1);

	// pixel: camera, and the sun - terrain light 0, whose position holds the direction the light travels
	D3DXMatrixInverse(&invView, nullptr, &view);	// the camera sits at the inverse view's translation
	const Coord3D &lightDir = TheGlobalData->m_terrainLightPos[0];
	Real sx = -lightDir.x, sy = -lightDir.y, sz = -lightDir.z;
	const Real len = sqrtf(sx * sx + sy * sy + sz * sz);
	if (len > 0.0f) { sx /= len; sy /= len; sz /= len; }
	const RGBColor &sun = TheGlobalData->m_terrainDiffuse[0];

	const float bumpEnvMat[4] = { in.bumpScale, 0.0f, 0.0f, in.bumpScale };
	// ndc -> reflection uv. Ronin @bugfix 30/09/2026 DX9: minus the TAA jitter - the mirror is drawn unjittered. A jitter j
	// in the projection moves ndc by -j (R3DTaa::getJitterNDC), so uv gains (+0.5 jx, -0.5 jy).
	const float texProj[4]    = { 0.5f, -0.5f, 0.5f + 0.5f * in.jitterX, 0.5f - 0.5f * in.jitterY };
	// Ronin @bugfix 01/10/2026 DX9: `water mirror 0` = reflections off - no share, not the last mirror image frozen in place.
	const float reflect[4]    = { g.mirror ? t.reflect : 0.0f, t.fresnelRipple, 0.0f, 0.0f };
	const float eye[4]        = { invView._41, invView._42, invView._43, 1.0f };
	const float sunDir[4]     = { sx, sy, sz, t.gloss };
	const float sunSharp[4]   = { sun.red * t.spec, sun.green * t.spec, sun.blue * t.spec, 0.0f };
	const float surface[4]    = { t.normals ? t.ripple : 0.0f, t.fresnel ? 1.0f : 0.0f, t.distort, t.f0 };	// Ronin @feature 02/10/2026 DX9: x = `ripple`
	const float look[4]       = { t.refract, t.shade, t.fresnelPow, (float)g.view };
	const float sheen[4]      = { sun.red * t.sheen, sun.green * t.sheen, sun.blue * t.sheen, t.sheenPow };
	dev->SetPixelShaderConstantF(10, bumpEnvMat, 1);
	dev->SetPixelShaderConstantF(11, texProj, 1);
	dev->SetPixelShaderConstantF(12, reflect, 1);
	dev->SetPixelShaderConstantF(13, eye, 1);
	dev->SetPixelShaderConstantF(14, sunDir, 1);
	dev->SetPixelShaderConstantF(15, sunSharp, 1);
	dev->SetPixelShaderConstantF(16, surface, 1);
	dev->SetPixelShaderConstantF(18, look, 1);
	dev->SetPixelShaderConstantF(19, sheen, 1);

	// Ronin @feature 29/09/2026 DX9: phase 1 - depth from the terrain heights. The sea's opacity is `opacity` (capped at the
	// map's MinOpacity), faded in over `shore` units from the waterline; a lake's is the map's own (c33.zw below).
	const Bool depthOn = (g.depth && in.heights != nullptr) ? TRUE : FALSE;
	const Real minOpacity = TheWaterTransparency->m_minWaterOpacity;
	const Bool soft = (TheGlobalData->m_showSoftWaterEdge && TheWaterTransparency->m_transparentWaterDepth > 0.0f && t.shore > 0.0f) ? TRUE : FALSE;
	const Real opacity = (t.opacity < minOpacity) ? t.opacity : minOpacity;
	const float heightUV[4] = { in.heightScaleU, in.heightScaleV, in.heightOffsetU, in.heightOffsetV };
	const float heightCfg[4] = { depthOn ? 1.0f : 0.0f, in.heightUnit, 1.0f / t.foamTile, in.clockSeconds };
	const float depthCfg[4]  = { 1.0f / t.clearDepth, t.deep, opacity, soft ? 1.0f / t.shore : 0.0f };
	const float foamCfg[4]   = { t.foam, 1.0f / t.foamWidth, t.texture, (in.edgeScaleU != 0.0f) ? 1.0f : 0.0f };
	// Ronin @bugfix 29/09/2026 DX9: one height texel in uv, and 1 / world units per texel - the terrain slope, which turns the
	// depth into a distance from the waterline; w: the depth shore effects stop at.
	const float heightStep[4] = { in.heightScaleU * MAP_XY_FACTOR, in.heightScaleV * MAP_XY_FACTOR, 1.0f / MAP_XY_FACTOR, t.shoreDepth };
	dev->SetPixelShaderConstantF(20, heightUV, 1);
	dev->SetPixelShaderConstantF(21, heightCfg, 1);
	dev->SetPixelShaderConstantF(22, depthCfg, 1);
	dev->SetPixelShaderConstantF(23, foamCfg, 1);
	dev->SetPixelShaderConstantF(24, heightStep, 1);
	// Ronin @feature 29/09/2026 DX9: the open-water lace (phase 1's first foam).
	const float seaFoam[4] = { t.seaFoam, 1.0f / t.seaFoamDepth, 1.0f / t.seaFoamTile, t.ownOpacity ? 0.0f : 1.0f };
	dev->SetPixelShaderConstantF(34, seaFoam, 1);
	// Ronin @feature 29/09/2026 DX9: phase 3 - refraction: the frame copy's texel size, the bend, on.
	const float refraction[4] = { in.sceneInvW, in.sceneInvH, t.bend, (in.scene != nullptr) ? 1.0f : 0.0f };
	dev->SetPixelShaderConstantF(35, refraction, 1);
	// Ronin @feature 08/10/2026 DX9: `mirrorsoft` - c62: the extra taps' offset in mirror uv, z: on. Above the wake's c56-c61.
	const float mirrorSoft[4] = { g.mirrorSoft * in.mirrorInvW, g.mirrorSoft * in.mirrorInvH, (g.mirrorSoft > 0.0f) ? 1.0f : 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(62, mirrorSoft, 1);

	// Ronin @feature 29/09/2026 DX9: shadows on the water - the terrain's shadow map, as W3DTreeBuffer binds it: off in the depth pass (the
	// map is the render target there). s7 through the wrapper (it caches stages 0-7), s8 raw. LINEAR = hardware PCF.
	const TerrainShadowPassState &sp = TheTerrainShadowPass;
	const Bool shadowOn = (sp.shadowTex != nullptr && !sp.inDepthPass && t.shadow > 0.0f) ? TRUE : FALSE;
	const float shadowCfg[4]  = { shadowOn ? 1.0f : 0.0f, sp.depthBias, sp.texelOffset, t.shadow };
	const float cascadeCfg[4] = { sp.depthBiasFar, sp.cascadeCount, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(44, shadowCfg, 1);
	dev->SetPixelShaderConstantF(45, cascadeCfg, 1);
	// Ronin @feature 02/10/2026 DX9: phase 4 - c46: the river's flow cycle, and layer A's uv -> the bank foam's (`foamtile`) and the
	// current foam's (`seafoamtile`).
	const float riverFlow[4] = { 1.0f / WATERSEA_FLOW_CYCLE, (190.0f * t.tile) / t.foamTile, (190.0f * t.tile) / t.seaFoamTile, 0.0f };
	dev->SetPixelShaderConstantF(46, riverFlow, 1);
	if (shadowOn)
	{
		dev->SetPixelShaderConstantF(36, sp.lightViewProjT, 4);
		dev->SetPixelShaderConstantF(40, sp.lightViewProjFarT, 4);
	}
	IDirect3DBaseTexture9 *const shadowNear = shadowOn ? sp.shadowTex : nullptr;
	IDirect3DBaseTexture9 *const shadowFar  = shadowOn ? ((sp.shadowTexFar != nullptr) ? sp.shadowTexFar : sp.shadowTex) : nullptr;
	DX8Wrapper::Set_DX8_Texture(7, shadowNear);
	DX8Wrapper::Set_DX8_Sampler_State(7, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Sampler_State(7, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	DX8Wrapper::Set_DX8_Sampler_State(7, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_State(7, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_State(7, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	dev->SetTexture(8, shadowFar);
	dev->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(8, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(8, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	// Ronin @feature 29/09/2026 DX9: phase 2 - Gerstner swell, vertex c12-c21, pixel c25-c33. The VS moves the grid by the
	// waves at least 4 cells long, ramped in over `swellDepth` from the waterline; the PS shades with all of them.
	const Bool swellOn = (in.swell && t.swell && in.heightsVTF && in.heights != nullptr) ? TRUE : FALSE;
	// Ronin @feature 25/10/2026 DX9: wakes raise the water with the swell off too: the VS's branch runs for either (`meshOn`),
	// the swell's waves at 0 height when it is off. PS c54 / c55 and s10; VS c23 / c24 and its sampler 1.
	const Bool wakeOn   = (in.wake != nullptr && g.wakes > 0.0f) ? TRUE : FALSE;
	const Bool wakeMesh = (wakeOn && in.wakeVTF && in.swell && in.heightsVTF && in.heights != nullptr) ? TRUE : FALSE;
	const Bool meshOn   = (swellOn || wakeMesh) ? TRUE : FALSE;
	float vsWave[2 * WATERSEA_WAVE_COUNT][4];
	float psWave[2 * WATERSEA_WAVE_COUNT][4];
	// Ronin @bugfix 03/10/2026 DX9: the grid's real cell decides which waves it can carry - it is coarser than `swellcell`
	// when the view outgrows the vertex budget (drawTrapezoidWater).
	const Real gridCell = (in.swellCell > 0.0f) ? in.swellCell : t.swellCell;
	// Ronin @feature 03/10/2026 DX9: phase 5 - the waves come from WaterSea_Waves, as the CPU's copy of the surface. The PS
	// reads a wave's height in units of the default sea's total (at most 1.5), for the crest tint and the whitecaps.
	WaterSeaWaveNow wave[WATERSEA_WAVE_COUNT];
	const Real totalHeight = WaterSea_Waves(in.kind, gridCell, wave);
	const Real shareUnit = (totalHeight > 1.5f * WATERSEA_SWELL_REF) ? 1.5f / totalHeight : 1.0f / WATERSEA_SWELL_REF;
	float capWave[WATERSEA_WAVE_COUNT][4];
	for (Int w = 0; w < WATERSEA_WAVE_COUNT; w++)
	{
		const Real dx     = wave[w].dx;
		const Real dy     = wave[w].dy;
		const Real k      = wave[w].k;
		const Real omega  = wave[w].omega;
		const Real a      = wave[w].a;
		const Real qa     = wave[w].qa;
		const Real aVS    = (swellOn && wave[w].moves) ? a : 0.0f;
		const Real qaVS   = (swellOn && wave[w].moves) ? qa : 0.0f;
		const Real share  = a * shareUnit;	// the PS sums these x sin: the height, for the crest tint and the whitecaps
		// the whitecap trails: the same sum a moment earlier, sin(th + phase) = sin cos(phase) + cos sin(phase). The phase is by
		// the wave's own speed, not `swellspeed`: a trail's length stays.
		for (Int lag = 0; lag < 2; lag++)
		{
			const Real phase = sqrtf(WATERSEA_GRAVITY * k) * WATERSEA_CAP_LAG[lag];
			capWave[w][2 * lag]     = share * cosf(phase);
			capWave[w][2 * lag + 1] = share * sinf(phase);
		}
		const float vs0[4] = { k * dx, k * dy, omega, aVS };
		const float vs1[4] = { qaVS * dx, qaVS * dy, 0.0f, 0.0f };
		const float ps0[4] = { k * dx, k * dy, omega, share };
		const float ps1[4] = { dx * k * a * WATERSEA_SWELL_NORMAL, dy * k * a * WATERSEA_SWELL_NORMAL, k * qa, 0.0f };
		for (Int c = 0; c < 4; c++)
		{
			vsWave[2 * w][c] = vs0[c];  vsWave[2 * w + 1][c] = vs1[c];
			psWave[2 * w][c] = ps0[c];  psWave[2 * w + 1][c] = ps1[c];
		}
	}
	const float swellVS[4]  = { in.clockSeconds, meshOn ? 1.0f : 0.0f, (t.swellDepth > 0.0f) ? 1.0f / t.swellDepth : 0.0f, in.heightUnit };
	// Ronin @bugfix 01/10/2026 DX9: zw - a lake's own soft shore, vanilla's slope 1 / (TransparentWaterDepth x MinOpacity) and
	// its cap MinOpacity (BaseHeightMap updateShorelineTile, initDestAlphaLUT); slope 0 = soft edge off, the source alpha.
	const Real lakeSpan  = TheWaterTransparency->m_transparentWaterDepth * minOpacity;
	const Real lakeSlope = (TheGlobalData->m_showSoftWaterEdge && lakeSpan > 0.0f) ? 1.0f / lakeSpan : 0.0f;
	const float swellPS[4]  = { in.clockSeconds, swellOn ? 1.0f : 0.0f, lakeSlope, minOpacity };
	dev->SetVertexShaderConstantF(12, &vsWave[0][0], 2 * WATERSEA_WAVE_COUNT);
	dev->SetVertexShaderConstantF(20, swellVS, 1);
	dev->SetVertexShaderConstantF(21, heightUV, 1);
	dev->SetPixelShaderConstantF(25, &psWave[0][0], 2 * WATERSEA_WAVE_COUNT);
	dev->SetPixelShaderConstantF(33, swellPS, 1);
	// Ronin @feature 03/10/2026 DX9: phase 5 - whitecaps: c47-c50 the trails per wave, c51 the foam's threshold and edge,
	// c52 the trail weights and `swelltint`.
	const float capCfg[4]   = { t.whitecapAt, 1.0f / WATERSEA_CAP_SOFT, t.whitecaps, WATERSEA_CAP_NOISE };
	// Ronin @feature 03/10/2026 DX9: phase 5 - w: shore breakers, the shore foam on the swell's time (needs the swell on)
	const float capTrail[4] = { WATERSEA_CAP_TRAIL[0], WATERSEA_CAP_TRAIL[1], t.swellTint, (t.breakers && swellOn) ? 1.0f : 0.0f };
	dev->SetPixelShaderConstantF(47, &capWave[0][0], WATERSEA_WAVE_COUNT);
	dev->SetPixelShaderConstantF(51, capCfg, 1);
	dev->SetPixelShaderConstantF(52, capTrail, 1);
	// Ronin @bugfix 03/10/2026 DX9: the shroud in the PS. The second pass that applied it drew the grid flat, in fixed
	// function: a sheet at the rest level over the troughs, and crests left unshrouded. s9 raw, as s8.
	const float shroudMap[4] = { in.shroudScaleX, in.shroudScaleY, in.shroudOffsetX, in.shroudOffsetY };
	dev->SetPixelShaderConstantF(53, shroudMap, 1);
	dev->SetTexture(9, in.shroud);
	dev->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(9, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(9, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	// Ronin @feature 03/10/2026 DX9: phase 5 - wakes (`wakeOn`, `wakeMesh` above). No texture: every strength 0, so whatever
	// an unbound sampler returns is not used. `wakeheight` is in the texture (renderWakes), height and slope together.
	const float wakeMap[4]  = { in.wakeScale, in.wakeScale, in.wakeOffsetU, in.wakeOffsetV };
	const float wakePS[4]   = { wakeOn ? g.wakes * g.wakeFoam : 0.0f, wakeOn ? g.wakes * g.wakeShade : 0.0f,
	                            wakeOn ? g.wakes / WATERSEA_SWELL_REF : 0.0f, wakeOn ? g.wakeTint : 0.0f };	// w: `waketint` (25/10/2026)
	const float wakeVS[4]   = { wakeMesh ? g.wakes : 0.0f, 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(54, wakeMap, 1);
	dev->SetPixelShaderConstantF(55, wakePS, 1);
	dev->SetVertexShaderConstantF(23, wakeMap, 1);
	dev->SetVertexShaderConstantF(24, wakeVS, 1);
	dev->SetTexture(10, wakeOn ? in.wake : nullptr);
	dev->SetSamplerState(10, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(10, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(10, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(10, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(10, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	// the vertex shader's heights: vs_3_0 s0 is D3DVERTEXTEXTURESAMPLER0, outside the wrapper's cached stages. Point sampled -
	// the one filter every vertex-texture format takes.
	dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, meshOn ? in.heights : nullptr);	// Ronin @feature 25/10/2026 DX9: swell or wakes
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	dev->SetTexture(D3DVERTEXTEXTURESAMPLER1, wakeMesh ? in.wake : nullptr);	// Ronin @feature 03/10/2026 DX9: phase 5 - wakes
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(D3DVERTEXTEXTURESAMPLER1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
}

// Ronin @feature 29/09/2026 DX9: phase 2 - the flat water's grid densifies to `swellCell` while this is TRUE.
// Ronin @feature 02/10/2026 DX9: phase 4 - per kind: a calm kind keeps the coarse grid.
inline Bool WaterSea_SwellActive(Int kind, Bool heightsVTF)
{
	return (WaterSea_Profile(kind).swell && heightsVTF) ? TRUE : FALSE;
}

// Ronin @feature 25/10/2026 DX9: the dense grid - for the swell, or while a wake is live (wakes raise the water without the
// swell). `wakeLive`: a wake texture this frame that the VS can read.
inline Bool WaterSea_MeshActive(Int kind, Bool heightsVTF, Bool wakeLive)
{
	return (heightsVTF && (WaterSea_Profile(kind).swell || (wakeLive && TheWaterSeaGlobal.wakes > 0.0f))) ? TRUE : FALSE;
}

// Ronin @feature 29/09/2026 DX9: the flat water's soft shore comes from the shader's depth, not destination alpha, when
// this is TRUE (W3DWater drawTrapezoidWater skips its DESTALPHA blend).
inline Bool WaterSea_DepthActive(Bool haveHeights)
{
	return (TheWaterSeaGlobal.depth && haveHeights) ? TRUE : FALSE;
}

// Ronin @feature 29/09/2026 DX9: TRUE = the shore foam alone, no vanilla shore-wave sprites (`water oldwaves 0`; both by default).
// Ronin @feature 02/10/2026 DX9: phase 4 - when the sea or the lake profile has shore foam.
inline Bool WaterSea_ShoreFoamActive(Bool haveHeights)
{
	const Bool anyFoam = (WaterSea_Profile(WATER_KIND_SEA).foam > 0.0f || WaterSea_Profile(WATER_KIND_LAKE).foam > 0.0f) ? TRUE : FALSE;
	return (WaterSea_DepthActive(haveHeights) && anyFoam && !TheWaterSeaGlobal.oldWaves) ? TRUE : FALSE;
}

// Ronin @feature 29/09/2026 DX9: drop the WaterSea texture references once the draw is done.
inline void WaterSea_Unbind()
{
	for (Int s = 0; s < WATERSEA_STAGES; s++)
		DX8Wrapper::Set_DX8_Texture(s, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);	// Ronin @feature 29/09/2026 DX9: phase 2
	DX8Wrapper::Set_DX8_Texture(7, nullptr);										// Ronin @feature 29/09/2026 DX9: shadows on the water
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(8, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(9, nullptr);	// Ronin @bugfix 03/10/2026 DX9: the shroud
	// Ronin @feature 03/10/2026 DX9: phase 5 - the wakes: next frame it is a render target again
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(10, nullptr);
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(D3DVERTEXTEXTURESAMPLER1, nullptr);
}
