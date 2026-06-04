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

// TheSuperHackers @refactor bobtista 10/04/2026 DX8Backend is the reference
// implementation of IRenderBackend that forwards every virtual method to the
// existing DX8Wrapper static facade. It adds zero new rendering logic and
// performs zero behavior changes — it is pure adaptation so the rest of the
// engine can start talking to an IRenderBackend pointer while still running
// on the established DX8 path.

#pragma once

#include "IRenderBackend.h"

class DX8Backend : public IRenderBackend
{
public:
    DX8Backend();
    virtual ~DX8Backend();

    // -- Backend lifecycle ----------------------------------------------------

    virtual void Initialize(void * hwnd, int width, int height);
    virtual void Shutdown();

    virtual bool Init_Render_System(void * hwnd, bool lite) override;
    virtual void Shutdown_Render_System() override;

    // -- Device selection, windowing and display-mode control -----------------

    virtual bool Set_Render_Device(const char * dev_name, int width, int height, int bits, int windowed, bool resize_window) override;
    virtual bool Set_Render_Device(int dev, int width, int height, int bits, int windowed, bool resize_window, bool reset_device, bool restore_assets) override;
    virtual bool Set_Any_Render_Device() override;
    virtual bool Set_Next_Render_Device() override;
    virtual bool Toggle_Windowed() override;
    virtual bool Is_Windowed() const override;
    virtual int Get_Render_Device() const override;
    virtual const RenderDeviceDescClass & Get_Render_Device_Desc(int deviceidx) override;
    virtual int Get_Render_Device_Count() const override;
    virtual const char * Get_Render_Device_Name(int device_index) override;
    virtual bool Set_Device_Resolution(int width, int height, int bits, int windowed, bool resize_window) override;
    virtual void Get_Render_Target_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed) override;
    virtual void Get_Device_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed) override;
    virtual int Get_Device_Resolution_Width() const override;
    virtual int Get_Device_Resolution_Height() const override;
    virtual bool Registry_Save_Render_Device(const char * sub_key) override;
    virtual bool Registry_Save_Render_Device(const char * sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth) override;
    virtual bool Registry_Load_Render_Device(const char * sub_key, bool resize_window) override;
    virtual bool Registry_Load_Render_Device(const char * sub_key, char * device, int device_len, int & width, int & height, int & depth, int & windowed, int & texture_depth) override;
    virtual void Set_Swap_Interval(int swap) override;
    virtual int Get_Swap_Interval() const override;

    // -- Device state queries -------------------------------------------------

    virtual bool Is_Device_Lost() const;
    virtual RenderBackendDeviceStatus Get_Device_Status() const;
    virtual void Reset_Device();
    virtual void Set_Device_Cleanup_Hook(RenderDeviceCleanupHook * hook) override;
    virtual bool Has_Stencil() const;
    virtual WW3DFormat Get_Back_Buffer_Format() const;
    virtual bool Get_Back_Buffer_Description(unsigned int num, RenderBackendSurfaceDescription & desc) const override;
    virtual bool Capture_Back_Buffer_Image(unsigned int num, RenderBackendImage & image) override;
    virtual bool Copy_Back_Buffer_To_Texture(unsigned int num, TextureClass * dst_texture) override;
    virtual void Set_Texture_Bitdepth(int bitdepth) override;
    virtual int Get_Texture_Bitdepth() const override;
    virtual bool Supports_Texture_Format(WW3DFormat format) const override;
    virtual bool Supports_Compressed_Textures() const override;
    virtual bool Supports_Bump_Envmap() const override;
    virtual bool Supports_Bump_Envmap_Luminance() const override;
    virtual bool Supports_Texture_Filter(RenderBackendTextureFilterCapability capability) const override;
    virtual bool Supports_Texture_Op(RenderBackendTextureOpCapability capability) const override;
    virtual bool Supports_Fog() const override;
    virtual bool Is_Legacy_Voodoo3() const override;
    virtual bool Supports_NPatches() const override;
    virtual bool Supports_Hardware_Transform_And_Lighting() const override;
    virtual bool Supports_Point_Sprites() const override;
    virtual RenderBackendTextureLimits Get_Texture_Limits() const override;
    virtual int Get_Max_Texture_Stages() const override;
    virtual bool Supports_Z_Bias() const override;
    virtual void Set_MSAA_Mode(RenderBackendMSAAMode mode);
    virtual RenderBackendMSAAMode Get_MSAA_Mode() const;
    virtual bool Supports_Dot3() const;
    virtual bool Get_Device_Identity(RenderBackendDeviceIdentity & identity) const override;
    virtual void Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit);

    // -- Frame lifecycle ------------------------------------------------------

    virtual void Begin_Scene();
    virtual void End_Scene(bool flip_frame);
    virtual void Flip_To_Primary();
    virtual void Begin_Device_Statistics() override;
    virtual void End_Device_Statistics() override;
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil);
    virtual void Set_Viewport(const RenderBackendViewport & viewport);
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

    virtual void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream);
    virtual void Set_Vertex_Buffer(const DynamicVBAccessClass & vba);
    virtual void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset);
    virtual void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset);
    virtual void Set_Index_Buffer_Index_Offset(unsigned int offset);
    virtual void Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & state) override;
    virtual void Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & state) override;
    virtual void Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & state) override;
    virtual void Release_Legacy_Render_State_For_Sorted_Draw() override;

    // -- State: shaders, materials, textures ---------------------------------

    virtual void Set_Shader(const ShaderClass & shader);
    virtual void Get_Shader(ShaderClass & shader);
    virtual void Set_Material(const VertexMaterialClass * material);
    virtual void Apply_Material_State(const RenderBackendMaterialState & material) override;
    virtual void Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source,
                                           RenderBackendMaterialColorSource diffuse_source,
                                           RenderBackendMaterialColorSource emissive_source) override;
    virtual void Set_Texture(unsigned int stage, TextureBaseClass * texture);
    virtual void Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture);
    virtual void Upload_Texture_Region(
        TextureClass * dst_texture,
        unsigned int dst_level,
        unsigned int dst_x, unsigned int dst_y,
        const void * src_data,
        unsigned int src_pitch,
        unsigned int region_width, unsigned int region_height,
        WW3DFormat format);
    virtual void Apply_Render_State_Changes();
    virtual void Apply_Default_State();
    virtual void Invalidate_Cached_Render_States();
    virtual void Set_Blend_Op(BlendOp op);
    virtual void Set_Blend_Factors(BlendFactor src, BlendFactor dest);
    virtual void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha);
    virtual void Set_Alpha_Blend_Enable(bool enable);
    virtual void Set_Alpha_Test_Enable(bool enable);
    virtual void Set_Alpha_Test_Reference(unsigned ref);
    virtual void Set_Alpha_Test_Function(CompareFunc func);
    virtual void Set_Normalize_Normals(bool enable);
    virtual void Show_Hardware_Cursor(bool show);
    virtual void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, const RenderBackendImage & image) override;
    virtual void Set_Hardware_Cursor_Position(int x, int y);
    virtual void Set_Stencil_Enable(bool enable);
    virtual void Set_Stencil_Func(CompareFunc func);
    virtual void Set_Stencil_Ref(unsigned int ref);
    virtual void Set_Stencil_Mask(unsigned int mask);
    virtual void Set_Stencil_Write_Mask(unsigned int mask);
    virtual void Set_Stencil_Pass_Op(StencilOp op);
    virtual void Set_Stencil_Fail_Op(StencilOp op);
    virtual void Set_Stencil_ZFail_Op(StencilOp op);

    // Extended render-state setters (see IRenderBackend.h).
    virtual void Set_Z_Bias(int bias);
    virtual void Set_Fill_Mode(FillMode mode);
    virtual void Set_Shade_Mode(ShadeMode mode);
    virtual void Set_Depth_Test_Enable(bool enable);
    virtual void Set_Depth_Write_Enable(bool enable);
    virtual void Set_Depth_Func(CompareFunc func);
    virtual bool Supports_Color_Write_Mask() const override;
    virtual unsigned Get_Color_Write_Mask() const override;
    virtual void Set_Color_Write_Mask(unsigned mask) override;
    virtual void Set_Lighting_Enable(bool enable);
    virtual void Set_Point_Sprite_Enable(bool enable) override;
    virtual void Set_Point_Scale_Enable(bool enable) override;
    virtual void Set_Point_Size(float size, float min_size, float max_size) override;
    virtual void Set_Point_Scale(float a, float b, float c) override;
    virtual void Set_Texture_Factor(unsigned argb);
    virtual void Configure_Grayscale_Texture_Stages() override;
    virtual void Configure_Custom_Edging_Cloud_Texture_Stages() override;
    virtual void Configure_Shadow_Volume_Fill_Texture_Stages() override;
    virtual void Set_Texture_Transform(unsigned stage, const Matrix4x4 & matrix) override;
    virtual void Set_Texture_Coord_Source(unsigned stage,
                                          RenderBackendTexcoordSource source,
                                          unsigned uv_array_index = 0) override;
    virtual void Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected) override;
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
    virtual void Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value) override;
    virtual CullMode Get_Cull_Mode() const override;
    virtual void Set_Cull_Mode(CullMode mode) override;

    // TheSuperHackers @bugfix bobtista 01/06/2026 Forward Override_* state
    // overrides 1:1 to the legacy DX8Wrapper render-state calls so the dx8
    // backend reproduces the pre-refactor rendering. Without these, the
    // terrain 2-pass blend, road alpha-test, water destalpha trick, and
    // custom-edging passes all execute with stale render state (e.g. terrain
    // pass 1 keeps texcoord index 0, ALPHABLENDENABLE off) so terrain tiles
    // never write to the framebuffer.
    virtual void Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend) override;
    virtual void Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func) override;
    virtual void Override_Alpha_Blend_Enable(bool enable) override;
    virtual void Override_Texcoord_Index(unsigned stage, unsigned uvIndex) override;

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
    virtual void Clear_Light(unsigned int index);
    virtual void Set_Ambient(const Vector3 & color);
    virtual const Vector3 & Get_Ambient() const;
    virtual void Set_Fog(bool enable, const Vector3 & color, float start, float end);
    virtual void Set_Fog_Enable(bool enable) override;
    virtual void Set_Fog_Color(unsigned argb) override;
    virtual unsigned Get_Fog_Color() const override;

    // TheSuperHackers @bugfix bobtista 02/06/2026 Additional forwarders
    // for the override calls the W3DWater batched draw path makes (commit
    // 0dc6548f2). BgfxBackend implements them; the IRenderBackend defaults
    // are empty no-ops. Override_Alpha_Blend_Enable is already declared
    // above as part of the Override_* set; the two below are unique to the
    // water batched draw and keep the override mechanism functional on dx8.
    virtual void Override_Material_Opacity(float opacity) override;
    virtual void Clear_State_Overrides() override;
    virtual void Apply_Stencil_Shadow_Darken(unsigned shadow_color,
                                             unsigned stencil_read_mask,
                                             unsigned stencil_ref,
                                             int x,
                                             int y,
                                             int width,
                                             int height) override;
    virtual bool Get_Fog_Enable() const;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env);
    virtual LightEnvironmentClass * Get_Light_Environment() const;
    virtual void Set_Specular_Enable(bool enable) override;
    virtual void Set_Patch_Segments(float level) override;

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
    virtual bool Is_Triangle_Draw_Enabled() const override;
    virtual void Set_Triangle_Draw_Enabled(bool enable) override;
    virtual void Draw_Screen_Color_Quad(unsigned color,
                                        int x,
                                        int y,
                                        int width,
                                        int height) override;
    virtual void Draw_Strip(unsigned short start_index,
                            unsigned short index_count,
                            unsigned short min_vertex_index,
                            unsigned short vertex_count);

    // -- Programmable pipeline ------------------------------------------------

    virtual const unsigned int * Get_Legacy_Vertex_Shader_Declaration(
        RenderBackendLegacyVertexDeclaration declaration) const override;
    virtual bool Create_Vertex_Shader(const unsigned int * declaration,
                                      const unsigned int * shader,
                                      unsigned int usage,
                                      unsigned long * handle) override;
    virtual bool Create_Pixel_Shader(const unsigned int * shader,
                                     unsigned long * handle) override;
    virtual void Delete_Vertex_Shader(unsigned long vertex_shader) override;
    virtual void Delete_Pixel_Shader(unsigned long pixel_shader) override;
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

    // -- Resource creation (asset ingress) -----------------------------------

    virtual RenderResource Create_Texture(const TextureDesc & desc);
    virtual RenderResource Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data);
    virtual RenderResource Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit);
    virtual void   Destroy_Resource(RenderResource h);

    virtual RenderResource Register_Texture_Resource(TextureBaseClass * tex);
    virtual RenderResource Register_Vertex_Buffer_Resource(VertexBufferClass * vb);
    virtual RenderResource Register_Index_Buffer_Resource(IndexBufferClass * ib);
};
