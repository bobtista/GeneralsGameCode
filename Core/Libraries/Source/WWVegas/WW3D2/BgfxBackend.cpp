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
bgfx::UniformHandle g_sTex1        = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_sTex2        = BGFX_INVALID_HANDLE;
bgfx::UniformHandle g_sTex3        = BGFX_INVALID_HANDLE;
// Alpha-test parameters consumed by fs_textured_lit_atest. .x is the
// reference threshold in [0, 1]; engine writes ShaderClass alpha-ref / 255.
// Named u_atestParams (not u_alphaRef) to avoid bgfx_shader.sh's internal
// u_alphaRef4 conflict. See fs_textured_lit_atest.sc for details.
bgfx::UniformHandle g_uAtestParams = BGFX_INVALID_HANDLE;

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

std::unordered_map<const VertexBufferClass *, bgfx::VertexBufferHandle> g_vbCache;
std::unordered_map<const IndexBufferClass *,  bgfx::IndexBufferHandle>  g_ibCache;
std::unordered_map<const TextureBaseClass *,  bgfx::TextureHandle>      g_textureCache;

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
bgfx::VertexBufferHandle g_currentBgfxVB    = BGFX_INVALID_HANDLE;
bgfx::IndexBufferHandle  g_currentBgfxIB    = BGFX_INVALID_HANDLE;
unsigned short           g_currentIBOffset  = 0;

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

bgfx::VertexBufferHandle EnsureBgfxVertexBuffer(const VertexBufferClass * vb)
{
    if (vb == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }
    auto it = g_vbCache.find(vb);
    if (it != g_vbCache.end())
    {
        return it->second;
    }

    if (kBgfxSkipBufferRead)
    {
        g_vbCache[vb] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    if (vb->Type() != BUFFER_TYPE_DX8)
    {
        // Sorting / dynamic buffers handled by a different path; mark
        // as cached-invalid so we do not retry every draw.
        g_vbCache[vb] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vb->FVF_Info(), layout))
    {
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxVertexBuffer: layout build failed "
                     "for fvf=0x%08x", vb->FVF_Info().Get_FVF()));
        g_vbCache[vb] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    const unsigned vertexCount = vb->Get_Vertex_Count();
    const unsigned strideBytes = vb->FVF_Info().Get_FVF_Size();
    const unsigned totalBytes  = vertexCount * strideBytes;
    if (totalBytes == 0)
    {
        g_vbCache[vb] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    DX8VertexBufferClass * dxVb =
        const_cast<DX8VertexBufferClass *>(static_cast<const DX8VertexBufferClass *>(vb));
    IDirect3DVertexBuffer8 * d3dVb = dxVb->Get_DX8_Vertex_Buffer();
    if (d3dVb == nullptr)
    {
        g_vbCache[vb] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    BYTE * srcData = nullptr;
    HRESULT hr = d3dVb->Lock(0, 0, &srcData, D3DLOCK_READONLY);
    if (FAILED(hr) || srcData == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxVertexBuffer: d3d8 Lock failed hr=0x%08x",
                     static_cast<unsigned>(hr)));
        g_vbCache[vb] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory * mem  = bgfx::copy(srcData, totalBytes);
    d3dVb->Unlock();

    bgfx::VertexBufferHandle h = bgfx::createVertexBuffer(mem, layout);
    g_vbCache[vb] = h;

    if (!g_loggedFirstBgfxVbCreate)
    {
        g_loggedFirstBgfxVbCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxVertexBuffer first create "
                     "fvf=0x%08x stride=%u verts=%u bytes=%u handle=%s",
                     vb->FVF_Info().Get_FVF(),
                     strideBytes, vertexCount, totalBytes,
                     bgfx::isValid(h) ? "ok" : "FAILED"));
    }
    return h;
}

bgfx::IndexBufferHandle EnsureBgfxIndexBuffer(const IndexBufferClass * ib)
{
    if (ib == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }
    auto it = g_ibCache.find(ib);
    if (it != g_ibCache.end())
    {
        return it->second;
    }

    if (kBgfxSkipBufferRead)
    {
        g_ibCache[ib] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    if (ib->Type() != BUFFER_TYPE_DX8)
    {
        g_ibCache[ib] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    const unsigned indexCount = ib->Get_Index_Count();
    const unsigned totalBytes = indexCount * sizeof(uint16_t);
    if (totalBytes == 0)
    {
        g_ibCache[ib] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    DX8IndexBufferClass * dxIb =
        const_cast<DX8IndexBufferClass *>(static_cast<const DX8IndexBufferClass *>(ib));
    IDirect3DIndexBuffer8 * d3dIb = dxIb->Get_DX8_Index_Buffer();
    if (d3dIb == nullptr)
    {
        g_ibCache[ib] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    BYTE * srcData = nullptr;
    HRESULT hr = d3dIb->Lock(0, 0, &srcData, D3DLOCK_READONLY);
    if (FAILED(hr) || srcData == nullptr)
    {
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxIndexBuffer: d3d8 Lock failed hr=0x%08x",
                     static_cast<unsigned>(hr)));
        g_ibCache[ib] = BGFX_INVALID_HANDLE;
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory * mem = bgfx::copy(srcData, totalBytes);
    d3dIb->Unlock();

    bgfx::IndexBufferHandle h = bgfx::createIndexBuffer(mem);
    g_ibCache[ib] = h;

    if (!g_loggedFirstBgfxIbCreate)
    {
        g_loggedFirstBgfxIbCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] EnsureBgfxIndexBuffer first create "
                     "indices=%u bytes=%u handle=%s",
                     indexCount, totalBytes,
                     bgfx::isValid(h) ? "ok" : "FAILED"));
    }
    return h;
}

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

    // Default the cached transforms to identity until the engine writes
    // real values via Set_Transform. This keeps the first few engine
    // submits well-defined even if they fire before any matrices are
    // captured.
    IdentityMatrix(g_bgfxWorld);
    IdentityMatrix(g_bgfxView);
    IdentityMatrix(g_bgfxProj);
    g_bgfxViewProjDirty = true;

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
    g_sTex1        = bgfx::createUniform("s_tex1",        bgfx::UniformType::Sampler);
    g_sTex2        = bgfx::createUniform("s_tex2",        bgfx::UniformType::Sampler);
    g_sTex3        = bgfx::createUniform("s_tex3",        bgfx::UniformType::Sampler);
    g_uAtestParams = bgfx::createUniform("u_atestParams", bgfx::UniformType::Vec4);

    // Phase 4F.1 default 1x1 white texture (opaque ABGR=0xffffffff).
    static const uint8_t kWhitePixel[4] = { 0xff, 0xff, 0xff, 0xff };
    g_defaultWhiteTexture = bgfx::createTexture2D(
        1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
        bgfx::copy(kWhitePixel, sizeof(kWhitePixel)));

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
        if (bgfx::isValid(g_uAtestParams))
        {
            bgfx::destroy(g_uAtestParams);
            g_uAtestParams = BGFX_INVALID_HANDLE;
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
    }
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

// Set_Index_Buffer_Index_Offset is inherited from DX8Backend so the dx8
// device sees the binding.

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

void BgfxBackend::Capture_Vertex_Data(const VertexBufferClass * vb,
                                      const void * data,
                                      unsigned int size_bytes)
{
    if (!g_bgfxInitialized || vb == nullptr || data == nullptr || size_bytes == 0)
    {
        return;
    }

    bgfx::VertexLayout layout;
    if (!BuildBgfxLayoutForFVF(vb->FVF_Info(), layout))
    {
        return;
    }

    // Replace any prior cached handle for the same VB pointer. The engine
    // can rewrite a VB's contents (e.g. dynamic terrain or animated meshes
    // that go through the same VertexBufferClass instance over its lifetime),
    // so the new write supersedes whatever bgfx had.
    auto it = g_vbCache.find(vb);
    if (it != g_vbCache.end() && bgfx::isValid(it->second))
    {
        bgfx::destroy(it->second);
    }

    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::VertexBufferHandle h = bgfx::createVertexBuffer(mem, layout);
    g_vbCache[vb] = h;

    if (!g_loggedFirstBgfxVbCreate)
    {
        g_loggedFirstBgfxVbCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Vertex_Data first create "
                     "fvf=0x%08x stride=%u verts=%u bytes=%u handle=%s",
                     vb->FVF_Info().Get_FVF(),
                     vb->FVF_Info().Get_FVF_Size(),
                     vb->Get_Vertex_Count(),
                     size_bytes,
                     bgfx::isValid(h) ? "ok" : "FAILED"));
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

    auto it = g_ibCache.find(ib);
    if (it != g_ibCache.end() && bgfx::isValid(it->second))
    {
        bgfx::destroy(it->second);
    }

    const bgfx::Memory * mem = bgfx::copy(data, size_bytes);
    bgfx::IndexBufferHandle h = bgfx::createIndexBuffer(mem);
    g_ibCache[ib] = h;

    if (!g_loggedFirstBgfxIbCreate)
    {
        g_loggedFirstBgfxIbCreate = true;
        WWDEBUG_SAY(("[BgfxBackend] Capture_Index_Data first create "
                     "indices=%u bytes=%u handle=%s",
                     ib->Get_Index_Count(),
                     size_bytes,
                     bgfx::isValid(h) ? "ok" : "FAILED"));
    }
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

// Get_Shader, Set_Material, Apply_Render_State_Changes, Apply_Default_State,
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
    if (!g_loggedFirstSetProj)
    {
        g_loggedFirstSetProj = true;
        WWDEBUG_SAY(("[BgfxBackend] Set_Projection_Transform_With_Z_Bias first call "
                     "row0=%.3f,%.3f,%.3f,%.3f",
                     matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3]));
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

    // Push the engine view+projection when they change. setViewTransform
    // applies until the next change so we do not need to call it per
    // submit, only when the engine has updated either matrix.
    if (g_bgfxViewProjDirty)
    {
        bgfx::setViewTransform(kBgfxEngineView, g_bgfxView, g_bgfxProj);
        g_bgfxViewProjDirty = false;
    }

    // World matrix is per-submit. bgfx consumes the value at submit time
    // and resets the per-draw transform after each submit.
    bgfx::setTransform(g_bgfxWorld);

    if (g_currentUseTransientVB)
    {
        bgfx::setVertexBuffer(0, &g_currentTransientVB, min_vertex_index, vertex_count);
    }
    else
    {
        bgfx::setVertexBuffer(0, g_currentBgfxVB, min_vertex_index, vertex_count);
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
                             static_cast<uint32_t>(g_currentIBOffset) + start_index,
                             static_cast<uint32_t>(polygon_count) * 3);
    }

    // Phase 4F.2 / 4G.3 bind engine textures on stages 0 and 1 by
    // Set_Texture, falling back to the 1x1 white default if no real
    // texture is cached. The default white * vertex color = vertex color,
    // which keeps untextured draws visible until every texture path
    // is migrated. For stage 1, white is also the multiplicative
    // identity, so single-texture draws (which don't bind stage 1) are
    // unaffected by the second sample.
    if (bgfx::isValid(g_sTex0))
    {
        const bgfx::TextureHandle bound =
            bgfx::isValid(g_currentBgfxTexture0)
                ? g_currentBgfxTexture0
                : g_defaultWhiteTexture;
        if (bgfx::isValid(bound))
        {
            bgfx::setTexture(0, g_sTex0, bound);
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
            bgfx::setTexture(1, g_sTex1, bound);
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
            bgfx::setTexture(2, g_sTex2, bound);
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
            bgfx::setTexture(3, g_sTex3, bound);
        }
    }

    // Use the cached state if it was built; otherwise fall back to a
    // sane default that at least lets the draw run.
    const uint64_t state = (g_currentBgfxState != 0)
        ? g_currentBgfxState
        : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setState(state);

    bgfx::submit(kBgfxEngineView, g_currentBgfxProgram);

    if (!g_loggedFirstBgfxSubmit)
    {
        g_loggedFirstBgfxSubmit = true;
        WWDEBUG_SAY(("[BgfxBackend] First engine bgfx::submit "
                     "(view=%u start=%u polys=%u min_v=%u count=%u state=0x%016llx)",
                     static_cast<unsigned>(kBgfxEngineView),
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
    SubmitEngineDraw(start_index, polygon_count, min_vertex_index, vertex_count);
}

// Draw_Strip, the programmable pipeline (Set_Vertex_Shader, Set_Pixel_Shader,
// Set_Vertex_Shader_Constant, Set_Pixel_Shader_Constant), and render targets
// (Create_Render_Target, Set_Render_Target_With_Z, Is_Render_To_Texture,
// Set_Shadow_Map, Get_Shadow_Map) are inherited from DX8Backend.
