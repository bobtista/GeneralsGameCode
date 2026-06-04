/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// TheSuperHackers @refactor bobtista 11/04/2026 BgfxBackend.
// IRenderBackend implementation that drives bgfx directly. It owns the
// render-state, transform, buffer, and texture snapshots it needs.
//
// This header MUST NOT be included from any VC6 translation unit. The VC6
// build always uses DX8Backend; the bgfx backend requires MSVC 2022+ and C++17.

#pragma once

#include "IRenderBackend.h"
#include "vector3.h"

class BgfxBackend : public IRenderBackend
{
public:
    BgfxBackend();
    virtual ~BgfxBackend();

    virtual bool Has_Shader_Pipeline() const override { return true; }
    virtual void Invalidate_Cached_Texture(TextureBaseClass * texture) override;
    virtual void Copy_Render_Target_To_Texture(TextureClass * dst_texture,
                                               TextureClass * src_render_target) override;
    virtual void Release_Cached_Texture(TextureBaseClass * texture) override;

    // -- Backend lifecycle ----------------------------------------------------
    //
    // Initialize creates the bgfx popup window and calls bgfx::init.
    // Shutdown tears down all bgfx resources before bgfx::shutdown.
    // Both override IRenderBackend's empty stubs.

    virtual void Initialize(void * hwnd, int width, int height) override;
    virtual void Shutdown() override;

    // -- Frame lifecycle ------------------------------------------------------
    //
    // Begin_Scene touches the bgfx views; End_Scene calls bgfx::frame to advance the swap chain.

    virtual void Begin_Scene() override;
    virtual void End_Scene(bool flip_frame) override;
    virtual void Invalidate_Cached_Render_States() override;
    virtual bool Has_Stencil() const override { return true; }
    virtual WW3DFormat Get_Back_Buffer_Format() const override;
    virtual bool Request_Native_Screen_Shot(const char * path) override;
    virtual void Set_Texture_Bitdepth(int bitdepth) override;
    virtual int Get_Texture_Bitdepth() const override;
    virtual bool Supports_Texture_Format(WW3DFormat format) const override;
    virtual bool Supports_Compressed_Textures() const override;
    virtual bool Supports_Bump_Envmap() const override { return false; }
    virtual bool Supports_Bump_Envmap_Luminance() const override { return false; }
    virtual bool Supports_Texture_Filter(RenderBackendTextureFilterCapability /*capability*/) const override { return true; }
    virtual bool Supports_Texture_Op(RenderBackendTextureOpCapability capability) const override;
    virtual bool Supports_Fog() const override { return true; }
    virtual bool Is_Legacy_Voodoo3() const override { return false; }
    virtual bool Supports_NPatches() const override { return false; }
    virtual bool Supports_Hardware_Transform_And_Lighting() const override { return true; }
    virtual bool Supports_Point_Sprites() const override { return false; }
    virtual RenderBackendTextureLimits Get_Texture_Limits() const override;
    virtual int Get_Max_Texture_Stages() const override;
    virtual bool Supports_Z_Bias() const override { return true; }
    virtual void Set_MSAA_Mode(RenderBackendMSAAMode mode) override;
    virtual RenderBackendMSAAMode Get_MSAA_Mode() const override;
    virtual bool Supports_Dot3() const override { return true; }
    virtual bool Get_Device_Identity(RenderBackendDeviceIdentity & identity) const override;
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0) override;
    virtual void Set_Viewport(const RenderBackendViewport & viewport) override;
    virtual bool Initialize_View_Capture(RenderBackendViewCaptureKind kind) override;
    virtual void Release_View_Capture(RenderBackendViewCaptureKind kind) override;
    virtual bool Supports_View_Capture(RenderBackendViewCaptureKind kind) const override;
    virtual bool Begin_View_Capture(RenderBackendViewCaptureKind kind) override;
    virtual bool End_View_Capture(RenderBackendViewCaptureKind kind) override;
    virtual bool Is_View_Capture_Active(RenderBackendViewCaptureKind kind) const override;
    virtual bool Has_View_Capture(RenderBackendViewCaptureKind kind) const override;
    virtual bool Bind_View_Capture_Texture(RenderBackendViewCaptureKind kind,
                                           unsigned int stage) override;
    virtual bool Draw_View_Capture_Quad(RenderBackendViewCaptureKind kind,
                                        const RenderBackendScreenVertex * vertices,
                                        unsigned int vertex_count,
                                        bool use_second_uv) override;
    virtual bool Draw_Screen_Quad(const RenderBackendScreenVertex * vertices,
                                  unsigned int vertex_count,
                                  bool use_second_uv) override;
    virtual bool Capture_Back_Buffer_RGBA(unsigned int display_width,
                                          unsigned int display_height,
                                          unsigned int image_size,
                                          unsigned char * output_pixels,
                                          unsigned int output_capacity,
                                          unsigned int * output_width,
                                          unsigned int * output_height) override;

    // -- Vertex / index buffers -----------------------------------------------
    //
    // Record the bgfx cache hit (or miss) for the current draw.

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream) override;
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba) override;
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset) override;
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset) override;
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset) override;

    // Write-side upload hooks. BgfxBackend uploads the data into the cache
    // for use by Set_Vertex_Buffer / Set_Index_Buffer.
    // Adds the dynamic variants for DynamicVBAccessClass /
    // DynamicIBAccessClass which get copied into bgfx transient buffers.

    virtual void Upload_Vertex_Buffer_Data(const VertexBufferClass * vb,
                                     const void * data,
                                     unsigned int size_bytes) override;
    virtual void Upload_Index_Buffer_Data(const IndexBufferClass * ib,
                                    const void * data,
                                    unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba,
                                             const void * data,
                                             unsigned int size_bytes) override;
    virtual void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba,
                                            const void * data,
                                            unsigned int size_bytes) override;
    virtual bool Supports_Instancing() const override;
    virtual bool Begin_Instanced_Batch(unsigned max_instances) override;
    virtual void Add_Instance(const float * world_matrix_4x4) override;
    virtual void Submit_Instanced_Batch(unsigned index_offset, unsigned triangle_count,
                                        unsigned min_vertex_index, unsigned vertex_count) override;

    virtual void * Begin_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba,
                                              unsigned int size_bytes) override;
    virtual void End_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba,
                                          const void * data,
                                          unsigned int size_bytes) override;
    virtual void * Begin_Dynamic_Index_Write(const DynamicIBAccessClass * iba,
                                             unsigned int size_bytes) override;
    virtual void End_Dynamic_Index_Write(const DynamicIBAccessClass * iba,
                                         const void * data,
                                         unsigned int size_bytes) override;
    virtual void Upload_Vertex_Buffer_Sub_Range(const VertexBufferClass * vb,
                                          const void * data,
                                          unsigned int start_vertex,
                                          unsigned int size_bytes) override;
    virtual void Upload_Index_Buffer_Sub_Range(const IndexBufferClass * ib,
                                         const void * data,
                                         unsigned int start_index,
                                         unsigned int size_bytes) override;
    virtual void Begin_Sorted_Batch_Pass() override;
    virtual void End_Sorted_Batch_Pass() override;
    virtual void Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & state) override;
    virtual void Set_Point_Group_Render_Active(bool active) override;
    virtual void Set_Streak_Render_Active(bool active) override;
    virtual void Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & state) override;
    virtual void Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & state) override;
    virtual void Release_Legacy_Render_State_For_Sorted_Draw() override;
    virtual void Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb,
                                    const DynamicIBAccessClass & dyn_ib,
                                    unsigned short polygon_count,
                                    unsigned short vertex_count) override;

    // -- State: shaders, materials, textures ---------------------------------
    //
    // Set_Shader picks a bgfx program and state mask from the preset bits.
    // Set_Texture caches the texture handle for bgfx submission.

    virtual void Set_Shader(const ShaderClass & shader) override;
    virtual void Set_Material(const VertexMaterialClass * material) override;
    virtual void Apply_Material_State(const RenderBackendMaterialState & material) override;
    virtual void Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source,
                                           RenderBackendMaterialColorSource diffuse_source,
                                           RenderBackendMaterialColorSource emissive_source) override;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture) override;
    virtual void Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture) override;
    virtual void Set_Light(unsigned int index, const LightClass & light) override;
    virtual void Clear_Light(unsigned int index) override;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) override;
    virtual void Set_Ambient(const Vector3 & color) override;
    virtual const Vector3 & Get_Ambient() const override;
    virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end) override;
    virtual void Set_Fog_Enable(bool enable) override;
    virtual void Set_Fog_Color(unsigned argb) override;
    virtual unsigned Get_Fog_Color() const override;
    virtual void Set_Specular_Enable(bool enable) override;
    virtual void Set_Patch_Segments(float level) override;
    virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest) override;
    virtual void Set_Blend_Op(BlendOp op) override;
    virtual void Set_Alpha_Blend_Enable(bool enable) override;
    virtual void Set_Alpha_Test_Enable(bool enable) override;
    virtual void Set_Alpha_Test_Reference(unsigned ref) override;
    virtual void Set_Alpha_Test_Function(CompareFunc func) override;
    virtual void Set_Normalize_Normals(bool enable) override;
    virtual void Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend) override;
    virtual void Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func) override;
    virtual void Override_Alpha_Blend_Enable(bool enable) override;
    virtual void Override_Texcoord_Index(unsigned stage, unsigned uvIndex) override;
    virtual void Override_Terrain_Blend(bool enable) override;
    virtual void Override_Material_Opacity(float opacity) override;
    virtual void Set_Texture_Transform(unsigned stage, const Matrix4x4& matrix) override;
    virtual void Clear_Texture_Transform(unsigned stage) override;
    virtual void Set_Texture_Coord_Source(unsigned stage,
                                          RenderBackendTexcoordSource source,
                                          unsigned uv_array_index = 0) override;
    virtual void Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected) override;
    virtual void Set_Texture_Bump_Env_Matrix(unsigned stage,
                                             float m00,
                                             float m01,
                                             float m10,
                                             float m11) override;
    virtual void Set_Texture_Bump_Env_Luminance(unsigned stage,
                                                float scale,
                                                float offset) override;
    virtual void Set_Texture_Color_Operation(unsigned stage,
                                             RenderBackendTextureOperation op) override;
    virtual void Set_Texture_Alpha_Operation(unsigned stage,
                                             RenderBackendTextureOperation op) override;
    virtual void Set_Texture_Color_Argument(unsigned stage,
                                            unsigned argument_index,
                                            RenderBackendTextureArgument arg) override;
    virtual void Set_Texture_Alpha_Argument(unsigned stage,
                                            unsigned argument_index,
                                            RenderBackendTextureArgument arg) override;
    virtual void Set_Texture_Coord_Generation(unsigned stage, bool cameraPosEnabled) override;
    virtual void Set_Texture_UV_Wrap(unsigned stage, bool enable) override;
    virtual void Set_Texture_Address_Mode(unsigned stage,
                                          RenderBackendTextureAddressMode u,
                                          RenderBackendTextureAddressMode v,
                                          RenderBackendTextureAddressMode w) override;
    virtual void Set_Texture_Sample_Filter(unsigned stage,
                                           RenderBackendTextureSampleFilter min_filter,
                                           RenderBackendTextureSampleFilter mag_filter,
                                           RenderBackendTextureSampleFilter mip_filter) override;
    virtual void Set_Texture_Min_Mag_Filter(unsigned stage,
                                            RenderBackendTextureSampleFilter min_filter,
                                            RenderBackendTextureSampleFilter mag_filter) override;
    virtual void Set_Texture_Mip_Filter(unsigned stage,
                                        RenderBackendTextureSampleFilter mip_filter) override;
    virtual void Set_Texture_Max_Anisotropy(unsigned stage, unsigned max_anisotropy) override;
    virtual void Set_Texture_Clamp_Mode(unsigned stage, bool clampU, bool clampV) override;
    virtual void Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value) override;
    virtual void Configure_Custom_Edging_Cloud_Texture_Stages() override;
    virtual void Configure_Shadow_Volume_Fill_Texture_Stages() override;
    virtual void Set_Shroud_Texture_Pass_Active(bool active, unsigned stage) override;
    virtual void Set_Object_Shroud_Texture_Pass_Active(bool active) override;
    virtual void Set_Object_Shroud_Alpha_Mask_Texture(TextureBaseClass * texture) override;
    virtual void Set_Object_Shroud_Dim_Factor(float factor) override;
    virtual void Set_Shroud_Texture_Params(float offset_x, float offset_y,
                                           float scale_x, float scale_y) override;
    virtual bool Requires_Delayed_Object_Shroud_Pass() const override { return true; }
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
    virtual bool Supports_Color_Write_Mask() const override { return true; }
    virtual unsigned Get_Color_Write_Mask() const override;
    virtual void Set_Color_Write_Mask(unsigned mask) override;
    virtual void Set_Lighting_Enable(bool enable) override;
    virtual void Skip_Next_Bgfx_Submit() override;
    virtual void Set_Projected_Shadow_Decal_Active(bool active) override;
    virtual void Set_Projected_Decal_Mode(RenderBackendProjectedDecalMode mode) override;
    virtual void Set_Shadow_Volume_Shader_Active(bool active) override;
    virtual void Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                             unsigned stencil_read_mask,
                                             unsigned stencil_ref,
                                             int x,
                                             int y,
                                             int width,
                                             int height) override;
    virtual void Submit_Shadow_Volume_Caps(unsigned strip_start_vertex,
                                           unsigned num_silhouette_verts) override;
    virtual void Submit_Shadow_Volume_Triangulated_Caps(
        unsigned strip_start_vertex,
        const short * local_cap_indices,
        unsigned cap_index_count) override;
    virtual bool Needs_Closed_Shadow_Volumes() const override;
    virtual void Capture_Shroud_Texture(TextureClass * dst_texture,
                                        const void * pixel_data,
                                        unsigned dst_width,
                                        unsigned dst_height,
                                        unsigned src_width,
                                        unsigned src_height,
                                        unsigned src_x,
                                        unsigned src_y,
                                        unsigned dst_x,
                                        unsigned dst_y,
                                        unsigned pitch,
                                        WW3DFormat format) override;
    virtual void Set_Texture_Factor(unsigned argb) override;

    virtual void Set_Z_Bias(int bias) override;
    virtual void Set_Normal_Bias(float bias) override;
    virtual void Set_Fill_Mode(FillMode mode) override;
    virtual void Set_Shade_Mode(ShadeMode mode) override;
    virtual void Set_Depth_Test_Enable(bool enable) override;
    virtual void Set_Depth_Write_Enable(bool enable) override;
    virtual void Set_Depth_Func(CompareFunc func) override;
    virtual void Set_Point_Sprite_Enable(bool enable) override;
    virtual void Set_Point_Scale_Enable(bool enable) override;
    virtual void Set_Point_Size(float size, float min_size, float max_size) override;
    virtual void Set_Point_Scale(float a, float b, float c) override;

    // bgfx stencil state capture.
    virtual void Set_Stencil_Enable(bool enable) override;
    virtual void Set_Stencil_Func(CompareFunc f) override;
    virtual void Set_Stencil_Ref(unsigned ref) override;
    virtual void Set_Stencil_Mask(unsigned mask) override;
    virtual void Set_Stencil_Write_Mask(unsigned mask) override;
    virtual void Set_Stencil_Pass_Op(StencilOp op) override;
    virtual void Set_Stencil_Fail_Op(StencilOp op) override;
    virtual void Set_Stencil_ZFail_Op(StencilOp op) override;
    virtual CullMode Get_Cull_Mode() const override;
    virtual void Set_Cull_Mode(CullMode mode) override;
    virtual void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture = nullptr) override;
    virtual void Clear_State_Overrides() override;

    // -- Transforms -----------------------------------------------------------
    //
    // Captures the engine's view / projection / world matrices into bgfx
    // column-major form.

    virtual void Set_Transform(TransformKind transform, const Matrix4x4 & m) override;
    virtual void Set_Transform(TransformKind transform, const Matrix3D & m) override;
    virtual void Get_Transform(TransformKind transform, Matrix4x4 & m) const override;
    virtual void Set_World_Identity() override;
    virtual void Set_View_Identity() override;
    virtual bool Is_World_Identity() const override;
    virtual bool Is_View_Identity() const override;
    virtual void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar) override;

    // -- Draw calls -----------------------------------------------------------
    //
    // Issues a real bgfx::submit if the cache lookup found a valid
    // VB+IB+program for the current state.

    virtual void Draw_Triangles(unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;
    virtual void Draw_Triangles(unsigned int buffer_type,
                                unsigned short start_index,
                                unsigned short polygon_count,
                                unsigned short min_vertex_index,
                                unsigned short vertex_count) override;
    virtual bool Is_Triangle_Draw_Enabled() const override;
    virtual void Set_Triangle_Draw_Enabled(bool enable) override;
    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count) override;

    // -- Programmable pipeline compatibility ---------------------------------

    virtual bool Load_Legacy_Shader(const char * path,
                                    const unsigned int * declaration,
                                    unsigned int usage,
                                    RenderBackendShaderKind kind,
                                    unsigned long * handle) override;
    virtual bool Create_Vertex_Shader(const unsigned int * declaration,
                                      const unsigned int * shader,
                                      unsigned int usage,
                                      unsigned long * handle) override;
    virtual bool Create_Pixel_Shader(const unsigned int * shader,
                                     unsigned long * handle) override;
    virtual bool Create_Legacy_Pixel_Shader(RenderBackendLegacyPixelShaderMode mode,
                                            unsigned long * handle) override;
    virtual void Delete_Vertex_Shader(unsigned long vertex_shader) override;
    virtual void Delete_Pixel_Shader(unsigned long pixel_shader) override;
    virtual void Set_Vertex_Shader(unsigned long vertex_shader) override;
    virtual void Set_Pixel_Shader(unsigned long pixel_shader) override;

    // -- Resource creation (asset ingress) ---------------------------
    //
    // Creates the corresponding bgfx resource. The returned RenderResource.id
    // encodes an index into a backend-local side table.

    virtual bool Requires_Legacy_Buffer_Resources() const override;
    virtual RenderResource Create_Texture(const TextureDesc & desc) override;
    virtual RenderResource Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data) override;
    virtual RenderResource Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit) override;
    virtual void   Destroy_Resource(RenderResource h) override;

    // Transitional: populate m_backendHandle on owner-backed wrapper
    // resources. See IRenderBackend.h for context.
    virtual RenderResource Register_Texture_Resource(TextureBaseClass * tex) override;
    virtual RenderResource Register_Vertex_Buffer_Resource(VertexBufferClass * vb) override;
    virtual RenderResource Register_Index_Buffer_Resource(IndexBufferClass * ib) override;

private:
    int m_textureBitDepth;
    RenderBackendMSAAMode m_msaaMode;
    // TheSuperHackers @bugfix bobtista 28/05/2026 Persist the ambient color in a real member so Get_Ambient() returns a stable lvalue mirror of g_draw.sceneAmbient.
    mutable Vector3 m_ambient;
};
