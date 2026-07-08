/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// TheSuperHackers @refactor bobtista 08/07/2026 Central registry for GGC_* runtime
// environment flags. Every flag is declared once in GGC_RUNTIME_FLAG_LIST below;
// the enum and the info table in GgcRuntimeFlags.cpp both expand from that list,
// so they cannot drift apart. Values are resolved from the environment once and
// cached. Diagnostic- and probe-tier flags resolve to "unset" unless the build
// defines GGC_DIAGNOSTIC_TOOLS (CMake option, default ON).
#pragma once

#include <limits.h>

enum GgcFlagTier
{
	GgcTier_Setting,      // player-meaningful configuration
	GgcTier_KillSwitch,   // disables an optimization/feature for bisection and support
	GgcTier_Diagnostic,   // logging/visualization; compiled out of resolution without GGC_DIAGNOSTIC_TOOLS
	GgcTier_Probe,        // pipeline-stage isolation probes; gated like Diagnostic
	GgcTier_Harness,      // automation: screenshots, auto-exit, triggers, frame timing
	GgcTier_Workaround    // platform escape hatches
};

enum GgcFlagType
{
	GgcFlagType_Presence, // Enabled() = variable set to any value, including "0"
	GgcFlagType_Truthy,   // Enabled() = set to "1" or "true"
	GgcFlagType_ZeroOff,  // Enabled() = unset, or set to a nonzero value ("0" disables)
	GgcFlagType_Int,      // IntValue() = atoi(value) when set, else the declared default
	GgcFlagType_Float,    // FloatValue() = atof(value) when set, else the declared default
	GgcFlagType_String    // StringValue() = raw value, NULL when unset
};

// X(id, name, alias, tier, type, defaultInt, defaultFloat, help)
// Call sites with nontrivial parsing (tri-state, clamping, dual semantics) keep
// their logic and read the raw value via StringValue(); the declared type/default
// then documents the common reading.
#define GGC_RUNTIME_FLAG_LIST(X) \
	/* --- Settings --- */ \
	X(GgcFlag_BgfxRenderer, "GGC_BGFX_RENDERER", NULL, GgcTier_Setting, GgcFlagType_String, 0, 0.0f, "bgfx renderer override: dx11, dx12, vulkan, metal, gl") \
	X(GgcFlag_BgfxMsaa, "GGC_BGFX_MSAA", NULL, GgcTier_Setting, GgcFlagType_Int, 0, 0.0f, "MSAA sample count for the scene framebuffer and backbuffer (0 = off)") \
	X(GgcFlag_BgfxRenderScale, "GGC_BGFX_RENDER_SCALE", NULL, GgcTier_Setting, GgcFlagType_Float, 0, 1.0f, "internal supersampling scale for the 3D scene, clamped to [1.0, 2.0]") \
	X(GgcFlag_BgfxHdr, "GGC_BGFX_HDR", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "RGBA16F scene color target with ACES tonemap") \
	X(GgcFlag_BgfxSsao, "GGC_BGFX_SSAO", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "screen-space ambient occlusion") \
	X(GgcFlag_BgfxSrgb, "GGC_BGFX_SRGB", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "sRGB backbuffer") \
	X(GgcFlag_BgfxShadowMap, "GGC_BGFX_SHADOWMAP", NULL, GgcTier_Setting, GgcFlagType_String, 0, 0.0f, "sun shadow map: unset = INI, 0 = force off, anything else = force on") \
	X(GgcFlag_BgfxShadowMode, "GGC_BGFX_SHADOW_MODE", NULL, GgcTier_Setting, GgcFlagType_String, 0, 0.0f, "shadow mode: stencil (default), or none/off to disable stencil volumes") \
	X(GgcFlag_BgfxShadowFullPcf, "GGC_BGFX_SHADOW_FULL_PCF", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "full 36-fetch PCF for sun shadows instead of the reduced 9-fetch kernel") \
	X(GgcFlag_BgfxPointFilter, "GGC_BGFX_POINT_FILTER", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "force point (nearest) texture filtering on all samplers") \
	X(GgcFlag_BgfxColorGrade, "GGC_BGFX_COLORGRADE", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "force-enable the color-grade post pass") \
	X(GgcFlag_BgfxBloom, "GGC_BGFX_BLOOM", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "force-enable the bloom post pass") \
	X(GgcFlag_BgfxVignette, "GGC_BGFX_VIGNETTE", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "force-enable the vignette post effect") \
	X(GgcFlag_BgfxChroma, "GGC_BGFX_CHROMA", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "force-enable the chromatic-aberration post effect") \
	X(GgcFlag_BgfxGrain, "GGC_BGFX_GRAIN", NULL, GgcTier_Setting, GgcFlagType_Presence, 0, 0.0f, "force-enable the film-grain post effect") \
	X(GgcFlag_AudioCacheMb, "GGC_AUDIO_CACHE_MB", NULL, GgcTier_Setting, GgcFlagType_Int, 0, 0.0f, "decoded-PCM audio cache size in MB, overrides the INI-derived floor") \
	/* --- Kill-switches --- */ \
	X(GgcFlag_BgfxNoInstancing, "GGC_BGFX_NO_INSTANCING", "GGC_BGFX_DISABLE_INSTANCING", GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable GPU-instanced batching of identical rigid meshes") \
	X(GgcFlag_BgfxInstancingNoReorder, "GGC_BGFX_INSTANCING_NO_REORDER", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "skip the render-task reorder that maximizes instanced run lengths") \
	X(GgcFlag_BgfxNoRenderThread, "GGC_BGFX_NO_RENDER_THREAD", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable the bgfx render-thread split (single-threaded submit)") \
	X(GgcFlag_BgfxCoalesceDynamicRangeUploads, "GGC_BGFX_COALESCE_DYNAMIC_RANGE_UPLOADS", NULL, GgcTier_KillSwitch, GgcFlagType_ZeroOff, 0, 0.0f, "coalesce dynamic VB/IB range uploads into one update (0 disables)") \
	X(GgcFlag_BgfxDisableSortedMaterialRecaptureSkip, "GGC_BGFX_DISABLE_SORTED_MATERIAL_RECAPTURE_SKIP", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable the sorted-material recapture-skip optimization") \
	X(GgcFlag_BgfxDisableSortedMaterialSnapshot, "GGC_BGFX_DISABLE_SORTED_MATERIAL_SNAPSHOT", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable the sorted-material snapshot fast path in sorted replay") \
	X(GgcFlag_BgfxDisableSortedTransformRestoreSkip, "GGC_BGFX_DISABLE_SORTED_TRANSFORM_RESTORE_SKIP", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable the same-transform skip in sorted batch replay") \
	X(GgcFlag_BgfxDisableSortedMeshRouting, "GGC_BGFX_DISABLE_SORTED_MESH_ROUTING", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "restore texture-name-only routing of model-space sorted draws") \
	X(GgcFlag_BgfxDisableSortedBatchStatePacketCache, "GGC_BGFX_DISABLE_SORTED_BATCH_STATE_PACKET_CACHE", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "rebuild sorted-batch backend state per apply instead of using the cached packet") \
	X(GgcFlag_BgfxDisableUnlitLightInputSkip, "GGC_BGFX_DISABLE_UNLIT_LIGHT_INPUT_SKIP", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable skipping light-uniform uploads for unlit draws") \
	X(GgcFlag_BgfxDisableInactiveShadowUniformSkip, "GGC_BGFX_DISABLE_INACTIVE_SHADOW_UNIFORM_SKIP", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable skipping shadow-uniform uploads while the shadow map is inactive") \
	X(GgcFlag_NoSortCoalesce, "GGC_NO_SORT_COALESCE", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "revert additive-run coalescing in the sorting flush to the per-run path") \
	X(GgcFlag_NoTrackBatch, "GGC_NO_TRACK_BATCH", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "force per-module terrain-track draws instead of per-texture batching") \
	X(GgcFlag_NoParticleBatch, "GGC_NO_PARTICLE_BATCH", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "force per-emitter particle submission instead of batched draws") \
	X(GgcFlag_NoVolumeMerge, "GGC_NO_VOLUME_MERGE", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "force legacy per-depth-layer volume-particle draws instead of one merged draw") \
	X(GgcFlag_NoCoplanarBiasGate, "GGC_NO_COPLANAR_BIAS_GATE", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "restore the unconditional coplanar-pair scan on every dynamic vertex write") \
	X(GgcFlag_NoCloudShadows, "GGC_NO_CLOUD_SHADOWS", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "force the cloud-shadow scrolling texture off") \
	X(GgcFlag_NoPropShadows, "GGC_NO_PROP_SHADOWS", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "stop marking opaque world objects as sun-shadow receivers") \
	X(GgcFlag_NoRotorShadow, "GGC_NO_ROTOR_SHADOW", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable the rotor-blur alpha-tested disc shadow caster") \
	X(GgcFlag_BgfxDepthClamp, "GGC_BGFX_DEPTH_CLAMP", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "re-enable depth clamp (fallback for shadow-volume near/far-clip regressions)") \
	X(GgcFlag_EnableLegacyStencilShadows, "GGC_ENABLE_LEGACY_STENCIL_SHADOWS", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "force the legacy stencil-shadow path on even when the shadow mode disables it") \
	X(GgcFlag_BgfxLegacyPostMeshStencilShadows, "GGC_BGFX_LEGACY_POSTMESH_STENCIL_SHADOWS", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "submit stencil volumes in legacy post-mesh order instead of the engine view") \
	X(GgcFlag_BgfxCullSubpixel, "GGC_BGFX_CULL_SUBPIXEL", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "skip base-mesh submission for drawables projecting under the min pixel size") \
	X(GgcFlag_BgfxCullMinPx, "GGC_BGFX_CULL_MIN_PX", NULL, GgcTier_KillSwitch, GgcFlagType_Float, 0, 2.0f, "projected-radius threshold in pixels for the subpixel cull") \
	X(GgcFlag_DisableParticleCannonTrackingLight, "GGC_DISABLE_PARTICLE_CANNON_TRACKING_LIGHT", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "disable the Particle Cannon beam-tracking dynamic light") \
	X(GgcFlag_DisableRawAnimStepSnap, "GGC_DISABLE_RAW_ANIM_STEP_SNAP", NULL, GgcTier_KillSwitch, GgcFlagType_Presence, 0, 0.0f, "opt out of the per-axis raw-anim translation step-snap fix") \
	/* --- Platform workarounds --- */ \
	X(GgcFlag_MacosFlush, "GGC_MACOS_FLUSH", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "re-enable serialized Metal submit (AGX shader-compile artifact workaround)") \
	X(GgcFlag_MacosNoFlush, "GGC_MACOS_NO_FLUSH", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "force serialized Metal submit off even when GGC_MACOS_FLUSH is set") \
	X(GgcFlag_MacosUseNsWindow, "GGC_MACOS_USE_NSWINDOW", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "pass the legacy NSWindow handle to bgfx instead of SDL3's CAMetalLayer") \
	X(GgcFlag_MacosPrewarm, "GGC_MACOS_PREWARM", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "re-enable the view pre-warm loop at init") \
	X(GgcFlag_StrictPoolShutdown, "GGC_STRICT_POOL_SHUTDOWN", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "opt back into strict ObjectPoolClass destructor teardown and asserts") \
	X(GgcFlag_DisableMouseWarp, "GGC_DISABLE_MOUSE_WARP", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "skip SDL_WarpMouseInWindow in mouse setPosition") \
	X(GgcFlag_DisableMouseGrab, "GGC_DISABLE_MOUSE_GRAB", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "skip SDL_SetWindowMouseGrab in mouse capture (breaks edge scroll)") \
	X(GgcFlag_SdlTextureCursor, "GGC_SDL_TEXTURE_CURSOR", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "enable fallback color-cursor creation from the in-game cursor texture") \
	X(GgcFlag_SdlOsCursor, "GGC_SDL_OS_CURSOR", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "force the OS cursor always visible, bypassing game cursor visibility") \
	X(GgcFlag_BgfxSkipStaticVolumeShadows, "GGC_BGFX_SKIP_STATIC_VOLUME_SHADOWS", NULL, GgcTier_Workaround, GgcFlagType_Presence, 0, 0.0f, "skip stencil volume shadows for static (non-vehicle) casters") \
	/* --- Automation harness --- */ \
	X(GgcFlag_BgfxScreenshotAfter, "GGC_BGFX_SCREENSHOT_AFTER", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "render frame at which to take the first bgfx screenshot") \
	X(GgcFlag_BgfxScreenshotInterval, "GGC_BGFX_SCREENSHOT_INTERVAL", NULL, GgcTier_Harness, GgcFlagType_Int, 500, 0.0f, "frames between repeated bgfx screenshots") \
	X(GgcFlag_BgfxScreenshotPath, "GGC_BGFX_SCREENSHOT_PATH", NULL, GgcTier_Harness, GgcFlagType_String, 0, 0.0f, "base output path for bgfx screenshots") \
	X(GgcFlag_BgfxScreenshotLogicFrame, "GGC_BGFX_SCREENSHOT_LOGICFRAME", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "logic frame that triggers one deterministic bgfx screenshot") \
	X(GgcFlag_Dx8ScreenshotAfter, "GGC_DX8_SCREENSHOT_AFTER", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "render frame at which to take the first DX8 backend screenshot") \
	X(GgcFlag_Dx8ScreenshotInterval, "GGC_DX8_SCREENSHOT_INTERVAL", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "frames between repeated DX8 screenshots") \
	X(GgcFlag_Dx8ScreenshotPath, "GGC_DX8_SCREENSHOT_PATH", NULL, GgcTier_Harness, GgcFlagType_String, 0, 0.0f, "base output path for DX8 screenshots") \
	X(GgcFlag_Dx8ScreenshotLogicFrame, "GGC_DX8_SCREENSHOT_LOGICFRAME", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "logic frame that triggers one deterministic DX8 screenshot") \
	X(GgcFlag_BgfxFrameTimingAfter, "GGC_BGFX_FRAME_TIMING_AFTER", NULL, GgcTier_Harness, GgcFlagType_Int, -1, 0.0f, "frame at which to start the per-section frame-timing CSV capture") \
	X(GgcFlag_BgfxFrameTimingInterval, "GGC_BGFX_FRAME_TIMING_INTERVAL", NULL, GgcTier_Harness, GgcFlagType_Int, 60, 0.0f, "frames between frame-timing CSV emits") \
	X(GgcFlag_BgfxFrameTimingPath, "GGC_BGFX_FRAME_TIMING_PATH", NULL, GgcTier_Harness, GgcFlagType_String, 0, 0.0f, "base path for the frame-timing CSV output") \
	X(GgcFlag_RenderDocCaptureAfter, "GGC_RENDERDOC_CAPTURE_AFTER", NULL, GgcTier_Harness, GgcFlagType_Int, -1, 0.0f, "frame at which to trigger a RenderDoc capture") \
	X(GgcFlag_RenderDocCaptureInterval, "GGC_RENDERDOC_CAPTURE_INTERVAL", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "frames between repeated RenderDoc captures") \
	X(GgcFlag_AutoExitSeconds, "GGC_AUTO_EXIT_SECONDS", NULL, GgcTier_Harness, GgcFlagType_Int, 0, 0.0f, "exit the game after N wall-clock seconds of engine update") \
	X(GgcFlag_FreezeLogicAfter, "GGC_FREEZE_LOGIC_AFTER", NULL, GgcTier_Harness, GgcFlagType_Int, -1, 0.0f, "freeze sim time once the logic frame reaches N; rendering continues") \
	X(GgcFlag_NoAudio, "GGC_NO_AUDIO", NULL, GgcTier_Harness, GgcFlagType_Truthy, 0, 0.0f, "force the dummy audio manager (headless/automation runs)") \
	X(GgcFlag_TriggerGuiCommand, "GGC_TRIGGER_GUI_COMMAND", NULL, GgcTier_Harness, GgcFlagType_String, 0, 0.0f, "auto-fire a named ControlBar GUI command after the trigger delay") \
	X(GgcFlag_TriggerSpecialPower, "GGC_TRIGGER_SPECIAL_POWER", NULL, GgcTier_Harness, GgcFlagType_String, 0, 0.0f, "auto-fire a named special power after the trigger delay") \
	X(GgcFlag_TriggerWorld, "GGC_TRIGGER_WORLD", NULL, GgcTier_Harness, GgcFlagType_String, 0, 0.0f, "world-coordinate target x,y[,z] for the auto-fired command or power") \
	X(GgcFlag_TriggerDelayFrames, "GGC_TRIGGER_DELAY_FRAMES", NULL, GgcTier_Harness, GgcFlagType_Int, 90, 0.0f, "delay in frames before the auto-fired command or power fires") \
	/* --- Diagnostics --- */ \
	X(GgcFlag_Trace, "GGC_TRACE", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "verbose [ggc] trace: init breadcrumbs, bgfx info lines, routing logs") \
	X(GgcFlag_BgfxDebug, "GGC_BGFX_DEBUG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "enable bgfx debug/verbose init diagnostics") \
	X(GgcFlag_BgfxPerfLog, "GGC_BGFX_PERF_LOG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "periodic perf-stats logging; numeric value overrides the emit interval") \
	X(GgcFlag_BgfxPerfDir, "GGC_BGFX_PERF_DIR", NULL, GgcTier_Diagnostic, GgcFlagType_String, 0, 0.0f, "directory for the perf-stats CSV log") \
	X(GgcFlag_DrawLogAfter, "GGC_DRAWLOG_AFTER", NULL, GgcTier_Diagnostic, GgcFlagType_Int, -1, 0.0f, "frame number at which to dump a per-draw-call log") \
	X(GgcFlag_DrawLogInterval, "GGC_DRAWLOG_INTERVAL", NULL, GgcTier_Diagnostic, GgcFlagType_Int, 0, 0.0f, "repeat interval in frames for further draw-call log dumps") \
	X(GgcFlag_DrawLogPath, "GGC_DRAWLOG_PATH", NULL, GgcTier_Diagnostic, GgcFlagType_String, 0, 0.0f, "base output path for draw-call logs") \
	X(GgcFlag_PointShadowViz, "GGC_POINT_SHADOW_VIZ", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "blit the point-shadow map to a screen-corner rect for inspection") \
	X(GgcFlag_BgfxTransientDiag, "GGC_BGFX_TRANSIENT_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log transient-buffer ownership decisions") \
	X(GgcFlag_BgfxBufferUpdateDiag, "GGC_BGFX_BUFFER_UPDATE_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log dynamic buffer-update events") \
	X(GgcFlag_StencilShadowDiag, "GGC_STENCIL_SHADOW_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log stencil-shadow events") \
	X(GgcFlag_ShadowPathDiag, "GGC_SHADOW_PATH_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log shadow-type resolution path per drawable") \
	X(GgcFlag_BgfxShroudPassDiag, "GGC_BGFX_SHROUD_PASS_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log shroud-pass draw decisions") \
	X(GgcFlag_BgfxSortedDecalDiag, "GGC_BGFX_SORTED_DECAL_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log sorted-decal draw decisions") \
	X(GgcFlag_BgfxRevealDiag, "GGC_BGFX_REVEAL_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log reveal-grid draw decisions") \
	X(GgcFlag_BgfxRevealDiagVerbose, "GGC_BGFX_REVEAL_DIAG_VERBOSE", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "verbose variant of the reveal-grid diagnostics") \
	X(GgcFlag_BgfxEffectSubmitDiag, "GGC_BGFX_EFFECT_SUBMIT_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log effect-overlay submit decisions") \
	X(GgcFlag_LogShadowCasterAudit, "GGC_LOG_SHADOW_CASTER_AUDIT", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "dump every world draw that could feed the sun shadow map") \
	X(GgcFlag_LogShadowCasterStart, "GGC_LOG_SHADOW_CASTER_START", NULL, GgcTier_Diagnostic, GgcFlagType_Int, 0, 0.0f, "first frame of the shadow-caster audit window") \
	X(GgcFlag_LogShadowCasterEnd, "GGC_LOG_SHADOW_CASTER_END", NULL, GgcTier_Diagnostic, GgcFlagType_Int, INT_MAX, 0.0f, "last frame of the shadow-caster audit window") \
	X(GgcFlag_EffectTextureDiag, "GGC_EFFECT_TEXTURE_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log uploads and alpha stats of effect textures") \
	X(GgcFlag_BgfxShroudDumpDir, "GGC_BGFX_SHROUD_DUMP_DIR", NULL, GgcTier_Diagnostic, GgcFlagType_String, 0, 0.0f, "directory to dump shroud texture uploads as PPM files") \
	X(GgcFlag_BgfxShroudDumpLimit, "GGC_BGFX_SHROUD_DUMP_LIMIT", NULL, GgcTier_Diagnostic, GgcFlagType_Int, 8, 0.0f, "max number of shroud PPM dumps per run") \
	X(GgcFlag_PlayerContextDiag, "GGC_PLAYER_CONTEXT_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log player-context lines per logged event") \
	X(GgcFlag_AudioDiag, "GGC_AUDIO_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log every audio cache miss with cache occupancy") \
	X(GgcFlag_LaserDiag, "GGC_LASER_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log laser draw events for Patriot binary-data-stream templates") \
	X(GgcFlag_LaserDiagAll, "GGC_LASER_DIAG_ALL", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "widen the laser diagnostics to all drawables") \
	X(GgcFlag_MapPreviewDiag, "GGC_MAP_PREVIEW_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log map-preview image draw details") \
	X(GgcFlag_LightEnvDiag, "GGC_LIGHT_ENV_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "print the first light-environment snapshots") \
	X(GgcFlag_PointGroupDiag, "GGC_POINTGROUP_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log per-particle-group render state") \
	X(GgcFlag_SeglineDiag, "GGC_SEGLINE_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log segline geometry and submit state") \
	X(GgcFlag_BgfxSortedPacketCollectorDiag, "GGC_BGFX_SORTED_PACKET_COLLECTOR_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "classify sorted-packet source/blend/fallback per flush") \
	X(GgcFlag_BgfxSortedPacketCollectorDiagLimit, "GGC_BGFX_SORTED_PACKET_COLLECTOR_DIAG_LIMIT", NULL, GgcTier_Diagnostic, GgcFlagType_Int, 512, 0.0f, "max flushes logged by the sorted-packet collector diagnostic") \
	X(GgcFlag_SortEffectDiag, "GGC_SORT_EFFECT_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log sorted-effect draw events for effect-named textures") \
	X(GgcFlag_SortEffectDiagAll, "GGC_SORT_EFFECT_DIAG_ALL", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "remove the texture-name filter of the sorted-effect diagnostics") \
	X(GgcFlag_CursorDiag, "GGC_CURSOR_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log cursor set/lookup and surface/texture build details") \
	X(GgcFlag_SdlSoftwareCursor, "GGC_SDL_SOFTWARE_CURSOR", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "draw a line-art software cursor via the display layer") \
	X(GgcFlag_DecalDiag, "GGC_DECAL_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log decal-shadow diagnostics") \
	X(GgcFlag_W3dAssetDiag, "GGC_W3D_ASSET_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log W3D asset-manager diagnostics") \
	X(GgcFlag_W3dFileDiag, "GGC_W3D_FILE_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log W3D file-resolution probes") \
	X(GgcFlag_ParticleDiag, "GGC_PARTICLE_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log per-emitter particle render details") \
	X(GgcFlag_SceneDiag, "GGC_SCENE_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log scene render-path diagnostic counters") \
	X(GgcFlag_ShroudDiag, "GGC_SHROUD_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log checksum and pixel stats of visible shroud data per capture") \
	X(GgcFlag_ShroudDiagLimit, "GGC_SHROUD_DIAG_LIMIT", NULL, GgcTier_Diagnostic, GgcFlagType_Int, 32, 0.0f, "max shroud diagnostic samples logged") \
	X(GgcFlag_Ww3dLoadDiag, "GGC_WW3D_LOAD_DIAG", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "log WW3D asset loads") \
	X(GgcFlag_DumpLocalObjects, "GGC_DUMP_LOCAL_OBJECTS", NULL, GgcTier_Diagnostic, GgcFlagType_Presence, 0, 0.0f, "dump every local-player object once") \
	/* --- Isolation probes --- */ \
	X(GgcFlag_ProbeNullSubmit, "GGC_PROBE_NULL_SUBMIT", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "discard all engine/sorted draws instead of submitting") \
	X(GgcFlag_ProbeFreezeState, "GGC_PROBE_FREEZE_STATE", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "freeze light/texture/material uniform and bind state") \
	X(GgcFlag_ProbeNoSorted, "GGC_PROBE_NO_SORTED", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "drop all sorted-view draws") \
	X(GgcFlag_ProbeNoTexBind, "GGC_PROBE_NO_TEXBIND", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip texture binds") \
	X(GgcFlag_ProbeNoMatUniform, "GGC_PROBE_NO_MATUNIFORM", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip material uniform uploads") \
	X(GgcFlag_ProbeNoLightUniform, "GGC_PROBE_NO_LIGHTUNIFORM", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip light uniform uploads") \
	X(GgcFlag_ProbeNoSortFlush, "GGC_PROBE_NO_SORT_FLUSH", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "discard all sorted translucency instead of drawing it") \
	X(GgcFlag_ProbeNoParticleRender, "GGC_PROBE_NO_PARTICLE_RENDER", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip particle-buffer rendering entirely") \
	X(GgcFlag_ProbeNoSceneObjectRender, "GGC_PROBE_NO_SCENE_OBJECT_RENDER", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip the scene render-object loop") \
	X(GgcFlag_ProbeIdentityInstances, "GGC_PROBE_IDENTITY_INSTANCES", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "write identity matrices into the instance batch") \
	X(GgcFlag_ProbeTransposeInstances, "GGC_PROBE_TRANSPOSE_INSTANCES", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "write transposed instance matrices (layout debugging)") \
	X(GgcFlag_BgfxEnableDiagnosticOverrides, "GGC_BGFX_ENABLE_DIAGNOSTIC_OVERRIDES", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "master arming switch for the GGC_BGFX_SKIP_* draw-skip probes") \
	X(GgcFlag_BgfxSkipRevealGrid, "GGC_BGFX_SKIP_REVEAL_GRID", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "discard reveal-grid-textured draws (needs diagnostic overrides)") \
	X(GgcFlag_BgfxSkipEffectOverlayDraws, "GGC_BGFX_SKIP_EFFECT_OVERLAY_DRAWS", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "discard draws routed to the effect-overlay view (needs diagnostic overrides)") \
	X(GgcFlag_BgfxSkipSortedDraws, "GGC_BGFX_SKIP_SORTED_DRAWS", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "discard draws routed to the sorted view (needs diagnostic overrides)") \
	X(GgcFlag_BgfxSkipShroudOverlay, "GGC_BGFX_SKIP_SHROUD_OVERLAY", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "discard shroud-overlay draws (needs diagnostic overrides)") \
	X(GgcFlag_BgfxSkipBlobShadows, "GGC_BGFX_SKIP_BLOB_SHADOWS", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip default blob/decal shadows (needs diagnostic overrides)") \
	X(GgcFlag_NoEffectOverlay, "GGC_NO_EFFECT_OVERLAY", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "prevent the effect-overlay view from activating") \
	X(GgcFlag_BgfxStencilNoApply, "GGC_BGFX_STENCIL_NO_APPLY", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "skip the fullscreen stencil-darken apply pass") \
	X(GgcFlag_BgfxStencilDepth, "GGC_BGFX_STENCIL_DEPTH", NULL, GgcTier_Probe, GgcFlagType_String, 0, 0.0f, "override the shadow-volume depth test: less or always") \
	X(GgcFlag_BgfxStencilTwoSided, "GGC_BGFX_STENCIL_TWO_SIDED", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "opt-in two-sided stencil volumes (known broken for elevated casters)") \
	X(GgcFlag_BgfxStencilInvertCull, "GGC_BGFX_STENCIL_INVERT_CULL", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "invert cull face for shadow-volume passes") \
	X(GgcFlag_BgfxStencilClampClip, "GGC_BGFX_STENCIL_CLAMP_CLIP", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "shader-side clamp-clip experiment for shadow volumes") \
	X(GgcFlag_BgfxStencilAlgo, "GGC_BGFX_STENCIL_ALGO", NULL, GgcTier_Probe, GgcFlagType_String, 0, 0.0f, "stencil volume algorithm selection: zfail, zfail-swap, zpass, zpass-swap") \
	X(GgcFlag_BgfxStencilIncrSat, "GGC_BGFX_STENCIL_INCR_SAT", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "use saturating stencil increment/decrement ops for shadow volumes") \
	X(GgcFlag_BgfxFlipCapWinding, "GGC_BGFX_FLIP_CAP_WINDING", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "flip triangle winding of shadow-volume caps") \
	X(GgcFlag_BgfxClosedShadowVolumes, "GGC_BGFX_CLOSED_SHADOW_VOLUMES", NULL, GgcTier_Probe, GgcFlagType_Presence, 0, 0.0f, "request closed (capped) shadow-volume geometry from the engine")

enum GgcFlagId
{
#define GGC_RUNTIME_FLAG_ENUM_ENTRY(id, name, alias, tier, type, defaultInt, defaultFloat, help) id,
	GGC_RUNTIME_FLAG_LIST(GGC_RUNTIME_FLAG_ENUM_ENTRY)
#undef GGC_RUNTIME_FLAG_ENUM_ENTRY
	GgcFlagCount
};

namespace GgcFlags
{

// Presence/Truthy/ZeroOff evaluation per the flag's declared type.
bool Enabled(GgcFlagId id);

// atoi of the value when the flag is set (and its tier is compiled in),
// else the declared default.
int IntValue(GgcFlagId id);

// atof of the value when set, else the declared default.
float FloatValue(GgcFlagId id);

// Raw value, NULL when unset or when the flag's tier is not compiled in.
// The returned pointer stays valid for the lifetime of the process.
const char *StringValue(GgcFlagId id);

// Prints the whole flag table with resolved values to stderr when
// GGC_LIST_FLAGS is set in the environment.
void DumpTableIfRequested();

}
