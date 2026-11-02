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

// Ronin @feature 20/09/2026 DX9: Temporal anti-aliasing. STEP 1 is the jitter only — no history buffer and no resolve
// yet, so switching it on looks like a faint shimmer. That shimmer IS the subpixel sampling TAA will later accumulate,
// and seeing it is how step 1 is verified. docs/AntiAliasing_Work.md.
//
// The jitter reaches the device through CameraClass::Apply and NOTHING else: Apply takes a local copy of the projection
// (camera.cpp:752-754), so ProjectionTransform itself stays clean and every consumer that reads the camera for picking,
// placement, culling or the view plane keeps the unjittered matrix. That separation is the whole safety argument.

#pragma once

#include "Lib/BaseType.h"

struct IDirect3DTexture9;

class CameraClass;
class MeshClass;
class Matrix3D;
class Matrix4x4;
class Vector3;
class TextureClass;

class R3DTaa
{
public:

	// Ronin @feature 20/09/2026 DX9: one step of the sample sequence, called once per frame before the views draw.
	// Does nothing while disabled, so the sequence does not advance and the jitter stays zero.
	static void beginFrame(void);

	// Ronin @feature 20/09/2026 DX9: wraps the 3D scene render so it lands in a texture we can read back. preRender
	// returns TRUE when it took the redirect, and ONLY then may postRender run. Both no-op when TAA is off or when a
	// script's view filter already owns the render-to-texture path - those are cinematic and win.
	// Ronin @feature 17/10/2026 DX9: with TAA off and MSAA off, HDR alone takes the redirect too - the same pair runs.
	static Bool preRender(const CameraClass &camera);
	static void postRender(void);
	// Ronin @feature 17/10/2026 DX9: when preRender declined - TAA off or MSAA on: the night look and the dark lift in a pass of
	// their own over the scene just drawn. Nothing at all by day.
	static void finishScene(const CameraClass &camera);

	// Ronin @feature 20/09/2026 DX9: reprojection needs the INTZ scene depth. SSAO owns that buffer when it is on; when
	// it is off TAA begins it, and then TAA must end it. Called beside R3DSsao's pair in W3DDisplay::draw.
	static void endFrame(void);
	// Ronin @feature 09/10/2026 DX9: TRUE while TAA holds the scene depth this frame - R3DLocalLights begins it only if nobody did.
	static Bool ownsDepth(void);

	// Frees shaders and targets while the device still exists. Called from W3DDisplay's shutdown, beside R3DSsao's.
	static void shutdown(void);
	// Ronin @bugfix 07/10/2026 DX9: before a device reset. Drops the GPU timer queries - a reset fails while one is alive.
	static void releaseResources(void);

	// The current sample offset in NDC units, ready to add to the projection's off-centre terms. Zero when disabled.
	// CameraClass::Apply is the only caller.
	static void getJitterNDC(float *outX, float *outY);

	// Ronin @feature 20/09/2026 DX9: on/off - Options.ini `DX9TAA = yes|no` at start, `taa 0|1` on the debug panel after.
	// isActive = enabled AND MSAA off; TAA does nothing under MSAA (no readable depth).
	static Bool isEnabled(void) { return s_enabled; }
	static void setEnabled(Bool on);
	static Bool isActive(void);

	// Ronin @diagnostic 20/09/2026 DX9: runtime knobs (`taa <knob> <value>`).
	// weight - history cap; debug - 1 reproj delta, 2 depth, 3 samples, 4 history, 13 velocity/reactive mask, 14 moving-shadow
	// mask, 15 mesh motion per frame;
	// sharpen - CAS strength 0..1 on the way to the screen, 0 = off; clamp - neighbourhood clamp strength where there is evidence of change.
	static void  setWeight(float w);
	static float getWeight(void);
	static void  setDebug(Int mode);
	static Int   getDebug(void);
	static void  setSharpen(float s);
	static float getSharpen(void);
	// Ronin @feature 12/10/2026 DX9: HDR stage 1a. The scene's redirect target (the engine's render texture) as 16-bit float, kept so
	// across device resets. Redirected while TAA runs, or for HDR alone (TAA off, MSAA off); unchanged until something writes above 1.
	static void  setFloatScene(Bool on);
	static Bool  getFloatScene(void);
	static void  describeScene(char *out, Int size);	// one line for the panel's `hdr`
	// Ronin @feature 12/10/2026 DX9: HDR stage 1b. The sun brighter than 1 (R3DLocalLights lifts the sun's share of the frame by the
	// gain of this time of day, tinted by `warm`), and `white`: the scene value the resolve's tone curve shows as white.
	static void  setSunGain(float v);
	static float getSunGain(void);
	static const CameraClass *getSceneCamera(void);	// Ronin @feature 21/10/2026 DX9: the 3D view's camera this frame (preRender)
	static void  setSunGainAt(Int slot, float v);	// Ronin @feature 21/10/2026 DX9: by slot, 0 morning .. 3 night (DX9Lights.ini)
	static float getSunGainAt(Int slot);
	static void  setSunWarm(float v);
	static float getSunWarm(void);
	static void  setWhite(float v);
	static float getWhite(void);
	static void  setSpill(float v);		// Ronin @bugfix 02/11/2026 DX9: `hdr spill` 0..1 - past white, colours run to white
	static float getSpill(void);
	static Bool  getSunLift(float rgb[3]);	// FALSE: no float scene this frame, or no gain - nothing to draw
	// Ronin @feature 21/10/2026 DX9: THE GRADE - the daytime colour in the final pass, never at night: warm 0 = none, 1 = the measured
	// tint; sat 1 = none; mids = the mid-tones' power, 1 = none. Off by default.
	static void  setGrade(Bool on);
	static Bool  getGrade(void);
	static void  setGradeWarm(float v);
	static float getGradeWarm(void);
	static void  setGradeSat(float v);
	static float getGradeSat(void);
	static void  setGradeMids(float v);
	static float getGradeMids(void);
	// Ronin @feature 22/10/2026 DX9: vibrance (dull colours raised most) and local contrast, by day in the final pass; 0 = off.
	static void  setVibrance(float v);
	static float getVibrance(void);
	static void  setLocalContrast(float v);
	static float getLocalContrast(void);
	static void  setNightVibrance(float v);		// Ronin @feature 23/10/2026 DX9: the same at night - only on what keeps its colour
	static float getNightVibrance(void);
	static void  setNightContrast(float v);
	static float getNightContrast(void);
	// Ronin @feature 15/10/2026 DX9: THE NIGHT LOOK (TAA's pass 2 or finishScene): dim colour goes to one tint (the map light's cast),
	// bright keeps its own. tint 0..1; lo / hi: the luminance where a pixel starts / ends keeping its colour; floor: the least it keeps.
	static void  setNight(Bool on);
	static Bool  getNight(void);
	static void  setNightTint(float v);
	static float getNightTint(void);
	static void  setNightLo(float v);
	static float getNightLo(void);
	static void  setNightHi(float v);
	static float getNightHi(void);
	static void  setNightFloor(float v);
	static float getNightFloor(void);
	static void  setNightLift(float v);		// the dark lift at night: luminance l -> l^this, the colour kept; 1 = none
	static float getNightLift(void);
	static void  setNightFull(float v);		// the map light's saturation at which the look is at full strength; milder lights get less
	static float getNightFull(void);
	static void  describeNight(char *out, Int size);	// one line for the panel's `night`
	// Ronin @feature 26/09/2026 DX9: mipbias - texture mip LOD bias while TAA runs, -2..0; default -1 (Options.ini DX9TAAMipBias).
	static void  setMipBias(float b);
	static float getMipBias(void);
	static void  setClamp(float v);
	static float getClamp(void);
	// Ronin @bugfix 27/09/2026 DX9: shadowmask - clamp still pixels in or near a moving mesh's shadow, on/off.
	static void  setShadowMask(Bool on);
	static Bool  getShadowMask(void);
	static void  setMaskCap(Bool on);		// Ronin @bugfix 27/09/2026 DX9: half history weight where a moving shadow changed
	static Bool  getMaskCap(void);
	static Int   getShadowMaskDraws(void);	// meshes drawn into the mask this frame, -1 = not drawn
	// Ronin @diagnostic 27/09/2026 DX9: GPU ms of postRender and of the mask pass, smoothed; -1 = no timestamp queries.
	static void  getGpuMs(float *total, float *mask);

	// Ronin @feature 24/09/2026 DX9: §14 mesh motion vectors. MeshClass::Render reports every mesh it draws outside the
	// shadow depth pass; a mesh that moved since last frame is re-drawn into the velocity target. vel - on/off.
	static void noteMesh(MeshClass *mesh);
	static void setVelocity(Bool on);
	static Bool getVelocity(void);
	static Int  getMoverCount(void);		// meshes in the velocity pass this frame
	static Int  getMeshSeen(void);

	// Ronin @feature 24/09/2026 DX9: CameraClass::Apply reports the clean projection and view it sends for the TAA
	// camera; the velocity pass and the reprojection use them.
	static void  noteCameraApplied(const Matrix4x4 &d3dProjClean, const Matrix3D &view);

	// Ronin @feature 24/09/2026 DX9: reactive mask - scrolling textures and moving translucent meshes. 0 off, else the
	// history weight cap on those pixels (1 = clamp only). The count is this frame's reactive draws.
	static void  setReactive(float w);
	static float getReactive(void);
	static Int   getReactiveCount(void);
	static Int   getSkinDrawCount(void);	// Ronin @feature 26/09/2026 DX9: skinned meshes in the velocity pass this frame
	static Int   getTrackMisses(void);	// Ronin @diagnostic 25/09/2026 DX9: meshes with no slot this frame; should be 0

	// disocc - reset the history where a mover was last frame and is not now: 0 off, 1 this pixel, 2 plus the edge band.
	static void  setDisocc(Int m);
	static Int   getDisocc(void);
	// Ronin @bugfix 26/09/2026 DX9: disoccv - a mover vacates a pixel only if it moved at least this many px (0 = any).
	// Ronin @bugfix 27/09/2026 DX9: idle infantry only now (their breathing); every other mesh vacates at any distance.
	static void  setDisoccV(float px);
	static float getDisoccV(void);

	// Ronin @feature 26/09/2026 DX9: auto-reactive. RTS3DScene::Flush calls noteOpaqueDone just before particles; the
	// resolve clamps wherever the final frame differs from that copy by more than autoreact (0 = off).
	static void  noteOpaqueDone(void);
	static void  setAutoReact(float t);
	static float getAutoReact(void);
	static void  setAutoFull(float t);	// Ronin @bugfix 08/10/2026 DX9: fully reactive from this difference on (`taa autofull`)
	static float getAutoFull(void);
	static Bool  getOpaqueOK(void);
	// Ronin @bugfix 26/09/2026 DX9: W3DTerrainVisual::addFactionBibDrawable hands over the build-placement bib's 4 world
	// corners; TAA flags that quad reactive for a few frames so the bib leaves no trail.
	static void  noteCursorBib(const Vector3 *corners);
	// Ronin @bugfix 26/10/2026 DX9: W3DProjectedShadowManager::queueDecal hands over every alpha / additive decal it draws (the
	// ability rings on the cursor, a unit's decal): centre, 4 corner offsets, texture, and whether it rides a unit.
	static void  noteMovingDecal(const void *key, const Vector3 &centre, const Vector3 *corners, TextureClass *texture, Bool additive,
								 Bool attached);
	// Ronin @bugfix 29/09/2026 DX9: the map's water polygons are flagged reactive in the velocity pass - animated every frame,
	// drawn by a custom render object neither the mesh pass nor the auto-reactive copy sees. `taa water 0|1`.
	static void  setWaterMask(Bool on);
	static Bool  getWaterMask(void);
	static Int   getWaterMaskTris(void);	// water triangles flagged this frame
	// Ronin @bugfix 03/10/2026 DX9: the water marks its own pixels (WaterSea_ps COLOR1). Around a water draw: begin binds
	// the mask as render target 1 and returns TRUE - then call end after the draw. FALSE = not this frame: draw as usual.
	static Bool  beginWaterMask(void);
	static void  endWaterMask(void);
	static Bool  getWaterMaskDrawn(void);	// last frame: the water marked itself (else the flat polygons)
	// Ronin @bugfix 09/10/2026 DX9: the local lights mark where a light CHANGED a pixel since last frame (LocalLight_ps COLOR1); the
	// resolve clamps there. Around R3DLocalLights' draws, as the water's pair.
	static Bool  beginLightMask(void);
	static void  endLightMask(void);
	// Ronin @bugfix 09/10/2026 DX9: the selection flash is drawn after the resolve, not through the history. The scene asks
	// ownsSelectionFlash before adding a drawable's flash to its lights; TRUE = leave it out.
	static Bool  ownsSelectionFlash(void);
	// Ronin @bugfix 09/10/2026 DX9: 1 / the map's own light on open ground, per channel: frame x this ~ the surface's colour, which the
	// flash is added to.
	static void  getMapLightFactor(float out[3]);
	// Ronin @feature 18/10/2026 DX9: screen-space reflections - last frame's picture and depth (borrowed for this frame), c16..c21,
	// and the frame before with c69..c74 for the reflections' change mask; FALSE: no whole last frame.
	static Bool  getLastFrame(IDirect3DTexture9 **colour, IDirect3DTexture9 **depth, float c16to21[24], IDirect3DTexture9 **older,
							  float c69to74[24]);
	// Ronin @feature 19/10/2026 DX9: ... and without TAA the frame keeps them itself (finishScene) while this is set, each frame.
	static void  setLastFrameWanted(Bool on);
	// Ronin @feature 18/10/2026 DX9: the reflections' change mask. Round a reflective draw (Set_Reflect_Mask_Hooks) begin binds the
	// light mask's red as target 1 and returns TRUE, then end after the draw. `taa reflmark 0..2`.
	static Bool  beginReflectMask(void);
	static void  endReflectMask(void);
	static void  setReflMark(Int mode);
	static Int   getReflMark(void);

	// Samples in the sequence before it repeats. 8 is the usual choice: long enough to look supersampled, short enough
	// that a stopped camera settles quickly.
	enum { SAMPLE_COUNT = 8 };

private:

	static Bool s_enabled;
};
