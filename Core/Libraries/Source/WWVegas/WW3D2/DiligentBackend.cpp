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

// TheSuperHackers @refactor bobtista 10/04/2026 DiligentBackend Phase 2 stub.
//
// Mirrors BgfxBackend in scope: every virtual method is a no-op (void) or
// returns a sensible default (non-void). The class exists to prove the
// compile-time backend selection works and that DiligentCore can be
// fetched, built, and linked against WW3D2. Phase 3 fills in real
// implementations.
//
// We include DiligentCore headers to force a compile-time dependency on
// the Diligent SDK when GGC_RENDER_BACKEND=diligent.

#include "DiligentBackend.h"

#include "vector3.h"

// Including the EngineFactoryD3D11 header here is intentional - it forces
// a compile-time dependency on DiligentCore when GGC_RENDER_BACKEND=diligent.
#define PLATFORM_WIN32 1
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"

#include <cstdio>

namespace
{
// Anchor a reference to one Diligent symbol so the linker must resolve
// Diligent symbols even though every virtual method below is a no-op.
[[maybe_unused]] const auto kDiligentLinkAnchor = &Diligent::GetEngineFactoryD3D11;

const Vector3 kZeroVec3(0.0f, 0.0f, 0.0f);
}

DiligentBackend::DiligentBackend()
{
    std::fprintf(stderr,
                 "[DiligentBackend] Phase 2 stub backend constructed. "
                 "Most IRenderBackend methods are no-ops; the game will NOT "
                 "render correctly through this backend yet. "
                 "See PHASE2.md.\n");
}

DiligentBackend::~DiligentBackend()
{
}

// -- Device state queries ----------------------------------------------------

bool DiligentBackend::Is_Device_Lost() const
{
    return false;
}

bool DiligentBackend::Has_Stencil()
{
    return false;
}

WW3DFormat DiligentBackend::Get_Back_Buffer_Format()
{
    return WW3D_FORMAT_UNKNOWN;
}

SurfaceClass * DiligentBackend::Get_Back_Buffer(unsigned int /*num*/)
{
    return nullptr;
}

void DiligentBackend::Set_Gamma(float /*gamma*/, float /*bright*/, float /*contrast*/,
                                bool /*calibrate*/, bool /*uselimit*/)
{
}

// -- Frame lifecycle ---------------------------------------------------------

void DiligentBackend::Begin_Scene()
{
}

void DiligentBackend::End_Scene(bool /*flip_frame*/)
{
}

void DiligentBackend::Flip_To_Primary()
{
}

void DiligentBackend::Clear(bool /*clear_color*/, bool /*clear_z_stencil*/,
                            const Vector3 & /*color*/,
                            float /*dest_alpha*/, float /*z*/, unsigned int /*stencil*/)
{
}

void DiligentBackend::Set_Viewport(const RenderBackendViewport & /*viewport*/)
{
}

// -- Vertex / index buffers --------------------------------------------------

void DiligentBackend::Set_Vertex_Buffer(const VertexBufferClass * /*vb*/, unsigned int /*stream*/)
{
}

void DiligentBackend::Set_Vertex_Buffer(const DynamicVBAccessClass & /*vba*/)
{
}

void DiligentBackend::Set_Index_Buffer(const IndexBufferClass * /*ib*/, unsigned short /*index_base_offset*/)
{
}

void DiligentBackend::Set_Index_Buffer(const DynamicIBAccessClass & /*iba*/, unsigned short /*index_base_offset*/)
{
}

void DiligentBackend::Set_Index_Buffer_Index_Offset(unsigned int /*offset*/)
{
}

// -- State: shaders, materials, textures ------------------------------------

void DiligentBackend::Set_Shader(const ShaderClass & /*shader*/)
{
}

void DiligentBackend::Get_Shader(ShaderClass & /*shader*/)
{
}

void DiligentBackend::Set_Material(const VertexMaterialClass * /*material*/)
{
}

void DiligentBackend::Set_Texture(unsigned int /*stage*/, TextureBaseClass * /*texture*/)
{
}

void DiligentBackend::Apply_Render_State_Changes()
{
}

void DiligentBackend::Apply_Default_State()
{
}

void DiligentBackend::Invalidate_Cached_Render_States()
{
}

// -- Transforms --------------------------------------------------------------

void DiligentBackend::Set_Transform(TransformKind /*transform*/, const Matrix4x4 & /*m*/)
{
}

void DiligentBackend::Set_Transform(TransformKind /*transform*/, const Matrix3D & /*m*/)
{
}

void DiligentBackend::Get_Transform(TransformKind /*transform*/, Matrix4x4 & /*m*/)
{
}

void DiligentBackend::Set_World_Identity()
{
}

void DiligentBackend::Set_View_Identity()
{
}

bool DiligentBackend::Is_World_Identity()
{
    return true;
}

bool DiligentBackend::Is_View_Identity()
{
    return true;
}

void DiligentBackend::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & /*matrix*/,
                                                           float /*znear*/, float /*zfar*/)
{
}

// -- Lighting and fog --------------------------------------------------------

void DiligentBackend::Set_Light(unsigned int /*index*/, const LightClass & /*light*/)
{
}

void DiligentBackend::Set_Ambient(const Vector3 & /*color*/)
{
}

const Vector3 & DiligentBackend::Get_Ambient() const
{
    return kZeroVec3;
}

void DiligentBackend::Set_Fog(bool /*enable*/, const Vector3 & /*color*/,
                              float /*start*/, float /*end*/)
{
}

bool DiligentBackend::Get_Fog_Enable() const
{
    return false;
}

void DiligentBackend::Set_Light_Environment(LightEnvironmentClass * /*light_env*/)
{
}

LightEnvironmentClass * DiligentBackend::Get_Light_Environment() const
{
    return nullptr;
}

// -- Draw calls --------------------------------------------------------------

void DiligentBackend::Draw_Triangles(unsigned short /*start_index*/,
                                     unsigned short /*polygon_count*/,
                                     unsigned short /*min_vertex_index*/,
                                     unsigned short /*vertex_count*/)
{
}

void DiligentBackend::Draw_Triangles(unsigned int /*buffer_type*/,
                                     unsigned short /*start_index*/,
                                     unsigned short /*polygon_count*/,
                                     unsigned short /*min_vertex_index*/,
                                     unsigned short /*vertex_count*/)
{
}

void DiligentBackend::Draw_Strip(unsigned short /*start_index*/,
                                 unsigned short /*index_count*/,
                                 unsigned short /*min_vertex_index*/,
                                 unsigned short /*vertex_count*/)
{
}

// -- Programmable pipeline ---------------------------------------------------

void DiligentBackend::Set_Vertex_Shader(unsigned long /*vertex_shader*/)
{
}

void DiligentBackend::Set_Pixel_Shader(unsigned long /*pixel_shader*/)
{
}

void DiligentBackend::Set_Vertex_Shader_Constant(int /*reg*/, const void * /*data*/, int /*count*/)
{
}

void DiligentBackend::Set_Pixel_Shader_Constant(int /*reg*/, const void * /*data*/, int /*count*/)
{
}

// -- Render targets ----------------------------------------------------------

TextureClass * DiligentBackend::Create_Render_Target(int /*width*/, int /*height*/, WW3DFormat /*format*/)
{
    return nullptr;
}

void DiligentBackend::Set_Render_Target_With_Z(TextureClass * /*texture*/, ZTextureClass * /*ztexture*/)
{
}

bool DiligentBackend::Is_Render_To_Texture()
{
    return false;
}

void DiligentBackend::Set_Shadow_Map(int /*idx*/, ZTextureClass * /*ztex*/)
{
}

ZTextureClass * DiligentBackend::Get_Shadow_Map(int /*idx*/)
{
    return nullptr;
}
