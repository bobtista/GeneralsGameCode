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

// Including the bgfx header here is intentional: it forces a compile-time
// dependency on the bgfx headers when GGC_RENDER_BACKEND=bgfx. If bgfx
// isn't available the build fails here, which is the right place to
// catch dependency problems.
#include <bgfx/bgfx.h>

#include <cstdio>

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
}

BgfxBackend::BgfxBackend()
{
    std::fprintf(stderr,
                 "[BgfxBackend] Phase 2 stub backend constructed. "
                 "Most IRenderBackend methods are no-ops; the game will NOT "
                 "render correctly through this backend yet. "
                 "See PHASE2.md.\n");
}

BgfxBackend::~BgfxBackend()
{
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

void BgfxBackend::Begin_Scene()
{
}

void BgfxBackend::End_Scene(bool /*flip_frame*/)
{
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
