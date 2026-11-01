/*
**	Command & Conquer Generals Zero Hour(tm)
**	DX9 port: the look of the reflective water (WaterType 2).
*/

// Ronin @feature 29/09/2026 DX9: every knob of the WaterSea look, set live by the debug panel's `water` command. Header
// only - C++17 inline variables, so a new knob touches no .cpp. docs/Water_Work.md.
// Ronin @feature 02/10/2026 DX9: phase 4 - one look profile per water kind (sea, lake, river), plus the switches that
// are global. The panel edits the profile `water profile` selects.
#pragma once

#include <cstddef>		// Ronin @feature 04/10/2026 DX9: offsetof, for the knob tables below
#include <cstring>
#include "Lib/BaseType.h"

// Ronin @feature 02/10/2026 DX9: phase 4 - the water kinds. Sea = a standing polygon reaching the map's edge.
enum WaterKind { WATER_KIND_SEA = 0, WATER_KIND_LAKE, WATER_KIND_RIVER, WATER_KIND_COUNT };

// Ronin @feature 02/10/2026 DX9: phase 4 - switches for all water at once.
struct WaterSeaGlobal
{
	Int  view;			///< 0 = the water; debug: 1 Fresnel, 2 N.V, 3 normal, 4 sun only, 5 reflection only, 6 body only, 7 depth, 8 foam, 9 shore distance, 10 swell (red crests, blue troughs, green weight), 11 shadow, 12 opacity mode, 13 shore-foam gates, 14 the wake texture
	Bool depth;			///< per-pixel shore, depth tint and foam (0 = the old destination-alpha shore)
	Bool refraction;	///< the frame under the water bent by the normals (off = the plain alpha blend)
	Bool oldWaves;		///< the vanilla shore-wave sprites as well as the shore foam
	Bool mirror;		///< reflections: render the mirror each frame; off = no mirror render and no reflection in the water
	Int  mirrorRes;		///< 1 = the screen's size, 2 = half of it (at most 1024 wide); the knob takes no other (Ronin, 08/10)
	Bool mirrorClip;	///< off = the old mirror, where the seabed seen from below covers the reflection
	Real mirrorFar;		///< the mirror camera's far plane, x the view's (1 = the view's own)
	Real mirrorSoft;	///< Ronin @feature 08/10/2026 DX9: four more taps this many mirror texels out, to 1.5; 0 = the single tap
	Real ride;			///< Ronin @feature 03/10/2026 DX9: phase 5 - how much floating units ride the swell: 0 = off, 1 = with the surface
	// Ronin @feature 03/10/2026 DX9: phase 5 - wakes: moving units leave a trail on the water (W3DWater.cpp renderWakes).
	Real wakes;			///< their strength - the shape's shading and the foam; 0 = off, nothing is drawn
	Real wakeFoam;		///< the foam's share of it
	Real wakeHeight;	///< the wake's broad swell, x its own height - the mesh and its shading together; 0 = none
	Real wakeShade;		///< Ronin @feature 04/10/2026 DX9: how far the wake's slopes tilt the lit normal, x the true slope (the swell's is 2)
	Real wakeSharp;		///< Ronin @feature 25/10/2026 DX9: the wake's crests: 0 = round, 1 = pointed (as `swellsharp` for the swell)
	Real wakeTint;		///< Ronin @feature 25/10/2026 DX9: wake crests lighter, its hollow darker (as `swelltint` for the swell)
	// Ronin @feature 04/10/2026 DX9: what a hull does to the water without a wake, as RA3's do.
	Real wakeRings;		///< the rings round a unit at rest; x their height, 0 = off
	Real wakeSurge;		///< the wave that runs on ahead of a unit that stops; x its height, 0 = off
	Bool oldWakes;		///< the vanilla wake sprites as well, on units the water draws a wake for
	Int  edit;			///< the profile the panel's knobs edit, a WaterKind
};

// Ronin @feature 29/09/2026 DX9: the look of one water kind.
struct WaterSeaTuning
{
	Real reflect;		///< reflection strength; Fresnel scales it when on
	Bool normals;		///< the two normal-map layers (FALSE = the old caust ripple, reflection only)
	Real ripple;		///< Ronin @feature 02/10/2026 DX9: ripple strength, the normal maps' slopes x this; 0 = mirror-flat, 1 = as authored
	Bool fresnel;		///< Schlick Fresnel scales the reflection
	Real f0;			///< Fresnel reflectance looking straight down (real water 0.02)
	Real fresnelPow;	///< Schlick exponent (physical 5; lower = more reflection away from vertical)
	Real fresnelRipple;	///< Ronin @bugfix 02/10/2026 DX9: how much the ripples move the Fresnel weight; 0 = the swell's only
	Real distort;		///< how far the normals move the reflection lookup (screen uv)
	Real refract;		///< how far the normals move the water texture lookup (texture uv) - the texture only; `bend` moves the seabed
	Real shade;			///< light and shade the normals put on the water body
	Real spec;			///< sharp sun glitter strength
	Real gloss;			///< sharp glitter power
	Real sheen;			///< broad sun sheen strength
	Real sheenPow;		///< broad sheen power
	Real speed;			///< normal-map scroll speed, x the base rates; a river's: its current, world units a second (flow map)
	Real tile;			///< normal-map size, x the base tile sizes (190 and 110 world units)
	// Ronin @feature 29/09/2026 DX9: phase 1 - water depth from the terrain heights (Water_Work.md §5b). Shore widths are
	// distances from the waterline (depth / terrain slope), not depths: ZH's shallow shelves run for hundreds of units.
	Real clearDepth;	///< world units over which `deep` takes full effect
	Real deep;			///< deep-water brightness, x the map's water colour; 1 = no darkening
	Bool ownOpacity;	///< Ronin @bugfix 01/10/2026 DX9: the map's own see-through (vanilla's soft shore), not `opacity`
	Real opacity;		///< past the soft shore, never above the map's MinOpacity; the seabed shows through the rest
	Real shore;			///< the soft shore fades in over this many world units from the waterline
	Real shoreDepth;	///< shore effects (foam, see-through) stop by this depth +- 1: an underwater cliff is no shore
	Real foam;			///< shore foam amount: how much of the pattern passes, 0 = off, ~4 = a solid band
	Real foamWidth;		///< how far the foam reaches from the waterline, world units
	Real foamTile;		///< foam pattern size, world units per tile
	Real texture;		///< how much of the map's water texture shows, 0..1; 1 = as the map made it
	// Ronin @feature 29/09/2026 DX9: the open-water lace.
	Real seaFoam;		///< strength, 0 = off
	Real seaFoamDepth;	///< fades out by this depth, world units
	Real seaFoamTile;	///< pattern size, world units per tile
	// Ronin @feature 29/09/2026 DX9: phase 2 - Gerstner swell on the flat water (Water_Work.md §5b).
	Bool swell;			///< on / off
	Real swellHeight;	///< amplitude, x the wave table's
	Real swellSize;		///< wavelength, x the wave table's
	Real swellSpeed;	///< x the deep-water speed
	Real swellDir;		///< wind direction, degrees from +x
	Real swellDepth;	///< Ronin @bugfix 03/10/2026 DX9: full height from this depth, world units (troughs stop above the seabed)
	Real swellCell;		///< grid cell under the swell, world units (vanilla's is about 40)
	// Ronin @feature 03/10/2026 DX9: phase 5 - waves that read from an RTS camera.
	Real swellSharp;	///< crest shape: 0 = round (circular orbits, phase 2's), 1 = pointed crests and wide troughs
	Real swellTint;		///< crests lighter and greener, troughs darker, by the wave's height; 0 = off
	Real whitecaps;		///< foam on the highest crests, with a short trail behind them; 0 = off
	Real whitecapAt;	///< how high a crest must be to break, x the default sea's total wave height (0.55; lower = more foam)
	Bool breakers;		///< the shore foam's wash and incoming front keep the swell's time: a front lands with each crest
	// Ronin @feature 29/09/2026 DX9: phase 3 - true refraction, the frame under the water bent by the normals.
	Real bend;			///< how far the normals bend it, screen uv
	// Ronin @feature 29/09/2026 DX9: shadows on the surface.
	Real shadow;		///< how much a shadow darkens the water body, 0 = no shadows (the sun's glitter always goes)
	// Ronin @feature 01/11/2026 DX9: the angle between the two ripple layers; low = the wind's streaks line up.
	Real cross;			///< degrees layer B turns from layer A (it was fixed at 60)
};

// Ronin @feature 04/10/2026 DX9: the built-in values on their own, so `water defaults` can return to them.
inline const WaterSeaGlobal TheWaterSeaGlobalDefault =
{
	0,			// view
	TRUE,		// depth
	TRUE,		// refraction
	TRUE,		// oldWaves - kept by default (Ronin, 29/09)
	TRUE,		// mirror
	1,			// mirrorRes - the screen's full size, for `mirrorsoft` to average (Ronin, 08/10; half size since 30/09)
	TRUE,		// mirrorClip
	4.0f,		// mirrorFar
	0.5f,		// mirrorSoft - calms the crawl of cut-out edges without the smear of higher values (Ronin, 08/10)
	1.0f,		// ride
	1.0f,		// wakes
	0.8f,		// wakeFoam
	4.0f,		// wakeHeight
	1.0f,		// wakeShade - as the swell's normal boost
	0.5f,		// wakeSharp
	0.45f,		// wakeTint - what `swelltint` gave wakes until it had its own
	2.0f,		// wakeRings
	3.0f,		// wakeSurge
	FALSE,		// oldWakes - the water's wake replaces them; `water oldwakes 1` brings them back (Ronin, 03/10)
	WATER_KIND_SEA,	// edit
};
inline WaterSeaGlobal TheWaterSeaGlobal = TheWaterSeaGlobalDefault;

// Ronin @feature 29/09/2026 DX9: defaults. The body refraction, shade, glitter and speed are what make it read as water
// from an RTS camera; Fresnel alone leaves ~10% reflection there.
// Ronin @feature 02/10/2026 DX9: phase 4 - the base every kind starts from, then each kind's differences.
inline WaterSeaTuning WaterSea_DefaultProfile(WaterKind kind)
{
	WaterSeaTuning t =
	{
		1.0f,		// reflect
		TRUE,		// normals
		1.0f,		// ripple
		TRUE,		// fresnel
		0.15f,		// f0 - with fresnelRipple 0.4: contrast in the reflection, no mercury (Ronin, 02/10)
		5.0f,		// fresnelPow
		0.5f,		// fresnelRipple (Ronin, 02/10)
		0.015f,		// distort - halved: the reflection wobbled into shapeless blobs (Ronin, 29/09)
		0.06f,		// refract
		0.5f,		// shade
		1.0f,		// spec
		250.0f,		// gloss
		0.10f,		// sheen
		24.0f,		// sheenPow
		5.0f,		// speed
		1.0f,		// tile
		40.0f,		// clearDepth
		1.0f,		// deep - no darkening until tuned
		FALSE,		// ownOpacity
		0.70f,		// opacity - see-through like RA3, not a glass of water (Ronin, 29/09)
		15.0f,		// shore
		2.0f,		// shoreDepth (Ronin, 29/09)
		1.3f,		// foam - how much of the pattern passes; 1.2 ~ max foam under the old gain (29/09)
		12.0f,		// foamWidth
		80.0f,		// foamTile
		1.0f,		// texture - the map's own
		0.6f,		// seaFoam
		4.0f,		// seaFoamDepth - halved: it whitened too much of a shallow sea (Ronin, 29/09)
		40.0f,		// seaFoamTile
		TRUE,		// swell
		0.7f,		// swellHeight
		1.0f,		// swellSize
		1.0f,		// swellSpeed
		30.0f,		// swellDir
		4.0f,		// swellDepth - world units; the old 1.5 x the 3.1-unit table was ~4.7 (03/10)
		10.0f,		// swellCell - the sea's short waves need it; no fps cost measured (Ronin, 03/10)
		0.4f,		// swellSharp
		0.45f,		// swellTint
		0.9f,		// whitecaps
		0.55f,		// whitecapAt - foam over ~2.5% of the sea at `swellheight` 1 (py preview, 03/10)
		TRUE,		// breakers
		0.02f,		// bend
		0.5f,		// shadow
		15.0f,		// cross - streaks along the wind, the preview Ronin picked (01/11); 60 crossed them into glitter
	};
	// Ronin @feature 02/10/2026 DX9: phase 4 - each kind's differences from the base. 
	// A lake keeps the map's own see-through and swells slow and low.
	if (kind == WATER_KIND_LAKE)
	{
		t.ownOpacity  = TRUE;
		t.speed       = 2.0f;		// (Ronin, 02/10)
		t.swellHeight = 0.4f;		// (Ronin, 02/10)
		t.swellSpeed  = 0.3f;		// (Ronin, 02/10)
		t.ripple      = 0.3f;		// the sun gathers into one broad highlight (Ronin, 02/10) - tuned on the sea's map
		t.gloss       = 180.0f;		// (Ronin, 02/10)
		t.sheen       = 0.15f;		// (Ronin, 02/10)
		// Ronin @feature 03/10/2026 DX9: phase 5 - the lake keeps phase 2's swell as it was settled: the calm wave table
		// (R3DWaterSea.h), round crests, no tint, the 20-unit grid.
		t.swellCell   = 20.0f;
		t.swellSharp  = 0.0f;
		t.swellTint   = 0.0f;
		t.whitecaps   = 0.0f;
		t.breakers    = FALSE;		// its 210-unit waves at speed 0.3 land every 21 s: the lake keeps the foam's own clock
		t.cross       = 60.0f;		// Ronin @feature 01/11/2026 DX9: as it was; the lake's map has no wind
	}
	// Ronin @feature 02/10/2026 DX9: phase 4 - a river's current and bank foam.
	// Ronin @bugfix 03/10/2026 DX9: the bank foam erodes a bubble field: `foam` = the share of the bank foamed at the
	// waterline, the reach varies to 15% of `foamwidth` along the bank.
	else if (kind == WATER_KIND_RIVER)
	{
		t.speed       = 8.0f;		// the current, world units a second
		t.foam        = 0.8f;
		t.foamWidth   = 10.0f;		// 20 reached too far for the river's size (Ronin, 03/10)
		t.foamTile    = 35.0f;		// big bubbles ~4.5 units, small ~2.5 (Ronin, 03/10)
		t.seaFoamTile = 60.0f;		// the lace's 360 cells (-LaceCells): ~3-unit cells (Ronin, 03/10)
		t.cross       = 60.0f;		// Ronin @feature 01/11/2026 DX9: as it was; the river's map has no wind
	}
	return t;
}

inline WaterSeaTuning TheWaterSeaProfiles[WATER_KIND_COUNT] =
{
	WaterSea_DefaultProfile(WATER_KIND_SEA),
	WaterSea_DefaultProfile(WATER_KIND_LAKE),
	WaterSea_DefaultProfile(WATER_KIND_RIVER),
};

// Ronin @feature 02/10/2026 DX9: phase 4 - the profile for a water kind, out-of-range = the sea's.
inline WaterSeaTuning &WaterSea_Profile(Int kind)
{
	return TheWaterSeaProfiles[(kind >= 0 && kind < WATER_KIND_COUNT) ? kind : WATER_KIND_SEA];
}

// Ronin @feature 04/10/2026 DX9: every knob by its name - one table for the debug panel's `water` command and for the
// water ini (R3DWaterSeaIni.h). A knob added to a struct above and to its table here is tunable in the panel and saved,
// with nothing else to touch. `saved` FALSE: not in the ini (the debug view; `mirror`, which is Options.ini's
// DX9WaterReflections).
enum WaterKnobKind { WATER_KNOB_REAL = 0, WATER_KNOB_BOOL, WATER_KNOB_INT };
struct WaterKnobInfo
{
	const char *name;
	Int         kind;		///< a WaterKnobKind
	size_t      offset;		///< into WaterSeaTuning (a profile's knob) or WaterSeaGlobal (all water's)
	Real        lo, hi;		///< the range it is held to; a switch is 0 / 1
	Bool        saved;
};

inline const WaterKnobInfo TheWaterProfileKnobs[] =
{
	{ "refl",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, reflect), 0.0f, 1.0f, TRUE },
	{ "normals",        WATER_KNOB_BOOL, offsetof(WaterSeaTuning, normals), 0.0f, 1.0f, TRUE },
	{ "ripple",         WATER_KNOB_REAL, offsetof(WaterSeaTuning, ripple), 0.0f, 2.0f, TRUE },
	{ "fresnel",        WATER_KNOB_BOOL, offsetof(WaterSeaTuning, fresnel), 0.0f, 1.0f, TRUE },
	{ "f0",             WATER_KNOB_REAL, offsetof(WaterSeaTuning, f0), 0.0f, 1.0f, TRUE },
	{ "fpow",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, fresnelPow), 0.5f, 10.0f, TRUE },
	{ "fresnelripple",  WATER_KNOB_REAL, offsetof(WaterSeaTuning, fresnelRipple), 0.0f, 1.0f, TRUE },
	{ "distort",        WATER_KNOB_REAL, offsetof(WaterSeaTuning, distort), 0.0f, 0.5f, TRUE },
	{ "refract",        WATER_KNOB_REAL, offsetof(WaterSeaTuning, refract), 0.0f, 0.5f, TRUE },
	{ "shade",          WATER_KNOB_REAL, offsetof(WaterSeaTuning, shade), 0.0f, 4.0f, TRUE },
	{ "spec",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, spec), 0.0f, 16.0f, TRUE },
	{ "gloss",          WATER_KNOB_REAL, offsetof(WaterSeaTuning, gloss), 1.0f, 4096.0f, TRUE },
	{ "sheen",          WATER_KNOB_REAL, offsetof(WaterSeaTuning, sheen), 0.0f, 8.0f, TRUE },
	{ "sheenpow",       WATER_KNOB_REAL, offsetof(WaterSeaTuning, sheenPow), 1.0f, 512.0f, TRUE },
	{ "speed",          WATER_KNOB_REAL, offsetof(WaterSeaTuning, speed), 0.0f, 50.0f, TRUE },
	{ "tile",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, tile), 0.1f, 10.0f, TRUE },
	{ "cross",          WATER_KNOB_REAL, offsetof(WaterSeaTuning, cross), 0.0f, 90.0f, TRUE },	// Ronin @feature 01/11/2026 DX9
	{ "deepdist",       WATER_KNOB_REAL, offsetof(WaterSeaTuning, clearDepth), 1.0f, 500.0f, TRUE },
	{ "deep",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, deep), 0.0f, 1.0f, TRUE },
	{ "ownopacity",     WATER_KNOB_BOOL, offsetof(WaterSeaTuning, ownOpacity), 0.0f, 1.0f, TRUE },
	{ "opacity",        WATER_KNOB_REAL, offsetof(WaterSeaTuning, opacity), 0.0f, 1.0f, TRUE },
	{ "shore",          WATER_KNOB_REAL, offsetof(WaterSeaTuning, shore), 0.0f, 200.0f, TRUE },
	{ "shoredepth",     WATER_KNOB_REAL, offsetof(WaterSeaTuning, shoreDepth), 0.5f, 100.0f, TRUE },
	{ "foam",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, foam), 0.0f, 4.0f, TRUE },
	{ "foamwidth",      WATER_KNOB_REAL, offsetof(WaterSeaTuning, foamWidth), 0.5f, 100.0f, TRUE },
	{ "foamtile",       WATER_KNOB_REAL, offsetof(WaterSeaTuning, foamTile), 1.0f, 500.0f, TRUE },
	{ "texture",        WATER_KNOB_REAL, offsetof(WaterSeaTuning, texture), 0.0f, 1.0f, TRUE },
	{ "seafoam",        WATER_KNOB_REAL, offsetof(WaterSeaTuning, seaFoam), 0.0f, 4.0f, TRUE },
	{ "seafoamdepth",   WATER_KNOB_REAL, offsetof(WaterSeaTuning, seaFoamDepth), 0.5f, 100.0f, TRUE },
	{ "seafoamtile",    WATER_KNOB_REAL, offsetof(WaterSeaTuning, seaFoamTile), 1.0f, 500.0f, TRUE },
	{ "swell",          WATER_KNOB_BOOL, offsetof(WaterSeaTuning, swell), 0.0f, 1.0f, TRUE },
	{ "swellheight",    WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellHeight), 0.0f, 10.0f, TRUE },
	{ "swellsize",      WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellSize), 0.1f, 10.0f, TRUE },
	{ "swellspeed",     WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellSpeed), 0.0f, 10.0f, TRUE },
	{ "swelldir",       WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellDir), -360.0f, 360.0f, TRUE },
	{ "swelldepth",     WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellDepth), 0.5f, 50.0f, TRUE },
	{ "swellcell",      WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellCell), 5.0f, 80.0f, TRUE },
	{ "swellsharp",     WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellSharp), 0.0f, 1.0f, TRUE },
	{ "swelltint",      WATER_KNOB_REAL, offsetof(WaterSeaTuning, swellTint), 0.0f, 1.5f, TRUE },
	{ "whitecaps",      WATER_KNOB_REAL, offsetof(WaterSeaTuning, whitecaps), 0.0f, 2.0f, TRUE },
	{ "whitecapat",     WATER_KNOB_REAL, offsetof(WaterSeaTuning, whitecapAt), 0.0f, 1.5f, TRUE },
	{ "breakers",       WATER_KNOB_BOOL, offsetof(WaterSeaTuning, breakers), 0.0f, 1.0f, TRUE },
	{ "bend",           WATER_KNOB_REAL, offsetof(WaterSeaTuning, bend), 0.0f, 0.2f, TRUE },
	{ "shadow",         WATER_KNOB_REAL, offsetof(WaterSeaTuning, shadow), 0.0f, 1.0f, TRUE },
};
inline const Int TheWaterProfileKnobCount = (Int)(sizeof(TheWaterProfileKnobs) / sizeof(TheWaterProfileKnobs[0]));

inline const WaterKnobInfo TheWaterGlobalKnobs[] =
{
	{ "view",           WATER_KNOB_INT, offsetof(WaterSeaGlobal, view), 0.0f, 14.0f, FALSE },
	{ "depth",          WATER_KNOB_BOOL, offsetof(WaterSeaGlobal, depth), 0.0f, 1.0f, TRUE },
	{ "refraction",     WATER_KNOB_BOOL, offsetof(WaterSeaGlobal, refraction), 0.0f, 1.0f, TRUE },
	{ "oldwaves",       WATER_KNOB_BOOL, offsetof(WaterSeaGlobal, oldWaves), 0.0f, 1.0f, TRUE },
	{ "mirror",         WATER_KNOB_BOOL, offsetof(WaterSeaGlobal, mirror), 0.0f, 1.0f, FALSE },
	{ "mirrorres",      WATER_KNOB_INT, offsetof(WaterSeaGlobal, mirrorRes), 1.0f, 2.0f, TRUE },
	{ "mirrorclip",     WATER_KNOB_BOOL, offsetof(WaterSeaGlobal, mirrorClip), 0.0f, 1.0f, TRUE },
	{ "mirrorfar",      WATER_KNOB_REAL, offsetof(WaterSeaGlobal, mirrorFar), 1.0f, 20.0f, TRUE },
	{ "mirrorsoft",     WATER_KNOB_REAL, offsetof(WaterSeaGlobal, mirrorSoft), 0.0f, 1.5f, TRUE },
	{ "ride",           WATER_KNOB_REAL, offsetof(WaterSeaGlobal, ride), 0.0f, 2.0f, TRUE },
	{ "wakes",          WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakes), 0.0f, 4.0f, TRUE },
	{ "wakefoam",       WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeFoam), 0.0f, 4.0f, TRUE },
	{ "wakeheight",     WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeHeight), 0.0f, 8.0f, TRUE },
	{ "wakeshade",      WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeShade), 0.0f, 12.0f, TRUE },
	{ "wakesharp",      WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeSharp), 0.0f, 1.0f, TRUE },
	{ "waketint",       WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeTint), 0.0f, 1.0f, TRUE },
	{ "wakerings",      WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeRings), 0.0f, 4.0f, TRUE },
	{ "wakesurge",      WATER_KNOB_REAL, offsetof(WaterSeaGlobal, wakeSurge), 0.0f, 4.0f, TRUE },
	{ "oldwakes",       WATER_KNOB_BOOL, offsetof(WaterSeaGlobal, oldWakes), 0.0f, 1.0f, TRUE },
};
inline const Int TheWaterGlobalKnobCount = (Int)(sizeof(TheWaterGlobalKnobs) / sizeof(TheWaterGlobalKnobs[0]));

inline const WaterKnobInfo *WaterSea_FindKnob(const WaterKnobInfo *table, Int count, const char *name)
{
	for (Int k = 0; k < count; k++)
		if (_stricmp(table[k].name, name) == 0)
			return &table[k];
	return nullptr;
}

inline Real WaterSea_GetKnob(const void *base, const WaterKnobInfo &k)
{
	const char *at = (const char *)base + k.offset;
	if (k.kind == WATER_KNOB_REAL)
		return *(const Real *)at;
	if (k.kind == WATER_KNOB_BOOL)
		return *(const Bool *)at ? 1.0f : 0.0f;
	return (Real)*(const Int *)at;
}

// Held to the knob's range; anything that is not a number (atof's 0) lands on 0 or the low end.
inline void WaterSea_SetKnob(void *base, const WaterKnobInfo &k, Real value)
{
	if (!(value >= k.lo))
		value = k.lo;
	if (value > k.hi)
		value = k.hi;
	char *at = (char *)base + k.offset;
	if (k.kind == WATER_KNOB_REAL)
		*(Real *)at = value;
	else if (k.kind == WATER_KNOB_BOOL)
		*(Bool *)at = (value != 0.0f) ? TRUE : FALSE;
	else
		*(Int *)at = (Int)value;
}

// Ronin @feature 04/10/2026 DX9: back to the built-in look (`water defaults`). The profile being edited, the debug view
// and `mirror` (Options.ini's) stay as they are.
inline void WaterSea_ResetDefaults()
{
	const Int  view   = TheWaterSeaGlobal.view;
	const Bool mirror = TheWaterSeaGlobal.mirror;
	const Int  edit   = TheWaterSeaGlobal.edit;
	TheWaterSeaGlobal = TheWaterSeaGlobalDefault;
	TheWaterSeaGlobal.view   = view;
	TheWaterSeaGlobal.mirror = mirror;
	TheWaterSeaGlobal.edit   = edit;
	for (Int k = 0; k < WATER_KIND_COUNT; k++)
		TheWaterSeaProfiles[k] = WaterSea_DefaultProfile((WaterKind)k);
}
