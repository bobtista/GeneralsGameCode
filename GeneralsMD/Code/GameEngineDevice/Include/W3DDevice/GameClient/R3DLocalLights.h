/*
**	Command & Conquer Generals Zero Hour(tm)
**	DX9 local lights, drawn in screen space.
*/

// Ronin @feature 09/10/2026 DX9: local lights (explosions, weapon flashes) per pixel from the scene depth, over terrain and
// meshes alike. While it runs, the per-vertex terrain re-light and the per-object light stand down (ownsDynamicLights).

#pragma once

#ifndef __W3DLOCALLIGHTS_H_
#define __W3DLOCALLIGHTS_H_

#include "Lib/BaseType.h"

class CameraClass;
class RTS3DScene;
class MeshClass;

class R3DLocalLights
{
public:
	static void beginFrame(void);			// W3DDisplay::draw, after R3DTaa::beginFrame: can this frame run
	static void renderPass(const CameraClass &camera, RTS3DScene *scene);	// RTS3DScene::Flush, after the trees
	static void endFrame(void);				// after R3DTaa::endFrame: ends the scene depth if this pass began it
	static void shutdown(void);				// frees the shaders and the frame copy while the device exists

	// Ronin @feature 09/10/2026 DX9: TRUE while this pass draws the scene's dynamic lights - the old paths must not add them again.
	static Bool ownsDynamicLights(void);
	// Ronin @feature 09/10/2026 DX9: TRUE = W3DDisplay::createLightPulse keeps the pulses it drops as too small for the 10-unit
	// vertex grid (26 of the game's 65). Per pixel they show.
	static Bool wantsSmallLights(void);
	// Ronin @feature 17/10/2026 DX9: R3DSsao runs its view depth prepass under MSAA for this too, SSAO or not: the lights need a depth.
	static Bool wantsDepthPrepass(void);
	// Ronin @feature 17/10/2026 DX9: an engine pulse's minimum reach (inner + width) and fade frames while the lights run - called
	// by W3DDisplay::createLightPulse. `lights pulsemin` / `pulsefade`, 0 = as the FX asks.
	static void minimumPulse(Real innerRadius, Real *attenuationWidth, UnsignedInt *decayFrames);
	static void  setPulseMin(float v);
	static float getPulseMin(void);
	static void  setPulseFade(float v);
	static float getPulseFade(void);
	// Ronin @feature 17/10/2026 DX9: `lights searchlight` - the police car's 2003 light lights round its beacons (0: our glow lights do).
	static void  setSearchLight(Bool on);
	static Bool  getSearchLight(void);

	// Ronin @feature 09/10/2026 DX9: the light fixtures' beams. MeshClass::Render asks this about every mesh once: 0 = no light fixture,
	// 1 = one, draw it, 2 = one whose beam a spot light replaces this frame: do not draw it. Lighting_Work.md 4.2.
	static int  noteFixtureMesh(MeshClass *mesh);
	// Ronin @feature 11/10/2026 DX9: lamp shadow maps. MeshClass::Render calls this for every mesh it draws; kept for the frame
	// (main pass, solid meshes) so each lamp's map can be drawn from what stands in its light. Lighting_Work.md 6b.
	static void noteCaster(MeshClass *mesh);
	// Ronin @feature 09/10/2026 DX9: TRUE = this mesh's beam polygons are clear this frame (their texture is swapped); TAA must not
	// treat the mesh as a moving-texture translucent one on their account.
	static Bool beamsHiddenIn(const MeshClass *mesh);

	// Ronin @diagnostic 09/10/2026 DX9: the panel's `lights` knobs. Session only, not saved.
	// Ronin @feature 12/10/2026 DX9: every `lights` knob by its name - one table for the debug panel and for DX9Lights.ini, a
	// modder's file in the game's Data\INI (`lights save` writes it; read at the first frame and by `lights load`).
	struct Knob
	{
		const char *name;
		Bool        whole;				// a switch or a count: printed and written without decimals
		Bool        saved;				// FALSE: not in the file (the debug view)
		void      (*set)(float);		// held to the knob's own range by its setter
		float     (*get)(void);
	};
	static Int         knobCount(void);
	static const Knob *knobAt(Int index);
	static const Knob *findKnob(const char *name);
	static void        iniPath(char *out, Int size);
	static Int         loadIni(Int *skipped);	// how many values were set, or -1 with no file; `skipped`: lines it could not use
	static Bool        saveIni(void);
	static void        resetDefaults(void);		// the built-in values; the file is untouched

	static void  setEnabled(Bool on);		// Ronin @feature 23/10/2026 DX9: `lights 0|1`, the master: all the new lighting, HDR too
	static Bool  isEnabled(void);
	static void  setEffects(Bool on);		// ... `lights locallighting 0|1`: the lamps - lights, beams, glows, effects, lamp shadows
	static Bool  isEffects(void);
	static void  setIntensity(float v);		// multiplies every light's colour
	static float getIntensity(void);
	static void  setOver(float v);			// how far above the surface's own colour a light may push a pixel; 1 = the old limit
	static float getOver(void);
	static void  setBoost(float v);			// frame-to-surface factor; 0 = worked out from the map's light values
	static float getBoost(void);
	static void  setRange(float v);			// multiplies every light's reach
	static float getRange(void);
	static void  setSmall(Bool on);			// see wantsSmallLights
	static Bool  getSmall(void);
	static void  setView(Int v);			// 0 off, 1 the light alone, 2 the facing used, 3 the rectangles drawn, 4 the lamps' shadow maps
	static Int   getView(void);
	static void  setBeams(float v);			// the beams' strength as lights; 0 = the art's own beams, no lights
	static float getBeams(void);
	static void  setShafts(float v);		// the beam in the air, drawn by the shader: its strength beside the light's; 0 = none
	static float getShafts(void);
	static void  setRays(float v);			// soft rays from the lamp in that beam; 0 = the smooth cone
	static float getRays(void);
	static void  setLampWidth(float v);		// how wide a beam leaves its lamp, in the art's own width there; 0 = from a point
	static float getLampWidth(void);
	static void  setRayTurn(Bool on);		// the rays turn with the world as the camera goes round a beam; off = they stay with the view
	static Bool  getRayTurn(void);
	static void  setSpill(float v);			// the dim light around a beam's cone: its strength beside the cone's; 0 = none
	static float getSpill(void);
	static void  setSpillWide(float v);		// ... and how wide it reaches, in cone widths
	static float getSpillWide(void);
	static void  setGlows(float v);			// the lamp glows as point lights: their strength; 0 = none
	static float getGlows(void);
	static void  setGlowReach(float v);		// multiplies every glow light's reach
	static float getGlowReach(void);
	static void  setGlowJoin(float v);		// lamps this near each other (art units) light as one, each keeping its corona; 0 = apart
	static float getGlowJoin(void);
	static void  setCoronas(float v);		// the lamp's own glow drawn by the shader, its strength; 0 = the art's glow textures
	static float getCoronas(void);
	static void  setCoronaSize(float v);	// its radius, in the art glow's own
	static float getCoronaSize(void);
	static void  setPools(Bool keep);		// TRUE = the art's painted pools of light (and welded glows) stay drawn
	static Bool  getPools(void);
	static void  setBeamReach(float v);		// a beam's light on surfaces ends this far along, in the art cone's own length
	static float getBeamReach(void);
	static void  setPoolFill(float v);		// strength of the light that stands in for a painted pool no lamp covers; 0 = none
	static float getPoolFill(void);
	static void  setHotspot(float v);		// the share of a beam's cone at full strength; 0 = it fades from the axis out
	static float getHotspot(void);
	// Ronin @feature 11/10/2026 DX9: lamp shadow maps: a depth map per beam, drawn from its lamp, and read by its light (stage B).
	static void  setLampMaps(Bool on);		// FALSE = no maps drawn, so no lamp shadows
	static Bool  getLampMaps(void);
	static void  setLampShadows(float v);	// how much a beam's shadow takes; 0 = the maps are drawn and not read
	static float getLampShadows(void);
	static void  setLampBias(float v);		// multiplies the receiver's bias: lower = tighter contact, more risk of stripes
	static float getLampBias(void);
	static void  setLampSelf(Bool on);		// TRUE = the lamp's own object casts in its light too
	static Bool  getLampSelf(void);
	static void  setLampSelfNear(float v);	// ... but only past this share of the beam's length: a housing round the bulb casts nothing
	static float getLampSelfNear(void);
	static void  setPointMaps(Bool on);		// TRUE = the engine's point lights (firing flashes, blasts) get a map too, straight down
	static Bool  getPointMaps(void);
	static void  setPointSides(Bool on);	// TRUE = a big point light low over the ground (a blast) gets four level maps as well
	static Bool  getPointSides(void);
	// Ronin @feature 10/10/2026 DX9: the sun's shade, for the lamps. Called round the mesh shader's draws (DX8InstanceManagerClass
	// hooks): binds a second target that takes the sun's shade (RigidInstance_ps COLOR1), in the main pass only.
	static void  beginSunShade(void);
	static void  endSunShade(void);
	static void  setSunFill(float v);		// how much of the sun's shade a lamp undoes on a mesh; 0 = none (as before)
	static float getSunFill(void);
	static void  setSunErase(float v);		// how fast a lamp restores what the sun's shadow took: gone at lamp strength 1 / this; 0 = never
	static float getSunErase(void);
	static void  setMapShadow(float v);		// 1 = a sun shadow leaves what this map's light is without its sun; 0 = 0.45 on every map
	static float getMapShadow(void);
	// Ronin @feature 12/10/2026 DX9: lighting, step 3. SSAO shades the map's ambient, not its sun. For SSAO's show shader: the sun's shade
	// records on stages 1 and 2, their floor in pixel constant c0 (w = `lights aosun`, 0 = SSAO as before), which exist in c1.
	static void  bindSunShadeForAO(Bool afterTrees);	// Ronin @bugfix 04/11/2026 DX9: TRUE = the light pass comes next, nothing between
	static void  setAoSun(float v);			// how far SSAO steps back where the sun reaches: 1 = down to the ambient's share; 0 = not at all
	static float getAoSun(void);
	// Ronin @feature 12/10/2026 DX9: lighting, step 5. What a reflective mesh mirrors: 0 = the art's one picture (as before), 1 = the
	// map's sky cube, 2 = the cube with the sky in every pane (seen from above, a pane mirrors the ground).
	static void  setReflect(float v);
	static float getReflect(void);
	// Ronin @feature 18/10/2026 DX9: reflections - one looking down takes the ground it meets (`reflectground` 0..1, 0 = the sky as
	// before); their strength by the view's angle (`fresnel`, 0 = off); `ssr`: screen-space reflections from last frame.
	static void  setReflectGround(float v);
	static float getReflectGround(void);
	static void  setFresnel(float v);
	static float getFresnel(void);
	static void  setSsr(Bool on);
	static Bool  getSsr(void);
	// Ronin @feature 12/10/2026 DX9: HDR stage 1b. W3DTreeBuffer says its shade record (Trees_ps COLOR1) is this frame's: the sun
	// lift then gives a canopy its own sun, not the ground's behind it.
	static void  noteTreeShade(void);
	// Ronin @feature 15/10/2026 DX9: the ground as a light: how much of what the ground sends up a face takes
	// (1 = by the ambient's measure, 0 = off).
	static void  setBounce(float v);
	static float getBounce(void);
	// Ronin @feature 22/10/2026 DX9: sun shafts - beams through the gaps in the cloud shadows: their strength per 100 units of lit
	// air (0 = off); how bright the cloud shadow must be to let the sun through (lower = wider gaps); how high the lit air reaches.
	static void  setSunShafts(float v);
	static float getSunShafts(void);
	static void  setShaftGaps(float v);
	static float getShaftGaps(void);
	static void  setShaftLayer(float v);
	static float getShaftLayer(void);
	static void  setShaftDust(float v);		// Ronin @feature 23/10/2026 DX9: the dust in the shafts, its strength (0 = none)
	static float getShaftDust(void);
	static void  drawShaftDust(void);		// ... drawn by R3DTaa::noteOpaqueDone, after its copy of the frame before the effects
	// Ronin @feature 15/10/2026 DX9: how far each time of day is "day" for the local lights, 0..1: the sun up full, dusk half, the
	// moon none. 1 = `daysun` / `dayshade` in full; 0 = the lights at full strength and `sunerase` in full.
	static void  setSunTime(Int timeOfDay, float v);		// 1 morning, 2 afternoon, 3 evening, 4 night
	static float getSunTime(Int timeOfDay);
	static void  setDaySun(float v);		// by day, a local light's strength where the sun shines (1 = all of it)
	static float getDaySun(void);
	static void  setDayShade(float v);		// by day, its strength in a sun shadow (1 = all of it)
	static float getDayShade(void);
	// Ronin @feature 15/10/2026 DX9: global light item 7 - glowing effects (additive particle systems) as point lights in their own
	// colour: how many (the strongest first; 0 = none), their strength and their reach.
	static void  setFxLights(float v);
	static float getFxLights(void);
	static void  setFxStrength(float v);
	static float getFxStrength(void);
	static void  setFxReach(float v);
	static float getFxReach(void);
	static void  setFxMerge(Bool on);		// glowing systems at one spot become one light (a blast's shock wave, flare, flames)
	static Bool  getFxMerge(void);
	static void  setFxFlares(Bool on);		// lens flares make effect lights too (they are a camera's glare: off by default)
	static void  setFxFire(float v);		// a fire reaches at least its size + this x its brightness share (fire casts light round it)
	static float getFxFire(void);
	// Ronin @feature 19/10/2026 DX9: how much of last frame's light an effect light keeps (at 30 fps) - it eases into a new strength,
	// reach and place instead of jumping; 0 = none.
	static void  setFxSmooth(float v);
	static float getFxSmooth(void);
	static Bool  getFxFlares(void);

	// Ronin @diagnostic 09/10/2026 DX9: last frame's numbers, for the panel's [LIGHT] row.
	struct Stats
	{
		Bool  active;			// the pass could run: shaders loaded and a readable scene depth
		Bool  ownsDepth;		// it began the scene depth itself - neither SSAO nor TAA had
		Int   live;				// enabled lights in the scene
		Int   drawn;			// of those, drawn
		float coverage;			// summed rectangle area, in screens
		Bool  masked;			// TAA took the change mask this frame
		float sceneLight[3];	// the frame-to-surface factor in use
		Int   beams;			// beams that became spot lights
		Int   glows;			// lamp glows that became point lights
		Int   pools;			// painted pools no lamp covers that got a light of their own
		Int   fx;				// Ronin @feature 15/10/2026 DX9: glowing effects drawn as lights (item 7)
		Int   fxCandidates;		// ... those with weight on screen
		Int   fxGlowing;		// ... the additive systems walked
		Int   sunShade;			// the sun's shade the lights read this frame: 1 = the meshes', 2 = the terrain's, 3 = both
		Int   casters;			// lamp shadow maps: solid meshes drawn this frame, the casters to choose from
		Int   lampMaps;			// ... lights that got a map
		Int   lampPoints;		// ... of them the engine's point lights (the rest are beams)
		Int   lampSides;		// ... and of those, the ones with four level maps as well (blasts)
		Int   lampDraws;		// ... draws into the maps, summed
		Int   lampMost;			// ... the most any one map took
		Int   lampTris;			// ... and their triangles, summed
		Int   lampOwn;			// ... casters left out as the lamp's own object (`lights lampself 0`)
		Int   lampSkins;		// ... and skinned meshes there was no room to deform
		Int   beamMeshes;		// beam-only meshes left undrawn (every pass counts)
		Int   beamModels;		// beam models read so far
		float gpuScene;			// Ronin @diagnostic 20/10/2026 DX9: GPU ms - the 3D scene up to the light pass; -1 = not measured
		float gpuShade;			// ... the sun's-shade pass
		float gpuLights;		// ... the light pass, the shade pass included
	};
	static void getStats(Stats *out);
	static void releaseQueries(void);		// Ronin @diagnostic 20/10/2026 DX9: the GPU timer's, before a device reset
	static void releaseShaftBuffers(void);	// Ronin @feature 22/10/2026 DX9: the sun shafts' half-size buffers, likewise
};

#endif
