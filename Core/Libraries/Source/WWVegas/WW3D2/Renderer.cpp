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

#include "Renderer.h"

#include "WWDebug/wwdebug.h"

#if defined(GGC_RENDER_BACKEND_BGFX)
#include "Backend/BgfxBackend.h"
typedef BgfxBackend LinkedBackend;
#else
#include "Backend/DX8Backend.h"
typedef DX8Backend LinkedBackend;
#endif

#if defined(GGC_RENDERER_VIRTUAL)
static IRenderBackend * Backend = nullptr;
#define RENDERER_CALL(name) Backend->name
#else
static LinkedBackend * Backend = nullptr;
#define RENDERER_CALL(name) Backend->LinkedBackend::name
#endif

static LinkedBackend * Create_Linked_Backend(void * window, bool lite)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
    (void)window;
    (void)lite;
    return new BgfxBackend();
#else
    return DX8Backend::Create(window, lite);
#endif
}

bool Renderer::Init(void * window, bool lite)
{
    WWASSERT(Backend == nullptr);
    if (Backend == nullptr)
    {
        Backend = Create_Linked_Backend(window, lite);
    }
    return Backend != nullptr;
}

void Renderer::Shutdown()
{
    delete Backend;
    Backend = nullptr;
}

bool Renderer::Is_Initialized()
{
    return Backend != nullptr;
}

bool Renderer::Has_Shader_Pipeline()
{
    return RENDERER_CALL(Has_Shader_Pipeline)();
}

void Renderer::Invalidate_Cached_Texture(TextureBaseClass * texture)
{
    return RENDERER_CALL(Invalidate_Cached_Texture)(texture);
}

void Renderer::Copy_Render_Target_To_Texture(TextureClass * dst_texture, TextureClass * src_render_target)
{
    return RENDERER_CALL(Copy_Render_Target_To_Texture)(dst_texture, src_render_target);
}

void Renderer::Release_Cached_Texture(TextureBaseClass * texture)
{
    return RENDERER_CALL(Release_Cached_Texture)(texture);
}

void Renderer::Set_Present_Letterbox(bool enabled, float aspectW, float aspectH)
{
    return RENDERER_CALL(Set_Present_Letterbox)(enabled, aspectW, aspectH);
}

bool Renderer::Is_Present_Letterbox_Active()
{
    return RENDERER_CALL(Is_Present_Letterbox_Active)();
}

int Renderer::Get_Present_Content_Width()
{
    return RENDERER_CALL(Get_Present_Content_Width)();
}

int Renderer::Get_Present_Content_Height()
{
    return RENDERER_CALL(Get_Present_Content_Height)();
}

int Renderer::Get_Present_Offset_X()
{
    return RENDERER_CALL(Get_Present_Offset_X)();
}

int Renderer::Get_Present_Offset_Y()
{
    return RENDERER_CALL(Get_Present_Offset_Y)();
}

void Renderer::Initialize(void * hwnd, int width, int height)
{
    return RENDERER_CALL(Initialize)(hwnd, width, height);
}

void Renderer::Shutdown_Context()
{
    return RENDERER_CALL(Shutdown)();
}

bool Renderer::Init_Render_System(void * hwnd, bool lite)
{
    return RENDERER_CALL(Init_Render_System)(hwnd, lite);
}

void Renderer::Shutdown_Render_System()
{
    return RENDERER_CALL(Shutdown_Render_System)();
}

bool Renderer::Set_Render_Device(const char * dev_name, int width, int height, int bits, int windowed, bool resize_window)
{
    return RENDERER_CALL(Set_Render_Device)(dev_name, width, height, bits, windowed, resize_window);
}

bool Renderer::Set_Render_Device(int dev, int width, int height, int bits, int windowed, bool resize_window, bool reset_device, bool restore_assets)
{
    return RENDERER_CALL(Set_Render_Device)(dev, width, height, bits, windowed, resize_window, reset_device, restore_assets);
}

bool Renderer::Set_Any_Render_Device()
{
    return RENDERER_CALL(Set_Any_Render_Device)();
}

bool Renderer::Set_Next_Render_Device()
{
    return RENDERER_CALL(Set_Next_Render_Device)();
}

bool Renderer::Toggle_Windowed()
{
    return RENDERER_CALL(Toggle_Windowed)();
}

bool Renderer::Is_Windowed()
{
    return RENDERER_CALL(Is_Windowed)();
}

int Renderer::Get_Render_Device()
{
    return RENDERER_CALL(Get_Render_Device)();
}

const RenderDeviceDescClass & Renderer::Get_Render_Device_Desc(int deviceidx)
{
    return RENDERER_CALL(Get_Render_Device_Desc)(deviceidx);
}

int Renderer::Get_Render_Device_Count()
{
    return RENDERER_CALL(Get_Render_Device_Count)();
}

const char * Renderer::Get_Render_Device_Name(int device_index)
{
    return RENDERER_CALL(Get_Render_Device_Name)(device_index);
}

bool Renderer::Set_Device_Resolution(int width, int height, int bits, int windowed, bool resize_window)
{
    return RENDERER_CALL(Set_Device_Resolution)(width, height, bits, windowed, resize_window);
}

void Renderer::Get_Render_Target_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed)
{
    return RENDERER_CALL(Get_Render_Target_Resolution)(set_w, set_h, set_bits, set_windowed);
}

void Renderer::Get_Device_Resolution(int & set_w, int & set_h, int & set_bits, bool & set_windowed)
{
    return RENDERER_CALL(Get_Device_Resolution)(set_w, set_h, set_bits, set_windowed);
}

int Renderer::Get_Device_Resolution_Width()
{
    return RENDERER_CALL(Get_Device_Resolution_Width)();
}

int Renderer::Get_Device_Resolution_Height()
{
    return RENDERER_CALL(Get_Device_Resolution_Height)();
}

bool Renderer::Registry_Save_Render_Device(const char * sub_key)
{
    return RENDERER_CALL(Registry_Save_Render_Device)(sub_key);
}

bool Renderer::Registry_Save_Render_Device(const char * sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth)
{
    return RENDERER_CALL(Registry_Save_Render_Device)(sub_key, device, width, height, depth, windowed, texture_depth);
}

bool Renderer::Registry_Load_Render_Device(const char * sub_key, bool resize_window)
{
    return RENDERER_CALL(Registry_Load_Render_Device)(sub_key, resize_window);
}

bool Renderer::Registry_Load_Render_Device(const char * sub_key, char * device, int device_len, int & width, int & height, int & depth, int & windowed, int & texture_depth)
{
    return RENDERER_CALL(Registry_Load_Render_Device)(sub_key, device, device_len, width, height, depth, windowed, texture_depth);
}

void Renderer::Set_Swap_Interval(int swap)
{
    return RENDERER_CALL(Set_Swap_Interval)(swap);
}

int Renderer::Get_Swap_Interval()
{
    return RENDERER_CALL(Get_Swap_Interval)();
}

bool Renderer::Is_Device_Lost()
{
    return RENDERER_CALL(Is_Device_Lost)();
}

RenderBackendDeviceStatus Renderer::Get_Device_Status()
{
    return RENDERER_CALL(Get_Device_Status)();
}

void Renderer::Reset_Device()
{
    return RENDERER_CALL(Reset_Device)();
}

void Renderer::Set_Device_Cleanup_Hook(RenderDeviceCleanupHook * hook)
{
    return RENDERER_CALL(Set_Device_Cleanup_Hook)(hook);
}

bool Renderer::Has_Stencil()
{
    return RENDERER_CALL(Has_Stencil)();
}

WW3DFormat Renderer::Get_Back_Buffer_Format()
{
    return RENDERER_CALL(Get_Back_Buffer_Format)();
}

bool Renderer::Get_Back_Buffer_Description(unsigned int num, RenderBackendSurfaceDescription & desc)
{
    return RENDERER_CALL(Get_Back_Buffer_Description)(num, desc);
}

bool Renderer::Capture_Back_Buffer_Image(unsigned int num, RenderBackendImage & image)
{
    return RENDERER_CALL(Capture_Back_Buffer_Image)(num, image);
}

bool Renderer::Copy_Back_Buffer_To_Texture(unsigned int num, TextureClass * dst_texture)
{
    return RENDERER_CALL(Copy_Back_Buffer_To_Texture)(num, dst_texture);
}

bool Renderer::Supports_Native_Screen_Shot()
{
    return RENDERER_CALL(Supports_Native_Screen_Shot)();
}

bool Renderer::Request_Native_Screen_Shot(const char * path)
{
    return RENDERER_CALL(Request_Native_Screen_Shot)(path);
}

void Renderer::Set_Texture_Bitdepth(int bitdepth)
{
    return RENDERER_CALL(Set_Texture_Bitdepth)(bitdepth);
}

int Renderer::Get_Texture_Bitdepth()
{
    return RENDERER_CALL(Get_Texture_Bitdepth)();
}

bool Renderer::Supports_Texture_Format(WW3DFormat format)
{
    return RENDERER_CALL(Supports_Texture_Format)(format);
}

bool Renderer::Supports_Compressed_Textures()
{
    return RENDERER_CALL(Supports_Compressed_Textures)();
}

bool Renderer::Supports_Bump_Envmap()
{
    return RENDERER_CALL(Supports_Bump_Envmap)();
}

bool Renderer::Supports_Bump_Envmap_Luminance()
{
    return RENDERER_CALL(Supports_Bump_Envmap_Luminance)();
}

bool Renderer::Supports_Texture_Filter(RenderBackendTextureFilterCapability capability)
{
    return RENDERER_CALL(Supports_Texture_Filter)(capability);
}

bool Renderer::Supports_Texture_Op(RenderBackendTextureOpCapability capability)
{
    return RENDERER_CALL(Supports_Texture_Op)(capability);
}

bool Renderer::Supports_Fog()
{
    return RENDERER_CALL(Supports_Fog)();
}

bool Renderer::Is_Legacy_Voodoo3()
{
    return RENDERER_CALL(Is_Legacy_Voodoo3)();
}

bool Renderer::Supports_NPatches()
{
    return RENDERER_CALL(Supports_NPatches)();
}

bool Renderer::Supports_Hardware_Transform_And_Lighting()
{
    return RENDERER_CALL(Supports_Hardware_Transform_And_Lighting)();
}

bool Renderer::Supports_Point_Sprites()
{
    return RENDERER_CALL(Supports_Point_Sprites)();
}

RenderBackendTextureLimits Renderer::Get_Texture_Limits()
{
    return RENDERER_CALL(Get_Texture_Limits)();
}

int Renderer::Get_Max_Texture_Stages()
{
    return RENDERER_CALL(Get_Max_Texture_Stages)();
}

int Renderer::Get_Max_Texture_Size()
{
    return RENDERER_CALL(Get_Max_Texture_Size)();
}

bool Renderer::Supports_Z_Bias()
{
    return RENDERER_CALL(Supports_Z_Bias)();
}

void Renderer::Set_MSAA_Mode(RenderBackendMSAAMode mode)
{
    return RENDERER_CALL(Set_MSAA_Mode)(mode);
}

RenderBackendMSAAMode Renderer::Get_MSAA_Mode()
{
    return RENDERER_CALL(Get_MSAA_Mode)();
}

bool Renderer::Supports_Dot3()
{
    return RENDERER_CALL(Supports_Dot3)();
}

bool Renderer::Get_Device_Identity(RenderBackendDeviceIdentity & identity)
{
    return RENDERER_CALL(Get_Device_Identity)(identity);
}

void Renderer::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    return RENDERER_CALL(Set_Gamma)(gamma, bright, contrast, calibrate, uselimit);
}

void Renderer::Begin_Scene()
{
    return RENDERER_CALL(Begin_Scene)();
}

void Renderer::End_Scene(bool flip_frame)
{
    return RENDERER_CALL(End_Scene)(flip_frame);
}

void Renderer::Flip_To_Primary()
{
    return RENDERER_CALL(Flip_To_Primary)();
}

void Renderer::Begin_Device_Statistics()
{
    return RENDERER_CALL(Begin_Device_Statistics)();
}

void Renderer::End_Device_Statistics()
{
    return RENDERER_CALL(End_Device_Statistics)();
}

void Renderer::Clear(bool clear_color, bool clear_z_stencil, const Vector3 & color, float dest_alpha, float z, unsigned int stencil)
{
    return RENDERER_CALL(Clear)(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void Renderer::Set_Viewport(const RenderBackendViewport & viewport)
{
    return RENDERER_CALL(Set_Viewport)(viewport);
}

bool Renderer::Initialize_View_Capture(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(Initialize_View_Capture)(kind);
}

void Renderer::Release_View_Capture(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(Release_View_Capture)(kind);
}

bool Renderer::Supports_View_Capture(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(Supports_View_Capture)(kind);
}

bool Renderer::Begin_View_Capture(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(Begin_View_Capture)(kind);
}

bool Renderer::End_View_Capture(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(End_View_Capture)(kind);
}

bool Renderer::Is_View_Capture_Active(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(Is_View_Capture_Active)(kind);
}

bool Renderer::Has_View_Capture(RenderBackendViewCaptureKind kind)
{
    return RENDERER_CALL(Has_View_Capture)(kind);
}

bool Renderer::Bind_View_Capture_Texture(RenderBackendViewCaptureKind kind, unsigned int stage)
{
    return RENDERER_CALL(Bind_View_Capture_Texture)(kind, stage);
}

bool Renderer::Draw_View_Capture_Quad(RenderBackendViewCaptureKind kind, const RenderBackendScreenVertex * vertices, unsigned int vertex_count, bool use_second_uv)
{
    return RENDERER_CALL(Draw_View_Capture_Quad)(kind, vertices, vertex_count, use_second_uv);
}

bool Renderer::Draw_Screen_Quad(const RenderBackendScreenVertex * vertices, unsigned int vertex_count, bool use_second_uv)
{
    return RENDERER_CALL(Draw_Screen_Quad)(vertices, vertex_count, use_second_uv);
}

bool Renderer::Capture_Back_Buffer_RGBA(unsigned int display_width, unsigned int display_height, unsigned int image_size, unsigned char * output_pixels, unsigned int output_capacity, unsigned int * output_width, unsigned int * output_height)
{
    return RENDERER_CALL(Capture_Back_Buffer_RGBA)(display_width, display_height, image_size, output_pixels, output_capacity, output_width, output_height);
}

void Renderer::Set_Vertex_Buffer(const VertexBufferClass * vb, unsigned int stream)
{
    return RENDERER_CALL(Set_Vertex_Buffer)(vb, stream);
}

void Renderer::Set_Vertex_Buffer(const DynamicVBAccessClass & vba)
{
    return RENDERER_CALL(Set_Vertex_Buffer)(vba);
}

void Renderer::Set_Index_Buffer(const IndexBufferClass * ib, unsigned short index_base_offset)
{
    return RENDERER_CALL(Set_Index_Buffer)(ib, index_base_offset);
}

void Renderer::Set_Index_Buffer(const DynamicIBAccessClass & iba, unsigned short index_base_offset)
{
    return RENDERER_CALL(Set_Index_Buffer)(iba, index_base_offset);
}

void Renderer::Set_Index_Buffer_Index_Offset(unsigned int offset)
{
    return RENDERER_CALL(Set_Index_Buffer_Index_Offset)(offset);
}

void Renderer::Upload_Vertex_Buffer_Data(const VertexBufferClass * vb, const void * data, unsigned int size_bytes)
{
    return RENDERER_CALL(Upload_Vertex_Buffer_Data)(vb, data, size_bytes);
}

void Renderer::Upload_Index_Buffer_Data(const IndexBufferClass * ib, const void * data, unsigned int size_bytes)
{
    return RENDERER_CALL(Upload_Index_Buffer_Data)(ib, data, size_bytes);
}

void Renderer::Capture_Dynamic_Vertex_Data(const DynamicVBAccessClass * vba, const void * data, unsigned int size_bytes)
{
    return RENDERER_CALL(Capture_Dynamic_Vertex_Data)(vba, data, size_bytes);
}

void Renderer::Capture_Dynamic_Index_Data(const DynamicIBAccessClass * iba, const void * data, unsigned int size_bytes)
{
    return RENDERER_CALL(Capture_Dynamic_Index_Data)(iba, data, size_bytes);
}

bool Renderer::Supports_Instancing()
{
    return RENDERER_CALL(Supports_Instancing)();
}

bool Renderer::Begin_Instanced_Batch(unsigned max_instances)
{
    return RENDERER_CALL(Begin_Instanced_Batch)(max_instances);
}

void Renderer::Add_Instance(const float * world_matrix_4x4)
{
    return RENDERER_CALL(Add_Instance)(world_matrix_4x4);
}

void Renderer::Submit_Instanced_Batch(unsigned index_offset, unsigned triangle_count, unsigned min_vertex_index, unsigned vertex_count)
{
    return RENDERER_CALL(Submit_Instanced_Batch)(index_offset, triangle_count, min_vertex_index, vertex_count);
}

void * Renderer::Begin_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba, unsigned int size_bytes)
{
    return RENDERER_CALL(Begin_Dynamic_Vertex_Write)(vba, size_bytes);
}

void Renderer::End_Dynamic_Vertex_Write(const DynamicVBAccessClass * vba, const void * data, unsigned int size_bytes)
{
    return RENDERER_CALL(End_Dynamic_Vertex_Write)(vba, data, size_bytes);
}

void * Renderer::Begin_Dynamic_Index_Write(const DynamicIBAccessClass * iba, unsigned int size_bytes)
{
    return RENDERER_CALL(Begin_Dynamic_Index_Write)(iba, size_bytes);
}

void Renderer::End_Dynamic_Index_Write(const DynamicIBAccessClass * iba, const void * data, unsigned int size_bytes)
{
    return RENDERER_CALL(End_Dynamic_Index_Write)(iba, data, size_bytes);
}

void Renderer::Upload_Vertex_Buffer_Sub_Range(const VertexBufferClass * vb, const void * data, unsigned int start_vertex, unsigned int size_bytes)
{
    return RENDERER_CALL(Upload_Vertex_Buffer_Sub_Range)(vb, data, start_vertex, size_bytes);
}

void Renderer::Upload_Index_Buffer_Sub_Range(const IndexBufferClass * ib, const void * data, unsigned int start_index, unsigned int size_bytes)
{
    return RENDERER_CALL(Upload_Index_Buffer_Sub_Range)(ib, data, start_index, size_bytes);
}

void Renderer::Begin_Sorted_Batch_Pass()
{
    return RENDERER_CALL(Begin_Sorted_Batch_Pass)();
}

void Renderer::End_Sorted_Batch_Pass()
{
    return RENDERER_CALL(End_Sorted_Batch_Pass)();
}

void Renderer::Apply_Sorted_Batch_State(const RenderBackendSortedBatchState & state)
{
    return RENDERER_CALL(Apply_Sorted_Batch_State)(state);
}

void Renderer::Set_Point_Group_Render_Active(bool active)
{
    return RENDERER_CALL(Set_Point_Group_Render_Active)(active);
}

void Renderer::Set_Streak_Render_Active(bool active)
{
    return RENDERER_CALL(Set_Streak_Render_Active)(active);
}

void Renderer::Set_Mesh_Render_Active(bool active)
{
    return RENDERER_CALL(Set_Mesh_Render_Active)(active);
}

void Renderer::Capture_Legacy_Render_State_For_Sorted_Draw(RenderStateStruct & state)
{
    return RENDERER_CALL(Capture_Legacy_Render_State_For_Sorted_Draw)(state);
}

void Renderer::Restore_Legacy_Render_State_For_Sorted_Draw(const RenderStateStruct & state)
{
    return RENDERER_CALL(Restore_Legacy_Render_State_For_Sorted_Draw)(state);
}

void Renderer::Release_Legacy_Render_State_For_Sorted_Draw()
{
    return RENDERER_CALL(Release_Legacy_Render_State_For_Sorted_Draw)();
}

void Renderer::Set_Sorted_Texture_Array_Page(int page)
{
    return RENDERER_CALL(Set_Sorted_Texture_Array_Page)(page);
}

bool Renderer::Submit_Sorted_Packet(const RenderBackendSortedBatchState & packet, unsigned int start_index, unsigned int polygon_count, unsigned int vertex_count, int array_page)
{
    return RENDERER_CALL(Submit_Sorted_Packet)(packet, start_index, polygon_count, vertex_count, array_page);
}

bool Renderer::Submit_Rigid_Packet(int ib_base_offset, unsigned int start_index, unsigned int primitive_count, unsigned int min_vertex_index, unsigned int vertex_count, bool triangle_strip)
{
    return RENDERER_CALL(Submit_Rigid_Packet)(ib_base_offset, start_index, primitive_count, min_vertex_index, vertex_count, triangle_strip);
}

void Renderer::Submit_Sorted_Draw(const DynamicVBAccessClass & dyn_vb, const DynamicIBAccessClass & dyn_ib, unsigned short polygon_count, unsigned short vertex_count)
{
    return RENDERER_CALL(Submit_Sorted_Draw)(dyn_vb, dyn_ib, polygon_count, vertex_count);
}

void Renderer::Set_Shader(const ShaderClass & shader)
{
    return RENDERER_CALL(Set_Shader)(shader);
}

void Renderer::Get_Shader(ShaderClass & shader)
{
    return RENDERER_CALL(Get_Shader)(shader);
}

void Renderer::Set_Material(const VertexMaterialClass * material)
{
    return RENDERER_CALL(Set_Material)(material);
}

void Renderer::Apply_Material_State(const RenderBackendMaterialState & material)
{
    return RENDERER_CALL(Apply_Material_State)(material);
}

void Renderer::Set_Material_Color_Source(RenderBackendMaterialColorSource ambient_source, RenderBackendMaterialColorSource diffuse_source, RenderBackendMaterialColorSource emissive_source)
{
    return RENDERER_CALL(Set_Material_Color_Source)(ambient_source, diffuse_source, emissive_source);
}

void Renderer::Set_Texture(unsigned int stage, TextureBaseClass * texture)
{
    return RENDERER_CALL(Set_Texture)(stage, texture);
}

void Renderer::Bind_Texture_Immediate(unsigned int stage, TextureBaseClass * texture)
{
    return RENDERER_CALL(Bind_Texture_Immediate)(stage, texture);
}

void Renderer::Upload_Texture_Region(TextureClass * dst_texture, unsigned int dst_level, unsigned int dst_x, unsigned int dst_y, const void * src_data, unsigned int src_pitch, unsigned int region_width, unsigned int region_height, WW3DFormat format)
{
    return RENDERER_CALL(Upload_Texture_Region)(dst_texture, dst_level, dst_x, dst_y, src_data, src_pitch, region_width, region_height, format);
}

void Renderer::Apply_Render_State_Changes()
{
    return RENDERER_CALL(Apply_Render_State_Changes)();
}

void Renderer::Apply_Default_State()
{
    return RENDERER_CALL(Apply_Default_State)();
}

void Renderer::Invalidate_Cached_Render_States()
{
    return RENDERER_CALL(Invalidate_Cached_Render_States)();
}

void Renderer::Set_Blend_Op(BlendOp op)
{
    return RENDERER_CALL(Set_Blend_Op)(op);
}

void Renderer::Set_Blend_Factors(BlendFactor src, BlendFactor dest)
{
    return RENDERER_CALL(Set_Blend_Factors)(src, dest);
}

void Renderer::Set_Color_Write_Enable(bool red, bool green, bool blue, bool alpha)
{
    return RENDERER_CALL(Set_Color_Write_Enable)(red, green, blue, alpha);
}

void Renderer::Set_Alpha_Blend_Enable(bool enable)
{
    return RENDERER_CALL(Set_Alpha_Blend_Enable)(enable);
}

void Renderer::Set_Alpha_Test_Enable(bool enable)
{
    return RENDERER_CALL(Set_Alpha_Test_Enable)(enable);
}

void Renderer::Set_Alpha_Test_Reference(unsigned ref)
{
    return RENDERER_CALL(Set_Alpha_Test_Reference)(ref);
}

void Renderer::Set_Alpha_Test_Function(CompareFunc func)
{
    return RENDERER_CALL(Set_Alpha_Test_Function)(func);
}

void Renderer::Set_Alpha_Test(bool enable, unsigned ref, CompareFunc func)
{
    return RENDERER_CALL(Set_Alpha_Test)(enable, ref, func);
}

void Renderer::Set_Normalize_Normals(bool enable)
{
    return RENDERER_CALL(Set_Normalize_Normals)(enable);
}

void Renderer::Show_Hardware_Cursor(bool show)
{
    return RENDERER_CALL(Show_Hardware_Cursor)(show);
}

void Renderer::Set_Hardware_Cursor_Image(int hotspot_x, int hotspot_y, const RenderBackendImage & image)
{
    return RENDERER_CALL(Set_Hardware_Cursor_Image)(hotspot_x, hotspot_y, image);
}

void Renderer::Set_Hardware_Cursor_Position(int x, int y)
{
    return RENDERER_CALL(Set_Hardware_Cursor_Position)(x, y);
}

void Renderer::Set_Stencil_Enable(bool enable)
{
    return RENDERER_CALL(Set_Stencil_Enable)(enable);
}

void Renderer::Set_Stencil_Func(CompareFunc func)
{
    return RENDERER_CALL(Set_Stencil_Func)(func);
}

void Renderer::Set_Stencil_Ref(unsigned int ref)
{
    return RENDERER_CALL(Set_Stencil_Ref)(ref);
}

void Renderer::Set_Stencil_Mask(unsigned int mask)
{
    return RENDERER_CALL(Set_Stencil_Mask)(mask);
}

void Renderer::Set_Stencil_Write_Mask(unsigned int mask)
{
    return RENDERER_CALL(Set_Stencil_Write_Mask)(mask);
}

void Renderer::Set_Stencil_Pass_Op(StencilOp op)
{
    return RENDERER_CALL(Set_Stencil_Pass_Op)(op);
}

void Renderer::Set_Stencil_Fail_Op(StencilOp op)
{
    return RENDERER_CALL(Set_Stencil_Fail_Op)(op);
}

void Renderer::Set_Stencil_ZFail_Op(StencilOp op)
{
    return RENDERER_CALL(Set_Stencil_ZFail_Op)(op);
}

void Renderer::Set_Z_Bias(int bias)
{
    return RENDERER_CALL(Set_Z_Bias)(bias);
}

void Renderer::Set_Normal_Bias(float bias)
{
    return RENDERER_CALL(Set_Normal_Bias)(bias);
}

void Renderer::Set_Fill_Mode(FillMode mode)
{
    return RENDERER_CALL(Set_Fill_Mode)(mode);
}

void Renderer::Set_Shade_Mode(ShadeMode mode)
{
    return RENDERER_CALL(Set_Shade_Mode)(mode);
}

void Renderer::Set_Depth_Test_Enable(bool enable)
{
    return RENDERER_CALL(Set_Depth_Test_Enable)(enable);
}

void Renderer::Set_Depth_Write_Enable(bool enable)
{
    return RENDERER_CALL(Set_Depth_Write_Enable)(enable);
}

void Renderer::Set_Depth_Func(CompareFunc func)
{
    return RENDERER_CALL(Set_Depth_Func)(func);
}

bool Renderer::Supports_Color_Write_Mask()
{
    return RENDERER_CALL(Supports_Color_Write_Mask)();
}

unsigned Renderer::Get_Color_Write_Mask()
{
    return RENDERER_CALL(Get_Color_Write_Mask)();
}

void Renderer::Set_Color_Write_Mask(unsigned mask)
{
    return RENDERER_CALL(Set_Color_Write_Mask)(mask);
}

void Renderer::Set_Lighting_Enable(bool enable)
{
    return RENDERER_CALL(Set_Lighting_Enable)(enable);
}

void Renderer::Set_Point_Sprite_Enable(bool enable)
{
    return RENDERER_CALL(Set_Point_Sprite_Enable)(enable);
}

void Renderer::Set_Point_Scale_Enable(bool enable)
{
    return RENDERER_CALL(Set_Point_Scale_Enable)(enable);
}

void Renderer::Set_Point_Size(float size, float min_size, float max_size)
{
    return RENDERER_CALL(Set_Point_Size)(size, min_size, max_size);
}

void Renderer::Set_Point_Scale(float a, float b, float c)
{
    return RENDERER_CALL(Set_Point_Scale)(a, b, c);
}

void Renderer::Set_Texture_Factor(unsigned argb)
{
    return RENDERER_CALL(Set_Texture_Factor)(argb);
}

CullMode Renderer::Get_Cull_Mode()
{
    return RENDERER_CALL(Get_Cull_Mode)();
}

void Renderer::Set_Cull_Mode(CullMode mode)
{
    return RENDERER_CALL(Set_Cull_Mode)(mode);
}

void Renderer::Set_Tree_Shader_Constants(const float swayTable[11][4], const float shroudOffset[4], const float shroudScale[4])
{
    return RENDERER_CALL(Set_Tree_Shader_Constants)(swayTable, shroudOffset, shroudScale);
}

void Renderer::Set_Tree_Vertex_Shader_Active(bool active)
{
    return RENDERER_CALL(Set_Tree_Vertex_Shader_Active)(active);
}

void Renderer::Set_Grayscale_Mode(bool enable)
{
    return RENDERER_CALL(Set_Grayscale_Mode)(enable);
}

void Renderer::Configure_Grayscale_Texture_Stages()
{
    return RENDERER_CALL(Configure_Grayscale_Texture_Stages)();
}

void Renderer::Configure_Custom_Edging_Cloud_Texture_Stages()
{
    return RENDERER_CALL(Configure_Custom_Edging_Cloud_Texture_Stages)();
}

void Renderer::Configure_Shadow_Volume_Fill_Texture_Stages()
{
    return RENDERER_CALL(Configure_Shadow_Volume_Fill_Texture_Stages)();
}

void Renderer::Set_Cloud_Shadow_Params(bool enable, float scroll_x, float scroll_y, float stretch, TextureClass * cloud_tex)
{
    return RENDERER_CALL(Set_Cloud_Shadow_Params)(enable, scroll_x, scroll_y, stretch, cloud_tex);
}

void Renderer::Set_Light_Map_Params(bool enable, float stretch, TextureClass * noise_tex)
{
    return RENDERER_CALL(Set_Light_Map_Params)(enable, stretch, noise_tex);
}

void Renderer::Set_Transform(TransformKind transform, const Matrix4x4 & m)
{
    return RENDERER_CALL(Set_Transform)(transform, m);
}

void Renderer::Set_Transform(TransformKind transform, const Matrix3D & m)
{
    return RENDERER_CALL(Set_Transform)(transform, m);
}

void Renderer::Get_Transform(TransformKind transform, Matrix4x4 & m)
{
    return RENDERER_CALL(Get_Transform)(transform, m);
}

void Renderer::Set_World_Identity()
{
    return RENDERER_CALL(Set_World_Identity)();
}

void Renderer::Set_View_Identity()
{
    return RENDERER_CALL(Set_View_Identity)();
}

bool Renderer::Is_World_Identity()
{
    return RENDERER_CALL(Is_World_Identity)();
}

bool Renderer::Is_View_Identity()
{
    return RENDERER_CALL(Is_View_Identity)();
}

void Renderer::Set_Projection_Transform_With_Z_Bias(const Matrix4x4 & matrix, float znear, float zfar)
{
    return RENDERER_CALL(Set_Projection_Transform_With_Z_Bias)(matrix, znear, zfar);
}

void Renderer::Set_Light(unsigned int index, const LightClass & light)
{
    return RENDERER_CALL(Set_Light)(index, light);
}

void Renderer::Clear_Light(unsigned int index)
{
    return RENDERER_CALL(Clear_Light)(index);
}

void Renderer::Set_Ambient(const Vector3 & color)
{
    return RENDERER_CALL(Set_Ambient)(color);
}

const Vector3 & Renderer::Get_Ambient()
{
    return RENDERER_CALL(Get_Ambient)();
}

void Renderer::Set_Fog(bool enable, const Vector3 & color, float start, float end)
{
    return RENDERER_CALL(Set_Fog)(enable, color, start, end);
}

void Renderer::Set_Fog_Enable(bool enable)
{
    return RENDERER_CALL(Set_Fog_Enable)(enable);
}

void Renderer::Set_Fog_Color(unsigned argb)
{
    return RENDERER_CALL(Set_Fog_Color)(argb);
}

unsigned Renderer::Get_Fog_Color()
{
    return RENDERER_CALL(Get_Fog_Color)();
}

bool Renderer::Get_Fog_Enable()
{
    return RENDERER_CALL(Get_Fog_Enable)();
}

void Renderer::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    return RENDERER_CALL(Set_Light_Environment)(light_env);
}

LightEnvironmentClass * Renderer::Get_Light_Environment()
{
    return RENDERER_CALL(Get_Light_Environment)();
}

void Renderer::Set_Specular_Enable(bool enable)
{
    return RENDERER_CALL(Set_Specular_Enable)(enable);
}

void Renderer::Set_Patch_Segments(float level)
{
    return RENDERER_CALL(Set_Patch_Segments)(level);
}

void Renderer::Override_Blend(BlendFactor srcBlend, BlendFactor dstBlend)
{
    return RENDERER_CALL(Override_Blend)(srcBlend, dstBlend);
}

void Renderer::Override_Alpha_Test(bool enable, unsigned ref, CompareFunc func)
{
    return RENDERER_CALL(Override_Alpha_Test)(enable, ref, func);
}

void Renderer::Override_Alpha_Blend_Enable(bool enable)
{
    return RENDERER_CALL(Override_Alpha_Blend_Enable)(enable);
}

void Renderer::Override_Texcoord_Index(unsigned stage, unsigned uvIndex)
{
    return RENDERER_CALL(Override_Texcoord_Index)(stage, uvIndex);
}

void Renderer::Override_Terrain_Blend(bool enable)
{
    return RENDERER_CALL(Override_Terrain_Blend)(enable);
}

void Renderer::Override_Material_Opacity(float opacity)
{
    return RENDERER_CALL(Override_Material_Opacity)(opacity);
}

void Renderer::Set_Texture_Transform(unsigned stage, const Matrix4x4& matrix)
{
    return RENDERER_CALL(Set_Texture_Transform)(stage, matrix);
}

void Renderer::Clear_Texture_Transform(unsigned stage)
{
    return RENDERER_CALL(Clear_Texture_Transform)(stage);
}

void Renderer::Set_Texture_Coord_Source(unsigned stage, RenderBackendTexcoordSource source, unsigned uv_array_index)
{
    return RENDERER_CALL(Set_Texture_Coord_Source)(stage, source, uv_array_index);
}

void Renderer::Set_Texture_Transform_Mode(unsigned stage, unsigned coord_count, bool projected)
{
    return RENDERER_CALL(Set_Texture_Transform_Mode)(stage, coord_count, projected);
}

void Renderer::Set_Texture_Bump_Env_Matrix(unsigned stage, float m00, float m01, float m10, float m11)
{
    return RENDERER_CALL(Set_Texture_Bump_Env_Matrix)(stage, m00, m01, m10, m11);
}

void Renderer::Set_Texture_Bump_Env_Luminance(unsigned stage, float scale, float offset)
{
    return RENDERER_CALL(Set_Texture_Bump_Env_Luminance)(stage, scale, offset);
}

void Renderer::Set_Texture_Color_Operation(unsigned stage, RenderBackendTextureOperation op)
{
    return RENDERER_CALL(Set_Texture_Color_Operation)(stage, op);
}

void Renderer::Set_Texture_Alpha_Operation(unsigned stage, RenderBackendTextureOperation op)
{
    return RENDERER_CALL(Set_Texture_Alpha_Operation)(stage, op);
}

void Renderer::Set_Texture_Color_Argument(unsigned stage, unsigned argument_index, RenderBackendTextureArgument arg)
{
    return RENDERER_CALL(Set_Texture_Color_Argument)(stage, argument_index, arg);
}

void Renderer::Set_Texture_Alpha_Argument(unsigned stage, unsigned argument_index, RenderBackendTextureArgument arg)
{
    return RENDERER_CALL(Set_Texture_Alpha_Argument)(stage, argument_index, arg);
}

void Renderer::Set_Texture_Coord_Generation(unsigned stage, bool cameraPosEnabled)
{
    return RENDERER_CALL(Set_Texture_Coord_Generation)(stage, cameraPosEnabled);
}

void Renderer::Set_Texture_UV_Wrap(unsigned stage, bool enable)
{
    return RENDERER_CALL(Set_Texture_UV_Wrap)(stage, enable);
}

void Renderer::Set_Texture_Address_Mode(unsigned stage, RenderBackendTextureAddressMode u, RenderBackendTextureAddressMode v, RenderBackendTextureAddressMode w)
{
    return RENDERER_CALL(Set_Texture_Address_Mode)(stage, u, v, w);
}

void Renderer::Set_Texture_Sample_Filter(unsigned stage, RenderBackendTextureSampleFilter min_filter, RenderBackendTextureSampleFilter mag_filter, RenderBackendTextureSampleFilter mip_filter)
{
    return RENDERER_CALL(Set_Texture_Sample_Filter)(stage, min_filter, mag_filter, mip_filter);
}

void Renderer::Set_Texture_Min_Mag_Filter(unsigned stage, RenderBackendTextureSampleFilter min_filter, RenderBackendTextureSampleFilter mag_filter)
{
    return RENDERER_CALL(Set_Texture_Min_Mag_Filter)(stage, min_filter, mag_filter);
}

void Renderer::Set_Texture_Mip_Filter(unsigned stage, RenderBackendTextureSampleFilter mip_filter)
{
    return RENDERER_CALL(Set_Texture_Mip_Filter)(stage, mip_filter);
}

void Renderer::Set_Texture_Max_Anisotropy(unsigned stage, unsigned max_anisotropy)
{
    return RENDERER_CALL(Set_Texture_Max_Anisotropy)(stage, max_anisotropy);
}

void Renderer::Set_Texture_Clamp_Mode(unsigned stage, bool clampU, bool clampV)
{
    return RENDERER_CALL(Set_Texture_Clamp_Mode)(stage, clampU, clampV);
}

void Renderer::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
    return RENDERER_CALL(Set_Texture_Stage_State)(stage, state, value);
}

void Renderer::Set_Shroud_Texture_Pass_Active(bool active, unsigned stage)
{
    return RENDERER_CALL(Set_Shroud_Texture_Pass_Active)(active, stage);
}

void Renderer::Set_Object_Shroud_Texture_Pass_Active(bool active)
{
    return RENDERER_CALL(Set_Object_Shroud_Texture_Pass_Active)(active);
}

void Renderer::Set_Object_Shroud_Alpha_Mask_Texture(TextureBaseClass * texture)
{
    return RENDERER_CALL(Set_Object_Shroud_Alpha_Mask_Texture)(texture);
}

void Renderer::Set_Shroud_Texture_Params(float offset_x, float offset_y, float scale_x, float scale_y)
{
    return RENDERER_CALL(Set_Shroud_Texture_Params)(offset_x, offset_y, scale_x, scale_y);
}

void Renderer::Create_Effect_Resources(bool allow_pixel_shaders)
{
    return RENDERER_CALL(Create_Effect_Resources)(allow_pixel_shaders);
}

void Renderer::Release_Effect_Resources()
{
    return RENDERER_CALL(Release_Effect_Resources)();
}

void Renderer::Draw_Road(const RenderBackendRoadDraw & road, unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count)
{
    return RENDERER_CALL(Draw_Road)(road, start_index, polygon_count, min_vertex_index, vertex_count);
}

void Renderer::Draw_Terrain(const RenderBackendTerrainDraw & terrain, VertexBufferClass * const * vertex_buffers, unsigned int vertex_buffer_count, unsigned short polygon_count, unsigned short vertex_count)
{
    return RENDERER_CALL(Draw_Terrain)(terrain, vertex_buffers, vertex_buffer_count, polygon_count, vertex_count);
}

void Renderer::Begin_Shroud_Overlay(const RenderBackendShroud & shroud)
{
    return RENDERER_CALL(Begin_Shroud_Overlay)(shroud);
}

void Renderer::End_Shroud_Overlay()
{
    return RENDERER_CALL(End_Shroud_Overlay)();
}

void Renderer::Draw_Water(const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count)
{
    return RENDERER_CALL(Draw_Water)(water, start_index, polygon_count, min_vertex_index, vertex_count);
}

void Renderer::Set_World_Texture_Projection(unsigned stage, const RenderBackendWorldProjection & projection)
{
    return RENDERER_CALL(Set_World_Texture_Projection)(stage, projection);
}

void Renderer::Clear_Texture_Projection(unsigned stage)
{
    return RENDERER_CALL(Clear_Texture_Projection)(stage);
}

bool Renderer::Requires_Delayed_Object_Shroud_Pass()
{
    return RENDERER_CALL(Requires_Delayed_Object_Shroud_Pass)();
}

void Renderer::Begin_Water_Overlay()
{
    return RENDERER_CALL(Begin_Water_Overlay)();
}

void Renderer::End_Water_Overlay()
{
    return RENDERER_CALL(End_Water_Overlay)();
}

void Renderer::Begin_Effect_Overlay()
{
    return RENDERER_CALL(Begin_Effect_Overlay)();
}

void Renderer::End_Effect_Overlay()
{
    return RENDERER_CALL(End_Effect_Overlay)();
}

bool Renderer::Begin_Smudge_Distortion(float tactical_width_fraction, float tactical_height_fraction)
{
    return RENDERER_CALL(Begin_Smudge_Distortion)(tactical_width_fraction, tactical_height_fraction);
}

void Renderer::End_Smudge_Distortion()
{
    return RENDERER_CALL(End_Smudge_Distortion)();
}

void Renderer::Clear_State_Overrides()
{
    return RENDERER_CALL(Clear_State_Overrides)();
}

void Renderer::Draw_Triangles(unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count)
{
    return RENDERER_CALL(Draw_Triangles)(start_index, polygon_count, min_vertex_index, vertex_count);
}

void Renderer::Draw_Triangles(unsigned int buffer_type, unsigned short start_index, unsigned short polygon_count, unsigned short min_vertex_index, unsigned short vertex_count)
{
    return RENDERER_CALL(Draw_Triangles)(buffer_type, start_index, polygon_count, min_vertex_index, vertex_count);
}

bool Renderer::Is_Triangle_Draw_Enabled()
{
    return RENDERER_CALL(Is_Triangle_Draw_Enabled)();
}

void Renderer::Set_Triangle_Draw_Enabled(bool enable)
{
    return RENDERER_CALL(Set_Triangle_Draw_Enabled)(enable);
}

void Renderer::Draw_Screen_Color_Quad(unsigned color, int x, int y, int width, int height)
{
    return RENDERER_CALL(Draw_Screen_Color_Quad)(color, x, y, width, height);
}

void Renderer::Draw_Screen_Multiply_Quad(unsigned color, int x, int y, int width, int height)
{
    return RENDERER_CALL(Draw_Screen_Multiply_Quad)(color, x, y, width, height);
}

void Renderer::Draw_Strip(unsigned short start_index, unsigned short index_count, unsigned short min_vertex_index, unsigned short vertex_count)
{
    return RENDERER_CALL(Draw_Strip)(start_index, index_count, min_vertex_index, vertex_count);
}

void Renderer::Skip_Next_Bgfx_Submit()
{
    return RENDERER_CALL(Skip_Next_Bgfx_Submit)();
}

void Renderer::Set_Projected_Shadow_Decal_Active(bool active)
{
    return RENDERER_CALL(Set_Projected_Shadow_Decal_Active)(active);
}

void Renderer::Set_Projected_Decal_Mode(RenderBackendProjectedDecalMode mode)
{
    return RENDERER_CALL(Set_Projected_Decal_Mode)(mode);
}

void Renderer::Set_Shadow_Volume_Shader_Active(bool active)
{
    return RENDERER_CALL(Set_Shadow_Volume_Shader_Active)(active);
}

void Renderer::Apply_Stencil_Shadow_Darken(unsigned shadow_color, unsigned stencil_read_mask, unsigned stencil_ref, int x, int y, int width, int height)
{
    return RENDERER_CALL(Apply_Stencil_Shadow_Darken)(shadow_color, stencil_read_mask, stencil_ref, x, y, width, height);
}

void Renderer::Submit_Shadow_Volume_Caps(unsigned strip_start_vertex, unsigned num_silhouette_verts)
{
    return RENDERER_CALL(Submit_Shadow_Volume_Caps)(strip_start_vertex, num_silhouette_verts);
}

void Renderer::Submit_Shadow_Volume_Triangulated_Caps(unsigned strip_start_vertex, const short * local_cap_indices, unsigned cap_index_count)
{
    return RENDERER_CALL(Submit_Shadow_Volume_Triangulated_Caps)(strip_start_vertex, local_cap_indices, cap_index_count);
}

bool Renderer::Needs_Closed_Shadow_Volumes()
{
    return RENDERER_CALL(Needs_Closed_Shadow_Volumes)();
}

void Renderer::Capture_Shroud_Texture(TextureClass * dst_texture, const void * pixel_data, unsigned dst_width, unsigned dst_height, unsigned src_width, unsigned src_height, unsigned src_x, unsigned src_y, unsigned dst_x, unsigned dst_y, unsigned pitch, WW3DFormat format, unsigned border_pixel)
{
    return RENDERER_CALL(Capture_Shroud_Texture)(dst_texture, pixel_data, dst_width, dst_height, src_width, src_height, src_x, src_y, dst_x, dst_y, pitch, format, border_pixel);
}

const unsigned int * Renderer::Get_Legacy_Vertex_Shader_Declaration(RenderBackendLegacyVertexDeclaration declaration)
{
    return RENDERER_CALL(Get_Legacy_Vertex_Shader_Declaration)(declaration);
}

bool Renderer::Load_Legacy_Shader(const char * path, const unsigned int * declaration, unsigned int usage, RenderBackendShaderKind kind, unsigned long * handle)
{
    return RENDERER_CALL(Load_Legacy_Shader)(path, declaration, usage, kind, handle);
}

bool Renderer::Create_Vertex_Shader(const unsigned int * declaration, const unsigned int * shader, unsigned int usage, unsigned long * handle)
{
    return RENDERER_CALL(Create_Vertex_Shader)(declaration, shader, usage, handle);
}

bool Renderer::Create_Pixel_Shader(const unsigned int * shader, unsigned long * handle)
{
    return RENDERER_CALL(Create_Pixel_Shader)(shader, handle);
}

bool Renderer::Create_Legacy_Pixel_Shader(RenderBackendLegacyPixelShaderMode mode, unsigned long * handle)
{
    return RENDERER_CALL(Create_Legacy_Pixel_Shader)(mode, handle);
}

void Renderer::Delete_Vertex_Shader(unsigned long vertex_shader)
{
    return RENDERER_CALL(Delete_Vertex_Shader)(vertex_shader);
}

void Renderer::Delete_Pixel_Shader(unsigned long pixel_shader)
{
    return RENDERER_CALL(Delete_Pixel_Shader)(pixel_shader);
}

void Renderer::Set_Vertex_Shader(unsigned long vertex_shader)
{
    return RENDERER_CALL(Set_Vertex_Shader)(vertex_shader);
}

void Renderer::Set_Pixel_Shader(unsigned long pixel_shader)
{
    return RENDERER_CALL(Set_Pixel_Shader)(pixel_shader);
}

void Renderer::Set_Vertex_Shader_Constant(int reg, const void * data, int count)
{
    return RENDERER_CALL(Set_Vertex_Shader_Constant)(reg, data, count);
}

void Renderer::Set_Pixel_Shader_Constant(int reg, const void * data, int count)
{
    return RENDERER_CALL(Set_Pixel_Shader_Constant)(reg, data, count);
}

TextureClass * Renderer::Create_Render_Target(int width, int height, WW3DFormat format)
{
    return RENDERER_CALL(Create_Render_Target)(width, height, format);
}

void Renderer::Set_Render_Target_With_Z(TextureClass * texture, ZTextureClass * ztexture)
{
    return RENDERER_CALL(Set_Render_Target_With_Z)(texture, ztexture);
}

bool Renderer::Is_Render_To_Texture()
{
    return RENDERER_CALL(Is_Render_To_Texture)();
}

void Renderer::Set_Shadow_Map(int idx, ZTextureClass * ztex)
{
    return RENDERER_CALL(Set_Shadow_Map)(idx, ztex);
}

ZTextureClass * Renderer::Get_Shadow_Map(int idx)
{
    return RENDERER_CALL(Get_Shadow_Map)(idx);
}

bool Renderer::Requires_Legacy_Buffer_Resources()
{
    return RENDERER_CALL(Requires_Legacy_Buffer_Resources)();
}

RenderResource Renderer::Create_Texture(const TextureDesc & desc)
{
    return RENDERER_CALL(Create_Texture)(desc);
}

RenderResource Renderer::Create_Vertex_Buffer(const BufferDesc & desc, const void * initial_data)
{
    return RENDERER_CALL(Create_Vertex_Buffer)(desc, initial_data);
}

RenderResource Renderer::Create_Index_Buffer(const BufferDesc & desc, const void * initial_data, bool indices_are_32bit)
{
    return RENDERER_CALL(Create_Index_Buffer)(desc, initial_data, indices_are_32bit);
}

void Renderer::Destroy_Resource(RenderResource h)
{
    return RENDERER_CALL(Destroy_Resource)(h);
}

RenderResource Renderer::Register_Texture_Resource(TextureBaseClass * tex)
{
    return RENDERER_CALL(Register_Texture_Resource)(tex);
}

RenderResource Renderer::Register_Vertex_Buffer_Resource(VertexBufferClass * vb)
{
    return RENDERER_CALL(Register_Vertex_Buffer_Resource)(vb);
}

RenderResource Renderer::Register_Index_Buffer_Resource(IndexBufferClass * ib)
{
    return RENDERER_CALL(Register_Index_Buffer_Resource)(ib);
}
