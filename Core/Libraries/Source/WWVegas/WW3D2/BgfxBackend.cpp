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
#include "shader.h"
#include "vector3.h"
#include "wwdebug.h"

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

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 preset shader
// programs. These cover the three most common ShaderClass preset buckets
// (lit textured, unlit textured, alpha-tested lit textured). See
// PHASE4_INVENTORY.md for the bucket analysis.
#include "vs_textured_lit_dx11.bin.h"
#include "fs_textured_lit_dx11.bin.h"
#include "vs_textured_unlit_dx11.bin.h"
#include "fs_textured_unlit_dx11.bin.h"
#include "fs_textured_lit_atest_dx11.bin.h"
#include "vs_solid_lit_dx11.bin.h"
#include "fs_solid_lit_dx11.bin.h"

namespace
{
// Anchor a reference to one bgfx symbol so the linker must resolve bgfx
// symbols even though every virtual method below is a no-op. This turns a
// "bgfx built but never used" scenario into a loud link failure if anything
// is misconfigured.
[[maybe_unused]] const auto kBgfxLinkAnchor = &bgfx::getCaps;

// Shared static vector returned from Get_Ambient(). Phase 3 will replace
// this with real per-frame ambient tracking.
const Vector3 kZeroVec3(0.0f, 0.0f, 0.0f);

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 1. Tracks
// whether bgfx::init has been called successfully, so Begin_Scene /
// End_Scene / Shutdown can skip bgfx calls if init was never reached.
bool g_bgfxInitialized = false;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 3. The
// popup window BgfxBackend owns and hands to bgfx::init. Null until
// Initialize runs, nulled again by Shutdown.
HWND g_bgfxWindow = nullptr;

const wchar_t * const kBgfxWindowClass = L"GGC_BgfxDebugWindow";
const int kBgfxWindowWidth  = 512;
const int kBgfxWindowHeight = 384;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.3 program handle
// for the passthrough shader pair. Created once in Initialize after bgfx::init
// succeeds, destroyed in Shutdown before bgfx::shutdown.
bgfx::ProgramHandle g_passthroughProgram = BGFX_INVALID_HANDLE;

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 preset shader
// programs. Created in Initialize, destroyed in Shutdown. Not yet used by
// any draw call - that wiring lands in a later session along with the
// ShaderClass preset -> ProgramHandle mapping table.
bgfx::ProgramHandle g_texturedLitProgram      = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle g_texturedUnlitProgram    = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle g_texturedLitAtestProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle g_solidLitProgram         = BGFX_INVALID_HANDLE;

// Sampler uniform shared by all textured fragment shaders. Bound to stage 0.
bgfx::UniformHandle g_sTex0        = BGFX_INVALID_HANDLE;
// Alpha-test parameters consumed by fs_textured_lit_atest. .x is the
// reference threshold in [0, 1]; engine writes ShaderClass alpha-ref / 255.
// Named u_atestParams (not u_alphaRef) to avoid bgfx_shader.sh's internal
// u_alphaRef4 conflict. See fs_textured_lit_atest.sc for details.
bgfx::UniformHandle g_uAtestParams = BGFX_INVALID_HANDLE;

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

bgfx::ProgramHandle PickProgramForShader(const ShaderClass & shader)
{
    const bool textured = (shader.Get_Texturing()        != ShaderClass::TEXTURING_DISABLE);
    const bool lit      = (shader.Get_Primary_Gradient() != ShaderClass::GRADIENT_DISABLE);
    const bool atest    = (shader.Get_Alpha_Test()       != ShaderClass::ALPHATEST_DISABLE);

    if (atest && textured)
    {
        return g_texturedLitAtestProgram;
    }
    if (textured && lit)
    {
        return g_texturedLitProgram;
    }
    if (textured && !lit)
    {
        return g_texturedUnlitProgram;
    }
    if (!textured && lit)
    {
        return g_solidLitProgram;
    }
    return g_passthroughProgram;
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

[[maybe_unused]] const auto kShaderTranslationAnchor = &PickProgramForShader;

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
    if (h.idx == g_passthroughProgram.idx)      return "passthrough";
    if (h.idx == g_texturedLitProgram.idx)      return "textured_lit";
    if (h.idx == g_texturedUnlitProgram.idx)    return "textured_unlit";
    if (h.idx == g_texturedLitAtestProgram.idx) return "textured_lit_atest";
    if (h.idx == g_solidLitProgram.idx)         return "solid_lit";
    return "?";
}

// Per-method "first call" flags so we can confirm the engine is routing
// each entry point through the bgfx backend at least once. Logged once,
// then suppressed to keep WWDebug.txt readable.
bool g_loggedFirstApplyRenderState = false;
bool g_loggedFirstSetTexture       = false;
bool g_loggedFirstSetMaterial      = false;
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
    initArgs.resolution.reset = BGFX_RESET_NONE;
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

    // Configure view 0 to clear the debug window to a dark teal so it's
    // visually obvious bgfx is running and alive.
    bgfx::setViewClear(0,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x1a3b5cff,  // dark teal, 0xRRGGBBAA
                       1.0f,
                       0);
    bgfx::setViewRect(0, 0, 0,
                      static_cast<uint16_t>(kBgfxWindowWidth),
                      static_cast<uint16_t>(kBgfxWindowHeight));

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

    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 build the
    // three preset shader programs. They share two vertex shaders and three
    // fragment shaders. The lit-atest program reuses vs_textured_lit. Each
    // createProgram(..., true) hands ownership of its shader handles to the
    // program, so we must NOT manually destroy them later.
    g_sTex0        = bgfx::createUniform("s_tex0",        bgfx::UniformType::Sampler);
    g_uAtestParams = bgfx::createUniform("u_atestParams", bgfx::UniformType::Vec4);

    const bgfx::Memory * vsLitMem      = bgfx::makeRef(vs_textured_lit_dx11,
                                                       sizeof(vs_textured_lit_dx11));
    const bgfx::Memory * fsLitMem      = bgfx::makeRef(fs_textured_lit_dx11,
                                                       sizeof(fs_textured_lit_dx11));
    const bgfx::Memory * vsUnlitMem    = bgfx::makeRef(vs_textured_unlit_dx11,
                                                       sizeof(vs_textured_unlit_dx11));
    const bgfx::Memory * fsUnlitMem    = bgfx::makeRef(fs_textured_unlit_dx11,
                                                       sizeof(fs_textured_unlit_dx11));
    const bgfx::Memory * fsAtestMem    = bgfx::makeRef(fs_textured_lit_atest_dx11,
                                                       sizeof(fs_textured_lit_atest_dx11));

    bgfx::ShaderHandle vsLit   = bgfx::createShader(vsLitMem);
    bgfx::ShaderHandle fsLit   = bgfx::createShader(fsLitMem);
    bgfx::ShaderHandle vsUnlit = bgfx::createShader(vsUnlitMem);
    bgfx::ShaderHandle fsUnlit = bgfx::createShader(fsUnlitMem);
    bgfx::ShaderHandle fsAtest = bgfx::createShader(fsAtestMem);

    if (bgfx::isValid(vsLit) && bgfx::isValid(fsLit))
    {
        bgfx::setName(vsLit, "vs_textured_lit");
        bgfx::setName(fsLit, "fs_textured_lit");
        // vsLit is also used by the atest program; createProgram(..., true)
        // would destroy it after the first call. Pass false here, then
        // again for the atest program; we destroy vsLit ourselves below.
        g_texturedLitProgram = bgfx::createProgram(vsLit, fsLit, false);
    }
    if (bgfx::isValid(vsUnlit) && bgfx::isValid(fsUnlit))
    {
        bgfx::setName(vsUnlit, "vs_textured_unlit");
        bgfx::setName(fsUnlit, "fs_textured_unlit");
        g_texturedUnlitProgram = bgfx::createProgram(vsUnlit, fsUnlit, true);
    }
    if (bgfx::isValid(vsLit) && bgfx::isValid(fsAtest))
    {
        bgfx::setName(fsAtest, "fs_textured_lit_atest");
        g_texturedLitAtestProgram = bgfx::createProgram(vsLit, fsAtest, false);
    }
    // Now safe to release standalone shader handles - the programs hold
    // their own internal refs.
    if (bgfx::isValid(vsLit))
    {
        bgfx::destroy(vsLit);
    }
    if (bgfx::isValid(fsLit))
    {
        bgfx::destroy(fsLit);
    }
    if (bgfx::isValid(fsAtest))
    {
        bgfx::destroy(fsAtest);
    }

    // Solid lit shader (bucket C) - vertex color only, no texture sample.
    const bgfx::Memory * vsSolidMem = bgfx::makeRef(vs_solid_lit_dx11,
                                                    sizeof(vs_solid_lit_dx11));
    const bgfx::Memory * fsSolidMem = bgfx::makeRef(fs_solid_lit_dx11,
                                                    sizeof(fs_solid_lit_dx11));
    bgfx::ShaderHandle vsSolid = bgfx::createShader(vsSolidMem);
    bgfx::ShaderHandle fsSolid = bgfx::createShader(fsSolidMem);
    if (bgfx::isValid(vsSolid) && bgfx::isValid(fsSolid))
    {
        bgfx::setName(vsSolid, "vs_solid_lit");
        bgfx::setName(fsSolid, "fs_solid_lit");
        g_solidLitProgram = bgfx::createProgram(vsSolid, fsSolid, true);
    }

    const bgfx::RendererType::Enum selected = bgfx::getRendererType();
    const char * rendererName = bgfx::getRendererName(selected);
    WWDEBUG_SAY(("[BgfxBackend] bgfx::init OK on debug window "
                 "(renderer=%s, %dx%d, hwnd=%p, passthrough=%s, "
                 "lit=%s, unlit=%s, lit_atest=%s, solid=%s).",
                 rendererName, kBgfxWindowWidth, kBgfxWindowHeight,
                 g_bgfxWindow,
                 bgfx::isValid(g_passthroughProgram)      ? "ok" : "FAILED",
                 bgfx::isValid(g_texturedLitProgram)      ? "ok" : "FAILED",
                 bgfx::isValid(g_texturedUnlitProgram)    ? "ok" : "FAILED",
                 bgfx::isValid(g_texturedLitAtestProgram) ? "ok" : "FAILED",
                 bgfx::isValid(g_solidLitProgram)         ? "ok" : "FAILED"));
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
        if (bgfx::isValid(g_texturedLitProgram))
        {
            bgfx::destroy(g_texturedLitProgram);
            g_texturedLitProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_texturedUnlitProgram))
        {
            bgfx::destroy(g_texturedUnlitProgram);
            g_texturedUnlitProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_texturedLitAtestProgram))
        {
            bgfx::destroy(g_texturedLitAtestProgram);
            g_texturedLitAtestProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_solidLitProgram))
        {
            bgfx::destroy(g_solidLitProgram);
            g_solidLitProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_sTex0))
        {
            bgfx::destroy(g_sTex0);
            g_sTex0 = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(g_uAtestParams))
        {
            bgfx::destroy(g_uAtestParams);
            g_uAtestParams = BGFX_INVALID_HANDLE;
        }
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

// -- Device state queries ----------------------------------------------------

bool BgfxBackend::Is_Device_Lost() const
{
    return false;
}

bool BgfxBackend::Has_Stencil()
{
    return false;
}

WW3DFormat BgfxBackend::Get_Back_Buffer_Format()
{
    return WW3D_FORMAT_UNKNOWN;
}

SurfaceClass * BgfxBackend::Get_Back_Buffer(unsigned int /*num*/)
{
    return nullptr;
}

void BgfxBackend::Set_Gamma(float /*gamma*/, float /*bright*/, float /*contrast*/,
                            bool /*calibrate*/, bool /*uselimit*/)
{
}

// -- Frame lifecycle ---------------------------------------------------------
//
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.3. Begin_Scene
// submits a rotating view setup for view 0. End_Scene submits a test
// triangle via the passthrough shader then calls bgfx::frame() to present.
// This is the first real bgfx draw call from Phase 4 code. Future phases
// replace the test triangle with actual scene geometry submitted through
// the IRenderBackend draw methods.

namespace
{
void SubmitTestTriangle()
{
    if (!bgfx::isValid(g_passthroughProgram))
    {
        return;
    }

    if (bgfx::getAvailTransientVertexBuffer(3, g_triangleLayout) < 3)
    {
        return;
    }

    // Explicitly set view 0's transforms to identity each frame. Without
    // this, u_modelViewProj in the vertex shader is whatever bgfx happens
    // to have from a previous view transform call (undefined on first use).
    static const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    bgfx::setViewTransform(0, identity, identity);

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, 3, g_triangleLayout);
    TriangleVertex * verts = reinterpret_cast<TriangleVertex *>(tvb.data);

    // Triangle in clip space. Z = 0.5 puts it safely inside D3D11's [0,1]
    // depth range away from both near and far clip planes. Format of the
    // color field is ABGR packed (bgfx convention): 0xAABBGGRR as a u32.
    verts[0].x =  0.0f;  verts[0].y =  0.5f;  verts[0].z = 0.5f;  verts[0].abgr = 0xff0000ff; // red   top
    verts[1].x =  0.5f;  verts[1].y = -0.5f;  verts[1].z = 0.5f;  verts[1].abgr = 0xff00ff00; // green right
    verts[2].x = -0.5f;  verts[2].y = -0.5f;  verts[2].z = 0.5f;  verts[2].abgr = 0xffff0000; // blue  left

    bgfx::setVertexBuffer(0, &tvb);
    // No culling — eliminate winding-order confusion while diagnosing.
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(0, g_passthroughProgram);
}
}

void BgfxBackend::Begin_Scene()
{
    if (!g_bgfxInitialized)
    {
        return;
    }
    bgfx::touch(0);
}

void BgfxBackend::End_Scene(bool /*flip_frame*/)
{
    if (!g_bgfxInitialized)
    {
        return;
    }
    SubmitTestTriangle();
    bgfx::frame();
}

void BgfxBackend::Flip_To_Primary()
{
}

void BgfxBackend::Clear(bool /*clear_color*/, bool /*clear_z_stencil*/,
                        const Vector3 & /*color*/,
                        float /*dest_alpha*/, float /*z*/, unsigned int /*stencil*/)
{
}

void BgfxBackend::Set_Viewport(const RenderBackendViewport & /*viewport*/)
{
}

// -- Vertex / index buffers --------------------------------------------------

void BgfxBackend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int /*stream*/)
{
    if (!g_loggedFirstSetVertexBuffer && vb != nullptr)
    {
        g_loggedFirstSetVertexBuffer = true;
        const FVFInfoClass & fvf = vb->FVF_Info();
        WWDEBUG_SAY(("[BgfxBackend] Set_Vertex_Buffer first call (static VB path) "
                     "fvf=0x%08x size=%u verts=%u type=%u",
                     fvf.Get_FVF(),
                     fvf.Get_FVF_Size(),
                     vb->Get_Vertex_Count(),
                     vb->Type()));

        // Phase 4C.2 verification: build a bgfx layout from this FVF and
        // log whether the resulting stride matches the source. If it does
        // we can confidently start creating real bgfx vertex buffers from
        // VertexBufferClass instances in a follow-up commit.
        bgfx::VertexLayout layout;
        const bool ok       = BuildBgfxLayoutForFVF(fvf, layout);
        const unsigned want = fvf.Get_FVF_Size();
        const unsigned got  = layout.getStride();
        WWDEBUG_SAY(("[BgfxBackend] FVF -> bgfx layout %s "
                     "(stride want=%u got=%u)",
                     ok ? "OK" : "MISMATCH", want, got));
    }
}

void BgfxBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    if (!g_loggedFirstSetVertexBuffer)
    {
        g_loggedFirstSetVertexBuffer = true;
        const FVFInfoClass & fvf = vba.FVF_Info();
        WWDEBUG_SAY(("[BgfxBackend] Set_Vertex_Buffer first call (dynamic VB path) "
                     "fvf=0x%08x size=%u verts=%u type=%u",
                     fvf.Get_FVF(),
                     fvf.Get_FVF_Size(),
                     vba.Get_Vertex_Count(),
                     vba.Get_Type()));
    }
}

void BgfxBackend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short /*index_base_offset*/)
{
    if (!g_loggedFirstSetIndexBuffer && ib != nullptr)
    {
        g_loggedFirstSetIndexBuffer = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Index_Buffer first call (static IB path) "
                     "indices=%u type=%u",
                     ib->Get_Index_Count(),
                     ib->Type()));
    }
}

void BgfxBackend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short /*index_base_offset*/)
{
    if (!g_loggedFirstSetIndexBuffer)
    {
        g_loggedFirstSetIndexBuffer = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Index_Buffer first call (dynamic IB path) "
                     "indices=%u type=%u",
                     iba.Get_Index_Count(),
                     iba.Get_Type()));
    }
}

void BgfxBackend::Set_Index_Buffer_Index_Offset(unsigned int /*offset*/)
{
}

// -- State: shaders, materials, textures ------------------------------------

void BgfxBackend::Set_Shader(const ShaderClass & shader)
{
    // TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.3 cache the
    // picked program and state bits for the next draw call, and log any
    // shader bit pattern we have not seen before (capped at kMaxLoggedPresets
    // to keep WWDebug.txt readable). No draw call consumes the cached state
    // yet - this is purely diagnostic so we can confirm the engine is
    // routing Set_Shader through the bgfx backend and see which preset
    // buckets the game actually uses.
    g_currentBgfxProgram = PickProgramForShader(shader);
    g_currentBgfxState   = BuildBgfxStateForShader(shader);

    if (RecordPresetIfNew(shader.Get_Bits()))
    {
        WWDEBUG_SAY(("[BgfxBackend] Set_Shader new preset bits=0x%08x -> "
                     "program=%s state=0x%016llx",
                     shader.Get_Bits(),
                     ProgramDebugName(g_currentBgfxProgram),
                     static_cast<unsigned long long>(g_currentBgfxState)));
    }
}

void BgfxBackend::Get_Shader(ShaderClass & /*shader*/)
{
}

void BgfxBackend::Set_Material(const VertexMaterialClass * /*material*/)
{
    if (!g_loggedFirstSetMaterial)
    {
        g_loggedFirstSetMaterial = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Material first call"));
    }
}

void BgfxBackend::Set_Texture(unsigned int stage, TextureBaseClass * /*texture*/)
{
    if (!g_loggedFirstSetTexture)
    {
        g_loggedFirstSetTexture = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Texture first call (stage=%u)", stage));
    }
}

void BgfxBackend::Apply_Render_State_Changes()
{
    if (!g_loggedFirstApplyRenderState)
    {
        g_loggedFirstApplyRenderState = true;
        WWDEBUG_SAY(("[BgfxBackend] Apply_Render_State_Changes first call"));
    }
}

void BgfxBackend::Apply_Default_State()
{
}

void BgfxBackend::Invalidate_Cached_Render_States()
{
}

void BgfxBackend::Set_Blend_Op(BlendOp /*op*/)
{
}

void BgfxBackend::Set_Blend_Factors(BlendFactor /*src*/, BlendFactor /*dest*/)
{
}

void BgfxBackend::Set_Color_Write_Enable(bool /*red*/, bool /*green*/, bool /*blue*/, bool /*alpha*/)
{
}

void BgfxBackend::Set_Alpha_Blend_Enable(bool /*enable*/)
{
}

void BgfxBackend::Show_Hardware_Cursor(bool /*show*/)
{
}

void BgfxBackend::Set_Hardware_Cursor_Image(int /*hotspot_x*/, int /*hotspot_y*/, SurfaceClass * /*surface*/)
{
}

void BgfxBackend::Set_Hardware_Cursor_Position(int /*x*/, int /*y*/)
{
}

void BgfxBackend::Set_Stencil_Enable(bool /*enable*/)
{
}

void BgfxBackend::Set_Stencil_Func(CompareFunc /*func*/)
{
}

void BgfxBackend::Set_Stencil_Ref(unsigned int /*ref*/)
{
}

void BgfxBackend::Set_Stencil_Mask(unsigned int /*mask*/)
{
}

void BgfxBackend::Set_Stencil_Write_Mask(unsigned int /*mask*/)
{
}

void BgfxBackend::Set_Stencil_Pass_Op(StencilOp /*op*/)
{
}

void BgfxBackend::Set_Stencil_Fail_Op(StencilOp /*op*/)
{
}

void BgfxBackend::Set_Stencil_ZFail_Op(StencilOp /*op*/)
{
}

// -- Transforms --------------------------------------------------------------

void BgfxBackend::Set_Transform(TransformKind /*transform*/, const Matrix4x4 & /*m*/)
{
}

void BgfxBackend::Set_Transform(TransformKind /*transform*/, const Matrix3D & /*m*/)
{
}

void BgfxBackend::Get_Transform(TransformKind /*transform*/, Matrix4x4 & /*m*/)
{
}

void BgfxBackend::Set_World_Identity()
{
}

void BgfxBackend::Set_View_Identity()
{
}

bool BgfxBackend::Is_World_Identity()
{
    return true;
}

bool BgfxBackend::Is_View_Identity()
{
    return true;
}

void BgfxBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & /*matrix*/,
                                                       float /*znear*/, float /*zfar*/)
{
}

// -- Lighting and fog --------------------------------------------------------

void BgfxBackend::Set_Light(unsigned int /*index*/, const LightClass & /*light*/)
{
}

void BgfxBackend::Set_Ambient(const Vector3 & /*color*/)
{
}

const Vector3 & BgfxBackend::Get_Ambient() const
{
    return kZeroVec3;
}

void BgfxBackend::Set_Fog(bool /*enable*/, const Vector3 & /*color*/,
                          float /*start*/, float /*end*/)
{
}

bool BgfxBackend::Get_Fog_Enable() const
{
    return false;
}

void BgfxBackend::Set_Light_Environment(LightEnvironmentClass * /*light_env*/)
{
}

LightEnvironmentClass * BgfxBackend::Get_Light_Environment() const
{
    return nullptr;
}

// -- Draw calls --------------------------------------------------------------

void BgfxBackend::Draw_Triangles(unsigned short /*start_index*/,
                                 unsigned short polygon_count,
                                 unsigned short /*min_vertex_index*/,
                                 unsigned short vertex_count)
{
    if (!g_loggedFirstDrawTriangles)
    {
        g_loggedFirstDrawTriangles = true;
        WWDEBUG_SAY(("[BgfxBackend] Draw_Triangles first call (polys=%u verts=%u)",
                     polygon_count, vertex_count));
    }
}

void BgfxBackend::Draw_Triangles(unsigned int /*buffer_type*/,
                                 unsigned short /*start_index*/,
                                 unsigned short polygon_count,
                                 unsigned short /*min_vertex_index*/,
                                 unsigned short vertex_count)
{
    if (!g_loggedFirstDrawTriangles)
    {
        g_loggedFirstDrawTriangles = true;
        WWDEBUG_SAY(("[BgfxBackend] Draw_Triangles first call typed (polys=%u verts=%u)",
                     polygon_count, vertex_count));
    }
}

void BgfxBackend::Draw_Strip(unsigned short /*start_index*/,
                             unsigned short /*index_count*/,
                             unsigned short /*min_vertex_index*/,
                             unsigned short /*vertex_count*/)
{
}

// -- Programmable pipeline ---------------------------------------------------

void BgfxBackend::Set_Vertex_Shader(unsigned long /*vertex_shader*/)
{
}

void BgfxBackend::Set_Pixel_Shader(unsigned long /*pixel_shader*/)
{
}

void BgfxBackend::Set_Vertex_Shader_Constant(int /*reg*/, const void * /*data*/, int /*count*/)
{
}

void BgfxBackend::Set_Pixel_Shader_Constant(int /*reg*/, const void * /*data*/, int /*count*/)
{
}

// -- Render targets ----------------------------------------------------------

TextureClass * BgfxBackend::Create_Render_Target(int /*width*/, int /*height*/, WW3DFormat /*format*/)
{
    return nullptr;
}

void BgfxBackend::Set_Render_Target_With_Z(TextureClass * /*texture*/, ZTextureClass * /*ztexture*/)
{
}

bool BgfxBackend::Is_Render_To_Texture()
{
    return false;
}

void BgfxBackend::Set_Shadow_Map(int /*idx*/, ZTextureClass * /*ztex*/)
{
}

ZTextureClass * BgfxBackend::Get_Shadow_Map(int /*idx*/)
{
    return nullptr;
}
