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

// TheSuperHackers @refactor bobtista 10/04/2026 DiligentBackend is the
// IRenderBackend implementation that targets Diligent Engine (DX11 on
// Windows, Vulkan on Linux, Metal-via-MoltenVK on macOS in open-source
// builds). Phase 2 provides a stub shell that compiles and links against
// DiligentCore but implements every virtual method as a no-op. Phase 3
// will fill in real implementations subsystem by subsystem.
//
// This header MUST NOT be included from any VC6 translation unit. The
// VC6 build always uses DX8Backend; the Diligent backend requires MSVC
// 2022+ and C++17.

#pragma once

#include "IRenderBackend.h"

class DiligentBackend : public IRenderBackend
{
public:
    DiligentBackend();
    virtual ~DiligentBackend();

    // -- Backend lifecycle ----------------------------------------------------

    virtual void Initialize(void * hwnd, int width, int height);
    virtual void Shutdown();

    // -- Device state queries -------------------------------------------------

    virtual bool Is_Device_Lost() const;
    virtual bool Has_Stencil() const;
    virtual WW3DFormat Get_Back_Buffer_Format() const;
    virtual SurfaceClass * Get_Back_Buffer(unsigned int num) const;
    virtual void Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit);

    // -- Frame lifecycle ------------------------------------------------------

    virtual void Begin_Scene();
    virtual void End_Scene(bool flip_frame);
    virtual void Flip_To_Primary();
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil);
    virtual void Set_Viewport(const RenderBackendViewport & viewport);

    // -- Vertex / index buffers -----------------------------------------------

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream);
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba);
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset);
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset);
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset);

    // -- State: shaders, materials, textures ---------------------------------

    virtual void Set_Shader(const ShaderClass & shader);
    virtual void Get_Shader(ShaderClass & shader);
    virtual void Set_Material(const VertexMaterialClass * material);
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture);
    virtual void Apply_Render_State_Changes();
    virtual void Apply_Default_State();
    virtual void Invalidate_Cached_Render_States();
    virtual void Set_Blend_Op(BlendOp op);
    virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest);
    virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha);
    virtual void Set_Alpha_Blend_Enable(bool enable);
    virtual void Show_Hardware_Cursor(bool show);
    virtual void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, SurfaceClass * surface);
    virtual void Set_Hardware_Cursor_Position(int x, int y);
    virtual void Set_Stencil_Enable(bool enable);
    virtual void Set_Stencil_Func(CompareFunc func);
    virtual void Set_Stencil_Ref(unsigned int ref);
    virtual void Set_Stencil_Mask(unsigned int mask);
    virtual void Set_Stencil_Write_Mask(unsigned int mask);
    virtual void Set_Stencil_Pass_Op(StencilOp op);
    virtual void Set_Stencil_Fail_Op(StencilOp op);
    virtual void Set_Stencil_ZFail_Op(StencilOp op);

    // Phase 4F render-state extension.
    virtual void Set_Z_Bias(int bias);
    virtual void Set_Fill_Mode(FillMode mode);
    virtual void Set_Depth_Test_Enable(bool enable);
    virtual void Set_Depth_Write_Enable(bool enable);
    virtual void Set_Depth_Func(CompareFunc func);
    virtual void Set_Color_Write_Mask(unsigned mask);
    virtual void Set_Lighting_Enable(bool enable);
    virtual void Set_Texture_Factor(unsigned argb);
    virtual void Set_Cull_Mode(CullMode mode);

    // -- Transforms -----------------------------------------------------------

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m);
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m);
    virtual void Get_Transform(TransformKind transform, Matrix4x4 & m) const;
    virtual void Set_World_Identity();
    virtual void Set_View_Identity();
    virtual bool Is_World_Identity() const;
    virtual bool Is_View_Identity() const;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar);

    // -- Lighting and fog -----------------------------------------------------

    virtual void Set_Light(unsigned int index, const LightClass & light);
    virtual void Set_Ambient(const Vector3 & color);
    virtual const Vector3 & Get_Ambient() const;
    virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end);
    virtual bool Get_Fog_Enable() const;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env);
    virtual LightEnvironmentClass * Get_Light_Environment() const;

    // -- Draw calls -----------------------------------------------------------

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count);
    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count);
    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count);

    // -- Programmable pipeline ------------------------------------------------

    virtual void Set_Vertex_Shader(unsigned long vertex_shader);
    virtual void Set_Pixel_Shader(unsigned long pixel_shader);
    virtual void Set_Vertex_Shader_Constant(int reg, const void * data, int count);
    virtual void Set_Pixel_Shader_Constant(int reg, const void * data, int count);

    // -- Render targets -------------------------------------------------------

    virtual TextureClass * Create_Render_Target(int width, int height, WW3DFormat format);
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture);
    virtual bool Is_Render_To_Texture() const;
    virtual void Set_Shadow_Map(int idx, ZTextureClass * ztex);
    virtual ZTextureClass * Get_Shadow_Map(int idx) const;
};
