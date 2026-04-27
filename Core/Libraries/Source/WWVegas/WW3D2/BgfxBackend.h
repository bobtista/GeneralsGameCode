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
// After the cutover the dx8 device goes away and the base class
// forwards become no-ops. Until then, this dual path is what keeps the
// dx8 main game window rendering correctly in the bgfx build.
//
// This header MUST NOT be included from any VC6 translation unit. The
// VC6 build always uses DX8Backend; the bgfx backend requires MSVC 2022+
// and C++17.

#pragma once

#include "DX8Backend.h"

// TheSuperHackers @refactor bobtista 22/04/2026 BgfxBackend
// always inherits from DX8Backend. The earlier preprocessor
// base-class swap was reverted because it broke DX8Wrapper's state
// tracking (which the sorting renderer and others read back). Instead,
// standalone mode (GGC_BGFX_STANDALONE) keeps the class hierarchy and
// just swaps DX8Wrapper's D3D8 device for a no-op stub at Init time
// (see StubD3D8Device.h). DX8Wrapper continues to populate render_state
// exactly as in ref-popup mode; its D3D calls execute against the stub
// and do nothing.

class BgfxBackend : public DX8Backend
{
public:
    BgfxBackend();
    virtual ~BgfxBackend();

    virtual bool Has_Shader_Pipeline() const override { return true; }
    virtual void Invalidate_Cached_Texture(TextureBaseClass * texture) override;
    virtual void Release_Cached_Texture(TextureBaseClass * texture) override;

    // -- Backend lifecycle ----------------------------------------------------
    //
    // Initialize creates the bgfx popup window and calls bgfx::init.
    // Shutdown tears down all bgfx resources before bgfx::shutdown.
    // Both override DX8Backend's empty stubs.

    virtual void Initialize(void * hwnd, int width, int height) override;
    virtual void Shutdown() override;

    // -- Frame lifecycle ------------------------------------------------------
    //
    // Begin_Scene calls bgfx::touch on view 0; End_Scene submits the test
    // triangle and calls bgfx::frame to advance the swap chain. The dx8
    // device's Begin_Scene/End_Scene are driven directly by ww3d.cpp via
    // DX8Wrapper, so the base class versions are intentionally empty.

    virtual void Begin_Scene() override;
    virtual void End_Scene(bool flip_frame) override;
    virtual void Set_Viewport(const RenderBackendViewport & viewport) override;

    // -- Vertex / index buffers -----------------------------------------------
    //
    // Override only the static-VB / IB setters to record the bgfx cache
    // hit (or miss) for the current draw. The base DX8Backend forwards
    // are still called so the d3d8 device sees the binding too.

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream) override;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) override;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) override;

    // Write-side capture hooks. DX8Backend inherits the
    // empty default from IRenderBackend; BgfxBackend captures the data
    // into the cache for use by Set_Vertex_Buffer / Set_Index_Buffer.
    // Adds the dynamic variants for DynamicVBAccessClass /
    // DynamicIBAccessClass which get copied into bgfx transient buffers.

    virtual void Capture_Vertex_Data(const VertexBufferClass * vb,
                                     const void * data,
                                     unsigned int size_bytes) override;
    virtual void Capture_Index_Data(const IndexBufferClass * ib,
                                    const void * data,
                                    unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data,
                                             unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data,
                                            unsigned int size_bytes) override;
    virtual void Capture_Vertex_Sub_Range(const VertexBufferClass * vb,
                                          const void * data,
                                          unsigned int start_vertex,
                                          unsigned int size_bytes) override;
    virtual void Capture_Index_Sub_Range(const IndexBufferClass * ib,
                                         const void * data,
                                         unsigned int start_index,
                                         unsigned int size_bytes) override;
    virtual void Begin_Sorted_Batch_Pass() override;
    virtual void End_Sorted_Batch_Pass() override;
    virtual void Capture_Sorted_Batch_Transforms(const Matrix4x4 & world,
                                                 const Matrix4x4 & view) override;
    virtual void Capture_Sorted_Batch_Light(const RenderBackendLight & light, bool enabled) override;
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                    const DynamicIBAccessClass & dyn_ib,
                                    unsigned short polygon_count,
                                    unsigned short vertex_count) override;

    // -- State: shaders, materials, textures ---------------------------------
    //
    // Set_Shader picks a bgfx program and state mask from the preset bits.
    // Set_Texture caches the texture handle. Both call the base DX8Backend
    // forward so the dx8 device also sees the change.

    virtual void Set_Shader(const ShaderClass & shader) override;
    virtual void Set_Material(const VertexMaterialClass * material) override;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) override;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) override;
    virtual void Set_Ambient(const Vector3 & color) override;
    virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest) override;
    virtual void Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend) override;
    virtual void Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func) override;
    virtual void Override_Alpha_Blend_Enable(bool enable) override;
    virtual void Override_Texcoord_Index(unsigned stage, unsigned uvIndex) override;
    virtual void Override_Terrain_Blend(bool enable) override;
    virtual void Override_Material_Opacity(float opacity) override;
    virtual void Begin_Water_Overlay() override;
    virtual void End_Water_Overlay() override;
    virtual void Begin_Effect_Overlay() override;
    virtual void End_Effect_Overlay() override;
    virtual bool Begin_Smudge_Distortion() override;
    virtual void End_Smudge_Distortion() override;

    // Tree / grass sway shader hooks (see IRenderBackend.h).
    virtual void Set_Tree_Shader_Constants(const float swayTable[11][4],
                                           const float shroudOffset[4],
                                           const float shroudScale[4]) override;
    virtual void Set_Tree_Vertex_Shader_Active(bool active) override;
    virtual void Set_Grayscale_Mode(bool enable) override;
    virtual void Set_Cloud_Shadow_Params(bool enable, float scroll_x, float scroll_y,
                                         float stretch, TextureClass * cloud_tex) override;
    virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha) override;
    virtual void Set_Color_Write_Mask(unsigned mask) override;
    virtual void Skip_Next_Bgfx_Submit() override;
    virtual void Set_Shadow_Volume_Shader_Active(bool active) override;
    virtual void Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                             unsigned stencil_read_mask,
                                             unsigned stencil_ref) override;
    virtual void Submit_Shadow_Volume_Caps(unsigned strip_start_vertex,
                                           unsigned num_silhouette_verts) override;
    virtual void Submit_Shadow_Volume_Triangulated_Caps(
        unsigned strip_start_vertex,
        const short * local_cap_indices,
        unsigned cap_index_count) override;
    virtual void Set_Shadow_Light_Position(float x, float y, float z) override;
    virtual void Capture_Shroud_Texture(TextureClass * dst_texture,
                                        const void * pixel_data,
                                        unsigned dst_width,
                                        unsigned dst_height,
                                        unsigned src_width,
                                        unsigned src_height,
                                        unsigned dst_x,
                                        unsigned dst_y,
                                        unsigned pitch,
                                        WW3DFormat format) override;
    virtual void Set_Texture_Factor(unsigned argb) override;

    virtual void Set_Depth_Func(CompareFunc func) override;

    // bgfx stencil state capture.
    virtual void Set_Stencil_Enable(bool enable) override;
    virtual void Set_Stencil_Func(CompareFunc f) override;
    virtual void Set_Stencil_Ref(unsigned ref) override;
    virtual void Set_Stencil_Mask(unsigned mask) override;
    virtual void Set_Stencil_Write_Mask(unsigned mask) override;
    virtual void Set_Stencil_Pass_Op(StencilOp op) override;
    virtual void Set_Stencil_Fail_Op(StencilOp op) override;
    virtual void Set_Stencil_ZFail_Op(StencilOp op) override;
    virtual void Set_Cull_Mode(CullMode mode) override;
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture = nullptr) override;
    virtual void Clear_State_Overrides() override;

    // -- Transforms -----------------------------------------------------------
    //
    // Captures the engine's view / projection / world matrices into bgfx
    // column-major form, then forwards to the base DX8Backend so the dx8
    // device also gets the matrices set.

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) override;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) override;
    virtual void Set_World_Identity() override;
    virtual void Set_View_Identity() override;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar) override;

    // -- Draw calls -----------------------------------------------------------
    //
    // Issues a real bgfx::submit on view 1 if the cache lookup found a
    // valid VB+IB+program for the current state, then forwards to the
    // base DX8Backend so the dx8 device also draws the geometry.

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;
    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;
    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count) override;

    // Everything else (Is_Device_Lost, Has_Stencil, Get_Back_Buffer*,
    // Set_Gamma, Flip_To_Primary, Clear, Set_Viewport,
    // Get_Shader, Set_Material, Apply_Render_State_Changes,
    // Apply_Default_State, Invalidate_Cached_Render_States, Set_Blend_*,
    // Set_Color_Write_Enable, Set_Alpha_Blend_Enable, Show/Set_Hardware_Cursor*,
    // Set_Stencil_*, Get_Transform, Set_Light*, Set/Get_Ambient,
    // Set/Get_Fog*, Set/Get_Light_Environment, Set_Vertex_Shader,
    // Set_Pixel_Shader, *_Constant, Create_Render_Target,
    // Is_Render_To_Texture, Set/Get_Shadow_Map)
    // is inherited from DX8Backend and forwards to DX8Wrapper unchanged.

    // -- Resource creation (asset ingress) ---------------------------
    //
    // Each override first forwards to DX8Backend so the ref-popup build's
    // D3D8 resource is created in parallel (that is how the DX8 reference
    // window stays in sync with bgfx). Then creates the corresponding bgfx
    // resource. The returned RenderResource.id encodes an index into a
    // backend-local side table that maps to the pair of (bgfx handle, D3D8
    // pointer).

    virtual RenderResource Create_Texture(const TextureDesc & desc) override;
    virtual RenderResource Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data) override;
    virtual RenderResource Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit) override;
    virtual RenderResource Create_Dynamic_Vertex_Buffer(const BufferDesc & desc) override;
    virtual RenderResource Create_Dynamic_Index_Buffer(const BufferDesc & desc, bool indices_are_32bit) override;
    virtual void * Map_Dynamic(RenderResource h, unsigned int offset, unsigned int size, bool discard) override;
    virtual void   Unmap_Dynamic(RenderResource h) override;
    virtual void   Update_Sub_Range(RenderResource h, unsigned int offset, const void * data, unsigned int size) override;
    virtual void   Destroy_Resource(RenderResource h) override;
    virtual void   Begin_Dynamic_Frame() override;

    // Transitional: populate m_backendHandle on resources created via the
    // legacy D3D8 loader. See IRenderBackend.h for context.
    virtual RenderResource Register_Loaded_Texture(TextureBaseClass * tex) override;
    virtual RenderResource Register_Loaded_Vertex_Buffer(VertexBufferClass * vb) override;
    virtual RenderResource Register_Loaded_Index_Buffer(IndexBufferClass * ib) override;
};
