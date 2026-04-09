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

 // Ronin @build 30/03/2026 DX9: D3DX effect handles must be large-address-safe in LAA builds.
#ifndef D3DXFX_LARGEADDRESS_HANDLE
#define D3DXFX_LARGEADDRESS_HANDLE
#endif


// Ronin 19/10/2025 DX8-to-DX9 compatibility layer must be included before always.h to block DX8 headers.
#include <d3d9.h>  // Native DX9
#include <d3dx9.h> // Ronin @bugfix 02/11/2025 DX9: Required for D3DXLoadSurfaceFromSurface

// Ronin @build 26/10/2025 DX9: Guard typedefs to prevent redefinition errors
#ifndef DX8_TO_DX9_TYPEDEFS_DEFINED
#define DX8_TO_DX9_TYPEDEFS_DEFINED

typedef IDirect3D9 IDirect3D8;
typedef IDirect3DDevice9 IDirect3DDevice8;
typedef IDirect3DVolume9 IDirect3DVolume8;
typedef IDirect3DSwapChain9 IDirect3DSwapChain8;
typedef D3DVIEWPORT9 D3DVIEWPORT8;
typedef IDirect3DBaseTexture9 IDirect3DBaseTexture8;
typedef IDirect3DTexture9 IDirect3DTexture8;
typedef IDirect3DCubeTexture9 IDirect3DCubeTexture8;
typedef IDirect3DVolumeTexture9 IDirect3DVolumeTexture8;
typedef IDirect3DSurface9 IDirect3DSurface8;

#endif // DX8_TO_DX9_TYPEDEFS_DEFINED

// Ronin @build 25/10/2025 DX9: Removed render states - map to DX9 equivalents
#define D3DRS_ZBIAS D3DRS_DEPTHBIAS
#define D3DFMT_W11V11U10 D3DFMT_UNKNOWN

#include "WWLib/always.h"
#include <Utility/bit_adapter.h>
#include "dllist.h"
#include "WWMath/matrix4.h"
#include "statistics.h"
#include "WWLib/wwstring.h"
#include "WW3D2/lightenvironment.h"
#include "WW3D2/shader.h"
#include "WWMath/vector4.h"
#include "WWLib/cpudetect.h"
#include "dx8caps.h"

#include "texture.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "WW3D2/vertmaterial.h"

// Ronin @feature 26/11/2025: Vertex declaration test system
#include "VertexDeclCache.h"
#include "WaterTracksDecl.h"

// Ronin @feature 2/12/2025: Layout binding enforcement
#ifdef _DEBUG
// @bugfix Ronin 18/01/2026: Use one shared flag for ASSERT/ALLOW (previously each macro had its own static).
namespace DX8WrapperLayoutBinding
{
	extern bool g_layoutBindingAllowed;

	// @debug Ronin 18/01/2026: Prints useful state when raw layout binds are attempted without authorization.
	void Report_LayoutBindingViolation(const char* api, const char* callsite);
}

// @debug Ronin 18/01/2026 DX9: Utility to format callsite
#ifndef DX8WRAP_STRINGIFY2
#define DX8WRAP_STRINGIFY2(x) #x
#define DX8WRAP_STRINGIFY(x) DX8WRAP_STRINGIFY2(x)
#endif

#define ASSERT_LAYOUT_BINDING_ALLOWED_API(apiName) \
	do { \
		if (!DX8WrapperLayoutBinding::g_layoutBindingAllowed) { \
			DX8WrapperLayoutBinding::Report_LayoutBindingViolation( \
				(apiName), __FILE__ ":" DX8WRAP_STRINGIFY(__LINE__)); \
			WWASSERT(0 && "Direct IA layout binding forbidden; use BindLayoutFVF/BindLayoutDecl"); \
		} \
	} while (0)

// Back-compat for existing callsites that don't specify API name
#define ASSERT_LAYOUT_BINDING_ALLOWED() ASSERT_LAYOUT_BINDING_ALLOWED_API("UnknownLayoutBind")

// @bugfix Ronin 18/01/2026: Scoped permission. Cannot accidentally remain enabled.
#define ALLOW_LAYOUT_BINDING() \
	struct ScopedLayoutAccess { \
		ScopedLayoutAccess() { DX8WrapperLayoutBinding::g_layoutBindingAllowed = true; } \
		~ScopedLayoutAccess() { DX8WrapperLayoutBinding::g_layoutBindingAllowed = false; } \
	} __layoutAccess

#else

#define ASSERT_LAYOUT_BINDING_ALLOWED_API(apiName) do {} while (0)
#define ASSERT_LAYOUT_BINDING_ALLOWED() do {} while (0)
#define ALLOW_LAYOUT_BINDING() do {} while (0)


#endif

// Call-site tracking macros
#ifndef STRINGIFY
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#endif

#define BIND_LAYOUT_FVF(fvf) \
    DX8Wrapper::BindLayoutFVF(fvf, __FILE__ ":" STRINGIFY(__LINE__))

#define BIND_LAYOUT_DECL(decl) \
    DX8Wrapper::BindLayoutDecl(decl, __FILE__ ":" STRINGIFY(__LINE__))


// Ronin @feature 28/11/2025: Call-site tracking macros
// Expands to BindLayoutFVF(fvf, "File.cpp:123") for ownership tracking
#ifndef STRINGIFY
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#endif

#define BIND_LAYOUT_FVF(fvf) \
    DX8Wrapper::BindLayoutFVF(fvf, __FILE__ ":" STRINGIFY(__LINE__))

#define BIND_LAYOUT_DECL(decl) \
    DX8Wrapper::BindLayoutDecl(decl, __FILE__ ":" STRINGIFY(__LINE__))

// Ronin @feature 27/11/2025: Pipeline state capture for debugging state pollution
#ifdef _DEBUG
struct PipelineStateSnapshot {
	const char* captureLocation = nullptr;
	DWORD fvf = 0;
	IDirect3DVertexDeclaration9* decl = nullptr;

	struct StreamBinding {
		IDirect3DVertexBuffer9* buffer = nullptr;
		UINT offset = 0;
		UINT stride = 0;
	} streams[4];

	IDirect3DIndexBuffer9* indexBuffer = nullptr;
	D3DVIEWPORT9 viewport = {};
	Matrix4x4 worldTransform;
	Matrix4x4 viewTransform;
	Matrix4x4 projectionTransform;

	// @bugfix Ronin 27/02/2026 DX9: Release all COM references captured by Get* calls
	~PipelineStateSnapshot()
	{
		if (decl) { decl->Release(); decl = nullptr; }
		for (int i = 0; i < 4; ++i) {
			if (streams[i].buffer) { streams[i].buffer->Release(); streams[i].buffer = nullptr; }
		}
		if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
	}

	// Non-copyable (prevent double-release)
	PipelineStateSnapshot() = default;
	PipelineStateSnapshot(const PipelineStateSnapshot&) = delete;
	PipelineStateSnapshot& operator=(const PipelineStateSnapshot&) = delete;
};

// Macro for capturing pipeline state with call-site tracking
#define CAPTURE_PIPELINE_STATE() \
    DX8Wrapper::Capture_Pipeline_State(__FILE__ ":" STRINGIFY(__LINE__))

// Macro for validating pipeline state was restored
#define VALIDATE_PIPELINE_RESTORED(snapshot) \
    DX8Wrapper::Validate_Pipeline_State_Restored(snapshot, __FILE__ ":" STRINGIFY(__LINE__))

#endif // _DEBUG

/*
** Registry value names
*/
#define	VALUE_NAME_RENDER_DEVICE_NAME					"RenderDeviceName"
#define	VALUE_NAME_RENDER_DEVICE_WIDTH				"RenderDeviceWidth"
#define	VALUE_NAME_RENDER_DEVICE_HEIGHT				"RenderDeviceHeight"
#define	VALUE_NAME_RENDER_DEVICE_DEPTH				"RenderDeviceBitDepth"
#define	VALUE_NAME_RENDER_DEVICE_WINDOWED			"RenderDeviceWindowed"
#define	VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH		"RenderDeviceTextureBitDepth"

const unsigned MAX_TEXTURE_STAGES=8;
const unsigned MAX_VERTEX_STREAMS=2;
const unsigned MAX_VERTEX_SHADER_CONSTANTS=96;
const unsigned MAX_PIXEL_SHADER_CONSTANTS=8;
const unsigned MAX_SHADOW_MAPS=1;
// @feature Ronin 26/03/2026 DX9: Keep wrapper light storage aligned with LightEnvironmentClass.
const unsigned MAX_LIGHTS = LightEnvironmentClass::MAX_LIGHTS;

enum {
	BUFFER_TYPE_DX8,
	BUFFER_TYPE_SORTING,
	BUFFER_TYPE_DYNAMIC_DX8,
	BUFFER_TYPE_DYNAMIC_SORTING,
	BUFFER_TYPE_INVALID
};

class VertexMaterialClass;
class CameraClass;
class LightEnvironmentClass;
class RenderDeviceDescClass;
class VertexBufferClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class DynamicIBAccessClass;
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

// Ronin @build 02/11/2025 DX9: Forward declare DXGetErrorString9A for enhanced error reporting
const char* DXGetErrorString9A(HRESULT hr);

void DX8_Assert();
void Log_DX8_ErrorCode(unsigned res);

WWINLINE void DX8_ErrorCode(unsigned res)
{
	if (res==D3D_OK) return;
	Log_DX8_ErrorCode(res);
}

// Converts a FLOAT to a DWORD for use in SetRenderState() calls
inline DWORD F2DW(FLOAT f) { return std::bit_cast<DWORD>(f); }

#ifdef WWDEBUG
#define DX8CALL_HRES(x,res) \
	{ \
		DX8_Assert(); \
		res = DX8Wrapper::_Get_D3D_Device8()->x; \
		DX8Wrapper::Increment_DX8_CallCount(); \
		if (FAILED(res)) { \
			WWDEBUG_SAY(("DX8CALL_HRES FAILED: %s returned 0x%08X (%s) at %s:%d", \
				#x, res, DXGetErrorString9A(res), __FILE__, __LINE__)); \
			if (res == D3DERR_INVALIDCALL) { \
				WWDEBUG_SAY(("  D3DERR_INVALIDCALL DETECTED!")); \
			} \
		} \
		DX8_ErrorCode(res); \
	}

#define DX8CALL(x) \
	{ \
		DX8_Assert(); \
		HRESULT __hr = DX8Wrapper::_Get_D3D_Device8()->x; \
		DX8Wrapper::Increment_DX8_CallCount(); \
		if (FAILED(__hr)) { \
			WWDEBUG_SAY(("DX8CALL FAILED: %s returned 0x%08X (%s) at %s:%d", \
				#x, __hr, DXGetErrorString9A(__hr), __FILE__, __LINE__)); \
			if (__hr == D3DERR_INVALIDCALL) { \
				WWDEBUG_SAY(("  D3DERR_INVALIDCALL DETECTED!")); \
			} \
		} \
		DX8_ErrorCode(__hr); \
	}

#define DX8CALL_D3D(x) \
	{ \
		DX8_Assert(); \
		HRESULT __hr = DX8Wrapper::_Get_D3D8()->x; \
		DX8Wrapper::Increment_DX8_CallCount(); \
		if (FAILED(__hr)) { \
			WWDEBUG_SAY(("DX8CALL_D3D FAILED: %s returned 0x%08X (%s) at %s:%d", \
				#x, __hr, DXGetErrorString9A(__hr), __FILE__, __LINE__)); \
			if (__hr == D3DERR_INVALIDCALL) { \
				WWDEBUG_SAY(("  D3DERR_INVALIDCALL DETECTED!")); \
			} \
		} \
		DX8_ErrorCode(__hr); \
	}

#define DX8_THREAD_ASSERT() if (_DX8SingleThreaded) { WWASSERT_PRINT(DX8Wrapper::_Get_Main_Thread_ID()==ThreadClass::_Get_Current_Thread_ID(),"DX8Wrapper::DX8 calls must be called from the main thread!"); }
#else
#define DX8CALL_HRES(x,res) res = DX8Wrapper::_Get_D3D_Device8()->x; DX8Wrapper::Increment_DX8_CallCount();
#define DX8CALL(x) DX8Wrapper::_Get_D3D_Device8()->x; DX8Wrapper::Increment_DX8_CallCount();
#define DX8CALL_D3D(x) DX8Wrapper::_Get_D3D8()->x; DX8Wrapper::Increment_DX8_CallCount();
#define DX8_THREAD_ASSERT() ;
#endif


#define no_EXTENDED_STATS
// EXTENDED_STATS collects additional timing statistics by turning off parts
// of the 3D drawing system (terrain, objects, etc.)
#ifdef EXTENDED_STATS
class DX8_Stats
{
public:
	bool m_showingStats;
	bool m_disableTerrain;
	bool m_disableWater;
	bool m_disableObjects;
	bool m_disableOverhead;
	bool m_disableConsole;
	int  m_debugLinesToShow;
	int	 m_sleepTime;
public:
	DX8_Stats() {
		m_disableConsole = m_showingStats = m_disableTerrain = m_disableWater = m_disableOverhead = m_disableObjects = false;
		m_sleepTime = 0;
		m_debugLinesToShow = -1; // -1 means show all expected lines of output
	}
};
#endif


// This virtual interface was added for the Generals RTS.
// It is called before resetting the dx8 device to ensure
// that all dx8 resources are released.  Otherwise reset fails. jba.
class DX8_CleanupHook
{
public:
	virtual void ReleaseResources()=0;
	virtual void ReAcquireResources()=0;
};


struct RenderStateStruct
{
	ShaderClass shader;
	VertexMaterialClass* material;
	TextureBaseClass * Textures[MAX_TEXTURE_STAGES];
	D3DLIGHT9 Lights[MAX_LIGHTS];
	bool LightEnable[MAX_LIGHTS];
	D3DMATRIX world;
	D3DMATRIX view;
	unsigned vertex_buffer_types[MAX_VERTEX_STREAMS];
	unsigned index_buffer_type;
	unsigned short vba_offset;
	unsigned short vba_count;
	unsigned short iba_offset;
	VertexBufferClass* vertex_buffers[MAX_VERTEX_STREAMS];
	IndexBufferClass* index_buffer;
	unsigned short index_base_offset;

	// Ronin @bugfix 15/01/2026 DX9: Track DynamicVBAccess FVF for correct stride verification
	DWORD vba_fvf;

	// Ronin @bugfix 26/01/2026 DX9: Track the actual D3D VB used by DynamicVBAccess.
// Without this, Apply() cannot reliably bind Stream0 for BUFFER_TYPE_DYNAMIC_DX8 (RTTI/unsafe casts break rendering).
	IDirect3DVertexBuffer9* vba_d3d_vb = nullptr;


	// Ronin @build 29/11/2025 DX9: Enhanced layout tracking
	DWORD currentFVF = 0; // Track active FVF code (0 = none)
	IDirect3DVertexDeclaration9* currentDecl = nullptr; // Track active declaration (NULL = none)
	const char* layoutOwner = "none";

	// Ronin @bugfix 01/12/2025: DX9 layout tracking for JIT binding
	IDirect3DVertexShader9* currentVS;
	IDirect3DPixelShader9* currentPS;

	// @debug Ronin 23/01/2026 DX9: Expected IA layout at DIP time (used for ENSURE).
	DWORD expectedFVF = 0;
	IDirect3DVertexDeclaration9* expectedDecl = nullptr;


	RenderStateStruct();
	~RenderStateStruct();

	RenderStateStruct& operator= (const RenderStateStruct& src);
};

/**
** DX8Wrapper
**
** DX8 interface wrapper class.  This encapsulates the DX8 interface; adding redundant state
** detection, stat tracking, etc etc.  In general, we will wrap all DX8 calls with at least
** an WWINLINE function so that we can add stat tracking, etc if needed.  Direct access to the
** D3D device will require "friend" status and should be granted only in extreme circumstances :-)
*/
class DX8Wrapper
{
	enum ChangedStates {
		WORLD_CHANGED = 1 << 0,
		VIEW_CHANGED = 1 << 1,

		LIGHT0_CHANGED = 1 << 2,
		LIGHT1_CHANGED = 1 << 3,
		LIGHT2_CHANGED = 1 << 4,
		LIGHT3_CHANGED = 1 << 5,
		LIGHT4_CHANGED = 1 << 6,
		LIGHT5_CHANGED = 1 << 7,
		LIGHT6_CHANGED = 1 << 8,
		LIGHT7_CHANGED = 1 << 9,
		LIGHT8_CHANGED = 1 << 10,
		LIGHT9_CHANGED = 1 << 11,

		TEXTURE0_CHANGED = 1 << 12,
		TEXTURE1_CHANGED = 1 << 13,
		TEXTURE2_CHANGED = 1 << 14,
		TEXTURE3_CHANGED = 1 << 15,

		MATERIAL_CHANGED = 1 << 16,
		SHADER_CHANGED = 1 << 17,
		VERTEX_BUFFER_CHANGED = 1 << 18,
		INDEX_BUFFER_CHANGED = 1 << 19,
		WORLD_IDENTITY = 1 << 20,
		VIEW_IDENTITY = 1 << 21,

		TEXTURES_CHANGED =
		TEXTURE0_CHANGED | TEXTURE1_CHANGED | TEXTURE2_CHANGED | TEXTURE3_CHANGED,
		LIGHTS_CHANGED =
		LIGHT0_CHANGED | LIGHT1_CHANGED | LIGHT2_CHANGED | LIGHT3_CHANGED |
		LIGHT4_CHANGED | LIGHT5_CHANGED | LIGHT6_CHANGED | LIGHT7_CHANGED |
		LIGHT8_CHANGED | LIGHT9_CHANGED,
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

	// Ronin @feature 10/03/2026 DX9: Bind existing GUI AntiAliasing option to DX9 multisampling.
	static void Set_Anti_Aliasing_Level(int level);
	static int Get_Anti_Aliasing_Level() { return AntiAliasingLevel; }

	// Ronin @bugfix 19/02/2026 DX9: Dirty VB/IB change flags without clearing the tracked pointers.
	// Used after instanced drawing so Apply_Render_State_Changes re-binds the container's buffers.
	static void Invalidate_Vertex_Buffer_State()
	{
		render_state_changed |= VERTEX_BUFFER_CHANGED | INDEX_BUFFER_CHANGED;
	}

	// Ronin @bugfix 14/09/2026 DX9: after clearing texture stages straight on the device, call this — otherwise both texture
	// caches still believe the old texture is bound and skip the next real bind (untextured, white infantry).
	static void Invalidate_Texture_State(unsigned firstStage, unsigned stageCount);

#ifdef WWDEBUG
	// @debug Ronin 18/01/2026 DX9: Allow draw-site to tag subsequent DIP logs with a human-readable label
	static void Set_Debug_Draw_Context(const char* label);
	static void Clear_Debug_Draw_Context();
	static const char* Get_Debug_Draw_Context();
#endif

	// Ronin @debug 10/01/2026 Add optional caller tag to index buffer binding for tracking IB rebinds
	static void Set_Index_Buffer(const IndexBufferClass* ib, unsigned short index_base_offset, const char* callerTag);
	static void Set_Index_Buffer(const DynamicIBAccessClass& iba, unsigned short index_base_offset, const char* callerTag);

	// Ronin @bugfix 09/01/2026 DX9: Force stream0 binding with explicit stride (keeps wrapper tracking coherent)
	static void Force_Stream0(IDirect3DVertexBuffer9* vb, UINT offset, UINT stride);

	// Ronin @feature 05/12/2025: Public accessors for render state clearing
	// Ronin @bugfix 12/01/2026: Clearing tracked layout must force a re-apply to keep device/wrapper coherent
	static void Clear_Current_Decl()
	{
		render_state.currentDecl = nullptr;
		render_state_changed |= VERTEX_BUFFER_CHANGED;
	}

	static void Clear_Current_FVF()
	{
		render_state.currentFVF = 0;
		render_state_changed |= VERTEX_BUFFER_CHANGED;
	}

	// Ronin @feature 05/01/2026: Public getters for debugging state pollution
	static IDirect3DVertexDeclaration9* Get_Current_Decl() { return render_state.currentDecl; }
	static DWORD Get_Current_FVF() { return render_state.currentFVF; }

	// Ronin @feature 27/11/2025: Vertex declaration cache for water tracks
	static VertexDeclCache* DeclCache;

	static void Init_Decl_Cache(IDirect3DDevice9* device);
	static void Shutdown_Decl_Cache();

	// Ronin @feature 27/11/2025: Central layout binding functions
	// These are the ONLY allowed places to call SetFVF/SetVertexDeclaration
	// Use BIND_LAYOUT_FVF(fvf) and BIND_LAYOUT_DECL(decl) macros for call-site tracking
	static void BindLayoutFVF(DWORD fvf, const char* owner);
	static void BindLayoutDecl(IDirect3DVertexDeclaration9* decl, const char* owner);

	// Ronin @feature 10/11/2025: DX9 shader/FVF management helpers

	/**
	 * Safely switches to fixed-function pipeline
	 * Clears all shaders and sets FVF
	 */
	static void Switch_To_Fixed_Function_Pipeline(DWORD fvf);

	/**
	 * Safely switches to programmable pipeline
	 * Clears FVF before setting shaders
	 */
	static void Switch_To_Programmable_Pipeline(
		IDirect3DVertexShader9* vs = nullptr,
		IDirect3DPixelShader9* ps = nullptr,
		IDirect3DVertexDeclaration9* customDecl = nullptr
	);
	/**
	 * Debug helper to validate current pipeline state
	 */
	static bool Validate_Pipeline_State(const char* callerName = "Unknown"); 

	/**
	 * Helper to set vertex declaration with state tracking
	 */
	static void Set_Vertex_Declaration(IDirect3DVertexDeclaration9* decl);

	// Add to DX8Wrapper class public section:
#ifdef _DEBUG
	static PipelineStateSnapshot* Capture_Pipeline_State(const char* location);
	static bool Validate_Pipeline_State_Restored(PipelineStateSnapshot* snapshot, const char* location);
	static void Log_Pipeline_State_Diff(const PipelineStateSnapshot* before, const PipelineStateSnapshot* after);
#endif

#ifdef EXTENDED_STATS
	static DX8_Stats stats;
#endif

	static unsigned long FrameCount;

	static bool Init(void * hwnd, bool lite = false);
	static void Shutdown();

	static void SetCleanupHook(DX8_CleanupHook *pCleanupHook) {m_pCleanupHook = pCleanupHook;};
	/*
	** Some WW3D sub-systems need to be initialized after the device is created and shutdown
	** before the device is released.
	*/
	static void	Do_Onetime_Device_Dependent_Inits();
	static void Do_Onetime_Device_Dependent_Shutdowns();

	static bool Is_Device_Lost() { return IsDeviceLost; }
	static bool Is_Initted() { return IsInitted; }

	static bool Has_Stencil ();
	static void Get_Format_Name(unsigned int format, StringClass *tex_format);

	/*
	** Rendering
	*/
	static void Begin_Scene();
	static void End_Scene(bool flip_frame = true);

	// Flip until the primary buffer is visible.
	static void Flip_To_Primary();

	static void Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha=0.0f, float z=1.0f, unsigned int stencil=0);

	// Ronin @feature 13/09/2026 DX9: SSAO step 1. The main pass's depth-stencil as a READABLE texture (INTZ). Begin swaps
	// it in for the device's own depth buffer and clears it; End puts the original back.
	static bool Is_Scene_Depth_Supported();
	static bool Begin_Scene_Depth();
	static void End_Scene_Depth();
	static void Release_Scene_Depth();
	// Ronin @feature 13/09/2026 DX9: SSAO step 3a. Mid-frame read: Suspend takes ours off the device (no depth-stencil) so a
	// pass can sample it; Resume puts it back. Always pair them.
	static bool Suspend_Scene_Depth();
	static void Resume_Scene_Depth();
	// NULL while bound or not written this frame — reading a bound depth-stencil is undefined.
	static IDirect3DTexture9* Peek_Scene_Depth_Texture() { return (SceneDepthWritten && (SceneDepthSaved == nullptr || SceneDepthSuspended)) ? SceneDepthTexture : nullptr; }
	// Ronin @feature 06/10/2026 DX9: SSAO under MSAA. The same INTZ, filled by a depth prepass instead of swapped into the
	// frame: Begin binds it with a dummy colour target, Freeze stops depth writes, End puts the frame's pair back.
	static bool Begin_Scene_Depth_Prepass();
	static void Freeze_Scene_Depth_Prepass();
	static void End_Scene_Depth_Prepass();
	static int  Get_Scene_Depth_Prepass_Target() { return ScenePrepassTarget; }

	static void	Set_Viewport(CONST D3DVIEWPORT8* pViewport);

	static void Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream=0);
	static void Set_Vertex_Buffer(const DynamicVBAccessClass& vba);
	static void Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset);
	static void Set_Index_Buffer(const DynamicIBAccessClass& iba,unsigned short index_base_offset);
	static void Set_Index_Buffer_Index_Offset(unsigned offset);

	// Ronin @Build 07/01/2026 DX9: Introduce BaseVertexIndex
	static void Set_Base_Vertex_Index(unsigned offset);

	static void Get_Render_State(RenderStateStruct& state);
	static void Set_Render_State(const RenderStateStruct& state);
	static void Release_Render_State();

	static void Set_DX8_Material(const D3DMATERIAL9* mat);

	static void Set_Gamma(float gamma,float bright,float contrast,bool calibrate=true,bool uselimit=true);

	// Set_ and Get_Transform() functions take the matrix in Westwood convention format.

	static void Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix,float znear, float zfar);	// pointer to 16 matrices

	static void Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix4x4& m);
	static void Set_Transform(D3DTRANSFORMSTATETYPE transform,const Matrix3D& m);
	static void Get_Transform(D3DTRANSFORMSTATETYPE transform, Matrix4x4& m);
	static void Set_World_Identity();
	static void Set_View_Identity();
	static bool Is_World_Identity();
	static bool Is_View_Identity();

	// Note that *_DX8_Transform() functions take the matrix in DX8 format - transposed from Westwood convention.

	static void _Set_DX8_Transform(D3DTRANSFORMSTATETYPE transform, const D3DMATRIX& m);
	static void _Get_DX8_Transform(D3DTRANSFORMSTATETYPE transform, D3DMATRIX& m);

	static void Set_DX8_Light(int index,D3DLIGHT9* light);
	static void Set_DX8_Render_State(D3DRENDERSTATETYPE state, unsigned value);
	static void Set_DX8_ZBias(unsigned int zbias);
	static void Set_DX8_Clip_Plane(DWORD Index, CONST float* pPlane);
	static void Set_DX8_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state, unsigned value);


	// @build Ronin 29/10/2025 DX9: 
	static void Set_DX8_Sampler_State(unsigned stage, D3DSAMPLERSTATETYPE state, unsigned value);
	static void Reset_Pass_Render_States();

	static void Set_FVF(unsigned fvf);

	static void Set_DX8_Texture(unsigned int stage, IDirect3DBaseTexture8* texture);
	static void Set_Light_Environment(LightEnvironmentClass* light_env);
	static LightEnvironmentClass* Get_Light_Environment() { return Light_Environment; }
	static void Set_Fog(bool enable, const Vector3 &color, float start, float end);

	// Deferred

	static void Set_Shader(const ShaderClass& shader);
	static void Get_Shader(ShaderClass& shader);
	static void Set_Texture(unsigned stage,TextureBaseClass* texture);
	static void Set_Material(const VertexMaterialClass* material);
	static void Set_Light(unsigned index,const D3DLIGHT9* light);
	static void Set_Light(unsigned index,const LightClass &light);

	static void Apply_Render_State_Changes();	// Apply deferred render state changes (will be called automatically by Draw...)

	static void Draw_Triangles(
		unsigned buffer_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	static void Draw_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	static void Draw_Strip(
		unsigned short start_index,
		unsigned short index_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

	/*
	** Resources
	*/

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

	static unsigned _Get_Main_Thread_ID() { return _MainThreadID; }
	static const D3DADAPTER_IDENTIFIER9& Get_Current_Adapter_Identifier() { return CurrentAdapterIdentifier; }

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
	static D3DCOLOR				Get_Fog_Color() { return FogColor; }

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
	**	DX8Wrapper::Set_Render_Target (swap_chain_ptr);
	**
	**	WW3D::Begin_Render (true, true, Vector3 (0, 0, 0));
	**	WW3D::Render (scene, camera, FALSE, FALSE);
	**	WW3D::End_Render ();
	**
	**	swap_chain_ptr->Present (nullptr, nullptr, nullptr, nullptr);
	**
	**	DX8Wrapper::Set_Render_Target ((IDirect3DSurface8 *)nullptr);
	**
	*/
	static IDirect3DSwapChain8 *	Create_Additional_Swap_Chain (HWND render_window);

	/*
	** Render target interface. If render target format is WW3D_FORMAT_UNKNOWN, current display format is used.
	*/
	static TextureClass *	Create_Render_Target (int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN);

	static void					Set_Render_Target (IDirect3DSurface8 *render_target, bool use_default_depth_buffer = false);
	static void					Set_Render_Target (IDirect3DSurface8* render_target, IDirect3DSurface8* dpeth_buffer);

	static void					Set_Render_Target (IDirect3DSwapChain8 *swap_chain);
	static bool					Is_Render_To_Texture() { return IsRenderToTexture; }

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
	static void					Set_Render_Target_With_Z (TextureClass * texture, ZTextureClass* ztexture=nullptr);

	static void Set_Shadow_Map(int idx, ZTextureClass* ztex) { Shadow_Map[idx]=ztex; }
	static ZTextureClass* Get_Shadow_Map(int idx) { return Shadow_Map[idx]; }
	// for depth map support KJM ^

	// shader system updates KJM v
	static void Apply_Default_State();

	static void Set_Vertex_Shader(DWORD vertex_shader);
	static void Set_Pixel_Shader(DWORD pixel_shader);

	static void Set_Vertex_Shader_Constant(int reg, const void* data, int count);
	static void Set_Pixel_Shader_Constant(int reg, const void* data, int count);

	static DWORD Get_Vertex_Processing_Behavior() { return Vertex_Processing_Behavior; }

	// Needed by scene lighting class
	static void						Set_Ambient(const Vector3& color);
	static const Vector3&		Get_Ambient() { return Ambient_Color; }
	// shader system updates KJM ^




	static IDirect3DDevice9* _Get_D3D_Device8() { return D3DDevice; }
	static IDirect3D9* _Get_D3D8() { return D3DInterface; }

	/// Returns the display format - added by TR for video playback - not part of W3D
	static WW3DFormat	getBackBufferFormat();
	static bool Reset_Device(bool reload_assets=true);

	static const DX8Caps*	Get_Current_Caps() { WWASSERT(CurrentCaps); return CurrentCaps; }

	static bool Registry_Save_Render_Device( const char * sub_key );
	static bool Registry_Load_Render_Device( const char * sub_key, bool resize_window );

	static const char* Get_DX8_Render_State_Name(D3DRENDERSTATETYPE state);
	static const char* Get_DX8_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state);
	static unsigned Get_DX8_Render_State(D3DRENDERSTATETYPE state) { return RenderStates[state]; }

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

	static void Invalidate_Cached_Render_States();

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

	static void	Set_Swap_Interval(int swap);
	static int	Get_Swap_Interval();
	static void Set_Polygon_Mode(int mode);

	/*
	** Internal functions
	*/
	static void Resize_And_Position_Window();
	static bool Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,D3DFORMAT * set_colorbuffer,D3DFORMAT * set_backbuffer, D3DFORMAT * set_zmode);
	static bool Find_Color_Mode(D3DFORMAT colorbuffer, int resx, int resy, UINT *mode);
	static bool Find_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT *zmode);
	static bool Test_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT zmode);
	static void Compute_Caps(WW3DFormat display_format);

	/*
	** Protected Member Variables
	*/

	static DX8_CleanupHook *m_pCleanupHook;

	static RenderStateStruct			render_state;
	static unsigned						render_state_changed;
	static D3DMATRIX						DX8Transforms[D3DTS_WORLD+1];

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
	static D3DFORMAT					DisplayFormat;
	static int								AntiAliasingLevel; // Ronin @feature 10/03/2026 DX9: Track current anti-aliasing level for use in device creation and state management.


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
	static unsigned						RenderStates[256];
	static unsigned						TextureStageStates[MAX_TEXTURE_STAGES][33];
	static unsigned						SamplerStates[MAX_TEXTURE_STAGES][14];  // @build Ronin 29/10/2025 DX9: Sampler states added to wrapper tracking	static IDirect3DBaseTexture8 *	Textures[MAX_TEXTURE_STAGES];
	static IDirect3DBaseTexture8* Textures[MAX_TEXTURE_STAGES];

	// These fog settings are constant for all objects in a given scene,
	// unlike the matching renderstates which vary based on shader settings.
	static bool								FogEnable;
	static D3DCOLOR						FogColor;

	static DX8FrameStatistics			FrameStatistics;
	static bool								CurrentDX8LightEnables[MAX_LIGHTS];

	static DX8Caps*						CurrentCaps;

	static D3DADAPTER_IDENTIFIER9		CurrentAdapterIdentifier;

	static IDirect3D9*					D3DInterface;			//d3d8;
	static IDirect3DDevice9*			D3DDevice;				//d3ddevice8;

	static IDirect3DSurface8 *			CurrentRenderTarget;
	static IDirect3DSurface8 *			CurrentDepthBuffer;
	static IDirect3DSurface8 *			DefaultRenderTarget;
	static IDirect3DSurface8 *			DefaultDepthBuffer;
	// Ronin @feature 13/09/2026 DX9: SSAO step 1. Scene depth as an INTZ texture — see Begin_Scene_Depth.
	static IDirect3DTexture9 *			SceneDepthTexture;
	static IDirect3DSurface9 *			SceneDepthSurface;
	static IDirect3DSurface9 *			SceneDepthSaved;		// the device's own depth buffer while ours is bound
	static int								SceneDepthSupport;		// -1 not probed, 0 no, 1 yes
	static bool								SceneDepthWritten;
	static bool								SceneDepthSuspended;	// ours is off the device mid-frame so a pass can read it
	// Ronin @feature 06/10/2026 DX9: SSAO under MSAA. The prepass's dummy colour target, the frame's pair it displaced,
	// and the frame's depth while the AO pass has none bound. ScenePrepassTarget: 0 none, 1 NULL format, 2 A8R8G8B8.
	static bool								Ensure_Scene_Depth(unsigned width, unsigned height);
	static bool								Is_Null_Target_Supported();
	static IDirect3DSurface9 *			ScenePrepassColor;
	static IDirect3DSurface9 *			ScenePrepassSavedColor;
	static IDirect3DSurface9 *			ScenePrepassSavedDepth;
	static IDirect3DSurface9 *			SceneDepthHeld;
	static int								ScenePrepassTarget;
	static int								NullTargetSupport;		// -1 not probed, 0 no, 1 yes
	static bool								SceneDepthFromPrepass;	// this frame's INTZ was filled by the prepass, not swapped in

	static unsigned							DrawPolygonLowBoundLimit;

	static bool								IsRenderToTexture;

	static int								ZBias;
	static float							ZNear;
	static float							ZFar;
	static D3DMATRIX					ProjectionMatrix;

	friend void DX8_Assert();
	friend class WW3D;
	friend class Renderer;
	friend class DX8IndexBufferClass;
	friend class DX8VertexBufferClass;
};

inline DWORD FloatToDword(float f) {
	return *reinterpret_cast<DWORD*>(&f);
}

WWINLINE void DX8Wrapper::Set_FVF(unsigned fvf)
{
	// Ronin @bugfix 05/12/2025: Fixed pipeline switching

	DX8_THREAD_ASSERT();

#ifdef _DEBUG
	//ASSERT_LAYOUT_BINDING_ALLOWED_API("SetFVF");
#endif

	IDirect3DDevice9* pDev = _Get_D3D_Device8();
	if (!pDev || fvf == 0) return;

	// Ronin @refactor 12/03/2026: I think clearing it inside Apply_Render_State_Changes
	// when we detect a pipeline switch, is cleaner and works. So I will keep it commented out.
	// Ronin @bugfix 08/12/2025: Testing "Clear vertex declaration before setting FVF"
	// FVF and explicit declarations are mutually exclusive per Microsoft DX9 docs
	/*IDirect3DVertexDeclaration9* currentDecl = nullptr;
	pDev->GetVertexDeclaration(&currentDecl);
	if (currentDecl) {
		pDev->SetVertexDeclaration(nullptr);
		currentDecl->Release();
		DX8_RECORD_DX8_CALLS();
#ifdef _DEBUG
		WWDEBUG_SAY(("Wrapper: cleared decl=%p before setting FVF=0x%08X", currentDecl, fvf));
#endif
	}*/

	// Step 1: Clear vertex shader when switching to fixed-function pipeline
// Per Microsoft: "call SetVertexShader(NULL)" before SetFVF
	IDirect3DVertexShader9* currentVS = nullptr;
	pDev->GetVertexShader(&currentVS);
	if (currentVS) {
		pDev->SetVertexShader(nullptr);
		currentVS->Release();
		DX8_RECORD_DX8_CALLS();
	}
	
	//We don't clear pixel shader cause many FVF paths use one. DX9 allows FVF and pixel shader to co-exist.

	// Step 2: Now set the FVF
	HRESULT hr = pDev->SetFVF(fvf);
	if (FAILED(hr)) {
		WWDEBUG_SAY(("❌ SetFVF(0x%08X) FAILED: 0x%08X", fvf, hr));
		return;
	}
	DX8_RECORD_DX8_CALLS();

	// Update tracking
	//render_state.currentDecl = nullptr;
	render_state.currentFVF = fvf;
	render_state.currentVS = nullptr;
	render_state.layoutOwner = "Set_FVF";

	render_state.expectedFVF = fvf;
	if (render_state.expectedDecl) {
		render_state.expectedDecl->Release();
		render_state.expectedDecl = nullptr;
	}

#ifdef _DEBUG
	// Verify it was actually set
	DWORD actualFVF = 0;
	pDev->GetFVF(&actualFVF);
	if (actualFVF != fvf) {
		WWDEBUG_SAY(("❌ Set_FVF MISMATCH: Expected 0x%08X, Got 0x%08X", fvf, actualFVF));
	}
	else {
		static unsigned long lastLogFrame = 0;
		if (FrameCount != lastLogFrame) {
			lastLogFrame = FrameCount;
			WWDEBUG_SAY(("✅ Set_FVF: 0x%08X (verified, VS/Decl cleared)", fvf));
		}
	}
#endif
}

// shader system updates KJM v
WWINLINE void DX8Wrapper::Set_Vertex_Shader(DWORD vertex_shader)
{
#if 0 //(gth) some code is bypassing this accessor function so we can't count on this variable...
	// may be incorrect if shaders are created and destroyed dynamically
	if (Vertex_Shader==vertex_shader) return;
#endif

	Vertex_Shader=vertex_shader;

	// DEBUG: Log what's being passed
#ifdef _DEBUG
	//WWDEBUG_SAY(("Set_Vertex_Shader called with: 0x%08X", vertex_shader));
#endif

	// Ronin @build 27/10/2025 DX9: Cast DWORD (FVF code or shader handle) to IDirect3DVertexShader9*
	// DX9 accepts both FVF codes and programmable shader pointers in the same parameter
	//DX8CALL(SetVertexShader((IDirect3DVertexShader9*)(DWORD_PTR)Vertex_Shader));

	IDirect3DDevice9* pDev = _Get_D3D_Device8();
	if (!pDev) return;

	if (vertex_shader == 0)
	{
		// DX8: SetVertexShader(0) = disable shader, revert to fixed-function
		// DX9: Just disable the programmable shader, don't touch FVF
		pDev->SetVertexShader(nullptr);
		DX8_RECORD_DX8_CALLS();
		return;
	}

	// Assume it's an FVF code
	pDev->SetFVF(vertex_shader);
	DX8_RECORD_DX8_CALLS();
}

WWINLINE void DX8Wrapper::Set_Pixel_Shader(DWORD pixel_shader)
{
	// may be incorrect if shaders are created and destroyed dynamically
	if (Pixel_Shader==pixel_shader) return;

	Pixel_Shader=pixel_shader;
	// Ronin @build 27/10/2025 DX9: Cast DWORD handle to IDirect3DPixelShader9*
	// DX9 treats NULL or small values as "no shader", otherwise uses as shader pointer
	DX8CALL(SetPixelShader((IDirect3DPixelShader9*)(DWORD_PTR)Pixel_Shader));
}

// Ronin @build 24/10/2025 DX9: SetVertexShaderConstant -> SetVertexShaderConstantF
WWINLINE void DX8Wrapper::Set_Vertex_Shader_Constant(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Vertex_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Vertex_Shader_Constants[reg],data,memsize);
	DX8CALL(SetVertexShaderConstantF(reg,(const float*)data,count));
}

// Ronin @build 24/10/2025 DX9: SetPixelShaderConstant -> SetPixelShaderConstantF
WWINLINE void DX8Wrapper::Set_Pixel_Shader_Constant(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Pixel_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Pixel_Shader_Constants[reg],data,memsize);
	DX8CALL(SetPixelShaderConstantF(reg,(const float*)data,count));
}
// shader system updates KJM ^

WWINLINE void DX8Wrapper::_Set_DX8_Transform(D3DTRANSFORMSTATETYPE transform, const D3DMATRIX& m)
{
	WWASSERT(transform<=D3DTS_WORLD);
#if 0 // (gth) this optimization is breaking generals because they set the transform behind our backs.
	if (mtx!=DX8Transforms[transform])
#endif
	{
		DX8Transforms[transform]=m;
		SNAPSHOT_SAY(("DX8 - SetTransform %d [%f,%f,%f,%f][%f,%f,%f,%f][%f,%f,%f,%f]",
			transform,
			m.m[0][0],m.m[0][1],m.m[0][2],m.m[0][3],
			m.m[1][0],m.m[1][1],m.m[1][2],m.m[1][3],
			m.m[2][0],m.m[2][1],m.m[2][2],m.m[2][3]));
		DX8_RECORD_MATRIX_CHANGE();
		DX8CALL(SetTransform(transform,&m));
	}
}

WWINLINE void DX8Wrapper::_Get_DX8_Transform(D3DTRANSFORMSTATETYPE transform, D3DMATRIX& m)
{
	DX8CALL(GetTransform(transform,&m));
}

// ----------------------------------------------------------------------------
//
// Set the index offset for the current index buffer
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_Index_Buffer_Index_Offset(unsigned offset)
{
	// @bugfix Ronin 06/01/2026 DX9: Keep BaseVertexIndex separate from index-buffer index offset.
	// In DX9, BaseVertexIndex is the 2nd parameter to DrawIndexedPrimitive.
	// Index offset should affect StartIndex, which we implement via `render_state.iba_offset`.
	if (render_state.iba_offset == (unsigned short)offset) return;

	render_state.iba_offset = (unsigned short)offset;
	render_state_changed |= INDEX_BUFFER_CHANGED;
}

WWINLINE void DX8Wrapper::Set_Base_Vertex_Index(unsigned offset)
{
	// Ronin @Build 07/01/2026 DX9: Terrain tracks need BaseVertexIndex to select correct slice in packed VB.
	if (render_state.index_base_offset == (unsigned short)offset) return;
	render_state.index_base_offset = (unsigned short)offset;
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
// Set the fog settings. This function should be used, rather than setting the
// appropriate renderstates directly, because the shader sets some of the
// renderstates on a per-mesh / per-pass basis depending on global fog states
// (stored in the wrapper) as well as the shader settings.
// This function should be called rarely - once per scene would be appropriate.
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_Fog(bool enable, const Vector3 &color, float start, float end)
{
	// Set global states
	FogEnable = enable;
	FogColor = Convert_Color(color,0.0f);

	// Invalidate the current shader (since the renderstates set by the shader
	// depend on the global fog settings as well as the actual shader settings)
	ShaderClass::Invalidate();

	// Set renderstates which are not affected by the shader
	Set_DX8_Render_State(D3DRS_FOGSTART, std::bit_cast<DWORD>(start));
	Set_DX8_Render_State(D3DRS_FOGEND,   std::bit_cast<DWORD>(end));
}


WWINLINE void DX8Wrapper::Set_Ambient(const Vector3& color)
{
	Ambient_Color=color;
	Set_DX8_Render_State(D3DRS_AMBIENT, DX8Wrapper::Convert_Color(color,0.0f));
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer to be used in the subsequent render calls. If there was
// a vertex buffer being used earlier, release the reference to it. Passing
// nullptr just will release the vertex buffer.
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_DX8_Material(const D3DMATERIAL9* mat)
{
	DX8_RECORD_MATERIAL_CHANGE();
	WWASSERT(mat);
	SNAPSHOT_SAY(("DX8 - SetMaterial"));
	DX8CALL(SetMaterial(mat));
}

WWINLINE void DX8Wrapper::Set_DX8_Light(int index, D3DLIGHT9* light)
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

WWINLINE void DX8Wrapper::Set_DX8_Render_State(D3DRENDERSTATETYPE state, unsigned value)
{
	// Can't monitor state changes because setShader call to GERD may change the states!
		// Ronin @bugfix 08/11/2025: DX9 cache comparison broken - bypass cache temporarily
	// TODO: Investigate why cache comparison fails in DX9
	// Can't monitor state changes because setShader call to GERD may change the states!
	// if (RenderStates[state]==value) return;  // DISABLED - cache check broken in DX9

	 // Ronin @debug - Confirm cache bypass is active
	static bool first_call = true;
	if (first_call) {
		WWDEBUG_SAY(("CACHE BYPASS ACTIVE - Set_DX8_Render_State modified!"));
		first_call = false;
	}

#ifdef DEBUG_LOGGING
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0, true);
		Get_DX8_Render_State_Value_Name(value_name, state, value);
		SNAPSHOT_SAY(("DX8 - SetRenderState(state: %s, value: %s)",
			Get_DX8_Render_State_Name(state),
			value_name.str()));
	}
#endif

	// Ronin @refactor 10/03/2026 DX9: Keep render-state cache write-through only for now.
	// Raw device SetRenderState callers still exist, so render-state early-outs remain unsafe.
	RenderStates[state] = value;
	DX8CALL(SetRenderState(state, value));
	DX8_RECORD_RENDER_STATE_CHANGE();

}

WWINLINE void DX8Wrapper::Set_DX8_ZBias(unsigned int zbias)
{
	// D3D8 D3DRS_ZBIAS accepted integer values 0-16.
	// D3D9 D3DRS_DEPTHBIAS accepts a float encoded as a DWORD.
	// Convert the legacy integer to a float depth bias value.
	float bias = zbias * (-0.000005f);
	Set_DX8_Render_State(D3DRS_DEPTHBIAS, *reinterpret_cast<unsigned*>(&bias));
}

WWINLINE void DX8Wrapper::Set_DX8_Clip_Plane(DWORD Index, CONST float* pPlane)
{
	DX8CALL(SetClipPlane( Index, pPlane ));
}

WWINLINE void DX8Wrapper::Set_DX8_Texture_Stage_State(unsigned stage, D3DTEXTURESTAGESTATETYPE state, unsigned value)
{
	if (stage >= MAX_TEXTURE_STAGES)
	{
		DX8CALL(SetTextureStageState(stage, state, value));
		DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
		return;
	}

	const unsigned stateIndex = (unsigned)state;
	if (stateIndex < 33) {
		if (TextureStageStates[stage][stateIndex] == value) {
			return;
		}
		TextureStageStates[stage][stateIndex] = value;
	}

#ifdef DEBUG_LOGGING
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0, true);
		Get_DX8_Texture_Stage_State_Value_Name(value_name, state, value);
		SNAPSHOT_SAY(("DX8 - SetTextureStageState(stage: %d, state: %s, value: %s)",
			stage,
			Get_DX8_Texture_Stage_State_Name(state),
			value_name.str()));
	}
#endif

	DX8CALL(SetTextureStageState(stage, state, value));
	DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
}


WWINLINE void DX8Wrapper::Set_DX8_Sampler_State(unsigned stage, D3DSAMPLERSTATETYPE state, unsigned value)
{
	if (stage >= MAX_TEXTURE_STAGES)
	{
		DX8CALL(SetSamplerState(stage, state, value));
		DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
		return;
	}

	const unsigned stateIndex = (unsigned)state;
	if (stateIndex < 14) {
		if (SamplerStates[stage][stateIndex] == value) {
			return;
		}
		SamplerStates[stage][stateIndex] = value;
	}

#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		SNAPSHOT_SAY(("DX8 - SetSamplerState(stage: %d, state: %d, value: %d)",
			stage, (int)state, value));
	}
#endif

	DX8CALL(SetSamplerState(stage, state, value));
	DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
}

WWINLINE void DX8Wrapper::Set_DX8_Texture(unsigned int stage, IDirect3DBaseTexture8* texture)
{
  	if (stage >= MAX_TEXTURE_STAGES)
  	{	DX8CALL(SetTexture(stage, texture));
  		return;
  	}

	if (Textures[stage]==texture) return;

	SNAPSHOT_SAY(("DX8 - SetTexture(%x) ",texture));

	if (Textures[stage]) Textures[stage]->Release();
	Textures[stage] = texture;
	if (Textures[stage]) Textures[stage]->AddRef();
	DX8CALL(SetTexture(stage, texture));
	DX8_RECORD_TEXTURE_CHANGE();
}

// Ronin @bugfix 25/01/2026 DX9: CopyRects removed in DX9, use StretchRect or UpdateSurface
WWINLINE void DX8Wrapper::_Copy_DX8_Rects(
	IDirect3DSurface8* pSourceSurface,
	CONST RECT* pSourceRectsArray,
	UINT cRects,
	IDirect3DSurface8* pDestinationSurface,
	CONST POINT* pDestPointsArray
)
{
	// DX9 removed CopyRects - use StretchRect for compatible surfaces or UpdateSurface for system->default
	if (!pSourceSurface || !pDestinationSurface) {
		return; // Nothing to do
	}

	// Get surface descriptions to determine compatibility
	D3DSURFACE_DESC srcDesc, dstDesc;
	pSourceSurface->GetDesc(&srcDesc);
	pDestinationSurface->GetDesc(&dstDesc);

	// If formats don't match or surfaces are in incompatible pools, use D3DXLoadSurfaceFromSurface
	bool needsConversion = (srcDesc.Format != dstDesc.Format) ||
	        (srcDesc.Pool == D3DPOOL_SYSTEMMEM && dstDesc.Pool == D3DPOOL_DEFAULT) ||
	           (srcDesc.Usage & D3DUSAGE_RENDERTARGET) != (dstDesc.Usage & D3DUSAGE_RENDERTARGET);

	if (cRects > 0 && pSourceRectsArray && pDestPointsArray) {
		// Copy each rect individually
		for (UINT i = 0; i < cRects; i++) {
			RECT destRect;
			destRect.left = pDestPointsArray[i].x;
			destRect.top = pDestPointsArray[i].y;
			destRect.right = destRect.left + (pSourceRectsArray[i].right - pSourceRectsArray[i].left);
			destRect.bottom = destRect.top + (pSourceRectsArray[i].bottom - pSourceRectsArray[i].top);

			if (needsConversion) {
				// Use D3DXLoadSurfaceFromSurface for format conversion or incompatible surfaces
				HRESULT hr = D3DXLoadSurfaceFromSurface(
					pDestinationSurface, NULL, &destRect,
					pSourceSurface, NULL, &pSourceRectsArray[i],
					D3DX_FILTER_NONE, 0);
				if (FAILED(hr)) {
					// Log but don't crash - this is non-critical for texture loading
					WWDEBUG_SAY(("D3DXLoadSurfaceFromSurface failed: 0x%08X", hr));
				}
			} else {
				// Try StretchRect for compatible surfaces
				HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->StretchRect(
					pSourceSurface, &pSourceRectsArray[i],
					pDestinationSurface, &destRect,
					D3DTEXF_NONE);
				if (FAILED(hr)) {
					// Fallback to D3DXLoadSurfaceFromSurface
					hr = D3DXLoadSurfaceFromSurface(
						pDestinationSurface, NULL, &destRect,
						pSourceSurface, NULL, &pSourceRectsArray[i],
						D3DX_FILTER_NONE, 0);
					if (FAILED(hr)) {
						WWDEBUG_SAY(("Both StretchRect and D3DXLoadSurfaceFromSurface failed: 0x%08X", hr));
					}
				}
			}
		}
	} else if (cRects == 0 || (!pSourceRectsArray && !pDestPointsArray)) {
		// Full surface copy
		if (needsConversion) {
			HRESULT hr = D3DXLoadSurfaceFromSurface(
				pDestinationSurface, NULL, NULL,
				pSourceSurface, NULL, NULL,
				D3DX_FILTER_NONE, 0);
			if (FAILED(hr)) {
				WWDEBUG_SAY(("D3DXLoadSurfaceFromSurface (full) failed: 0x%08X", hr));
			}
		} else {
			HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->StretchRect(
				pSourceSurface, NULL,
				pDestinationSurface, NULL,
				D3DTEXF_NONE);
			if (FAILED(hr)) {
				// Fallback to D3DXLoadSurfaceFromSurface
				hr = D3DXLoadSurfaceFromSurface(
					pDestinationSurface, NULL, NULL,
					pSourceSurface, NULL, NULL,
					D3DX_FILTER_NONE, 0);
				if (FAILED(hr)) {
					WWDEBUG_SAY(("Both StretchRect and D3DXLoadSurfaceFromSurface (full) failed: 0x%08X", hr));
				}
			}
		}
	}
}

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
	state=render_state;
}

WWINLINE void DX8Wrapper::Get_Shader(ShaderClass& shader)
{
	shader=render_state.shader;
}

WWINLINE void DX8Wrapper::Set_Texture(unsigned stage,TextureBaseClass* texture)
{
	WWASSERT(stage < (unsigned int)CurrentCaps->Get_Max_Textures_Per_Pass());

	// Check if texture is changing
	if (texture == render_state.Textures[stage]) return;

	// Update render state (deferred binding)
	REF_PTR_SET(render_state.Textures[stage], texture);
	render_state_changed |= (TEXTURE0_CHANGED << stage);

}

WWINLINE void DX8Wrapper::Set_Material(const VertexMaterialClass* material)
{
/*	if (material && render_state.material &&
		// !stricmp(material->Get_Name(),render_state.material->Get_Name())) {
		material->Get_CRC()!=render_state.material->Get_CRC()) {
		return;
	}
*/
//	if (material==render_state.material) {
//		return;
//	}
	REF_PTR_SET(render_state.material,const_cast<VertexMaterialClass*>(material));
	render_state_changed|=MATERIAL_CHANGED;
	SNAPSHOT_SAY(("DX8Wrapper::Set_Material(%s)",material ? material->Get_Name() : "null"));
}

WWINLINE void DX8Wrapper::Set_Shader(const ShaderClass& shader)
{
	if (!ShaderClass::ShaderDirty && ((unsigned&)shader==(unsigned&)render_state.shader)) {
		return;
	}
	render_state.shader=shader;
	render_state_changed|=SHADER_CHANGED;

	SNAPSHOT_SAY(("DX8Wrapper::Set_Shader(%s)",shader.Get_Description().str()));
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
		render_state.world=To_D3DMATRIX(m);
		render_state_changed|=(unsigned)WORLD_CHANGED;
		render_state_changed&=~(unsigned)WORLD_IDENTITY;
		break;
	case D3DTS_VIEW:
		render_state.view=To_D3DMATRIX(m);
		render_state_changed|=(unsigned)VIEW_CHANGED;
		render_state_changed&=~(unsigned)VIEW_IDENTITY;
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
		render_state.world=To_D3DMATRIX(m);
		render_state_changed|=(unsigned)WORLD_CHANGED;
		render_state_changed&=~(unsigned)WORLD_IDENTITY;
		break;
	case D3DTS_VIEW:
		render_state.view=To_D3DMATRIX(m);
		render_state_changed|=(unsigned)VIEW_CHANGED;
		render_state_changed&=~(unsigned)VIEW_IDENTITY;
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
	return !!(render_state_changed&(unsigned)WORLD_IDENTITY);
}

WWINLINE bool DX8Wrapper::Is_View_Identity()
{
	return !!(render_state_changed&(unsigned)VIEW_IDENTITY);
}

WWINLINE void DX8Wrapper::Get_Transform(D3DTRANSFORMSTATETYPE transform, Matrix4x4& m)
{
	switch ((int)transform) {
	case D3DTS_WORLD:
		if (render_state_changed&WORLD_IDENTITY) m.Make_Identity();
		else m=To_Matrix4x4(render_state.world);
		break;
	case D3DTS_VIEW:
		if (render_state_changed&VIEW_IDENTITY) m.Make_Identity();
		else m=To_Matrix4x4(render_state.view);
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
	int i;

	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i)
	{
		if (render_state.vertex_buffers[i])
		{
			render_state.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	render_state=state;
	render_state_changed=0xffffffff;

	if (render_state.index_buffer) {
		render_state.index_buffer->Add_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i)
	{
		if (render_state.vertex_buffers[i])
		{
			render_state.vertex_buffers[i]->Add_Engine_Ref();
		}
	}
}

WWINLINE void DX8Wrapper::Release_Render_State()
{
	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}

	for (unsigned s = 0; s < MAX_VERTEX_STREAMS; ++s) {
		if (render_state.vertex_buffers[s]) {
			render_state.vertex_buffers[s]->Release_Engine_Ref();
		}
		REF_PTR_RELEASE(render_state.vertex_buffers[s]);
	}
	REF_PTR_RELEASE(render_state.index_buffer);
	REF_PTR_RELEASE(render_state.material);
	for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i) REF_PTR_RELEASE(render_state.Textures[i]);

	// @bugfix Ronin 27/02/2026 DX9: Release COM refs on DX9-specific IA tracking members
	if (render_state.vba_d3d_vb) {
		render_state.vba_d3d_vb->Release();
		render_state.vba_d3d_vb = nullptr;
	}
	if (render_state.expectedDecl) {
		render_state.expectedDecl->Release();
		render_state.expectedDecl = nullptr;
	}

	render_state.vba_fvf = 0;
	render_state.currentFVF = 0;
	render_state.currentDecl = nullptr;
	render_state.currentVS = nullptr;
	render_state.currentPS = nullptr;
	render_state.expectedFVF = 0;
	render_state.layoutOwner = "none";
}


WWINLINE RenderStateStruct::RenderStateStruct()
	:
	material(0),
	index_buffer(0),
	vba_fvf(0), // Ronin @bugfix 15/01/2026 DX9: Initialize DynamicVBAccess FVF tracking
	vba_d3d_vb(nullptr), // Ronin @bugfix 26/01/2026 DX9: Initialize dynamic D3D VB tracking.
	expectedFVF(0),
	expectedDecl(nullptr),
	currentVS(nullptr),
	currentPS(nullptr), 
	currentFVF(0), 
	currentDecl(nullptr),
	layoutOwner("none")
{
	unsigned i;
	for (i = 0; i < MAX_VERTEX_STREAMS; ++i) vertex_buffers[i] = 0;
	for (i = 0; i < MAX_TEXTURE_STAGES; ++i) Textures[i] = 0;
	for (i = 0; i < MAX_LIGHTS; ++i) LightEnable[i] = false;
}

WWINLINE RenderStateStruct::~RenderStateStruct()
{
	unsigned i;
	REF_PTR_RELEASE(material);
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(vertex_buffers[i]);
	}
	REF_PTR_RELEASE(index_buffer);

	for (i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_RELEASE(Textures[i]);
	}

	if (expectedDecl) expectedDecl->Release();
	expectedDecl = nullptr;

	// Ronin @bugfix 26/01/2026 DX9: Release dynamic D3D VB tracking.
	// Ronin @bugfix 27/02/2026 DX9: Release COM refs on DX9-specific IA tracking members
	if (vba_d3d_vb) { vba_d3d_vb->Release(); vba_d3d_vb = nullptr; }
	if (expectedDecl) { expectedDecl->Release(); expectedDecl = nullptr; }
}


WWINLINE RenderStateStruct& RenderStateStruct::operator= (const RenderStateStruct& src)
{
	unsigned i;
	REF_PTR_SET(material,src.material);
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_SET(vertex_buffers[i],src.vertex_buffers[i]);
	}
	REF_PTR_SET(index_buffer,src.index_buffer);

	for (i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_SET(Textures[i],src.Textures[i]);
	}

	for (i = 0;i<MAX_LIGHTS;++i)
	{
		LightEnable[i] = src.LightEnable[i];
		if (LightEnable[i]) { Lights[i] = src.Lights[i]; }
	}

	shader=src.shader;
	world=src.world;
	view=src.view;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		vertex_buffer_types[i]=src.vertex_buffer_types[i];
	}
	index_buffer_type=src.index_buffer_type;
	vba_offset=src.vba_offset;
	vba_count=src.vba_count;
	iba_offset=src.iba_offset;
	index_base_offset=src.index_base_offset;

	// Ronin @bugfix 15/01/2026 DX9: Carry DynamicVBAccess FVF tracking through state copies
	vba_fvf = src.vba_fvf;

	currentFVF = src.currentFVF;  
	currentDecl = src.currentDecl;
	currentVS = src.currentVS;
	currentPS = src.currentPS;
	layoutOwner = src.layoutOwner;

	// Copy expected layout (with AddRef)
	expectedFVF = src.expectedFVF;
	if (expectedDecl) {
		expectedDecl->Release();
	}
	expectedDecl = src.expectedDecl;
	if (expectedDecl) {
		expectedDecl->AddRef();
	}

	// Ronin @bugfix 26/01/2026 DX9: Copy dynamic D3D VB tracking safely.
	if (vba_d3d_vb) {
		vba_d3d_vb->Release();
		vba_d3d_vb = nullptr;
	}
	vba_d3d_vb = src.vba_d3d_vb;
	if (vba_d3d_vb) {
		vba_d3d_vb->AddRef();
	}


	return *this;
}
