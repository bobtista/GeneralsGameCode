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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8wrapper.h                           $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 2:40p                                              $*
 *                                                                                             *
 *                    $Revision:: 92                                                          $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Render to shadow buffer texture support														*
 * 08/05/02 KM Texture class redesign
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#include "dllist.h"
#if defined(GGC_RENDER_BACKEND_BGFX)
#include "dx8standalonetypes.h"
#else
#include "d3d8.h"
#endif
#include "matrix4.h"
#include "statistics.h"
#include "wwstring.h"
#include "lightenvironment.h"
#include "shader.h"
#include "vector4.h"
#include "cpudetect.h"
#include "dx8caps.h"
#include "dx8deviceinterop.h"
#include "RenderBufferTypes.h"
#include "RenderDeviceCleanupHook.h"

#include "texture.h"
#include "vertexbuffer.h"
#include "indexbuffer.h"
#include "vertmaterial.h"
#include "RenderStateCache.h"
#include "FixedFunctionState.h"

// TheSuperHackers @refactor bobtista 10/04/2026 Flag DX8Wrapper methods that have an IRenderBackend equivalent so the
// compiler lists every remaining call site as a warning. The WW3D2 library
// itself (g_ww3d2 STATIC) defines GGC_ALLOW_DX8WRAPPER to suppress the
// warning inside DX8Backend.cpp / dx8wrapper.cpp and the rest of WW3D2,
// which legitimately call DX8Wrapper directly. VC6 tools get a no-op.
#if defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(GGC_ALLOW_DX8WRAPPER)
#  define GGC_RB_DEPRECATED __declspec(deprecated("migrate to g_renderBackend equivalent"))
#else
#  define GGC_RB_DEPRECATED
#endif

/*
** Registry value names
*/
#define	VALUE_NAME_RENDER_DEVICE_NAME					"RenderDeviceName"
#define	VALUE_NAME_RENDER_DEVICE_WIDTH				"RenderDeviceWidth"
#define	VALUE_NAME_RENDER_DEVICE_HEIGHT				"RenderDeviceHeight"
#define	VALUE_NAME_RENDER_DEVICE_DEPTH				"RenderDeviceDepth"
#define	VALUE_NAME_RENDER_DEVICE_WINDOWED			"RenderDeviceWindowed"
#define	VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH		"RenderDeviceTextureDepth"

const unsigned MAX_VERTEX_SHADER_CONSTANTS=96;
const unsigned MAX_PIXEL_SHADER_CONSTANTS=8;
const unsigned MAX_SHADOW_MAPS=1;

class VertexMaterialClass;
class CameraClass;
class LightEnvironmentClass;
class RenderDeviceDescClass;
class VertexBufferClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class DynamicIBAccessClass;
class DX8VertexBufferClass;
class DX8IndexBufferClass;
class TextureClass;
class LightClass;
class SurfaceClass;

struct DX8FrameStatistics
{
	DX8FrameStatistics() :
		matrix_changes(0),
		material_changes(0),
		vertex_buffer_changes(0),
		index_buffer_changes(0),
		light_changes(0),
		texture_changes(0),
		render_state_changes(0),
		texture_stage_state_changes(0),
		dx8_calls(0),
		draw_calls(0)
	{
	}

	unsigned matrix_changes;
	unsigned material_changes;
	unsigned vertex_buffer_changes;
	unsigned index_buffer_changes;
	unsigned light_changes;
	unsigned texture_changes;
	unsigned render_state_changes;
	unsigned texture_stage_state_changes;
	unsigned dx8_calls;
	unsigned draw_calls;
};

#define DX8_RECORD_MATRIX_CHANGE()				FrameStatistics.matrix_changes++
#define DX8_RECORD_MATERIAL_CHANGE()			FrameStatistics.material_changes++
#define DX8_RECORD_VERTEX_BUFFER_CHANGE()		FrameStatistics.vertex_buffer_changes++
#define DX8_RECORD_INDEX_BUFFER_CHANGE()		FrameStatistics.index_buffer_changes++
#define DX8_RECORD_LIGHT_CHANGE()				FrameStatistics.light_changes++
#define DX8_RECORD_TEXTURE_CHANGE()				FrameStatistics.texture_changes++
#define DX8_RECORD_RENDER_STATE_CHANGE()		FrameStatistics.render_state_changes++
#define DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE() FrameStatistics.texture_stage_state_changes++
#define DX8_RECORD_DX8_CALLS()					FrameStatistics.dx8_calls++
#define DX8_RECORD_DRAW_CALLS()					FrameStatistics.draw_calls++

extern bool _DX8SingleThreaded;

WWINLINE void DX8_ErrorCode(unsigned res)
{
	if (res==S_OK) return;
	Log_DX8_ErrorCode(res);
}

#ifdef WWDEBUG
#define DX8_THREAD_ASSERT() if (_DX8SingleThreaded) { WWASSERT_PRINT(DX8Wrapper::_Get_Main_Thread_ID()==ThreadClass::_Get_Current_Thread_ID(),"DX8Wrapper::DX8 calls must be called from the main thread!"); }
#else
#define DX8_THREAD_ASSERT() ;
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
#ifdef WWDEBUG
#define DX8CALL_HRES(x,res) DX8_Assert(); res = DX8_Call_Device()->x; DX8_ErrorCode(res); DX8Wrapper::Increment_DX8_CallCount();
#define DX8CALL(x) DX8_Assert(); DX8_ErrorCode(DX8_Call_Device()->x); DX8Wrapper::Increment_DX8_CallCount();
#define DX8CALL_D3D(x) DX8_Assert(); DX8_ErrorCode(DX8_Call_Interface()->x); DX8Wrapper::Increment_DX8_CallCount();
#else
#define DX8CALL_HRES(x,res) res = DX8_Call_Device()->x; DX8Wrapper::Increment_DX8_CallCount();
#define DX8CALL(x) DX8_Call_Device()->x; DX8Wrapper::Increment_DX8_CallCount();
#define DX8CALL_D3D(x) DX8_Call_Interface()->x; DX8Wrapper::Increment_DX8_CallCount();
#endif
#endif


#define no_EXTENDED_STATS
// EXTENDED_STATS collects additional timing statistics by turning off parts
// of the 3D drawing system (terrain, objects, etc.)
#ifdef EXTENDED_STATS
#include "renderdebugstats.h"
#endif


/**
** DX8Wrapper
**
** DX8 interface wrapper class.  This encapsulates the DX8 interface; adding redundant state
** detection, stat tracking, etc etc.  In general, we will wrap all DX8 calls with at least
** an WWINLINE function so that we can add stat tracking, etc if needed.  Direct access to the
** legacy device will require "friend" status and should be granted only in extreme circumstances :-)
*/
class DX8Wrapper
{
	enum ChangedStates {
		WORLD_CHANGED = FixedFunctionState::WORLD_CHANGED,
		VIEW_CHANGED = FixedFunctionState::VIEW_CHANGED,
		LIGHT0_CHANGED = FixedFunctionState::LIGHT0_CHANGED,
		LIGHT1_CHANGED = FixedFunctionState::LIGHT1_CHANGED,
		LIGHT2_CHANGED = FixedFunctionState::LIGHT2_CHANGED,
		LIGHT3_CHANGED = FixedFunctionState::LIGHT3_CHANGED,
		TEXTURE0_CHANGED = FixedFunctionState::TEXTURE0_CHANGED,
		TEXTURE1_CHANGED = FixedFunctionState::TEXTURE1_CHANGED,
		TEXTURE2_CHANGED = FixedFunctionState::TEXTURE2_CHANGED,
		TEXTURE3_CHANGED = FixedFunctionState::TEXTURE3_CHANGED,
		MATERIAL_CHANGED = FixedFunctionState::MATERIAL_CHANGED,
		SHADER_CHANGED = FixedFunctionState::SHADER_CHANGED,
		VERTEX_BUFFER_CHANGED = FixedFunctionState::VERTEX_BUFFER_CHANGED,
		INDEX_BUFFER_CHANGED = FixedFunctionState::INDEX_BUFFER_CHANGED,
		WORLD_IDENTITY = FixedFunctionState::WORLD_IDENTITY,
		VIEW_IDENTITY = FixedFunctionState::VIEW_IDENTITY,

		TEXTURES_CHANGED = FixedFunctionState::TEXTURES_CHANGED,
		LIGHTS_CHANGED = FixedFunctionState::LIGHTS_CHANGED,
	};

	static void Draw_Sorting_IB_VB(
		unsigned primitive_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	static void Draw(
		unsigned primitive_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index=0,
		unsigned short vertex_count=0);

public:
#ifdef EXTENDED_STATS
	static RenderDebugStats &stats;
#endif

	static bool Init(void * hwnd, bool lite = false);
	static void Shutdown();

	static void SetCleanupHook(RenderDeviceCleanupHook *pCleanupHook) {m_pCleanupHook = pCleanupHook;};
	/*
	** Some WW3D sub-systems need to be initialized after the device is created and shutdown
	** before the device is released.
	*/
	static void	Do_Onetime_Device_Dependent_Inits();
	static void Do_Onetime_Device_Dependent_Shutdowns();

	GGC_RB_DEPRECATED static bool Is_Device_Lost() { return IsDeviceLost; }
	static bool Is_Initted() { return IsInitted; }

	GGC_RB_DEPRECATED static bool Has_Stencil ();
	static void Get_Format_Name(unsigned int format, StringClass *tex_format);

	/*
	** Rendering
	*/
	GGC_RB_DEPRECATED static void Begin_Scene();
	GGC_RB_DEPRECATED static void End_Scene(bool flip_frame = true);

	// Flip until the primary buffer is visible.
	GGC_RB_DEPRECATED static void Flip_To_Primary();

	GGC_RB_DEPRECATED static void Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha=0.0f, float z=1.0f, unsigned int stencil=0);

#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void	Set_Viewport(CONST D3DVIEWPORT8* pViewport);
#endif

	GGC_RB_DEPRECATED static void Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream=0);
	GGC_RB_DEPRECATED static void Set_Vertex_Buffer(const DynamicVBAccessClass& vba);
	GGC_RB_DEPRECATED static void Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset);
	GGC_RB_DEPRECATED static void Set_Index_Buffer(const DynamicIBAccessClass& iba,unsigned short index_base_offset);
	GGC_RB_DEPRECATED static void Set_Index_Buffer_Index_Offset(unsigned offset);

	static void Get_Render_State(RenderStateStruct& state);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_Render_State(const RenderStateStruct& state);
#endif
	static void Release_Render_State();
	// TheSuperHackers @perf bobtista 28/04/2026 Const-ref peek avoids the
	// RenderStateStruct copy assignment, which does REF_PTR_SET on material,
	// MAX_VERTEX_STREAMS vertex buffers, the index buffer, and every entry
	// of Textures[MAX_TEXTURE_STAGES]. Read-only callers (e.g. per-draw
	// light/texture sync in BgfxBackend) should use this instead.
	static const RenderStateStruct & Peek_Render_State() { return FixedFunctionState::Peek_Render_State(); }

#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_DX8_Material(const D3DMATERIAL8* mat);
#endif

	GGC_RB_DEPRECATED static void Set_Gamma(float gamma,float bright,float contrast,bool calibrate=true,bool uselimit=true);

	// Set_ and Get_Transform() functions take the matrix in Westwood convention format.

	GGC_RB_DEPRECATED static void Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix,float znear, float zfar);	// pointer to 16 matrices

#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix4x4& m);
	GGC_RB_DEPRECATED static void Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix3D& m);
	GGC_RB_DEPRECATED static void Get_Transform(D3DTRANSFORMSTATETYPE transform, Matrix4x4& m);
#endif
	GGC_RB_DEPRECATED static void Set_World_Identity();
	GGC_RB_DEPRECATED static void Set_View_Identity();
	GGC_RB_DEPRECATED static bool Is_World_Identity();
	GGC_RB_DEPRECATED static bool Is_View_Identity();

	// Note that *_DX8_Transform() functions take the matrix in DX8 format - transposed from Westwood convention.

	static void Commit_Fixed_Function_Transform(unsigned transform, const LegacyTransformMatrix& m);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void _Set_DX8_Transform(D3DTRANSFORMSTATETYPE transform, const D3DMATRIX& m);
#endif
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void _Get_DX8_Transform(D3DTRANSFORMSTATETYPE transform, D3DMATRIX& m);
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_DX8_Light(int index,D3DLIGHT8* light);
#endif
	static void Commit_Fixed_Function_Render_Value(unsigned state, unsigned value);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_DX8_Render_State(D3DRENDERSTATETYPE state, unsigned value);
#endif
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_DX8_Clip_Plane(DWORD Index, CONST float* pPlane);
#endif
	static void Commit_Fixed_Function_Texture_Stage_Value(unsigned stage, unsigned state, unsigned value);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_DX8_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state, unsigned value);
	static void Set_DX8_Texture(unsigned int stage, IDirect3DBaseTexture8* texture);
#endif
#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Set_Light_Environment(LightEnvironmentClass* light_env);
#endif
	GGC_RB_DEPRECATED static LightEnvironmentClass* Get_Light_Environment() { return Light_Environment; }
#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Set_Fog(bool enable, const Vector3 &color, float start, float end);
#endif

	// Deferred

#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Set_Shader(const ShaderClass& shader);
#endif
	GGC_RB_DEPRECATED static void Get_Shader(ShaderClass& shader);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Set_Texture(unsigned stage,TextureBaseClass* texture);
	GGC_RB_DEPRECATED static void Set_Material(const VertexMaterialClass* material);
	static void Set_Light(unsigned index,const D3DLIGHT8* light);
	GGC_RB_DEPRECATED static void Set_Light(unsigned index,const LightClass &light);
#endif
	static void Commit_Fixed_Function_Texture(unsigned stage,TextureBaseClass* texture);
	static void Commit_Deferred_Render_State_Changes();

#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Apply_Render_State_Changes();	// Apply deferred render state changes (will be called automatically by Draw...)
#endif

	GGC_RB_DEPRECATED static void Draw_Triangles(
		unsigned buffer_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	GGC_RB_DEPRECATED static void Draw_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	GGC_RB_DEPRECATED static void Draw_Strip(
		unsigned short start_index,
		unsigned short index_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	/*
	** Resources
	*/

#if !defined(GGC_RENDER_BACKEND_BGFX)
	static IDirect3DVolumeTexture8* _Create_DX8_Volume_Texture
	(
		unsigned int width,
		unsigned int height,
		unsigned int depth,
		WW3DFormat format,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED
	);

	static IDirect3DCubeTexture8* _Create_DX8_Cube_Texture
	(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED,
		bool rendertarget=false
	);


	static IDirect3DTexture8* _Create_DX8_ZTexture
	(
		unsigned int width,
		unsigned int height,
		WW3DZFormat zformat,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED
	);


	static IDirect3DTexture8 * _Create_DX8_Texture
	(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		D3DPOOL pool=D3DPOOL_MANAGED,
		bool rendertarget=false
	);
	static IDirect3DTexture8 * _Create_DX8_Texture(const char *filename, MipCountType mip_level_count);
	static IDirect3DTexture8 * _Create_DX8_Texture(IDirect3DSurface8 *surface, MipCountType mip_level_count);

	static IDirect3DSurface8 * _Create_DX8_Surface(unsigned int width, unsigned int height, WW3DFormat format);
	static IDirect3DSurface8 * _Create_DX8_Surface(const char *filename);
	static IDirect3DSurface8 * _Get_DX8_Front_Buffer();
	static SurfaceClass * _Get_DX8_Back_Buffer(unsigned int num=0);

	static void _Copy_DX8_Rects(
			IDirect3DSurface8* pSourceSurface,
			CONST RECT* pSourceRectsArray,
			UINT cRects,
			IDirect3DSurface8* pDestinationSurface,
			CONST POINT* pDestPointsArray
	);

	static void _Update_Texture(TextureClass *system, TextureClass *video);
	static void Flush_DX8_Resource_Manager(unsigned int bytes=0);
	static unsigned int Get_Free_Texture_RAM();
#endif

	static unsigned _Get_Main_Thread_ID() { return _MainThreadID; }

	/*
	** Statistics
	*/
	static void Begin_Statistics();
	static void End_Statistics();
	static const DX8FrameStatistics& Get_Last_Frame_Statistics();
	static unsigned long Get_FrameCount();
	static void Increment_DX8_CallCount() { DX8_RECORD_DX8_CALLS(); }

	// Needed by shader class
	static bool						Get_Fog_Enable() { return FogEnable; }
	static unsigned				Get_Fog_Color() { return FogColor; }

	// Utilities
	static Vector4 Convert_Color(unsigned color);
	static unsigned int Convert_Color(const Vector4& color);
	static unsigned int Convert_Color(const Vector3& color, const float alpha);
	static void Clamp_Color(Vector4& color);
	static unsigned int Convert_Color_Clamp(const Vector4& color);

	static void			  Set_Alpha (const float alpha, unsigned int &color);

	static void _Enable_Triangle_Draw(bool enable) { _EnableTriangleDraw=enable; }
	static bool _Is_Triangle_Draw_Enabled() { return _EnableTriangleDraw; }

	/*
	** Additional swap chain interface
	**
	**		Use this interface to render to multiple windows (in windowed mode).
	**	To render to an additional window, the sequence of calls should look
	**	something like this:
	**
	**	Set the additional swap chain as the active render target.
	**
	**	WW3D::Begin_Render (true, true, Vector3 (0, 0, 0));
	**	WW3D::Render (scene, camera, FALSE, FALSE);
	**	WW3D::End_Render ();
	**
	**	swap_chain_ptr->Present (nullptr, nullptr, nullptr, nullptr);
	**
	**	Restore the default render target.
	**
	*/
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static IDirect3DSwapChain8 *	Create_Additional_Swap_Chain (HWND render_window);

	/*
	** Render target interface. If render target format is WW3D_FORMAT_UNKNOWN, current display format is used.
	*/
	GGC_RB_DEPRECATED static TextureClass *	Create_Render_Target (int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN);

	static void					Set_Render_Target (IDirect3DSurface8 *render_target, bool use_default_depth_buffer = false);
	static void					Set_Render_Target (IDirect3DSurface8* render_target, IDirect3DSurface8* dpeth_buffer);

	static void					Set_Render_Target (IDirect3DSwapChain8 *swap_chain);
	GGC_RB_DEPRECATED static bool					Is_Render_To_Texture() { return IsRenderToTexture; }

	// for depth map support KJM V
	static void Create_Render_Target
	(
		int width,
		int height,
		WW3DFormat format,
		WW3DZFormat zformat,
		TextureClass** target,
		ZTextureClass** depth_buffer
	);
	GGC_RB_DEPRECATED static void					Set_Render_Target_With_Z (TextureClass * texture, ZTextureClass* ztexture=nullptr);
#endif

	GGC_RB_DEPRECATED static void Set_Shadow_Map(int idx, ZTextureClass* ztex) { Shadow_Map[idx]=ztex; }
	GGC_RB_DEPRECATED static ZTextureClass* Get_Shadow_Map(int idx) { return Shadow_Map[idx]; }
	// for depth map support KJM ^

	// shader system updates KJM v
	GGC_RB_DEPRECATED static void Apply_Default_State();

#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void Set_Vertex_Shader(DWORD vertex_shader);
	GGC_RB_DEPRECATED static void Set_Pixel_Shader(DWORD pixel_shader);

	GGC_RB_DEPRECATED static void Set_Vertex_Shader_Constant(int reg, const void* data, int count);
	GGC_RB_DEPRECATED static void Set_Pixel_Shader_Constant(int reg, const void* data, int count);
#endif
	static void Commit_Vertex_Shader_Value(DWORD vertex_shader);
	static void Commit_Pixel_Shader_Value(DWORD pixel_shader);
	static void Commit_Vertex_Shader_Constants(int reg, const void* data, int count);
	static void Commit_Pixel_Shader_Constants(int reg, const void* data, int count);

	static DWORD Get_Vertex_Processing_Behavior() { return Vertex_Processing_Behavior; }

	// Needed by scene lighting class
#if !defined(GGC_RENDER_BACKEND_BGFX)
	GGC_RB_DEPRECATED static void						Set_Ambient(const Vector3& color);
#endif
	GGC_RB_DEPRECATED static const Vector3&		Get_Ambient() { return Ambient_Color; }
	// shader system updates KJM ^




#if !defined(GGC_RENDER_BACKEND_BGFX)
	// TheSuperHackers @build bobtista 01/06/2026 Out-of-line getters; the
	// header cannot reference the file-static D3DDevice / D3DInterface
	// pointers (defined in dx8wrapper.cpp) from inline bodies -- every TU
	// including this header would otherwise fail to compile.
	static IDirect3DDevice8* _Get_D3D_Device8();
	static IDirect3D8* _Get_D3D8();
#endif
	/// Returns the display format - added by TR for video playback - not part of W3D
	static WW3DFormat	getBackBufferFormat();
	static bool Reset_Device(bool reload_assets=true);

	// TheSuperHackers @feature bobtista 16/04/2026 Move DX8 device to a different window
	static void Set_Device_Window(HWND hwnd, int width, int height);

	static const DX8Caps*	Get_Current_Caps() { WWASSERT(CurrentCaps); return CurrentCaps; }

	static bool Registry_Save_Render_Device( const char * sub_key );
	static bool Registry_Load_Render_Device( const char * sub_key, bool resize_window );

#if !defined(GGC_RENDER_BACKEND_BGFX)
	static const char* Get_DX8_Render_State_Name(D3DRENDERSTATETYPE state);
	static const char* Get_DX8_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state);
	static unsigned Get_DX8_Render_State(D3DRENDERSTATETYPE state) { return RenderStateCache::Get_Render_State((unsigned)state); }
	static unsigned Get_DX8_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state) { return RenderStateCache::Get_Texture_Stage_State(stage,(unsigned)state); }

	// Names of the specific values of render states and texture stage states
	static void Get_DX8_Texture_Stage_State_Value_Name(StringClass& name, D3DTEXTURESTAGESTATETYPE state, unsigned value);
	static void Get_DX8_Render_State_Value_Name(StringClass& name, D3DRENDERSTATETYPE state, unsigned value);

	static const char* Get_DX8_Texture_Address_Name(unsigned value);
	static const char* Get_DX8_Texture_Filter_Name(unsigned value);
	static const char* Get_DX8_Texture_Arg_Name(unsigned value);
	static const char* Get_DX8_Texture_Op_Name(unsigned value);
	static const char* Get_DX8_Texture_Transform_Flag_Name(unsigned value);
	static const char* Get_DX8_ZBuffer_Type_Name(unsigned value);
	static const char* Get_DX8_Fill_Mode_Name(unsigned value);
	static const char* Get_DX8_Shade_Mode_Name(unsigned value);
	static const char* Get_DX8_Blend_Name(unsigned value);
	static const char* Get_DX8_Cull_Mode_Name(unsigned value);
	static const char* Get_DX8_Cmp_Func_Name(unsigned value);
	static const char* Get_DX8_Fog_Mode_Name(unsigned value);
	static const char* Get_DX8_Stencil_Op_Name(unsigned value);
	static const char* Get_DX8_Material_Source_Name(unsigned value);
	static const char* Get_DX8_Vertex_Blend_Flag_Name(unsigned value);
	static const char* Get_DX8_Patch_Edge_Style_Name(unsigned value);
	static const char* Get_DX8_Debug_Monitor_Token_Name(unsigned value);
	static const char* Get_DX8_Blend_Op_Name(unsigned value);
#endif

	GGC_RB_DEPRECATED static void Invalidate_Cached_Render_States();

	static void Set_Draw_Polygon_Low_Bound_Limit(unsigned n) { DrawPolygonLowBoundLimit=n; }

protected:

	static bool	Create_Device();
	static void Release_Device();

	static void Reset_Statistics();
	static void Enumerate_Devices();
	static void Set_Default_Global_Render_States();

	/*
	** Device Selection Code.
	** For backward compatibility, the public interface for these functions is in the ww3d.
	** header file.  These functions are protected so that we aren't exposing two interfaces.
	*/
	static bool Set_Any_Render_Device();
	static bool	Set_Render_Device(const char * dev_name,int width=-1,int height=-1,int bits=-1,int windowed=-1,bool resize_window=false);
	static bool	Set_Render_Device(int dev=-1,int resx=-1,int resy=-1,int bits=-1,int windowed=-1,bool resize_window = false, bool reset_device = false, bool restore_assets=true);
	static bool Set_Next_Render_Device();
	static bool Toggle_Windowed();

	static int	Get_Render_Device_Count();
	static int	Get_Render_Device();
	static const RenderDeviceDescClass & Get_Render_Device_Desc(int deviceidx);
	static const char * Get_Render_Device_Name(int device_index);
	static bool Set_Device_Resolution(int width=-1,int height=-1,int bits=-1,int windowed=-1, bool resize_window=false);
	static void Get_Device_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed);
	static void Get_Render_Target_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed);
	static int	Get_Device_Resolution_Width() { return ResolutionWidth; }
	static int	Get_Device_Resolution_Height() { return ResolutionHeight; }

	static bool Registry_Save_Render_Device( const char *sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth);
	static bool Registry_Load_Render_Device( const char * sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int &texture_depth);
	static bool Is_Windowed() { return IsWindowed; }

	static void	Set_Texture_Bitdepth(int depth)	{ WWASSERT(depth==16 || depth==32); TextureBitDepth = depth; }
	static int	Get_Texture_Bitdepth()			{ return TextureBitDepth; }

#if !defined(GGC_RENDER_BACKEND_BGFX)
	static void Set_MSAA_Mode(D3DMULTISAMPLE_TYPE mode) { MultiSampleAntiAliasing = static_cast<unsigned>(mode); }
	static D3DMULTISAMPLE_TYPE Get_MSAA_Mode() { return static_cast<D3DMULTISAMPLE_TYPE>(MultiSampleAntiAliasing); }
#endif

	static void	Set_Swap_Interval(int swap);
	static int	Get_Swap_Interval();
	static void Set_Polygon_Mode(int mode);

	/*
	** Internal functions
	*/
	static void Resize_And_Position_Window();
	static bool Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,unsigned * set_colorbuffer,unsigned * set_backbuffer, unsigned * set_zmode);
	static bool Find_Color_Mode(unsigned colorbuffer, int resx, int resy, UINT *mode);
	static bool Find_Z_Mode(unsigned colorbuffer,unsigned backbuffer, unsigned *zmode);
	static bool Test_Z_Mode(unsigned colorbuffer,unsigned backbuffer, unsigned zmode);
	static void Compute_Caps(WW3DFormat display_format);

	/*
	** Protected Member Variables
	*/

	static RenderDeviceCleanupHook *m_pCleanupHook;

	static bool								IsInitted;
	static bool								IsDeviceLost;
	static void *							Hwnd;
	static unsigned						_MainThreadID;

	static bool								_EnableTriangleDraw;

	static int								CurRenderDevice;
	static int								ResolutionWidth;
	static int								ResolutionHeight;
	static int								BitDepth;
	static int								TextureBitDepth;
	static bool								IsWindowed;
	static unsigned						DisplayFormat;
	static unsigned						MultiSampleAntiAliasing;


	// shader system updates KJM v
	static DWORD							Vertex_Shader;
	static DWORD							Pixel_Shader;

	static Vector4							Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
	static Vector4							Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

	static LightEnvironmentClass*		Light_Environment;

	static DWORD							Vertex_Processing_Behavior;

	static ZTextureClass*				Shadow_Map[MAX_SHADOW_MAPS];

	static Vector3							Ambient_Color;
	// shader system updates KJM ^

	static bool								world_identity;

	// These fog settings are constant for all objects in a given scene,
	// unlike the matching renderstates which vary based on shader settings.
	static bool								FogEnable;
	static unsigned						FogColor;

	static DX8FrameStatistics			FrameStatistics;
	static bool								CurrentDX8LightEnables[4];

	static unsigned long FrameCount;

	static DX8Caps*						CurrentCaps;

	static unsigned							DrawPolygonLowBoundLimit;

	static bool								IsRenderToTexture;

	static int								ZBias;
	static float							ZNear;
	static float							ZFar;
#if !defined(GGC_RENDER_BACKEND_BGFX)
	static D3DMATRIX					ProjectionMatrix;
#endif

	friend void DX8_Assert();
	friend class WW3D;
	friend class DX8IndexBufferClass;
	friend class DX8VertexBufferClass;
	// TheSuperHackers @build bobtista 01/06/2026 DX8Backend is the
	// reference adapter that bridges IRenderBackend to DX8Wrapper. It needs
	// access to the protected MSAA / texture-bitdepth accessors.
	friend class DX8Backend;
};

// shader system updates KJM v
WWINLINE void DX8Wrapper::Commit_Vertex_Shader_Value(DWORD vertex_shader)
{
#if 0 //(gth) some code is bypassing this accessor function so we can't count on this variable...
	// may be incorrect if shaders are created and destroyed dynamically
	if (Vertex_Shader==vertex_shader) return;
#endif

	Vertex_Shader=vertex_shader;
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetVertexShader(Vertex_Shader));
#endif
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_Vertex_Shader(DWORD vertex_shader)
{
	Commit_Vertex_Shader_Value(vertex_shader);
}
#endif

WWINLINE void DX8Wrapper::Commit_Pixel_Shader_Value(DWORD pixel_shader)
{
	// may be incorrect if shaders are created and destroyed dynamically
	if (Pixel_Shader==pixel_shader) return;

	Pixel_Shader=pixel_shader;
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetPixelShader(Pixel_Shader));
#endif
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_Pixel_Shader(DWORD pixel_shader)
{
	Commit_Pixel_Shader_Value(pixel_shader);
}
#endif

WWINLINE void DX8Wrapper::Commit_Vertex_Shader_Constants(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Vertex_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Vertex_Shader_Constants[reg],data,memsize);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetVertexShaderConstant(reg,data,count));
#endif
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_Vertex_Shader_Constant(int reg, const void* data, int count)
{
	Commit_Vertex_Shader_Constants(reg, data, count);
}
#endif

WWINLINE void DX8Wrapper::Commit_Pixel_Shader_Constants(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Pixel_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Pixel_Shader_Constants[reg],data,memsize);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetPixelShaderConstant(reg,data,count));
#endif
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_Pixel_Shader_Constant(int reg, const void* data, int count)
{
	Commit_Pixel_Shader_Constants(reg, data, count);
}
#endif
// shader system updates KJM ^

WWINLINE void DX8Wrapper::Commit_Fixed_Function_Transform(unsigned transform, const LegacyTransformMatrix& m)
{
	{
		RenderStateCache::Set_Transform(transform,m);
		SNAPSHOT_SAY(("DX8 - SetTransform %d [%f,%f,%f,%f][%f,%f,%f,%f][%f,%f,%f,%f]",
			transform,
			m.m[0][0],m.m[0][1],m.m[0][2],m.m[0][3],
			m.m[1][0],m.m[1][1],m.m[1][2],m.m[1][3],
			m.m[2][0],m.m[2][1],m.m[2][2],m.m[2][3]));
		DX8_RECORD_MATRIX_CHANGE();
#if !defined(GGC_RENDER_BACKEND_BGFX)
		DX8CALL(SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(transform),&m));
#endif
	}
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::_Set_DX8_Transform(D3DTRANSFORMSTATETYPE transform, const D3DMATRIX& m)
{
	Commit_Fixed_Function_Transform(transform, m);
}
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::_Get_DX8_Transform(D3DTRANSFORMSTATETYPE transform, D3DMATRIX& m)
{
	DX8CALL(GetTransform(transform,&m));
}
#endif

// ----------------------------------------------------------------------------
//
// Set the index offset for the current index buffer
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_Index_Buffer_Index_Offset(unsigned offset)
{
	if (FixedFunctionState::Render_State().index_base_offset==offset) return;
	FixedFunctionState::Render_State().index_base_offset=offset;
	FixedFunctionState::Changed_Mask()|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
// Set the fog settings. This function should be used, rather than setting the
// appropriate renderstates directly, because the shader sets some of the
// renderstates on a per-mesh / per-pass basis depending on global fog states
// (stored in the wrapper) as well as the shader settings.
// This function should be called rarely - once per scene would be appropriate.
// ----------------------------------------------------------------------------

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_Fog(bool enable, const Vector3 &color, float start, float end)
{
	// Set global states
	FogEnable = enable;
	FogColor = Convert_Color(color,0.0f);

	// Invalidate the current shader (since the renderstates set by the shader
	// depend on the global fog settings as well as the actual shader settings)
	ShaderClass::Invalidate();

	// Set renderstates which are not affected by the shader
	Commit_Fixed_Function_Render_Value(D3DRS_FOGSTART, *(DWORD *)(&start));
	Commit_Fixed_Function_Render_Value(D3DRS_FOGEND,   *(DWORD *)(&end));
}


WWINLINE void DX8Wrapper::Set_Ambient(const Vector3& color)
{
	Ambient_Color=color;
	Commit_Fixed_Function_Render_Value(D3DRS_AMBIENT, DX8Wrapper::Convert_Color(color,0.0f));
}
#endif

// ----------------------------------------------------------------------------
//
// Set vertex buffer to be used in the subsequent render calls. If there was
// a vertex buffer being used earlier, release the reference to it. Passing
// nullptr just will release the vertex buffer.
//
// ----------------------------------------------------------------------------

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_DX8_Material(const D3DMATERIAL8* mat)
{
	DX8_RECORD_MATERIAL_CHANGE();
	WWASSERT(mat);
	SNAPSHOT_SAY(("DX8 - SetMaterial"));
	DX8CALL(SetMaterial(mat));
}

WWINLINE void DX8Wrapper::Set_DX8_Light(int index, D3DLIGHT8* light)
{
	if (light) {
		DX8_RECORD_LIGHT_CHANGE();
		DX8CALL(SetLight(index,light));
		DX8CALL(LightEnable(index,TRUE));
		CurrentDX8LightEnables[index]=true;
		SNAPSHOT_SAY(("DX8 - SetLight %d",index));
	}
	else if (CurrentDX8LightEnables[index]) {
		DX8_RECORD_LIGHT_CHANGE();
		CurrentDX8LightEnables[index]=false;
		DX8CALL(LightEnable(index,FALSE));
		SNAPSHOT_SAY(("DX8 - DisableLight %d",index));
	}
}
#endif

WWINLINE void DX8Wrapper::Commit_Fixed_Function_Render_Value(unsigned state, unsigned value)
{
	// Can't monitor state changes because setShader call to GERD may change the states!
	if (RenderStateCache::Get_Render_State(state)==value) return;

#if defined(MESH_RENDER_SNAPSHOT_ENABLED) && !defined(GGC_RENDER_BACKEND_BGFX)
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0,true);
		const auto legacy_state = static_cast<D3DRENDERSTATETYPE>(state);
		Get_DX8_Render_State_Value_Name(value_name,legacy_state,value);
		SNAPSHOT_SAY(("DX8 - SetRenderState(state: %s, value: %s)",
			Get_DX8_Render_State_Name(legacy_state),
			value_name.str()));
	}
#endif

	RenderStateCache::Set_Render_State(state,value);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetRenderState( static_cast<D3DRENDERSTATETYPE>(state), value ));
#endif
	DX8_RECORD_RENDER_STATE_CHANGE();
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_DX8_Render_State(D3DRENDERSTATETYPE state, unsigned value)
{
	Commit_Fixed_Function_Render_Value(state, value);
}
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_DX8_Clip_Plane(DWORD Index, CONST float* pPlane)
{
	DX8CALL(SetClipPlane( Index, pPlane ));
}
#endif

WWINLINE void DX8Wrapper::Commit_Fixed_Function_Texture_Stage_Value(unsigned stage, unsigned state, unsigned value)
{
	if (stage >= MAX_TEXTURE_STAGES)
	{
#if !defined(GGC_RENDER_BACKEND_BGFX)
		DX8CALL(SetTextureStageState( stage, static_cast<D3DTEXTURESTAGESTATETYPE>(state), value ));
		DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
#endif
  		return;
  	}

	// Can't monitor state changes because setShader call to GERD may change the states!
	if (RenderStateCache::Get_Texture_Stage_State(stage,state)==value) return;
#if defined(MESH_RENDER_SNAPSHOT_ENABLED) && !defined(GGC_RENDER_BACKEND_BGFX)
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0,true);
		const auto legacy_state = static_cast<D3DTEXTURESTAGESTATETYPE>(state);
		Get_DX8_Texture_Stage_State_Value_Name(value_name,legacy_state,value);
		SNAPSHOT_SAY(("DX8 - SetTextureStageState(stage: %d, state: %s, value: %s)",
			stage,
			Get_DX8_Texture_Stage_State_Name(legacy_state),
			value_name.str()));
	}
#endif

	RenderStateCache::Set_Texture_Stage_State(stage,state,value);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetTextureStageState( stage, static_cast<D3DTEXTURESTAGESTATETYPE>(state), value ));
#endif
	DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_DX8_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state, unsigned value)
{
	Commit_Fixed_Function_Texture_Stage_Value(stage, state, value);
}

WWINLINE void DX8Wrapper::Set_DX8_Texture(unsigned int stage, IDirect3DBaseTexture8* texture)
{
  	if (stage >= MAX_TEXTURE_STAGES)
  	{	DX8CALL(SetTexture(stage, texture));
  		return;
  	}

	if (FixedFunctionState::Raw_Texture(stage)==texture) return;

	SNAPSHOT_SAY(("DX8 - SetTexture(%x) ",texture));

	FixedFunctionState::Set_Raw_Texture(stage, texture);
	DX8CALL(SetTexture(stage, texture));
	DX8_RECORD_TEXTURE_CHANGE();
}
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::_Copy_DX8_Rects(
  IDirect3DSurface8* pSourceSurface,
  CONST RECT* pSourceRectsArray,
  UINT cRects,
  IDirect3DSurface8* pDestinationSurface,
  CONST POINT* pDestPointsArray
)
{
	DX8CALL(CopyRects(
  pSourceSurface,
  pSourceRectsArray,
  cRects,
  pDestinationSurface,
	  pDestPointsArray));
}
#endif

WWINLINE Vector4 DX8Wrapper::Convert_Color(unsigned color)
{
	Vector4 col;
	col[3]=((color&0xff000000)>>24)/255.0f;
	col[0]=((color&0xff0000)>>16)/255.0f;
	col[1]=((color&0xff00)>>8)/255.0f;
	col[2]=((color&0xff)>>0)/255.0f;
//	col=Vector4(1.0f,1.0f,1.0f,1.0f);
	return col;
}

#if 0
WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector3& color, const float alpha)
{
	WWASSERT(color.X<=1.0f);
	WWASSERT(color.Y<=1.0f);
	WWASSERT(color.Z<=1.0f);
	WWASSERT(alpha<=1.0f);
	WWASSERT(color.X>=0.0f);
	WWASSERT(color.Y>=0.0f);
	WWASSERT(color.Z>=0.0f);
	WWASSERT(alpha>=0.0f);

	return D3DCOLOR_COLORVALUE(color.X,color.Y,color.Z,alpha);
}
WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector4& color)
{
	WWASSERT(color.X<=1.0f);
	WWASSERT(color.Y<=1.0f);
	WWASSERT(color.Z<=1.0f);
	WWASSERT(color.W<=1.0f);
	WWASSERT(color.X>=0.0f);
	WWASSERT(color.Y>=0.0f);
	WWASSERT(color.Z>=0.0f);
	WWASSERT(color.W>=0.0f);

	return D3DCOLOR_COLORVALUE(color.X,color.Y,color.Z,color.W);
}
#else

// ----------------------------------------------------------------------------
//
// Convert RGBA color from float vector to 32 bit integer
// Note: Color vector needs to be clamped to [0...1] range!
//
// ----------------------------------------------------------------------------

WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector3& color,float alpha)
{
#if defined(_MSC_VER) && _MSC_VER < 1300
	const float scale = 255.0;
	unsigned int col;

	// Multiply r, g, b and a components (0.0,...,1.0) by 255 and convert to integer. Or the integer values togerher
	// such that 32 bit integer has AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB.
	__asm
	{
		sub	esp,20					// space for a, r, g and b float plus fpu rounding mode

		// Store the fpu rounding mode

		fwait
		fstcw		[esp+16]				// store control word to stack
		mov		eax,[esp+16]		// load it to eax
		mov		edi,eax				// take copy
		and		eax,~(1024|2048)	// mask out certain bits
		or			eax,(1024|2048)	// or with precision control value "truncate"
		sub		edi,eax				// did it change?
		jz			skip					// .. if not, skip
		mov		[esp],eax			// .. change control word
		fldcw		[esp]
skip:

		// Convert the color

		mov	esi,dword ptr color
		fld	dword ptr[scale]

		fld	dword ptr[esi]			// r
		fld	dword ptr[esi+4]		// g
		fld	dword ptr[esi+8]		// b
		fld	dword ptr[alpha]		// a
		fld	st(4)
		fmul	st(4),st
		fmul	st(3),st
		fmul	st(2),st
		fmulp	st(1),st
		fistp	dword ptr[esp+0]		// a
		fistp	dword ptr[esp+4]		// b
		fistp	dword ptr[esp+8]		// g
		fistp	dword ptr[esp+12]		// r
		mov	ecx,[esp]				// a
		mov	eax,[esp+4]				// b
		mov	edx,[esp+8]				// g
		mov	ebx,[esp+12]			// r
		shl	ecx,24					// a << 24
		shl	ebx,16					// r << 16
		shl	edx,8						//	g << 8
		or		eax,ecx					// (a << 24) | b
		or		eax,ebx					// (a << 24) | (r << 16) | b
		or		eax,edx					// (a << 24) | (r << 16) | (g << 8) | b

		fstp	st(0)

		// Restore fpu rounding mode

		cmp	edi,0					// did we change the value?
		je		not_changed			// nope... skip now...
		fwait
		fldcw	[esp+16];
not_changed:
		add	esp,20

		mov	col,eax
	}
	return col;
#else
	return color.Convert_To_ARGB(alpha);
#endif // defined(_MSC_VER) && _MSC_VER < 1300
}

// ----------------------------------------------------------------------------
//
// Clamp color vector to [0...1] range
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Clamp_Color(Vector4& color)
{
#if defined(_MSC_VER) && _MSC_VER < 1300
	if (CPUDetectClass::Has_CMOV_Instruction()) {
	__asm
	{
		mov	esi,dword ptr color

		mov edx,0x3f800000

		mov edi,dword ptr[esi]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi],edi

		mov edi,dword ptr[esi+4]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+4],edi

		mov edi,dword ptr[esi+8]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+8],edi

		mov edi,dword ptr[esi+12]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+12],edi
	}
	return;
	}
#endif // defined(_MSC_VER) && _MSC_VER < 1300

	for (int i=0;i<4;++i) {
		float f=(color[i]<0.0f) ? 0.0f : color[i];
		color[i]=(f>1.0f) ? 1.0f : f;
	}
}

// ----------------------------------------------------------------------------
//
// Convert RGBA color from float vector to 32 bit integer
//
// ----------------------------------------------------------------------------

WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector4& color)
{
	return Convert_Color(reinterpret_cast<const Vector3&>(color),color[3]);
}

WWINLINE unsigned int DX8Wrapper::Convert_Color_Clamp(const Vector4& color)
{
	Vector4 clamped_color=color;
	DX8Wrapper::Clamp_Color(clamped_color);
	return Convert_Color(reinterpret_cast<const Vector3&>(clamped_color),clamped_color[3]);
}

#endif


WWINLINE void DX8Wrapper::Set_Alpha (const float alpha, unsigned int &color)
{
	unsigned char *component = (unsigned char*) &color;

	component [3] = 255.0f * alpha;
}

WWINLINE void DX8Wrapper::Get_Render_State(RenderStateStruct& state)
{
	FixedFunctionState::Capture_Render_State(state);
}

WWINLINE void DX8Wrapper::Get_Shader(ShaderClass& shader)
{
	shader=FixedFunctionState::Render_State().shader;
}

WWINLINE void DX8Wrapper::Commit_Fixed_Function_Texture(unsigned stage,TextureBaseClass* texture)
{
	WWASSERT(stage<(unsigned int)CurrentCaps->Get_Max_Textures_Per_Pass());
	FixedFunctionState::Set_Texture(stage, texture);
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
WWINLINE void DX8Wrapper::Set_Texture(unsigned stage,TextureBaseClass* texture)
{
	Commit_Fixed_Function_Texture(stage, texture);
}

WWINLINE void DX8Wrapper::Set_Material(const VertexMaterialClass* material)
{
/*	if (material && FixedFunctionState::Render_State().material &&
		// !stricmp(material->Get_Name(),FixedFunctionState::Render_State().material->Get_Name())) {
		material->Get_CRC()!=FixedFunctionState::Render_State().material->Get_CRC()) {
		return;
	}
*/
//	if (material==FixedFunctionState::Render_State().material) {
//		return;
//	}
	FixedFunctionState::Set_Material(material);
	SNAPSHOT_SAY(("DX8Wrapper::Set_Material(%s)",material ? material->Get_Name() : "null"));
}

WWINLINE void DX8Wrapper::Set_Shader(const ShaderClass& shader)
{
	if (!FixedFunctionState::Set_Shader(shader, ShaderClass::ShaderDirty)) {
		return;
	}
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	StringClass str;
#endif
	SNAPSHOT_SAY(("DX8Wrapper::Set_Shader(%s)",shader.Get_Description(str).str()));
}

WWINLINE void DX8Wrapper::Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix, float znear, float zfar)
{
	ZFar=zfar;
	ZNear=znear;
	ProjectionMatrix=To_D3DMATRIX(matrix);

	if (!Get_Current_Caps()->Support_ZBias() && ZNear!=ZFar) {
		D3DMATRIX tmp=ProjectionMatrix;
		float tmp_zbias=ZBias;
		tmp_zbias*=(1.0f/16.0f);
		tmp_zbias*=1.0f / (ZFar - ZNear);
		tmp.m[2][2]-=tmp_zbias*tmp.m[3][2];
		DX8CALL(SetTransform(D3DTS_PROJECTION,&tmp));
	}
	else {
		DX8CALL(SetTransform(D3DTS_PROJECTION,&ProjectionMatrix));
	}
}

WWINLINE void DX8Wrapper::Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix4x4& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		FixedFunctionState::Render_State().world=To_D3DMATRIX(m);
		FixedFunctionState::Changed_Mask()|=(unsigned)WORLD_CHANGED;
		FixedFunctionState::Changed_Mask()&=~(unsigned)WORLD_IDENTITY;
		break;
	case D3DTS_VIEW:
		FixedFunctionState::Render_State().view=To_D3DMATRIX(m);
		FixedFunctionState::Changed_Mask()|=(unsigned)VIEW_CHANGED;
		FixedFunctionState::Changed_Mask()&=~(unsigned)VIEW_IDENTITY;
		break;
	case D3DTS_PROJECTION:
		{
			D3DMATRIX ProjectionMatrix=To_D3DMATRIX(m);
			ZFar=0.0f;
			ZNear=0.0f;
			DX8CALL(SetTransform(D3DTS_PROJECTION,&ProjectionMatrix));
		}
		break;
	default:
		DX8_RECORD_MATRIX_CHANGE();
		D3DMATRIX dxm=To_D3DMATRIX(m);
		DX8CALL(SetTransform(transform,&dxm));
		break;
	}
}

WWINLINE void DX8Wrapper::Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix3D& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		FixedFunctionState::Render_State().world=To_D3DMATRIX(m);
		FixedFunctionState::Changed_Mask()|=(unsigned)WORLD_CHANGED;
		FixedFunctionState::Changed_Mask()&=~(unsigned)WORLD_IDENTITY;
		break;
	case D3DTS_VIEW:
		FixedFunctionState::Render_State().view=To_D3DMATRIX(m);
		FixedFunctionState::Changed_Mask()|=(unsigned)VIEW_CHANGED;
		FixedFunctionState::Changed_Mask()&=~(unsigned)VIEW_IDENTITY;
		break;
	default:
		DX8_RECORD_MATRIX_CHANGE();
		D3DMATRIX dxm=To_D3DMATRIX(m);
		DX8CALL(SetTransform(transform,&dxm));
		break;
	}
}

WWINLINE bool DX8Wrapper::Is_World_Identity()
{
	return FixedFunctionState::Is_World_Identity();
}

WWINLINE bool DX8Wrapper::Is_View_Identity()
{
	return FixedFunctionState::Is_View_Identity();
}

WWINLINE void DX8Wrapper::Get_Transform(D3DTRANSFORMSTATETYPE transform, Matrix4x4& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		if (FixedFunctionState::Changed_Mask()&WORLD_IDENTITY) m.Make_Identity();
		else m=To_Matrix4x4(FixedFunctionState::Render_State().world);
		break;
	case D3DTS_VIEW:
		if (FixedFunctionState::Changed_Mask()&VIEW_IDENTITY) m.Make_Identity();
		else m=To_Matrix4x4(FixedFunctionState::Render_State().view);
		break;
	default:
		D3DMATRIX dxm;
		DX8CALL(GetTransform(transform,&dxm));
		m=To_Matrix4x4(dxm);
		break;
	}
}

WWINLINE void DX8Wrapper::Set_Render_State(const RenderStateStruct& state)
{
	FixedFunctionState::Restore_Render_State(state);
}
#endif

WWINLINE void DX8Wrapper::Release_Render_State()
{
	FixedFunctionState::Release_Render_State();
}
