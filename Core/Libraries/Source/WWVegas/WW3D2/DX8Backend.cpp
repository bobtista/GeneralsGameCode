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

// TheSuperHackers @refactor bobtista 10/04/2026 DX8Backend forwarding adapter.
// Every method in this file is a one-line trampoline to the existing
// DX8Wrapper static API. Keep it that way — if behavior needs to change it
// should change in DX8Wrapper, not here.

#include "DX8Backend.h"

#include "dx8wrapper.h"
#include "vector3.h"
#include "matrix4.h"
#include "matrix3d.h"
#include "light.h"
#include "lightenvironment.h"
#include "texture.h"
#include <string.h>

DX8Backend::DX8Backend()
{
}

DX8Backend::~DX8Backend()
{
}

// -- Backend lifecycle -------------------------------------------------------
//
// DX8Backend is a passive forwarder: DX8Wrapper::Init has already done the
// real device creation before this runs, so there is nothing to do here.
// These exist so the abstract interface has a uniform lifecycle hook.

void DX8Backend::Initialize(void * /*hwnd*/, int /*width*/, int /*height*/)
{
}

void DX8Backend::Shutdown()
{
}

// -- Device state queries ----------------------------------------------------

bool DX8Backend::Is_Device_Lost() const
{
    return DX8Wrapper::Is_Device_Lost();
}

bool DX8Backend::Has_Stencil() const
{
    return DX8Wrapper::Has_Stencil();
}

WW3DFormat DX8Backend::Get_Back_Buffer_Format() const
{
    return DX8Wrapper::getBackBufferFormat();
}

SurfaceClass * DX8Backend::Get_Back_Buffer(unsigned int num) const
{
    return DX8Wrapper::_Get_DX8_Back_Buffer(num);
}

void DX8Backend::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    DX8Wrapper::Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

// -- Frame lifecycle ---------------------------------------------------------
//
// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4 session 2b.
// Begin_Scene/End_Scene are intentionally empty during the bgfx cutover.
// ww3d.cpp's WW3D::Begin_Render/End_Render call DX8Wrapper::Begin_Scene /
// End_Scene directly because the D3D8 device is still the primary renderer
// in both the =dx8 and =bgfx builds. The g_renderBackend->Begin_Scene /
// End_Scene pair is a parallel per-frame hook used by BgfxBackend to call
// bgfx::touch / bgfx::frame alongside the DX8 pipeline. When DX8 is finally
// removed (post-Phase 4) these will re-acquire their original forwarding
// behavior.

void DX8Backend::Begin_Scene()
{
}

void DX8Backend::End_Scene(bool /*flip_frame*/)
{
}

void DX8Backend::Flip_To_Primary()
{
    DX8Wrapper::Flip_To_Primary();
}

void DX8Backend::Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil)
{
    DX8Wrapper::Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void DX8Backend::Set_Viewport(const RenderBackendViewport & viewport)
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

// -- Vertex / index buffers --------------------------------------------------

void DX8Backend::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    DX8Wrapper::Set_Vertex_Buffer(vb, stream);
}

void DX8Backend::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    DX8Wrapper::Set_Vertex_Buffer(vba);
}

void DX8Backend::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    DX8Wrapper::Set_Index_Buffer(ib, index_base_offset);
}

void DX8Backend::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    DX8Wrapper::Set_Index_Buffer(iba, index_base_offset);
}

void DX8Backend::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    DX8Wrapper::Set_Index_Buffer_Index_Offset(offset);
}

// -- State: shaders, materials, textures ------------------------------------

void DX8Backend::Set_Shader(const ShaderClass & shader)
{
    DX8Wrapper::Set_Shader(shader);
}

void DX8Backend::Get_Shader(ShaderClass & shader)
{
    DX8Wrapper::Get_Shader(shader);
}

void DX8Backend::Set_Material(const VertexMaterialClass * material)
{
    DX8Wrapper::Set_Material(material);
}

void DX8Backend::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    DX8Wrapper::Set_Texture(stage, texture);
}

void DX8Backend::Apply_Render_State_Changes()
{
    DX8Wrapper::Apply_Render_State_Changes();
}

void DX8Backend::Apply_Default_State()
{
    DX8Wrapper::Apply_Default_State();
}

void DX8Backend::Invalidate_Cached_Render_States()
{
    DX8Wrapper::Invalidate_Cached_Render_States();
}

void DX8Backend::Set_Blend_Op(BlendOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_BLENDOP, static_cast<unsigned>(op));
}

void DX8Backend::Set_Blend_Factors(BlendFactor src, BlendFactor dest)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,  static_cast<unsigned>(src));
    DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, static_cast<unsigned>(dest));
}

void DX8Backend::Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha)
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

void DX8Backend::Set_Alpha_Blend_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Show_Hardware_Cursor(bool show)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr)
    {
        pDev->ShowCursor(show ? TRUE : FALSE);
    }
}

void DX8Backend::Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, SurfaceClass * surface)
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

void DX8Backend::Set_Hardware_Cursor_Position(int x, int y)
{
    IDirect3DDevice8 * pDev = DX8Wrapper::_Get_D3D_Device8();
    if (pDev != nullptr)
    {
        pDev->SetCursorPosition(x, y, D3DCURSOR_IMMEDIATE_UPDATE);
    }
}

void DX8Backend::Set_Stencil_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Stencil_Func(CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFUNC, static_cast<unsigned>(func));
}

void DX8Backend::Set_Stencil_Ref(unsigned int ref)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILREF, ref);
}

void DX8Backend::Set_Stencil_Mask(unsigned int mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILMASK, mask);
}

void DX8Backend::Set_Stencil_Write_Mask(unsigned int mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILWRITEMASK, mask);
}

void DX8Backend::Set_Stencil_Pass_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILPASS, static_cast<unsigned>(op));
}

void DX8Backend::Set_Stencil_Fail_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILFAIL, static_cast<unsigned>(op));
}

void DX8Backend::Set_Stencil_ZFail_Op(StencilOp op)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_STENCILZFAIL, static_cast<unsigned>(op));
}

void DX8Backend::Set_Z_Bias(int bias)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZBIAS, static_cast<unsigned>(bias));
}

void DX8Backend::Set_Fill_Mode(FillMode mode)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_FILLMODE, static_cast<unsigned>(mode));
}

void DX8Backend::Set_Depth_Test_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Depth_Write_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Depth_Func(CompareFunc func)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, static_cast<unsigned>(func));
}

void DX8Backend::Set_Color_Write_Mask(unsigned mask)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, mask);
}

void DX8Backend::Set_Lighting_Enable(bool enable)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_LIGHTING, enable ? TRUE : FALSE);
}

void DX8Backend::Set_Texture_Factor(unsigned argb)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_TEXTUREFACTOR, argb);
}

void DX8Backend::Set_Cull_Mode(CullMode mode)
{
    DX8Wrapper::Set_DX8_Render_State(D3DRS_CULLMODE, static_cast<unsigned>(mode));
}

// -- Transforms --------------------------------------------------------------

void DX8Backend::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    DX8Wrapper::Set_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX8Backend::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    DX8Wrapper::Set_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX8Backend::Get_Transform(TransformKind transform, Matrix4x4 & m) const
{
    DX8Wrapper::Get_Transform(static_cast<D3DTRANSFORMSTATETYPE>(transform), m);
}

void DX8Backend::Set_World_Identity()
{
    DX8Wrapper::Set_World_Identity();
}

void DX8Backend::Set_View_Identity()
{
    DX8Wrapper::Set_View_Identity();
}

bool DX8Backend::Is_World_Identity() const
{
    return DX8Wrapper::Is_World_Identity();
}

bool DX8Backend::Is_View_Identity() const
{
    return DX8Wrapper::Is_View_Identity();
}

void DX8Backend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
    DX8Wrapper::Set_Projection_Transform_With_Z_Bias(matrix, znear, zfar);
}

// -- Lighting and fog --------------------------------------------------------

void DX8Backend::Set_Light(unsigned int index, const LightClass & light)
{
    DX8Wrapper::Set_Light(index, light);
}

void DX8Backend::Set_Ambient(const Vector3 & color)
{
    DX8Wrapper::Set_Ambient(color);
}

const Vector3 & DX8Backend::Get_Ambient() const
{
    return DX8Wrapper::Get_Ambient();
}

void DX8Backend::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
    DX8Wrapper::Set_Fog(enable, color, start, end);
}

bool DX8Backend::Get_Fog_Enable() const
{
    return DX8Wrapper::Get_Fog_Enable();
}

void DX8Backend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    DX8Wrapper::Set_Light_Environment(light_env);
}

LightEnvironmentClass * DX8Backend::Get_Light_Environment() const
{
    return DX8Wrapper::Get_Light_Environment();
}

// -- Draw calls --------------------------------------------------------------

void DX8Backend::Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Wrapper::Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
}

void DX8Backend::Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count)
{
    DX8Wrapper::Draw_Triangles(buffer_type, start_index, polygon_count, min_vertex_index, vertex_count);
}

void DX8Backend::Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count)
{
    DX8Wrapper::Draw_Strip(start_index, index_count, min_vertex_index, vertex_count);
}

// -- Programmable pipeline ---------------------------------------------------

void DX8Backend::Set_Vertex_Shader(unsigned long vertex_shader)
{
    DX8Wrapper::Set_Vertex_Shader(static_cast<DWORD>(vertex_shader));
}

void DX8Backend::Set_Pixel_Shader(unsigned long pixel_shader)
{
    DX8Wrapper::Set_Pixel_Shader(static_cast<DWORD>(pixel_shader));
}

void DX8Backend::Set_Vertex_Shader_Constant(int reg, const void * data, int count)
{
    DX8Wrapper::Set_Vertex_Shader_Constant(reg, data, count);
}

void DX8Backend::Set_Pixel_Shader_Constant(int reg, const void * data, int count)
{
    DX8Wrapper::Set_Pixel_Shader_Constant(reg, data, count);
}

// -- Render targets ----------------------------------------------------------

TextureClass * DX8Backend::Create_Render_Target(int width, int height, WW3DFormat format)
{
    TextureClass * tex = DX8Wrapper::Create_Render_Target(width, height, format);
    return tex;
}

void DX8Backend::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    DX8Wrapper::Set_Render_Target_With_Z(texture, ztexture);
}

bool DX8Backend::Is_Render_To_Texture() const
{
    return DX8Wrapper::Is_Render_To_Texture();
}

void DX8Backend::Set_Shadow_Map(int idx, ZTextureClass * ztex)
{
    DX8Wrapper::Set_Shadow_Map(idx, ztex);
}

ZTextureClass * DX8Backend::Get_Shadow_Map(int idx) const
{
    return DX8Wrapper::Get_Shadow_Map(idx);
}

// -- Resource creation (Phase 5 asset ingress) -------------------------------
//
// RenderResource.id encoding for DX8Backend: the raw IDirect3D*8 pointer
// cast to uint64. This keeps the handle trivially invertible back to the
// D3D8 resource type for any DX8-specific code that needs it, and lets
// the BgfxBackend ref-popup mirror path look up the D3D8 pointer from the
// returned handle without any side table.

RenderResource DX8Backend::Create_Texture(const TextureDesc & desc)
{
    const MipCountType mip_count = static_cast<MipCountType>(desc.mip_count);
    IDirect3DTexture8 * tex = DX8Wrapper::_Create_DX8_Texture(
        desc.width, desc.height, desc.format, mip_count,
        D3DPOOL_MANAGED, desc.is_render_target);

    if (tex != nullptr && !desc.is_render_target && desc.mips != nullptr) {
        for (unsigned char level = 0; level < desc.mip_count; ++level) {
            const MipSlice & slice = desc.mips[level];
            if (slice.data == nullptr || slice.size_bytes == 0) {
                continue;
            }
            D3DLOCKED_RECT locked;
            if (SUCCEEDED(tex->LockRect(level, &locked, nullptr, 0))) {
                if (slice.pitch != 0 && static_cast<unsigned>(locked.Pitch) == slice.pitch) {
                    memcpy(locked.pBits, slice.data, slice.size_bytes);
                } else if (slice.pitch != 0) {
                    const unsigned rows = slice.size_bytes / slice.pitch;
                    for (unsigned row = 0; row < rows; ++row) {
                        memcpy(
                            static_cast<unsigned char *>(locked.pBits) + row * locked.Pitch,
                            static_cast<const unsigned char *>(slice.data) + row * slice.pitch,
                            slice.pitch);
                    }
                } else {
                    // Compressed: the caller's size_bytes already accounts
                    // for block-compressed row packing.
                    memcpy(locked.pBits, slice.data, slice.size_bytes);
                }
                tex->UnlockRect(level);
            }
        }
    }

    RenderResource rr;
    rr.id = reinterpret_cast<unsigned __int64>(tex);
    return rr;
}

RenderResource DX8Backend::Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data)
{
    IDirect3DVertexBuffer8 * vb = nullptr;
    const DWORD usage = desc.dynamic ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : D3DUSAGE_WRITEONLY;
    const D3DPOOL pool = desc.dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
    DX8Wrapper::_Get_D3D_Device8()->CreateVertexBuffer(
        desc.size_bytes, usage, desc.layout.fvf, pool, &vb);

    if (vb != nullptr && initial_data != nullptr)
    {
        unsigned char * dst = nullptr;
        if (SUCCEEDED(vb->Lock(0, desc.size_bytes, &dst, 0)))
        {
            memcpy(dst, initial_data, desc.size_bytes);
            vb->Unlock();
        }
    }

    RenderResource rr;
    rr.id = reinterpret_cast<unsigned __int64>(vb);
    return rr;
}

RenderResource DX8Backend::Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit)
{
    IDirect3DIndexBuffer8 * ib = nullptr;
    const DWORD usage = desc.dynamic ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : D3DUSAGE_WRITEONLY;
    const D3DPOOL pool = desc.dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
    const D3DFORMAT fmt = indices_are_32bit ? D3DFMT_INDEX32 : D3DFMT_INDEX16;
    DX8Wrapper::_Get_D3D_Device8()->CreateIndexBuffer(
        desc.size_bytes, usage, fmt, pool, &ib);

    if (ib != nullptr && initial_data != nullptr)
    {
        unsigned char * dst = nullptr;
        if (SUCCEEDED(ib->Lock(0, desc.size_bytes, &dst, 0)))
        {
            memcpy(dst, initial_data, desc.size_bytes);
            ib->Unlock();
        }
    }

    RenderResource rr;
    rr.id = reinterpret_cast<unsigned __int64>(ib);
    return rr;
}

RenderResource DX8Backend::Create_Dynamic_Vertex_Buffer(const BufferDesc & desc)
{
    BufferDesc copy = desc;
    copy.dynamic = true;
    return Create_Vertex_Buffer(copy, nullptr);
}

RenderResource DX8Backend::Create_Dynamic_Index_Buffer(const BufferDesc & desc, bool indices_are_32bit)
{
    BufferDesc copy = desc;
    copy.dynamic = true;
    return Create_Index_Buffer(copy, nullptr, indices_are_32bit);
}

void * DX8Backend::Map_Dynamic(RenderResource h, unsigned int offset, unsigned int size, bool discard)
{
    // The DX8 backend cannot distinguish vertex vs index buffers from the
    // handle alone, but IDirect3DVertexBuffer8::Lock and
    // IDirect3DIndexBuffer8::Lock share a signature at the binary level
    // through the same vtable offset on their common IUnknown-derived
    // layout. However, to avoid UB we require the caller to only invoke
    // Map_Dynamic on handles that were returned by a Create_Dynamic_*
    // method — which is how DynamicVBAccessClass / DynamicIBAccessClass
    // already use it. We route through IDirect3DVertexBuffer8's interface
    // because the Lock signature matches both.
    IDirect3DVertexBuffer8 * buf = reinterpret_cast<IDirect3DVertexBuffer8 *>(h.id);
    if (buf == nullptr) {
        return nullptr;
    }
    const DWORD flags = discard ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
    unsigned char * ptr = nullptr;
    if (FAILED(buf->Lock(offset, size, &ptr, flags))) {
        return nullptr;
    }
    return ptr;
}

void DX8Backend::Unmap_Dynamic(RenderResource h)
{
    IDirect3DVertexBuffer8 * buf = reinterpret_cast<IDirect3DVertexBuffer8 *>(h.id);
    if (buf != nullptr) {
        buf->Unlock();
    }
}

void DX8Backend::Update_Sub_Range(RenderResource h, unsigned int offset, const void * data, unsigned int size)
{
    IDirect3DVertexBuffer8 * buf = reinterpret_cast<IDirect3DVertexBuffer8 *>(h.id);
    if (buf == nullptr || data == nullptr || size == 0) {
        return;
    }
    unsigned char * dst = nullptr;
    if (SUCCEEDED(buf->Lock(offset, size, &dst, D3DLOCK_NOOVERWRITE))) {
        memcpy(dst, data, size);
        buf->Unlock();
    }
}

void DX8Backend::Destroy_Resource(RenderResource h)
{
    IUnknown * obj = reinterpret_cast<IUnknown *>(h.id);
    if (obj != nullptr)
    {
        obj->Release();
    }
}

void DX8Backend::Begin_Dynamic_Frame()
{
    // D3D8 dynamic buffers are managed by the runtime — nothing to reset
    // per-frame at the backend level. Ring-lifetime tracking stays in the
    // caller (DynamicVBAccessClass / DynamicIBAccessClass).
}

// Phase 5 Option 1 transitional: the legacy DX8 path already owns these
// resources through TextureBaseClass / DX8VertexBufferClass /
// DX8IndexBufferClass. DX8Backend must stay a passive forwarding adapter,
// so registered legacy resources do not get an owning RenderResource.

RenderResource DX8Backend::Register_Loaded_Texture(TextureBaseClass * /*tex*/)
{
    return kInvalidRenderResource;
}

RenderResource DX8Backend::Register_Loaded_Vertex_Buffer(VertexBufferClass * /*vb*/)
{
    return kInvalidRenderResource;
}

RenderResource DX8Backend::Register_Loaded_Index_Buffer(IndexBufferClass * /*ib*/)
{
    return kInvalidRenderResource;
}
