/*
**	Command & Conquer Generals Zero Hour(tm)
**	DX9 local lights, drawn in screen space.
*/

// Ronin @feature 09/10/2026 DX9: local lights. Each enabled W3DDynamicLight is one quad over the screen rectangle its
// sphere covers; LocalLight.pso rebuilds position and facing from the scene depth and adds the light to the frame.

#include "WW3D2/dx8todx9.h"
#include "WWLib/always.h"
#include "Lib/BaseType.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/camera.h"
#include "WW3D2/texture.h"
#include "WW3D2/formconv.h"
#include "WW3D2/robjlist.h"
#include "WW3D2/mesh.h"			// Ronin @feature 09/10/2026 DX9: the beams: which meshes only look like lights
#include "WW3D2/meshmdl.h"
#include "WW3D2/hlod.h"				// Ronin @bugfix 11/10/2026 DX9: a lamp the animation has sunk out of sight (lampSunk)
#include "WW3D2/hanim.h"
#include "WW3D2/shader.h"
#include "WW3D2/dx8instancing.h"	// Ronin @feature 10/10/2026 DX9: the hooks round the mesh shader's draws
#include "W3DDevice/GameClient/R3DShadowMapState.h"
#include "Common/GlobalData.h"
#include "GameLogic/TerrainLogic.h"	// Ronin @feature 11/10/2026 DX9: how high a point light is over the ground
#include "W3DDevice/GameClient/R3DLocalLights.h"
#include "W3DDevice/GameClient/R3DSkyCube.h"	// Ronin @feature 12/10/2026 DX9: the sky cube: kept current each frame, shown by view 5
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "GameClient/ParticleSys.h"			// Ronin @feature 15/10/2026 DX9: item 7, glowing effects as lights
#include "GameLogic/GameLogic.h"			// Ronin @bugfix 17/10/2026 DX9: a projectile's exhaust is lit at the projectile
#include "GameLogic/Object.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/R3DSsao.h"
#include "W3DDevice/GameClient/R3DTaa.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"	// Ronin @feature 22/10/2026 DX9: the sun shafts: the ground's cloud drift
#include "WW3D2/assetmgr.h"
#include <d3dx9math.h>				// Ronin @feature 11/10/2026 DX9: the lamp shadow maps' matrices, as R3DTaa.cpp
#include "Common/FileSystem.h"		// Ronin @feature 12/10/2026 DX9: DX9Lights.ini
#include "Common/File.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Ronin @feature 09/10/2026 DX9: more lights than this in one frame are counted (live) and left undrawn.
static const Int MAX_LIGHTS_DRAWN = 512;		// Ronin @feature 25/10/2026 DX9: 256 -> 512, a joined string's bulbs draw a corona each

static IDirect3DVertexShader9	*s_quadVS        = NULL;
static IDirect3DPixelShader9	*s_lightPS       = NULL;
static Bool						s_shadersTried  = FALSE;
// Ronin @feature 11/10/2026 DX9: the lamp shadow maps' depth draw - the pair TAA's shadow mask ships: a mesh in, its depth out as
// two bytes of colour.
static IDirect3DVertexShader9	*s_lampVS        = NULL;
static IDirect3DPixelShader9	*s_lampPS        = NULL;

static Bool						s_enabled       = TRUE;		// Ronin @feature 23/10/2026 DX9: the master (Options.ini DX9Lighting)
static Bool						s_effects       = TRUE;		// ... the lamps under it (DX9LightEffects); off = the game's own lights
static float					s_intensity     = 1.0f;
static float					s_over          = 1.5f;
static float					s_boost         = 0.0f;
static Int						s_view          = 0;
static float					s_range         = 1.0f;
static Bool						s_small         = TRUE;

static Bool						s_active        = FALSE;	// decided in beginFrame, read until the next one
static Bool						s_ownsDepth     = FALSE;
static Bool						s_passDone      = FALSE;	// the first main-pass flush of the frame is the one

// Ronin @feature 09/10/2026 DX9: the frame before any light. A TextureClass render target, so the texture manager
// releases and recreates it across a device reset - as the SSAO pair.
static TextureClass				*s_copy         = NULL;
static UnsignedInt				s_copyW         = 0;
static UnsignedInt				s_copyH         = 0;
static D3DFORMAT				s_copyFormat    = D3DFMT_UNKNOWN;

static R3DLocalLights::Stats	s_stats;	// this frame, being counted
static R3DLocalLights::Stats	s_shown;	// last frame, complete

// Ronin @diagnostic 20/10/2026 DX9: GPU TIME of the 3D scene up to the light pass, the sun's-shade pass and the light pass, from
// timestamps read LIGHT_GPU_RING frames later so nothing waits (R3DTaa's pattern). ms, eased; -1 = not measured. [LIGHT2] row.
enum { LIGHT_GPU_RING = 4, LIGHT_GPU_MARKS = 5 };	// 0 beginFrame, 1 light pass start, 2 shade pass start, 3 its end, 4 light pass end
struct LightGpuFrame { IDirect3DQuery9 *disjoint; IDirect3DQuery9 *freq; IDirect3DQuery9 *t[LIGHT_GPU_MARKS]; Bool marked[LIGHT_GPU_MARKS];
					   Bool issued; };
static LightGpuFrame			s_gpu[LIGHT_GPU_RING];
static Int						s_gpuSlot       = 0;
static Bool						s_gpuOK         = FALSE;
static Bool						s_gpuTried      = FALSE;	// made once; again after releaseQueries (a device reset)
static Bool						s_gpuOpen       = FALSE;	// between beginFrame and endFrame
static float					s_gpuMs[3]      = { -1.0f, -1.0f, -1.0f };	// scene, shade, lights

struct LightDraw
{
	float c[40];			// c5..c10: the light now (position, diffuse, ambient) and as it was last frame; c11, c12: the two axes; c13: shafts
	float x0, y0, x1, y1;	// target pixels
	Int   map;				// its shadow map's tile (s_lampMaps), or -1
	Int   mapWas;			// ... and last frame's (s_lampMapsPrev), or -1
	Int   sides;			// a blast's four level maps: the first one's tile, or -1
};

// Ronin @bugfix 09/10/2026 DX9: each drawn light as it was, for TAA's change mask (LocalLight_ps COLOR1): the resolve clamps where a
// light changed a pixel since last frame. Without it lit ground lagged and every mover's shadow footprint showed.
struct LightMemo
{
	const void *key;		// the W3DDynamicLight, or a beam's mesh
	Int         sub;		// -1 = a W3DDynamicLight, else the beam's index in its mesh
	Vector3     world;
	float       inner, outer;
	float       diffuse[3], ambient[3];	// times intensity
	Vector3     axis;		// a spot: away from the lamp
	float       cosOuter, edge;	// a point light: -2 and 1, so the cone term is 1
	float       shaft;		// the beam in the air, its strength; 0 for a point light
	float       lampRadius = 0.0f;	// ... and how wide it leaves its lamp (c13.w)
	float       coronaRadius = 0.0f, coronaStrength = 0.0f;	// a lamp's own glow, drawn by the shader (c14); 0 = none
	float       peakLum = 0.0f, peakOuter = 0.0f;	// an engine pulse: its brightest and its widest so far (its smooth end)
	Vector3     boundCentre;	// the sphere that holds everything it reaches
	float       boundRadius;
	Bool        seen;		// a light of this frame matched it
	Int         map = -1;	// its shadow map's tile that frame, or -1
	Vector3     mapLamp, mapAxis;	// ... and the pose that map was drawn from, kept while the lamp only sways
	Bool        mapHeld = FALSE;
};
static LightMemo				s_memo[MAX_LIGHTS_DRAWN];
static Int						s_memoCount     = 0;

struct ViewMap
{
	float zNear, zFar;
	float planeMinX, planeMaxY, planeW, planeH;	// the view plane at depth 1
	float vx0, vy0, vx1, vy1;					// the 3D viewport in target pixels
};

// Ronin @feature 09/10/2026 DX9: same reader as R3DSsao.cpp's, which is file-static there. Caller frees with HeapFree.
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
	blob = readShaderBlob("shaders\\LocalLight.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_lightPS)))
			s_lightPS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaVelMesh.vso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreateVertexShader(blob, &s_lampVS)))
			s_lampVS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
	blob = readShaderBlob("shaders\\TaaMoverLight.pso");
	if (blob != NULL)
	{
		if (FAILED(dev->CreatePixelShader(blob, &s_lampPS)))
			s_lampPS = NULL;
		HeapFree(GetProcessHeap(), 0, blob);
	}
}

// Ronin @feature 09/10/2026 DX9: at the target's own size and format, so StretchRect neither scales nor converts (it resolves MSAA).
static Bool ensureCopy(UnsignedInt width, UnsignedInt height, D3DFORMAT format)
{
	if (s_copy != NULL && s_copyW == width && s_copyH == height && s_copyFormat == format)
		return TRUE;

	REF_PTR_RELEASE(s_copy);
	s_copyW = 0;
	s_copyH = 0;
	s_copyFormat = D3DFMT_UNKNOWN;

	WW3DFormat wwFormat = D3DFormat_To_WW3DFormat(format);
	if (wwFormat == WW3D_FORMAT_UNKNOWN)
		wwFormat = WW3D_FORMAT_A8R8G8B8;
	s_copy = NEW_REF(TextureClass, (width, height, wwFormat, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
	if (s_copy->Peek_D3D_Base_Texture() == NULL)
	{
		REF_PTR_RELEASE(s_copy);	// a macro with its own if - keep the braces
		return FALSE;
	}
	s_copyW = width;
	s_copyH = height;
	s_copyFormat = format;
	return TRUE;
}

// Ronin @feature 09/10/2026 DX9: 1 / the map's own light on open ground, per channel, so frame x this ~ the surface's colour.
// R3DTaa works it out (its selection flash is added to the same surface); `lights boost` overrides it here.
static void sceneLightFactor(float out[3])
{
	if (s_boost > 0.0f)
	{
		out[0] = out[1] = out[2] = s_boost;
		return;
	}
	R3DTaa::getMapLightFactor(out);
}

// Ronin @feature 09/10/2026 DX9: LIGHTING PART 2, THE BEAMS (docs/Lighting_Work.md 4.2). A cone of light in the art is loose
// quads on the `lightbeam` texture. Each beam becomes a spot light; its quads are no longer drawn.
static const Int   MAX_FIXTURE_MODELS = 1024;		// a power of two; the whole game ships 733 beam meshes
static const Int   MAX_FIXTURE_BEAMS  = 2048;		// ... holding 1210 beams
static const Int   MAX_BEAMS_A_MESH   = 16;
static const Int   MAX_FIXTURE_VERTS  = 1024;
static const Int   MAX_FIXTURE_QUADS  = 256;
static const Int   MAX_FRAME_BEAMS    = 192;
static const Int   MAX_FIXTURE_GLOWS  = 4096;
static const Int   MAX_GLOWS_A_MESH   = 64;		// Ronin @feature 25/10/2026 DX9: 24 -> 64 - the palace has 48 lamps, 40 of them bulbs
static const Int   MAX_FRAME_GLOWS    = 384;
static const Int   MAX_FIXTURE_POOLS  = 2048;
static const Int   MAX_POOLS_A_MESH   = 40;
static const Int   MAX_FRAME_POOLS    = 128;
static const float BEAM_REACH         = 1.3f;		// the beam in the air ends this far along, in the art cone's length
static const float BEAM_INNER         = 0.1f;		// the light on surfaces is at full strength to this share of its reach
static const float BEAM_LAMP_USUAL    = 0.83f;	// a beam's radius at its lamp: the median of the 5,738 shipped beam quads
static const float BEAM_LAMP_MOST     = 0.25f;	// ... as a share of its far end's: above this the art is a sheet, not a cone
// Ronin @feature 09/10/2026 DX9: a beam the art draws level (a headlight, 1 degree UP on the Humvee) never reaches the ground: its pool
// was 7% of a lamp's (run 1; scratchpad beam_ground_sim.py). It is aimed at the ground this share of its length ahead.
static const float BEAM_AIM_AHEAD     = 0.8f;
static const float BEAM_AIM_MAX_UP    = 0.0872f;	// sin 5 deg: a beam that points up more than this is meant to
static const float BEAM_AIM_MAX_DOWN  = 0.8660f;	// sin 60 deg: one this steep already lights what is under it
static const float BEAM_AIM_MAX_TILT  = 0.4363f;	// 25 deg, in radians: never tipped further than this
static const float BEAM_COLOUR[3]     = { 1.0f, 1.0f, 0.9f };	// the texture's own tint (255,255,230)

struct FixtureBeam
{
	Vector3 lamp, end;		// in the mesh's own space
	float   endRadius;
	float   lampRadius;		// Ronin @bugfix 12/10/2026 DX9: the art's beam at its lamp: no shipped beam starts from a point
};
struct FixtureModel
{
	const MeshModelClass *key;
	Int     verts, polys;
	Vector3 first;			// its first vertex: a freed model's address can come back as another beam of the same size
	Int     firstBeam, beamCount;
	Int     firstGlow, glowCount;
	Int     firstPool, poolCount;	// the painted pools: pieces on a glow texture too large to be a lamp
	Bool    glowOnly;		// every polygon is a lamp's glow: with coronas on the mesh need not be drawn
	Bool    glowUp;			// its glow polygons face up its own Z: the art can blink it by sinking it (lampSunk)
	Bool    pure;			// every polygon is beam: the mesh need not be drawn at all
};
static FixtureModel	s_fixModels[MAX_FIXTURE_MODELS];
static FixtureBeam	s_fixBeams[MAX_FIXTURE_BEAMS];
static Int			s_fixBeamCount  = 0;
static Int			s_fixModelCount = 0;
struct FixtureGlow
{
	Vector3 centre;			// in the mesh's own space
	float   radius;			// of the art's glow
	Int     kind;			// GLOW_TABLE
	float   spread;			// Ronin @feature 25/10/2026 DX9: > 0 = a joined string's light, this far round its centre; no corona
	float   power;			// ... its light, in lamps (1 = a lone lamp); 0 = a bulb of a joined string: its corona only
};
static FixtureGlow	s_fixGlows[MAX_FIXTURE_GLOWS];
static Int			s_fixGlowCount  = 0;
static FixtureGlow	s_fixPools[MAX_FIXTURE_POOLS];	// radius = half the piece's diagonal
static Int			s_fixPoolCount  = 0;
struct FramePool
{
	const void *mesh;
	Int         index;
	Vector3     pos;		// world
	float       radius;
	Int         kind;
};
static FramePool	s_framePools[MAX_FRAME_POOLS];
static Int			s_framePoolCount = 0;
struct FrameGlow
{
	const void *mesh;
	Int         index;
	Vector3     pos;		// world
	float       scale;		// its object's
	float       radius;		// of the art's glow, world
	Int         kind;
	float       spread;		// Ronin @feature 25/10/2026 DX9: FixtureGlow's, world
	float       power;
};
static FrameGlow	s_frameGlows[MAX_FRAME_GLOWS];
static Int			s_frameGlowCount = 0;

struct FrameBeam
{
	const void *mesh;
	Int         index;
	Vector3     lamp, end;	// world
	float       endRadius;
	float       lampRadius;
	Vector3     up;			// its object's up, and the lamp's height over the object's own origin: the ground under it
	float       height;
};
static FrameBeam	s_frameBeams[MAX_FRAME_BEAMS];
static Int			s_frameBeamCount = 0;
struct SeenMesh { const MeshClass *mesh; UnsignedInt frame; };
static SeenMesh		s_seenMesh[512];
static UnsignedInt	s_fixFrame  = 1;
static Bool			s_collect   = FALSE;	// between beginFrame and the pass: a rendered beam mesh adds its beams
static float		s_beams     = 0.9f;		// `lights beams`: the light on surfaces (1.5 until 17/10: pools far too strong)
static const float	BEAM_CONE_REF = 1.5f;	// the `beams` the cone in the air was tuned at: it keeps that look at any `beams`
static float		s_spill     = 0.5f;		// `lights spill`: the dim light around a beam's cone, beside the cone's own
static float		s_spillWide = 2.2f;		// `lights spillwide`: how wide it reaches, in cone widths
static float		s_shafts    = 0.4f;		// `lights shafts`: the beam in the air, beside the light's own strength
static float		s_rays      = 0.5f;		// `lights rays`: soft rays from the lamp in that beam (LocalLight_ps shaft); 0 = the smooth cone
static float		s_lampWidth = 1.0f;		// `lights lampwidth`: how wide a beam leaves its lamp, in the art's own width there; 0 = from a point
static Bool			s_rayTurn   = FALSE;	// `lights rayturn`: the rays turn with the world as the camera goes round a beam; 0 = they stay with the view
static Bool			s_beamsOn   = FALSE;	// this frame the beams are lights (decided in beginFrame)
static float		s_glows     = 0.5f;		// `lights glows`: the lamp glows as point lights, their strength; 0 = none
static float		s_glowReach = 0.8f;		// `lights glowreach`: multiplies GLOW_TABLE's reach
static float		s_glowJoin  = 2.5f;		// Ronin @feature 25/10/2026 DX9: `lights glowjoin`: lamps this near each other light as one
static Bool			s_fixReread = FALSE;	// ... it changed: every model's lamps are read again
static Bool			s_glowsOn   = FALSE;
static float		s_coronas   = 1.5f;		// `lights coronas`: the lamp's own glow drawn by the shader; 0 = the art's glow textures
static float		s_coronaSize = 1.0f;	// `lights coronasize`: its radius, in the art glow's own
static Bool			s_coronasOn = FALSE;
static float		s_beamReach = 1.8f;		// `lights beamreach`: a beam's light on surfaces ends this far along, in the art cone's length
static float		s_hotspot   = 0.5f;		// `lights hotspot`: the share of a beam's cone at full strength; the rest fades to its edge
static float		s_poolFill  = 0.6f;		// `lights poolfill`: the light that stands in for a painted pool no lamp covers; 0 = none
static Bool			s_poolsOn   = FALSE;
static Bool			s_keepPools = FALSE;	// `lights pools 1`: the glow textures stay drawn (lamp glows welded into meshes, painted pools)
static Bool			s_glowRejected = FALSE;	// readGlows: a piece of the last model read was no lamp

// Ronin @feature 09/10/2026 DX9: the beam textures met so far. While the beams are lights each one's surface is swapped for a
// clear texel, which hides the beam polygons welded into other meshes (35 night models, the War Factory among them).
struct BeamTexture
{
	TextureClass          *tex;		// a reference is held
	IDirect3DBaseTexture9 *own;		// its real surface while the clear one stands in; a reference is held
	Bool                   glow;		// a glow texture (lamp glows and the painted pools), not a beam's
};
static BeamTexture			s_beamTex[16];
static Int					s_beamTexCount = 0;
static IDirect3DTexture9	*s_clearTex    = NULL;

static Bool isBeamTexture(TextureClass *tex)
{
	if (tex == NULL)
		return FALSE;
	const char *name = tex->Get_Texture_Name();
	return (name != NULL && _strnicmp(name, "lightbeam", 9) == 0) ? TRUE : FALSE;	// lightbeam, lightbeamdiffuse
}

static void rememberBeamTexture(TextureClass *tex, Bool glow = FALSE)
{
	for (Int i = 0; i < s_beamTexCount; ++i)
		if (s_beamTex[i].tex == tex)
			return;
	if (s_beamTexCount >= (Int)(sizeof(s_beamTex) / sizeof(s_beamTex[0])))
		return;
	tex->Add_Ref();
	s_beamTex[s_beamTexCount].tex = tex;
	s_beamTex[s_beamTexCount].own = NULL;
	s_beamTex[s_beamTexCount].glow = glow;
	++s_beamTexCount;
}

static void restoreBeamTexture(BeamTexture &bt)
{
	if (s_clearTex != NULL && bt.tex->Peek_D3D_Base_Texture() == s_clearTex)
	{
		if (bt.own != NULL)
			bt.tex->Set_D3D_Base_Texture(bt.own);
		else
			bt.tex->Invalidate();		// it never had its own surface here: let it load as usual
	}
	if (bt.own != NULL)
	{
		bt.own->Release();
		bt.own = NULL;
	}
}

// Ronin @feature 09/10/2026 DX9: every frame, since the texture manager may reload a texture and put its real surface back.
static void swapBeamTextures(IDirect3DDevice9 *dev, Bool hideBeams, Bool hideGlows)
{
	for (Int i = 0; i < s_beamTexCount; )
	{
		BeamTexture &bt = s_beamTex[i];
		const Bool hide = bt.glow ? hideGlows : hideBeams;
		if (bt.tex->Num_Refs() == 1)		// only this list still holds it: its map was unloaded
		{
			restoreBeamTexture(bt);
			bt.tex->Release_Ref();
			s_beamTex[i] = s_beamTex[--s_beamTexCount];
			continue;
		}
		if (!hide)
			restoreBeamTexture(bt);
		else
		{
			if (s_clearTex == NULL && dev != NULL &&
				SUCCEEDED(dev->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &s_clearTex, NULL)) && s_clearTex != NULL)
			{
				D3DLOCKED_RECT lr;
				if (SUCCEEDED(s_clearTex->LockRect(0, &lr, NULL, 0)))
				{
					*(DWORD *)lr.pBits = 0x00000000;		// black and clear: nothing under either blend
					s_clearTex->UnlockRect(0);
				}
			}
			IDirect3DBaseTexture9 *cur = bt.tex->Peek_D3D_Base_Texture();
			if (s_clearTex != NULL && cur != s_clearTex)
			{
				if (bt.own != NULL)
					bt.own->Release();
				bt.own = cur;
				if (bt.own != NULL)
					bt.own->AddRef();
				bt.tex->Set_D3D_Base_Texture(s_clearTex);
			}
		}
		++i;
	}
}

static inline UnsignedInt fixtureHash(const void *p, UnsignedInt mask)
{
	UnsignedInt v = (UnsignedInt)(size_t)p;
	v = (v >> 4) * 2654435761u;
	return (v >> 12) & mask;
}

// Ronin @feature 09/10/2026 DX9: is polygon i on a beam texture. Polygons come in runs of one texture, so the name is compared
// once a run.
struct BeamPolys
{
	MeshModelClass *mdl;
	Bool            perPoly, single;
	TextureClass   *last;
	Bool            lastIs;
	Bool is(Int i)
	{
		if (!perPoly)
			return single;
		TextureClass *t = mdl->Peek_Texture(i, 0, 0);
		if (t != last)
		{
			last   = t;
			lastIs = isBeamTexture(t);
			if (lastIs)
				rememberBeamTexture(t);
		}
		return lastIs;
	}
};

// Ronin @feature 09/10/2026 DX9: LIGHTING PART 2, STEP 2 - THE LAMP GLOWS. Colours measured from the textures (weighted by
// brightness, Lighting_Work.md 3.5); reach is a first guess by kind, scaled by `lights glowreach`.
struct GlowKind
{
	const char *name;		// the texture, without its extension
	float       colour[3];
	float       reach;
	Bool        hide;		// its art may go clear. Not `coplight`: 9 meshes draw it OPAQUE (hovercraft, battleship lamps)
};
// Ronin @bugfix 25/10/2026 DX9: a glow lights a halo, as the runway lamps' 20 - the street lamps' 40/30/25 painted pools over the
// beams' own (a GLA night building carries 9 to 48 of them).
static const GlowKind GLOW_TABLE[] =
{
	{ "coplight4",      { 1.00f, 0.99f, 0.54f }, 20.0f, TRUE },		// street and building lamps
	{ "coplight3",      { 1.00f, 0.82f, 0.46f }, 20.0f, TRUE },
	{ "coplight",       { 1.00f, 0.23f, 0.15f }, 20.0f, FALSE },
	{ "spotlight",      { 1.00f, 0.99f, 0.46f }, 20.0f, TRUE },
	{ "pmredlight",     { 1.00f, 0.29f, 0.28f }, 18.0f, TRUE },		// red warning lamps
	{ "pmrunwaylightr", { 1.00f, 0.42f, 0.42f }, 20.0f, TRUE },
	{ "pmrunwaylight",  { 0.40f, 0.85f, 1.00f }, 20.0f, TRUE },
	{ "pmgreenlight",   { 0.21f, 1.00f, 0.21f }, 15.0f, TRUE },
	{ "atyellowlite2",  { 1.00f, 0.97f, 0.31f }, 20.0f, TRUE },
};
static const Int   GLOW_KINDS      = (Int)(sizeof(GLOW_TABLE) / sizeof(GLOW_TABLE[0]));
static const float GLOW_MAX_RADIUS = 6.0f;		// a larger piece on these textures is a painted pool, not a lamp
static const float GLOW_RADIUS_USUAL = 2.0f;	// Ronin @feature 25/10/2026 DX9: the most common lamp's: 1727 of the art's 3956 (glow_survey.py)

static Int glowTextureKind(TextureClass *tex)
{
	if (tex == NULL)
		return -1;
	const char *name = tex->Get_Texture_Name();
	if (name == NULL)
		return -1;
	for (Int k = 0; k < GLOW_KINDS; ++k)
	{
		const size_t len = strlen(GLOW_TABLE[k].name);
		if (_strnicmp(name, GLOW_TABLE[k].name, len) == 0 && (name[len] == '.' || name[len] == 0))
			return k;
	}
	return -1;
}

// Ronin @feature 09/10/2026 DX9: polygon i's glow kind, -1 = none. As BeamPolys: the name is compared once a run of polygons.
struct GlowPolys
{
	MeshModelClass *mdl;
	Bool            perPoly;
	Int             single;
	TextureClass   *last;
	Int             lastKind;
	Int kind(Int i)
	{
		if (!perPoly)
			return single;
		TextureClass *t = mdl->Peek_Texture(i, 0, 0);
		if (t != last)
		{
			last     = t;
			lastKind = glowTextureKind(t);
			if (lastKind >= 0 && GLOW_TABLE[lastKind].hide)
				rememberBeamTexture(t, TRUE);
		}
		return lastKind;
	}
};

static inline Int fixRoot(const UnsignedShort *p, Int x)
{
	while (p[x] != x)
		x = p[x];
	return x;
}

// Ronin @feature 09/10/2026 DX9: a model's lamp glows, once. Its glow polygons, welded and joined into pieces; a small piece is
// a lamp, pieces that share a centre (two crossed quads) are one lamp.
static void readGlows(MeshModelClass *mdl, FixtureModel *slot, GlowPolys &gp)
{
	const Int nv = mdl->Get_Vertex_Count();
	const Int np = mdl->Get_Polygon_Count();
	const Vector3  *verts = mdl->Get_Vertex_Array();
	const TriIndex *polys = mdl->Get_Polygon_Array();
	s_glowRejected = (nv > MAX_FIXTURE_VERTS) ? TRUE : FALSE;
	if (nv > MAX_FIXTURE_VERTS)
		return;
	static UnsignedShort parent[MAX_FIXTURE_VERTS];
	static signed char   kind[MAX_FIXTURE_VERTS];		// -1 = on no glow polygon
	memset(kind, -1, (size_t)nv);
	for (Int i = 0; i < np; ++i)
	{
		const Int k = gp.kind(i);
		if (k < 0)
			continue;
		kind[polys[i].I] = (signed char)k;
		kind[polys[i].J] = (signed char)k;
		kind[polys[i].K] = (signed char)k;
	}
	for (Int v = 0; v < nv; ++v)
	{
		parent[v] = (UnsignedShort)v;
		if (kind[v] < 0)
			continue;
		for (Int w = 0; w < v; ++w)
		{
			if (kind[w] >= 0 && fabsf(verts[v].X - verts[w].X) < 0.02f && fabsf(verts[v].Y - verts[w].Y) < 0.02f &&
				fabsf(verts[v].Z - verts[w].Z) < 0.02f)
			{
				parent[v] = parent[w];
				break;
			}
		}
	}
	for (Int i = 0; i < np; ++i)
	{
		if (gp.kind(i) < 0)
			continue;
		const Int a = fixRoot(parent, polys[i].I);
		const Int b = fixRoot(parent, polys[i].J);
		parent[b] = (UnsignedShort)a;
		parent[fixRoot(parent, polys[i].K)] = (UnsignedShort)a;
	}
	// Ronin @feature 25/10/2026 DX9: every lamp first; then lamps within `glowjoin` of each other light as one, each keeping its corona
	// (the palace's 40 bulbs, 1.9 apart, were 24 street lamps' light and the cap hid the rest).
	static const Int MAX_MESH_LAMPS = MAX_FIXTURE_VERTS / 3;
	static FixtureGlow   lamps[MAX_MESH_LAMPS];
	static UnsignedShort joinTo[MAX_MESH_LAMPS];
	Int lampCount = 0;
	for (Int r = 0; r < nv; ++r)
	{
		if (kind[r] < 0 || fixRoot(parent, r) != r)
			continue;
		Vector3 lo(1.0e9f, 1.0e9f, 1.0e9f), hi(-1.0e9f, -1.0e9f, -1.0e9f);
		for (Int v = 0; v < nv; ++v)
		{
			if (kind[v] < 0 || fixRoot(parent, v) != r)
				continue;
			if (verts[v].X < lo.X) lo.X = verts[v].X;
			if (verts[v].Y < lo.Y) lo.Y = verts[v].Y;
			if (verts[v].Z < lo.Z) lo.Z = verts[v].Z;
			if (verts[v].X > hi.X) hi.X = verts[v].X;
			if (verts[v].Y > hi.Y) hi.Y = verts[v].Y;
			if (verts[v].Z > hi.Z) hi.Z = verts[v].Z;
		}
		const Vector3 centre = (lo + hi) * 0.5f;
		const float   radius = (hi - lo).Length() * 0.5f;
		if (radius > GLOW_MAX_RADIUS || radius < 0.05f)
		{
			s_glowRejected = TRUE;
			// Ronin @feature 09/10/2026 DX9: a painted pool of light. Kept by its size; one that no lamp covers gets a light (renderPass).
			if (radius > GLOW_MAX_RADIUS && GLOW_TABLE[kind[r]].hide && slot->poolCount < MAX_POOLS_A_MESH)
			{
				FixtureGlow &fp = s_fixPools[s_fixPoolCount++];
				fp.centre = centre;
				fp.radius = radius;
				fp.kind   = kind[r];
				fp.spread = 0.0f;
				fp.power  = 1.0f;
				++slot->poolCount;
			}
			continue;
		}
		Bool merged = FALSE;
		for (Int g = 0; g < lampCount && !merged; ++g)
		{
			FixtureGlow &fg = lamps[g];
			const float reach = (radius > fg.radius) ? radius : fg.radius;
			if ((centre - fg.centre).Length() <= 0.75f * reach)
			{
				fg.radius = reach;
				merged = TRUE;
			}
		}
		if (!merged && lampCount < MAX_MESH_LAMPS)
		{
			FixtureGlow &fg = lamps[lampCount++];
			fg.centre = centre;
			fg.radius = radius;
			fg.kind   = kind[r];
			fg.spread = 0.0f;
			fg.power  = 1.0f;
		}
		else if (!merged)
			s_glowRejected = TRUE;
	}

	// Ronin @feature 25/10/2026 DX9: joined by chains of near neighbours; each group's root is its first lamp.
	for (Int a = 0; a < lampCount; ++a)
	{
		joinTo[a] = (UnsignedShort)a;
		for (Int b = 0; b < a && s_glowJoin > 0.0f; ++b)
		{
			if ((lamps[a].centre - lamps[b].centre).Length() > s_glowJoin)
				continue;
			const Int ra = fixRoot(joinTo, a);
			const Int rb = fixRoot(joinTo, b);
			if (ra != rb)
				joinTo[(ra > rb) ? ra : rb] = (UnsignedShort)((ra < rb) ? ra : rb);
		}
	}
	// Ronin @feature 25/10/2026 DX9: the lights first, then the joined bulbs' coronas - what the cap leaves out is a corona, never a light.
	for (Int pass = 0; pass < 2; ++pass)
	{
		for (Int a = 0; a < lampCount; ++a)
		{
			const Int root = fixRoot(joinTo, a);
			if (pass == 0 && root != a)
				continue;
			Int members = 0;
			float area = 0.0f;
			Vector3 lo(1.0e9f, 1.0e9f, 1.0e9f), hi(-1.0e9f, -1.0e9f, -1.0e9f);
			for (Int b = root; b < lampCount; ++b)
			{
				if (fixRoot(joinTo, b) != root)
					continue;
				++members;
				const float share = lamps[b].radius / GLOW_RADIUS_USUAL;
				area += share * share;
				const Vector3 &c = lamps[b].centre;
				if (c.X < lo.X) lo.X = c.X;
				if (c.Y < lo.Y) lo.Y = c.Y;
				if (c.Z < lo.Z) lo.Z = c.Z;
				if (c.X > hi.X) hi.X = c.X;
				if (c.Y > hi.Y) hi.Y = c.Y;
				if (c.Z > hi.Z) hi.Z = c.Z;
			}
			if (pass == 1 && members < 2)
				continue;
			if (slot->glowCount >= MAX_GLOWS_A_MESH)
			{
				s_glowRejected = TRUE;
				continue;
			}
			FixtureGlow &fg = s_fixGlows[s_fixGlowCount++];
			fg = lamps[a];
			++slot->glowCount;
			if (pass == 1)
				fg.power = 0.0f;		// a bulb: its corona; the string's light is the joined one
			else if (members > 1)
			{
				// Ronin @feature 25/10/2026 DX9: the joined light - the light its bulbs' glow area holds, in usual lamps, never more than
				// they gave apart; as far round as the string runs.
				fg.centre = (lo + hi) * 0.5f;
				fg.spread = (hi - lo).Length() * 0.5f;
				fg.power  = (area < (float)members) ? area : (float)members;
				if (fg.spread < 0.01f)
					fg.spread = 0.01f;
			}
		}
	}
}

// Ronin @feature 09/10/2026 DX9: reads a model's beams, once. Rule (tried on all 1210 shipped beams, scratchpad beam_survey3.py):
// a beam is 3..8 loose quads; each quad's two ends are its lowest and highest V, the narrow one is the lamp. NULL = no beam here.
static const FixtureModel *fixtureModel(MeshModelClass *mdl)
{
	const Int nv = mdl->Get_Vertex_Count();
	const Int np = mdl->Get_Polygon_Count();
	const Vector3  *verts = mdl->Get_Vertex_Array();
	const TriIndex *polys = mdl->Get_Polygon_Array();
	if (nv <= 0 || np <= 0 || verts == NULL || polys == NULL || mdl->Get_Pass_Count() <= 0 || mdl->Get_Flag(MeshGeometryClass::SKIN))
		return NULL;

	const UnsignedInt h = fixtureHash(mdl, MAX_FIXTURE_MODELS - 1);
	FixtureModel *slot = NULL;
	for (Int probe = 0; probe < 16; ++probe)
	{
		FixtureModel &m = s_fixModels[(h + probe) & (MAX_FIXTURE_MODELS - 1)];
		if (m.key == mdl && m.verts == nv && m.polys == np && m.first == verts[0])
			return &m;
		if (m.key == NULL || m.key == mdl)
		{
			slot = &m;
			break;
		}
	}

	BeamPolys bp;
	bp.mdl     = mdl;
	bp.perPoly = mdl->Has_Texture_Array(0, 0) ? TRUE : FALSE;
	bp.single  = FALSE;
	bp.last    = NULL;
	bp.lastIs  = FALSE;
	GlowPolys gp;
	gp.mdl      = mdl;
	gp.perPoly  = bp.perPoly;
	gp.single   = -1;
	gp.last     = NULL;
	gp.lastKind = -1;
	if (!bp.perPoly)
	{
		TextureClass *t = mdl->Peek_Single_Texture(0, 0);
		bp.single = isBeamTexture(t);
		gp.single = bp.single ? -1 : glowTextureKind(t);
		if (gp.single >= 0 && GLOW_TABLE[gp.single].hide)
			rememberBeamTexture(t, TRUE);
		if (!bp.single && gp.single < 0)
			return NULL;
		if (bp.single)
			rememberBeamTexture(t);
	}
	Int beamPolys = 0, glowPolys = 0;
	Bool keepArt = FALSE;		// Ronin @bugfix 09/10/2026 DX9: a glow whose art must stay drawn: the mesh is never skipped
	Vector3 glowFacing(0.0f, 0.0f, 0.0f);	// Ronin @bugfix 11/10/2026 DX9: the glow polygons' normals summed, and their lengths
	float glowArea = 0.0f;
	for (Int i = 0; i < np; ++i)
	{
		if (bp.is(i))
			++beamPolys;
		else if (gp.kind(i) >= 0)
		{
			++glowPolys;
			if (!GLOW_TABLE[gp.kind(i)].hide)
				keepArt = TRUE;
			Vector3 n;
			Vector3::Cross_Product(verts[polys[i].J] - verts[polys[i].I], verts[polys[i].K] - verts[polys[i].I], &n);
			glowFacing += n;
			glowArea += n.Length();
		}
	}
	if (beamPolys == 0 && glowPolys == 0)
		return NULL;

	// Ronin @feature 09/10/2026 DX9: a full table or a full pool starts over - every model is read again when it is next drawn.
	if (slot == NULL || s_fixBeamCount + MAX_BEAMS_A_MESH > MAX_FIXTURE_BEAMS || s_fixGlowCount + MAX_GLOWS_A_MESH > MAX_FIXTURE_GLOWS ||
		s_fixPoolCount + MAX_POOLS_A_MESH > MAX_FIXTURE_POOLS)
	{
		memset(s_fixModels, 0, sizeof(s_fixModels));
		s_fixBeamCount  = 0;
		s_fixGlowCount  = 0;
		s_fixPoolCount  = 0;
		s_fixModelCount = 0;
		slot = &s_fixModels[h];
	}
	slot->key       = mdl;
	slot->verts     = nv;
	slot->polys     = np;
	slot->first     = verts[0];
	slot->firstBeam = s_fixBeamCount;
	slot->beamCount = 0;
	slot->pure      = (beamPolys == np) ? TRUE : FALSE;
	slot->firstGlow = s_fixGlowCount;
	slot->glowCount = 0;
	slot->firstPool = s_fixPoolCount;
	slot->poolCount = 0;
	++s_fixModelCount;
	slot->glowOnly  = FALSE;
	slot->glowUp    = (glowArea > 0.0f && glowFacing.Z > 0.7f * glowArea) ? TRUE : FALSE;
	if (glowPolys > 0)
	{
		readGlows(mdl, slot, gp);
		slot->glowOnly = (glowPolys == np && slot->glowCount > 0 && !s_glowRejected && !keepArt) ? TRUE : FALSE;
	}
	if (beamPolys == 0)
		return slot;

	const Vector2 *uv = mdl->Get_UV_Array(0, 0);
	if (uv == NULL || nv > MAX_FIXTURE_VERTS)
		return slot;		// hidden with the others, but no light can be read off it

	// Ronin @feature 09/10/2026 DX9: the beam's vertices, welded by position, joined through its polygons: each piece is one quad.
	static UnsignedShort parent[MAX_FIXTURE_VERTS];
	static UnsignedByte  used[MAX_FIXTURE_VERTS];
	memset(used, 0, (size_t)nv);
	for (Int i = 0; i < np; ++i)
	{
		if (!bp.is(i))
			continue;
		used[polys[i].I] = 1;
		used[polys[i].J] = 1;
		used[polys[i].K] = 1;
	}
	for (Int v = 0; v < nv; ++v)
	{
		parent[v] = (UnsignedShort)v;
		if (!used[v])
			continue;
		for (Int w = 0; w < v; ++w)
		{
			if (used[w] && fabsf(verts[v].X - verts[w].X) < 0.02f && fabsf(verts[v].Y - verts[w].Y) < 0.02f &&
				fabsf(verts[v].Z - verts[w].Z) < 0.02f)
			{
				parent[v] = parent[w];
				break;
			}
		}
	}
	struct Find
	{
		static Int root(const UnsignedShort *p, Int x)
		{
			while (p[x] != x)
				x = p[x];
			return x;
		}
	};
	for (Int i = 0; i < np; ++i)
	{
		if (!bp.is(i))
			continue;
		const Int a = Find::root(parent, polys[i].I);
		const Int b = Find::root(parent, polys[i].J);
		const Int c = Find::root(parent, polys[i].K);
		parent[b] = (UnsignedShort)a;
		parent[Find::root(parent, c)] = (UnsignedShort)a;
	}

	struct Quad
	{
		Vector3 lamp, end;
		float   lampR, endR;
		Int     beam;
	};
	static Quad quads[MAX_FIXTURE_QUADS];
	Int quadCount = 0;
	for (Int r = 0; r < nv && quadCount < MAX_FIXTURE_QUADS; ++r)
	{
		if (!used[r] || Find::root(parent, r) != r)
			continue;
		float vMin = 1.0e9f, vMax = -1.0e9f;
		for (Int v = 0; v < nv; ++v)
		{
			if (!used[v] || Find::root(parent, v) != r)
				continue;
			if (uv[v].Y < vMin) vMin = uv[v].Y;
			if (uv[v].Y > vMax) vMax = uv[v].Y;
		}
		const float span = vMax - vMin;
		if (span < 0.2f)
			continue;
		Vector3 c[2] = { Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };
		Int     n[2] = { 0, 0 };
		for (Int v = 0; v < nv; ++v)
		{
			if (!used[v] || Find::root(parent, v) != r)
				continue;
			const Int e = (uv[v].Y <= vMin + 0.1f * span) ? 0 : ((uv[v].Y >= vMax - 0.1f * span) ? 1 : -1);
			if (e >= 0)
			{
				c[e] += verts[v];
				++n[e];
			}
		}
		if (n[0] == 0 || n[1] == 0)
			continue;
		c[0] /= (float)n[0];
		c[1] /= (float)n[1];
		float rad[2] = { 0.0f, 0.0f };
		for (Int v = 0; v < nv; ++v)
		{
			if (!used[v] || Find::root(parent, v) != r)
				continue;
			const Int e = (uv[v].Y <= vMin + 0.1f * span) ? 0 : ((uv[v].Y >= vMax - 0.1f * span) ? 1 : -1);
			if (e >= 0)
			{
				const float d = (verts[v] - c[e]).Length();
				if (d > rad[e])
					rad[e] = d;
			}
		}
		const Int lampEnd = (rad[0] <= rad[1]) ? 0 : 1;		// the narrow end is the lamp
		Quad &q = quads[quadCount];
		q.lamp  = c[lampEnd];
		q.end   = c[lampEnd ^ 1];
		q.lampR = rad[lampEnd];
		q.endR  = rad[lampEnd ^ 1];
		q.beam  = -1;
		if ((q.end - q.lamp).Length() < 0.5f)
			continue;
		++quadCount;
	}

	// Ronin @feature 09/10/2026 DX9: quads that start at one lamp and end together are one beam.
	struct Group
	{
		Vector3 lamp, end;		// the first quad's
		float   lampR, endR;
	};
	Group groups[MAX_BEAMS_A_MESH];
	Int groupCount = 0;
	for (Int qi = 0; qi < quadCount; ++qi)
	{
		Quad &q = quads[qi];
		for (Int g = 0; g < groupCount; ++g)
		{
			const float lampR = (q.lampR > groups[g].lampR) ? q.lampR : groups[g].lampR;
			const float endR  = (q.endR  > groups[g].endR)  ? q.endR  : groups[g].endR;
			if ((q.lamp - groups[g].lamp).Length() <= 2.5f * lampR + 0.3f && (q.end - groups[g].end).Length() <= 2.5f * endR)
			{
				q.beam = g;
				groups[g].lampR = lampR;
				groups[g].endR  = endR;
				break;
			}
		}
		if (q.beam < 0 && groupCount < MAX_BEAMS_A_MESH)
		{
			q.beam = groupCount;
			groups[groupCount].lamp  = q.lamp;
			groups[groupCount].end   = q.end;
			groups[groupCount].lampR = q.lampR;
			groups[groupCount].endR  = q.endR;
			++groupCount;
		}
	}
	for (Int g = 0; g < groupCount; ++g)
	{
		Vector3 lamp(0.0f, 0.0f, 0.0f), end(0.0f, 0.0f, 0.0f);
		Int n = 0;
		for (Int qi = 0; qi < quadCount; ++qi)
		{
			if (quads[qi].beam != g)
				continue;
			lamp += quads[qi].lamp;
			end  += quads[qi].end;
			++n;
		}
		if (n == 0)
			continue;
		lamp /= (float)n;
		end  /= (float)n;
		float endRadius = 0.0f;
		for (Int qi = 0; qi < quadCount; ++qi)
		{
			if (quads[qi].beam != g)
				continue;
			const float d = (quads[qi].end - end).Length() + quads[qi].endR;
			if (d > endRadius)
				endRadius = d;
		}
		if ((end - lamp).Length() < 0.5f || endRadius < 0.05f)
			continue;
		float lampRadius = 0.0f;
		for (Int qi = 0; qi < quadCount; ++qi)
		{
			if (quads[qi].beam != g)
				continue;
			const float d = (quads[qi].lamp - lamp).Length() + quads[qi].lampR;
			if (d > lampRadius)
				lampRadius = d;
		}
		FixtureBeam &fb = s_fixBeams[s_fixBeamCount++];
		fb.lamp      = lamp;
		fb.end       = end;
		fb.endRadius = endRadius;
		// Ronin @bugfix 12/10/2026 DX9: a SHEET of diffuse light has no lamp's width to give (the derrick's: 38 units across at its
		// lamp). 322 of the shipped quads, their narrow end over half the wide one; every other is under a fifth. Those: the usual.
		const float most = BEAM_LAMP_MOST * endRadius;
		fb.lampRadius = (lampRadius <= most) ? lampRadius : ((BEAM_LAMP_USUAL < most) ? BEAM_LAMP_USUAL : most);
		++slot->beamCount;
	}
	return slot;
}

// Ronin @bugfix 11/10/2026 DX9: THE BLINKED LAMPS. The art blinks a lamp by JUMPING its bone down its own Z, behind what it is
// mounted on (197 of the 199 shipped cases face up that Z: Lighting_Work.md 6c). One record an animation's bone, made once.
struct BlinkBone
{
	const HAnimClass *anim;
	Int         pivot, frames, pivots;
	UnsignedInt name;		// the animation's name, hashed: a freed animation's address can come back as another
	float       top;		// the highest Z the animation gives the bone
	float       jump;		// the largest change of Z in one frame; 0 = the bone is not blinked
};
static BlinkBone	s_blinkBones[256];

static const BlinkBone *blinkBone(HAnimClass *anim, Int pivot, Int frames)
{
	UnsignedInt name = 2166136261u;
	for (const char *c = anim->Get_Name(); c != NULL && *c != 0; ++c)
		name = (name ^ (UnsignedInt)(unsigned char)*c) * 16777619u;
	const Int pivots = anim->Get_Num_Pivots();
	const UnsignedInt h = fixtureHash(anim, 255) + (UnsignedInt)pivot * 7u;
	BlinkBone *slot = &s_blinkBones[h & 255];
	for (Int probe = 0; probe < 8; ++probe)
	{
		BlinkBone &b = s_blinkBones[(h + probe) & 255];
		if (b.anim == anim && b.pivot == pivot && b.frames == frames && b.pivots == pivots && b.name == name)
			return &b;
		if (b.anim == NULL)
		{
			slot = &b;
			break;
		}
	}
	Vector3 t;
	anim->Get_Translation(t, pivot, (float)(frames - 1));
	float was = t.Z, low = t.Z, top = t.Z, jump = 0.0f;		// from the last frame: a looped animation jumps there too
	for (Int f = 0; f < frames; ++f)
	{
		anim->Get_Translation(t, pivot, (float)f);
		low  = (t.Z < low) ? t.Z : low;
		top  = (t.Z > top) ? t.Z : top;
		jump = (fabsf(t.Z - was) > jump) ? fabsf(t.Z - was) : jump;
		was  = t.Z;
	}
	slot->anim   = anim;
	slot->pivot  = pivot;
	slot->frames = frames;
	slot->pivots = pivots;
	slot->name   = name;
	slot->top    = top;
	// Ronin @bugfix 11/10/2026 DX9: a jump is most of the way in one frame. A bone that glides (a lamp on a rising wall) is left lit.
	slot->jump   = (top - low >= 0.05f && jump >= 0.4f * (top - low)) ? jump : 0.0f;
	return slot;
}

// Ronin @bugfix 11/10/2026 DX9: is this lamp's mesh in the place its animation hides it? Lit only at the top of the jump.
static Bool lampSunk(MeshClass *mesh)
{
	RenderObjClass *box = mesh->Get_Container();
	if (box == NULL || box->Class_ID() != RenderObjClass::CLASSID_HLOD)
		return FALSE;
	float frame = 0.0f, mult = 1.0f;
	int frames = 0, mode = 0;
	HAnimClass *anim = static_cast<HLodClass *>(box)->Peek_Animation_And_Info(frame, frames, mode, mult);
	if (anim == NULL || frames < 2)
		return FALSE;
	const Int pivot = box->Get_Sub_Object_Bone_Index(mesh);
	if (pivot <= 0 || pivot >= anim->Get_Num_Pivots())
		return FALSE;
	const BlinkBone *bb = blinkBone(anim, pivot, frames);
	if (bb->jump <= 0.0f)
		return FALSE;
	Vector3 t;
	anim->Get_Translation(t, pivot, frame);
	return (bb->top - t.Z > 0.5f * bb->jump) ? TRUE : FALSE;
}

// Ronin @feature 09/10/2026 DX9: called by MeshClass::Render for every mesh it has not ruled out, in every pass. Only a mesh that
// is being drawn reaches this, so a unit hidden by the fog of war gives no light away.
int R3DLocalLights::noteFixtureMesh(MeshClass *mesh)
{
	MeshModelClass *mdl = (mesh != NULL) ? mesh->Peek_Model() : NULL;
	if (mdl == NULL)
		return 1;		// no model yet: ask again
	const FixtureModel *fm = fixtureModel(mdl);
	if (fm == NULL)
		return 0;
	if (s_collect && ((s_beamsOn && fm->beamCount > 0) || (s_glowsOn && fm->glowCount > 0) || (s_poolsOn && fm->poolCount > 0)))
	{
		// Ronin @feature 09/10/2026 DX9: once a frame - the water's mirror renders the scene again inside the same window.
		const UnsignedInt h = fixtureHash(mesh, 511);
		Bool taken = FALSE;
		SeenMesh *spot = NULL;
		for (Int probe = 0; probe < 8 && spot == NULL; ++probe)
		{
			SeenMesh &s = s_seenMesh[(h + probe) & 511];
			if (s.frame == s_fixFrame && s.mesh == mesh)
			{
				taken = TRUE;
				break;
			}
			if (s.frame != s_fixFrame)
				spot = &s;
		}
		if (!taken && spot != NULL)
		{
			spot->mesh  = mesh;
			spot->frame = s_fixFrame;
			const Matrix3D &tm = mesh->Get_Transform();
			RenderObjClass *top = mesh;
			while (top->Get_Container() != NULL)
				top = top->Get_Container();
			const Matrix3D &root = top->Get_Transform();
			Vector3 rootUp = root.Get_Z_Vector();
			const float upLen = rootUp.Length();
			if (upLen > 0.0001f)
				rootUp /= upLen;		// Ronin @feature 09/10/2026 DX9: a scaled object's axes are not unit length
			const Vector3 rootPos = root.Get_Translation();
			for (Int b = 0; s_beamsOn && b < fm->beamCount && s_frameBeamCount < MAX_FRAME_BEAMS; ++b)
			{
				const FixtureBeam &fb = s_fixBeams[fm->firstBeam + b];
				FrameBeam &o = s_frameBeams[s_frameBeamCount];
				Matrix3D::Transform_Vector(tm, fb.lamp, &o.lamp);
				Matrix3D::Transform_Vector(tm, fb.end, &o.end);
				const float own   = (fb.end - fb.lamp).Length();
				const float world = (o.end - o.lamp).Length();
				if (own < 0.01f || world < 0.01f)
					continue;
				o.endRadius = fb.endRadius * (world / own);		// a scaled object scales its beam
				o.lampRadius = fb.lampRadius * (world / own);
				o.up        = rootUp;
				o.height    = Vector3::Dot_Product(o.lamp - rootPos, rootUp);
				o.mesh      = mesh;
				o.index     = b;
				++s_frameBeamCount;
			}
			// Ronin @bugfix 11/10/2026 DX9: a lamp its animation has sunk out of sight gives no light: the battleship's two red lamps
			// shone on from inside the hull while their art blinked.
			const Bool sunk = (fm->glowUp && (fm->glowCount > 0 || fm->poolCount > 0)) ? lampSunk(mesh) : FALSE;
			for (Int g = 0; s_glowsOn && !sunk && g < fm->glowCount && s_frameGlowCount < MAX_FRAME_GLOWS; ++g)
			{
				const FixtureGlow &fg = s_fixGlows[fm->firstGlow + g];
				FrameGlow &o = s_frameGlows[s_frameGlowCount];
				Vector3 beside;
				Matrix3D::Transform_Vector(tm, fg.centre, &o.pos);
				Matrix3D::Transform_Vector(tm, fg.centre + Vector3(1.0f, 0.0f, 0.0f), &beside);
				o.scale = (beside - o.pos).Length();
				o.radius = fg.radius * o.scale;
				o.spread = fg.spread * o.scale;
				o.power  = fg.power;
				o.mesh  = mesh;
				o.index = g;
				o.kind  = fg.kind;
				++s_frameGlowCount;
			}
			for (Int p = 0; s_poolsOn && !sunk && p < fm->poolCount && s_framePoolCount < MAX_FRAME_POOLS; ++p)
			{
				const FixtureGlow &fp = s_fixPools[fm->firstPool + p];
				FramePool &o = s_framePools[s_framePoolCount];
				Vector3 beside;
				Matrix3D::Transform_Vector(tm, fp.centre, &o.pos);
				Matrix3D::Transform_Vector(tm, fp.centre + Vector3(1.0f, 0.0f, 0.0f), &beside);
				o.radius = fp.radius * (beside - o.pos).Length();
				o.mesh   = mesh;
				o.index  = p;
				o.kind   = fp.kind;
				++s_framePoolCount;
			}
		}
	}
	if (s_beamsOn && fm->pure)
	{
		++s_stats.beamMeshes;
		return 2;
	}
	if (s_coronasOn && fm->glowOnly)
		return 2;		// Ronin @feature 09/10/2026 DX9: its glow is drawn by the light pass
	return 1;
}

// Ronin @feature 11/10/2026 DX9: LAMP SHADOW MAPS (docs/Lighting_Work.md 6b) - the solid meshes drawn this frame, kept so each lamp's
// map can be drawn from what stands in its light.
struct LampCaster
{
	MeshClass            *mesh;		// valid for this frame only
	const RenderObjClass *root;		// the object it is part of: a lamp's own object casts nothing in its light
	float                 boxC[3];	// its world box: centre
	float                 boxE[3];	// ... and extent
	Bool                  skin;		// drawn through its bones: deformed on the CPU when a map first wants it
	Int                   skinAt;	// ... where in s_skinVerts; -1 = not yet this frame, -2 = no room
	Bool                  perPoly;	// a shader per polygon: only its solid ones cast
};
static const Int	MAX_LAMP_CASTERS = 4096;
static LampCaster	s_casters[MAX_LAMP_CASTERS];
static Int			s_casterCount = 0;
static Bool			s_casterOpen  = FALSE;	// between beginFrame and the light pass

// Ronin @feature 11/10/2026 DX9: what casts a lamp's shadow is drawn solid. Blended or alpha-tested (glass, fences, a beam welded into
// a hull) casts nothing.
static inline Bool castsLampShadow(ShaderClass shader)
{
	return (shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ZERO &&
			shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_DISABLE) ? TRUE : FALSE;
}

void R3DLocalLights::noteCaster(MeshClass *mesh)
{
	if (!s_casterOpen || mesh == NULL || s_casterCount >= MAX_LAMP_CASTERS || ShaderClass::Is_Backface_Culling_Inverted() ||
		TheTerrainShadowPass.inDepthPass || TheTerrainShadowPass.viewDepthPass)
		return;		// the main pass only: not the mirror, not a depth pass
	MeshModelClass *mdl = mesh->Peek_Model();
	if (mdl == NULL || mdl->Get_Polygon_Count() <= 0 || mdl->Get_Pass_Count() <= 0)
		return;
	const Bool perPoly = mdl->Has_Shader_Array(0) ? TRUE : FALSE;
	if (!perPoly && !castsLampShadow(mdl->Get_Single_Shader(0)))
		return;
	// Ronin @feature 11/10/2026 DX9: a skin is deformed through its container's bones (as R3DTaa::noteMesh): without them, no caster.
	const Bool isSkin = mdl->Get_Flag(MeshGeometryClass::SKIN) ? TRUE : FALSE;
	if (isSkin && (mesh->Get_Container() == NULL || mesh->Get_Container()->Get_HTree() == NULL))
		return;
	const RenderObjClass *top = mesh;
	while (top->Get_Container() != NULL)
		top = top->Get_Container();
	const AABoxClass &box = mesh->Get_Bounding_Box();
	LampCaster &c = s_casters[s_casterCount++];
	c.mesh    = mesh;
	c.root    = top;
	c.boxC[0] = box.Center.X; c.boxC[1] = box.Center.Y; c.boxC[2] = box.Center.Z;
	c.boxE[0] = box.Extent.X; c.boxE[1] = box.Extent.Y; c.boxE[2] = box.Extent.Z;
	c.skin    = isSkin;
	c.skinAt  = -1;
	c.perPoly = perPoly;
}

// Ronin @feature 11/10/2026 DX9: STAGE B1 - each beam on screen gets a tile of one atlas, and the casters in its light are drawn into it
// from the lamp, and its light reads it (LocalLight_ps lampShadow). `lights view 4` shows the tiles.
static const Int	LAMP_ATLAS     = 2048;
static const Int	LAMP_TILE      = 256;
// Ronin @bugfix 12/10/2026 DX9: a beam's map keeps its pose while its lamp stays this near it: 2 degrees (the cosine) and 0.1 units.
static const float	LAMP_HOLD_COS  = 0.99939f;
static const float	LAMP_HOLD_MOVE = 0.1f;
static const Int	LAMP_TILES_ROW = LAMP_ATLAS / LAMP_TILE;
static const Int	MAX_LAMP_MAPS  = LAMP_TILES_ROW * LAMP_TILES_ROW;
static const float	LAMP_NEAR      = 0.5f;
// Ronin @feature 11/10/2026 DX9: a point light's one map looks straight down, 70 degrees to each side. Its near plane is 1.5: 108 of
// 434 muzzle bones sit inside their barrel or turret, none of those faces is deeper (scratchpad muzzle_down_survey2.py).
static const float	LAMP_POINT_NEAR = 1.5f;
static const float	LAMP_POINT_TAN  = 2.75f;
static const Int	MAX_POINT_MAPS  = 24;
// Ronin @feature 11/10/2026 DX9: the downward map sees the ground out to 2.75 x the light's height: lower than a third of its reach, the
// light goes on past that. The game's blast lights last 2 seconds (median) and reach 30 to 100 units: those get four level maps too.
static const Int	MAX_SIDE_LIGHTS     = 4;
static const float	LAMP_SIDE_LOW       = 0.34f;
static const float	LAMP_SIDE_MIN_REACH = 20.0f;
static const float	LAMP_MAX_TAN   = 1.732f;	// a map is never wider than 60 degrees off its axis: past that the light has no shadow

struct LampMap
{
	Vector3               lamp, axis;		// world; the axis as the light is aimed
	float                 tanHalf;			// of its square view
	float                 nearPlane, farPlane;
	Bool                  point;			// an engine point light's downward map, not a beam's
	Vector3               boundCentre;		// the light's own sphere
	float                 boundRadius;
	const RenderObjClass *owner;			// the object the lamp is part of
	float                 selfNear;			// its own object casts only past this depth
	D3DXMATRIX            viewProj;			// world to the map's clip space, row vectors
};
static LampMap			s_lampMaps[MAX_LAMP_MAPS];
static Int				s_lampMapCount = 0;
// Ronin @feature 11/10/2026 DX9: last frame's maps are kept - their depth in the other atlas, their matrices here - so each light reads
// its shadow as it is and as it was: where the two differ, the shadow MOVED, and TAA's change mask is told (stage C).
static LampMap			s_lampMapsPrev[MAX_LAMP_MAPS];
static Int				s_lampPrevCount = 0;
static Int				s_lampFlip      = 0;	// which of the two depth atlases holds this frame's maps
// Ronin @feature 11/10/2026 DX9: skinned casters are deformed into this, once a frame each, when a map first wants them.
static const Int		LAMP_SKIN_VERTS = 65536;
static Vector3			s_skinVerts[LAMP_SKIN_VERTS];
static Int				s_skinUsed      = 0;
static TextureClass		*s_lampColour  = NULL;	// the same depth as colour, two bytes a texel: what `lights view 4` shows
static ZTextureClass	*s_lampDepth[2] = { NULL, NULL };	// D24S8, compared by the sampler - as the sun's map; this frame's and last frame's
static Bool				s_lampFailed   = FALSE;
static Bool				s_lampDrawn    = FALSE;	// this frame's tiles are in the atlas
static Bool				s_lampMapsOn   = TRUE;	// `lights lampmaps`
static float			s_lampShadows  = 1.0f;	// `lights lampshadows`
static float			s_lampBias     = 1.0f;	// `lights lampbias`
// Ronin @feature 11/10/2026 DX9: 152 of the art's 1210 beams start inside their own model (72 of 226 headlights): as casters those
// models would put the lamp out. Cut off for the first fifth of the beam, 1 is left (scratchpad lamp_self_survey2.py).
static Bool				s_lampSelf     = TRUE;	// `lights lampself`
static float			s_lampSelfNear = 0.2f;	// `lights lampselfnear`
static Bool				s_pointMaps    = TRUE;	// `lights pointmaps`
static Bool				s_pointSides   = TRUE;	// `lights pointsides`
// Ronin @feature 11/10/2026 DX9: the receiver's bias, from the off-line preview (lampmap_read_sim2.py): along the normal x the sine of
// the light's angle off it, and towards the lamp; in map texels at the receiver, plus a little in units.
static const float		LAMP_BIAS_NORMAL = 1.5f;
static const float		LAMP_BIAS_LIGHT  = 1.0f;
static const float		LAMP_BIAS_UNITS  = 0.05f;

void  R3DLocalLights::setLampMaps(Bool on)		{ s_lampMapsOn = on; }
Bool  R3DLocalLights::getLampMaps(void)			{ return s_lampMapsOn; }
void  R3DLocalLights::setLampShadows(float v)	{ s_lampShadows = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getLampShadows(void)		{ return s_lampShadows; }
void  R3DLocalLights::setLampBias(float v)		{ s_lampBias = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getLampBias(void)			{ return s_lampBias; }
void  R3DLocalLights::setLampSelf(Bool on)		{ s_lampSelf = on; }
Bool  R3DLocalLights::getLampSelf(void)			{ return s_lampSelf; }
void  R3DLocalLights::setLampSelfNear(float v)	{ s_lampSelfNear = (v < 0.0f) ? 0.0f : ((v > 0.5f) ? 0.5f : v); }
float R3DLocalLights::getLampSelfNear(void)		{ return s_lampSelfNear; }
void  R3DLocalLights::setPointMaps(Bool on)		{ s_pointMaps = on; }
Bool  R3DLocalLights::getPointMaps(void)		{ return s_pointMaps; }
void  R3DLocalLights::setPointSides(Bool on)	{ s_pointSides = on; }
Bool  R3DLocalLights::getPointSides(void)		{ return s_pointSides; }

// Ronin @feature 11/10/2026 DX9: TextureClass targets, so the texture manager carries them across a device reset - as TAA's mask pair.
static Bool ensureLampTargets(void)
{
	if (s_lampColour != NULL && s_lampDepth[0] != NULL && s_lampDepth[1] != NULL)
		return TRUE;
	if (s_lampFailed)
		return FALSE;
	s_lampColour = NEW_REF(TextureClass, (LAMP_ATLAS, LAMP_ATLAS, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1,
										  TextureClass::POOL_DEFAULT, true));
	for (Int i = 0; i < 2; ++i)
		s_lampDepth[i] = NEW_REF(ZTextureClass, (LAMP_ATLAS, LAMP_ATLAS, WW3D_ZFORMAT_D24S8, MIP_LEVELS_1,
												 TextureClass::POOL_DEFAULT));
	if (s_lampColour->Peek_D3D_Base_Texture() == NULL || s_lampDepth[0]->Peek_D3D_Base_Texture() == NULL ||
		s_lampDepth[1]->Peek_D3D_Base_Texture() == NULL)
	{
		REF_PTR_RELEASE(s_lampColour);
		REF_PTR_RELEASE(s_lampDepth[0]);
		REF_PTR_RELEASE(s_lampDepth[1]);
		s_lampFailed = TRUE;
		return FALSE;
	}
	return TRUE;
}

// Ronin @feature 11/10/2026 DX9: draws this frame's maps. Target, depth surface and every state it touches that the pass does not
// set again are read raw and written back raw. Called with the pass's own states already saved.
static void drawLampMaps(IDirect3DDevice9 *dev)
{
	s_lampDrawn = FALSE;
	if (s_lampMapCount <= 0 || s_lampVS == NULL || s_lampPS == NULL || !ensureLampTargets())
		return;
	s_lampFlip ^= 1;		// last frame's atlas is left as it is
	IDirect3DSurface9 *cSurf  = s_lampColour->Get_D3D_Surface_Level();
	IDirect3DSurface9 *zSurf  = s_lampDepth[s_lampFlip]->Get_D3D_Surface_Level();
	IDirect3DSurface9 *prevRT = NULL;
	IDirect3DSurface9 *prevDS = NULL;
	if (cSurf != NULL && zSurf != NULL && SUCCEEDED(dev->GetRenderTarget(0, &prevRT)) && prevRT != NULL)
	{
		dev->GetDepthStencilSurface(&prevDS);		// none while the scene depth is suspended
		DWORD zFunc, slopeBias, depthBias, scissor, clipOn;
		float savedPlane[4];
		dev->GetRenderState(D3DRS_CLIPPLANEENABLE,     &clipOn);
		const Bool havePlane = SUCCEEDED(dev->GetClipPlane(0, savedPlane));
		dev->GetRenderState(D3DRS_ZFUNC,               &zFunc);
		dev->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, &slopeBias);
		dev->GetRenderState(D3DRS_DEPTHBIAS,           &depthBias);
		dev->GetRenderState(D3DRS_SCISSORTESTENABLE,   &scissor);
		float savedVS[8 * 4];
		const Bool haveVS = SUCCEEDED(dev->GetVertexShaderConstantF(0, savedVS, 8));
		if (SUCCEEDED(dev->SetRenderTarget(0, cSurf)) && SUCCEEDED(dev->SetDepthStencilSurface(zSurf)))
		{
			dev->SetRenderState(D3DRS_ZENABLE,             D3DZB_TRUE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE,        TRUE);
			dev->SetRenderState(D3DRS_ZFUNC,               D3DCMP_LESSEQUAL);
			dev->SetRenderState(D3DRS_CULLMODE,            D3DCULL_NONE);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE,    FALSE);
			dev->SetRenderState(D3DRS_ALPHATESTENABLE,     FALSE);
			dev->SetRenderState(D3DRS_STENCILENABLE,       FALSE);
			dev->SetRenderState(D3DRS_SCISSORTESTENABLE,   FALSE);
			// Ronin @feature 11/10/2026 DX9: no rasteriser bias: the bias goes on the receiver when the lights read the maps (B2), as the sun's.
			dev->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
			dev->SetRenderState(D3DRS_DEPTHBIAS,           0);
			dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
														D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
			DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ, "R3DLocalLights::lampMaps");
			dev->SetVertexShader(s_lampVS);		// AFTER the layout bind - it clears the VS
			dev->SetPixelShader(s_lampPS);
			for (Int i = 0; i < s_lampMapCount; ++i)
			{
				LampMap &lm = s_lampMaps[i];
				// Ronin @feature 11/10/2026 DX9: Clear is held to the viewport: only this lamp's tile. White = depth 1 = nothing in its light.
				D3DVIEWPORT9 tile;
				tile.X = (DWORD)((i % LAMP_TILES_ROW) * LAMP_TILE);
				tile.Y = (DWORD)((i / LAMP_TILES_ROW) * LAMP_TILE);
				tile.Width = LAMP_TILE; tile.Height = LAMP_TILE; tile.MinZ = 0.0f; tile.MaxZ = 1.0f;
				dev->SetViewport(&tile);
				dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0);

				const D3DXVECTOR3 eye(lm.lamp.X, lm.lamp.Y, lm.lamp.Z);
				const D3DXVECTOR3 at(lm.lamp.X + lm.axis.X, lm.lamp.Y + lm.axis.Y, lm.lamp.Z + lm.axis.Z);
				const D3DXVECTOR3 up((fabsf(lm.axis.Z) > 0.99f) ? 1.0f : 0.0f, 0.0f, (fabsf(lm.axis.Z) > 0.99f) ? 0.0f : 1.0f);
				D3DXMATRIX view, proj;
				D3DXMatrixLookAtLH(&view, &eye, &at, &up);
				D3DXMatrixPerspectiveFovLH(&proj, 2.0f * atanf(lm.tanHalf), 1.0f, lm.nearPlane, lm.farPlane);
				D3DXMatrixMultiply(&lm.viewProj, &view, &proj);
				// Ronin @feature 11/10/2026 DX9: the lamp's own object is cut off nearer than selfNear. With a vertex shader the plane is in
				// clip space, where depth is w and z = w * _33 + _43: this is w - selfNear >= 0.
				const float ownPlane[4] = { 0.0f, 0.0f, -lm.selfNear / proj._43, 1.0f + lm.selfNear * proj._33 / proj._43 };
				dev->SetClipPlane(0, ownPlane);

				// Ronin @feature 11/10/2026 DX9: the cone round the map's square view - its corners are sqrt 2 further out than its sides.
				const float coneCos = 1.0f / sqrtf(1.0f + 2.0f * lm.tanHalf * lm.tanHalf);
				const float coneSin = sqrtf(1.0f - coneCos * coneCos);
				const float centre[3] = { lm.boundCentre.X, lm.boundCentre.Y, lm.boundCentre.Z };
				Int draws = 0;
				for (Int ci = 0; ci < s_casterCount; ++ci)
				{
					LampCaster &lc = s_casters[ci];
					float d2 = 0.0f;		// its box against the light's sphere
					for (Int ax = 0; ax < 3; ++ax)
					{
						const float gap = fabsf(centre[ax] - lc.boxC[ax]) - lc.boxE[ax];
						if (gap > 0.0f)
							d2 += gap * gap;
					}
					if (d2 > lm.boundRadius * lm.boundRadius)
						continue;
					// Ronin @feature 11/10/2026 DX9: then the sphere round its box against the cone: behind the lamp, or wholly off to a side.
					const Vector3 d(lc.boxC[0] - lm.lamp.X, lc.boxC[1] - lm.lamp.Y, lc.boxC[2] - lm.lamp.Z);
					const float r     = sqrtf(lc.boxE[0] * lc.boxE[0] + lc.boxE[1] * lc.boxE[1] + lc.boxE[2] * lc.boxE[2]);
					const float along = Vector3::Dot_Product(d, lm.axis);
					const float off2  = Vector3::Dot_Product(d, d) - along * along;
					const float off   = (off2 > 0.0f) ? sqrtf(off2) : 0.0f;
					if (along + r < lm.nearPlane || off * coneCos - along * coneSin > r)
						continue;
					const Bool own = (lc.root == lm.owner) ? TRUE : FALSE;
					if (own && !s_lampSelf)
					{
						++s_stats.lampOwn;
						continue;
					}
					MeshModelClass *mdl = lc.mesh->Peek_Model();
					if (mdl == NULL)
						continue;
					const Int vcount = mdl->Get_Vertex_Count();
					const Int pcount = mdl->Get_Polygon_Count();
					const TriIndex *polys = mdl->Get_Polygon_Array();
					if (vcount <= 0 || pcount <= 0 || vcount > 65535 || polys == NULL)	// 16-bit indices
						continue;
					const Vector3 *verts = NULL;
					D3DXMATRIX m, mT;
					if (lc.skin)
					{
						// Ronin @feature 11/10/2026 DX9: stage C: a skin's vertices as its bones leave them this frame - in world space.
						if (lc.skinAt == -1)
						{
							lc.skinAt = -2;
							if (s_skinUsed + vcount <= LAMP_SKIN_VERTS)
							{
								lc.mesh->Get_Deformed_Vertices(s_skinVerts + s_skinUsed);
								lc.skinAt = s_skinUsed;
								s_skinUsed += vcount;
							}
						}
						if (lc.skinAt < 0)
						{
							++s_stats.lampSkins;
							continue;
						}
						verts = s_skinVerts + lc.skinAt;
						m     = lm.viewProj;
					}
					else
					{
						verts = mdl->Get_Vertex_Array();
						if (verts == NULL)
							continue;
						D3DXMATRIX w(To_D3DMATRIX(lc.mesh->Get_Transform()));
						D3DXMatrixMultiply(&m, &w, &lm.viewProj);
					}
					D3DXMatrixTranspose(&mT, &m);
					dev->SetVertexShaderConstantF(0, (const float *)&mT, 4);
					dev->SetVertexShaderConstantF(4, (const float *)&mT, 4);	// TaaVelMesh.vso's second output, unused
					dev->SetRenderState(D3DRS_CLIPPLANEENABLE, own ? D3DCLIPPLANE0 : 0);
					if (!lc.perPoly)
					{
						dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)vcount, (UINT)pcount,
													polys, D3DFMT_INDEX16, verts, sizeof(Vector3));
						s_stats.lampTris += pcount;
						++draws;
						continue;
					}
					// Ronin @feature 11/10/2026 DX9: a shader per polygon: the solid ones, in runs.
					Int start = -1;
					for (Int p = 0; p <= pcount; ++p)
					{
						const Bool solid = (p < pcount && castsLampShadow(mdl->Get_Shader(p, 0))) ? TRUE : FALSE;
						if (solid && start < 0)
							start = p;
						else if (!solid && start >= 0)
						{
							dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)vcount, (UINT)(p - start),
														polys + start, D3DFMT_INDEX16, verts, sizeof(Vector3));
							s_stats.lampTris += p - start;
							++draws;
							start = -1;
						}
					}
				}
				s_stats.lampDraws += draws;
				if (lm.point)
					++s_stats.lampPoints;
				if (draws > s_stats.lampMost)
					s_stats.lampMost = draws;
			}
			s_stats.lampMaps = s_lampMapCount;
			s_lampDrawn = TRUE;
		}
		dev->SetRenderTarget(0, prevRT);
		dev->SetDepthStencilSurface(prevDS);
		dev->SetRenderState(D3DRS_ZFUNC,               zFunc);
		dev->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, slopeBias);
		dev->SetRenderState(D3DRS_DEPTHBIAS,           depthBias);
		dev->SetRenderState(D3DRS_SCISSORTESTENABLE,   scissor);
		dev->SetRenderState(D3DRS_CLIPPLANEENABLE,     clipOn);
		if (havePlane)
			dev->SetClipPlane(0, savedPlane);
		if (haveVS)
			dev->SetVertexShaderConstantF(0, savedVS, 8);
	}
	if (prevRT != NULL) prevRT->Release();
	if (prevDS != NULL) prevDS->Release();
	if (cSurf  != NULL) cSurf->Release();
	if (zSurf  != NULL) zSurf->Release();
}

// Ronin @feature 11/10/2026 DX9: view space to a lamp's tile, as atlas uv x w, depth, w - transposed for upload - and the tile in uv.
// D3D9: a pixel's centre is a whole number and a texel's is a half, hence the + 0.5.
static void lampRegisters(const LampMap &lm, Int tileIndex, const D3DXMATRIX &viewToWorld, float *clip16, float *tile4)
{
	const float atlas = (float)LAMP_ATLAS;
	const float tx = (float)((tileIndex % LAMP_TILES_ROW) * LAMP_TILE), ty = (float)((tileIndex / LAMP_TILES_ROW) * LAMP_TILE);
	D3DXMATRIX toTile, m, mT;
	D3DXMatrixIdentity(&toTile);
	toTile._11 =  0.5f * (float)LAMP_TILE / atlas;
	toTile._22 = -0.5f * (float)LAMP_TILE / atlas;
	toTile._41 = (tx + 0.5f * (float)LAMP_TILE + 0.5f) / atlas;
	toTile._42 = (ty + 0.5f * (float)LAMP_TILE + 0.5f) / atlas;
	D3DXMatrixMultiply(&m, &viewToWorld, &lm.viewProj);
	D3DXMatrixMultiply(&m, &m, &toTile);
	D3DXMatrixTranspose(&mT, &m);
	memcpy(clip16, &mT, 16 * sizeof(float));
	tile4[0] = tx / atlas; tile4[1] = ty / atlas;
	tile4[2] = (tx + (float)LAMP_TILE) / atlas; tile4[3] = (ty + (float)LAMP_TILE) / atlas;
}

Bool R3DLocalLights::beamsHiddenIn(const MeshClass *mesh)
{
	return (s_beamsOn && mesh != NULL && mesh->Get_Light_Fixture() == 2) ? TRUE : FALSE;
}

void  R3DLocalLights::setBeams(float v)	{ s_beams = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getBeams(void)	{ return s_beams; }
void  R3DLocalLights::setShafts(float v)	{ s_shafts = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DLocalLights::getShafts(void)	{ return s_shafts; }
void  R3DLocalLights::setRays(float v)	{ s_rays = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
float R3DLocalLights::getRays(void)	{ return s_rays; }
void  R3DLocalLights::setLampWidth(float v)	{ s_lampWidth = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getLampWidth(void)	{ return s_lampWidth; }
void  R3DLocalLights::setRayTurn(Bool on)	{ s_rayTurn = on; }
Bool  R3DLocalLights::getRayTurn(void)	{ return s_rayTurn; }
void  R3DLocalLights::setSpill(float v)	{ s_spill = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getSpill(void)	{ return s_spill; }
void  R3DLocalLights::setSpillWide(float v)	{ s_spillWide = (v < 1.2f) ? 1.2f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getSpillWide(void)	{ return s_spillWide; }
void  R3DLocalLights::setGlows(float v)	{ s_glows = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getGlows(void)	{ return s_glows; }
void  R3DLocalLights::setGlowReach(float v)	{ s_glowReach = (v < 0.25f) ? 0.25f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getGlowReach(void)	{ return s_glowReach; }
void  R3DLocalLights::setGlowJoin(float v)
{
	const float was = s_glowJoin;
	s_glowJoin = (v < 0.0f) ? 0.0f : ((v > 10.0f) ? 10.0f : v);
	if (s_glowJoin != was)
		s_fixReread = TRUE;
}
float R3DLocalLights::getGlowJoin(void)	{ return s_glowJoin; }
void  R3DLocalLights::setCoronas(float v)	{ s_coronas = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getCoronas(void)	{ return s_coronas; }
void  R3DLocalLights::setCoronaSize(float v)	{ s_coronaSize = (v < 0.25f) ? 0.25f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getCoronaSize(void)	{ return s_coronaSize; }
void  R3DLocalLights::setPools(Bool keep)	{ s_keepPools = keep; }
Bool  R3DLocalLights::getPools(void)	{ return s_keepPools; }
void  R3DLocalLights::setBeamReach(float v)	{ s_beamReach = (v < 1.0f) ? 1.0f : ((v > 3.0f) ? 3.0f : v); }
float R3DLocalLights::getBeamReach(void)	{ return s_beamReach; }
void  R3DLocalLights::setPoolFill(float v)	{ s_poolFill = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getPoolFill(void)	{ return s_poolFill; }
void  R3DLocalLights::setHotspot(float v)	{ s_hotspot = (v < 0.0f) ? 0.0f : ((v > 0.9f) ? 0.9f : v); }
float R3DLocalLights::getHotspot(void)	{ return s_hotspot; }

// Ronin @feature 30/10/2026 DX9: the new lighting carries the 2003 cloud shadows and ground lighting: they are on while it is.
static void lightingTakesLegacyLight(void)
{
	if (s_enabled && TheWritableGlobalData != NULL)
	{
		TheWritableGlobalData->m_useCloudMap = TRUE;
		TheWritableGlobalData->m_useLightMap = TRUE;
	}
}
void R3DLocalLights::setEnabled(Bool on)	{ s_enabled = on; lightingTakesLegacyLight(); }
Bool R3DLocalLights::isEnabled(void)		{ return s_enabled; }
void R3DLocalLights::setEffects(Bool on)	{ s_effects = on; }
Bool R3DLocalLights::isEffects(void)		{ return s_effects; }
void R3DLocalLights::setIntensity(float v)	{ s_intensity = (v < 0.0f) ? 0.0f : ((v > 8.0f) ? 8.0f : v); }
float R3DLocalLights::getIntensity(void)	{ return s_intensity; }
void R3DLocalLights::setOver(float v)		{ s_over = (v < 1.0f) ? 1.0f : ((v > 8.0f) ? 8.0f : v); }
float R3DLocalLights::getOver(void)			{ return s_over; }
void R3DLocalLights::setBoost(float v)		{ s_boost = (v < 0.0f) ? 0.0f : ((v > 8.0f) ? 8.0f : v); }
float R3DLocalLights::getBoost(void)		{ return s_boost; }
void R3DLocalLights::setView(Int v)			{ s_view = (v < 0) ? 0 : ((v == 9) ? 9 : ((v > 6) ? 6 : v)); }	// Ronin @diagnostic 22/10/2026 DX9: 9 = the sun shafts' march
Int  R3DLocalLights::getView(void)			{ return s_view; }
Bool R3DLocalLights::ownsDynamicLights(void)	{ return (s_active && s_effects) ? TRUE : FALSE; }	// Ronin @feature 23/10/2026 DX9: lamps off = the game's
void R3DLocalLights::setRange(float v)		{ s_range = (v < 0.25f) ? 0.25f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getRange(void)		{ return s_range; }
void R3DLocalLights::setSmall(Bool on)		{ s_small = on; }
Bool R3DLocalLights::getSmall(void)			{ return s_small; }
// Ronin @feature 17/10/2026 DX9: the pulse minimum - `lights pulsemin` (units of reach) and `lights pulsefade` (frames).
static float s_pulseMin  = 18.0f;		// value for the testers, 17/10
static Int   s_pulseFade = 11;
void R3DLocalLights::minimumPulse(Real innerRadius, Real *attenuationWidth, UnsignedInt *decayFrames)
{
	if (!s_active || !s_effects || attenuationWidth == NULL || decayFrames == NULL)
		return;
	if (s_pulseMin > 0.0f && innerRadius + *attenuationWidth < s_pulseMin)
		*attenuationWidth = s_pulseMin - innerRadius;
	if (s_pulseFade > 0 && *decayFrames != 0 && *decayFrames < (UnsignedInt)s_pulseFade)
		*decayFrames = (UnsignedInt)s_pulseFade;		// 0 = a pulse that never fades: left so
}
void  R3DLocalLights::setPulseMin(float v)	{ s_pulseMin = (v < 0.0f) ? 0.0f : ((v > 60.0f) ? 60.0f : v); }
float R3DLocalLights::getPulseMin(void)	{ return s_pulseMin; }
void  R3DLocalLights::setPulseFade(float v)	{ s_pulseFade = (v < 0.5f) ? 0 : ((v > 30.0f) ? 30 : (Int)(v + 0.5f)); }
float R3DLocalLights::getPulseFade(void)	{ return (float)s_pulseFade; }

Bool R3DLocalLights::wantsSmallLights(void)	{ return (s_active && s_effects && s_small) ? TRUE : FALSE; }
// Ronin @feature 17/10/2026 DX9: under MSAA the scene depth comes only from the view prepass - asked for while the lights are on.
Bool R3DLocalLights::wantsDepthPrepass(void)	{ return s_enabled ? TRUE : FALSE; }


void R3DLocalLights::getStats(Stats *out)
{
	if (out == NULL)
		return;
	*out = s_shown;
	out->gpuScene  = s_gpuMs[0];
	out->gpuShade  = s_gpuMs[1];
	out->gpuLights = s_gpuMs[2];
}

// Ronin @diagnostic 20/10/2026 DX9: a device reset fails while any query lives: R3DTaa::releaseResources calls this with its own.
void R3DLocalLights::releaseQueries(void)
{
	for (Int i = 0; i < LIGHT_GPU_RING; ++i)
	{
		LightGpuFrame &f = s_gpu[i];
		if (f.disjoint != NULL) { f.disjoint->Release(); f.disjoint = NULL; }
		if (f.freq != NULL)     { f.freq->Release();     f.freq = NULL; }
		for (Int m = 0; m < LIGHT_GPU_MARKS; ++m)
		{
			if (f.t[m] != NULL) { f.t[m]->Release(); f.t[m] = NULL; }
			f.marked[m] = FALSE;
		}
		f.issued = FALSE;
	}
	s_gpuOK    = FALSE;
	s_gpuTried = FALSE;
	s_gpuOpen  = FALSE;
}

static void gpuMark(Int m)
{
	if (!s_gpuOK || !s_gpuOpen)
		return;
	s_gpu[s_gpuSlot].t[m]->Issue(D3DISSUE_END);
	s_gpu[s_gpuSlot].marked[m] = TRUE;
}

static void gpuEase(Int k, Bool have, UINT64 from, UINT64 to, double msPerTick)
{
	if (!have || to < from)
	{
		s_gpuMs[k] = -1.0f;
		return;
	}
	const float ms = (float)((double)(to - from) * msPerTick);
	s_gpuMs[k] = (s_gpuMs[k] < 0.0f) ? ms : (s_gpuMs[k] * 0.9f + ms * 0.1f);
}

// Ronin @diagnostic 20/10/2026 DX9: reads this slot's frame of LIGHT_GPU_RING frames ago, if the GPU finished it; opens it at mark 0.
static void gpuFrameBegin(IDirect3DDevice9 *dev)
{
	if (!s_gpuTried && dev != NULL)
	{
		Bool ok = TRUE;
		for (Int i = 0; i < LIGHT_GPU_RING && ok; ++i)
		{
			LightGpuFrame &f = s_gpu[i];
			ok = SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &f.disjoint)) &&
				 SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &f.freq));
			for (Int m = 0; m < LIGHT_GPU_MARKS && ok; ++m)
				ok = SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &f.t[m]));
		}
		if (!ok)
			R3DLocalLights::releaseQueries();		// no timestamps on this card: the row shows n/a
		s_gpuOK    = ok;
		s_gpuTried = TRUE;
	}
	if (!s_gpuOK)
		return;
	LightGpuFrame &f = s_gpu[s_gpuSlot];
	if (f.issued)
	{
		BOOL   disjoint = TRUE;
		UINT64 freq = 0, t[LIGHT_GPU_MARKS] = { 0, 0, 0, 0, 0 };
		Bool   ready = (f.disjoint->GetData(&disjoint, sizeof(disjoint), 0) == S_OK) &&
					   (f.freq->GetData(&freq, sizeof(freq), 0) == S_OK);
		for (Int m = 0; m < LIGHT_GPU_MARKS && ready; ++m)
			if (f.marked[m])
				ready = (f.t[m]->GetData(&t[m], sizeof(UINT64), 0) == S_OK);
		if (ready && !disjoint && freq > 0)
		{
			const double msPerTick = 1000.0 / (double)freq;
			gpuEase(0, f.marked[0] && f.marked[1], t[0], t[1], msPerTick);
			gpuEase(1, f.marked[2] && f.marked[3], t[2], t[3], msPerTick);
			gpuEase(2, f.marked[1] && f.marked[4], t[1], t[4], msPerTick);
		}
		f.issued = FALSE;
	}
	for (Int m = 0; m < LIGHT_GPU_MARKS; ++m)
		f.marked[m] = FALSE;
	f.disjoint->Issue(D3DISSUE_BEGIN);
	s_gpuOpen = TRUE;
	gpuMark(0);
}

static void gpuFrameEnd(void)
{
	if (!s_gpuOK || !s_gpuOpen)
		return;
	LightGpuFrame &f = s_gpu[s_gpuSlot];
	f.freq->Issue(D3DISSUE_END);
	f.disjoint->Issue(D3DISSUE_END);
	f.issued  = TRUE;
	s_gpuOpen = FALSE;
	s_gpuSlot = (s_gpuSlot + 1) % LIGHT_GPU_RING;
}

// Ronin @feature 10/10/2026 DX9: LIGHTING PART 3, STEP 1 - the sun's shade as the mesh shader applied it, kept per pixel (RigidInstance_ps
// COLOR1), so a lamp lights the surface the sun's shadow hides. Bound round Flush_Single_Rigid's draws, main pass only.
static TextureClass			*s_shadeTex     = NULL;
static UnsignedInt			s_shadeW        = 0;
static UnsignedInt			s_shadeH        = 0;
static Bool					s_shadeMapReuse = FALSE;	// Ronin @bugfix 04/11/2026 DX9: SSAO ran it after the trees: the light pass reads it as it is
static UnsignedInt			s_shadeMapFrame = 0;		// Ronin @feature 21/10/2026 DX9: the s_fixFrame the shade pass last filled it in
static Bool runShadePass(IDirect3DDevice9 *dev, IDirect3DTexture9 *depthTex, const D3DSURFACE_DESC &rd, const CameraClass &camera);
static Bool					s_shadeOpen     = FALSE;	// between beginFrame and the light pass
static Bool					s_shadeBound    = FALSE;
static Bool					s_shadeWritten  = FALSE;	// bound at least once this frame
static const void			*s_shadeBeside  = NULL;	// the target it was first bound beside this frame: compared, never used
static float				s_sunFill       = 1.0f;	// `lights sunfill`
static float				s_sunErase      = 3.0f;	// `lights sunerase`: a lamp restores what the sun's shadow took; gone at strength 1/3
static Bool					s_shadeFrame    = FALSE;	// this frame records the shade (lights on, a scene depth): kept to the next frame's start
static float				s_aoSun         = 1.0f;	// `lights aosun`
static Bool					s_treeShadeFrame = FALSE;	// the trees drew into their own shade record this frame (noteTreeShade)
static float				s_bounce        = 1.0f;	// `lights bounce`
static Int						s_fxLights      = 24;		// `lights fxlights`: the strongest glowing effects drawn as lights
static float				s_sunTime[4]    = { 1.0f, 1.0f, 0.5f, 0.0f };	// `lights sunmorning..sunnight`: how far each time is day
static float				s_sunNow        = 1.0f;	// this frame's, from the time of day (beginFrame)
static float				s_daySun        = 0.2f;	// `lights daysun`: by day a local light keeps 20% where the sun shines
static float				s_dayShade      = 0.5f;	// `lights dayshade`: ... and 50% in a sun shadow
static float				s_fxStrength    = 1.0f;	// `lights fxstrength`
static float				s_fxReach       = 1.0f;	// `lights fxreach`
static Bool					s_fxMerge       = TRUE;	// `lights fxmerge`
static Bool					s_fxFlares      = FALSE;	// `lights fxflares`
static float				s_fxFire        = 40.0f;	// `lights fxfire`
static float				s_fxSmooth      = 0.7f;	// `lights fxsmooth`: the share of last frame's light an effect light keeps, at 30 fps
static UnsignedInt			s_fxClock       = 0;		// timeGetTime of the last pass that eased them (0 = none yet)
static Bool					s_searchLight   = TRUE;	// `lights searchlight`: the police car's 2003 light is its beacons' light
static const Int			SEARCH_LIGHTS   = 32;
static const float			SEARCH_BEACONS  = 6.0f;	// a lamp glow this near it is one of its beacons (2.1 and 2.8 on the car)
static W3DDynamicLight		*s_searchKeys[SEARCH_LIGHTS];	// last pass's search lights: one fades for 5 frames at the car's end
static Int						s_searchCount   = 0;
static const Int			FX_CANDIDATES   = 64;		// the strongest kept while the systems are read
static const float			FX_SIZE_CAP     = 20.0f;	// a particle counts at most this wide: a lens flare's sprite is not its light
static const float			FX_POWER_FULL   = 200.0f;	// colour x size^2 summed: strength sqrt(t / (1 + t)), t = power / this
static Int						s_reflect       = 2;		// `lights reflect`
static float				s_reflectGround = 0.5f;	// `lights reflectground`: half ground, half sky - his pick 18/10
static float				s_fresnel       = 2.0f;	// `lights fresnel`: 2 by his word 18/10 (1 did not read)
static Bool					s_ssr           = TRUE;	// `lights ssr`: the screen-space reflections
static const float			SSR_RUN         = 300.0f;	// a reflection's ray runs this far through last frame's depth
static const float			SSR_THICK       = 10.0f;	// ... and meets a surface it ends this far behind, at most
static const float			GROUND_RAY_MAX  = 400.0f;	// a reflection looking down meets the ground within this far

void R3DLocalLights::beginSunShade(void)
{
	if (!s_shadeOpen || s_shadeBound || (s_sunFill <= 0.0f && s_aoSun <= 0.0f) || ShaderClass::Is_Backface_Culling_Inverted() ||
		TheTerrainShadowPass.inDepthPass || TheTerrainShadowPass.viewDepthPass)
		return;		// not the main pass (the mirror draws with culling inverted), or not wanted
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface9 *rt = NULL;
	if (dev == NULL || FAILED(dev->GetRenderTarget(0, &rt)) || rt == NULL)
		return;
	D3DSURFACE_DESC rd;
	// Ronin @feature 10/10/2026 DX9: a second target must match the first: never under MSAA, and only beside the frame's own target.
	const Bool usable = (SUCCEEDED(rt->GetDesc(&rd)) && rd.MultiSampleType == D3DMULTISAMPLE_NONE &&
						 (s_shadeBeside == NULL || s_shadeBeside == rt)) ? TRUE : FALSE;
	const void *beside = rt;
	rt->Release();
	if (!usable)
		return;
	if (s_shadeTex == NULL || s_shadeW != rd.Width || s_shadeH != rd.Height)
	{
		REF_PTR_RELEASE(s_shadeTex);
		s_shadeW = 0;
		s_shadeH = 0;
		s_shadeTex = NEW_REF(TextureClass, (rd.Width, rd.Height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
		if (s_shadeTex->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_shadeTex);	// a macro with its own if - keep the braces
			return;
		}
		s_shadeW = rd.Width;
		s_shadeH = rd.Height;
	}
	IDirect3DSurface9 *surf = s_shadeTex->Get_D3D_Surface_Level();
	if (surf == NULL)
		return;
	if (s_shadeBeside == NULL)
	{
		dev->ColorFill(surf, NULL, 0);		// 0 = no mesh wrote here: the light pass leaves such a pixel as it was
		s_shadeBeside  = beside;
		s_shadeWritten = TRUE;
	}
	if (SUCCEEDED(dev->SetRenderTarget(1, surf)))
	{
		s_shadeBound = TRUE;
		// Ronin @bugfix 19/10/2026 DX9: all four channels - target 1 keeps the last pass's write mask, and the lights' is green + alpha.
		dev->SetRenderState(D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE |
												 D3DCOLORWRITEENABLE_ALPHA);
	}
	surf->Release();
}

void R3DLocalLights::endSunShade(void)
{
	if (!s_shadeBound)
		return;
	s_shadeBound = FALSE;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev != NULL)
		dev->SetRenderTarget(1, NULL);
}

void  R3DLocalLights::setSunFill(float v)	{ s_sunFill = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getSunFill(void)		{ return s_sunFill; }
void  R3DLocalLights::setSunErase(float v)	{ s_sunErase = (v < 0.0f) ? 0.0f : ((v > 8.0f) ? 8.0f : v); }
float R3DLocalLights::getSunErase(void)		{ return s_sunErase; }

// Ronin @feature 10/10/2026 DX9: what a sun shadow leaves on THIS map: its light without the sun over its light
// with it, on flat ground, per channel (it was 0.45 everywhere). For TerrainShadow_ps (c21) and RigidInstance_ps (c24).
static float				s_mapShadow     = 1.0f;	// `lights mapshadow`

static void publishMapShadow(void)
{
	float rest[3] = { 0.0f, 0.0f, 0.0f };
	float sun[3]  = { 0.0f, 0.0f, 0.0f };
	if (TheGlobalData != NULL)
	{
		for (Int i = 0; i < MAX_GLOBAL_LIGHTS; ++i)
		{
			const GlobalData::TerrainLighting &tl = TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][i];
			const float len = sqrtf(tl.lightPos.x * tl.lightPos.x + tl.lightPos.y * tl.lightPos.y + tl.lightPos.z * tl.lightPos.z);
			float down = (len > 0.0001f) ? (-tl.lightPos.z / len) : 0.0f;
			if (down < 0.0f)
				down = 0.0f;
			const float direct[3] = { tl.diffuse.red * down, tl.diffuse.green * down, tl.diffuse.blue * down };
			float *into = (i == 0) ? sun : rest;		// light 0 is the sun: the one the shadow map is fitted to
			into[0] += direct[0];
			into[1] += direct[1];
			into[2] += direct[2];
			rest[0] += tl.ambient.red;
			rest[1] += tl.ambient.green;
			rest[2] += tl.ambient.blue;
		}
	}
	float *out = TheTerrainShadowPass.shadeColour;
	for (Int c = 0; c < 3; ++c)
	{
		const float total = rest[c] + sun[c];
		float v = (total > 0.001f) ? (rest[c] / total) : 0.45f;
		if (v < 0.1f) v = 0.1f;
		if (v > 1.0f) v = 1.0f;
		out[c] = v;
	}
	out[3] = s_enabled ? s_mapShadow : 0.0f;		// Ronin @feature 23/10/2026 DX9: under the master; off = 0.45 on every map, as before
	DX8InstanceManagerClass::Set_Shadow_Shade(out);
}

void  R3DLocalLights::setMapShadow(float v)	{ s_mapShadow = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getMapShadow(void)	{ return s_mapShadow; }

// Ronin @feature 12/10/2026 DX9: LIGHTING, STEP 3 - SSAO is the shade of the map's AMBIENT. Where the sun reaches, the ambient is only
// the shade's floor over the shade of the pixel's light: SSAO's multiply (SsaoShow_ps) takes that much. Needs the sun's shadow maps.
void R3DLocalLights::bindSunShadeForAO(Bool afterTrees)
{
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL)
		return;
	IDirect3DBaseTexture9 *mesh = NULL;
	IDirect3DBaseTexture9 *ground = NULL;
	IDirect3DSurface9 *rt = NULL;
	D3DSURFACE_DESC rd;
	if (s_aoSun > 0.0f && s_shadeFrame && TheTerrainShadowPass.enabled && SUCCEEDED(dev->GetRenderTarget(0, &rt)) && rt != NULL &&
		SUCCEEDED(rt->GetDesc(&rd)))
	{
		// Ronin @feature 21/10/2026 DX9: the shade pass's record, as the lights read it: every pixel's own shade (glass, trees and
		// infantry too), under MSAA as well. Run here while SSAO has the scene depth out; its stage 0 is put back after.
		const CameraClass *cam = R3DTaa::getSceneCamera();
		IDirect3DTexture9 *depthTex = DX8Wrapper::Peek_Scene_Depth_Texture();
		D3DSURFACE_DESC dd;
		Bool fromMap = FALSE;
		if (cam != NULL && depthTex != NULL && SUCCEEDED(depthTex->GetLevelDesc(0, &dd)) && dd.Width == rd.Width && dd.Height == rd.Height)
		{
			IDirect3DBaseTexture9 *keepTex = NULL;
			dev->GetTexture(0, &keepTex);
			static const D3DSAMPLERSTATETYPE KEEP_SAMP[5] = { D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_ADDRESSU,
															  D3DSAMP_ADDRESSV };
			DWORD keepSamp[5];
			for (Int k = 0; k < 5; ++k)
				dev->GetSamplerState(0, KEEP_SAMP[k], &keepSamp[k]);
			fromMap = runShadePass(dev, depthTex, rd, *cam);
			s_shadeMapReuse = (fromMap && afterTrees) ? TRUE : FALSE;
			dev->SetTexture(0, keepTex);
			if (keepTex != NULL)
				keepTex->Release();
			for (Int k = 0; k < 5; ++k)
				dev->SetSamplerState(0, KEEP_SAMP[k], keepSamp[k]);
		}
		else
			fromMap = (s_shadeMapFrame == s_fixFrame && s_shadeTex != NULL && s_shadeW == rd.Width && s_shadeH == rd.Height) ? TRUE : FALSE;
		if (fromMap)
			mesh = s_shadeTex->Peek_D3D_Texture();
		else if (rd.MultiSampleType == D3DMULTISAMPLE_NONE)		// neither old record is written under MSAA
		{
			if (s_shadeWritten && s_shadeTex != NULL && s_shadeW == rd.Width && s_shadeH == rd.Height)
				mesh = s_shadeTex->Peek_D3D_Texture();
			D3DSURFACE_DESC gd;
			if (TheTerrainShadowPass.accumSurf != NULL && TheTerrainShadowPass.accumCur != NULL &&
				SUCCEEDED(((IDirect3DTexture9 *)TheTerrainShadowPass.accumCur)->GetLevelDesc(0, &gd)) && gd.Width == rd.Width &&
				gd.Height == rd.Height)
				ground = TheTerrainShadowPass.accumCur;
		}
	}
	if (rt != NULL)
		rt->Release();
	// Ronin @feature 12/10/2026 DX9: the floor as both shadow shaders make it (TerrainShadow_ps, RigidInstance_ps): 0.45, or the map's own.
	const float *map = TheTerrainShadowPass.shadeColour;
	float c[2 * 4] = { 0.45f, 0.45f, 0.45f, (mesh != NULL || ground != NULL) ? s_aoSun : 0.0f,
					   (mesh != NULL) ? 1.0f : 0.0f, (ground != NULL) ? 1.0f : 0.0f, 0.0f, 0.0f };
	for (Int ch = 0; ch < 3; ++ch)
	{
		c[ch] += (map[ch] - 0.45f) * map[3];
		if (c[ch] < 0.05f)
			c[ch] = 0.05f;
	}
	dev->SetPixelShaderConstantF(0, c, 2);
	for (DWORD stage = 1; stage <= 2; ++stage)
	{
		dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	dev->SetTexture(1, mesh);
	dev->SetTexture(2, ground);
}

void  R3DLocalLights::setAoSun(float v)	{ s_aoSun = (v < 0.0f) ? 0.0f : ((v > 3.0f) ? 3.0f : v); }
float R3DLocalLights::getAoSun(void)		{ return s_aoSun; }
void  R3DLocalLights::setReflect(float v)	{ s_reflect = (v < 0.5f) ? 0 : ((v < 1.5f) ? 1 : 2); }
float R3DLocalLights::getReflect(void)	{ return (float)s_reflect; }
void  R3DLocalLights::setReflectGround(float v)	{ s_reflectGround = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getReflectGround(void)		{ return s_reflectGround; }
void  R3DLocalLights::setFresnel(float v)	{ s_fresnel = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getFresnel(void)		{ return s_fresnel; }
void  R3DLocalLights::setSsr(Bool on)		{ s_ssr = on; }
Bool  R3DLocalLights::getSsr(void)		{ return s_ssr; }

// Ronin @feature 22/10/2026 DX9: SUN SHAFTS through the cloud gaps - `lights sunshafts` (strength per 100 units of lit air; 0 = off),
// `shaftgaps` (the cloud shadow's brightness that lets the sun through), `shaftlayer` (how high the lit air reaches).
static float					s_sunShafts     = 0.0f;
static float					s_shaftGaps     = 0.965f;
static float					s_shaftLayer    = 150.0f;
static TextureClass				*s_cloudTex     = NULL;		// the ground's cloud shadow texture, for the gaps
static float					s_shaftDust     = 1.0f;		
static const CameraClass		*s_dustCamera   = NULL;		// the light pass's camera, this frame only
static UnsignedInt				s_dustFrame     = 0;
void  R3DLocalLights::setSunShafts(float v)	{ s_sunShafts = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getSunShafts(void)		{ return s_sunShafts; }
void  R3DLocalLights::setShaftGaps(float v)	{ s_shaftGaps = (v < 0.5f) ? 0.5f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getShaftGaps(void)		{ return s_shaftGaps; }
void  R3DLocalLights::setShaftLayer(float v)	{ s_shaftLayer = (v < 20.0f) ? 20.0f : ((v > 400.0f) ? 400.0f : v); }
float R3DLocalLights::getShaftLayer(void)	{ return s_shaftLayer; }
void  R3DLocalLights::setShaftDust(float v)	{ s_shaftDust = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getShaftDust(void)		{ return s_shaftDust; }

// Ronin @feature 12/10/2026 DX9: THE LIGHTS' INI. Every `lights` knob by its name, in the panel's order: one table for the panel
// (set, print) and for DX9Lights.ini, a modder's file. A knob added here is tunable and kept, with nothing else to touch.
#define LIGHT_REAL(text, setter, getter)   { text, FALSE, TRUE, [](float v) { R3DLocalLights::setter(v); }, []() -> float { return R3DLocalLights::getter(); } }
#define LIGHT_SWITCH(text, setter, getter) { text, TRUE, TRUE, [](float v) { R3DLocalLights::setter((v != 0.0f) ? TRUE : FALSE); }, []() -> float { return R3DLocalLights::getter() ? 1.0f : 0.0f; } }
static const R3DLocalLights::Knob s_knobs[] =
{
	LIGHT_REAL("intensity", setIntensity, getIntensity),
	LIGHT_REAL("range", setRange, getRange),
	LIGHT_SWITCH("small", setSmall, getSmall),
	LIGHT_REAL("over", setOver, getOver),
	LIGHT_REAL("boost", setBoost, getBoost),
	{ "view", TRUE, FALSE, [](float v) { R3DLocalLights::setView((Int)v); }, []() -> float { return (float)R3DLocalLights::getView(); } },
	// Ronin @feature 23/10/2026 DX9: the lamps' switch - a player's (Options.ini DX9LightEffects), so never in DX9Lights.ini.
	{ "locallighting", TRUE, FALSE, [](float v) { R3DLocalLights::setEffects((v != 0.0f) ? TRUE : FALSE); }, []() -> float { return R3DLocalLights::isEffects() ? 1.0f : 0.0f; } },
	LIGHT_REAL("beams", setBeams, getBeams),
	LIGHT_REAL("shafts", setShafts, getShafts),
	LIGHT_REAL("rays", setRays, getRays),
	LIGHT_SWITCH("rayturn", setRayTurn, getRayTurn),
	LIGHT_REAL("lampwidth", setLampWidth, getLampWidth),
	LIGHT_REAL("spill", setSpill, getSpill),
	LIGHT_REAL("spillwide", setSpillWide, getSpillWide),
	LIGHT_REAL("glows", setGlows, getGlows),
	LIGHT_REAL("glowreach", setGlowReach, getGlowReach),
	LIGHT_REAL("glowjoin", setGlowJoin, getGlowJoin),
	LIGHT_REAL("coronas", setCoronas, getCoronas),
	LIGHT_REAL("coronasize", setCoronaSize, getCoronaSize),
	LIGHT_SWITCH("pools", setPools, getPools),
	LIGHT_REAL("beamreach", setBeamReach, getBeamReach),
	LIGHT_REAL("poolfill", setPoolFill, getPoolFill),
	LIGHT_REAL("hotspot", setHotspot, getHotspot),
	LIGHT_REAL("sunfill", setSunFill, getSunFill),
	LIGHT_REAL("sunerase", setSunErase, getSunErase),
	LIGHT_REAL("mapshadow", setMapShadow, getMapShadow),
	LIGHT_REAL("aosun", setAoSun, getAoSun),
	// Ronin @feature 24/10/2026 DX9: SSAO's colour bleed (R3DSsao) - the bounce from the occluders, the AO in the surface's colour.
	// aosun and these two are set and shown by `ssao` (sun, bleed, colour); these two are panel only, never in DX9Lights.ini.
	{ "aobleed", FALSE, FALSE, [](float v) { R3DSsao::setBleed(v); }, []() -> float { return R3DSsao::getBleed(); } },
	{ "aocolour", FALSE, FALSE, [](float v) { R3DSsao::setColourAO(v); }, []() -> float { return R3DSsao::getColourAO(); } },
	LIGHT_REAL("reflect", setReflect, getReflect),
	LIGHT_REAL("reflectground", setReflectGround, getReflectGround),
	LIGHT_REAL("fresnel", setFresnel, getFresnel),
	LIGHT_SWITCH("ssr", setSsr, getSsr),
	LIGHT_REAL("bounce", setBounce, getBounce),
	LIGHT_REAL("sunshafts", setSunShafts, getSunShafts),
	LIGHT_REAL("shaftgaps", setShaftGaps, getShaftGaps),
	LIGHT_REAL("shaftlayer", setShaftLayer, getShaftLayer),
	LIGHT_REAL("shaftdust", setShaftDust, getShaftDust),
	{ "sunmorning", FALSE, TRUE, [](float v) { R3DLocalLights::setSunTime(1, v); }, []() -> float { return R3DLocalLights::getSunTime(1); } },
	{ "sunafternoon", FALSE, TRUE, [](float v) { R3DLocalLights::setSunTime(2, v); }, []() -> float { return R3DLocalLights::getSunTime(2); } },
	{ "sunevening", FALSE, TRUE, [](float v) { R3DLocalLights::setSunTime(3, v); }, []() -> float { return R3DLocalLights::getSunTime(3); } },
	{ "sunnight", FALSE, TRUE, [](float v) { R3DLocalLights::setSunTime(4, v); }, []() -> float { return R3DLocalLights::getSunTime(4); } },
	LIGHT_REAL("daysun", setDaySun, getDaySun),
	LIGHT_REAL("dayshade", setDayShade, getDayShade),
	LIGHT_REAL("fxlights", setFxLights, getFxLights),
	LIGHT_REAL("fxstrength", setFxStrength, getFxStrength),
	LIGHT_REAL("fxreach", setFxReach, getFxReach),
	LIGHT_SWITCH("fxmerge", setFxMerge, getFxMerge),
	LIGHT_SWITCH("fxflares", setFxFlares, getFxFlares),
	LIGHT_REAL("pulsemin", setPulseMin, getPulseMin),
	LIGHT_REAL("pulsefade", setPulseFade, getPulseFade),
	LIGHT_SWITCH("searchlight", setSearchLight, getSearchLight),
	LIGHT_REAL("fxfire", setFxFire, getFxFire),
	LIGHT_REAL("fxsmooth", setFxSmooth, getFxSmooth),
	// Ronin @feature 15/10/2026 DX9: the night look and the dark lift live in R3DTaa; listed here so DX9Lights.ini keeps them.
	{ "night", TRUE, TRUE, [](float v) { R3DTaa::setNight((v != 0.0f) ? TRUE : FALSE); }, []() -> float { return R3DTaa::getNight() ? 1.0f : 0.0f; } },
	{ "nighttint", FALSE, TRUE, [](float v) { R3DTaa::setNightTint(v); }, []() -> float { return R3DTaa::getNightTint(); } },
	{ "nightlo", FALSE, TRUE, [](float v) { R3DTaa::setNightLo(v); }, []() -> float { return R3DTaa::getNightLo(); } },
	{ "nighthi", FALSE, TRUE, [](float v) { R3DTaa::setNightHi(v); }, []() -> float { return R3DTaa::getNightHi(); } },
	{ "nightfloor", FALSE, TRUE, [](float v) { R3DTaa::setNightFloor(v); }, []() -> float { return R3DTaa::getNightFloor(); } },
	{ "nightfull", FALSE, TRUE, [](float v) { R3DTaa::setNightFull(v); }, []() -> float { return R3DTaa::getNightFull(); } },
	{ "nightlift", FALSE, TRUE, [](float v) { R3DTaa::setNightLift(v); }, []() -> float { return R3DTaa::getNightLift(); } },
	// Ronin @feature 21/10/2026 DX9: the daytime grade (R3DTaa) - final-pass colour as the night look is, so DX9Lights.ini keeps it too.
	{ "grade", TRUE, TRUE, [](float v) { R3DTaa::setGrade((v != 0.0f) ? TRUE : FALSE); }, []() -> float { return R3DTaa::getGrade() ? 1.0f : 0.0f; } },
	{ "gradewarm", FALSE, TRUE, [](float v) { R3DTaa::setGradeWarm(v); }, []() -> float { return R3DTaa::getGradeWarm(); } },
	{ "gradesat", FALSE, TRUE, [](float v) { R3DTaa::setGradeSat(v); }, []() -> float { return R3DTaa::getGradeSat(); } },
	{ "grademids", FALSE, TRUE, [](float v) { R3DTaa::setGradeMids(v); }, []() -> float { return R3DTaa::getGradeMids(); } },
	// Ronin @feature 22/10/2026 DX9: vibrance and local contrast (R3DTaa), by day - the final pass's colour, kept in DX9Lights.ini.
	{ "vibrance", FALSE, TRUE, [](float v) { R3DTaa::setVibrance(v); }, []() -> float { return R3DTaa::getVibrance(); } },
	{ "localcontrast", FALSE, TRUE, [](float v) { R3DTaa::setLocalContrast(v); }, []() -> float { return R3DTaa::getLocalContrast(); } },
	{ "nightvibrance", FALSE, TRUE, [](float v) { R3DTaa::setNightVibrance(v); }, []() -> float { return R3DTaa::getNightVibrance(); } },
	{ "nightcontrast", FALSE, TRUE, [](float v) { R3DTaa::setNightContrast(v); }, []() -> float { return R3DTaa::getNightContrast(); } },
	// Ronin @feature 21/10/2026 DX9: HDR's look (R3DTaa, `hdr`) - the sun's lift by the time of day, its warmth, the curve's white. Its on /
	// off is the player's: Options.ini DX9HDR.
	{ "hdrmorning", FALSE, TRUE, [](float v) { R3DTaa::setSunGainAt(0, v); }, []() -> float { return R3DTaa::getSunGainAt(0); } },
	{ "hdrafternoon", FALSE, TRUE, [](float v) { R3DTaa::setSunGainAt(1, v); }, []() -> float { return R3DTaa::getSunGainAt(1); } },
	{ "hdrevening", FALSE, TRUE, [](float v) { R3DTaa::setSunGainAt(2, v); }, []() -> float { return R3DTaa::getSunGainAt(2); } },
	{ "hdrnight", FALSE, TRUE, [](float v) { R3DTaa::setSunGainAt(3, v); }, []() -> float { return R3DTaa::getSunGainAt(3); } },
	{ "hdrwarm", FALSE, TRUE, [](float v) { R3DTaa::setSunWarm(v); }, []() -> float { return R3DTaa::getSunWarm(); } },
	{ "hdrwhite", FALSE, TRUE, [](float v) { R3DTaa::setWhite(v); }, []() -> float { return R3DTaa::getWhite(); } },
	{ "hdrspill", FALSE, TRUE, [](float v) { R3DTaa::setSpill(v); }, []() -> float { return R3DTaa::getSpill(); } },	// Ronin @bugfix 02/11/2026 DX9
	LIGHT_SWITCH("lampmaps", setLampMaps, getLampMaps),
	LIGHT_REAL("lampshadows", setLampShadows, getLampShadows),
	LIGHT_REAL("lampbias", setLampBias, getLampBias),
	LIGHT_SWITCH("lampself", setLampSelf, getLampSelf),
	LIGHT_REAL("lampselfnear", setLampSelfNear, getLampSelfNear),
	LIGHT_SWITCH("pointmaps", setPointMaps, getPointMaps),
	LIGHT_SWITCH("pointsides", setPointSides, getPointSides),
};
#undef LIGHT_REAL
#undef LIGHT_SWITCH
static const Int	s_knobCount = (Int)(sizeof(s_knobs) / sizeof(s_knobs[0]));
static float		s_knobDefaults[sizeof(s_knobs) / sizeof(s_knobs[0])];
static Bool			s_knobsStarted = FALSE;

Int R3DLocalLights::knobCount(void)	{ return s_knobCount; }
const R3DLocalLights::Knob *R3DLocalLights::knobAt(Int index)	{ return (index >= 0 && index < s_knobCount) ? &s_knobs[index] : NULL; }
const R3DLocalLights::Knob *R3DLocalLights::findKnob(const char *name)
{
	for (Int k = 0; name != NULL && k < s_knobCount; ++k)
	{
		if (_stricmp(name, s_knobs[k].name) == 0)
			return &s_knobs[k];
	}
	return NULL;
}

// Ronin @feature 12/10/2026 DX9: the file is a modder's, not a player's setting: in the game's own Data\INI, where a loose
// GameData.ini goes - not beside Options.ini.
void R3DLocalLights::iniPath(char *out, Int size)
{
	strncpy(out, "Data\\INI\\DX9Lights.ini", (size_t)size);
	out[size - 1] = 0;
}

// Ronin @feature 12/10/2026 DX9: reads the file over the current values: `name = value` lines, `;` or `#` comments, a missing line
// keeps what the knob has; a switch also takes yes / no. Through the game's file system: a mod's .big can carry it.
Int R3DLocalLights::loadIni(Int *skipped)
{
	if (skipped != NULL)
		*skipped = 0;
	char path[512];
	iniPath(path, (Int)sizeof(path));
	File *file = (TheFileSystem != NULL) ? TheFileSystem->openFile(path, File::READ | File::BINARY) : NULL;
	if (file == NULL)
		return -1;
	static char text[8192];
	const Int size = file->read(text, (Int)sizeof(text) - 1);
	file->close();
	text[(size > 0) ? size : 0] = 0;
	Int set = 0, bad = 0;
	for (char *line = text, *next = NULL; line != NULL && *line != 0; line = next)
	{
		next = strchr(line, '\n');
		if (next != NULL)
			*next++ = 0;
		char *p = line;
		while (*p == ' ' || *p == '\t')
			++p;
		if (*p == ';' || *p == '#' || *p == '[' || *p == '\r' || *p == '\n' || *p == 0)
			continue;		// a comment, a blank line, the [Lights] heading
		char *equals = strchr(p, '=');
		if (equals == NULL)
		{
			++bad;
			continue;
		}
		char *nameEnd = equals;
		while (nameEnd > p && (nameEnd[-1] == ' ' || nameEnd[-1] == '\t'))
			--nameEnd;
		*nameEnd = 0;
		const char *value = equals + 1;
		while (*value == ' ' || *value == '\t')
			++value;
		const Knob *knob = findKnob(p);
		if (knob == NULL || !knob->saved)
		{
			++bad;
			continue;
		}
		knob->set((*value == 'y' || *value == 'Y') ? 1.0f : (float)atof(value));
		++set;
	}
	if (skipped != NULL)
		*skipped = bad;
	return set;
}

Bool R3DLocalLights::saveIni(void)
{
	char path[512];
	iniPath(path, (Int)sizeof(path));
	CreateDirectoryA("Data", NULL);		// a loose file in the game's folder
	CreateDirectoryA("Data\\INI", NULL);
	FILE *file = fopen(path, "wt");
	if (file == NULL)
		return FALSE;
	fprintf(file, "; DX9 local lights: lamps, beams, their shadows - the look of the per-pixel lights. For modders.\n");
	fprintf(file, "; Written by `lights save` in the debug panel; read when the game starts and by `lights load`.\n");
	fprintf(file, "; The names are the panel's `lights` knobs. A missing line keeps the built-in value;\n");
	fprintf(file, "; delete the file to return to the built-in look.\n");
	fprintf(file, "; On / off is not here: Options.ini DX9Lighting = yes | no (all of it, HDR too), DX9LightEffects = yes | no (the lamps).\n");
	fprintf(file, "\n[Lights]\n");
	for (Int k = 0; k < s_knobCount; ++k)
	{
		if (!s_knobs[k].saved)
			continue;
		if (s_knobs[k].whole)
			fprintf(file, "%s = %d\n", s_knobs[k].name, (int)s_knobs[k].get());
		else
			fprintf(file, "%s = %g\n", s_knobs[k].name, s_knobs[k].get());
	}
	const Bool ok = (ferror(file) == 0) ? TRUE : FALSE;
	fclose(file);
	return ok;
}

// Ronin @feature 12/10/2026 DX9: the values the knobs had before the file was read. The debug view stays as it is.
void R3DLocalLights::resetDefaults(void)
{
	for (Int k = 0; s_knobsStarted && k < s_knobCount; ++k)
	{
		if (s_knobs[k].saved)
			s_knobs[k].set(s_knobDefaults[k]);
	}
}

void R3DLocalLights::beginFrame(void)
{
	if (!s_knobsStarted)		// Ronin @feature 12/10/2026 DX9: once: the built-in values are noted, then the file is read over them
	{
		s_knobsStarted = TRUE;
		for (Int k = 0; k < s_knobCount; ++k)
			s_knobDefaults[k] = s_knobs[k].get();
		loadIni(NULL);
		// Ronin @feature 23/10/2026 DX9: on / off is the player's: Options.ini DX9Lighting (all of it) and DX9LightEffects (the lamps).
		// `lights 0|1` and `lights locallighting 0|1` still flip them for a session.
		if (TheGlobalData != NULL)
		{
			s_enabled = TheGlobalData->m_lighting;
			lightingTakesLegacyLight();
			s_effects = TheGlobalData->m_lightEffects;
		}
	}
	gpuFrameEnd();		// Ronin @diagnostic 20/10/2026 DX9: a frame left open (no endFrame) is closed before the next one opens
	if (s_enabled)
		gpuFrameBegin(DX8Wrapper::_Get_D3D_Device8());
	R3DSkyCube::update();		// Ronin @feature 12/10/2026 DX9: the map's sky cube: built again when the map, its time or its sky changed
	// Ronin @feature 12/10/2026 DX9: the sky cube: the reflective meshes mirror it (`lights reflect`). With the lights off, the art's picture.
	DX8InstanceManagerClass::Set_Sky_Reflection(R3DSkyCube::peek(), s_enabled ? (float)s_reflect : 0.0f);
	// Ronin @feature 18/10/2026 DX9: reflections of the ground - the ground map and c12..c15 for ReflectiveRigid_ps: uv, the knobs,
	// the level light and the height. All 0 with the lights off: the reflection as before.
	{
		float c[4 * 4] = { 0.0f };
		float uv[4], amb[3], light[3], lo = 0.0f, span = 0.0f;
		IDirect3DTexture9 *ground = R3DSkyCube::groundMap(uv, amb);
		R3DSkyCube::groundLevel(light, &lo, &span);
		if (s_enabled)
		{
			for (Int k = 0; k < 4; ++k)
				c[k] = uv[k];
			c[4] = (ground != NULL && s_reflect > 0) ? s_reflectGround : 0.0f;
			c[5] = s_fresnel;
			c[8] = light[0]; c[9] = light[1]; c[10] = light[2]; c[11] = span;
			c[12] = lo; c[13] = GROUND_RAY_MAX;
		}
		DX8InstanceManagerClass::Set_Reflection_Ground(s_enabled ? ground : NULL, c);
	}
	// Ronin @feature 18/10/2026 DX9: screen-space reflections - TAA's last frame and c16..c21 for ReflectiveRigid_ps, the frame before
	// and c69..c74 for the change mask. Off with the lights, `lights ssr 0` or `reflect 0`: the sky and ground ones stay.
	{
		IDirect3DTexture9 *prevColour = NULL, *prevDepth = NULL, *olderColour = NULL;
		float c[6 * 4] = { 0.0f };
		float cOlder[6 * 4] = { 0.0f };
		R3DTaa::setLastFrameWanted((s_enabled && s_ssr && s_reflect > 0) ? TRUE : FALSE);	// Ronin @feature 19/10/2026 DX9: TAA or not
		if (s_enabled && s_ssr && s_reflect > 0 && R3DTaa::getLastFrame(&prevColour, &prevDepth, c, &olderColour, cOlder))
		{
			c[22] = SSR_RUN;
			c[23] = SSR_THICK;
			DX8InstanceManagerClass::Set_Screen_Reflection(prevColour, prevDepth, c, olderColour, cOlder);
		}
		else
			DX8InstanceManagerClass::Set_Screen_Reflection(NULL, NULL, NULL, NULL, NULL);
		DX8InstanceManagerClass::Set_Reflect_Mask_Hooks(&R3DTaa::beginReflectMask, &R3DTaa::endReflectMask);
	}
	publishMapShadow();		// Ronin @feature 10/10/2026 DX9: every frame, whether or not the light pass runs
	s_shown = s_stats;
	s_stats.live     = 0;
	s_stats.drawn    = 0;
	s_stats.coverage = 0.0f;
	s_stats.masked   = FALSE;
	s_stats.beams      = 0;
	s_stats.glows      = 0;
	s_stats.pools      = 0;
	s_stats.fx         = 0;
	s_stats.fxCandidates = 0;
	s_stats.fxGlowing  = 0;
	s_stats.casters    = 0;
	s_stats.lampMaps   = 0;
	s_stats.lampDraws  = 0;
	s_stats.lampMost   = 0;
	s_stats.lampTris   = 0;
	s_stats.lampOwn    = 0;
	s_stats.lampPoints = 0;
	s_stats.lampSides  = 0;
	s_stats.lampSkins  = 0;
	s_stats.beamMeshes = 0;
	s_stats.beamModels = s_fixModelCount;
	if (!s_passDone)
		s_memoCount = 0;		// Ronin @bugfix 09/10/2026 DX9: no pass last frame: nothing to compare with
	s_passDone  = FALSE;
	s_ownsDepth = FALSE;

	Bool active = FALSE;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (s_enabled && dev != NULL)
	{
		ensureShaders(dev);
		if (s_quadVS != NULL && s_lightPS != NULL)
		{
			// Ronin @feature 09/10/2026 DX9: SSAO and TAA begin the scene depth before this; begin it here only when neither did.
			// Ronin @feature 17/10/2026 DX9: under MSAA the view prepass fills it - for SSAO, or for the lights alone (wantsDepthPrepass).
			if (R3DSsao::isActive() || R3DTaa::ownsDepth() || R3DSsao::getPrepassTarget() != 0)
				active = TRUE;
			else
			{
				s_ownsDepth = DX8Wrapper::Begin_Scene_Depth() ? TRUE : FALSE;
				active = s_ownsDepth;
			}
		}
	}
	s_active = active;
	s_stats.active    = active;
	s_stats.ownsDepth = s_ownsDepth;

	// Ronin @feature 09/10/2026 DX9: the beams. Meshes rendered from here to the pass add theirs; the shadow passes of the NEXT frame run
	// before its beginFrame and only skip. The hook is set every frame, as TAA's.
	// Ronin @feature 25/10/2026 DX9: `glowjoin` changed - every model's lamps are read again when next drawn, joined the new way.
	if (s_fixReread)
	{
		memset(s_fixModels, 0, sizeof(s_fixModels));
		s_fixBeamCount  = 0;
		s_fixGlowCount  = 0;
		s_fixPoolCount  = 0;
		s_fixModelCount = 0;
		s_fixReread     = FALSE;
	}
	++s_fixFrame;
	s_frameBeamCount = 0;
	s_frameGlowCount = 0;
	s_framePoolCount = 0;
	// Ronin @feature 15/10/2026 DX9: how far this time of day is day, for `daysun` / `dayshade` / `sunerase`. The glows light at every time.
	{
		const Int tod = (TheGlobalData != NULL) ? (Int)TheGlobalData->m_timeOfDay : 4;
		s_sunNow = s_sunTime[(tod >= 1 && tod <= 4) ? (tod - 1) : 3];
	}
	// Ronin @bugfix 15/10/2026 DX9: on at every time of day: the swap keeps the art's old pools and cones hidden.
	s_beamsOn = (active && s_effects && s_beams > 0.0f) ? TRUE : FALSE;		// Ronin @feature 23/10/2026 DX9: lamps off = the art's own
	s_glowsOn = (active && s_effects && s_glows > 0.0f) ? TRUE : FALSE;
	s_coronasOn = (s_glowsOn && s_coronas > 0.0f) ? TRUE : FALSE;
	s_poolsOn = (s_coronasOn && !s_keepPools && s_poolFill > 0.0f) ? TRUE : FALSE;	// the painted pools are hidden and want filling
	s_collect = (s_beamsOn || s_glowsOn || s_poolsOn) ? TRUE : FALSE;
	// Ronin @feature 09/10/2026 DX9: with the lamps' glows drawn by the shader, their textures go clear - and with them the pools
	// of light painted on ground and walls, which the real lights now make.
	swapBeamTextures(dev, s_beamsOn, (s_coronasOn && !s_keepPools) ? TRUE : FALSE);
	MeshClass::Set_Light_Fixture_Note_Func(&R3DLocalLights::noteFixtureMesh);
	// Ronin @feature 10/10/2026 DX9: the sun's shade is recorded from here to the light pass. The hooks are set every frame.
	endSunShade();
	s_shadeOpen    = active;
	s_shadeFrame   = active;
	s_treeShadeFrame = FALSE;
	s_shadeWritten = FALSE;
	s_shadeBeside  = NULL;
	DX8InstanceManagerClass::Set_Sun_Shade_Hooks(&R3DLocalLights::beginSunShade, &R3DLocalLights::endSunShade);
	// Ronin @feature 11/10/2026 DX9: the lamp shadow maps: the frame's casters are listed from here to the light pass.
	s_casterCount  = 0;
	s_casterOpen   = (active && s_effects && s_lampMapsOn && (s_beamsOn || s_pointMaps)) ? TRUE : FALSE;	// point lights want casters without any beam
	// Ronin @feature 11/10/2026 DX9: stage C: what last frame's maps were, before this frame's take the list.
	s_lampPrevCount = s_lampDrawn ? s_lampMapCount : 0;
	for (Int pm = 0; pm < s_lampPrevCount; ++pm)
		s_lampMapsPrev[pm] = s_lampMaps[pm];
	s_lampMapCount = 0;
	s_lampDrawn    = FALSE;
	s_skinUsed     = 0;
	MeshClass::Set_Light_Caster_Note_Func(&R3DLocalLights::noteCaster);
}

void R3DLocalLights::endFrame(void)
{
	s_dustCamera = NULL;		// Ronin @feature 23/10/2026 DX9: the camera was this frame's
	// Ronin @bugfix 19/10/2026 DX9: last frame's picture is borrowed for this frame only. Handed back here, or the sun's shadow map,
	// drawn before the next beginFrame, binds it after a device reset freed it (the MSAA switch: heap corruption in d3d9).
	DX8InstanceManagerClass::Set_Screen_Reflection(NULL, NULL, NULL, NULL, NULL);
	gpuFrameEnd();
	s_shadeOpen = FALSE;
	s_casterOpen = FALSE;
	endSunShade();
	if (!s_ownsDepth)
		return;
	s_ownsDepth = FALSE;
	DX8Wrapper::End_Scene_Depth();
}

// Ronin @feature 09/10/2026 DX9: the screen rectangle of a view-space sphere, in view-plane units at depth 1. Conservative:
// each side is taken at the sphere's nearest and farthest depth. FALSE = wholly behind the camera or past the far plane.
static Bool sphereViewRect(const Vector3 &c, float r, float zNear, float zFar, float *xMin, float *xMax, float *yMin, float *yMax,
						   Bool *wholeView)
{
	const float e = -c.Z;			// distance along the view axis
	if (e + r <= zNear || e - r >= zFar)
		return FALSE;
	*wholeView = (e - r < zNear) ? TRUE : FALSE;	// the camera is inside it, or it cuts the near plane
	if (*wholeView)
		return TRUE;
	const float eNear = e - r;
	const float eFar  = e + r;
	const float xa = (c.X - r) / eNear, xb = (c.X - r) / eFar;
	const float xc = (c.X + r) / eNear, xd = (c.X + r) / eFar;
	const float ya = (c.Y - r) / eNear, yb = (c.Y - r) / eFar;
	const float yc = (c.Y + r) / eNear, yd = (c.Y + r) / eFar;
	*xMin = (xa < xb) ? xa : xb;
	*xMax = (xc > xd) ? xc : xd;
	*yMin = (ya < yb) ? ya : yb;
	*yMax = (yc > yd) ? yc : yd;
	return TRUE;
}

// Ronin @feature 09/10/2026 DX9: a view-space sphere's rectangle in target pixels, clipped to the 3D viewport. FALSE = not on screen.
static Bool spherePixelRect(const Vector3 &c, float r, const ViewMap &vm, float rect[4])
{
	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	Bool wholeView = FALSE;
	if (!sphereViewRect(c, r, vm.zNear, vm.zFar, &xMin, &xMax, &yMin, &yMax, &wholeView))
		return FALSE;
	rect[0] = vm.vx0; rect[1] = vm.vy0; rect[2] = vm.vx1; rect[3] = vm.vy1;
	if (wholeView)
		return TRUE;
	// Ronin @feature 09/10/2026 DX9: view-plane units to target pixels; the screen's Y grows downward, the plane's upward.
	const float px0 = vm.vx0 + ((xMin - vm.planeMinX) / vm.planeW) * (vm.vx1 - vm.vx0);
	const float px1 = vm.vx0 + ((xMax - vm.planeMinX) / vm.planeW) * (vm.vx1 - vm.vx0);
	const float py0 = vm.vy0 + ((vm.planeMaxY - yMax) / vm.planeH) * (vm.vy1 - vm.vy0);
	const float py1 = vm.vy0 + ((vm.planeMaxY - yMin) / vm.planeH) * (vm.vy1 - vm.vy0);
	if (px0 > vm.vx0) rect[0] = floorf(px0);
	if (py0 > vm.vy0) rect[1] = floorf(py0);
	if (px1 < vm.vx1) rect[2] = ceilf(px1);
	if (py1 < vm.vy1) rect[3] = ceilf(py1);
	return (rect[2] > rect[0] && rect[3] > rect[1]) ? TRUE : FALSE;
}

// Ronin @bugfix 09/10/2026 DX9: one draw: the light now and as it was last frame, over the rectangle that holds both spheres.
// FALSE = neither is on screen.
static Bool makeDraw(LightDraw &d, const LightMemo &now, const LightMemo &was, const Matrix3D &cameraTM, const ViewMap &vm)
{
	Vector3 viewNow, viewWas;
	Matrix3D::Inverse_Transform_Vector(cameraTM, now.world, &viewNow);
	Matrix3D::Inverse_Transform_Vector(cameraTM, was.world, &viewWas);
	Vector3 boundNow, boundWas, axisNow, axisWas;
	Matrix3D::Inverse_Transform_Vector(cameraTM, now.boundCentre, &boundNow);
	Matrix3D::Inverse_Transform_Vector(cameraTM, was.boundCentre, &boundWas);
	Matrix3D::Inverse_Rotate_Vector(cameraTM, now.axis, &axisNow);
	Matrix3D::Inverse_Rotate_Vector(cameraTM, was.axis, &axisWas);
	float a[4], b[4];
	const Bool onNow = spherePixelRect(boundNow, now.boundRadius, vm, a);
	const Bool onWas = spherePixelRect(boundWas, was.boundRadius, vm, b);
	if (!onNow && !onWas)
		return FALSE;
	const float *r0 = onNow ? a : b;
	const float *r1 = onWas ? b : a;
	d.x0 = (r0[0] < r1[0]) ? r0[0] : r1[0];
	d.y0 = (r0[1] < r1[1]) ? r0[1] : r1[1];
	d.x1 = (r0[2] > r1[2]) ? r0[2] : r1[2];
	d.y1 = (r0[3] > r1[3]) ? r0[3] : r1[3];
	d.map = -1;
	d.sides = -1;
	d.mapWas = was.map;		// a light that was not there copies this frame's memo, which has none yet

	const LightMemo *src[2]  = { &now, &was };
	const Vector3   *view[2] = { &viewNow, &viewWas };
	const Vector3   *axis[2] = { &axisNow, &axisWas };
	// Ronin @feature 11/10/2026 DX9: `lights rays`: this lamp's own turn of the ray pattern (c13.z), the same for as long as it lives.
	const UnsignedInt seed = ((UnsignedInt)(size_t)now.key >> 4) * 2654435761u + (UnsignedInt)(now.sub + 1) * 40503u;
	d.c[34] = (float)((seed >> 12) & 1023) / 1024.0f;
	d.c[35] = now.lampRadius;
	for (Int s = 0; s < 2; ++s)
	{
		float *x = d.c + 24 + s * 4;
		x[0] = axis[s]->X; x[1] = axis[s]->Y; x[2] = axis[s]->Z; x[3] = src[s]->cosOuter;
		d.c[32 + s] = src[s]->shaft;
		d.c[36 + s * 2] = src[s]->coronaRadius;
		d.c[37 + s * 2] = src[s]->coronaStrength;
		float *c = d.c + s * 12;
		const float span = src[s]->outer - src[s]->inner;
		c[0] = view[s]->X; c[1] = view[s]->Y; c[2] = view[s]->Z; c[3] = 1.0f / ((span > 0.5f) ? span : 0.5f);
		c[4] = src[s]->diffuse[0]; c[5] = src[s]->diffuse[1]; c[6]  = src[s]->diffuse[2]; c[7]  = src[s]->inner;
		c[8] = src[s]->ambient[0]; c[9] = src[s]->ambient[1]; c[10] = src[s]->ambient[2]; c[11] = src[s]->edge;
	}
	return TRUE;
}

// Ronin @feature 12/10/2026 DX9: called by W3DTreeBuffer once it has cleared and bound the trees' shade record for the frame:
// without it the record on that target is an older frame's.
void R3DLocalLights::noteTreeShade(void)	{ s_treeShadeFrame = TRUE; }
void  R3DLocalLights::setBounce(float v)	{ s_bounce = (v < 0.0f) ? 0.0f : ((v > 2.0f) ? 2.0f : v); }
void  R3DLocalLights::setSunTime(Int tod, float v)	{ if (tod >= 1 && tod <= 4) s_sunTime[tod - 1] = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getSunTime(Int tod)			{ return (tod >= 1 && tod <= 4) ? s_sunTime[tod - 1] : 0.0f; }
void  R3DLocalLights::setDaySun(float v)	{ s_daySun = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getDaySun(void)		{ return s_daySun; }
void  R3DLocalLights::setDayShade(float v)	{ s_dayShade = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v); }
float R3DLocalLights::getDayShade(void)	{ return s_dayShade; }
void  R3DLocalLights::setFxLights(float v)	{ s_fxLights = (v < 0.5f) ? 0 : ((v > 64.0f) ? 64 : (Int)(v + 0.5f)); }
float R3DLocalLights::getFxLights(void)		{ return (float)s_fxLights; }
void  R3DLocalLights::setFxStrength(float v)	{ s_fxStrength = (v < 0.0f) ? 0.0f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getFxStrength(void)	{ return s_fxStrength; }
void  R3DLocalLights::setFxReach(float v)	{ s_fxReach = (v < 0.25f) ? 0.25f : ((v > 4.0f) ? 4.0f : v); }
float R3DLocalLights::getFxReach(void)		{ return s_fxReach; }
void  R3DLocalLights::setFxMerge(Bool on)	{ s_fxMerge = on; }
Bool  R3DLocalLights::getFxMerge(void)		{ return s_fxMerge; }
void  R3DLocalLights::setFxFlares(Bool on)	{ s_fxFlares = on; }
Bool  R3DLocalLights::getFxFlares(void)		{ return s_fxFlares; }
void  R3DLocalLights::setFxFire(float v)	{ s_fxFire = (v < 0.0f) ? 0.0f : ((v > 200.0f) ? 200.0f : v); }
float R3DLocalLights::getFxFire(void)		{ return s_fxFire; }
void  R3DLocalLights::setFxSmooth(float v)	{ s_fxSmooth = (v < 0.0f) ? 0.0f : ((v > 0.95f) ? 0.95f : v); }
float R3DLocalLights::getFxSmooth(void)		{ return s_fxSmooth; }
void  R3DLocalLights::setSearchLight(Bool on)	{ s_searchLight = on; }
Bool  R3DLocalLights::getSearchLight(void)	{ return s_searchLight; }
float R3DLocalLights::getBounce(void)		{ return s_bounce; }

// Ronin @feature 12/10/2026 DX9: HDR STAGE 1B - THE SUN LIFT. With a float frame the sun may be brighter than 1: one quad adds the
// frame times (gain - 1) times the sun's share of each pixel, from the two shade records. Before the lamps' copy of the frame.
static Bool sunLiftWanted(const D3DSURFACE_DESC &rd)
{
	float lift[3];
	return (s_view != 1 && s_view != 2 && s_quadVS != NULL && s_lightPS != NULL && rd.Format == D3DFMT_A16B16G16R16F &&
			rd.MultiSampleType == D3DMULTISAMPLE_NONE && s_shadeFrame && TheTerrainShadowPass.enabled && R3DTaa::getSunLift(lift)) ? TRUE : FALSE;
}

// Ronin @feature 21/10/2026 DX9: `fromMap` - the shade pass ran this frame: its record holds every pixel's shade (infantry, glass and
// trees too, which kept none of their own), so it stands in for the meshes', the ground's and the trees' records.
static void drawSunLift(IDirect3DDevice9 *dev, const D3DSURFACE_DESC &rd, Bool fromMap)
{
	float lift[3];
	if (!sunLiftWanted(rd) || !R3DTaa::getSunLift(lift))
		return;
	IDirect3DSurface9 *rt1 = NULL;
	if (SUCCEEDED(dev->GetRenderTarget(1, &rt1)) && rt1 != NULL)
	{
		rt1->Release();		// a second target still bound: not this pass's frame
		return;
	}
	IDirect3DBaseTexture9 *mesh = NULL;
	IDirect3DBaseTexture9 *ground = NULL;
	IDirect3DBaseTexture9 *trees = NULL;
	if (fromMap && s_shadeTex != NULL && s_shadeW == rd.Width && s_shadeH == rd.Height)
		mesh = s_shadeTex->Peek_D3D_Texture();
	else
	{
		if (s_shadeWritten && s_shadeTex != NULL && s_shadeW == rd.Width && s_shadeH == rd.Height)
			mesh = s_shadeTex->Peek_D3D_Texture();
		D3DSURFACE_DESC gd;
		if (TheTerrainShadowPass.accumSurf != NULL && TheTerrainShadowPass.accumCur != NULL &&
			SUCCEEDED(((IDirect3DTexture9 *)TheTerrainShadowPass.accumCur)->GetLevelDesc(0, &gd)) && gd.Width == rd.Width &&
			gd.Height == rd.Height)
			ground = TheTerrainShadowPass.accumCur;
		// Ronin @feature 12/10/2026 DX9: the trees' own record, if they drew into it this frame; else a canopy takes the ground's behind it.
		if (s_treeShadeFrame && TheTerrainShadowPass.treeAccumCur != NULL &&
			SUCCEEDED(((IDirect3DTexture9 *)TheTerrainShadowPass.treeAccumCur)->GetLevelDesc(0, &gd)) && gd.Width == rd.Width &&
			gd.Height == rd.Height)
			trees = TheTerrainShadowPass.treeAccumCur;
	}
	if (mesh == NULL && ground == NULL)
		return;

	// Ronin @feature 12/10/2026 DX9: everything touched is read raw and written back raw, as the light pass does.
	DWORD zEnable, zWrite, alphaBlend, alphaTest, stencil, cull, colorWrite, srcBlend, destBlend;
	dev->GetRenderState(D3DRS_ZENABLE,          &zEnable);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &zWrite);
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphaBlend);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &alphaTest);
	dev->GetRenderState(D3DRS_STENCILENABLE,    &stencil);
	dev->GetRenderState(D3DRS_CULLMODE,         &cull);
	dev->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
	dev->GetRenderState(D3DRS_SRCBLEND,         &srcBlend);
	dev->GetRenderState(D3DRS_DESTBLEND,        &destBlend);
	D3DVIEWPORT9 oldVP;
	const Bool haveVP = SUCCEEDED(dev->GetViewport(&oldVP));
	float saved[7 * 4];		// c0..c6
	const Bool haveConstants = SUCCEEDED(dev->GetPixelShaderConstantF(0, saved, 7));

	dev->SetRenderState(D3DRS_ZENABLE, FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_DESTCOLOR);		// frame x this ...
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);			// ... + frame
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	D3DVIEWPORT9 fullVP;
	fullVP.X = 0; fullVP.Y = 0; fullVP.Width = rd.Width; fullVP.Height = rd.Height; fullVP.MinZ = 0.0f; fullVP.MaxZ = 1.0f;
	dev->SetViewport(&fullVP);

	// Ronin @feature 12/10/2026 DX9: the floor as both shadow shaders make it: 0.45, or the map's own (as bindSunShadeForAO).
	const float *map = TheTerrainShadowPass.shadeColour;
	float c[7 * 4];
	memset(c, 0, sizeof(c));
	c[2] = 6.0f;		// c0.z: the shader's view 6
	c[3] = (trees != NULL) ? 1.0f : 0.0f;
	for (Int ch = 0; ch < 3; ++ch)
	{
		float f = 0.45f + (map[ch] - 0.45f) * map[3];
		c[5 * 4 + ch] = (f < 0.05f) ? 0.05f : f;		// c5.rgb
		c[6 * 4 + ch] = lift[ch];						// c6.rgb
	}
	c[5 * 4 + 3] = (mesh != NULL) ? 1.0f : 0.0f;
	c[6 * 4 + 3] = (ground != NULL) ? 1.0f : 0.0f;
	dev->SetPixelShaderConstantF(0, c, 7);
	static const DWORD stages[3] = { 2, 3, 5 };
	for (Int n = 0; n < 3; ++n)
	{
		const DWORD stage = stages[n];
		dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	dev->SetTexture(2, mesh);
	dev->SetTexture(3, ground);
	dev->SetTexture(5, trees);

	DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DLocalLights::drawSunLift");
	dev->SetVertexShader(s_quadVS);		// AFTER the layout bind - BindLayoutFVF clears the VS
	dev->SetPixelShader(s_lightPS);
	const float w = (float)rd.Width, h = (float)rd.Height;
	const float l = (-0.5f / w) * 2.0f - 1.0f, r = ((w - 0.5f) / w) * 2.0f - 1.0f;		// half a pixel: the UVs stay exact
	const float t = 1.0f - (-0.5f / h) * 2.0f, b = 1.0f - ((h - 0.5f) / h) * 2.0f;
	struct LiftVertex { float x, y, z; float u, v; };
	const LiftVertex q[4] = { { l, t, 0.0f, 0.0f, 0.0f }, { r, t, 0.0f, 1.0f, 0.0f }, { l, b, 0.0f, 0.0f, 1.0f }, { r, b, 0.0f, 1.0f, 1.0f } };
	dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(LiftVertex));

	dev->SetTexture(2, NULL);
	dev->SetTexture(3, NULL);
	dev->SetTexture(5, NULL);
	if (haveVP)
		dev->SetViewport(&oldVP);
	dev->SetRenderState(D3DRS_ZENABLE,          zEnable);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     zWrite);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, alphaBlend);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  alphaTest);
	dev->SetRenderState(D3DRS_STENCILENABLE,    stencil);
	dev->SetRenderState(D3DRS_CULLMODE,         cull);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, colorWrite);
	dev->SetRenderState(D3DRS_SRCBLEND,         srcBlend);
	dev->SetRenderState(D3DRS_DESTBLEND,        destBlend);
	if (haveConstants)
		dev->SetPixelShaderConstantF(0, saved, 7);
	dev->SetVertexShader(NULL);
	dev->SetPixelShader(NULL);
	DX8Wrapper::Invalidate_Cached_Render_States();
}

// Ronin @feature 20/10/2026 DX9: view space -> each cascade's clip - the map's matrix (world -> clip, transposed) x the camera's -
// then the maps' bias and texel, towards the sun, the shade's colour (c84..c95). TRUE when there are two maps.
static Bool sunMapConstants(const CameraClass &camera, float c84[12 * 4])
{
	const TerrainShadowPassState &sp = TheTerrainShadowPass;
	const Matrix3D &tm = camera.Get_Transform();
	const float cam[4][4] = { { tm[0][0], tm[0][1], tm[0][2], tm[0][3] }, { tm[1][0], tm[1][1], tm[1][2], tm[1][3] },
							  { tm[2][0], tm[2][1], tm[2][2], tm[2][3] }, { 0.0f, 0.0f, 0.0f, 1.0f } };
	const Bool twoMaps = (sp.cascadeCount > 1.5f && sp.shadowTexFar != NULL) ? TRUE : FALSE;
	for (Int m = 0; m < 2; ++m)
	{
		const float *lt = (m == 1 && twoMaps) ? sp.lightViewProjFarT : sp.lightViewProjT;
		for (Int i = 0; i < 4; ++i)
			for (Int j = 0; j < 4; ++j)
				c84[m * 16 + i * 4 + j] = lt[i * 4 + 0] * cam[0][j] + lt[i * 4 + 1] * cam[1][j] + lt[i * 4 + 2] * cam[2][j] +
										   lt[i * 4 + 3] * cam[3][j];
	}
	c84[32] = sp.depthBias;
	c84[33] = sp.texelOffset;
	c84[34] = sp.texelWorldSize;
	c84[35] = twoMaps ? 2.0f : 1.0f;
	c84[36] = twoMaps ? sp.depthBiasFar : sp.depthBias;
	c84[37] = twoMaps ? sp.texelWorldSizeFar : sp.texelWorldSize;
	c84[38] = 0.0f;
	c84[39] = 0.0f;
	for (Int j = 0; j < 3; ++j)		// towards the sun, the camera's rotation undone
		c84[40 + j] = -(cam[0][j] * sp.lightTravelDir[0] + cam[1][j] * sp.lightTravelDir[1] + cam[2][j] * sp.lightTravelDir[2]);
	c84[43] = 0.0f;
	for (Int k = 0; k < 4; ++k)
		c84[44 + k] = sp.shadeColour[k];
	return twoMaps;
}

// Ronin @feature 20/10/2026 DX9: THE SUN'S SHADE, in every mode. LocalLight_ps view 8 works it out per pixel from the scene depth and the
// sun's map, into the meshes' record: MSAA has none, and theirs leaves gaps (glass, trees) where the ground behind was read.
static Bool shadeFromSunMap(IDirect3DDevice9 *dev, IDirect3DTexture9 *depthTex, const D3DSURFACE_DESC &rd, const CameraClass &camera,
							const float c0to3[16])
{
	const TerrainShadowPassState &sp = TheTerrainShadowPass;
	if (!sp.active || sp.shadowTex == NULL || depthTex == NULL || s_lightPS == NULL || s_quadVS == NULL)
		return FALSE;
	if (s_shadeTex == NULL || s_shadeW != rd.Width || s_shadeH != rd.Height)
	{
		REF_PTR_RELEASE(s_shadeTex);
		s_shadeW = 0;
		s_shadeH = 0;
		s_shadeTex = NEW_REF(TextureClass, (rd.Width, rd.Height, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1, TextureClass::POOL_DEFAULT, true));
		if (s_shadeTex->Peek_D3D_Base_Texture() == NULL)
		{
			REF_PTR_RELEASE(s_shadeTex);	// a macro with its own if - keep the braces
			return FALSE;
		}
		s_shadeW = rd.Width;
		s_shadeH = rd.Height;
	}
	float c84[12 * 4];
	const Bool twoMaps = sunMapConstants(camera, c84);

	IDirect3DSurface9 *surf = s_shadeTex->Get_D3D_Surface_Level();
	IDirect3DSurface9 *oldRT = NULL, *oldDS = NULL;
	if (surf == NULL || FAILED(dev->GetRenderTarget(0, &oldRT)) || oldRT == NULL)
	{
		if (surf != NULL)
			surf->Release();
		return FALSE;
	}
	D3DVIEWPORT9 oldVp;
	const Bool haveOldVp = SUCCEEDED(dev->GetViewport(&oldVp)) ? TRUE : FALSE;
	dev->GetDepthStencilSurface(&oldDS);
	dev->SetDepthStencilSurface(NULL);		// the frame's is multisampled; this target is not
	IDirect3DSurface9 *oldRT1 = NULL;
	if (SUCCEEDED(dev->GetRenderTarget(1, &oldRT1)) && oldRT1 != NULL)
		dev->SetRenderTarget(1, NULL);		// Ronin @feature 20/10/2026 DX9: TAA's light mask, bound by now - not this draw's to write
	const Bool ok = SUCCEEDED(dev->SetRenderTarget(0, surf)) ? TRUE : FALSE;
	if (ok)
	{
		DWORD oldBlend = 0, oldTest = 0, oldWrite = 0;
		dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldBlend);
		dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &oldTest);
		dev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldWrite);
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		dev->SetRenderState(D3DRS_ALPHATESTENABLE,  FALSE);
		// Ronin @bugfix 25/10/2026 DX9: its own depth, cull, stencil, scissor and clip states - it ran on the last draw's, and from some
		// angles those hid the whole quad (0.03 ms, every shade unwritten: the palace's bright/dim flip).
		DWORD oldZ = 0, oldZWrite = 0, oldCull = 0, oldStencil = 0, oldScissor = 0, oldClip = 0;
		dev->GetRenderState(D3DRS_ZENABLE,           &oldZ);
		dev->GetRenderState(D3DRS_ZWRITEENABLE,      &oldZWrite);
		dev->GetRenderState(D3DRS_CULLMODE,          &oldCull);
		dev->GetRenderState(D3DRS_STENCILENABLE,     &oldStencil);
		dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);
		dev->GetRenderState(D3DRS_CLIPPLANEENABLE,   &oldClip);
		dev->SetRenderState(D3DRS_ZENABLE,           D3DZB_FALSE);
		dev->SetRenderState(D3DRS_ZWRITEENABLE,      FALSE);
		dev->SetRenderState(D3DRS_CULLMODE,          D3DCULL_NONE);
		dev->SetRenderState(D3DRS_STENCILENABLE,     FALSE);
		dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		dev->SetRenderState(D3DRS_CLIPPLANEENABLE,   0);
		dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE |
													D3DCOLORWRITEENABLE_ALPHA);
		DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DLocalLights::shadeFromSunMap");
		dev->SetVertexShader(s_quadVS);		// after the layout bind - BindLayoutFVF clears the VS
		dev->SetPixelShader(s_lightPS);
		// Ronin @feature 21/10/2026 DX9: c0..c3 and c15 put back after - this runs before the light pass keeps what it restores.
		float keep0[4 * 4], keep15[4];
		const Bool kept = (SUCCEEDED(dev->GetPixelShaderConstantF(0, keep0, 4)) && SUCCEEDED(dev->GetPixelShaderConstantF(15, keep15, 1))) ? TRUE : FALSE;
		dev->SetPixelShaderConstantF(0, c0to3, 4);
		// Ronin @feature 20/10/2026 DX9: TAA's shift this frame, as the lights take it: without it a rebuilt point sits ~0.12 off its surface.
		float jitterX = 0.0f, jitterY = 0.0f;
		R3DTaa::getJitterNDC(&jitterX, &jitterY);
		const float jitter[4] = { -0.5f * jitterX, 0.5f * jitterY, 0.0f, 0.0f };
		dev->SetPixelShaderConstantF(15, jitter, 1);
		dev->SetPixelShaderConstantF(84, c84, 12);
		dev->SetTexture(0, depthTex);
		dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		dev->SetTexture(8, sp.shadowTex);
		dev->SetTexture(9, twoMaps ? sp.shadowTexFar : sp.shadowTex);
		for (DWORD st = 8; st <= 9; ++st)
		{
			dev->SetSamplerState(st, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);		// a depth texture: the hardware's compare
			dev->SetSamplerState(st, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			dev->SetSamplerState(st, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			dev->SetSamplerState(st, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			dev->SetSamplerState(st, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		}
		const float targetW = (float)rd.Width, targetH = (float)rd.Height;
		const float l = (-0.5f / targetW) * 2.0f - 1.0f, r = ((targetW - 0.5f) / targetW) * 2.0f - 1.0f;
		const float t = 1.0f - (-0.5f / targetH) * 2.0f, b = 1.0f - ((targetH - 0.5f) / targetH) * 2.0f;
		struct ShadeVertex { float x, y, z; float u, v; };
		const ShadeVertex q[4] = { { l, t, 0.0f, 0.0f, 0.0f }, { r, t, 0.0f, 1.0f, 0.0f }, { l, b, 0.0f, 0.0f, 1.0f }, { r, b, 0.0f, 1.0f, 1.0f } };
		gpuMark(2);
		dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(ShadeVertex));
		gpuMark(3);
		dev->SetTexture(8, NULL);
		dev->SetTexture(9, NULL);
		dev->SetTexture(0, NULL);		// Ronin @feature 21/10/2026 DX9: the scene depth - the depth buffer again once resumed
		DX8Wrapper::Invalidate_Texture_State(0, 1);
		dev->SetVertexShader(NULL);
		dev->SetPixelShader(NULL);
		if (kept)
		{
			dev->SetPixelShaderConstantF(0, keep0, 4);
			dev->SetPixelShaderConstantF(15, keep15, 1);
		}
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldBlend);
		dev->SetRenderState(D3DRS_ALPHATESTENABLE,  oldTest);
		dev->SetRenderState(D3DRS_COLORWRITEENABLE, oldWrite);
		dev->SetRenderState(D3DRS_ZENABLE,           oldZ);
		dev->SetRenderState(D3DRS_ZWRITEENABLE,      oldZWrite);
		dev->SetRenderState(D3DRS_CULLMODE,          oldCull);
		dev->SetRenderState(D3DRS_STENCILENABLE,     oldStencil);
		dev->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
		dev->SetRenderState(D3DRS_CLIPPLANEENABLE,   oldClip);
	}
	dev->SetRenderTarget(0, oldRT);
	if (oldRT1 != NULL)
	{
		dev->SetRenderTarget(1, oldRT1);
		oldRT1->Release();
	}
	dev->SetDepthStencilSurface(oldDS);
	if (haveOldVp)
		dev->SetViewport(&oldVp);
	if (oldDS != NULL)
		oldDS->Release();
	oldRT->Release();
	surf->Release();
	return ok;
}

// Ronin @feature 21/10/2026 DX9: the shade pass from the camera's own view numbers - for the light pass, and for SSAO's `aosun`.
static Bool runShadePass(IDirect3DDevice9 *dev, IDirect3DTexture9 *depthTex, const D3DSURFACE_DESC &rd, const CameraClass &camera)
{
	float zNear = 1.0f;
	float zFar  = 1000.0f;
	camera.Get_Clip_Planes(zNear, zFar);
	Vector2 planeMin, planeMax, vpMin, vpMax;
	camera.Get_View_Plane(planeMin, planeMax);
	camera.Get_Viewport(vpMin, vpMax);
	const float c0to3[16] = { zNear, zFar, 8.0f, s_over, planeMin.X, planeMax.Y, planeMax.X - planeMin.X, planeMax.Y - planeMin.Y,
							  vpMin.X, vpMin.Y, vpMax.X - vpMin.X, vpMax.Y - vpMin.Y, 1.0f / (float)rd.Width, 1.0f / (float)rd.Height,
							  0.0f, 0.0f };
	const Bool ok = shadeFromSunMap(dev, depthTex, rd, camera, c0to3);
	s_shadeMapFrame = ok ? s_fixFrame : 0;
	return ok;
}

// Ronin @feature 22/10/2026 DX9: SUN SHAFTS (LocalLight_ps views 9 and 10): each view ray's air up to `shaftlayer` above its surface, lit
// where the sun gets through a gap in the cloud shadows and past the casters. View 9 shows the lit share alone; view 10 adds the beams.
static TextureClass *cloudTexture(void)
{
	if (s_cloudTex == NULL)
		s_cloudTex = WW3DAssetManager::Get_Instance()->Get_Texture("TSCloudMed.tga");		// the ground's own (TerrainTex.cpp)
	if (s_cloudTex != NULL && !s_cloudTex->Is_Initialized())
		s_cloudTex->Init();
	return (s_cloudTex != NULL && !s_cloudTex->Is_Missing_Texture() && s_cloudTex->Peek_D3D_Texture() != NULL) ? s_cloudTex : NULL;
}

static Bool sunShaftsWanted(void)
{
	// Ronin @feature 22/10/2026 DX9: by day, under the cloud shadows - no clouds, no gaps to shine through.
	return (s_sunShafts > 0.0f && s_view == 0 && TheTerrainShadowPass.active && TheTerrainShadowPass.shadowTex != NULL &&
			TheGlobalData != NULL && TheGlobalData->m_timeOfDay != TIME_OF_DAY_NIGHT && TheGlobalData->m_useCloudMap &&
			cloudTexture() != NULL) ? TRUE : FALSE;
}

// Ronin @feature 22/10/2026 DX9: the beams' two half-size buffers (16-bit float: the lit air, the depth). A device reset fails while
// they live: R3DTaa::releaseResources lets them go, and the next shafts frame makes them again.
static IDirect3DTexture9			*s_shaftBuf[2]  = { NULL, NULL };
static UnsignedInt				s_shaftBufW     = 0;
static UnsignedInt				s_shaftBufH     = 0;

void R3DLocalLights::releaseShaftBuffers(void)
{
	for (Int i = 0; i < 2; ++i)
	{
		if (s_shaftBuf[i] != NULL)
		{
			s_shaftBuf[i]->Release();
			s_shaftBuf[i] = NULL;
		}
	}
	s_shaftBufW = 0;
	s_shaftBufH = 0;
}

static Bool ensureShaftBuffers(IDirect3DDevice9 *dev, UnsignedInt w, UnsignedInt h)
{
	if (s_shaftBuf[0] != NULL && s_shaftBuf[1] != NULL && s_shaftBufW == w && s_shaftBufH == h)
		return TRUE;
	R3DLocalLights::releaseShaftBuffers();
	for (Int i = 0; i < 2; ++i)
	{
		if (FAILED(dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &s_shaftBuf[i], NULL)))
		{
			s_shaftBuf[i] = NULL;
			R3DLocalLights::releaseShaftBuffers();
			return FALSE;
		}
	}
	s_shaftBufW = w;
	s_shaftBufH = h;
	return TRUE;
}

// Ronin @feature 22/10/2026 DX9: a quad over a whole target, half a pixel over, so its UVs land on texel centres.
static void shaftQuad(IDirect3DDevice9 *dev, UnsignedInt width, UnsignedInt height)
{
	const float w = (float)width, h = (float)height;
	const float l = (-0.5f / w) * 2.0f - 1.0f, r = ((w - 0.5f) / w) * 2.0f - 1.0f;
	const float t = 1.0f - (-0.5f / h) * 2.0f, b = 1.0f - ((h - 0.5f) / h) * 2.0f;
	struct ShaftVertex { float x, y, z; float u, v; };
	const ShaftVertex q[4] = { { l, t, 0.0f, 0.0f, 0.0f }, { r, t, 0.0f, 1.0f, 0.0f }, { l, b, 0.0f, 0.0f, 1.0f }, { r, b, 0.0f, 1.0f, 1.0f } };
	dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(ShaftVertex));
}

static void drawSunShafts(IDirect3DDevice9 *dev, IDirect3DTexture9 *depthTex, const D3DSURFACE_DESC &rd, const CameraClass &camera, Int view)
{
	const TerrainShadowPassState &sp = TheTerrainShadowPass;
	if (!sp.active || sp.shadowTex == NULL || depthTex == NULL || s_lightPS == NULL || s_quadVS == NULL || TheGlobalData == NULL)
		return;
	float zNear = 1.0f;
	float zFar  = 1000.0f;
	camera.Get_Clip_Planes(zNear, zFar);
	Vector2 planeMin, planeMax, vpMin, vpMax;
	camera.Get_View_Plane(planeMin, planeMax);
	camera.Get_Viewport(vpMin, vpMax);
	float c0to3[16] = { zNear, zFar, (float)view, s_over, planeMin.X, planeMax.Y, planeMax.X - planeMin.X, planeMax.Y - planeMin.Y,
						vpMin.X, vpMin.Y, vpMax.X - vpMin.X, vpMax.Y - vpMin.Y, 1.0f / (float)rd.Width, 1.0f / (float)rd.Height,
						0.0f, 0.0f };
	float jitterX = 0.0f, jitterY = 0.0f;
	R3DTaa::getJitterNDC(&jitterX, &jitterY);
	// Ronin @feature 22/10/2026 DX9: the step starts move on each frame only for TAA to average; without it they stay put.
	const float c15[4] = { -0.5f * jitterX, 0.5f * jitterY, 0.0f, R3DTaa::isActive() ? (float)(s_fixFrame & 63) : 0.0f };
	Vector3 worldZ;
	Matrix3D::Inverse_Rotate_Vector(camera.Get_Transform(), Vector3(0.0f, 0.0f, 1.0f), &worldZ);
	const float c32[4] = { worldZ.X, worldZ.Y, worldZ.Z, s_shaftLayer };
	// Ronin @feature 22/10/2026 DX9: the map's sun on objects and the strength; the gaps: the cut, clouds on, the cloud map's mip (one
	// down - the full size shows its compression blocks); its scale and drift, the ground's; view space -> world; the sun's travel.
	const GlobalData::TerrainLighting &sun = TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][0];
	TextureClass *clouds = cloudTexture();
	const Bool cloudsOn = (clouds != NULL && TheGlobalData->m_useCloudMap) ? TRUE : FALSE;
	float cloudScale = 0.0f, cloudX = 0.0f, cloudY = 0.0f;
	W3DShaderManager::getCloudMapState(&cloudScale, &cloudX, &cloudY);
	const Matrix3D &tm = camera.Get_Transform();
	const float c34[7 * 4] = { sun.diffuse.red, sun.diffuse.green, sun.diffuse.blue, s_sunShafts,
							   s_shaftGaps, cloudsOn ? 1.0f : 0.0f, 1.0f, s_shaftDust,
							   cloudScale, cloudX, cloudY, (float)(timeGetTime() % 3600000u) * 0.001f,
							   tm[0][0], tm[0][1], tm[0][2], tm[0][3],
							   tm[1][0], tm[1][1], tm[1][2], tm[1][3],
							   tm[2][0], tm[2][1], tm[2][2], tm[2][3],
							   sp.lightTravelDir[0], sp.lightTravelDir[1], sp.lightTravelDir[2], 0.0f };
	float c84[12 * 4];
	const Bool twoMaps = sunMapConstants(camera, c84);

	DWORD zEnable, zWrite, alphaBlend, alphaTest, stencil, cull, colorWrite, srcBlend, destBlend;
	dev->GetRenderState(D3DRS_ZENABLE,          &zEnable);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &zWrite);
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphaBlend);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &alphaTest);
	dev->GetRenderState(D3DRS_STENCILENABLE,    &stencil);
	dev->GetRenderState(D3DRS_CULLMODE,         &cull);
	dev->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
	dev->GetRenderState(D3DRS_SRCBLEND,         &srcBlend);
	dev->GetRenderState(D3DRS_DESTBLEND,        &destBlend);
	D3DVIEWPORT9 oldVP;
	const Bool haveVP = SUCCEEDED(dev->GetViewport(&oldVP));
	float keep0[4 * 4], keep15[4], keep32[4], keep34[8 * 4];
	const Bool kept = (SUCCEEDED(dev->GetPixelShaderConstantF(0, keep0, 4)) && SUCCEEDED(dev->GetPixelShaderConstantF(15, keep15, 1)) &&
					   SUCCEEDED(dev->GetPixelShaderConstantF(32, keep32, 1)) && SUCCEEDED(dev->GetPixelShaderConstantF(34, keep34, 8))) ? TRUE : FALSE;
	dev->SetRenderState(D3DRS_ZENABLE, FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	D3DVIEWPORT9 fullVP;
	fullVP.X = 0; fullVP.Y = 0; fullVP.Width = rd.Width; fullVP.Height = rd.Height; fullVP.MinZ = 0.0f; fullVP.MaxZ = 1.0f;

	dev->SetPixelShaderConstantF(0, c0to3, 4);
	dev->SetPixelShaderConstantF(15, c15, 1);
	dev->SetPixelShaderConstantF(32, c32, 1);
	dev->SetPixelShaderConstantF(34, c34, 7);
	dev->SetPixelShaderConstantF(84, c84, 12);
	dev->SetTexture(10, cloudsOn ? clouds->Peek_D3D_Texture() : NULL);
	dev->SetSamplerState(10, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(10, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(10, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(10, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	dev->SetSamplerState(10, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	dev->SetTexture(0, depthTex);
	dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetTexture(9, twoMaps ? sp.shadowTexFar : sp.shadowTex);
	dev->SetSamplerState(9, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);		// a depth texture: the hardware's compare
	dev->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	dev->SetSamplerState(9, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	dev->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(11, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(11, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	dev->SetSamplerState(11, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	dev->SetSamplerState(11, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	dev->SetSamplerState(11, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DLocalLights::drawSunShafts");
	dev->SetVertexShader(s_quadVS);		// after the layout bind - BindLayoutFVF clears the VS
	dev->SetPixelShader(s_lightPS);
	if (view == 10)
	{
		// Ronin @feature 22/10/2026 DX9: march (view 10) and blur (view 11) at half size, the frame's targets and depth put aside; then
		// the beams added to the frame at full size (view 12).
		const UnsignedInt hw = (rd.Width + 1) / 2, hh = (rd.Height + 1) / 2;
		IDirect3DSurface9 *frameRT = NULL, *frameRT1 = NULL, *frameDS = NULL;
		if (ensureShaftBuffers(dev, hw, hh) && SUCCEEDED(dev->GetRenderTarget(0, &frameRT)) && frameRT != NULL)
		{
			if (SUCCEEDED(dev->GetRenderTarget(1, &frameRT1)) && frameRT1 != NULL)
				dev->SetRenderTarget(1, NULL);
			dev->GetDepthStencilSurface(&frameDS);
			dev->SetDepthStencilSurface(NULL);		// the frame's may be multisampled, and is larger
			const float c41[4] = { 1.0f / (float)hw, 1.0f / (float)hh, 1.0f / (float)rd.Width, 1.0f / (float)rd.Height };
			dev->SetPixelShaderConstantF(41, c41, 1);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE |
														D3DCOLORWRITEENABLE_ALPHA);
			D3DVIEWPORT9 halfVP;
			halfVP.X = 0; halfVP.Y = 0; halfVP.Width = hw; halfVP.Height = hh; halfVP.MinZ = 0.0f; halfVP.MaxZ = 1.0f;
			Bool ok = TRUE;
			for (Int pass = 0; pass < 2 && ok; ++pass)
			{
				IDirect3DSurface9 *surf = NULL;
				ok = (SUCCEEDED(s_shaftBuf[pass]->GetSurfaceLevel(0, &surf)) && surf != NULL && SUCCEEDED(dev->SetRenderTarget(0, surf))) ? TRUE : FALSE;
				if (surf != NULL)
					surf->Release();
				if (!ok)
					break;
				dev->SetViewport(&halfVP);
				c0to3[2] = (pass == 0) ? 10.0f : 11.0f;
				dev->SetPixelShaderConstantF(0, c0to3, 1);
				dev->SetTexture(11, (pass == 0) ? NULL : s_shaftBuf[0]);
				shaftQuad(dev, hw, hh);
			}
			dev->SetTexture(11, NULL);
			dev->SetRenderTarget(0, frameRT);
			if (frameRT1 != NULL)
			{
				dev->SetRenderTarget(1, frameRT1);
				frameRT1->Release();
			}
			dev->SetDepthStencilSurface(frameDS);
			if (frameDS != NULL)
				frameDS->Release();
			frameRT->Release();
			if (ok)
			{
				dev->SetViewport(&fullVP);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);		// the beams' light, added to the frame
				dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
				c0to3[2] = 12.0f;
				dev->SetPixelShaderConstantF(0, c0to3, 1);
				dev->SetTexture(11, s_shaftBuf[1]);
				shaftQuad(dev, rd.Width, rd.Height);
				dev->SetTexture(11, NULL);
			}
		}
	}
	else
	{
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, (view == 13) ? TRUE : FALSE);		// view 9 replaces the frame; 13's dust is added
		dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		dev->SetViewport(&fullVP);
		shaftQuad(dev, rd.Width, rd.Height);
	}

	dev->SetTexture(10, NULL);
	dev->SetTexture(9, NULL);
	dev->SetTexture(0, NULL);
	DX8Wrapper::Invalidate_Texture_State(0, 1);
	dev->SetVertexShader(NULL);
	dev->SetPixelShader(NULL);
	if (kept)
	{
		dev->SetPixelShaderConstantF(0, keep0, 4);
		dev->SetPixelShaderConstantF(15, keep15, 1);
		dev->SetPixelShaderConstantF(32, keep32, 1);
		dev->SetPixelShaderConstantF(34, keep34, 8);
	}
	if (haveVP)
		dev->SetViewport(&oldVP);
	dev->SetRenderState(D3DRS_ZENABLE,          zEnable);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     zWrite);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, alphaBlend);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  alphaTest);
	dev->SetRenderState(D3DRS_STENCILENABLE,    stencil);
	dev->SetRenderState(D3DRS_CULLMODE,         cull);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, colorWrite);
	dev->SetRenderState(D3DRS_SRCBLEND,         srcBlend);
	dev->SetRenderState(D3DRS_DESTBLEND,        destBlend);
}

// Ronin @feature 23/10/2026 DX9: DUST IN THE SHAFTS (`lights shaftdust`), drawn after TAA's copy of the frame before the effects, so its
// auto-reactive mask treats a mote as it treats a snowflake. Once a frame, with the light pass's camera; full size, added.
void R3DLocalLights::drawShaftDust(void)
{
	if (s_shaftDust <= 0.0f || s_dustCamera == NULL || s_dustFrame == s_fixFrame || !sunShaftsWanted())
		return;
	s_dustFrame = s_fixFrame;
	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface9 *rt = NULL, *rt1 = NULL;
	if (dev == NULL || FAILED(dev->GetRenderTarget(0, &rt)) || rt == NULL)
		return;
	D3DSURFACE_DESC rd;
	const Bool rtOK = SUCCEEDED(rt->GetDesc(&rd)) ? TRUE : FALSE;
	rt->Release();
	if (SUCCEEDED(dev->GetRenderTarget(1, &rt1)) && rt1 != NULL)
	{
		rt1->Release();		// a second target bound: not this pass's frame
		return;
	}
	if (!rtOK || !DX8Wrapper::Suspend_Scene_Depth())
		return;
	IDirect3DTexture9 *depthTex = DX8Wrapper::Peek_Scene_Depth_Texture();
	D3DSURFACE_DESC dd;
	if (depthTex != NULL && SUCCEEDED(depthTex->GetLevelDesc(0, &dd)) && dd.Width == rd.Width && dd.Height == rd.Height)
		drawSunShafts(dev, depthTex, rd, *s_dustCamera, 13);
	DX8Wrapper::Invalidate_Cached_Render_States();
	DX8Wrapper::Resume_Scene_Depth();
}

void R3DLocalLights::renderPass(const CameraClass &camera, RTS3DScene *scene)
{
	if (!s_active || s_passDone || scene == NULL)
		return;
	s_passDone = TRUE;
	s_dustCamera = &camera;		// Ronin @feature 23/10/2026 DX9: for the shafts' dust, later this frame
	// Ronin @diagnostic 20/10/2026 DX9: the pass's GPU time: marks 1 and 4 round everything below, early returns included.
	struct GpuPassMarks { GpuPassMarks() { gpuMark(1); } ~GpuPassMarks() { gpuMark(4); } } gpuPassMarks;
	s_shadeOpen = FALSE;		// Ronin @feature 10/10/2026 DX9: the sun's shade: what is drawn from here on is not under the lights
	s_casterOpen = FALSE;
	s_stats.casters = s_casterCount;
	endSunShade();
	s_collect  = FALSE;		// Ronin @feature 09/10/2026 DX9: the beams are in; later renders this frame only skip

	// Ronin @bugfix 09/10/2026 DX9: last frame's lights, taken out of the memo - any early return below leaves it empty.
	static LightMemo last[MAX_LIGHTS_DRAWN];
	const Int lastCount = s_memoCount;
	for (Int m = 0; m < lastCount; ++m)
		last[m] = s_memo[m];
	s_memoCount = 0;

	IDirect3DDevice9 *dev = DX8Wrapper::_Get_D3D_Device8();
	if (dev == NULL)
		return;

	IDirect3DSurface9 *rt = NULL;
	D3DSURFACE_DESC rd;
	if (FAILED(dev->GetRenderTarget(0, &rt)) || rt == NULL)
		return;
	if (FAILED(rt->GetDesc(&rd)) || rd.Width == 0 || rd.Height == 0)
	{
		rt->Release();
		return;
	}

	float zNear = 1.0f;
	float zFar  = 1000.0f;
	camera.Get_Clip_Planes(zNear, zFar);
	Vector2 planeMin, planeMax, vpMin, vpMax;
	camera.Get_View_Plane(planeMin, planeMax);
	camera.Get_Viewport(vpMin, vpMax);
	const float planeW = planeMax.X - planeMin.X;
	const float planeH = planeMax.Y - planeMin.Y;
	const float targetW = (float)rd.Width;
	const float targetH = (float)rd.Height;
	// Ronin @feature 09/10/2026 DX9: the 3D viewport in target pixels - lights never draw outside it.
	const float vx0 = vpMin.X * targetW, vy0 = vpMin.Y * targetH;
	const float vx1 = vpMax.X * targetW, vy1 = vpMax.Y * targetH;

	// Ronin @feature 09/10/2026 DX9: gather first. With nothing to draw the device is not touched at all.
	static LightDraw draws[MAX_LIGHTS_DRAWN];
	Int count = 0;
	Int lightCount = 0;
	if (s_effects && planeW > 0.0f && planeH > 0.0f && vx1 > vx0 && vy1 > vy0)		// Ronin @feature 23/10/2026 DX9: the lamps' switch
	{
		const Matrix3D &cameraTM = camera.Get_Transform();
		const ViewMap vm = { zNear, zFar, planeMin.X, planeMax.Y, planeW, planeH, vx0, vy0, vx1, vy1 };
		Int pointLights = 0, sideLights = 0;		// with a downward map; with four level ones as well
		W3DDynamicLight *searchWas[SEARCH_LIGHTS];
		const Int searchWasCount = s_searchCount;
		for (Int w = 0; w < searchWasCount; ++w)
			searchWas[w] = s_searchKeys[w];
		s_searchCount = 0;
		Vector3 searchAt[SEARCH_LIGHTS];
		Int searchAtCount = 0;
		RefRenderObjListIterator it(scene->getDynamicLights());
		for (it.First(); !it.Is_Done(); it.Next())
		{
			W3DDynamicLight *light = (W3DDynamicLight *)it.Peek_Obj();
			if (light == NULL || !light->isEnabled())
				continue;
			++s_stats.live;
			// Ronin @feature 17/10/2026 DX9: the police car's search light never fades; `lights searchlight 0` leaves it out (its
			// beacons' glows light the car), also over its 5-frame fade when the car goes.
			Bool search = (light->getDecayFrames() == 0) ? TRUE : FALSE;
			for (Int w = 0; !search && w < searchWasCount; ++w)
				search = (searchWas[w] == light) ? TRUE : FALSE;
			if (search)
			{
				if (s_searchCount < SEARCH_LIGHTS)
					s_searchKeys[s_searchCount++] = light;
				if (!s_searchLight)
					continue;
				if (searchAtCount < SEARCH_LIGHTS)
					searchAt[searchAtCount++] = light->Get_Position();
			}
			if (count >= MAX_LIGHTS_DRAWN || light->Get_Type() != LightClass::POINT)
				continue;

			LightMemo now;
			now.key   = light;
			now.sub   = -1;
			now.seen  = FALSE;
			now.world = light->Get_Position();
			now.axis.Set(0.0f, 0.0f, 1.0f);
			now.cosOuter = -2.0f;
			now.edge     = 1.0f;
			now.shaft    = 0.0f;
			light->Get_Far_Attenuation_Range(now.inner, now.outer);
			now.inner *= s_range;
			now.outer *= s_range;
			Vector3 diffuse, ambient;
			light->Get_Diffuse(&diffuse);
			light->Get_Ambient(&ambient);
			const float k = light->Get_Intensity() * s_intensity;
			if ((diffuse.X + diffuse.Y + diffuse.Z + ambient.X + ambient.Y + ambient.Z) * k <= 0.002f)
				continue;
			now.diffuse[0] = diffuse.X * k; now.diffuse[1] = diffuse.Y * k; now.diffuse[2] = diffuse.Z * k;
			now.ambient[0] = ambient.X * k; now.ambient[1] = ambient.Y * k; now.ambient[2] = ambient.Z * k;

			// Ronin @bugfix 09/10/2026 DX9: as it was last frame; a light that was not there is the same sphere, dark.
			LightMemo was = now;
			was.diffuse[0] = was.diffuse[1] = was.diffuse[2] = 0.0f;
			was.ambient[0] = was.ambient[1] = was.ambient[2] = 0.0f;
			for (Int m = 0; m < lastCount; ++m)
			{
				if (last[m].key == now.key && last[m].sub == now.sub)
				{
					was = last[m];
					last[m].seen = TRUE;
					break;
				}
			}
			// Ronin @bugfix 16/10/2026 DX9: a pulse shrinks to its inner radius, then only fades (W3DDynamicLight); it was dropped the
			// moment the two met, still half bright. From there it keeps closing with its fade, over a soft edge.
			{
				const float lumNow = 0.299f * (now.diffuse[0] + now.ambient[0]) + 0.587f * (now.diffuse[1] + now.ambient[1]) +
									 0.114f * (now.diffuse[2] + now.ambient[2]);
				now.peakLum   = (was.peakLum > lumNow) ? was.peakLum : lumNow;
				now.peakOuter = (was.peakOuter > now.outer) ? was.peakOuter : now.outer;
				if (now.outer - now.inner < 0.5f)
				{
					const float closing = (now.peakLum > 0.0001f) ? now.peakOuter * lumNow / now.peakLum : now.outer;
					if (closing < now.outer)
						now.outer = closing;
					now.inner = now.outer * 0.7f;
				}
				if (now.outer < 0.5f)
					continue;
			}
			now.boundCentre = now.world;
			now.boundRadius = now.outer;
			if (!makeDraw(draws[count], now, was, cameraTM, vm))
				continue;		// off screen
			s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
			s_memo[s_memoCount++] = now;
			++count;
			++lightCount;
			// Ronin @feature 11/10/2026 DX9: lamp shadow maps, stage D first piece - an engine point light gets ONE map, straight down:
			// what is under it shadows the ground (the pool under a firing unit). The beams' tiles are kept for them.
			// Ronin @bugfix 17/10/2026 DX9: not the police car's light - 2003 gave it no shadow, and 8 up it sits inside the car's body.
			if (!search && s_lampMapsOn && s_pointMaps && pointLights < MAX_POINT_MAPS && s_lampMapCount + s_frameBeamCount < MAX_LAMP_MAPS)
			{
				draws[count - 1].map = s_lampMapCount;
				s_memo[s_memoCount - 1].map = s_lampMapCount;
				LampMap &lm = s_lampMaps[s_lampMapCount++];
				lm.lamp        = now.world;
				lm.axis.Set(0.0f, 0.0f, -1.0f);
				lm.tanHalf     = LAMP_POINT_TAN;
				lm.nearPlane   = LAMP_POINT_NEAR;
				lm.farPlane    = (now.outer > LAMP_POINT_NEAR * 4.0f) ? now.outer : (LAMP_POINT_NEAR * 4.0f);
				lm.point       = TRUE;
				lm.boundCentre = now.boundCentre;
				lm.boundRadius = now.boundRadius;
				lm.owner       = NULL;				// who fired is not known here: the near plane does its job
				lm.selfNear    = LAMP_POINT_NEAR;
				++pointLights;
				// Ronin @feature 11/10/2026 DX9: a big light low over the ground (a blast) reaches past what the downward map sees: four
				// more maps, level, a quarter turn apart - what stands round it shadows the ground behind.
				const float ground = (TheTerrainLogic != NULL) ? TheTerrainLogic->getGroundHeight(now.world.X, now.world.Y) : now.world.Z;
				if (s_pointSides && sideLights < MAX_SIDE_LIGHTS && now.outer >= LAMP_SIDE_MIN_REACH &&
					now.world.Z - ground < LAMP_SIDE_LOW * now.outer && s_lampMapCount + 4 + s_frameBeamCount <= MAX_LAMP_MAPS)
				{
					// Ronin @feature 11/10/2026 DX9: +X, -X, +Y, -Y: LocalLight_ps lampSide counts on this order.
					static const float SIDE_AXES[4][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f } };
					draws[count - 1].sides = s_lampMapCount;
					s_stats.lampSides = ++sideLights;
					for (Int face = 0; face < 4; ++face)
					{
						LampMap &sm = s_lampMaps[s_lampMapCount++];
						sm = lm;
						sm.axis.Set(SIDE_AXES[face][0], SIDE_AXES[face][1], 0.0f);
						sm.tanHalf = 1.0f;
						sm.point   = FALSE;		// its light is counted once, by the downward map
					}
				}
			}
		}

		// Ronin @feature 09/10/2026 DX9: the beams: a spot at the lamp, as wide as the art's cone, reaching a little past its end; a level
		// one is aimed at the ground (BEAM_AIM_AHEAD). Mostly flat: a headlight meets the road at a shallow angle.
		for (Int fb = 0; fb < s_frameBeamCount; ++fb)
		{
			const FrameBeam &beam = s_frameBeams[fb];
			++s_stats.live;
			if (count >= MAX_LIGHTS_DRAWN)
				continue;
			Vector3 along = beam.end - beam.lamp;
			const float len = along.Length();
			const float k   = s_beams * s_intensity;
			if (len < 0.5f || k <= 0.002f)
				continue;
			along /= len;
			const float rise = Vector3::Dot_Product(along, beam.up);		// the sine of its angle above level
			Vector3 flat = along - beam.up * rise;
			const float flatLen = flat.Length();
			if (beam.height > 0.5f && flatLen > 0.001f && rise <= BEAM_AIM_MAX_UP && -rise <= BEAM_AIM_MAX_DOWN)
			{
				const float wantDown = atan2f(beam.height, BEAM_AIM_AHEAD * len);
				const float haveDown = atan2f(-rise, flatLen);
				if (haveDown < wantDown)
				{
					const float down = (wantDown < haveDown + BEAM_AIM_MAX_TILT) ? wantDown : (haveDown + BEAM_AIM_MAX_TILT);
					along = flat * (cosf(down) / flatLen) - beam.up * sinf(down);
				}
			}
			LightMemo now;
			now.key   = beam.mesh;
			now.sub   = beam.index;
			now.seen  = FALSE;
			now.world = beam.lamp;
			now.axis  = along;
			// Ronin @bugfix 09/10/2026 DX9: the light runs well past the art's cone and fades all the way (it stopped at 1.3 lengths, on a straight
			// ramp: an edge round every pool). The beam in the air keeps its own end - c14.x, the corona's slot, unused by a spot.
			now.outer = len * s_beamReach * s_range;
			now.inner = now.outer * BEAM_INNER;
			// Ronin @bugfix 17/10/2026 DX9: the cone in the air no longer follows `beams` - the pools can be turned down alone.
			now.shaft = s_shafts * BEAM_CONE_REF / ((s_beams > 0.01f) ? s_beams : 0.01f);
			now.coronaRadius = len * BEAM_REACH * s_range;
			now.lampRadius = beam.lampRadius * s_lampWidth;
			float tanOuter = beam.endRadius / len;
			if (tanOuter < 0.05f) tanOuter = 0.05f;
			if (tanOuter > 3.0f)  tanOuter = 3.0f;
			const float tanInner = tanOuter * s_hotspot;
			const float cosInner = 1.0f / sqrtf(1.0f + tanInner * tanInner);
			now.cosOuter = 1.0f / sqrtf(1.0f + tanOuter * tanOuter);
			now.edge     = 1.0f / (((cosInner - now.cosOuter) > 0.001f) ? (cosInner - now.cosOuter) : 0.001f);
			// Ronin @feature 09/10/2026 DX9: the cone inside the reach fits a sphere half way along it; a very wide one is the whole reach.
			// Ronin @feature 09/10/2026 DX9: measured on the spill, wider than the cone (LocalLight_ps coneTerm) - or its rectangle cuts it.
			const float tanSpill = tanOuter * s_spillWide;
			const float cosSpill = 1.0f / sqrtf(1.0f + tanSpill * tanSpill);
			if (cosSpill >= 0.5f)
			{
				now.boundCentre = beam.lamp + along * (now.outer * 0.5f);
				now.boundRadius = now.outer * sqrtf(1.25f - cosSpill);
			}
			else
			{
				now.boundCentre = beam.lamp;
				now.boundRadius = now.outer;
			}
			for (Int ch = 0; ch < 3; ++ch)
			{
				now.diffuse[ch] = BEAM_COLOUR[ch] * k * 0.35f;
				now.ambient[ch] = BEAM_COLOUR[ch] * k * 0.65f;
			}
			LightMemo was = now;
			was.diffuse[0] = was.diffuse[1] = was.diffuse[2] = 0.0f;
			was.ambient[0] = was.ambient[1] = was.ambient[2] = 0.0f;
			for (Int m = 0; m < lastCount; ++m)
			{
				if (last[m].key == now.key && last[m].sub == now.sub)
				{
					was = last[m];
					last[m].seen = TRUE;
					break;
				}
			}
			if (!makeDraw(draws[count], now, was, cameraTM, vm))
				continue;		// off screen
			s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
			s_memo[s_memoCount++] = now;
			++count;
			++lightCount;
			// Ronin @feature 11/10/2026 DX9: lamp shadow maps, stage B1 - this beam's map: from the lamp, along the light as aimed, as wide
			// as its spill and as deep as its reach. Drawn by drawLampMaps once the pass has saved its states.
			if (s_lampMapsOn && s_lampMapCount < MAX_LAMP_MAPS)
			{
				draws[count - 1].map = s_lampMapCount;
				s_memo[s_memoCount - 1].map = s_lampMapCount;
				// Ronin @bugfix 12/10/2026 DX9: the oil derrick's lamp art sways by under a degree (the only shipped beam that does). Its
				// map swung with it, its texels sliding over bars thinner than a texel: the lattice's shadows flickered. The map holds still.
				LightMemo &kept = s_memo[s_memoCount - 1];
				const Bool hold = (was.mapHeld && (was.mapLamp - now.world).Length() < LAMP_HOLD_MOVE &&
								   Vector3::Dot_Product(was.mapAxis, now.axis) > LAMP_HOLD_COS) ? TRUE : FALSE;
				kept.mapLamp = hold ? was.mapLamp : now.world;
				kept.mapAxis = hold ? was.mapAxis : now.axis;
				kept.mapHeld = TRUE;
				LampMap &lm = s_lampMaps[s_lampMapCount++];
				lm.selfNear    = (s_lampSelfNear * len > LAMP_NEAR) ? (s_lampSelfNear * len) : LAMP_NEAR;
				lm.nearPlane   = LAMP_NEAR;
				lm.point       = FALSE;
				lm.lamp        = kept.mapLamp;
				lm.axis        = kept.mapAxis;
				lm.tanHalf     = (tanSpill < LAMP_MAX_TAN) ? tanSpill : LAMP_MAX_TAN;
				lm.farPlane    = (now.outer > LAMP_NEAR * 4.0f) ? now.outer : (LAMP_NEAR * 4.0f);
				lm.boundCentre = now.boundCentre;
				lm.boundRadius = now.boundRadius;
				const RenderObjClass *top = (const MeshClass *)beam.mesh;
				while (top->Get_Container() != NULL)
					top = top->Get_Container();
				lm.owner = top;
			}
			++s_stats.beams;
		}

		// Ronin @feature 09/10/2026 DX9: the lamp glows: a point light at each, its colour and reach by its texture (GLOW_TABLE). The glow
		// itself is still drawn - the lamp has to look lit.
		for (Int fg = 0; fg < s_frameGlowCount; ++fg)
		{
			const FrameGlow &glow = s_frameGlows[fg];
			++s_stats.live;
			const float k = s_glows * s_intensity;
			if (count >= MAX_LIGHTS_DRAWN || k <= 0.002f)
				continue;
			// Ronin @feature 25/10/2026 DX9: a joined string's bulb is its corona alone (the string's light is one); with the art's own
			// glows shown, nothing. The joined light has no corona: its bulbs draw theirs.
			const Bool bulb   = (glow.power <= 0.0f) ? TRUE : FALSE;
			const Bool joined = (glow.spread > 0.0f) ? TRUE : FALSE;
			if (bulb && !s_coronasOn)
				continue;
			// Ronin @bugfix 25/10/2026 DX9: a bulb lights no surface - its colour 1/100, its corona x100 (the shader draws colour x corona);
			// at full strength each painted a strip on the wall it hangs on.
			const float bulbShare = 0.01f;
			const float power = bulb ? bulbShare : glow.power;
			const GlowKind &gk = GLOW_TABLE[glow.kind];
			LightMemo now;
			now.key   = glow.mesh;
			now.sub   = 1000 + glow.index;		// apart from the same mesh's beams
			now.coronaRadius   = glow.radius * s_coronaSize;
			now.coronaStrength = (s_coronasOn && !joined) ? (s_coronas / (k * (bulb ? bulbShare : 1.0f))) : 0.0f;		// the shader multiplies by the light's colour x k
			now.seen  = FALSE;
			now.world = glow.pos;
			now.outer = (gk.reach * glow.scale * s_glowReach + glow.spread) * s_range;
			// Ronin @feature 17/10/2026 DX9: a police car's beacon - the car's own light lights round it; the glow keeps its corona.
			for (Int s = 0; s < searchAtCount; ++s)
			{
				if ((searchAt[s] - glow.pos).Length() <= SEARCH_BEACONS)
				{
					now.outer = now.coronaRadius;
					break;
				}
			}
			if (bulb)
				now.outer = (now.coronaRadius > 0.7f) ? now.coronaRadius : 0.7f;
			now.inner = now.outer * 0.25f;
			if (now.outer - now.inner < 0.5f)
				continue;
			now.axis.Set(0.0f, 0.0f, 1.0f);
			now.cosOuter = -2.0f;
			now.edge     = 1.0f;
			now.shaft    = 0.0f;
			now.boundCentre = now.world;
			now.boundRadius = now.outer;
			for (Int ch = 0; ch < 3; ++ch)
			{
				now.diffuse[ch] = gk.colour[ch] * k * 0.5f * power;
				now.ambient[ch] = gk.colour[ch] * k * 0.5f * power;
			}
			LightMemo was = now;
			was.diffuse[0] = was.diffuse[1] = was.diffuse[2] = 0.0f;
			was.ambient[0] = was.ambient[1] = was.ambient[2] = 0.0f;
			for (Int m = 0; m < lastCount; ++m)
			{
				if (last[m].key == now.key && last[m].sub == now.sub)
				{
					was = last[m];
					last[m].seen = TRUE;
					break;
				}
			}
			if (!makeDraw(draws[count], now, was, cameraTM, vm))
				continue;		// off screen
			s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
			s_memo[s_memoCount++] = now;
			++count;
			++lightCount;
			++s_stats.glows;
		}

		// Ronin @feature 09/10/2026 DX9: a painted pool that no beam lands on and no lamp stands by (351 of the art's 1155) would go
		// dark with its texture: a flat light of its own size and colour stands in. `lights poolfill`.
		for (Int fp = 0; fp < s_framePoolCount; ++fp)
		{
			const FramePool &pool = s_framePools[fp];
			Bool covered = FALSE;
			for (Int fb = 0; fb < s_frameBeamCount && !covered; ++fb)
				covered = ((s_frameBeams[fb].end - pool.pos).Length() <= pool.radius + s_frameBeams[fb].endRadius ||
						   (s_frameBeams[fb].lamp - pool.pos).Length() <= pool.radius) ? TRUE : FALSE;
			for (Int fg = 0; fg < s_frameGlowCount && !covered; ++fg)
				covered = ((s_frameGlows[fg].pos - pool.pos).Length() <=
						   pool.radius + 0.5f * GLOW_TABLE[s_frameGlows[fg].kind].reach * s_frameGlows[fg].scale) ? TRUE : FALSE;
			if (covered)
				continue;
			++s_stats.live;
			const float k = s_poolFill * s_intensity;
			if (count >= MAX_LIGHTS_DRAWN || k <= 0.002f)
				continue;
			const GlowKind &gk = GLOW_TABLE[pool.kind];
			LightMemo now;
			now.key   = pool.mesh;
			now.sub   = 2000 + pool.index;		// apart from the same mesh's beams and glows
			now.seen  = FALSE;
			now.world = pool.pos;
			now.outer = pool.radius * s_range;
			now.inner = now.outer * 0.1f;
			if (now.outer - now.inner < 0.5f)
				continue;
			now.axis.Set(0.0f, 0.0f, 1.0f);
			now.cosOuter = -2.0f;
			now.edge     = 1.0f;
			now.shaft    = 0.0f;
			now.boundCentre = now.world;
			now.boundRadius = now.outer;
			for (Int ch = 0; ch < 3; ++ch)
			{
				now.diffuse[ch] = 0.0f;		// flat: the light sits IN the painted surface
				now.ambient[ch] = gk.colour[ch] * k;
			}
			LightMemo was = now;
			was.ambient[0] = was.ambient[1] = was.ambient[2] = 0.0f;
			for (Int m = 0; m < lastCount; ++m)
			{
				if (last[m].key == now.key && last[m].sub == now.sub)
				{
					was = last[m];
					last[m].seen = TRUE;
					break;
				}
			}
			if (!makeDraw(draws[count], now, was, cameraTM, vm))
				continue;		// off screen
			s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
			s_memo[s_memoCount++] = now;
			++count;
			++lightCount;
			++s_stats.pools;
		}

		// Ronin @feature 15/10/2026 DX9: GLOBAL LIGHT ITEM 7 - GLOWING EFFECTS LIGHT THEIR SURROUNDINGS in their own colour. Each additive
		// particle system with particles on screen is one point light at their centre, reaching past their spread; the strongest kept.
		float fxEase = 1.0f;	// Ronin @feature 19/10/2026 DX9: 1 = an effect light takes what its particles say at once
		if (s_fxLights > 0 && s_fxStrength > 0.0f && TheParticleSystemManager != NULL)
		{
			struct FxGlow { const ParticleSystem *sys; Vector3 at; float col[3]; float power; float spread; Bool fire; };
			static FxGlow fx[FX_CANDIDATES];
			Int fxCount = 0;
			ParticleSystemManager::ParticleSystemList &systems = TheParticleSystemManager->getAllParticleSystems();
			for (ParticleSystemManager::ParticleSystemListIt sit = systems.begin(); sit != systems.end(); ++sit)
			{
				ParticleSystem *sys = *sit;
				if (sys == NULL || sys->getShaderType() != ParticleSystemInfo::ADDITIVE)
					continue;
				// Ronin @bugfix 17/10/2026 DX9: a ripple or shock-wave ring (all 40 on EXShockWav, flat) is water or air moving, not a light -
				// the battleship's ripples lit the whole ship white.
				if (_strnicmp(sys->getParticleTypeName().str(), "exshockwav", 10) == 0)
					continue;
				// Ronin @feature 16/10/2026 DX9: a lens flare is a camera's glare, not a light in the world - its big sprite made the Humvee's
				// wide pool (`lights fxflares`).
				Bool isFire = FALSE;
				if (sys->getTemplate() != NULL)
				{
					char low[96];
					const char *name = sys->getTemplate()->getName().str();
					Int n = 0;
					for (; name[n] != 0 && n < (Int)sizeof(low) - 1; ++n)
						low[n] = (char)tolower((unsigned char)name[n]);
					low[n] = 0;
					if (!s_fxFlares && (strstr(low, "flare") != NULL || strstr(low, "lenz") != NULL || strstr(low, "lens") != NULL))
						continue;
					// Ronin @feature 16/10/2026 DX9: fires by name - fire casts light round it (118 of the 475 non-flare additive systems).
					static const char *const FIRE_WORDS[] = { "fire", "flame", "burn", "smolder", "ember", "napalm", "inferno" };
					for (Int fw = 0; fw < (Int)(sizeof(FIRE_WORDS) / sizeof(FIRE_WORDS[0])) && !isFire; ++fw)
						isFire = (strstr(low, FIRE_WORDS[fw]) != NULL) ? TRUE : FALSE;
				}
				++s_stats.fxGlowing;
				FxGlow g;
				g.sys = sys;
				g.fire = isFire;
				g.power = 0.0f;
				g.spread = 0.0f;
				float wsum = 0.0f, sizeSum = 0.0f, pos[3] = { 0.0f, 0.0f, 0.0f };
				g.col[0] = g.col[1] = g.col[2] = 0.0f;
				// Ronin @bugfix 15/10/2026 DX9: every particle, not only the unculled - that flag is the last particle draw's, which at
				// this point is not this view's (fx=0/0/208 in his shot). Off-screen lights are dropped below, by their sphere.
				for (Particle *p = sys->getFirstParticle(); p != NULL; p = p->m_systemNext)
				{
					const float size = (p->getSize() < FX_SIZE_CAP) ? p->getSize() : FX_SIZE_CAP;
					// Ronin @bugfix 16/10/2026 DX9: each counts by the light it gives, colour luminance x area: not alpha (additive is ONE / ONE,
					// 284 of 589 keep alpha 0), not area alone (a rocket's faded trail puffs pulled the light back along its path).
					const RGBColor *c = p->getColor();
					const float w = (0.299f * c->red + 0.587f * c->green + 0.114f * c->blue) * size * size;
					if (w <= 0.0f)
						continue;
					const Coord3D *at = p->getPosition();
					g.col[0] += c->red * w; g.col[1] += c->green * w; g.col[2] += c->blue * w;
					pos[0] += at->x * w; pos[1] += at->y * w; pos[2] += at->z * w;
					g.power += w;
					wsum += w;
					sizeSum += w * size;
				}
				if (wsum <= 0.0f || g.power <= 0.0f)
					continue;
				for (Int k = 0; k < 3; ++k)
					g.col[k] /= wsum;
				g.at.Set(pos[0] / wsum, pos[1] / wsum, pos[2] / wsum);
				// Ronin @bugfix 17/10/2026 DX9: a projectile's exhaust is lit at its youngest particle; once its projectile is gone, not at
				// all (the blast lights the impact). By ID: a system drops its attachment when its object goes (ParticleSys.cpp:2344).
				static ParticleSystemID s_trailIds[256];
				static Int s_trailNext = 0;
				Bool trail = FALSE;
				{
					Object *owner = (sys->getAttachedObject() != INVALID_ID && TheGameLogic != NULL)
									? TheGameLogic->findObjectByID(sys->getAttachedObject()) : NULL;
					Bool known = FALSE;
					for (Int t = 0; t < 256 && !known; ++t)
						known = (s_trailIds[t] == sys->getSystemID()) ? TRUE : FALSE;
					if (owner != NULL && owner->isKindOf(KINDOF_PROJECTILE))
					{
						trail = TRUE;
						if (!known)
						{
							s_trailIds[s_trailNext] = sys->getSystemID();
							s_trailNext = (s_trailNext + 1) & 255;
						}
					}
					else if (known)
						continue;		// a spent trail: its projectile is gone
				}
				UnsignedInt youngest = 0xFFFFFFFFu;
				if (trail)
				{
					for (Particle *p = sys->getFirstParticle(); p != NULL; p = p->m_systemNext)
					{
						if (p->getElapsedFrames() < youngest)
						{
							youngest = p->getElapsedFrames();
							const Coord3D *at = p->getPosition();
							g.at.Set(at->x, at->y, at->z);
						}
					}
				}
				// Ronin @bugfix 16/10/2026 DX9: the spread by weight (1.5 x the RMS distance), not the farthest particle - a blast's few
				// far-flung flames made its pool huge.
				float d2sum = 0.0f, wNear = 0.0f, sizeNear = 0.0f;
				for (Particle *p = sys->getFirstParticle(); p != NULL; p = p->m_systemNext)
				{
					if (trail && p->getElapsedFrames() > youngest + 6)
						continue;
					const float size = (p->getSize() < FX_SIZE_CAP) ? p->getSize() : FX_SIZE_CAP;
					const RGBColor *c = p->getColor();
					const Coord3D *at = p->getPosition();
					const float d = (Vector3(at->x, at->y, at->z) - g.at).Length();
					const float w = (0.299f * c->red + 0.587f * c->green + 0.114f * c->blue) * size * size;
					d2sum += w * d * d;
					wNear += w;
					sizeNear += w * size;
				}
				// Ronin @feature 16/10/2026 DX9: the effect's size - its particles' spread and half their sprites' weighted size.
				g.spread = (wNear > 0.0f) ? (1.5f * sqrtf(d2sum / wNear) + 0.5f * (sizeNear / wNear)) : 0.5f * (sizeSum / wsum);
				// Ronin @bugfix 15/10/2026 DX9: on this camera's screen, as the light it would be - else it never takes a slot.
				{
					Vector3 inView;
					Matrix3D::Inverse_Transform_Vector(cameraTM, g.at, &inView);
					float onScreen[4];
					if (!spherePixelRect(inView, (2.0f * g.spread + (isFire ? s_fxFire : 0.0f)) * s_fxReach * s_range, vm, onScreen))
						continue;
				}
				++s_stats.fxCandidates;
				// Ronin @feature 15/10/2026 DX9: keep the FX_CANDIDATES strongest, strongest first.
				Int slot = fxCount;
				if (fxCount < FX_CANDIDATES)
					++fxCount;
				else if (g.power <= fx[FX_CANDIDATES - 1].power)
					continue;
				else
					slot = FX_CANDIDATES - 1;
				while (slot > 0 && fx[slot - 1].power < g.power)
				{
					fx[slot] = fx[slot - 1];
					--slot;
				}
				fx[slot] = g;
			}
			// Ronin @bugfix 16/10/2026 DX9: the reach is the effect's own size, up to twice it by its brightness (no fixed 40: casings lit a
			// 40-unit pool); a fire reaches at least its size + `fxfire` x its brightness share - fire lights round it.
			auto fxReachOf = [](float size, float power, Bool fire) -> float
			{
				const float t = power / FX_POWER_FULL;
				const float soft = sqrtf(t / (1.0f + t));
				const float bySize = size * (1.0f + soft);
				const float byFire = fire ? (size + s_fxFire * soft) : 0.0f;
				return (byFire > bySize) ? byFire : bySize;
			};
			// Ronin @bugfix 16/10/2026 DX9: one blast is several glowing systems at one spot, and each was a light - stacked, far too
			// bright. Centres within half the bigger reach are one light: power summed, centre and colour by power. `lights fxmerge`.
			static FxGlow merged[FX_CANDIDATES];
			Int mergedCount = 0;
			for (Int f = 0; f < fxCount; ++f)
			{
				const FxGlow &g = fx[f];
				Int into = -1;
				for (Int m = 0; m < mergedCount && into < 0 && s_fxMerge; ++m)
				{
					const float ra = fxReachOf(merged[m].spread, merged[m].power, merged[m].fire), rb = fxReachOf(g.spread, g.power, g.fire);
					if ((merged[m].at - g.at).Length() < 0.5f * ((ra > rb) ? ra : rb) * s_fxReach)
						into = m;
				}
				if (into < 0)
				{
					merged[mergedCount++] = g;
					continue;
				}
				FxGlow &mm = merged[into];
				const float total = mm.power + g.power;
				const Vector3 at = (mm.at * mm.power + g.at * g.power) / total;
				const float reachA = mm.spread + (mm.at - at).Length(), reachB = g.spread + (g.at - at).Length();
				for (Int k = 0; k < 3; ++k)
					mm.col[k] = (mm.col[k] * mm.power + g.col[k] * g.power) / total;
				mm.at = at;
				mm.spread = (reachA > reachB) ? reachA : reachB;
				mm.power = total;
				mm.fire = (mm.fire || g.fire) ? TRUE : FALSE;
			}
			const Int fxKeep = (mergedCount < s_fxLights) ? mergedCount : s_fxLights;
			// Ronin @feature 19/10/2026 DX9: `lights fxsmooth` - how far this pass moves each effect light to what its particles say: by the
			// time since the last pass, so the same at 20 fps as at 60.
			const UnsignedInt fxNow = timeGetTime();
			float fxDt = (s_fxClock != 0) ? (float)(fxNow - s_fxClock) * 0.001f : (1.0f / 30.0f);
			fxDt = (fxDt < 0.0f) ? 0.0f : ((fxDt > 0.25f) ? 0.25f : fxDt);
			s_fxClock = fxNow;
			fxEase = (s_fxSmooth > 0.0f) ? (1.0f - powf(s_fxSmooth, fxDt * 30.0f)) : 1.0f;
			for (Int f = 0; f < fxKeep && count < MAX_LIGHTS_DRAWN; ++f)
			{
				const FxGlow &g = merged[f];
				const float peak = (g.col[0] > g.col[1]) ? ((g.col[0] > g.col[2]) ? g.col[0] : g.col[2]) : ((g.col[1] > g.col[2]) ? g.col[1] : g.col[2]);
				if (peak < 0.02f)
					continue;
				// Ronin @bugfix 16/10/2026 DX9: no hard knee - it held full strength and dropped at the end; now it fades all the way.
				const float t = g.power / FX_POWER_FULL;
				const float strength = s_fxStrength * s_intensity * sqrtf(t / (1.0f + t));
				LightMemo now;
				now.key   = g.sys;
				now.sub   = 3000;		// apart from every other kind
				now.seen  = FALSE;
				now.world = g.at;
				now.outer = fxReachOf(g.spread, g.power, g.fire) * s_fxReach * s_range;
				now.inner = now.outer * 0.15f;
				if (now.outer - now.inner < 0.5f)
					continue;
				now.axis.Set(0.0f, 0.0f, 1.0f);
				now.cosOuter = -2.0f;
				now.edge     = 1.0f;
				now.shaft    = 0.0f;
				now.boundCentre = now.world;
				now.boundRadius = now.outer;
				for (Int ch = 0; ch < 3; ++ch)
				{
					now.diffuse[ch] = g.col[ch] / peak * strength;		// its own colour at full, its brightness in the strength
					now.ambient[ch] = now.diffuse[ch] * 0.25f;			// a little on faces turned away
				}
				LightMemo was = now;
				was.diffuse[0] = was.diffuse[1] = was.diffuse[2] = 0.0f;
				was.ambient[0] = was.ambient[1] = was.ambient[2] = 0.0f;
				// Ronin @feature 19/10/2026 DX9: last frame's light this one continues - its own system, or with `fxsmooth` the nearest
				// effect light within half their reach: when the systems regroup, the light that was there.
				Int match = -1;
				float matchD = 0.0f;
				for (Int m = 0; m < lastCount; ++m)
				{
					if (last[m].sub != now.sub || last[m].seen)
						continue;
					if (last[m].key == now.key)
					{
						match = m;
						break;
					}
					const float d = (last[m].world - now.world).Length();
					const float halfReach = 0.5f * ((last[m].outer > now.outer) ? last[m].outer : now.outer);
					if (s_fxSmooth > 0.0f && d < halfReach && (match < 0 || d < matchD))
					{
						match = m;
						matchD = d;
					}
				}
				if (match >= 0)
				{
					was = last[match];
					last[match].seen = TRUE;
					// Ronin @feature 19/10/2026 DX9: strength, reach and place ease from it (`fxsmooth`) instead of jumping.
					for (Int ch = 0; ch < 3; ++ch)
					{
						now.diffuse[ch] = was.diffuse[ch] + (now.diffuse[ch] - was.diffuse[ch]) * fxEase;
						now.ambient[ch] = was.ambient[ch] + (now.ambient[ch] - was.ambient[ch]) * fxEase;
					}
					now.world = was.world + (now.world - was.world) * fxEase;
					now.outer = was.outer + (now.outer - was.outer) * fxEase;
					now.inner = now.outer * 0.15f;
					now.boundCentre = now.world;
					now.boundRadius = now.outer;
				}
				else
				{
					for (Int ch = 0; ch < 3; ++ch)		// Ronin @feature 19/10/2026 DX9: a new one fades in
					{
						now.diffuse[ch] *= fxEase;
						now.ambient[ch] *= fxEase;
					}
				}
				if (!makeDraw(draws[count], now, was, cameraTM, vm))
					continue;		// off screen
				s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
				s_memo[s_memoCount++] = now;
				++count;
				++lightCount;
				++s_stats.live;
				++s_stats.fx;
			}
		}

		// Ronin @bugfix 09/10/2026 DX9: a light drawn last frame and gone now gets one dark draw, so the mask marks where it went out.
		for (Int m = 0; m < lastCount && count < MAX_LIGHTS_DRAWN; ++m)
		{
			if (last[m].seen)
				continue;
			// Ronin @feature 19/10/2026 DX9: an effect light no system holds now fades out (`fxsmooth`), kept in the memo while it does.
			if (last[m].sub == 3000 && fxEase < 1.0f && s_memoCount < MAX_LIGHTS_DRAWN)
			{
				LightMemo fading = last[m];
				float peak = 0.0f;
				for (Int ch = 0; ch < 3; ++ch)
				{
					fading.diffuse[ch] *= 1.0f - fxEase;
					fading.ambient[ch] *= 1.0f - fxEase;
					peak = (fading.diffuse[ch] > peak) ? fading.diffuse[ch] : peak;
				}
				fading.seen = FALSE;
				if (peak > 0.01f)
				{
					if (!makeDraw(draws[count], fading, last[m], cameraTM, vm))
						continue;
					s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
					s_memo[s_memoCount++] = fading;
					++count;
					++lightCount;
					++s_stats.fx;
					continue;
				}
			}
			LightMemo gone = last[m];
			gone.diffuse[0] = gone.diffuse[1] = gone.diffuse[2] = 0.0f;
			gone.ambient[0] = gone.ambient[1] = gone.ambient[2] = 0.0f;
			if (!makeDraw(draws[count], gone, last[m], cameraTM, vm))
				continue;
			s_stats.coverage += ((draws[count].x1 - draws[count].x0) * (draws[count].y1 - draws[count].y0)) / (targetW * targetH);
			++count;
		}
	}
	// Ronin @feature 15/10/2026 DX9: the ground as a light (`lights bounce`) wants the pass with no lamp on screen too.
	float bounceUV[4], bounceAmb[3];
	IDirect3DTexture9 *bounceMap = (s_bounce > 0.0f && (s_view == 0 || s_view == 6)) ? R3DSkyCube::groundMap(bounceUV, bounceAmb) : NULL;
	// Ronin @feature 21/10/2026 DX9: HDR's sun lift (stage 1b) draws with or without a light on screen - after the shade pass below.
	const Bool liftWanted = sunLiftWanted(rd);
	const Bool shaftsWanted = sunShaftsWanted();		// Ronin @feature 22/10/2026 DX9: the sun shafts draw with no light on screen too
	if (count == 0 && s_view != 5 && s_view != 9 && bounceMap == NULL && !liftWanted && !shaftsWanted)		// Ronin @diagnostic 12/10/2026 DX9: view 5 (the sky cube) draws with no light on screen too
	{
		rt->Release();
		return;
	}

	// Ronin @feature 09/10/2026 DX9: a second render target still bound would force every target to one size - as the SSAO pass.
	IDirect3DSurface9 *rt1 = NULL;
	if (SUCCEEDED(dev->GetRenderTarget(1, &rt1)) && rt1 != NULL)
	{
		rt1->Release();
		rt->Release();
		return;
	}

	if (!DX8Wrapper::Suspend_Scene_Depth())
	{
		rt->Release();
		return;
	}

	// Ronin @feature 09/10/2026 DX9: the depth must be the frame's size (it is, in both the swap and the prepass), and the
	// frame is copied once - every light reads the frame as it stood before any of them.
	IDirect3DTexture9 *depthTex = DX8Wrapper::Peek_Scene_Depth_Texture();
	D3DSURFACE_DESC dd;
	const Bool depthOK = (depthTex != NULL && SUCCEEDED(depthTex->GetLevelDesc(0, &dd)) && dd.Width == rd.Width &&
						  dd.Height == rd.Height) ? TRUE : FALSE;
	// Ronin @feature 21/10/2026 DX9: the sun's shade from its map first: the lights read it, and so does HDR's lift, which goes into the
	// frame before the copy. In every mode - MSAA has no records, and the meshes' leave gaps (glass, trees) where the ground behind was read.
	Bool meshShade = (s_shadeWritten && s_shadeTex != NULL && s_shadeW == rd.Width && s_shadeH == rd.Height) ? TRUE : FALSE;
	Bool shadeFromMap = FALSE;
	// Ronin @bugfix 02/11/2026 DX9: for the lamps only with one on screen - 0.43 ms at 1600x900 for none; bounce then reads the records.
	if (depthOK && ((s_effects && s_sunFill > 0.0f && count > 0) || liftWanted))
	{
		// Ronin @bugfix 04/11/2026 DX9: SSAO's `aosun` ran it this frame after the trees, the scene as it stands now: 0.43 ms saved.
		if (s_shadeMapReuse && s_shadeMapFrame == s_fixFrame && s_shadeTex != NULL && s_shadeW == rd.Width && s_shadeH == rd.Height)
			shadeFromMap = TRUE;
		else
			shadeFromMap = runShadePass(dev, depthTex, rd, camera);
		meshShade = shadeFromMap ? TRUE : meshShade;
	}
	drawSunLift(dev, rd, shadeFromMap);		// Ronin @feature 12/10/2026 DX9: HDR stage 1b: with or without a light on screen
	if (s_view == 9)		// Ronin @diagnostic 22/10/2026 DX9: the sun shafts' march alone, no light after it
	{
		if (depthOK)
			drawSunShafts(dev, depthTex, rd, camera, 9);
		DX8Wrapper::Invalidate_Cached_Render_States();
		DX8Wrapper::Resume_Scene_Depth();
		rt->Release();
		return;
	}
	if (count == 0 && s_view != 5 && bounceMap == NULL)		// Ronin @feature 21/10/2026 DX9: the lift was all this frame wanted
	{
		if (depthOK && shaftsWanted)
			drawSunShafts(dev, depthTex, rd, camera, 10);		// Ronin @feature 22/10/2026 DX9: ... or the sun shafts
		DX8Wrapper::Invalidate_Cached_Render_States();
		DX8Wrapper::Resume_Scene_Depth();
		rt->Release();
		return;
	}
	IDirect3DSurface9 *copySurf = NULL;
	// Ronin @bugfix 02/11/2026 DX9: only the lamps read the frame's copy (s1): with none on screen - the ground's bounce alone - none is taken.
	const Bool copyWanted = (count > 0) ? TRUE : FALSE;
	Bool ready = (depthOK && (!copyWanted || ensureCopy(rd.Width, rd.Height, rd.Format))) ? TRUE : FALSE;
	if (ready && copyWanted)
	{
		copySurf = s_copy->Get_D3D_Surface_Level();
		ready = (copySurf != NULL && SUCCEEDED(dev->StretchRect(rt, NULL, copySurf, NULL, D3DTEXF_NONE))) ? TRUE : FALSE;
	}
	if (copySurf != NULL)
		copySurf->Release();
	rt->Release();
	if (!ready)
	{
		DX8Wrapper::Resume_Scene_Depth();
		return;
	}

	// Ronin @feature 09/10/2026 DX9: everything the pass touches is read raw and written back raw - states, viewport and the
	// pixel constants a later draw may still count on - and the wrapper cache is invalidated at the end, as the SSAO pass.
	DWORD zEnable, zWrite, alphaBlend, alphaTest, stencil, cull, colorWrite, srcBlend, destBlend;
	dev->GetRenderState(D3DRS_ZENABLE,          &zEnable);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &zWrite);
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphaBlend);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &alphaTest);
	dev->GetRenderState(D3DRS_STENCILENABLE,    &stencil);
	dev->GetRenderState(D3DRS_CULLMODE,         &cull);
	dev->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
	dev->GetRenderState(D3DRS_SRCBLEND,         &srcBlend);
	dev->GetRenderState(D3DRS_DESTBLEND,        &destBlend);
	D3DVIEWPORT9 oldVP;
	const Bool haveVP = SUCCEEDED(dev->GetViewport(&oldVP));
	float savedConstants[34 * 4];		// c0..c15, c16..c29: a light's shadow maps, c30..c32: the world's axes, c33: `dayshade`
	const Bool haveConstants = SUCCEEDED(dev->GetPixelShaderConstantF(0, savedConstants, 34));
	float savedFrameLights[7 * 4];		// Ronin @feature 25/10/2026 DX9: c44..c50, the map's lights by facing
	const Bool haveFrameLights = SUCCEEDED(dev->GetPixelShaderConstantF(44, savedFrameLights, 7));

	// Ronin @feature 11/10/2026 DX9: lamp shadow maps, stage B1: into their own target, before the pass sets anything of its own.
	drawLampMaps(dev);

	dev->SetRenderState(D3DRS_ZENABLE, FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	// Ronin @feature 09/10/2026 DX9: added to the frame (views 1 and 2 replace it). RGB only - destination alpha is the frame's.
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, (s_view == 1 || s_view == 2) ? FALSE : TRUE);
	dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

	// Ronin @bugfix 09/10/2026 DX9: TAA's change mask as render target 1 while the lights draw (it blends as target 0 does: summed).
	const Bool masked = R3DTaa::beginLightMask();
	s_stats.masked = masked;

	// Ronin @feature 09/10/2026 DX9: the quads are in whole-target pixels, so the viewport is the whole target for the pass.
	D3DVIEWPORT9 fullVP;
	fullVP.X = 0; fullVP.Y = 0; fullVP.Width = rd.Width; fullVP.Height = rd.Height; fullVP.MinZ = 0.0f; fullVP.MaxZ = 1.0f;
	dev->SetViewport(&fullVP);

	float factor[3];
	sceneLightFactor(factor);
	s_stats.sceneLight[0] = factor[0];
	s_stats.sceneLight[1] = factor[1];
	s_stats.sceneLight[2] = factor[2];
	// Ronin @feature 10/10/2026 DX9: the terrain's shade. Its shadow pass renders this frame's term into a screen texture and
	// multiplies that into the frame (HeightMap.cpp renderTerrainShadowPass), so it is exact. Never under MSAA: not written then.
	IDirect3DBaseTexture9 *groundShade = NULL;
	if (s_sunFill > 0.0f && !shadeFromMap && rd.MultiSampleType == D3DMULTISAMPLE_NONE && TheTerrainShadowPass.enabled &&
		TheTerrainShadowPass.accumSurf != NULL && TheTerrainShadowPass.accumCur != NULL)
	{
		D3DSURFACE_DESC gd;
		if (SUCCEEDED(((IDirect3DTexture9 *)TheTerrainShadowPass.accumCur)->GetLevelDesc(0, &gd)) && gd.Width == rd.Width &&
			gd.Height == rd.Height)
			groundShade = TheTerrainShadowPass.accumCur;
	}
	const float shadeUse = (meshShade || groundShade != NULL) ? s_sunFill : 0.0f;
	s_stats.sunShade = (shadeUse > 0.0f) ? ((meshShade ? (shadeFromMap ? 4 : 1) : 0) + ((groundShade != NULL) ? 2 : 0)) : 0;	// 4 = from the map
	// Ronin @feature 15/10/2026 DX9: the shade's floor, as the shadow shaders make it, as one luminance (c4.w): where a pixel is in the sun.
	float floorLum = 0.45f;
	{
		const float *sc = TheTerrainShadowPass.shadeColour;
		floorLum = 0.299f * (0.45f + (sc[0] - 0.45f) * sc[3]) + 0.587f * (0.45f + (sc[1] - 0.45f) * sc[3]) + 0.114f * (0.45f + (sc[2] - 0.45f) * sc[3]);
	}
	const float shared[5 * 4] =
	{
		zNear, zFar, (s_view >= 4) ? 0.0f : (float)s_view, s_over,		// views 4 and 5 are the lit frame with something drawn over it
		planeMin.X, planeMax.Y, planeW, planeH,
		vpMin.X, vpMin.Y, vpMax.X - vpMin.X, vpMax.Y - vpMin.Y,
		1.0f / targetW, 1.0f / targetH, shadeUse, s_sunErase * (1.0f - s_sunNow),		// Ronin @bugfix 16/10/2026 DX9: sunerase not by day
		factor[0], factor[1], factor[2], floorLum
	};
	dev->SetPixelShaderConstantF(0, shared, 5);
	// Ronin @bugfix 25/10/2026 DX9: the map's lights for the frame's light by each pixel's facing (c44..c50): ambient summed, then per
	// light the way to it in view space and its diffuse. Off with `boost`, its own factor.
	{
		float frameLights[7 * 4];
		memset(frameLights, 0, sizeof(frameLights));
		frameLights[3] = (s_boost <= 0.0f && TheGlobalData != NULL) ? 1.0f : 0.0f;
		if (TheGlobalData != NULL)
		{
			const Matrix3D &viewTM = camera.Get_Transform();
			for (Int i = 0; i < MAX_GLOBAL_LIGHTS && i < 3; ++i)
			{
				const GlobalData::TerrainLighting &tl = TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][i];
				frameLights[0] += tl.ambient.red;
				frameLights[1] += tl.ambient.green;
				frameLights[2] += tl.ambient.blue;
				Vector3 toLight(-tl.lightPos.x, -tl.lightPos.y, -tl.lightPos.z), toLightView;
				const float len = toLight.Length();
				if (len > 0.0001f)
					toLight /= len;
				Matrix3D::Inverse_Rotate_Vector(viewTM, toLight, &toLightView);
				float *dir = frameLights + 4 + i * 8, *diff = dir + 4;
				dir[0] = toLightView.X; dir[1] = toLightView.Y; dir[2] = toLightView.Z;
				diff[0] = (len > 0.0001f) ? tl.diffuse.red : 0.0f;
				diff[1] = (len > 0.0001f) ? tl.diffuse.green : 0.0f;
				diff[2] = (len > 0.0001f) ? tl.diffuse.blue : 0.0f;
			}
		}
		dev->SetPixelShaderConstantF(44, frameLights, 7);
	}
	// Ronin @bugfix 11/10/2026 DX9: TAA shifts the frame by part of a pixel, another each frame (camera.cpp). Rebuilt from the depth as
	// if it had not, a point is up to 0.15 units off its surface: the pools of lamps close to what they light flickered in shadow.
	float jitterX = 0.0f, jitterY = 0.0f;
	R3DTaa::getJitterNDC(&jitterX, &jitterY);
	const float jitter[4] = { -0.5f * jitterX, 0.5f * jitterY, s_rays, 1.0f + (s_daySun - 1.0f) * s_sunNow };		// the shift, in viewport widths and heights; z: `lights rays`
	dev->SetPixelShaderConstantF(15, jitter, 1);
	// Ronin @feature 16/10/2026 DX9: c33.x - a local light's strength in a sun shadow, by the time of day (`dayshade`).
	const float dayShade[4] = { 1.0f + (s_dayShade - 1.0f) * s_sunNow, 0.0f, 0.0f, 0.0f };
	dev->SetPixelShaderConstantF(33, dayShade, 1);
	// Ronin @feature 11/10/2026 DX9: the world's axes in view space, for every light: a blast's level maps are picked by world direction.
	Vector3 worldX, worldY, worldZ;
	Matrix3D::Inverse_Rotate_Vector(camera.Get_Transform(), Vector3(1.0f, 0.0f, 0.0f), &worldX);
	Matrix3D::Inverse_Rotate_Vector(camera.Get_Transform(), Vector3(0.0f, 1.0f, 0.0f), &worldY);
	Matrix3D::Inverse_Rotate_Vector(camera.Get_Transform(), Vector3(0.0f, 0.0f, 1.0f), &worldZ);
	// Ronin @feature 11/10/2026 DX9: their w: what else every light shares - the spill's strength and 1 / its width, `lights rayturn`.
	const float axes[3 * 4] = { worldX.X, worldX.Y, worldX.Z, s_spill, worldY.X, worldY.Y, worldY.Z, 1.0f / s_spillWide,
								worldZ.X, worldZ.Y, worldZ.Z, s_rayTurn ? 1.0f : 0.0f };
	dev->SetPixelShaderConstantF(30, axes, 3);

	for (DWORD stage = 0; stage < 4; ++stage)
	{
		dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	dev->SetTexture(0, depthTex);
	dev->SetTexture(1, (s_copy != NULL) ? s_copy->Peek_D3D_Texture() : NULL);	// Ronin @bugfix 02/11/2026 DX9: none taken yet
	dev->SetTexture(2, (shadeUse > 0.0f && meshShade) ? s_shadeTex->Peek_D3D_Texture() : NULL);
	dev->SetTexture(3, (shadeUse > 0.0f) ? groundShade : NULL);
	// Ronin @feature 11/10/2026 DX9: the lamps' shadow maps: this frame's atlas on s4, last frame's on s6, LINEAR - the sampler
	// compares 2x2 texels and blends.
	IDirect3DBaseTexture9 *mapsNow = (s_lampDrawn && s_lampShadows > 0.0f && s_lampDepth[s_lampFlip] != NULL)
									 ? s_lampDepth[s_lampFlip]->Peek_D3D_Base_Texture() : NULL;
	IDirect3DBaseTexture9 *mapsWas = (s_lampPrevCount > 0 && s_lampDepth[s_lampFlip ^ 1] != NULL)
									 ? s_lampDepth[s_lampFlip ^ 1]->Peek_D3D_Base_Texture() : NULL;
	const Bool readMaps = (mapsNow != NULL) ? TRUE : FALSE;
	for (DWORD stage = 4; stage <= 6; stage += 2)
	{
		dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}
	dev->SetTexture(4, mapsNow);
	dev->SetTexture(6, readMaps ? mapsWas : NULL);
	const D3DXMATRIX viewToWorld(To_D3DMATRIX(camera.Get_Transform()));
	float noMap[14 * 4];						// tiles of no size: the shader finds every pixel outside them, so lit
	memset(noMap, 0, sizeof(noMap));
	noMap[52] = -1.0f;							// c29.x: no level maps
	Bool mapUp = TRUE;							// whatever c16..c29 hold is not ours yet

	// Ronin @feature 09/10/2026 DX9: the layout goes through the wrapper - a raw SetFVF leaves its cached layout stale.
	DX8Wrapper::BindLayoutFVF(D3DFVF_XYZ | D3DFVF_TEX1, "R3DLocalLights::renderPass");
	dev->SetVertexShader(s_quadVS);		// AFTER the layout bind - BindLayoutFVF clears the VS
	dev->SetPixelShader(s_lightPS);

	// Ronin @feature 15/10/2026 DX9: THE GROUND AS A LIGHT (docs/Lighting_Work.md 4.4c). One quad over the frame,
	// twice: once adding, once darkening. Before the lamps, which add to it; its own constants go back for them.
	if (bounceMap != NULL)
	{
		const Vector3 eye = camera.Get_Transform().Get_Translation();
		float bc[3 * 4] = { eye.X, eye.Y, meshShade ? 1.0f : 0.0f, (groundShade != NULL) ? 1.0f : 0.0f,
							bounceUV[0], bounceUV[1], bounceUV[2], bounceUV[3],
							bounceAmb[0], bounceAmb[1], bounceAmb[2], s_bounce };
		dev->SetPixelShaderConstantF(5, bc, 3);
		dev->SetTexture(2, meshShade ? s_shadeTex->Peek_D3D_Texture() : NULL);
		dev->SetTexture(3, groundShade);
		dev->SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(5, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(5, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(5, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		dev->SetTexture(5, bounceMap);
		const float bl = (-0.5f / targetW) * 2.0f - 1.0f, br = ((targetW - 0.5f) / targetW) * 2.0f - 1.0f;
		const float bt = 1.0f - (-0.5f / targetH) * 2.0f, bb = 1.0f - ((targetH - 0.5f) / targetH) * 2.0f;
		struct BounceVertex { float x, y, z; float u, v; };
		const BounceVertex bq[4] = { { bl, bt, 0.0f, 0.0f, 0.0f }, { br, bt, 0.0f, 1.0f, 0.0f }, { bl, bb, 0.0f, 0.0f, 1.0f }, { br, bb, 0.0f, 1.0f, 1.0f } };
		// Ronin @diagnostic 15/10/2026 DX9: `lights view 6` - drawn once, over the frame, as grey round its middle.
		if (s_view == 6)
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		// Ronin @feature 18/10/2026 DX9: nothing into the reflections' change mask: the second half (dest x src) wiped it.
		DWORD maskWrites = 0;
		if (masked)
		{
			dev->GetRenderState(D3DRS_COLORWRITEENABLE1, &maskWrites);
			dev->SetRenderState(D3DRS_COLORWRITEENABLE1, 0);
		}
		// Ronin @bugfix 02/11/2026 DX9: both halves in one pass (c0.w 2): the shader writes (1 + k) / 2, blended dest x src x 2.
		const float c0[4] = { shared[0], shared[1], 7.0f, (s_view == 6) ? 0.0f : 2.0f };
		dev->SetPixelShaderConstantF(0, c0, 1);
		dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_DESTCOLOR);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, bq, sizeof(BounceVertex));
		if (masked)
			dev->SetRenderState(D3DRS_COLORWRITEENABLE1, maskWrites);
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		dev->SetPixelShaderConstantF(0, shared, 1);
		dev->SetTexture(2, (shadeUse > 0.0f && meshShade) ? s_shadeTex->Peek_D3D_Texture() : NULL);
		dev->SetTexture(3, (shadeUse > 0.0f) ? groundShade : NULL);
		dev->SetTexture(5, NULL);
	}

	for (Int i = 0; i < count; ++i)
	{
		const LightDraw &d = draws[i];
		dev->SetPixelShaderConstantF(5, d.c, 10);
		const Bool mapNow = (readMaps && d.map >= 0) ? TRUE : FALSE;
		const Bool mapWas = (readMaps && mapsWas != NULL && d.mapWas >= 0 && d.mapWas < s_lampPrevCount) ? TRUE : FALSE;
		if (mapNow || mapWas)
		{
			// Ronin @feature 11/10/2026 DX9: c16-c22 this frame's map, c23-c28 last frame's map of the same light - through THIS
			// frame's view, so one pixel asks both about the same place. A map that is missing stays a tile of no size.
			float lamp[14 * 4];
			memset(lamp, 0, sizeof(lamp));
			lamp[52] = -1.0f;
			if (mapNow && d.sides >= 0)
			{
				// Ronin @feature 11/10/2026 DX9: c29 - a blast's four level maps: the first tile, the two terms of their depth, a texel.
				const LampMap &sm = s_lampMaps[d.sides];
				lamp[52] = (float)d.sides;
				lamp[53] = sm.farPlane / (sm.farPlane - sm.nearPlane);
				lamp[54] = sm.nearPlane * sm.farPlane / (sm.farPlane - sm.nearPlane);
				lamp[55] = 2.0f * sm.tanHalf / (float)LAMP_TILE;
			}
			if (mapNow)
			{
				lampRegisters(s_lampMaps[d.map], d.map, viewToWorld, lamp, lamp + 16);
				lamp[20] = 2.0f * s_lampMaps[d.map].tanHalf / (float)LAMP_TILE;		// a texel's width at depth 1
			}
			lamp[21] = LAMP_BIAS_NORMAL * s_lampBias; lamp[22] = LAMP_BIAS_LIGHT * s_lampBias; lamp[23] = LAMP_BIAS_UNITS * s_lampBias;
			lamp[24] = 0.5f / (float)LAMP_ATLAS; lamp[25] = s_lampShadows;
			if (mapWas)
			{
				lampRegisters(s_lampMapsPrev[d.mapWas], d.mapWas, viewToWorld, lamp + 28, lamp + 44);
				lamp[48] = 2.0f * s_lampMapsPrev[d.mapWas].tanHalf / (float)LAMP_TILE;
			}
			dev->SetPixelShaderConstantF(16, lamp, 14);
			mapUp = TRUE;
		}
		else if (mapUp)
		{
			dev->SetPixelShaderConstantF(16, noMap, 14);
			mapUp = FALSE;
		}

		// Ronin @feature 09/10/2026 DX9: D3D9 texel centres sit half a pixel off pixel centres: the geometry moves, the UVs stay exact.
		const float l = ((d.x0 - 0.5f) / targetW) * 2.0f - 1.0f;
		const float r = ((d.x1 - 0.5f) / targetW) * 2.0f - 1.0f;
		const float t = 1.0f - ((d.y0 - 0.5f) / targetH) * 2.0f;
		const float b = 1.0f - ((d.y1 - 0.5f) / targetH) * 2.0f;
		const float u0 = d.x0 / targetW, u1 = d.x1 / targetW;
		const float v0 = d.y0 / targetH, v1 = d.y1 / targetH;
		struct QuadVertex { float x, y, z; float u, v; } q[4];
		q[0].x = l; q[0].y = t; q[0].z = 0.0f; q[0].u = u0; q[0].v = v0;
		q[1].x = r; q[1].y = t; q[1].z = 0.0f; q[1].u = u1; q[1].v = v0;
		q[2].x = l; q[2].y = b; q[2].z = 0.0f; q[2].u = u0; q[2].v = v1;
		q[3].x = r; q[3].y = b; q[3].z = 0.0f; q[3].u = u1; q[3].v = v1;
		dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadVertex));
	}
	s_stats.drawn = lightCount;
	if (masked)
		R3DTaa::endLightMask();
	if (shaftsWanted)
		drawSunShafts(dev, depthTex, rd, camera, 10);		// Ronin @feature 22/10/2026 DX9: after the lights - the air is in front of what they lit

	// Ronin @feature 11/10/2026 DX9: the lights are in: the frame's depth goes back before the maps' view, which writes it.
	dev->SetTexture(0, NULL);
	dev->SetTexture(4, NULL);
	dev->SetTexture(6, NULL);
	DX8Wrapper::Resume_Scene_Depth();

	// Ronin @diagnostic 11/10/2026 DX9: `lights view 4` - the lamps' shadow maps, a tile each, from the top right of the view, over the
	// lit frame. Written at the nearest depth: the water and the effects, drawn later, covered them.
	if (s_view == 4 && s_lampDrawn && s_lampColour != NULL && s_lampColour->Peek_D3D_Texture() != NULL)
	{
		const float showMaps[4] = { zNear, zFar, 4.0f, s_over };
		dev->SetPixelShaderConstantF(0, showMaps, 1);
		DWORD zFuncWas;
		dev->GetRenderState(D3DRS_ZFUNC, &zFuncWas);
		dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		dev->SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		dev->SetSamplerState(5, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetSamplerState(5, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(5, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		dev->SetTexture(5, s_lampColour->Peek_D3D_Texture());
		const Int   columns = 6;
		const float side    = floorf((vx1 - vx0) / 12.0f);
		const float share   = (float)LAMP_TILE / (float)LAMP_ATLAS;
		for (Int i = 0; i < s_lampMapCount; ++i)
		{
			const float x0 = vx1 - (float)(columns - (i % columns)) * (side + 2.0f);
			const float y0 = vy0 + 2.0f + (float)(i / columns) * (side + 2.0f);
			if (y0 + side > vy1)
				break;
			const float depths[4] = { s_lampMaps[i].nearPlane, s_lampMaps[i].farPlane, 0.0f, 0.0f };
			dev->SetPixelShaderConstantF(5, depths, 1);
			const float l = ((x0 - 0.5f) / targetW) * 2.0f - 1.0f;
			const float r = ((x0 + side - 0.5f) / targetW) * 2.0f - 1.0f;
			const float t = 1.0f - ((y0 - 0.5f) / targetH) * 2.0f;
			const float b = 1.0f - ((y0 + side - 0.5f) / targetH) * 2.0f;
			const float u0 = (float)(i % LAMP_TILES_ROW) * share, u1 = u0 + share;
			const float v0 = (float)(i / LAMP_TILES_ROW) * share, v1 = v0 + share;
			struct TileVertex { float x, y, z; float u, v; } q[4];
			q[0].x = l; q[0].y = t; q[0].z = 0.0f; q[0].u = u0; q[0].v = v0;
			q[1].x = r; q[1].y = t; q[1].z = 0.0f; q[1].u = u1; q[1].v = v0;
			q[2].x = l; q[2].y = b; q[2].z = 0.0f; q[2].u = u0; q[2].v = v1;
			q[3].x = r; q[3].y = b; q[3].z = 0.0f; q[3].u = u1; q[3].v = v1;
			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(TileVertex));
		}
		dev->SetTexture(5, NULL);
		dev->SetRenderState(D3DRS_ZFUNC, zFuncWas);
	}

	// Ronin @diagnostic 12/10/2026 DX9: `lights view 5` - the sky cube as a panorama across the top of the view: all round from left
	// to right, straight up at its top edge, straight down at its bottom. Over the lit frame, at the nearest depth as view 4 is.
	if (s_view == 5 && R3DSkyCube::peek() != NULL)
	{
		const float showSky[4] = { zNear, zFar, 5.0f, s_over };
		dev->SetPixelShaderConstantF(0, showSky, 1);
		DWORD zFuncWas;
		dev->GetRenderState(D3DRS_ZFUNC, &zFuncWas);
		dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		dev->SetSamplerState(7, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(7, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(7, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		dev->SetTexture(7, R3DSkyCube::peek());
		const float wide = floorf((vx1 - vx0) * 0.6f), high = floorf(wide * 0.5f);
		const float x0 = vx0 + floorf((vx1 - vx0 - wide) * 0.5f), y0 = vy0 + 2.0f;
		const float l = ((x0 - 0.5f) / targetW) * 2.0f - 1.0f;
		const float r = ((x0 + wide - 0.5f) / targetW) * 2.0f - 1.0f;
		const float t = 1.0f - ((y0 - 0.5f) / targetH) * 2.0f;
		const float b = 1.0f - ((y0 + high - 0.5f) / targetH) * 2.0f;
		struct SkyVertex { float x, y, z; float u, v; };
		const SkyVertex q[4] = { { l, t, 0.0f, 0.0f, 0.0f }, { r, t, 0.0f, 1.0f, 0.0f }, { l, b, 0.0f, 0.0f, 1.0f }, { r, b, 0.0f, 1.0f, 1.0f } };
		dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(SkyVertex));
		dev->SetTexture(7, NULL);
		dev->SetRenderState(D3DRS_ZFUNC, zFuncWas);
	}

	// Ronin @feature 09/10/2026 DX9: unbind, then the 3D viewport, the frame's depth, states and constants; the wrapper re-asserts itself.
	dev->SetTexture(1, NULL);
	dev->SetTexture(2, NULL);
	dev->SetTexture(3, NULL);
	if (haveVP)
		dev->SetViewport(&oldVP);
	dev->SetRenderState(D3DRS_ZENABLE,          zEnable);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     zWrite);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, alphaBlend);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  alphaTest);
	dev->SetRenderState(D3DRS_STENCILENABLE,    stencil);
	dev->SetRenderState(D3DRS_CULLMODE,         cull);
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, colorWrite);
	dev->SetRenderState(D3DRS_SRCBLEND,         srcBlend);
	dev->SetRenderState(D3DRS_DESTBLEND,        destBlend);
	if (haveConstants)
		dev->SetPixelShaderConstantF(0, savedConstants, 34);
	if (haveFrameLights)
		dev->SetPixelShaderConstantF(44, savedFrameLights, 7);

	dev->SetVertexShader(NULL);
	dev->SetPixelShader(NULL);
	DX8Wrapper::Invalidate_Cached_Render_States();
}

void R3DLocalLights::shutdown(void)
{
	DX8InstanceManagerClass::Set_Sky_Reflection(NULL, 0.0f);	// Ronin @feature 12/10/2026 DX9: the mesh renderer holds the cube too
	DX8InstanceManagerClass::Set_Reflection_Ground(NULL, NULL);	// Ronin @feature 18/10/2026 DX9: ... and the ground map
	DX8InstanceManagerClass::Set_Screen_Reflection(NULL, NULL, NULL, NULL, NULL);	// Ronin @feature 18/10/2026 DX9: ... and last frame's
	DX8InstanceManagerClass::Set_Reflect_Mask_Hooks(NULL, NULL);
	R3DSkyCube::shutdown();
	// Ronin @feature 09/10/2026 DX9: the beams: every swapped texture gets its own surface back, and meshes stop asking.
	MeshClass::Set_Light_Fixture_Note_Func(NULL);
	DX8InstanceManagerClass::Set_Sun_Shade_Hooks(NULL, NULL);
	MeshClass::Set_Light_Caster_Note_Func(NULL);
	s_casterOpen = FALSE;
	endSunShade();
	s_shadeOpen = FALSE;
	s_shadeFrame = FALSE;
	REF_PTR_RELEASE(s_shadeTex);
	s_shadeW = 0;
	s_shadeH = 0;
	REF_PTR_RELEASE(s_cloudTex);		// Ronin @feature 22/10/2026 DX9: the sun shafts' cloud gaps
	releaseShaftBuffers();
	s_beamsOn = FALSE;
	s_collect = FALSE;
	for (Int i = 0; i < s_beamTexCount; ++i)
	{
		restoreBeamTexture(s_beamTex[i]);
		s_beamTex[i].tex->Release_Ref();
	}
	s_beamTexCount = 0;
	if (s_clearTex != NULL)
	{
		s_clearTex->Release();
		s_clearTex = NULL;
	}
	REF_PTR_RELEASE(s_copy);
	s_copyW = 0;
	s_copyH = 0;
	s_copyFormat = D3DFMT_UNKNOWN;
	REF_PTR_RELEASE(s_lampColour);
	REF_PTR_RELEASE(s_lampDepth[0]);
	REF_PTR_RELEASE(s_lampDepth[1]);
	s_lampPrevCount = 0;
	s_lampFailed   = FALSE;
	s_lampDrawn    = FALSE;
	s_lampMapCount = 0;
	if (s_lampVS != NULL)
	{
		s_lampVS->Release();
		s_lampVS = NULL;
	}
	if (s_lampPS != NULL)
	{
		s_lampPS->Release();
		s_lampPS = NULL;
	}
	if (s_lightPS != NULL)
	{
		s_lightPS->Release();
		s_lightPS = NULL;
	}
	if (s_quadVS != NULL)
	{
		s_quadVS->Release();
		s_quadVS = NULL;
	}
	s_shadersTried = FALSE;
	s_active = FALSE;
}
