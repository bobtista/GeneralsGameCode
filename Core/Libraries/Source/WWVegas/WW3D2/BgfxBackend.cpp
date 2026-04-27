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

// TheSuperHackers @refactor bobtista 10/04/2026 BgfxBackend.
// IRenderBackend implementation that drives bgfx as the primary
// rendering backend, translating the engine's DX8-era draw calls
// into bgfx submits with a fixed-function-emulating uber shader.

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

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. bgfx takes the main
// game window. A secondary popup is created for D3D8 reference output.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

#include "BgfxBackendState.h"

// Render-state globals. Defined here (external linkage), declared `extern`
// in BgfxBackendState.h so BgfxBackendTextures.cpp can reference them.
BgfxDevice     g_device;
BgfxUniforms   g_uniforms;
BgfxDraw       g_draw;
BgfxOverrides  g_overrides;
BgfxViewFlags  g_views;
BgfxFrame      g_frame;
BgfxCaches     g_caches;
// Phase 5 asset-ingress resource side-table. id 0 is reserved invalid.
BgfxPhase5Resources g_phase5 = { {}, 1 };


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

static uint32_t MapCmpFuncToBgfxStencilTest(int f)
{
    switch (f)
    {
        case D3DCMP_NEVER:        return BGFX_STENCIL_TEST_NEVER;
        case D3DCMP_LESS:         return BGFX_STENCIL_TEST_LESS;
        case D3DCMP_EQUAL:        return BGFX_STENCIL_TEST_EQUAL;
        case D3DCMP_LESSEQUAL:    return BGFX_STENCIL_TEST_LEQUAL;
        case D3DCMP_GREATER:      return BGFX_STENCIL_TEST_GREATER;
        case D3DCMP_NOTEQUAL:     return BGFX_STENCIL_TEST_NOTEQUAL;
        case D3DCMP_GREATEREQUAL: return BGFX_STENCIL_TEST_GEQUAL;
        case D3DCMP_ALWAYS: default: return BGFX_STENCIL_TEST_ALWAYS;
    }
}

// TheSuperHackers @refactor bobtista 20/04/2026 Map a D3DSTENCILOP (1..8) to the bgfx stencil op ordinal, then shift into PASS_Z / FAIL_S / FAIL_Z bit positions.
static uint32_t MapStencilOpToBgfx(int op, uint32_t shift)
{
    uint32_t ord;
    switch (op)
    {
        case D3DSTENCILOP_KEEP:    ord = 1; break;
        case D3DSTENCILOP_ZERO:    ord = 0; break;
        case D3DSTENCILOP_REPLACE: ord = 2; break;
        case D3DSTENCILOP_INCRSAT: ord = 4; break;
        case D3DSTENCILOP_DECRSAT: ord = 6; break;
        case D3DSTENCILOP_INVERT:  ord = 7; break;
        case D3DSTENCILOP_INCR:    ord = 3; break;
        case D3DSTENCILOP_DECR:    ord = 5; break;
        default:                   ord = 1; break;
    }
    return ord << shift;
}

static void UpdateShadowStencilState()
{
    if (!g_draw.stencilEnabled)
    {
        g_draw.shadowStencilFront = BGFX_STENCIL_NONE;
        g_draw.shadowStencilBack  = BGFX_STENCIL_NONE;
        return;
    }
    g_draw.shadowStencilFront = g_draw.stencilFuncBits
        | BGFX_STENCIL_FUNC_REF(g_draw.stencilRef & 0xFF)
        | BGFX_STENCIL_FUNC_RMASK(g_draw.stencilReadMask & 0xFF)
        | g_draw.stencilFailOpBits
        | g_draw.stencilZFailOpBits
        | g_draw.stencilPassOpBits;
    g_draw.shadowStencilBack = BGFX_STENCIL_NONE;
}

// Sway table: 11 entries (no-sway at index 0, MAX_SWAY_TYPES=10 active).

// Sampler uniform shared by all textured fragment shaders. Bound to stage 0.
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.9 material
// diffuse uniform. Carries the VertexMaterialClass::Get_Diffuse color
// + opacity from Set_Material into the fragment shader so team colors
// (which the W3D engine writes into the material diffuse channel) and
// alpha fades modulate the output.
// TheSuperHackers @feature bobtista 20/04/2026 Material emissive uniform.
// D3D8 fixed-function adds the material's emissive color to the lit
// output (D3DRS_EMISSIVEMATERIALSOURCE, MATERIAL.Emissive). Self-
// illuminating meshes like the "move here" hint rely on this to produce
// their glow color even when no light reaches them. fs_uber needs this
// term or such meshes render black.
// Grayscale output override for disabled 2D UI elements (Render2DClass::Enable_Grayscale).
// D3D8 path uses D3DTOP_DOTPRODUCT3 + TFACTOR=0x80A5CA8E (luminance weights); bgfx path
// applies the dot-product at the end of fs_uber when g_draw.grayscaleEnable[0] > 0.5.
// Alpha-test parameters consumed by fs_textured_lit_atest. .x is the
// reference threshold in [0, 1]; engine writes ShaderClass alpha-ref / 255.
// Named u_atestParams (not u_alphaRef) to avoid bgfx_shader.sh's internal
// u_alphaRef4 conflict. See fs_textured_lit_atest.sc for details.

// Phase 5A TSS operation uniforms. Encode the DX8 texture stage state
// operations so the uber fragment shader can evaluate them at runtime.

// Post-ShaderClass blend/alpha-test overrides. Set by Override_Blend /
// Override_Alpha_Test, cleared by Clear_State_Overrides (called from Set_Shader).
// TheSuperHackers @feature bobtista 16/04/2026 Phase 4L 2D overlay active flag.
// Set by Set_View_Identity (Render2DClass enters 2D mode), cleared by
// Set_Transform(VIEW) when a real camera view is restored or at Begin_Scene.

// UV set selection: when > 0, the fragment shader samples stage 0
// from v_texcoord1 instead of v_texcoord0. Set by the terrain shader
// when it changes D3DTSS_TEXCOORDINDEX to 1 for the blend pass.

// Shroud pass: the D3D8 path uses D3DTSS_TCI_CAMERASPACEPOSITION to auto-
// generate UVs from camera-space vertex positions, combined with a texture
// transform matrix. bgfx has no equivalent, so we upload the full texture
// matrix and let the vertex shader compute the UVs explicitly.

// TheSuperHackers @feature bobtista 20/04/2026 Cloud-shadow modulation
// for terrain (DX8 ST_TERRAIN_BASE_NOISE1 / _NOISE12). Scroll offset
// advances each frame from TerrainShader2Stage::m_xOffset/m_yOffset;
// the fragment shader multiplies the sampled cloud color into the base.
// .xy = scroll offset, .z = world→UV stretch, .w > 0.5 = enable.
// .y = terrain blend flag: when > 0, shader does lerp(tex0, tex1, vertex_alpha)
// using UV set 0 for tex0 and UV set 1 for tex1

// Phase 5B: lighting uniforms. The engine supports up to 4 lights
// (typically 1 directional sun + 0-3 point lights). We pack light data
// into vec4 arrays and push them per-draw so the uber fragment shader
// can evaluate real N.L lighting instead of the hardcoded fake sun.
// 4 lights packed into vec4 arrays (one element per light).
// u_lightDirs[i].xyz = direction toward light, .w = enabled flag
// u_lightColors[i].rgb = diffuse color
// Defaults match the old hardcoded sun: direction TOWARD light (positive),
// white diffuse, reasonable ambient. These are used until the first
// Set_Light_Environment call provides real game lights.
// Phase 5B: tracks whether the current material has lighting enabled.
// When false, the vertex color contains pre-baked lighting (terrain)
// and the fragment shader should NOT apply N.L lighting on top.

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4F.1 default 1x1
// white texture. Real Set_Texture wiring is not in place yet, so the
// textured shaders need SOMETHING bound to s_tex0 or D3D11 returns
// undefined values. A 1x1 opaque white texture is a sensible default
// because the fragment shader does texColor * v_color0 - white * vc
// gives the vertex color through unmodified.
// Track which engine textures are render targets so we can use the
// transparent fallback instead of white.

// Vertex layout used by the test triangle. Position + packed RGBA color.
// Initialized in Initialize since bgfx::VertexLayout::begin needs bgfx to be
// up and running (it queries the active renderer for the pos type).

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.1 standard
// vertex layouts. One per common FVF format. Initialized in Initialize,
// used by Phase 4C.2's vertex buffer creation path. Names follow the FVF
// tags - P=position, N=normal, D=diffuse (color0), T<n>=texcoord<n>.

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
{
    WWDEBUG_SAY(("[BgfxBackend] Backend constructed."));
}

BgfxBackend::~BgfxBackend()
{
}

// -- Backend lifecycle -------------------------------------------------------

namespace
{
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.2 ShaderClass
// translation table. Maps a ShaderClass instance to (program handle,
// bgfx state bits). Defined but not yet called by any draw path - the
// wiring lands in a later session along with the per-stage TSS uber-shader.
//
// Mapping rules:
//   - alpha-test enabled & textured & lit -> g_texturedLitAtestProgram
//   - textured & lit                      -> g_texturedLitProgram
//   - textured & !lit                     -> g_texturedUnlitProgram
//   - !textured & lit                     -> g_solidLitProgram
//   - else                                -> g_device.passthroughProgram (debug)
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

// Default D3D8 alpha-test reference (0x60/255 = 0.376) used when a shader has ALPHATEST enabled without an explicit reference. Matches the implicit D3D8 behavior preserved by ShaderClass presets.
const float kDefaultAlphaTestRef = 0x60 / 255.0f;

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
                secColorOp  = kTssBlendCurAlpha;
                secCArg1Src = kTssArgTexture;
                break;
            case ShaderClass::DETAILCOLOR_DETAILBLEND:
                secColorOp  = kTssBlendTexAlpha;
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
        *atestRef = kDefaultAlphaTestRef;
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
// TheSuperHackers @fix bobtista 19/04/2026 Track D3D8 texture pointers
// and dimensions alongside TextureClass* to detect stale cache entries
// (address reuse) and enable in-place updates for same-sized textures.
// TheSuperHackers @fix bobtista 19/04/2026 Deferred texture destruction.
// When a stale texture cache entry is detected (D3D8 pointer changed), the
// old bgfx handle can't be destroyed immediately because in-flight draws
// may still reference it. Double-buffer: collect in current frame, destroy
// after the NEXT bgfx::frame() (2 frames later = guaranteed safe).

// The bgfx texture currently bound to stage 0 by Set_Texture. Used by
// SubmitEngineDraw - falls back to g_device.defaultWhiteTexture if invalid.

// Per-stage sampler flags captured from the source TextureClass's
// Get_U/V_Addr_Mode in Set_Texture. Default 0 = use bgfx's creation-time
// default (usually linear filter + wrap). Shoreline LUT needs CLAMP
// because its U coord can exceed [0,1] and WRAP produces a visible
// stripe/checker artifact at the boundary.

// The most recent buffers and offsets cached from Set_Vertex_Buffer /
// Set_Index_Buffer. Read by Draw_Triangles when it issues the bgfx
// submit. Cleared (made invalid) on Shutdown.

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.2 transient
// (dynamic) buffer state. Capture_Dynamic_Vertex_Data allocs a bgfx
// transient VB and records the owning DynamicVBAccessClass pointer so
// the matching Set_Vertex_Buffer(DynamicVBAccessClass&) call can claim
// it. The transient buffers are auto-freed at bgfx::frame time; we
// only track validity within the current frame.
// Current draw call uses transient buffers if these are set. They
// shadow the static VB/IB handles above - SubmitEngineDraw picks the
// transient path when these are true.

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

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.12 dedicated
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
// Frames to wait before showing the DX8 reference popup. The game's input system and shell menu need a beat after device init to fully settle; popping the ref window earlier steals focus and blocks mouse capture. ~0.5s at 60fps.
const int kDX8RefWindowShowDelayFrames   = 30;

// Render-to-texture state. Set by Set_Render_Target_With_Z, cleared
// when the back buffer is restored. SubmitEngineDraw routes to
// kBgfxRTTView while this is true.

// True between Begin_Sorted_Batch_Pass and End_Sorted_Batch_Pass;
// SubmitEngineDraw routes to kBgfxEngineSortView and uses
// g_frame.sortWorld while this is set.

// Per-batch effective world for sorted draws: the pre-multiplied
// sortView * sortWorld (in bgfx column-major form) captured from the
// engine's render_state by Capture_Sorted_Batch_Transforms.
// Phase 4I.2 CSM: raw model-to-world matrix for sorted draws, WITHOUT
// the camera view baked in. Used for shadow caster submissions where
// the light's view+proj replaces the camera's. g_frame.sortWorld has
// model*cameraView which contaminates the light-space transform.

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.13 Set by
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
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4F.2 texture
// capture. Unlike vertex buffers, W3D textures default to POOL_MANAGED,
// which is safe to lock with D3DLOCK_READONLY on the Intel UHD driver.
// We can read the source d3d8 texture data on demand from inside
// Set_Texture without an engine-side write hook. POOL_DEFAULT textures
// (render targets, dynamic textures) are skipped to avoid the same
// corruption that hit vertex buffers.

} // end anonymous namespace (helpers moved to BgfxBackendTextures.cpp)


namespace { // reopen anonymous namespace


// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I.2 CSM light
// transform computation. Called once per frame to build an ortho
// projection from a hardcoded sun direction, fitted to a generous
// world-space box centered on the tactical view. Stores results in
// g_frame.shadowLightView / g_frame.shadowLightProj (bgfx column-major), then
// pushes them to the shadow map view. Not yet tied to the engine's
// actual sun direction — Phase 4I.2 Session D hooks that up.
static const float kSunDistanceFromGround = 10000.0f;

static void UpdateShadowLightTransform()
{
    // The engine's shadow sun position (from W3DShadowManager) is
    // normalize(-terrainLightPos) * kSunDistanceFromGround.
    // Extract the DIRECTION by normalizing, then place the light eye
    // at a reasonable distance along that direction from the look-at.
    // Read the sun direction from the D3D device's light 0 each frame.
    // This ensures the CSM matches the scene lighting even when
    // Set_Shadow_Light_Position isn't called (e.g., save game loads
    // that restore TOD without going through W3DTerrainLogic::loadMap).
    // The D3D light direction points FROM sun TOWARD ground; negate
    // to get the direction TOWARD the sun (matching the convention
    // used by g_frame.shadowSunPosX/Y/Z from setLightPosition).
    float sunX = g_frame.shadowSunPosX;
    float sunY = g_frame.shadowSunPosY;
    float sunZ = g_frame.shadowSunPosZ;
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
                sunX = lx * (kSunDistanceFromGround / ll);
                sunY = ly * (kSunDistanceFromGround / ll);
                sunZ = lz * (kSunDistanceFromGround / ll);
            }
        }
    }
    if (sunX == 0.0f && sunY == 0.0f && sunZ == 0.0f)
    {
        sunX = 500.0f; sunY = 800.0f; sunZ = 1500.0f;
    }
    const float sunLen = std::sqrt(sunX*sunX + sunY*sunY + sunZ*sunZ);
    if (sunLen < 0.001f)
    {
        return;
    }
    // Normalized direction FROM origin TOWARD sun.
    const float sdx = sunX / sunLen;
    const float sdy = sunY / sunLen;
    const float sdz = sunZ / sunLen;

    // Extract camera position from the captured view matrix.
    float viewTx = g_frame.view[12];
    float viewTy = g_frame.view[13];
    float viewTz = g_frame.view[14];
    float camPosX = -(g_frame.view[0]*viewTx + g_frame.view[1]*viewTy + g_frame.view[2]*viewTz);
    float camPosY = -(g_frame.view[4]*viewTx + g_frame.view[5]*viewTy + g_frame.view[6]*viewTz);
    float camPosZ = -(g_frame.view[8]*viewTx + g_frame.view[9]*viewTy + g_frame.view[10]*viewTz);

    // Compute where the camera looks at on the ground plane (Z=0).
    // The 3rd row of the bgfx view matrix (column-major: indices
    // 2, 6, 10) is the view-space Z axis in world coordinates.
    // For this engine (Z-up, DX8 left-handed), it points BACKWARD
    // (away from the scene), so negate to get FORWARD (toward scene).
    float fwdX = -g_frame.view[2];
    float fwdY = -g_frame.view[6];
    float fwdZ = -g_frame.view[10];
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
    bx::mtxLookAt(g_frame.shadowLightView,
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
    bx::mtxOrtho(g_frame.shadowLightProj,
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
        bx::mtxMul(lightVP, g_frame.shadowLightView, g_frame.shadowLightProj);
        // Row-vector: (0,0,0,1) * VP = (VP[12], VP[13], VP[14], VP[15])
        // VP[12..13] are the clip-space position of the world origin.
        const float halfRes = static_cast<float>(kShadowMapResolution) * 0.5f;
        const float texelX = lightVP[12] * halfRes;
        const float texelY = lightVP[13] * halfRes;
        const float snappedX = std::round(texelX);
        const float snappedY = std::round(texelY);
        const float offsetX = (snappedX - texelX) / halfRes;
        const float offsetY = (snappedY - texelY) / halfRes;
        g_frame.shadowLightProj[12] += offsetX;
        g_frame.shadowLightProj[13] += offsetY;
    }

    bgfx::setViewTransform(kBgfxShadowMapView, g_frame.shadowLightView, g_frame.shadowLightProj);
    g_frame.shadowLightCaptured = true;
}

}

void BgfxBackend::Initialize(void * hwnd, int /*width*/, int /*height*/)
{
    if (g_device.initialized)
    {
        WWDEBUG_SAY(("[BgfxBackend] Initialize called twice; ignoring."));
        return;
    }

    // TheSuperHackers @feature bobtista 16/04/2026 Phase 4K. bgfx takes the
    // main game window; DX8 moves to a secondary popup for reference.
    g_device.window = static_cast<HWND>(hwnd);
    if (g_device.window == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] hwnd is null. Backend will remain dormant."));
        return;
    }

    RECT clientRect;
    if (GetClientRect(g_device.window, &clientRect))
    {
        g_device.width  = clientRect.right  - clientRect.left;
        g_device.height = clientRect.bottom - clientRect.top;
    }
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
    pd.nwh = g_device.window;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    bgfx::setPlatformData(pd);

    bgfx::Init initArgs;
    initArgs.type = bgfx::RendererType::Count;
    initArgs.callback = &g_bgfxCallback;
    initArgs.resolution.width = static_cast<uint32_t>(g_device.width);
    initArgs.resolution.height = static_cast<uint32_t>(g_device.height);
    initArgs.resolution.reset = BGFX_RESET_NONE;
    initArgs.platformData = pd;

    if (!bgfx::init(initArgs))
    {
        WWDEBUG_SAY(("[BgfxBackend] bgfx::init FAILED. Backend will remain dormant."));
        g_device.window = nullptr;
        return;
    }

    g_device.initialized = true;

    // TheSuperHackers @refactor bobtista 16/04/2026 Phase 4K. The explicit
    // bgfx::reset() after init is removed because it triggers a DXGI
    // assertion when bgfx owns the main game HWND. The init call already
    // configured the resolution and format correctly.

    // Configure view 0 to clear the debug window to a dark teal so it's
    // visually obvious bgfx is running and alive. View 0 holds the test
    // triangle (Phase 4B sentinel).
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

    // Phase 4I shadow-volume view. Sequential so the two-pass algorithm
    // (front INCR / back DECR) runs in submit order. No clear. View
    // transform is pushed per-frame from the engine camera via the
    // dirty-flag logic alongside view 1.
    bgfx::setViewClear(kBgfxShadowVolumeView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kBgfxShadowVolumeView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));
    bgfx::setViewMode(kBgfxShadowVolumeView, bgfx::ViewMode::Sequential);

    // Phase 4I shadow darken apply pass. Sequential, identity transforms
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

    // Phase 4L UI overlay view. Sequential mode preserves draw order for
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

    // Default the cached transforms to identity until the engine writes
    // real values via Set_Transform. This keeps the first few engine
    // submits well-defined even if they fire before any matrices are
    // captured.
    IdentityMatrix(g_frame.world);
    IdentityMatrix(g_frame.view);
    IdentityMatrix(g_frame.proj);
    IdentityMatrix(g_frame.sortWorld);
    g_frame.cameraProjDirty = true;

    // Sort view gets identity view + current projection. setViewTransform
    // persists for the life of the bgfx view; we re-apply the projection
    // in Set_Projection_Transform_With_Z_Bias whenever it changes.
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_frame.proj);
    }

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.3 create the
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
        vs_passthrough_dx11, sizeof(vs_passthrough_dx11), "vs_passthrough",
        fs_passthrough_dx11, sizeof(fs_passthrough_dx11), "fs_passthrough");

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
    g_uniforms.uMatDiffuse  = bgfx::createUniform("u_matDiffuse",  bgfx::UniformType::Vec4);
    g_uniforms.uMatEmissive = bgfx::createUniform("u_matEmissive", bgfx::UniformType::Vec4);
    g_uniforms.uAtestParams = bgfx::createUniform("u_atestParams", bgfx::UniformType::Vec4);
    g_uniforms.uTssOps0     = bgfx::createUniform("u_tssOps0",     bgfx::UniformType::Vec4);
    g_uniforms.uTssOps1     = bgfx::createUniform("u_tssOps1",     bgfx::UniformType::Vec4);
    g_uniforms.uLightDirs   = bgfx::createUniform("u_lightDirs",   bgfx::UniformType::Vec4, 4);
    g_uniforms.uLightColors = bgfx::createUniform("u_lightColors", bgfx::UniformType::Vec4, 4);
    g_uniforms.uSceneAmbient  = bgfx::createUniform("u_sceneAmbient",   bgfx::UniformType::Vec4);
    g_uniforms.uLightingEnabled = bgfx::createUniform("u_lightingEnabled", bgfx::UniformType::Vec4);
    g_uniforms.uTexcoordSelect  = bgfx::createUniform("u_texcoordSelect",  bgfx::UniformType::Vec4);
    g_uniforms.uTexcoordSelect2 = bgfx::createUniform("u_texcoordSelect2", bgfx::UniformType::Vec4);
    g_uniforms.uTexcoordSource  = bgfx::createUniform("u_texcoordSource",  bgfx::UniformType::Vec4);
    g_uniforms.uVertexColorFlags = bgfx::createUniform("u_vertexColorFlags", bgfx::UniformType::Vec4);
    g_uniforms.uGrayscaleEnable = bgfx::createUniform("u_grayscaleEnable", bgfx::UniformType::Vec4);
    g_uniforms.uShroudParams = bgfx::createUniform("u_shroudParams", bgfx::UniformType::Vec4);
    g_uniforms.uCloudParams  = bgfx::createUniform("u_cloudParams",  bgfx::UniformType::Vec4);
    g_uniforms.uTexTransform0 = bgfx::createUniform("u_texTransform0", bgfx::UniformType::Vec4);
    g_uniforms.uTexTransform1 = bgfx::createUniform("u_texTransform1", bgfx::UniformType::Vec4);
    g_uniforms.uTex1Transform0 = bgfx::createUniform("u_tex1Transform0", bgfx::UniformType::Vec4);
    g_uniforms.uTex1Transform1 = bgfx::createUniform("u_tex1Transform1", bgfx::UniformType::Vec4);
    g_uniforms.sCloudMap     = bgfx::createUniform("s_cloudMap",     bgfx::UniformType::Sampler);

    // Default 1x1 white texture. Used as fallback for missing textures.
    // Multiplying by white is the identity operation.
    static const uint8_t kWhitePixel[4] = { 0xff, 0xff, 0xff, 0xff };
    g_device.defaultWhiteTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWhitePixel, sizeof(kWhitePixel)));
    // Water fallback for render target textures (water reflections).
    // Semi-opaque dark blue simulates the water surface so the hull below
    // is partially hidden and water ripple particles blend naturally.
    // RGBA: (30, 50, 70, 180) = dark blue-grey, ~70% opaque.
    static const uint8_t kWaterPixel[4] = { 0x1e, 0x32, 0x46, 0xb4 };
    g_device.defaultTransparentTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWaterPixel, sizeof(kWaterPixel)));

    g_device.uberProgram = CreateShaderProgram(
        vs_uber_dx11, sizeof(vs_uber_dx11), "vs_uber",
        fs_uber_dx11, sizeof(fs_uber_dx11), "fs_uber");

    g_device.treeProgram = CreateShaderProgram(
        vs_trees_dx11, sizeof(vs_trees_dx11), "vs_trees",
        fs_uber_dx11, sizeof(fs_uber_dx11), "fs_uber");

    g_device.shadowVolumeProgram = CreateShaderProgram(
        vs_shadow_volume_dx11, sizeof(vs_shadow_volume_dx11), "vs_shadow_volume",
        fs_shadow_volume_dx11, sizeof(fs_shadow_volume_dx11), "fs_shadow_volume");

    g_device.shadowApplyProgram = CreateShaderProgram(
        vs_shadow_apply_dx11, sizeof(vs_shadow_apply_dx11), "vs_shadow_apply",
        fs_shadow_apply_dx11, sizeof(fs_shadow_apply_dx11), "fs_shadow_apply");
    g_uniforms.uShadowColor = bgfx::createUniform("u_shadowColor", bgfx::UniformType::Vec4);
    g_uniforms.uShadowBias  = bgfx::createUniform("u_shadowBias",  bgfx::UniformType::Vec4);

    g_device.shadowCasterProgram = CreateShaderProgram(
        vs_shadow_caster_dx11, sizeof(vs_shadow_caster_dx11), "vs_shadow_caster",
        fs_shadow_caster_dx11, sizeof(fs_shadow_caster_dx11), "fs_shadow_caster");

    // Shadow map depth render target. 1024x1024 D24 with compare-less
    // sampling for hardware PCF (sampler2DShadow in fs_uber).
    // Shadow map: D32F depth texture with comparison sampler for
    // hardware PCF (matches bgfx example 16). D32F gives full float
    // precision, eliminating shadow acne from depth quantization.
    const uint64_t depthFlags = BGFX_TEXTURE_RT
        | BGFX_SAMPLER_COMPARE_LEQUAL
        | BGFX_SAMPLER_U_CLAMP
        | BGFX_SAMPLER_V_CLAMP;
    g_device.shadowMapDepth = bgfx::createTexture2D(
        kShadowMapResolution, kShadowMapResolution, false, 1,
        bgfx::TextureFormat::D32F, depthFlags);
    if (bgfx::isValid(g_device.shadowMapDepth))
    {
        bgfx::setName(g_device.shadowMapDepth, "shadowMapD32F");
        g_device.shadowMapFB = bgfx::createFrameBuffer(1, &g_device.shadowMapDepth, true);
        // Clear both color (R32F=1.0 = far plane) and depth for the
        // caster pass's own depth test. 0x3f800000 = float 1.0 as RGBA.
        bgfx::setViewClear(kBgfxShadowMapView,
                           BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           0xffffffffu,  // RGBA all bits set ≈ max depth
                           1.0f, 0);
        bgfx::setViewRect(kBgfxShadowMapView, 0, 0, kShadowMapResolution, kShadowMapResolution);
        bgfx::setViewFrameBuffer(kBgfxShadowMapView, g_device.shadowMapFB);
        bgfx::setViewMode(kBgfxShadowMapView, bgfx::ViewMode::Sequential);
        // Pass inverse shadow map resolution to the shader via u_shadowParams.y
        // so the PCF kernel texel size stays in sync with the texture.
        g_draw.shadowParams[1] = 1.0f / static_cast<float>(kShadowMapResolution);

        WWDEBUG_SAY(("[CSM] shadow map FB=%u tex=%u valid=%d",
                     g_device.shadowMapFB.idx, g_device.shadowMapDepth.idx,
                     bgfx::isValid(g_device.shadowMapFB) ? 1 : 0));

        // Phase 4I.2 view ORDER: shadow map (view 8) MUST render before
        // the engine view (view 1) samples it in fs_uber. By default
        // bgfx renders views in view-id order (0, 1, 2, ...) — view 8
        // would run AFTER view 1, and the scene would sample empty /
        // stale shadow map. Explicit order: shadow map first, then the
        // rest of the views in their natural ascending order.
        const bgfx::ViewId order[] = {
            kBgfxDebugView,           // 0 — full-canvas clear quad, MUST run first
            kBgfxShadowMapView,       // 8 — shadow caster depth pass
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

    g_uniforms.uShadowLightViewProj = bgfx::createUniform("u_shadowLightViewProj",
                                                 bgfx::UniformType::Mat4);
    g_uniforms.uShadowParams        = bgfx::createUniform("u_shadowParams",
                                                 bgfx::UniformType::Vec4);
    g_uniforms.sShadowMap           = bgfx::createUniform("s_shadowMap",
                                                 bgfx::UniformType::Sampler);

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
    if (g_device.initialized)
    {
        DestroyBgfxHandle(g_device.passthroughProgram);
        DestroyBgfxHandle(g_device.fullscreenClearVB);
        DestroyBgfxHandle(g_device.uberProgram);
        DestroyBgfxHandle(g_device.treeProgram);
        DestroyBgfxHandle(g_uniforms.uSwayTable);
        DestroyBgfxHandle(g_uniforms.uShroudOffset);
        DestroyBgfxHandle(g_uniforms.uShroudScale);
        DestroyBgfxHandle(g_uniforms.sTex0);
        DestroyBgfxHandle(g_uniforms.sTex1);
        DestroyBgfxHandle(g_uniforms.sTex2);
        DestroyBgfxHandle(g_uniforms.sTex3);
        DestroyBgfxHandle(g_uniforms.uMatDiffuse);
        DestroyBgfxHandle(g_uniforms.uAtestParams);
        DestroyBgfxHandle(g_uniforms.uTssOps0);
        DestroyBgfxHandle(g_uniforms.uTssOps1);
        DestroyBgfxHandle(g_uniforms.uLightDirs);
        DestroyBgfxHandle(g_uniforms.uLightColors);
        DestroyBgfxHandle(g_uniforms.uSceneAmbient);
        DestroyBgfxHandle(g_uniforms.uLightingEnabled);
        DestroyBgfxHandle(g_uniforms.uTexcoordSelect);
        DestroyBgfxHandle(g_uniforms.uTexcoordSelect2);
        DestroyBgfxHandle(g_uniforms.uTexcoordSource);
        DestroyBgfxHandle(g_uniforms.uVertexColorFlags);
        DestroyBgfxHandle(g_uniforms.uShroudParams);
        DestroyBgfxHandle(g_uniforms.uCloudParams);
        DestroyBgfxHandle(g_uniforms.uTexTransform0);
        DestroyBgfxHandle(g_uniforms.uTexTransform1);
        DestroyBgfxHandle(g_uniforms.uTex1Transform0);
        DestroyBgfxHandle(g_uniforms.uTex1Transform1);
        DestroyBgfxHandle(g_uniforms.sCloudMap);
        DestroyBgfxHandle(g_device.shadowVolumeProgram);
        DestroyBgfxHandle(g_device.shadowApplyProgram);
        DestroyBgfxHandle(g_device.shadowCasterProgram);
        DestroyBgfxHandle(g_device.shadowMapFB);
        DestroyBgfxHandle(g_device.shadowMapDepth);
        DestroyBgfxHandle(g_uniforms.uShadowColor);
        DestroyBgfxHandle(g_uniforms.uShadowBias);
        DestroyBgfxHandle(g_uniforms.uShadowLightViewProj);
        DestroyBgfxHandle(g_uniforms.uShadowParams);
        DestroyBgfxHandle(g_uniforms.sShadowMap);
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
        // Phase 4C.3 cached engine buffers. Destroy before bgfx::shutdown
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
        g_draw.vb         = BGFX_INVALID_HANDLE;
        g_draw.ib         = BGFX_INVALID_HANDLE;
        g_draw.tex[0]   = BGFX_INVALID_HANDLE;
        g_draw.tex[1]   = BGFX_INVALID_HANDLE;
        g_draw.tex[2]   = BGFX_INVALID_HANDLE;
        g_draw.tex[3]   = BGFX_INVALID_HANDLE;
        g_draw.ibOffset       = 0;
        g_draw.useTransientVB = false;
        g_draw.useTransientIB = false;
        g_draw.pendingVB.valid    = false;
        g_draw.pendingIB.valid    = false;
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
        bgfx::shutdown();
        g_device.initialized = false;
        WWDEBUG_SAY(("[BgfxBackend] bgfx::shutdown complete."));
    }

    // Phase 4K: bgfx window is the main game window, do not destroy it.
    // DX8's secondary reference window is owned by DX8Wrapper.
    g_device.window = nullptr;
}

// Device state queries (Is_Device_Lost / Has_Stencil / Get_Back_Buffer_Format
// / Get_Back_Buffer / Set_Gamma) are inherited from DX8Backend.

// -- Viewport ----------------------------------------------------------------

void BgfxBackend::Set_Viewport(const RenderBackendViewport & viewport)
{
    // Do NOT call DX8Backend::Set_Viewport here — this method is called
    // FROM DX8Wrapper::Set_Viewport, so the D3D8 viewport is already set.
    // Calling the base class would cause infinite recursion.

    if (!g_device.initialized)
        return;

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
    bgfx::setViewRect(kBgfxRTTView, x, y, w, h);
}

// -- Frame lifecycle ---------------------------------------------------------

void BgfxBackend::Begin_Scene()
{
    if (!g_device.initialized)
    {
        return;
    }

    // Destroy PREVIOUS frame's deferred textures. These were queued in
    // frame N-1, survived through bgfx::frame() at End_Scene of frame N-1,
    // so all in-flight draws referencing them are guaranteed complete.
    for (auto & h : g_caches.deferredDestroysPrev)
    {
        if (bgfx::isValid(h))
            bgfx::destroy(h);
    }
    g_caches.deferredDestroysPrev.clear();
    // Show the DX8 reference popup after a few frames, giving the game's
    // input system time to fully initialize. Showing too early steals focus
    // and permanently blocks mouse capture.
    {
        static int s_dx8RefFrameCount = 0;
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

    // Check if the game window was resized (e.g., by Set_Render_Device) and
    // update bgfx's swapchain to match. Without this, bgfx renders at the
    // old resolution while the game expects the new one.
    if (g_device.window)
    {
        RECT cr;
        if (GetClientRect(g_device.window, &cr))
        {
            int w = cr.right - cr.left;
            int h = cr.bottom - cr.top;
            if (w > 0 && h > 0 && (w != g_device.width || h != g_device.height))
            {
                WWDEBUG_SAY(("[BgfxBackend] Window resized %dx%d -> %dx%d, calling bgfx::reset.",
                             g_device.width, g_device.height, w, h));
                g_device.width = w;
                g_device.height = h;
                bgfx::reset(g_device.width, g_device.height, BGFX_RESET_NONE);
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
    bgfx::touch(kBgfxShadowMapView);
    bgfx::touch(kBgfxUIView);
    // Phase 4I.2 CSM: light transform updated lazily at first draw
    // (see SubmitEngineDraw), not here, because g_frame.view is still
    // identity at Begin_Scene time.
    g_frame.shadowLightCaptured = false;
    g_views.overlay2DActive = false;
    // TheSuperHackers @fix bobtista 21/04/2026 Reset ALL 3D view rects to
    // full canvas at Begin_Scene, not just water view. Previously only the
    // water view was reset here, so when a frame did not call Set_Viewport
    // (e.g. the post-game shell map whose full-canvas camera hits the
    // isFullCanvas early-return below), the engine/sort/overlay/shadow
    // views retained the tactical rect (1280x640 excluding the HUD) from
    // the last in-game frame. Water view at full canvas composited over
    // an engine view that only wrote to the top 640 — the bottom 160 got
    // the debug view's alpha=1 clear and water blended opaque there via
    // DESTALPHA, while the top had low dest alpha from terrain writes
    // and water blended invisible. Matching reset keeps the views in sync.
    bgfx::setViewRect(kBgfxEngineView,        0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxEngineSortView,    0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxWaterView,         0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxEffectOverlayView, 0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxShadowVolumeView,  0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxShadowApplyView,   0, 0, g_device.width, g_device.height);
    bgfx::setViewRect(kBgfxRTTView,           0, 0, g_device.width, g_device.height);
    // No clear on water view — it composites over the opaque scene.
    bgfx::setViewClear(kBgfxWaterView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    g_views.renderToTexture = false;

    // TheSuperHackers @fix bobtista 21/04/2026 Reset the terrain-blend flag
    // at Begin_Scene. Clear_State_Overrides (called from Set_Shader)
    // deliberately preserves g_draw.texcoordSelect[1] because
    // Override_Terrain_Blend is invoked by the shader manager BEFORE
    // Set_Shader — clearing it there would wipe the flag every draw. But
    // nothing was actually resetting it per-frame, so if in-game terrain
    // rendering ever left it at 1.0 (e.g., on the final frame before a
    // map transition), it leaked across Begin_Scene and caused subsequent
    // water draws on the shell map to take the fs_uber terrain-blend path
    // (u_texcoordSelect.y > 0.5), which mixes s_tex0+s_tex1 with hardcoded
    // alpha=1 and discards pixels based on TSS alpha — producing large
    // missing-water areas on the post-game shellmap while DX8 rendered
    // correctly.
    g_draw.texcoordSelect[1] = 0.0f;

    // TheSuperHackers @bugfix bobtista 23/04/2026 Phase 5.2 — clear the
    // cached sampler bindings at the start of every frame. Without this,
    // whatever slot-0 texture the last frame's final 2D UI draw bound
    // (e.g. a font-atlas handle carrying debug-clock glyphs) persists
    // into the next frame's 3D passes. Any 3D draw that doesn't
    // explicitly call Set_Texture(0, ...) then samples the font atlas
    // as its base texture, producing a tiled-glyph pattern on terrain
    // and water (RenderDoc confirmed: fs_uber tex0 = TH 76, a 64x64
    // BGRA4 "20:31:56" clock atlas, during a water draw with thousands
    // of indices). BindTextureStages at submit time still falls back to
    // defaultWhiteTexture if a slot is invalid, so clearing here just
    // guarantees no stale handle survives the frame boundary.
    for (int i = 0; i < 4; ++i)
    {
        g_draw.tex[i] = BGFX_INVALID_HANDLE;
        g_draw.samplerFlags[i] = 0;
    }

    // TheSuperHackers @fix bobtista 21/04/2026 Reset transient view flags
    // defensively at Begin_Scene. Each flag has an intended begin/end pair
    // (Begin_Effect_Overlay/End_Effect_Overlay, Set_Shadow_Volume_Shader_Active
    // true/false, etc.), but engine code paths can skip the end call when a
    // map transition or early-exit interrupts a frame. Relying on the end
    // call matches the water-view-rect bug we just fixed: the next frame
    // inherits stuck state and renders incorrectly. overlay2DActive and
    // renderToTexture were already reset above; these match that policy.
    g_views.waterOverrideActive      = false;
    g_views.effectOverlayActive      = false;
    g_views.inSortFlush              = false;
    g_views.treeShaderActive         = false;
    g_views.shadowVolumeActive       = false;
    g_views.skipNextSubmitEngineDraw = false;
}

void BgfxBackend::End_Scene(bool /*flip_frame*/)
{
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
        g_frame.cameraCaptured = false;
    }
    if (g_frame.sortProjCaptured)
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_frame.sortProj);
        g_frame.sortProjCaptured = false;
    }

    // Phase 4L: push identity transforms and current rect to the UI view
    // so 2D overlay draws land in screen space over the 3D scene.
    {
        float identityMtx[16];
        IdentityMatrix(identityMtx);
        bgfx::setViewTransform(kBgfxUIView, identityMtx, identityMtx);
    }
    bgfx::setViewRect(kBgfxUIView, 0, 0,
                      static_cast<uint16_t>(g_device.width),
                      static_cast<uint16_t>(g_device.height));

    // Phase 4I.2: debug view (0) runs FIRST to emit the backbuffer
    // clear quad, then shadow map (view 8) so its depth texture is
    // populated before the scene samples it. Then RTT (3), engine
    // opaque (1), shadow volume fill (6), shadow darken (7), water (4),
    // sort (2), effect overlay (5), UI overlay (10) last.
    // TheSuperHackers @bugfix bobtista 20/04/2026 view 0 MUST be
    // included — when omitted, bgfx defers it to the end with a 1x1
    // viewport, and the full-canvas clear never fires (causing
    // flickering UI leftovers under the control bar on frames where
    // that area is not overdrawn).
    bgfx::ViewId viewOrder[] = {
        kBgfxDebugView,            // 0 — full-canvas clear quad, must run first
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

    // Rotate deferred texture destroy buffers. Current frame's deferred
    // handles move to "prev" — they'll be destroyed at the NEXT Begin_Scene
    // after one more bgfx::frame() guarantees all references are gone.
    g_caches.deferredDestroysPrev.insert(g_caches.deferredDestroysPrev.end(),
        g_caches.deferredDestroys.begin(), g_caches.deferredDestroys.end());
    g_caches.deferredDestroys.clear();

    // Transient buffers are freed at bgfx::frame time. Invalidate the
    // pending and current slots so nothing next frame tries to reuse
    // a dead handle.
    g_draw.pendingVB.valid    = false;
    g_draw.pendingIB.valid    = false;
    g_draw.useTransientVB = false;
    g_draw.useTransientIB = false;
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
    g_draw.useTransientVB = false;
    // TheSuperHackers @bugfix bobtista 27/04/2026 D3D8 supplies a white
    // diffuse color when the bound FVF has no COLOR0 element. bgfx
    // missing attributes read as zero, so tell the shader when it must
    // substitute the fixed-function default.
    g_draw.vertexColorFlags[0] = (vb != nullptr
        && (vb->FVF_Info().Get_FVF() & D3DFVF_DIFFUSE)) ? 1.0f : 0.0f;
    auto it = g_caches.vb.find(vb);
    if (it != g_caches.vb.end())
    {
        g_draw.vb = it->second.handle;
    }
    else
    {
        g_draw.vb = BGFX_INVALID_HANDLE;
        // On-demand capture for static VBs that were never captured
        // via WriteLockClass (e.g. rotor meshes loaded at startup).
        // Lock the D3D VB, copy the data into a bgfx dynamic VB, and
        // cache it for future use.
        if (vb != nullptr && g_device.initialized
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
                    auto it2 = g_caches.vb.find(vb);
                    if (it2 != g_caches.vb.end())
                    {
                        g_draw.vb = it2->second.handle;
                    }
                }
            }
        }
    }
}

void BgfxBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    DX8Backend::Set_Vertex_Buffer(vba);
    g_draw.vertexColorFlags[0] =
        (vba.FVF_Info().Get_FVF() & D3DFVF_DIFFUSE) ? 1.0f : 0.0f;
    // Phase 4G.2: if the matching Capture_Dynamic_Vertex_Data already
    // allocated a transient VB for this access class, claim it for the
    // next draw. Otherwise miss the cache and skip the bgfx submit.
    if (g_draw.pendingVB.valid && g_draw.pendingVB.owner == &vba)
    {
        g_draw.useTransientVB = true;
        g_draw.transientVB    = g_draw.pendingVB.tvb;
        g_draw.pendingVB.valid    = false;
    }
    else
    {
        g_draw.useTransientVB = false;
        g_draw.vb         = BGFX_INVALID_HANDLE;
    }
}

void BgfxBackend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(ib, index_base_offset);
    g_draw.useTransientIB = false;
    auto it = g_caches.ib.find(ib);
    if (it != g_caches.ib.end())
    {
        g_draw.ib = it->second.handle;
    }
    else
    {
        g_draw.ib = BGFX_INVALID_HANDLE;
        // On-demand capture for static IBs not yet in cache.
        if (ib != nullptr && g_device.initialized
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
                    auto it2 = g_caches.ib.find(ib);
                    if (it2 != g_caches.ib.end())
                    {
                        g_draw.ib = it2->second.handle;
                    }
                }
            }
        }
    }
    g_draw.ibOffset = index_base_offset;
}

void BgfxBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Backend::Set_Index_Buffer(iba, index_base_offset);
    if (g_draw.pendingIB.valid && g_draw.pendingIB.owner == &iba)
    {
        g_draw.useTransientIB = true;
        g_draw.transientIB    = g_draw.pendingIB.tib;
        g_draw.pendingIB.valid    = false;
    }
    else
    {
        g_draw.useTransientIB = false;
        g_draw.ib         = BGFX_INVALID_HANDLE;
    }
    g_draw.ibOffset = index_base_offset;
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
    g_draw.ibOffset = static_cast<unsigned short>(offset);
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

    auto it = g_caches.vb.find(vb);
    if (it != g_caches.vb.end())
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
    // Mismatched stride (typically for skinned-mesh FVFs like
    // D3DFVF_XYZB* that BuildBgfxLayoutForFVF does not fully cover)
    // would create a too-small bgfx buffer and cause truncation
    // warnings + D3D11 CreateBuffer crashes when the engine writes
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
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicVertexBufferHandle h = bgfx::createDynamicVertexBuffer(num_verts, layout);
    BgfxVbCacheEntry e{ h, num_verts, engine_stride };
    g_caches.vb[vb] = e;
    return h;
}

bgfx::DynamicIndexBufferHandle EnsureDynamicIndexBuffer(const IndexBufferClass * ib)
{
    const uint32_t num_indices = static_cast<uint32_t>(ib->Get_Index_Count());

    auto it = g_caches.ib.find(ib);
    if (it != g_caches.ib.end())
    {
        if (it->second.num_indices == num_indices && bgfx::isValid(it->second.handle))
        {
            return it->second.handle;
        }
        if (bgfx::isValid(it->second.handle))
        {
            bgfx::destroy(it->second.handle);
        }
        g_caches.ib.erase(it);
    }
    if (num_indices == 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicIndexBufferHandle h = bgfx::createDynamicIndexBuffer(num_indices);
    BgfxIbCacheEntry e{ h, num_indices };
    g_caches.ib[ib] = e;
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

void BgfxBackend::Capture_Vertex_Data(const VertexBufferClass * vb,
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
            WWDEBUG_SAY(("[BgfxBackend] skip VB full-capture: "
                         "size_bytes=%u stride=%u total=%u",
                         size_bytes, stride, buffer_bytes));
        }
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
            WWDEBUG_SAY(("[BgfxBackend] skip IB full-capture: "
                         "size_bytes=%u total=%u", size_bytes, buffer_bytes));
        }
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
            WWDEBUG_SAY(("[BgfxBackend] skip VB capture: stride=0 vb=%p", vb));
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
            WWDEBUG_SAY(("[BgfxBackend] skip VB capture: out-of-range "
                         "start_vert=%u size_bytes=%u stride=%u total=%u",
                         start_vertex, size_bytes, stride, buffer_bytes));
        }
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
            WWDEBUG_SAY(("[BgfxBackend] skip IB capture: out-of-range "
                         "start_idx=%u size_bytes=%u total=%u",
                         start_index, size_bytes, buffer_bytes));
        }
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
// g_frame.sortWorld; End flips it back. Capture computes the per-batch
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
            g_frame.sortWorld[r * 4 + c] = s;
            // Phase 4I.2: store raw model (no camera view baked in)
            // for shadow caster submissions.
        }
    }
    // Store raw sortWorld (model-to-world only) for CSM caster use.
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            g_frame.sortWorldRaw[r * 4 + c] = sortWorld[r][c];
        }
    }
}

void BgfxBackend::Capture_Sorted_Batch_Light(const RenderBackendLight & light, bool enabled)
{
    // Sort batch lights are always light 0 (the primary directional).
    // Scene ambient was already captured by Set_Light_Environment.
    // D3D8 direction points FROM light toward surface; negate for N.L.
    if (enabled)
    {
        g_draw.lightDirs[0][0] = -light.direction[0];
        g_draw.lightDirs[0][1] = -light.direction[1];
        g_draw.lightDirs[0][2] = -light.direction[2];
        g_draw.lightDirs[0][3] = 1.0f;
        g_draw.lightColors[0][0] = light.diffuse[0];
        g_draw.lightColors[0][1] = light.diffuse[1];
        g_draw.lightColors[0][2] = light.diffuse[2];
    }
    else
    {
        g_draw.lightDirs[0][3] = 0.0f;
    }
}

// TheSuperHackers @refactor bobtista 26/04/2026 Shared submit helpers used by
// both Submit_Sorted_Draw and SubmitEngineDraw to avoid duplicated blocks.
static uint64_t ApplyCullModeOverride(uint64_t state)
{
    unsigned d3dCull = DX8Wrapper::Get_DX8_Render_State(D3DRS_CULLMODE);
    state &= ~(BGFX_STATE_CULL_CW | BGFX_STATE_CULL_CCW);
    if (d3dCull == D3DCULL_CW)
    {
        state |= BGFX_STATE_CULL_CW;
    }
    else if (d3dCull == D3DCULL_CCW)
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

static void BindShadowMapTexture()
{
    if (bgfx::isValid(g_device.shadowMapDepth) && bgfx::isValid(g_uniforms.sShadowMap))
    {
        bgfx::setTexture(4, g_uniforms.sShadowMap, g_device.shadowMapDepth);
    }
}

static void BindTextureStages()
{
    // bgfx default (flags=0) is bilinear filtering. No explicit flags needed.
    if (bgfx::isValid(g_uniforms.sTex0))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_draw.tex[0]) ? g_draw.tex[0] : g_device.defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(0, g_uniforms.sTex0, bound, g_draw.samplerFlags[0]);
        }
    }
    if (bgfx::isValid(g_uniforms.sTex1))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_draw.tex[1]) ? g_draw.tex[1] : g_device.defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(1, g_uniforms.sTex1, bound, g_draw.samplerFlags[1]);
        }
    }
    if (bgfx::isValid(g_uniforms.sTex2))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_draw.tex[2]) ? g_draw.tex[2] : g_device.defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(2, g_uniforms.sTex2, bound, g_draw.samplerFlags[2]);
        }
    }
    if (bgfx::isValid(g_uniforms.sTex3))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_draw.tex[3]) ? g_draw.tex[3] : g_device.defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(3, g_uniforms.sTex3, bound, g_draw.samplerFlags[3]);
        }
    }
}

static float GetTexcoordSource(unsigned texcoordGen)
{
    if (texcoordGen == D3DTSS_TCI_CAMERASPACENORMAL)
    {
        return 1.0f;
    }
    if (texcoordGen == D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR)
    {
        return 2.0f;
    }
    if (texcoordGen == D3DTSS_TCI_CAMERASPACEPOSITION)
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
    D3DMATRIX texMtx;
    DX8Wrapper::_Get_DX8_Transform(
        static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), texMtx);
    row0[0] = texMtx.m[0][0];
    row0[1] = texMtx.m[1][0];
    row0[2] = texMtx.m[2][0];
    row0[3] = texMtx.m[3][0];
    row1[0] = texMtx.m[0][1];
    row1[1] = texMtx.m[1][1];
    row1[2] = texMtx.m[2][1];
    row1[3] = texMtx.m[3][1];
}

static void UpdateTextureTransforms()
{
    // TheSuperHackers @bugfix bobtista 25/04/2026 Honor material-stage texture
    // matrices in the bgfx uber shader. W3D atlas mappers animate tank
    // treads, bike wheels, and other sub-materials by setting
    // D3DTS_TEXTUREn plus D3DTTFF_COUNT2. Passing raw UVs sampled the
    // unused black padding in those atlases; stage 1 matters for detail
    // and environment-mapped sub-materials.
    const unsigned texcoordIndex =
        DX8Wrapper::Get_DX8_Texture_Stage_State(0, D3DTSS_TEXCOORDINDEX);
    const unsigned uvIndex = texcoordIndex & 0xFFFF;
    const unsigned texcoordGen = texcoordIndex & 0xFFFF0000;
    // TheSuperHackers @info bobtista 26/04/2026 Only UV sets 0 and 1 are
    // supported. D3D8 allows up to 8 UV sets via D3DTSS_TEXCOORDINDEX but
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

    const unsigned texFlags =
        DX8Wrapper::Get_DX8_Texture_Stage_State(0, D3DTSS_TEXTURETRANSFORMFLAGS);
    if ((texFlags & D3DTTFF_COUNT2) == D3DTTFF_COUNT2)
    {
        g_draw.texcoordSelect[3] = 1.0f;
        ReadTextureTransform(0, g_draw.texTransform0, g_draw.texTransform1);
    }
    else
    {
        g_draw.texcoordSelect[3] = 0.0f;
        SetIdentityTextureTransform(g_draw.texTransform0, g_draw.texTransform1);
    }

    const unsigned texcoordIndex1 =
        DX8Wrapper::Get_DX8_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX);
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

    const unsigned texFlags1 =
        DX8Wrapper::Get_DX8_Texture_Stage_State(1, D3DTSS_TEXTURETRANSFORMFLAGS);
    if ((texFlags1 & D3DTTFF_COUNT2) == D3DTTFF_COUNT2)
    {
        g_draw.texcoordSelect2[1] = 1.0f;
        ReadTextureTransform(1, g_draw.tex1Transform0, g_draw.tex1Transform1);
    }
    else
    {
        g_draw.texcoordSelect2[1] = 0.0f;
        SetIdentityTextureTransform(g_draw.tex1Transform0, g_draw.tex1Transform1);
    }
}

static void UploadMaterialUniforms()
{
    if (bgfx::isValid(g_uniforms.uMatDiffuse))
    {
        bgfx::setUniform(g_uniforms.uMatDiffuse, g_draw.matDiffuse);
        if (bgfx::isValid(g_uniforms.uMatEmissive))
            bgfx::setUniform(g_uniforms.uMatEmissive, g_draw.matEmissive);
    }

    if (bgfx::isValid(g_uniforms.uTssOps0))
    {
        bgfx::setUniform(g_uniforms.uTssOps0, g_draw.tssOps0);
    }
    if (bgfx::isValid(g_uniforms.uTssOps1))
    {
        bgfx::setUniform(g_uniforms.uTssOps1, g_draw.tssOps1);
    }
    if (bgfx::isValid(g_uniforms.uAtestParams))
    {
        const float effectiveAtestRef = g_overrides.atestActive ? g_overrides.atestRef : g_draw.atestRef;
        float atestParams[4] = { effectiveAtestRef, 0.0f, 0.0f, 0.0f };
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
    if (bgfx::isValid(g_uniforms.uGrayscaleEnable))
    {
        bgfx::setUniform(g_uniforms.uGrayscaleEnable, g_draw.grayscaleEnable);
    }
    if (bgfx::isValid(g_uniforms.uCloudParams))
    {
        bgfx::setUniform(g_uniforms.uCloudParams, g_draw.cloudParams);
    }
    if (bgfx::isValid(g_uniforms.uShadowParams))
    {
        bgfx::setUniform(g_uniforms.uShadowParams, g_draw.shadowParams);
    }
    if (bgfx::isValid(g_uniforms.uTexTransform0))
    {
        bgfx::setUniform(g_uniforms.uTexTransform0, g_draw.texTransform0);
    }
    if (bgfx::isValid(g_uniforms.uTexTransform1))
    {
        bgfx::setUniform(g_uniforms.uTexTransform1, g_draw.texTransform1);
    }
    if (bgfx::isValid(g_uniforms.uTex1Transform0))
    {
        bgfx::setUniform(g_uniforms.uTex1Transform0, g_draw.tex1Transform0);
    }
    if (bgfx::isValid(g_uniforms.uTex1Transform1))
    {
        bgfx::setUniform(g_uniforms.uTex1Transform1, g_draw.tex1Transform1);
    }
    if (bgfx::isValid(g_uniforms.sCloudMap) && bgfx::isValid(g_draw.cloudTex))
    {
        // WRAP addressing matches the DX8 cloud pass at W3DShaderManager.cpp:1742.
        bgfx::setTexture(5, g_uniforms.sCloudMap, g_draw.cloudTex, BGFX_SAMPLER_NONE);
    }
}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.13 sorted VB
// direct-draw submit. Called from DX8Wrapper::Draw_Sorting_IB_VB after
// it populates an internal dynamic VB and dynamic IB by copying a slice
// of the sorting VB/IB with the correct vba_offset / iba_offset /
// index_base_offset / min_vertex_index arithmetic. The engine's lock
// destructors already fired Capture_Dynamic_* for those inner buffers,
// so g_draw.pendingVB / g_draw.pendingIB hold their transients keyed by
// the exact access-class pointers we were just handed.
//
// We pull those transients into local draw state, emit a single bgfx
// submit to the sorted view with the inner buffers and remapped args
// (start_index=0, min_vertex_index=0, count relative to the inner
// buffers), and set g_views.skipNextSubmitEngineDraw so the outer Draw_Triangles
// does not emit a second, incorrect submit using the old sorting-VB
// args.

void BgfxBackend::Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                     const DynamicIBAccessClass & dyn_ib,
                                     unsigned short polygon_count,
                                     unsigned short vertex_count)
{
    if (!g_device.initialized)
    {
        return;
    }

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
        return;
    }

    const bgfx::TransientVertexBuffer vb = g_draw.pendingVB.tvb;
    const bgfx::TransientIndexBuffer  ib = g_draw.pendingIB.tib;
    g_draw.pendingVB.valid = false;
    g_draw.pendingIB.valid = false;

    if (!bgfx::isValid(g_draw.program))
    {
        g_views.skipNextSubmitEngineDraw = true;
        return;
    }

    // Sort view's view+proj were set up at init (identity view,
    // projection tracks opaque view via Set_Projection_Transform_With_Z_Bias).
    // World is the current g_frame.sortWorld if we are inside a sort batch,
    // otherwise the regular g_frame.world (rigid FVF category with sorting=true
    // has no batch-wrapped Apply_Render_State - it uses the per-mesh world
    // set by the caller via g_renderBackend->Set_Transform).
    const float * worldMtx = g_views.inSortFlush ? g_frame.sortWorld : g_frame.world;
    bgfx::setTransform(worldMtx);

    bgfx::setVertexBuffer(0, &vb, 0, vertex_count);
    bgfx::setIndexBuffer(&ib, 0, static_cast<uint32_t>(polygon_count) * 3);

    BindTextureStages();
    UpdateTextureTransforms();
    g_draw.shadowParams[0] = 0.0f;
    UploadMaterialUniforms();
    if (bgfx::isValid(g_uniforms.uTexcoordSelect))
    {
        bgfx::setUniform(g_uniforms.uTexcoordSelect, g_draw.texcoordSelect);
    }

    uint64_t state = (g_draw.state != 0)
        ? g_draw.state
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    state = ApplyCullModeOverride(state);
    state = ApplyColorWriteOverride(state);

    bgfx::setState(state);
    BindShadowMapTexture();

    bgfx::submit(kBgfxEngineSortView, g_draw.program);

    // Phase 4I.2 CSM: also submit sorted geometry as shadow caster.
    if (bgfx::isValid(g_device.shadowCasterProgram)
        && bgfx::isValid(g_device.shadowMapFB))
    {
        bgfx::setVertexBuffer(0, &vb);
        bgfx::setIndexBuffer(&ib, 0,
                             static_cast<uint32_t>(polygon_count) * 3);
        bgfx::setTransform(g_frame.sortWorldRaw);
        const uint64_t casterState =
            BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CW;
        bgfx::setState(casterState);
        bgfx::submit(kBgfxShadowMapView, g_device.shadowCasterProgram);
    }

    g_views.skipNextSubmitEngineDraw = true;
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
// own pointer in g_draw.pendingVB and claims the transient for the draw.

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
        g_draw.pendingVB.valid = false;
        return;
    }

    bgfx::allocTransientVertexBuffer(&g_draw.pendingVB.tvb, num_verts, layout);
    const uint32_t copy_bytes = num_verts * layout.getStride();
    const uint32_t bytes = (size_bytes < copy_bytes) ? size_bytes : copy_bytes;
    std::memcpy(g_draw.pendingVB.tvb.data, data, bytes);
    g_draw.pendingVB.owner = vba;
    g_draw.pendingVB.valid = true;

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
        g_draw.pendingIB.valid = false;
        return;
    }

    bgfx::allocTransientIndexBuffer(&g_draw.pendingIB.tib, num_indices);
    const uint32_t copy_bytes = num_indices * sizeof(uint16_t);
    const uint32_t bytes = (size_bytes < copy_bytes) ? size_bytes : copy_bytes;
    std::memcpy(g_draw.pendingIB.tib.data, data, bytes);
    g_draw.pendingIB.owner = iba;
    g_draw.pendingIB.valid = true;
}

// -- State: shaders, materials, textures ------------------------------------

void BgfxBackend::Set_Shader(const ShaderClass & shader)
{
    DX8Backend::Set_Shader(shader);
    g_draw.program = g_device.uberProgram;
    g_draw.state   = BuildBgfxStateForShader(shader);
    BuildTssOpsForShader(shader, g_draw.tssOps0, g_draw.tssOps1, &g_draw.atestRef);
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
        // TheSuperHackers @bugfix bobtista 20/04/2026 Honor
        // VertexMaterialClass's Diffuse_Color_Source. When set to COLOR1
        // (D3DMCS_COLOR1), D3D's FF pipeline uses the VERTEX diffuse as
        // the material's effective diffuse and ignores material->Get_Diffuse.
        // This is the PRELIT_DIFFUSE preset used by projected shadow decals,
        // HUD/2D overlays, etc. Without this guard, fs_uber's
        // `current *= u_matDiffuse` darkens the decals with whatever stale
        // or default color the material carries (often black), producing
        // the "black scorch-like patches" on the beach where alpha decals
        // are batched. Force matDiffuse=1 so vertex color passes through.
        Vector3 diffuse(1.0f, 1.0f, 1.0f);
        const VertexMaterialClass::ColorSourceType diffuseSource =
            const_cast<VertexMaterialClass *>(material)->Get_Diffuse_Color_Source();
        if (diffuseSource == VertexMaterialClass::MATERIAL)
        {
            material->Get_Diffuse(&diffuse);
        }
        g_draw.matDiffuse[0] = diffuse.X;
        g_draw.matDiffuse[1] = diffuse.Y;
        g_draw.matDiffuse[2] = diffuse.Z;
        g_draw.matDiffuse[3] = material->Get_Opacity();
        // Track whether this material uses hardware lighting. When false
        // (terrain, pre-lit meshes), vertex colors already contain the
        // lit result and the shader must NOT apply N.L on top.
        g_draw.lightingEnabled[0] = material->Get_Lighting() ? 1.0f : 0.0f;
        // Emissive color — self-illumination. Self-glow meshes (e.g.
        // SCMoveHint.w3d "move here" indicator) set the material emissive
        // to their glow color and rely on D3D fixed-function adding it
        // on top of the lit diffuse. Without this, they render black
        // because the lit math produces 0 and there's no emissive term.
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
        g_draw.matEmissive[0] = 0.0f;
        g_draw.matEmissive[1] = 0.0f;
        g_draw.matEmissive[2] = 0.0f;
        g_draw.matEmissive[3] = 0.0f;
        // Null material means no material-driven lighting. Dazzle and
        // similar effect overlays call Set_Material(nullptr) and bake
        // their per-vertex intensity into diffuse.rgb; the lit path in
        // fs_uber would ignore that baked color and output tex * lit,
        // producing bright flashes where the dazzle should be invisible.
        // Force the unlit path so vertex diffuse modulates the output.
        g_draw.lightingEnabled[0] = 0.0f;
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
            g_caches.renderTarget.count(texture) > 0)
        {
            h = g_device.defaultWhiteTexture;
            static bool s_loggedRTFallback = false;
            if (!s_loggedRTFallback)
            {
                s_loggedRTFallback = true;
                TextureClass * t2d_fb = texture->As_TextureClass();
                WWDEBUG_SAY(("[BgfxBackend] RT FALLBACK: stage=%u using white fallback for %s",
                             stage,
                             t2d_fb ? t2d_fb->Get_Full_Path().str() : "(null)"));
            }
        }
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
            case 0: g_draw.tex[0] = h;
                    g_draw.samplerFlags[0] = samplerFlags; break;
            case 1: g_draw.tex[1] = h;
                    g_draw.samplerFlags[1] = samplerFlags; break;
            case 2: g_draw.tex[2] = h;
                    g_draw.samplerFlags[2] = samplerFlags; break;
            case 3: g_draw.tex[3] = h;
                    g_draw.samplerFlags[3] = samplerFlags; break;
            default: break;
        }
    }
}

void BgfxBackend::Set_Ambient(const Vector3 & color)
{
    DX8Backend::Set_Ambient(color);
    g_draw.sceneAmbient[0] = color.X;
    g_draw.sceneAmbient[1] = color.Y;
    g_draw.sceneAmbient[2] = color.Z;
}

// Maps WW3D BlendFactor enum (1..8) to bgfx blend-factor bits. Index 0 unused.
static const uint64_t kBgfxBlendMap[9] = {
    0,
    BGFX_STATE_BLEND_ZERO,           // 1 = RB_BLEND_ZERO
    BGFX_STATE_BLEND_ONE,            // 2 = RB_BLEND_ONE
    BGFX_STATE_BLEND_SRC_COLOR,      // 3
    BGFX_STATE_BLEND_INV_SRC_COLOR,  // 4
    BGFX_STATE_BLEND_SRC_ALPHA,      // 5 = RB_BLEND_SRC_ALPHA
    BGFX_STATE_BLEND_INV_SRC_ALPHA,  // 6 = RB_BLEND_INV_SRC_ALPHA
    BGFX_STATE_BLEND_DST_ALPHA,      // 7 = RB_BLEND_DEST_ALPHA
    BGFX_STATE_BLEND_INV_DST_ALPHA   // 8 = RB_BLEND_INV_DEST_ALPHA
};

// TheSuperHackers @fix bobtista 20/04/2026 The DX8Backend default only updates D3D render state, leaving bgfx's g_overrides.blendBits stale. Water rendering relies on this to restore SRC_ALPHA / INV_SRC_ALPHA blending after its DESTALPHA shoreline pass — otherwise the DESTALPHA state set by Override_Material_Opacity() persists into the next draw (e.g. the faction-emblem quad on the command-center bib), painting it black.
void BgfxBackend::Set_Blend_Factors(BlendFactor src, BlendFactor dest)
{
    DX8Backend::Set_Blend_Factors(src, dest);
    const unsigned s = static_cast<unsigned>(src);
    const unsigned d = static_cast<unsigned>(dest);
    if (s >= 1 && s <= 8 && d >= 1 && d <= 8)
    {
        g_overrides.SetBlend(BGFX_STATE_BLEND_FUNC(kBgfxBlendMap[s], kBgfxBlendMap[d]));
    }
}

void BgfxBackend::Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend)
{
    const unsigned srcIdx = static_cast<unsigned>(srcBlend);
    const unsigned dstIdx = static_cast<unsigned>(dstBlend);
    if (srcIdx >= 1 && srcIdx <= 8 && dstIdx >= 1 && dstIdx <= 8)
    {
        g_overrides.SetBlend(BGFX_STATE_BLEND_FUNC(kBgfxBlendMap[srcIdx], kBgfxBlendMap[dstIdx]));
    }
    else
    {
        static bool s_loggedBlendBad = false;
        if (!s_loggedBlendBad)
        {
            s_loggedBlendBad = true;
            WWDEBUG_SAY(("[BgfxBackend] BLEND OVERRIDE BAD VALUES: src=%u dst=%u (out of range 1-8)",
                         srcIdx, dstIdx));
        }
    }
    // RB_BLEND_* enum values match D3DBLEND_* (verified in IRenderBackend.h comments), so the integer passed to D3D8 is correct.
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, srcIdx);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, dstIdx);
}

void BgfxBackend::Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func)
{
    g_overrides.atestActive = enable;
    g_overrides.atestRef = enable ? (ref / 255.0f) : 0.0f;
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, enable ? TRUE : FALSE);
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF, ref);
    // RB_CMP_* enum values match D3DCMP_*, so the integer passed to D3D8 is correct.
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC, static_cast<unsigned>(func));
}

void BgfxBackend::Override_Alpha_Blend_Enable(bool enable)
{
    if (enable)
    {
        g_overrides.SetBlend(BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                  BGFX_STATE_BLEND_INV_SRC_ALPHA));
    }
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void BgfxBackend::Override_Texcoord_Index(unsigned stage, unsigned uvIndex)
{
    if (stage == 0)
    {
        g_draw.texcoordSelect[0] = (uvIndex == 1) ? 1.0f : 0.0f;
    }
    DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, uvIndex);
}

void BgfxBackend::Override_Terrain_Blend(bool enable)
{
    g_draw.texcoordSelect[1] = enable ? 1.0f : 0.0f;
}

void BgfxBackend::Override_Material_Opacity(float opacity)
{
    // TheSuperHackers @fix bobtista 20/04/2026 Only override opacity.
    // Previously this also forced DESTALPHA blend as a bgfx-side stand-in
    // for the D3D8 shoreline-alpha water feather. That stale DESTALPHA +
    // matDiffuse.a=0.5 state leaked into the next draw (a small quad on
    // the command-center bib, captured in RenderDoc as a water-textured
    // draw producing pure black). The water code already sets DESTALPHA
    // explicitly via Set_Blend_Factors when soft water edge is enabled,
    // and BgfxBackend::Set_Blend_Factors now propagates that to bgfx. So
    // this override only needs to handle the opacity uniform.
    g_draw.matDiffuse[3] = opacity;
    g_views.waterOverrideActive = true;
}

void BgfxBackend::Begin_Effect_Overlay()
{
    g_views.effectOverlayActive = true;
}

void BgfxBackend::End_Effect_Overlay()
{
    g_views.effectOverlayActive = false;
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
    DX8Backend::Set_Color_Write_Enable(red, green, blue, alpha);
    uint64_t mask = 0;
    if (red)   mask |= BGFX_STATE_WRITE_R;
    if (green) mask |= BGFX_STATE_WRITE_G;
    if (blue)  mask |= BGFX_STATE_WRITE_B;
    if (alpha) mask |= BGFX_STATE_WRITE_A;
    g_overrides.colorWriteOverride = static_cast<int>(mask);
    g_overrides.suppressDraw = false;
}

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I mirror the
// DWORD variant into g_overrides.colorWriteOverride so stencil shadow volume
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
    g_overrides.colorWriteOverride = static_cast<int>(bgfxMask);
    g_overrides.suppressDraw = false;
}

void BgfxBackend::Skip_Next_Bgfx_Submit()
{
    g_views.skipNextSubmitEngineDraw = true;
}

// TheSuperHackers @fix bobtista 16/04/2026 Remove g_draw.matDiffuse aliasing from texture factor.
// The shadow decal draw is now skipped via Skip_Next_Bgfx_Submit so the aliasing
// is unnecessary and it clobbers team colors.
void BgfxBackend::Set_Texture_Factor(unsigned argb)
{
    DX8Backend::Set_Texture_Factor(argb);
}

void BgfxBackend::Set_Shadow_Light_Position(float x, float y, float z)
{
    g_frame.shadowSunPosX = x;
    g_frame.shadowSunPosY = y;
    g_frame.shadowSunPosZ = z;
    g_frame.shadowSunPosSet = true;
    g_frame.shadowLightCaptured = false;
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

    if (!g_device.initialized
        || !g_views.shadowVolumeActive
        || !bgfx::isValid(g_device.shadowVolumeProgram)
        || !bgfx::isValid(g_draw.vb)
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
    if (g_draw.stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECR
        || g_draw.stencilPassOpBits == BGFX_STENCIL_OP_PASS_Z_DECRSAT)
    {
        return;
    }

    // No face culling, two-sided stencil matching the side-wall submit.
    uint64_t state = BGFX_STATE_DEPTH_TEST_LEQUAL;
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

    bgfx::submit(kBgfxShadowVolumeView, g_device.shadowVolumeProgram);
}

void BgfxBackend::Submit_Shadow_Volume_Triangulated_Caps(
    unsigned strip_start_vertex,
    const short * local_cap_indices,
    unsigned cap_index_count)
{
    if (!g_device.initialized
        || !g_views.shadowVolumeActive
        || !bgfx::isValid(g_device.shadowVolumeProgram)
        || !bgfx::isValid(g_draw.vb)
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
    if (g_draw.cullModeBits == 1)      state |= BGFX_STATE_CULL_CW;
    else if (g_draw.cullModeBits == 2) state |= BGFX_STATE_CULL_CCW;
    bgfx::setState(state);
    bgfx::setStencil(g_draw.shadowStencilFront, g_draw.shadowStencilBack);

    bgfx::setVertexBuffer(0, g_draw.vb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTransform(g_frame.world);

    bgfx::submit(kBgfxShadowVolumeView, g_device.shadowVolumeProgram);
}

void BgfxBackend::Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                              unsigned stencil_read_mask,
                                              unsigned stencil_ref)
{
    DX8Backend::Apply_Stencil_Shadow_Darken(shadow_color, stencil_read_mask, stencil_ref);
    if (!g_device.initialized || !bgfx::isValid(g_device.shadowApplyProgram))
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
    if (bgfx::isValid(g_uniforms.uShadowColor))
        bgfx::setUniform(g_uniforms.uShadowColor, color);

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

    bgfx::submit(kBgfxShadowApplyView, g_device.shadowApplyProgram);
}

void BgfxBackend::Set_Stencil_Enable(bool enable)
{
    DX8Backend::Set_Stencil_Enable(enable);
    g_draw.stencilEnabled = enable;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Func(CompareFunc f)
{
    DX8Backend::Set_Stencil_Func(f);
    g_draw.stencilFuncBits = MapCmpFuncToBgfxStencilTest(static_cast<int>(f));
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Ref(unsigned ref)
{
    DX8Backend::Set_Stencil_Ref(ref);
    g_draw.stencilRef = ref;
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Mask(unsigned mask)
{
    DX8Backend::Set_Stencil_Mask(mask);
    g_draw.stencilReadMask = mask;
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
    g_draw.stencilPassOpBits = MapStencilOpToBgfx(static_cast<int>(op), BGFX_STENCIL_OP_PASS_Z_SHIFT);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_Fail_Op(StencilOp op)
{
    DX8Backend::Set_Stencil_Fail_Op(op);
    g_draw.stencilFailOpBits = MapStencilOpToBgfx(static_cast<int>(op), BGFX_STENCIL_OP_FAIL_S_SHIFT);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Stencil_ZFail_Op(StencilOp op)
{
    DX8Backend::Set_Stencil_ZFail_Op(op);
    g_draw.stencilZFailOpBits = MapStencilOpToBgfx(static_cast<int>(op), BGFX_STENCIL_OP_FAIL_Z_SHIFT);
    UpdateShadowStencilState();
}

void BgfxBackend::Set_Cull_Mode(CullMode mode)
{
    DX8Backend::Set_Cull_Mode(mode);
    switch (mode)
    {
        case RB_CULL_CW:  g_draw.cullModeBits = 1; break;
        case RB_CULL_CCW: g_draw.cullModeBits = 2; break;
        case RB_CULL_NONE:
        default:          g_draw.cullModeBits = 0; break;
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
        g_draw.state &= ~BGFX_STATE_DEPTH_TEST_MASK;
        g_draw.state |= kDepthMap[idx];
    }
}

void BgfxBackend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    DX8Backend::Set_Render_Target_With_Z(texture, ztexture);
    if (texture == nullptr || !g_device.initialized)
    {
        g_views.renderToTexture = false;
        return;
    }

    auto it = g_caches.framebuffer.find(texture);
    if (it == g_caches.framebuffer.end())
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
        g_caches.framebuffer[texture] = entry;
        it = g_caches.framebuffer.find(texture);

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

    g_views.renderToTexture = true;
}

void BgfxBackend::Clear_State_Overrides()
{
    g_overrides.Reset();
    g_draw.texcoordSelect[0] = 0.0f;
    // Do NOT clear g_draw.texcoordSelect[1] (terrain blend) here.
    // Override_Terrain_Blend is called from the shader manager BEFORE
    // Set_Shader, so clearing it in Set_Shader (which calls us) would
    // undo the terrain blend flag every frame. Terrain blend is reset
    // at Begin_Scene (above) and by Override_Terrain_Blend(false).
}

void BgfxBackend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    DX8Backend::Set_Light_Environment(light_env);
    if (light_env != nullptr)
    {
        const Vector3 & ambient = light_env->Get_Equivalent_Ambient();
        g_draw.sceneAmbient[0] = ambient.X;
        g_draw.sceneAmbient[1] = ambient.Y;
        g_draw.sceneAmbient[2] = ambient.Z;

        const int count = light_env->Get_Light_Count();
        for (int i = 0; i < 4; ++i)
        {
            if (i < count)
            {
                const Vector3 & dir = light_env->Get_Light_Direction(i);
                const Vector3 & dif = light_env->Get_Light_Diffuse(i);
                g_draw.lightDirs[i][0] = -dir.X;
                g_draw.lightDirs[i][1] = -dir.Y;
                g_draw.lightDirs[i][2] = -dir.Z;
                g_draw.lightDirs[i][3] = 1.0f; // enabled
                g_draw.lightColors[i][0] = dif.X;
                g_draw.lightColors[i][1] = dif.Y;
                g_draw.lightColors[i][2] = dif.Z;
                g_draw.lightColors[i][3] = 1.0f;
            }
            else
            {
                g_draw.lightDirs[i][3] = 0.0f; // disabled
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
    DX8Backend::Set_Transform(transform, m);
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

// Get_Transform / Is_World_Identity / Is_View_Identity are inherited
// from DX8Backend.

void BgfxBackend::Set_World_Identity()
{
    DX8Backend::Set_World_Identity();
    IdentityMatrix(g_frame.world);
}

void BgfxBackend::Set_View_Identity()
{
    DX8Backend::Set_View_Identity();
    IdentityMatrix(g_frame.view);
    g_frame.cameraProjDirty = true;
    g_views.overlay2DActive = true;
}

void BgfxBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
                                                       float znear, float zfar)
{
    DX8Backend::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);
    W3DMatrix4ToBgfx(matrix, g_frame.proj);
    g_frame.cameraProjDirty = true;

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
    if (g_overrides.suppressDraw)
    {
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }
    // Detect when the engine has restored the back buffer. The water/shadow
    // code calls DX8Wrapper::Set_Render_Target(nullptr) directly, bypassing
    // g_renderBackend. Poll the DX8 state to keep g_views.renderToTexture in sync.
    if (g_views.renderToTexture && !DX8Wrapper::Is_Render_To_Texture())
    {
        g_views.renderToTexture = false;
    }
    if (!g_device.initialized)
    {
        return;
    }
    if (!bgfx::isValid(g_draw.program))
    {
        return;
    }
    const bool have_vb = g_draw.useTransientVB || bgfx::isValid(g_draw.vb);
    const bool have_ib = g_draw.useTransientIB || bgfx::isValid(g_draw.ib);
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
    // where DX8 state restores clear g_views.overlay2DActive between the
    // Set_View_Identity call and the actual Draw_Triangles.
    bool is2D = g_views.overlay2DActive;
    if (!is2D && !g_views.renderToTexture && !g_views.waterOverrideActive
        && !g_views.effectOverlayActive && !g_views.inSortFlush)
    {
        // Check if view matrix is identity (2D mode)
        const float *v = g_frame.view;
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
    else if (g_views.renderToTexture)
    {
        submitView = kBgfxRTTView;
    }
    else if (g_views.waterOverrideActive)
    {
        submitView = kBgfxWaterView;
    }
    else if (g_views.effectOverlayActive)
    {
        submitView = kBgfxEffectOverlayView;
    }
    else if (g_views.inSortFlush)
    {
        submitView = kBgfxEngineSortView;
    }
    else
    {
        submitView = kBgfxEngineView;
    }
    // TheSuperHackers @bugfix bobtista 27/04/2026 Only world draws should
    // receive CSM shadows. The menu, FPS counter, RTT passes, water, and
    // effect overlays reuse the uber shader but are not in light space,
    // so sampling the shadow map there fades the UI.
    g_draw.shadowParams[0] = (submitView == kBgfxEngineView) ? 1.0f : 0.0f;

    const float *      worldMtx   = g_views.inSortFlush ? g_frame.sortWorld     : g_frame.world;

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
        // Phase 4I.2 CSM: update light transform every frame so
        // shadows follow the camera as it pans/zooms.
        if (!g_frame.shadowLightCaptured)
        {
            UpdateShadowLightTransform();
        }
        bgfx::setViewTransform(kBgfxEngineView, g_frame.view, g_frame.proj);
        // Shadow-volume view shares the engine camera; push the same
        // view+proj so the extrusion geometry lands where the opaque
        // geometry in view 1 landed.
        bgfx::setViewTransform(kBgfxShadowVolumeView, g_frame.view, g_frame.proj);
        g_frame.cameraProjDirty = false;
    }
    // During RTT, push the current (reflected/shadow) view+proj to the RTT view.
    if (g_views.renderToTexture && g_frame.cameraProjDirty)
    {
        bgfx::setViewTransform(kBgfxRTTView, g_frame.view, g_frame.proj);
        g_frame.cameraProjDirty = false;
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
    // incorrectly adding g_draw.ibOffset to the IB start - that
    // shifted which indices were read instead of which vertex they
    // resolved to, so meshes with a non-zero base_vertex_offset drew
    // garbled geometry from other meshes' vertex data.
    const uint32_t base_vertex = static_cast<uint32_t>(g_draw.ibOffset);
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
                                  base_vertex, vertex_count);
        }
    }
    else
    {
        bgfx::setVertexBuffer(0, g_draw.vb, base_vertex, vertex_count);
    }

    if (g_draw.useTransientIB)
    {
        bgfx::setIndexBuffer(&g_draw.transientIB,
                             start_index,
                             static_cast<uint32_t>(polygon_count) * 3);
    }
    else
    {
        bgfx::setIndexBuffer(g_draw.ib,
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

    // TheSuperHackers @fix bobtista 19/04/2026 During sort flush, the
    // sorting renderer's Apply_Render_State calls DX8Wrapper::Set_Texture
    // which has a cache check — if the texture pointer matches the cached
    // one, it skips BgfxBackend::Set_Texture entirely. This leaves
    // g_draw.tex[0]-3 stale (e.g., pointing at font glyphs instead
    // of particle textures). Force-sync bgfx handles from DX8Wrapper's
    // current state before each sorted draw.
    if (g_views.inSortFlush)
    {
        RenderStateStruct sortRS;
        DX8Wrapper::Get_Render_State(sortRS);

        for (int si = 0; si < 4; ++si)
        {
            TextureClass * sortTex = static_cast<TextureClass *>(sortRS.Textures[si]);
            bgfx::TextureHandle h = EnsureBgfxTexture(sortTex);
            if (!bgfx::isValid(h))
                h = g_device.defaultWhiteTexture;
            switch (si)
            {
                case 0: g_draw.tex[0] = h; break;
                case 1: g_draw.tex[1] = h; break;
                case 2: g_draw.tex[2] = h; break;
                case 3: g_draw.tex[3] = h; break;
            }
        }
    }
    BindTextureStages();
    UpdateTextureTransforms();
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
                g_draw.lightDirs[li][0] = -dl.Direction.x;
                g_draw.lightDirs[li][1] = -dl.Direction.y;
                g_draw.lightDirs[li][2] = -dl.Direction.z;
                g_draw.lightDirs[li][3] = 1.0f;
                g_draw.lightColors[li][0] = dl.Diffuse.r;
                g_draw.lightColors[li][1] = dl.Diffuse.g;
                g_draw.lightColors[li][2] = dl.Diffuse.b;
                g_draw.lightColors[li][3] = 1.0f;
            }
            else
            {
                g_draw.lightDirs[li][3] = 0.0f;
            }
        }
        const unsigned ambientColor = DX8Wrapper::Get_DX8_Render_State(D3DRS_AMBIENT);
        g_draw.sceneAmbient[0] = ((ambientColor >> 16) & 0xFF) / 255.0f;
        g_draw.sceneAmbient[1] = ((ambientColor >>  8) & 0xFF) / 255.0f;
        g_draw.sceneAmbient[2] = ((ambientColor >>  0) & 0xFF) / 255.0f;
    }
    if (bgfx::isValid(g_uniforms.uLightDirs))
        bgfx::setUniform(g_uniforms.uLightDirs, g_draw.lightDirs, 4);
    if (bgfx::isValid(g_uniforms.uLightColors))
        bgfx::setUniform(g_uniforms.uLightColors, g_draw.lightColors, 4);
    if (bgfx::isValid(g_uniforms.uSceneAmbient))
        bgfx::setUniform(g_uniforms.uSceneAmbient, g_draw.sceneAmbient);
    if (bgfx::isValid(g_uniforms.uLightingEnabled))
    {
        // TheSuperHackers @bugfix bobtista 15/04/2026 force lighting off
        // for additive-blend draws. Particle / dazzle / scanner sprites
        // bake their per-vertex intensity attenuation into vertex diffuse
        // (DazzleRenderObjClass and the particle system do this). The
        // uber-shader's lit branch ignores vertex diffuse and outputs
        // tex * lit_color, which renders these effects at full intensity
        // even when they should be invisible — producing bright white
        // halos around things like the microwave-scanner dome.
        const uint64_t blendBitsForLight = g_draw.state & BGFX_STATE_BLEND_MASK;
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
            || (blendBitsForLight == kAlphaSA_ISA && g_draw.tssOps0[0] < 1.5f))
        {
            float forced[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(g_uniforms.uLightingEnabled, forced);
        }
        else
        {
            bgfx::setUniform(g_uniforms.uLightingEnabled, g_draw.lightingEnabled);
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
                    g_draw.texcoordSelect[2] = 1.0f;
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
                    if (bgfx::isValid(g_uniforms.uShroudParams))
                        bgfx::setUniform(g_uniforms.uShroudParams, shroudParams);
                }
            }
        }
        if (!shroudDetected)
        {
            g_draw.texcoordSelect[2] = 0.0f;
        }
    }

    if (bgfx::isValid(g_uniforms.uTexcoordSelect))
    {
        bgfx::setUniform(g_uniforms.uTexcoordSelect, g_draw.texcoordSelect);
    }

    uint64_t state = (g_draw.state != 0)
        ? g_draw.state
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    state |= BGFX_STATE_MSAA;

    state = ApplyCullModeOverride(state);

    if (g_overrides.blendActive)
    {
        state &= ~BGFX_STATE_BLEND_MASK;
        state |= g_overrides.blendBits;
    }

    state = ApplyColorWriteOverride(state);

    if (g_views.waterOverrideActive)
    {
        g_views.waterOverrideActive = false;
    }

    if (g_views.shadowVolumeActive && bgfx::isValid(g_device.shadowVolumeProgram))
    {
        bgfx::discard(BGFX_DISCARD_ALL);
        return;
    }

    bgfx::setState(state);

    // Phase 4I.2 CSM: compute light view-projection once for both
    // receiver uniform and caster submit.
    float lightVP[16];
    bx::mtxMul(lightVP, g_frame.shadowLightView, g_frame.shadowLightProj);

    if (bgfx::isValid(g_uniforms.uShadowLightViewProj))
    {
        // Compose model * lightView * lightProj per-draw.
        // Use the CORRECT model matrix: g_frame.sortWorldRaw for sorted
        // draws (no camera view baked in), g_frame.world for opaque.
        float shadowMVP[16];
        const float * modelMtx = g_views.inSortFlush ? g_frame.sortWorldRaw : worldMtx;
        bx::mtxMul(shadowMVP, modelMtx, lightVP);
        bgfx::setUniform(g_uniforms.uShadowLightViewProj, shadowMVP);
    }

    BindShadowMapTexture();

    // Phase 4H tree / grass sway shader takes over the program slot
    // and uploads its own constants when active. Otherwise fall back
    // to whatever ShaderClass picked (g_draw.program).
    bgfx::ProgramHandle program = g_draw.program;
    if (g_views.treeShaderActive && bgfx::isValid(g_device.treeProgram))
    {
        program = g_device.treeProgram;
        if (bgfx::isValid(g_uniforms.uSwayTable))
            bgfx::setUniform(g_uniforms.uSwayTable, g_draw.swayTable, kSwayTableEntries);
        if (bgfx::isValid(g_uniforms.uShroudOffset))
            bgfx::setUniform(g_uniforms.uShroudOffset, g_draw.shroudOffset);
        if (bgfx::isValid(g_uniforms.uShroudScale))
            bgfx::setUniform(g_uniforms.uShroudScale, g_draw.shroudScale);
    }
    bgfx::submit(submitView, program);

    // Phase 4I.2 CSM caster pass. Submit opaque and sort-flush geometry
    // to the shadow map view (view 8). Skip RTT, water, and effect
    // overlay (those are non-shadow-casters by design). bgfx preserves
    // the per-view transform (set in UpdateShadowLightTransform) so
    // the caster rasterizes from the light's POV.
    const bool isShadowCaster =
        (submitView == kBgfxEngineView || submitView == kBgfxEngineSortView)
        && !g_views.overlay2DActive;
    const bool hasVB = g_draw.useTransientVB || bgfx::isValid(g_draw.vb);
    if (isShadowCaster
        && bgfx::isValid(g_device.shadowCasterProgram)
        && hasVB)
    {
        // Re-bind VB/IB/transform (bgfx consumes them per-submit).
        if (g_draw.useTransientVB)
        {
            if (g_views.inSortFlush)
                bgfx::setVertexBuffer(0, &g_draw.transientVB);
            else
                bgfx::setVertexBuffer(0, &g_draw.transientVB,
                                      static_cast<uint32_t>(g_draw.ibOffset),
                                      vertex_count);
        }
        else
        {
            bgfx::setVertexBuffer(0, g_draw.vb,
                                  static_cast<uint32_t>(g_draw.ibOffset),
                                  vertex_count);
        }
        if (g_draw.useTransientIB)
            bgfx::setIndexBuffer(&g_draw.transientIB,
                                 start_index,
                                 static_cast<uint32_t>(polygon_count) * 3);
        else
            bgfx::setIndexBuffer(g_draw.ib,
                                 start_index,
                                 static_cast<uint32_t>(polygon_count) * 3);
        // Use the world transform used by the just-issued engine
        // submit. For sort-flush draws that's g_frame.sortWorld (which
        // has the sort's batch transform baked in). For regular
        // opaque it's g_frame.world.
        bgfx::setTransform(worldMtx);
        {
            float casterMVP[16];
            const float * casterModel = g_views.inSortFlush ? g_frame.sortWorldRaw : worldMtx;
            bx::mtxMul(casterMVP, casterModel, lightVP);
            bgfx::setUniform(g_uniforms.uShadowLightViewProj, casterMVP);
        }
        const uint64_t casterState =
            BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CW;
        bgfx::setState(casterState);
        bgfx::submit(kBgfxShadowMapView, g_device.shadowCasterProgram);
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
    if (g_views.skipNextSubmitEngineDraw)
    {
        g_views.skipNextSubmitEngineDraw = false;
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
    if (g_views.skipNextSubmitEngineDraw)
    {
        g_views.skipNextSubmitEngineDraw = false;
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
    if (g_views.skipNextSubmitEngineDraw)
    {
        g_views.skipNextSubmitEngineDraw = false;
        return;
    }

    SubmitEngineDraw(start_index, index_count, min_vertex_index, vertex_count);
}

// The programmable pipeline (Set_Vertex_Shader, Set_Pixel_Shader,
// Set_Vertex_Shader_Constant, Set_Pixel_Shader_Constant), and render targets
// (Create_Render_Target, Set_Render_Target_With_Z, Is_Render_To_Texture,
// Set_Shadow_Map, Get_Shadow_Map) are inherited from DX8Backend.

// ===========================================================================
// Phase 5 asset-ingress resource creation
// ===========================================================================
//
// Each method creates BOTH the bgfx resource and (via the DX8Backend base)
// the D3D8 resource, so the ref-popup build sees both. The returned
// RenderResource.id is a monotonically-increasing key into g_phase5.table;
// the entry holds the bgfx handle(s) and the D3D8 pointer stored as a
// void* "d3d_mirror" for eventual DX8-specific use (e.g. the texture class
// still populating its D3DTexture field in ref-popup builds).
//
// Forward declaration — defined in BgfxBackendTextures.cpp.
bgfx::TextureFormat::Enum TranslateWW3DFormat(WW3DFormat fmt);

namespace {

unsigned __int64 AllocPhase5Id()
{
    const unsigned __int64 id = g_phase5.next_id++;
    if (g_phase5.next_id == 0) {
        // Roll-over guard — rarely hit; start back at 1 to avoid colliding
        // with kInvalidRenderResource.
        g_phase5.next_id = 1;
    }
    return id;
}

// Copy a MipSlice into a bgfx::Memory buffer sized for the slice.
const bgfx::Memory * CopySliceToBgfxMemory(const MipSlice & slice)
{
    if (slice.data == nullptr || slice.size_bytes == 0) {
        return nullptr;
    }
    return bgfx::copy(slice.data, slice.size_bytes);
}

} // namespace

RenderResource BgfxBackend::Create_Texture(const TextureDesc & desc)
{
    // Mirror to DX8 first so the ref popup's D3D8 texture exists.
    RenderResource dx8_rr = DX8Backend::Create_Texture(desc);

    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_TEXTURE;
    entry.d3d_mirror = reinterpret_cast<void *>(dx8_rr.id);
    entry.texture = BGFX_INVALID_HANDLE;

    if (!desc.is_render_target && desc.mips != nullptr && desc.mip_count > 0) {
        const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(desc.format);
        if (bgfxFmt != bgfx::TextureFormat::Unknown) {
            // For now, upload mip 0 and let bgfx's immutable-texture path
            // handle the rest. Multi-mip static upload will land with
            // Stage 1 when the loader changes feed us full mip chains.
            const bgfx::Memory * mem = CopySliceToBgfxMemory(desc.mips[0]);
            if (mem != nullptr) {
                const uint64_t texFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
                entry.texture = bgfx::createTexture2D(
                    desc.width, desc.height,
                    desc.mip_count > 1,  // hasMips
                    1, bgfxFmt, texFlags, mem);
            }
        }
    }
    // Render targets are allocated separately through Set_Render_Target_With_Z.

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

RenderResource BgfxBackend::Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data)
{
    RenderResource dx8_rr = DX8Backend::Create_Vertex_Buffer(desc, initial_data);

    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_VB;
    entry.d3d_mirror = reinterpret_cast<void *>(dx8_rr.id);
    entry.size_bytes = desc.size_bytes;
    entry.vb = BGFX_INVALID_HANDLE;
    // Static bgfx VB creation requires a layout. We build one lazily from
    // the FVF in a later stage; for now record the entry and leave vb
    // invalid — bgfx callers go through existing VB caches keyed by the
    // VertexBufferClass*, not this handle, until Stage 2 rewires them.
    (void)initial_data;
    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

RenderResource BgfxBackend::Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit)
{
    RenderResource dx8_rr = DX8Backend::Create_Index_Buffer(desc, initial_data, indices_are_32bit);

    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_IB;
    entry.d3d_mirror = reinterpret_cast<void *>(dx8_rr.id);
    entry.size_bytes = desc.size_bytes;
    entry.ib = BGFX_INVALID_HANDLE;
    // Same as vertex buffers — bgfx-side creation is deferred to Stage 2.
    (void)initial_data;
    (void)indices_are_32bit;

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

RenderResource BgfxBackend::Create_Dynamic_Vertex_Buffer(const BufferDesc & desc)
{
    RenderResource dx8_rr = DX8Backend::Create_Dynamic_Vertex_Buffer(desc);

    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_DYN_VB;
    entry.d3d_mirror = reinterpret_cast<void *>(dx8_rr.id);
    entry.size_bytes = desc.size_bytes;
    entry.dvb = BGFX_INVALID_HANDLE;
    // Dynamic VBs in bgfx are implemented on-the-fly via Map_Dynamic
    // allocating a transient buffer per map. The dvb handle here is a
    // reservation placeholder for the non-transient path if ever needed.

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

RenderResource BgfxBackend::Create_Dynamic_Index_Buffer(const BufferDesc & desc, bool indices_are_32bit)
{
    RenderResource dx8_rr = DX8Backend::Create_Dynamic_Index_Buffer(desc, indices_are_32bit);

    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_DYN_IB;
    entry.d3d_mirror = reinterpret_cast<void *>(dx8_rr.id);
    entry.size_bytes = desc.size_bytes;
    entry.dib = BGFX_INVALID_HANDLE;

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

void * BgfxBackend::Map_Dynamic(RenderResource h, unsigned int offset, unsigned int size, bool discard)
{
    auto it = g_phase5.table.find(h.id);
    if (it == g_phase5.table.end()) {
        return nullptr;
    }
    BgfxPhase5Entry & entry = it->second;
    // Return the D3D8-mapped pointer. On Unmap, we will snapshot the written
    // bytes into a bgfx transient buffer before the D3D unlock happens.
    RenderResource dx8_rr;
    dx8_rr.id = reinterpret_cast<unsigned __int64>(entry.d3d_mirror);
    return DX8Backend::Map_Dynamic(dx8_rr, offset, size, discard);
}

void BgfxBackend::Unmap_Dynamic(RenderResource h)
{
    auto it = g_phase5.table.find(h.id);
    if (it == g_phase5.table.end()) {
        return;
    }
    BgfxPhase5Entry & entry = it->second;
    RenderResource dx8_rr;
    dx8_rr.id = reinterpret_cast<unsigned __int64>(entry.d3d_mirror);
    DX8Backend::Unmap_Dynamic(dx8_rr);
    // Stage 3 will add the bgfx transient allocation + snapshot here.
}

void BgfxBackend::Update_Sub_Range(RenderResource h, unsigned int offset, const void * data, unsigned int size)
{
    auto it = g_phase5.table.find(h.id);
    if (it == g_phase5.table.end()) {
        return;
    }
    BgfxPhase5Entry & entry = it->second;
    RenderResource dx8_rr;
    dx8_rr.id = reinterpret_cast<unsigned __int64>(entry.d3d_mirror);
    DX8Backend::Update_Sub_Range(dx8_rr, offset, data, size);
}

void BgfxBackend::Destroy_Resource(RenderResource h)
{
    auto it = g_phase5.table.find(h.id);
    if (it == g_phase5.table.end()) {
        return;
    }
    BgfxPhase5Entry & entry = it->second;

    // Destroy bgfx side.
    switch (entry.kind) {
        case BGFX_RR_KIND_TEXTURE:
            if (bgfx::isValid(entry.texture)) {
                g_caches.deferredDestroys.push_back(entry.texture);
            }
            break;
        case BGFX_RR_KIND_VB:
            if (bgfx::isValid(entry.vb)) {
                bgfx::destroy(entry.vb);
            }
            break;
        case BGFX_RR_KIND_IB:
            if (bgfx::isValid(entry.ib)) {
                bgfx::destroy(entry.ib);
            }
            break;
        case BGFX_RR_KIND_DYN_VB:
            if (bgfx::isValid(entry.dvb)) {
                bgfx::destroy(entry.dvb);
            }
            break;
        case BGFX_RR_KIND_DYN_IB:
            if (bgfx::isValid(entry.dib)) {
                bgfx::destroy(entry.dib);
            }
            break;
        case BGFX_RR_KIND_NONE:
        default:
            break;
    }

    // Release the D3D8 mirror.
    RenderResource dx8_rr;
    dx8_rr.id = reinterpret_cast<unsigned __int64>(entry.d3d_mirror);
    DX8Backend::Destroy_Resource(dx8_rr);
    g_phase5.table.erase(it);
}

void BgfxBackend::Begin_Dynamic_Frame()
{
    // Clear per-frame transient tracking on dynamic entries. The transient
    // buffers themselves are owned by bgfx's ring allocator.
    for (auto & kv : g_phase5.table) {
        BgfxPhase5Entry & entry = kv.second;
        entry.using_transient_vb = false;
        entry.using_transient_ib = false;
    }
    DX8Backend::Begin_Dynamic_Frame();
}

// -- Phase 5 transitional Register_Loaded_* ---------------------------------

RenderResource BgfxBackend::Register_Loaded_Texture(TextureBaseClass * tex)
{
    if (tex == nullptr) {
        return kInvalidRenderResource;
    }
    // Ensure the bgfx-side texture exists (peek+lock+upload from the D3D8
    // mirror that the legacy loader already created). The returned handle
    // is owned by g_caches.texture (keyed on TextureBaseClass*), NOT by
    // this phase5 entry — Release_Cached_Texture in the dtor queues it
    // for deferred destroy. We leave entry.texture invalid so
    // Destroy_Resource doesn't try to destroy the same handle twice.
    EnsureBgfxTexture(tex);

    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind       = BGFX_RR_KIND_TEXTURE;
    entry.texture    = BGFX_INVALID_HANDLE;
    entry.d3d_mirror = nullptr;

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

RenderResource BgfxBackend::Register_Loaded_Vertex_Buffer(VertexBufferClass * vb)
{
    if (vb == nullptr) {
        return kInvalidRenderResource;
    }
    // IMPORTANT: do NOT store the VertexBufferClass* as d3d_mirror —
    // Destroy_Resource would cast it to IUnknown* and call Release(), which
    // lands on whatever the third virtual of VertexBufferClass happens to
    // be and crashes. The VB's D3D resource lifetime is owned by the
    // DX8VertexBufferClass dtor; we have no cleanup to do on the DX8 side.
    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_VB;
    entry.vb = BGFX_INVALID_HANDLE;
    entry.d3d_mirror = nullptr;

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}

RenderResource BgfxBackend::Register_Loaded_Index_Buffer(IndexBufferClass * ib)
{
    if (ib == nullptr) {
        return kInvalidRenderResource;
    }
    // Same rationale as Register_Loaded_Vertex_Buffer — leave d3d_mirror
    // null so Destroy_Resource's DX8-side Release does nothing.
    BgfxPhase5Entry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.kind = BGFX_RR_KIND_IB;
    entry.ib = BGFX_INVALID_HANDLE;
    entry.d3d_mirror = nullptr;

    RenderResource rr;
    rr.id = AllocPhase5Id();
    g_phase5.table[rr.id] = entry;
    return rr;
}
