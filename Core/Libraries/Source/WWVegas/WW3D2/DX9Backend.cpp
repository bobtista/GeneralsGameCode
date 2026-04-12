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

// TheSuperHackers @feature bobtista 12/04/2026 DX9Backend Phase 2 stub.
//
// Every virtual method is a no-op (void) or returns a sensible default
// (non-void). The class exists to prove the compile-time backend selection
// mechanism works and to verify that the DX9 SDK can be fetched, built,
// and linked against WW3D2. Phase 3 fills in real implementations as
// individual rendering subsystems are migrated off DX8Wrapper statics.
//
// We deliberately #include <d3d9.h> and create an IDirect3D9 object so
// that if the FetchContent + link pipeline is broken, we get a compile or
// link error during Phase 2 rather than discovering it deep inside Phase 3.

#include "DX9Backend.h"
#include "vector3.h"
#include <d3d9.h>

DX9Backend::DX9Backend()
    : m_pD3D(nullptr)
    , m_pDevice(nullptr)
    , m_hwnd(nullptr)
    , m_deviceLost(false)
    , m_fogEnabled(false)
    , m_ambient(0.0f, 0.0f, 0.0f)
    , m_lightEnv(nullptr)
{
}

DX9Backend::~DX9Backend()
{
    Shutdown();
}

void DX9Backend::Initialize(void * hwnd, int width, int height)
{
    m_hwnd = hwnd;
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (m_pD3D == nullptr)
        return;

    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof(pp));
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = static_cast<UINT>(width);
    pp.BackBufferHeight = static_cast<UINT>(height);
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr = m_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        static_cast<HWND>(hwnd),
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &pp, &m_pDevice);

    if (FAILED(hr))
    {
        hr = m_pD3D->CreateDevice(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            static_cast<HWND>(hwnd),
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            &pp, &m_pDevice);
    }

    if (FAILED(hr))
    {
        m_pD3D->Release();
        m_pD3D = nullptr;
    }
}

void DX9Backend::Shutdown()
{
    if (m_pDevice != nullptr)
    {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
    if (m_pD3D != nullptr)
    {
        m_pD3D->Release();
        m_pD3D = nullptr;
    }
}

// -- Device state queries ----------------------------------------------------

bool DX9Backend::Is_Device_Lost() const
{
    return m_deviceLost;
}

bool DX9Backend::Has_Stencil()
{
    return true;
}

WW3DFormat DX9Backend::Get_Back_Buffer_Format()
{
    return WW3D_FORMAT_X8R8G8B8;
}

SurfaceClass * DX9Backend::Get_Back_Buffer(unsigned int /*num*/)
{
    return nullptr;
}

void DX9Backend::Set_Gamma(float /*gamma*/, float /*bright*/, float /*contrast*/,
                           bool /*calibrate*/, bool /*uselimit*/)
{
}

// -- Frame lifecycle ---------------------------------------------------------

void DX9Backend::Begin_Scene()
{
}

void DX9Backend::End_Scene(bool /*flip_frame*/)
{
}

void DX9Backend::Flip_To_Primary()
{
}

void DX9Backend::Clear(bool /*clear_color*/, bool /*clear_z_stencil*/,
                       const Vector3 & /*color*/,
                       float /*dest_alpha*/, float /*z*/, unsigned int /*stencil*/)
{
}

void DX9Backend::Set_Viewport(const RenderBackendViewport & /*viewport*/)
{
}

// -- Vertex / index buffers --------------------------------------------------

void DX9Backend::Set_Vertex_Buffer(const VertexBufferClass * /*vb*/, unsigned int /*stream*/)
{
}

void DX9Backend::Set_Vertex_Buffer(const DynamicVBAccessClass & /*vba*/)
{
}

void DX9Backend::Set_Index_Buffer(const IndexBufferClass * /*ib*/, unsigned short /*index_base_offset*/)
{
}

void DX9Backend::Set_Index_Buffer(const DynamicIBAccessClass & /*iba*/, unsigned short /*index_base_offset*/)
{
}

void DX9Backend::Set_Index_Buffer_Index_Offset(unsigned int /*offset*/)
{
}

// -- State: shaders, materials, textures ------------------------------------

void DX9Backend::Set_Shader(const ShaderClass & /*shader*/)
{
}

void DX9Backend::Get_Shader(ShaderClass & /*shader*/)
{
}

void DX9Backend::Set_Material(const VertexMaterialClass * /*material*/)
{
}

void DX9Backend::Set_Texture(unsigned int /*stage*/, TextureBaseClass * /*texture*/)
{
}

void DX9Backend::Apply_Render_State_Changes()
{
}

void DX9Backend::Apply_Default_State()
{
}

void DX9Backend::Invalidate_Cached_Render_States()
{
}

void DX9Backend::Set_Blend_Op(BlendOp /*op*/)
{
}

void DX9Backend::Set_Blend_Factors(BlendFactor /*src*/, BlendFactor /*dest*/)
{
}

void DX9Backend::Set_Color_Write_Enable(bool /*red*/, bool /*green*/, bool /*blue*/, bool /*alpha*/)
{
}

void DX9Backend::Set_Alpha_Blend_Enable(bool /*enable*/)
{
}

void DX9Backend::Show_Hardware_Cursor(bool /*show*/)
{
}

void DX9Backend::Set_Hardware_Cursor_Image(int /*hotspot_x*/, int /*hotspot_y*/, SurfaceClass * /*surface*/)
{
}

void DX9Backend::Set_Hardware_Cursor_Position(int /*x*/, int /*y*/)
{
}

void DX9Backend::Set_Stencil_Enable(bool /*enable*/)
{
}

void DX9Backend::Set_Stencil_Func(CompareFunc /*func*/)
{
}

void DX9Backend::Set_Stencil_Ref(unsigned int /*ref*/)
{
}

void DX9Backend::Set_Stencil_Mask(unsigned int /*mask*/)
{
}

void DX9Backend::Set_Stencil_Write_Mask(unsigned int /*mask*/)
{
}

void DX9Backend::Set_Stencil_Pass_Op(StencilOp /*op*/)
{
}

void DX9Backend::Set_Stencil_Fail_Op(StencilOp /*op*/)
{
}

void DX9Backend::Set_Stencil_ZFail_Op(StencilOp /*op*/)
{
}

// -- Transforms --------------------------------------------------------------

void DX9Backend::Set_Transform(TransformKind /*transform*/, const Matrix4x4 & /*m*/)
{
}

void DX9Backend::Set_Transform(TransformKind /*transform*/, const Matrix3D & /*m*/)
{
}

void DX9Backend::Get_Transform(TransformKind /*transform*/, Matrix4x4 & /*m*/)
{
}

void DX9Backend::Set_World_Identity()
{
}

void DX9Backend::Set_View_Identity()
{
}

bool DX9Backend::Is_World_Identity()
{
    return true;
}

bool DX9Backend::Is_View_Identity()
{
    return true;
}

void DX9Backend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & /*matrix*/,
                                                      float /*znear*/, float /*zfar*/)
{
}

// -- Lighting and fog --------------------------------------------------------

void DX9Backend::Set_Light(unsigned int /*index*/, const LightClass & /*light*/)
{
}

void DX9Backend::Set_Ambient(const Vector3 & color)
{
    m_ambient = color;
}

const Vector3 & DX9Backend::Get_Ambient() const
{
    return m_ambient;
}

void DX9Backend::Set_Fog(bool enable, const Vector3 & /*color*/,
                         float /*start*/, float /*end*/)
{
    m_fogEnabled = enable;
}

bool DX9Backend::Get_Fog_Enable() const
{
    return m_fogEnabled;
}

void DX9Backend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    m_lightEnv = light_env;
}

LightEnvironmentClass * DX9Backend::Get_Light_Environment() const
{
    return m_lightEnv;
}

// -- Draw calls --------------------------------------------------------------

void DX9Backend::Draw_Triangles(unsigned short /*start_index*/,
                                unsigned short /*polygon_count*/,
                                unsigned short /*min_vertex_index*/,
                                unsigned short /*vertex_count*/)
{
}

void DX9Backend::Draw_Triangles(unsigned int /*buffer_type*/,
                                unsigned short /*start_index*/,
                                unsigned short /*polygon_count*/,
                                unsigned short /*min_vertex_index*/,
                                unsigned short /*vertex_count*/)
{
}

void DX9Backend::Draw_Strip(unsigned short /*start_index*/,
                            unsigned short /*index_count*/,
                            unsigned short /*min_vertex_index*/,
                            unsigned short /*vertex_count*/)
{
}

// -- Programmable pipeline ---------------------------------------------------

void DX9Backend::Set_Vertex_Shader(unsigned long /*vertex_shader*/)
{
}

void DX9Backend::Set_Pixel_Shader(unsigned long /*pixel_shader*/)
{
}

void DX9Backend::Set_Vertex_Shader_Constant(int /*reg*/, const void * /*data*/, int /*count*/)
{
}

void DX9Backend::Set_Pixel_Shader_Constant(int /*reg*/, const void * /*data*/, int /*count*/)
{
}

// -- Render targets ----------------------------------------------------------

TextureClass * DX9Backend::Create_Render_Target(int /*width*/, int /*height*/, WW3DFormat /*format*/)
{
    return nullptr;
}

void DX9Backend::Set_Render_Target_With_Z(TextureClass * /*texture*/, ZTextureClass * /*ztexture*/)
{
}

bool DX9Backend::Is_Render_To_Texture()
{
    return false;
}

void DX9Backend::Set_Shadow_Map(int /*idx*/, ZTextureClass * /*ztex*/)
{
}

ZTextureClass * DX9Backend::Get_Shadow_Map(int /*idx*/)
{
    return nullptr;
}
