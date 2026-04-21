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

// TheSuperHackers @refactor bobtista 10/04/2026 BgfxBackend Phase 2 stub.
//
// Every virtual method is a no-op (void) or returns a sensible default
// (non-void). The class exists to prove the compile-time backend selection
// mechanism works and to verify that bgfx itself can be fetched, built,
// and linked against WW3D2. Phase 3 fills in real implementations as
// individual rendering subsystems are migrated off DX8Wrapper statics.
//
// We deliberately #include <bgfx/bgfx.h> and reference one bgfx symbol so
// that if the FetchContent + link pipeline is broken, we get a compile or
// link error during Phase 2 rather than discovering it deep inside Phase 3.

#include "BgfxBackend.h"

#include "dx8fvf.h"
#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
#include "dx8wrapper.h"
#include "lightenvironment.h"
#include "matrix3d.h"
#include "matrix4.h"
#include "shader.h"
#include "texture.h"
#include "texturefilter.h"
#include "vector3.h"
#include "ww3dformat.h"
#include "wwdebug.h"

#include <cstring>
#include <d3d8.h>

#include <unordered_map>

// Including the bgfx header here is intentional: it forces a compile-time
// dependency on the bgfx headers when GGC_RENDER_BACKEND=bgfx. If bgfx
// isn't available the build fails here, which is the right place to
// catch dependency problems.
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. bgfx takes the main
// game window. A secondary popup is created for D3D8 reference output.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.2 compiled shader
// bytecode. These headers are generated at build time by ggc_compile_bgfx_shader
// (cmake/bgfx.cmake) and end up in the target's binary dir. They define
// vs_passthrough_dx11[] and fs_passthrough_dx11[] as static const uint8_t
// arrays we hand to bgfx::createShader via bgfx::makeRef.
#include "vs_passthrough_dx11.bin.h"
#include "fs_passthrough_dx11.bin.h"

// TheSuperHackers @refactor bobtista 12/04/2026 Phase 5A uber shader pair.
// Single program handles all TSS combinations via uniforms. Replaces the
// Phase 4D.1 per-preset shader pairs.
#include "vs_uber_dx11.bin.h"
#include "vs_trees_dx11.bin.h"
#include "fs_uber_dx11.bin.h"

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// volume program. Vertex shader is a trivial XYZ->clip transform; fragment
// writes nothing visible because color writes are disabled for the pass.
#include "vs_shadow_volume_dx11.bin.h"
#include "fs_shadow_volume_dx11.bin.h"
#include "vs_shadow_apply_dx11.bin.h"
#include "fs_shadow_apply_dx11.bin.h"

// Phase 4I.2 CSM caster pass shaders.
#include "vs_shadow_caster_dx11.bin.h"
#include "fs_shadow_caster_dx11.bin.h"

namespace
{
// Anchor a reference to one bgfx symbol so the linker must resolve bgfx
// symbols even though every virtual method below is a no-op. This turns a
// "bgfx built but never used" scenario into a loud link failure if anything
// is misconfigured.
[[maybe_unused]] const auto kBgfxLinkAnchor = &bgfx::getCaps;

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I bgfx callback
// so fatal errors and debug trace messages land in DebugLogFileD.txt
// instead of silently firing bx::debugBreak. Without this, internal
// bgfx validation failures produce only a raw breakpoint with no text.
class BgfxLoggingCallback : public bgfx::CallbackI
{
public:
    ~BgfxLoggingCallback() override {}

    void fatal(const char * filePath, uint16_t line, bgfx::Fatal::Enum code, const char * str) override
    {
        WWDEBUG_SAY(("[BgfxBackend] FATAL code=%d at %s:%u: %s",
                     static_cast<int>(code), filePath ? filePath : "?", line, str ? str : "?"));
    }
    void traceVargs(const char * filePath, uint16_t line, const char * format, va_list argList) override
    {
        char buf[512];
        std::vsnprintf(buf, sizeof(buf), format, argList);
        // Strip trailing newline for single-line log output.
        size_t len = std::strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) { buf[--len] = '\0'; }
        WWDEBUG_SAY(("[bgfx] %s:%u: %s", filePath ? filePath : "?", line, buf));
    }
    void profilerBegin(const char *, uint32_t, const char *, uint16_t) override {}
    void profilerBeginLiteral(const char *, uint32_t, const char *, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void *, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void *, uint32_t) override {}
    void screenShot(const char *, uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum,
                    const void *, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void *, uint32_t) override {}
};
BgfxLoggingCallback g_bgfxCallback;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 1. Tracks
// whether bgfx::init has been called successfully, so Begin_Scene /
// End_Scene / Shutdown can skip bgfx calls if init was never reached.
bool g_bgfxInitialized = false;

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. bgfx now renders
// into the main game window. The DX8 reference popup is a secondary window
// where the D3D8 device is moved so its output remains visible for debugging.
HWND g_bgfxWindow = nullptr;
const wchar_t * const kBgfxWindowClass = L"GGC_BgfxDebugWindow";

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. bgfx resolution
// matches the main game window. Set during Initialize from GetClientRect.
int g_bgfxWidth  = 800;
int g_bgfxHeight = 600;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.3 program handle
// for the passthrough shader pair. Created once in Initialize after bgfx::init
// succeeds, destroyed in Shutdown before bgfx::shutdown.
bgfx::ProgramHandle g_passthroughProgram = BGFX_INVALID_HANDLE;

// TheSuperHackers @refactor bobtista 12/04/2026 Phase 5A uber shader
// program. Single program handles all TSS combinations via uniforms.
bgfx::ProgramHandle g_uberProgram = BGFX_INVALID_HANDLE;

// TheSuperHackers @refactor bobtista 14/04/2026 Phase 4H tree / grass
// vertex shader program (port of Trees.nvv). Uses vs_trees + fs_uber.
// Activated by Set_Tree_Vertex_Shader_Active when W3DTreeBuffer wants
// to render swaying grass; reverts to g_uberProgram otherwise.
bgfx::ProgramHandle g_treeProgram = BGFX_INVALID_HANDLE;

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// volume program + active flag + captured stencil state bits. When active
// the next SubmitEngineDraw uses g_shadowVolumeProgram with color writes
// off and g_shadowStencilState applied.
bgfx::ProgramHandle g_shadowVolumeProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle g_shadowApplyProgram  = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uShadowColor        = BGFX_INVALID_HANDLE;

// Phase 4I.2 CSM caster pass program + render target.
bgfx::ProgramHandle      g_shadowCasterProgram = BGFX_INVALID_HANDLE;
bgfx::FrameBufferHandle  g_shadowMapFB         = BGFX_INVALID_HANDLE;
bgfx::TextureHandle      g_shadowMapDepth      = BGFX_INVALID_HANDLE;
// Light-space view+proj for the shadow map view (updated per frame).
float g_shadowLightView[16];
float g_shadowLightProj[16];
bool  g_shadowLightCaptured = false;
// World-space sun position for shadow casting, set by the engine's
// W3DShadowManager::setLightPosition via Set_Shadow_Light_Position.
// Differs from the N.L shading light (g_currentLightDirs).
float g_shadowSunPosX = 0.0f;
float g_shadowSunPosY = 0.0f;
float g_shadowSunPosZ = 1500.0f;
bool  g_shadowSunPosSet = false;
// Uniform + sampler binding point for the main-pass shader to sample
// the shadow map (Phase 4I.2 Session B, hooked into fs_uber).
bgfx::UniformHandle g_uShadowLightViewProj = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_sShadowMap            = BGFX_INVALID_HANDLE;
// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I polygon-offset
// equivalent for stencil shadow volume passes. bgfx has no state bit
// for polygon offset, so the bias is applied in the volume vertex
// shader (vs_shadow_volume.sc). .x is the post-projection Z offset;
// negative values push toward the camera (closer to near plane).
bgfx::UniformHandle g_uShadowBias         = BGFX_INVALID_HANDLE;
bool     g_shadowVolumeActive = false;
uint32_t g_shadowStencilFront = BGFX_STENCIL_NONE;
uint32_t g_shadowStencilBack  = BGFX_STENCIL_NONE;

// Engine stencil state captured from Set_Stencil_* calls. Encoded lazily
// into g_shadowStencilFront every time a component changes.
bool     g_stencilEnabled     = false;
uint32_t g_stencilRef          = 0;
uint32_t g_stencilReadMask     = 0xFF;
uint32_t g_stencilFuncBits     = BGFX_STENCIL_TEST_ALWAYS;
uint32_t g_stencilPassOpBits   = BGFX_STENCIL_OP_PASS_Z_KEEP;
uint32_t g_stencilFailOpBits   = BGFX_STENCIL_OP_FAIL_S_KEEP;
uint32_t g_stencilZFailOpBits  = BGFX_STENCIL_OP_FAIL_Z_KEEP;

// Current engine cull mode (RB_CULL_NONE / CW / CCW). Captured by
// BgfxBackend::Set_Cull_Mode; consumed by the shadow volume submit so
// the two-pass stencil algorithm (CW INCR pass + CCW DECR pass) writes
// stencil counts that don't cancel each other out.
int g_cullModeBits = 0;  // 0 = RB_CULL_NONE, 1 = CW, 2 = CCW

static uint32_t MapCmpFuncToBgfxStencilTest(int f)
{
    switch (f)
    {
        case 1: return BGFX_STENCIL_TEST_NEVER;
        case 2: return BGFX_STENCIL_TEST_LESS;
        case 3: return BGFX_STENCIL_TEST_EQUAL;
        case 4: return BGFX_STENCIL_TEST_LEQUAL;
        case 5: return BGFX_STENCIL_TEST_GREATER;
        case 6: return BGFX_STENCIL_TEST_NOTEQUAL;
        case 7: return BGFX_STENCIL_TEST_GEQUAL;
        case 8: default: return BGFX_STENCIL_TEST_ALWAYS;
    }
}

static uint32_t MapStencilOpToBgfxPassZ(int op)
{
    switch (op)
    {
        case 1: return BGFX_STENCIL_OP_PASS_Z_KEEP;
        case 2: return BGFX_STENCIL_OP_PASS_Z_ZERO;
        case 3: return BGFX_STENCIL_OP_PASS_Z_REPLACE;
        case 4: return BGFX_STENCIL_OP_PASS_Z_INCRSAT;
        case 5: return BGFX_STENCIL_OP_PASS_Z_DECRSAT;
        case 6: return BGFX_STENCIL_OP_PASS_Z_INVERT;
        case 7: return BGFX_STENCIL_OP_PASS_Z_INCR;
        case 8: return BGFX_STENCIL_OP_PASS_Z_DECR;
        default: return BGFX_STENCIL_OP_PASS_Z_KEEP;
    }
}

static uint32_t MapStencilOpToBgfxFailS(int op)
{
    switch (op)
    {
        case 1: return BGFX_STENCIL_OP_FAIL_S_KEEP;
        case 2: return BGFX_STENCIL_OP_FAIL_S_ZERO;
        case 3: return BGFX_STENCIL_OP_FAIL_S_REPLACE;
        case 4: return BGFX_STENCIL_OP_FAIL_S_INCRSAT;
        case 5: return BGFX_STENCIL_OP_FAIL_S_DECRSAT;
        case 6: return BGFX_STENCIL_OP_FAIL_S_INVERT;
        case 7: return BGFX_STENCIL_OP_FAIL_S_INCR;
        case 8: return BGFX_STENCIL_OP_FAIL_S_DECR;
        default: return BGFX_STENCIL_OP_FAIL_S_KEEP;
    }
}

static uint32_t MapStencilOpToBgfxFailZ(int op)
{
    switch (op)
    {
        case 1: return BGFX_STENCIL_OP_FAIL_Z_KEEP;
        case 2: return BGFX_STENCIL_OP_FAIL_Z_ZERO;
        case 3: return BGFX_STENCIL_OP_FAIL_Z_REPLACE;
        case 4: return BGFX_STENCIL_OP_FAIL_Z_INCRSAT;
        case 5: return BGFX_STENCIL_OP_FAIL_Z_DECRSAT;
        case 6: return BGFX_STENCIL_OP_FAIL_Z_INVERT;
        case 7: return BGFX_STENCIL_OP_FAIL_Z_INCR;
        case 8: return BGFX_STENCIL_OP_FAIL_Z_DECR;
        default: return BGFX_STENCIL_OP_FAIL_Z_KEEP;
    }
}

static void UpdateShadowStencilState()
{
    if (!g_stencilEnabled)
    {
        g_shadowStencilFront = BGFX_STENCIL_NONE;
        g_shadowStencilBack  = BGFX_STENCIL_NONE;
        return;
    }
    g_shadowStencilFront = g_stencilFuncBits
        | BGFX_STENCIL_FUNC_REF(g_stencilRef & 0xFF)
        | BGFX_STENCIL_FUNC_RMASK(g_stencilReadMask & 0xFF)
        | g_stencilFailOpBits
        | g_stencilZFailOpBits
        | g_stencilPassOpBits;
    g_shadowStencilBack = BGFX_STENCIL_NONE;
}

bgfx::UniformHandle g_uSwayTable     = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uShroudOffset  = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uShroudScale   = BGFX_INVALID_HANDLE;
// Sway table: 11 entries (no-sway at index 0, MAX_SWAY_TYPES=10 active).
float g_currentSwayTable[11][4] = {{0}};
float g_currentShroudOffset[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
float g_currentShroudScale[4]   = { 0.0f, 0.0f, 1.0f, 1.0f };
bool  g_treeShaderActive = false;

// Sampler uniform shared by all textured fragment shaders. Bound to stage 0.
bgfx::UniformHandle g_sTex0        = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_sTex1        = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_sTex2        = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_sTex3        = BGFX_INVALID_HANDLE;
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.9 material
// diffuse uniform. Carries the VertexMaterialClass::Get_Diffuse color
// + opacity from Set_Material into the fragment shader so team colors
// (which the W3D engine writes into the material diffuse channel) and
// alpha fades modulate the output.
bgfx::UniformHandle g_uMatDiffuse  = BGFX_INVALID_HANDLE;
float               g_currentMatDiffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
// Alpha-test parameters consumed by fs_textured_lit_atest. .x is the
// reference threshold in [0, 1]; engine writes ShaderClass alpha-ref / 255.
// Named u_atestParams (not u_alphaRef) to avoid bgfx_shader.sh's internal
// u_alphaRef4 conflict. See fs_textured_lit_atest.sc for details.
bgfx::UniformHandle g_uAtestParams = BGFX_INVALID_HANDLE;

// Phase 5A TSS operation uniforms. Encode the DX8 texture stage state
// operations so the uber fragment shader can evaluate them at runtime.
bgfx::UniformHandle g_uTssOps0    = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uTssOps1    = BGFX_INVALID_HANDLE;
float               g_currentTssOps0[4] = { 3.0f, 3.0f, 0.0f, 0.0f }; // default: MODULATE color+alpha, no secondary
float               g_currentTssOps1[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // default: arg1=TEXTURE for all
float               g_currentAtestRef   = 0.0f; // alpha test reference, 0 = disabled

// Post-ShaderClass blend/alpha-test overrides. Set by Override_Blend /
// Override_Alpha_Test, cleared by Clear_State_Overrides (called from Set_Shader).
bool                g_blendOverrideActive = false;
uint64_t            g_blendOverrideBits  = 0;
bool                g_atestOverrideActive = false;
float               g_atestOverrideRef   = 0.0f;
bool                g_suppressBgfxDraw   = false;
int                 g_colorWriteOverride = -1;
bool                g_effectOverlayActive = false;
// TheSuperHackers @feature bobtista 16/04/2026 Phase 4L 2D overlay active flag.
// Set by Set_View_Identity (Render2DClass enters 2D mode), cleared by
// Set_Transform(VIEW) when a real camera view is restored or at Begin_Scene.
bool                g_2DOverlayActive = false;

// UV set selection: when > 0, the fragment shader samples stage 0
// from v_texcoord1 instead of v_texcoord0. Set by the terrain shader
// when it changes D3DTSS_TEXCOORDINDEX to 1 for the blend pass.
bgfx::UniformHandle g_uTexcoordSelect = BGFX_INVALID_HANDLE;
float               g_currentTexcoordSelect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Shroud pass: the D3D8 path uses D3DTSS_TCI_CAMERASPACEPOSITION to auto-
// generate UVs from camera-space vertex positions, combined with a texture
// transform matrix. bgfx has no equivalent, so we upload the full texture
// matrix and let the vertex shader compute the UVs explicitly.
bgfx::UniformHandle g_uShroudParams = BGFX_INVALID_HANDLE;
// .y = terrain blend flag: when > 0, shader does lerp(tex0, tex1, vertex_alpha)
// using UV set 0 for tex0 and UV set 1 for tex1

// Phase 5B: lighting uniforms. The engine supports up to 4 lights
// (typically 1 directional sun + 0-3 point lights). We pack light data
// into vec4 arrays and push them per-draw so the uber fragment shader
// can evaluate real N.L lighting instead of the hardcoded fake sun.
// 4 lights packed into vec4 arrays (one element per light).
// u_lightDirs[i].xyz = direction toward light, .w = enabled flag
// u_lightColors[i].rgb = diffuse color
bgfx::UniformHandle g_uLightDirs      = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uLightColors    = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uSceneAmbient   = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_uLightingEnabled = BGFX_INVALID_HANDLE;
// Defaults match the old hardcoded sun: direction TOWARD light (positive),
// white diffuse, reasonable ambient. These are used until the first
// Set_Light_Environment call provides real game lights.
float               g_currentLightDirs[4][4]   = {
    { 0.35f, 0.55f, 0.75f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};
float               g_currentLightColors[4][4] = {
    { 0.75f, 0.75f, 0.75f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};
float               g_currentSceneAmbient[4] = { 0.45f, 0.45f, 0.45f, 1.0f };

// Phase 5B: tracks whether the current material has lighting enabled.
// When false, the vertex color contains pre-baked lighting (terrain)
// and the fragment shader should NOT apply N.L lighting on top.
float               g_currentLightingEnabled[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4F.1 default 1x1
// white texture. Real Set_Texture wiring is not in place yet, so the
// textured shaders need SOMETHING bound to s_tex0 or D3D11 returns
// undefined values. A 1x1 opaque white texture is a sensible default
// because the fragment shader does texColor * v_color0 - white * vc
// gives the vertex color through unmodified.
bgfx::TextureHandle g_defaultWhiteTexture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_defaultTransparentTexture = BGFX_INVALID_HANDLE;
// Track which engine textures are render targets so we can use the
// transparent fallback instead of white.
std::unordered_map<const TextureBaseClass *, bool> g_renderTargetSet;

struct BgfxFramebufferEntry
{
    bgfx::FrameBufferHandle fb;
    bgfx::TextureHandle     colorTex;
    uint16_t                width;
    uint16_t                height;
};
std::unordered_map<const TextureBaseClass *, BgfxFramebufferEntry> g_framebufferCache;

// Vertex layout used by the test triangle. Position + packed RGBA color.
// Initialized in Initialize since bgfx::VertexLayout::begin needs bgfx to be
// up and running (it queries the active renderer for the pos type).
bgfx::VertexLayout g_triangleLayout;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.1 standard
// vertex layouts. One per common FVF format (see PHASE4_INVENTORY.md
// FVF table). Initialized in Initialize, used by Phase 4C.2's vertex
// buffer creation path. Names follow the FVF tags - P=position,
// N=normal, D=diffuse (color0), T<n>=texcoord<n>.
bgfx::VertexLayout g_layoutP;            // XYZ
bgfx::VertexLayout g_layoutPN;           // XYZ + NORMAL
bgfx::VertexLayout g_layoutPNT1;         // XYZ + NORMAL + TEX1
bgfx::VertexLayout g_layoutPNT2;         // XYZ + NORMAL + TEX1 + TEX2
bgfx::VertexLayout g_layoutPT1;          // XYZ + TEX1
bgfx::VertexLayout g_layoutPDT1;         // XYZ + DIFFUSE + TEX1
bgfx::VertexLayout g_layoutPNDT1;        // XYZ + NORMAL + DIFFUSE + TEX1
bgfx::VertexLayout g_layoutPNDT2;        // XYZ + NORMAL + DIFFUSE + TEX1 + TEX2

void BuildStandardVertexLayouts()
{
    g_layoutP
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

    g_layoutPN
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
        .end();

    g_layoutPNT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_layoutPNT2
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
        .end();

    g_layoutPT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_layoutPDT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_layoutPNDT1
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    g_layoutPNDT2
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
        .end();
}

struct TriangleVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
};
}

BgfxBackend::BgfxBackend()
{
    WWDEBUG_SAY(("[BgfxBackend] Phase 4 session 1 backend constructed. "
                 "Most IRenderBackend methods are still no-ops; DX8Wrapper "
                 "still owns the real device. bgfx is initialized in Noop "
                 "renderer mode only. See PHASE4.md."));
}

BgfxBackend::~BgfxBackend()
{
}

// -- Backend lifecycle -------------------------------------------------------
//
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 3. BgfxBackend
// creates its own top-level popup window and hands that HWND to bgfx::init.
// Session 2b proved that DWM promotes whichever swapchain is actively
// presenting to the game's main HWND, so sharing one window with DX8 loses
// DX8's output. Using a separate window sidesteps the conflict entirely:
// DX8 owns the main game window, bgfx owns a small debug popup next to it.
// Once we're ready to make bgfx the primary renderer we'll destroy the
// debug window and point bgfx at the main HWND. See PHASE4.md.

namespace
{
// Minimal window procedure for the bgfx debug window. DefWindowProc handles
// everything we care about for a pure rendering target (close, resize, focus).
LRESULT CALLBACK BgfxDebugWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Register a window class for the bgfx debug window. Returns true on success
// or if the class was already registered.
bool RegisterBgfxDebugWindowClass()
{
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = BgfxDebugWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kBgfxWindowClass;

    if (RegisterClassExW(&wc) == 0)
    {
        const DWORD err = GetLastError();
        if (err == ERROR_CLASS_ALREADY_EXISTS)
        {
            return true;
        }
        WWDEBUG_SAY(("[BgfxBackend] RegisterClassExW failed, GetLastError=%lu.",
                     err));
        return false;
    }
    return true;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.2 ShaderClass
// translation table. Maps a ShaderClass instance to (program handle,
// bgfx state bits). Defined but not yet called by any draw path - the
// wiring lands in a later session along with the per-stage TSS uber-shader.
//
// Mapping rules (see PHASE4_INVENTORY.md preset table):
//   - alpha-test enabled & textured & lit -> g_texturedLitAtestProgram
//   - textured & lit                      -> g_texturedLitProgram
//   - textured & !lit                     -> g_texturedUnlitProgram
//   - !textured & lit                     -> g_solidLitProgram
//   - else                                -> g_passthroughProgram (debug)
//
// State bit translation is mechanical: depth compare, depth write, color
// write, blend factors, cull. Detail-blend / fog / specular gradient are
// not handled here yet - they need shader code, not state bits, and that
// lands when we add the terrain shader and the fixed-function lighting
// path.

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

// Phase 5A: extract TSS operation IDs from ShaderClass preset bits.
// Maps the same logic as shader.cpp's Apply() into float IDs that the
// uber fragment shader evaluates at runtime.
//
// TSS op IDs must match the #defines in fs_uber.sc:
//   0=DISABLE 1=SELECTARG1 2=SELECTARG2 3=MODULATE 4=MODULATE2X
//   5=ADD 6=ADDSIGNED 7=SUBTRACT 8=BLENDTEXALPHA 9=BLENDCURALPHA 10=ADDSMOOTH
// Arg source IDs: 0=TEXTURE 1=DIFFUSE 2=CURRENT

void BuildTssOpsForShader(const ShaderClass & shader,
                          float * ops0, float * ops1, float * atestRef)
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
                priColorOp  = 1.0f; // SELECTARG1
                priAlphaOp  = 1.0f; // SELECTARG1
                priCArg1Src = 0.0f; // TEXTURE
                priAArg1Src = 0.0f; // TEXTURE
                break;
            default:
            case ShaderClass::GRADIENT_MODULATE:
                priColorOp  = 3.0f; // MODULATE
                priAlphaOp  = 3.0f; // MODULATE
                priCArg1Src = 0.0f; // TEXTURE
                priAArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::GRADIENT_ADD:
                priColorOp  = 5.0f; // ADD
                priAlphaOp  = 3.0f; // MODULATE (alpha always modulates)
                priCArg1Src = 0.0f; // TEXTURE
                priAArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::GRADIENT_MODULATE2X:
                priColorOp  = 4.0f; // MODULATE2X
                priAlphaOp  = 3.0f; // MODULATE
                priCArg1Src = 0.0f; // TEXTURE
                priAArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::GRADIENT_BUMPENVMAP:
            case ShaderClass::GRADIENT_BUMPENVMAPLUMINANCE:
                priColorOp  = 1.0f; // SELECTARG1
                priAlphaOp  = 1.0f; // SELECTARG1
                priCArg1Src = 1.0f; // DIFFUSE
                priAArg1Src = 1.0f; // DIFFUSE
                break;
        }

        switch (shader.Get_Post_Detail_Color_Func())
        {
            default:
            case ShaderClass::DETAILCOLOR_DISABLE:
                secColorOp = 0.0f;
                break;
            case ShaderClass::DETAILCOLOR_DETAIL:
                secColorOp  = 1.0f; // SELECTARG1
                secCArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILCOLOR_SCALE:
                secColorOp  = 3.0f; // MODULATE
                secCArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILCOLOR_INVSCALE:
                secColorOp  = 10.0f; // ADDSMOOTH
                secCArg1Src = 0.0f;  // TEXTURE
                break;
            case ShaderClass::DETAILCOLOR_ADD:
                secColorOp  = 5.0f; // ADD
                secCArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILCOLOR_SUB:
                secColorOp  = 7.0f; // SUBTRACT (current - tex)
                secCArg1Src = 2.0f; // CURRENT as arg1 (so result = current - tex)
                break;
            case ShaderClass::DETAILCOLOR_SUBR:
                secColorOp  = 7.0f; // SUBTRACT (tex - current)
                secCArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILCOLOR_BLEND:
                secColorOp  = 9.0f; // BLENDCURRENTALPHA
                secCArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILCOLOR_DETAILBLEND:
                secColorOp  = 8.0f; // BLENDTEXTUREALPHA
                secCArg1Src = 0.0f; // TEXTURE
                break;
        }

        switch (shader.Get_Post_Detail_Alpha_Func())
        {
            default:
            case ShaderClass::DETAILALPHA_DISABLE:
                secAlphaOp = 0.0f;
                break;
            case ShaderClass::DETAILALPHA_DETAIL:
                secAlphaOp  = 1.0f; // SELECTARG1
                secAArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILALPHA_SCALE:
                secAlphaOp  = 3.0f; // MODULATE
                secAArg1Src = 0.0f; // TEXTURE
                break;
            case ShaderClass::DETAILALPHA_INVSCALE:
                secAlphaOp  = 10.0f; // ADDSMOOTH
                secAArg1Src = 0.0f;  // TEXTURE
                break;
        }
    }
    else
    {
        switch (shader.Get_Primary_Gradient())
        {
            case ShaderClass::GRADIENT_DISABLE:
                priColorOp = 0.0f; // DISABLE (output black/default)
                priAlphaOp = 0.0f;
                break;
            default:
            case ShaderClass::GRADIENT_MODULATE:
            case ShaderClass::GRADIENT_ADD:
                priColorOp  = 2.0f; // SELECTARG2
                priAlphaOp  = 2.0f; // SELECTARG2
                priCArg1Src = 0.0f;
                priAArg1Src = 0.0f;
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
        *atestRef = 0x60 / 255.0f;
    }
    else
    {
        *atestRef = 0.0f;
    }
}

uint64_t BuildBgfxStateForShader(const ShaderClass & shader)
{
    uint64_t state = 0;

    state |= TranslateDepthCompare(shader.Get_Depth_Compare());

    if (shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE)
    {
        state |= BGFX_STATE_WRITE_Z;
    }

    if (shader.Get_Color_Mask() == ShaderClass::COLOR_WRITE_ENABLE)
    {
        state |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    }

    const uint64_t srcBits = TranslateBlendFactor(shader.Get_Src_Blend_Func());
    const uint64_t dstBits = TranslateBlendFactor(shader.Get_Dst_Blend_Func());
    state |= BGFX_STATE_BLEND_FUNC(srcBits, dstBits);

    if (shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_ENABLE)
    {
        // W3D used D3D's clockwise = front by default. bgfx CW culls the
        // clockwise face (i.e. the back face when CW is the front), so
        // BGFX_STATE_CULL_CW matches the W3D convention.
        state |= BGFX_STATE_CULL_CW;
    }

    return state;
}

[[maybe_unused]] const auto kShaderTranslationAnchor = &BuildTssOpsForShader;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.2 generic FVF
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

bool BuildBgfxLayoutForFVF(const FVFInfoClass & fvf, bgfx::VertexLayout & out)
{
    const unsigned bits      = fvf.Get_FVF();
    const unsigned totalSize = fvf.Get_FVF_Size();

    out.begin();
    unsigned cursor = 0;

    if ((bits & D3DFVF_XYZ) == D3DFVF_XYZ)
    {
        AddAttribAtOffset(out, cursor, fvf.Get_Location_Offset(),
                          bgfx::Attrib::Position, 3, bgfx::AttribType::Float, false,
                          3 * sizeof(float));
    }
    else if ((bits & D3DFVF_XYZRHW) == D3DFVF_XYZRHW)
    {
        // Pre-transformed: 4 floats (x, y, z, rhw). bgfx has no native
        // pre-transformed attribute - declare as 4-component position
        // and the shader is expected to bypass the projection matrix.
        AddAttribAtOffset(out, cursor, fvf.Get_Location_Offset(),
                          bgfx::Attrib::Position, 4, bgfx::AttribType::Float, false,
                          4 * sizeof(float));
    }

    if ((bits & D3DFVF_NORMAL) == D3DFVF_NORMAL)
    {
        AddAttribAtOffset(out, cursor, fvf.Get_Normal_Offset(),
                          bgfx::Attrib::Normal, 3, bgfx::AttribType::Float, false,
                          3 * sizeof(float));
    }

    if ((bits & D3DFVF_DIFFUSE) == D3DFVF_DIFFUSE)
    {
        // D3DCOLOR is BGRA u8x4 packed; bgfx Color0 with normalized=true
        // matches that on D3D11.
        AddAttribAtOffset(out, cursor, fvf.Get_Diffuse_Offset(),
                          bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true,
                          sizeof(uint32_t));
    }

    if ((bits & D3DFVF_SPECULAR) == D3DFVF_SPECULAR)
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

    const unsigned numTex = (bits & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (unsigned i = 0; i < numTex && i < 8; ++i)
    {
        unsigned componentCount = 2;
        if      ((static_cast<int>(bits) & D3DFVF_TEXCOORDSIZE1(i)) == D3DFVF_TEXCOORDSIZE1(i)) componentCount = 1;
        else if ((static_cast<int>(bits) & D3DFVF_TEXCOORDSIZE3(i)) == D3DFVF_TEXCOORDSIZE3(i)) componentCount = 3;
        else if ((static_cast<int>(bits) & D3DFVF_TEXCOORDSIZE4(i)) == D3DFVF_TEXCOORDSIZE4(i)) componentCount = 4;
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

[[maybe_unused]] const auto kFvfLayoutAnchor = &BuildBgfxLayoutForFVF;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.3 vertex/index
// buffer caches. The engine recycles VertexBufferClass / IndexBufferClass
// instances throughout its lifetime, so we cache the bgfx handle keyed
// by the source pointer. On first encounter we lock the source buffer
// for read, copy bytes via bgfx::copy(), and create a bgfx static buffer.
// Cache is destroyed wholesale in Shutdown - we don't try to react to
// VertexBufferClass destruction yet, which means handles may outlive
// their source until shutdown. That's fine for the prototype.
//
// Pointer reuse is theoretically possible but unlikely in a single
// session; if it bites us we'll add a generation counter or hook the
// destructor.

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.6 switched from
// static bgfx VB/IB handles to dynamic ones. Rigid mesh category containers
// fill their shared VB / IB one sub-range at a time via AppendLockClass,
// which requires in-place sub-range updates that only dynamic bgfx buffers
// support. Full-buffer writes (WriteLockClass) also go through the same
// dynamic path - created once, updated with bgfx::update as the engine
// rewrites the buffer.
// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I cache entries
// store (handle, num_verts, stride) so we can detect the case where the
// engine destroys a VertexBufferClass and reuses the memory address for
// a new VB with different dimensions — otherwise we'd hand back a stale
// handle and bgfx would truncate writes / crash on staging creation.
struct BgfxVbCacheEntry {
    bgfx::DynamicVertexBufferHandle handle;
    uint32_t num_verts;
    uint32_t stride;
};
struct BgfxIbCacheEntry {
    bgfx::DynamicIndexBufferHandle handle;
    uint32_t num_indices;
};
std::unordered_map<const VertexBufferClass *, BgfxVbCacheEntry> g_vbCache;
std::unordered_map<const IndexBufferClass *,  BgfxIbCacheEntry> g_ibCache;
std::unordered_map<const TextureBaseClass *,  bgfx::TextureHandle>             g_textureCache;

// The bgfx texture currently bound to stage 0 by Set_Texture. Used by
// SubmitEngineDraw - falls back to g_defaultWhiteTexture if invalid.
bgfx::TextureHandle g_currentBgfxTexture0 = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_currentBgfxTexture1 = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_currentBgfxTexture2 = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_currentBgfxTexture3 = BGFX_INVALID_HANDLE;

// Per-stage sampler flags captured from the source TextureClass's
// Get_U/V_Addr_Mode in Set_Texture. Default 0 = use bgfx's creation-time
// default (usually linear filter + wrap). Shoreline LUT needs CLAMP
// because its U coord can exceed [0,1] and WRAP produces a visible
// stripe/checker artifact at the boundary.
uint32_t g_currentSamplerFlags0 = 0;
uint32_t g_currentSamplerFlags1 = 0;
uint32_t g_currentSamplerFlags2 = 0;
uint32_t g_currentSamplerFlags3 = 0;

// The most recent buffers and offsets cached from Set_Vertex_Buffer /
// Set_Index_Buffer. Read by Draw_Triangles when it issues the bgfx
// submit. Cleared (made invalid) on Shutdown.
bgfx::DynamicVertexBufferHandle g_currentBgfxVB    = BGFX_INVALID_HANDLE;
bgfx::DynamicIndexBufferHandle  g_currentBgfxIB    = BGFX_INVALID_HANDLE;
unsigned short                  g_currentIBOffset  = 0;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.2 transient
// (dynamic) buffer state. Capture_Dynamic_Vertex_Data allocs a bgfx
// transient VB and records the owning DynamicVBAccessClass pointer so
// the matching Set_Vertex_Buffer(DynamicVBAccessClass&) call can claim
// it. The transient buffers are auto-freed at bgfx::frame time; we
// only track validity within the current frame.
struct PendingTransientVB
{
    bool                        valid;
    const DynamicVBAccessClass * owner;
    bgfx::TransientVertexBuffer  tvb;
};
struct PendingTransientIB
{
    bool                        valid;
    const DynamicIBAccessClass * owner;
    bgfx::TransientIndexBuffer   tib;
};
PendingTransientVB g_pendingDynVB = { false, nullptr, {} };
PendingTransientIB g_pendingDynIB = { false, nullptr, {} };

// Current draw call uses transient buffers if these are set. They
// shadow the static VB/IB handles above - SubmitEngineDraw picks the
// transient path when these are true.
bool                        g_currentUseTransientVB = false;
bgfx::TransientVertexBuffer g_currentTransientVB    = {};
bool                        g_currentUseTransientIB = false;
bgfx::TransientIndexBuffer  g_currentTransientIB    = {};

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4E.4 transform
// capture. The engine calls Set_Transform with world / view / projection
// matrices in W3D row-major form (Vector4 Row[4]). bgfx wants column-
// major float[16] for setViewTransform / setTransform. We convert with
// a transpose copy. Whether the resulting matrices produce visually
// correct geometry depends on multiplication order conventions; if
// things look mirrored or upside-down, the row-vector vs column-vector
// convention is the next thing to investigate.
//
// We capture all three matrices and apply them per-submit. View and
// projection are written via setViewTransform on view 1; world is set
// per-submit via setTransform.
float g_bgfxWorld[16];
float g_bgfxView[16];
float g_bgfxProj[16];
bool  g_bgfxViewProjDirty = true;

// Snapshot of g_bgfxView and g_bgfxProj captured at the first opaque
// draw of each frame. Re-applied to view 1 at End_Scene to prevent
// later Set_Projection calls (water, shadows, sneak attack) from
// retroactively stomping the camera projection via setViewTransform.
float g_bgfxCameraView[16];
float g_bgfxCameraProj[16];
bool  g_bgfxCameraCaptured = false;

// Engine geometry submits to its own view so it does not collide with
// the test triangle on view 0. View 0 keeps the test triangle for the
// "is bgfx alive" sentinel; view 1 is engine geometry under engine
// transforms. Both render to the popup back buffer.
const bgfx::ViewId kBgfxDebugView  = 0;
const bgfx::ViewId kBgfxEngineView = 1;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.12 dedicated
// view id for sorted draws. View 2's view matrix is permanently
// identity and its projection tracks view 1's. Per-batch sort
// transforms get pre-multiplied into g_bgfxSortWorld so view 2 never
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
// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I dedicated
// views for stencil shadow volumes + darken-apply pass. Running in
// sequential order on the same framebuffer as view 1 guarantees:
//  - Opaque geometry in view 1 has populated the depth buffer before
//    view 6 shadow-volume submits execute (the depth-pass algorithm
//    requires this; default program+depth sort on view 1 could reorder
//    the volume submits ahead of opaque draws that populate depth).
//  - The two-pass volume algorithm (CW front INCR, then CCW back DECR)
//    executes in submit order so paired increments and decrements land
//    on the same stencil bit.
//  - The apply pass on view 7 runs after all stencil writes are done.
// No color/depth/stencil clear on these views - they share the engine
// view's attachments and the engine view already cleared them.
const bgfx::ViewId kBgfxShadowVolumeView = 6;
const bgfx::ViewId kBgfxShadowApplyView  = 7;
// Phase 4I.2 CSM caster pass view. Depth-only render target, renders
// opaque casters from the sun's perspective into a D24 shadow map.
const bgfx::ViewId kBgfxShadowMapView    = 8;
// TheSuperHackers @feature bobtista 16/04/2026 Phase 4L dedicated view for
// 2D UI overlay draws (Render2DClass). Sequential mode preserves draw order;
// identity view+projection so screen-space quads render at their authored
// positions. Composites over the 3D scene as the last view in the order.
const bgfx::ViewId kBgfxUIView           = 10;
const uint16_t kShadowMapResolution      = 4096;
const float kShadowOrthoSize             = 1200.0f;
const float kShadowCameraDistance        = 2000.0f;
const float kShadowOrthoFarMargin        = 1000.0f;
const int kSwayTableEntries              = 11;

// Render-to-texture state. Set by Set_Render_Target_With_Z, cleared
// when the back buffer is restored. SubmitEngineDraw routes to
// kBgfxRTTView while this is true.
bool g_renderToTexture = false;

// True between Begin_Sorted_Batch_Pass and End_Sorted_Batch_Pass;
// SubmitEngineDraw routes to kBgfxEngineSortView and uses
// g_bgfxSortWorld while this is set.
bool  g_inSortFlush = false;

// Per-batch effective world for sorted draws: the pre-multiplied
// sortView * sortWorld (in bgfx column-major form) captured from the
// engine's render_state by Capture_Sorted_Batch_Transforms.
float g_bgfxSortWorld[16];
// Phase 4I.2 CSM: raw model-to-world matrix for sorted draws, WITHOUT
// the camera view baked in. Used for shadow caster submissions where
// the light's view+proj replaces the camera's. g_bgfxSortWorld has
// model*cameraView which contaminates the light-space transform.
float g_bgfxSortWorldRaw[16];

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.13 Set by
// Submit_Sorted_Draw after it emits the bgfx submit for a sorting VB
// direct draw. The outer BgfxBackend::Draw_Triangles consumes this
// flag to skip its SubmitEngineDraw - the draw was already issued
// with correctly remapped args against the inner dynamic buffers,
// so falling through would emit a second, incorrect submit.
bool  g_skipNextSubmitEngineDraw = false;

// Water override — set by Override_Material_Opacity, consumed by
// SubmitEngineDraw to route to the water view and apply DESTALPHA blend.
bool g_waterOverrideActive = false;

// Snapshot of g_bgfxProj at the time the sort flush runs. The engine
// calls Set_Projection_Transform_With_Z_Bias multiple times per frame
// (camera, water reflections, shadows). The LAST call may use a tiny
// near-field frustum that clips all sort geometry. We capture the
// projection at sort-flush time (when it's still the camera projection)
// and re-apply it to view 2 at End_Scene time.
float g_bgfxSortProj[16];
bool  g_bgfxSortProjCaptured = false;

void IdentityMatrix(float * out)
{
    out[0]  = 1.0f; out[1]  = 0.0f; out[2]  = 0.0f; out[3]  = 0.0f;
    out[4]  = 0.0f; out[5]  = 1.0f; out[6]  = 0.0f; out[7]  = 0.0f;
    out[8]  = 0.0f; out[9]  = 0.0f; out[10] = 1.0f; out[11] = 0.0f;
    out[12] = 0.0f; out[13] = 0.0f; out[14] = 0.0f; out[15] = 1.0f;
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

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. Aspect correction
// is no longer needed because bgfx renders into the same window as the game.
// The engine's projection matrix already matches the bgfx framebuffer aspect.

// TheSuperHackers @bugfix bobtista 11/04/2026 Phase 4C.3 buffer copy
// kill switch. The Intel UHD Graphics driver corrupts the dx8 GPU
// copy of POOL_DEFAULT vertex buffers even when we lock with
// D3DLOCK_READONLY. The read-side path is fully disabled; bgfx VBs
// are now populated by the write-side capture hooks
// (Capture_Vertex_Data / Capture_Index_Data) called from the engine
// at write-lock time. See Phase 4C.4 in the comments below.
constexpr bool kBgfxSkipBufferRead = true;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4F.2 texture
// capture. Unlike vertex buffers, W3D textures default to POOL_MANAGED,
// which is safe to lock with D3DLOCK_READONLY on the Intel UHD driver.
// We can read the source d3d8 texture data on demand from inside
// Set_Texture without an engine-side write hook. POOL_DEFAULT textures
// (render targets, dynamic textures) are skipped to avoid the same
// corruption that hit vertex buffers.

bgfx::TextureFormat::Enum TranslateWW3DFormat(WW3DFormat fmt)
{
    switch (fmt)
    {
        case WW3D_FORMAT_A8R8G8B8:
        case WW3D_FORMAT_X8R8G8B8:
            // D3D8 stores ARGB as 0xAARRGGBB which on little-endian
            // memory is BB GG RR AA - matches bgfx BGRA8 byte order.
            return bgfx::TextureFormat::BGRA8;
        case WW3D_FORMAT_R5G6B5:    return bgfx::TextureFormat::R5G6B5;
        case WW3D_FORMAT_A1R5G5B5:  return bgfx::TextureFormat::BGR5A1;
        case WW3D_FORMAT_A4R4G4B4:  return bgfx::TextureFormat::BGRA4;
        case WW3D_FORMAT_A8:        return bgfx::TextureFormat::A8;
        case WW3D_FORMAT_L8:        return bgfx::TextureFormat::R8;
        case WW3D_FORMAT_DXT1:      return bgfx::TextureFormat::BC1;
        case WW3D_FORMAT_DXT2:
        case WW3D_FORMAT_DXT3:      return bgfx::TextureFormat::BC2;
        case WW3D_FORMAT_DXT4:
        case WW3D_FORMAT_DXT5:      return bgfx::TextureFormat::BC3;
        default:                    return bgfx::TextureFormat::Unknown;
    }
}

bgfx::TextureHandle EnsureBgfxTexture(TextureBaseClass * tex)
{
    if (tex == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }
    auto it = g_textureCache.find(tex);
    if (it != g_textureCache.end())
    {
        return it->second;
    }

    // Only handle TextureClass (regular 2D) for now. Cube and volume
    // textures take a different path and would need their own helpers.
    TextureClass * tex2d = tex->As_TextureClass();
    if (tex2d == nullptr)
    {
        g_textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    if (tex->Get_Pool() == TextureBaseClass::POOL_DEFAULT)
    {
        auto fbIt = g_framebufferCache.find(tex);
        if (fbIt != g_framebufferCache.end())
        {
            static bool s_loggedRTTResolve = false;
            if (!s_loggedRTTResolve)
            {
                s_loggedRTTResolve = true;
                WWDEBUG_SAY(("[BgfxBackend] RTT RESOLVE: POOL_DEFAULT tex=%p "
                             "resolved to framebuffer color texture %dx%d",
                             tex, fbIt->second.width, fbIt->second.height));
            }
            return fbIt->second.colorTex;
        }

        static int s_poolDefaultCount = 0;
        if (s_poolDefaultCount < 10)
        {
            ++s_poolDefaultCount;
            WWDEBUG_SAY(("[BgfxBackend] POOL_DEFAULT texture skipped #%d: %s %dx%d fmt=%d",
                         s_poolDefaultCount,
                         tex2d->Get_Full_Path().str(),
                         tex2d->Get_Width(), tex2d->Get_Height(),
                         static_cast<int>(tex2d->Get_Texture_Format())));
        }
        g_renderTargetSet[tex] = true;
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(tex2d->Get_Texture_Format());
    if (bgfxFmt == bgfx::TextureFormat::Unknown)
    {
        static bool s_loggedUnknownFmt = false;
        if (!s_loggedUnknownFmt)
        {
            s_loggedUnknownFmt = true;
            WWDEBUG_SAY(("[BgfxBackend] UNKNOWN texture format: %s ww3dfmt=%d",
                         tex2d->Get_Full_Path().str(),
                         static_cast<int>(tex2d->Get_Texture_Format())));
        }
        g_textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    IDirect3DBaseTexture8 * baseTex = tex->Peek_D3D_Base_Texture();
    if (baseTex == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }

    IDirect3DTexture8 * d3dTex = static_cast<IDirect3DTexture8 *>(baseTex);

    D3DSURFACE_DESC desc;
    if (FAILED(d3dTex->GetLevelDesc(0, &desc)))
    {
        g_textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    D3DLOCKED_RECT locked = { 0 };
    HRESULT hr = d3dTex->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
    if (FAILED(hr) || locked.pBits == NULL)
    {
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxTexture LockRect failed hr=0x%08x", static_cast<unsigned>(hr)));
        g_textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    // TheSuperHackers @bugfix bobtista 16/04/2026 Compute tightly-packed row
    // pitch that bgfx expects and re-pack if the D3D8 locked pitch differs
    // (drivers may add row padding for alignment). bgfx asserts that the
    // memory size equals its internal storageSize, so we must match exactly.
    const bool isCompressed =
        bgfxFmt == bgfx::TextureFormat::BC1 ||
        bgfxFmt == bgfx::TextureFormat::BC2 ||
        bgfxFmt == bgfx::TextureFormat::BC3;

    unsigned expectedPitch = 0;
    unsigned numRows = 0;
    if (isCompressed)
    {
        const unsigned blockWidth = 4;
        const unsigned blockHeight = 4;
        const unsigned blockSize = (bgfxFmt == bgfx::TextureFormat::BC1) ? 8 : 16;
        expectedPitch = ((desc.Width + blockWidth - 1) / blockWidth) * blockSize;
        numRows = (desc.Height + blockHeight - 1) / blockHeight;
    }
    else
    {
        unsigned bpp = 0;
        switch (bgfxFmt)
        {
            case bgfx::TextureFormat::BGRA8: bpp = 32; break;
            case bgfx::TextureFormat::R5G6B5: bpp = 16; break;
            case bgfx::TextureFormat::BGR5A1: bpp = 16; break;
            case bgfx::TextureFormat::BGRA4: bpp = 16; break;
            case bgfx::TextureFormat::A8: bpp = 8; break;
            case bgfx::TextureFormat::R8: bpp = 8; break;
            default: bpp = 32; break;
        }
        expectedPitch = desc.Width * bpp / 8;
        numRows = desc.Height;
    }

    const unsigned totalBytes = numRows * expectedPitch;
    const unsigned srcPitch = static_cast<unsigned>(locked.Pitch);

    const bgfx::Memory * mem = bgfx::alloc(totalBytes);
    if (srcPitch == expectedPitch)
    {
        std::memcpy(mem->data, locked.pBits, totalBytes);
    }
    else
    {
        const unsigned copyPitch = (srcPitch < expectedPitch) ? srcPitch : expectedPitch;
        const uint8_t * src = static_cast<const uint8_t *>(locked.pBits);
        uint8_t * dst = mem->data;
        for (unsigned row = 0; row < numRows; ++row)
        {
            std::memcpy(dst, src, copyPitch);
            if (copyPitch < expectedPitch)
            {
                std::memset(dst + copyPitch, 0, expectedPitch - copyPitch);
            }
            src += srcPitch;
            dst += expectedPitch;
        }
    }
    d3dTex->UnlockRect(0);

    {
        static int s_texLog = 0;
        if (s_texLog++ < 200)
        {
            TextureClass * t = tex->As_TextureClass();
            WWDEBUG_SAY(("[TEX CREATE #%d] '%s' %ux%u d3dfmt=%d bgfxFmt=%d "
                "srcPitch=%u expPitch=%u rows=%u bytes=%u comp=%d",
                s_texLog,
                t ? (const char*)t->Get_Full_Path() : "?",
                desc.Width, desc.Height,
                static_cast<int>(desc.Format),
                static_cast<int>(bgfxFmt),
                srcPitch, expectedPitch, numRows, totalBytes,
                isCompressed ? 1 : 0));
        }
    }

    bgfx::TextureHandle h = bgfx::createTexture2D(
        static_cast<uint16_t>(desc.Width),
        static_cast<uint16_t>(desc.Height),
        false, 1,
        bgfxFmt,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC,
        mem);

    g_textureCache[tex] = h;
    return h;
}

bgfx::ProgramHandle g_currentBgfxProgram = BGFX_INVALID_HANDLE;
uint64_t            g_currentBgfxState   = 0;

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I.2 CSM light
// transform computation. Called once per frame to build an ortho
// projection from a hardcoded sun direction, fitted to a generous
// world-space box centered on the tactical view. Stores results in
// g_shadowLightView / g_shadowLightProj (bgfx column-major), then
// pushes them to the shadow map view. Not yet tied to the engine's
// actual sun direction — Phase 4I.2 Session D hooks that up.
static void UpdateShadowLightTransform()
{
    // The engine's shadow sun position (from W3DShadowManager) is
    // normalize(-terrainLightPos) * SUN_DISTANCE_FROM_GROUND (10000).
    // Extract the DIRECTION by normalizing, then place the light eye
    // at a reasonable distance along that direction from the look-at.
    // Read the sun direction from the D3D device's light 0 each frame.
    // This ensures the CSM matches the scene lighting even when
    // Set_Shadow_Light_Position isn't called (e.g., save game loads
    // that restore TOD without going through W3DTerrainLogic::loadMap).
    // The D3D light direction points FROM sun TOWARD ground; negate
    // to get the direction TOWARD the sun (matching the convention
    // used by g_shadowSunPosX/Y/Z from setLightPosition).
    float sunX = g_shadowSunPosX;
    float sunY = g_shadowSunPosY;
    float sunZ = g_shadowSunPosZ;
    {
        RenderStateStruct rs;
        DX8Wrapper::Get_Render_State(rs);
        if (rs.LightEnable[0])
        {
            const float lx = -rs.Lights[0].Direction.x;
            const float ly = -rs.Lights[0].Direction.y;
            const float lz = -rs.Lights[0].Direction.z;
            const float ll = std::sqrt(lx*lx + ly*ly + lz*lz);
            if (ll > 0.001f)
            {
                sunX = lx * (10000.0f / ll);
                sunY = ly * (10000.0f / ll);
                sunZ = lz * (10000.0f / ll);
            }
        }
    }
    if (sunX == 0.0f && sunY == 0.0f && sunZ == 0.0f)
    {
        sunX = 500.0f; sunY = 800.0f; sunZ = 1500.0f;
    }
    const float sunLen = std::sqrt(sunX*sunX + sunY*sunY + sunZ*sunZ);
    if (sunLen < 0.001f) return;
    // Normalized direction FROM origin TOWARD sun.
    const float sdx = sunX / sunLen;
    const float sdy = sunY / sunLen;
    const float sdz = sunZ / sunLen;

    // Extract camera position from the captured view matrix.
    float viewTx = g_bgfxView[12];
    float viewTy = g_bgfxView[13];
    float viewTz = g_bgfxView[14];
    float camPosX = -(g_bgfxView[0]*viewTx + g_bgfxView[1]*viewTy + g_bgfxView[2]*viewTz);
    float camPosY = -(g_bgfxView[4]*viewTx + g_bgfxView[5]*viewTy + g_bgfxView[6]*viewTz);
    float camPosZ = -(g_bgfxView[8]*viewTx + g_bgfxView[9]*viewTy + g_bgfxView[10]*viewTz);

    // Compute where the camera looks at on the ground plane (Z=0).
    // The 3rd row of the bgfx view matrix (column-major: indices
    // 2, 6, 10) is the view-space Z axis in world coordinates.
    // For this engine (Z-up, DX8 left-handed), it points BACKWARD
    // (away from the scene), so negate to get FORWARD (toward scene).
    float fwdX = -g_bgfxView[2];
    float fwdY = -g_bgfxView[6];
    float fwdZ = -g_bgfxView[10];
    // Ray-plane intersection: t = -camPosZ / fwdZ
    float atX = camPosX;
    float atY = camPosY;
    const float atZ = 0.0f;
    if (std::fabs(fwdZ) > 0.001f)
    {
        float t = -camPosZ / fwdZ;
        if (t > 0.0f && t < 10000.0f)
        {
            atX = camPosX + fwdX * t;
            atY = camPosY + fwdY * t;
        }
    }

    // Place light eye along the sun direction from the look-at point.
    const float eyeX = atX + sdx * kShadowCameraDistance;
    const float eyeY = atY + sdy * kShadowCameraDistance;
    const float eyeZ = atZ + sdz * kShadowCameraDistance;
    const float upX = 0.0f, upY = 0.0f, upZ = 1.0f;

    // Explicit left-handed to match bx::mtxOrtho's D3D convention
    // (positive Z = forward, near/far > 0). Right-handed (bx default)
    // flips Z, putting the scene at negative view-space Z which falls
    // outside the [near, far] clip range → refZ ≈ 1.0 for everything.
    bx::mtxLookAt(g_shadowLightView,
                  bx::Vec3(eyeX, eyeY, eyeZ),
                  bx::Vec3(atX, atY, atZ),
                  bx::Vec3(upX, upY, upZ),
                  bx::Handedness::Left);

    // Ortho in light-space coordinates. The visible scene spans wider
    // in light-space than camera-space due to the oblique angle.
    // 1200 units with 2048 resolution = ~1.2 units/texel.
    const float orthoNear  = 1.0f;
    const float orthoFar   = kShadowCameraDistance + kShadowOrthoFarMargin;
    const bgfx::Caps * caps = bgfx::getCaps();
    bx::mtxOrtho(g_shadowLightProj,
                 -kShadowOrthoSize, +kShadowOrthoSize,
                 -kShadowOrthoSize, +kShadowOrthoSize,
                 orthoNear, orthoFar,
                 0.0f,
                 caps ? caps->homogeneousDepth : false);

    // Shadow map stabilization: snap the combined view-projection to
    // texel boundaries so the shadow map doesn't sub-pixel-shift as the
    // camera pans (causes pixelated wobbling on shadow edges).
    // Approach: transform the world origin through view*proj to get its
    // shadow-map texel position, round to the nearest integer texel, and
    // apply the fractional offset back into the projection matrix.
    {
        float lightVP[16];
        bx::mtxMul(lightVP, g_shadowLightView, g_shadowLightProj);
        // Row-vector: (0,0,0,1) * VP = (VP[12], VP[13], VP[14], VP[15])
        // VP[12..13] are the clip-space position of the world origin.
        const float halfRes = static_cast<float>(kShadowMapResolution) * 0.5f;
        const float texelX = lightVP[12] * halfRes;
        const float texelY = lightVP[13] * halfRes;
        const float snappedX = std::round(texelX);
        const float snappedY = std::round(texelY);
        const float offsetX = (snappedX - texelX) / halfRes;
        const float offsetY = (snappedY - texelY) / halfRes;
        g_shadowLightProj[12] += offsetX;
        g_shadowLightProj[13] += offsetY;
    }

    bgfx::setViewTransform(kBgfxShadowMapView, g_shadowLightView, g_shadowLightProj);
    g_shadowLightCaptured = true;
}

}

void BgfxBackend::Initialize(void * hwnd, int /*width*/, int /*height*/)
{
    if (g_bgfxInitialized)
    {
        WWDEBUG_SAY(("[BgfxBackend] Initialize called twice; ignoring."));
        return;
    }

    // TheSuperHackers @feature bobtista 16/04/2026 Phase 4K. bgfx takes the
    // main game window; DX8 moves to a secondary popup for reference.
    g_bgfxWindow = static_cast<HWND>(hwnd);
    if (g_bgfxWindow == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] hwnd is null. Backend will remain dormant."));
        return;
    }

    RECT clientRect;
    if (GetClientRect(g_bgfxWindow, &clientRect))
    {
        g_bgfxWidth  = clientRect.right  - clientRect.left;
        g_bgfxHeight = clientRect.bottom - clientRect.top;
    }
    if (g_bgfxWidth <= 0)
    {
        g_bgfxWidth = 800;
    }
    if (g_bgfxHeight <= 0)
    {
        g_bgfxHeight = 600;
    }
    WWDEBUG_SAY(("[BgfxBackend] Using main game window %p (%dx%d) for bgfx.",
                 g_bgfxWindow, g_bgfxWidth, g_bgfxHeight));

    bgfx::renderFrame();

    bgfx::PlatformData pd;
    pd.ndt = nullptr;
    pd.nwh = g_bgfxWindow;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    bgfx::setPlatformData(pd);

    bgfx::Init initArgs;
    initArgs.type = bgfx::RendererType::Count;
    initArgs.callback = &g_bgfxCallback;
    initArgs.resolution.width = static_cast<uint32_t>(g_bgfxWidth);
    initArgs.resolution.height = static_cast<uint32_t>(g_bgfxHeight);
    initArgs.resolution.reset = BGFX_RESET_NONE;
    initArgs.platformData = pd;

    if (!bgfx::init(initArgs))
    {
        WWDEBUG_SAY(("[BgfxBackend] bgfx::init FAILED. Backend will remain dormant."));
        g_bgfxWindow = nullptr;
        return;
    }

    g_bgfxInitialized = true;

    // TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. The explicit
    // bgfx::reset() after init is removed because it triggers a DXGI
    // assertion when bgfx owns the main game HWND. The init call already
    // configured the resolution and format correctly.

    // Configure view 0 to clear the debug window to a dark teal so it's
    // visually obvious bgfx is running and alive. View 0 holds the test
    // triangle (Phase 4B sentinel).
    bgfx::setViewClear(kBgfxDebugView,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x1a3b5cff,  // dark teal, 0xRRGGBBAA
                       1.0f,
                       0);
    bgfx::setViewRect(kBgfxDebugView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));

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
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.12 sorted
    // draws view. No clear (reuses view 1's color + depth so sorted
    // particles z-test correctly against opaque geometry), same rect.
    // View matrix is permanently identity; projection tracks view 1's
    // via Set_Projection_Transform_With_Z_Bias.
    bgfx::setViewClear(kBgfxEngineSortView,
                       BGFX_CLEAR_NONE,
                       0x00000000,
                       1.0f,
                       0);
    bgfx::setViewRect(kBgfxEngineSortView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));

    // Effect overlay view for dazzle draws with NDC-space vertices.
    // Permanent identity view + identity projection; reuses the
    // backbuffer + depth from earlier views. No clear.
    bgfx::setViewClear(kBgfxEffectOverlayView,
                       BGFX_CLEAR_NONE,
                       0x00000000,
                       1.0f,
                       0);
    bgfx::setViewRect(kBgfxEffectOverlayView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxEffectOverlayView, identityMtx, identityMtx);
    }

    // Phase 4I shadow-volume view. Sequential so the two-pass algorithm
    // (front INCR / back DECR) runs in submit order. No clear. View
    // transform is pushed per-frame from the engine camera via the
    // dirty-flag logic alongside view 1.
    bgfx::setViewClear(kBgfxShadowVolumeView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxShadowVolumeView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));
    bgfx::setViewMode(kBgfxShadowVolumeView, bgfx::ViewMode::Sequential);

    // Phase 4I shadow darken apply pass. Sequential, identity transforms
    // (the fullscreen quad is authored in clip space).
    bgfx::setViewClear(kBgfxShadowApplyView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxShadowApplyView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));
    bgfx::setViewMode(kBgfxShadowApplyView, bgfx::ViewMode::Sequential);
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxShadowApplyView, identityMtx, identityMtx);
    }

    // Phase 4L UI overlay view. Sequential mode preserves draw order for
    // 2D quads; identity view+projection; no clear so it composites over
    // the 3D scene.
    bgfx::setViewClear(kBgfxUIView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxUIView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));
    bgfx::setViewMode(kBgfxUIView, bgfx::ViewMode::Sequential);
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxUIView, identityMtx, identityMtx);
    }

    // Default the cached transforms to identity until the engine writes
    // real values via Set_Transform. This keeps the first few engine
    // submits well-defined even if they fire before any matrices are
    // captured.
    IdentityMatrix(g_bgfxWorld);
    IdentityMatrix(g_bgfxView);
    IdentityMatrix(g_bgfxProj);
    IdentityMatrix(g_bgfxSortWorld);
    g_bgfxViewProjDirty = true;

    // Sort view gets identity view + current projection. setViewTransform
    // persists for the life of the bgfx view; we re-apply the projection
    // in Set_Projection_Transform_With_Z_Bias whenever it changes.
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_bgfxProj);
    }

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.3 create the
    // passthrough shader program and vertex layout so End_Scene can submit
    // a test triangle. If shader creation fails the backend still runs but
    // the triangle is skipped.
    g_triangleLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    BuildStandardVertexLayouts();

    const bgfx::Memory * vsMem = bgfx::makeRef(vs_passthrough_dx11,
                                               sizeof(vs_passthrough_dx11));
    const bgfx::Memory * fsMem = bgfx::makeRef(fs_passthrough_dx11,
                                               sizeof(fs_passthrough_dx11));
    bgfx::ShaderHandle vsHandle = bgfx::createShader(vsMem);
    bgfx::ShaderHandle fsHandle = bgfx::createShader(fsMem);
    if (bgfx::isValid(vsHandle) && bgfx::isValid(fsHandle))
    {
        bgfx::setName(vsHandle, "vs_passthrough");
        bgfx::setName(fsHandle, "fs_passthrough");
        g_passthroughProgram = bgfx::createProgram(vsHandle, fsHandle, true);
    }
    else
    {
        if (bgfx::isValid(vsHandle)) bgfx::destroy(vsHandle);
        if (bgfx::isValid(fsHandle)) bgfx::destroy(fsHandle);
        WWDEBUG_SAY(("[BgfxBackend] passthrough shader createShader FAILED."));
    }

    g_sTex0        = bgfx::createUniform("s_tex0",        bgfx::UniformType::Sampler);
    g_sTex1        = bgfx::createUniform("s_tex1",        bgfx::UniformType::Sampler);
    g_sTex2        = bgfx::createUniform("s_tex2",        bgfx::UniformType::Sampler);
    g_sTex3        = bgfx::createUniform("s_tex3",        bgfx::UniformType::Sampler);
    g_uMatDiffuse  = bgfx::createUniform("u_matDiffuse",  bgfx::UniformType::Vec4);
    g_uAtestParams = bgfx::createUniform("u_atestParams", bgfx::UniformType::Vec4);
    g_uTssOps0     = bgfx::createUniform("u_tssOps0",     bgfx::UniformType::Vec4);
    g_uTssOps1     = bgfx::createUniform("u_tssOps1",     bgfx::UniformType::Vec4);
    g_uLightDirs   = bgfx::createUniform("u_lightDirs",   bgfx::UniformType::Vec4, 4);
    g_uLightColors = bgfx::createUniform("u_lightColors", bgfx::UniformType::Vec4, 4);
    g_uSceneAmbient  = bgfx::createUniform("u_sceneAmbient",   bgfx::UniformType::Vec4);
    g_uLightingEnabled = bgfx::createUniform("u_lightingEnabled", bgfx::UniformType::Vec4);
    g_uTexcoordSelect  = bgfx::createUniform("u_texcoordSelect",  bgfx::UniformType::Vec4);
    g_uShroudParams = bgfx::createUniform("u_shroudParams", bgfx::UniformType::Vec4);

    // Default 1x1 white texture. Used as fallback for missing textures.
    // Multiplying by white is the identity operation.
    static const uint8_t kWhitePixel[4] = { 0xff, 0xff, 0xff, 0xff };
    g_defaultWhiteTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWhitePixel, sizeof(kWhitePixel)));
    // Water fallback for render target textures (water reflections).
    // Semi-opaque dark blue simulates the water surface so the hull below
    // is partially hidden and water ripple particles blend naturally.
    // RGBA: (30, 50, 70, 180) = dark blue-grey, ~70% opaque.
    static const uint8_t kWaterPixel[4] = { 0x1e, 0x32, 0x46, 0xb4 };
    g_defaultTransparentTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWaterPixel, sizeof(kWaterPixel)));

    // Phase 5A uber shader program — single program for all engine draws.
    const bgfx::Memory * vsUberMem = bgfx::makeRef(vs_uber_dx11, sizeof(vs_uber_dx11));
    const bgfx::Memory * fsUberMem = bgfx::makeRef(fs_uber_dx11, sizeof(fs_uber_dx11));
    bgfx::ShaderHandle vsUber = bgfx::createShader(vsUberMem);
    bgfx::ShaderHandle fsUber = bgfx::createShader(fsUberMem);
    if (bgfx::isValid(vsUber) && bgfx::isValid(fsUber))
    {
        bgfx::setName(vsUber, "vs_uber");
        bgfx::setName(fsUber, "fs_uber");
        g_uberProgram = bgfx::createProgram(vsUber, fsUber, true);
    }
    else
    {
        if (bgfx::isValid(vsUber)) bgfx::destroy(vsUber);
        if (bgfx::isValid(fsUber)) bgfx::destroy(fsUber);
        WWDEBUG_SAY(("[BgfxBackend] uber shader createShader FAILED."));
    }

    // Phase 4H tree/grass sway vertex shader program. Reuses fs_uber.
    const bgfx::Memory * vsTreesMem = bgfx::makeRef(vs_trees_dx11, sizeof(vs_trees_dx11));
    const bgfx::Memory * fsUberMem2 = bgfx::makeRef(fs_uber_dx11, sizeof(fs_uber_dx11));
    bgfx::ShaderHandle vsTrees = bgfx::createShader(vsTreesMem);
    bgfx::ShaderHandle fsUber2 = bgfx::createShader(fsUberMem2);
    if (bgfx::isValid(vsTrees) && bgfx::isValid(fsUber2))
    {
        bgfx::setName(vsTrees, "vs_trees");
        g_treeProgram = bgfx::createProgram(vsTrees, fsUber2, true);
    }
    else
    {
        if (bgfx::isValid(vsTrees)) bgfx::destroy(vsTrees);
        if (bgfx::isValid(fsUber2)) bgfx::destroy(fsUber2);
        WWDEBUG_SAY(("[BgfxBackend] tree shader createShader FAILED."));
    }
    // Phase 4I stencil shadow volume program. XYZ-only verts, no color output.
    const bgfx::Memory * vsShadowMem = bgfx::makeRef(vs_shadow_volume_dx11, sizeof(vs_shadow_volume_dx11));
    const bgfx::Memory * fsShadowMem = bgfx::makeRef(fs_shadow_volume_dx11, sizeof(fs_shadow_volume_dx11));
    bgfx::ShaderHandle vsShadow = bgfx::createShader(vsShadowMem);
    bgfx::ShaderHandle fsShadow = bgfx::createShader(fsShadowMem);
    if (bgfx::isValid(vsShadow) && bgfx::isValid(fsShadow))
    {
        bgfx::setName(vsShadow, "vs_shadow_volume");
        bgfx::setName(fsShadow, "fs_shadow_volume");
        g_shadowVolumeProgram = bgfx::createProgram(vsShadow, fsShadow, true);
    }
    else
    {
        if (bgfx::isValid(vsShadow)) bgfx::destroy(vsShadow);
        if (bgfx::isValid(fsShadow)) bgfx::destroy(fsShadow);
        WWDEBUG_SAY(("[BgfxBackend] shadow volume shader createShader FAILED."));
    }

    // Phase 4I shadow darken apply program. Fullscreen clip-space quad.
    const bgfx::Memory * vsApplyMem = bgfx::makeRef(vs_shadow_apply_dx11, sizeof(vs_shadow_apply_dx11));
    const bgfx::Memory * fsApplyMem = bgfx::makeRef(fs_shadow_apply_dx11, sizeof(fs_shadow_apply_dx11));
    bgfx::ShaderHandle vsApply = bgfx::createShader(vsApplyMem);
    bgfx::ShaderHandle fsApply = bgfx::createShader(fsApplyMem);
    if (bgfx::isValid(vsApply) && bgfx::isValid(fsApply))
    {
        bgfx::setName(vsApply, "vs_shadow_apply");
        bgfx::setName(fsApply, "fs_shadow_apply");
        g_shadowApplyProgram = bgfx::createProgram(vsApply, fsApply, true);
    }
    else
    {
        if (bgfx::isValid(vsApply)) bgfx::destroy(vsApply);
        if (bgfx::isValid(fsApply)) bgfx::destroy(fsApply);
        WWDEBUG_SAY(("[BgfxBackend] shadow apply shader createShader FAILED."));
    }
    g_uShadowColor = bgfx::createUniform("u_shadowColor", bgfx::UniformType::Vec4);
    g_uShadowBias  = bgfx::createUniform("u_shadowBias",  bgfx::UniformType::Vec4);

    // Phase 4I.2 CSM caster program + shadow map render target.
    const bgfx::Memory * vsCasterMem = bgfx::makeRef(vs_shadow_caster_dx11, sizeof(vs_shadow_caster_dx11));
    const bgfx::Memory * fsCasterMem = bgfx::makeRef(fs_shadow_caster_dx11, sizeof(fs_shadow_caster_dx11));
    bgfx::ShaderHandle vsCaster = bgfx::createShader(vsCasterMem);
    bgfx::ShaderHandle fsCaster = bgfx::createShader(fsCasterMem);
    if (bgfx::isValid(vsCaster) && bgfx::isValid(fsCaster))
    {
        bgfx::setName(vsCaster, "vs_shadow_caster");
        bgfx::setName(fsCaster, "fs_shadow_caster");
        g_shadowCasterProgram = bgfx::createProgram(vsCaster, fsCaster, true);
    }
    else
    {
        if (bgfx::isValid(vsCaster)) bgfx::destroy(vsCaster);
        if (bgfx::isValid(fsCaster)) bgfx::destroy(fsCaster);
        WWDEBUG_SAY(("[BgfxBackend] CSM caster shader createShader FAILED."));
    }

    // Shadow map depth render target. 1024x1024 D24 with compare-less
    // sampling for hardware PCF (sampler2DShadow in fs_uber).
    // Shadow map: D32F depth texture with comparison sampler for
    // hardware PCF (matches bgfx example 16). D32F gives full float
    // precision, eliminating shadow acne from depth quantization.
    const uint64_t depthFlags = BGFX_TEXTURE_RT
        | BGFX_SAMPLER_COMPARE_LEQUAL
        | BGFX_SAMPLER_U_CLAMP
        | BGFX_SAMPLER_V_CLAMP;
    g_shadowMapDepth = bgfx::createTexture2D(
        kShadowMapResolution, kShadowMapResolution, false, 1,
        bgfx::TextureFormat::D32F, depthFlags);
    if (bgfx::isValid(g_shadowMapDepth))
    {
        bgfx::setName(g_shadowMapDepth, "shadowMapD32F");
        g_shadowMapFB = bgfx::createFrameBuffer(1, &g_shadowMapDepth, true);
        // Clear both color (R32F=1.0 = far plane) and depth for the
        // caster pass's own depth test. 0x3f800000 = float 1.0 as RGBA.
        bgfx::setViewClear(kBgfxShadowMapView,
                           BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           0xffffffffu,  // RGBA all bits set ≈ max depth
                           1.0f, 0);
        bgfx::setViewRect(kBgfxShadowMapView, 0, 0, kShadowMapResolution, kShadowMapResolution);
        bgfx::setViewFrameBuffer(kBgfxShadowMapView, g_shadowMapFB);
        bgfx::setViewMode(kBgfxShadowMapView, bgfx::ViewMode::Sequential);
        WWDEBUG_SAY(("[CSM] shadow map FB=%u tex=%u valid=%d",
                     g_shadowMapFB.idx, g_shadowMapDepth.idx,
                     bgfx::isValid(g_shadowMapFB) ? 1 : 0));

        // Phase 4I.2 view ORDER: shadow map (view 8) MUST render before
        // the engine view (view 1) samples it in fs_uber. By default
        // bgfx renders views in view-id order (0, 1, 2, ...) — view 8
        // would run AFTER view 1, and the scene would sample empty /
        // stale shadow map. Explicit order: shadow map first, then the
        // rest of the views in their natural ascending order.
        const bgfx::ViewId order[] = {
            kBgfxShadowMapView,       // 8 — shadow caster depth pass
            kBgfxDebugView,            // test/passthrough
            kBgfxRTTView,             // 3
            kBgfxEngineView,          // 1
            kBgfxShadowVolumeView,    // 6 — stencil shadow volume fill
            kBgfxShadowApplyView,     // 7 — stencil shadow darken
            kBgfxWaterView,           // 4
            kBgfxEngineSortView,      // 2
            kBgfxEffectOverlayView,   // 5
            kBgfxUIView,              // 10 — 2D UI overlay (last)
        };
        bgfx::setViewOrder(kBgfxDebugView, BX_COUNTOF(order), order);
        WWDEBUG_SAY(("[CSM] view order set: shadow map (view %u) runs first",
                     kBgfxShadowMapView));
    }
    else
    {
        WWDEBUG_SAY(("[BgfxBackend] CSM shadow map texture creation FAILED."));
    }

    g_uShadowLightViewProj = bgfx::createUniform("u_shadowLightViewProj",
                                                 bgfx::UniformType::Mat4);
    g_sShadowMap           = bgfx::createUniform("s_shadowMap",
                                                 bgfx::UniformType::Sampler);

    g_uSwayTable    = bgfx::createUniform("u_swayTable",    bgfx::UniformType::Vec4, kSwayTableEntries);
    g_uShroudOffset = bgfx::createUniform("u_shroudOffset", bgfx::UniformType::Vec4);
    g_uShroudScale  = bgfx::createUniform("u_shroudScale",  bgfx::UniformType::Vec4);

    const bgfx::RendererType::Enum selected = bgfx::getRendererType();
    const char * rendererName = bgfx::getRendererName(selected);
    const bgfx::Caps * caps = bgfx::getCaps();
    WWDEBUG_SAY(("[BgfxBackend] bgfx::init OK on main window "
                 "(renderer=%s, %dx%d, hwnd=%p, passthrough=%s, uber=%s).",
                 rendererName, g_bgfxWidth, g_bgfxHeight,
                 g_bgfxWindow,
                 bgfx::isValid(g_passthroughProgram) ? "ok" : "FAILED",
                 bgfx::isValid(g_uberProgram)        ? "ok" : "FAILED"));
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

    // TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. DX8 now creates
    // its own secondary reference window in DX8Wrapper::Init, so no need to
    // create or move anything here.
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
    if (g_bgfxInitialized)
    {
        DestroyBgfxHandle(g_passthroughProgram);
        DestroyBgfxHandle(g_uberProgram);
        DestroyBgfxHandle(g_treeProgram);
        DestroyBgfxHandle(g_uSwayTable);
        DestroyBgfxHandle(g_uShroudOffset);
        DestroyBgfxHandle(g_uShroudScale);
        DestroyBgfxHandle(g_sTex0);
        DestroyBgfxHandle(g_sTex1);
        DestroyBgfxHandle(g_sTex2);
        DestroyBgfxHandle(g_sTex3);
        DestroyBgfxHandle(g_uMatDiffuse);
        DestroyBgfxHandle(g_uAtestParams);
        DestroyBgfxHandle(g_uTssOps0);
        DestroyBgfxHandle(g_uTssOps1);
        DestroyBgfxHandle(g_uLightDirs);
        DestroyBgfxHandle(g_uLightColors);
        DestroyBgfxHandle(g_uSceneAmbient);
        DestroyBgfxHandle(g_uLightingEnabled);
        DestroyBgfxHandle(g_uTexcoordSelect);
        DestroyBgfxHandle(g_uShroudParams);
        DestroyBgfxHandle(g_shadowVolumeProgram);
        DestroyBgfxHandle(g_shadowApplyProgram);
        DestroyBgfxHandle(g_shadowCasterProgram);
        DestroyBgfxHandle(g_shadowMapFB);
        DestroyBgfxHandle(g_shadowMapDepth);
        DestroyBgfxHandle(g_uShadowColor);
        DestroyBgfxHandle(g_uShadowBias);
        DestroyBgfxHandle(g_uShadowLightViewProj);
        DestroyBgfxHandle(g_sShadowMap);
        DestroyBgfxHandle(g_defaultWhiteTexture);
        DestroyBgfxHandle(g_defaultTransparentTexture);
        g_renderTargetSet.clear();
        for (auto & kv : g_framebufferCache)
        {
            if (bgfx::isValid(kv.second.fb))
            {
                bgfx::destroy(kv.second.fb);
            }
        }
        g_framebufferCache.clear();
        // Phase 4C.3 cached engine buffers. Destroy before bgfx::shutdown
        // so the handles outlive nothing.
        for (auto & kv : g_vbCache)
        {
            if (bgfx::isValid(kv.second.handle))
            {
                bgfx::destroy(kv.second.handle);
            }
        }
        g_vbCache.clear();
        for (auto & kv : g_ibCache)
        {
            if (bgfx::isValid(kv.second.handle))
            {
                bgfx::destroy(kv.second.handle);
            }
        }
        g_ibCache.clear();
        for (auto & kv : g_textureCache)
        {
            if (bgfx::isValid(kv.second))
            {
                bgfx::destroy(kv.second);
            }
        }
        g_textureCache.clear();
        g_currentBgfxVB         = BGFX_INVALID_HANDLE;
        g_currentBgfxIB         = BGFX_INVALID_HANDLE;
        g_currentBgfxTexture0   = BGFX_INVALID_HANDLE;
        g_currentBgfxTexture1   = BGFX_INVALID_HANDLE;
        g_currentBgfxTexture2   = BGFX_INVALID_HANDLE;
        g_currentBgfxTexture3   = BGFX_INVALID_HANDLE;
        g_currentIBOffset       = 0;
        g_currentUseTransientVB = false;
        g_currentUseTransientIB = false;
        g_pendingDynVB.valid    = false;
        g_pendingDynIB.valid    = false;
        bgfx::shutdown();
        g_bgfxInitialized = false;
        WWDEBUG_SAY(("[BgfxBackend] bgfx::shutdown complete."));
    }

    // Phase 4K: bgfx window is the main game window, do not destroy it.
    // DX8's secondary reference window is owned by DX8Wrapper.
    g_bgfxWindow = nullptr;
}

// Device state queries (Is_Device_Lost / Has_Stencil / Get_Back_Buffer_Format
// / Get_Back_Buffer / Set_Gamma) are inherited from DX8Backend.

// -- Frame lifecycle ---------------------------------------------------------

void BgfxBackend::Begin_Scene()
{
    if (!g_bgfxInitialized)
    {
        return;
    }
    // Show the DX8 reference popup after a few frames, giving the game's
    // input system time to fully initialize. Showing too early steals focus
    // and permanently blocks mouse capture.
    {
        static int s_dx8RefFrameCount = 0;
        if (s_dx8RefFrameCount >= 0)
        {
            s_dx8RefFrameCount++;
            if (s_dx8RefFrameCount > 30)
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
                if (g_bgfxWindow)
                {
                    SetForegroundWindow(g_bgfxWindow);
                    SetFocus(g_bgfxWindow);
                }
            }
        }
    }

    // Check if the game window was resized (e.g., by Set_Render_Device) and
    // update bgfx's swapchain to match. Without this, bgfx renders at the
    // old resolution while the game expects the new one.
    if (g_bgfxWindow)
    {
        RECT cr;
        if (GetClientRect(g_bgfxWindow, &cr))
        {
            int w = cr.right - cr.left;
            int h = cr.bottom - cr.top;
            if (w > 0 && h > 0 && (w != g_bgfxWidth || h != g_bgfxHeight))
            {
                WWDEBUG_SAY(("[BgfxBackend] Window resized %dx%d -> %dx%d, calling bgfx::reset.",
                             g_bgfxWidth, g_bgfxHeight, w, h));
                g_bgfxWidth = w;
                g_bgfxHeight = h;
                bgfx::reset(g_bgfxWidth, g_bgfxHeight, BGFX_RESET_NONE);
            }
        }
    }

    bgfx::touch(kBgfxDebugView);
    bgfx::touch(kBgfxEngineView);
    bgfx::touch(kBgfxEngineSortView);
    bgfx::touch(kBgfxWaterView);
    bgfx::touch(kBgfxEffectOverlayView);
    bgfx::touch(kBgfxShadowVolumeView);
    bgfx::touch(kBgfxShadowApplyView);
    bgfx::touch(kBgfxShadowMapView);
    bgfx::touch(kBgfxUIView);
    // Phase 4I.2 CSM: light transform updated lazily at first draw
    // (see SubmitEngineDraw), not here, because g_bgfxView is still
    // identity at Begin_Scene time.
    g_shadowLightCaptured = false;
    g_2DOverlayActive = false;
    bgfx::setViewRect(kBgfxWaterView, 0, 0, g_bgfxWidth, g_bgfxHeight);
    // No clear on water view — it composites over the opaque scene.
    bgfx::setViewClear(kBgfxWaterView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    g_renderToTexture = false;
}

void BgfxBackend::End_Scene(bool /*flip_frame*/)
{
    if (!g_bgfxInitialized)
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
    if (g_bgfxCameraCaptured)
    {
        bgfx::setViewTransform(kBgfxEngineView, g_bgfxCameraView, g_bgfxCameraProj);
        bgfx::setViewTransform(kBgfxWaterView, g_bgfxCameraView, g_bgfxCameraProj);
        bgfx::setViewTransform(kBgfxShadowVolumeView, g_bgfxCameraView, g_bgfxCameraProj);
        g_bgfxCameraCaptured = false;
    }
    if (g_bgfxSortProjCaptured)
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_bgfxSortProj);
        g_bgfxSortProjCaptured = false;
    }

    // Phase 4L: push identity transforms and current rect to the UI view
    // so 2D overlay draws land in screen space over the 3D scene.
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxUIView, identityMtx, identityMtx);
    }
    bgfx::setViewRect(kBgfxUIView, 0, 0,
                      static_cast<uint16_t>(g_bgfxWidth),
                      static_cast<uint16_t>(g_bgfxHeight));

    // Phase 4I.2: shadow map (view 8) renders FIRST so the depth
    // texture is populated before the scene samples it. Then RTT (3),
    // engine opaque (1), shadow volume fill (6), shadow darken (7),
    // water (4), sort (2), effect overlay (5), UI overlay (10) last.
    bgfx::ViewId viewOrder[] = {
        kBgfxShadowMapView,        // 8 — shadow caster depth pass
        kBgfxRTTView,              // 3
        kBgfxEngineView,           // 1
        kBgfxShadowVolumeView,     // 6 — stencil shadow volume fill
        kBgfxShadowApplyView,      // 7 — stencil shadow darken
        kBgfxWaterView,            // 4
        kBgfxEngineSortView,       // 2
        kBgfxEffectOverlayView,    // 5
        kBgfxUIView,               // 10 — 2D UI overlay (last)
    };
    bgfx::setViewOrder(kBgfxDebugView, BX_COUNTOF(viewOrder), viewOrder);

    bgfx::frame();

    // Transient buffers are freed at bgfx::frame time. Invalidate the
    // pending and current slots so nothing next frame tries to reuse
    // a dead handle.
    g_pendingDynVB.valid    = false;
    g_pendingDynIB.valid    = false;
    g_currentUseTransientVB = false;
    g_currentUseTransientIB = false;
}

// Flip_To_Primary, Clear, Set_Viewport are inherited from DX8Backend.

// -- Vertex / index buffers --------------------------------------------------

void BgfxBackend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    DX8Backend::Set_Vertex_Buffer(vb, stream);

    // Phase 4C.4: cache is populated by Capture_Vertex_Data on the engine's
    // own write lock. Set_Vertex_Buffer just looks up whatever is already
    // there. Engine VBs that have not been written via the WriteLockClass
    // path yet (e.g. those filled by raw d3d8 calls) will miss the cache
    // and the bgfx submit will be skipped.
    g_currentUseTransientVB = false;
    auto it = g_vbCache.find(vb);
    if (it != g_vbCache.end())
    {
        g_currentBgfxVB = it->second.handle;
    }
    else
    {
        g_currentBgfxVB = BGFX_INVALID_HANDLE;
        // On-demand capture for static VBs that were never captured
        // via WriteLockClass (e.g. rotor meshes loaded at startup).
        // Lock the D3D VB, copy the data into a bgfx dynamic VB, and
        // cache it for future use.
        if (vb != nullptr && g_bgfxInitialized
            && (vb->Type() == BUFFER_TYPE_DX8))
        {
            DX8VertexBufferClass * dx8vb =
                static_cast<DX8VertexBufferClass *>(
                    const_cast<VertexBufferClass *>(vb));
            IDirect3DVertexBuffer8 * d3dvb = dx8vb->Get_DX8_Vertex_Buffer();
            if (d3dvb != nullptr)
            {
                const unsigned int stride = vb->FVF_Info().Get_FVF_Size();
                const unsigned int num_verts = vb->Get_Vertex_Count();
                const unsigned int bytes = num_verts * stride;
                BYTE * data = nullptr;
                HRESULT hr = d3dvb->Lock(0, bytes, &data, D3DLOCK_READONLY);
                if (SUCCEEDED(hr) && data != nullptr)
                {
                    Capture_Vertex_Data(vb, data, bytes);
                    d3dvb->Unlock();
                    // Re-lookup after capture
                    auto it2 = g_vbCache.find(vb);
                    if (it2 != g_vbCache.end())
                    {
                        g_currentBgfxVB = it2->second.handle;
                    }
                }
            }
        }
    }
}

void BgfxBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    DX8Backend::Set_Vertex_Buffer(vba);

    // Phase 4G.2: if the matching Capture_Dynamic_Vertex_Data already
    // allocated a transient VB for this access class, claim it for the
    // next draw. Otherwise miss the cache and skip the bgfx submit.
    if (g_pendingDynVB.valid && g_pendingDynVB.owner == &vba)
    {
        g_currentUseTransientVB = true;
        g_currentTransientVB    = g_pendingDynVB.tvb;
        g_pendingDynVB.valid    = false;
    }
    else
    {
        g_currentUseTransientVB = false;
        g_currentBgfxVB         = BGFX_INVALID_HANDLE;
    }
}

void BgfxBackend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(ib, index_base_offset);

    g_currentUseTransientIB = false;
    auto it = g_ibCache.find(ib);
    if (it != g_ibCache.end())
    {
        g_currentBgfxIB = it->second.handle;
    }
    else
    {
        g_currentBgfxIB = BGFX_INVALID_HANDLE;
        // On-demand capture for static IBs not yet in cache.
        if (ib != nullptr && g_bgfxInitialized
            && (ib->Type() == BUFFER_TYPE_DX8))
        {
            DX8IndexBufferClass * dx8ib =
                static_cast<DX8IndexBufferClass *>(
                    const_cast<IndexBufferClass *>(ib));
            IDirect3DIndexBuffer8 * d3dib = dx8ib->Get_DX8_Index_Buffer();
            if (d3dib != nullptr)
            {
                const unsigned int num_indices = ib->Get_Index_Count();
                const unsigned int bytes = num_indices * sizeof(unsigned short);
                BYTE * data = nullptr;
                HRESULT hr = d3dib->Lock(0, bytes, &data, D3DLOCK_READONLY);
                if (SUCCEEDED(hr) && data != nullptr)
                {
                    Capture_Index_Data(ib, data, bytes);
                    d3dib->Unlock();
                    auto it2 = g_ibCache.find(ib);
                    if (it2 != g_ibCache.end())
                    {
                        g_currentBgfxIB = it2->second.handle;
                    }
                }
            }
        }
    }
    g_currentIBOffset = index_base_offset;
}

void BgfxBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(iba, index_base_offset);

    if (g_pendingDynIB.valid && g_pendingDynIB.owner == &iba)
    {
        g_currentUseTransientIB = true;
        g_currentTransientIB    = g_pendingDynIB.tib;
        g_pendingDynIB.valid    = false;
    }
    else
    {
        g_currentUseTransientIB = false;
        g_currentBgfxIB         = BGFX_INVALID_HANDLE;
    }
    g_currentIBOffset = index_base_offset;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.8 override
// Set_Index_Buffer_Index_Offset so we capture the per-mesh base vertex
// offset. DX8PolygonRendererClass::Render calls this once per mesh
// before Draw_Triangles to shift which vertex slot in the shared
// category VB each index resolves to. Without this override the call
// forwards to DX8Backend -> DX8Wrapper and the bgfx path keeps using
// the stale offset from Set_Index_Buffer, so every mesh inside the
// same rigid FVF category would draw using the first mesh's vertex
// slots. Must call the base so the dx8 device still gets the update.
void BgfxBackend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    DX8Backend::Set_Index_Buffer_Index_Offset(offset);
    g_currentIBOffset = static_cast<unsigned short>(offset);
}

// -- Phase 4C.4 write-side capture -------------------------------------------
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
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.6 dynamic buffer
// ensure helpers. Return the cached dynamic VB / IB handle for the given
// engine buffer, creating it sized to the full capacity on first sight.
// Returned handle is guaranteed valid on success; invalid handle on
// failure. Used by both the full-buffer (WriteLockClass) and sub-range
// (AppendLockClass) capture paths.
bgfx::DynamicVertexBufferHandle EnsureDynamicVertexBuffer(const VertexBufferClass * vb)
{
    const uint32_t num_verts = static_cast<uint32_t>(vb->Get_Vertex_Count());
    const uint32_t engine_stride = vb->FVF_Info().Get_FVF_Size();

    auto it = g_vbCache.find(vb);
    if (it != g_vbCache.end())
    {
        // Stale pointer reuse detection: engine can destroy a VB and
        // allocate a new one at the same address with different
        // dimensions. If dimensions changed, drop the stale handle.
        if (it->second.num_verts == num_verts && it->second.stride == engine_stride)
        {
            return it->second.handle;
        }
        if (bgfx::isValid(it->second.handle))
        {
            bgfx::destroy(it->second.handle);
        }
        g_vbCache.erase(it);
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
    // Mismatched stride (typically for skinned-mesh FVFs like
    // D3DFVF_XYZB* that BuildBgfxLayoutForFVF does not fully cover)
    // would create a too-small bgfx buffer and cause truncation
    // warnings + D3D11 CreateBuffer crashes when the engine writes
    // a larger per-vertex stride than bgfx allocated.
    const uint32_t layout_stride = layout.getStride();
    if (layout_stride == 0 || layout_stride != engine_stride)
    {
        WWDEBUG_SAY(("[BgfxBackend] skip VB cache: layout stride=%u != "
                     "engine stride=%u (fvf=0x%x num_verts=%u) - unsupported FVF",
                     layout_stride, engine_stride,
                     vb->FVF_Info().Get_FVF(), num_verts));
        BgfxVbCacheEntry e{ BGFX_INVALID_HANDLE, num_verts, engine_stride };
        g_vbCache[vb] = e;
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicVertexBufferHandle h = bgfx::createDynamicVertexBuffer(num_verts, layout);
    BgfxVbCacheEntry e{ h, num_verts, engine_stride };
    g_vbCache[vb] = e;
    return h;
}

bgfx::DynamicIndexBufferHandle EnsureDynamicIndexBuffer(const IndexBufferClass * ib)
{
    const uint32_t num_indices = static_cast<uint32_t>(ib->Get_Index_Count());

    auto it = g_ibCache.find(ib);
    if (it != g_ibCache.end())
    {
        if (it->second.num_indices == num_indices && bgfx::isValid(it->second.handle))
        {
            return it->second.handle;
        }
        if (bgfx::isValid(it->second.handle))
        {
            bgfx::destroy(it->second.handle);
        }
        g_ibCache.erase(it);
    }
    if (num_indices == 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicIndexBufferHandle h = bgfx::createDynamicIndexBuffer(num_indices);
    BgfxIbCacheEntry e{ h, num_indices };
    g_ibCache[ib] = e;
    return h;
}
}

// TheSuperHackers @feature bobtista 17/04/2026 Shroud texture capture for
// bgfx. The shroud destination texture is POOL_DEFAULT, which the bgfx
// texture-upload path in EnsureBgfxTexture skips (cannot lock). Instead,
// the shroud system pushes its system-memory pixel data here every frame
// after CopyRects. We create a bgfx texture on first call and updateTexture2D
// on subsequent frames, storing the handle in g_textureCache keyed by the
// engine's destination TextureClass so EnsureBgfxTexture finds it on lookup
// before reaching the POOL_DEFAULT early-out.
void BgfxBackend::Capture_Shroud_Texture(TextureClass * dst_texture,
                                          const void * pixel_data,
                                          unsigned dst_width,
                                          unsigned dst_height,
                                          unsigned src_width,
                                          unsigned src_height,
                                          unsigned dst_x,
                                          unsigned dst_y,
                                          unsigned pitch,
                                          WW3DFormat format)
{
    if (!g_bgfxInitialized || dst_texture == nullptr || pixel_data == nullptr)
    {
        return;
    }

    const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(format);
    if (bgfxFmt == bgfx::TextureFormat::Unknown)
    {
        return;
    }

    const unsigned bpp = (bgfxFmt == bgfx::TextureFormat::BGRA4 ||
                          bgfxFmt == bgfx::TextureFormat::R5G6B5) ? 2 : 4;

    // TheSuperHackers @bugfix bobtista 17/04/2026 invalidate stale cache
    // entries when the shroud texture pointer changes (save/load destroys
    // and recreates m_pDstTexture). Without this, the old bgfx handle
    // stays in g_textureCache keyed by the freed pointer, and if the
    // address is reused, updateTexture2D crashes with mismatched dimensions.
    static TextureClass * s_lastShroudDst = nullptr;
    static unsigned s_lastShroudW = 0;
    static unsigned s_lastShroudH = 0;
    if (dst_texture != s_lastShroudDst
        || dst_width != s_lastShroudW
        || dst_height != s_lastShroudH)
    {
        if (s_lastShroudDst != nullptr)
        {
            auto oldIt = g_textureCache.find(s_lastShroudDst);
            if (oldIt != g_textureCache.end())
            {
                if (bgfx::isValid(oldIt->second))
                    bgfx::destroy(oldIt->second);
                g_textureCache.erase(oldIt);
            }
            g_renderTargetSet.erase(s_lastShroudDst);
        }
        s_lastShroudDst = dst_texture;
        s_lastShroudW = dst_width;
        s_lastShroudH = dst_height;
    }

    auto it = g_textureCache.find(dst_texture);
    if (it == g_textureCache.end() || !bgfx::isValid(it->second))
    {
        bgfx::TextureHandle h = bgfx::createTexture2D(
            static_cast<uint16_t>(dst_width),
            static_cast<uint16_t>(dst_height),
            false, 1,
            bgfxFmt,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        g_textureCache[dst_texture] = h;

        WWDEBUG_SAY(("[BgfxBackend] Shroud texture created: dst=%ux%u src=%ux%u "
                     "fmt=%d bgfxFmt=%d bpp=%u off=(%u,%u)",
                     dst_width, dst_height, src_width, src_height,
                     static_cast<int>(format), static_cast<int>(bgfxFmt),
                     bpp, dst_x, dst_y));
    }

    bgfx::TextureHandle h = g_textureCache[dst_texture];
    if (!bgfx::isValid(h))
    {
        return;
    }

    // Build the shroud image into a persistent buffer and use makeRef
    // to avoid copying into bgfx's command buffer (which has a 64KB
    // default limit and overflows when combined with texture creation
    // bursts like the satellite reveal). The persistent buffer stays
    // valid until the next frame's update overwrites it.
    const unsigned fullSize = dst_width * dst_height * bpp;
    if (fullSize == 0 || src_width == 0 || src_height == 0
        || dst_width > 0xFFFF || dst_height > 0xFFFF)
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::alloc(fullSize);
    std::memset(mem->data, 0xFF, fullSize);
    const unsigned rowBytes = src_width * bpp;
    for (unsigned row = 0; row < src_height; ++row)
    {
        const unsigned dstOffset = ((dst_y + row) * dst_width + dst_x) * bpp;
        const unsigned srcOffset = row * pitch;
        if (dstOffset + rowBytes <= fullSize && srcOffset + rowBytes <= src_height * pitch)
        {
            std::memcpy(mem->data + dstOffset, static_cast<const uint8_t *>(pixel_data) + srcOffset, rowBytes);
        }
    }
    bgfx::updateTexture2D(h, 0, 0, 0, 0,
                          static_cast<uint16_t>(dst_width),
                          static_cast<uint16_t>(dst_height),
                          mem, static_cast<uint16_t>(dst_width * bpp));
}

void BgfxBackend::Capture_Vertex_Data(const VertexBufferClass * vb,
                                      const void * data,
                                      unsigned int size_bytes)
{
    if (!g_bgfxInitialized || vb == nullptr || data == nullptr || size_bytes == 0)
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
        WWDEBUG_SAY(("[BgfxBackend] skip VB full-capture: "
                     "size_bytes=%u stride=%u total=%u",
                     size_bytes, stride, buffer_bytes));
        return;
    }

    bgfx::DynamicVertexBufferHandle h = EnsureDynamicVertexBuffer(vb);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::update(h, 0, mem);
}

void BgfxBackend::Capture_Index_Data(const IndexBufferClass * ib,
                                     const void * data,
                                     unsigned int size_bytes)
{
    if (!g_bgfxInitialized || ib == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(ib->Get_Index_Count()) * sizeof(uint16_t);
    if (buffer_bytes == 0 || size_bytes > buffer_bytes)
    {
        WWDEBUG_SAY(("[BgfxBackend] skip IB full-capture: "
                     "size_bytes=%u total=%u", size_bytes, buffer_bytes));
        return;
    }
    bgfx::DynamicIndexBufferHandle h = EnsureDynamicIndexBuffer(ib);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::update(h, 0, mem);
}

void BgfxBackend::Capture_Vertex_Sub_Range(const VertexBufferClass * vb,
                                           const void * data,
                                           unsigned int start_vertex,
                                           unsigned int size_bytes)
{
    if (!g_bgfxInitialized || vb == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    // Guard: total update must fit in the dynamic VB allocation.
    const uint32_t stride = vb->FVF_Info().Get_FVF_Size();
    if (stride == 0)
    {
        WWDEBUG_SAY(("[BgfxBackend] skip VB capture: stride=0 vb=%p", vb));
        return;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(vb->Get_Vertex_Count()) * stride;
    const uint64_t end_byte = static_cast<uint64_t>(start_vertex) * stride + size_bytes;
    if (end_byte > buffer_bytes)
    {
        WWDEBUG_SAY(("[BgfxBackend] skip VB capture: out-of-range "
                     "start_vert=%u size_bytes=%u stride=%u total=%u",
                     start_vertex, size_bytes, stride, buffer_bytes));
        return;
    }
    bgfx::DynamicVertexBufferHandle h = EnsureDynamicVertexBuffer(vb);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::update(h, start_vertex, mem);

}

void BgfxBackend::Capture_Index_Sub_Range(const IndexBufferClass * ib,
                                          const void * data,
                                          unsigned int start_index,
                                          unsigned int size_bytes)
{
    if (!g_bgfxInitialized || ib == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    const uint32_t buffer_bytes = static_cast<uint32_t>(ib->Get_Index_Count()) * sizeof(uint16_t);
    const uint64_t end_byte = static_cast<uint64_t>(start_index) * sizeof(uint16_t) + size_bytes;
    if (end_byte > buffer_bytes)
    {
        WWDEBUG_SAY(("[BgfxBackend] skip IB capture: out-of-range "
                     "start_idx=%u size_bytes=%u total=%u",
                     start_index, size_bytes, buffer_bytes));
        return;
    }
    bgfx::DynamicIndexBufferHandle h = EnsureDynamicIndexBuffer(ib);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::update(h, start_index, mem);

}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.12 sorted
// draw pass routing. The sort flush calls Begin / Capture / End
// around its per-batch draw loop. Begin flips the routing flag so
// SubmitEngineDraw pushes submits into kBgfxEngineSortView using
// g_bgfxSortWorld; End flips it back. Capture computes the per-batch
// effective world matrix and stores it in bgfx column-major form.
//
// Matrix math: W3D Matrix4x4 is row-major with row vectors, so the
// engine pipeline is v' = v * sortWorld * sortView. In column-vector
// form that is (sortView^T * sortWorld^T) * v, i.e. bgfx's world
// matrix must be sortView^T * sortWorld^T. We compute the combined
// row-major product sortWorld * sortView directly into bgfx
// column-major layout (the transpose is baked into the index
// pattern), avoiding an intermediate copy.

void BgfxBackend::Begin_Sorted_Batch_Pass()
{
    g_inSortFlush = true;
    if (!g_bgfxSortProjCaptured)
    {
        std::memcpy(g_bgfxSortProj, g_bgfxProj, sizeof(g_bgfxSortProj));
        g_bgfxSortProjCaptured = true;
    }
}

void BgfxBackend::End_Sorted_Batch_Pass()
{
    g_inSortFlush = false;
}

void BgfxBackend::Capture_Sorted_Batch_Transforms(const Matrix4x4 & sortWorld,
                                                  const Matrix4x4 & sortView)
{
    // Compute the D3D row-major product sortWorld * sortView, then store
    // it as row-major float[16] (r*4+c). bgfx on D3D11 interprets the
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
            g_bgfxSortWorld[r * 4 + c] = s;
            // Phase 4I.2: store raw model (no camera view baked in)
            // for shadow caster submissions.
        }
    }
    // Store raw sortWorld (model-to-world only) for CSM caster use.
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            g_bgfxSortWorldRaw[r * 4 + c] = sortWorld[r][c];
        }
    }
}

void BgfxBackend::Capture_Sorted_Batch_Light(const D3DLIGHT8 & light, bool enabled)
{
    // Sort batch lights are always light 0 (the primary directional).
    // Scene ambient was already captured by Set_Light_Environment.
    // D3D8 Direction points FROM light toward surface; negate for N.L.
    if (enabled)
    {
        g_currentLightDirs[0][0] = -light.Direction.x;
        g_currentLightDirs[0][1] = -light.Direction.y;
        g_currentLightDirs[0][2] = -light.Direction.z;
        g_currentLightDirs[0][3] = 1.0f;
        g_currentLightColors[0][0] = light.Diffuse.r;
        g_currentLightColors[0][1] = light.Diffuse.g;
        g_currentLightColors[0][2] = light.Diffuse.b;
    }
    else
    {
        g_currentLightDirs[0][3] = 0.0f;
    }
}

static void BindTextureStages()
{
    // bgfx default (flags=0) is bilinear filtering. No explicit flags needed.
    if (bgfx::isValid(g_sTex0))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture0) ? g_currentBgfxTexture0 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(0, g_sTex0, bound, g_currentSamplerFlags0);
        }
    }
    if (bgfx::isValid(g_sTex1))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture1) ? g_currentBgfxTexture1 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(1, g_sTex1, bound, g_currentSamplerFlags1);
        }
    }
    if (bgfx::isValid(g_sTex2))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture2) ? g_currentBgfxTexture2 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(2, g_sTex2, bound, g_currentSamplerFlags2);
        }
    }
    if (bgfx::isValid(g_sTex3))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture3) ? g_currentBgfxTexture3 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(3, g_sTex3, bound, g_currentSamplerFlags3);
        }
    }
}

static void UploadMaterialUniforms()
{
    if (bgfx::isValid(g_uMatDiffuse))
    {
        bgfx::setUniform(g_uMatDiffuse, g_currentMatDiffuse);
    }

    if (bgfx::isValid(g_uTssOps0))
    {
        bgfx::setUniform(g_uTssOps0, g_currentTssOps0);
    }
    if (bgfx::isValid(g_uTssOps1))
    {
        bgfx::setUniform(g_uTssOps1, g_currentTssOps1);
    }
    if (bgfx::isValid(g_uAtestParams))
    {
        float atestParams[4] = { g_currentAtestRef, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(g_uAtestParams, atestParams);
    }
}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.13 sorted VB
// direct-draw submit. Called from DX8Wrapper::Draw_Sorting_IB_VB after
// it populates an internal dynamic VB and dynamic IB by copying a slice
// of the sorting VB/IB with the correct vba_offset / iba_offset /
// index_base_offset / min_vertex_index arithmetic. The engine's lock
// destructors already fired Capture_Dynamic_* for those inner buffers,
// so g_pendingDynVB / g_pendingDynIB hold their transients keyed by
// the exact access-class pointers we were just handed.
//
// We pull those transients into local draw state, emit a single bgfx
// submit to the sorted view with the inner buffers and remapped args
// (start_index=0, min_vertex_index=0, count relative to the inner
// buffers), and set g_skipNextSubmitEngineDraw so the outer Draw_Triangles
// does not emit a second, incorrect submit using the old sorting-VB
// args.

void BgfxBackend::Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                     const DynamicIBAccessClass & dyn_ib,
                                     unsigned short polygon_count,
                                     unsigned short vertex_count)
{
    if (!g_bgfxInitialized)
    {
        return;
    }

    // The inner dynamic buffers' WriteLockClass dtors already ran, so
    // Capture_Dynamic_Vertex_Data / Capture_Dynamic_Index_Data should
    // have stashed their transients keyed by &dyn_vb / &dyn_ib.
    if (!g_pendingDynVB.valid || g_pendingDynVB.owner != &dyn_vb)
    {
        static bool s_loggedSkipVB = false;
        if (!s_loggedSkipVB)
        {
            s_loggedSkipVB = true;
            WWDEBUG_SAY(("[BgfxBackend] Submit_Sorted_Draw SKIP: pendingDynVB not "
                         "claimable (valid=%d ownerMatch=%d)",
                         int(g_pendingDynVB.valid),
                         int(g_pendingDynVB.owner == &dyn_vb)));
        }
        return;
    }
    if (!g_pendingDynIB.valid || g_pendingDynIB.owner != &dyn_ib)
    {
        static bool s_loggedSkipIB = false;
        if (!s_loggedSkipIB)
        {
            s_loggedSkipIB = true;
            WWDEBUG_SAY(("[BgfxBackend] Submit_Sorted_Draw SKIP: pendingDynIB not "
                         "claimable (valid=%d ownerMatch=%d)",
                         int(g_pendingDynIB.valid),
                         int(g_pendingDynIB.owner == &dyn_ib)));
        }
        return;
    }

    const bgfx::TransientVertexBuffer vb = g_pendingDynVB.tvb;
    const bgfx::TransientIndexBuffer  ib = g_pendingDynIB.tib;
    g_pendingDynVB.valid = false;
    g_pendingDynIB.valid = false;

    if (!bgfx::isValid(g_currentBgfxProgram))
    {
        g_skipNextSubmitEngineDraw = true;
        return;
    }

    // Sort view's view+proj were set up at init (identity view,
    // projection tracks opaque view via Set_Projection_Transform_With_Z_Bias).
    // World is the current g_bgfxSortWorld if we are inside a sort batch,
    // otherwise the regular g_bgfxWorld (rigid FVF category with sorting=true
    // has no batch-wrapped Apply_Render_State - it uses the per-mesh world
    // set by the caller via g_renderBackend->Set_Transform).
    const float * worldMtx = g_inSortFlush ? g_bgfxSortWorld : g_bgfxWorld;
    bgfx::setTransform(worldMtx);

    bgfx::setVertexBuffer(0, &vb, 0, vertex_count);
    bgfx::setIndexBuffer(&ib, 0, static_cast<uint32_t>(polygon_count) * 3);

    BindTextureStages();
    UploadMaterialUniforms();

    uint64_t state = (g_currentBgfxState != 0)
        ? g_currentBgfxState
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    // TheSuperHackers @fix bobtista 16/04/2026 Override cull mode from
    // D3D device state — same as SubmitEngineDraw.
    {
        unsigned d3dCull = DX8Wrapper::Get_DX8_Render_State(D3DRS_CULLMODE);
        state &= ~(BGFX_STATE_CULL_CW | BGFX_STATE_CULL_CCW);
        if (d3dCull == 2) // D3DCULL_CW
            state |= BGFX_STATE_CULL_CW;
        else if (d3dCull == 3) // D3DCULL_CCW
            state |= BGFX_STATE_CULL_CCW;
    }

    // Apply color write mask override — same as SubmitEngineDraw.
    // Without this, sorted particles/effects write alpha, destroying
    // the shoreline depth gradient used by DESTALPHA water blending.
    if (g_colorWriteOverride >= 0)
    {
        state &= ~(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        state |= static_cast<uint64_t>(g_colorWriteOverride);
    }

    bgfx::setState(state);
    bgfx::submit(kBgfxEngineSortView, g_currentBgfxProgram);

    // Phase 4I.2 CSM: also submit sorted geometry as shadow caster.
    if (bgfx::isValid(g_shadowCasterProgram)
        && bgfx::isValid(g_shadowMapFB))
    {
        bgfx::setVertexBuffer(0, &vb);
        bgfx::setIndexBuffer(&ib, 0,
                             static_cast<uint32_t>(polygon_count) * 3);
        bgfx::setTransform(g_bgfxSortWorldRaw);
        const uint64_t casterState =
            BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CW;
        bgfx::setState(casterState);
        bgfx::submit(kBgfxShadowMapView, g_shadowCasterProgram);
    }

    g_skipNextSubmitEngineDraw = true;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.2 dynamic
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
// own pointer in g_pendingDynVB and claims the transient for the draw.

void BgfxBackend::Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                              const void * data,
                                              unsigned int size_bytes)
{
    if (!g_bgfxInitialized || vba == nullptr || data == nullptr || size_bytes == 0)
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
        g_pendingDynVB.valid = false;
        return;
    }

    bgfx::allocTransientVertexBuffer(&g_pendingDynVB.tvb, num_verts, layout);
    const uint32_t copy_bytes = num_verts * layout.getStride();
    const uint32_t bytes = (size_bytes < copy_bytes) ? size_bytes : copy_bytes;
    std::memcpy(g_pendingDynVB.tvb.data, data, bytes);
    g_pendingDynVB.owner = vba;
    g_pendingDynVB.valid = true;

}

void BgfxBackend::Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                             const void * data,
                                             unsigned int size_bytes)
{
    if (!g_bgfxInitialized || iba == nullptr || data == nullptr || size_bytes == 0)
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
        g_pendingDynIB.valid = false;
        return;
    }

    bgfx::allocTransientIndexBuffer(&g_pendingDynIB.tib, num_indices);
    const uint32_t copy_bytes = num_indices * sizeof(uint16_t);
    const uint32_t bytes = (size_bytes < copy_bytes) ? size_bytes : copy_bytes;
    std::memcpy(g_pendingDynIB.tib.data, data, bytes);
    g_pendingDynIB.owner = iba;
    g_pendingDynIB.valid = true;
}

// -- State: shaders, materials, textures ------------------------------------

void BgfxBackend::Set_Shader(const ShaderClass & shader)
{
    DX8Backend::Set_Shader(shader);

    g_currentBgfxProgram = g_uberProgram;
    g_currentBgfxState   = BuildBgfxStateForShader(shader);
    BuildTssOpsForShader(shader, g_currentTssOps0, g_currentTssOps1, &g_currentAtestRef);
    Clear_State_Overrides();
}

void BgfxBackend::Set_Material(const VertexMaterialClass * material)
{
    DX8Backend::Set_Material(material);

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.9 capture
    // material diffuse + opacity so the fragment shader can tint output
    // with team colors. Generals writes the player color into the
    // VertexMaterialClass diffuse channel; without this override bgfx
    // meshes render with stale or default colors and units are either
    // washed-out white or tinted by whatever the previous draw used.
    if (material != nullptr)
    {
        Vector3 diffuse(1.0f, 1.0f, 1.0f);
        material->Get_Diffuse(&diffuse);
        g_currentMatDiffuse[0] = diffuse.X;
        g_currentMatDiffuse[1] = diffuse.Y;
        g_currentMatDiffuse[2] = diffuse.Z;
        g_currentMatDiffuse[3] = material->Get_Opacity();
        // Track whether this material uses hardware lighting. When false
        // (terrain, pre-lit meshes), vertex colors already contain the
        // lit result and the shader must NOT apply N.L on top.
        g_currentLightingEnabled[0] = material->Get_Lighting() ? 1.0f : 0.0f;
    }
    else
    {
        g_currentMatDiffuse[0] = 1.0f;
        g_currentMatDiffuse[1] = 1.0f;
        g_currentMatDiffuse[2] = 1.0f;
        g_currentMatDiffuse[3] = 1.0f;
        // Null material means no material-driven lighting. Dazzle and
        // similar effect overlays call Set_Material(nullptr) and bake
        // their per-vertex intensity into diffuse.rgb; the lit path in
        // fs_uber would ignore that baked color and output tex * lit,
        // producing bright flashes where the dazzle should be invisible.
        // Force the unlit path so vertex diffuse modulates the output.
        g_currentLightingEnabled[0] = 0.0f;
    }
}

void BgfxBackend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Backend::Set_Texture(stage, texture);

    // Phase 4G.3 / 4G.4: stages 0-3 wired. Covers terrain base + detail
    // + cloud + noise, the standard 4-stage layout used by the
    // FlatHeightMap pixel shader family. Stages above 3 still fall
    // through unmigrated.
    {
        bgfx::TextureHandle h = EnsureBgfxTexture(texture);
        // TheSuperHackers @bugfix bobtista 16/04/2026 Phase 4I.2 use white
        // fallback for render target textures instead of dark blue. The
        // blue fallback was intended for water reflections but applies to
        // ALL unresolved RT textures, tinting the entire world blue.
        // White is the multiplicative identity — it passes through the
        // vertex/material color and lets the scene look correct until the
        // RT texture is properly captured.
        if (!bgfx::isValid(h) && texture != nullptr &&
            g_renderTargetSet.count(texture) > 0)
        {
            h = g_defaultWhiteTexture;
            static int s_loggedRTFallback = 0;
            if (s_loggedRTFallback < 5)
            {
                ++s_loggedRTFallback;
                TextureClass * t2d_fb = texture->As_TextureClass();
                WWDEBUG_SAY(("[BgfxBackend] RT FALLBACK: stage=%u using white fallback for %s",
                             stage,
                             t2d_fb ? t2d_fb->Get_Full_Path().str() : "(null)"));
            }
        }
        // Log when an invalid texture goes to white fallback
        if (!bgfx::isValid(h) && texture != nullptr &&
            g_renderTargetSet.count(texture) == 0)
        {
            static int s_loggedWhiteFallback = 0;
            if (s_loggedWhiteFallback < 5)
            {
                ++s_loggedWhiteFallback;
                TextureClass * t2d_fb = texture->As_TextureClass();
                WWDEBUG_SAY(("[BgfxBackend] WHITE FALLBACK: stage=%u tex=%s pool=%d",
                             stage,
                             t2d_fb ? t2d_fb->Get_Full_Path().str() : "(null)",
                             texture->Get_Pool()));
            }
        }
        TextureClass * t2d_name = texture ? texture->As_TextureClass() : nullptr;
        // Capture the source texture's wrap mode into bgfx sampler flags
        // so we can pass it at bind time. Without this, WRAP is the
        // default and ramp/LUT textures with CLAMP semantics produce
        // visible stripe artifacts at U>=1 (the shoreline checkerboard).
        uint32_t samplerFlags = 0;
        if (t2d_name != nullptr)
        {
            const TextureFilterClass & flt = t2d_name->Get_Filter();
            if (flt.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP)
                samplerFlags |= BGFX_SAMPLER_U_CLAMP;
            if (flt.Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP)
                samplerFlags |= BGFX_SAMPLER_V_CLAMP;
        }
        switch (stage)
        {
            case 0: g_currentBgfxTexture0 = h;
                    g_currentSamplerFlags0 = samplerFlags; break;
            case 1: g_currentBgfxTexture1 = h;
                    g_currentSamplerFlags1 = samplerFlags; break;
            case 2: g_currentBgfxTexture2 = h; g_currentSamplerFlags2 = samplerFlags; break;
            case 3: g_currentBgfxTexture3 = h; g_currentSamplerFlags3 = samplerFlags; break;
            default: break;
        }
    }
}

void BgfxBackend::Set_Ambient(const Vector3 & color)
{
    DX8Backend::Set_Ambient(color);
    g_currentSceneAmbient[0] = color.X;
    g_currentSceneAmbient[1] = color.Y;
    g_currentSceneAmbient[2] = color.Z;
}

void BgfxBackend::Override_Blend(unsigned srcBlend, unsigned dstBlend)
{
    static const uint64_t kBlendMap[9] = {
        0,
        BGFX_STATE_BLEND_ZERO,           // 1
        BGFX_STATE_BLEND_ONE,            // 2
        BGFX_STATE_BLEND_SRC_COLOR,      // 3
        BGFX_STATE_BLEND_INV_SRC_COLOR,  // 4
        BGFX_STATE_BLEND_SRC_ALPHA,      // 5
        BGFX_STATE_BLEND_INV_SRC_ALPHA,  // 6
        BGFX_STATE_BLEND_DST_ALPHA,      // 7
        BGFX_STATE_BLEND_INV_DST_ALPHA   // 8
    };
    if (srcBlend >= 1 && srcBlend <= 8 && dstBlend >= 1 && dstBlend <= 8)
    {
        g_blendOverrideActive = true;
        g_blendOverrideBits = BGFX_STATE_BLEND_FUNC(kBlendMap[srcBlend], kBlendMap[dstBlend]);
    }
    else
    {
        static bool s_loggedBlendBad = false;
        if (!s_loggedBlendBad)
        {
            s_loggedBlendBad = true;
            WWDEBUG_SAY(("[BgfxBackend] BLEND OVERRIDE BAD VALUES: src=%u dst=%u (out of range 1-8)",
                         srcBlend, dstBlend));
        }
    }
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, srcBlend);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, dstBlend);
}

void BgfxBackend::Override_Alpha_Test(bool enable, unsigned ref, unsigned func)
{
    g_atestOverrideActive = enable;
    g_atestOverrideRef = enable ? (ref / 255.0f) : 0.0f;
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, enable ? TRUE : FALSE);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF, ref);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC, func);
}

void BgfxBackend::Override_Alpha_Blend_Enable(bool enable)
{
    if (enable)
    {
        g_blendOverrideActive = true;
        g_blendOverrideBits = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                     BGFX_STATE_BLEND_INV_SRC_ALPHA);
    }
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void BgfxBackend::Override_Texcoord_Index(unsigned stage, unsigned uvIndex)
{
    if (stage == 0)
    {
        g_currentTexcoordSelect[0] = (uvIndex == 1) ? 1.0f : 0.0f;
    }
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, uvIndex);
}

void BgfxBackend::Override_Terrain_Blend(bool enable)
{
    g_currentTexcoordSelect[1] = enable ? 1.0f : 0.0f;
}

void BgfxBackend::Override_Material_Opacity(float opacity)
{
    g_currentMatDiffuse[3] = opacity;
    g_waterOverrideActive = true;
    g_blendOverrideActive = true;
    g_blendOverrideBits = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_ALPHA,
                                                  BGFX_STATE_BLEND_INV_DST_ALPHA);
}

void BgfxBackend::Begin_Effect_Overlay()
{
    g_effectOverlayActive = true;
}

void BgfxBackend::End_Effect_Overlay()
{
    g_effectOverlayActive = false;
}

void BgfxBackend::Set_Tree_Shader_Constants(const float swayTable[11][4],
                                            const float shroudOffset[4],
                                            const float shroudScale[4])
{
    std::memcpy(g_currentSwayTable,    swayTable,    sizeof(g_currentSwayTable));
    std::memcpy(g_currentShroudOffset, shroudOffset, sizeof(g_currentShroudOffset));
    std::memcpy(g_currentShroudScale,  shroudScale,  sizeof(g_currentShroudScale));
}

void BgfxBackend::Set_Tree_Vertex_Shader_Active(bool active)
{
    g_treeShaderActive = active;
}

void BgfxBackend::Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha)
{
    DX8Backend::Set_Color_Write_Enable(red, green, blue, alpha);
    uint64_t mask = 0;
    if (red)   mask |= BGFX_STATE_WRITE_R;
    if (green) mask |= BGFX_STATE_WRITE_G;
    if (blue)  mask |= BGFX_STATE_WRITE_B;
    if (alpha) mask |= BGFX_STATE_WRITE_A;
    g_colorWriteOverride = static_cast<int>(mask);
    g_suppressBgfxDraw = false;
}

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I mirror the
// DWORD variant into g_colorWriteOverride so stencil shadow volume
// passes that call Set_Color_Write_Mask(0) actually disable bgfx color
// writes. Previously the call fell through to DX8Backend only, leaving
// shadow volumes drawing as solid black geometry.
void BgfxBackend::Set_Color_Write_Mask(unsigned mask)
{
    DX8Backend::Set_Color_Write_Mask(mask);
    uint64_t bgfxMask = 0;
    if (mask & D3DCOLORWRITEENABLE_RED)   bgfxMask |= BGFX_STATE_WRITE_R;
    if (mask & D3DCOLORWRITEENABLE_GREEN) bgfxMask |= BGFX_STATE_WRITE_G;
    if (mask & D3DCOLORWRITEENABLE_BLUE)  bgfxMask |= BGFX_STATE_WRITE_B;
    if (mask & D3DCOLORWRITEENABLE_ALPHA) bgfxMask |= BGFX_STATE_WRITE_A;
    g_colorWriteOverride = static_cast<int>(bgfxMask);
    g_suppressBgfxDraw = false;
}

void BgfxBackend::Skip_Next_Bgfx_Submit()
{
    g_skipNextSubmitEngineDraw = true;
}

// TheSuperHackers @fix bobtista 16/04/2026 Remove g_currentMatDiffuse aliasing from texture factor.
// The shadow decal draw is now skipped via Skip_Next_Bgfx_Submit so the aliasing
// is unnecessary and it clobbers team colors.
void BgfxBackend::Set_Texture_Factor(unsigned argb)
{
    DX8Backend::Set_Texture_Factor(argb);
}

void BgfxBackend::Set_Shadow_Light_Position(float x, float y, float z)
{
    g_shadowSunPosX = x;
    g_shadowSunPosY = y;
    g_shadowSunPosZ = z;
    g_shadowSunPosSet = true;
    g_shadowLightCaptured = false;
    static int s_slpLog = 0;
    if (s_slpLog++ < 5)
    {
        WWDEBUG_SAY(("[CSM] Set_Shadow_Light_Position(%.1f, %.1f, %.1f)",
            x, y, z));
    }
}

void BgfxBackend::Set_Shadow_Volume_Shader_Active(bool active)
{
    g_shadowVolumeActive = active;
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

    if (!g_bgfxInitialized
        || !g_shadowVolumeActive
        || !bgfx::isValid(g_shadowVolumeProgram)
        || !bgfx::isValid(g_currentBgfxVB)
        || num_silhouette_verts < 3)
    {
        return;
    }

    const unsigned tris_per_cap = num_silhouette_verts - 2;
    const unsigned total_indices = 2 * tris_per_cap * 3;

    if (bgfx::getAvailTransientIndexBuffer(total_indices) < total_indices)
    {
        return;
    }

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, total_indices);
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
    if (g_stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECR
        || g_stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECRSAT)
    {
        return;
    }

    // No face culling, two-sided stencil matching the side-wall submit.
    uint64_t state = BGFX_STATE_DEPTH_TEST_LEQUAL;
    bgfx::setState(state);
    const uint32_t commonBits = g_stencilFuncBits
        | BGFX_STENCIL_FUNC_REF(g_stencilRef & 0xFF)
        | BGFX_STENCIL_FUNC_RMASK(g_stencilReadMask & 0xFF)
        | g_stencilFailOpBits
        | g_stencilZFailOpBits;
    bgfx::setStencil(commonBits | BGFX_STENCIL_OP_PASS_Z_DECRSAT,
                     commonBits | BGFX_STENCIL_OP_PASS_Z_INCRSAT);

    bgfx::setVertexBuffer(0, g_currentBgfxVB);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTransform(g_bgfxWorld);

    bgfx::submit(kBgfxShadowVolumeView, g_shadowVolumeProgram);
}

void BgfxBackend::Submit_Shadow_Volume_Triangulated_Caps(
    unsigned strip_start_vertex,
    const short * local_cap_indices,
    unsigned cap_index_count)
{
    if (!g_bgfxInitialized
        || !g_shadowVolumeActive
        || !bgfx::isValid(g_shadowVolumeProgram)
        || !bgfx::isValid(g_currentBgfxVB)
        || local_cap_indices == nullptr
        || cap_index_count < 3)
    {
        return;
    }

    // front cap + back cap (reversed winding)
    const unsigned total_indices = cap_index_count * 2;

    if (bgfx::getAvailTransientIndexBuffer(total_indices) < total_indices)
    {
        return;
    }

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, total_indices);
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
    uint64_t state = BGFX_STATE_DEPTH_TEST_LEQUAL;
    if (g_cullModeBits == 1)      state |= BGFX_STATE_CULL_CW;
    else if (g_cullModeBits == 2) state |= BGFX_STATE_CULL_CCW;
    bgfx::setState(state);
    bgfx::setStencil(g_shadowStencilFront, g_shadowStencilBack);

    bgfx::setVertexBuffer(0, g_currentBgfxVB);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTransform(g_bgfxWorld);

    bgfx::submit(kBgfxShadowVolumeView, g_shadowVolumeProgram);
}

void BgfxBackend::Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                              unsigned stencil_read_mask,
                                              unsigned stencil_ref)
{
    DX8Backend::Apply_Stencil_Shadow_Darken(shadow_color, stencil_read_mask, stencil_ref);

    if (!g_bgfxInitialized || !bgfx::isValid(g_shadowApplyProgram))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();

    if (!bgfx::allocTransientBuffers(&tvb, layout, 4, &tib, 6))
        return;

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
    if (bgfx::isValid(g_uShadowColor))
        bgfx::setUniform(g_uShadowColor, color);

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
        | BGFX_STATE_DEPTH_TEST_ALWAYS
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
    bgfx::setState(state);

    uint32_t stencil = BGFX_STENCIL_TEST_LEQUAL
        | BGFX_STENCIL_FUNC_REF(stencil_ref & 0xFF)
        | BGFX_STENCIL_FUNC_RMASK(stencil_read_mask & 0xFF)
        | BGFX_STENCIL_OP_FAIL_S_KEEP
        | BGFX_STENCIL_OP_FAIL_Z_KEEP
        | BGFX_STENCIL_OP_PASS_Z_KEEP;
    bgfx::setStencil(stencil);

    bgfx::submit(kBgfxShadowApplyView, g_shadowApplyProgram);
}

void BgfxBackend::Set_Stencil_Enable(bool enable)
{
    DX8Backend::Set_Stencil_Enable(enable);
    g_stencilEnabled = enable;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Func(CompareFunc f)
{
    DX8Backend::Set_Stencil_Func(f);
    g_stencilFuncBits = MapCmpFuncToBgfxStencilTest(static_cast<int>(f));
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Ref(unsigned ref)
{
    DX8Backend::Set_Stencil_Ref(ref);
    g_stencilRef = ref;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Mask(unsigned mask)
{
    DX8Backend::Set_Stencil_Mask(mask);
    g_stencilReadMask = mask;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Write_Mask(unsigned mask)
{
    DX8Backend::Set_Stencil_Write_Mask(mask);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Pass_Op(StencilOp op)
{
    DX8Backend::Set_Stencil_Pass_Op(op);
    g_stencilPassOpBits = MapStencilOpToBgfxPassZ(static_cast<int>(op));
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Fail_Op(StencilOp op)
{
    DX8Backend::Set_Stencil_Fail_Op(op);
    g_stencilFailOpBits = MapStencilOpToBgfxFailS(static_cast<int>(op));
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_ZFail_Op(StencilOp op)
{
    DX8Backend::Set_Stencil_ZFail_Op(op);
    g_stencilZFailOpBits = MapStencilOpToBgfxFailZ(static_cast<int>(op));
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Cull_Mode(CullMode mode)
{
    DX8Backend::Set_Cull_Mode(mode);
    switch (mode)
    {
        case RB_CULL_CW:  g_cullModeBits = 1; break;
        case RB_CULL_CCW: g_cullModeBits = 2; break;
        case RB_CULL_NONE:
        default:          g_cullModeBits = 0; break;
    }
}

void BgfxBackend::Set_Depth_Func(CompareFunc func)
{
    DX8Backend::Set_Depth_Func(func);
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
    const unsigned idx = static_cast<unsigned>(func);
    if (idx < 9)
    {
        g_currentBgfxState &= ~BGFX_STATE_DEPTH_TEST_MASK;
        g_currentBgfxState |= kDepthMap[idx];
    }
}

void BgfxBackend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    DX8Backend::Set_Render_Target_With_Z(texture, ztexture);

    if (texture == nullptr || !g_bgfxInitialized)
    {
        g_renderToTexture = false;
        return;
    }

    auto it = g_framebufferCache.find(texture);
    if (it == g_framebufferCache.end())
    {
        TextureClass * tex2d = texture->As_TextureClass();
        const uint16_t w = tex2d ? static_cast<uint16_t>(tex2d->Get_Width())  : 64;
        const uint16_t h = tex2d ? static_cast<uint16_t>(tex2d->Get_Height()) : 64;

        bgfx::TextureHandle colorTex = bgfx::createTexture2D(
            w, h, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle depthTex = bgfx::createTexture2D(
            w, h, false, 1, bgfx::TextureFormat::D24S8,
            BGFX_TEXTURE_RT_WRITE_ONLY);

        bgfx::TextureHandle attachments[2] = { colorTex, depthTex };
        bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(2, attachments, true);

        BgfxFramebufferEntry entry = { fb, colorTex, w, h };
        g_framebufferCache[texture] = entry;
        it = g_framebufferCache.find(texture);

        WWDEBUG_SAY(("[BgfxBackend] RTT framebuffer created %dx%d for tex=%p",
                     w, h, texture));
    }

    const BgfxFramebufferEntry & entry = it->second;

    bgfx::setViewFrameBuffer(kBgfxRTTView, entry.fb);
    bgfx::setViewRect(kBgfxRTTView, 0, 0, entry.width, entry.height);
    bgfx::setViewClear(kBgfxRTTView,
                        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                        0x000000ff, 1.0f, 0);
    bgfx::touch(kBgfxRTTView);

    g_renderToTexture = true;
}

void BgfxBackend::Clear_State_Overrides()
{
    g_blendOverrideActive = false;
    g_atestOverrideActive = false;
    g_suppressBgfxDraw = false;
    g_colorWriteOverride = -1;
    g_currentTexcoordSelect[0] = 0.0f;
    // Do NOT clear g_currentTexcoordSelect[1] (terrain blend) here.
    // Override_Terrain_Blend is called from the shader manager BEFORE
    // Set_Shader, so clearing it in Set_Shader (which calls us) would
    // undo the terrain blend flag every frame. Terrain blend is reset
    // at Begin_Scene and by Override_Terrain_Blend(false) explicitly.
}

void BgfxBackend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    DX8Backend::Set_Light_Environment(light_env);

    if (light_env != nullptr)
    {
        const Vector3 & ambient = light_env->Get_Equivalent_Ambient();
        g_currentSceneAmbient[0] = ambient.X;
        g_currentSceneAmbient[1] = ambient.Y;
        g_currentSceneAmbient[2] = ambient.Z;

        const int count = light_env->Get_Light_Count();
        for (int i = 0; i < 4; ++i)
        {
            if (i < count)
            {
                const Vector3 & dir = light_env->Get_Light_Direction(i);
                const Vector3 & dif = light_env->Get_Light_Diffuse(i);
                g_currentLightDirs[i][0] = -dir.X;
                g_currentLightDirs[i][1] = -dir.Y;
                g_currentLightDirs[i][2] = -dir.Z;
                g_currentLightDirs[i][3] = 1.0f; // enabled
                g_currentLightColors[i][0] = dif.X;
                g_currentLightColors[i][1] = dif.Y;
                g_currentLightColors[i][2] = dif.Z;
                g_currentLightColors[i][3] = 1.0f;
            }
            else
            {
                g_currentLightDirs[i][3] = 0.0f; // disabled
            }
        }
    }
}

// Get_Shader, Apply_Render_State_Changes, Apply_Default_State,
// Invalidate_Cached_Render_States, Set_Blend_Op, Set_Blend_Factors,
// Set_Color_Write_Enable, Set_Alpha_Blend_Enable, hardware cursor, and
// Set_Stencil_* are all inherited from DX8Backend and forward unchanged
// to DX8Wrapper.

// -- Transforms --------------------------------------------------------------

void BgfxBackend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    DX8Backend::Set_Transform(transform, m);

    switch (transform)
    {
        case RB_TRANSFORM_WORLD:
            W3DMatrix4ToBgfx(m, g_bgfxWorld);
            break;
        case RB_TRANSFORM_VIEW:
            W3DMatrix4ToBgfx(m, g_bgfxView);
            g_bgfxViewProjDirty = true;
            g_2DOverlayActive = false;
            break;
        case RB_TRANSFORM_PROJECTION:
            W3DMatrix4ToBgfx(m, g_bgfxProj);
            g_bgfxViewProjDirty = true;
            break;
        default:
            break;
    }
}

void BgfxBackend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    DX8Backend::Set_Transform(transform, m);

    switch (transform)
    {
        case RB_TRANSFORM_WORLD:
            W3DMatrix3DToBgfx(m, g_bgfxWorld);
            break;
        case RB_TRANSFORM_VIEW:
            W3DMatrix3DToBgfx(m, g_bgfxView);
            g_bgfxViewProjDirty = true;
            g_2DOverlayActive = false;
            break;
        default:
            break;
    }
}

// Get_Transform / Is_World_Identity / Is_View_Identity are inherited
// from DX8Backend.

void BgfxBackend::Set_World_Identity()
{
    DX8Backend::Set_World_Identity();
    IdentityMatrix(g_bgfxWorld);
}

void BgfxBackend::Set_View_Identity()
{
    DX8Backend::Set_View_Identity();
    IdentityMatrix(g_bgfxView);
    g_bgfxViewProjDirty = true;
    g_2DOverlayActive = true;
}

void BgfxBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
                                                       float znear, float zfar)
{
    DX8Backend::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);

    W3DMatrix4ToBgfx(matrix, g_bgfxProj);
    g_bgfxViewProjDirty = true;

}

// Lighting and fog (Set_Light, Set_Ambient, Get_Ambient, Set_Fog,
// Get_Fog_Enable, Set_Light_Environment, Get_Light_Environment) are
// inherited from DX8Backend.

// -- Draw calls --------------------------------------------------------------

namespace
{
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.3 actual bgfx
// submit. Called from both Draw_Triangles overloads when we have a
// valid cached VB + IB + program. State and program were cached by
// Set_Shader; the buffers were cached by Set_Vertex_Buffer /
// Set_Index_Buffer. Submits to view 0, the same view the test
// triangle uses, so the popup will show both overlapping until we
// move engine geometry to its own view.
//
// View transforms remain identity. Engine geometry is in world
// coordinates which will be far off-screen under an identity
// projection - so this draws "nothing visible" except as a way to
// prove the submit pipeline works without crashing. Real
// view/projection wiring lands in the next session.
void SubmitEngineDraw(unsigned short start_index,
                      unsigned short polygon_count,
                      unsigned short min_vertex_index,
                      unsigned short vertex_count)
{
    if (g_suppressBgfxDraw)
    {
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }
    // Detect when the engine has restored the back buffer. The water/shadow
    // code calls DX8Wrapper::Set_Render_Target(nullptr) directly, bypassing
    // g_renderBackend. Poll the DX8 state to keep g_renderToTexture in sync.
    if (g_renderToTexture && !DX8Wrapper::Is_Render_To_Texture())
    {
        g_renderToTexture = false;
    }
    if (!g_bgfxInitialized)
    {
        return;
    }
    if (!bgfx::isValid(g_currentBgfxProgram))
    {
        return;
    }
    const bool have_vb = g_currentUseTransientVB || bgfx::isValid(g_currentBgfxVB);
    const bool have_ib = g_currentUseTransientIB || bgfx::isValid(g_currentBgfxIB);
    if (!have_vb || !have_ib)
    {
        return;
    }

    // Phase 4G.12: route to the dedicated sorted view when the sort
    // flush has activated it. The sort view's view+proj were set at
    // init and are refreshed by Set_Projection_Transform_With_Z_Bias,
    // so it never needs a per-submit setViewTransform - only view 1
    // (the opaque view) uses the dirty flag.
    // Secondary 2D detection: if the view matrix is identity and the
    // projection has no perspective (w-divide), this is a 2D overlay
    // draw even if Set_View_Identity wasn't called. This catches draws
    // where DX8 state restores clear g_2DOverlayActive between the
    // Set_View_Identity call and the actual Draw_Triangles.
    bool is2D = g_2DOverlayActive;
    if (!is2D && !g_renderToTexture && !g_waterOverrideActive
        && !g_effectOverlayActive && !g_inSortFlush)
    {
        // Check if view matrix is identity (2D mode)
        const float *v = g_bgfxView;
        if (v[0] == 1.0f && v[5] == 1.0f && v[10] == 1.0f && v[15] == 1.0f
            && v[1] == 0.0f && v[2] == 0.0f && v[4] == 0.0f && v[6] == 0.0f)
        {
            is2D = true;
        }
    }

    bgfx::ViewId submitView;
    if (is2D)
    {
        submitView = kBgfxUIView;
    }
    else if (g_renderToTexture)
    {
        submitView = kBgfxRTTView;
    }
    else if (g_waterOverrideActive)
    {
        submitView = kBgfxWaterView;
    }
    else if (g_effectOverlayActive)
    {
        submitView = kBgfxEffectOverlayView;
    }
    else if (g_inSortFlush)
    {
        submitView = kBgfxEngineSortView;
    }
    else
    {
        submitView = kBgfxEngineView;
    }
    const float *      worldMtx   = g_inSortFlush ? g_bgfxSortWorld     : g_bgfxWorld;

    // Push the engine view+projection when they change. setViewTransform
    // applies until the next change so we do not need to call it per
    // submit, only when the engine has updated either matrix. Sort view
    // draws never touch g_bgfxView so the opaque view is never stomped.
    if (!g_inSortFlush && !g_renderToTexture && !g_2DOverlayActive && g_bgfxViewProjDirty)
    {
        // Capture the camera view+proj at the first opaque draw of each
        // frame. Later Set_Projection calls (water, shadows, sneak attack)
        // may overwrite g_bgfxProj with a different frustum. We re-apply
        // the camera projection to view 1 at End_Scene time.
        if (!g_bgfxCameraCaptured)
        {
            std::memcpy(g_bgfxCameraView, g_bgfxView, sizeof(g_bgfxCameraView));
            std::memcpy(g_bgfxCameraProj, g_bgfxProj, sizeof(g_bgfxCameraProj));
            g_bgfxCameraCaptured = true;
        }
        // Phase 4I.2 CSM: update light transform every frame so
        // shadows follow the camera as it pans/zooms.
        if (!g_shadowLightCaptured)
        {
            UpdateShadowLightTransform();
        }
        bgfx::setViewTransform(kBgfxEngineView, g_bgfxView, g_bgfxProj);
        // Shadow-volume view shares the engine camera; push the same
        // view+proj so the extrusion geometry lands where the opaque
        // geometry in view 1 landed.
        bgfx::setViewTransform(kBgfxShadowVolumeView, g_bgfxView, g_bgfxProj);
        g_bgfxViewProjDirty = false;
    }
    // During RTT, push the current (reflected/shadow) view+proj to the RTT view.
    if (g_renderToTexture && g_bgfxViewProjDirty)
    {
        bgfx::setViewTransform(kBgfxRTTView, g_bgfxView, g_bgfxProj);
        g_bgfxViewProjDirty = false;
    }

    // World matrix is per-submit. bgfx consumes the value at submit time
    // and resets the per-draw transform after each submit.
    bgfx::setTransform(worldMtx);

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.7 offset
    // semantics. DX8Wrapper::Set_Index_Buffer_Index_Offset and
    // Set_Vertex_Buffer(DynamicVBAccessClass) set a d3d8 BaseVertexIndex
    // (passed to SetIndices). d3d8 implicitly adds that value to every
    // index during the draw, i.e. effective_vertex = VB[IB[start+i] +
    // base]. The bgfx equivalent is setVertexBuffer's _startVertex
    // parameter, NOT an IB offset. Previously SubmitEngineDraw was
    // incorrectly adding g_currentIBOffset to the IB start - that
    // shifted which indices were read instead of which vertex they
    // resolved to, so meshes with a non-zero base_vertex_offset drew
    // garbled geometry from other meshes' vertex data.
    const uint32_t base_vertex = static_cast<uint32_t>(g_currentIBOffset);
    if (g_currentUseTransientVB)
    {
        if (g_inSortFlush)
        {
            // Sort flush: use 2-arg overload (binds entire buffer).
            // Sort indices are absolute offsets into the full transient,
            // so no base vertex offset is needed. The 4-arg overload
            // would restrict the vertex range and clip high indices.
            bgfx::setVertexBuffer(0, &g_currentTransientVB);
        }
        else
        {
            // Skin/dynamic draws: apply base_vertex as startVertex.
            // Each mesh part within the shared transient VB has a
            // different base offset (from Set_Index_Buffer_Index_Offset).
            // Without this, all mesh parts read from vertex 0 and
            // infantry/vehicles are invisible or garbled.
            bgfx::setVertexBuffer(0, &g_currentTransientVB,
                                  base_vertex, vertex_count);
        }
    }
    else
    {
        bgfx::setVertexBuffer(0, g_currentBgfxVB, base_vertex, vertex_count);
    }

    if (g_currentUseTransientIB)
    {
        bgfx::setIndexBuffer(&g_currentTransientIB,
                             start_index,
                             static_cast<uint32_t>(polygon_count) * 3);
    }
    else
    {
        bgfx::setIndexBuffer(g_currentBgfxIB,
                             start_index,
                             static_cast<uint32_t>(polygon_count) * 3);
    }

    // Phase 4F.2 / 4G.3 bind engine textures on stages 0 and 1 by
    // Set_Texture, falling back to the 1x1 white default if no real
    // texture is cached. The default white * vertex color = vertex color,
    // which keeps untextured draws visible until every texture path
    // is migrated. For stage 1, white is also the multiplicative
    // identity, so single-texture draws (which don't bind stage 1) are
    // unaffected by the second sample.
    //
    // Phase 4G.9 force trilinear filtering on all sampler stages.
    // DX8Wrapper normally sets per-stage MIN/MAG/MIP filters via
    // Set_DX8_Texture_Stage_State which bypasses g_renderBackend, so
    // the bgfx samplers use their creation-time default (often point).
    // Jamming BGFX_SAMPLER_MIN_ANISOTROPIC | MAG_ANISOTROPIC | MIP_LINEAR
    // at bind time makes diagonal terrain edges, rocks, and unit
    // textures stop looking blocky.
    // Pass 0 = use the sampler's creation-time defaults, which is
    // linear min/mag + mip-linear (trilinear) if the texture has
    // mipmaps. Passing an explicit MIP_POINT here was forcing nearest-
    // neighbor mip selection, breaking mipmap smoothing and making
    // distant terrain patches look blocky. Let bgfx pick the best
    // filter it can for each texture.
    BindTextureStages();
    UploadMaterialUniforms();
    // Read current D3D light state per-draw. Set_Light_Environment and
    // Set_Ambient capture some paths, but many callers set lights via
    // DX8Wrapper::Set_DX8_Light or Apply_Render_State_Changes directly,
    // bypassing g_renderBackend. Reading the device state here ensures
    // bgfx always has the correct lights regardless of how they were set.
    {
        RenderStateStruct rs;
        DX8Wrapper::Get_Render_State(rs);
        for (int li = 0; li < 4; ++li)
        {
            if (rs.LightEnable[li])
            {
                const D3DLIGHT8 & dl = rs.Lights[li];
                g_currentLightDirs[li][0] = -dl.Direction.x;
                g_currentLightDirs[li][1] = -dl.Direction.y;
                g_currentLightDirs[li][2] = -dl.Direction.z;
                g_currentLightDirs[li][3] = 1.0f;
                g_currentLightColors[li][0] = dl.Diffuse.r;
                g_currentLightColors[li][1] = dl.Diffuse.g;
                g_currentLightColors[li][2] = dl.Diffuse.b;
                g_currentLightColors[li][3] = 1.0f;
            }
            else
            {
                g_currentLightDirs[li][3] = 0.0f;
            }
        }
        const unsigned ambientColor = DX8Wrapper::Get_DX8_Render_State(D3DRS_AMBIENT);
        g_currentSceneAmbient[0] = ((ambientColor >> 16) & 0xFF) / 255.0f;
        g_currentSceneAmbient[1] = ((ambientColor >>  8) & 0xFF) / 255.0f;
        g_currentSceneAmbient[2] = ((ambientColor >>  0) & 0xFF) / 255.0f;
    }
    if (bgfx::isValid(g_uLightDirs))
        bgfx::setUniform(g_uLightDirs, g_currentLightDirs, 4);
    if (bgfx::isValid(g_uLightColors))
        bgfx::setUniform(g_uLightColors, g_currentLightColors, 4);
    if (bgfx::isValid(g_uSceneAmbient))
        bgfx::setUniform(g_uSceneAmbient, g_currentSceneAmbient);
    if (bgfx::isValid(g_uLightingEnabled))
    {
        // TheSuperHackers @bugfix bobtista 15/04/2026 force lighting off
        // for additive-blend draws. Particle / dazzle / scanner sprites
        // bake their per-vertex intensity attenuation into vertex diffuse
        // (DazzleRenderObjClass and the particle system do this). The
        // uber-shader's lit branch ignores vertex diffuse and outputs
        // tex * lit_color, which renders these effects at full intensity
        // even when they should be invisible — producing bright white
        // halos around things like the microwave-scanner dome.
        const uint64_t blendBitsForLight = g_currentBgfxState & BGFX_STATE_BLEND_MASK;
        const uint64_t kAddONE_ONE = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                                           BGFX_STATE_BLEND_ONE);
        const uint64_t kAddSA_ONE  = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                           BGFX_STATE_BLEND_ONE);
        // TheSuperHackers @fix bobtista 16/04/2026 Also force lighting off for alpha-blend
        // particles (SRC_ALPHA/INV_SRC_ALPHA) when priColorOp is SELECTARG1 (GRADIENT_DISABLE),
        // so the lit path does not ignore vertex color on particles that bake intensity into it.
        const uint64_t kAlphaSA_ISA = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                             BGFX_STATE_BLEND_INV_SRC_ALPHA);
        if (blendBitsForLight == kAddONE_ONE || blendBitsForLight == kAddSA_ONE
            || (blendBitsForLight == kAlphaSA_ISA && g_currentTssOps0[0] < 1.5f))
        {
            float forced[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(g_uLightingEnabled, forced);
        }
        else
        {
            bgfx::setUniform(g_uLightingEnabled, g_currentLightingEnabled);
        }
    }

    // Detect shroud pass: D3D8 uses TCI_CAMERASPACEPOSITION + depth func
    // EQUAL to render a multiplicative shroud overlay. Both conditions must
    // be true to avoid false positives from other effects that set TCI bits.
    {
        bool shroudDetected = false;
        unsigned depthFunc = DX8Wrapper::Get_DX8_Render_State(D3DRS_ZFUNC);
        if (depthFunc == D3DCMP_EQUAL)
        {
            for (unsigned stg = 0; stg < 4 && !shroudDetected; ++stg)
            {
                unsigned tci = DX8Wrapper::Get_DX8_Texture_Stage_State(stg, D3DTSS_TEXCOORDINDEX);
                if (tci & D3DTSS_TCI_CAMERASPACEPOSITION)
                {
                    shroudDetected = true;
                    g_currentTexcoordSelect[2] = 1.0f;
                    // Extract shroud offset+scale by cancelling inv(view)
                    // from the texture matrix: view * texMtx = T * S.
                    D3DMATRIX texMtx;
                    DX8Wrapper::_Get_DX8_Transform(
                        static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stg), texMtx);
                    D3DMATRIX viewMtx;
                    DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, viewMtx);
                    // Manual 4x4 multiply: ts = view * texMtx (D3D row-major)
                    D3DMATRIX ts;
                    for (int rr = 0; rr < 4; rr++)
                        for (int cc = 0; cc < 4; cc++)
                        {
                            ts.m[rr][cc] = 0;
                            for (int k = 0; k < 4; k++)
                                ts.m[rr][cc] += viewMtx.m[rr][k] * texMtx.m[k][cc];
                        }
                    // ts = T * S: scale on diagonal, translated scale in row 3
                    float shroudParams[4] = {
                        (ts.m[0][0] != 0.0f) ? ts.m[3][0] / ts.m[0][0] : 0.0f,
                        (ts.m[1][1] != 0.0f) ? ts.m[3][1] / ts.m[1][1] : 0.0f,
                        ts.m[0][0],
                        ts.m[1][1]
                    };
                    if (bgfx::isValid(g_uShroudParams))
                        bgfx::setUniform(g_uShroudParams, shroudParams);
                }
            }
        }
        if (!shroudDetected)
        {
            g_currentTexcoordSelect[2] = 0.0f;
        }
    }

    if (bgfx::isValid(g_uTexcoordSelect))
        bgfx::setUniform(g_uTexcoordSelect, g_currentTexcoordSelect);

    uint64_t state = (g_currentBgfxState != 0)
        ? g_currentBgfxState
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    state |= BGFX_STATE_MSAA;

    // TheSuperHackers @fix bobtista 16/04/2026 Override cull mode from
    // D3D device state. ShaderClass::Apply() may change D3DRS_CULLMODE
    // directly (e.g. water reflection inversion) without going through
    // g_renderBackend->Set_Cull_Mode().
    {
        unsigned d3dCull = DX8Wrapper::Get_DX8_Render_State(D3DRS_CULLMODE);
        state &= ~(BGFX_STATE_CULL_CW | BGFX_STATE_CULL_CCW);
        if (d3dCull == 2) // D3DCULL_CW
            state |= BGFX_STATE_CULL_CW;
        else if (d3dCull == 3) // D3DCULL_CCW
            state |= BGFX_STATE_CULL_CCW;
        // D3DCULL_NONE (1) = no cull bits set
    }

    // Apply post-ShaderClass blend override (terrain blend, W3DCustomEdging, etc.)
    if (g_blendOverrideActive)
    {
        state &= ~BGFX_STATE_BLEND_MASK;
        state |= g_blendOverrideBits;
    }
    // Apply post-ShaderClass alpha test override
    if (g_atestOverrideActive)
    {
        if (bgfx::isValid(g_uAtestParams))
        {
            float atestParams[4] = { g_atestOverrideRef, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(g_uAtestParams, atestParams);
        }
    }

    // Sort view uses the same projection as the opaque view, so particle
    // Z values (in view space) produce the same depth as opaque geometry
    // at the same world position. Use the shader's normal depth compare
    // (typically LEQUAL) for correct depth testing against opaque geometry.
    // No DEPTH_TEST_ALWAYS override needed.

    // Apply color write mask override from Set_Color_Write_Enable.
    // This handles both alpha-only writes (shoreline depth gradient)
    // and RGB-only writes (terrain with alpha writes disabled).
    if (g_colorWriteOverride >= 0)
    {
        state &= ~(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        state |= static_cast<uint64_t>(g_colorWriteOverride);
    }

    if (g_waterOverrideActive)
    {
        g_waterOverrideActive = false;
    }

    if (g_shadowVolumeActive && bgfx::isValid(g_shadowVolumeProgram))
    {
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }

    bgfx::setState(state);

    // Phase 4I.2 CSM: compute light view-projection once for both
    // receiver uniform and caster submit.
    float lightVP[16];
    bx::mtxMul(lightVP, g_shadowLightView, g_shadowLightProj);

    if (bgfx::isValid(g_uShadowLightViewProj)
        && bgfx::isValid(g_shadowMapDepth)
        && bgfx::isValid(g_sShadowMap))
    {
        // Compose model * lightView * lightProj per-draw.
        // Use the CORRECT model matrix: g_bgfxSortWorldRaw for sorted
        // draws (no camera view baked in), g_bgfxWorld for opaque.
        float shadowMVP[16];
        const float * modelMtx = g_inSortFlush ? g_bgfxSortWorldRaw : worldMtx;
        bx::mtxMul(shadowMVP, modelMtx, lightVP);
        bgfx::setUniform(g_uShadowLightViewProj, shadowMVP);
        bgfx::setTexture(4, g_sShadowMap, g_shadowMapDepth);
    }

    // Phase 4H tree / grass sway shader takes over the program slot
    // and uploads its own constants when active. Otherwise fall back
    // to whatever ShaderClass picked (g_currentBgfxProgram).
    bgfx::ProgramHandle program = g_currentBgfxProgram;
    if (g_treeShaderActive && bgfx::isValid(g_treeProgram))
    {
        program = g_treeProgram;
        if (bgfx::isValid(g_uSwayTable))
            bgfx::setUniform(g_uSwayTable, g_currentSwayTable, kSwayTableEntries);
        if (bgfx::isValid(g_uShroudOffset))
            bgfx::setUniform(g_uShroudOffset, g_currentShroudOffset);
        if (bgfx::isValid(g_uShroudScale))
            bgfx::setUniform(g_uShroudScale, g_currentShroudScale);
    }
    bgfx::submit(submitView, program);

    // Phase 4I.2 CSM caster pass. Submit opaque and sort-flush geometry
    // to the shadow map view (view 8). Skip RTT, water, and effect
    // overlay (those are non-shadow-casters by design). bgfx preserves
    // the per-view transform (set in UpdateShadowLightTransform) so
    // the caster rasterizes from the light's POV.
    const bool isShadowCaster =
        (submitView == kBgfxEngineView || submitView == kBgfxEngineSortView)
        && !g_2DOverlayActive;
    const bool hasVB = g_currentUseTransientVB || bgfx::isValid(g_currentBgfxVB);
    if (isShadowCaster
        && bgfx::isValid(g_shadowCasterProgram)
        && hasVB)
    {
        // Re-bind VB/IB/transform (bgfx consumes them per-submit).
        if (g_currentUseTransientVB)
        {
            if (g_inSortFlush)
                bgfx::setVertexBuffer(0, &g_currentTransientVB);
            else
                bgfx::setVertexBuffer(0, &g_currentTransientVB,
                                      static_cast<uint32_t>(g_currentIBOffset),
                                      vertex_count);
        }
        else
        {
            bgfx::setVertexBuffer(0, g_currentBgfxVB,
                                  static_cast<uint32_t>(g_currentIBOffset),
                                  vertex_count);
        }
        if (g_currentUseTransientIB)
            bgfx::setIndexBuffer(&g_currentTransientIB,
                                 start_index,
                                 static_cast<uint32_t>(polygon_count) * 3);
        else
            bgfx::setIndexBuffer(g_currentBgfxIB,
                                 start_index,
                                 static_cast<uint32_t>(polygon_count) * 3);
        // Use the world transform used by the just-issued engine
        // submit. For sort-flush draws that's g_bgfxSortWorld (which
        // has the sort's batch transform baked in). For regular
        // opaque it's g_bgfxWorld.
        bgfx::setTransform(worldMtx);
        {
            float casterMVP[16];
            const float * casterModel = g_inSortFlush ? g_bgfxSortWorldRaw : worldMtx;
            bx::mtxMul(casterMVP, casterModel, lightVP);
            bgfx::setUniform(g_uShadowLightViewProj, casterMVP);
        }
        const uint64_t casterState =
            BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CW;
        bgfx::setState(casterState);
        bgfx::submit(kBgfxShadowMapView, g_shadowCasterProgram);
    }
}
}

void BgfxBackend::Draw_Triangles(unsigned short start_index,
                                 unsigned short polygon_count,
                                 unsigned short min_vertex_index,
                                 unsigned short vertex_count)
{
    DX8Backend::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

    // Phase 4G.13: if DX8Wrapper::Draw_Sorting_IB_VB already submitted
    // the draw with correctly remapped args against its internal dynamic
    // buffers, skip the outer submit.
    if (g_skipNextSubmitEngineDraw)
    {
        g_skipNextSubmitEngineDraw = false;
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
    DX8Backend::Draw_Triangles(buffer_type, start_index, polygon_count, min_vertex_index, vertex_count);

    if (g_skipNextSubmitEngineDraw)
    {
        g_skipNextSubmitEngineDraw = false;
        return;
    }
    SubmitEngineDraw(start_index, polygon_count, min_vertex_index, vertex_count);
}

// TheSuperHackers @feature bobtista 16/04/2026 Draw_Strip override so strip-based
// geometry (e.g. water tracks) goes through bgfx instead of silently falling back
// to the DX8-only base class.
void BgfxBackend::Draw_Strip(unsigned short start_index,
                             unsigned short index_count,
                             unsigned short min_vertex_index,
                             unsigned short vertex_count)
{
    DX8Backend::Draw_Strip(start_index, index_count, min_vertex_index, vertex_count);

    if (g_skipNextSubmitEngineDraw)
    {
        g_skipNextSubmitEngineDraw = false;
        return;
    }

    SubmitEngineDraw(start_index, index_count, min_vertex_index, vertex_count);
}

// The programmable pipeline (Set_Vertex_Shader, Set_Pixel_Shader,
// Set_Vertex_Shader_Constant, Set_Pixel_Shader_Constant), and render targets
// (Create_Render_Target, Set_Render_Target_With_Z, Is_Render_To_Texture,
// Set_Shadow_Map, Get_Shadow_Map) are inherited from DX8Backend.
