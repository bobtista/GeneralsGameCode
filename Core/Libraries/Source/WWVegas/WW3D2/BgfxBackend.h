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

// TheSuperHackers @refactor bobtista 11/04/2026 BgfxBackend inherits from
// DX8Backend so the dx8 device stays correctly programmed for as long as
// the bgfx and dx8 backends are co-resident. The base class implementation
// of every method forwards to DX8Wrapper. BgfxBackend overrides the
// methods that need bgfx-specific extras (capture matrices, build a
// vertex/index buffer cache, pick a bgfx program from a ShaderClass,
// etc.) and calls the base class first to keep the dx8 device functional.
//
// After the Phase 4K cutover the dx8 device goes away and the base class
// forwards become no-ops. Until then, this dual path is what keeps the
// dx8 main game window rendering correctly in the bgfx build.
//
// This header MUST NOT be included from any VC6 translation unit. The
// VC6 build always uses DX8Backend; the bgfx backend requires MSVC 2022+
// and C++17. See RENDER_BACKEND.md and PHASE4.md.

#pragma once

#include "DX8Backend.h"

class BgfxBackend : public DX8Backend
{
public:
    BgfxBackend();
    virtual ~BgfxBackend();

    // -- Backend lifecycle ----------------------------------------------------
    //
    // Initialize creates the bgfx popup window and calls bgfx::init.
    // Shutdown tears down all bgfx resources before bgfx::shutdown.
    // Both override DX8Backend's empty stubs.

    virtual void Initialize(void * hwnd, int width, int height);
    virtual void Shutdown();

    // -- Frame lifecycle ------------------------------------------------------
    //
    // Begin_Scene calls bgfx::touch on view 0; End_Scene submits the test
    // triangle and calls bgfx::frame to advance the swap chain. The dx8
    // device's Begin_Scene/End_Scene are driven directly by ww3d.cpp via
    // DX8Wrapper, so the base class versions are intentionally empty.

    virtual void Begin_Scene();
    virtual void End_Scene(bool flip_frame);

    // -- Vertex / index buffers -----------------------------------------------
    //
    // Override only the static-VB / IB setters to record the bgfx cache
    // hit (or miss) for the current draw. The base DX8Backend forwards
    // are still called so the d3d8 device sees the binding too.

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream);
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba);
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset);
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset);
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset);

    // Phase 4C.4 write-side capture hooks. DX8Backend inherits the
    // empty default from IRenderBackend; BgfxBackend captures the data
    // into the cache for use by Set_Vertex_Buffer / Set_Index_Buffer.
    // Phase 4G.2 adds the dynamic variants for DynamicVBAccessClass /
    // DynamicIBAccessClass which get copied into bgfx transient buffers.

    virtual void Capture_Vertex_Data(const VertexBufferClass * vb,
                                     const void * data,
                                     unsigned int size_bytes);
    virtual void Capture_Index_Data(const IndexBufferClass * ib,
                                    const void * data,
                                    unsigned int size_bytes);
    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data,
                                             unsigned int size_bytes);
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data,
                                            unsigned int size_bytes);
    virtual void Capture_Vertex_Sub_Range(const VertexBufferClass * vb,
                                          const void * data,
                                          unsigned int start_vertex,
                                          unsigned int size_bytes);
    virtual void Capture_Index_Sub_Range(const IndexBufferClass * ib,
                                         const void * data,
                                         unsigned int start_index,
                                         unsigned int size_bytes);
    virtual void Begin_Sorted_Batch_Pass();
    virtual void End_Sorted_Batch_Pass();
    virtual void Capture_Sorted_Batch_Transforms(const Matrix4x4 & world,
                                                 const Matrix4x4 & view);
    virtual void Capture_Sorted_Batch_Light(const D3DLIGHT8 & light, bool enabled);
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                    const DynamicIBAccessClass & dyn_ib,
                                    unsigned short polygon_count,
                                    unsigned short vertex_count);

    // -- State: shaders, materials, textures ---------------------------------
    //
    // Set_Shader picks a bgfx program and state mask from the preset bits.
    // Set_Texture caches the texture handle. Both call the base DX8Backend
    // forward so the dx8 device also sees the change.

    virtual void Set_Shader(const ShaderClass & shader);
    virtual void Set_Material(const VertexMaterialClass * material);
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture);
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env);
    virtual void Set_Ambient(const Vector3 & color);

    // -- Transforms -----------------------------------------------------------
    //
    // Captures the engine's view / projection / world matrices into bgfx
    // column-major form, then forwards to the base DX8Backend so the dx8
    // device also gets the matrices set.

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m);
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m);
    virtual void Set_World_Identity();
    virtual void Set_View_Identity();
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar);

    // -- Draw calls -----------------------------------------------------------
    //
    // Issues a real bgfx::submit on view 1 if the cache lookup found a
    // valid VB+IB+program for the current state, then forwards to the
    // base DX8Backend so the dx8 device also draws the geometry.

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count);
    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count);

    // Everything else (Is_Device_Lost, Has_Stencil, Get_Back_Buffer*,
    // Set_Gamma, Flip_To_Primary, Clear, Set_Viewport,
    // Get_Shader, Set_Material, Apply_Render_State_Changes,
    // Apply_Default_State, Invalidate_Cached_Render_States, Set_Blend_*,
    // Set_Color_Write_Enable, Set_Alpha_Blend_Enable, Show/Set_Hardware_Cursor*,
    // Set_Stencil_*, Get_Transform, Set_Light*, Set/Get_Ambient,
    // Set/Get_Fog*, Set/Get_Light_Environment, Draw_Strip, Set_Vertex_Shader,
    // Set_Pixel_Shader, *_Constant, Create_Render_Target,
    // Set_Render_Target_With_Z, Is_Render_To_Texture, Set/Get_Shadow_Map)
    // is inherited from DX8Backend and forwards to DX8Wrapper unchanged.
};
