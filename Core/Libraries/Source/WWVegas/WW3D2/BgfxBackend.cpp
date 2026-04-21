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

// Vertex layout used by the test triangle. Position + packed RGBA color.
// Initialized in Initialize since bgfx::VertexLayout::begin needs bgfx to be
// up and running (it queries the active renderer for the pos type).
bgfx::VertexLayout g_triangleLayout;

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

    const bgfx::RendererType::Enum selected = bgfx::getRendererType();
    const char * rendererName = bgfx::getRendererName(selected);
    WWDEBUG_SAY(("[BgfxBackend] bgfx::init OK on debug window "
                 "(renderer=%s, %dx%d, hwnd=%p, passthrough=%s).",
                 rendererName, kBgfxWindowWidth, kBgfxWindowHeight,
                 g_bgfxWindow,
                 bgfx::isValid(g_passthroughProgram) ? "ok" : "FAILED"));
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

void BgfxBackend::Set_Vertex_Buffer(const VertexBufferClass * /*vb*/, unsigned int /*stream*/)
{
}

void BgfxBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & /*vba*/)
{
}

void BgfxBackend::Set_Index_Buffer(const IndexBufferClass * /*ib*/, unsigned short /*index_base_offset*/)
{
}

void BgfxBackend::Set_Index_Buffer(const DynamicIBAccessClass & /*iba*/, unsigned short /*index_base_offset*/)
{
}

void BgfxBackend::Set_Index_Buffer_Index_Offset(unsigned int /*offset*/)
{
}

// -- State: shaders, materials, textures ------------------------------------

void BgfxBackend::Set_Shader(const ShaderClass & /*shader*/)
{
}

void BgfxBackend::Get_Shader(ShaderClass & /*shader*/)
{
}

void BgfxBackend::Set_Material(const VertexMaterialClass * /*material*/)
{
}

void BgfxBackend::Set_Texture(unsigned int /*stage*/, TextureBaseClass * /*texture*/)
{
}

void BgfxBackend::Apply_Render_State_Changes()
{
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
                                 unsigned short /*polygon_count*/,
                                 unsigned short /*min_vertex_index*/,
                                 unsigned short /*vertex_count*/)
{
}

void BgfxBackend::Draw_Triangles(unsigned int /*buffer_type*/,
                                 unsigned short /*start_index*/,
                                 unsigned short /*polygon_count*/,
                                 unsigned short /*min_vertex_index*/,
                                 unsigned short /*vertex_count*/)
{
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
