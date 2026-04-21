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
#include "matrix3d.h"
#include "matrix4.h"
#include "shader.h"
#include "texture.h"
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

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 3. BgfxBackend
// now creates its own top-level popup window and hands that HWND to bgfx::init.
// Required because Windows DWM promotes whichever swapchain is actively
// presenting to a HWND, so sharing the game's HWND with DX8 loses DX8's
// output. A separate HWND sidesteps the conflict entirely. See PHASE4.md.
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
#include "fs_uber_dx11.bin.h"

namespace
{
// Anchor a reference to one bgfx symbol so the linker must resolve bgfx
// symbols even though every virtual method below is a no-op. This turns a
// "bgfx built but never used" scenario into a loud link failure if anything
// is misconfigured.
[[maybe_unused]] const auto kBgfxLinkAnchor = &bgfx::getCaps;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 1. Tracks
// whether bgfx::init has been called successfully, so Begin_Scene /
// End_Scene / Shutdown can skip bgfx calls if init was never reached.
bool g_bgfxInitialized = false;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 3. The
// popup window BgfxBackend owns and hands to bgfx::init. Null until
// Initialize runs, nulled again by Shutdown.
HWND g_bgfxWindow = nullptr;

const wchar_t * const kBgfxWindowClass = L"GGC_BgfxDebugWindow";
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4F.3 popup
// dimensions chosen to match the game's 1.6 aspect ratio (1280x800)
// so engine geometry rendered with the game's projection matrix
// does not look horizontally squished.
const int kBgfxWindowWidth  = 800;
const int kBgfxWindowHeight = 500;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.3 program handle
// for the passthrough shader pair. Created once in Initialize after bgfx::init
// succeeds, destroyed in Shutdown before bgfx::shutdown.
bgfx::ProgramHandle g_passthroughProgram = BGFX_INVALID_HANDLE;

// TheSuperHackers @refactor bobtista 12/04/2026 Phase 5A uber shader
// program. Single program handles all TSS combinations via uniforms.
bgfx::ProgramHandle g_uberProgram = BGFX_INVALID_HANDLE;

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

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4F.1 default 1x1
// white texture. Real Set_Texture wiring is not in place yet, so the
// textured shaders need SOMETHING bound to s_tex0 or D3D11 returns
// undefined values. A 1x1 opaque white texture is a sensible default
// because the fragment shader does texColor * v_color0 - white * vc
// gives the vertex color through unmodified.
bgfx::TextureHandle g_defaultWhiteTexture = BGFX_INVALID_HANDLE;

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
std::unordered_map<const VertexBufferClass *, bgfx::DynamicVertexBufferHandle> g_vbCache;
std::unordered_map<const IndexBufferClass *,  bgfx::DynamicIndexBufferHandle>  g_ibCache;
std::unordered_map<const TextureBaseClass *,  bgfx::TextureHandle>             g_textureCache;

// The bgfx texture currently bound to stage 0 by Set_Texture. Used by
// SubmitEngineDraw - falls back to g_defaultWhiteTexture if invalid.
bgfx::TextureHandle g_currentBgfxTexture0 = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_currentBgfxTexture1 = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_currentBgfxTexture2 = BGFX_INVALID_HANDLE;
bgfx::TextureHandle g_currentBgfxTexture3 = BGFX_INVALID_HANDLE;
bool g_loggedFirstBgfxTextureCreate = false;

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

bool g_loggedFirstBgfxVbCreate = false;
bool g_loggedFirstBgfxIbCreate = false;
bool g_loggedFirstBgfxSubmit   = false;
bool g_loggedFirstDynVbCapture = false;
bool g_loggedFirstDynIbCapture = false;

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
bool  g_loggedFirstSetView = false;
bool  g_loggedFirstSetProj = false;
bool  g_loggedFirstSetWorld = false;

// Engine geometry submits to its own view so it does not collide with
// the test triangle on view 0. View 0 keeps the test triangle for the
// "is bgfx alive" sentinel; view 1 is engine geometry under engine
// transforms. Both render to the popup back buffer.
const bgfx::ViewId kBgfxEngineView = 1;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.12 dedicated
// view id for sorted draws. View 2's view matrix is permanently
// identity and its projection tracks view 1's. Per-batch sort
// transforms get pre-multiplied into g_bgfxSortWorld so view 2 never
// needs setViewTransform updates per batch - which is critical,
// because bgfx::setViewTransform is per-view-for-the-whole-frame and
// would otherwise stomp view 1's camera view if shared.
const bgfx::ViewId kBgfxEngineSortView = 2;

// True between Begin_Sorted_Batch_Pass and End_Sorted_Batch_Pass;
// SubmitEngineDraw routes to kBgfxEngineSortView and uses
// g_bgfxSortWorld while this is set.
bool  g_inSortFlush = false;

// Per-batch effective world for sorted draws: the pre-multiplied
// sortView * sortWorld (in bgfx column-major form) captured from the
// engine's render_state by Capture_Sorted_Batch_Transforms.
float g_bgfxSortWorld[16];

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.13 Set by
// Submit_Sorted_Draw after it emits the bgfx submit for a sorting VB
// direct draw. The outer BgfxBackend::Draw_Triangles consumes this
// flag to skip its SubmitEngineDraw - the draw was already issued
// with correctly remapped args against the inner dynamic buffers,
// so falling through would emit a second, incorrect submit.
bool  g_skipNextSubmitEngineDraw = false;


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

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.1 aspect ratio
// correction. The engine computes the projection matrix for the main
// game window's aspect ratio. The bgfx popup has a fixed 800x500
// framebuffer (aspect 1.6), so applying the engine's projection
// directly stretches geometry horizontally when the game runs at a
// different aspect (e.g. 1024x768 = 1.33, or 1280x720 = 1.78). Rescale
// column 0 by (gameAspect / popupAspect) so the popup shows the same
// field of view the engine is rendering to the main window.
//
// The game aspect is extracted from the projection itself: for a
// standard perspective matrix proj[0][0] = cot(fov/2)/aspect and
// proj[1][1] = cot(fov/2), so gameAspect = proj[1][1] / proj[0][0].
// Avoids touching DX8Wrapper's protected resolution accessors and
// works even when the engine later reshapes the backbuffer.
void ApplyPopupAspectCorrection(float * proj)
{
    // Column-major: proj[0] is (col 0, row 0), proj[5] is (col 1, row 1).
    const float p00 = proj[0];
    const float p11 = proj[5];
    if (p00 <= 0.0f || p11 <= 0.0f)
    {
        return;
    }
    const float gameAspect  = p11 / p00;
    const float popupAspect = static_cast<float>(kBgfxWindowWidth) /
                              static_cast<float>(kBgfxWindowHeight);
    if (popupAspect <= 0.0f)
    {
        return;
    }
    const float scale = gameAspect / popupAspect;
    proj[0] *= scale;
    proj[1] *= scale;
    proj[2] *= scale;
    proj[3] *= scale;
}

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
        // POOL_DEFAULT textures (render targets, dynamic) need a
        // different code path. Skip for now.
        g_textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::TextureFormat::Enum bgfxFmt = TranslateWW3DFormat(tex2d->Get_Texture_Format());
    if (bgfxFmt == bgfx::TextureFormat::Unknown)
    {
        g_textureCache[tex] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    IDirect3DBaseTexture8 * baseTex = tex->Peek_D3D_Base_Texture();
    if (baseTex == nullptr)
    {
        // Texture not yet uploaded to D3D - the background loader thread
        // may not have finished. DO NOT cache the failure: we want to
        // retry on the next Set_Texture call so the bgfx handle gets
        // populated as soon as the engine finishes loading. Otherwise a
        // texture that was simply slow to load would stay white forever.
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

    // Compute the byte size of the surface. For uncompressed formats it
    // is height * pitch. For DXT it is height/4 * pitch (block-compressed).
    const bool isCompressed =
        bgfxFmt == bgfx::TextureFormat::BC1 ||
        bgfxFmt == bgfx::TextureFormat::BC2 ||
        bgfxFmt == bgfx::TextureFormat::BC3;

    unsigned heightBlocks = desc.Height;
    if (isCompressed)
    {
        heightBlocks = (desc.Height + 3) / 4;
    }
    const unsigned totalBytes = heightBlocks * static_cast<unsigned>(locked.Pitch);

    const bgfx::Memory * mem = bgfx::copy(locked.pBits, totalBytes);
    d3dTex->UnlockRect(0);

    bgfx::TextureHandle h = bgfx::createTexture2D(
        static_cast<uint16_t>(desc.Width),
        static_cast<uint16_t>(desc.Height),
        false, 1,
        bgfxFmt,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC,
        mem);

    g_textureCache[tex] = h;

    if (!g_loggedFirstBgfxTextureCreate)
    {
        g_loggedFirstBgfxTextureCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxTexture first create "
                     "%dx%d ww3dfmt=%u bytes=%u handle=%s",
                     desc.Width, desc.Height,
                     static_cast<unsigned>(tex2d->Get_Texture_Format()),
                     totalBytes,
                     bgfx::isValid(h) ? "ok" : "FAILED"));
    }
    return h;
}

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.3 Set_Shader
// telemetry. The bgfx backend caches the picked program and state bits
// for the most recent ShaderClass it received, and tracks the unique
// shader bit patterns it has seen so we can log them once each. This
// is diagnostic only - no draw call uses these yet, but it confirms
// that the engine is calling Set_Shader on the bgfx backend and gives
// us real preset coverage data without touching rendering.
bgfx::ProgramHandle g_currentBgfxProgram = BGFX_INVALID_HANDLE;
uint64_t            g_currentBgfxState   = 0;

const int kMaxLoggedPresets = 32;
unsigned int g_loggedPresetBits[kMaxLoggedPresets] = { 0 };
int          g_loggedPresetCount = 0;

bool RecordPresetIfNew(unsigned int bits)
{
    for (int i = 0; i < g_loggedPresetCount; ++i)
    {
        if (g_loggedPresetBits[i] == bits)
        {
            return false;
        }
    }
    if (g_loggedPresetCount < kMaxLoggedPresets)
    {
        g_loggedPresetBits[g_loggedPresetCount++] = bits;
        return true;
    }
    return false;
}

const char * ProgramDebugName(bgfx::ProgramHandle h)
{
    if (h.idx == g_passthroughProgram.idx) return "passthrough";
    if (h.idx == g_uberProgram.idx)        return "uber";
    return "?";
}

// Per-method "first call" flags so we can confirm the engine is routing
// each entry point through the bgfx backend at least once. Logged once,
// then suppressed to keep WWDebug.txt readable.
bool g_loggedFirstSetTexture       = false;
bool g_loggedFirstDrawTriangles    = false;
bool g_loggedFirstSetVertexBuffer  = false;
bool g_loggedFirstSetIndexBuffer   = false;

// Create the bgfx debug popup window. Returns the HWND or nullptr on failure.
HWND CreateBgfxDebugWindow()
{
    if (!RegisterBgfxDebugWindowClass())
    {
        return nullptr;
    }

    const DWORD style   = WS_OVERLAPPEDWINDOW;
    const DWORD exStyle = WS_EX_NOACTIVATE;

    RECT rc = { 0, 0, kBgfxWindowWidth, kBgfxWindowHeight };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    HWND hwnd = CreateWindowExW(
        exStyle,
        kBgfxWindowClass,
        L"bgfx backend [Phase 4]",
        style,
        100, 100,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);

    if (hwnd == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] CreateWindowExW failed, GetLastError=%lu.",
                     GetLastError()));
        return nullptr;
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return hwnd;
}
}

void BgfxBackend::Initialize(void * /*hwnd*/, int /*width*/, int /*height*/)
{
    if (g_bgfxInitialized)
    {
        WWDEBUG_SAY(("[BgfxBackend] Initialize called twice; ignoring."));
        return;
    }

    g_bgfxWindow = CreateBgfxDebugWindow();
    if (g_bgfxWindow == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] Could not create debug window. "
                     "Backend will remain dormant."));
        return;
    }

    // Force bgfx into single-threaded rendering mode by calling
    // bgfx::renderFrame BEFORE bgfx::init. Makes bgfx::frame() fully
    // synchronous on the calling thread. See PHASE4.md.
    bgfx::renderFrame();

    bgfx::PlatformData pd;
    pd.ndt = nullptr;
    pd.nwh = g_bgfxWindow;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    bgfx::setPlatformData(pd);

    bgfx::Init initArgs;
    initArgs.type = bgfx::RendererType::Count;  // auto-select (D3D11 on Windows)
    initArgs.resolution.width = static_cast<uint32_t>(kBgfxWindowWidth);
    initArgs.resolution.height = static_cast<uint32_t>(kBgfxWindowHeight);
    // Phase 4G.10 enable 4x MSAA on the bgfx framebuffer. Without this,
    // polygon boundaries between terrain tiles, rock shorelines, and
    // unit silhouettes alias hard at the popup's lower resolution.
    initArgs.resolution.reset = BGFX_RESET_MSAA_X4;
    initArgs.platformData = pd;

    if (!bgfx::init(initArgs))
    {
        WWDEBUG_SAY(("[BgfxBackend] bgfx::init FAILED on debug window. "
                     "Backend will remain dormant."));
        DestroyWindow(g_bgfxWindow);
        g_bgfxWindow = nullptr;
        return;
    }

    g_bgfxInitialized = true;

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.10 force
    // a bgfx::reset() after init so the MSAA flag actually takes effect.
    // On D3D11 bgfx needs an explicit reset call to rebuild the swapchain
    // with a multi-sampled back buffer; setting resolution.reset in the
    // Init struct alone isn't enough in every bgfx build.
    bgfx::reset(static_cast<uint32_t>(kBgfxWindowWidth),
                static_cast<uint32_t>(kBgfxWindowHeight),
                BGFX_RESET_MSAA_X4);

    // Configure view 0 to clear the debug window to a dark teal so it's
    // visually obvious bgfx is running and alive. View 0 holds the test
    // triangle (Phase 4B sentinel).
    bgfx::setViewClear(0,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x1a3b5cff,  // dark teal, 0xRRGGBBAA
                       1.0f,
                       0);
    bgfx::setViewRect(0, 0, 0,
                      static_cast<uint16_t>(kBgfxWindowWidth),
                      static_cast<uint16_t>(kBgfxWindowHeight));

    // View 1 is the engine geometry view. Same render target, but its
    // own clear/depth and (eventually) its own view+projection matrices
    // captured from the engine's Set_Transform calls. Drawn after view 0
    // so engine geometry overlays the test triangle.
    bgfx::setViewClear(kBgfxEngineView,
                       BGFX_CLEAR_DEPTH,
                       0x00000000,
                       1.0f,
                       0);
    bgfx::setViewRect(kBgfxEngineView, 0, 0,
                      static_cast<uint16_t>(kBgfxWindowWidth),
                      static_cast<uint16_t>(kBgfxWindowHeight));

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
                      static_cast<uint16_t>(kBgfxWindowWidth),
                      static_cast<uint16_t>(kBgfxWindowHeight));

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

    // Phase 4F.1 default 1x1 white texture (opaque ABGR=0xffffffff).
    static const uint8_t kWhitePixel[4] = { 0xff, 0xff, 0xff, 0xff };
    g_defaultWhiteTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWhitePixel, sizeof(kWhitePixel)));

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
        WWDEBUG_SAY(("[BgfxBackend] uber shader createShader FAILED."));
    }

    const bgfx::RendererType::Enum selected = bgfx::getRendererType();
    const char * rendererName = bgfx::getRendererName(selected);
    WWDEBUG_SAY(("[BgfxBackend] bgfx::init OK on debug window "
                 "(renderer=%s, %dx%d, hwnd=%p, passthrough=%s, uber=%s).",
                 rendererName, kBgfxWindowWidth, kBgfxWindowHeight,
                 g_bgfxWindow,
                 bgfx::isValid(g_passthroughProgram) ? "ok" : "FAILED",
                 bgfx::isValid(g_uberProgram)        ? "ok" : "FAILED"));
}

void BgfxBackend::Shutdown()
{
    if (g_bgfxInitialized)
    {
        if (bgfx::isValid(g_passthroughProgram))
        {
            bgfx::destroy(g_passthroughProgram);
            g_passthroughProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_uberProgram))
        {
            bgfx::destroy(g_uberProgram);
            g_uberProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_sTex0))
        {
            bgfx::destroy(g_sTex0);
            g_sTex0 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_sTex1))
        {
            bgfx::destroy(g_sTex1);
            g_sTex1 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_sTex2))
        {
            bgfx::destroy(g_sTex2);
            g_sTex2 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_sTex3))
        {
            bgfx::destroy(g_sTex3);
            g_sTex3 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_uMatDiffuse))
        {
            bgfx::destroy(g_uMatDiffuse);
            g_uMatDiffuse = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_uAtestParams))
        {
            bgfx::destroy(g_uAtestParams);
            g_uAtestParams = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_uTssOps0))
        {
            bgfx::destroy(g_uTssOps0);
            g_uTssOps0 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_uTssOps1))
        {
            bgfx::destroy(g_uTssOps1);
            g_uTssOps1 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_defaultWhiteTexture))
        {
            bgfx::destroy(g_defaultWhiteTexture);
            g_defaultWhiteTexture = BGFX_INVALID_HANDLE;
        }
        // Phase 4C.3 cached engine buffers. Destroy before bgfx::shutdown
        // so the handles outlive nothing.
        for (auto & kv : g_vbCache)
        {
            if (bgfx::isValid(kv.second))
            {
                bgfx::destroy(kv.second);
            }
        }
        g_vbCache.clear();
        for (auto & kv : g_ibCache)
        {
            if (bgfx::isValid(kv.second))
            {
                bgfx::destroy(kv.second);
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

    if (g_bgfxWindow != nullptr)
    {
        DestroyWindow(g_bgfxWindow);
        g_bgfxWindow = nullptr;
    }
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
    bgfx::touch(0);
    bgfx::touch(kBgfxEngineView);
    bgfx::touch(kBgfxEngineSortView);
}

void BgfxBackend::End_Scene(bool /*flip_frame*/)
{
    if (!g_bgfxInitialized)
    {
        return;
    }
    // Re-apply the camera projection to the sort view. The engine calls
    // Set_Projection_Transform_With_Z_Bias multiple times per frame
    // (camera, water reflections, shadows). The last call often uses a
    // tiny near-field frustum that clips all sort geometry. Since bgfx's
    // setViewTransform is retroactive for the whole frame, we re-apply
    // the projection that was active at sort-flush time.
    if (g_bgfxSortProjCaptured)
    {
        float identityView[16];
        IdentityMatrix(identityView);
        bgfx::setViewTransform(kBgfxEngineSortView, identityView, g_bgfxSortProj);
        g_bgfxSortProjCaptured = false;
    }

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
        g_currentBgfxVB = it->second;
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
                        g_currentBgfxVB = it2->second;
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

    if (!g_loggedFirstSetIndexBuffer && ib != nullptr)
    {
        g_loggedFirstSetIndexBuffer = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Index_Buffer first call (static IB path) "
                     "indices=%u type=%u",
                     ib->Get_Index_Count(),
                     ib->Type()));
    }
    g_currentUseTransientIB = false;
    auto it = g_ibCache.find(ib);
    if (it != g_ibCache.end())
    {
        g_currentBgfxIB = it->second;
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
                        g_currentBgfxIB = it2->second;
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
    auto it = g_vbCache.find(vb);
    if (it != g_vbCache.end() && bgfx::isValid(it->second))
    {
        return it->second;
    }
    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vb->FVF_Info(), layout))
    {
        return BGFX_INVALID_HANDLE;
    }
    const uint32_t num_verts = static_cast<uint32_t>(vb->Get_Vertex_Count());
    if (num_verts == 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicVertexBufferHandle h = bgfx::createDynamicVertexBuffer(num_verts, layout);
    g_vbCache[vb] = h;
    return h;
}

bgfx::DynamicIndexBufferHandle EnsureDynamicIndexBuffer(const IndexBufferClass * ib)
{
    auto it = g_ibCache.find(ib);
    if (it != g_ibCache.end() && bgfx::isValid(it->second))
    {
        return it->second;
    }
    const uint32_t num_indices = static_cast<uint32_t>(ib->Get_Index_Count());
    if (num_indices == 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    bgfx::DynamicIndexBufferHandle h = bgfx::createDynamicIndexBuffer(num_indices);
    g_ibCache[ib] = h;
    return h;
}
}

void BgfxBackend::Capture_Vertex_Data(const VertexBufferClass * vb,
                                      const void * data,
                                      unsigned int size_bytes)
{
    if (!g_bgfxInitialized || vb == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    bgfx::DynamicVertexBufferHandle h = EnsureDynamicVertexBuffer(vb);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::update(h, 0, mem);

    if (!g_loggedFirstBgfxVbCreate)
    {
        g_loggedFirstBgfxVbCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Vertex_Data first update "
                     "fvf=0x%08x stride=%u verts=%u bytes=%u",
                     vb->FVF_Info().Get_FVF(),
                     vb->FVF_Info().Get_FVF_Size(),
                     vb->Get_Vertex_Count(),
                     size_bytes));
    }
}

void BgfxBackend::Capture_Index_Data(const IndexBufferClass * ib,
                                     const void * data,
                                     unsigned int size_bytes)
{
    if (!g_bgfxInitialized || ib == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }
    bgfx::DynamicIndexBufferHandle h = EnsureDynamicIndexBuffer(ib);
    if (!bgfx::isValid(h))
    {
        return;
    }
    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::update(h, 0, mem);

    if (!g_loggedFirstBgfxIbCreate)
    {
        g_loggedFirstBgfxIbCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Index_Data first update "
                     "indices=%u bytes=%u",
                     ib->Get_Index_Count(),
                     size_bytes));
    }
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
static bool g_loggedFirstSortBegin   = false;
static bool g_loggedFirstSortCapture = false;

void BgfxBackend::Begin_Sorted_Batch_Pass()
{
    g_inSortFlush = true;

    // Capture the projection NOW (while it's still the camera projection).
    // Later Set_Projection calls (water, shadows) may overwrite g_bgfxProj
    // with a tiny frustum. We re-apply this saved projection at End_Scene.
    if (!g_bgfxSortProjCaptured)
    {
        std::memcpy(g_bgfxSortProj, g_bgfxProj, sizeof(g_bgfxSortProj));
        g_bgfxSortProjCaptured = true;
    }

    if (!g_loggedFirstSortBegin)
    {
        g_loggedFirstSortBegin = true;
        WWDEBUG_SAY(("[BgfxBackend] Begin_Sorted_Batch_Pass first fire "
                     "transVB=%d transIB=%d",
                     int(g_currentUseTransientVB),
                     int(g_currentUseTransientIB)));
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
        }
    }

    if (!g_loggedFirstSortCapture)
    {
        g_loggedFirstSortCapture = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Sorted_Batch_Transforms first "
                     "sortWorld row0=%.3f,%.3f,%.3f,%.3f sortView row0=%.3f,%.3f,%.3f,%.3f",
                     sortWorld[0][0], sortWorld[0][1], sortWorld[0][2], sortWorld[0][3],
                     sortView[0][0],  sortView[0][1],  sortView[0][2],  sortView[0][3]));
        WWDEBUG_SAY(("[BgfxBackend]   combined (column-major) col0=%.3f,%.3f,%.3f,%.3f col3=%.3f,%.3f,%.3f,%.3f",
                     g_bgfxSortWorld[0],  g_bgfxSortWorld[1],  g_bgfxSortWorld[2],  g_bgfxSortWorld[3],
                     g_bgfxSortWorld[12], g_bgfxSortWorld[13], g_bgfxSortWorld[14], g_bgfxSortWorld[15]));
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
static bool g_loggedFirstSortedDirectSubmit = false;

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
        if (!g_loggedFirstSortedDirectSubmit)
        {
            g_loggedFirstSortedDirectSubmit = true;
            WWDEBUG_SAY(("[BgfxBackend] Submit_Sorted_Draw SKIP: pendingDynVB not "
                         "claimable (valid=%d ownerMatch=%d)",
                         int(g_pendingDynVB.valid),
                         int(g_pendingDynVB.owner == &dyn_vb)));
        }
        return;
    }
    if (!g_pendingDynIB.valid || g_pendingDynIB.owner != &dyn_ib)
    {
        if (!g_loggedFirstSortedDirectSubmit)
        {
            g_loggedFirstSortedDirectSubmit = true;
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

    const uint32_t kSamplerFlags = 0;
    if (bgfx::isValid(g_sTex0))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture0) ? g_currentBgfxTexture0 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(0, g_sTex0, bound, kSamplerFlags);
        }
    }
    if (bgfx::isValid(g_sTex1))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture1) ? g_currentBgfxTexture1 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(1, g_sTex1, bound, kSamplerFlags);
        }
    }
    if (bgfx::isValid(g_sTex2))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture2) ? g_currentBgfxTexture2 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(2, g_sTex2, bound, kSamplerFlags);
        }
    }
    if (bgfx::isValid(g_sTex3))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture3) ? g_currentBgfxTexture3 : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(3, g_sTex3, bound, kSamplerFlags);
        }
    }

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

    uint64_t state = (g_currentBgfxState != 0)
        ? g_currentBgfxState
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    bgfx::setState(state);

    bgfx::submit(kBgfxEngineSortView, g_currentBgfxProgram);

    if (!g_loggedFirstSortedDirectSubmit)
    {
        g_loggedFirstSortedDirectSubmit = true;
        WWDEBUG_SAY(("[BgfxBackend] Submit_Sorted_Draw first fire "
                     "(polys=%u verts=%u worldCol3=%.2f,%.2f,%.2f state=0x%016llx)",
                     polygon_count, vertex_count,
                     worldMtx[12], worldMtx[13], worldMtx[14],
                     static_cast<unsigned long long>(state)));
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

    if (!g_loggedFirstDynVbCapture)
    {
        g_loggedFirstDynVbCapture = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Dynamic_Vertex_Data first "
                     "fvf=0x%08x stride=%u verts=%u bytes=%u",
                     vba->FVF_Info().Get_FVF(),
                     layout.getStride(),
                     num_verts,
                     bytes));
    }

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

    if (!g_loggedFirstDynIbCapture)
    {
        g_loggedFirstDynIbCapture = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Dynamic_Index_Data first "
                     "indices=%u bytes=%u",
                     num_indices,
                     bytes));
    }

}

// -- State: shaders, materials, textures ------------------------------------

void BgfxBackend::Set_Shader(const ShaderClass & shader)
{
    DX8Backend::Set_Shader(shader);

    g_currentBgfxProgram = g_uberProgram;
    g_currentBgfxState   = BuildBgfxStateForShader(shader);
    BuildTssOpsForShader(shader, g_currentTssOps0, g_currentTssOps1, &g_currentAtestRef);

    if (RecordPresetIfNew(shader.Get_Bits()))
    {
        WWDEBUG_SAY(("[BgfxBackend] Set_Shader new preset bits=0x%08x -> "
                     "tssOps=(%.0f,%.0f,%.0f,%.0f) state=0x%016llx",
                     shader.Get_Bits(),
                     g_currentTssOps0[0], g_currentTssOps0[1],
                     g_currentTssOps0[2], g_currentTssOps0[3],
                     static_cast<unsigned long long>(g_currentBgfxState)));
    }
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
    }
    else
    {
        g_currentMatDiffuse[0] = 1.0f;
        g_currentMatDiffuse[1] = 1.0f;
        g_currentMatDiffuse[2] = 1.0f;
        g_currentMatDiffuse[3] = 1.0f;
    }
}

void BgfxBackend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Backend::Set_Texture(stage, texture);

    if (!g_loggedFirstSetTexture)
    {
        g_loggedFirstSetTexture = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Texture first call (stage=%u)", stage));
    }
    // Phase 4G.3 / 4G.4: stages 0-3 wired. Covers terrain base + detail
    // + cloud + noise, the standard 4-stage layout used by the
    // FlatHeightMap pixel shader family. Stages above 3 still fall
    // through unmigrated.
    switch (stage)
    {
        case 0: g_currentBgfxTexture0 = EnsureBgfxTexture(texture); break;
        case 1: g_currentBgfxTexture1 = EnsureBgfxTexture(texture); break;
        case 2: g_currentBgfxTexture2 = EnsureBgfxTexture(texture); break;
        case 3: g_currentBgfxTexture3 = EnsureBgfxTexture(texture); break;
        default: break;
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
            if (!g_loggedFirstSetWorld)
            {
                g_loggedFirstSetWorld = true;
                WWDEBUG_SAY(("[BgfxBackend] Set_Transform first WORLD (Matrix4x4) "
                             "row0=%.3f,%.3f,%.3f,%.3f",
                             m[0][0], m[0][1], m[0][2], m[0][3]));
            }
            break;
        case RB_TRANSFORM_VIEW:
            W3DMatrix4ToBgfx(m, g_bgfxView);
            g_bgfxViewProjDirty = true;
            if (!g_loggedFirstSetView)
            {
                g_loggedFirstSetView = true;
                WWDEBUG_SAY(("[BgfxBackend] Set_Transform first VIEW (Matrix4x4) "
                             "row0=%.3f,%.3f,%.3f,%.3f",
                             m[0][0], m[0][1], m[0][2], m[0][3]));
            }
            break;
        case RB_TRANSFORM_PROJECTION:
            W3DMatrix4ToBgfx(m, g_bgfxProj);
            ApplyPopupAspectCorrection(g_bgfxProj);
            g_bgfxViewProjDirty = true;
            if (!g_loggedFirstSetProj)
            {
                g_loggedFirstSetProj = true;
                WWDEBUG_SAY(("[BgfxBackend] Set_Transform first PROJ (Matrix4x4) "
                             "row0=%.3f,%.3f,%.3f,%.3f",
                             m[0][0], m[0][1], m[0][2], m[0][3]));
            }
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
            if (!g_loggedFirstSetWorld)
            {
                g_loggedFirstSetWorld = true;
                WWDEBUG_SAY(("[BgfxBackend] Set_Transform first WORLD (Matrix3D) "
                             "row0=%.3f,%.3f,%.3f,%.3f",
                             m[0][0], m[0][1], m[0][2], m[0][3]));
            }
            break;
        case RB_TRANSFORM_VIEW:
            W3DMatrix3DToBgfx(m, g_bgfxView);
            g_bgfxViewProjDirty = true;
            if (!g_loggedFirstSetView)
            {
                g_loggedFirstSetView = true;
                WWDEBUG_SAY(("[BgfxBackend] Set_Transform first VIEW (Matrix3D) "
                             "row0=%.3f,%.3f,%.3f,%.3f",
                             m[0][0], m[0][1], m[0][2], m[0][3]));
            }
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
}

void BgfxBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix,
                                                       float znear, float zfar)
{
    DX8Backend::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);

    W3DMatrix4ToBgfx(matrix, g_bgfxProj);
    ApplyPopupAspectCorrection(g_bgfxProj);
    g_bgfxViewProjDirty = true;

    // View 2's projection is now set at End_Scene time from the saved
    // sort-flush projection (g_bgfxSortProj). This avoids the issue
    // where later Set_Projection calls (water, shadows) overwrite view
    // 2's projection with a tiny near-field frustum.
    if (!g_loggedFirstSetProj)
    {
        g_loggedFirstSetProj = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Projection_Transform_With_Z_Bias first call "
                     "row0=%.3f,%.3f,%.3f,%.3f",
                     matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3]));
        // Dump full bgfx column-major projection for Z debugging
        WWDEBUG_SAY(("[BgfxBackend] PROJ bgfx col-major: "
                     "col0=%.4f,%.4f,%.4f,%.4f "
                     "col1=%.4f,%.4f,%.4f,%.4f "
                     "col2=%.4f,%.4f,%.4f,%.4f "
                     "col3=%.4f,%.4f,%.4f,%.4f",
                     g_bgfxProj[0],  g_bgfxProj[1],  g_bgfxProj[2],  g_bgfxProj[3],
                     g_bgfxProj[4],  g_bgfxProj[5],  g_bgfxProj[6],  g_bgfxProj[7],
                     g_bgfxProj[8],  g_bgfxProj[9],  g_bgfxProj[10], g_bgfxProj[11],
                     g_bgfxProj[12], g_bgfxProj[13], g_bgfxProj[14], g_bgfxProj[15]));
        // Manual clip-space computation for a known sort vertex
        // v = (-170.29, -5.09, -499.62, 1.0)
        float vx=-170.29f, vy=-5.09f, vz=-499.62f, vw=1.0f;
        float cx = g_bgfxProj[0]*vx + g_bgfxProj[4]*vy + g_bgfxProj[8]*vz  + g_bgfxProj[12]*vw;
        float cy = g_bgfxProj[1]*vx + g_bgfxProj[5]*vy + g_bgfxProj[9]*vz  + g_bgfxProj[13]*vw;
        float cz = g_bgfxProj[2]*vx + g_bgfxProj[6]*vy + g_bgfxProj[10]*vz + g_bgfxProj[14]*vw;
        float cw = g_bgfxProj[3]*vx + g_bgfxProj[7]*vy + g_bgfxProj[11]*vz + g_bgfxProj[15]*vw;
        WWDEBUG_SAY(("[BgfxBackend] PROJ TEST: clip=%.2f,%.2f,%.2f,%.2f ndc=%.4f,%.4f,%.4f",
                     cx, cy, cz, cw,
                     cw != 0.0f ? cx/cw : 0.0f,
                     cw != 0.0f ? cy/cw : 0.0f,
                     cw != 0.0f ? cz/cw : 0.0f));
    }
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
    const bgfx::ViewId submitView = g_inSortFlush ? kBgfxEngineSortView : kBgfxEngineView;
    const float *      worldMtx   = g_inSortFlush ? g_bgfxSortWorld     : g_bgfxWorld;

    // Push the engine view+projection when they change. setViewTransform
    // applies until the next change so we do not need to call it per
    // submit, only when the engine has updated either matrix. Sort view
    // draws never touch g_bgfxView so the opaque view is never stomped.
    if (!g_inSortFlush && g_bgfxViewProjDirty)
    {
        bgfx::setViewTransform(kBgfxEngineView, g_bgfxView, g_bgfxProj);
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
    const uint32_t kSamplerFlags = 0;
    if (bgfx::isValid(g_sTex0))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture0)
                ? g_currentBgfxTexture0
                : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(0, g_sTex0, bound, kSamplerFlags);
        }
    }
    if (bgfx::isValid(g_sTex1))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture1)
                ? g_currentBgfxTexture1
                : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(1, g_sTex1, bound, kSamplerFlags);
        }
    }
    if (bgfx::isValid(g_sTex2))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture2)
                ? g_currentBgfxTexture2
                : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(2, g_sTex2, bound, kSamplerFlags);
        }
    }
    if (bgfx::isValid(g_sTex3))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture3)
                ? g_currentBgfxTexture3
                : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(3, g_sTex3, bound, kSamplerFlags);
        }
    }

    // Phase 4G.9 push the material diffuse (team color + opacity) so
    // the fragment shader can tint output. Updated by Set_Material.
    if (bgfx::isValid(g_uMatDiffuse))
    {
        bgfx::setUniform(g_uMatDiffuse, g_currentMatDiffuse);
    }

    // Phase 5A: push TSS operation uniforms so the uber fragment shader
    // knows how to combine texture stages.
    if (bgfx::isValid(g_uTssOps0))
    {
        bgfx::setUniform(g_uTssOps0, g_currentTssOps0);
    }
    if (bgfx::isValid(g_uTssOps1))
    {
        bgfx::setUniform(g_uTssOps1, g_currentTssOps1);
    }
    // Phase 5A: push alpha test reference. Previously created but never
    // set per-draw — now extracted from ShaderClass in Set_Shader.
    if (bgfx::isValid(g_uAtestParams))
    {
        float atestParams[4] = { g_currentAtestRef, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(g_uAtestParams, atestParams);
    }

    // Use the cached state if it was built; otherwise fall back to a
    // sane default that at least lets the draw run.
    uint64_t state = (g_currentBgfxState != 0)
        ? g_currentBgfxState
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    // Phase 4G.15: disable depth test for sort view draws. Sort
    // particles are pre-transformed to view space with identity
    // world/view, so their Z values don't match the opaque view's
    // depth buffer (which used the real camera view transform).
    if (submitView == kBgfxEngineSortView)
    {
        state &= ~BGFX_STATE_DEPTH_TEST_MASK;
        state |= BGFX_STATE_DEPTH_TEST_ALWAYS;
    }

    bgfx::setState(state);

    bgfx::submit(submitView, g_currentBgfxProgram);

    if (!g_loggedFirstBgfxSubmit)
    {
        g_loggedFirstBgfxSubmit = true;
        WWDEBUG_SAY(("[BgfxBackend] First engine bgfx::submit "
                     "(view=%u start=%u polys=%u min_v=%u count=%u state=0x%016llx)",
                     static_cast<unsigned>(submitView),
                     start_index, polygon_count,
                     min_vertex_index, vertex_count,
                     static_cast<unsigned long long>(state)));
    }
}
}

void BgfxBackend::Draw_Triangles(unsigned short start_index,
                                 unsigned short polygon_count,
                                 unsigned short min_vertex_index,
                                 unsigned short vertex_count)
{
    DX8Backend::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

    if (!g_loggedFirstDrawTriangles)
    {
        g_loggedFirstDrawTriangles = true;
        WWDEBUG_SAY(("[BgfxBackend] Draw_Triangles first call (polys=%u verts=%u)",
                     polygon_count, vertex_count));
    }

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

    if (!g_loggedFirstDrawTriangles)
    {
        g_loggedFirstDrawTriangles = true;
        WWDEBUG_SAY(("[BgfxBackend] Draw_Triangles first call typed (polys=%u verts=%u)",
                     polygon_count, vertex_count));
    }
    if (g_skipNextSubmitEngineDraw)
    {
        g_skipNextSubmitEngineDraw = false;
        return;
    }
    SubmitEngineDraw(start_index, polygon_count, min_vertex_index, vertex_count);
}

// Draw_Strip, the programmable pipeline (Set_Vertex_Shader, Set_Pixel_Shader,
// Set_Vertex_Shader_Constant, Set_Pixel_Shader_Constant), and render targets
// (Create_Render_Target, Set_Render_Target_With_Z, Is_Render_To_Texture,
// Set_Shadow_Map, Get_Shadow_Map) are inherited from DX8Backend.
