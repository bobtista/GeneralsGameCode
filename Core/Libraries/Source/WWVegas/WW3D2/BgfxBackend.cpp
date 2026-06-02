/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// TheSuperHackers @refactor bobtista 10/04/2026 BgfxBackend.
// IRenderBackend implementation that drives bgfx as the primary
// rendering backend, translating the engine's DX8-era draw calls
// into bgfx submits with a fixed-function-emulating uber shader.

#include "BgfxBackend.h"

#include "RenderStateDefs.h"
#include "DXTUtils.h"
#include "dx8fvf.h"
#include "FixedFunctionState.h"
#include "indexbuffer.h"
#include "light.h"
#include "lightenvironment.h"
#include "matrix3d.h"
#include "matrix4.h"
#include "RenderStateCache.h"
#include "shader.h"
#include "texture.h"
#include "texturefilter.h"
#include "vertexbuffer.h"
#include "vector3.h"
#include "ww3d.h"
#include "ww3dformat.h"
#include "wwdebug.h"
#include "wwmath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <unordered_map>

#include <rts/profile.h>
// TheSuperHackers @perf bobtista 24/06/2026 rts/profile.h pulls in Tracy.hpp (C++ API) but the bgfx profiler callbacks use the Tracy C API.
#if defined(RTS_PROFILE_TRACY)
#include <tracy/TracyC.h>
#endif

// Including the bgfx header here is intentional: it forces a compile-time
// dependency on the bgfx headers when GGC_RENDER_BACKEND=bgfx. If bgfx
// isn't available the build fails here, which is the right place to
// catch dependency problems.
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#if defined(SAGE_USE_SDL3)
#include <SDL3/SDL.h>
#endif

// TheSuperHackers @refactor bobtista 16/04/2026 bgfx takes the main
// game window. A secondary popup is created for legacy reference output.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// TheSuperHackers @refactor bobtista 11/04/2026 Compiled shader
// bytecode. These headers are generated at build time by ggc_compile_bgfx_shader
// (cmake/bgfx.cmake) and end up in the target's binary dir.
#if defined(GGC_BGFX_RENDERER_METAL)
#include "vs_passthrough_metal.bin.h"
#include "fs_passthrough_metal.bin.h"
#include "vs_uber_metal.bin.h"
#include "vs_uber_instanced_metal.bin.h"
#include "vs_trees_metal.bin.h"
#include "fs_uber_metal.bin.h"
#include "vs_shadow_volume_metal.bin.h"
#include "fs_shadow_volume_metal.bin.h"
#include "vs_shadow_apply_metal.bin.h"
#include "fs_shadow_apply_metal.bin.h"
#include "vs_scene_composite_metal.bin.h"
#include "fs_scene_composite_metal.bin.h"
#include "vs_scene_depth_metal.bin.h"
#include "fs_scene_depth_metal.bin.h"
#include "vs_smudge_metal.bin.h"
#include "fs_smudge_metal.bin.h"
#define GGC_BGFX_SHADER(name) name##_metal
#else
#include "vs_passthrough_dx11.bin.h"
#include "fs_passthrough_dx11.bin.h"

// TheSuperHackers @refactor bobtista 12/04/2026 Uber shader pair.
// Single program handles all TSS combinations via uniforms. Replaces the
// Per-preset shader pairs.
#include "vs_uber_dx11.bin.h"
#include "vs_uber_instanced_dx11.bin.h"
#include "vs_trees_dx11.bin.h"
#include "fs_uber_dx11.bin.h"

// TheSuperHackers @refactor bobtista 15/04/2026 Stencil shadow
// volume program. Vertex shader is a trivial XYZ->clip transform; fragment
// writes nothing visible because color writes are disabled for the pass.
#include "vs_shadow_volume_dx11.bin.h"
#include "fs_shadow_volume_dx11.bin.h"
#include "vs_shadow_apply_dx11.bin.h"
#include "fs_shadow_apply_dx11.bin.h"

#include "vs_scene_composite_dx11.bin.h"
#include "fs_scene_composite_dx11.bin.h"
#include "vs_scene_depth_dx11.bin.h"
#include "fs_scene_depth_dx11.bin.h"
#include "vs_smudge_dx11.bin.h"
#include "fs_smudge_dx11.bin.h"
#define GGC_BGFX_SHADER(name) name##_dx11
#endif

#include "BgfxBackendState.h"

#ifdef RTS_ZEROHOUR
extern "C" void GGC_GetBgfxPostProcessParams(float * params);
extern "C" void GGC_GetBgfxDiagnosticFlags(int * logStats, int * noSceneFramebuffer, int * noPostFx);
extern "C" void GGC_GetBgfxSoftParticleParams(float * params);
extern "C" int  GGC_GetBgfxScreenshotFrame();
extern "C" const char * GGC_GetBgfxScreenshotPath();
extern "C" void GGC_ClearBgfxScreenshotRequest();
#endif

// Render-state globals. Defined here (external linkage), declared `extern`
// in BgfxBackendState.h so BgfxBackendTextures.cpp can reference them.
BgfxDevice     g_device;
BgfxUniforms   g_uniforms;
BgfxDraw       g_draw;
BgfxOverrides  g_overrides;
BgfxViewFlags  g_views;
BgfxFrame      g_frame;
BgfxStats      g_stats;
BgfxCaches     g_caches;
// Asset-ingress resource side-table. id 0 is reserved invalid.
BgfxResourceRegistry g_resourceRegistry = { {}, 1 };

// Defined in BgfxBackendTextures.cpp.
bgfx::TextureFormat::Enum TranslateWW3DFormat(WW3DFormat fmt);

#if defined(__APPLE__) && defined(SAGE_USE_SDL3)
// TheSuperHackers @bugfix bobtista 30/04/2026 Owned by SDL3Main.cpp
// (filled in main() right after SDL_Metal_CreateView). Kept at global
// scope here so GetNativeWindowHandle below can reach it across the
// surrounding anonymous namespace.
extern void *TheSDL3MetalLayer;
#endif


namespace
{
// TSS operation IDs matching fs_uber.sc #defines. Used in BuildTssOpsForShader
// and UpdateTextureStageOps to encode fixed-function texture stage state as
// float uniforms consumed by the uber fragment shader.
static const float kTssDisable       =  0.0f;
static const float kTssSelectArg1    =  1.0f;
static const float kTssSelectArg2    =  2.0f;
static const float kTssModulate      =  3.0f;
static const float kTssModulate2x    =  4.0f;
static const float kTssAdd           =  5.0f;
static const float kTssAddSigned     =  6.0f;
static const float kTssSubtract      =  7.0f;
static const float kTssBlendTexAlpha =  8.0f;
static const float kTssBlendCurAlpha =  9.0f;
static const float kTssAddSmooth     = 10.0f;

// TSS argument source IDs (packed into arg1/arg2 uniform channels).
static const float kTssArgTexture =  0.0f;
static const float kTssArgDiffuse =  1.0f;
static const float kTssArgCurrent =  2.0f;
static const unsigned kTextureArgumentSelectMask = 0x0000000f;

constexpr unsigned kTextureAddressWrap = 1, kTextureAddressClamp = 3, kTextureAddressBorder = 4;
constexpr unsigned kTextureSampleNone = 0, kTextureSamplePoint = 1, kTextureSampleLinear = 2, kTextureSampleAnisotropic = 3;
constexpr unsigned kTexcoordGenPassthru = 0x00000000, kTexcoordGenCameraNormal = 0x00010000, kTexcoordGenCameraReflection = 0x00030000, kTexcoordGenCameraPosition = 0x00020000;
constexpr unsigned kTextureTransformDisable = 0, kTextureTransformProjected = 256, kTextureTransformCount2 = 2, kTextureTransformCount3 = 3;
constexpr unsigned kTextureTransformStage0 = 16, kTransformView = 2;
constexpr unsigned kTextureArgCurrent = static_cast<unsigned>(RB_TEXARG_CURRENT), kTextureArgTexture = static_cast<unsigned>(RB_TEXARG_TEXTURE), kTextureArgDiffuse = static_cast<unsigned>(RB_TEXARG_DIFFUSE);
constexpr unsigned kTextureOpDisable = static_cast<unsigned>(RB_TEXOP_DISABLE), kTextureOpSelectArg1 = static_cast<unsigned>(RB_TEXOP_SELECTARG1), kTextureOpSelectArg2 = static_cast<unsigned>(RB_TEXOP_SELECTARG2);

static float TextureOpToTssOp(unsigned value)
{
    switch (static_cast<RenderBackendTextureOperation>(value))
    {
        case RB_TEXOP_DISABLE:            return kTssDisable;
        case RB_TEXOP_SELECTARG1:         return kTssSelectArg1;
        case RB_TEXOP_SELECTARG2:         return kTssSelectArg2;
        case RB_TEXOP_MODULATE:           return kTssModulate;
        case RB_TEXOP_MODULATE2X:         return kTssModulate2x;
        case RB_TEXOP_ADD:                return kTssAdd;
        case RB_TEXOP_ADDSIGNED:          return kTssAddSigned;
        case RB_TEXOP_SUBTRACT:           return kTssSubtract;
        case RB_TEXOP_BLENDTEXTUREALPHA:  return kTssBlendTexAlpha;
        case RB_TEXOP_BLENDCURRENTALPHA:  return kTssBlendCurAlpha;
        case RB_TEXOP_ADDSMOOTH:          return kTssAddSmooth;
        default:                          return kTssSelectArg1;
    }
}

static float TextureArgToTssArg(unsigned value)
{
    switch (static_cast<RenderBackendTextureArgument>(value & kTextureArgumentSelectMask))
    {
        case RB_TEXARG_TEXTURE: return kTssArgTexture;
        case RB_TEXARG_CURRENT: return kTssArgCurrent;
        case RB_TEXARG_DIFFUSE:
        default:                return kTssArgDiffuse;
    }
}

static unsigned long AllocateLegacyShaderHandle()
{
    static unsigned long nextHandle = 1;
    return nextHandle++;
}

static std::unordered_map<unsigned long, RenderBackendLegacyPixelShaderMode> g_legacyPixelShaderModes;

static void ResetFrameStats()
{
    const uint32_t nextFrame = g_stats.frameIndex + 1;
    std::memset(&g_stats, 0, sizeof(g_stats));
    g_stats.frameIndex = nextFrame;
}

struct BgfxDiagnosticFlags
{
    bool logStats;
    bool noSceneFramebuffer;
    bool noPostFx;
};

static BgfxDiagnosticFlags GetBgfxDiagnosticFlags()
{
    BgfxDiagnosticFlags flags = { false, false, false };
#ifdef RTS_ZEROHOUR
    int logStats = 0;
    int noSceneFramebuffer = 0;
    int noPostFx = 0;
    GGC_GetBgfxDiagnosticFlags(&logStats, &noSceneFramebuffer, &noPostFx);
    flags.logStats = logStats != 0;
    flags.noSceneFramebuffer = noSceneFramebuffer != 0;
    flags.noPostFx = noPostFx != 0;
#endif
    return flags;
}

enum class BgfxShadowMode
{
    Stencil,
    None
};

static BgfxShadowMode GetBgfxShadowMode()
{
    if (const char * mode = std::getenv("GGC_BGFX_SHADOW_MODE"))
    {
        if (std::strcmp(mode, "stencil") == 0)
        {
            return BgfxShadowMode::Stencil;
        }
        if (std::strcmp(mode, "none") == 0 || std::strcmp(mode, "off") == 0)
        {
            return BgfxShadowMode::None;
        }
    }
    return BgfxShadowMode::Stencil;
}

static bool BgfxStencilShadowsEnabled()
{
    const BgfxShadowMode mode = GetBgfxShadowMode();
    return mode == BgfxShadowMode::Stencil;
}

static bool g_triangleDrawEnabled = true;

static bool IsBgfxStatsLoggingEnabled()
{
    if (std::getenv("GGC_BGFX_PERF_LOG") != nullptr) {
        return true;
    }
    return GetBgfxDiagnosticFlags().logStats;
}

static double BgfxTicksToMs(int64_t ticks, int64_t frequency)
{
    if (frequency <= 0)
    {
        return -1.0;
    }
    return (static_cast<double>(ticks) * 1000.0) / static_cast<double>(frequency);
}

struct BgfxStatsLogWindow
{
    bool initialized;
    LARGE_INTEGER frequency;
    LARGE_INTEGER lastCounter;
    double elapsedSeconds;
    double windowSeconds;
    uint32_t frames;
    double bgfxNumDraw;
    double bgfxNumBlit;
    double bgfxCpuFrameMs;
    double bgfxGpuFrameMs;
    double bgfxWaitRenderMs;
    double bgfxWaitSubmitMs;
    uint32_t bgfxGpuFrameCount;
    uint32_t backendDraws;
    uint32_t backendSkipped;
    uint32_t baseSubmits;
    uint32_t sceneDepthSubmits;
    uint32_t shadowVolumeSubmits;
    uint32_t shadowApplySubmits;
    uint32_t smudgeSubmits;
    uint32_t sceneCompositeSubmits;
    uint32_t debugSubmits;
    uint32_t worldDraws;
    uint32_t uiDraws;
    uint32_t waterDraws;
    uint32_t sortedDraws;
    uint32_t effectDraws;
    uint32_t rttDraws;
    uint32_t smudgeDraws;
    uint32_t textureBinds;
    uint32_t textureCreates;
    uint32_t textureUploads;
    uint32_t textureCopies;
    uint32_t materialUniformUploads;
    uint32_t lightUniformUploads;
    uint32_t textureTransformUpdates;
    uint32_t renderStateCopies;
    uint32_t transientVbAllocations;
    uint32_t transientIbAllocations;
    uint32_t transientVbDraws;
    uint32_t transientIbDraws;
    uint32_t dynamicVbAllocations;
    uint32_t dynamicIbAllocations;
    uint32_t instancedSavedDrawCalls;
    double bgfxTransientVbUsed;
    double bgfxTransientIbUsed;
    int64_t textureMemoryUsed;
    int64_t rtMemoryUsed;
    uint16_t numTextures;
    uint16_t numFrameBuffers;
};

static BgfxStatsLogWindow g_bgfxStatsLog = {};

struct BgfxPerfSession
{
    uint32_t windows;
    uint32_t totalFrames;
    double totalSeconds;
    double cpuMsMin;
    double cpuMsMax;
    double cpuMsSum;
    double fpsMin;
    double fpsMax;
    uint32_t drawsMin;
    uint32_t drawsMax;
    uint64_t drawsSum;
    uint64_t uploadsSum;
    int64_t peakTexMem;
    double transientVbSum;
    double transientIbSum;
};

static BgfxPerfSession g_perfSession = {};

static void PerfSessionAccumulate(double windowSeconds, uint32_t windowFrames,
                                   double cpuMsAvg, double fps,
                                   uint32_t drawsAvg, uint32_t uploads,
                                   int64_t texMem, double transVb, double transIb)
{
    if (g_perfSession.windows == 0)
    {
        g_perfSession.cpuMsMin = cpuMsAvg;
        g_perfSession.cpuMsMax = cpuMsAvg;
        g_perfSession.fpsMin = fps;
        g_perfSession.fpsMax = fps;
        g_perfSession.drawsMin = drawsAvg;
        g_perfSession.drawsMax = drawsAvg;
    }
    else
    {
        if (cpuMsAvg < g_perfSession.cpuMsMin) { g_perfSession.cpuMsMin = cpuMsAvg; }
        if (cpuMsAvg > g_perfSession.cpuMsMax) { g_perfSession.cpuMsMax = cpuMsAvg; }
        if (fps < g_perfSession.fpsMin) { g_perfSession.fpsMin = fps; }
        if (fps > g_perfSession.fpsMax) { g_perfSession.fpsMax = fps; }
        if (drawsAvg < g_perfSession.drawsMin) { g_perfSession.drawsMin = drawsAvg; }
        if (drawsAvg > g_perfSession.drawsMax) { g_perfSession.drawsMax = drawsAvg; }
    }
    g_perfSession.windows++;
    g_perfSession.totalFrames += windowFrames;
    g_perfSession.totalSeconds += windowSeconds;
    g_perfSession.cpuMsSum += cpuMsAvg * windowFrames;
    g_perfSession.drawsSum += static_cast<uint64_t>(drawsAvg) * windowFrames;
    g_perfSession.uploadsSum += uploads;
    if (texMem > g_perfSession.peakTexMem) { g_perfSession.peakTexMem = texMem; }
    g_perfSession.transientVbSum += transVb * windowFrames;
    g_perfSession.transientIbSum += transIb * windowFrames;
}

static void PerfSessionPrintSummary()
{
    if (g_perfSession.totalFrames == 0) { return; }
    const double frames = static_cast<double>(g_perfSession.totalFrames);
    const double avgFps = frames / g_perfSession.totalSeconds;
    const double avgCpu = g_perfSession.cpuMsSum / frames;
    const double avgDraws = static_cast<double>(g_perfSession.drawsSum) / frames;
    std::fprintf(stderr,
        "\nBGFX_PERF_SUMMARY: %.1fs %u frames\n"
        "  fps:     avg=%.1f  min=%.1f  max=%.1f\n"
        "  cpu:     avg=%.2fms  min=%.2fms  max=%.2fms\n"
        "  draws:   avg=%.0f  min=%u  max=%u\n"
        "  uploads: %llu total (%.2f/frame)\n"
        "  texMem:  peak=%lldKB\n"
        "  transVB: avg=%.0f bytes/frame\n"
        "  transIB: avg=%.0f bytes/frame\n",
        g_perfSession.totalSeconds, g_perfSession.totalFrames,
        avgFps, g_perfSession.fpsMin, g_perfSession.fpsMax,
        avgCpu, g_perfSession.cpuMsMin, g_perfSession.cpuMsMax,
        avgDraws, g_perfSession.drawsMin, g_perfSession.drawsMax,
        static_cast<unsigned long long>(g_perfSession.uploadsSum),
        static_cast<double>(g_perfSession.uploadsSum) / frames,
        static_cast<long long>(g_perfSession.peakTexMem / 1024),
        g_perfSession.transientVbSum / frames,
        g_perfSession.transientIbSum / frames);
}

static double AverageOrMinusOne(double total, uint32_t count)
{
    if (count == 0)
    {
        return -1.0;
    }
    return total / static_cast<double>(count);
}

static uint32_t MakeBgfxClearColor(const Vector3 & color, float alpha)
{
    const uint32_t r = static_cast<uint32_t>(WWMath::Clamp(color.X, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint32_t g = static_cast<uint32_t>(WWMath::Clamp(color.Y, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint32_t b = static_cast<uint32_t>(WWMath::Clamp(color.Z, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint32_t a = static_cast<uint32_t>(WWMath::Clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static uint32_t MakeLegacyARGBColor(const Vector3 & color, float alpha)
{
    const uint32_t r = static_cast<uint32_t>(WWMath::Clamp(color.X, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint32_t g = static_cast<uint32_t>(WWMath::Clamp(color.Y, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint32_t b = static_cast<uint32_t>(WWMath::Clamp(color.Z, 0.0f, 1.0f) * 255.0f + 0.5f);
    const uint32_t a = static_cast<uint32_t>(WWMath::Clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static uint32_t FloatAsDword(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void ResetBgfxStatsLogWindow()
{
    g_bgfxStatsLog.windowSeconds = 0.0;
    g_bgfxStatsLog.frames = 0;
    g_bgfxStatsLog.bgfxNumDraw = 0.0;
    g_bgfxStatsLog.bgfxNumBlit = 0.0;
    g_bgfxStatsLog.bgfxCpuFrameMs = 0.0;
    g_bgfxStatsLog.bgfxGpuFrameMs = 0.0;
    g_bgfxStatsLog.bgfxWaitRenderMs = 0.0;
    g_bgfxStatsLog.bgfxWaitSubmitMs = 0.0;
    g_bgfxStatsLog.bgfxGpuFrameCount = 0;
    g_bgfxStatsLog.backendDraws = 0;
    g_bgfxStatsLog.backendSkipped = 0;
    g_bgfxStatsLog.baseSubmits = 0;
    g_bgfxStatsLog.sceneDepthSubmits = 0;
    g_bgfxStatsLog.shadowVolumeSubmits = 0;
    g_bgfxStatsLog.shadowApplySubmits = 0;
    g_bgfxStatsLog.smudgeSubmits = 0;
    g_bgfxStatsLog.sceneCompositeSubmits = 0;
    g_bgfxStatsLog.debugSubmits = 0;
    g_bgfxStatsLog.worldDraws = 0;
    g_bgfxStatsLog.uiDraws = 0;
    g_bgfxStatsLog.waterDraws = 0;
    g_bgfxStatsLog.sortedDraws = 0;
    g_bgfxStatsLog.effectDraws = 0;
    g_bgfxStatsLog.rttDraws = 0;
    g_bgfxStatsLog.smudgeDraws = 0;
    g_bgfxStatsLog.textureBinds = 0;
    g_bgfxStatsLog.textureCreates = 0;
    g_bgfxStatsLog.textureUploads = 0;
    g_bgfxStatsLog.textureCopies = 0;
    g_bgfxStatsLog.materialUniformUploads = 0;
    g_bgfxStatsLog.lightUniformUploads = 0;
    g_bgfxStatsLog.textureTransformUpdates = 0;
    g_bgfxStatsLog.renderStateCopies = 0;
    g_bgfxStatsLog.transientVbAllocations = 0;
    g_bgfxStatsLog.transientIbAllocations = 0;
    g_bgfxStatsLog.transientVbDraws = 0;
    g_bgfxStatsLog.transientIbDraws = 0;
    g_bgfxStatsLog.dynamicVbAllocations = 0;
    g_bgfxStatsLog.dynamicIbAllocations = 0;
    g_bgfxStatsLog.bgfxTransientVbUsed = 0.0;
    g_bgfxStatsLog.bgfxTransientIbUsed = 0.0;
    g_bgfxStatsLog.textureMemoryUsed = 0;
    g_bgfxStatsLog.rtMemoryUsed = 0;
    g_bgfxStatsLog.numTextures = 0;
    g_bgfxStatsLog.numFrameBuffers = 0;
}

// TheSuperHackers @bugfix bobtista 28/05/2026 Allow the perf-log directory to be overridden by GGC_BGFX_PERF_DIR and fall back to the current working directory; the previous hard-coded "C:\\tmp\\bgfx_perf" only ever worked on Windows.
static std::string GetBgfxPerfLogPath()
{
    std::filesystem::path dir;
    if (const char *env = std::getenv("GGC_BGFX_PERF_DIR"))
    {
        dir = env;
    }
    else
    {
        dir = std::filesystem::current_path();
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / "PerfLog_BgfxStats.csv").string();
}

static void InitializeBgfxStatsLog()
{
    g_bgfxStatsLog.initialized = true;
    g_bgfxStatsLog.elapsedSeconds = 0.0;
    QueryPerformanceFrequency(&g_bgfxStatsLog.frequency);
    QueryPerformanceCounter(&g_bgfxStatsLog.lastCounter);
    ResetBgfxStatsLogWindow();

    const std::string logPath = GetBgfxPerfLogPath();
    FILE * file = fopen(logPath.c_str(), "wt");
    if (file != nullptr)
    {
        fprintf(file, "elapsed_seconds,window_seconds,window_frames,bgfx_num_draw_avg,bgfx_num_blit_avg,bgfx_cpu_frame_ms_avg,bgfx_gpu_ms_avg,bgfx_wait_render_ms_avg,bgfx_wait_submit_ms_avg,backend_draws_avg,backend_skipped_avg,base_submits_avg,scene_depth_submits_avg,shadow_volume_submits_avg,shadow_apply_submits_avg,smudge_submits_avg,scene_composite_submits_avg,debug_submits_avg,world_draws_avg,ui_draws_avg,water_draws_avg,sorted_draws_avg,effect_draws_avg,rtt_draws_avg,smudge_draws_avg,texture_binds_avg,texture_creates_avg,texture_uploads_avg,texture_copies_avg,material_uniforms_avg,light_uniforms_avg,texture_transform_updates_avg,render_state_copies_avg,transient_vb_alloc_avg,transient_ib_alloc_avg,transient_vb_draw_avg,transient_ib_draw_avg,dynamic_vb_alloc_avg,dynamic_ib_alloc_avg,bgfx_transient_vb_used_avg,bgfx_transient_ib_used_avg,bgfx_texture_memory,bgfx_rt_memory,bgfx_num_textures,bgfx_num_framebuffers\n");
        fclose(file);
    }
    else
    {
        WWDEBUG_SAY(("[BgfxBackend] Failed to open PerfLog_BgfxStats.csv"));
    }
}

static void FlushBgfxStatsLogWindow()
{
    if (g_bgfxStatsLog.frames == 0)
    {
        return;
    }

    const std::string logPath = GetBgfxPerfLogPath();
    FILE * file = fopen(logPath.c_str(), "at");
    if (file != nullptr)
    {
        const double frames = static_cast<double>(g_bgfxStatsLog.frames);
        // TheSuperHackers @bugfix bobtista 28/05/2026 Format string and arg list got out of sync: drop one stray %.3f and use portable %lld/%llu instead of MSVC-only %I64d.
        fprintf(file, "%.3f,%.3f,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%lld,%lld,%u,%u\n",
            g_bgfxStatsLog.elapsedSeconds,
            g_bgfxStatsLog.windowSeconds,
            g_bgfxStatsLog.frames,
            g_bgfxStatsLog.bgfxNumDraw / frames,
            g_bgfxStatsLog.bgfxNumBlit / frames,
            g_bgfxStatsLog.bgfxCpuFrameMs / frames,
            AverageOrMinusOne(g_bgfxStatsLog.bgfxGpuFrameMs, g_bgfxStatsLog.bgfxGpuFrameCount),
            g_bgfxStatsLog.bgfxWaitRenderMs / frames,
            g_bgfxStatsLog.bgfxWaitSubmitMs / frames,
            static_cast<double>(g_bgfxStatsLog.backendDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.backendSkipped) / frames,
            static_cast<double>(g_bgfxStatsLog.baseSubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.sceneDepthSubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.shadowVolumeSubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.shadowApplySubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.smudgeSubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.sceneCompositeSubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.debugSubmits) / frames,
            static_cast<double>(g_bgfxStatsLog.worldDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.uiDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.waterDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.sortedDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.effectDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.rttDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.smudgeDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.textureBinds) / frames,
            static_cast<double>(g_bgfxStatsLog.textureCreates) / frames,
            static_cast<double>(g_bgfxStatsLog.textureUploads) / frames,
            static_cast<double>(g_bgfxStatsLog.textureCopies) / frames,
            static_cast<double>(g_bgfxStatsLog.materialUniformUploads) / frames,
            static_cast<double>(g_bgfxStatsLog.lightUniformUploads) / frames,
            static_cast<double>(g_bgfxStatsLog.textureTransformUpdates) / frames,
            static_cast<double>(g_bgfxStatsLog.renderStateCopies) / frames,
            static_cast<double>(g_bgfxStatsLog.transientVbAllocations) / frames,
            static_cast<double>(g_bgfxStatsLog.transientIbAllocations) / frames,
            static_cast<double>(g_bgfxStatsLog.transientVbDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.transientIbDraws) / frames,
            static_cast<double>(g_bgfxStatsLog.dynamicVbAllocations) / frames,
            static_cast<double>(g_bgfxStatsLog.dynamicIbAllocations) / frames,
            g_bgfxStatsLog.bgfxTransientVbUsed / frames,
            g_bgfxStatsLog.bgfxTransientIbUsed / frames,
            static_cast<long long>(g_bgfxStatsLog.textureMemoryUsed),
            static_cast<long long>(g_bgfxStatsLog.rtMemoryUsed),
            g_bgfxStatsLog.numTextures,
            g_bgfxStatsLog.numFrameBuffers);
        fclose(file);
    }

    if (std::getenv("GGC_BGFX_PERF_LOG") != nullptr)
    {
        const double frames = static_cast<double>(g_bgfxStatsLog.frames);
        const double fps = frames / g_bgfxStatsLog.windowSeconds;
        const double cpuMs = g_bgfxStatsLog.bgfxCpuFrameMs / frames;
        const uint32_t draws = static_cast<uint32_t>(g_bgfxStatsLog.backendDraws / g_bgfxStatsLog.frames);
        const double uploads = static_cast<double>(g_bgfxStatsLog.textureUploads) / frames;
        const double transVb = g_bgfxStatsLog.bgfxTransientVbUsed / frames;
        const double transIb = g_bgfxStatsLog.bgfxTransientIbUsed / frames;
        std::fprintf(stderr,
            "BGFX_PERF: %.1fs fps=%.1f cpu=%.2fms draws=%u uploads=%.0f texMem=%lldKB transVB=%.0f transIB=%.0f instSaved=%u\n",
            g_bgfxStatsLog.elapsedSeconds, fps, cpuMs, draws, uploads,
            static_cast<long long>(g_bgfxStatsLog.textureMemoryUsed / 1024),
            transVb, transIb, g_bgfxStatsLog.instancedSavedDrawCalls);
        PerfSessionAccumulate(g_bgfxStatsLog.windowSeconds, g_bgfxStatsLog.frames,
                              cpuMs, fps, draws, g_bgfxStatsLog.textureUploads,
                              g_bgfxStatsLog.textureMemoryUsed, transVb, transIb);
    }

    ResetBgfxStatsLogWindow();
}

static void UpdateBgfxStatsLog()
{
    if (!IsBgfxStatsLoggingEnabled())
    {
        return;
    }
    if (!g_bgfxStatsLog.initialized)
    {
        InitializeBgfxStatsLog();
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double deltaSeconds = 0.0;
    if (g_bgfxStatsLog.frequency.QuadPart > 0)
    {
        deltaSeconds =
            static_cast<double>(now.QuadPart - g_bgfxStatsLog.lastCounter.QuadPart) /
            static_cast<double>(g_bgfxStatsLog.frequency.QuadPart);
    }
    g_bgfxStatsLog.lastCounter = now;
    g_bgfxStatsLog.elapsedSeconds += deltaSeconds;
    g_bgfxStatsLog.windowSeconds += deltaSeconds;
    ++g_bgfxStatsLog.frames;

    const bgfx::Stats * stats = bgfx::getStats();
    if (stats != nullptr)
    {
        g_bgfxStatsLog.bgfxNumDraw += stats->numDraw;
        g_bgfxStatsLog.bgfxNumBlit += stats->numBlit;
        g_bgfxStatsLog.bgfxCpuFrameMs += BgfxTicksToMs(stats->cpuTimeFrame, stats->cpuTimerFreq);
        g_bgfxStatsLog.bgfxWaitRenderMs += BgfxTicksToMs(stats->waitRender, stats->cpuTimerFreq);
        g_bgfxStatsLog.bgfxWaitSubmitMs += BgfxTicksToMs(stats->waitSubmit, stats->cpuTimerFreq);
        if (stats->gpuTimerFreq > 0 && stats->gpuTimeEnd >= stats->gpuTimeBegin)
        {
            g_bgfxStatsLog.bgfxGpuFrameMs += BgfxTicksToMs(stats->gpuTimeEnd - stats->gpuTimeBegin, stats->gpuTimerFreq);
            ++g_bgfxStatsLog.bgfxGpuFrameCount;
        }
        g_bgfxStatsLog.bgfxTransientVbUsed += stats->transientVbUsed;
        g_bgfxStatsLog.bgfxTransientIbUsed += stats->transientIbUsed;
        g_bgfxStatsLog.textureMemoryUsed = stats->textureMemoryUsed;
        g_bgfxStatsLog.rtMemoryUsed = stats->rtMemoryUsed;
        g_bgfxStatsLog.numTextures = stats->numTextures;
        g_bgfxStatsLog.numFrameBuffers = stats->numFrameBuffers;
    }

    g_bgfxStatsLog.backendDraws += g_stats.drawCalls;
    g_bgfxStatsLog.backendSkipped += g_stats.skippedDraws;
    g_bgfxStatsLog.baseSubmits += g_stats.baseSubmits;
    g_bgfxStatsLog.sceneDepthSubmits += g_stats.sceneDepthSubmits;
    g_bgfxStatsLog.shadowVolumeSubmits += g_stats.shadowVolumeSubmits;
    g_bgfxStatsLog.shadowApplySubmits += g_stats.shadowApplySubmits;
    g_bgfxStatsLog.smudgeSubmits += g_stats.smudgeSubmits;
    g_bgfxStatsLog.sceneCompositeSubmits += g_stats.sceneCompositeSubmits;
    g_bgfxStatsLog.debugSubmits += g_stats.debugSubmits;
    g_bgfxStatsLog.worldDraws += g_stats.worldDraws;
    g_bgfxStatsLog.uiDraws += g_stats.uiDraws;
    g_bgfxStatsLog.waterDraws += g_stats.waterDraws;
    g_bgfxStatsLog.sortedDraws += g_stats.sortedDraws;
    g_bgfxStatsLog.effectDraws += g_stats.effectDraws;
    g_bgfxStatsLog.rttDraws += g_stats.rttDraws;
    g_bgfxStatsLog.smudgeDraws += g_stats.smudgeDraws;
    g_bgfxStatsLog.textureBinds += g_stats.textureBinds;
    g_bgfxStatsLog.textureCreates += g_stats.textureCreates;
    g_bgfxStatsLog.textureUploads += g_stats.textureUploads;
    g_bgfxStatsLog.textureCopies += g_stats.textureCopies;
    g_bgfxStatsLog.materialUniformUploads += g_stats.materialUniformUploads;
    g_bgfxStatsLog.lightUniformUploads += g_stats.lightUniformUploads;
    g_bgfxStatsLog.textureTransformUpdates += g_stats.textureTransformUpdates;
    g_bgfxStatsLog.renderStateCopies += g_stats.renderStateCopies;
    g_bgfxStatsLog.transientVbAllocations += g_stats.transientVbAllocations;
    g_bgfxStatsLog.transientIbAllocations += g_stats.transientIbAllocations;
    g_bgfxStatsLog.transientVbDraws += g_stats.transientVbDraws;
    g_bgfxStatsLog.transientIbDraws += g_stats.transientIbDraws;
    g_bgfxStatsLog.dynamicVbAllocations += g_stats.dynamicVbAllocations;
    g_bgfxStatsLog.dynamicIbAllocations += g_stats.dynamicIbAllocations;
    g_bgfxStatsLog.instancedSavedDrawCalls += g_stats.instancedSavedDrawCalls;

    if (g_bgfxStatsLog.windowSeconds >= 1.0)
    {
        FlushBgfxStatsLogWindow();
    }
}

static void LogFrameStats()
{
#ifdef RTS_DEBUG
    if (g_stats.frameIndex <= 10 || (g_stats.frameIndex % 60) == 0)
    {
        WWDEBUG_SAY(("[BGFX PERF] frame=%u draws=%u skipped=%u submits(base/depth/vol/apply/smudge/comp/debug)=%u/%u/%u/%u/%u/%u/%u views(world/ui/water/sort/effect/rtt/smudge)=%u/%u/%u/%u/%u/%u/%u binds=%u uniforms(mat/light)=%u/%u texxf=%u rsCopies=%u transientAlloc(vb/ib)=%u/%u transientDraw(vb/ib)=%u/%u dynAlloc(vb/ib)=%u/%u",
            g_stats.frameIndex,
            g_stats.drawCalls,
            g_stats.skippedDraws,
            g_stats.baseSubmits,
            g_stats.sceneDepthSubmits,
            g_stats.shadowVolumeSubmits,
            g_stats.shadowApplySubmits,
            g_stats.smudgeSubmits,
            g_stats.sceneCompositeSubmits,
            g_stats.debugSubmits,
            g_stats.worldDraws,
            g_stats.uiDraws,
            g_stats.waterDraws,
            g_stats.sortedDraws,
            g_stats.effectDraws,
            g_stats.rttDraws,
            g_stats.smudgeDraws,
            g_stats.textureBinds,
            g_stats.materialUniformUploads,
            g_stats.lightUniformUploads,
            g_stats.textureTransformUpdates,
            g_stats.renderStateCopies,
            g_stats.transientVbAllocations,
            g_stats.transientIbAllocations,
            g_stats.transientVbDraws,
            g_stats.transientIbDraws,
            g_stats.dynamicVbAllocations,
            g_stats.dynamicIbAllocations));
    }
#endif
}

// TheSuperHackers @refactor bobtista 15/04/2026 bgfx callback
// so fatal errors and debug trace messages land in DebugLogFileD.txt
// instead of silently firing bx::debugBreak. Without this, internal
// bgfx validation failures produce only a raw breakpoint with no text.
class BgfxLoggingCallback : public bgfx::CallbackI
{
public:
#if defined(RTS_PROFILE_TRACY)
    BgfxLoggingCallback()
        : m_profilerDepth(0)
        , m_profilerOverflow(0)
    {
    }
#endif
    ~BgfxLoggingCallback() override {}

    void fatal(const char * filePath, uint16_t line, bgfx::Fatal::Enum code, const char * str) override
    {
        // TheSuperHackers @build bobtista 30/04/2026 Always print bgfx fatal
        // messages to stderr — WWDEBUG_SAY is a no-op in release builds, but
        // we want diagnostics for the macOS bring-up.
        std::fprintf(stderr, "[bgfx] FATAL code=%d at %s:%u: %s\n",
                     static_cast<int>(code), filePath ? filePath : "?", line, str ? str : "?");
        std::fflush(stderr);
    }
    void traceVargs(const char * filePath, uint16_t line, const char * format, va_list argList) override
    {
        // TheSuperHackers @perf bobtista 02/06/2026 A Debug-config build compiles bgfx
        // itself in Debug, which turns on its internal BX_TRACE narration (per texture,
        // per uniform, per bind). Printing+flushing every one of those thousands of lines
        // per frame dragged the Debug build to ~5fps. Suppress the informational flood by
        // default; surface WARN/ERROR lines always, and emit everything only when the
        // operator opts in via GGC_TRACE. The level word is literal in bgfx's format
        // string, so we can classify before formatting and skip the vsnprintf entirely
        // for suppressed lines.
        static const bool s_verbose = (std::getenv("GGC_TRACE") != nullptr);
        const bool isImportant = (format != nullptr)
            && (std::strstr(format, "WARN") != nullptr || std::strstr(format, "ERROR") != nullptr);
        if (!s_verbose && !isImportant)
        {
            return;
        }
        char buf[512];
        std::vsnprintf(buf, sizeof(buf), format, argList);
        size_t len = std::strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) { buf[--len] = '\0'; }
        std::fprintf(stderr, "[bgfx] %s:%u: %s\n", filePath ? filePath : "?", line, buf);
        // Flush important lines so they survive a subsequent crash; let the opt-in verbose
        // flood rely on normal stdio buffering to avoid a syscall per line.
        if (isImportant)
        {
            std::fflush(stderr);
        }
    }
    void profilerBegin(const char * name, uint32_t /*abgr*/, const char * filePath, uint16_t line) override
    {
#if defined(RTS_PROFILE_TRACY)
        if (m_profilerDepth >= kProfilerStackMax)
        {
            ++m_profilerOverflow;
            return;
        }
        const uint64_t srcloc = ___tracy_alloc_srcloc_name(
            line, filePath, std::strlen(filePath),
            name, std::strlen(name),
            name, std::strlen(name), 0);
        m_profilerStack[m_profilerDepth] = ___tracy_emit_zone_begin_alloc(srcloc, 1);
        ++m_profilerDepth;
#else
        (void)name; (void)filePath; (void)line;
#endif
    }
    void profilerBeginLiteral(const char * name, uint32_t /*abgr*/, const char * filePath, uint16_t line) override
    {
#if defined(RTS_PROFILE_TRACY)
        if (m_profilerDepth >= kProfilerStackMax)
        {
            ++m_profilerOverflow;
            return;
        }
        const struct ___tracy_source_location_data srcloc = { name, name, filePath, line, 0 };
        m_profilerStack[m_profilerDepth] = ___tracy_emit_zone_begin(&srcloc, 1);
        ++m_profilerDepth;
#else
        (void)name; (void)filePath; (void)line;
#endif
    }
    void profilerEnd() override
    {
#if defined(RTS_PROFILE_TRACY)
        if (m_profilerOverflow > 0)
        {
            --m_profilerOverflow;
            return;
        }
        if (m_profilerDepth > 0)
        {
            --m_profilerDepth;
            ___tracy_emit_zone_end(m_profilerStack[m_profilerDepth]);
        }
#endif
    }
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void *, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void *, uint32_t) override {}
    void screenShot(const char * filePath, uint32_t width, uint32_t height, uint32_t pitch,
                    bgfx::TextureFormat::Enum /*format*/, const void * data, uint32_t /*size*/,
                    bool yflip) override
    {
        // bgfx delivers BGRA8 pixels; emit a minimal 32-bpp BMP. BMP is widely
        // readable by the Win32 GDI / .NET imaging stack with no extra
        // dependencies, so any tooling that picks up the captures can decode
        // them directly. The filePath's extension is not inspected — caller
        // controls naming.
        if (filePath == nullptr || data == nullptr || width == 0 || height == 0)
        {
            return;
        }
        FILE * f = fopen(filePath, "wb");
        if (f == nullptr)
        {
            WWDEBUG_SAY(("[BgfxBackend] screenShot: fopen %s failed", filePath));
            return;
        }
        const uint32_t rowBytes = width * 4;
        const uint32_t pixelBytes = rowBytes * height;
        const uint32_t fileSize = 14 + 40 + pixelBytes;
        uint8_t fileHdr[14] = {0};
        fileHdr[0] = 'B'; fileHdr[1] = 'M';
        fileHdr[2] = static_cast<uint8_t>(fileSize & 0xFF);
        fileHdr[3] = static_cast<uint8_t>((fileSize >> 8) & 0xFF);
        fileHdr[4] = static_cast<uint8_t>((fileSize >> 16) & 0xFF);
        fileHdr[5] = static_cast<uint8_t>((fileSize >> 24) & 0xFF);
        fileHdr[10] = 54; // pixel data offset
        uint8_t infoHdr[40] = {0};
        infoHdr[0] = 40;
        infoHdr[4] = static_cast<uint8_t>(width & 0xFF);
        infoHdr[5] = static_cast<uint8_t>((width >> 8) & 0xFF);
        infoHdr[6] = static_cast<uint8_t>((width >> 16) & 0xFF);
        infoHdr[7] = static_cast<uint8_t>((width >> 24) & 0xFF);
        // BMP: positive height = bottom-up storage (first row = bottom);
        // negative height = top-down storage (first row = top). bgfx
        // yflip=true means data is bottom-up (OpenGL origin), yflip=false
        // means top-down (D3D origin). Match accordingly so we never flip
        // on the CPU.
        const int32_t h = static_cast<int32_t>(height);
        const int32_t signedH = yflip ? h : -h;
        const uint32_t storeH = static_cast<uint32_t>(signedH);
        infoHdr[8]  = static_cast<uint8_t>(storeH & 0xFF);
        infoHdr[9]  = static_cast<uint8_t>((storeH >> 8) & 0xFF);
        infoHdr[10] = static_cast<uint8_t>((storeH >> 16) & 0xFF);
        infoHdr[11] = static_cast<uint8_t>((storeH >> 24) & 0xFF);
        infoHdr[12] = 1;  // planes
        infoHdr[14] = 32; // bpp
        // BI_RGB compression = 0 (no compression). BGRA byte order is the
        // BMP default for 32bpp BI_RGB.
        fwrite(fileHdr, 1, sizeof(fileHdr), f);
        fwrite(infoHdr, 1, sizeof(infoHdr), f);
        const uint8_t * src = static_cast<const uint8_t *>(data);
        for (uint32_t y = 0; y < height; ++y)
        {
            fwrite(src + y * pitch, 1, rowBytes, f);
        }
        fclose(f);
        WWDEBUG_SAY(("[BgfxBackend] screenShot wrote %ux%u to %s", width, height, filePath));
    }
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void *, uint32_t) override {}

#if defined(RTS_PROFILE_TRACY)
    static const int kProfilerStackMax = 64;
    TracyCZoneCtx m_profilerStack[kProfilerStackMax];
    int m_profilerDepth;
    int m_profilerOverflow;
#endif
};

BgfxLoggingCallback g_bgfxCallback;

static uint32_t MapCmpFuncToBgfxStencilTest(CompareFunc f)
{
    switch (f)
    {
        case RB_CMP_NEVER:         return BGFX_STENCIL_TEST_NEVER;
        case RB_CMP_LESS:          return BGFX_STENCIL_TEST_LESS;
        case RB_CMP_EQUAL:         return BGFX_STENCIL_TEST_EQUAL;
        case RB_CMP_LESS_EQUAL:    return BGFX_STENCIL_TEST_LEQUAL;
        case RB_CMP_GREATER:       return BGFX_STENCIL_TEST_GREATER;
        case RB_CMP_NOT_EQUAL:     return BGFX_STENCIL_TEST_NOTEQUAL;
        case RB_CMP_GREATER_EQUAL: return BGFX_STENCIL_TEST_GEQUAL;
        case RB_CMP_ALWAYS: default: return BGFX_STENCIL_TEST_ALWAYS;
    }
}

static uint32_t MapStencilOpToBgfx(StencilOp op, uint32_t shift)
{
    uint32_t ord;
    switch (op)
    {
        case RB_STENCIL_OP_KEEP:     ord = 1; break;
        case RB_STENCIL_OP_ZERO:     ord = 0; break;
        case RB_STENCIL_OP_REPLACE:  ord = 2; break;
        case RB_STENCIL_OP_INCR_SAT: ord = 4; break;
        case RB_STENCIL_OP_DECR_SAT: ord = 6; break;
        case RB_STENCIL_OP_INVERT:   ord = 7; break;
        case RB_STENCIL_OP_INCR:     ord = 3; break;
        case RB_STENCIL_OP_DECR:     ord = 5; break;
        default:                     ord = 1; break;
    }
    return ord << shift;
}

static uint32_t BuildCurrentStencilState()
{
    if (!g_draw.stencilEnabled)
    {
        return BGFX_STENCIL_NONE;
    }
    return g_draw.stencilFuncBits
        | BGFX_STENCIL_FUNC_REF(g_draw.stencilRef & 0xFF)
        | BGFX_STENCIL_FUNC_RMASK(g_draw.stencilReadMask & 0xFF)
        | g_draw.stencilFailOpBits
        | g_draw.stencilZFailOpBits
        | g_draw.stencilPassOpBits;
}

static void UpdateShadowStencilState()
{
    g_draw.shadowStencilFront = BuildCurrentStencilState();
    if (g_draw.shadowStencilFront == BGFX_STENCIL_NONE)
    {
        g_draw.shadowStencilBack = BGFX_STENCIL_NONE;
        return;
    }
    g_draw.shadowStencilBack = BGFX_STENCIL_NONE;
}

// TheSuperHackers @refactor bobtista 26/04/2026 Shader program creation
// helper. Creates a bgfx program from compiled bytecode, sets debug names,
// and cleans up on failure.
bgfx::ProgramHandle CreateShaderProgram(
    const uint8_t * vsData, uint32_t vsSize, const char * vsName,
    const uint8_t * fsData, uint32_t fsSize, const char * fsName)
{
    bgfx::ShaderHandle vs = bgfx::createShader(bgfx::makeRef(vsData, vsSize));
    bgfx::ShaderHandle fs = bgfx::createShader(bgfx::makeRef(fsData, fsSize));
    if (bgfx::isValid(vs) && bgfx::isValid(fs))
    {
        bgfx::setName(vs, vsName);
        bgfx::setName(fs, fsName);
        return bgfx::createProgram(vs, fs, true);
    }
    if (bgfx::isValid(vs))
    {
        bgfx::destroy(vs);
    }
    if (bgfx::isValid(fs))
    {
        bgfx::destroy(fs);
    }
    WWDEBUG_SAY(("[BgfxBackend] %s + %s createShader FAILED.", vsName, fsName));
    return BGFX_INVALID_HANDLE;
}

bgfx::RendererType::Enum GetConfiguredRendererType()
{
#if defined(__APPLE__)
    // TheSuperHackers @bugfix bobtista 30/04/2026 GGC_BGFX_RENDERER lets
    // us A/B-test backends at run time on macOS. Apple's Metal JIT
    // compiler (AGX/MTLCompiler) has multiple intermittent crash bugs
    // on macOS Tahoe + M4 - hand-compiling fragment shaders, EOT
    // helpers, and constant-clear programs all crash with EXC_BAD_ACCESS
    // at different rates per launch. OpenGL backend goes through a
    // separate (older, deprecated, but stable) driver path so it is
    // useful as a fallback. "metal" / "gl" / "vulkan" supported.
    const char *override_ = std::getenv("GGC_BGFX_RENDERER");
    if (override_ != nullptr)
    {
        if (std::strcmp(override_, "gl") == 0 || std::strcmp(override_, "opengl") == 0)
        {
            return bgfx::RendererType::OpenGL;
        }
        if (std::strcmp(override_, "vulkan") == 0)
        {
            return bgfx::RendererType::Vulkan;
        }
    }
#endif
#if defined(GGC_BGFX_RENDERER_METAL)
    return bgfx::RendererType::Metal;
#elif defined(GGC_BGFX_RENDERER_VULKAN)
    return bgfx::RendererType::Vulkan;
#else
    return bgfx::RendererType::Direct3D11;
#endif
}

void *GetNativeWindowHandle(void *window)
{
    if (window == NULL)
    {
        return NULL;
    }
#if defined(SAGE_USE_SDL3)
#if defined(__APPLE__)
    // TheSuperHackers @bugfix bobtista 30/04/2026 SDL_Metal_CreateView
    // gives us a CAMetalLayer that bgfx can take as platformData.nwh
    // directly. Passing the NSWindow instead lets bgfx try to install
    // its own CAMetalLayer on the contentView, which fights with
    // SDL3's own layer setup and intermittently crashes Apple's AGX
    // driver in AGCDeserializedReply during pipeline-state compile.
    // GGC_MACOS_USE_NSWINDOW=1 forces the legacy NSWindow path so we
    // can A/B-test which form the local Apple Silicon variant prefers.
    if (::TheSDL3MetalLayer != NULL && std::getenv("GGC_MACOS_USE_NSWINDOW") == nullptr)
    {
        return ::TheSDL3MetalLayer;
    }
#endif
    SDL_Window *sdlWindow = static_cast<SDL_Window *>(window);
    SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
#if defined(__APPLE__)
    void *nativeWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (nativeWindow != NULL)
    {
        return nativeWindow;
    }
#elif defined(_WIN32)
    void *nativeWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (nativeWindow != NULL)
    {
        return nativeWindow;
    }
#endif
#endif
    return window;
}

void BuildStandardVertexLayouts()
{
    g_device.layoutP
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

    g_device.layoutPN
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
        .end();

    g_device.layoutPNT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_device.layoutPNT2
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
        .end();

    g_device.layoutPT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_device.layoutPDT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_device.layoutPNDT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_device.layoutPNDT2
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
        .end();
}

}

BgfxBackend::BgfxBackend()
    : m_textureBitDepth(16)
    , m_msaaMode(RB_MSAA_NONE)
{
    WWDEBUG_SAY(("[BgfxBackend] Backend constructed."));
}

BgfxBackend::~BgfxBackend()
{
}

// -- Backend lifecycle -------------------------------------------------------

namespace
{
// TheSuperHackers @refactor bobtista 11/04/2026 ShaderClass
// translation table. Maps a ShaderClass instance to (program handle,
// bgfx state bits).
//
// Mapping rules:
//   - alpha-test enabled & textured & lit -> g_texturedLitAtestProgram
//   - textured & lit                      -> g_texturedLitProgram
//   - textured & !lit                     -> g_texturedUnlitProgram
//   - !textured & lit                     -> g_solidLitProgram
//   - else                                -> g_device.passthroughProgram (debug)
//
// State bit translation is mechanical: depth compare, depth write, color
// write, blend factors, cull. Detail-blend / fog / specular gradient need
// shader code rather than state bits, so they are not handled by this
// translation.

uint64_t TranslateBlendFactor(ShaderClass::SrcBlendFuncType src)
{
    switch (src)
    {
        case ShaderClass::SRCBLEND_ZERO:                 return BGFX_STATE_BLEND_ZERO;
        case ShaderClass::SRCBLEND_ONE:                  return BGFX_STATE_BLEND_ONE;
        case ShaderClass::SRCBLEND_SRC_ALPHA:            return BGFX_STATE_BLEND_SRC_ALPHA;
        case ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA:  return BGFX_STATE_BLEND_INV_SRC_ALPHA;
        default:                                         return BGFX_STATE_BLEND_ONE;
    }
}

uint64_t TranslateBlendFactor(ShaderClass::DstBlendFuncType dst)
{
    switch (dst)
    {
        case ShaderClass::DSTBLEND_ZERO:                 return BGFX_STATE_BLEND_ZERO;
        case ShaderClass::DSTBLEND_ONE:                  return BGFX_STATE_BLEND_ONE;
        case ShaderClass::DSTBLEND_SRC_COLOR:            return BGFX_STATE_BLEND_SRC_COLOR;
        case ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR:  return BGFX_STATE_BLEND_INV_SRC_COLOR;
        case ShaderClass::DSTBLEND_SRC_ALPHA:            return BGFX_STATE_BLEND_SRC_ALPHA;
        case ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA:  return BGFX_STATE_BLEND_INV_SRC_ALPHA;
        default:                                         return BGFX_STATE_BLEND_ZERO;
    }
}

uint64_t TranslateDepthCompare(ShaderClass::DepthCompareType cmp)
{
    switch (cmp)
    {
        case ShaderClass::PASS_NEVER:    return BGFX_STATE_DEPTH_TEST_NEVER;
        case ShaderClass::PASS_LESS:     return BGFX_STATE_DEPTH_TEST_LESS;
        case ShaderClass::PASS_EQUAL:    return BGFX_STATE_DEPTH_TEST_EQUAL;
        case ShaderClass::PASS_LEQUAL:   return BGFX_STATE_DEPTH_TEST_LEQUAL;
        case ShaderClass::PASS_GREATER:  return BGFX_STATE_DEPTH_TEST_GREATER;
        case ShaderClass::PASS_NOTEQUAL: return BGFX_STATE_DEPTH_TEST_NOTEQUAL;
        case ShaderClass::PASS_GEQUAL:   return BGFX_STATE_DEPTH_TEST_GEQUAL;
        case ShaderClass::PASS_ALWAYS:   return BGFX_STATE_DEPTH_TEST_ALWAYS;
        default:                         return BGFX_STATE_DEPTH_TEST_LEQUAL;
    }
}

CompareFunc MapShaderDepthCompareToBackendCompare(ShaderClass::DepthCompareType cmp)
{
    switch (cmp)
    {
        case ShaderClass::PASS_NEVER:    return RB_CMP_NEVER;
        case ShaderClass::PASS_LESS:     return RB_CMP_LESS;
        case ShaderClass::PASS_EQUAL:    return RB_CMP_EQUAL;
        case ShaderClass::PASS_LEQUAL:   return RB_CMP_LESS_EQUAL;
        case ShaderClass::PASS_GREATER:  return RB_CMP_GREATER;
        case ShaderClass::PASS_NOTEQUAL: return RB_CMP_NOT_EQUAL;
        case ShaderClass::PASS_GEQUAL:   return RB_CMP_GREATER_EQUAL;
        case ShaderClass::PASS_ALWAYS:   return RB_CMP_ALWAYS;
        default:                         return RB_CMP_LESS_EQUAL;
    }
}

// Extract TSS operation IDs from ShaderClass preset bits.
// Maps the same logic as shader.cpp's Apply() into float IDs that the
// uber fragment shader evaluates at runtime.
//
// TSS op IDs must match the #defines in fs_uber.sc:
//   0=DISABLE 1=SELECTARG1 2=SELECTARG2 3=MODULATE 4=MODULATE2X
//   5=ADD 6=ADDSIGNED 7=SUBTRACT 8=BLENDTEXALPHA 9=BLENDCURALPHA 10=ADDSMOOTH
// Arg source IDs: 0=TEXTURE 1=DIFFUSE 2=CURRENT

// Default legacy alpha-test reference (0x60/255 = 0.376) used when a shader has ALPHATEST enabled without an explicit reference. Matches the implicit behavior preserved by ShaderClass presets.
const float kDefaultAlphaTestRef = 0x60 / 255.0f;

void BuildTssOpsForShader(const ShaderClass & shader,
                          float * ops0, float * ops1, float * atestRef, float * atestFunc)
{
    float priColorOp  = 3.0f; // MODULATE
    float priAlphaOp  = 3.0f; // MODULATE
    float priCArg1Src = 0.0f; // TEXTURE
    float priAArg1Src = 0.0f; // TEXTURE
    float secColorOp  = 0.0f; // DISABLE
    float secAlphaOp  = 0.0f; // DISABLE
    float secCArg1Src = 0.0f; // TEXTURE
    float secAArg1Src = 0.0f; // TEXTURE

    if (shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE)
    {
        switch (shader.Get_Primary_Gradient())
        {
            case ShaderClass::GRADIENT_DISABLE:
                priColorOp  = kTssSelectArg1;
                priAlphaOp  = kTssSelectArg1;
                priCArg1Src = kTssArgTexture;
                priAArg1Src = kTssArgTexture;
                break;
            default:
            case ShaderClass::GRADIENT_MODULATE:
                priColorOp  = kTssModulate;
                priAlphaOp  = kTssModulate;
                priCArg1Src = kTssArgTexture;
                priAArg1Src = kTssArgTexture;
                break;
            case ShaderClass::GRADIENT_ADD:
                priColorOp  = kTssAdd;
                priAlphaOp  = kTssModulate;
                priCArg1Src = kTssArgTexture;
                priAArg1Src = kTssArgTexture;
                break;
            case ShaderClass::GRADIENT_MODULATE2X:
                priColorOp  = kTssModulate2x;
                priAlphaOp  = kTssModulate;
                priCArg1Src = kTssArgTexture;
                priAArg1Src = kTssArgTexture;
                break;
            case ShaderClass::GRADIENT_BUMPENVMAP:
            case ShaderClass::GRADIENT_BUMPENVMAPLUMINANCE:
                priColorOp  = kTssSelectArg1;
                priAlphaOp  = kTssSelectArg1;
                priCArg1Src = kTssArgDiffuse;
                priAArg1Src = kTssArgDiffuse;
                break;
        }

        switch (shader.Get_Post_Detail_Color_Func())
        {
            default:
            case ShaderClass::DETAILCOLOR_DISABLE:
                secColorOp = kTssDisable;
                break;
            case ShaderClass::DETAILCOLOR_DETAIL:
                secColorOp  = kTssSelectArg1;
                secCArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILCOLOR_SCALE:
                secColorOp  = kTssModulate;
                secCArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILCOLOR_INVSCALE:
                secColorOp  = kTssAddSmooth;
                secCArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILCOLOR_ADD:
                secColorOp  = kTssAdd;
                secCArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILCOLOR_SUB:
                secColorOp  = kTssSubtract;
                secCArg1Src = kTssArgCurrent; // result = current - tex
                break;
            case ShaderClass::DETAILCOLOR_SUBR:
                secColorOp  = kTssSubtract;
                secCArg1Src = kTssArgTexture; // result = tex - current
                break;
            case ShaderClass::DETAILCOLOR_BLEND:
                secColorOp  = kTssBlendTexAlpha;
                secCArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILCOLOR_DETAILBLEND:
                secColorOp  = kTssBlendCurAlpha;
                secCArg1Src = kTssArgTexture;
                break;
        }

        switch (shader.Get_Post_Detail_Alpha_Func())
        {
            default:
            case ShaderClass::DETAILALPHA_DISABLE:
                secAlphaOp = kTssDisable;
                break;
            case ShaderClass::DETAILALPHA_DETAIL:
                secAlphaOp  = kTssSelectArg1;
                secAArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILALPHA_SCALE:
                secAlphaOp  = kTssModulate;
                secAArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILALPHA_INVSCALE:
                secAlphaOp  = kTssAddSmooth;
                secAArg1Src = kTssArgTexture;
                break;
        }
    }
    else
    {
        switch (shader.Get_Primary_Gradient())
        {
            case ShaderClass::GRADIENT_DISABLE:
                priColorOp = kTssDisable;
                priAlphaOp = kTssDisable;
                break;
            default:
            case ShaderClass::GRADIENT_MODULATE:
            case ShaderClass::GRADIENT_ADD:
                priColorOp  = kTssSelectArg2;
                priAlphaOp  = kTssSelectArg2;
                priCArg1Src = kTssArgTexture;
                priAArg1Src = kTssArgTexture;
                break;
        }
    }

    ops0[0] = priColorOp;
    ops0[1] = priAlphaOp;
    ops0[2] = secColorOp;
    ops0[3] = secAlphaOp;

    ops1[0] = priCArg1Src;
    ops1[1] = priAArg1Src;
    ops1[2] = secCArg1Src;
    ops1[3] = secAArg1Src;

    if (shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE)
    {
        if (shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA)
        {
            *atestRef = 1.0f - kDefaultAlphaTestRef;
            *atestFunc = static_cast<float>(RB_CMP_LESS_EQUAL);
        }
        else
        {
            *atestRef = kDefaultAlphaTestRef;
            *atestFunc = static_cast<float>(RB_CMP_GREATER_EQUAL);
        }
    }
    else
    {
        *atestRef = 0.0f;
        *atestFunc = 0.0f;
    }
}

uint64_t BuildBgfxStateForShader(const ShaderClass & shader)
{
    uint64_t state = 0;

    if (shader.Get_Color_Mask() == ShaderClass::COLOR_WRITE_ENABLE)
    {
        state |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    }

    if (shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_ENABLE)
    {
        // W3D used D3D's clockwise = front by default. bgfx CW culls the
        // clockwise face (i.e. the back face when CW is the front), so
        // BGFX_STATE_CULL_CW matches the W3D convention.
        state |= BGFX_STATE_CULL_CW;
    }

    return state;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Generic FVF
// to bgfx::VertexLayout translator. Walks the FVFInfoClass offset
// table and emits attributes in offset order. Handles arbitrary FVF
// combinations including padding and unused texcoord stages by issuing
// skip() calls between attributes whose offsets do not abut.
//
// The bgfx layout API does not let you assign explicit offsets, only
// running totals via add()/skip(). So this helper accumulates the
// offsets in source order and emits skip() calls to fast-forward to
// each attribute's true offset, then adds the attribute itself.
//
// Returns true if a layout was built. Caller must have called begin()
// already and must call end() afterwards.

void AddAttribAtOffset(bgfx::VertexLayout & layout,
                       unsigned & cursor,
                       unsigned target_offset,
                       bgfx::Attrib::Enum attr,
                       uint8_t count,
                       bgfx::AttribType::Enum type,
                       bool normalized,
                       unsigned attr_size_bytes)
{
    if (target_offset > cursor)
    {
        layout.skip(static_cast<uint8_t>(target_offset - cursor));
        cursor = target_offset;
    }
    layout.add(attr, count, type, normalized);
    cursor += attr_size_bytes;
}

// TheSuperHackers @perf bobtista 28/04/2026 Cache built layouts keyed by
// FVF bits. FVFInfoClass derives all offsets from the FVF bits in its
// constructor, so two instances with the same Get_FVF() produce identical
// layouts. Hit on every dynamic capture (particles, lines, 2D quads) and
// every static VB upload — the engine churns through a handful of FVF
// combos repeatedly.
static bool BuildBgfxLayoutForFVFUncached(const FVFInfoClass & fvf, bgfx::VertexLayout & out);

bool BuildBgfxLayoutForFVF(const FVFInfoClass & fvf, bgfx::VertexLayout & out)
{
    struct CachedLayout
    {
        bgfx::VertexLayout layout;
        bool ok;
    };
    static std::unordered_map<unsigned, CachedLayout> s_cache;
    const unsigned key = fvf.Get_FVF();
    std::unordered_map<unsigned, CachedLayout>::iterator it = s_cache.find(key);
    if (it != s_cache.end())
    {
        out = it->second.layout;
        return it->second.ok;
    }
    CachedLayout entry;
    entry.ok = BuildBgfxLayoutForFVFUncached(fvf, entry.layout);
    out = entry.layout;
    s_cache[key] = entry;
    return entry.ok;
}

static bool BuildBgfxLayoutForFVFUncached(const FVFInfoClass & fvf, bgfx::VertexLayout & out)
{
    const unsigned bits      = fvf.Get_FVF();
    const unsigned totalSize = fvf.Get_FVF_Size();

    out.begin();
    unsigned cursor = 0;

    if ((bits & DX8_FVF_FLAG_XYZ) == DX8_FVF_FLAG_XYZ)
    {
        AddAttribAtOffset(out, cursor, fvf.Get_Location_Offset(),
                          bgfx::Attrib::Position, 3, bgfx::AttribType::Float, false,
                          3 * sizeof(float));
    }
    else if ((bits & DX8_FVF_FLAG_XYZRHW) == DX8_FVF_FLAG_XYZRHW)
    {
        // Pre-transformed: 4 floats (x, y, z, rhw). bgfx has no native
        // pre-transformed attribute - declare as 4-component position
        // and the shader is expected to bypass the projection matrix.
        AddAttribAtOffset(out, cursor, fvf.Get_Location_Offset(),
                          bgfx::Attrib::Position, 4, bgfx::AttribType::Float, false,
                          4 * sizeof(float));
    }

    if (fvf.Has_Normal())
    {
        AddAttribAtOffset(out, cursor, fvf.Get_Normal_Offset(),
                          bgfx::Attrib::Normal, 3, bgfx::AttribType::Float, false,
                          3 * sizeof(float));
    }

    if (fvf.Has_Diffuse())
    {
        // Legacy vertex color is BGRA u8x4 packed; the shader swizzles it
        // back to engine ARGB channels after bgfx normalizes the bytes.
        AddAttribAtOffset(out, cursor, fvf.Get_Diffuse_Offset(),
                          bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true,
                          sizeof(uint32_t));
    }

    if (fvf.Has_Specular())
    {
        AddAttribAtOffset(out, cursor, fvf.Get_Specular_Offset(),
                          bgfx::Attrib::Color1, 4, bgfx::AttribType::Uint8, true,
                          sizeof(uint32_t));
    }

    // Texcoord sets - decode each stage's size from the FVF bits.
    const bgfx::Attrib::Enum kTexAttr[8] = {
        bgfx::Attrib::TexCoord0, bgfx::Attrib::TexCoord1,
        bgfx::Attrib::TexCoord2, bgfx::Attrib::TexCoord3,
        bgfx::Attrib::TexCoord4, bgfx::Attrib::TexCoord5,
        bgfx::Attrib::TexCoord6, bgfx::Attrib::TexCoord7,
    };

    const unsigned numTex = fvf.Get_UV_Channel_Count();
    for (unsigned i = 0; i < numTex && i < 8; ++i)
    {
        unsigned componentCount = 2;
        if ((bits & DX8_FVF_TEXCOORDSIZE1(i)) == DX8_FVF_TEXCOORDSIZE1(i))
        {
            componentCount = 1;
        }
        else if ((bits & DX8_FVF_TEXCOORDSIZE3(i)) == DX8_FVF_TEXCOORDSIZE3(i))
        {
            componentCount = 3;
        }
        else if ((bits & DX8_FVF_TEXCOORDSIZE4(i)) == DX8_FVF_TEXCOORDSIZE4(i))
        {
            componentCount = 4;
        }
        // else default 2

        AddAttribAtOffset(out, cursor, fvf.Get_Tex_Offset(i),
                          kTexAttr[i],
                          static_cast<uint8_t>(componentCount),
                          bgfx::AttribType::Float, false,
                          componentCount * sizeof(float));
    }

    // Pad up to the FVF stride if there is trailing space the bgfx
    // layout has not accounted for. This keeps strides in lockstep so
    // bgfx reads vertices at the same byte boundaries the engine writes
    // them at.
    if (totalSize > cursor)
    {
        out.skip(static_cast<uint8_t>(totalSize - cursor));
    }

    out.end();
    return out.getStride() == totalSize;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Vertex/index buffer caches keyed by the
// source buffer pointer: copy bytes on first sight, create a bgfx buffer, destroy
// wholesale in Shutdown.

// TheSuperHackers @refactor bobtista 11/04/2026 Switched from
// static bgfx VB/IB handles to dynamic ones. Rigid mesh category containers
// fill their shared VB / IB one sub-range at a time via AppendLockClass,
// which requires in-place sub-range updates that only dynamic bgfx buffers
// support. Full-buffer writes (WriteLockClass) also go through the same
// dynamic path - created once, updated with bgfx::update as the engine
// rewrites the buffer.
// TheSuperHackers @refactor bobtista 15/04/2026 Cache entries
// store (handle, num_verts, stride) so we can detect the case where the
// engine destroys a VertexBufferClass and reuses the memory address for
// a new VB with different dimensions — otherwise we'd hand back a stale
// handle and bgfx would truncate writes / crash on staging creation.
// TheSuperHackers @fix bobtista 19/04/2026 Track raw texture pointers
// and dimensions alongside TextureClass* to detect stale cache entries
// (address reuse) and enable in-place updates for same-sized textures.
// TheSuperHackers @fix bobtista 19/04/2026 Deferred texture destruction.
// When a stale texture cache entry is detected (raw pointer changed), the
// old bgfx handle can't be destroyed immediately because in-flight draws
// may still reference it. Double-buffer: collect in current frame, destroy
// after the NEXT bgfx::frame() (2 frames later = guaranteed safe).

// The bgfx texture currently bound to stage 0 by Set_Texture.

// Per-stage sampler flags captured from the source TextureClass's
// Get_U/V_Addr_Mode in Set_Texture. Default 0 = use bgfx's creation-time
// default (usually linear filter + wrap). Shoreline LUT needs CLAMP
// because its U coord can exceed [0,1] and WRAP produces a visible
// stripe/checker artifact at the boundary.

// The most recent buffers and offsets cached from Set_Vertex_Buffer /
// Set_Index_Buffer. Read by Draw_Triangles when it issues the bgfx
// submit. Cleared (made invalid) on Shutdown.

// TheSuperHackers @refactor bobtista 11/04/2026 Transient
// (dynamic) buffer state. Capture_Dynamic_Vertex_Data allocs a bgfx
// transient VB and records the owning DynamicVBAccessClass pointer so
// the matching Set_Vertex_Buffer(DynamicVBAccessClass&) call can claim
// it. The transient buffers are auto-freed at bgfx::frame time; we
// only track validity within the current frame.
// Current draw call uses transient buffers if these are set. They
// shadow the static VB/IB handles above - SubmitEngineDraw picks the
// transient path when these are true.

// TheSuperHackers @refactor bobtista 11/04/2026 Transform
// capture. The engine calls Set_Transform with world / view / projection
// matrices in W3D row-major form (Vector4 Row[4]). bgfx wants column-
// major float[16] for setViewTransform / setTransform. We convert with
// a transpose copy.
//
// We capture all three matrices and apply them per-submit. View and
// projection are written via setViewTransform on view 1; world is set
// per-submit via setTransform.

// Snapshot of g_frame.view and g_frame.proj captured at the first opaque
// draw of each frame. Re-applied to view 1 at End_Scene to prevent
// later Set_Projection calls (water, shadows, sneak attack) from
// retroactively stomping the camera projection via setViewTransform.

// Engine geometry submits to its own view so it does not collide with
// the test triangle on view 0. View 0 keeps the test triangle for the
// "is bgfx alive" sentinel; view 1 is engine geometry under engine
// transforms. Both render to the popup back buffer.
const bgfx::ViewId kBgfxDebugView  = 0;
const bgfx::ViewId kBgfxEngineView = 1;
const int kBgfxTextureStages = 4;

// TheSuperHackers @refactor bobtista 11/04/2026 Dedicated
// view id for sorted draws. View 2's view matrix is permanently
// identity and its projection tracks view 1's. Per-batch sort
// transforms get pre-multiplied into g_frame.sortWorld so view 2 never
// needs setViewTransform updates per batch - which is critical,
// because bgfx::setViewTransform is per-view-for-the-whole-frame and
// would otherwise stomp view 1's camera view if shared.
const bgfx::ViewId kBgfxEngineSortView = 2;
const bgfx::ViewId kBgfxRTTView = 3;
const bgfx::ViewId kBgfxWaterView = 4;
// Effect overlay view for dazzle / lens flare / muzzle flash draws that
// submit vertices already in clip/NDC space. These require identity
// view and identity projection to render correctly. Routing them
// through the sort view (which has the camera perspective projection)
// re-projects their NDC coords and pushes them off-screen.
const bgfx::ViewId kBgfxEffectOverlayView = 5;
// TheSuperHackers @refactor bobtista 15/04/2026 Dedicated views for stencil shadow volumes
// + darken apply. Sequential order after view 1 guarantees depth is populated first and
// the INCR/DECR volume passes execute in submit order; no clears — attachments are shared.
const bgfx::ViewId kBgfxShadowVolumeView = 6;
const bgfx::ViewId kBgfxShadowApplyView  = 7;
// Multiplicative shroud overlay must run after all regular 3D scene/detail
// draws, otherwise later depth-equal building/detail passes can overwrite it.
const bgfx::ViewId kBgfxShroudOverlayView = 8;
// TheSuperHackers @feature bobtista 27/04/2026 Scene composite view. World,
// water, sorted translucency, and effect overlays render into an offscreen
// scene framebuffer; this view copies scene color to the swapchain before UI.
const bgfx::ViewId kBgfxSceneCompositeView = 9;
// TheSuperHackers @feature bobtista 27/04/2026 Native bgfx smudge/heat-haze
// views. The copy view snapshots scene color, then the draw view writes
// distorted samples back into the scene framebuffer before final composite.
const bgfx::ViewId kBgfxSmudgeCopyView      = 12;
const bgfx::ViewId kBgfxSmudgeView          = 13;
// TheSuperHackers @feature bobtista 16/04/2026 Dedicated view for
// 2D UI overlay draws (Render2DClass). Sequential mode preserves draw order;
// identity view+projection so screen-space quads render at their authored
// positions. Composites over the 3D scene as the last view in the order.
const bgfx::ViewId kBgfxUIView           = 10;
// TheSuperHackers @feature bobtista 27/04/2026 Readable scene-depth view.
// Opaque world draws are duplicated here into an R32F target so later post
// and particle passes can sample depth without touching the D24S8 stencil
// surface used by the main scene framebuffer.
const bgfx::ViewId kBgfxSceneDepthView   = 11;
const uint8_t kBgfxSceneDepthSamplerStage = 6;
const float kSoftParticleDepthFadeScale  = 80.0f;
const int kSwayTableEntries              = 11;
const float kPostSharpenAmount           = 0.08f;
const float kPostSaturation              = 1.015f;
const float kPostContrast                = 1.01f;
const float kPostFxaaAmount              = 0.35f;
// Frames to wait before showing the DX8 reference popup. The game's input system and shell menu need a beat after device init to fully settle; popping the ref window earlier steals focus and blocks mouse capture. ~0.5s at 60fps.
const int kDX8RefWindowShowDelayFrames   = 30;
// TheSuperHackers @bugfix bobtista 28/05/2026 File-scope so a device reset can rewind it; otherwise the function-local static remembered the "already shown" -1 sentinel through bgfx::reset cycles.
static int s_dx8RefFrameCount = 0;

// Render-to-texture state. Set by Set_Render_Target_With_Z, cleared
// when the back buffer is restored. SubmitEngineDraw routes to
// kBgfxRTTView while this is true.

// True between Begin_Sorted_Batch_Pass and End_Sorted_Batch_Pass;
// SubmitEngineDraw routes to kBgfxEngineSortView and uses
// g_frame.sortWorld while this is set.

// Per-batch effective world for sorted draws: the pre-multiplied
// sortView * sortWorld (in bgfx column-major form) captured from the
// engine's sorted replay state.
// TheSuperHackers @refactor bobtista 11/04/2026 Set by
// Submit_Sorted_Draw after it emits the bgfx submit for a sorting VB
// direct draw. The outer BgfxBackend::Draw_Triangles consumes this
// flag to skip its SubmitEngineDraw - the draw was already issued
// with correctly remapped args against the inner dynamic buffers,
// so falling through would emit a second, incorrect submit.

// Water override — set by Override_Material_Opacity, consumed by
// SubmitEngineDraw to route to the water view and apply DESTALPHA blend.

// Snapshot of g_frame.proj at the time the sort flush runs. The engine
// calls Set_Projection_Transform_With_Z_Bias multiple times per frame
// (camera, water reflections, shadows). The LAST call may use a tiny
// near-field frustum that clips all sort geometry. We capture the
// projection at sort-flush time (when it's still the camera projection)
// and re-apply it to view 2 at End_Scene time.

void IdentityMatrix(float * out)
{
    out[0]  = 1.0f; out[1]  = 0.0f; out[2]  = 0.0f; out[3]  = 0.0f;
    out[4]  = 0.0f; out[5]  = 1.0f; out[6]  = 0.0f; out[7]  = 0.0f;
    out[8]  = 0.0f; out[9]  = 0.0f; out[10] = 1.0f; out[11] = 0.0f;
    out[12] = 0.0f; out[13] = 0.0f; out[14] = 0.0f; out[15] = 1.0f;
}

static bool GetBackendWindowSize(HWND window, int &width, int &height)
{
    width = 0;
    height = 0;
#if defined(SAGE_USE_SDL3)
    if (window != nullptr)
    {
        SDL_Window *sdlWindow = static_cast<SDL_Window *>(window);
        SDL_GetWindowSize(sdlWindow, &width, &height);
        if (width <= 0 || height <= 0)
        {
            SDL_GetWindowSizeInPixels(sdlWindow, &width, &height);
        }
    }
#else
    RECT clientRect;
    if (GetClientRect(window, &clientRect))
    {
        width = clientRect.right - clientRect.left;
        height = clientRect.bottom - clientRect.top;
    }
#endif
    return width > 0 && height > 0;
}

static bool NearlyEqual(float a, float b)
{
    const float epsilon = 0.00001f;
    return a > b - epsilon && a < b + epsilon;
}

static bool IsIdentityViewMatrix(const float *m)
{
    return NearlyEqual(m[0], 1.0f) && NearlyEqual(m[5], 1.0f)
        && NearlyEqual(m[10], 1.0f) && NearlyEqual(m[15], 1.0f)
        && NearlyEqual(m[1], 0.0f) && NearlyEqual(m[2], 0.0f)
        && NearlyEqual(m[3], 0.0f) && NearlyEqual(m[4], 0.0f)
        && NearlyEqual(m[6], 0.0f) && NearlyEqual(m[7], 0.0f)
        && NearlyEqual(m[8], 0.0f) && NearlyEqual(m[9], 0.0f)
        && NearlyEqual(m[11], 0.0f);
}

static bool IsNonPerspectiveProjection(const float *m)
{
    // W3DMatrix4ToBgfx transpose-copies into bgfx's column-major layout.
    // Perspective camera projections carry m[3][3] == 0, which lands in
    // slot 15. Screen/orthographic projections keep slot 15 at 1.
    return NearlyEqual(m[15], 1.0f);
}

// W3D Matrix4x4 stores Vector4 Row[4] in row-major order. bgfx wants
// column-major float[16]. Transpose-copy.
void W3DMatrix4ToBgfx(const Matrix4x4 & m, float * out)
{
    out[0]  = m[0][0]; out[4]  = m[0][1]; out[8]  = m[0][2]; out[12] = m[0][3];
    out[1]  = m[1][0]; out[5]  = m[1][1]; out[9]  = m[1][2]; out[13] = m[1][3];
    out[2]  = m[2][0]; out[6]  = m[2][1]; out[10] = m[2][2]; out[14] = m[2][3];
    out[3]  = m[3][0]; out[7]  = m[3][1]; out[11] = m[3][2]; out[15] = m[3][3];
}

// W3D Matrix3D stores Vector4 Row[3] - the bottom row is implicitly
// (0,0,0,1). Same transpose convention as Matrix4x4 but the missing
// row needs to be filled in.
void W3DMatrix3DToBgfx(const Matrix3D & m, float * out)
{
    out[0]  = m[0][0]; out[4]  = m[0][1]; out[8]  = m[0][2]; out[12] = m[0][3];
    out[1]  = m[1][0]; out[5]  = m[1][1]; out[9]  = m[1][2]; out[13] = m[1][3];
    out[2]  = m[2][0]; out[6]  = m[2][1]; out[10] = m[2][2]; out[14] = m[2][3];
    out[3]  = 0.0f;    out[7]  = 0.0f;    out[11] = 0.0f;    out[15] = 1.0f;
}

auto MakeLegacyCacheMatrix(const Matrix4x4 & m)
{
    return To_D3DMATRIX(m);
}

auto MakeLegacyCacheMatrix(const Matrix3D & m)
{
    return To_D3DMATRIX(m);
}

auto MakeIdentityLegacyCacheMatrix()
{
    Matrix4x4 identity(true);
    return MakeLegacyCacheMatrix(identity);
}

void CacheTransform(TransformKind transform, const Matrix4x4 & m)
{
    FixedFunctionState::Set_Transform_Matrix(static_cast<unsigned>(transform), MakeLegacyCacheMatrix(m));
}

void CacheTransform(TransformKind transform, const Matrix3D & m)
{
    FixedFunctionState::Set_Transform_Matrix(static_cast<unsigned>(transform), MakeLegacyCacheMatrix(m));
}

void CacheIdentityTransform(TransformKind transform)
{
    Matrix4x4 identity(true);
    CacheTransform(transform, identity);
}

bool IsCachedTransformIdentity(TransformKind transform)
{
    auto matrix = MakeIdentityLegacyCacheMatrix();
    FixedFunctionState::Transform_Matrix(static_cast<unsigned>(transform), matrix);
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            const float expected = (row == col) ? 1.0f : 0.0f;
            if (matrix.m[row][col] != expected)
            {
                return false;
            }
        }
    }
    return true;
}

static void GetPostParams(float * params)
{
    params[0] = kPostSharpenAmount;
    params[1] = kPostSaturation;
    params[2] = kPostContrast;
    params[3] = kPostFxaaAmount;
#ifdef RTS_ZEROHOUR
    GGC_GetBgfxPostProcessParams(params);
    params[0] = WWMath::Clamp(params[0], 0.0f, 1.0f);
    params[1] = WWMath::Clamp(params[1], 0.0f, 2.0f);
    params[2] = WWMath::Clamp(params[2], 0.0f, 2.0f);
    params[3] = WWMath::Clamp(params[3], 0.0f, 1.0f);
#endif
    if (GetBgfxDiagnosticFlags().noPostFx)
    {
        params[0] = 0.0f;
        params[1] = 1.0f;
        params[2] = 1.0f;
        params[3] = 0.0f;
    }
}

static void GetSoftParticleParams(float * params)
{
    params[0] = 1.0f;
    params[1] = kSoftParticleDepthFadeScale;
    params[2] = 0.0f;
    params[3] = 0.0f;
#ifdef RTS_ZEROHOUR
    GGC_GetBgfxSoftParticleParams(params);
    params[0] = params[0] > 0.5f ? 1.0f : 0.0f;
    params[1] = WWMath::Clamp(params[1], 0.0f, 500.0f);
#endif
}

static bool IsReadableSceneDepthEnabled()
{
    if (GetBgfxDiagnosticFlags().noSceneFramebuffer)
    {
        return false;
    }

    float softParticleParams[4];
    GetSoftParticleParams(softParticleParams);
    return softParticleParams[0] > 0.5f;
}

// TheSuperHackers @refactor bobtista 16/04/2026 Aspect correction
// is no longer needed because bgfx renders into the same window as the game.
// The engine's projection matrix already matches the bgfx framebuffer aspect.

// TheSuperHackers @refactor bobtista 11/04/2026 Texture
// capture. Unlike vertex buffers, W3D textures default to POOL_MANAGED,
// which is safe to lock read-only on the Intel UHD driver.
// We can read the source d3d8 texture data on demand from inside
// Set_Texture without an engine-side write hook. POOL_DEFAULT textures
// (render targets, dynamic textures) are skipped to avoid the same
// corruption that hit vertex buffers.

} // end anonymous namespace (helpers moved to BgfxBackendTextures.cpp)


namespace { // reopen anonymous namespace

static void DestroySceneFramebuffer()
{
    if (bgfx::isValid(g_device.sceneFB))
    {
        bgfx::destroy(g_device.sceneFB);
    }
    if (bgfx::isValid(g_device.sceneReadableDepthFB))
    {
        bgfx::destroy(g_device.sceneReadableDepthFB);
    }
    if (bgfx::isValid(g_device.sceneSmudgeCopy))
    {
        bgfx::destroy(g_device.sceneSmudgeCopy);
    }
    g_device.sceneFB = BGFX_INVALID_HANDLE;
    g_device.sceneColor = BGFX_INVALID_HANDLE;
    g_device.sceneDepth = BGFX_INVALID_HANDLE;
    g_device.sceneSmudgeCopy = BGFX_INVALID_HANDLE;
    g_device.sceneReadableDepthFB = BGFX_INVALID_HANDLE;
    g_device.sceneReadableDepth = BGFX_INVALID_HANDLE;
    g_device.sceneReadableDepthTest = BGFX_INVALID_HANDLE;
    g_device.sceneWidth = 0;
    g_device.sceneHeight = 0;
}

static bool CreateSceneFramebuffer()
{
    DestroySceneFramebuffer();

    const uint16_t w = static_cast<uint16_t>(g_device.width > 0 ? g_device.width : 1);
    const uint16_t h = static_cast<uint16_t>(g_device.height > 0 ? g_device.height : 1);
    const uint64_t colorFlags = BGFX_TEXTURE_RT
        | BGFX_SAMPLER_POINT
        | BGFX_SAMPLER_U_CLAMP
        | BGFX_SAMPLER_V_CLAMP;
    bgfx::TextureHandle colorTex = bgfx::createTexture2D(
        w, h, false, 1, bgfx::TextureFormat::RGBA8, colorFlags);
    bgfx::TextureHandle depthTex = bgfx::createTexture2D(
        w, h, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);

    bgfx::TextureHandle attachments[2] = { colorTex, depthTex };
    bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(2, attachments, true);
    if (!bgfx::isValid(fb))
    {
        if (bgfx::isValid(colorTex))
        {
            bgfx::destroy(colorTex);
        }
        if (bgfx::isValid(depthTex))
        {
            bgfx::destroy(depthTex);
        }
        WWDEBUG_SAY(("[BgfxBackend] Scene framebuffer creation FAILED (%dx%d).",
                     w, h));
        return false;
    }

    g_device.sceneFB = fb;
    g_device.sceneColor = colorTex;
    g_device.sceneDepth = depthTex;
    g_device.sceneSmudgeCopy = bgfx::createTexture2D(
        w, h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST
        | BGFX_SAMPLER_U_CLAMP
        | BGFX_SAMPLER_V_CLAMP);
    g_device.sceneWidth = w;
    g_device.sceneHeight = h;
    bgfx::setName(g_device.sceneColor, "sceneColorRGBA8");
    bgfx::setName(g_device.sceneDepth, "sceneDepthD24S8");
    if (bgfx::isValid(g_device.sceneSmudgeCopy))
    {
        bgfx::setName(g_device.sceneSmudgeCopy, "sceneSmudgeCopyRGBA8");
    }

    if (IsReadableSceneDepthEnabled())
    {
        // TheSuperHackers @feature bobtista 27/04/2026 Keep the main scene
        // D24S8 attachment write-only for stencil shadows, and build a separate
        // sampleable R32F depth texture for post effects and soft particles.
        // TheSuperHackers @performance bobtista 29/04/2026 Only allocate and
        // submit this readable depth path while an effect actively samples it.
        const uint64_t readableDepthFlags = BGFX_TEXTURE_RT
            | BGFX_SAMPLER_POINT
            | BGFX_SAMPLER_U_CLAMP
            | BGFX_SAMPLER_V_CLAMP;
        bgfx::TextureHandle readableDepthTex = bgfx::createTexture2D(
            w, h, false, 1, bgfx::TextureFormat::R32F, readableDepthFlags);
        bgfx::TextureHandle readableDepthTest = bgfx::createTexture2D(
            w, h, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
        bgfx::FrameBufferHandle depthFB = BGFX_INVALID_HANDLE;
        if (bgfx::isValid(readableDepthTex) && bgfx::isValid(readableDepthTest))
        {
            bgfx::TextureHandle depthAttachments[2] = { readableDepthTex, readableDepthTest };
            depthFB = bgfx::createFrameBuffer(2, depthAttachments, true);
        }
        if (bgfx::isValid(depthFB))
        {
            g_device.sceneReadableDepthFB = depthFB;
            g_device.sceneReadableDepth = readableDepthTex;
            g_device.sceneReadableDepthTest = readableDepthTest;
            bgfx::setName(g_device.sceneReadableDepth, "sceneReadableDepthR32F");
            bgfx::setName(g_device.sceneReadableDepthTest, "sceneReadableDepthD24S8");
        }
        else
        {
            if (bgfx::isValid(readableDepthTex))
            {
                bgfx::destroy(readableDepthTex);
            }
            if (bgfx::isValid(readableDepthTest))
            {
                bgfx::destroy(readableDepthTest);
            }
            WWDEBUG_SAY(("[BgfxBackend] Readable scene depth creation FAILED (%dx%d).",
                         w, h));
        }
    }

    WWDEBUG_SAY(("[BgfxBackend] Scene framebuffer created %dx%d.", w, h));
    return true;
}

static void ApplySceneFramebufferToViews()
{
    const BgfxDiagnosticFlags diagnostics = GetBgfxDiagnosticFlags();
    const bool useSceneFramebuffer =
        !diagnostics.noSceneFramebuffer
        && bgfx::isValid(g_device.sceneFB)
        && bgfx::isValid(g_device.sceneColor)
        && bgfx::isValid(g_device.sceneCompositeProgram);
    bgfx::FrameBufferHandle sceneFB = BGFX_INVALID_HANDLE;
    if (useSceneFramebuffer)
    {
        sceneFB = g_device.sceneFB;
    }
    if (!useSceneFramebuffer)
    {
        bgfx::setViewFrameBuffer(kBgfxSceneCompositeView, BGFX_INVALID_HANDLE);
    }
    bgfx::setViewFrameBuffer(kBgfxEngineView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxEngineSortView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxWaterView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxEffectOverlayView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxShadowVolumeView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxShadowApplyView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxShroudOverlayView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxSmudgeCopyView, BGFX_INVALID_HANDLE);
    bgfx::setViewFrameBuffer(kBgfxSmudgeView, sceneFB);
    bgfx::setViewFrameBuffer(kBgfxSceneCompositeView, BGFX_INVALID_HANDLE);
    bgfx::setViewFrameBuffer(kBgfxSceneDepthView, g_device.sceneReadableDepthFB);
    bgfx::setViewFrameBuffer(kBgfxUIView, BGFX_INVALID_HANDLE);
}

static void SubmitSceneComposite()
{
    if (GetBgfxDiagnosticFlags().noSceneFramebuffer
        || !bgfx::isValid(g_device.sceneFB)
        || !bgfx::isValid(g_device.sceneColor)
        || !bgfx::isValid(g_device.sceneCompositeProgram)
        || !bgfx::isValid(g_device.fullscreenClearVB)
        || !bgfx::isValid(g_uniforms.sTex0))
    {
        bgfx::touch(kBgfxSceneCompositeView);
        return;
    }

    float identity[16];
    IdentityMatrix(identity);
    bgfx::setViewTransform(kBgfxSceneCompositeView, identity, identity);
    bgfx::setViewRect(kBgfxSceneCompositeView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setTexture(0, g_uniforms.sTex0, g_device.sceneColor,
                     BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    g_stats.textureBinds++;
    if (bgfx::isValid(g_uniforms.sSceneDepth) && bgfx::isValid(g_device.sceneReadableDepth))
    {
        bgfx::setTexture(1, g_uniforms.sSceneDepth, g_device.sceneReadableDepth,
                         BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        g_stats.textureBinds++;
    }
    // TheSuperHackers @feature bobtista 27/04/2026 Post controls come from
    // Zero Hour GameData when available; the local defaults are deliberately
    // subtle so the scene keeps the original Zero Hour art direction.
    float postParams[4];
    GetPostParams(postParams);
    const float postTexelSize[4] = {
        1.0f / static_cast<float>(g_device.width),
        1.0f / static_cast<float>(g_device.height),
        0.0f,
        0.0f
    };
    if (bgfx::isValid(g_uniforms.uPostParams))
    {
        bgfx::setUniform(g_uniforms.uPostParams, postParams);
    }
    if (bgfx::isValid(g_uniforms.uPostTexelSize))
    {
        bgfx::setUniform(g_uniforms.uPostTexelSize, postTexelSize);
    }
    bgfx::setVertexBuffer(0, g_device.fullscreenClearVB);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(kBgfxSceneCompositeView, g_device.sceneCompositeProgram);
    g_stats.sceneCompositeSubmits++;
}

}

void BgfxBackend::Initialize(void * hwnd, int /*width*/, int /*height*/)
{
#if defined(__APPLE__)
    if (std::getenv("GGC_TRACE") != nullptr)
    {
        std::fprintf(stderr, "[ggc] BgfxBackend::Initialize hwnd=%p\n", hwnd);
        std::fflush(stderr);
    }
#endif
    // TheSuperHackers @bugfix bobtista 28/05/2026 Rewind the DX8-ref show-and-focus counter so device-reset scenarios re-run the focus reassert.
    s_dx8RefFrameCount = 0;
    if (g_device.initialized)
    {
        WWDEBUG_SAY(("[BgfxBackend] Initialize called twice; ignoring."));
        return;
    }

    // TheSuperHackers @feature bobtista 16/04/2026 bgfx takes the
    // main game window; DX8 moves to a secondary popup for reference.
    g_device.window = static_cast<HWND>(hwnd);
    if (g_device.window == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] hwnd is null. Backend will remain dormant."));
        return;
    }

    GetBackendWindowSize(g_device.window, g_device.width, g_device.height);
    if (g_device.width <= 0)
    {
        g_device.width = 800;
    }
    if (g_device.height <= 0)
    {
        g_device.height = 600;
    }
    WWDEBUG_SAY(("[BgfxBackend] Using main game window %p (%dx%d) for bgfx.",
                 g_device.window, g_device.width, g_device.height));

    bgfx::renderFrame();

    bgfx::PlatformData pd;
    pd.ndt = nullptr;
    pd.nwh = GetNativeWindowHandle(g_device.window);
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    bgfx::setPlatformData(pd);

    bgfx::Init initArgs;
    initArgs.type = GetConfiguredRendererType();
    initArgs.callback = &g_bgfxCallback;
    initArgs.resolution.width = static_cast<uint32_t>(g_device.width);
    initArgs.resolution.height = static_cast<uint32_t>(g_device.height);
    initArgs.resolution.reset = BGFX_RESET_NONE;
    {
        int msaaLevel = 0;
        const char * msaaEnv = std::getenv("GGC_BGFX_MSAA");
        if (msaaEnv != nullptr)
        {
            msaaLevel = std::atoi(msaaEnv);
            if (msaaLevel <= 0) { msaaLevel = 4; }
        }
        if (msaaLevel >= 16) { initArgs.resolution.reset |= BGFX_RESET_MSAA_X16; }
        else if (msaaLevel >= 8) { initArgs.resolution.reset |= BGFX_RESET_MSAA_X8; }
        else if (msaaLevel >= 4) { initArgs.resolution.reset |= BGFX_RESET_MSAA_X4; }
        else if (msaaLevel >= 2) { initArgs.resolution.reset |= BGFX_RESET_MSAA_X2; }
        g_device.msaaResetFlags = initArgs.resolution.reset & (BGFX_RESET_MSAA_X2 | BGFX_RESET_MSAA_X4 | BGFX_RESET_MSAA_X8 | BGFX_RESET_MSAA_X16);
    }
    g_device.srgbEnabled = std::getenv("GGC_BGFX_SRGB") != nullptr;
    if (g_device.srgbEnabled)
    {
        initArgs.resolution.reset |= BGFX_RESET_SRGB_BACKBUFFER;
    }
    if (std::getenv("GGC_BGFX_NO_DEPTH_CLAMP") == nullptr)
    {
        initArgs.resolution.reset |= BGFX_RESET_DEPTH_CLAMP;
    }
#if defined(__APPLE__)
    // TheSuperHackers @bugfix bobtista 30/04/2026 BGFX_RESET_FLUSH_AFTER_RENDER
    // serializes Metal command buffer submission instead of pipelining a
    // frame ahead. AGX on M4 / macOS Tahoe loses internal helper-shader
    // compiles when many command encoders are in flight; flushing after
    // each render call lets the background compile queue drain. The flag
    // costs ~1 frame of latency but is harmless. GGC_MACOS_NO_FLUSH=1
    // disables it for A/B testing.
    if (std::getenv("GGC_MACOS_NO_FLUSH") == nullptr)
    {
        initArgs.resolution.reset |= BGFX_RESET_FLUSH_AFTER_RENDER;
    }
#endif
    initArgs.platformData = pd;
    // TheSuperHackers @bugfix bobtista 27/04/2026 Keep bgfx on a normal
    // native device even in game Debug builds. The backend debug layer raises
    // DXGI-facility exceptions inside bgfx::frame before the engine can
    // reach shellmap or command-line save loads.
    // TheSuperHackers @build bobtista 30/04/2026 Allow GGC_BGFX_DEBUG=1 to
    // turn on bgfx's verbose diagnostics for macOS bring-up.
    initArgs.debug = std::getenv("GGC_BGFX_DEBUG") != nullptr;

#if defined(__APPLE__)
    if (std::getenv("GGC_TRACE") != nullptr)
    {
        std::fprintf(stderr, "[ggc] calling bgfx::init nwh=%p\n", pd.nwh);
        std::fflush(stderr);
    }
#endif

    if (!bgfx::init(initArgs))
    {
        WWDEBUG_SAY(("[BgfxBackend] bgfx::init FAILED. Backend will remain dormant."));
        g_device.window = nullptr;
        return;
    }

    g_device.initialized = true;

#if defined(__APPLE__)
    if (std::getenv("GGC_TRACE") != nullptr)
    {
        std::fprintf(stderr, "[ggc] bgfx::init OK\n");
        std::fflush(stderr);
    }
#endif

    // TheSuperHackers @refactor bobtista 16/04/2026 The explicit
    // bgfx::reset() after init is removed because it triggers a DXGI
    // assertion when bgfx owns the main game HWND. The init call already
    // configured the resolution and format correctly.
    // TheSuperHackers @feature bobtista 27/04/2026 Create the full-canvas
    // scene framebuffer. 3D views render here first, then view 9 composites
    // the scene to the swapchain before UI draws.
    if (!GetBgfxDiagnosticFlags().noSceneFramebuffer)
    {
        CreateSceneFramebuffer();
    }
    else
    {
        WWDEBUG_SAY(("[BgfxBackend] Diagnostic bgfxNoSceneFramebuffer enabled."));
    }

    // Configure view 0 to clear the debug window to a dark teal so it's
    // visually obvious bgfx is running and alive. View 0 holds the test
    // triangle (sentinel).
    bgfx::setViewClear(kBgfxDebugView,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x000000ff,  // black
                       1.0f,
                       0);
    bgfx::setViewFrameBuffer(kBgfxDebugView, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(kBgfxDebugView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));

    // View 1 is the engine geometry view. Same render target, but its
    // own clear/depth and (eventually) its own view+projection matrices
    // captured from the engine's Set_Transform calls. Drawn after view 0
    // so engine geometry overlays the test triangle.
    // Clear depth AND color. The color clear initializes the framebuffer
    // alpha to ~0.7 (m_minWaterOpacity) for the DESTALPHA water technique.
    // Without this, deep water areas without shoreline tiles have alpha=0
    // (transparent) and the water polygon edge creates a visible zigzag.
    // The RGB clear is black; terrain overwrites it with its own color.
    bgfx::setViewClear(kBgfxEngineView,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                       0x000000ff,  // Alpha=1.0 matches TransparentWaterMinOpacity=1.0 from INI
                       1.0f,
                       0);
    // Sequential mode preserves the engine's draw order: terrain
    // first, then shadow decals on top at equal depth. Without this,
    // Default sort can place decals before terrain → overwritten.
    bgfx::setViewMode(kBgfxEngineView, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(kBgfxEngineView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));

    // TheSuperHackers @refactor bobtista 11/04/2026 Sorted
    // draws view. No clear (reuses view 1's color + depth so sorted
    // particles z-test correctly against opaque geometry), same rect.
    // View matrix is permanently identity; projection tracks view 1's
    // via Set_Projection_Transform_With_Z_Bias.
    bgfx::setViewClear(kBgfxEngineSortView,
                       BGFX_CLEAR_NONE,
                       0x00000000,
                       1.0f,
                       0);
    bgfx::setViewMode(kBgfxEngineSortView, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(kBgfxEngineSortView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));

    // Effect overlay view for dazzle draws with NDC-space vertices.
    // Permanent identity view + identity projection; reuses the
    // backbuffer + depth from earlier views. No clear.
    bgfx::setViewClear(kBgfxEffectOverlayView,
                       BGFX_CLEAR_NONE,
                       0x00000000,
                       1.0f,
                       0);
    bgfx::setViewRect(kBgfxEffectOverlayView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxEffectOverlayView, identityMtx, identityMtx);
    }

    // Shadow-volume view. Sequential so the two-pass algorithm
    // (front INCR / back DECR) runs in submit order. Clear stencil here,
    // on the same view that writes/tests it; clearing only the earlier
    // engine view does not establish the Metal stencil attachment for this
    // pass reliably. View transform is pushed per-frame from the engine
    // camera via the dirty-flag logic alongside view 1.
    bgfx::setViewClear(kBgfxShadowVolumeView, BGFX_CLEAR_STENCIL, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxShadowVolumeView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxShadowVolumeView, bgfx::ViewMode::Sequential);

    // Shadow darken apply pass. Sequential, identity transforms
    // (the fullscreen quad is authored in clip space).
    bgfx::setViewClear(kBgfxShadowApplyView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxShadowApplyView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxShadowApplyView, bgfx::ViewMode::Sequential);
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxShadowApplyView, identityMtx, identityMtx);
    }

    bgfx::setViewClear(kBgfxShroudOverlayView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxShroudOverlayView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxShroudOverlayView, bgfx::ViewMode::Sequential);

    bgfx::setViewClear(kBgfxSceneDepthView,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0xffffffffu,
                       1.0f,
                       0);
    bgfx::setViewRect(kBgfxSceneDepthView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxSceneDepthView, bgfx::ViewMode::Default);

    bgfx::setViewClear(kBgfxSceneCompositeView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxSceneCompositeView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxSceneCompositeView, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kBgfxSmudgeCopyView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxSmudgeCopyView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxSmudgeCopyView, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kBgfxSmudgeView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxSmudgeView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxSmudgeView, bgfx::ViewMode::Sequential);
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxSceneCompositeView, identityMtx, identityMtx);
        bgfx::setViewTransform(kBgfxSmudgeCopyView, identityMtx, identityMtx);
        bgfx::setViewTransform(kBgfxSmudgeView, identityMtx, identityMtx);
    }

    // UI overlay view. Sequential mode preserves draw order for
    // 2D quads; identity view+projection; no clear so it composites over
    // the 3D scene.
    bgfx::setViewClear(kBgfxUIView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxUIView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxUIView, bgfx::ViewMode::Sequential);
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxUIView, identityMtx, identityMtx);
    }
    ApplySceneFramebufferToViews();

    // Default the cached transforms to identity until the engine writes
    // real values via Set_Transform. This keeps the first few engine
    // submits well-defined even if they fire before any matrices are
    // captured.
    IdentityMatrix(g_frame.world);
    IdentityMatrix(g_frame.view);
    IdentityMatrix(g_frame.proj);
    IdentityMatrix(g_frame.sortWorld);
    CacheIdentityTransform(RB_TRANSFORM_WORLD);
    CacheIdentityTransform(RB_TRANSFORM_VIEW);
    CacheIdentityTransform(RB_TRANSFORM_PROJECTION);
    g_frame.cameraProjDirty = true;

    // Sort view gets identity view + current projection. setViewTransform
    // persists for the life of the bgfx view; we re-apply the projection
    // in Set_Projection_Transform_With_Z_Bias whenever it changes.
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_frame.proj);
    }

    // TheSuperHackers @refactor bobtista 11/04/2026 Create the
    // passthrough shader program and vertex layout so End_Scene can submit
    // a test triangle. If shader creation fails the backend still runs but
    // the triangle is skipped.
    g_device.triangleLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    BuildStandardVertexLayouts();

    g_device.passthroughProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_passthrough), sizeof(GGC_BGFX_SHADER(vs_passthrough)), "vs_passthrough",
        GGC_BGFX_SHADER(fs_passthrough), sizeof(GGC_BGFX_SHADER(fs_passthrough)), "fs_passthrough");

    g_device.sceneCompositeProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_scene_composite), sizeof(GGC_BGFX_SHADER(vs_scene_composite)), "vs_scene_composite",
        GGC_BGFX_SHADER(fs_scene_composite), sizeof(GGC_BGFX_SHADER(fs_scene_composite)), "fs_scene_composite");
    g_device.sceneDepthProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_scene_depth), sizeof(GGC_BGFX_SHADER(vs_scene_depth)), "vs_scene_depth",
        GGC_BGFX_SHADER(fs_scene_depth), sizeof(GGC_BGFX_SHADER(fs_scene_depth)), "fs_scene_depth");
    g_device.smudgeProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_smudge), sizeof(GGC_BGFX_SHADER(vs_smudge)), "vs_smudge",
        GGC_BGFX_SHADER(fs_smudge), sizeof(GGC_BGFX_SHADER(fs_smudge)), "fs_smudge");
    ApplySceneFramebufferToViews();

    // Fullscreen-clear VB. Single triangle in NDC that covers the entire
    // clip-space rectangle; submitted to view 0 every frame (Begin_Scene).
    {
        struct ClearVert { float x, y, z; uint32_t rgba; };
        static const ClearVert verts[3] = {
            { -1.0f, -3.0f, 0.0f, 0xff000000u },
            { -1.0f,  1.0f, 0.0f, 0xff000000u },
            {  3.0f,  1.0f, 0.0f, 0xff000000u },
        };
        g_device.fullscreenClearVB = bgfx::createVertexBuffer(
            bgfx::makeRef(verts, sizeof(verts)), g_device.triangleLayout);
    }

    g_uniforms.sTex0        = bgfx::createUniform("s_tex0",        bgfx::UniformType::Sampler);
    g_uniforms.sTex1        = bgfx::createUniform("s_tex1",        bgfx::UniformType::Sampler);
    g_uniforms.sTex2        = bgfx::createUniform("s_tex2",        bgfx::UniformType::Sampler);
    g_uniforms.sTex3        = bgfx::createUniform("s_tex3",        bgfx::UniformType::Sampler);
    g_uniforms.sSceneDepth  = bgfx::createUniform("s_sceneDepth",  bgfx::UniformType::Sampler);
    g_uniforms.uMatDiffuse  = bgfx::createUniform("u_matDiffuse",  bgfx::UniformType::Vec4);
    g_uniforms.uMatAmbient  = bgfx::createUniform("u_matAmbient",  bgfx::UniformType::Vec4);
    g_uniforms.uMatEmissive = bgfx::createUniform("u_matEmissive", bgfx::UniformType::Vec4);
    g_uniforms.uAtestParams = bgfx::createUniform("u_atestParams", bgfx::UniformType::Vec4);
    g_uniforms.uTssOps0     = bgfx::createUniform("u_tssOps0",     bgfx::UniformType::Vec4);
    g_uniforms.uTssOps1     = bgfx::createUniform("u_tssOps1",     bgfx::UniformType::Vec4);
    g_uniforms.uLightDirs   = bgfx::createUniform("u_lightDirs",   bgfx::UniformType::Vec4, 4);
    g_uniforms.uLightColors = bgfx::createUniform("u_lightColors", bgfx::UniformType::Vec4, 4);
    g_uniforms.uLightAmbients = bgfx::createUniform("u_lightAmbients", bgfx::UniformType::Vec4, 4);
    g_uniforms.uLightPositions = bgfx::createUniform("u_lightPositions", bgfx::UniformType::Vec4, 4);
    g_uniforms.uLightParams = bgfx::createUniform("u_lightParams", bgfx::UniformType::Vec4, 4);
    g_uniforms.uSceneAmbient  = bgfx::createUniform("u_sceneAmbient",   bgfx::UniformType::Vec4);
    g_uniforms.uLightingEnabled = bgfx::createUniform("u_lightingEnabled", bgfx::UniformType::Vec4);
    g_uniforms.uTexcoordSelect  = bgfx::createUniform("u_texcoordSelect",  bgfx::UniformType::Vec4);
    g_uniforms.uTexcoordSelect2 = bgfx::createUniform("u_texcoordSelect2", bgfx::UniformType::Vec4);
    g_uniforms.uProjectedDecalMode = bgfx::createUniform("u_projectedDecalMode", bgfx::UniformType::Vec4);
    g_uniforms.uTexcoordSource  = bgfx::createUniform("u_texcoordSource",  bgfx::UniformType::Vec4);
    g_uniforms.uVertexColorFlags = bgfx::createUniform("u_vertexColorFlags", bgfx::UniformType::Vec4);
    g_uniforms.uGrayscaleEnable = bgfx::createUniform("u_grayscaleEnable", bgfx::UniformType::Vec4);
    g_uniforms.uObjectShroudDim = bgfx::createUniform("u_objectShroudDim", bgfx::UniformType::Vec4);
    g_uniforms.uShroudParams = bgfx::createUniform("u_shroudParams", bgfx::UniformType::Vec4);
    g_uniforms.uCloudParams  = bgfx::createUniform("u_cloudParams",  bgfx::UniformType::Vec4);
    g_uniforms.uTexTransform0 = bgfx::createUniform("u_texTransform0", bgfx::UniformType::Vec4);
    g_uniforms.uTexTransform1 = bgfx::createUniform("u_texTransform1", bgfx::UniformType::Vec4);
    g_uniforms.uTexTransform0Z = bgfx::createUniform("u_texTransform0Z", bgfx::UniformType::Vec4);
    g_uniforms.uTex1Transform0 = bgfx::createUniform("u_tex1Transform0", bgfx::UniformType::Vec4);
    g_uniforms.uTex1Transform1 = bgfx::createUniform("u_tex1Transform1", bgfx::UniformType::Vec4);
    g_uniforms.uTex1TransformZ = bgfx::createUniform("u_tex1TransformZ", bgfx::UniformType::Vec4);
    g_uniforms.uTex2Transform0 = bgfx::createUniform("u_tex2Transform0", bgfx::UniformType::Vec4);
    g_uniforms.uTex2Transform1 = bgfx::createUniform("u_tex2Transform1", bgfx::UniformType::Vec4);
    g_uniforms.uTexProjected = bgfx::createUniform("u_texProjected", bgfx::UniformType::Vec4);
    g_uniforms.uLegacyPixelShaderMode = bgfx::createUniform("u_legacyPixelShaderMode", bgfx::UniformType::Vec4);
    g_uniforms.uZBias = bgfx::createUniform("u_zBias", bgfx::UniformType::Vec4);
    g_uniforms.sCloudMap     = bgfx::createUniform("s_cloudMap",     bgfx::UniformType::Sampler);

    // Default 1x1 white texture. Used as fallback for missing textures.
    // Multiplying by white is the identity operation.
    static const uint8_t kWhitePixel[4] = { 0xff, 0xff, 0xff, 0xff };
    g_device.defaultWhiteTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWhitePixel, sizeof(kWhitePixel)));
    // Transparent fallback for missing blended textures. The legacy missing
    // texture is useful on opaque geometry, but particle/effect draws can
    // amplify it into large black/magenta quads.
    static const uint8_t kTransparentPixel[4] = { 0x00, 0x00, 0x00, 0x00 };
    g_device.defaultTransparentTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kTransparentPixel, sizeof(kTransparentPixel)));

    g_device.uberProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_uber), sizeof(GGC_BGFX_SHADER(vs_uber)), "vs_uber",
        GGC_BGFX_SHADER(fs_uber), sizeof(GGC_BGFX_SHADER(fs_uber)), "fs_uber");

    g_device.uberInstancedProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_uber_instanced), sizeof(GGC_BGFX_SHADER(vs_uber_instanced)), "vs_uber_instanced",
        GGC_BGFX_SHADER(fs_uber), sizeof(GGC_BGFX_SHADER(fs_uber)), "fs_uber");

    g_device.treeProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_trees), sizeof(GGC_BGFX_SHADER(vs_trees)), "vs_trees",
        GGC_BGFX_SHADER(fs_uber), sizeof(GGC_BGFX_SHADER(fs_uber)), "fs_uber");

    g_device.shadowVolumeProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_shadow_volume), sizeof(GGC_BGFX_SHADER(vs_shadow_volume)), "vs_shadow_volume",
        GGC_BGFX_SHADER(fs_shadow_volume), sizeof(GGC_BGFX_SHADER(fs_shadow_volume)), "fs_shadow_volume");

    g_device.shadowApplyProgram = CreateShaderProgram(
        GGC_BGFX_SHADER(vs_shadow_apply), sizeof(GGC_BGFX_SHADER(vs_shadow_apply)), "vs_shadow_apply",
        GGC_BGFX_SHADER(fs_shadow_apply), sizeof(GGC_BGFX_SHADER(fs_shadow_apply)), "fs_shadow_apply");
    g_uniforms.uShadowColor = bgfx::createUniform("u_shadowColor", bgfx::UniformType::Vec4);
    g_uniforms.uShadowBias  = bgfx::createUniform("u_shadowBias",  bgfx::UniformType::Vec4);
    g_uniforms.uPostParams = bgfx::createUniform("u_postParams", bgfx::UniformType::Vec4);
    g_uniforms.uPostTexelSize = bgfx::createUniform("u_postTexelSize", bgfx::UniformType::Vec4);
    g_uniforms.uSoftParticleParams = bgfx::createUniform("u_softParticleParams", bgfx::UniformType::Vec4);

    // Keep view order explicit. Stencil shadow volumes, sorted decals/effects,
    // scene-depth copies, post effects, and UI all depend on stable ordering.
    const bgfx::ViewId order[] = {
        kBgfxDebugView,
        kBgfxRTTView,
        kBgfxEngineView,
        kBgfxSceneDepthView,
        kBgfxShadowVolumeView,
        kBgfxShadowApplyView,
        kBgfxWaterView,
        kBgfxEngineSortView,
        kBgfxEffectOverlayView,
        kBgfxShroudOverlayView,
        kBgfxSmudgeCopyView,
        kBgfxSmudgeView,
        kBgfxSceneCompositeView,
        kBgfxUIView,
    };
    bgfx::setViewOrder(kBgfxDebugView, BX_COUNTOF(order), order);

    g_uniforms.uSwayTable    = bgfx::createUniform("u_swayTable",    bgfx::UniformType::Vec4, kSwayTableEntries);
    g_uniforms.uShroudOffset = bgfx::createUniform("u_shroudOffset", bgfx::UniformType::Vec4);
    g_uniforms.uShroudScale  = bgfx::createUniform("u_shroudScale",  bgfx::UniformType::Vec4);

    const bgfx::RendererType::Enum selected = bgfx::getRendererType();
    const char * rendererName = bgfx::getRendererName(selected);
    const bgfx::Caps * caps = bgfx::getCaps();
    WWDEBUG_SAY(("[BgfxBackend] bgfx::init OK on main window "
                 "(renderer=%s, %dx%d, hwnd=%p, passthrough=%s, uber=%s).",
                 rendererName, g_device.width, g_device.height,
                 g_device.window,
                 bgfx::isValid(g_device.passthroughProgram) ? "ok" : "FAILED",
                 bgfx::isValid(g_device.uberProgram)        ? "ok" : "FAILED"));
    // Log whether RGBA8 is supported as a render target (needed for
    // DESTALPHA water technique — back buffer must have alpha channel).
    const bool rgba8Supported = (caps->formats[bgfx::TextureFormat::RGBA8] &
                                  BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0;
    const bool bgra8Supported = (caps->formats[bgfx::TextureFormat::BGRA8] &
                                  BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0;
    WWDEBUG_SAY(("[BgfxBackend] Caps: RGBA8_FB=%d BGRA8_FB=%d "
                 "homogeneousDepth=%d originBottomLeft=%d",
                 rgba8Supported ? 1 : 0, bgra8Supported ? 1 : 0,
                 caps->homogeneousDepth ? 1 : 0,
                 caps->originBottomLeft ? 1 : 0));

    // TheSuperHackers @refactor bobtista 16/04/2026 DX8 now creates
    // its own secondary reference window in DX8Wrapper::Init, so no need to
    // create or move anything here.

    // TheSuperHackers @bugfix bobtista 30/04/2026 The pre-warm loop was
    // intended to serialize AGX helper-shader compilation during init,
    // but in practice it touches 14 views in a single frame and the
    // resulting fan-out of parallel EndOfTile / BlitFastClear compiles
    // is exactly the race we were trying to avoid. Disabled by default;
    // GGC_MACOS_PREWARM=1 turns it back on for experimentation.
#if defined(__APPLE__)
    if (std::getenv("GGC_MACOS_PREWARM") != nullptr)
    {
        const bool trace = std::getenv("GGC_TRACE") != nullptr;
        if (trace)
        {
            std::fprintf(stderr, "[ggc] pre-warm loop start\n");
            std::fflush(stderr);
        }
        const bgfx::ViewId allViews[] = {
            kBgfxDebugView, kBgfxEngineView, kBgfxEngineSortView,
            kBgfxRTTView, kBgfxWaterView, kBgfxEffectOverlayView,
            kBgfxShadowVolumeView, kBgfxShadowApplyView,
            kBgfxShroudOverlayView, kBgfxSceneDepthView,
            kBgfxSmudgeCopyView, kBgfxSmudgeView,
            kBgfxSceneCompositeView, kBgfxUIView,
        };
        for (int pass = 0; pass < 8; ++pass)
        {
            for (size_t i = 0; i < sizeof(allViews) / sizeof(allViews[0]); ++i)
            {
                bgfx::touch(allViews[i]);
            }
            bgfx::frame();
            if (trace)
            {
                std::fprintf(stderr, "[ggc] pre-warm pass %d done\n", pass);
                std::fflush(stderr);
            }
        }
        if (trace)
        {
            std::fprintf(stderr, "[ggc] pre-warm loop done\n");
            std::fflush(stderr);
        }
    }
#endif
}

template<typename H>
static void DestroyBgfxHandle(H & h)
{
    if (bgfx::isValid(h))
    {
        bgfx::destroy(h);
        h = BGFX_INVALID_HANDLE;
    }
}

void BgfxBackend::Shutdown()
{
    if (std::getenv("GGC_BGFX_PERF_LOG") != nullptr)
    {
        PerfSessionPrintSummary();
    }

    if (g_device.initialized)
    {
        // TheSuperHackers @bugfix bobtista 02/06/2026 Unbind every view's framebuffer and pump
        // empty frames before destroying resources: retires in-flight command-buffer references
        // and flushes a partial frame that Metal would otherwise assert on at shutdown.
        const bgfx::ViewId kBgfxMaxViewId = 15;
        for (bgfx::ViewId v = 0; v <= kBgfxMaxViewId; ++v)
        {
            bgfx::setViewFrameBuffer(v, BGFX_INVALID_HANDLE);
        }
        const int kShutdownDrainFrames = 4;
        for (int prep = 0; prep < kShutdownDrainFrames; ++prep)
        {
            bgfx::frame();
        }

        DestroyBgfxHandle(g_device.passthroughProgram);
        DestroyBgfxHandle(g_device.sceneCompositeProgram);
        DestroyBgfxHandle(g_device.sceneDepthProgram);
        DestroyBgfxHandle(g_device.smudgeProgram);
        DestroyBgfxHandle(g_device.fullscreenClearVB);
        DestroyBgfxHandle(g_device.uberProgram);
        DestroyBgfxHandle(g_device.uberInstancedProgram);
        DestroyBgfxHandle(g_device.treeProgram);
        DestroyBgfxHandle(g_uniforms.uSwayTable);
        DestroyBgfxHandle(g_uniforms.uShroudOffset);
        DestroyBgfxHandle(g_uniforms.uShroudScale);
        DestroyBgfxHandle(g_uniforms.sTex0);
        DestroyBgfxHandle(g_uniforms.sTex1);
        DestroyBgfxHandle(g_uniforms.sTex2);
        DestroyBgfxHandle(g_uniforms.sTex3);
        DestroyBgfxHandle(g_uniforms.sSceneDepth);
        DestroyBgfxHandle(g_uniforms.uMatDiffuse);
        DestroyBgfxHandle(g_uniforms.uMatAmbient);
        DestroyBgfxHandle(g_uniforms.uAtestParams);
        DestroyBgfxHandle(g_uniforms.uTssOps0);
        DestroyBgfxHandle(g_uniforms.uTssOps1);
        DestroyBgfxHandle(g_uniforms.uLightDirs);
        DestroyBgfxHandle(g_uniforms.uLightColors);
        DestroyBgfxHandle(g_uniforms.uLightAmbients);
        DestroyBgfxHandle(g_uniforms.uLightPositions);
        DestroyBgfxHandle(g_uniforms.uLightParams);
        DestroyBgfxHandle(g_uniforms.uSceneAmbient);
        DestroyBgfxHandle(g_uniforms.uLightingEnabled);
        DestroyBgfxHandle(g_uniforms.uTexcoordSelect);
        DestroyBgfxHandle(g_uniforms.uTexcoordSelect2);
        DestroyBgfxHandle(g_uniforms.uProjectedDecalMode);
        DestroyBgfxHandle(g_uniforms.uTexcoordSource);
        DestroyBgfxHandle(g_uniforms.uVertexColorFlags);
        DestroyBgfxHandle(g_uniforms.uObjectShroudDim);
        DestroyBgfxHandle(g_uniforms.uShroudParams);
        DestroyBgfxHandle(g_uniforms.uCloudParams);
        DestroyBgfxHandle(g_uniforms.uTexTransform0);
        DestroyBgfxHandle(g_uniforms.uTexTransform1);
        DestroyBgfxHandle(g_uniforms.uTexTransform0Z);
        DestroyBgfxHandle(g_uniforms.uTex1Transform0);
        DestroyBgfxHandle(g_uniforms.uTex1Transform1);
        DestroyBgfxHandle(g_uniforms.uTex1TransformZ);
        DestroyBgfxHandle(g_uniforms.uTex2Transform0);
        DestroyBgfxHandle(g_uniforms.uTex2Transform1);
        DestroyBgfxHandle(g_uniforms.uTexProjected);
        DestroyBgfxHandle(g_uniforms.uLegacyPixelShaderMode);
        DestroyBgfxHandle(g_uniforms.uZBias);
        DestroyBgfxHandle(g_uniforms.sCloudMap);
        DestroyBgfxHandle(g_device.shadowVolumeProgram);
        DestroyBgfxHandle(g_device.shadowApplyProgram);
        DestroySceneFramebuffer();
        DestroyBgfxHandle(g_uniforms.uShadowColor);
        DestroyBgfxHandle(g_uniforms.uPostParams);
        DestroyBgfxHandle(g_uniforms.uPostTexelSize);
        DestroyBgfxHandle(g_uniforms.uSoftParticleParams);
        DestroyBgfxHandle(g_uniforms.uShadowBias);
        DestroyBgfxHandle(g_uniforms.uMatEmissive);
        DestroyBgfxHandle(g_uniforms.uGrayscaleEnable);
        DestroyBgfxHandle(g_device.defaultWhiteTexture);
        DestroyBgfxHandle(g_device.defaultTransparentTexture);
        g_caches.renderTarget.clear();
        for (auto & kv : g_caches.framebuffer)
        {
            if (bgfx::isValid(kv.second.fb))
            {
                bgfx::destroy(kv.second.fb);
            }
        }
        g_caches.framebuffer.clear();
        // Cached engine buffers. Destroy before bgfx::shutdown
        // so the handles outlive nothing.
        for (auto & kv : g_caches.vb)
        {
            if (bgfx::isValid(kv.second.handle))
            {
                bgfx::destroy(kv.second.handle);
            }
        }
        g_caches.vb.clear();
        for (auto & kv : g_caches.ib)
        {
            if (bgfx::isValid(kv.second.handle))
            {
                bgfx::destroy(kv.second.handle);
            }
        }
        g_caches.ib.clear();
        for (auto & kv : g_caches.texture)
        {
            if (bgfx::isValid(kv.second))
            {
                bgfx::destroy(kv.second);
            }
        }
        g_caches.texture.clear();
        for (auto & kv : g_caches.textureBaseMip)
        {
            if (bgfx::isValid(kv.second))
            {
                bgfx::destroy(kv.second);
            }
        }
        g_caches.textureBaseMip.clear();
        g_caches.textureInfo.clear();
        g_caches.textureBaseMipInfo.clear();
        g_draw.vb         = BGFX_INVALID_HANDLE;
        g_draw.ib         = BGFX_INVALID_HANDLE;
        g_draw.staticVB   = BGFX_INVALID_HANDLE;
        g_draw.staticIB   = BGFX_INVALID_HANDLE;
        g_draw.tex[0]   = BGFX_INVALID_HANDLE;
        g_draw.tex[1]   = BGFX_INVALID_HANDLE;
        g_draw.tex[2]   = BGFX_INVALID_HANDLE;
        g_draw.tex[3]   = BGFX_INVALID_HANDLE;
        g_draw.sourceTextures[0] = nullptr;
        g_draw.sourceTextures[1] = nullptr;
        g_draw.sourceTextures[2] = nullptr;
        g_draw.sourceTextures[3] = nullptr;
        g_draw.sourceMaterial = nullptr;
        g_draw.explicitMaterialState = false;
        g_draw.ibOffset       = 0;
        g_draw.useStaticVB = false;
        g_draw.useStaticIB = false;
        g_draw.useTransientVB = false;
        g_draw.useTransientIB = false;
        g_draw.pendingVB.valid    = false;
        g_draw.pendingVB.coplanarNormalBias = false;
        g_draw.pendingIB.valid    = false;
        g_draw.activeTransientVBOwner = nullptr;
        g_draw.activeTransientIBOwner = nullptr;
        g_draw.activeVertexNormalBias = false;
        // Flush both deferred-destroy queues — bgfx::shutdown() tolerates stale handles but strict debug builds may assert.
        for (auto & h : g_caches.deferredDestroys)
        {
            if (bgfx::isValid(h))
            {
                bgfx::destroy(h);
            }
        }
        for (auto & h : g_caches.deferredDestroysPrev)
        {
            if (bgfx::isValid(h))
            {
                bgfx::destroy(h);
            }
        }
        g_caches.deferredDestroys.clear();
        g_caches.deferredDestroysPrev.clear();
        // TheSuperHackers @bugfix bobtista 02/06/2026 Drain the dynamic VB/IB deferred-
        // destroy queues too, so resized-out handles do not leak past shutdown.
        for (auto & h : g_caches.deferredDestroyVB)     { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        for (auto & h : g_caches.deferredDestroyVBPrev) { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        for (auto & h : g_caches.deferredDestroyIB)     { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        for (auto & h : g_caches.deferredDestroyIBPrev) { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        g_caches.deferredDestroyVB.clear();
        g_caches.deferredDestroyVBPrev.clear();
        g_caches.deferredDestroyIB.clear();
        g_caches.deferredDestroyIBPrev.clear();
        for (auto & h : g_caches.deferredDestroyStaticVB)     { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        for (auto & h : g_caches.deferredDestroyStaticVBPrev) { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        for (auto & h : g_caches.deferredDestroyStaticIB)     { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        for (auto & h : g_caches.deferredDestroyStaticIBPrev) { if (bgfx::isValid(h)) { bgfx::destroy(h); } }
        g_caches.deferredDestroyStaticVB.clear();
        g_caches.deferredDestroyStaticVBPrev.clear();
        g_caches.deferredDestroyStaticIB.clear();
        g_caches.deferredDestroyStaticIBPrev.clear();
        // TheSuperHackers @bugfix bobtista 02/06/2026 Destroy remaining registry entries at
        // shutdown. The isValid guards make Register_* entries (whose handles live in the caches
        // drained above) a safe no-op; Create_Texture entries own their texture/fb and need it.
        for (auto & kv : g_resourceRegistry.table)
        {
            BgfxResourceEntry & entry = kv.second;
            switch (entry.kind)
            {
                case BGFX_RR_KIND_VB:     DestroyBgfxHandle(entry.vb);  break;
                case BGFX_RR_KIND_IB:     DestroyBgfxHandle(entry.ib);  break;
                case BGFX_RR_KIND_TEXTURE:
                    if (bgfx::isValid(entry.fb))
                    {
                        DestroyBgfxHandle(entry.fb);
                    }
                    else
                    {
                        DestroyBgfxHandle(entry.texture);
                    }
                    break;
                default: break;
            }
        }
        g_resourceRegistry.table.clear();
        // TheSuperHackers @bugfix bobtista 02/06/2026 bgfx releases native GPU resources
        // lazily across its frame-latency window, so a single bgfx::frame() after queuing
        // all the destroys above leaves many still pending when bgfx::shutdown() runs,
        // producing a flood of "RefCount is 1 (expected 0)" warnings at exit. Pump enough
        // frames to fully drain the deferred native-release pipeline before shutdown.
        // bgfx's internal BGFX_CONFIG_MAX_FRAME_LATENCY is not exposed in the public
        // headers; its default is 3, so 4 frames covers the worst case with margin.
        const int kShutdownFlushFrames = 4;
        for (int flush = 0; flush < kShutdownFlushFrames; ++flush)
        {
            bgfx::frame();
        }
        bgfx::shutdown();
        g_device.initialized = false;
        WWDEBUG_SAY(("[BgfxBackend] bgfx::shutdown complete."));
    }

    // bgfx window is the main game window, do not destroy it.
    // DX8's secondary reference window is owned by DX8Wrapper.
    g_device.window = nullptr;
}

// -- Viewport ----------------------------------------------------------------

void BgfxBackend::Set_Viewport(const RenderBackendViewport & viewport)
{
    // Do NOT call the DX8 base viewport setter here - this method is called
    // FROM DX8Wrapper::Set_Viewport, so the legacy viewport is already set.
    // Calling the base class would cause infinite recursion.

    if (!g_device.initialized)
    {
        return;
    }

    // TheSuperHackers @fix bobtista 19/04/2026 Sync bgfx view rects with the
    // game's viewport. Without this, bgfx uses the full window for the 3D
    // scene while the game's picking/camera uses a smaller viewport (excluding
    // the control bar), causing a vertical click offset when selecting units.
    const uint16_t x = static_cast<uint16_t>(viewport.x);
    const uint16_t y = static_cast<uint16_t>(viewport.y);
    const uint16_t w = static_cast<uint16_t>(viewport.width);
    const uint16_t h = static_cast<uint16_t>(viewport.height);

    // TheSuperHackers @fix bobtista 20/04/2026 DX8Wrapper::Set_Viewport
    // is called from TWO very different contexts each frame:
    //   1. CameraClass::Apply() with the tactical 3D viewport
    //      (e.g., 1280x640 when the control bar is visible)
    //   2. Render2DClass::Render() with the full-canvas viewport
    //      (1280x800) for 2D UI drawing
    // The 2D UI has its own bgfx view (kBgfxUIView) so its rect is
    // independent. If we let the Render2DClass call stomp the 3D engine
    // views with the full-canvas rect, the 3D scene renders stretched
    // while the picking code still normalizes mouse Y through the
    // 640-tall tactical view — producing a vertical click offset that
    // scales with Y position. Ignore updates whose dimensions match the
    // full bgfx canvas: the 3D engine views should keep the smaller
    // tactical rect set by CameraClass::Apply.
    const bool isFullCanvas =
        (x == 0 && y == 0 &&
         static_cast<int>(w) == g_device.width &&
         static_cast<int>(h) == g_device.height);
    if (isFullCanvas)
    {
        return;
    }

    bgfx::setViewRect(kBgfxEngineView, x, y, w, h);
    bgfx::setViewRect(kBgfxEngineSortView, x, y, w, h);
    bgfx::setViewRect(kBgfxWaterView, x, y, w, h);
    bgfx::setViewRect(kBgfxEffectOverlayView, x, y, w, h);
    bgfx::setViewRect(kBgfxShadowVolumeView, x, y, w, h);
    bgfx::setViewRect(kBgfxShadowApplyView, x, y, w, h);
    bgfx::setViewRect(kBgfxSceneDepthView, x, y, w, h);
    bgfx::setViewRect(kBgfxSmudgeCopyView, x, y, w, h);
    bgfx::setViewRect(kBgfxSmudgeView, x, y, w, h);
    bgfx::setViewRect(kBgfxRTTView, x, y, w, h);
}

// -- View capture / post-effect primitives ----------------------------------

bool BgfxBackend::Initialize_View_Capture(RenderBackendViewCaptureKind kind)
{
    (void)kind;
    // Native bgfx post effects use the scene framebuffer directly. The legacy
    // W3DShaderManager filter capture path is intentionally reported as
    // unsupported until those filters are ported to scene-composite passes.
    return false;
}

void BgfxBackend::Release_View_Capture(RenderBackendViewCaptureKind kind)
{
    (void)kind;
}

bool BgfxBackend::Supports_View_Capture(RenderBackendViewCaptureKind kind) const
{
    (void)kind;
    return false;
}

bool BgfxBackend::Begin_View_Capture(RenderBackendViewCaptureKind kind)
{
    (void)kind;
    return false;
}

bool BgfxBackend::End_View_Capture(RenderBackendViewCaptureKind kind)
{
    (void)kind;
    return false;
}

bool BgfxBackend::Is_View_Capture_Active(RenderBackendViewCaptureKind kind) const
{
    (void)kind;
    return false;
}

bool BgfxBackend::Has_View_Capture(RenderBackendViewCaptureKind kind) const
{
    (void)kind;
    return false;
}

bool BgfxBackend::Bind_View_Capture_Texture(RenderBackendViewCaptureKind kind, unsigned int stage)
{
    (void)kind;
    (void)stage;
    return false;
}

bool BgfxBackend::Draw_View_Capture_Quad(RenderBackendViewCaptureKind kind,
                                         const RenderBackendScreenVertex * vertices,
                                         unsigned int vertex_count,
                                         bool use_second_uv)
{
    (void)kind;
    (void)vertices;
    (void)vertex_count;
    (void)use_second_uv;
    return false;
}

bool BgfxBackend::Draw_Screen_Quad(const RenderBackendScreenVertex * vertices,
                                   unsigned int vertex_count,
                                   bool use_second_uv)
{
    (void)vertices;
    (void)vertex_count;
    (void)use_second_uv;
    return false;
}

bool BgfxBackend::Capture_Back_Buffer_RGBA(unsigned int display_width,
                                           unsigned int display_height,
                                           unsigned int image_size,
                                           unsigned char * output_pixels,
                                           unsigned int output_capacity,
                                           unsigned int * output_width,
                                           unsigned int * output_height)
{
    (void)display_width;
    (void)display_height;
    (void)image_size;
    (void)output_pixels;
    (void)output_capacity;
    (void)output_width;
    (void)output_height;
    return false;
}

bool BgfxBackend::Request_Native_Screen_Shot(const char * path)
{
    if (!g_device.initialized || path == nullptr || path[0] == '\0')
    {
        return false;
    }

    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    return true;
}

// -- Frame lifecycle ---------------------------------------------------------

void BgfxBackend::Begin_Scene()
{
    PROFILER_SECTION_NAME("bgfx Begin_Scene");
    if (!g_device.initialized)
    {
        return;
    }

    const bool preserveRenderToTexture =
        g_views.renderToTexture && g_views.renderTargetTexture != nullptr;

    ResetFrameStats();

    // Destroy PREVIOUS frame's deferred textures. These were queued in
    // frame N-1, survived through bgfx::frame() at End_Scene of frame N-1,
    // so all in-flight draws referencing them are guaranteed complete.
    for (auto & h : g_caches.deferredDestroysPrev)
    {
        if (bgfx::isValid(h))
        {
            bgfx::destroy(h);
        }
    }
    g_caches.deferredDestroysPrev.clear();
    // TheSuperHackers @bugfix bobtista 02/06/2026 Same one-frame-delayed destroy for
    // dynamic VB/IB handles orphaned by a resize last frame.
    for (auto & h : g_caches.deferredDestroyVBPrev)
    {
        if (bgfx::isValid(h))
        {
            bgfx::destroy(h);
        }
    }
    g_caches.deferredDestroyVBPrev.clear();
    for (auto & h : g_caches.deferredDestroyIBPrev)
    {
        if (bgfx::isValid(h))
        {
            bgfx::destroy(h);
        }
    }
    g_caches.deferredDestroyIBPrev.clear();
    // TheSuperHackers @bugfix bobtista 02/06/2026 Same one-frame-delayed destroy for static
    // VB/IB handles dropped by a demotion-to-dynamic last frame.
    for (auto & h : g_caches.deferredDestroyStaticVBPrev)
    {
        if (bgfx::isValid(h))
        {
            bgfx::destroy(h);
        }
    }
    g_caches.deferredDestroyStaticVBPrev.clear();
    for (auto & h : g_caches.deferredDestroyStaticIBPrev)
    {
        if (bgfx::isValid(h))
        {
            bgfx::destroy(h);
        }
    }
    g_caches.deferredDestroyStaticIBPrev.clear();
    // Show the DX8 reference popup after a few frames, giving the game's
    // input system time to fully initialize. Showing too early steals focus
    // and permanently blocks mouse capture.
    // TheSuperHackers @build bobtista 29/04/2026 No DX8 ref popup on
    // non-Windows builds (no Win32 windowing API).
#ifdef _WIN32
    {
        if (s_dx8RefFrameCount >= 0)
        {
            s_dx8RefFrameCount++;
            if (s_dx8RefFrameCount > kDX8RefWindowShowDelayFrames)
            {
                s_dx8RefFrameCount = -1;
                HWND dx8Hwnd = FindWindowW(L"GGC_DX8RefWindow", nullptr);
                if (dx8Hwnd)
                {
                    ShowWindow(dx8Hwnd, SW_SHOWNA);
                }
                // Re-assert focus on the main game window. Use SetFocus
                // instead of SetForegroundWindow which has restrictions
                // on Windows that can cause it to silently fail.
                if (g_device.window)
                {
                    SetForegroundWindow(g_device.window);
                    SetFocus(g_device.window);
                }
            }
        }
    }
#endif

    // Check if the game window was resized (e.g., by Set_Render_Device) and
    // update bgfx's swapchain to match. Without this, bgfx renders at the
    // old resolution while the game expects the new one.
    if (g_device.window)
    {
        int w = 0;
        int h = 0;
        if (GetBackendWindowSize(g_device.window, w, h))
        {
            if (w != g_device.width || h != g_device.height)
            {
                WWDEBUG_SAY(("[BgfxBackend] Window resized %dx%d -> %dx%d, calling bgfx::reset.",
                             g_device.width, g_device.height, w, h));
                DestroySceneFramebuffer();
                g_device.width = w;
                g_device.height = h;
                uint32_t resetFlags = BGFX_RESET_NONE | g_device.msaaResetFlags
                    | (g_device.srgbEnabled ? BGFX_RESET_SRGB_BACKBUFFER : 0);
                if (std::getenv("GGC_BGFX_NO_DEPTH_CLAMP") == nullptr)
                {
                    resetFlags |= BGFX_RESET_DEPTH_CLAMP;
                }
#if defined(__APPLE__)
                if (std::getenv("GGC_MACOS_NO_FLUSH") == nullptr)
                {
                    resetFlags |= BGFX_RESET_FLUSH_AFTER_RENDER;
                }
#endif
                bgfx::reset(g_device.width, g_device.height, resetFlags);
                CreateSceneFramebuffer();
                ApplySceneFramebufferToViews();
            }
        }
    }

    // TheSuperHackers @fix bobtista 20/04/2026 Force a real draw on view 0
    // each frame so bgfx actually processes the view and covers the
    // backbuffer. Without this, touch-only activation leaves the backbuffer
    // with stale content from prior frames — visible as flickering yellow
    // strips from the chat button animation leaking under the control bar.
    if (bgfx::isValid(g_device.passthroughProgram) && bgfx::isValid(g_device.fullscreenClearVB))
    {
        float identity[16];
        IdentityMatrix(identity);
        bgfx::setViewRect(kBgfxDebugView, 0, 0,
                          static_cast<uint16_t>(g_device.width),
                          static_cast<uint16_t>(g_device.height));
        bgfx::setViewTransform(kBgfxDebugView, identity, identity);
        bgfx::setVertexBuffer(0, g_device.fullscreenClearVB);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                       | BGFX_STATE_DEPTH_TEST_ALWAYS);
        bgfx::submit(kBgfxDebugView, g_device.passthroughProgram);
        g_stats.debugSubmits++;
    }
    else
    {
        bgfx::touch(kBgfxDebugView);
    }
    bgfx::touch(kBgfxEngineView);
    bgfx::touch(kBgfxEngineSortView);
    bgfx::touch(kBgfxWaterView);
    bgfx::touch(kBgfxEffectOverlayView);
    bgfx::touch(kBgfxShadowVolumeView);
    bgfx::touch(kBgfxShadowApplyView);
    bgfx::touch(kBgfxShroudOverlayView);
    bgfx::touch(kBgfxSmudgeCopyView);
    bgfx::touch(kBgfxSmudgeView);
    if (bgfx::isValid(g_device.sceneReadableDepthFB))
    {
        bgfx::touch(kBgfxSceneDepthView);
    }
    bgfx::touch(kBgfxSceneCompositeView);
    bgfx::touch(kBgfxUIView);
    g_views.overlay2DActive = false;
    // TheSuperHackers @fix bobtista 21/04/2026 Reset ALL 3D view rects to full canvas at
    // Begin_Scene so a frame that never calls Set_Viewport does not inherit the last
    // tactical rect on some views but not others.
    bgfx::setViewRect(kBgfxEngineView,        0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxEngineSortView,    0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxWaterView,         0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxEffectOverlayView, 0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxShadowVolumeView,  0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxShadowApplyView,   0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxShroudOverlayView, 0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxSceneDepthView,    0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxSceneCompositeView, 0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxSmudgeCopyView,    0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxSmudgeView,        0, 0, g_device.width, g_device.height);
    if (!preserveRenderToTexture)
    {
        bgfx::setViewRect(kBgfxRTTView,       0, 0, g_device.width, g_device.height);
    }
    bgfx::setViewRect(kBgfxUIView,            0, 0, g_device.width, g_device.height);
    {
        // TheSuperHackers @bugfix bobtista 30/04/2026 Keep the dedicated
        // 2D UI view in sync with runtime window-size changes too. Leaving
        // it at the startup rect makes control-bar/radar art render through
        // an old canvas while text/widgets are laid out for the new size.
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxUIView, identityMtx, identityMtx);
    }
    // No clear on water view — it composites over the opaque scene.
    bgfx::setViewClear(kBgfxWaterView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    // TheSuperHackers @bugfix bobtista 28/04/2026 Preserve water submit
    // order. Shore foam/water-track quads are submitted after the water
    // surface; bgfx default sorting can place the surface last and cover
    // the foam.
    bgfx::setViewMode(kBgfxWaterView, bgfx::ViewMode::Sequential);
    if (preserveRenderToTexture)
    {
        auto rtIt = g_caches.framebuffer.find(g_views.renderTargetTexture);
        if (rtIt != g_caches.framebuffer.end())
        {
            const BgfxFramebufferEntry & entry = rtIt->second;
            bgfx::setViewFrameBuffer(kBgfxRTTView, entry.fb);
            bgfx::setViewRect(kBgfxRTTView, 0, 0, entry.width, entry.height);
            bgfx::touch(kBgfxRTTView);
            g_views.renderToTexture = true;
        }
    }
    else
    {
        g_views.renderToTexture = false;
        g_views.renderTargetTexture = nullptr;
    }
    g_views.smudgeActive = false;

    // TheSuperHackers @fix bobtista 21/04/2026 Reset the terrain-blend flag each frame.
    // Clear_State_Overrides deliberately preserves g_draw.texcoordSelect[1] because
    // Override_Terrain_Blend runs BEFORE Set_Shader, so Begin_Scene must clear it instead.
    g_draw.texcoordSelect[1] = 0.0f;

    // TheSuperHackers @bugfix bobtista 23/04/2026 Clear cached sampler bindings every frame so
    // the previous frame's final UI texture cannot leak into 3D draws that skip Set_Texture.
    for (int i = 0; i < 4; ++i)
    {
        g_draw.tex[i] = BGFX_INVALID_HANDLE;
        g_draw.samplerFlags[i] = 0;
        g_draw.mipFilterDisabled[i] = false;
        g_draw.textureIsMissing[i] = false;
        g_draw.sourceTextures[i] = nullptr;
    }
    // TheSuperHackers @fix bobtista 21/04/2026 Defensively reset transient view flags at
    // Begin_Scene; a map transition or early exit can skip an End_* call and leak stuck state.
    g_views.waterOverrideActive      = false;
    g_views.waterOverlayActive       = false;
    g_views.effectOverlayActive      = false;
    g_views.smudgeActive             = false;
    g_views.inSortFlush              = false;
    g_views.treeShaderActive         = false;
    g_views.shadowVolumeActive       = false;
    g_views.shroudTexturePassActive  = false;
    g_views.shroudTexturePassStage   = 0;
    g_views.projectedShadowDecalActive = false;
    g_views.projectedDecalMode       = RB_PROJECTED_DECAL_NONE;
    g_views.skipNextSubmitEngineDraw = false;
}

void BgfxBackend::Clear(bool clear_color, bool clear_z_stencil,
                        const Vector3 & color,
                        float dest_alpha, float z, unsigned int stencil)
{
    if (!g_device.initialized || !g_views.renderToTexture
        || g_views.renderTargetTexture == nullptr)
    {
        return;
    }

    uint16_t clearFlags = BGFX_CLEAR_NONE;
    if (clear_color)
    {
        clearFlags |= BGFX_CLEAR_COLOR;
    }
    if (clear_z_stencil)
    {
        clearFlags |= BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL;
    }
    if (clearFlags == BGFX_CLEAR_NONE)
    {
        return;
    }

    bgfx::setViewClear(kBgfxRTTView,
                        clearFlags,
                        MakeBgfxClearColor(color, dest_alpha),
                        z,
                        static_cast<uint8_t>(stencil));
    bgfx::touch(kBgfxRTTView);
}

void BgfxBackend::End_Scene(bool /*flip_frame*/)
{
    PROFILER_SECTION_NAME("bgfx End_Scene");
    if (!g_device.initialized)
    {
        return;
    }
    // Re-apply captured camera transforms to both views. The engine calls
    // Set_Projection_Transform_With_Z_Bias multiple times per frame
    // (camera, water reflections, shadows, sneak attack). Since bgfx's
    // setViewTransform is retroactive for the whole frame, the last call
    // would stomp earlier draws. We re-apply the camera projection that
    // was active at the first opaque draw (view 1) and at sort-flush
    // time (view 2).
    if (g_frame.cameraCaptured)
    {
        bgfx::setViewTransform(kBgfxEngineView, g_frame.cameraView, g_frame.cameraProj);
        bgfx::setViewTransform(kBgfxWaterView, g_frame.cameraView, g_frame.cameraProj);
        bgfx::setViewTransform(kBgfxShadowVolumeView, g_frame.cameraView, g_frame.cameraProj);
        bgfx::setViewTransform(kBgfxShroudOverlayView, g_frame.cameraView, g_frame.cameraProj);
        bgfx::setViewTransform(kBgfxSceneDepthView, g_frame.cameraView, g_frame.cameraProj);
        g_frame.cameraCaptured = false;
    }
    if (g_frame.sortProjCaptured)
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_frame.sortProj);
        g_frame.sortProjCaptured = false;
    }
    // Push identity transforms and current rect to the UI view
    // so 2D overlay draws land in screen space over the 3D scene.
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxUIView, identityMtx, identityMtx);
    }
    bgfx::setViewRect(kBgfxUIView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));

    // Debug view (0) runs FIRST to emit the backbuffer clear quad. Then RTT
    // (3), engine opaque (1), scene depth (11), shadow volume fill (6), shadow
    // darken (7), water (4), sort (2), effect overlay (5), heat-haze smudge
    // copy/draw (12/13), scene composite (9), UI overlay (10) last.
    // TheSuperHackers @bugfix bobtista 20/04/2026 View 0 MUST be
    // included — when omitted, bgfx defers it to the end with a 1x1
    // viewport, and the full-canvas clear never fires (causing
    // flickering UI leftovers under the control bar on frames where
    // that area is not overdrawn).
    bgfx::ViewId viewOrder[] = {
        kBgfxDebugView,            // 0 — full-canvas clear quad, must run first
        kBgfxRTTView,              // 3
        kBgfxEngineView,           // 1
        kBgfxSceneDepthView,       // 11 — readable opaque scene depth
        kBgfxShadowVolumeView,     // 6 — stencil shadow volume fill
        kBgfxShadowApplyView,      // 7 — stencil shadow darken
        kBgfxWaterView,            // 4
        kBgfxEngineSortView,       // 2
        kBgfxEffectOverlayView,    // 5
        kBgfxShroudOverlayView,    // 8 — shroud darkening after scene detail
        kBgfxSmudgeCopyView,       // 12 — scene-color snapshot for heat haze
        kBgfxSmudgeView,           // 13 — heat-haze/smudge distortion
        kBgfxSceneCompositeView,   // 9 — scene color to swapchain
        kBgfxUIView,               // 10 — 2D UI overlay (last)
    };
    bgfx::setViewOrder(kBgfxDebugView, BX_COUNTOF(viewOrder), viewOrder);

    SubmitSceneComposite();
    LogFrameStats();
    UpdateBgfxStatsLog();

    // TheSuperHackers @feature bobtista 02/05/2026 -bgfxScreenshotAfter N
    // arms a periodic native back-buffer capture: once frameIndex >= N, every
    // 500 bgfx frames we request bgfx::requestScreenShot into a .NNNNNN.bmp
    // suffixed file derived from the configured base path. Lets a developer
    // (or automated harness) pick whichever frame corresponds to the
    // gameplay state of interest, since early frames are loading screens.
#ifdef RTS_ZEROHOUR
    {
        int captureFrame = GGC_GetBgfxScreenshotFrame();
        if (captureFrame <= 0)
        {
            if (const char * frameEnv = std::getenv("GGC_BGFX_SCREENSHOT_AFTER"))
            {
                captureFrame = std::atoi(frameEnv);
            }
        }
        uint32_t interval = 500;
        if (const char * intervalEnv = std::getenv("GGC_BGFX_SCREENSHOT_INTERVAL"))
        {
            const int parsedInterval = std::atoi(intervalEnv);
            if (parsedInterval > 0)
            {
                interval = static_cast<uint32_t>(parsedInterval);
            }
        }
        static uint32_t s_lastShotFrame = 0;
        if (captureFrame > 0
            && static_cast<int>(g_stats.frameIndex) >= captureFrame
            && (g_stats.frameIndex - s_lastShotFrame) >= interval)
        {
            s_lastShotFrame = g_stats.frameIndex;
            const char * basePath = GGC_GetBgfxScreenshotPath();
            if ((basePath == nullptr || basePath[0] == '\0'))
            {
                basePath = std::getenv("GGC_BGFX_SCREENSHOT_PATH");
            }
            if (basePath != nullptr && basePath[0] != '\0')
            {
                char numbered[512];
                std::snprintf(numbered, sizeof(numbered), "%s.%06u.bmp",
                              basePath, g_stats.frameIndex);
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, numbered);
            }
        }
    }
#endif

    bgfx::frame();
    // TheSuperHackers @perf bobtista 24/06/2026 Feed per-frame bgfx stats to
    // Tracy plots so CPU time, draw count, and GPU time share one timeline.
#if defined(RTS_PROFILE_TRACY)
    {
        const bgfx::Stats * pstats = bgfx::getStats();
        if (pstats != NULL && pstats->cpuTimerFreq != 0 && pstats->gpuTimerFreq != 0)
        {
            const double toMsCpu = 1000.0 / double(pstats->cpuTimerFreq);
            const double toMsGpu = 1000.0 / double(pstats->gpuTimerFreq);
            PROFILER_PLOT("draws", double(pstats->numDraw));
            PROFILER_PLOT("cpu frame ms", double(pstats->cpuTimeEnd - pstats->cpuTimeBegin) * toMsCpu);
            PROFILER_PLOT("gpu frame ms", double(pstats->gpuTimeEnd - pstats->gpuTimeBegin) * toMsGpu);
            PROFILER_PLOT("transient vb kb", double(pstats->transientVbUsed) / 1024.0);
        }
    }
#endif

#if defined(SAGE_USE_SDL3)
    if (!g_device.mainWindowShown && g_device.window != nullptr)
    {
        SDL_ShowWindow(static_cast<SDL_Window *>(g_device.window));
        g_device.mainWindowShown = true;
    }
#endif

    // Rotate deferred texture destroy buffers. Current frame's deferred
    // handles move to "prev" — they'll be destroyed at the NEXT Begin_Scene
    // after one more bgfx::frame() guarantees all references are gone.
    // Begin_Scene drained prev, so it is empty here; swap is cheaper than
    // insert+clear and avoids any vector growth.
    g_caches.deferredDestroysPrev.swap(g_caches.deferredDestroys);
    // TheSuperHackers @bugfix bobtista 02/06/2026 Rotate the dynamic VB/IB deferred-
    // destroy queues the same way.
    g_caches.deferredDestroyVBPrev.swap(g_caches.deferredDestroyVB);
    g_caches.deferredDestroyIBPrev.swap(g_caches.deferredDestroyIB);
    g_caches.deferredDestroyStaticVBPrev.swap(g_caches.deferredDestroyStaticVB);
    g_caches.deferredDestroyStaticIBPrev.swap(g_caches.deferredDestroyStaticIB);

    // Transient buffers are freed at bgfx::frame time. Invalidate the
    // pending and current slots so nothing next frame tries to reuse
    // a dead handle.
    g_draw.pendingVB.valid    = false;
    g_draw.pendingVB.coplanarNormalBias = false;
    g_draw.pendingIB.valid    = false;
    g_draw.useTransientVB = false;
    g_draw.useTransientIB = false;
    g_draw.activeTransientVBOwner = nullptr;
    g_draw.activeTransientIBOwner = nullptr;
    g_draw.activeVertexNormalBias = false;
}

WW3DFormat BgfxBackend::Get_Back_Buffer_Format() const
{
    return WW3D_FORMAT_A8R8G8B8;
}

void BgfxBackend::Set_Texture_Bitdepth(int bitdepth)
{
    WWASSERT(bitdepth == 16 || bitdepth == 32);
    if (bitdepth == 16 || bitdepth == 32)
    {
        m_textureBitDepth = bitdepth;
    }
}

int BgfxBackend::Get_Texture_Bitdepth() const
{
    return m_textureBitDepth;
}

bool BgfxBackend::Supports_Texture_Op(RenderBackendTextureOpCapability capability) const
{
    switch (capability)
    {
        case RB_TEXTURE_OP_SELECTARG1:
        case RB_TEXTURE_OP_MODULATE:
        case RB_TEXTURE_OP_MODULATE2X:
        case RB_TEXTURE_OP_ADD:
        case RB_TEXTURE_OP_ADDSMOOTH:
        case RB_TEXTURE_OP_SUBTRACT:
        case RB_TEXTURE_OP_BLENDTEXTUREALPHA:
        case RB_TEXTURE_OP_BLENDCURRENTALPHA:
        case RB_TEXTURE_OP_ADDSIGNED:
            return true;
        case RB_TEXTURE_OP_BUMPENVMAP:
        case RB_TEXTURE_OP_BUMPENVMAPLUMINANCE:
        case RB_TEXTURE_OP_ADDSIGNED2X:
        case RB_TEXTURE_OP_MODULATEALPHA_ADDCOLOR:
        default:
            return false;
    }
}

RenderBackendTextureLimits BgfxBackend::Get_Texture_Limits() const
{
    constexpr unsigned kTextureAspectRatioLimit = 8;
    const bgfx::Caps * caps = bgfx::getCaps();
    if (caps == nullptr)
    {
        return IRenderBackend::Get_Texture_Limits();
    }

    return {
        caps->limits.maxTextureSize,
        caps->limits.maxTextureSize,
        caps->limits.maxTextureSize,
        kTextureAspectRatioLimit
    };
}

int BgfxBackend::Get_Max_Texture_Stages() const
{
    return kBgfxTextureStages;
}

void BgfxBackend::Set_MSAA_Mode(RenderBackendMSAAMode mode)
{
    m_msaaMode = mode;
}

RenderBackendMSAAMode BgfxBackend::Get_MSAA_Mode() const
{
    return m_msaaMode;
}

bool BgfxBackend::Get_Device_Identity(RenderBackendDeviceIdentity & identity) const
{
    identity = {};
    identity.max_simultaneous_textures = kBgfxTextureStages;
    identity.pixel_shader_major = 2;
    identity.pixel_shader_minor = 0;
    return true;
}

static void LogBgfxTransientDiag(const char *event,
                                 const char *kind,
                                 const void *owner,
                                 uint32_t count,
                                 bool pendingValid,
                                 bool pendingOwnerMatch,
                                 bool active,
                                 bool activeOwnerMatch,
                                 const char *decision)
{
    if (std::getenv("GGC_BGFX_TRANSIENT_DIAG") == nullptr)
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_bgfx_transient_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s frame=%u kind=%s owner=%p count=%u pendingValid=%d pendingOwnerMatch=%d active=%d activeOwnerMatch=%d decision=%s inSort=%d\n",
                     event,
                     g_stats.frameIndex,
                     kind,
                     owner,
                     count,
                     pendingValid ? 1 : 0,
                     pendingOwnerMatch ? 1 : 0,
                     active ? 1 : 0,
                     activeOwnerMatch ? 1 : 0,
                     decision ? decision : "",
                     g_views.inSortFlush ? 1 : 0);
        std::fclose(diag);
    }
}

namespace
{
bgfx::DynamicVertexBufferHandle FindResourceVertexBufferHandle(const VertexBufferClass * vb);
bgfx::DynamicIndexBufferHandle FindResourceIndexBufferHandle(const IndexBufferClass * ib);
bgfx::VertexBufferHandle FindResourceStaticVertexBufferHandle(const VertexBufferClass * vb);
bgfx::IndexBufferHandle FindResourceStaticIndexBufferHandle(const IndexBufferClass * ib);
void MirrorDynamicVertexHandleToResource(const VertexBufferClass * vb,
                                         bgfx::DynamicVertexBufferHandle handle);
void MirrorDynamicIndexHandleToResource(const IndexBufferClass * ib,
                                        bgfx::DynamicIndexBufferHandle handle);
}

// -- Vertex / index buffers --------------------------------------------------

void BgfxBackend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    FixedFunctionState::Set_Vertex_Buffer(vb, stream);
    (void)stream;
    // Cache is populated by Upload_Vertex_Buffer_Data on the engine's own write
    // lock. Set_Vertex_Buffer just looks up whatever is already there; on a
    // miss it can rebuild from the buffer object's CPU-side write snapshot.
    g_draw.useTransientVB = false;
    g_draw.useStaticVB = false;
    g_draw.staticVB = BGFX_INVALID_HANDLE;
    g_draw.activeTransientVBOwner = nullptr;
    g_draw.activeVertexNormalBias = false;
    // TheSuperHackers @bugfix bobtista 27/04/2026 Legacy fixed-function
    // supplies a white diffuse color when the bound FVF has no COLOR0 element. bgfx
    // missing attributes read as zero, so tell the shader when it must
    // substitute the fixed-function default.
    g_draw.vertexColorFlags[0] = (vb != nullptr
        && vb->FVF_Info().Has_Diffuse()) ? 1.0f : 0.0f;
    g_draw.fvfHasNormal = (vb != nullptr
        && vb->FVF_Info().Has_Normal());
    bgfx::VertexBufferHandle staticResourceHandle = FindResourceStaticVertexBufferHandle(vb);
    if (bgfx::isValid(staticResourceHandle))
    {
        g_draw.staticVB = staticResourceHandle;
        g_draw.vb = BGFX_INVALID_HANDLE;
        g_draw.useStaticVB = true;
    }
    else
    {
        bgfx::DynamicVertexBufferHandle resourceHandle = FindResourceVertexBufferHandle(vb);
        if (bgfx::isValid(resourceHandle))
        {
            g_draw.vb = resourceHandle;
        }
        else
        {
            auto it = g_caches.vb.find(vb);
            if (it != g_caches.vb.end())
            {
                g_draw.vb = it->second.handle;
                MirrorDynamicVertexHandleToResource(vb, it->second.handle);
            }
            else
            {
                g_draw.vb = BGFX_INVALID_HANDLE;
                // Last-resort capture for static VBs that were written before bgfx
                // registration/capture was active. Do not lock the legacy buffer here:
                // bgfx must consume the backend-neutral CPU snapshot maintained by
                // the buffer write paths.
                if (vb != nullptr && g_device.initialized && vb->Has_CPU_Buffer_Data())
                {
                    const unsigned int bytes =
                        vb->Get_Vertex_Count() * vb->FVF_Info().Get_FVF_Size();
                    if (vb->Get_CPU_Buffer_Size() >= bytes)
                    {
                        Upload_Vertex_Buffer_Data(vb, vb->Peek_CPU_Buffer_Data(), bytes);
                        bgfx::VertexBufferHandle staticHandle = FindResourceStaticVertexBufferHandle(vb);
                        if (bgfx::isValid(staticHandle))
                        {
                            g_draw.staticVB = staticHandle;
                            g_draw.vb = BGFX_INVALID_HANDLE;
                            g_draw.useStaticVB = true;
                        }
                        else
                        {
                            auto it2 = g_caches.vb.find(vb);
                            if (it2 != g_caches.vb.end())
                            {
                                g_draw.vb = it2->second.handle;
                                MirrorDynamicVertexHandleToResource(vb, it2->second.handle);
                            }
                        }
                    }
                }
            }
        }
    }
}

void BgfxBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    FixedFunctionState::Set_Vertex_Buffer(vba);
    g_draw.vertexColorFlags[0] =
        vba.FVF_Info().Has_Diffuse() ? 1.0f : 0.0f;
    g_draw.fvfHasNormal =
        vba.FVF_Info().Has_Normal();
    g_draw.useStaticVB = false;
    g_draw.staticVB = BGFX_INVALID_HANDLE;
    // If the matching Capture_Dynamic_Vertex_Data already
    // allocated a transient VB for this access class, claim it for the
    // next draw. Otherwise miss the cache and skip the bgfx submit.
    if (g_draw.pendingVB.valid && g_draw.pendingVB.owner == &vba)
    {
        LogBgfxTransientDiag("set", "vb", &vba,
                             static_cast<uint32_t>(vba.Get_Vertex_Count()),
                             g_draw.pendingVB.valid, true,
                             g_draw.useTransientVB,
                             g_draw.activeTransientVBOwner == &vba,
                             "claim-pending");
        g_draw.useTransientVB = true;
        g_draw.transientVB    = g_draw.pendingVB.tvb;
        g_draw.activeVertexNormalBias = g_draw.pendingVB.coplanarNormalBias;
        g_draw.pendingVB.valid    = false;
        g_draw.activeTransientVBOwner = &vba;
    }
    else if (g_draw.useTransientVB && g_draw.activeTransientVBOwner == &vba)
    {
        LogBgfxTransientDiag("set", "vb", &vba,
                             static_cast<uint32_t>(vba.Get_Vertex_Count()),
                             g_draw.pendingVB.valid,
                             g_draw.pendingVB.owner == &vba,
                             true,
                             true,
                             "reuse-active");
        // The sorting renderer applies a saved material/texture state for
        // each sorted run, then rebinds the same per-flush transient VB. A
        // transient buffer remains valid until bgfx::frame(), so allow that
        // same access object to be rebound after the first claim.
    }
    else
    {
        LogBgfxTransientDiag("set", "vb", &vba,
                             static_cast<uint32_t>(vba.Get_Vertex_Count()),
                             g_draw.pendingVB.valid,
                             g_draw.pendingVB.owner == &vba,
                             g_draw.useTransientVB,
                             g_draw.activeTransientVBOwner == &vba,
                             "miss");
        g_draw.useTransientVB = false;
        g_draw.vb         = BGFX_INVALID_HANDLE;
        g_draw.activeTransientVBOwner = nullptr;
        g_draw.activeVertexNormalBias = false;
    }
}

void BgfxBackend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    FixedFunctionState::Set_Index_Buffer(ib, index_base_offset);
    g_draw.useTransientIB = false;
    g_draw.useStaticIB = false;
    g_draw.staticIB = BGFX_INVALID_HANDLE;
    g_draw.activeTransientIBOwner = nullptr;
    bgfx::IndexBufferHandle staticResourceHandle = FindResourceStaticIndexBufferHandle(ib);
    if (bgfx::isValid(staticResourceHandle))
    {
        g_draw.staticIB = staticResourceHandle;
        g_draw.ib = BGFX_INVALID_HANDLE;
        g_draw.useStaticIB = true;
    }
    else
    {
        bgfx::DynamicIndexBufferHandle resourceHandle = FindResourceIndexBufferHandle(ib);
        if (bgfx::isValid(resourceHandle))
        {
            g_draw.ib = resourceHandle;
        }
        else
        {
            auto it = g_caches.ib.find(ib);
            if (it != g_caches.ib.end())
            {
                g_draw.ib = it->second.handle;
                MirrorDynamicIndexHandleToResource(ib, it->second.handle);
            }
            else
            {
                g_draw.ib = BGFX_INVALID_HANDLE;
                // Last-resort capture for static IBs not yet in cache. Use the
                // backend-neutral CPU snapshot instead of locking a legacy index buffer.
                if (ib != nullptr && g_device.initialized && ib->Has_CPU_Buffer_Data())
                {
                    const unsigned int bytes = ib->Get_Index_Count() * sizeof(unsigned short);
                    if (ib->Get_CPU_Buffer_Size() >= bytes)
                    {
                        Upload_Index_Buffer_Data(ib, ib->Peek_CPU_Buffer_Data(), bytes);
                        bgfx::IndexBufferHandle staticHandle = FindResourceStaticIndexBufferHandle(ib);
                        if (bgfx::isValid(staticHandle))
                        {
                            g_draw.staticIB = staticHandle;
                            g_draw.ib = BGFX_INVALID_HANDLE;
                            g_draw.useStaticIB = true;
                        }
                        else
                        {
                            auto it2 = g_caches.ib.find(ib);
                            if (it2 != g_caches.ib.end())
                            {
                                g_draw.ib = it2->second.handle;
                                MirrorDynamicIndexHandleToResource(ib, it2->second.handle);
                            }
                        }
                    }
                }
            }
        }
    }
    g_draw.ibOffset = index_base_offset;
}

void BgfxBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    FixedFunctionState::Set_Index_Buffer(iba, index_base_offset);
    g_draw.useStaticIB = false;
    g_draw.staticIB = BGFX_INVALID_HANDLE;
    if (g_draw.pendingIB.valid && g_draw.pendingIB.owner == &iba)
    {
        LogBgfxTransientDiag("set", "ib", &iba,
                             static_cast<uint32_t>(iba.Get_Index_Count()),
                             g_draw.pendingIB.valid, true,
                             g_draw.useTransientIB,
                             g_draw.activeTransientIBOwner == &iba,
                             "claim-pending");
        g_draw.useTransientIB = true;
        g_draw.transientIB    = g_draw.pendingIB.tib;
        g_draw.pendingIB.valid    = false;
        g_draw.activeTransientIBOwner = &iba;
    }
    else if (g_draw.useTransientIB && g_draw.activeTransientIBOwner == &iba)
    {
        LogBgfxTransientDiag("set", "ib", &iba,
                             static_cast<uint32_t>(iba.Get_Index_Count()),
                             g_draw.pendingIB.valid,
                             g_draw.pendingIB.owner == &iba,
                             true,
                             true,
                             "reuse-active");
        // See Set_Vertex_Buffer(DynamicVBAccessClass&): sorted runs reuse
        // the same transient IB several times before the frame boundary.
    }
    else
    {
        LogBgfxTransientDiag("set", "ib", &iba,
                             static_cast<uint32_t>(iba.Get_Index_Count()),
                             g_draw.pendingIB.valid,
                             g_draw.pendingIB.owner == &iba,
                             g_draw.useTransientIB,
                             g_draw.activeTransientIBOwner == &iba,
                             "miss");
        g_draw.useTransientIB = false;
        g_draw.ib         = BGFX_INVALID_HANDLE;
        g_draw.activeTransientIBOwner = nullptr;
    }
    g_draw.ibOffset = index_base_offset;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Override
// Set_Index_Buffer_Index_Offset so we capture the per-mesh base vertex
// offset. DX8PolygonRendererClass::Render calls this once per mesh
// before Draw_Triangles to shift which vertex slot in the shared
// category VB each index resolves to. Without this override the bgfx
// path keeps using the stale offset from Set_Index_Buffer, so every
// mesh inside the same rigid FVF category would draw using the first
// mesh's vertex slots.
void BgfxBackend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    g_draw.ibOffset = static_cast<unsigned short>(offset);
}

// -- Write-side capture ----------------------------------------------------
//
// Called from VertexBufferClass::WriteLockClass / IndexBufferClass::WriteLockClass
// destructors after the engine has finished writing data through the
// CPU-mapped lock pointer. The pointer is still valid (Unlock has not yet
// been called) so we can safely copy the bytes into bgfx-managed memory and
// stamp out a static bgfx VB/IB. Cached by source pointer; reused on every
// subsequent Set_Vertex_Buffer that references the same engine VB.
//
// Cleanly bypasses the Intel UHD POOL_DEFAULT lock corruption: we never lock
// the source d3d8 buffer ourselves. We piggyback on the engine's own write
// lock, which the engine has to do anyway and which the driver handles
// correctly because it is a real WRITE lock.

namespace
{
static void LogBgfxBufferUpdate(const char *kind,
                                const void *owner,
                                const void *src,
                                unsigned int offset,
                                unsigned int size_bytes,
                                uint16_t handle_idx,
                                const bgfx::Memory *mem)
{
    if (std::getenv("GGC_BGFX_BUFFER_UPDATE_DIAG") == nullptr)
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_bgfx_buffer_update_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s frame=%u owner=%p src=%p offset=%u size=%u handle=%u mem=%p memData=%p memSize=%u\n",
                     kind,
                     g_stats.frameIndex,
                     owner,
                     src,
                     offset,
                     size_bytes,
                     handle_idx,
                     static_cast<const void *>(mem),
                     mem != nullptr ? static_cast<const void *>(mem->data) : nullptr,
                     mem != nullptr ? mem->size : 0);
        std::fclose(diag);
    }
}

// TheSuperHackers @refactor bobtista 11/04/2026 Dynamic buffer
// ensure helpers. Return the cached dynamic VB / IB handle for the given
// engine buffer, creating it sized to the full capacity on first sight.
// Returned handle is guaranteed valid on success; invalid handle on
// failure. Used by both the full-buffer (WriteLockClass) and sub-range
// (AppendLockClass) capture paths.
BgfxResourceEntry * FindVertexBufferResourceEntry(const VertexBufferClass * vb)
{
    if (vb == nullptr || !vb->Has_Backend_Resource())
    {
        return nullptr;
    }
    auto it = g_resourceRegistry.table.find(vb->Get_Backend_Resource().id);
    if (it == g_resourceRegistry.table.end())
    {
        return nullptr;
    }
    BgfxResourceEntry & entry = it->second;
    if (entry.kind != BGFX_RR_KIND_VB || entry.owner != vb)
    {
        return nullptr;
    }
    return &entry;
}

BgfxResourceEntry * FindIndexBufferResourceEntry(const IndexBufferClass * ib)
{
    if (ib == nullptr || !ib->Has_Backend_Resource())
    {
        return nullptr;
    }
    auto it = g_resourceRegistry.table.find(ib->Get_Backend_Resource().id);
    if (it == g_resourceRegistry.table.end())
    {
        return nullptr;
    }
    BgfxResourceEntry & entry = it->second;
    if (entry.kind != BGFX_RR_KIND_IB || entry.owner != ib)
    {
        return nullptr;
    }
    return &entry;
}

void MirrorDynamicVertexHandleToResource(const VertexBufferClass * vb,
                                         bgfx::DynamicVertexBufferHandle handle)
{
    if (BgfxResourceEntry * entry = FindVertexBufferResourceEntry(vb))
    {
        entry->dvb = handle;
    }
}

void MirrorDynamicIndexHandleToResource(const IndexBufferClass * ib,
                                        bgfx::DynamicIndexBufferHandle handle)
{
    if (BgfxResourceEntry * entry = FindIndexBufferResourceEntry(ib))
    {
        entry->dib = handle;
    }
}

void ClearDynamicVertexHandleFromResource(const VertexBufferClass * vb,
                                          bgfx::DynamicVertexBufferHandle stale)
{
    if (BgfxResourceEntry * entry = FindVertexBufferResourceEntry(vb))
    {
        if (!bgfx::isValid(entry->dvb) || entry->dvb.idx == stale.idx)
        {
            entry->dvb = BGFX_INVALID_HANDLE;
        }
    }
}

void ClearDynamicIndexHandleFromResource(const IndexBufferClass * ib,
                                         bgfx::DynamicIndexBufferHandle stale)
{
    if (BgfxResourceEntry * entry = FindIndexBufferResourceEntry(ib))
    {
        if (!bgfx::isValid(entry->dib) || entry->dib.idx == stale.idx)
        {
            entry->dib = BGFX_INVALID_HANDLE;
        }
    }
}

bgfx::DynamicVertexBufferHandle FindResourceVertexBufferHandle(const VertexBufferClass * vb)
{
    if (BgfxResourceEntry * entry = FindVertexBufferResourceEntry(vb))
    {
        if (bgfx::isValid(entry->dvb))
        {
            return entry->dvb;
        }
    }
    return BGFX_INVALID_HANDLE;
}

bgfx::DynamicIndexBufferHandle FindResourceIndexBufferHandle(const IndexBufferClass * ib)
{
    if (BgfxResourceEntry * entry = FindIndexBufferResourceEntry(ib))
    {
        if (bgfx::isValid(entry->dib))
        {
            return entry->dib;
        }
    }
    return BGFX_INVALID_HANDLE;
}

bgfx::VertexBufferHandle FindResourceStaticVertexBufferHandle(const VertexBufferClass * vb)
{
    if (BgfxResourceEntry * entry = FindVertexBufferResourceEntry(vb))
    {
        if (bgfx::isValid(entry->vb))
        {
            return entry->vb;
        }
    }
    return BGFX_INVALID_HANDLE;
}

bgfx::IndexBufferHandle FindResourceStaticIndexBufferHandle(const IndexBufferClass * ib)
{
    if (BgfxResourceEntry * entry = FindIndexBufferResourceEntry(ib))
    {
        if (bgfx::isValid(entry->ib))
        {
            return entry->ib;
        }
    }
    return BGFX_INVALID_HANDLE;
}

void DestroyStaticVertexResource(BgfxResourceEntry & entry)
{
    if (!bgfx::isValid(entry.vb))
    {
        return;
    }
    if (g_draw.useStaticVB
        && bgfx::isValid(g_draw.staticVB)
        && g_draw.staticVB.idx == entry.vb.idx)
    {
        g_draw.staticVB = BGFX_INVALID_HANDLE;
        g_draw.useStaticVB = false;
    }
    bgfx::destroy(entry.vb);
    entry.vb = BGFX_INVALID_HANDLE;
}

void DestroyStaticIndexResource(BgfxResourceEntry & entry)
{
    if (!bgfx::isValid(entry.ib))
    {
        return;
    }
    if (g_draw.useStaticIB
        && bgfx::isValid(g_draw.staticIB)
        && g_draw.staticIB.idx == entry.ib.idx)
    {
        g_draw.staticIB = BGFX_INVALID_HANDLE;
        g_draw.useStaticIB = false;
    }
    bgfx::destroy(entry.ib);
    entry.ib = BGFX_INVALID_HANDLE;
}

// TheSuperHackers @bugfix bobtista 02/06/2026 Like DestroyStaticVertexResource but defers
// the bgfx::destroy by one frame. Used when a static-eligible buffer demotes to the dynamic
// path mid-frame: the immutable buffer may still be referenced by a draw recorded earlier
// this frame, so destroying it now triggers a "RefCount is 1 (expected 0)" warning.
void DeferDestroyStaticVertexResource(BgfxResourceEntry & entry)
{
    if (!bgfx::isValid(entry.vb))
    {
        return;
    }
    if (g_draw.useStaticVB
        && bgfx::isValid(g_draw.staticVB)
        && g_draw.staticVB.idx == entry.vb.idx)
    {
        g_draw.staticVB = BGFX_INVALID_HANDLE;
        g_draw.useStaticVB = false;
    }
    g_caches.deferredDestroyStaticVB.push_back(entry.vb);
    entry.vb = BGFX_INVALID_HANDLE;
}

void DeferDestroyStaticIndexResource(BgfxResourceEntry & entry)
{
    if (!bgfx::isValid(entry.ib))
    {
        return;
    }
    if (g_draw.useStaticIB
        && bgfx::isValid(g_draw.staticIB)
        && g_draw.staticIB.idx == entry.ib.idx)
    {
        g_draw.staticIB = BGFX_INVALID_HANDLE;
        g_draw.useStaticIB = false;
    }
    g_caches.deferredDestroyStaticIB.push_back(entry.ib);
    entry.ib = BGFX_INVALID_HANDLE;
}

// TheSuperHackers @perf bobtista 02/06/2026 FNV-1a 64-bit content hash used to detect
// byte-identical re-uploads of static-eligible buffers so the GPU buffer recreate can be
// skipped. size_bytes seeds the hash so a size change can never collide with a content match.
static uint64_t HashBufferContent(const void * data, unsigned int size_bytes)
{
    uint64_t hash = 1469598103934665603ULL ^ static_cast<uint64_t>(size_bytes);
    const unsigned char * bytes = static_cast<const unsigned char *>(data);
    for (unsigned int i = 0; i < size_bytes; ++i)
    {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool TryCaptureStaticVertexBuffer(const VertexBufferClass * vb,
                                  const void * data,
                                  unsigned int size_bytes)
{
    if (vb == nullptr || data == nullptr || !vb->Is_Backend_Static_Eligible())
    {
        return false;
    }
    BgfxResourceEntry * entry = FindVertexBufferResourceEntry(vb);
    if (entry == nullptr || bgfx::isValid(entry->dvb))
    {
        return false;
    }
    const uint32_t stride = vb->FVF_Info().Get_FVF_Size();
    const uint32_t buffer_bytes = static_cast<uint32_t>(vb->Get_Vertex_Count()) * stride;
    if (stride == 0 || buffer_bytes == 0 || size_bytes != buffer_bytes)
    {
        return false;
    }
    const uint64_t contentHash = HashBufferContent(data, size_bytes);
    if (bgfx::isValid(entry->vb))
    {
        if (entry->vbContentHash == contentHash)
        {
            return true;
        }
        // TheSuperHackers @perf bobtista 02/06/2026 Content changed, so this
        // static-eligible buffer is effectively dynamic. Drop the immutable buffer and
        // fall through to the in-place dynamic path, which reuses one native buffer for
        // the buffer's lifetime. Recreating an immutable buffer (and orphaning the old
        // one) every frame was the dominant source of the "RefCount is 1 (expected 0)"
        // leak warnings at shutdown. EnsureDynamicVertexBuffer marks entry->dvb valid,
        // so subsequent uploads skip this static path entirely.
        DeferDestroyStaticVertexResource(*entry);
        return false;
    }
    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vb->FVF_Info(), layout) || layout.getStride() != stride)
    {
        DestroyStaticVertexResource(*entry);
        return false;
    }

    bgfx::VertexBufferHandle h = bgfx::createVertexBuffer(bgfx::copy(data, size_bytes), layout);
    DestroyStaticVertexResource(*entry);
    if (!bgfx::isValid(h))
    {
        return false;
    }
    entry->vb = h;
    entry->vbContentHash = contentHash;
    return true;
}

bool TryCaptureStaticIndexBuffer(const IndexBufferClass * ib,
                                 const void * data,
                                 unsigned int size_bytes)
{
    if (ib == nullptr || data == nullptr || !ib->Is_Backend_Static_Eligible())
    {
        return false;
    }
    BgfxResourceEntry * entry = FindIndexBufferResourceEntry(ib);
    if (entry == nullptr || bgfx::isValid(entry->dib))
    {
        return false;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(ib->Get_Index_Count()) * sizeof(uint16_t);
    if (buffer_bytes == 0 || size_bytes != buffer_bytes)
    {
        return false;
    }
    const uint64_t contentHash = HashBufferContent(data, size_bytes);
    if (bgfx::isValid(entry->ib))
    {
        if (entry->ibContentHash == contentHash)
        {
            return true;
        }
        // TheSuperHackers @perf bobtista 02/06/2026 Content changed: demote to the in-place
        // dynamic path instead of recreating an immutable buffer every frame. See the
        // matching note in TryCaptureStaticVertexBuffer.
        DeferDestroyStaticIndexResource(*entry);
        return false;
    }
    bgfx::IndexBufferHandle h = bgfx::createIndexBuffer(bgfx::copy(data, size_bytes));
    DestroyStaticIndexResource(*entry);
    if (!bgfx::isValid(h))
    {
        return false;
    }
    entry->ib = h;
    entry->ibContentHash = contentHash;
    return true;
}

bgfx::DynamicVertexBufferHandle EnsureDynamicVertexBuffer(const VertexBufferClass * vb)
{
    const uint32_t num_verts = static_cast<uint32_t>(vb->Get_Vertex_Count());
    const uint32_t engine_stride = vb->FVF_Info().Get_FVF_Size();

    auto it = g_caches.vb.find(vb);
    if (it != g_caches.vb.end())
    {
        // TheSuperHackers @perf bobtista 02/06/2026 Grow-only reuse. The cached
        // num_verts is the bgfx buffer's CAPACITY. As long as the layout (stride) is
        // unchanged and the buffer is at least as large as the engine now needs, reuse
        // it and let the upload write only the live sub-range. The engine resizes these
        // dynamic buffers nearly every frame; recreating a GPU buffer each time wasted
        // CPU and (because bgfx keeps frames in flight) produced an unbounded stream of
        // "RefCount is 1 (expected 0)" destroy warnings. Only recreate to GROW, or when
        // the vertex layout changes.
        if (it->second.stride == engine_stride
            && it->second.num_verts >= num_verts
            && bgfx::isValid(it->second.handle))
        {
            MirrorDynamicVertexHandleToResource(vb, it->second.handle);
            return it->second.handle;
        }
        ClearDynamicVertexHandleFromResource(vb, it->second.handle);
        if (bgfx::isValid(it->second.handle))
        {
            // Defer the destroy of the grown-out handle by one frame: a draw recorded
            // earlier this frame may still reference it until bgfx::frame() executes.
            g_caches.deferredDestroyVB.push_back(it->second.handle);
        }
        g_caches.vb.erase(it);
    }

    if (num_verts == 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vb->FVF_Info(), layout))
    {
        return BGFX_INVALID_HANDLE;
    }
    // Mismatched stride (typically for skinned-mesh weighted-position FVFs
    // that BuildBgfxLayoutForFVF does not fully cover)
    // would create a too-small bgfx buffer and cause truncation
    // warnings + native buffer creation crashes when the engine writes
    // a larger per-vertex stride than bgfx allocated.
    const uint32_t layout_stride = layout.getStride();
    if (layout_stride == 0 || layout_stride != engine_stride)
    {
        static bool s_loggedVbStrideSkip = false;
        if (!s_loggedVbStrideSkip)
        {
            s_loggedVbStrideSkip = true;
            WWDEBUG_SAY(("[BgfxBackend] skip VB cache: layout stride=%u != "
                         "engine stride=%u (fvf=0x%x num_verts=%u) - unsupported FVF",
                         layout_stride, engine_stride,
                         vb->FVF_Info().Get_FVF(), num_verts));
        }
        BgfxVbCacheEntry e{ BGFX_INVALID_HANDLE, num_verts, engine_stride };
        g_caches.vb[vb] = e;
        MirrorDynamicVertexHandleToResource(vb, BGFX_INVALID_HANDLE);
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicVertexBufferHandle h = bgfx::createDynamicVertexBuffer(num_verts, layout);
    g_stats.dynamicVbAllocations++;
    BgfxVbCacheEntry e{ h, num_verts, engine_stride };
    g_caches.vb[vb] = e;
    MirrorDynamicVertexHandleToResource(vb, h);
    return h;
}

bgfx::DynamicIndexBufferHandle EnsureDynamicIndexBuffer(const IndexBufferClass * ib)
{
    const uint32_t num_indices = static_cast<uint32_t>(ib->Get_Index_Count());

    auto it = g_caches.ib.find(ib);
    if (it != g_caches.ib.end())
    {
        // TheSuperHackers @perf bobtista 02/06/2026 Grow-only reuse; cached num_indices
        // is the buffer CAPACITY. See the matching note in EnsureDynamicVertexBuffer.
        if (it->second.num_indices >= num_indices && bgfx::isValid(it->second.handle))
        {
            MirrorDynamicIndexHandleToResource(ib, it->second.handle);
            return it->second.handle;
        }
        ClearDynamicIndexHandleFromResource(ib, it->second.handle);
        if (bgfx::isValid(it->second.handle))
        {
            // Defer the destroy of the grown-out handle by one frame.
            g_caches.deferredDestroyIB.push_back(it->second.handle);
        }
        g_caches.ib.erase(it);
    }
    if (num_indices == 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicIndexBufferHandle h = bgfx::createDynamicIndexBuffer(num_indices);
    g_stats.dynamicIbAllocations++;
    BgfxIbCacheEntry e{ h, num_indices };
    g_caches.ib[ib] = e;
    MirrorDynamicIndexHandleToResource(ib, h);
    return h;
}
}

// TheSuperHackers @feature bobtista 17/04/2026 Shroud texture capture for
// bgfx. The shroud destination texture is POOL_DEFAULT, which the bgfx
// texture-upload path in EnsureBgfxTexture skips (cannot lock). Instead,
// the shroud system pushes its system-memory pixel data here every frame
// after CopyRects. We create a bgfx texture on first call and updateTexture2D
// on subsequent frames, storing the handle in g_caches.texture keyed by the
// engine's destination TextureClass so EnsureBgfxTexture finds it on lookup
// before reaching the POOL_DEFAULT early-out.

void BgfxBackend::Upload_Vertex_Buffer_Data(const VertexBufferClass * vb,
                                      const void * data,
                                      unsigned int size_bytes)
{
    if (!g_device.initialized || vb == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    // Guard: clamp write to the dynamic VB allocation so bgfx's
    // E_INVALIDARG path at renderer_d3d11.cpp:4038 (CreateBuffer for
    // staging) never fires.
    const uint32_t stride = vb->FVF_Info().Get_FVF_Size();
    const uint32_t buffer_bytes = static_cast<uint32_t>(vb->Get_Vertex_Count()) * stride;
    if (stride == 0 || buffer_bytes == 0 || size_bytes > buffer_bytes)
    {
        static bool s_loggedVbCaptureSkip = false;
        if (!s_loggedVbCaptureSkip)
        {
            s_loggedVbCaptureSkip = true;
            WWDEBUG_SAY(("[BgfxBackend] skip VB full-upload: "
                         "size_bytes=%u stride=%u total=%u",
                         size_bytes, stride, buffer_bytes));
        }
        return;
    }
    if (TryCaptureStaticVertexBuffer(vb, data, size_bytes))
    {
        return;
    }

    bgfx::DynamicVertexBufferHandle h = EnsureDynamicVertexBuffer(vb);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    LogBgfxBufferUpdate("vb-full", vb, data, 0, size_bytes, h.idx, mem);
    bgfx::update(h, 0, mem);
}

void BgfxBackend::Upload_Index_Buffer_Data(const IndexBufferClass * ib,
                                     const void * data,
                                     unsigned int size_bytes)
{
    if (!g_device.initialized || ib == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(ib->Get_Index_Count()) * sizeof(uint16_t);
    if (buffer_bytes == 0 || size_bytes > buffer_bytes)
    {
        static bool s_loggedIbCaptureSkip = false;
        if (!s_loggedIbCaptureSkip)
        {
            s_loggedIbCaptureSkip = true;
            WWDEBUG_SAY(("[BgfxBackend] skip IB full-upload: "
                         "size_bytes=%u total=%u", size_bytes, buffer_bytes));
        }
        return;
    }
    if (TryCaptureStaticIndexBuffer(ib, data, size_bytes))
    {
        return;
    }
    bgfx::DynamicIndexBufferHandle h = EnsureDynamicIndexBuffer(ib);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    LogBgfxBufferUpdate("ib-full", ib, data, 0, size_bytes, h.idx, mem);
    bgfx::update(h, 0, mem);
}

void BgfxBackend::Upload_Vertex_Buffer_Sub_Range(const VertexBufferClass * vb,
                                           const void * data,
                                           unsigned int start_vertex,
                                           unsigned int size_bytes)
{
    if (!g_device.initialized || vb == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    // Guard: total update must fit in the dynamic VB allocation.
    const uint32_t stride = vb->FVF_Info().Get_FVF_Size();
    if (stride == 0)
    {
        static bool s_loggedVbStrideZero = false;
        if (!s_loggedVbStrideZero)
        {
            s_loggedVbStrideZero = true;
            WWDEBUG_SAY(("[BgfxBackend] skip VB upload: stride=0 vb=%p", vb));
        }
        return;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(vb->Get_Vertex_Count()) * stride;
    const uint64_t end_byte = static_cast<uint64_t>(start_vertex) * stride + size_bytes;
    if (end_byte > buffer_bytes)
    {
        static bool s_loggedVbSubRangeOor = false;
        if (!s_loggedVbSubRangeOor)
        {
            s_loggedVbSubRangeOor = true;
            WWDEBUG_SAY(("[BgfxBackend] skip VB upload: out-of-range "
                         "start_vert=%u size_bytes=%u stride=%u total=%u",
                         start_vertex, size_bytes, stride, buffer_bytes));
        }
        return;
    }
    if (BgfxResourceEntry * entry = FindVertexBufferResourceEntry(vb))
    {
        DestroyStaticVertexResource(*entry);
    }
    bgfx::DynamicVertexBufferHandle h = EnsureDynamicVertexBuffer(vb);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    LogBgfxBufferUpdate("vb-range", vb, data, start_vertex, size_bytes, h.idx, mem);
    bgfx::update(h, start_vertex, mem);

}

void BgfxBackend::Upload_Index_Buffer_Sub_Range(const IndexBufferClass * ib,
                                          const void * data,
                                          unsigned int start_index,
                                          unsigned int size_bytes)
{
    if (!g_device.initialized || ib == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(ib->Get_Index_Count()) * sizeof(uint16_t);
    const uint64_t end_byte = static_cast<uint64_t>(start_index) * sizeof(uint16_t) + size_bytes;
    if (end_byte > buffer_bytes)
    {
        static bool s_loggedIbSubRangeOor = false;
        if (!s_loggedIbSubRangeOor)
        {
            s_loggedIbSubRangeOor = true;
            WWDEBUG_SAY(("[BgfxBackend] skip IB upload: out-of-range "
                         "start_idx=%u size_bytes=%u total=%u",
                         start_index, size_bytes, buffer_bytes));
        }
        return;
    }
    if (BgfxResourceEntry * entry = FindIndexBufferResourceEntry(ib))
    {
        DestroyStaticIndexResource(*entry);
    }
    bgfx::DynamicIndexBufferHandle h = EnsureDynamicIndexBuffer(ib);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    LogBgfxBufferUpdate("ib-range", ib, data, start_index, size_bytes, h.idx, mem);
    bgfx::update(h, start_index, mem);

}

// TheSuperHackers @refactor bobtista 11/04/2026 Sorted draw pass routing. Begin/End flip
// the flag that routes SubmitEngineDraw to kBgfxEngineSortView; Capture stores the
// per-batch world as sortView^T * sortWorld^T directly in bgfx column-major layout.

void BgfxBackend::Begin_Sorted_Batch_Pass()
{
    g_views.inSortFlush = true;
    if (!g_frame.sortProjCaptured)
    {
        std::memcpy(g_frame.sortProj, g_frame.proj, sizeof(g_frame.sortProj));
        g_frame.sortProjCaptured = true;
    }
}

void BgfxBackend::End_Sorted_Batch_Pass()
{
    g_views.inSortFlush = false;
    g_views.sortedBatchDrawFlags = RB_SORTED_DRAW_NONE;
}

static void CaptureSortedBatchTransformsForBgfx(const Matrix4x4 & sortWorld,
                                                const Matrix4x4 & sortView)
{
    // Compute the legacy row-major product sortWorld * sortView, then store
    // it as row-major float[16] (r*4+c). bgfx native backends interpret the
    // raw bytes as column-major HLSL float4x4, which makes mul(M, v) use
    // the ROWS of our stored matrix — matching D3D's row-vector convention.
    // The previous [c*4+r] storage put the translation at the W component
    // of each column (indices 3,7,11) instead of row 3 (indices 12,13,14),
    // which works for identity transforms (particles) but breaks for mesh
    // transforms with translation (helicopter rotors).
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                s += sortWorld[r][k] * sortView[k][c];
            }
            g_frame.sortWorld[r * 4 + c] = s;
            // Store raw model (no camera view baked in)
            // for shadow caster submissions.
        }
    }
    // Store raw sortWorld (model-to-world only) in bgfx column-major form.
    // This is used when sorted world decals need to render through a normal
    // camera view rather than the pre-view-multiplied sort view.
    // TheSuperHackers @bugfix bobtista 17/05/2026 sortWorldRaw must match the
    // bgfx HLSL column-vector convention (D3D row vectors transposed), same as
    // sortWorld above. Storing as [c*4+r] left the matrix transposed relative
    // to what bgfx::setTransform expects, so the rotor-blur draw's world
    // transform applied incorrectly: vertices wobbled around the hub instead of
    // rotating with it. Use [r*4+c] to match sortWorld's layout.
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            g_frame.sortWorldRaw[r * 4 + c] = sortWorld[r][c];
        }
    }
}

void BgfxBackend::Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & state)
{
    g_views.sortedBatchDrawFlags = state.draw_flags;
    if (state.shader != nullptr)
    {
        Set_Shader(*state.shader);
    }
    Set_Material(state.material);
    for (unsigned i = 0; i < RB_MAX_TEXTURE_STAGES; ++i)
    {
        Set_Texture(i, state.textures[i]);
    }
    if (state.world != nullptr && state.view != nullptr)
    {
        CaptureSortedBatchTransformsForBgfx(*state.world, *state.view);
    }
    for (int i = 0; i < 4; ++i)
    {
        const RenderBackendLight & light = state.lights.lights[i];
        if (state.lights.enabled[i])
        {
            g_draw.lightDirs[i][0] = -light.direction[0];
            g_draw.lightDirs[i][1] = -light.direction[1];
            g_draw.lightDirs[i][2] = -light.direction[2];
            g_draw.lightDirs[i][3] = 1.0f;
            g_draw.lightColors[i][0] = light.diffuse[0];
            g_draw.lightColors[i][1] = light.diffuse[1];
            g_draw.lightColors[i][2] = light.diffuse[2];
            g_draw.lightColors[i][3] = 1.0f;
            g_draw.lightAmbients[i][0] = light.ambient[0];
            g_draw.lightAmbients[i][1] = light.ambient[1];
            g_draw.lightAmbients[i][2] = light.ambient[2];
            g_draw.lightAmbients[i][3] = 1.0f;
            g_draw.lightParams[i][0] = 0.0f;
            g_draw.lightParams[i][1] = 0.0f;
            g_draw.lightParams[i][2] = 0.0f;
            g_draw.lightParams[i][3] = 1.0f;
        }
        else
        {
            g_draw.lightDirs[i][3] = 0.0f;
            g_draw.lightParams[i][3] = 0.0f;
        }
    }
}

void BgfxBackend::Set_Point_Group_Render_Active(bool active)
{
    g_views.pointGroupRenderActive = active;
}

void BgfxBackend::Set_Streak_Render_Active(bool active)
{
    g_views.streakRenderActive = active;
}

static const char * TextureDebugName(TextureBaseClass * texture);
static bool ContainsCaseInsensitive(const char *haystack, const char *needle);

void BgfxBackend::Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & state)
{
    // Transitional boundary for sorted replay. SortingRenderer snapshots a
    // full fixed-function state so it can replay translucent geometry later.
    // bgfx still mirrors draw state into FixedFunctionState for that snapshot
    // today; future phases should make that state shape backend-neutral too.
    FixedFunctionState::Capture_Render_State(state);
    if (g_views.pointGroupRenderActive)
    {
        state.sorted_draw_flags |= RB_SORTED_DRAW_POINT_GROUP;
    }
    if (g_views.streakRenderActive)
    {
        state.sorted_draw_flags |= RB_SORTED_DRAW_STREAK;
    }

    // TheSuperHackers @bugfix bobtista 17/05/2026 These sorted meshes are authored in local
    // model space, but FixedFunctionState's world is hard-wired to identity on bgfx; capture
    // the live per-mesh world so the replay places and rotates them correctly.
    const char *texName = TextureDebugName(g_draw.sourceTextures[0]);
    if (texName != nullptr
        && (ContainsCaseInsensitive(texName, "avcomanche_p")
            || ContainsCaseInsensitive(texName, "ubsnkatak_01")
            || ContainsCaseInsensitive(texName, "coplight")))
    {
        FixedFunctionState::Transform_Matrix(
            static_cast<unsigned>(RB_TRANSFORM_WORLD), state.world);
    }
}

void BgfxBackend::Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & state)
{
    (void)state;
}

void BgfxBackend::Release_Legacy_Render_State_For_Sorted_Draw()
{
    FixedFunctionState::Release_Render_State();
}

// TheSuperHackers @refactor bobtista 26/04/2026 Shared submit helpers used by
// both Submit_Sorted_Draw and SubmitEngineDraw to avoid duplicated blocks.
static uint64_t ApplyCullModeOverride(uint64_t state)
{
    CullMode cullMode = static_cast<CullMode>(FixedFunctionState::Cull_Mode(RB_CULL_NONE));
    state &= ~(BGFX_STATE_CULL_CW | BGFX_STATE_CULL_CCW);
    if (cullMode == RB_CULL_CW)
    {
        state |= BGFX_STATE_CULL_CW;
    }
    else if (cullMode == RB_CULL_CCW)
    {
        state |= BGFX_STATE_CULL_CCW;
    }
    return state;
}

static uint64_t ApplyColorWriteOverride(uint64_t state)
{
    if (g_overrides.colorWriteOverride >= 0)
    {
        state &= ~(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        state |= static_cast<uint64_t>(g_overrides.colorWriteOverride);
    }
    return state;
}

static uint64_t ApplyBlendEquation(uint64_t state)
{
    state &= ~BGFX_STATE_BLEND_EQUATION_MASK;
    if ((state & BGFX_STATE_BLEND_MASK) != 0)
    {
        state |= g_draw.blendEquationBits;
    }
    return state;
}

static bool IsOpaqueBlend(BlendFactor src, BlendFactor dest)
{
    return src == RB_BLEND_ONE && dest == RB_BLEND_ZERO;
}

static uint64_t ApplyBlendState(uint64_t state)
{
    state &= ~(BGFX_STATE_BLEND_MASK | BGFX_STATE_BLEND_EQUATION_MASK);
    const bool blendEnabled = g_overrides.blendEnableActive
        ? g_overrides.blendEnableValue
        : g_draw.alphaBlendEnabled;
    if (blendEnabled)
    {
        const uint64_t blendBits = g_overrides.blendActive
            ? g_overrides.blendBits
            : g_draw.blendFuncBits;
        state |= blendBits;
        state |= g_draw.blendEquationBits;
    }
    return state;
}

static uint64_t ApplyDepthState(uint64_t state)
{
    state &= ~(BGFX_STATE_DEPTH_TEST_MASK | BGFX_STATE_WRITE_Z);
    if (g_draw.depthTestEnabled)
    {
        state |= g_draw.depthFuncBits;
        if (g_draw.depthWriteEnabled)
        {
            state |= BGFX_STATE_WRITE_Z;
        }
    }
    return state;
}

static uint64_t GetEffectiveDrawState()
{
    uint64_t state = (g_draw.state != 0)
        ? g_draw.state
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    state = ApplyDepthState(state);
    state = ApplyBlendState(state);
    return state;
}

static bool LegacyStencilShadowsEnabled()
{
    return BgfxStencilShadowsEnabled()
        || std::getenv("GGC_ENABLE_LEGACY_STENCIL_SHADOWS") != nullptr;
}

static bool ShouldLogBgfxStencilShadows()
{
    return std::getenv("GGC_STENCIL_SHADOW_DIAG") != nullptr
        || std::getenv("GGC_SHADOW_PATH_DIAG") != nullptr;
}

static bool BgfxPreMeshStencilShadows()
{
    // bgfx view order is global, not call-order based. Submitting the legacy
    // stencil volumes into the engine view preserves the W3D call order used
    // by the bgfx path: terrain/projected decals first, stencil darken next,
    // opaque meshes later. That keeps volume shadows on terrain without
    // multiplying the lighting on units/buildings/effects.
    return std::getenv("GGC_BGFX_LEGACY_POSTMESH_STENCIL_SHADOWS") == nullptr;
}

static bgfx::ViewId BgfxShadowVolumeSubmitView()
{
    return BgfxPreMeshStencilShadows() ? kBgfxEngineView : kBgfxShadowVolumeView;
}

static uint64_t BgfxShadowVolumeDepthState()
{
    const char *depth = std::getenv("GGC_BGFX_STENCIL_DEPTH");
    if (depth != nullptr && std::strcmp(depth, "less") == 0)
    {
        return BGFX_STATE_DEPTH_TEST_LESS;
    }
    if (depth != nullptr && std::strcmp(depth, "always") == 0)
    {
        return BGFX_STATE_DEPTH_TEST_ALWAYS;
    }
    return BGFX_STATE_DEPTH_TEST_LEQUAL;
}

static bool BgfxTwoSidedStencilVolumes()
{
    return std::getenv("GGC_BGFX_STENCIL_TWO_SIDED") != nullptr;
}

static unsigned BgfxShadowCullModeBits()
{
    // Use the same face selection as the W3D/DX8 shadow-volume pass. The
    // earlier bgfx-only inversion made the default z-pass counts cancel out
    // on useful receivers, leaving vehicles and aircraft without shadows.
    if (std::getenv("GGC_BGFX_STENCIL_INVERT_CULL") == nullptr)
    {
        return g_draw.cullModeBits;
    }
    if (g_draw.cullModeBits == 1)
    {
        return 2;
    }
    if (g_draw.cullModeBits == 2)
    {
        return 1;
    }
    return g_draw.cullModeBits;
}

static void BindShadowVolumeBiasUniform()
{
    if (!bgfx::isValid(g_uniforms.uShadowBias))
    {
        return;
    }

    float bias[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (std::getenv("GGC_BGFX_STENCIL_CLAMP_CLIP") != nullptr)
    {
        bias[1] = 1.0f;
    }
    bgfx::setUniform(g_uniforms.uShadowBias, bias);
}

static bool BgfxSwapTwoSidedStencilVolumeOps()
{
    const char *algo = std::getenv("GGC_BGFX_STENCIL_ALGO");
    return algo != nullptr && std::strcmp(algo, "zpass-swap") == 0;
}

static void LogBgfxStencilShadowEvent(const char *event, const char *reason,
                                      unsigned countA, unsigned countB)
{
    if (!ShouldLogBgfxStencilShadows())
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_stencil_shadow_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s frame=%u enabled=%d mode=%d active=%d submits=%u a=%u b=%u reason=%s\n",
                     event,
                     g_stats.frameIndex,
                     LegacyStencilShadowsEnabled() ? 1 : 0,
                     static_cast<int>(GetBgfxShadowMode()),
                     g_views.shadowVolumeActive ? 1 : 0,
                     g_stats.shadowVolumeSubmits,
                     countA,
                     countB,
                     reason != nullptr ? reason : "");
        std::fclose(diag);
    }
}

static bool IsStandardAlphaBlend(uint64_t state)
{
    const uint64_t kAlphaSA_ISA = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                        BGFX_STATE_BLEND_INV_SRC_ALPHA);
    return (state & BGFX_STATE_BLEND_MASK) == kAlphaSA_ISA;
}

static bool IsOneOneAdditiveBlend(uint64_t state)
{
    const uint64_t kAddOneOne = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                                      BGFX_STATE_BLEND_ONE);
    return (state & BGFX_STATE_BLEND_MASK) == kAddOneOne;
}

static bool IsAnyAdditiveBlend(uint64_t state)
{
    const uint64_t blend = state & BGFX_STATE_BLEND_MASK;
    const uint64_t kOneOne = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                                    BGFX_STATE_BLEND_ONE);
    const uint64_t kSaOne  = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                    BGFX_STATE_BLEND_ONE);
    return blend == kOneOne || blend == kSaOne;
}

static bool IsMultiplicativeBlend(uint64_t state)
{
    const uint64_t blend = state & BGFX_STATE_BLEND_MASK;
    const uint64_t kZeroSrcColor = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO,
                                                          BGFX_STATE_BLEND_SRC_COLOR);
    const uint64_t kDstColorZero = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR,
                                                          BGFX_STATE_BLEND_ZERO);
    return blend == kZeroSrcColor || blend == kDstColorZero;
}

static bool IsSoftParticleCandidate(uint64_t state)
{
    // Soft depth fading is only appropriate for particle-style sorted quads.
    // Some sorted alpha draws are world decals/material passes (for example
    // command-center floor emblems) that sit directly on opaque geometry and
    // must not be faded out against the scene depth.
    return IsStandardAlphaBlend(state) && g_draw.tssOps0[0] < 1.5f;
}

static bool IsSortedMaterialDecal(uint64_t state)
{
    return IsStandardAlphaBlend(state)
        && !IsSoftParticleCandidate(state)
        && g_draw.tssOps0[0] > 2.5f && g_draw.tssOps0[0] < 3.5f
        && g_draw.tssOps0[3] > 0.5f;
}

static const char * TextureDebugName(TextureBaseClass * texture);
static bool ContainsCaseInsensitive(const char *haystack, const char *needle);

static bool IsSortedAlphaDepthDecal(uint64_t state)
{
    const unsigned particleFlags = RB_SORTED_DRAW_POINT_GROUP | RB_SORTED_DRAW_STREAK;
    return g_views.inSortFlush
        && (g_views.sortedBatchDrawFlags & particleFlags) == 0
        && IsStandardAlphaBlend(state)
        && !IsSoftParticleCandidate(state)
        && (state & BGFX_STATE_WRITE_Z) == 0
        && g_draw.tssOps0[0] > 2.5f && g_draw.tssOps0[0] < 3.5f
        && g_draw.tssOps0[1] > 2.5f && g_draw.tssOps0[1] < 3.5f
        && g_draw.tssOps0[2] < 0.5f
        && g_draw.tssOps0[3] < 0.5f
        && (g_draw.texcoordSelect2[0] < 0.5f
            || ContainsCaseInsensitive(TextureDebugName(g_draw.sourceTextures[0]), "ubsnkatak_01"));
}

static bool IsSortedParticleEffect(uint64_t state)
{
    const unsigned particleFlags = RB_SORTED_DRAW_POINT_GROUP | RB_SORTED_DRAW_STREAK;
    return g_views.inSortFlush
        && (g_views.sortedBatchDrawFlags & particleFlags) != 0
        && (state & BGFX_STATE_BLEND_MASK) != 0;
}

static bool IsSneakAttackAlphaDepthDecal(uint64_t state)
{
    return IsSortedAlphaDepthDecal(state)
        && ContainsCaseInsensitive(TextureDebugName(g_draw.sourceTextures[0]), "ubsnkatak_01");
}

static bool IsSneakAttackCoplanarSurface()
{
    const char * name = TextureDebugName(g_draw.sourceTextures[0]);
    return name != nullptr
        && ContainsCaseInsensitive(name, "ubsnkatak_0")
        && !ContainsCaseInsensitive(name, "ubsnkatak_01");
}

static bool ShouldApplySubmittedNormalBias(uint64_t state)
{
    return IsSortedMaterialDecal(state)
        || IsSortedAlphaDepthDecal(state)
        || IsSneakAttackCoplanarSurface();
}

static bool IsSortedRotorBlur(uint64_t state)
{
    return g_views.inSortFlush
        && IsStandardAlphaBlend(state)
        && ((g_draw.tssOps0[0] > 0.5f && g_draw.tssOps0[0] < 1.5f)
            || (g_draw.tssOps0[0] > 2.5f && g_draw.tssOps0[0] < 3.5f))
        && ((g_draw.tssOps0[1] > 0.5f && g_draw.tssOps0[1] < 1.5f)
            || (g_draw.tssOps0[1] > 2.5f && g_draw.tssOps0[1] < 3.5f))
        && g_draw.tssOps0[2] < 0.5f
        && g_draw.tssOps0[3] < 0.5f
        && ContainsCaseInsensitive(TextureDebugName(g_draw.sourceTextures[0]), "avcomanche_p");
}

// TheSuperHackers @bugfix bobtista 25/05/2026 Police-car lightbar glow meshes
// (CopLight*.tga) live in model space and rely on the per-mesh world transform
// the same way the Chinook rotor blur and Sneak Attack dirt plane do. Routing
// them through the sort view, whose pre-view-multiplied matrix and Z-biased
// projection are tuned for camera-facing particles, washes them out and leaves
// the glow dim/invisible. Route through the engine view with the raw model
// world so each animated coplight quad lands at the lightbar with normal
// brightness, exactly like the other model-space sorted meshes above.
static bool IsSortedCopLightSprite(uint64_t /*state*/)
{
    return g_views.inSortFlush
        && ContainsCaseInsensitive(TextureDebugName(g_draw.sourceTextures[0]), "coplight");
}

static bool ShouldForceUnlitForBakedColorDraw(uint64_t state)
{
    return IsAnyAdditiveBlend(state)
        || IsSortedParticleEffect(state)
        || IsSoftParticleCandidate(state)
        || IsSortedMaterialDecal(state)
        || IsSortedAlphaDepthDecal(state)
        || IsSortedRotorBlur(state);
}

static uint64_t ApplySortedMaterialDecalDepthState(uint64_t state)
{
    if (!IsSortedMaterialDecal(state) && !IsSortedAlphaDepthDecal(state))
    {
        return state;
    }

    // W3DBibBuffer's legacy shader is PASS_ALWAYS because the DX8 path relied
    // on draw order plus fixed-function z-bias for driveway bibs/faction
    // emblems. In bgfx these draws are routed through the sorted view after
    // units, so PASS_ALWAYS makes the ground decal alpha-blend over bulldozers
    // as they exit the command center. Keep the decal z-bias that prevents
    // coplanar ground fighting, but still test against scene depth so vehicles
    // and other opaque meshes occlude the decal.
    state &= ~BGFX_STATE_DEPTH_TEST_MASK;
    state |= BGFX_STATE_DEPTH_TEST_LEQUAL;
    state &= ~BGFX_STATE_WRITE_Z;
    return state;
}

static uint64_t ApplyDelayedObjectShroudDepthState(uint64_t state)
{
    // The object shroud overlay is submitted in a later bgfx view so building
    // detail passes cannot draw over it. Keep DX8's depth-equal behavior:
    // alpha-tested base pixels write object depth, while transparent card
    // pixels leave terrain depth behind and must not receive object shroud.
    state &= ~BGFX_STATE_DEPTH_TEST_MASK;
    state |= BGFX_STATE_DEPTH_TEST_EQUAL;
    state &= ~BGFX_STATE_WRITE_Z;
    return state;
}

static bool ShouldHideMissingTextureForCurrentDraw(uint64_t state)
{
    // Keep missing textures visible on opaque geometry so bad assets are still
    // diagnosable. In blended/sorted/effect passes, the checker texture becomes
    // the artifact itself, e.g. missing spy-satellite smoke particles drawing
    // black radiating blocks.
    return (state & BGFX_STATE_BLEND_MASK) != 0
        || g_views.inSortFlush
        || g_views.effectOverlayActive;
}

static bool ShouldSkipHiddenMissingTextureDraw(uint64_t state)
{
    if (!ShouldHideMissingTextureForCurrentDraw(state))
    {
        return false;
    }

    const bool usesStage1 = g_draw.tssOps0[2] > 0.5f || g_draw.tssOps0[3] > 0.5f;
    const bool usesLateStages = usesStage1 && g_draw.texcoordSelect[1] > 0.5f;
    if (g_draw.textureIsMissing[0]
        || (usesStage1 && g_draw.textureIsMissing[1])
        || (usesLateStages && (g_draw.textureIsMissing[2] || g_draw.textureIsMissing[3])))
    {
        return true;
    }
    return false;
}

static bool IsMissingOrUnavailableTexture(TextureBaseClass * texture, bgfx::TextureHandle handle)
{
    if (texture == nullptr)
    {
        return false;
    }
    if (texture->Is_Missing_Texture())
    {
        return true;
    }
    return !bgfx::isValid(handle);
}

static bool ShouldLogBgfxShroudPass()
{
    return std::getenv("GGC_BGFX_SHROUD_PASS_DIAG") != nullptr;
}

static bool ShouldLogBgfxSortedDecals()
{
    return std::getenv("GGC_BGFX_SORTED_DECAL_DIAG") != nullptr;
}

static bool ShouldLogBgfxRevealDiag()
{
    return std::getenv("GGC_BGFX_REVEAL_DIAG") != nullptr;
}

static bool ShouldLogBgfxRevealDiagVerbose()
{
    return std::getenv("GGC_BGFX_REVEAL_DIAG_VERBOSE") != nullptr;
}

static bool ShouldLogBgfxEffectSubmitDiag()
{
    return std::getenv("GGC_BGFX_EFFECT_SUBMIT_DIAG") != nullptr;
}

static uint32_t GetCurrentStageSamplerFlags(unsigned stage);

static bool ShouldAllowBgfxDiagnosticDrawOverrides()
{
    return std::getenv("GGC_BGFX_ENABLE_DIAGNOSTIC_OVERRIDES") != nullptr;
}

static const char * TextureDebugName(TextureBaseClass * texture)
{
    TextureClass * tex2d = texture ? texture->As_TextureClass() : nullptr;
    return tex2d ? tex2d->Get_Full_Path().str() : "(null)";
}

static bool ContainsCaseInsensitive(const char *haystack, const char *needle)
{
    if (haystack == nullptr || needle == nullptr || *needle == '\0')
    {
        return false;
    }

    const size_t needleLen = std::strlen(needle);
    for (const char *p = haystack; *p != '\0'; ++p)
    {
        if (strnicmp(p, needle, needleLen) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool IsRevealRelevantTextureName(const char *name)
{
    return ContainsCaseInsensitive(name, "exgrid")
        || ContainsCaseInsensitive(name, "exredsmokepuff")
        || ContainsCaseInsensitive(name, "smoke")
        || ContainsCaseInsensitive(name, "shadow");
}

static bool IsRevealGridTexture(TextureBaseClass *texture)
{
    return ContainsCaseInsensitive(TextureDebugName(texture), "exgrid");
}

static bool IsEffectTextureName(const char *name)
{
    return ContainsCaseInsensitive(name, "ex")
        || ContainsCaseInsensitive(name, "fire")
        || ContainsCaseInsensitive(name, "missile")
        || ContainsCaseInsensitive(name, "flame")
        || ContainsCaseInsensitive(name, "smoke")
        || ContainsCaseInsensitive(name, "noise");
}

static void LogBgfxEffectSubmit(const char *event,
                                bgfx::ViewId view,
                                unsigned short polygonCount,
                                unsigned short vertexCount,
                                uint64_t state,
                                const char *decision)
{
    if (!ShouldLogBgfxEffectSubmitDiag())
    {
        return;
    }

    const char *tex0 = TextureDebugName(g_draw.sourceTextures[0]);
    const char *tex1 = TextureDebugName(g_draw.sourceTextures[1]);
    const char *tex2 = TextureDebugName(g_draw.sourceTextures[2]);
    const char *tex3 = TextureDebugName(g_draw.sourceTextures[3]);
    const bool effectTex = IsEffectTextureName(tex0)
        || IsEffectTextureName(tex1)
        || IsEffectTextureName(tex2)
        || IsEffectTextureName(tex3);
    const bool interestingState = IsAnyAdditiveBlend(state)
        || IsStandardAlphaBlend(state)
        || view == kBgfxEngineSortView
        || view == kBgfxEffectOverlayView;
    if (!effectTex && !interestingState)
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_bgfx_effect_submit_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s frame=%u view=%u polys=%u verts=%u decision=%s raw=0x%llx state=0x%llx blend=0x%llx depth=0x%llx wz=%d inSort=%d effect=%d programValid=%d texValid=(%d,%d,%d,%d) missing=(%d,%d,%d,%d) tex=(%s|%s|%s|%s) tss0=(%.1f,%.1f,%.1f,%.1f) tss1=(%.1f,%.1f,%.1f,%.1f) texSel=(%.1f,%.1f,%.1f,%.1f) texSel2=(%.1f,%.1f,%.1f,%.1f) lighting=%.1f\n",
                     event,
                     g_stats.frameIndex,
                     static_cast<unsigned>(view),
                     static_cast<unsigned>(polygonCount),
                     static_cast<unsigned>(vertexCount),
                     decision ? decision : "",
                     static_cast<unsigned long long>(g_draw.state),
                     static_cast<unsigned long long>(state),
                     static_cast<unsigned long long>(state & BGFX_STATE_BLEND_MASK),
                     static_cast<unsigned long long>(state & BGFX_STATE_DEPTH_TEST_MASK),
                     (state & BGFX_STATE_WRITE_Z) != 0 ? 1 : 0,
                     g_views.inSortFlush ? 1 : 0,
                     g_views.effectOverlayActive ? 1 : 0,
                     bgfx::isValid(g_draw.program) ? 1 : 0,
                     bgfx::isValid(g_draw.tex[0]) ? 1 : 0,
                     bgfx::isValid(g_draw.tex[1]) ? 1 : 0,
                     bgfx::isValid(g_draw.tex[2]) ? 1 : 0,
                     bgfx::isValid(g_draw.tex[3]) ? 1 : 0,
                     g_draw.textureIsMissing[0] ? 1 : 0,
                     g_draw.textureIsMissing[1] ? 1 : 0,
                     g_draw.textureIsMissing[2] ? 1 : 0,
                     g_draw.textureIsMissing[3] ? 1 : 0,
                     tex0, tex1, tex2, tex3,
                     g_draw.tssOps0[0], g_draw.tssOps0[1],
                     g_draw.tssOps0[2], g_draw.tssOps0[3],
                     g_draw.tssOps1[0], g_draw.tssOps1[1],
                     g_draw.tssOps1[2], g_draw.tssOps1[3],
                     g_draw.texcoordSelect[0], g_draw.texcoordSelect[1],
                     g_draw.texcoordSelect[2], g_draw.texcoordSelect[3],
                     g_draw.texcoordSelect2[0], g_draw.texcoordSelect2[1],
                     g_draw.texcoordSelect2[2], g_draw.texcoordSelect2[3],
                     g_draw.lightingEnabled[0]);
        std::fclose(diag);
    }
}

static void LogBgfxRevealDraw(const char *event,
                              bgfx::ViewId view,
                              unsigned short polygonCount,
                              unsigned short vertexCount,
                              uint64_t state,
                              const char *decision)
{
    if (!ShouldLogBgfxRevealDiag())
    {
        return;
    }

    const char *tex0 = TextureDebugName(g_draw.sourceTextures[0]);
    const char *tex1 = TextureDebugName(g_draw.sourceTextures[1]);
    const char *tex2 = TextureDebugName(g_draw.sourceTextures[2]);
    const char *tex3 = TextureDebugName(g_draw.sourceTextures[3]);
    const bool relevantTexture =
        IsRevealRelevantTextureName(tex0)
        || IsRevealRelevantTextureName(tex1)
        || IsRevealRelevantTextureName(tex2)
        || IsRevealRelevantTextureName(tex3);
    const bool relevantState =
        g_views.projectedShadowDecalActive
        || g_views.projectedDecalMode != RB_PROJECTED_DECAL_NONE
        || g_draw.textureIsMissing[0]
        || g_draw.textureIsMissing[1]
        || g_draw.textureIsMissing[2]
        || g_draw.textureIsMissing[3]
        || (ShouldLogBgfxRevealDiagVerbose() && IsAnyAdditiveBlend(state))
        || IsMultiplicativeBlend(state);
    if (!relevantTexture && !relevantState)
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_bgfx_reveal_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s frame=%u view=%u polys=%u verts=%u decision=%s raw=0x%llx state=0x%llx blend=0x%llx depth=0x%llx wz=%d sort=%d effect=%d decalMode=%u missing=(%d,%d,%d,%d) tex=(%s|%s|%s|%s) tss0=(%.1f,%.1f,%.1f,%.1f) tss1=(%.1f,%.1f,%.1f,%.1f) texSel=(%.1f,%.1f,%.1f,%.1f) texSel2=(%.1f,%.1f,%.1f,%.1f)\n",
                     event,
                     g_stats.frameIndex,
                     static_cast<unsigned>(view),
                     static_cast<unsigned>(polygonCount),
                     static_cast<unsigned>(vertexCount),
                     decision ? decision : "",
                     static_cast<unsigned long long>(g_draw.state),
                     static_cast<unsigned long long>(state),
                     static_cast<unsigned long long>(state & BGFX_STATE_BLEND_MASK),
                     static_cast<unsigned long long>(state & BGFX_STATE_DEPTH_TEST_MASK),
                     (state & BGFX_STATE_WRITE_Z) != 0 ? 1 : 0,
                     g_views.inSortFlush ? 1 : 0,
                     g_views.effectOverlayActive ? 1 : 0,
                     g_views.projectedDecalMode,
                     g_draw.textureIsMissing[0] ? 1 : 0,
                     g_draw.textureIsMissing[1] ? 1 : 0,
                     g_draw.textureIsMissing[2] ? 1 : 0,
                     g_draw.textureIsMissing[3] ? 1 : 0,
                     tex0, tex1, tex2, tex3,
                     g_draw.tssOps0[0], g_draw.tssOps0[1],
                     g_draw.tssOps0[2], g_draw.tssOps0[3],
                     g_draw.tssOps1[0], g_draw.tssOps1[1],
                     g_draw.tssOps1[2], g_draw.tssOps1[3],
                     g_draw.texcoordSelect[0], g_draw.texcoordSelect[1],
                     g_draw.texcoordSelect[2], g_draw.texcoordSelect[3],
                     g_draw.texcoordSelect2[0], g_draw.texcoordSelect2[1],
                     g_draw.texcoordSelect2[2], g_draw.texcoordSelect2[3]);
        std::fclose(diag);
    }
}

static void LogBgfxSortedMaterialDecal(const char *event,
                                       bgfx::ViewId view,
                                       unsigned short polygonCount,
                                       unsigned short vertexCount,
                                       uint64_t state)
{
    if (!ShouldLogBgfxSortedDecals() || !IsSortedMaterialDecal(GetEffectiveDrawState()))
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_bgfx_sorted_decal_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s frame=%u view=%u polys=%u verts=%u state=0x%llx final=0x%llx depth=0x%llx zbias=%.6f rawZBias=%u tex0=%s tex1=%s tss0=(%.1f,%.1f,%.1f,%.1f) texSel=(%.1f,%.1f,%.1f,%.1f)\n",
                     event,
                     g_stats.frameIndex,
                     static_cast<unsigned>(view),
                     static_cast<unsigned>(polygonCount),
                     static_cast<unsigned>(vertexCount),
                     static_cast<unsigned long long>(g_draw.state),
                     static_cast<unsigned long long>(state),
                     static_cast<unsigned long long>(state & BGFX_STATE_DEPTH_TEST_MASK),
                     g_draw.zBias[0],
                     g_draw.zBiasUnits,
                     TextureDebugName(g_draw.sourceTextures[0]),
                     TextureDebugName(g_draw.sourceTextures[1]),
                     g_draw.tssOps0[0], g_draw.tssOps0[1],
                     g_draw.tssOps0[2], g_draw.tssOps0[3],
                     g_draw.texcoordSelect[0], g_draw.texcoordSelect[1],
                     g_draw.texcoordSelect[2], g_draw.texcoordSelect[3]);
        std::fclose(diag);
    }
}

static bool IsDefaultInfantryBlobShadowTexture(TextureBaseClass * texture)
{
    TextureClass * tex2d = texture ? texture->As_TextureClass() : nullptr;
    if (tex2d == nullptr)
    {
        return false;
    }

    const char *name = tex2d->Get_Full_Path().str();
    const char *base = name;
    for (const char *p = name; *p != '\0'; ++p)
    {
        if (*p == '\\' || *p == '/')
        {
            base = p + 1;
        }
    }

    return stricmp(base, "shadowi.tga") == 0
        || stricmp(base, "shadowi.dds") == 0;
}

static void UpdateAlphaMaskedShadowDecalMode()
{
    const uint64_t multiplicativeBlend =
        BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_SRC_COLOR);
    uint64_t state = GetEffectiveDrawState();
    const bool isAlphaMaskedShadow =
        IsDefaultInfantryBlobShadowTexture(g_draw.sourceTextures[0])
        && ((state & BGFX_STATE_BLEND_MASK) == multiplicativeBlend);
    g_draw.texcoordSelect2[2] = isAlphaMaskedShadow ? 1.0f : 0.0f;
}

static void UpdateAlphaMaskAndSortedEffectModes(uint64_t state)
{
    UpdateAlphaMaskedShadowDecalMode();
    if (IsSortedRotorBlur(state))
    {
        // avcomanche_p stores the rotor blur as a sorted mask. The legacy
        // fixed-function path keeps it independent from the vehicle material
        // opacity; the shader's normal MODULATE path can otherwise multiply it
        // away after replaying the sorted pool.
        g_draw.texcoordSelect2[2] = 2.0f;
    }
    else if (IsSneakAttackAlphaDepthDecal(state))
    {
        // UBSnkAtak_01 is the Sneak Attack's authored dirt/mound alpha quad.
        // Its sorted replay can inherit a zero-opacity material from the W3D
        // pass, which makes the wide dirt stain disappear while the opaque
        // mound geometry remains. Keep this path texture-alpha driven.
        g_draw.texcoordSelect2[2] = 3.0f;
    }
}

static RenderBackendProjectedDecalMode GetEffectiveProjectedDecalModeForCurrentDraw()
{
    RenderBackendProjectedDecalMode mode =
        static_cast<RenderBackendProjectedDecalMode>(g_views.projectedDecalMode);
    if (mode != RB_PROJECTED_DECAL_BLOB_SHADOW)
    {
        return mode;
    }

    const uint64_t multiplicativeBlend =
        BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_SRC_COLOR);
    uint64_t state = GetEffectiveDrawState();
    const bool validBlobShadow =
        IsDefaultInfantryBlobShadowTexture(g_draw.sourceTextures[0])
        && ((state & BGFX_STATE_BLEND_MASK) == multiplicativeBlend);

    return validBlobShadow ? RB_PROJECTED_DECAL_BLOB_SHADOW : RB_PROJECTED_DECAL_MULTIPLY;
}

static void UpdateProjectedDecalModeForCurrentDraw()
{
    const RenderBackendProjectedDecalMode mode = GetEffectiveProjectedDecalModeForCurrentDraw();
    g_draw.projectedDecalMode[0] = static_cast<float>(mode);
    g_draw.projectedDecalMode[1] = 0.0f;
    g_draw.projectedDecalMode[2] = 0.0f;
    g_draw.projectedDecalMode[3] = 0.0f;
}

static bool IsEffectiveProjectedBlobShadowDraw()
{
    return GetEffectiveProjectedDecalModeForCurrentDraw() == RB_PROJECTED_DECAL_BLOB_SHADOW;
}

static bool IsEffectiveProjectedShadowDraw()
{
    const RenderBackendProjectedDecalMode mode = GetEffectiveProjectedDecalModeForCurrentDraw();
    return mode == RB_PROJECTED_DECAL_BLOB_SHADOW
        || mode == RB_PROJECTED_DECAL_MULTIPLY;
}

static bool IsProjectedAdditiveDecalDraw()
{
    return g_views.projectedDecalMode == RB_PROJECTED_DECAL_ADDITIVE;
}

static uint64_t ApplyProjectedAdditiveDecalDrawState(uint64_t state)
{
    if (!IsProjectedAdditiveDecalDraw())
    {
        return state;
    }

    // DX8 projected additive decals write visual RGB into a backbuffer whose
    // alpha is not sampled later. The bgfx path renders world passes through an
    // intermediate scene target, so keep additive decal alpha isolated while
    // preserving the original RGB ONE/ONE blend.
    return state & ~BGFX_STATE_WRITE_A;
}

static void LogBgfxShroudPass(const char *event,
                              bgfx::ViewId view,
                              unsigned short polygonCount,
                              unsigned depthFunc,
                              unsigned tci0,
                              bool shroudDetected,
                              const float *shroudParams)
{
    if (!ShouldLogBgfxShroudPass())
    {
        return;
    }

    if (FILE *diag = std::fopen("ggc_bgfx_shroud_pass_diag.txt", "a"))
    {
        std::fprintf(diag,
                     "%s bgfxFrame=%u view=%u polys=%u depthFunc=%u stencil=%d active=%d objectActive=%d objectDim=%.3f activeStage=%u state=0x%llx tex0=%u tex1=%u tex2=%u tex3=%u sampler0=0x%x tci0=0x%x tss0=(%.1f,%.1f,%.1f,%.1f) texSel=(%.1f,%.1f,%.1f,%.1f) shroud=%d params=(%.6f,%.6f,%.6f,%.6f) names=(%s|%s|%s|%s)\n",
                     event,
                     g_stats.frameIndex,
                     static_cast<unsigned>(view),
                     static_cast<unsigned>(polygonCount),
                     depthFunc,
                     g_draw.stencilEnabled ? 1 : 0,
                     g_views.shroudTexturePassActive ? 1 : 0,
                     g_views.objectShroudTexturePassActive ? 1 : 0,
                     g_draw.objectShroudDim[0],
                     g_views.shroudTexturePassStage,
                     static_cast<unsigned long long>(g_draw.state),
                     bgfx::isValid(g_draw.tex[0]) ? g_draw.tex[0].idx : 0xffff,
                     bgfx::isValid(g_draw.tex[1]) ? g_draw.tex[1].idx : 0xffff,
                     bgfx::isValid(g_draw.tex[2]) ? g_draw.tex[2].idx : 0xffff,
                     bgfx::isValid(g_draw.tex[3]) ? g_draw.tex[3].idx : 0xffff,
                     g_draw.samplerFlags[0],
                     tci0,
                     g_draw.tssOps0[0], g_draw.tssOps0[1], g_draw.tssOps0[2], g_draw.tssOps0[3],
                     g_draw.texcoordSelect[0], g_draw.texcoordSelect[1], g_draw.texcoordSelect[2], g_draw.texcoordSelect[3],
                     shroudDetected ? 1 : 0,
                     shroudParams[0], shroudParams[1], shroudParams[2], shroudParams[3],
                     TextureDebugName(g_draw.sourceTextures[0]),
                     TextureDebugName(g_draw.sourceTextures[1]),
                     TextureDebugName(g_draw.sourceTextures[2]),
                     TextureDebugName(g_draw.sourceTextures[3]));
        std::fclose(diag);
    }
}

// TheSuperHackers @bugfix bobtista 09/07/2026 Legacy D3DRS_ZBIAS shifted depth
// by a few depth-buffer ULPs per unit - just enough to break coplanar ties.
// The previous emulation of 0.001 NDC per unit was ~2000x stronger and pulled
// biased draws through solid terrain at RTS camera distances: Fortress
// Avalanche's stale vanilla shore surf (authored below the ZH map's raised
// hill) rendered on top of the grass instead of being depth-rejected as on
// DX8. Two ULPs of a 24-bit depth buffer per unit keeps hundreds of ULPs of
// tie-break margin at the legacy maximum of 8 while lifting geometry well
// under a world unit at gameplay depths. Sorted material decals are
// unaffected: they clamp to their own tuned floor below.
static const float kZBiasPerUnit = 0.000002f;

static void TraceLegacyZBiasTranslation(unsigned zbiasUnits)
{
    static bool s_logged = false;
    if (!s_logged && zbiasUnits != 0 && std::getenv("GGC_TRACE") != nullptr)
    {
        s_logged = true;
        std::fprintf(stderr, "[ggc] legacy z-bias %d translates to %.3g ndc\n",
            static_cast<int>(zbiasUnits),
            static_cast<double>(zbiasUnits) * kZBiasPerUnit);
    }
}

// NDC z-pull applied to coplanar sorted decals so they win the LEQUAL test
// against the opaque sub-mesh they sit on. Keep this much smaller than the
// generic legacy z-bias conversion: a large clip-space pull makes command-center
// floor emblems render in front of bulldozers as they leave the building.
static const float kSortedDecalMinZBias = 0.00025f;
static const float kSortedDecalMaxZBias = 0.00075f;
// UBSnkAtak_01 sits under the entrance mesh as part of the model art. Do not
// pull it toward the camera or the dirt quad blends over the tunnel shell.
static const float kSneakAttackDecalMinZBias = 0.0f;
static const float kSneakAttackDecalMaxZBias = 0.0f;

static void ClampSortedMaterialDecalZBias()
{
    const uint64_t state = GetEffectiveDrawState();
    if (!IsSortedMaterialDecal(state) && !IsSortedAlphaDepthDecal(state))
    {
        return;
    }

    const float minZBias = IsSneakAttackAlphaDepthDecal(state)
        ? kSneakAttackDecalMinZBias
        : kSortedDecalMinZBias;
    const float maxZBias = IsSneakAttackAlphaDepthDecal(state)
        ? kSneakAttackDecalMaxZBias
        : kSortedDecalMaxZBias;

    if (g_draw.zBias[0] < minZBias)
    {
        g_draw.zBias[0] = minZBias;
    }
    else if (g_draw.zBias[0] > maxZBias)
    {
        g_draw.zBias[0] = maxZBias;
    }
}

static void BindSoftParticleDepth(bool enable)
{
    float params[4];
    GetSoftParticleParams(params);

    if (!enable || params[1] <= 0.0f)
    {
        params[0] = 0.0f;
    }

    if (params[0] > 0.5f
        && g_device.width > 0
        && g_device.height > 0
        && bgfx::isValid(g_device.sceneReadableDepth)
        && bgfx::isValid(g_uniforms.sSceneDepth))
    {
        params[0] = 1.0f;
        params[2] = 1.0f / static_cast<float>(g_device.width);
        params[3] = 1.0f / static_cast<float>(g_device.height);
        bgfx::setTexture(kBgfxSceneDepthSamplerStage, g_uniforms.sSceneDepth,
                         g_device.sceneReadableDepth,
                         BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        g_stats.textureBinds++;
    }
    else
    {
        params[0] = 0.0f;
        // TheSuperHackers @bugfix bobtista 30/04/2026 fs_uber declares
        // SAMPLER2D(s_sceneDepth, 6); Metal validation requires slot 6
        // to be bound on every draw even though u_softParticleParams.x
        // = 0 makes the shader skip the sample. defaultWhiteTexture is
        // a 1x1 RGBA8 placeholder that satisfies the validator without
        // creating a read/write hazard against the real depth target.
        if (bgfx::isValid(g_uniforms.sSceneDepth) && bgfx::isValid(g_device.defaultWhiteTexture))
        {
            bgfx::setTexture(kBgfxSceneDepthSamplerStage, g_uniforms.sSceneDepth,
                             g_device.defaultWhiteTexture,
                             BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        }
    }

    if (bgfx::isValid(g_uniforms.uSoftParticleParams))
    {
        // TheSuperHackers @feature bobtista 27/04/2026 Use the readable
        // opaque scene-depth target for conservative soft particles. This
        // starts with standard alpha-blended sorted draws only so additive
        // lasers, fire, and scanner effects keep their legacy intensity.
        bgfx::setUniform(g_uniforms.uSoftParticleParams, params);
    }
}

static uint32_t GetCurrentStageSamplerFlags(unsigned stage)
{
    return (stage < 4) ? g_draw.samplerFlags[stage] : 0;
}

static bool IsCurrentStageMipFilterDisabled(unsigned stage)
{
    return stage < 4 && g_draw.mipFilterDisabled[stage];
}

static bool ShouldBindSortedParticleBaseMip(unsigned stage)
{
    // Point-group and streak renderers generate camera-facing particle quads
    // in the sorted pass. Under bgfx/Metal the authored lower effect mips can
    // erase thin smoke/contrail sprites at normal gameplay zoom, while the
    // legacy particle path keeps these sprites legible. Bind stage 0 through
    // the existing one-mip sibling for those dynamic particle draws only; do
    // not apply this to sorted decals or their detail stages.
    // TheSuperHackers @bugfix bobtista 25/05/2026 The police-car lightbar glow
    // sprites (CopLight*.tga) are tiny sorted additive meshes whose authored
    // lower mips carry the red/blue/yellow color. Binding the one-mip
    // compatibility texture collapses them back to a dull level-0 hotspot, so
    // exclude them from this remap and let them sample the full mip chain.
    if (stage == 0
        && ContainsCaseInsensitive(TextureDebugName(g_draw.sourceTextures[0]), "coplight"))
    {
        return false;
    }
    return stage == 0
        && IsSortedParticleEffect(GetEffectiveDrawState());
}

static bgfx::TextureHandle GetCurrentStageTextureHandle(unsigned stage)
{
    if (stage >= 4)
    {
        return BGFX_INVALID_HANDLE;
    }

    TextureBaseClass *texture = g_draw.sourceTextures[stage];
    if (texture != nullptr && ShouldBindSortedParticleBaseMip(stage))
    {
        return EnsureBgfxTexture(texture, true);
    }
    if (texture != nullptr && IsCurrentStageMipFilterDisabled(stage))
    {
        // bgfx has point/linear mip selection flags but no sampler flag for
        // disabled mip filtering. Bind a one-mip sibling texture to preserve
        // the legacy "sample level 0 only" behavior.
        return EnsureBgfxTexture(texture, true);
    }
    return g_draw.tex[stage];
}

static void UploadLightUniforms()
{
    g_stats.lightUniformUploads++;
    if (bgfx::isValid(g_uniforms.uLightDirs))
    {
        bgfx::setUniform(g_uniforms.uLightDirs, g_draw.lightDirs, 4);
    }
    if (bgfx::isValid(g_uniforms.uLightColors))
    {
        bgfx::setUniform(g_uniforms.uLightColors, g_draw.lightColors, 4);
    }
    if (bgfx::isValid(g_uniforms.uLightAmbients))
    {
        bgfx::setUniform(g_uniforms.uLightAmbients, g_draw.lightAmbients, 4);
    }
    if (bgfx::isValid(g_uniforms.uLightPositions))
    {
        bgfx::setUniform(g_uniforms.uLightPositions, g_draw.lightPositions, 4);
    }
    if (bgfx::isValid(g_uniforms.uLightParams))
    {
        bgfx::setUniform(g_uniforms.uLightParams, g_draw.lightParams, 4);
    }
}

static void BindTextureStages()
{
    const uint64_t state = GetEffectiveDrawState();
    if (bgfx::isValid(g_uniforms.sTex0))
    {
        const bgfx::TextureHandle stageTexture = GetCurrentStageTextureHandle(0);
        const bgfx::TextureHandle bound =
            g_draw.textureIsMissing[0] && ShouldHideMissingTextureForCurrentDraw(state)
                ? g_device.defaultTransparentTexture
                : (bgfx::isValid(stageTexture) ? stageTexture : g_device.defaultWhiteTexture);
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(0, g_uniforms.sTex0, bound, GetCurrentStageSamplerFlags(0));
            g_stats.textureBinds++;
        }
    }
    if (bgfx::isValid(g_uniforms.sTex1))
    {
        const bgfx::TextureHandle stageTexture = GetCurrentStageTextureHandle(1);
        const bgfx::TextureHandle bound =
            g_draw.textureIsMissing[1] && ShouldHideMissingTextureForCurrentDraw(state)
                ? g_device.defaultTransparentTexture
                : (bgfx::isValid(stageTexture) ? stageTexture : g_device.defaultWhiteTexture);
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(1, g_uniforms.sTex1, bound, GetCurrentStageSamplerFlags(1));
            g_stats.textureBinds++;
        }
    }
    if (bgfx::isValid(g_uniforms.sTex2))
    {
        const bgfx::TextureHandle stageTexture = GetCurrentStageTextureHandle(2);
        const bgfx::TextureHandle bound =
            g_draw.textureIsMissing[2] && ShouldHideMissingTextureForCurrentDraw(state)
                ? g_device.defaultTransparentTexture
                : (bgfx::isValid(stageTexture) ? stageTexture : g_device.defaultWhiteTexture);
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(2, g_uniforms.sTex2, bound, GetCurrentStageSamplerFlags(2));
            g_stats.textureBinds++;
        }
    }
    if (bgfx::isValid(g_uniforms.sTex3))
    {
        const bgfx::TextureHandle stageTexture = GetCurrentStageTextureHandle(3);
        const bgfx::TextureHandle bound =
            g_draw.textureIsMissing[3] && ShouldHideMissingTextureForCurrentDraw(state)
                ? g_device.defaultTransparentTexture
                : (bgfx::isValid(stageTexture) ? stageTexture : g_device.defaultWhiteTexture);
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(3, g_uniforms.sTex3, bound, GetCurrentStageSamplerFlags(3));
            g_stats.textureBinds++;
        }
    }
}

static float GetTexcoordSource(unsigned texcoordGen)
{
    if (texcoordGen == kTexcoordGenCameraNormal)
    {
        return 1.0f;
    }
    if (texcoordGen == kTexcoordGenCameraReflection)
    {
        return 2.0f;
    }
    if (texcoordGen == kTexcoordGenCameraPosition)
    {
        return 3.0f;
    }
    return 0.0f;
}

static void SetIdentityTextureTransform(float * row0, float * row1)
{
    row0[0] = 1.0f;
    row0[1] = 0.0f;
    row0[2] = 0.0f;
    row0[3] = 0.0f;
    row1[0] = 0.0f;
    row1[1] = 1.0f;
    row1[2] = 0.0f;
    row1[3] = 0.0f;
}

static void ReadTextureTransform(unsigned stage, float * row0, float * row1)
{
    auto texMtx = MakeIdentityLegacyCacheMatrix();
    FixedFunctionState::Transform_Matrix(kTextureTransformStage0 + stage, texMtx);
    row0[0] = texMtx.m[0][0];
    row0[1] = texMtx.m[1][0];
    row0[2] = texMtx.m[2][0];
    row0[3] = texMtx.m[3][0];
    row1[0] = texMtx.m[0][1];
    row1[1] = texMtx.m[1][1];
    row1[2] = texMtx.m[2][1];
    row1[3] = texMtx.m[3][1];
}

// TheSuperHackers @feature bobtista 30/04/2026 Read column 2 of the texture
// matrix for projected 3-component stages - TexProjectClass uses
// this column (= ViewToPixel row 3 in MatrixMapperClass::Apply) as the
// projected W. The shader divides UV.xy by this value at vertex time.
static void ReadTextureTransformZ(unsigned stage, float * rowZ)
{
    auto texMtx = MakeIdentityLegacyCacheMatrix();
    FixedFunctionState::Transform_Matrix(kTextureTransformStage0 + stage, texMtx);
    rowZ[0] = texMtx.m[0][2];
    rowZ[1] = texMtx.m[1][2];
    rowZ[2] = texMtx.m[2][2];
    rowZ[3] = texMtx.m[3][2];
}

static void UpdateTextureTransforms()
{
    g_stats.textureTransformUpdates++;
    // TheSuperHackers @bugfix bobtista 25/04/2026 Honor material-stage texture
    // matrices in the bgfx uber shader. W3D atlas mappers animate tank
    // treads, bike wheels, and other sub-materials by setting
    // cached texture transform plus two-coordinate transform mode. Passing raw UVs sampled the
    // unused black padding in those atlases; stage 1 matters for detail
    // and environment-mapped sub-materials.
    const unsigned texcoordIndex = g_draw.texcoordIndex[0];
    const unsigned uvIndex = texcoordIndex & 0xFFFF;
    const unsigned texcoordGen = texcoordIndex & 0xFFFF0000;
    // TheSuperHackers @info bobtista 26/04/2026 Only UV sets 0 and 1 are
    // supported. The legacy fixed-function path allows up to 8 UV sets but
    // the uber shader only has v_texcoord0/v_texcoord1. Extend if any
    // material is found using UV set 2+.
    if (uvIndex > 1)
    {
        static bool s_loggedUV2 = false;
        if (!s_loggedUV2)
        {
            s_loggedUV2 = true;
            WWDEBUG_SAY(("[BgfxBackend] Stage 0 TEXCOORDINDEX uses UV set %u (only 0/1 supported)", uvIndex));
        }
    }
    g_draw.texcoordSelect[0] = (uvIndex == 1) ? 1.0f : 0.0f;
    g_draw.texcoordSource[0] = GetTexcoordSource(texcoordGen);

    const unsigned texFlags = g_draw.textureTransformFlags[0];
    const unsigned texCount = texFlags & 0xFFu;
    const bool texProjected0 = (texFlags & kTextureTransformProjected) != 0
        && texCount >= kTextureTransformCount3;
    if (texCount >= kTextureTransformCount2)
    {
        g_draw.texcoordSelect[3] = 1.0f;
        ReadTextureTransform(0, g_draw.texTransform0, g_draw.texTransform1);
        if (texProjected0)
        {
            ReadTextureTransformZ(0, g_draw.texTransform0Z);
        }
    }
    else
    {
        g_draw.texcoordSelect[3] = 0.0f;
        SetIdentityTextureTransform(g_draw.texTransform0, g_draw.texTransform1);
    }
    g_draw.texProjected[0] = texProjected0 ? 1.0f : 0.0f;

    const unsigned texcoordIndex1 = g_draw.texcoordIndex[1];
    const unsigned uvIndex1 = texcoordIndex1 & 0xFFFF;
    const unsigned texcoordGen1 = texcoordIndex1 & 0xFFFF0000;
    if (uvIndex1 > 1)
    {
        static bool s_loggedUV2_s1 = false;
        if (!s_loggedUV2_s1)
        {
            s_loggedUV2_s1 = true;
            WWDEBUG_SAY(("[BgfxBackend] Stage 1 TEXCOORDINDEX uses UV set %u (only 0/1 supported)", uvIndex1));
        }
    }
    g_draw.texcoordSelect2[0] = (uvIndex1 == 1) ? 1.0f : 0.0f;
    g_draw.texcoordSource[1] = GetTexcoordSource(texcoordGen1);

    const unsigned texFlags1 = g_draw.textureTransformFlags[1];
    const unsigned texCount1 = texFlags1 & 0xFFu;
    const bool texProjected1 = (texFlags1 & kTextureTransformProjected) != 0
        && texCount1 >= kTextureTransformCount3;
    if (texCount1 >= kTextureTransformCount2)
    {
        g_draw.texcoordSelect2[1] = 1.0f;
        ReadTextureTransform(1, g_draw.tex1Transform0, g_draw.tex1Transform1);
        if (texProjected1)
        {
            ReadTextureTransformZ(1, g_draw.tex1TransformZ);
        }
    }
    else
    {
        g_draw.texcoordSelect2[1] = 0.0f;
        SetIdentityTextureTransform(g_draw.tex1Transform0, g_draw.tex1Transform1);
    }
    g_draw.texProjected[1] = texProjected1 ? 1.0f : 0.0f;

    const unsigned texcoordIndex2 = g_draw.texcoordIndex[2];
    const unsigned texcoordGen2 = texcoordIndex2 & 0xFFFF0000;
    g_draw.texcoordSource[2] = GetTexcoordSource(texcoordGen2);

    const unsigned texFlags2 = g_draw.textureTransformFlags[2];
    const unsigned texCount2 = texFlags2 & 0xFFu;
    if (texCount2 >= kTextureTransformCount2)
    {
        ReadTextureTransform(2, g_draw.tex2Transform0, g_draw.tex2Transform1);
    }
    else
    {
        SetIdentityTextureTransform(g_draw.tex2Transform0, g_draw.tex2Transform1);
    }
}

static void UploadMaterialUniforms()
{
    g_stats.materialUniformUploads++;
    if (bgfx::isValid(g_uniforms.uMatDiffuse))
    {
        bgfx::setUniform(g_uniforms.uMatDiffuse, g_draw.matDiffuse);
    }
    if (bgfx::isValid(g_uniforms.uMatAmbient))
    {
        bgfx::setUniform(g_uniforms.uMatAmbient, g_draw.matAmbient);
    }
    if (bgfx::isValid(g_uniforms.uMatEmissive))
    {
        bgfx::setUniform(g_uniforms.uMatEmissive, g_draw.matEmissive);
    }

    if (bgfx::isValid(g_uniforms.uTssOps0))
    {
        float tssOps0[4] = {
            g_draw.tssOps0[0], g_draw.tssOps0[1],
            g_draw.shaderTssOps0[2], g_draw.shaderTssOps0[3]
        };
        bgfx::setUniform(g_uniforms.uTssOps0, tssOps0);
    }
    if (bgfx::isValid(g_uniforms.uTssOps1))
    {
        bgfx::setUniform(g_uniforms.uTssOps1, g_draw.tssOps1);
    }
    if (bgfx::isValid(g_uniforms.uAtestParams))
    {
        const float effectiveAtestRef = g_overrides.atestActive ? g_overrides.atestRef : g_draw.atestRef;
        const float effectiveAtestFunc = g_overrides.atestActive ? g_overrides.atestFunc : (g_draw.atestEnabled ? g_draw.atestFunc : 0.0f);
        float atestParams[4] = { effectiveAtestRef, effectiveAtestFunc, 0.0f, 0.0f };
        bgfx::setUniform(g_uniforms.uAtestParams, atestParams);
    }
    if (bgfx::isValid(g_uniforms.uTexcoordSource))
    {
        bgfx::setUniform(g_uniforms.uTexcoordSource, g_draw.texcoordSource);
    }
    if (bgfx::isValid(g_uniforms.uVertexColorFlags))
    {
        bgfx::setUniform(g_uniforms.uVertexColorFlags, g_draw.vertexColorFlags);
    }
    if (bgfx::isValid(g_uniforms.uTexcoordSelect2))
    {
        bgfx::setUniform(g_uniforms.uTexcoordSelect2, g_draw.texcoordSelect2);
    }
    if (bgfx::isValid(g_uniforms.uProjectedDecalMode))
    {
        bgfx::setUniform(g_uniforms.uProjectedDecalMode, g_draw.projectedDecalMode);
    }
    if (bgfx::isValid(g_uniforms.uGrayscaleEnable))
    {
        bgfx::setUniform(g_uniforms.uGrayscaleEnable, g_draw.grayscaleEnable);
    }
    if (bgfx::isValid(g_uniforms.uObjectShroudDim))
    {
        float objectShroudDim[4] = {
            g_views.objectShroudTexturePassActive ? g_draw.objectShroudDim[0] : 1.0f,
            g_views.objectShroudTexturePassActive ? g_draw.objectShroudDim[1] : 0.0f,
            g_views.objectShroudTexturePassActive ? g_draw.objectShroudDim[2] : 0.0f,
            g_draw.objectShroudDim[3]
        };
        bgfx::setUniform(g_uniforms.uObjectShroudDim, objectShroudDim);
    }
    if (bgfx::isValid(g_uniforms.uCloudParams))
    {
        bgfx::setUniform(g_uniforms.uCloudParams, g_draw.cloudParams);
    }
    if (bgfx::isValid(g_uniforms.uTexTransform0))
    {
        bgfx::setUniform(g_uniforms.uTexTransform0, g_draw.texTransform0);
    }
    if (bgfx::isValid(g_uniforms.uTexTransform1))
    {
        bgfx::setUniform(g_uniforms.uTexTransform1, g_draw.texTransform1);
    }
    if (bgfx::isValid(g_uniforms.uTexTransform0Z))
    {
        bgfx::setUniform(g_uniforms.uTexTransform0Z, g_draw.texTransform0Z);
    }
    if (bgfx::isValid(g_uniforms.uTex1Transform0))
    {
        bgfx::setUniform(g_uniforms.uTex1Transform0, g_draw.tex1Transform0);
    }
    if (bgfx::isValid(g_uniforms.uTex1Transform1))
    {
        bgfx::setUniform(g_uniforms.uTex1Transform1, g_draw.tex1Transform1);
    }
    if (bgfx::isValid(g_uniforms.uTex1TransformZ))
    {
        bgfx::setUniform(g_uniforms.uTex1TransformZ, g_draw.tex1TransformZ);
    }
    if (bgfx::isValid(g_uniforms.uTex2Transform0))
    {
        bgfx::setUniform(g_uniforms.uTex2Transform0, g_draw.tex2Transform0);
    }
    if (bgfx::isValid(g_uniforms.uTex2Transform1))
    {
        bgfx::setUniform(g_uniforms.uTex2Transform1, g_draw.tex2Transform1);
    }
    if (bgfx::isValid(g_uniforms.uTexProjected))
    {
        bgfx::setUniform(g_uniforms.uTexProjected, g_draw.texProjected);
    }
    if (bgfx::isValid(g_uniforms.uLegacyPixelShaderMode))
    {
        bgfx::setUniform(g_uniforms.uLegacyPixelShaderMode, g_draw.legacyPixelShaderMode);
    }
    if (bgfx::isValid(g_uniforms.uZBias))
    {
        bgfx::setUniform(g_uniforms.uZBias, g_draw.zBias);
    }
    if (bgfx::isValid(g_uniforms.sCloudMap))
    {
        // WRAP addressing matches the DX8 cloud pass at W3DShaderManager.cpp:1742.
        // TheSuperHackers @bugfix bobtista 30/04/2026 fs_uber declares
        // SAMPLER2D(s_cloudMap, 5); Metal validation requires slot 5
        // to be bound on every draw even when u_cloudParams.w = 0
        // disables the cloud blend. Fall back to defaultWhiteTexture
        // when no cloud texture has been set yet (early frames, UI).
        bgfx::TextureHandle h = bgfx::isValid(g_draw.cloudTex)
            ? g_draw.cloudTex
            : g_device.defaultWhiteTexture;
        if (bgfx::isValid(h))
        {
            bgfx::setTexture(5, g_uniforms.sCloudMap, h, BGFX_SAMPLER_NONE);
            g_stats.textureBinds++;
        }
    }
}

// Mirror the material fields used by fs_uber without applying DX8 state.
// This is called from Set_Material and again at submit time because
// DX8Wrapper::Draw can apply pending material state directly through
// VertexMaterialClass::Apply after the bgfx backend saw the original setter.
static void CaptureMaterialStateForBgfx(const VertexMaterialClass * material)
{
    if (material != nullptr)
    {
        Vector3 diffuse(1.0f, 1.0f, 1.0f);
        Vector3 ambient(1.0f, 1.0f, 1.0f);
        const VertexMaterialClass::ColorSourceType diffuseSource =
            const_cast<VertexMaterialClass *>(material)->Get_Diffuse_Color_Source();
        const VertexMaterialClass::ColorSourceType ambientSource =
            const_cast<VertexMaterialClass *>(material)->Get_Ambient_Color_Source();
        const VertexMaterialClass::ColorSourceType emissiveSource =
            const_cast<VertexMaterialClass *>(material)->Get_Emissive_Color_Source();
        if (diffuseSource == VertexMaterialClass::MATERIAL)
        {
            material->Get_Diffuse(&diffuse);
        }
        if (ambientSource == VertexMaterialClass::MATERIAL)
        {
            material->Get_Ambient(&ambient);
        }
        g_draw.matDiffuse[0] = diffuse.X;
        g_draw.matDiffuse[1] = diffuse.Y;
        g_draw.matDiffuse[2] = diffuse.Z;
        g_draw.matDiffuse[3] = material->Get_Opacity();
        g_draw.matAmbient[0] = ambient.X;
        g_draw.matAmbient[1] = ambient.Y;
        g_draw.matAmbient[2] = ambient.Z;
        g_draw.matAmbient[3] = 1.0f;
        g_draw.vertexColorFlags[1] =
            (diffuseSource == VertexMaterialClass::COLOR1) ? 1.0f : 0.0f;
        g_draw.vertexColorFlags[2] =
            (ambientSource == VertexMaterialClass::COLOR1) ? 1.0f : 0.0f;
        g_draw.vertexColorFlags[3] =
            (emissiveSource == VertexMaterialClass::COLOR1) ? 1.0f : 0.0f;
        g_draw.lightingEnabled[0] =
            (material->Get_Lighting()
             && FixedFunctionState::Lighting_Enabled(false)
             && !WW3D::Is_Coloring_Enabled()) ? 1.0f : 0.0f;

        Vector3 emissive(0.0f, 0.0f, 0.0f);
        material->Get_Emissive(&emissive);
        g_draw.matEmissive[0] = emissive.X;
        g_draw.matEmissive[1] = emissive.Y;
        g_draw.matEmissive[2] = emissive.Z;
        g_draw.matEmissive[3] = 0.0f;
    }
    else
    {
        g_draw.matDiffuse[0] = 1.0f;
        g_draw.matDiffuse[1] = 1.0f;
        g_draw.matDiffuse[2] = 1.0f;
        g_draw.matDiffuse[3] = 1.0f;
        g_draw.matAmbient[0] = 1.0f;
        g_draw.matAmbient[1] = 1.0f;
        g_draw.matAmbient[2] = 1.0f;
        g_draw.matAmbient[3] = 1.0f;
        g_draw.matEmissive[0] = 0.0f;
        g_draw.matEmissive[1] = 0.0f;
        g_draw.matEmissive[2] = 0.0f;
        g_draw.matEmissive[3] = 0.0f;
        g_draw.vertexColorFlags[1] = 0.0f;
        g_draw.vertexColorFlags[2] = 0.0f;
        g_draw.vertexColorFlags[3] = 0.0f;
        g_draw.lightingEnabled[0] = 0.0f;
    }
}

// TheSuperHackers @refactor bobtista 11/04/2026 Sorted VB direct-draw submit: claims the
// transients Capture_Dynamic_* stashed for Draw_Sorting_IB_VB's inner buffers, submits to
// the sorted view with remapped args, and skips the outer Draw_Triangles submit.

void BgfxBackend::Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                     const DynamicIBAccessClass & dyn_ib,
                                     unsigned short polygon_count,
                                     unsigned short vertex_count)
{
    if (!g_device.initialized)
    {
        return;
    }
    g_stats.sortedDraws++;

    // The inner dynamic buffers' WriteLockClass dtors already ran, so
    // Capture_Dynamic_Vertex_Data / Capture_Dynamic_Index_Data should
    // have stashed their transients keyed by &dyn_vb / &dyn_ib.
    if (!g_draw.pendingVB.valid || g_draw.pendingVB.owner != &dyn_vb)
    {
        static bool s_loggedSkipVB = false;
        if (!s_loggedSkipVB)
        {
            s_loggedSkipVB = true;
            WWDEBUG_SAY(("[BgfxBackend] Submit_Sorted_Draw SKIP: pendingDynVB not "
                         "claimable (valid=%d ownerMatch=%d)",
                         int(g_draw.pendingVB.valid),
                         int(g_draw.pendingVB.owner == &dyn_vb)));
        }
        g_stats.skippedDraws++;
        return;
    }
    if (!g_draw.pendingIB.valid || g_draw.pendingIB.owner != &dyn_ib)
    {
        static bool s_loggedSkipIB = false;
        if (!s_loggedSkipIB)
        {
            s_loggedSkipIB = true;
            WWDEBUG_SAY(("[BgfxBackend] Submit_Sorted_Draw SKIP: pendingDynIB not "
                         "claimable (valid=%d ownerMatch=%d)",
                         int(g_draw.pendingIB.valid),
                         int(g_draw.pendingIB.owner == &dyn_ib)));
        }
        g_stats.skippedDraws++;
        return;
    }

    const bgfx::TransientVertexBuffer vb = g_draw.pendingVB.tvb;
    const bgfx::TransientIndexBuffer  ib = g_draw.pendingIB.tib;
    const FVFInfoClass & traceFvf = dyn_vb.FVF_Info();
    const unsigned traceStride = traceFvf.Get_FVF_Size();
    g_draw.activeVertexNormalBias = g_draw.pendingVB.coplanarNormalBias;
    g_draw.pendingVB.valid = false;
    g_draw.pendingIB.valid = false;

    if (!bgfx::isValid(g_draw.program))
    {
        g_views.skipNextSubmitEngineDraw = true;
        g_stats.skippedDraws++;
        return;
    }

    // Sort view's view+proj were set up at init (identity view,
    // projection tracks opaque view via Set_Projection_Transform_With_Z_Bias).
    // World is the current g_frame.sortWorld if we are inside a sort batch,
    // otherwise the regular g_frame.world (rigid FVF category with sorting=true
    // has no batch-wrapped Apply_Render_State - it uses the per-mesh world
    // set by the caller via g_renderBackend->Set_Transform).
    const uint64_t earlyState = GetEffectiveDrawState();
    const bool copLightSprite = IsSortedCopLightSprite(earlyState);
    const bgfx::ViewId submitView = copLightSprite ? kBgfxEngineView : kBgfxEngineSortView;
    const float * worldMtx = g_views.inSortFlush ? g_frame.sortWorld : g_frame.world;
    if (copLightSprite)
    {
        // Coplight glow quads are authored in model space the same way the
        // Chinook rotor blur is. Use the raw per-mesh world so each bone's
        // animated position lands the quad on the lightbar instead of folding
        // through the sort view's pre-multiplied matrix.
        worldMtx = g_frame.sortWorldRaw;
    }
    bgfx::setTransform(worldMtx);

    bgfx::setVertexBuffer(0, &vb, 0, vertex_count);
    bgfx::setIndexBuffer(&ib, 0, static_cast<uint32_t>(polygon_count) * 3);

    BindTextureStages();
    UpdateTextureTransforms();
    if (IsSortedMaterialDecal(GetEffectiveDrawState()))
    {
        // Terrain rendering leaves this flag set until reset by the
        // shader manager. Sorted material decals, including command-center
        // driveway emblems, use the fixed-function TSS path and must not
        // inherit the terrain pixel-shader branch. Submit_Sorted_Draw can
        // be reached by rigid sorting draws even when g_views.inSortFlush
        // is false, so key this reset to the draw signature itself.
        g_draw.texcoordSelect[1] = 0.0f;
    }
    {
        g_draw.zBias[0] = static_cast<float>(g_draw.zBiasUnits) * kZBiasPerUnit;
        TraceLegacyZBiasTranslation(g_draw.zBiasUnits);
        const bool applySubmittedNormalBias = ShouldApplySubmittedNormalBias(GetEffectiveDrawState());
        const bool normalBiasFromGeometry =
            g_draw.normalBias[0] != 0.0f
            || (g_draw.activeVertexNormalBias && applySubmittedNormalBias)
            || IsSneakAttackCoplanarSurface();
        g_draw.zBias[1] = normalBiasFromGeometry
            ? ((g_draw.normalBias[0] < 0.02f) ? 0.02f : g_draw.normalBias[0])
            : 0.0f;
        ClampSortedMaterialDecalZBias();
    }
    UpdateProjectedDecalModeForCurrentDraw();
    uint64_t state = GetEffectiveDrawState();
    {
        g_draw.texcoordSelect2[3] = IsAnyAdditiveBlend(state)
            ? 1.0f
            : 0.0f;
    }
    UpdateAlphaMaskAndSortedEffectModes(state);
    UploadMaterialUniforms();
    if (bgfx::isValid(g_uniforms.uTexcoordSelect))
    {
        bgfx::setUniform(g_uniforms.uTexcoordSelect, g_draw.texcoordSelect);
    }
    UploadLightUniforms();
    // Match SubmitEngineDraw for sorted dynamic particles/effects. Additive
    // sprites, soft alpha particles, and material decals bake intensity in
    // vertex diffuse or the source texture; the shader's lit branch would
    // ignore that baked color and multiply by scene light instead.
    if (bgfx::isValid(g_uniforms.uLightingEnabled))
    {
        float lit[4] = { g_draw.lightingEnabled[0], 0.0f, 0.0f, 0.0f };
        if (ShouldForceUnlitForBakedColorDraw(state))
        {
            lit[0] = 0.0f;
        }
        bgfx::setUniform(g_uniforms.uLightingEnabled, lit);
    }

    state = ApplyCullModeOverride(state);
    state = ApplyBlendEquation(state);
    state = ApplyProjectedAdditiveDecalDrawState(state);
    state = ApplyColorWriteOverride(state);
    state = ApplySortedMaterialDecalDepthState(state);
    LogBgfxSortedMaterialDecal("submit-sorted", submitView,
                               polygon_count, vertex_count, state);
    LogBgfxEffectSubmit("submit-sorted", submitView,
                        polygon_count, vertex_count, state, "pre-skip");
    LogBgfxRevealDraw("submit-sorted", submitView,
                      polygon_count, vertex_count, state, "pre-skip");

    if (ShouldAllowBgfxDiagnosticDrawOverrides()
        && std::getenv("GGC_BGFX_SKIP_REVEAL_GRID") != nullptr
        && IsRevealGridTexture(g_draw.sourceTextures[0]))
    {
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }

    if (ShouldSkipHiddenMissingTextureDraw(state))
    {
        LogBgfxRevealDraw("submit-sorted", submitView,
                          polygon_count, vertex_count, state, "skip-missing");
        g_stats.skippedDraws++;
        return;
    }

    bgfx::setState(state);
    BindSoftParticleDepth(submitView == kBgfxEngineSortView
                          && IsSoftParticleCandidate(state));
    bgfx::submit(submitView, g_draw.program);
    LogBgfxEffectSubmit("submit-sorted", submitView,
                        polygon_count, vertex_count, state, "submit");
    LogBgfxRevealDraw("submit-sorted", submitView,
                      polygon_count, vertex_count, state, "submit");
    g_stats.baseSubmits++;
    g_stats.transientVbDraws++;
    g_stats.transientIbDraws++;
    g_views.skipNextSubmitEngineDraw = true;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Dynamic
// capture. DynamicVBAccessClass / DynamicIBAccessClass are CPU-side
// views onto a ring buffer that changes every frame (particles, sprites,
// skinned meshes, HUD). Creating a bgfx VB per frame would churn the
// GPU allocator, so the bgfx side uses transient buffers which are
// auto-freed at the next bgfx::frame.
//
// Flow: engine's WriteLockClass destructor calls this with the locked
// sub-range. We alloc a transient buffer of exactly that size, memcpy
// the data in, and stash the handle keyed by the access class pointer.
// The matching Set_Vertex_Buffer(DynamicVBAccessClass&) later sees its
// own pointer in g_draw.pendingVB and claims the transient for the draw.

static bool SameSubmittedPosition(const float * a, const float * b)
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz < 0.000001f;
}

static bool HasSubmittedOppositeNormalPairs(const FVFInfoClass & fvf,
                                            const void * data,
                                            uint32_t numVerts)
{
    if (!fvf.Has_Normal() || data == nullptr || numVerts < 2 || numVerts > 4096)
    {
        return false;
    }

    const uint8_t * bytes = static_cast<const uint8_t *>(data);
    const unsigned stride = fvf.Get_FVF_Size();
    const unsigned positionOffset = fvf.Get_Location_Offset();
    const unsigned normalOffset = fvf.Get_Normal_Offset();
    for (uint32_t i = 0; i < numVerts; ++i)
    {
        const float * pi = reinterpret_cast<const float *>(bytes + i * stride + positionOffset);
        const float * ni = reinterpret_cast<const float *>(bytes + i * stride + normalOffset);
        for (uint32_t j = i + 1; j < numVerts; ++j)
        {
            const float * pj = reinterpret_cast<const float *>(bytes + j * stride + positionOffset);
            if (!SameSubmittedPosition(pi, pj))
            {
                continue;
            }
            const float * nj = reinterpret_cast<const float *>(bytes + j * stride + normalOffset);
            const float dot = ni[0] * nj[0] + ni[1] * nj[1] + ni[2] * nj[2];
            if (dot < -0.9f)
            {
                return true;
            }
        }
    }
    return false;
}

void BgfxBackend::Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                              const void * data,
                                              unsigned int size_bytes)
{
    if (!g_device.initialized || vba == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }

    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vba->FVF_Info(), layout))
    {
        return;
    }

    const uint32_t num_verts = static_cast<uint32_t>(vba->Get_Vertex_Count());
    if (num_verts == 0)
    {
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(num_verts, layout) < num_verts)
    {
        LogBgfxTransientDiag("capture", "vb", vba, num_verts,
                             g_draw.pendingVB.valid,
                             g_draw.pendingVB.owner == vba,
                             g_draw.useTransientVB,
                             g_draw.activeTransientVBOwner == vba,
                             "no-avail");
        g_draw.pendingVB.valid = false;
        g_draw.pendingVB.coplanarNormalBias = false;
        return;
    }

    bgfx::allocTransientVertexBuffer(&g_draw.pendingVB.tvb, num_verts, layout);
    g_stats.transientVbAllocations++;
    const uint32_t copy_bytes = num_verts * layout.getStride();
    const uint32_t bytes = (size_bytes < copy_bytes) ? size_bytes : copy_bytes;
    std::memcpy(g_draw.pendingVB.tvb.data, data, bytes);
    g_draw.pendingVB.owner = vba;
    g_draw.pendingVB.valid = true;
    g_draw.pendingVB.coplanarNormalBias = HasSubmittedOppositeNormalPairs(vba->FVF_Info(), data, num_verts);
    LogBgfxTransientDiag("capture", "vb", vba, num_verts,
                         true,
                         true,
                         g_draw.useTransientVB,
                         g_draw.activeTransientVBOwner == vba,
                         "ok");

    // TheSuperHackers @bugfix bobtista 30/04/2026 Track FVF normal presence
    // for transient-VB submits so the engine-view targeted lit-on override
    // in SubmitEngineDraw works for translucent meshes and other paths that
    // never go through Set_Vertex_Buffer.
    g_draw.fvfHasNormal = vba->FVF_Info().Has_Normal();
}

void BgfxBackend::Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                             const void * data,
                                             unsigned int size_bytes)
{
    if (!g_device.initialized || iba == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }

    const uint32_t num_indices = static_cast<uint32_t>(iba->Get_Index_Count());
    if (num_indices == 0)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(num_indices) < num_indices)
    {
        LogBgfxTransientDiag("capture", "ib", iba, num_indices,
                             g_draw.pendingIB.valid,
                             g_draw.pendingIB.owner == iba,
                             g_draw.useTransientIB,
                             g_draw.activeTransientIBOwner == iba,
                             "no-avail");
        g_draw.pendingIB.valid = false;
        return;
    }

    bgfx::allocTransientIndexBuffer(&g_draw.pendingIB.tib, num_indices);
    g_stats.transientIbAllocations++;
    const uint32_t copy_bytes = num_indices * sizeof(uint16_t);
    const uint32_t bytes = (size_bytes < copy_bytes) ? size_bytes : copy_bytes;
    std::memcpy(g_draw.pendingIB.tib.data, data, bytes);
    g_draw.pendingIB.owner = iba;
    g_draw.pendingIB.valid = true;
    LogBgfxTransientDiag("capture", "ib", iba, num_indices,
                         true,
                         true,
                         g_draw.useTransientIB,
                         g_draw.activeTransientIBOwner == iba,
                         "ok");
}

void * BgfxBackend::Begin_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba,
                                                unsigned int size_bytes)
{
    if (!g_device.initialized || vba == nullptr || size_bytes == 0) {
        return nullptr;
    }
    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vba->FVF_Info(), layout)) {
        return nullptr;
    }
    const uint32_t num_verts = static_cast<uint32_t>(vba->Get_Vertex_Count());
    if (num_verts == 0 || bgfx::getAvailTransientVertexBuffer(num_verts, layout) < num_verts) {
        return nullptr;
    }
    bgfx::allocTransientVertexBuffer(&g_draw.pendingVB.tvb, num_verts, layout);
    g_stats.transientVbAllocations++;
    return g_draw.pendingVB.tvb.data;
}

void BgfxBackend::End_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba,
                                            const void * data,
                                            unsigned int size_bytes)
{
    if (!g_device.initialized || vba == nullptr) {
        return;
    }
    g_draw.pendingVB.owner = vba;
    g_draw.pendingVB.valid = true;
    g_draw.pendingVB.coplanarNormalBias = HasSubmittedOppositeNormalPairs(
        vba->FVF_Info(), data, vba->Get_Vertex_Count());
    g_draw.fvfHasNormal = vba->FVF_Info().Has_Normal();
}

void * BgfxBackend::Begin_Dynamic_Index_Write(const DynamicIBAccessClass * iba,
                                               unsigned int size_bytes)
{
    if (!g_device.initialized || iba == nullptr || size_bytes == 0) {
        return nullptr;
    }
    const uint32_t num_indices = static_cast<uint32_t>(iba->Get_Index_Count());
    if (num_indices == 0 || bgfx::getAvailTransientIndexBuffer(num_indices) < num_indices) {
        return nullptr;
    }
    bgfx::allocTransientIndexBuffer(&g_draw.pendingIB.tib, num_indices);
    g_stats.transientIbAllocations++;
    return g_draw.pendingIB.tib.data;
}

void BgfxBackend::End_Dynamic_Index_Write(const DynamicIBAccessClass * iba,
                                           const void * data,
                                           unsigned int size_bytes)
{
    if (!g_device.initialized || iba == nullptr) {
        return;
    }
    g_draw.pendingIB.owner = iba;
    g_draw.pendingIB.valid = true;
}

// -- Instancing -------------------------------------------------------------

bool BgfxBackend::Supports_Instancing() const
{
    return g_device.initialized
        && (bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING) != 0;
}

bool BgfxBackend::Begin_Instanced_Batch(unsigned max_instances)
{
    if (!Supports_Instancing() || max_instances == 0) {
        return false;
    }

    bgfx::allocInstanceDataBuffer(&g_draw.instanceBatch, max_instances, 64);
    if (g_draw.instanceBatch.data == nullptr) {
        return false;
    }

    g_draw.instanceCount = 0;
    g_draw.instanceMax = max_instances;
    g_draw.instanceBatchActive = true;
    return true;
}

void BgfxBackend::Add_Instance(const float * world_matrix_4x4)
{
    if (!g_draw.instanceBatchActive || g_draw.instanceCount >= g_draw.instanceMax) {
        return;
    }
    std::memcpy(g_draw.instanceBatch.data + g_draw.instanceCount * 64, world_matrix_4x4, 64);
    g_draw.instanceCount++;
}

void BgfxBackend::Submit_Instanced_Batch(unsigned index_offset,
                                          unsigned triangle_count,
                                          unsigned min_vertex_index,
                                          unsigned vertex_count)
{
    if (!g_draw.instanceBatchActive || g_draw.instanceCount == 0) {
        g_draw.instanceBatchActive = false;
        return;
    }
    g_draw.instanceBatchActive = false;

    bgfx::setInstanceDataBuffer(&g_draw.instanceBatch, 0, g_draw.instanceCount);

    float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    bgfx::setTransform(identity);

    bgfx::ProgramHandle savedProgram = g_draw.program;
    g_draw.program = g_device.uberInstancedProgram;

    Draw_Triangles(
        static_cast<unsigned short>(index_offset),
        static_cast<unsigned short>(triangle_count),
        static_cast<unsigned short>(min_vertex_index),
        static_cast<unsigned short>(vertex_count));

    g_draw.program = savedProgram;
    g_stats.instancedSavedDrawCalls += g_draw.instanceCount - 1;
}

// -- State: shaders, materials, textures ------------------------------------

void BgfxBackend::Set_Shader(const ShaderClass & shader)
{
    FixedFunctionState::Set_Shader(shader);
    g_draw.program = g_device.uberProgram;
    g_draw.state   = BuildBgfxStateForShader(shader);
    const uint64_t srcBits = TranslateBlendFactor(shader.Get_Src_Blend_Func());
    const uint64_t dstBits = TranslateBlendFactor(shader.Get_Dst_Blend_Func());
    g_draw.blendFuncBits = BGFX_STATE_BLEND_FUNC(srcBits, dstBits);
    g_draw.shaderBlendFuncBits = g_draw.blendFuncBits;
    g_draw.blendEquationBits = BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD);
    {
        bool newBlend = !(srcBits == BGFX_STATE_BLEND_ONE && dstBits == BGFX_STATE_BLEND_ZERO);
        g_draw.alphaBlendEnabled = newBlend;
        g_draw.shaderAlphaBlendEnabled = newBlend;
    }
    g_draw.alphaBlendExplicitlySet = false;
    g_draw.depthTestEnabled = true;
    g_draw.depthWriteEnabled = shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE;
    g_draw.depthFuncBits = TranslateDepthCompare(shader.Get_Depth_Compare());
    g_draw.depthFunc = static_cast<unsigned>(MapShaderDepthCompareToBackendCompare(shader.Get_Depth_Compare()));
    BuildTssOpsForShader(shader, g_draw.tssOps0, g_draw.tssOps1, &g_draw.atestRef, &g_draw.atestFunc);
    BuildTssOpsForShader(shader, g_draw.shaderTssOps0, g_draw.shaderTssOps1, &g_draw.shaderAtestRef, &g_draw.shaderAtestFunc);
    g_draw.atestEnabled = g_draw.atestFunc > 0.0f;
    g_draw.legacyPixelShaderMode[0] = static_cast<float>(RB_LEGACY_PIXEL_SHADER_NONE);
    Clear_State_Overrides();
}

void BgfxBackend::Set_Material(const VertexMaterialClass * material)
{
    g_draw.sourceMaterial = material;
    FixedFunctionState::Set_Material(material);
    const bool lightingEnabled =
        material != nullptr
        && material->Get_Lighting()
        && !WW3D::Is_Coloring_Enabled();
    FixedFunctionState::Set_Lighting_Enabled(lightingEnabled);
    g_draw.explicitMaterialState = false;
    CaptureMaterialStateForBgfx(material);
}

void BgfxBackend::Apply_Material_State(const RenderBackendMaterialState & material)
{
    g_draw.explicitMaterialState = true;
    for (int i = 0; i < 4; ++i)
    {
        g_draw.matDiffuse[i] = material.diffuse[i];
        g_draw.matAmbient[i] = material.ambient[i];
        g_draw.matEmissive[i] = material.emissive[i];
    }
}

void BgfxBackend::Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source,
                                            RenderBackendMaterialColorSource diffuse_source,
                                            RenderBackendMaterialColorSource emissive_source)
{
    FixedFunctionState::Set_Material_Color_Sources(
        static_cast<unsigned>(ambient_source),
        static_cast<unsigned>(diffuse_source),
        static_cast<unsigned>(emissive_source));
    g_draw.vertexColorFlags[1] = (diffuse_source == RB_MATERIAL_COLOR_SOURCE_COLOR1) ? 1.0f : 0.0f;
    g_draw.vertexColorFlags[2] = (ambient_source == RB_MATERIAL_COLOR_SOURCE_COLOR1) ? 1.0f : 0.0f;
    g_draw.vertexColorFlags[3] = (emissive_source == RB_MATERIAL_COLOR_SOURCE_COLOR1) ? 1.0f : 0.0f;
}

void BgfxBackend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    FixedFunctionState::Set_Texture(stage, texture);
    // Stages 0-3 wired. Covers terrain base + detail
    // + cloud + noise, the standard 4-stage layout used by the
    // FlatHeightMap pixel shader family. Stages above 3 still fall
    // through unmigrated.
    {
        TextureClass * t2d_name = texture ? texture->As_TextureClass() : nullptr;
        if (t2d_name != nullptr)
        {
            // DX8 applies the texture object's filter/address state when the
            // deferred texture bind is flushed. Apply it before later bind
            // logic interprets the current stage sampler state.
            t2d_name->Get_Filter().Apply(stage);
        }

        bgfx::TextureHandle h = EnsureBgfxTexture(texture);
        const bool missingOrUnavailable = IsMissingOrUnavailableTexture(texture, h);
        if (!bgfx::isValid(h) && texture != nullptr &&
            g_caches.renderTarget.count(texture) == 0)
        {
            static bool s_loggedWhiteFallback = false;
            if (!s_loggedWhiteFallback)
            {
                s_loggedWhiteFallback = true;
                TextureClass * t2d_fb = texture->As_TextureClass();
                WWDEBUG_SAY(("[BgfxBackend] WHITE FALLBACK: stage=%u tex=%s pool=%d",
                             stage,
                             t2d_fb ? t2d_fb->Get_Full_Path().str() : "(null)",
                             texture->Get_Pool()));
            }
        }
        // TextureFilterClass::Apply() updates the semantic sampler state above.
        // Preserve those bits when recording the bind; bridge atlases and thin
        // particles depend on mip filtering staying disabled after Set_Texture.
        const uint32_t samplerFlags = stage < 4 ? g_draw.samplerFlags[stage] : 0;
        const bool mipFilterDisabled = stage < 4 && g_draw.mipFilterDisabled[stage];
        switch (stage)
        {
            case 0: g_draw.tex[0] = h;
                    g_draw.sourceTextures[0] = texture;
                    g_draw.samplerFlags[0] = samplerFlags;
                    g_draw.mipFilterDisabled[0] = mipFilterDisabled;
                    g_draw.textureIsMissing[0] = missingOrUnavailable; break;
            case 1: g_draw.tex[1] = h;
                    g_draw.sourceTextures[1] = texture;
                    g_draw.samplerFlags[1] = samplerFlags;
                    g_draw.mipFilterDisabled[1] = mipFilterDisabled;
                    g_draw.textureIsMissing[1] = missingOrUnavailable; break;
            case 2: g_draw.tex[2] = h;
                    g_draw.sourceTextures[2] = texture;
                    g_draw.samplerFlags[2] = samplerFlags;
                    g_draw.mipFilterDisabled[2] = mipFilterDisabled;
                    g_draw.textureIsMissing[2] = missingOrUnavailable; break;
            case 3: g_draw.tex[3] = h;
                    g_draw.sourceTextures[3] = texture;
                    g_draw.samplerFlags[3] = samplerFlags;
                    g_draw.mipFilterDisabled[3] = mipFilterDisabled;
                    g_draw.textureIsMissing[3] = missingOrUnavailable; break;
            default: break;
        }
    }
}

void BgfxBackend::Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture)
{
    Set_Texture(stage, texture);
}

void BgfxBackend::Set_Ambient(const Vector3 & color)
{
    FixedFunctionState::Set_Ambient_Color(MakeLegacyARGBColor(color, 0.0f));
    g_draw.sceneAmbient[0] = color.X;
    g_draw.sceneAmbient[1] = color.Y;
    g_draw.sceneAmbient[2] = color.Z;
}

const Vector3 & BgfxBackend::Get_Ambient() const
{
    m_ambient.Set(g_draw.sceneAmbient[0], g_draw.sceneAmbient[1], g_draw.sceneAmbient[2]);
    return m_ambient;
}

void BgfxBackend::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
    (void)enable;
    (void)color;
    (void)start;
    (void)end;
}

void BgfxBackend::Set_Fog_Enable(bool enable)
{
    FixedFunctionState::Set_Fog_Enabled(enable);
}

void BgfxBackend::Set_Fog_Color(unsigned argb)
{
    FixedFunctionState::Set_Fog_Color(argb);
}

unsigned BgfxBackend::Get_Fog_Color() const
{
    return FixedFunctionState::Fog_Color(0);
}

void BgfxBackend::Set_Specular_Enable(bool enable)
{
    FixedFunctionState::Set_Specular_Enabled(enable);
}

void BgfxBackend::Set_Patch_Segments(float level)
{
    FixedFunctionState::Set_Patch_Segments_Bits(FloatAsDword(level));
}

void BgfxBackend::Set_Light(unsigned int index, const LightClass & light)
{
    if (index >= 4)
    {
        return;
    }

    Vector3 color;
    light.Get_Diffuse(&color);
    color *= light.Get_Intensity();
    g_draw.lightColors[index][0] = color.X;
    g_draw.lightColors[index][1] = color.Y;
    g_draw.lightColors[index][2] = color.Z;
    g_draw.lightColors[index][3] = 1.0f;

    light.Get_Ambient(&color);
    color *= light.Get_Intensity();
    g_draw.lightAmbients[index][0] = color.X;
    g_draw.lightAmbients[index][1] = color.Y;
    g_draw.lightAmbients[index][2] = color.Z;
    g_draw.lightAmbients[index][3] = 1.0f;

    Vector3 position = light.Get_Position();
    g_draw.lightPositions[index][0] = position.X;
    g_draw.lightPositions[index][1] = position.Y;
    g_draw.lightPositions[index][2] = position.Z;
    g_draw.lightPositions[index][3] = 1.0f;

    Vector3 direction;
    light.Get_Spot_Direction(direction);
    g_draw.lightDirs[index][0] = -direction.X;
    g_draw.lightDirs[index][1] = -direction.Y;
    g_draw.lightDirs[index][2] = -direction.Z;
    g_draw.lightDirs[index][3] = 1.0f;

    g_draw.lightParams[index][0] = 0.0f;
    g_draw.lightParams[index][1] = light.Get_Attenuation_Range();
    g_draw.lightParams[index][2] =
        (light.Get_Type() == LightClass::POINT || light.Get_Type() == LightClass::SPOT) ? 1.0f : 0.0f;
    g_draw.lightParams[index][3] = 1.0f;
}

void BgfxBackend::Clear_Light(unsigned int index)
{
    if (index >= 4)
    {
        return;
    }

    g_draw.lightDirs[index][3] = 0.0f;
    g_draw.lightColors[index][3] = 0.0f;
    g_draw.lightAmbients[index][3] = 0.0f;
    g_draw.lightParams[index][3] = 0.0f;
}

// Maps WW3D BlendFactor enum (1..11) to bgfx blend-factor bits. Index 0 unused.
static const uint64_t kBgfxBlendMap[12] = {
    0,
    BGFX_STATE_BLEND_ZERO,           // 1 = RB_BLEND_ZERO
    BGFX_STATE_BLEND_ONE,            // 2 = RB_BLEND_ONE
    BGFX_STATE_BLEND_SRC_COLOR,      // 3 = RB_BLEND_SRC_COLOR
    BGFX_STATE_BLEND_INV_SRC_COLOR,  // 4 = RB_BLEND_INV_SRC_COLOR
    BGFX_STATE_BLEND_SRC_ALPHA,      // 5 = RB_BLEND_SRC_ALPHA
    BGFX_STATE_BLEND_INV_SRC_ALPHA,  // 6 = RB_BLEND_INV_SRC_ALPHA
    BGFX_STATE_BLEND_DST_ALPHA,      // 7 = RB_BLEND_DEST_ALPHA
    BGFX_STATE_BLEND_INV_DST_ALPHA,  // 8 = RB_BLEND_INV_DEST_ALPHA
    BGFX_STATE_BLEND_DST_COLOR,      // 9 = RB_BLEND_DEST_COLOR
    BGFX_STATE_BLEND_INV_DST_COLOR,  // 10 = RB_BLEND_INV_DEST_COLOR
    BGFX_STATE_BLEND_SRC_ALPHA_SAT   // 11 = RB_BLEND_SRC_ALPHA_SAT
};

static uint64_t TranslateBlendOp(BlendOp op)
{
    switch (op)
    {
        case RB_BLEND_OP_SUBTRACT:
            return BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_SUB);
        case RB_BLEND_OP_REV_SUBTRACT:
            return BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_REVSUB);
        case RB_BLEND_OP_MIN:
            return BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MIN);
        case RB_BLEND_OP_MAX:
            return BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MAX);
        case RB_BLEND_OP_ADD:
        default:
            return BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD);
    }
}

// TheSuperHackers @fix bobtista 20/04/2026 Water rendering relies on this
// to restore SRC_ALPHA / INV_SRC_ALPHA blending after its DESTALPHA shoreline
// pass. Otherwise the DESTALPHA state set by Override_Material_Opacity()
// persists into the next draw (e.g. the faction-emblem quad on the
// command-center bib), painting it black.
void BgfxBackend::Set_Blend_Factors(BlendFactor src, BlendFactor dest)
{
    const unsigned s = static_cast<unsigned>(src);
    const unsigned d = static_cast<unsigned>(dest);
    FixedFunctionState::Set_Blend_Factors(s, d);
    if (s >= 1 && s <= 11 && d >= 1 && d <= 11)
    {
        g_draw.blendFuncBits = BGFX_STATE_BLEND_FUNC(kBgfxBlendMap[s], kBgfxBlendMap[d]);
        // TheSuperHackers @bugfix bobtista 26/05/2026 If a shader override
        // (Override_Alpha_Blend_Enable / Override_Blend) is already active,
        // propagate the explicit Set_Blend_Factors into the override so the
        // engine's intent wins. ApplyBlendState prefers g_overrides.blendBits
        // over g_draw.blendFuncBits when an override is active, which silently
        // dropped the water surface's DEST_ALPHA / INV_DEST_ALPHA shoreline
        // feather (Override_Alpha_Blend_Enable had stamped the override at
        // SRC_ALPHA / INV_SRC_ALPHA just above the explicit Set_Blend_Factors).
        if (g_overrides.blendActive)
        {
            g_overrides.SetBlend(g_draw.blendFuncBits);
        }
        if (!g_draw.alphaBlendExplicitlySet && !IsOpaqueBlend(src, dest))
        {
            g_draw.alphaBlendEnabled = true;
        }
    }
}

void BgfxBackend::Set_Blend_Op(BlendOp op)
{
    FixedFunctionState::Set_Blend_Op(static_cast<unsigned>(op));
    g_draw.blendEquationBits = TranslateBlendOp(op);
}

void BgfxBackend::Set_Alpha_Blend_Enable(bool enable)
{
    FixedFunctionState::Set_Alpha_Blend_Enabled(enable);
    g_draw.alphaBlendEnabled = enable;
    g_draw.alphaBlendExplicitlySet = true;
}

// TheSuperHackers @bugfix bobtista 28/05/2026 Each single-knob alpha-test setter now writes only its own bucket through Set_Cached_Render_State; the prior code routed through Set_Alpha_Test_State and clobbered the other two buckets with zeros whenever their semantic cache flags were still false.
void BgfxBackend::Set_Alpha_Test_Enable(bool enable)
{
    FixedFunctionState::Set_Cached_Render_State(RS::ALPHATESTENABLE, enable ? 1U : 0U);
    g_draw.atestEnabled = enable;
}

void BgfxBackend::Set_Alpha_Test_Reference(unsigned ref)
{
    FixedFunctionState::Set_Cached_Render_State(RS::ALPHAREF, ref);
    g_draw.atestRef = ref / 255.0f;
}

void BgfxBackend::Set_Alpha_Test_Function(CompareFunc func)
{
    FixedFunctionState::Set_Cached_Render_State(RS::ALPHAFUNC, static_cast<unsigned>(func));
    g_draw.atestFunc = static_cast<float>(func);
}

void BgfxBackend::Set_Normalize_Normals(bool enable)
{
    FixedFunctionState::Set_Normalize_Normals_Enabled(enable);
}

void BgfxBackend::Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend)
{
    const unsigned srcIdx = static_cast<unsigned>(srcBlend);
    const unsigned dstIdx = static_cast<unsigned>(dstBlend);
    if (srcIdx >= 1 && srcIdx <= 11 && dstIdx >= 1 && dstIdx <= 11)
    {
        g_overrides.SetBlend(BGFX_STATE_BLEND_FUNC(kBgfxBlendMap[srcIdx], kBgfxBlendMap[dstIdx]));
        g_overrides.SetBlendEnable(true);
    }
    else
    {
        static bool s_loggedBlendBad = false;
        if (!s_loggedBlendBad)
        {
            s_loggedBlendBad = true;
            WWDEBUG_SAY(("[BgfxBackend] BLEND OVERRIDE BAD VALUES: src=%u dst=%u (out of range 1-11)",
                         srcIdx, dstIdx));
        }
    }
    FixedFunctionState::Set_Blend_Factors(srcIdx, dstIdx);
}

void BgfxBackend::Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func)
{
    g_overrides.atestActive = enable;
    g_overrides.atestRef = enable ? (ref / 255.0f) : 0.0f;
    g_overrides.atestFunc = enable ? static_cast<float>(func) : 0.0f;
    // TheSuperHackers @bugfix bobtista 28/05/2026 When disabling the override, only clear ALPHATESTENABLE; preserve the existing ALPHAFUNC/ALPHAREF so downstream code observes the unmodified shader state.
    if (enable)
    {
        FixedFunctionState::Set_Alpha_Test_State(enable, ref, static_cast<unsigned>(func));
    }
    else
    {
        FixedFunctionState::Set_Cached_Render_State(RS::ALPHATESTENABLE, 0U);
    }
}

void BgfxBackend::Override_Alpha_Blend_Enable(bool enable)
{
    g_overrides.SetBlendEnable(enable);
    if (enable)
    {
        g_overrides.SetBlend(BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                  BGFX_STATE_BLEND_INV_SRC_ALPHA));
    }
    FixedFunctionState::Set_Alpha_Blend_Enabled(enable);
}

void BgfxBackend::Override_Texcoord_Index(unsigned stage, unsigned uvIndex)
{
    if (stage < 4)
    {
        g_draw.texcoordIndex[stage] = uvIndex;
    }
    if (stage == 0)
    {
        g_draw.texcoordSelect[0] = (uvIndex == 1) ? 1.0f : 0.0f;
    }
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::TEXCOORDINDEX, uvIndex);
}

void BgfxBackend::Set_Texture_Transform(unsigned stage, const Matrix4x4 & matrix)
{
    auto cacheMatrix = MakeLegacyCacheMatrix(matrix);
    FixedFunctionState::Set_Transform_Matrix(kTextureTransformStage0 + stage, cacheMatrix);

    if (stage == 0)
    {
        ReadTextureTransform(0, g_draw.texTransform0, g_draw.texTransform1);
        ReadTextureTransformZ(0, g_draw.texTransform0Z);
        g_draw.texcoordSelect[3] = 1.0f;
    }
    else if (stage == 1)
    {
        ReadTextureTransform(1, g_draw.tex1Transform0, g_draw.tex1Transform1);
        ReadTextureTransformZ(1, g_draw.tex1TransformZ);
        g_draw.texcoordSelect2[1] = 1.0f;
    }
}

void BgfxBackend::Clear_Texture_Transform(unsigned stage)
{
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::TEXCOORDINDEX, stage);
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::TEXTURETRANSFORMFLAGS, kTextureTransformDisable);

    if (stage < 4)
    {
        g_draw.texcoordIndex[stage] = stage;
        g_draw.textureTransformFlags[stage] = kTextureTransformDisable;
        g_draw.texcoordSource[stage] = 0.0f;
        g_draw.texProjected[stage] = 0.0f;
    }

    if (stage == 0)
    {
        g_draw.texcoordSelect[3] = 0.0f;
        SetIdentityTextureTransform(g_draw.texTransform0, g_draw.texTransform1);
        g_draw.texTransform0Z[0] = 0.0f;
        g_draw.texTransform0Z[1] = 0.0f;
        g_draw.texTransform0Z[2] = 1.0f;
        g_draw.texTransform0Z[3] = 0.0f;
    }
    else if (stage == 1)
    {
        g_draw.texcoordSelect2[1] = 0.0f;
        SetIdentityTextureTransform(g_draw.tex1Transform0, g_draw.tex1Transform1);
        g_draw.tex1TransformZ[0] = 0.0f;
        g_draw.tex1TransformZ[1] = 0.0f;
        g_draw.tex1TransformZ[2] = 1.0f;
        g_draw.tex1TransformZ[3] = 0.0f;
    }
}

void BgfxBackend::Set_Texture_Coord_Source(unsigned stage,
                                           RenderBackendTexcoordSource source,
                                           unsigned uv_array_index)
{
    unsigned tci = uv_array_index;
    switch (source)
    {
    case RB_TEXCOORD_MESH_UV:
        tci = kTexcoordGenPassthru | uv_array_index;
        break;
    case RB_TEXCOORD_CAMERA_SPACE_NORMAL:
        tci = kTexcoordGenCameraNormal | uv_array_index;
        break;
    case RB_TEXCOORD_CAMERA_SPACE_REFLECTION:
        tci = kTexcoordGenCameraReflection | uv_array_index;
        break;
    case RB_TEXCOORD_CAMERA_SPACE_POSITION:
        tci = kTexcoordGenCameraPosition | uv_array_index;
        break;
    }

    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::TEXCOORDINDEX, tci);
    if (stage < 4)
    {
        g_draw.texcoordIndex[stage] = tci;
        g_draw.texcoordSource[stage] = static_cast<float>(source);
    }
    if (stage == 0)
    {
        g_draw.texcoordSelect[0] = (uv_array_index == 1) ? 1.0f : 0.0f;
    }
    else if (stage == 1)
    {
        g_draw.texcoordSelect2[0] = (uv_array_index == 1) ? 1.0f : 0.0f;
    }
}

void BgfxBackend::Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected)
{
    const unsigned flags = (coord_count == 0 ? kTextureTransformDisable : coord_count)
        | (projected ? kTextureTransformProjected : 0);

    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::TEXTURETRANSFORMFLAGS, flags);
    if (stage < 4)
    {
        g_draw.textureTransformFlags[stage] = flags;
        g_draw.texProjected[stage] = projected && coord_count >= 3 ? 1.0f : 0.0f;
    }
}

void BgfxBackend::Set_Texture_Bump_Env_Matrix(unsigned stage,
                                              float m00,
                                              float m01,
                                              float m10,
                                              float m11)
{
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::BUMPENVMAT00, FloatAsDword(m00));
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::BUMPENVMAT01, FloatAsDword(m01));
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::BUMPENVMAT10, FloatAsDword(m10));
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::BUMPENVMAT11, FloatAsDword(m11));
}

void BgfxBackend::Set_Texture_Bump_Env_Luminance(unsigned stage,
                                                 float scale,
                                                 float offset)
{
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::BUMPENVLSCALE, FloatAsDword(scale));
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::BUMPENVLOFFSET, FloatAsDword(offset));
}

void BgfxBackend::Set_Texture_Color_Operation(unsigned stage, RenderBackendTextureOperation op)
{
    Set_Texture_Stage_State(stage, TSS::COLOROP, static_cast<unsigned>(op));
}

void BgfxBackend::Set_Texture_Alpha_Operation(unsigned stage, RenderBackendTextureOperation op)
{
    Set_Texture_Stage_State(stage, TSS::ALPHAOP, static_cast<unsigned>(op));
}

void BgfxBackend::Set_Texture_Color_Argument(unsigned stage,
                                             unsigned argument_index,
                                             RenderBackendTextureArgument arg)
{
    static const unsigned states[] = {
        TSS::COLORARG0,
        TSS::COLORARG1,
        TSS::COLORARG2,
    };
    if (argument_index >= sizeof(states) / sizeof(states[0]))
        return;

    Set_Texture_Stage_State(stage, states[argument_index], static_cast<unsigned>(arg));
}

void BgfxBackend::Set_Texture_Alpha_Argument(unsigned stage,
                                             unsigned argument_index,
                                             RenderBackendTextureArgument arg)
{
    static const unsigned states[] = {
        TSS::ALPHAARG0,
        TSS::ALPHAARG1,
        TSS::ALPHAARG2,
    };
    if (argument_index >= sizeof(states) / sizeof(states[0]))
        return;

    Set_Texture_Stage_State(stage, states[argument_index], static_cast<unsigned>(arg));
}

void BgfxBackend::Set_Texture_Coord_Generation(unsigned stage, bool cameraPosEnabled)
{
    Set_Texture_Coord_Source(stage,
                             cameraPosEnabled ? RB_TEXCOORD_CAMERA_SPACE_POSITION : RB_TEXCOORD_MESH_UV,
                             stage);
}

void BgfxBackend::Set_Texture_UV_Wrap(unsigned stage, bool enable)
{
    if (stage == 0)
    {
        g_draw.objectShroudDim[3] = enable ? 1.0f : 0.0f;
    }
}

static unsigned TextureAddressModeToLegacyStageState(RenderBackendTextureAddressMode mode)
{
    switch (mode)
    {
        case RB_TEXTURE_ADDRESS_CLAMP:
            return kTextureAddressClamp;
        case RB_TEXTURE_ADDRESS_BORDER:
            return kTextureAddressBorder;
        case RB_TEXTURE_ADDRESS_WRAP:
        default:
            return kTextureAddressWrap;
    }
}

void BgfxBackend::Set_Texture_Address_Mode(unsigned stage,
                                           RenderBackendTextureAddressMode u,
                                           RenderBackendTextureAddressMode v,
                                           RenderBackendTextureAddressMode w)
{
    Set_Texture_Stage_State(stage, TSS::ADDRESSU, TextureAddressModeToLegacyStageState(u));
    Set_Texture_Stage_State(stage, TSS::ADDRESSV, TextureAddressModeToLegacyStageState(v));
    Set_Texture_Stage_State(stage, TSS::ADDRESSW, TextureAddressModeToLegacyStageState(w));
}

static unsigned TextureSampleFilterToLegacyStageState(RenderBackendTextureSampleFilter filter)
{
    switch (filter)
    {
        case RB_TEXTURE_SAMPLE_NONE:
            return kTextureSampleNone;
        case RB_TEXTURE_SAMPLE_POINT:
            return kTextureSamplePoint;
        case RB_TEXTURE_SAMPLE_ANISOTROPIC:
            return kTextureSampleAnisotropic;
        case RB_TEXTURE_SAMPLE_LINEAR:
        default:
            return kTextureSampleLinear;
    }
}

void BgfxBackend::Set_Texture_Sample_Filter(unsigned stage,
                                            RenderBackendTextureSampleFilter min_filter,
                                            RenderBackendTextureSampleFilter mag_filter,
                                            RenderBackendTextureSampleFilter mip_filter)
{
    Set_Texture_Stage_State(stage, TSS::MINFILTER, TextureSampleFilterToLegacyStageState(min_filter));
    Set_Texture_Stage_State(stage, TSS::MAGFILTER, TextureSampleFilterToLegacyStageState(mag_filter));
    Set_Texture_Stage_State(stage, TSS::MIPFILTER, TextureSampleFilterToLegacyStageState(mip_filter));
}

void BgfxBackend::Set_Texture_Min_Mag_Filter(unsigned stage,
                                             RenderBackendTextureSampleFilter min_filter,
                                             RenderBackendTextureSampleFilter mag_filter)
{
    Set_Texture_Stage_State(stage, TSS::MINFILTER, TextureSampleFilterToLegacyStageState(min_filter));
    Set_Texture_Stage_State(stage, TSS::MAGFILTER, TextureSampleFilterToLegacyStageState(mag_filter));
}

void BgfxBackend::Set_Texture_Mip_Filter(unsigned stage, RenderBackendTextureSampleFilter mip_filter)
{
    Set_Texture_Stage_State(stage, TSS::MIPFILTER, TextureSampleFilterToLegacyStageState(mip_filter));
}

void BgfxBackend::Set_Texture_Max_Anisotropy(unsigned stage, unsigned max_anisotropy)
{
    Set_Texture_Stage_State(stage, TSS::MAXANISOTROPY, max_anisotropy);
}

void BgfxBackend::Set_Texture_Clamp_Mode(unsigned stage, bool clampU, bool clampV)
{
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::ADDRESSU,
        clampU ? kTextureAddressClamp : kTextureAddressWrap);
    FixedFunctionState::Set_Texture_Stage_State(stage, TSS::ADDRESSV,
        clampV ? kTextureAddressClamp : kTextureAddressWrap);

    if (stage < 4)
    {
        g_draw.samplerFlags[stage] &= ~(BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        if (clampU)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_U_CLAMP;
        }
        if (clampV)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_V_CLAMP;
        }
    }
}

void BgfxBackend::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
    FixedFunctionState::Set_Texture_Stage_State(stage, state, value);
    if (stage >= 4)
    {
        return;
    }

    if (stage == 0)
    {
        if (state == TSS::COLOROP)
        {
            g_draw.tssOps0[0] = TextureOpToTssOp(value);
        }
        else if (state == TSS::ALPHAOP)
        {
            g_draw.tssOps0[1] = TextureOpToTssOp(value);
        }
        else if (state == TSS::COLORARG1)
        {
            g_draw.tssOps1[0] = TextureArgToTssArg(value);
        }
        else if (state == TSS::ALPHAARG1)
        {
            g_draw.tssOps1[1] = TextureArgToTssArg(value);
        }
    }
    else if (stage == 1)
    {
        if (state == TSS::COLOROP)
        {
            g_draw.tssOps0[2] = TextureOpToTssOp(value);
        }
        else if (state == TSS::ALPHAOP)
        {
            g_draw.tssOps0[3] = TextureOpToTssOp(value);
        }
        else if (state == TSS::COLORARG1)
        {
            g_draw.tssOps1[2] = TextureArgToTssArg(value);
        }
        else if (state == TSS::ALPHAARG1)
        {
            g_draw.tssOps1[3] = TextureArgToTssArg(value);
        }
    }

    if (state == TSS::ADDRESSU)
    {
        g_draw.samplerFlags[stage] &= ~BGFX_SAMPLER_U_CLAMP;
        if (value == kTextureAddressClamp)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_U_CLAMP;
        }
    }
    else if (state == TSS::ADDRESSV)
    {
        g_draw.samplerFlags[stage] &= ~BGFX_SAMPLER_V_CLAMP;
        if (value == kTextureAddressClamp)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_V_CLAMP;
        }
    }
    else if (state == TSS::MINFILTER)
    {
        g_draw.samplerFlags[stage] &= ~(BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MIN_ANISOTROPIC);
        if (value == kTextureSamplePoint)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_MIN_POINT;
        }
        else if (value == kTextureSampleAnisotropic)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_MIN_ANISOTROPIC;
        }
    }
    else if (state == TSS::MAGFILTER)
    {
        g_draw.samplerFlags[stage] &= ~(BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MAG_ANISOTROPIC);
        if (value == kTextureSamplePoint)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_MAG_POINT;
        }
        else if (value == kTextureSampleAnisotropic)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_MAG_ANISOTROPIC;
        }
    }
    else if (state == TSS::MIPFILTER)
    {
        g_draw.samplerFlags[stage] &= ~BGFX_SAMPLER_MIP_POINT;
        g_draw.mipFilterDisabled[stage] = value == kTextureSampleNone;
        if (value == kTextureSamplePoint)
        {
            g_draw.samplerFlags[stage] |= BGFX_SAMPLER_MIP_POINT;
        }
    }
    else if (state == TSS::TEXCOORDINDEX)
    {
        g_draw.texcoordIndex[stage] = value;
        const unsigned uvIndex = value & 0xFFFFu;
        const unsigned texcoordGen = value & 0xFFFF0000u;
        if (stage == 3 && texcoordGen == kTexcoordGenCameraPosition)
        {
            g_draw.texcoordSource[3] = 3.0f;
        }
        else if (stage == 3)
        {
            g_draw.texcoordSource[3] = (uvIndex == 1) ? 1.0f : 0.0f;
        }
    }
    else if (state == TSS::TEXTURETRANSFORMFLAGS)
    {
        g_draw.textureTransformFlags[stage] = value;
    }
}

void BgfxBackend::Configure_Custom_Edging_Cloud_Texture_Stages()
{
    Set_Texture_Stage_State(0, TSS::ALPHAARG1, kTextureArgCurrent);
    Set_Texture_Stage_State(0, TSS::ALPHAOP, kTextureOpSelectArg1);

    Set_Texture_Stage_State(1, TSS::COLORARG1, kTextureArgCurrent);
    Set_Texture_Stage_State(1, TSS::COLORARG2, kTextureArgTexture);
    Set_Texture_Stage_State(1, TSS::COLOROP, kTextureOpSelectArg1);
    Set_Texture_Stage_State(1, TSS::ALPHAARG1, kTextureArgCurrent);
    Set_Texture_Stage_State(1, TSS::ALPHAARG2, kTextureArgTexture);
    Set_Texture_Stage_State(1, TSS::ALPHAOP, kTextureOpSelectArg2);
    Set_Texture_Stage_State(1, TSS::TEXCOORDINDEX, 1);
}

void BgfxBackend::Configure_Shadow_Volume_Fill_Texture_Stages()
{
    Set_Texture_Stage_State(0, TSS::COLORARG1, kTextureArgTexture);
    Set_Texture_Stage_State(0, TSS::COLORARG2, kTextureArgDiffuse);
    Set_Texture_Stage_State(0, TSS::COLOROP, kTextureOpSelectArg2);
    Set_Texture_Stage_State(0, TSS::ALPHAOP, kTextureOpDisable);
    Set_Texture_Stage_State(0, TSS::TEXCOORDINDEX, 0);

    Set_Texture_Stage_State(1, TSS::COLOROP, kTextureOpDisable);
    Set_Texture_Stage_State(1, TSS::ALPHAOP, kTextureOpDisable);
    Set_Texture_Stage_State(1, TSS::TEXCOORDINDEX, 1);
}

void BgfxBackend::Set_Shroud_Texture_Pass_Active(bool active, unsigned stage)
{
    g_views.shroudTexturePassActive = active;
    g_views.shroudTexturePassStage = stage;
    if (!active)
    {
        g_draw.shroudTextureParamsValid = false;
        g_views.objectShroudTexturePassActive = false;
    }
    if (!active || stage != 0)
    {
        g_draw.texcoordSelect[2] = 0.0f;
    }
}

void BgfxBackend::Set_Object_Shroud_Texture_Pass_Active(bool active)
{
    g_views.objectShroudTexturePassActive = active;
}

void BgfxBackend::Set_Object_Shroud_Alpha_Mask_Texture(TextureBaseClass * texture)
{
    g_draw.objectShroudDim[1] = texture != nullptr ? 1.0f : 0.0f;
    // The delayed object-shroud shader uses the object's base texture as an
    // alpha mask, but it renders under the shroud shader rather than the
    // object's original shader. Preserve WW3D's cutout coverage by applying
    // the same default alpha-test cutoff used by alpha-tested meshes.
    g_draw.objectShroudDim[2] = texture != nullptr ? kDefaultAlphaTestRef : 0.0f;
    if (texture == nullptr)
    {
        return;
    }

    bgfx::TextureHandle h = EnsureBgfxTexture(texture);
    g_draw.tex[1] = h;
    g_draw.sourceTextures[1] = texture;
    g_draw.textureIsMissing[1] = IsMissingOrUnavailableTexture(texture, h);

    g_draw.samplerFlags[1] = 0;
    if (TextureClass * t2d = texture->As_TextureClass())
    {
        const TextureFilterClass & flt = t2d->Get_Filter();
        if (flt.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP)
        {
            g_draw.samplerFlags[1] |= BGFX_SAMPLER_U_CLAMP;
        }
        if (flt.Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP)
        {
            g_draw.samplerFlags[1] |= BGFX_SAMPLER_V_CLAMP;
        }
    }
}

void BgfxBackend::Set_Object_Shroud_Dim_Factor(float factor)
{
    if (factor < 0.0f)
    {
        factor = 0.0f;
    }
    else if (factor > 1.0f)
    {
        factor = 1.0f;
    }
    g_draw.objectShroudDim[0] = factor;
}

void BgfxBackend::Set_Shroud_Texture_Params(float offset_x, float offset_y,
                                             float scale_x, float scale_y)
{
    g_draw.shroudTextureParams[0] = offset_x;
    g_draw.shroudTextureParams[1] = offset_y;
    g_draw.shroudTextureParams[2] = scale_x;
    g_draw.shroudTextureParams[3] = scale_y;
    g_draw.shroudTextureParamsValid = true;
}

void BgfxBackend::Override_Terrain_Blend(bool enable)
{
    g_draw.texcoordSelect[1] = enable ? 1.0f : 0.0f;
}

void BgfxBackend::Override_Material_Opacity(float opacity)
{
    // TheSuperHackers @fix bobtista 20/04/2026 Only override the opacity uniform; the water
    // code sets DESTALPHA explicitly via Set_Blend_Factors when soft water edge is enabled.
    g_draw.matDiffuse[3] = opacity;
    g_views.waterOverrideActive = true;
}

void BgfxBackend::Begin_Water_Overlay()
{
    g_views.waterOverlayActive = true;
}

void BgfxBackend::End_Water_Overlay()
{
    g_views.waterOverlayActive = false;
}

void BgfxBackend::Begin_Effect_Overlay()
{
    if (std::getenv("GGC_NO_EFFECT_OVERLAY") != nullptr)
    {
        return;
    }
    g_views.effectOverlayActive = true;
}

void BgfxBackend::End_Effect_Overlay()
{
    g_views.effectOverlayActive = false;
}

bool BgfxBackend::Begin_Smudge_Distortion()
{
    const BgfxDiagnosticFlags diagnostics = GetBgfxDiagnosticFlags();
    if (diagnostics.noSceneFramebuffer
        || diagnostics.noPostFx
        || !bgfx::isValid(g_device.sceneColor)
        || !bgfx::isValid(g_device.sceneSmudgeCopy)
        || !bgfx::isValid(g_device.smudgeProgram))
    {
        return false;
    }

    bgfx::blit(kBgfxSmudgeCopyView, g_device.sceneSmudgeCopy, 0, 0,
               g_device.sceneColor);
    g_views.smudgeActive = true;
    return true;
}

void BgfxBackend::End_Smudge_Distortion()
{
    g_views.smudgeActive = false;
}

void BgfxBackend::Set_Tree_Shader_Constants(const float swayTable[11][4],
                                            const float shroudOffset[4],
                                            const float shroudScale[4])
{
    std::memcpy(g_draw.swayTable,    swayTable,    sizeof(g_draw.swayTable));
    std::memcpy(g_draw.shroudOffset, shroudOffset, sizeof(g_draw.shroudOffset));
    std::memcpy(g_draw.shroudScale,  shroudScale,  sizeof(g_draw.shroudScale));
}

void BgfxBackend::Set_Tree_Vertex_Shader_Active(bool active)
{
    g_views.treeShaderActive = active;
}

void BgfxBackend::Set_Grayscale_Mode(bool enable)
{
    g_draw.grayscaleEnable[0] = enable ? 1.0f : 0.0f;
}

void BgfxBackend::Set_Cloud_Shadow_Params(bool enable, float scroll_x, float scroll_y,
                                          float stretch, TextureClass * cloud_tex)
{
    if (std::getenv("GGC_NO_CLOUD_SHADOWS") != nullptr)
    {
        enable = false;
        cloud_tex = nullptr;
    }

    g_draw.cloudParams[0] = scroll_x;
    g_draw.cloudParams[1] = scroll_y;
    g_draw.cloudParams[2] = stretch;
    g_draw.cloudParams[3] = enable ? 1.0f : 0.0f;

    if (enable && cloud_tex != nullptr)
    {
        g_draw.cloudTex = EnsureBgfxTexture(cloud_tex);
    }
    else
    {
        g_draw.cloudTex = BGFX_INVALID_HANDLE;
    }
}

void BgfxBackend::Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha)
{
    uint64_t mask = 0;
    if (red)
    {
        mask |= BGFX_STATE_WRITE_R;
    }
    if (green)
    {
        mask |= BGFX_STATE_WRITE_G;
    }
    if (blue)
    {
        mask |= BGFX_STATE_WRITE_B;
    }
    if (alpha)
    {
        mask |= BGFX_STATE_WRITE_A;
    }
    unsigned d3dMask = 0;
    if (red)
    {
        d3dMask |= RB_COLOR_RED;
    }
    if (green)
    {
        d3dMask |= RB_COLOR_GREEN;
    }
    if (blue)
    {
        d3dMask |= RB_COLOR_BLUE;
    }
    if (alpha)
    {
        d3dMask |= RB_COLOR_ALPHA;
    }
    FixedFunctionState::Set_Color_Write_Mask(d3dMask);
    g_overrides.colorWriteOverride = static_cast<int>(mask);
    g_overrides.suppressDraw = false;
}

// TheSuperHackers @refactor bobtista 15/04/2026 Mirror the
// DWORD variant into g_overrides.colorWriteOverride so stencil shadow volume
// passes that call Set_Color_Write_Mask(0) actually disable bgfx color writes.
unsigned BgfxBackend::Get_Color_Write_Mask() const
{
    return FixedFunctionState::Color_Write_Mask(
        RB_COLOR_RED | RB_COLOR_GREEN | RB_COLOR_BLUE | RB_COLOR_ALPHA);
}

void BgfxBackend::Set_Color_Write_Mask(unsigned mask)
{
    FixedFunctionState::Set_Color_Write_Mask(mask);
    uint64_t bgfxMask = 0;
    if (mask & RB_COLOR_RED)
    {
        bgfxMask |= BGFX_STATE_WRITE_R;
    }
    if (mask & RB_COLOR_GREEN)
    {
        bgfxMask |= BGFX_STATE_WRITE_G;
    }
    if (mask & RB_COLOR_BLUE)
    {
        bgfxMask |= BGFX_STATE_WRITE_B;
    }
    if (mask & RB_COLOR_ALPHA)
    {
        bgfxMask |= BGFX_STATE_WRITE_A;
    }
    g_overrides.colorWriteOverride = static_cast<int>(bgfxMask);
    g_overrides.suppressDraw = false;
}

void BgfxBackend::Set_Lighting_Enable(bool enable)
{
    FixedFunctionState::Set_Lighting_Enabled(enable);
    g_draw.lightingEnabled[0] = enable ? 1.0f : 0.0f;
}

void BgfxBackend::Set_Point_Sprite_Enable(bool enable)
{
    FixedFunctionState::Set_Point_Sprite_Enabled(enable);
}

void BgfxBackend::Set_Point_Scale_Enable(bool enable)
{
    FixedFunctionState::Set_Point_Scale_Enabled(enable);
}

void BgfxBackend::Set_Point_Size(float size, float min_size, float max_size)
{
    FixedFunctionState::Set_Point_Size_Bits(
        FloatAsDword(size),
        FloatAsDword(min_size),
        FloatAsDword(max_size));
}

void BgfxBackend::Set_Point_Scale(float a, float b, float c)
{
    FixedFunctionState::Set_Point_Scale_Bits(
        FloatAsDword(a),
        FloatAsDword(b),
        FloatAsDword(c));
}

void BgfxBackend::Skip_Next_Bgfx_Submit()
{
    g_views.skipNextSubmitEngineDraw = true;
}

void BgfxBackend::Set_Projected_Shadow_Decal_Active(bool active)
{
    Set_Projected_Decal_Mode(active ? RB_PROJECTED_DECAL_BLOB_SHADOW : RB_PROJECTED_DECAL_NONE);
}

void BgfxBackend::Set_Projected_Decal_Mode(RenderBackendProjectedDecalMode mode)
{
    g_views.projectedDecalMode = static_cast<unsigned>(mode);
    g_views.projectedShadowDecalActive = mode != RB_PROJECTED_DECAL_NONE;
}

// TheSuperHackers @fix bobtista 16/04/2026 Remove g_draw.matDiffuse aliasing from texture factor.
// The shadow decal draw is now skipped via Skip_Next_Bgfx_Submit so the aliasing
// is unnecessary and it clobbers team colors.
void BgfxBackend::Set_Texture_Factor(unsigned argb)
{
    FixedFunctionState::Set_Texture_Factor(argb);
}

void BgfxBackend::Set_Shadow_Volume_Shader_Active(bool active)
{
    g_views.shadowVolumeActive = active;
    UpdateShadowStencilState();
}

void BgfxBackend::Submit_Shadow_Volume_Caps(unsigned strip_start_vertex,
                                            unsigned num_silhouette_verts)
{
    // Called after the engine's side-wall Draw_Triangles for a volume.
    // Generates a front cap (fan of caster-level verts, preserving the
    // engine's silhouette winding) and a back cap (fan of extruded verts,
    // reversed winding so outward normals point away from the light)
    // as a transient index buffer referencing the already-bound dynamic
    // vertex buffer. Submits to view 6 with the same stencil/cull state
    // the side walls used — the caller is mid-pass (INCR or DECRSAT).

    if (!LegacyStencilShadowsEnabled()
        || !g_device.initialized
        || !g_views.shadowVolumeActive
        || !bgfx::isValid(g_device.shadowVolumeProgram)
        || !bgfx::isValid(g_draw.vb)
        || num_silhouette_verts < 3)
    {
        LogBgfxStencilShadowEvent("caps-skip", "disabled-or-invalid",
                                  num_silhouette_verts, 0);
        return;
    }

    const unsigned tris_per_cap = num_silhouette_verts - 2;
    const unsigned total_indices = 2 * tris_per_cap * 3;

    if (bgfx::getAvailTransientIndexBuffer(total_indices) < total_indices)
    {
        LogBgfxStencilShadowEvent("caps-skip", "no-transient-index-buffer",
                                  num_silhouette_verts, total_indices);
        return;
    }

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, total_indices);
    g_stats.transientIbAllocations++;
    uint16_t * idx = reinterpret_cast<uint16_t *>(tib.data);

    // Front cap: fan around caster-level verts. Winding FLIPPED vs
    // initial guess — the engine's silhouette traversal direction may
    // not match our assumed "outward = up" for the front cap.
    for (unsigned i = 1; i < num_silhouette_verts - 1; ++i)
    {
        *idx++ = static_cast<uint16_t>(strip_start_vertex + 0);
        *idx++ = static_cast<uint16_t>(strip_start_vertex + 2 * (i + 1));
        *idx++ = static_cast<uint16_t>(strip_start_vertex + 2 * i);
    }
    // Back cap: fan around extruded verts (odd offsets), winding opposite
    // of front cap (so opposite outward normal).
    for (unsigned i = 1; i < num_silhouette_verts - 1; ++i)
    {
        *idx++ = static_cast<uint16_t>(strip_start_vertex + 1);
        *idx++ = static_cast<uint16_t>(strip_start_vertex + 2 * i + 1);
        *idx++ = static_cast<uint16_t>(strip_start_vertex + 2 * (i + 1) + 1);
    }

    // Skip second pass — single-pass two-sided handles both faces.
    if (g_draw.stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECR
        || g_draw.stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECRSAT)
    {
        LogBgfxStencilShadowEvent("caps-skip", "second-pass-two-sided",
                                  num_silhouette_verts, total_indices);
        return;
    }

    // No face culling, two-sided stencil matching the side-wall submit.
    uint64_t state = BgfxShadowVolumeDepthState();
    bgfx::setState(state);
    const uint32_t commonBits = g_draw.stencilFuncBits
        | BGFX_STENCIL_FUNC_REF(g_draw.stencilRef & 0xFF)
        | BGFX_STENCIL_FUNC_RMASK(g_draw.stencilReadMask & 0xFF)
        | g_draw.stencilFailOpBits
        | g_draw.stencilZFailOpBits;
    bgfx::setStencil(commonBits | BGFX_STENCIL_OP_PASS_Z_DECRSAT,
                     commonBits | BGFX_STENCIL_OP_PASS_Z_INCRSAT);

    bgfx::setVertexBuffer(0, g_draw.vb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTransform(g_frame.world);
    BindShadowVolumeBiasUniform();

    bgfx::submit(BgfxShadowVolumeSubmitView(), g_device.shadowVolumeProgram);
    g_stats.shadowVolumeSubmits++;
    LogBgfxStencilShadowEvent("caps-submit", nullptr,
                              num_silhouette_verts, total_indices);
}

void BgfxBackend::Submit_Shadow_Volume_Triangulated_Caps(
    unsigned strip_start_vertex,
    const short * local_cap_indices,
    unsigned cap_index_count)
{
    if (!LegacyStencilShadowsEnabled()
        || !g_device.initialized
        || !g_views.shadowVolumeActive
        || !bgfx::isValid(g_device.shadowVolumeProgram)
        || !bgfx::isValid(g_draw.vb)
        || local_cap_indices == nullptr
        || cap_index_count < 3)
    {
        LogBgfxStencilShadowEvent("tri-caps-skip", "disabled-or-invalid",
                                  cap_index_count, 0);
        return;
    }

    // front cap + back cap (reversed winding)
    const unsigned total_indices = cap_index_count * 2;

    if (bgfx::getAvailTransientIndexBuffer(total_indices) < total_indices)
    {
        LogBgfxStencilShadowEvent("tri-caps-skip", "no-transient-index-buffer",
                                  cap_index_count, total_indices);
        return;
    }

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, total_indices);
    g_stats.transientIbAllocations++;
    uint16_t * idx = reinterpret_cast<uint16_t *>(tib.data);

    // Front cap: local indices map to caster-level verts at
    // strip_start + 2*local (even offsets). Winding preserved.
    for (unsigned i = 0; i + 2 < cap_index_count; i += 3)
    {
        *idx++ = static_cast<uint16_t>(
            strip_start_vertex + 2 * static_cast<unsigned>(local_cap_indices[i + 0]));
        *idx++ = static_cast<uint16_t>(
            strip_start_vertex + 2 * static_cast<unsigned>(local_cap_indices[i + 1]));
        *idx++ = static_cast<uint16_t>(
            strip_start_vertex + 2 * static_cast<unsigned>(local_cap_indices[i + 2]));
    }
    // Back cap: local indices map to extruded verts at
    // strip_start + 2*local + 1 (odd offsets). Winding REVERSED so
    // outward normal points away from light (opposite of front cap).
    for (unsigned i = 0; i + 2 < cap_index_count; i += 3)
    {
        *idx++ = static_cast<uint16_t>(
            strip_start_vertex + 2 * static_cast<unsigned>(local_cap_indices[i + 0]) + 1);
        *idx++ = static_cast<uint16_t>(
            strip_start_vertex + 2 * static_cast<unsigned>(local_cap_indices[i + 2]) + 1);
        *idx++ = static_cast<uint16_t>(
            strip_start_vertex + 2 * static_cast<unsigned>(local_cap_indices[i + 1]) + 1);
    }

    // Mirror the side-wall submit's state.
    uint64_t state = BgfxShadowVolumeDepthState();
    if (g_draw.cullModeBits == 1)
    {
        state |= BGFX_STATE_CULL_CW;
    }
    else if (g_draw.cullModeBits == 2)
    {
        state |= BGFX_STATE_CULL_CCW;
    }
    bgfx::setState(state);
    bgfx::setStencil(g_draw.shadowStencilFront, g_draw.shadowStencilBack);

    bgfx::setVertexBuffer(0, g_draw.vb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTransform(g_frame.world);
    BindShadowVolumeBiasUniform();

    bgfx::submit(BgfxShadowVolumeSubmitView(), g_device.shadowVolumeProgram);
    g_stats.shadowVolumeSubmits++;
    LogBgfxStencilShadowEvent("tri-caps-submit", nullptr,
                              cap_index_count, total_indices);
}

bool BgfxBackend::Needs_Closed_Shadow_Volumes() const
{
    return LegacyStencilShadowsEnabled()
        && std::getenv("GGC_BGFX_CLOSED_SHADOW_VOLUMES") != nullptr;
}

void BgfxBackend::Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                              unsigned stencil_read_mask,
                                              unsigned stencil_ref,
                                              int /*x*/,
                                              int /*y*/,
                                              int /*width*/,
                                              int /*height*/)
{
    if (!LegacyStencilShadowsEnabled()
        || std::getenv("GGC_BGFX_STENCIL_NO_APPLY") != nullptr
        || !g_device.initialized
        || !bgfx::isValid(g_device.shadowApplyProgram))
    {
        LogBgfxStencilShadowEvent("darken-skip", "disabled-or-invalid",
                                  stencil_read_mask, stencil_ref);
        return;
    }
    // The legacy DX8 shadow manager draws its volume geometry through raw D3D
    // state in some paths. bgfx only sees stencil writes that are explicitly
    // submitted through this backend. If no bgfx shadow volume touched stencil
    // this frame, the fullscreen darken quad would read stale stencil contents
    // left by unrelated passes and darken buildings/terrain based on camera
    // position. Only apply the darken pass when bgfx populated the matching
    // shadow stencil first.
    if (g_stats.shadowVolumeSubmits == 0)
    {
        LogBgfxStencilShadowEvent("darken-skip", "no-volume-submits",
                                  stencil_read_mask, stencil_ref);
        return;
    }

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();

    if (!bgfx::allocTransientBuffers(&tvb, layout, 4, &tib, 6))
    {
        LogBgfxStencilShadowEvent("darken-skip", "no-transient-buffers",
                                  stencil_read_mask, stencil_ref);
        return;
    }
    g_stats.transientVbAllocations++;
    g_stats.transientIbAllocations++;

    float * verts = (float *)tvb.data;
    verts[0] = -1.0f; verts[1] = -1.0f; verts[2] = 0.0f;
    verts[3] =  1.0f; verts[4] = -1.0f; verts[5] = 0.0f;
    verts[6] = -1.0f; verts[7] =  1.0f; verts[8] = 0.0f;
    verts[9] =  1.0f; verts[10] =  1.0f; verts[11] = 0.0f;

    uint16_t * idx = (uint16_t *)tib.data;
    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 2; idx[4] = 1; idx[5] = 3;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);

    float color[4];
    color[0] = static_cast<float>((shadow_color >> 16) & 0xFF) / 255.0f;
    color[1] = static_cast<float>((shadow_color >>  8) & 0xFF) / 255.0f;
    color[2] = static_cast<float>((shadow_color      ) & 0xFF) / 255.0f;
    color[3] = static_cast<float>((shadow_color >> 24) & 0xFF) / 255.0f;
    if (bgfx::isValid(g_uniforms.uShadowColor))
    {
        bgfx::setUniform(g_uniforms.uShadowColor, color);
    }

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
        | BGFX_STATE_DEPTH_TEST_ALWAYS
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
    bgfx::setState(state);

    // Shadow volumes leave a non-zero count in stencil where the fullscreen
    // darken quad should apply. Use an explicit front/back state because the
    // clip-space quad winding differs across backends.
    uint32_t stencil = BGFX_STENCIL_TEST_NOTEQUAL
        | BGFX_STENCIL_FUNC_REF(0)
        | BGFX_STENCIL_FUNC_RMASK(stencil_read_mask & 0xFF)
        | BGFX_STENCIL_OP_FAIL_S_KEEP
        | BGFX_STENCIL_OP_FAIL_Z_KEEP
        | BGFX_STENCIL_OP_PASS_Z_KEEP;
    bgfx::setStencil(stencil, stencil);

    bgfx::submit(BgfxShadowVolumeSubmitView(), g_device.shadowApplyProgram);
    g_stats.shadowApplySubmits++;
    LogBgfxStencilShadowEvent("darken-submit", nullptr,
                              stencil_read_mask, stencil_ref);
}

void BgfxBackend::Set_Stencil_Enable(bool enable)
{
    FixedFunctionState::Set_Stencil_Enabled(enable);
    g_draw.stencilEnabled = enable;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Func(CompareFunc f)
{
    FixedFunctionState::Set_Stencil_Function(static_cast<unsigned>(f));
    g_draw.stencilFuncBits = MapCmpFuncToBgfxStencilTest(f);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Ref(unsigned ref)
{
    FixedFunctionState::Set_Stencil_Reference(ref);
    g_draw.stencilRef = ref;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Mask(unsigned mask)
{
    FixedFunctionState::Set_Stencil_Read_Mask(mask);
    g_draw.stencilReadMask = mask;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Write_Mask(unsigned mask)
{
    FixedFunctionState::Set_Stencil_Write_Mask(mask);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Pass_Op(StencilOp op)
{
    FixedFunctionState::Set_Stencil_Pass_Op(static_cast<unsigned>(op));
    g_draw.stencilPassOpBits = MapStencilOpToBgfx(op, BGFX_STENCIL_OP_PASS_Z_SHIFT);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Fail_Op(StencilOp op)
{
    FixedFunctionState::Set_Stencil_Fail_Op(static_cast<unsigned>(op));
    g_draw.stencilFailOpBits = MapStencilOpToBgfx(op, BGFX_STENCIL_OP_FAIL_S_SHIFT);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_ZFail_Op(StencilOp op)
{
    FixedFunctionState::Set_Stencil_ZFail_Op(static_cast<unsigned>(op));
    g_draw.stencilZFailOpBits = MapStencilOpToBgfx(op, BGFX_STENCIL_OP_FAIL_Z_SHIFT);
    UpdateShadowStencilState();
}

CullMode BgfxBackend::Get_Cull_Mode() const
{
    return static_cast<CullMode>(FixedFunctionState::Cull_Mode(RB_CULL_NONE));
}

void BgfxBackend::Set_Cull_Mode(CullMode mode)
{
    FixedFunctionState::Set_Cull_Mode(static_cast<unsigned>(mode));
    switch (mode)
    {
        case RB_CULL_CW:  g_draw.cullModeBits = 1; break;
        case RB_CULL_CCW: g_draw.cullModeBits = 2; break;
        case RB_CULL_NONE:
        default:          g_draw.cullModeBits = 0; break;
    }
}

void BgfxBackend::Set_Z_Bias(int bias)
{
    FixedFunctionState::Set_Z_Bias(bias);
    g_draw.zBiasUnits = static_cast<unsigned>(bias) & 0xFFu;
}

void BgfxBackend::Set_Normal_Bias(float bias)
{
    g_draw.normalBias[0] = bias;
}

void BgfxBackend::Set_Fill_Mode(FillMode mode)
{
    FixedFunctionState::Set_Fill_Mode(static_cast<unsigned>(mode));
}

void BgfxBackend::Set_Shade_Mode(ShadeMode mode)
{
    FixedFunctionState::Set_Shade_Mode(static_cast<unsigned>(mode));
}

void BgfxBackend::Set_Depth_Test_Enable(bool enable)
{
    FixedFunctionState::Set_Depth_Test_Enabled(enable);
    g_draw.depthTestEnabled = enable;
}

void BgfxBackend::Set_Depth_Write_Enable(bool enable)
{
    FixedFunctionState::Set_Depth_Write_Enabled(enable);
    g_draw.depthWriteEnabled = enable;
}

void BgfxBackend::Set_Depth_Func(CompareFunc func)
{
    const unsigned idx = static_cast<unsigned>(func);
    FixedFunctionState::Set_Depth_Function(idx);
    g_draw.depthFunc = idx;
    static const uint64_t kDepthMap[] = {
        0,                              // 0 (unused)
        BGFX_STATE_DEPTH_TEST_NEVER,    // RB_CMP_NEVER = 1
        BGFX_STATE_DEPTH_TEST_LESS,     // RB_CMP_LESS = 2
        BGFX_STATE_DEPTH_TEST_EQUAL,    // RB_CMP_EQUAL = 3
        BGFX_STATE_DEPTH_TEST_LEQUAL,   // RB_CMP_LESS_EQUAL = 4
        BGFX_STATE_DEPTH_TEST_GREATER,  // RB_CMP_GREATER = 5
        BGFX_STATE_DEPTH_TEST_NOTEQUAL, // RB_CMP_NOT_EQUAL = 6
        BGFX_STATE_DEPTH_TEST_GEQUAL,   // RB_CMP_GREATER_EQUAL = 7
        BGFX_STATE_DEPTH_TEST_ALWAYS,   // RB_CMP_ALWAYS = 8
    };
    if (idx < 9)
    {
        g_draw.depthFuncBits = kDepthMap[idx];
    }
}

static bgfx::TextureFormat::Enum Resolve_Render_Target_Color_Format(WW3DFormat format)
{
    bgfx::TextureFormat::Enum bgfxFormat = TranslateWW3DFormat(format);
    const bgfx::Caps *caps = bgfx::getCaps();
    if (caps != nullptr
        && bgfxFormat != bgfx::TextureFormat::Unknown
        && (caps->formats[bgfxFormat] & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0)
    {
        return bgfxFormat;
    }

    if (caps != nullptr
        && (caps->formats[bgfx::TextureFormat::BGRA8] & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0)
    {
        return bgfx::TextureFormat::BGRA8;
    }
    return bgfx::TextureFormat::RGBA8;
}

static const BgfxFramebufferEntry *Ensure_Render_Target_Framebuffer(TextureClass *texture)
{
    if (texture == nullptr || !g_device.initialized)
    {
        return nullptr;
    }

    auto it = g_caches.framebuffer.find(texture);
    if (it == g_caches.framebuffer.end())
    {
        const uint16_t w = static_cast<uint16_t>(texture->Get_Width());
        const uint16_t h = static_cast<uint16_t>(texture->Get_Height());
        const bgfx::TextureFormat::Enum colorFormat =
            Resolve_Render_Target_Color_Format(texture->Get_Texture_Format());

        bgfx::TextureHandle colorTex = bgfx::createTexture2D(
            w, h, false, 1, colorFormat,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle depthTex = bgfx::createTexture2D(
            w, h, false, 1, bgfx::TextureFormat::D24S8,
            BGFX_TEXTURE_RT_WRITE_ONLY);

        bgfx::TextureHandle attachments[2] = { colorTex, depthTex };
        bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(2, attachments, true);
        if (!bgfx::isValid(fb))
        {
            if (bgfx::isValid(colorTex)) {
                bgfx::destroy(colorTex);
            }
            if (bgfx::isValid(depthTex)) {
                bgfx::destroy(depthTex);
            }
            return nullptr;
        }

        BgfxFramebufferEntry entry = { fb, colorTex, w, h };
        g_caches.framebuffer[texture] = entry;
        it = g_caches.framebuffer.find(texture);

        WWDEBUG_SAY(("[BgfxBackend] RTT framebuffer created %dx%d for tex=%p",
                     w, h, texture));
    }

    return &it->second;
}

void BgfxBackend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    // The engine-supplied depth target is intentionally unused; the bgfx framebuffer allocates its own D24S8 attachment.
    (void)ztexture;
    if (texture == nullptr || !g_device.initialized)
    {
        g_views.renderToTexture = false;
        g_views.renderTargetTexture = nullptr;
        return;
    }

    const BgfxFramebufferEntry *entry = Ensure_Render_Target_Framebuffer(texture);
    if (entry == nullptr) {
        g_views.renderToTexture = false;
        g_views.renderTargetTexture = nullptr;
        return;
    }

    bgfx::setViewFrameBuffer(kBgfxRTTView, entry->fb);
    bgfx::setViewRect(kBgfxRTTView, 0, 0, entry->width, entry->height);
    bgfx::setViewClear(kBgfxRTTView,
                        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                        0x000000ff, 1.0f, 0);
    bgfx::touch(kBgfxRTTView);

    g_views.renderToTexture = true;
    g_views.renderTargetTexture = texture;
}

void BgfxBackend::Clear_State_Overrides()
{
    g_overrides.Reset();
    g_draw.texcoordSelect[0] = 0.0f;
    g_draw.objectShroudDim[3] = 0.0f;
    g_draw.delayedObjectShroudPass = false;
    // Do NOT clear g_draw.texcoordSelect[1] (terrain blend) here.
    // Override_Terrain_Blend is called from the shader manager BEFORE
    // Set_Shader, so clearing it in Set_Shader (which calls us) would
    // undo the terrain blend flag every frame. Terrain blend is reset
    // at Begin_Scene (above) and by Override_Terrain_Blend(false).
}

static LightEnvironmentClass * g_lastLightEnv = nullptr;

void BgfxBackend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    if (light_env != nullptr)
    {
        g_lastLightEnv = light_env;
        const Vector3 & ambient = light_env->Get_Equivalent_Ambient();
        FixedFunctionState::Set_Ambient_Color(MakeLegacyARGBColor(ambient, 0.0f));
        g_draw.sceneAmbient[0] = ambient.X;
        g_draw.sceneAmbient[1] = ambient.Y;
        g_draw.sceneAmbient[2] = ambient.Z;

        const int count = light_env->Get_Light_Count();
        for (int i = 0; i < 4; ++i)
        {
            if (i < count)
            {
                const Vector3 & dir = light_env->Get_Light_Direction(i);
                g_draw.lightDirs[i][0] = dir.X;
                g_draw.lightDirs[i][1] = dir.Y;
                g_draw.lightDirs[i][2] = dir.Z;
                g_draw.lightDirs[i][3] = 1.0f; // enabled
                if (light_env->isPointLight(i))
                {
                    const Vector3 & dif = light_env->getPointDiffuse(i);
                    const Vector3 & amb = light_env->getPointAmbient(i);
                    const Vector3 & pos = light_env->getPointCenter(i);
                    g_draw.lightColors[i][0] = dif.X;
                    g_draw.lightColors[i][1] = dif.Y;
                    g_draw.lightColors[i][2] = dif.Z;
                    g_draw.lightAmbients[i][0] = amb.X;
                    g_draw.lightAmbients[i][1] = amb.Y;
                    g_draw.lightAmbients[i][2] = amb.Z;
                    g_draw.lightPositions[i][0] = pos.X;
                    g_draw.lightPositions[i][1] = pos.Y;
                    g_draw.lightPositions[i][2] = pos.Z;
                    g_draw.lightParams[i][0] = light_env->getPointIrad(i);
                    g_draw.lightParams[i][1] = light_env->getPointOrad(i);
                    g_draw.lightParams[i][2] = 1.0f;
                    g_draw.lightParams[i][3] = 1.0f;
                }
                else
                {
                    const Vector3 & dif = light_env->Get_Light_Diffuse(i);
                    g_draw.lightColors[i][0] = dif.X;
                    g_draw.lightColors[i][1] = dif.Y;
                    g_draw.lightColors[i][2] = dif.Z;
                    g_draw.lightAmbients[i][0] = 0.0f;
                    g_draw.lightAmbients[i][1] = 0.0f;
                    g_draw.lightAmbients[i][2] = 0.0f;
                    g_draw.lightPositions[i][0] = 0.0f;
                    g_draw.lightPositions[i][1] = 0.0f;
                    g_draw.lightPositions[i][2] = 0.0f;
                    g_draw.lightParams[i][0] = 0.0f;
                    g_draw.lightParams[i][1] = 0.0f;
                    g_draw.lightParams[i][2] = 0.0f;
                    g_draw.lightParams[i][3] = 1.0f;
                }
                g_draw.lightColors[i][3] = 1.0f;
            }
            else
            {
                g_draw.lightDirs[i][3] = 0.0f; // disabled
                g_draw.lightColors[i][3] = 0.0f;
                g_draw.lightAmbients[i][3] = 0.0f;
                g_draw.lightParams[i][3] = 0.0f;
            }
        }
    }
}

// -- Transforms --------------------------------------------------------------

void BgfxBackend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    CacheTransform(transform, m);
    switch (transform)
    {
        case RB_TRANSFORM_WORLD:
            W3DMatrix4ToBgfx(m, g_frame.world);
            break;
        case RB_TRANSFORM_VIEW:
            W3DMatrix4ToBgfx(m, g_frame.view);
            g_frame.cameraProjDirty = true;
            g_views.overlay2DActive = false;
            break;
        case RB_TRANSFORM_PROJECTION:
            W3DMatrix4ToBgfx(m, g_frame.proj);
            g_frame.cameraProjDirty = true;
            break;
        default:
            break;
    }
}

void BgfxBackend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    CacheTransform(transform, m);
    switch (transform)
    {
        case RB_TRANSFORM_WORLD:
            W3DMatrix3DToBgfx(m, g_frame.world);
            break;
        case RB_TRANSFORM_VIEW:
            W3DMatrix3DToBgfx(m, g_frame.view);
            g_frame.cameraProjDirty = true;
            g_views.overlay2DActive = false;
            break;
        default:
            break;
    }
}

void BgfxBackend::Get_Transform(TransformKind transform, Matrix4x4 & m) const
{
    auto matrix = MakeIdentityLegacyCacheMatrix();
    FixedFunctionState::Transform_Matrix(static_cast<unsigned>(transform), matrix);
    m = To_Matrix4x4(matrix);
}

void BgfxBackend::Set_World_Identity()
{
    CacheIdentityTransform(RB_TRANSFORM_WORLD);
    FixedFunctionState::Set_World_Identity();
    IdentityMatrix(g_frame.world);
}

void BgfxBackend::Set_View_Identity()
{
    CacheIdentityTransform(RB_TRANSFORM_VIEW);
    FixedFunctionState::Set_View_Identity();
    IdentityMatrix(g_frame.view);
    g_frame.cameraProjDirty = true;
    g_views.overlay2DActive = true;
}

bool BgfxBackend::Is_World_Identity() const
{
    return IsCachedTransformIdentity(RB_TRANSFORM_WORLD);
}

bool BgfxBackend::Is_View_Identity() const
{
    return IsCachedTransformIdentity(RB_TRANSFORM_VIEW);
}

void BgfxBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
                                                       float znear, float zfar)
{
    (void)znear;
    (void)zfar;
    CacheTransform(RB_TRANSFORM_PROJECTION, matrix);
    W3DMatrix4ToBgfx(matrix, g_frame.proj);
    g_frame.cameraProjDirty = true;

}

// -- Draw calls --------------------------------------------------------------

namespace
{
// TheSuperHackers @refactor bobtista 11/04/2026 bgfx submit. Called from
// both Draw_Triangles overloads when we have a valid cached VB + IB +
// program. State and program were cached by Set_Shader; the buffers were
// cached by Set_Vertex_Buffer / Set_Index_Buffer.
void SubmitEngineDraw(unsigned short start_index,
                      unsigned short polygon_count,
                      unsigned short min_vertex_index,
                      unsigned short vertex_count,
                      bool triangle_strip = false)
{
    if (g_overrides.suppressDraw)
    {
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }
    if (!g_device.initialized)
    {
        return;
    }
    g_stats.drawCalls++;
    if (!g_draw.alphaBlendExplicitlySet)
    {
        g_draw.alphaBlendEnabled = g_draw.shaderAlphaBlendEnabled;
        if (g_draw.alphaBlendEnabled)
        {
            g_draw.blendFuncBits = g_draw.shaderBlendFuncBits;
        }
        g_draw.atestRef = g_draw.shaderAtestRef;
        g_draw.atestFunc = g_draw.shaderAtestFunc;
        g_draw.atestEnabled = g_draw.atestFunc > 0.0f;
    }
    if (!bgfx::isValid(g_draw.program))
    {
        LogBgfxEffectSubmit("submit-engine",
                            g_views.inSortFlush ? kBgfxEngineSortView : kBgfxEngineView,
                            polygon_count,
                            vertex_count,
                            g_draw.state,
                            "skip-no-program");
        g_stats.skippedDraws++;
        return;
    }
    const bool have_vb = g_draw.useTransientVB
        || (g_draw.useStaticVB && bgfx::isValid(g_draw.staticVB))
        || bgfx::isValid(g_draw.vb);
    const bool have_ib = g_draw.useTransientIB
        || (g_draw.useStaticIB && bgfx::isValid(g_draw.staticIB))
        || bgfx::isValid(g_draw.ib);
    if (!have_vb || !have_ib)
    {
        LogBgfxEffectSubmit("submit-engine",
                            g_views.inSortFlush ? kBgfxEngineSortView : kBgfxEngineView,
                            polygon_count,
                            vertex_count,
                            g_draw.state,
                            !have_vb && !have_ib ? "skip-no-vb-ib" : (!have_vb ? "skip-no-vb" : "skip-no-ib"));
        g_stats.skippedDraws++;
        return;
    }
    if (g_draw.useTransientVB)
    {
        g_stats.transientVbDraws++;
    }
    if (g_draw.useTransientIB)
    {
        g_stats.transientIbDraws++;
    }

    // Route to the dedicated sorted view when the sort
    // flush has activated it. The sort view's view+proj were set at
    // init and are refreshed by Set_Projection_Transform_With_Z_Bias,
    // so it never needs a per-submit setViewTransform - only view 1
    // (the opaque view) uses the dirty flag.
    // Secondary 2D detection: if the view matrix is identity and the
    // projection has no perspective (w-divide), this is a 2D overlay
    // draw even if Set_View_Identity wasn't called. This catches draws
    // where DX8 state restores clear g_views.overlay2DActive between the
    // Set_View_Identity call and the actual Draw_Triangles.
    bool is2D = g_views.overlay2DActive;
    if (!is2D && !g_views.renderToTexture && !g_views.waterOverrideActive
        && !g_views.waterOverlayActive
        && !g_views.effectOverlayActive && !g_views.inSortFlush)
    {
        // Camera-space particles and smudges also draw with an identity view,
        // but keep the camera perspective projection. Only infer 2D when both
        // the view and projection match screen-space drawing.
        if (IsIdentityViewMatrix(g_frame.view) && IsNonPerspectiveProjection(g_frame.proj))
        {
            is2D = true;
        }
    }

    bgfx::ViewId submitView;
    if (g_views.smudgeActive)
    {
        submitView = kBgfxSmudgeView;
    }
    else if (g_views.effectOverlayActive)
    {
        submitView = kBgfxEffectOverlayView;
    }
    else if (is2D)
    {
        submitView = kBgfxUIView;
    }
    else if (g_views.renderToTexture)
    {
        submitView = kBgfxRTTView;
    }
    else if (g_views.waterOverrideActive || g_views.waterOverlayActive)
    {
        submitView = kBgfxWaterView;
    }
    else if (g_views.inSortFlush)
    {
        submitView = kBgfxEngineSortView;
    }
    else
    {
        submitView = kBgfxEngineView;
    }
    const uint64_t routeState = GetEffectiveDrawState();
    if (IsSortedRotorBlur(routeState)
        || IsSneakAttackAlphaDepthDecal(routeState)
        || IsSortedCopLightSprite(routeState))
    {
        // These sorted meshes need their raw model world matrix and the normal
        // camera view. The pre-view-multiplied sort matrix lands local W3D
        // model quads away from the object.
        submitView = kBgfxEngineView;
    }
    const BgfxDiagnosticFlags diagnostics = GetBgfxDiagnosticFlags();
    switch (submitView)
    {
        case kBgfxUIView:          g_stats.uiDraws++; break;
        case kBgfxWaterView:       g_stats.waterDraws++; break;
        case kBgfxEngineSortView:  g_stats.sortedDraws++; break;
        case kBgfxEffectOverlayView: g_stats.effectDraws++; break;
        case kBgfxRTTView:         g_stats.rttDraws++; break;
        case kBgfxSmudgeView:      g_stats.smudgeDraws++; break;
        default:                   g_stats.worldDraws++; break;
    }
    if (ShouldAllowBgfxDiagnosticDrawOverrides()
        && std::getenv("GGC_BGFX_SKIP_EFFECT_OVERLAY_DRAWS") != nullptr
        && submitView == kBgfxEffectOverlayView)
    {
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }
    // Push the engine view+projection when they change. setViewTransform
    // applies until the next change so we do not need to call it per
    // submit, only when the engine has updated either matrix. Sort view
    // draws never touch g_frame.view so the opaque view is never stomped.
    if (!g_views.inSortFlush && !g_views.renderToTexture && !g_views.overlay2DActive && g_frame.cameraProjDirty)
    {
        // Capture the camera view+proj at the first opaque draw of each
        // frame. Later Set_Projection calls (water, shadows, sneak attack)
        // may overwrite g_frame.proj with a different frustum. We re-apply
        // the camera projection to view 1 at End_Scene time.
        if (!g_frame.cameraCaptured)
        {
            std::memcpy(g_frame.cameraView, g_frame.view, sizeof(g_frame.cameraView));
            std::memcpy(g_frame.cameraProj, g_frame.proj, sizeof(g_frame.cameraProj));
            g_frame.cameraCaptured = true;
        }
        bgfx::setViewTransform(kBgfxEngineView, g_frame.view, g_frame.proj);
        // Shadow-volume view shares the engine camera; push the same
        // view+proj so the extrusion geometry lands where the opaque
        // geometry in view 1 landed.
        bgfx::setViewTransform(kBgfxShadowVolumeView, g_frame.view, g_frame.proj);
        bgfx::setViewTransform(kBgfxShroudOverlayView, g_frame.view, g_frame.proj);
        bgfx::setViewTransform(kBgfxSceneDepthView, g_frame.view, g_frame.proj);
        g_frame.cameraProjDirty = false;
    }
    // During RTT, push the current (reflected/shadow) view+proj to the RTT view.
    if (g_views.renderToTexture && g_frame.cameraProjDirty)
    {
        bgfx::setViewTransform(kBgfxRTTView, g_frame.view, g_frame.proj);
        g_frame.cameraProjDirty = false;
    }
    if (g_views.smudgeActive)
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxSmudgeView, identityView, g_frame.proj);
    }

    float identityWorld[16];
    const float * worldMtx = g_views.inSortFlush
        ? g_frame.sortWorld
        : g_frame.world;
    if (IsSortedRotorBlur(routeState)
        || IsSneakAttackAlphaDepthDecal(routeState)
        || IsSortedCopLightSprite(routeState))
    {
        worldMtx = g_frame.sortWorldRaw;
    }
    if (is2D)
    {
        // TheSuperHackers @bugfix bobtista 30/04/2026 2D UI vertices are
        // authored in screen/clip space for the UI view. Do not let a stale
        // world matrix from the preceding 3D draw offset textured control-bar
        // quads away from their text/widgets.
        IdentityMatrix(identityWorld);
        worldMtx = identityWorld;
    }

    bgfx::setTransform(worldMtx);

    // TheSuperHackers @refactor bobtista 11/04/2026 d3d8's BaseVertexIndex maps to bgfx's
    // setVertexBuffer _startVertex, not an IB start offset: it biases which vertex an index
    // resolves to, not which indices are read.
    const uint32_t base_vertex = static_cast<uint32_t>(g_draw.ibOffset);
    const uint32_t bindVertexCount =
        static_cast<uint32_t>(min_vertex_index) + static_cast<uint32_t>(vertex_count);

    if (g_draw.useTransientVB)
    {
        if (g_views.inSortFlush)
        {
            // Sort flush: use 2-arg overload (binds entire buffer).
            // Sort indices are absolute offsets into the full transient,
            // so no base vertex offset is needed. The 4-arg overload
            // would restrict the vertex range and clip high indices.
            bgfx::setVertexBuffer(0, &g_draw.transientVB);
        }
        else
        {
            // Skin/dynamic draws: apply base_vertex as startVertex.
            // Each mesh part within the shared transient VB has a
            // different base offset (from Set_Index_Buffer_Index_Offset).
            // Without this, all mesh parts read from vertex 0 and
            // infantry/vehicles are invisible or garbled.
            bgfx::setVertexBuffer(0, &g_draw.transientVB,
                                  base_vertex, bindVertexCount);
        }
    }
    else
    {
        if (g_draw.useStaticVB)
        {
            bgfx::setVertexBuffer(0, g_draw.staticVB, base_vertex, bindVertexCount);
        }
        else
        {
            bgfx::setVertexBuffer(0, g_draw.vb, base_vertex, bindVertexCount);
        }
    }

    const uint32_t indexCount = triangle_strip
        ? static_cast<uint32_t>(polygon_count) + 2
        : static_cast<uint32_t>(polygon_count) * 3;
    uint32_t indexStart = start_index;

    if (g_draw.useTransientIB)
    {
        bgfx::setIndexBuffer(&g_draw.transientIB,
                             indexStart,
                             indexCount);
    }
    else
    {
        if (g_draw.useStaticIB)
        {
            bgfx::setIndexBuffer(g_draw.staticIB,
                                 indexStart,
                                 indexCount);
        }
        else
        {
            bgfx::setIndexBuffer(g_draw.ib,
                                 indexStart,
                                 indexCount);
        }
    }

    if (g_views.smudgeActive)
    {
        if (bgfx::isValid(g_device.smudgeProgram)
            && bgfx::isValid(g_device.sceneSmudgeCopy)
            && bgfx::isValid(g_uniforms.sTex0))
        {
            bgfx::setTexture(0, g_uniforms.sTex0, g_device.sceneSmudgeCopy,
                             BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
            g_stats.textureBinds++;
            bgfx::setState(BGFX_STATE_WRITE_RGB
                           | BGFX_STATE_DEPTH_TEST_ALWAYS
                           | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                   BGFX_STATE_BLEND_INV_SRC_ALPHA)
                           | BGFX_STATE_MSAA);
            bgfx::submit(kBgfxSmudgeView, g_device.smudgeProgram);
            g_stats.smudgeSubmits++;
        }
        else
        {
            g_stats.skippedDraws++;
            bgfx::discard(BGFX_DISCARD_ALL);
        }
        return;
    }

    // Bind engine textures with the 1x1 white fallback (identity for vertex color and for the
    // stage-1 multiply). Sampler flags 0 = the sampler's creation-time defaults, i.e. trilinear
    // when the texture has mips.

    BindTextureStages();
    UpdateTextureTransforms();

    if (is2D)
    {
        // Render2DClass-authored quads only carry UV0 in screen space.
        // Stale world texture coordinate routing/transform state can
        // otherwise redirect video/UI samples into black atlas padding.
        g_draw.texcoordSelect[0] = 0.0f;
        g_draw.texcoordSelect[3] = 0.0f;
        g_draw.texcoordSelect2[0] = 0.0f;
        g_draw.texcoordSelect2[1] = 0.0f;
        for (unsigned stage = 0; stage < 4; ++stage)
        {
            g_draw.texcoordSource[stage] = 0.0f;
        }
        g_draw.texProjected[0] = 0.0f;
        g_draw.texProjected[1] = 0.0f;
        SetIdentityTextureTransform(g_draw.texTransform0, g_draw.texTransform1);
        SetIdentityTextureTransform(g_draw.tex1Transform0, g_draw.tex1Transform1);
        SetIdentityTextureTransform(g_draw.tex2Transform0, g_draw.tex2Transform1);
        g_draw.texTransform0Z[0] = 0.0f;
        g_draw.texTransform0Z[1] = 0.0f;
        g_draw.texTransform0Z[2] = 1.0f;
        g_draw.texTransform0Z[3] = 0.0f;
        g_draw.tex1TransformZ[0] = 0.0f;
        g_draw.tex1TransformZ[1] = 0.0f;
        g_draw.tex1TransformZ[2] = 1.0f;
        g_draw.tex1TransformZ[3] = 0.0f;
    }
    if (!g_draw.explicitMaterialState)
    {
        CaptureMaterialStateForBgfx(g_draw.sourceMaterial);
    }
    if (submitView == kBgfxEngineSortView && IsSortedMaterialDecal(GetEffectiveDrawState()))
    {
        // Terrain rendering leaves this flag set until reset by the shader
        // manager. Sorted material decals use the fixed-function TSS path and
        // must not inherit the terrain pixel-shader branch.
        g_draw.texcoordSelect[1] = 0.0f;
    }
    // TheSuperHackers @bugfix bobtista 30/04/2026 Z-bias must be picked
    // BEFORE UploadMaterialUniforms — that helper is what actually pushes
    // u_zBias to the GPU. Setting g_draw.zBias afterward leaves the uniform
    // at the previous (or default) value and defeats the whole fix.
    {
        g_draw.zBias[0] = static_cast<float>(g_draw.zBiasUnits) * kZBiasPerUnit;
        TraceLegacyZBiasTranslation(g_draw.zBiasUnits);
        const bool applySubmittedNormalBias = ShouldApplySubmittedNormalBias(routeState);
        const bool normalBiasFromGeometry =
            !is2D
            && (g_draw.normalBias[0] != 0.0f
                || (g_draw.activeVertexNormalBias && applySubmittedNormalBias)
                || IsSneakAttackCoplanarSurface());
        g_draw.zBias[1] = normalBiasFromGeometry
            ? ((g_draw.normalBias[0] < 0.02f) ? 0.02f : g_draw.normalBias[0])
            : 0.0f;
        // TheSuperHackers @bugfix bobtista 02/05/2026 Sorted material decals
        // (alpha-blend + DEPTH_WRITE_OFF + postdetail-alpha) such as the USA
        // strategy center floor emblem (ABBTCMDHQS.SWORD) sit coplanar with
        // an opaque sub-mesh. DX8 LEQUAL wins ties; bgfx's projection rounds
        // slightly so the decal ends up just behind the slab and is occluded.
        // Pull the NDC z slightly toward the camera so coplanar decals beat
        // the surface they sit on, but clamp the pull tightly so the ground
        // decal still loses to vehicles sitting above it.
        ClampSortedMaterialDecalZBias();
    }
    UpdateProjectedDecalModeForCurrentDraw();
    const uint64_t earlyState = GetEffectiveDrawState();
    if (IsMultiplicativeBlend(earlyState)
        && (earlyState & BGFX_STATE_WRITE_Z) == 0
        && (earlyState & BGFX_STATE_DEPTH_TEST_MASK) != BGFX_STATE_DEPTH_TEST_EQUAL
        && !IsEffectiveProjectedShadowDraw())
    {
        LogBgfxRevealDraw("submit-engine", submitView,
                          polygon_count, vertex_count, earlyState,
                          "skip-multiply");
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }
    {
        uint64_t blendState = earlyState;
        g_draw.texcoordSelect2[3] = IsAnyAdditiveBlend(blendState)
            ? 1.0f
            : 0.0f;
        UpdateAlphaMaskAndSortedEffectModes(blendState);
    }
    UploadMaterialUniforms();
    if (g_draw.lightDirs[0][3] < 0.5f)
    {
        const auto &rs = FixedFunctionState::Render_State();
        for (int i = 0; i < 4; ++i)
        {
            if (rs.LightEnable[i])
            {
                const auto &dl = rs.Lights[i];
                g_draw.lightDirs[i][0] = -dl.Direction.x;
                g_draw.lightDirs[i][1] = -dl.Direction.y;
                g_draw.lightDirs[i][2] = -dl.Direction.z;
                g_draw.lightDirs[i][3] = 1.0f;
                g_draw.lightColors[i][0] = dl.Diffuse.r;
                g_draw.lightColors[i][1] = dl.Diffuse.g;
                g_draw.lightColors[i][2] = dl.Diffuse.b;
                g_draw.lightColors[i][3] = 1.0f;
                g_draw.lightAmbients[i][0] = dl.Ambient.r;
                g_draw.lightAmbients[i][1] = dl.Ambient.g;
                g_draw.lightAmbients[i][2] = dl.Ambient.b;
                g_draw.lightAmbients[i][3] = 1.0f;
                g_draw.lightParams[i][3] = 1.0f;
            }
        }
    }
    UploadLightUniforms();
    if (bgfx::isValid(g_uniforms.uSceneAmbient))
    {
        bgfx::setUniform(g_uniforms.uSceneAmbient, g_draw.sceneAmbient);
    }
    if (bgfx::isValid(g_uniforms.uLightingEnabled))
    {
        // TheSuperHackers @bugfix bobtista 15/04/2026 Force lighting off for additive/alpha
        // particle and sorted-decal draws that bake intensity or final color into vertex diffuse
        // or recolored tex0; the lit branch would otherwise ignore that baking.
        uint64_t effectiveLightingState = earlyState;
        if (ShouldForceUnlitForBakedColorDraw(effectiveLightingState))
        {
            float forced[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(g_uniforms.uLightingEnabled, forced);
        }
        else
        {
            bgfx::setUniform(g_uniforms.uLightingEnabled, g_draw.lightingEnabled);
        }
    }

    // Detect shroud pass: legacy setup uses TCI_CAMERASPACEPOSITION + depth func
    // EQUAL to render a multiplicative shroud overlay. Both conditions must
    // be true to avoid false positives from other effects that set TCI bits.
    bool shroudDetected = false;
    {
        unsigned depthFunc = g_draw.depthFunc;
        const unsigned stg = 0;
        unsigned tci = g_draw.texcoordIndex[stg];
        float shroudParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        // Projected terrain receivers and cloud/noise stages also use
        // TCI_CAMERASPACEPOSITION. Only the actual shroud overlay uses the
        // stage-0 multiplicative sprite path that needs this world-space
        // shortcut; projector receivers must keep the normal texture matrix.
        const bool explicitShroudPass =
            g_views.shroudTexturePassActive && g_views.shroudTexturePassStage == stg;
        const bool legacyShroudSignature =
            g_draw.tssOps0[0] > 2.5f && g_draw.tssOps0[0] < 3.5f;
        if (explicitShroudPass)
        {
            // Shroud setup is explicit backend state. It must not inherit a
            // stale stencil test from previous shadow/player-color passes.
            g_draw.stencilEnabled = false;
        }
        if (depthFunc == static_cast<unsigned>(RB_CMP_EQUAL)
            && (explicitShroudPass || (!g_draw.stencilEnabled && legacyShroudSignature)))
        {
            if (tci & kTexcoordGenCameraPosition)
            {
                shroudDetected = true;
                g_draw.texcoordSelect[2] = 1.0f;
                if (g_draw.shroudTextureParamsValid)
                {
                    std::memcpy(shroudParams, g_draw.shroudTextureParams, sizeof(shroudParams));
                }
                else
                {
                    // Legacy fallback for call sites that only expose the cached
                    // texture matrix. Dedicated shroud setup paths provide
                    // direct world-space params; decomposing camera-space
                    // matrices is fragile across compatibility layers.
                    auto texMtx = MakeIdentityLegacyCacheMatrix();
                    FixedFunctionState::Transform_Matrix(kTextureTransformStage0 + stg, texMtx);
                    auto viewMtx = MakeIdentityLegacyCacheMatrix();
                    FixedFunctionState::Transform_Matrix(kTransformView, viewMtx);
                    auto ts = MakeIdentityLegacyCacheMatrix();
                    for (int rr = 0; rr < 4; rr++)
                    {
                        for (int cc = 0; cc < 4; cc++)
                        {
                            ts.m[rr][cc] = 0;
                            for (int k = 0; k < 4; k++)
                            {
                                ts.m[rr][cc] += viewMtx.m[rr][k] * texMtx.m[k][cc];
                            }
                        }
                    }
                    shroudParams[0] = (ts.m[0][0] != 0.0f) ? ts.m[3][0] / ts.m[0][0] : 0.0f;
                    shroudParams[1] = (ts.m[1][1] != 0.0f) ? ts.m[3][1] / ts.m[1][1] : 0.0f;
                    shroudParams[2] = ts.m[0][0];
                    shroudParams[3] = ts.m[1][1];
                }
                if (bgfx::isValid(g_uniforms.uShroudParams))
                {
                    bgfx::setUniform(g_uniforms.uShroudParams, shroudParams);
                }
            }
        }
        if (!shroudDetected)
        {
            g_draw.texcoordSelect[2] = 0.0f;
        }
        else if (ShouldAllowBgfxDiagnosticDrawOverrides()
            && std::getenv("GGC_BGFX_SKIP_SHROUD_OVERLAY") != nullptr)
        {
            bgfx::discard(BGFX_DISCARD_ALL);
            return;
        }
        const bool delayedObjectShroudPass =
            shroudDetected
            && g_views.objectShroudTexturePassActive
            && submitView == kBgfxEngineView;
        if (delayedObjectShroudPass)
        {
            submitView = kBgfxShroudOverlayView;
        }
        g_draw.delayedObjectShroudPass = delayedObjectShroudPass;
        if ((tci & kTexcoordGenCameraPosition)
            || depthFunc == static_cast<unsigned>(RB_CMP_EQUAL)
            || shroudDetected)
        {
            LogBgfxShroudPass("shroud-candidate", submitView, polygon_count, depthFunc, tci, shroudDetected, shroudParams);
        }
    }

    if (bgfx::isValid(g_uniforms.uTexcoordSelect))
    {
        bgfx::setUniform(g_uniforms.uTexcoordSelect, g_draw.texcoordSelect);
    }

    uint64_t state = GetEffectiveDrawState();
    state |= BGFX_STATE_MSAA;

    state = ApplyCullModeOverride(state);
    if (IsSortedRotorBlur(state))
    {
        // The rotor blur is built from camera-facing sorted cards. Once the
        // cards are replayed through the normal engine view, culling can drop
        // one side of the blur depending on the camera angle.
        state &= ~(BGFX_STATE_CULL_CW | BGFX_STATE_CULL_CCW);
    }
    state = ApplyBlendEquation(state);

    state = ApplyColorWriteOverride(state);
    if (triangle_strip)
    {
        state &= ~BGFX_STATE_PT_MASK;
        state |= BGFX_STATE_PT_TRISTRIP;
    }
    if (g_views.waterOverrideActive)
    {
        g_views.waterOverrideActive = false;
    }
    state = ApplyProjectedAdditiveDecalDrawState(state);
    state = ApplySortedMaterialDecalDepthState(state);
    if (g_draw.delayedObjectShroudPass)
    {
        state = ApplyDelayedObjectShroudDepthState(state);
    }
    LogBgfxSortedMaterialDecal("submit-engine", submitView,
                               polygon_count, vertex_count, state);
    LogBgfxEffectSubmit("submit-engine", submitView,
                        polygon_count, vertex_count, state, "pre-skip");
    LogBgfxRevealDraw("submit-engine", submitView,
                      polygon_count, vertex_count, state, "pre-skip");
    if (ShouldAllowBgfxDiagnosticDrawOverrides()
        && std::getenv("GGC_BGFX_SKIP_REVEAL_GRID") != nullptr
        && IsRevealGridTexture(g_draw.sourceTextures[0]))
    {
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }

    if (ShouldSkipHiddenMissingTextureDraw(state))
    {
        LogBgfxRevealDraw("submit-engine", submitView,
                          polygon_count, vertex_count, state, "skip-missing");
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }

    if (g_views.shadowVolumeActive && bgfx::isValid(g_device.shadowVolumeProgram))
    {
        if (LegacyStencilShadowsEnabled())
        {
            if (BgfxTwoSidedStencilVolumes()
                && (g_draw.stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECR
                    || g_draw.stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECRSAT))
            {
                g_stats.skippedDraws++;
                bgfx::discard(BGFX_DISCARD_ALL);
                return;
            }
            uint64_t volumeState = BgfxShadowVolumeDepthState() | (state & BGFX_STATE_PT_MASK);
            const unsigned shadowCullModeBits = BgfxShadowCullModeBits();
            if (!BgfxTwoSidedStencilVolumes() && shadowCullModeBits == 1)
            {
                volumeState |= BGFX_STATE_CULL_CW;
            }
            else if (!BgfxTwoSidedStencilVolumes() && shadowCullModeBits == 2)
            {
                volumeState |= BGFX_STATE_CULL_CCW;
            }
            bgfx::setState(volumeState);
            if (BgfxTwoSidedStencilVolumes())
            {
                const uint32_t common = g_draw.stencilFuncBits
                    | BGFX_STENCIL_FUNC_REF(g_draw.stencilRef & 0xFF)
                    | BGFX_STENCIL_FUNC_RMASK(g_draw.stencilReadMask & 0xFF)
                    | g_draw.stencilFailOpBits
                    | g_draw.stencilZFailOpBits;
                if (BgfxSwapTwoSidedStencilVolumeOps())
                {
                    bgfx::setStencil(common | BGFX_STENCIL_OP_PASS_Z_DECRSAT,
                                     common | BGFX_STENCIL_OP_PASS_Z_INCR);
                }
                else
                {
                    bgfx::setStencil(common | BGFX_STENCIL_OP_PASS_Z_INCR,
                                     common | BGFX_STENCIL_OP_PASS_Z_DECRSAT);
                }
            }
            else
            {
                bgfx::setStencil(BuildCurrentStencilState());
            }
            BindShadowVolumeBiasUniform();
            bgfx::submit(BgfxShadowVolumeSubmitView(), g_device.shadowVolumeProgram);
            g_stats.shadowVolumeSubmits++;
            LogBgfxStencilShadowEvent("side-wall-submit", nullptr,
                                      polygon_count, vertex_count);
            return;
        }
        LogBgfxStencilShadowEvent("side-wall-skip", "legacy-stencil-disabled",
                                  polygon_count, vertex_count);
        g_stats.skippedDraws++;
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }

    const bool writesDepth = (state & BGFX_STATE_WRITE_Z) != 0;
    const bool isBlended = (state & BGFX_STATE_BLEND_MASK) != 0;
    const bool isAlphaTested = g_overrides.atestActive ? (g_overrides.atestFunc > 0.0f) : g_draw.atestEnabled;
    const bool isSceneDepthCaster =
        submitView == kBgfxEngineView
        && !g_views.overlay2DActive
        && writesDepth
        && !isBlended
        && !isAlphaTested;

    // Sorted translucent/effect draws (particles, lasers, material decals)
    // are submitted after the world pass and should not inherit stale stencil
    // state from shroud/player-color/shadow passes. Keeping stencil active
    // here clips effects such as the particle-cannon beam against buildings.
    const bool sortedTranslucentEffect = submitView == kBgfxEngineSortView
        && isBlended;
    const bool sortedMaterialDecal = submitView == kBgfxEngineSortView
        && (IsSortedMaterialDecal(state)
            || IsSortedAlphaDepthDecal(state)
            || IsSortedRotorBlur(state));
    // Sort-flushed material decals (command-center driveway emblems, upgrade
    // floor marks) are ordinary alpha decals. The wrapper can still have
    // stencil state cached from shroud/player-color passes; applying it here
    // clips revealed decals out completely.
    const bool applyStencil = g_draw.stencilEnabled
        && submitView != kBgfxUIView
        && !sortedTranslucentEffect
        && !sortedMaterialDecal;

    bgfx::setState(state);
    if (applyStencil)
    {
        bgfx::setStencil(BuildCurrentStencilState());
    }

    BindSoftParticleDepth(submitView == kBgfxEngineSortView
                          && isBlended
                          && IsSoftParticleCandidate(state));

    // Tree / grass sway shader takes over the program slot
    // and uploads its own constants when active. Otherwise fall back
    // to whatever ShaderClass picked (g_draw.program).
    bgfx::ProgramHandle program = g_draw.program;
    if (g_views.treeShaderActive && bgfx::isValid(g_device.treeProgram))
    {
        program = g_device.treeProgram;
        if (bgfx::isValid(g_uniforms.uSwayTable))
        {
            bgfx::setUniform(g_uniforms.uSwayTable, g_draw.swayTable, kSwayTableEntries);
        }
        if (bgfx::isValid(g_uniforms.uShroudOffset))
        {
            bgfx::setUniform(g_uniforms.uShroudOffset, g_draw.shroudOffset);
        }
        if (bgfx::isValid(g_uniforms.uShroudScale))
        {
            bgfx::setUniform(g_uniforms.uShroudScale, g_draw.shroudScale);
        }
    }
    bgfx::submit(submitView, program);
    LogBgfxEffectSubmit("submit-engine", submitView,
                        polygon_count, vertex_count, state, "submit");
    LogBgfxRevealDraw("submit-engine", submitView,
                      polygon_count, vertex_count, state, "submit");
    g_stats.baseSubmits++;

    const bool hasVB = g_draw.useTransientVB
        || (g_draw.useStaticVB && bgfx::isValid(g_draw.staticVB))
        || bgfx::isValid(g_draw.vb);
    if (isSceneDepthCaster
        && bgfx::isValid(g_device.sceneDepthProgram)
        && bgfx::isValid(g_device.sceneReadableDepthFB)
        && hasVB)
    {
        // TheSuperHackers @feature bobtista 27/04/2026 Duplicate opaque
        // non-alpha-tested world geometry into a sampleable R32F scene-depth
        // target. Alpha-tested draws are skipped until the depth shader also
        // mirrors fs_uber's texture alpha discard.
        if (g_draw.useTransientVB)
        {
            bgfx::setVertexBuffer(0, &g_draw.transientVB,
                                  static_cast<uint32_t>(g_draw.ibOffset),
                                  bindVertexCount);
        }
        else
        {
            if (g_draw.useStaticVB)
            {
                bgfx::setVertexBuffer(0, g_draw.staticVB,
                                      static_cast<uint32_t>(g_draw.ibOffset),
                                      bindVertexCount);
            }
            else
            {
                bgfx::setVertexBuffer(0, g_draw.vb,
                                      static_cast<uint32_t>(g_draw.ibOffset),
                                      bindVertexCount);
            }
        }
        if (g_draw.useTransientIB)
        {
            bgfx::setIndexBuffer(&g_draw.transientIB,
                                 start_index,
                                 indexCount);
        }
        else
        {
            if (g_draw.useStaticIB)
            {
                bgfx::setIndexBuffer(g_draw.staticIB,
                                     start_index,
                                     indexCount);
            }
            else
            {
                bgfx::setIndexBuffer(g_draw.ib,
                                     start_index,
                                     indexCount);
            }
        }
        bgfx::setTransform(worldMtx);
        const uint64_t depthState =
            BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | (state & BGFX_STATE_CULL_MASK)
            | (state & BGFX_STATE_PT_MASK);
        bgfx::setState(depthState);
        bgfx::submit(kBgfxSceneDepthView, g_device.sceneDepthProgram);
        g_stats.sceneDepthSubmits++;
    }
}
}

void BgfxBackend::Draw_Triangles(unsigned short start_index,
                                 unsigned short polygon_count,
                                 unsigned short min_vertex_index,
                                 unsigned short vertex_count)
{
    // If DX8Wrapper::Draw_Sorting_IB_VB already submitted
    // the draw with correctly remapped args against its internal dynamic
    // buffers, skip the outer submit.
    if (g_views.skipNextSubmitEngineDraw)
    {
        g_views.skipNextSubmitEngineDraw = false;
        return;
    }
    if (!g_triangleDrawEnabled)
    {
        return;
    }
    SubmitEngineDraw(start_index, polygon_count, min_vertex_index, vertex_count);
}

void BgfxBackend::Draw_Triangles(unsigned int buffer_type,
                                 unsigned short start_index,
                                 unsigned short polygon_count,
                                 unsigned short min_vertex_index,
                                 unsigned short vertex_count)
{
    (void)buffer_type;
    if (g_views.skipNextSubmitEngineDraw)
    {
        g_views.skipNextSubmitEngineDraw = false;
        return;
    }
    if (!g_triangleDrawEnabled)
    {
        return;
    }
    SubmitEngineDraw(start_index, polygon_count, min_vertex_index, vertex_count);
}

bool BgfxBackend::Is_Triangle_Draw_Enabled() const
{
    return g_triangleDrawEnabled;
}

void BgfxBackend::Set_Triangle_Draw_Enabled(bool enable)
{
    g_triangleDrawEnabled = enable;
}

// TheSuperHackers @feature bobtista 16/04/2026 Draw_Strip override so strip-based
// geometry (e.g. water tracks) goes through bgfx instead of silently falling back
// to the DX8-only base class.
void BgfxBackend::Draw_Strip(unsigned short start_index,
                             unsigned short index_count,
                             unsigned short min_vertex_index,
                             unsigned short vertex_count)
{
    if (g_views.skipNextSubmitEngineDraw)
    {
        g_views.skipNextSubmitEngineDraw = false;
        return;
    }
    if (!g_triangleDrawEnabled)
    {
        return;
    }

    SubmitEngineDraw(start_index, index_count, min_vertex_index, vertex_count, true);
}

// -- Programmable pipeline compatibility ------------------------------------

bool BgfxBackend::Load_Legacy_Shader(const char * path,
                                     const unsigned int * declaration,
                                     unsigned int usage,
                                     RenderBackendShaderKind kind,
                                     unsigned long * handle)
{
    (void)path;
    (void)declaration;
    (void)usage;
    (void)kind;
    if (handle == nullptr) {
        return false;
    }

    *handle = AllocateLegacyShaderHandle();
    return true;
}

bool BgfxBackend::Create_Vertex_Shader(const unsigned int * declaration,
                                       const unsigned int * shader,
                                       unsigned int usage,
                                       unsigned long * handle)
{
    (void)declaration;
    (void)shader;
    (void)usage;
    if (handle == nullptr) {
        return false;
    }

    *handle = AllocateLegacyShaderHandle();
    return true;
}

bool BgfxBackend::Create_Pixel_Shader(const unsigned int * shader,
                                      unsigned long * handle)
{
    (void)shader;
    if (handle == nullptr) {
        return false;
    }

    *handle = AllocateLegacyShaderHandle();
    return true;
}

bool BgfxBackend::Create_Legacy_Pixel_Shader(RenderBackendLegacyPixelShaderMode mode,
                                             unsigned long * handle)
{
    if (handle == nullptr || mode == RB_LEGACY_PIXEL_SHADER_NONE) {
        return false;
    }

    *handle = AllocateLegacyShaderHandle();
    g_legacyPixelShaderModes[*handle] = mode;
    return true;
}

void BgfxBackend::Delete_Vertex_Shader(unsigned long vertex_shader)
{
    (void)vertex_shader;
}

void BgfxBackend::Delete_Pixel_Shader(unsigned long pixel_shader)
{
    g_legacyPixelShaderModes.erase(pixel_shader);
}

void BgfxBackend::Set_Vertex_Shader(unsigned long vertex_shader)
{
    (void)vertex_shader;
}

void BgfxBackend::Set_Pixel_Shader(unsigned long pixel_shader)
{
    RenderBackendLegacyPixelShaderMode mode = RB_LEGACY_PIXEL_SHADER_NONE;
    auto it = g_legacyPixelShaderModes.find(pixel_shader);
    if (it != g_legacyPixelShaderModes.end())
    {
        mode = it->second;
    }
    g_draw.legacyPixelShaderMode[0] = static_cast<float>(mode);
}

// ===========================================================================
// Asset-ingress resource creation
// ===========================================================================
//
// The returned RenderResource.id is a monotonically-increasing key into
// g_resourceRegistry.table; the entry holds the bgfx handle(s). Owner-backed resources
// still enter through the transitional *_Resource hooks and the older caches
// keyed by their owner objects.
//
namespace {

unsigned __int64 AllocResourceId()
{
    const unsigned __int64 id = g_resourceRegistry.next_id++;
    if (g_resourceRegistry.next_id == 0) {
        // Roll-over guard — rarely hit; start back at 1 to avoid colliding
        // with kInvalidRenderResource.
        g_resourceRegistry.next_id = 1;
    }
    return id;
}

bool IsCompressedTextureFormat(WW3DFormat format)
{
    return format == WW3D_FORMAT_DXT1
        || format == WW3D_FORMAT_DXT2
        || format == WW3D_FORMAT_DXT3
        || format == WW3D_FORMAT_DXT4
        || format == WW3D_FORMAT_DXT5;
}

unsigned CompressedTextureBlockSize(WW3DFormat format)
{
    return format == WW3D_FORMAT_DXT1 ? 8 : 16;
}

// Copy a MipSlice into tightly packed bgfx memory for updateTexture2D.
const bgfx::Memory * CopySliceToBgfxMemory(const TextureDesc & desc, const MipSlice & slice)
{
    if (slice.data == nullptr || slice.size_bytes == 0 || slice.width == 0 || slice.height == 0) {
        return nullptr;
    }

    const bool compressed = IsCompressedTextureFormat(desc.format);
    const unsigned rows = compressed ? DXT_SurfaceRows(slice.height) : slice.height;
    const unsigned expectedPitch = compressed
        ? DXT_SurfacePitch(slice.width, CompressedTextureBlockSize(desc.format))
        : slice.width * Get_Bytes_Per_Pixel(desc.format);
    if (rows == 0 || expectedPitch == 0) {
        return nullptr;
    }

    const unsigned sourcePitch = slice.pitch != 0 ? slice.pitch : expectedPitch;
    const unsigned requiredSourceBytes = (rows - 1) * sourcePitch + expectedPitch;
    if (slice.size_bytes < requiredSourceBytes) {
        return nullptr;
    }

    const unsigned uploadBytes = rows * expectedPitch;
    const bgfx::Memory * mem = bgfx::alloc(uploadBytes);
    const uint8_t * src = static_cast<const uint8_t *>(slice.data);
    uint8_t * dst = mem->data;
    for (unsigned row = 0; row < rows; ++row) {
        std::memcpy(dst, src, expectedPitch);
        src += sourcePitch;
        dst += expectedPitch;
    }
    return mem;
}

RenderResource RegisterResourceEntry(const BgfxResourceEntry & entry)
{
    RenderResource rr;
    rr.id = AllocResourceId();
    g_resourceRegistry.table[rr.id] = entry;
    return rr;
}

BgfxResourceEntry MakeVertexBufferResourceEntry(VertexBufferClass * owner)
{
    BgfxResourceEntry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_VB;
    entry.vb = BGFX_INVALID_HANDLE;
    entry.dvb = BGFX_INVALID_HANDLE;
    entry.d3d_mirror = nullptr;
    entry.owner = owner;
    return entry;
}

BgfxResourceEntry MakeIndexBufferResourceEntry(IndexBufferClass * owner)
{
    BgfxResourceEntry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_IB;
    entry.ib = BGFX_INVALID_HANDLE;
    entry.dib = BGFX_INVALID_HANDLE;
    entry.d3d_mirror = nullptr;
    entry.owner = owner;
    return entry;
}

bgfx::VertexBufferHandle CreateStaticVertexBufferFromInitialData(const BufferDesc & desc,
                                                                 const void * initial_data)
{
    if (initial_data == nullptr
        || desc.size_bytes == 0
        || desc.layout.fvf == 0
        || desc.layout.stride == 0
        || (desc.size_bytes % desc.layout.stride) != 0)
    {
        return BGFX_INVALID_HANDLE;
    }

    FVFInfoClass fvf(desc.layout.fvf);
    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(fvf, layout) || layout.getStride() != desc.layout.stride)
    {
        return BGFX_INVALID_HANDLE;
    }

    bgfx::VertexBufferHandle h = bgfx::createVertexBuffer(bgfx::copy(initial_data, desc.size_bytes), layout);
    return h;
}

bgfx::IndexBufferHandle CreateStaticIndexBufferFromInitialData(const BufferDesc & desc,
                                                               const void * initial_data,
                                                               bool indices_are_32bit)
{
    const unsigned int indexSize = indices_are_32bit ? sizeof(uint32_t) : sizeof(uint16_t);
    if (initial_data == nullptr
        || desc.size_bytes == 0
        || (desc.size_bytes % indexSize) != 0)
    {
        return BGFX_INVALID_HANDLE;
    }

    const uint64_t flags = indices_are_32bit ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE;
    bgfx::IndexBufferHandle h = bgfx::createIndexBuffer(bgfx::copy(initial_data, desc.size_bytes), flags);
    return h;
}

} // namespace

bool BgfxBackend::Requires_Legacy_Buffer_Resources() const
{
    return false;
}

RenderResource BgfxBackend::Create_Texture(const TextureDesc & desc)
{
    BgfxResourceEntry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_TEXTURE;
    entry.d3d_mirror = nullptr;
    entry.texture = BGFX_INVALID_HANDLE;
    entry.fb = BGFX_INVALID_HANDLE;
    entry.width = desc.width;
    entry.height = desc.height;

    if (desc.is_render_target) {
        const bgfx::TextureFormat::Enum colorFormat =
            Resolve_Render_Target_Color_Format(desc.format);
        bgfx::TextureHandle colorTex = bgfx::createTexture2D(
            desc.width, desc.height, false, 1, colorFormat,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle depthTex = bgfx::createTexture2D(
            desc.width, desc.height, false, 1, bgfx::TextureFormat::D24S8,
            BGFX_TEXTURE_RT_WRITE_ONLY);
        bgfx::TextureHandle attachments[2] = { colorTex, depthTex };
        entry.fb = bgfx::createFrameBuffer(2, attachments, true);
        if (bgfx::isValid(entry.fb)) {
            entry.texture = colorTex;
        } else {
            if (bgfx::isValid(colorTex)) {
                bgfx::destroy(colorTex);
            }
            if (bgfx::isValid(depthTex)) {
                bgfx::destroy(depthTex);
            }
        }
    } else if (desc.mips != nullptr && desc.mip_count > 0) {
        const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(desc.format);
        if (bgfxFmt != bgfx::TextureFormat::Unknown) {
            const uint64_t texFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
            entry.texture = bgfx::createTexture2D(
                desc.width, desc.height,
                desc.mip_count > 1,
                1, bgfxFmt, texFlags, nullptr);
            if (bgfx::isValid(entry.texture)) {
                bool uploadedAllLevels = true;
                for (unsigned mip = 0; mip < desc.mip_count; ++mip) {
                    const MipSlice & slice = desc.mips[mip];
                    const bgfx::Memory * mem = CopySliceToBgfxMemory(desc, slice);
                    if (mem == nullptr) {
                        uploadedAllLevels = false;
                        break;
                    }
                    bgfx::updateTexture2D(
                        entry.texture,
                        0,
                        static_cast<uint8_t>(mip),
                        0,
                        0,
                        slice.width,
                        slice.height,
                        mem);
                }
                if (!uploadedAllLevels) {
                    g_caches.deferredDestroys.push_back(entry.texture);
                    entry.texture = BGFX_INVALID_HANDLE;
                }
            }
        }
    }

    return RegisterResourceEntry(entry);
}

RenderResource BgfxBackend::Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data)
{
    BgfxResourceEntry entry = MakeVertexBufferResourceEntry(nullptr);
    entry.vb = CreateStaticVertexBufferFromInitialData(desc, initial_data);
    return RegisterResourceEntry(entry);
}

RenderResource BgfxBackend::Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit)
{
    BgfxResourceEntry entry = MakeIndexBufferResourceEntry(nullptr);
    entry.ib = CreateStaticIndexBufferFromInitialData(desc, initial_data, indices_are_32bit);
    return RegisterResourceEntry(entry);
}

void BgfxBackend::Destroy_Resource(RenderResource h)
{
    auto it = g_resourceRegistry.table.find(h.id);
    if (it == g_resourceRegistry.table.end()) {
        return;
    }
    BgfxResourceEntry & entry = it->second;

    // Destroy bgfx side.
    switch (entry.kind) {
        case BGFX_RR_KIND_TEXTURE:
            if (bgfx::isValid(entry.fb)) {
                bgfx::destroy(entry.fb);
            } else if (bgfx::isValid(entry.texture)) {
                g_caches.deferredDestroys.push_back(entry.texture);
            }
            break;
        case BGFX_RR_KIND_VB:
        {
            const VertexBufferClass *owner = static_cast<const VertexBufferClass *>(entry.owner);
            bool destroyedDynamic = false;
            auto vbIt = g_caches.vb.find(owner);
            if (vbIt != g_caches.vb.end())
            {
                if (bgfx::isValid(vbIt->second.handle))
                {
                    if (bgfx::isValid(g_draw.vb) && g_draw.vb.idx == vbIt->second.handle.idx)
                    {
                        g_draw.vb = BGFX_INVALID_HANDLE;
                    }
                    // TheSuperHackers @bugfix bobtista 02/06/2026 Defer one frame: the
                    // engine frees this dynamic VB mid-frame, but a draw recorded earlier
                    // this frame may still reference the handle until bgfx::frame(). Immediate
                    // destroy here was the source of the per-frame "RefCount is 1 (expected 0)"
                    // warnings (the texture case above already defers for the same reason).
                    g_caches.deferredDestroyVB.push_back(vbIt->second.handle);
                    destroyedDynamic = bgfx::isValid(entry.dvb) && entry.dvb.idx == vbIt->second.handle.idx;
                }
                g_caches.vb.erase(vbIt);
            }
            if (bgfx::isValid(entry.dvb) && !destroyedDynamic)
            {
                if (bgfx::isValid(g_draw.vb) && g_draw.vb.idx == entry.dvb.idx)
                {
                    g_draw.vb = BGFX_INVALID_HANDLE;
                }
                g_caches.deferredDestroyVB.push_back(entry.dvb);
            }
            DestroyStaticVertexResource(entry);
            break;
        }
        case BGFX_RR_KIND_IB:
        {
            const IndexBufferClass *owner = static_cast<const IndexBufferClass *>(entry.owner);
            bool destroyedDynamic = false;
            auto ibIt = g_caches.ib.find(owner);
            if (ibIt != g_caches.ib.end())
            {
                if (bgfx::isValid(ibIt->second.handle))
                {
                    if (bgfx::isValid(g_draw.ib) && g_draw.ib.idx == ibIt->second.handle.idx)
                    {
                        g_draw.ib = BGFX_INVALID_HANDLE;
                    }
                    // TheSuperHackers @bugfix bobtista 02/06/2026 Defer one frame; see the
                    // matching note in the VB case above.
                    g_caches.deferredDestroyIB.push_back(ibIt->second.handle);
                    destroyedDynamic = bgfx::isValid(entry.dib) && entry.dib.idx == ibIt->second.handle.idx;
                }
                g_caches.ib.erase(ibIt);
            }
            if (bgfx::isValid(entry.dib) && !destroyedDynamic)
            {
                if (bgfx::isValid(g_draw.ib) && g_draw.ib.idx == entry.dib.idx)
                {
                    g_draw.ib = BGFX_INVALID_HANDLE;
                }
                g_caches.deferredDestroyIB.push_back(entry.dib);
            }
            DestroyStaticIndexResource(entry);
            break;
        }
        case BGFX_RR_KIND_NONE:
        default:
            break;
    }

    g_resourceRegistry.table.erase(it);
}

// -- Transitional owner-backed resource hooks -------------------------------

RenderResource BgfxBackend::Register_Texture_Resource(TextureBaseClass * tex)
{
    if (tex == nullptr) {
        return kInvalidRenderResource;
    }
    // Ensure the bgfx-side texture exists (peek+lock+upload from the legacy
    // mirror that the legacy loader already created). The returned handle
    // is owned by g_caches.texture (keyed on TextureBaseClass*), NOT by
    // this registry entry — Release_Cached_Texture in the dtor queues it
    // for deferred destroy. We leave entry.texture invalid so
    // Destroy_Resource doesn't try to destroy the same handle twice.
    if (tex->Is_Render_Target())
    {
        Ensure_Render_Target_Framebuffer(tex->As_TextureClass());
    }
    else
    {
        EnsureBgfxTexture(tex);
    }

    BgfxResourceEntry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind       = BGFX_RR_KIND_TEXTURE;
    entry.texture    = BGFX_INVALID_HANDLE;
    entry.fb         = BGFX_INVALID_HANDLE;
    entry.d3d_mirror = nullptr;
    entry.owner      = tex;

    return RegisterResourceEntry(entry);
}

RenderResource BgfxBackend::Register_Vertex_Buffer_Resource(VertexBufferClass * vb)
{
    if (vb == nullptr) {
        return kInvalidRenderResource;
    }
    // IMPORTANT: do NOT store the VertexBufferClass* as d3d_mirror —
    // Destroy_Resource would cast it to IUnknown* and call Release(), which
    // lands on whatever the third virtual of VertexBufferClass happens to
    // be and crashes. The VB's legacy resource lifetime is owned by the
    // render wrapper dtor; we have no cleanup to do on the reference side.
    return RegisterResourceEntry(MakeVertexBufferResourceEntry(vb));
}

RenderResource BgfxBackend::Register_Index_Buffer_Resource(IndexBufferClass * ib)
{
    if (ib == nullptr) {
        return kInvalidRenderResource;
    }
    // Same rationale as Register_Vertex_Buffer_Resource — leave d3d_mirror
    // null so Destroy_Resource's reference-side Release does nothing.
    return RegisterResourceEntry(MakeIndexBufferResourceEntry(ib));
}
