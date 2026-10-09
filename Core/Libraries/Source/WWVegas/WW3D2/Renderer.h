/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// TheSuperHackers @refactor bobtista 09/10/2026 Static render facade. Call sites use Renderer::X.
// By default Renderer::X calls the linked backend class directly. With GGC_RENDERER_VIRTUAL it
// dispatches through IRenderBackend, so a build can hold more than one backend.

#pragma once

#include "IRenderBackend.h"

typedef RenderBackendViewport RenderViewport;

class Renderer
{
public:
    // Initialized in WW3D::Init and shut down in WW3D::Shutdown.
    static bool Init(void * window, bool lite);
    static void Shutdown();
    static bool Is_Initialized();

    static bool Has_Shader_Pipeline();
    static void Invalidate_Cached_Texture(TextureBaseClass * texture);
    static void Copy_Render_Target_To_Texture(TextureClass * dst_texture, TextureClass * src_render_target);
    static void Release_Cached_Texture(TextureBaseClass * texture);
    static void Set_Present_Letterbox(bool enabled, float aspectW, float aspectH);
    static bool Is_Present_Letterbox_Active();
    static int Get_Present_Content_Width();
    static int Get_Present_Content_Height();
    static int Get_Present_Offset_X();
    static int Get_Present_Offset_Y();
    static void Initialize(void * hwnd, int width, int height);
    static void Shutdown_Context();
    static bool Init_Render_System(void * hwnd, bool lite);
    static void Shutdown_Render_System();
    static bool Set_Render_Device(const char * dev_name, int width, int height, int bits, int windowed, bool resize_window);
    static bool Set_Render_Device(int dev, int width, int height, int bits, int windowed, bool resize_window, bool reset_device, bool restore_assets);
    static bool Set_Any_Render_Device();
    static bool Set_Next_Render_Device();
    static bool Toggle_Windowed();
    static bool Is_Windowed();
    static int Get_Render_Device();
    static const RenderDeviceDescClass & Get_Render_Device_Desc(int deviceidx);
    static int Get_Render_Device_Count();
    static const char * Get_Render_Device_Name(int device_index);
    static bool Set_Device_Resolution(int width, int height, int bits, int windowed, bool resize_window);
    static void Get_Render_Target_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed);
    static void Get_Device_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed);
    static int Get_Device_Resolution_Width();
    static int Get_Device_Resolution_Height();
    static bool Registry_Save_Render_Device(const char * sub_key);
    static bool Registry_Save_Render_Device(const char * sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth);
    static bool Registry_Load_Render_Device(const char * sub_key, bool resize_window);
    static bool Registry_Load_Render_Device(const char * sub_key, char * device, int device_len, int & width, int & height, int & depth, int & windowed, int & texture_depth);
    static void Set_Swap_Interval(int swap);
    static int Get_Swap_Interval();
    static bool Is_Device_Lost();
    static RenderBackendDeviceStatus Get_Device_Status();
    static void Reset_Device();
    static void Set_Device_Cleanup_Hook(RenderDeviceCleanupHook * hook);
    static bool Has_Stencil();
    static WW3DFormat Get_Back_Buffer_Format();
    static bool Get_Back_Buffer_Description(unsigned int num, RenderBackendSurfaceDescription & desc);
    static bool Capture_Back_Buffer_Image(unsigned int num, RenderBackendImage & image);
    static bool Copy_Back_Buffer_To_Texture(unsigned int num, TextureClass * dst_texture);
    static bool Supports_Native_Screen_Shot();
    static bool Request_Native_Screen_Shot(const char * path);
    static void Set_Texture_Bitdepth(int bitdepth);
    static int Get_Texture_Bitdepth();
    static bool Supports_Texture_Format(WW3DFormat format);
    static bool Supports_Compressed_Textures();
    static bool Supports_Bump_Envmap();
    static bool Supports_Bump_Envmap_Luminance();
    static bool Supports_Texture_Filter(RenderBackendTextureFilterCapability capability);
    static bool Supports_Texture_Op(RenderBackendTextureOpCapability capability);
    static bool Supports_Fog();
    static bool Is_Legacy_Voodoo3();
    static bool Supports_NPatches();
    static bool Supports_Hardware_Transform_And_Lighting();
    static bool Supports_Point_Sprites();
    static RenderBackendTextureLimits Get_Texture_Limits();
    static int Get_Max_Texture_Stages();
    static int Get_Max_Texture_Size();
    static bool Supports_Z_Bias();
    static void Set_MSAA_Mode(RenderBackendMSAAMode mode);
    static RenderBackendMSAAMode Get_MSAA_Mode();
    static bool Supports_Dot3();
    static bool Get_Device_Identity(RenderBackendDeviceIdentity & identity);
    static void Set_Gamma(float gamma, float bright, float contrast, bool calibrate = true, bool uselimit = true);
    static void Begin_Scene();
    static void End_Scene(bool flip_frame = true);
    static void Flip_To_Primary();
    static void Begin_Device_Statistics();
    static void End_Device_Statistics();
    static void Clear(bool clear_color, bool clear_z_stencil, const Vector3 & color, float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0);
    static void Set_Viewport(const RenderBackendViewport & viewport);
    static bool Initialize_View_Capture(RenderBackendViewCaptureKind kind);
    static void Release_View_Capture(RenderBackendViewCaptureKind kind);
    static bool Supports_View_Capture(RenderBackendViewCaptureKind kind);
    static bool Begin_View_Capture(RenderBackendViewCaptureKind kind);
    static bool End_View_Capture(RenderBackendViewCaptureKind kind);
    static bool Is_View_Capture_Active(RenderBackendViewCaptureKind kind);
    static bool Has_View_Capture(RenderBackendViewCaptureKind kind);
    static bool Bind_View_Capture_Texture(RenderBackendViewCaptureKind kind, unsigned int stage);
    static bool Draw_View_Capture_Quad(RenderBackendViewCaptureKind kind, const RenderBackendScreenVertex * vertices, unsigned int vertex_count, bool use_second_uv);
    static bool Draw_Screen_Quad(const RenderBackendScreenVertex * vertices, unsigned int vertex_count, bool use_second_uv);
    static bool Capture_Back_Buffer_RGBA(unsigned int display_width, unsigned int display_height, unsigned int image_size, unsigned char * output_pixels, unsigned int output_capacity, unsigned int * output_width, unsigned int * output_height);
    static void Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream = 0);
    static void Set_Vertex_Buffer(const DynamicVBAccessClass & vba);
    static void Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset);
    static void Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset);
    static void Set_Index_Buffer_Index_Offset(unsigned int offset);
    static void Upload_Vertex_Buffer_Data(const VertexBufferClass * vb, const void * data, unsigned int size_bytes);
    static void Upload_Index_Buffer_Data(const IndexBufferClass * ib, const void * data, unsigned int size_bytes);
    static void Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba, const void * data, unsigned int size_bytes);
    static void Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba, const void * data, unsigned int size_bytes);
    static bool Supports_Instancing();
    static bool Begin_Instanced_Batch(unsigned max_instances);
    static void Add_Instance(const float * world_matrix_4x4);
    static void Submit_Instanced_Batch(unsigned index_offset, unsigned triangle_count, unsigned min_vertex_index, unsigned vertex_count);
    static void * Begin_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba, unsigned int size_bytes);
    static void End_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba, const void * data, unsigned int size_bytes);
    static void * Begin_Dynamic_Index_Write(const DynamicIBAccessClass * iba, unsigned int size_bytes);
    static void End_Dynamic_Index_Write(const DynamicIBAccessClass * iba, const void * data, unsigned int size_bytes);
    static void Upload_Vertex_Buffer_Sub_Range(const VertexBufferClass * vb, const void * data, unsigned int start_vertex, unsigned int size_bytes);
    static void Upload_Index_Buffer_Sub_Range(const IndexBufferClass * ib, const void * data, unsigned int start_index, unsigned int size_bytes);
    static void Begin_Sorted_Batch_Pass();
    static void End_Sorted_Batch_Pass();
    static void Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & state);
    static void Set_Point_Group_Render_Active(bool active);
    static void Set_Streak_Render_Active(bool active);
    static void Set_Mesh_Render_Active(bool active);
    static void Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & state);
    static void Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & state);
    static void Release_Legacy_Render_State_For_Sorted_Draw();
    static void Set_Sorted_Texture_Array_Page(int page);
    static bool Submit_Sorted_Packet(const RenderBackendSortedBatchState & packet, unsigned int start_index, unsigned int polygon_count, unsigned int vertex_count, int array_page);
    static bool Submit_Rigid_Packet(int ib_base_offset, unsigned int start_index, unsigned int primitive_count, unsigned int min_vertex_index, unsigned int vertex_count, bool triangle_strip);
    static void Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb, const DynamicIBAccessClass & dyn_ib, unsigned short polygon_count, unsigned short vertex_count);
    static void Set_Shader(const ShaderClass & shader);
    static void Get_Shader(ShaderClass & shader);
    static void Set_Material(const VertexMaterialClass * material);
    static void Apply_Material_State(const RenderBackendMaterialState & material);
    static void Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source, RenderBackendMaterialColorSource diffuse_source, RenderBackendMaterialColorSource emissive_source);
    static void Set_Texture(unsigned int stage, TextureBaseClass * texture);
    static void Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture);
    static void Upload_Texture_Region(TextureClass * dst_texture, unsigned int dst_level, unsigned int dst_x, unsigned int dst_y, const void * src_data, unsigned int src_pitch, unsigned int region_width, unsigned int region_height, WW3DFormat format);
    static void Apply_Render_State_Changes();
    static void Apply_Default_State();
    static void Invalidate_Cached_Render_States();
    static void Set_Blend_Op(BlendOp op);
    static void Set_Blend_Factors(BlendFactor src, BlendFactor dest);
    static void Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha);
    static void Set_Alpha_Blend_Enable(bool enable);
    static void Set_Alpha_Test_Enable(bool enable);
    static void Set_Alpha_Test_Reference(unsigned ref);
    static void Set_Alpha_Test_Function(CompareFunc func);
    static void Set_Alpha_Test(bool enable, unsigned ref, CompareFunc func);
    static void Set_Normalize_Normals(bool enable);
    static void Show_Hardware_Cursor(bool show);
    static void Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, const RenderBackendImage & image);
    static void Set_Hardware_Cursor_Position(int x, int y);
    static void Set_Stencil_Enable(bool enable);
    static void Set_Stencil_Func(CompareFunc func);
    static void Set_Stencil_Ref(unsigned int ref);
    static void Set_Stencil_Mask(unsigned int mask);
    static void Set_Stencil_Write_Mask(unsigned int mask);
    static void Set_Stencil_Pass_Op(StencilOp op);
    static void Set_Stencil_Fail_Op(StencilOp op);
    static void Set_Stencil_ZFail_Op(StencilOp op);
    static void Set_Z_Bias(int bias);
    static void Set_Normal_Bias(float bias);
    static void Set_Fill_Mode(FillMode mode);
    static void Set_Shade_Mode(ShadeMode mode);
    static void Set_Depth_Test_Enable(bool enable);
    static void Set_Depth_Write_Enable(bool enable);
    static void Set_Depth_Func(CompareFunc func);
    static bool Supports_Color_Write_Mask();
    static unsigned Get_Color_Write_Mask();
    static void Set_Color_Write_Mask(unsigned mask);
    static void Set_Lighting_Enable(bool enable);
    static void Set_Point_Sprite_Enable(bool enable);
    static void Set_Point_Scale_Enable(bool enable);
    static void Set_Point_Size(float size, float min_size, float max_size);
    static void Set_Point_Scale(float a, float b, float c);
    static void Set_Texture_Factor(unsigned argb);
    static CullMode Get_Cull_Mode();
    static void Set_Cull_Mode(CullMode mode);
    static void Set_Tree_Shader_Constants(const float swayTable[11][4], const float shroudOffset[4], const float shroudScale[4]);
    static void Set_Tree_Vertex_Shader_Active(bool active);
    static void Set_Grayscale_Mode(bool enable);
    static void Configure_Grayscale_Texture_Stages();
    static void Configure_Custom_Edging_Cloud_Texture_Stages();
    static void Configure_Shadow_Volume_Fill_Texture_Stages();
    static void Set_Cloud_Shadow_Params(bool enable, float scroll_x, float scroll_y, float stretch, TextureClass * cloud_tex);
    static void Set_Light_Map_Params(bool enable, float stretch, TextureClass * noise_tex);
    static void Set_Transform(TransformKind transform, const Matrix4x4 & m);
    static void Set_Transform(TransformKind transform, const Matrix3D & m);
    static void Get_Transform(TransformKind transform, Matrix4x4 & m);
    static void Set_World_Identity();
    static void Set_View_Identity();
    static bool Is_World_Identity();
    static bool Is_View_Identity();
    static void Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar);
    static void Set_Light(unsigned int index, const LightClass & light);
    static void Clear_Light(unsigned int index);
    static void Set_Ambient(const Vector3 & color);
    static const Vector3 & Get_Ambient();
    static void Set_Fog(bool enable, const Vector3 & color, float start, float end);
    static void Set_Fog_Enable(bool enable);
    static void Set_Fog_Color(unsigned argb);
    static unsigned Get_Fog_Color();
    static bool Get_Fog_Enable();
    static void Set_Light_Environment(LightEnvironmentClass * light_env);
    static LightEnvironmentClass * Get_Light_Environment();
    static void Set_Specular_Enable(bool enable);
    static void Set_Patch_Segments(float level);
    static void Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend);
    static void Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func);
    static void Override_Alpha_Blend_Enable(bool enable);
    static void Override_Texcoord_Index(unsigned stage, unsigned uvIndex);
    static void Override_Terrain_Blend(bool enable);
    static void Override_Material_Opacity(float opacity);
    static void Set_Texture_Transform(unsigned stage, const Matrix4x4& matrix);
    static void Clear_Texture_Transform(unsigned stage);
    static void Set_Texture_Coord_Source(unsigned stage, RenderBackendTexcoordSource source, unsigned uv_array_index = 0);
    static void Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected);
    static void Set_Texture_Bump_Env_Matrix(unsigned stage, float m00, float m01, float m10, float m11);
    static void Set_Texture_Bump_Env_Luminance(unsigned stage, float scale, float offset);
    static void Set_Texture_Color_Operation(unsigned stage, RenderBackendTextureOperation op);
    static void Set_Texture_Alpha_Operation(unsigned stage, RenderBackendTextureOperation op);
    static void Set_Texture_Color_Argument(unsigned stage, unsigned argument_index, RenderBackendTextureArgument arg);
    static void Set_Texture_Alpha_Argument(unsigned stage, unsigned argument_index, RenderBackendTextureArgument arg);
    static void Set_Texture_Coord_Generation(unsigned stage, bool cameraPosEnabled);
    static void Set_Texture_UV_Wrap(unsigned stage, bool enable);
    static void Set_Texture_Address_Mode(unsigned stage, RenderBackendTextureAddressMode u, RenderBackendTextureAddressMode v, RenderBackendTextureAddressMode w);
    static void Set_Texture_Sample_Filter(unsigned stage, RenderBackendTextureSampleFilter min_filter, RenderBackendTextureSampleFilter mag_filter, RenderBackendTextureSampleFilter mip_filter);
    static void Set_Texture_Min_Mag_Filter(unsigned stage, RenderBackendTextureSampleFilter min_filter, RenderBackendTextureSampleFilter mag_filter);
    static void Set_Texture_Mip_Filter(unsigned stage, RenderBackendTextureSampleFilter mip_filter);
    static void Set_Texture_Max_Anisotropy(unsigned stage, unsigned max_anisotropy);
    static void Set_Texture_Clamp_Mode(unsigned stage, bool clampU, bool clampV);
    static void Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value);
    static void Set_Shroud_Texture_Pass_Active(bool active, unsigned stage);
    static void Set_Object_Shroud_Texture_Pass_Active(bool active);
    static void Set_Object_Shroud_Alpha_Mask_Texture(TextureBaseClass * texture);
    static void Set_Shroud_Texture_Params(float offset_x, float offset_y, float scale_x, float scale_y);
    static void Create_Effect_Resources(bool allow_pixel_shaders);
    static void Release_Effect_Resources();
    static void Draw_Road(const RenderBackendRoadDraw & road, unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count);
    static void Draw_Terrain(const RenderBackendTerrainDraw & terrain, VertexBufferClass * const * vertex_buffers, unsigned int vertex_buffer_count, unsigned short polygon_count, unsigned short vertex_count);
    static void Begin_Shroud_Overlay(const RenderBackendShroud & shroud);
    static void End_Shroud_Overlay();
    static void Draw_Water(const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count);
    static void Set_World_Texture_Projection(unsigned stage, const RenderBackendWorldProjection & projection);
    static void Clear_Texture_Projection(unsigned stage);
    static bool Requires_Delayed_Object_Shroud_Pass();
    static void Begin_Water_Overlay();
    static void End_Water_Overlay();
    static void Begin_Effect_Overlay();
    static void End_Effect_Overlay();
    static bool Begin_Smudge_Distortion(float tactical_width_fraction = 1.0f, float tactical_height_fraction = 1.0f);
    static void End_Smudge_Distortion();
    static void Clear_State_Overrides();
    static void Draw_Triangles(unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count);
    static void Draw_Triangles(unsigned int buffer_type, unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count);
    static bool Is_Triangle_Draw_Enabled();
    static void Set_Triangle_Draw_Enabled(bool enable);
    static void Draw_Screen_Color_Quad(unsigned color, int x, int y, int width, int height);
    static void Draw_Screen_Multiply_Quad(unsigned color, int x, int y, int width, int height);
    static void Draw_Strip(unsigned short start_index, unsigned short index_count, unsigned short min_vertex_index, unsigned short vertex_count);
    static void Skip_Next_Bgfx_Submit();
    static void Set_Projected_Shadow_Decal_Active(bool active);
    static void Set_Projected_Decal_Mode(RenderBackendProjectedDecalMode mode);
    static void Set_Shadow_Volume_Shader_Active(bool active);
    static void Apply_Stencil_Shadow_Darken(unsigned shadow_color, unsigned stencil_read_mask, unsigned stencil_ref, int x, int y, int width, int height);
    static void Submit_Shadow_Volume_Caps(unsigned strip_start_vertex, unsigned num_silhouette_verts);
    static void Submit_Shadow_Volume_Triangulated_Caps(unsigned strip_start_vertex, const short * local_cap_indices, unsigned cap_index_count);
    static bool Needs_Closed_Shadow_Volumes();
    static void Capture_Shroud_Texture(TextureClass * dst_texture, const void * pixel_data, unsigned dst_width, unsigned dst_height, unsigned src_width, unsigned src_height, unsigned src_x, unsigned src_y, unsigned dst_x, unsigned dst_y, unsigned pitch, WW3DFormat format, unsigned border_pixel = 0xFFFFu);
    static const unsigned int * Get_Legacy_Vertex_Shader_Declaration(RenderBackendLegacyVertexDeclaration declaration);
    static bool Load_Legacy_Shader(const char * path, const unsigned int * declaration, unsigned int usage, RenderBackendShaderKind kind, unsigned long * handle);
    static bool Create_Vertex_Shader(const unsigned int * declaration, const unsigned int * shader, unsigned int usage, unsigned long * handle);
    static bool Create_Pixel_Shader(const unsigned int * shader, unsigned long * handle);
    static bool Create_Legacy_Pixel_Shader(RenderBackendLegacyPixelShaderMode mode, unsigned long * handle);
    static void Delete_Vertex_Shader(unsigned long vertex_shader);
    static void Delete_Pixel_Shader(unsigned long pixel_shader);
    static void Set_Vertex_Shader(unsigned long vertex_shader);
    static void Set_Pixel_Shader(unsigned long pixel_shader);
    static void Set_Vertex_Shader_Constant(int reg, const void * data, int count);
    static void Set_Pixel_Shader_Constant(int reg, const void * data, int count);
    static TextureClass * Create_Render_Target(int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN);
    static void Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture = nullptr);
    static bool Is_Render_To_Texture();
    static void Set_Shadow_Map(int idx, ZTextureClass * ztex);
    static ZTextureClass * Get_Shadow_Map(int idx);
    static bool Requires_Legacy_Buffer_Resources();
    static RenderResource Create_Texture(const TextureDesc & desc);
    static RenderResource Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data);
    static RenderResource Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit);
    static void Destroy_Resource(RenderResource h);
    static RenderResource Register_Texture_Resource(TextureBaseClass * tex);
    static RenderResource Register_Vertex_Buffer_Resource(VertexBufferClass * vb);
    static RenderResource Register_Index_Buffer_Resource(IndexBufferClass * ib);
};
