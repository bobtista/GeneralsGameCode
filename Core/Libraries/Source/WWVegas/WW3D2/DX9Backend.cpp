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

// TheSuperHackers @feature bobtista 12/04/2026 DX9Backend implementation.
//
// Initial pass: forwards all rendering calls through DX8Wrapper so the game
// renders identically to the DX8 build. The DX9 device created in
// Initialize() is available for incremental migration in follow-up work.
// The DX8Wrapper still owns the real device and all internal state (render
// state cache, sorting system, vertex/index buffer binding, etc.).
//
// This approach gets pixels on screen immediately, matching the DX8Backend
// forwarding pattern, while establishing the DX9 SDK dependency and build
// pipeline.

#include "DX9Backend.h"

#include "dx8wrapper.h"
#include "vector3.h"
#include "matrix4.h"
#include "matrix3d.h"
#include "light.h"
#include "lightenvironment.h"

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

// -- Backend lifecycle --------------------------------------------------------

void DX9Backend::Initialize(void * hwnd, int width, int height)
{
    m_hwnd = hwnd;

    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (m_pD3D == nullptr)
    {
        return;
    }

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
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        static_cast<HWND>(hwnd),
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &pp,
        &m_pDevice);

    if (FAILED(hr))
    {
        hr = m_pD3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            static_cast<HWND>(hwnd),
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            &pp,
            &m_pDevice);
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

// -- Device state queries -----------------------------------------------------

bool DX9Backend::Is_Device_Lost() const
{
    return DX8Wrapper::Is_Device_Lost();
}

bool DX9Backend::Has_Stencil()
{
    return DX8Wrapper::Has_Stencil();
}

WW3DFormat DX9Backend::Get_Back_Buffer_Format()
{
    return DX8Wrapper::getBackBufferFormat();
}

SurfaceClass * DX9Backend::Get_Back_Buffer(unsigned int num)
{
    return DX8Wrapper::_Get_DX8_Back_Buffer(num);
}

void DX9Backend::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    DX8Wrapper::Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

// -- Frame lifecycle ----------------------------------------------------------

void DX9Backend::Begin_Scene()
{
    DX8Wrapper::Begin_Scene();
}

void DX9Backend::End_Scene(bool flip_frame)
{
    DX8Wrapper::End_Scene(flip_frame);
}

void DX9Backend::Flip_To_Primary()
{
    DX8Wrapper::Flip_To_Primary();
}

void DX9Backend::Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil)
{
    DX8Wrapper::Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void DX9Backend::Set_Viewport(const RenderBackendViewport & viewport)
{
    D3DVIEWPORT8 vp;
    vp.X      = viewport.x;
    vp.Y      = viewport.y;
    vp.Width  = viewport.width;
    vp.Height = viewport.height;
    vp.MinZ   = viewport.min_z;
    vp.MaxZ   = viewport.max_z;
    DX8Wrapper::Set_Viewport(&vp);
}

// -- Vertex / index buffers ---------------------------------------------------

void DX9Backend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    DX8Wrapper::Set_Vertex_Buffer(vb, stream);
}

void DX9Backend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    DX8Wrapper::Set_Vertex_Buffer(vba);
}

void DX9Backend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    DX8Wrapper::Set_Index_Buffer(ib, index_base_offset);
}

void DX9Backend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Wrapper::Set_Index_Buffer(iba, index_base_offset);
}

void DX9Backend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    DX8Wrapper::Set_Index_Buffer_Index_Offset(offset);
}

// -- State: shaders, materials, textures -------------------------------------

void DX9Backend::Set_Shader(const ShaderClass & shader)
{
    DX8Wrapper::Set_Shader(shader);
}

void DX9Backend::Get_Shader(ShaderClass & shader)
{
    DX8Wrapper::Get_Shader(shader);
}

void DX9Backend::Set_Material(const VertexMaterialClass * material)
{
    DX8Wrapper::Set_Material(material);
}

void DX9Backend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Wrapper::Set_Texture(stage, texture);
}

void DX9Backend::Apply_Render_State_Changes()
{
    DX8Wrapper::Apply_Render_State_Changes();
}

void DX9Backend::Apply_Default_State()
{
    DX8Wrapper::Apply_Default_State();
}

void DX9Backend::Invalidate_Cached_Render_States()
{
    DX8Wrapper::Invalidate_Cached_Render_States();
}

void DX9Backend::Set_Blend_Op(BlendOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, static_cast<unsigned>(op));
}

void DX9Backend::Set_Blend_Factors(BlendFactor src, BlendFactor dest)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, static_cast<unsigned>(src));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, static_cast<unsigned>(dest));
}

void DX9Backend::Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha)
{
    unsigned mask = 0;
    if (red)
    {
        mask |= D3DCOLORWRITEENABLE_RED;
    }
    if (green)
    {
        mask |= D3DCOLORWRITEENABLE_GREEN;
    }
    if (blue)
    {
        mask |= D3DCOLORWRITEENABLE_BLUE;
    }
    if (alpha)
    {
        mask |= D3DCOLORWRITEENABLE_ALPHA;
    }
    DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, mask);
}

void DX9Backend::Set_Alpha_Blend_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Show_Hardware_Cursor(bool show)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr)
    {
        pDev->ShowCursor(show ? TRUE : FALSE);
    }
}

void DX9Backend::Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, SurfaceClass * surface)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr && surface != nullptr)
    {
        pDev->SetCursorProperties(
            static_cast<UINT>(hotspot_x),
            static_cast<UINT>(hotspot_y),
            surface->Peek_D3D_Surface());
    }
}

void DX9Backend::Set_Hardware_Cursor_Position(int x, int y)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr)
    {
        pDev->SetCursorPosition(x, y, D3DCURSOR_IMMEDIATE_UPDATE);
    }
}

void DX9Backend::Set_Stencil_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, enable ? TRUE : FALSE);
}

void DX9Backend::Set_Stencil_Func(CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFUNC, static_cast<unsigned>(func));
}

void DX9Backend::Set_Stencil_Ref(unsigned int ref)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILREF, ref);
}

void DX9Backend::Set_Stencil_Mask(unsigned int mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILMASK, mask);
}

void DX9Backend::Set_Stencil_Write_Mask(unsigned int mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILWRITEMASK, mask);
}

void DX9Backend::Set_Stencil_Pass_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILPASS, static_cast<unsigned>(op));
}

void DX9Backend::Set_Stencil_Fail_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFAIL, static_cast<unsigned>(op));
}

void DX9Backend::Set_Stencil_ZFail_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILZFAIL, static_cast<unsigned>(op));
}

// -- Transforms ---------------------------------------------------------------

void DX9Backend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    DX8Wrapper::Set_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX9Backend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    DX8Wrapper::Set_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX9Backend::Get_Transform(TransformKind transform, Matrix4x4 & m)
{
    DX8Wrapper::Get_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX9Backend::Set_World_Identity()
{
    DX8Wrapper::Set_World_Identity();
}

void DX9Backend::Set_View_Identity()
{
    DX8Wrapper::Set_View_Identity();
}

bool DX9Backend::Is_World_Identity()
{
    return DX8Wrapper::Is_World_Identity();
}

bool DX9Backend::Is_View_Identity()
{
    return DX8Wrapper::Is_View_Identity();
}

void DX9Backend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
    DX8Wrapper::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);
}

// -- Lighting and fog ---------------------------------------------------------

void DX9Backend::Set_Light(unsigned int index, const LightClass & light)
{
    DX8Wrapper::Set_Light(index, light);
}

void DX9Backend::Set_Ambient(const Vector3 & color)
{
    m_ambient = color;
    DX8Wrapper::Set_Ambient(color);
}

const Vector3 & DX9Backend::Get_Ambient() const
{
    return DX8Wrapper::Get_Ambient();
}

void DX9Backend::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
    m_fogEnabled = enable;
    DX8Wrapper::Set_Fog(enable, color, start, end);
}

bool DX9Backend::Get_Fog_Enable() const
{
    return DX8Wrapper::Get_Fog_Enable();
}

void DX9Backend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    m_lightEnv = light_env;
    DX8Wrapper::Set_Light_Environment(light_env);
}

LightEnvironmentClass * DX9Backend::Get_Light_Environment() const
{
    return DX8Wrapper::Get_Light_Environment();
}

// -- Draw calls ---------------------------------------------------------------

void DX9Backend::Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Wrapper::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
}

void DX9Backend::Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Wrapper::Draw_Triangles(buffer_type, start_index, polygon_count, min_vertex_index, vertex_count);
}

void DX9Backend::Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count)
{
    DX8Wrapper::Draw_Strip(start_index, index_count, min_vertex_index, vertex_count);
}

// -- Programmable pipeline ----------------------------------------------------

void DX9Backend::Set_Vertex_Shader(unsigned long vertex_shader)
{
    DX8Wrapper::Set_Vertex_Shader(static_cast<DWORD>(vertex_shader));
}

void DX9Backend::Set_Pixel_Shader(unsigned long pixel_shader)
{
    DX8Wrapper::Set_Pixel_Shader(static_cast<DWORD>(pixel_shader));
}

void DX9Backend::Set_Vertex_Shader_Constant(int reg, const void * data, int count)
{
    DX8Wrapper::Set_Vertex_Shader_Constant(reg, data, count);
}

void DX9Backend::Set_Pixel_Shader_Constant(int reg, const void * data, int count)
{
    DX8Wrapper::Set_Pixel_Shader_Constant(reg, data, count);
}

// -- Render targets -----------------------------------------------------------

TextureClass * DX9Backend::Create_Render_Target(int width, int height, WW3DFormat format)
{
    return DX8Wrapper::Create_Render_Target(width, height, format);
}

void DX9Backend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    DX8Wrapper::Set_Render_Target_With_Z(texture, ztexture);
}

bool DX9Backend::Is_Render_To_Texture()
{
    return DX8Wrapper::Is_Render_To_Texture();
}

void DX9Backend::Set_Shadow_Map(int idx, ZTextureClass * ztex)
{
    DX8Wrapper::Set_Shadow_Map(idx, ztex);
}

ZTextureClass * DX9Backend::Get_Shadow_Map(int idx)
{
    return DX8Wrapper::Get_Shadow_Map(idx);
}
