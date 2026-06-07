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
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8wrapper.cpp                         $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 170                                                         $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Render to shadow buffer texture support														*
 * 06/27/02 KM Shader system updates																				*
 * 08/05/02 KM Texture class redesign
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   DX8Wrapper::_Update_Texture -- Copies a texture from system memory to video memory        *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

//#define CREATE_DX8_MULTI_THREADED
//#define CREATE_DX8_FPU_PRESERVE
#define WW3D_DEVTYPE Legacy_Device_Type(1)

#if !defined(WINVER) || WINVER < 0x0500
#undef WINVER
#define WINVER 0x0500 // Required to access GetMonitorInfo in VC6.
#endif

#include "dx8wrapper.h"
#include "texturecompatibilityinterop.h"
#include "RenderStateDefs.h"
#include "dx8webbrowser.h"
#include "dx8fvf.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "dx8renderer.h"
#include "RenderBackend.h"
#include "IRenderBackend.h"
#include "ww3d.h"
#include "camera.h"
#include "wwstring.h"
#include "matrix4.h"
#include "vertmaterial.h"
#include "rddesc.h"
#include "lightenvironment.h"
#include "statistics.h"
#include "registry.h"
#include "boxrobj.h"
#include "pointgr.h"
#include "render2d.h"
#include "sortingrenderer.h"
#include "shattersystem.h"
#include "light.h"
#include "assetmgr.h"
#include "textureloader.h"
#include "missingtexture.h"
#include "thread.h"
#if !defined(GGC_RENDER_BACKEND_BGFX)
#include <d3dx8core.h>
#endif
#include "pot.h"
#include "wwprofile.h"
#include "ffactory.h"
#include "dx8caps.h"
#include "dx8formatconv.h"
#include "TextureResourceManager.h"
#include "bound.h"
#include "DbgHelpGuard.h"

#include "shdlib.h"

#include <cstdio>
#include <cstring>

#if defined(GGC_RENDER_BACKEND_BGFX)
#include "TARGA.h"
#include "ww3dformat.h"

HRESULT Standalone_Filter_Legacy_Texture_Mips(IDirect3DBaseTexture8 *base_texture, unsigned int src_level);
HRESULT Standalone_Copy_Legacy_Surface(
	IDirect3DSurface8 *destination,
	const RECT *destination_rect,
	IDirect3DSurface8 *source,
	const RECT *source_rect);
#endif

static D3DDEVTYPE Legacy_Device_Type(unsigned value) { return static_cast<D3DDEVTYPE>(value); }
static D3DRESOURCETYPE Legacy_Resource_Type(unsigned value) { return static_cast<D3DRESOURCETYPE>(value); }

#if !defined(GGC_RENDER_BACKEND_BGFX)
static IDirect3DVertexBuffer8 *Legacy_Vertex_Buffer(VertexBufferClass *buffer)
{
	return static_cast<IDirect3DVertexBuffer8 *>(
		static_cast<DX8VertexBufferClass *>(buffer)->Get_Legacy_Vertex_Buffer());
}

static IDirect3DIndexBuffer8 *Legacy_Index_Buffer(IndexBufferClass *buffer)
{
	return static_cast<IDirect3DIndexBuffer8 *>(
		static_cast<DX8IndexBufferClass *>(buffer)->Get_Legacy_Index_Buffer());
}
#endif

static LegacyFixedFunctionColor To_Legacy_Color(const D3DCOLORVALUE &color)
{
	LegacyFixedFunctionColor result;
	result.r = color.r;
	result.g = color.g;
	result.b = color.b;
	result.a = color.a;
	return result;
}

static D3DCOLORVALUE To_D3D_Color(const LegacyFixedFunctionColor &color)
{
	D3DCOLORVALUE result;
	result.r = color.r;
	result.g = color.g;
	result.b = color.b;
	result.a = color.a;
	return result;
}

static LegacyFixedFunctionVector3 To_Legacy_Vector(const D3DVECTOR &vector)
{
	LegacyFixedFunctionVector3 result;
	result.x = vector.x;
	result.y = vector.y;
	result.z = vector.z;
	return result;
}

static D3DVECTOR To_D3D_Vector(const LegacyFixedFunctionVector3 &vector)
{
	D3DVECTOR result;
	result.x = vector.x;
	result.y = vector.y;
	result.z = vector.z;
	return result;
}

static LegacyFixedFunctionLight To_Legacy_Light(const D3DLIGHT8 &light)
{
	LegacyFixedFunctionLight result;
	result.Type = static_cast<unsigned int>(light.Type);
	result.Diffuse = To_Legacy_Color(light.Diffuse);
	result.Specular = To_Legacy_Color(light.Specular);
	result.Ambient = To_Legacy_Color(light.Ambient);
	result.Position = To_Legacy_Vector(light.Position);
	result.Direction = To_Legacy_Vector(light.Direction);
	result.Range = light.Range;
	result.Falloff = light.Falloff;
	result.Attenuation0 = light.Attenuation0;
	result.Attenuation1 = light.Attenuation1;
	result.Attenuation2 = light.Attenuation2;
	result.Theta = light.Theta;
	result.Phi = light.Phi;
	return result;
}

static D3DLIGHT8 To_D3D_Light(const LegacyFixedFunctionLight &light)
{
	D3DLIGHT8 result;
	result.Type = static_cast<D3DLIGHTTYPE>(light.Type);
	result.Diffuse = To_D3D_Color(light.Diffuse);
	result.Specular = To_D3D_Color(light.Specular);
	result.Ambient = To_D3D_Color(light.Ambient);
	result.Position = To_D3D_Vector(light.Position);
	result.Direction = To_D3D_Vector(light.Direction);
	result.Range = light.Range;
	result.Falloff = light.Falloff;
	result.Attenuation0 = light.Attenuation0;
	result.Attenuation1 = light.Attenuation1;
	result.Attenuation2 = light.Attenuation2;
	result.Theta = light.Theta;
	result.Phi = light.Phi;
	return result;
}

const int DEFAULT_RESOLUTION_WIDTH = 640;
const int DEFAULT_RESOLUTION_HEIGHT = 480;
const int DEFAULT_BIT_DEPTH = 32;
const int DEFAULT_TEXTURE_BIT_DEPTH = 16;
const unsigned DEFAULT_MSAA = 0;
const DWORD LEGACY_NO_WHQL_LEVEL = 0x00000002L;
const DWORD LEGACY_CAP_HW_TRANSFORM_AND_LIGHT = 0x00010000L;
const DWORD LEGACY_CREATE_FPU_PRESERVE = 0x00000002L;
const DWORD LEGACY_CREATE_MULTITHREADED = 0x00000004L;
const DWORD LEGACY_CREATE_SOFTWARE_VERTEXPROCESSING = 0x00000020L;
const DWORD LEGACY_CREATE_MIXED_VERTEXPROCESSING = 0x00000080L;

DX8FrameStatistics DX8Wrapper::FrameStatistics;
static DX8FrameStatistics LastFrameStatistics;

static void Log_Missing_Texture_File(const char *reason, const char *filename)
{
	char message[512];
	snprintf(
		message,
		sizeof(message),
		"Missing texture %s: %s\n",
		reason ? reason : "load failed",
		filename ? filename : "(null)");
	fprintf(stderr, "%s", message);
	fflush(stderr);
	OutputDebugString(message);
}

bool DX8Wrapper_IsWindowed = true;

// FPU_PRESERVE
int DX8Wrapper_PreserveFPU = 0;

/***********************************************************************************
**
** DX8Wrapper Static Variables
**
***********************************************************************************/

static HWND						_Hwnd															= nullptr;
bool								DX8Wrapper::IsInitted									= false;
bool								DX8Wrapper::_EnableTriangleDraw						= true;

int								DX8Wrapper::CurRenderDevice							= -1;
int								DX8Wrapper::ResolutionWidth							= DEFAULT_RESOLUTION_WIDTH;
int								DX8Wrapper::ResolutionHeight							= DEFAULT_RESOLUTION_HEIGHT;
int								DX8Wrapper::BitDepth										= DEFAULT_BIT_DEPTH;
int								DX8Wrapper::TextureBitDepth							= DEFAULT_TEXTURE_BIT_DEPTH;
bool								DX8Wrapper::IsWindowed									= false;
unsigned						DX8Wrapper::DisplayFormat	= 0;
unsigned						DX8Wrapper::MultiSampleAntiAliasing	= DEFAULT_MSAA;

// shader system additions KJM v
DWORD								DX8Wrapper::Vertex_Shader								= 0;
DWORD								DX8Wrapper::Pixel_Shader								= 0;

Vector4							DX8Wrapper::Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
Vector4							DX8Wrapper::Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

LightEnvironmentClass*		DX8Wrapper::Light_Environment							= nullptr;

DWORD								DX8Wrapper::Vertex_Processing_Behavior				= 0;
ZTextureClass*					DX8Wrapper::Shadow_Map[MAX_SHADOW_MAPS];

Vector3							DX8Wrapper::Ambient_Color;
// shader system additions KJM ^

bool								DX8Wrapper::world_identity;

bool								DX8Wrapper::FogEnable									= false;
unsigned							DX8Wrapper::FogColor										= 0;

static IDirect3D8 *			D3DInterface								= nullptr;
static IDirect3DDevice8 *	D3DDevice									= nullptr;

// TheSuperHackers @build bobtista 01/06/2026 Out-of-line getters for the
// file-static D3D8 device + interface pointers above. dx8wrapper.h forward-
// declares these; defining them inline there would expose the file-static
// pointers to every TU that includes the header, which fails to compile.
#if !defined(GGC_RENDER_BACKEND_BGFX)
IDirect3DDevice8 * DX8Wrapper::_Get_D3D_Device8() { return D3DDevice; }
IDirect3D8 * DX8Wrapper::_Get_D3D8() { return D3DInterface; }
#endif

static IDirect3DSurface8 *	CurrentRenderTarget						= nullptr;
static IDirect3DSurface8 *	CurrentDepthBuffer						= nullptr;
static IDirect3DSurface8 *	DefaultRenderTarget						= nullptr;
static IDirect3DSurface8 *	DefaultDepthBuffer						= nullptr;
bool								DX8Wrapper::IsRenderToTexture							= false;

unsigned							DX8Wrapper::_MainThreadID								= 0;
bool								DX8Wrapper::CurrentDX8LightEnables[4];
bool								DX8Wrapper::IsDeviceLost;
int								DX8Wrapper::ZBias;
float								DX8Wrapper::ZNear;
float								DX8Wrapper::ZFar;
#if !defined(GGC_RENDER_BACKEND_BGFX)
D3DMATRIX						DX8Wrapper::ProjectionMatrix;
#endif
DX8Caps*							DX8Wrapper::CurrentCaps = nullptr;

// Hack test... this disables rendering of batches of too few polygons.
unsigned							DX8Wrapper::DrawPolygonLowBoundLimit=0;

static D3DADAPTER_IDENTIFIER8 CurrentAdapterIdentifier;

unsigned long DX8Wrapper::FrameCount = 0;

bool								_DX8SingleThreaded										= false;

static D3DPRESENT_PARAMETERS								_PresentParameters;

#if defined(GGC_RENDER_BACKEND_BGFX)
static bool StandaloneDeviceCreated = false;

static void Fill_Standalone_DX8_Caps(D3DCAPS8 &caps)
{
	::ZeroMemory(&caps, sizeof(caps));
	caps.DeviceType = D3DDEVTYPE_HAL;
	caps.AdapterOrdinal = 0;
	caps.Caps = 0;
	caps.Caps2 = D3DCAPS2_CANRENDERWINDOWED | D3DCAPS2_DYNAMICTEXTURES | D3DCAPS2_FULLSCREENGAMMA | D3DCAPS2_CANCALIBRATEGAMMA;
	caps.Caps3 = 0;
	caps.PresentationIntervals = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_INTERVAL_IMMEDIATE | D3DPRESENT_INTERVAL_ONE;
	caps.CursorCaps = D3DCURSORCAPS_COLOR | D3DCURSORCAPS_LOWRES;
	caps.DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE | D3DDEVCAPS_DRAWPRIMTLVERTEX
		| D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY
		| D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY
		| D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY
		| D3DDEVCAPS_CANRENDERAFTERFLIP | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
		| D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX
		| D3DDEVCAPS_HWRASTERIZATION;
	caps.PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_LINEPATTERNREP
		| D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW
		| D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_CLIPTLVERTS
		| D3DPMISCCAPS_TSSARGTEMP | D3DPMISCCAPS_BLENDOP;
	caps.RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST
		| D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_FOGTABLE
		| D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZBIAS
		| D3DPRASTERCAPS_ANISOTROPY | D3DPRASTERCAPS_WFOG | D3DPRASTERCAPS_ZFOG
		| D3DPRASTERCAPS_COLORPERSPECTIVE;
	caps.ZCmpCaps = D3DPCMPCAPS_NEVER | D3DPCMPCAPS_LESS | D3DPCMPCAPS_EQUAL
		| D3DPCMPCAPS_LESSEQUAL | D3DPCMPCAPS_GREATER | D3DPCMPCAPS_NOTEQUAL
		| D3DPCMPCAPS_GREATEREQUAL | D3DPCMPCAPS_ALWAYS;
	caps.SrcBlendCaps = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE
		| D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_INVSRCCOLOR
		| D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_INVSRCALPHA
		| D3DPBLENDCAPS_DESTALPHA | D3DPBLENDCAPS_INVDESTALPHA
		| D3DPBLENDCAPS_DESTCOLOR | D3DPBLENDCAPS_INVDESTCOLOR
		| D3DPBLENDCAPS_SRCALPHASAT | D3DPBLENDCAPS_BOTHSRCALPHA
		| D3DPBLENDCAPS_BOTHINVSRCALPHA;
	caps.DestBlendCaps = caps.SrcBlendCaps;
	caps.AlphaCmpCaps = caps.ZCmpCaps;
	caps.ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_SPECULARGOURAUDRGB
		| D3DPSHADECAPS_ALPHAGOURAUDBLEND | D3DPSHADECAPS_FOGGOURAUD;
	caps.TextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA
		| D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE | D3DPTEXTURECAPS_ALPHAPALETTE
		| D3DPTEXTURECAPS_PROJECTED | D3DPTEXTURECAPS_CUBEMAP
		| D3DPTEXTURECAPS_VOLUMEMAP | D3DPTEXTURECAPS_MIPMAP
		| D3DPTEXTURECAPS_MIPVOLUMEMAP | D3DPTEXTURECAPS_MIPCUBEMAP;
	caps.TextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR
		| D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MIPFPOINT
		| D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MAGFPOINT
		| D3DPTFILTERCAPS_MAGFLINEAR | D3DPTFILTERCAPS_MAGFANISOTROPIC;
	caps.CubeTextureFilterCaps = caps.TextureFilterCaps;
	caps.VolumeTextureFilterCaps = caps.TextureFilterCaps;
	caps.TextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR
		| D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_BORDER
		| D3DPTADDRESSCAPS_INDEPENDENTUV | D3DPTADDRESSCAPS_MIRRORONCE;
	caps.VolumeTextureAddressCaps = caps.TextureAddressCaps;
	caps.LineCaps = D3DLINECAPS_TEXTURE | D3DLINECAPS_ZTEST | D3DLINECAPS_BLEND | D3DLINECAPS_ALPHACMP | D3DLINECAPS_FOG;
	caps.MaxTextureWidth = 4096;
	caps.MaxTextureHeight = 4096;
	caps.MaxVolumeExtent = 256;
	caps.MaxTextureRepeat = 8192;
	caps.MaxTextureAspectRatio = 0;
	caps.MaxAnisotropy = 16;
	caps.MaxVertexW = 1e10f;
	caps.GuardBandLeft = -32768.0f;
	caps.GuardBandTop = -32768.0f;
	caps.GuardBandRight = 32768.0f;
	caps.GuardBandBottom = 32768.0f;
	caps.ExtentsAdjust = 0.0f;
	caps.StencilCaps = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_ZERO | D3DSTENCILCAPS_REPLACE
		| D3DSTENCILCAPS_INCRSAT | D3DSTENCILCAPS_DECRSAT | D3DSTENCILCAPS_INVERT
		| D3DSTENCILCAPS_INCR | D3DSTENCILCAPS_DECR;
	caps.FVFCaps = 8 | D3DFVFCAPS_PSIZE;
	caps.TextureOpCaps = D3DTEXOPCAPS_DISABLE | D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_SELECTARG2
		| D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_MODULATE2X | D3DTEXOPCAPS_MODULATE4X
		| D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_ADDSIGNED2X
		| D3DTEXOPCAPS_SUBTRACT | D3DTEXOPCAPS_ADDSMOOTH
		| D3DTEXOPCAPS_BLENDDIFFUSEALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHA
		| D3DTEXOPCAPS_BLENDFACTORALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM
		| D3DTEXOPCAPS_BLENDCURRENTALPHA | D3DTEXOPCAPS_PREMODULATE
		| D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR | D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
		| D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR | D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA
		| D3DTEXOPCAPS_BUMPENVMAP | D3DTEXOPCAPS_BUMPENVMAPLUMINANCE
		| D3DTEXOPCAPS_DOTPRODUCT3 | D3DTEXOPCAPS_MULTIPLYADD | D3DTEXOPCAPS_LERP;
	caps.MaxTextureBlendStages = 8;
	caps.MaxSimultaneousTextures = 4;
	caps.VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7
		| D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS
		| D3DVTXPCAPS_LOCALVIEWER | D3DVTXPCAPS_TWEENING;
	caps.MaxActiveLights = 8;
	caps.MaxUserClipPlanes = 6;
	caps.MaxVertexBlendMatrices = 4;
	caps.MaxVertexBlendMatrixIndex = 0;
	caps.MaxPointSize = 256.0f;
	caps.MaxPrimitiveCount = 65535;
	caps.MaxVertexIndex = 65535;
	caps.MaxStreams = 8;
	caps.MaxStreamStride = 256;
	caps.VertexShaderVersion = D3DVS_VERSION(1, 1);
	caps.MaxVertexShaderConst = 256;
	caps.PixelShaderVersion = D3DPS_VERSION(1, 1);
	caps.MaxPixelShaderValue = 1.0f;
}

static void Fill_Standalone_Adapter_Identifier(D3DADAPTER_IDENTIFIER8 &identifier)
{
	::ZeroMemory(&identifier, sizeof(identifier));
	std::snprintf(identifier.Driver, sizeof(identifier.Driver), "%s", "bgfx");
	std::snprintf(identifier.Description, sizeof(identifier.Description), "%s", "Generals bgfx standalone");
#ifdef _WIN32
	identifier.DriverVersion.QuadPart = 0;
#else
	identifier.DriverVersionHighPart = 0;
	identifier.DriverVersionLowPart = 0;
#endif
}

#endif

template <typename T>
static T Legacy_Value(unsigned value)
{
	return static_cast<T>(value);
}

static auto Legacy_Format(unsigned value) { return Legacy_Value<decltype(_PresentParameters.BackBufferFormat)>(value); }
static auto Legacy_Multisample_Type(unsigned value) { return Legacy_Value<decltype(_PresentParameters.MultiSampleType)>(value); }
static auto Legacy_Swap_Effect(unsigned value) { return Legacy_Value<decltype(_PresentParameters.SwapEffect)>(value); }
static DynamicVectorClass<StringClass>					_RenderDeviceNameTable;
static DynamicVectorClass<StringClass>					_RenderDeviceShortNameTable;
static DynamicVectorClass<RenderDeviceDescClass>	_RenderDeviceDescriptionTable;

static HRESULT Copy_Legacy_Surface_Compat(
	IDirect3DSurface8 *destination,
	const RECT *destination_rect,
	IDirect3DSurface8 *source,
	const RECT *source_rect,
	unsigned int filter)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)filter;
	return Standalone_Copy_Legacy_Surface(destination, destination_rect, source, source_rect);
#else
	return D3DXLoadSurfaceFromSurface(
		destination,
		nullptr,
		destination_rect,
		source,
		nullptr,
		source_rect,
		filter,
		0);
#endif
}

static HRESULT Filter_Legacy_Texture_Mips_Compat(IDirect3DBaseTexture8 *base_texture, unsigned int src_level)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	return Standalone_Filter_Legacy_Texture_Mips(base_texture, src_level);
#else
	return D3DXFilterTexture(base_texture, nullptr, src_level, D3DX_FILTER_BOX);
#endif
}

IDirect3DDevice8* DX8_Call_Device()
{
	return D3DDevice;
}

IDirect3D8* DX8_Call_Interface()
{
	return D3DInterface;
}


typedef IDirect3D8* (WINAPI *Direct3DCreate8Type) (UINT SDKVersion);
Direct3DCreate8Type	Direct3DCreate8Ptr = nullptr;
HINSTANCE D3D8Lib = nullptr;

RenderDeviceCleanupHook *DX8Wrapper::m_pCleanupHook=nullptr;
#ifdef EXTENDED_STATS
RenderDebugStats &DX8Wrapper::stats = g_renderDebugStats;
#endif
/***********************************************************************************
**
** DX8Wrapper Implementation
**
***********************************************************************************/

static HRESULT Get_DX8_Error_String(unsigned res, char *buffer, size_t buffer_size)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	if (buffer == nullptr || buffer_size == 0)
	{
		return D3D_OK;
	}
	const char *message = nullptr;
	switch (res)
	{
	case D3D_OK:                  message = "D3D_OK"; break;
	case D3DERR_INVALIDCALL:      message = "D3DERR_INVALIDCALL"; break;
	case D3DERR_NOTAVAILABLE:     message = "D3DERR_NOTAVAILABLE"; break;
	case D3DERR_OUTOFVIDEOMEMORY: message = "D3DERR_OUTOFVIDEOMEMORY"; break;
	case E_OUTOFMEMORY:           message = "E_OUTOFMEMORY"; break;
	case E_NOTIMPL:               message = "E_NOTIMPL"; break;
	case E_FAIL:                  message = "E_FAIL"; break;
	default:                      message = "D3D unknown error"; break;
	}
	std::snprintf(buffer, buffer_size, "%s", message);
	return D3D_OK;
#else
	return D3DXGetErrorStringA(res, buffer, static_cast<UINT>(buffer_size));
#endif
}

void Log_DX8_ErrorCode(unsigned res)
{
	char tmp[256]="";

	HRESULT new_res=Get_DX8_Error_String(
		res,
		tmp,
		sizeof(tmp));

	if (new_res==S_OK) {
		WWDEBUG_SAY((tmp));
	}

	WWASSERT(0);
}

void Non_Fatal_Log_DX8_ErrorCode(unsigned res,const char * file,int line)
{
	char tmp[256]="";

	HRESULT new_res=Get_DX8_Error_String(
		res,
		tmp,
		sizeof(tmp));

	if (new_res==S_OK) {
		WWDEBUG_SAY(("DX8 Error: %s, File: %s, Line: %d",tmp,file,line));
	}
}

// TheSuperHackers @info helmutbuhler 14/04/2025
// Helper function that moves x and y such that the inner rect fits into the outer rect.
// If the inner rect already is in the outer rect, then this does nothing.
// If the inner rect is larger than the outer rect, then the inner rect will be aligned to the top left of the outer rect.
void MoveRectIntoOtherRect(const RECT& inner, const RECT& outer, int* x, int* y)
{
	int dx = 0;
	if (inner.right > outer.right)
		dx = outer.right-inner.right;
	if (inner.left < outer.left)
		dx = outer.left-inner.left;

	int dy = 0;
	if (inner.bottom > outer.bottom)
		dy = outer.bottom-inner.bottom;
	if (inner.top < outer.top)
		dy = outer.top-inner.top;

	*x += dx;
	*y += dy;
}


bool DX8Wrapper::Init(void * hwnd, bool lite)
{
	WWASSERT(!IsInitted);

	// zero memory
	RenderStateCache::Clear();
	memset(Vertex_Shader_Constants,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
	memset(Pixel_Shader_Constants,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
	FixedFunctionState::Clear_Raw();
	memset(Shadow_Map,0,sizeof(ZTextureClass*)*MAX_SHADOW_MAPS);

	/*
	** Initialize all variables!
	*/

	_Hwnd = (HWND)hwnd;
	_MainThreadID=ThreadClass::_Get_Current_Thread_ID();
	WWDEBUG_SAY(("DX8Wrapper main thread: 0x%x",_MainThreadID));
	CurRenderDevice = -1;
	ResolutionWidth = DEFAULT_RESOLUTION_WIDTH;
	ResolutionHeight = DEFAULT_RESOLUTION_HEIGHT;
	// Initialize Render2DClass Screen Resolution
	Render2DClass::Set_Screen_Resolution( RectClass( 0, 0, ResolutionWidth, ResolutionHeight ) );
	BitDepth = DEFAULT_BIT_DEPTH;
	IsWindowed = false;
	DX8Wrapper_IsWindowed = false;

	for (int light=0;light<4;++light) CurrentDX8LightEnables[light]=false;

	//old_vertex_shader; TODO
	//old_sr_shader;
	//current_shader;

	//world_identity;
	//CurrentFogColor;

	D3DInterface = nullptr;
	D3DDevice = nullptr;

	WWDEBUG_SAY(("Reset DX8Wrapper statistics"));
	Reset_Statistics();

	Invalidate_Cached_Render_States();

	if (!lite) {
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWDEBUG_SAY(("Using standalone bgfx device metadata"));
		IsInitted = true;
		Enumerate_Devices();
		WWDEBUG_SAY(("DX8Wrapper Init completed (standalone bgfx)"));
#else
		D3D8Lib = LoadLibrary("D3D8.DLL");

		if (D3D8Lib == nullptr) return false;	// Return false at this point if init failed

		Direct3DCreate8Ptr = (Direct3DCreate8Type) GetProcAddress(D3D8Lib, "Direct3DCreate8");
		if (Direct3DCreate8Ptr == nullptr) return false;

		/*
		** Create the D3D interface object
		*/
		WWDEBUG_SAY(("Create Direct3D8"));
		{
			// TheSuperHackers @bugfix xezon 13/06/2025 Front load the system dbghelp.dll to prevent
			// the graphics driver from potentially loading the old game dbghelp.dll and then crashing the game process.
			DbgHelpGuard dbgHelpGuard;

			D3DInterface = Direct3DCreate8Ptr(D3D_SDK_VERSION);		// TODO: handle failure cases...
		}
		if (D3DInterface == nullptr) {
			return(false);
		}
		IsInitted = true;

		/*
		** Enumerate the available devices
		*/
		WWDEBUG_SAY(("Enumerate devices"));
		Enumerate_Devices();
		WWDEBUG_SAY(("DX8Wrapper Init completed"));
#endif
	}

	return(true);
}

void DX8Wrapper::Shutdown()
{
	if (D3DDevice
#if defined(GGC_RENDER_BACKEND_BGFX)
		|| StandaloneDeviceCreated
#endif
	) {

#if !defined(GGC_RENDER_BACKEND_BGFX)
		Set_Render_Target ((IDirect3DSurface8 *)nullptr);
#endif
		Release_Device();
	}

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (D3DInterface) {
		D3DInterface->Release();
		D3DInterface=nullptr;

	}
#endif

	if (CurrentCaps)
	{
		FixedFunctionState::Release_Raw_Textures();
	}

	if (D3D8Lib) {
		FreeLibrary(D3D8Lib);
		D3D8Lib = nullptr;
	}

	_RenderDeviceNameTable.Clear();		 // note - Delete_All() resizes the vector, causing a reallocation.  Clear is better. jba.
	_RenderDeviceShortNameTable.Clear();
	_RenderDeviceDescriptionTable.Clear();

	DX8Caps::Shutdown();
	IsInitted = false;		// 010803 srj
}

void DX8Wrapper::Do_Onetime_Device_Dependent_Inits()
{
	/*
	** Set Global render states (some of which depend on caps)
	*/
	Compute_Caps(D3DFormat_To_WW3DFormat(Legacy_Format(DisplayFormat)));

	// TheSuperHackers @refactor bobtista 11/04/2026 Construct and
	// initialize the render backend BEFORE the engine
	// subsystem _Init() calls below. Several of them (notably
	// PointGroupClass::_Init and BoxRenderObjClass::Init) allocate
	// static index / vertex buffers and populate them via the Write
	// lock classes, whose destructors call g_renderBackend->Capture_*
	// to mirror the data into the bgfx caches. If the backend is not
	// fully initialized (g_bgfxInitialized == false) at that moment
	// those buffers are never captured and every later bgfx draw that
	// binds them misses the cache and silently skips submission -
	// which is why particles, lasers, tracers, and debris were
	// invisible in the bgfx popup even though the underlying W3D
	// effect modules ran. The d3d8 device and HWND are already valid
	// by the time this function is called, so it is safe to run the
	// backend's real Initialize() here, well before the _Init() calls.
	Init_Render_Backend();
	// bgfx and legacy DX8 both render to the real game HWND.
	g_renderBackend->Initialize(_Hwnd, ResolutionWidth, ResolutionHeight);

   /*
	** Initialize any other subsystems inside of WW3D
	*/
	MissingTexture::_Init();
	TextureFilterClass::_Init_Filters(
		(TextureFilterClass::TextureFilterMode)WW3D::Get_Texture_Filter(),
		(TextureFilterClass::AnisotropicFilterMode)WW3D::Get_Anisotropy_Level()
	);
	TheDX8MeshRenderer.Init();
	SHD_INIT;
	BoxRenderObjClass::Init();
	VertexMaterialClass::Init();
	PointGroupClass::_Init(); // This needs the VertexMaterialClass to be initted
	ShatterSystem::Init();
	TextureLoader::Init();

	Set_Default_Global_Render_States();
}

inline DWORD F2DW(float f) { return *((unsigned*)&f); }
void DX8Wrapper::Set_Default_Global_Render_States()
{
	DX8_THREAD_ASSERT();

	Commit_Fixed_Function_Render_Value(48, Get_Current_Caps()->Support_Range_Fog() ? TRUE : FALSE);
	Commit_Fixed_Function_Render_Value(35, 0);
	Commit_Fixed_Function_Render_Value(140, 3);
	Commit_Fixed_Function_Render_Value(146, 0);
	Commit_Fixed_Function_Render_Value(141, TRUE);
	Commit_Fixed_Function_Render_Value(47,0);
	Commit_Fixed_Function_Texture_Stage_Value(1, 22, F2DW(1.0f));
	Commit_Fixed_Function_Texture_Stage_Value(1, 23, F2DW(0.0f));
	Commit_Fixed_Function_Texture_Stage_Value(0, 7,F2DW(1.0f));
	Commit_Fixed_Function_Texture_Stage_Value(0, 8,F2DW(0.0f));
	Commit_Fixed_Function_Texture_Stage_Value(0, 9,F2DW(0.0f));
	Commit_Fixed_Function_Texture_Stage_Value(0, 10,F2DW(1.0f));

//	Commit_Fixed_Function_Render_Value(22, 1);
	// Set dither mode here?
}

void DX8Wrapper::Invalidate_Cached_Render_States()
{
	FixedFunctionState::Changed_Mask()=0;
	RenderStateCache::Invalidate();

#if !defined(GGC_RENDER_BACKEND_BGFX)
	int a;
	for (a=0;a<MAX_TEXTURE_STAGES;++a)
	{
		//Need to explicitly set texture to null, otherwise app will not be able to
		//set it to null because of redundant state checker. MW
		if (_Get_D3D_Device8())
			_Get_D3D_Device8()->SetTexture(a,nullptr);
	}
#endif
	FixedFunctionState::Release_Raw_Textures();

	ShaderClass::Invalidate();

	//Need to explicitly set render-state texture pointers to null. MW
	Release_Render_State();

}

void DX8Wrapper::Do_Onetime_Device_Dependent_Shutdowns()
{
	// TheSuperHackers @refactor bobtista 10/04/2026 Tear down the render
	// backend before the D3D device is released so any backend-owned
	// resources get released first.
	if (g_renderBackend != nullptr)
	{
		// Symmetric counterpart to the Initialize call in
		// Do_Onetime_Device_Dependent_Inits.
		g_renderBackend->Shutdown();
	}
	Shutdown_Render_Backend();

	/*
	** Shutdown ww3d systems
	*/
	FixedFunctionState::Release_Render_State();


	TextureLoader::Deinit();
	SortingRendererClass::Deinit();
	DynamicVBAccessClass::_Deinit();
	DynamicIBAccessClass::_Deinit();
	ShatterSystem::Shutdown();
	PointGroupClass::_Shutdown();
	VertexMaterialClass::Shutdown();
	BoxRenderObjClass::Shutdown();
	SHD_SHUTDOWN;
	TheDX8MeshRenderer.Shutdown();
	MissingTexture::_Deinit();

	delete CurrentCaps;
	CurrentCaps=nullptr;

}


bool DX8Wrapper::Create_Device()
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT(!StandaloneDeviceCreated);

	D3DCAPS8 caps;
	Fill_Standalone_DX8_Caps(caps);
	Fill_Standalone_Adapter_Identifier(CurrentAdapterIdentifier);

	Vertex_Processing_Behavior=(caps.DevCaps&LEGACY_CAP_HW_TRANSFORM_AND_LIGHT) ?
		LEGACY_CREATE_MIXED_VERTEXPROCESSING : LEGACY_CREATE_SOFTWARE_VERTEXPROCESSING;

#ifdef CREATE_DX8_MULTI_THREADED
	Vertex_Processing_Behavior|=LEGACY_CREATE_MULTITHREADED;
	_DX8SingleThreaded=false;
#else
	_DX8SingleThreaded=true;
#endif

	if (DX8Wrapper_PreserveFPU)
		Vertex_Processing_Behavior |= LEGACY_CREATE_FPU_PRESERVE;

#ifdef CREATE_DX8_FPU_PRESERVE
	Vertex_Processing_Behavior|=LEGACY_CREATE_FPU_PRESERVE;
#endif

	StandaloneDeviceCreated = true;
	Do_Onetime_Device_Dependent_Inits();
	return true;
#else
	WWASSERT(D3DDevice==nullptr);	// for now, once you've created a device, you're stuck with it!

	D3DCAPS8 caps;
	if
	(
		FAILED
		(
			D3DInterface->GetDeviceCaps
			(
				CurRenderDevice,
				WW3D_DEVTYPE,
				&caps
			)
		)
	)
	{
		return false;
	}

	::ZeroMemory(&CurrentAdapterIdentifier, sizeof(CurrentAdapterIdentifier));

	if
	(
		FAILED
		(
			D3DInterface->GetAdapterIdentifier
			(
				CurRenderDevice,
				LEGACY_NO_WHQL_LEVEL,
				&CurrentAdapterIdentifier
			)
			)
	)
	{
		return false;
	}

	Vertex_Processing_Behavior=(caps.DevCaps&LEGACY_CAP_HW_TRANSFORM_AND_LIGHT) ?
		LEGACY_CREATE_MIXED_VERTEXPROCESSING : LEGACY_CREATE_SOFTWARE_VERTEXPROCESSING;

	// enable this when all 'get' dx calls are removed KJM
	/*if (caps.DevCaps&LEGACY_CAP_PURE_DEVICE)
	{
		Vertex_Processing_Behavior|=LEGACY_CREATE_PUREDEVICE;
	}*/

#ifdef CREATE_DX8_MULTI_THREADED
	Vertex_Processing_Behavior|=LEGACY_CREATE_MULTITHREADED;
	_DX8SingleThreaded=false;
#else
	_DX8SingleThreaded=true;
#endif

	if (DX8Wrapper_PreserveFPU)
		Vertex_Processing_Behavior |= LEGACY_CREATE_FPU_PRESERVE;

#ifdef CREATE_DX8_FPU_PRESERVE
	Vertex_Processing_Behavior|=LEGACY_CREATE_FPU_PRESERVE;
#endif

	// TheSuperHackers @bugfix xezon 13/06/2025 Front load the system dbghelp.dll to prevent
	// the graphics driver from potentially loading the old game dbghelp.dll and then crashing the game process.
	DbgHelpGuard dbgHelpGuard;

	HRESULT hr=D3DInterface->CreateDevice
	(
		CurRenderDevice,
		WW3D_DEVTYPE,
		_Hwnd,
		Vertex_Processing_Behavior,
		&_PresentParameters,
		&D3DDevice
	);

	if (FAILED(hr))
	{
		// The device selection may fail because the device lied that it supports 32 bit zbuffer with 16 bit
		// display. This happens at least on Voodoo2.

		const unsigned backbuffer_format = static_cast<unsigned>(_PresentParameters.BackBufferFormat);
		const unsigned depth_format = static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat);
		if ((backbuffer_format==23 ||
			backbuffer_format==24 ||
			backbuffer_format==25) &&
			(depth_format==71 ||
			depth_format==75 ||
			depth_format==77))
		{
			_PresentParameters.AutoDepthStencilFormat=Legacy_Format(80);
			hr = D3DInterface->CreateDevice
			(
				CurRenderDevice,
				WW3D_DEVTYPE,
				_Hwnd,
				Vertex_Processing_Behavior,
				&_PresentParameters,
				&D3DDevice
			);

			if (FAILED(hr))
			{
				return false;
			}
        }
		else
		{
				return false;
		}
	}

	dbgHelpGuard.deactivate();

	/*
	** Initialize all subsystems
	*/
	Do_Onetime_Device_Dependent_Inits();
	return true;
#endif
}

bool DX8Wrapper::Reset_Device(bool reload_assets)
{
	WWDEBUG_SAY(("Resetting device."));
	DX8_THREAD_ASSERT();
	if ((IsInitted) && (D3DDevice != nullptr
#if defined(GGC_RENDER_BACKEND_BGFX)
		|| StandaloneDeviceCreated
#endif
		)) {
		// Release all non-MANAGED stuff
		WW3D::_Invalidate_Textures();

		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i)
		{
			Set_Vertex_Buffer (nullptr,i);
		}
		Set_Index_Buffer (nullptr, 0);
		if (m_pCleanupHook) {
			m_pCleanupHook->ReleaseResources();
		}
		DynamicVBAccessClass::_Deinit();
		DynamicIBAccessClass::_Deinit();
		TextureResourceManagerClass::Release_Textures();
		SHD_SHUTDOWN_SHADERS;

		// Reset frame count to reflect the flipping chain being reset by Reset()
		FrameCount = 0;

		memset(Vertex_Shader_Constants,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
		memset(Pixel_Shader_Constants,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);

#if !defined(GGC_RENDER_BACKEND_BGFX)
		HRESULT hr=_Get_D3D_Device8()->TestCooperativeLevel();
		if (hr != D3DERR_DEVICELOST )
		{	DX8CALL_HRES(Reset(&_PresentParameters),hr)
			if (hr != D3D_OK)
				return false;	//reset failed.
		}
		else
			return false;	//device is lost and can't be reset.
#endif

		if (reload_assets)
		{
			TextureResourceManagerClass::Recreate_Textures();
			if (m_pCleanupHook) {
				m_pCleanupHook->ReAcquireResources();
			}
		}
		Invalidate_Cached_Render_States();
		Set_Default_Global_Render_States();
		SHD_INIT_SHADERS;
		WWDEBUG_SAY(("Device reset completed"));
		return true;
	}
	WWDEBUG_SAY(("Device reset failed"));
	return false;
}

void DX8Wrapper::Release_Device()
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	if (StandaloneDeviceCreated) {
		FixedFunctionState::Release_Raw_Textures();

		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i)
		{
			if (FixedFunctionState::Render_State().vertex_buffers[i]) FixedFunctionState::Render_State().vertex_buffers[i]->Release_Engine_Ref();
			REF_PTR_RELEASE(FixedFunctionState::Render_State().vertex_buffers[i]);
		}
		if (FixedFunctionState::Render_State().index_buffer) FixedFunctionState::Render_State().index_buffer->Release_Engine_Ref();
		REF_PTR_RELEASE(FixedFunctionState::Render_State().index_buffer);

		Do_Onetime_Device_Dependent_Shutdowns();
		StandaloneDeviceCreated = false;
	}
#else
	if (D3DDevice) {

		for (int a=0;a<MAX_TEXTURE_STAGES;++a)
		{	//release references to any textures that were used in last rendering call
			DX8CALL(SetTexture(a,nullptr));
		}
		FixedFunctionState::Release_Raw_Textures();

		DX8CALL(SetStreamSource(0, nullptr, 0));	//release reference count on last rendered vertex buffer
		DX8CALL(SetIndices(nullptr,0));	//release reference count on last rendered index buffer


		/*
		** Release the current vertex and index buffers
		*/
		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i)
		{
			if (FixedFunctionState::Render_State().vertex_buffers[i]) FixedFunctionState::Render_State().vertex_buffers[i]->Release_Engine_Ref();
			REF_PTR_RELEASE(FixedFunctionState::Render_State().vertex_buffers[i]);
		}
		if (FixedFunctionState::Render_State().index_buffer) FixedFunctionState::Render_State().index_buffer->Release_Engine_Ref();
		REF_PTR_RELEASE(FixedFunctionState::Render_State().index_buffer);

		/*
		** Shutdown all subsystems
		*/
		Do_Onetime_Device_Dependent_Shutdowns();

		/*
		** Release the device
		*/

		D3DDevice->Release();
		D3DDevice=nullptr;
	}
#endif
}

void DX8Wrapper::Enumerate_Devices()
{
	DX8_Assert();

#if defined(GGC_RENDER_BACKEND_BGFX)
	D3DADAPTER_IDENTIFIER8 id;
	Fill_Standalone_Adapter_Identifier(id);

	RenderDeviceDescClass desc;
	desc.set_device_name(id.Description);
	desc.set_driver_name(id.Driver);
	desc.set_driver_version("0.0.0.0");
	desc.reset_resolution_list();
	desc.add_resolution(640, 480, 32);
	desc.add_resolution(800, 600, 32);
	desc.add_resolution(1024, 768, 32);
	desc.add_resolution(1280, 720, 32);
	desc.add_resolution(1280, 1024, 32);
	desc.add_resolution(1920, 1080, 32);

	StringClass device_name(id.Description, true);
	_RenderDeviceNameTable.Add(device_name);
	_RenderDeviceShortNameTable.Add(device_name);
	_RenderDeviceDescriptionTable.Add(desc);
#else
	int adapter_count = D3DInterface->GetAdapterCount();
	for (int adapter_index=0; adapter_index<adapter_count; adapter_index++) {

		D3DADAPTER_IDENTIFIER8 id;
		::ZeroMemory(&id, sizeof(id));
		HRESULT res = D3DInterface->GetAdapterIdentifier(adapter_index,LEGACY_NO_WHQL_LEVEL,&id);

		if (res == S_OK) {

			/*
			** Set up the render device description
			** TODO: Fill in more fields of the render device description?  (need some lookup tables)
			*/
			RenderDeviceDescClass desc;
			desc.set_device_name(id.Description);
			desc.set_driver_name(id.Driver);

			char buf[64];
#ifdef _WIN32
			sprintf(buf,"%d.%d.%d.%d", //"%04x.%04x.%04x.%04x",
				HIWORD(id.DriverVersion.HighPart),
				LOWORD(id.DriverVersion.HighPart),
				HIWORD(id.DriverVersion.LowPart),
				LOWORD(id.DriverVersion.LowPart));
#else
			sprintf(buf,"%d.%d.%d.%d", //"%04x.%04x.%04x.%04x",
				HIWORD(id.DriverVersionHighPart),
				LOWORD(id.DriverVersionHighPart),
				HIWORD(id.DriverVersionLowPart),
				LOWORD(id.DriverVersionLowPart));
#endif

			desc.set_driver_version(buf);

			D3DCAPS8 caps;
			::ZeroMemory(&caps, sizeof(caps));
			D3DADAPTER_IDENTIFIER8 adapter_identifier;
			::ZeroMemory(&adapter_identifier, sizeof(adapter_identifier));
			D3DInterface->GetDeviceCaps(adapter_index,WW3D_DEVTYPE,&caps);
			D3DInterface->GetAdapterIdentifier(adapter_index,LEGACY_NO_WHQL_LEVEL,&adapter_identifier);

			DX8Caps dx8caps(D3DInterface,static_cast<const void*>(&caps),WW3D_FORMAT_UNKNOWN,&adapter_identifier);

			/*
			** Enumerate the resolutions
			*/
			desc.reset_resolution_list();
			int mode_count = D3DInterface->GetAdapterModeCount(adapter_index);
			for (int mode_index=0; mode_index<mode_count; mode_index++) {
				D3DDISPLAYMODE d3dmode;
				::ZeroMemory(&d3dmode, sizeof(d3dmode));
				HRESULT res = D3DInterface->EnumAdapterModes(adapter_index,mode_index,&d3dmode);

				if (res == S_OK) {
					int bits = 0;
					switch (static_cast<unsigned>(d3dmode.Format))
					{
						case 20:
						case 21:
						case 22:		bits = 32; break;

						case 23:
						case 24:		bits = 16; break;
					}

					// Some cards fail in certain modes, DX8Caps keeps list of those.
					if (!dx8caps.Is_Valid_Display_Format(d3dmode.Width,d3dmode.Height,D3DFormat_To_WW3DFormat(d3dmode.Format))) {
						bits=0;
					}

					/*
					** If we recognize the format, add it to the list
					** TODO: should we handle more formats?  will any cards report more than 24 or 16 bit?
					*/
					if (bits != 0) {
						desc.add_resolution(d3dmode.Width,d3dmode.Height,bits);
					}
				}
			}

			// IML: If the device has one or more valid resolutions add it to the device list.
			// NOTE: Testing has shown that there are drivers with zero resolutions.
			if (desc.Enumerate_Resolutions().Count() > 0) {

				/*
				** Set up the device name
				*/
				StringClass device_name(id.Description,true);
				_RenderDeviceNameTable.Add(device_name);
				_RenderDeviceShortNameTable.Add(device_name);	// for now, just add the same name to the "pretty name table"

				/*
				** Add the render device to our table
				*/
				_RenderDeviceDescriptionTable.Add(desc);
			}
		}
	}
#endif
}

bool DX8Wrapper::Set_Any_Render_Device()
{
	// Try fullscreen first
	int dev_number = 0;
	for (; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if (Set_Render_Device(dev_number,-1,-1,-1,0,false)) {
			return true;
		}
	}

	// Then windowed
	for (dev_number = 0; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if (Set_Render_Device(dev_number,-1,-1,-1,1,false)) {
			return true;
		}
	}

	return false;
}

bool DX8Wrapper::Set_Render_Device
(
	const char * dev_name,
	int width,
	int height,
	int bits,
	int windowed,
	bool resize_window
)
{
	for ( int dev_number = 0; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if ( strcmp( dev_name, _RenderDeviceNameTable[dev_number]) == 0) {
			return Set_Render_Device( dev_number, width, height, bits, windowed, resize_window );
		}

		if ( strcmp( dev_name, _RenderDeviceShortNameTable[dev_number]) == 0) {
			return Set_Render_Device( dev_number, width, height, bits, windowed, resize_window );
		}
	}
	return false;
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
void DX8Wrapper::Get_Format_Name(unsigned int format, StringClass *tex_format)
{
		*tex_format="Unknown";
		switch (format) {
		case D3DFMT_A8R8G8B8: *tex_format="D3DFMT_A8R8G8B8"; break;
		case D3DFMT_R8G8B8: *tex_format="D3DFMT_R8G8B8"; break;
		case D3DFMT_A4R4G4B4: *tex_format="D3DFMT_A4R4G4B4"; break;
		case D3DFMT_A1R5G5B5: *tex_format="D3DFMT_A1R5G5B5"; break;
		case D3DFMT_R5G6B5: *tex_format="D3DFMT_R5G6B5"; break;
		case D3DFMT_L8: *tex_format="D3DFMT_L8"; break;
		case D3DFMT_A8: *tex_format="D3DFMT_A8"; break;
		case D3DFMT_P8: *tex_format="D3DFMT_P8"; break;
		case D3DFMT_X8R8G8B8: *tex_format="D3DFMT_X8R8G8B8"; break;
		case D3DFMT_X1R5G5B5: *tex_format="D3DFMT_X1R5G5B5"; break;
		case D3DFMT_R3G3B2: *tex_format="D3DFMT_R3G3B2"; break;
		case D3DFMT_A8R3G3B2: *tex_format="D3DFMT_A8R3G3B2"; break;
		case D3DFMT_X4R4G4B4: *tex_format="D3DFMT_X4R4G4B4"; break;
		case D3DFMT_A8P8: *tex_format="D3DFMT_A8P8"; break;
		case D3DFMT_A8L8: *tex_format="D3DFMT_A8L8"; break;
		case D3DFMT_A4L4: *tex_format="D3DFMT_A4L4"; break;
		case D3DFMT_V8U8: *tex_format="D3DFMT_V8U8"; break;
		case D3DFMT_L6V5U5: *tex_format="D3DFMT_L6V5U5"; break;
		case D3DFMT_X8L8V8U8: *tex_format="D3DFMT_X8L8V8U8"; break;
		case D3DFMT_Q8W8V8U8: *tex_format="D3DFMT_Q8W8V8U8"; break;
		case D3DFMT_V16U16: *tex_format="D3DFMT_V16U16"; break;
		case D3DFMT_W11V11U10: *tex_format="D3DFMT_W11V11U10"; break;
		case D3DFMT_UYVY: *tex_format="D3DFMT_UYVY"; break;
		case D3DFMT_YUY2: *tex_format="D3DFMT_YUY2"; break;
		case D3DFMT_DXT1: *tex_format="D3DFMT_DXT1"; break;
		case D3DFMT_DXT2: *tex_format="D3DFMT_DXT2"; break;
		case D3DFMT_DXT3: *tex_format="D3DFMT_DXT3"; break;
		case D3DFMT_DXT4: *tex_format="D3DFMT_DXT4"; break;
		case D3DFMT_DXT5: *tex_format="D3DFMT_DXT5"; break;
		case D3DFMT_D16_LOCKABLE: *tex_format="D3DFMT_D16_LOCKABLE"; break;
		case D3DFMT_D32: *tex_format="D3DFMT_D32"; break;
		case D3DFMT_D15S1: *tex_format="D3DFMT_D15S1"; break;
		case D3DFMT_D24S8: *tex_format="D3DFMT_D24S8"; break;
		case D3DFMT_D16: *tex_format="D3DFMT_D16"; break;
		case D3DFMT_D24X8: *tex_format="D3DFMT_D24X8"; break;
		case D3DFMT_D24X4S4: *tex_format="D3DFMT_D24X4S4"; break;
		default:	break;
		}
}
#endif

void DX8Wrapper::Resize_And_Position_Window()
{
#if defined(SAGE_USE_SDL3)
	// TheSuperHackers @bugfix bobtista 07/06/2026 SDL3 owns window sizing, positioning and
	// fullscreen (SDL3Main and W3DDisplay::setDisplayMode). The legacy Win32 SetWindowPos path
	// below fights it: with the windowed/fullscreen choice now honored, the !IsWindowed branch
	// shrinks the SDL fullscreen window to the logical resolution at the top-left corner. Skip it.
	return;
#else
	// Get the current dimensions of the 'render area' of the window
	RECT rect = { 0 };
	::GetClientRect (_Hwnd, &rect);

	// Is the window the correct size for this resolution?
	if ((rect.right-rect.left) != ResolutionWidth ||
			(rect.bottom-rect.top) != ResolutionHeight) {

		// Calculate what the main window's bounding rectangle should be to
		// accommodate this resolution
		rect.left = 0;
		rect.top = 0;
		rect.right = ResolutionWidth;
		rect.bottom = ResolutionHeight;
		DWORD dwstyle = ::GetWindowLong (_Hwnd, GWL_STYLE);
		AdjustWindowRect (&rect, dwstyle, FALSE);
		int width = rect.right-rect.left;
		int height = rect.bottom-rect.top;

		// Resize the window to fit this resolution
		if (!IsWindowed)
		{
			::SetWindowPos(_Hwnd, HWND_TOPMOST, 0, 0, width, height, 0);

			DEBUG_LOG(("Window resized to w:%d h:%d", width, height));
		}
		else
		{
			// TheSuperHackers @feature helmutbuhler 14/04/2025
			// Center the window in the workarea of the monitor it is on.
			MONITORINFO mi = {sizeof(MONITORINFO)};
			GetMonitorInfo(MonitorFromWindow(_Hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
			int left = (mi.rcWork.left + mi.rcWork.right - width) / 2;
			int top  = (mi.rcWork.top + mi.rcWork.bottom - height) / 2;

			// TheSuperHackers @feature helmutbuhler 14/04/2025
			// Move the window to try fit it into the monitor area, if one of its dimensions is larger than the work area.
			// Otherwise align the window to the top left edges, if it is even larger than the monitor area.
			RECT rectClient;
			rectClient.left = left - rect.left;
			rectClient.top = top - rect.top;
			rectClient.right = rectClient.left + ResolutionWidth;
			rectClient.bottom = rectClient.top + ResolutionHeight;
			MoveRectIntoOtherRect(rectClient, mi.rcMonitor, &left, &top);

			::SetWindowPos (_Hwnd, nullptr, left, top, width, height, SWP_NOZORDER);

			DEBUG_LOG(("Window positioned to x:%d y:%d, resized to w:%d h:%d", left, top, width, height));
		}
	}
#endif
}

bool DX8Wrapper::Set_Render_Device(int dev, int width, int height, int bits, int windowed,
								   bool resize_window,bool reset_device, bool restore_assets)
{
	WWASSERT(IsInitted);
	WWASSERT(dev >= -1);
	WWASSERT(dev < _RenderDeviceNameTable.Count());

	/*
	** If user has never selected a render device, start out with device 0
	*/
	if ((CurRenderDevice == -1) && (dev == -1)) {
		CurRenderDevice = 0;
	} else if (dev != -1) {
		CurRenderDevice = dev;
	}

	/*
	** If user doesn't want to change res, set the res variables to match the
	** current resolution
	*/
	if (width != -1)		ResolutionWidth = width;
	if (height != -1)		ResolutionHeight = height;

	if (bits != -1)		BitDepth = bits;
	if (windowed != -1)	IsWindowed = (windowed != 0);
	DX8Wrapper_IsWindowed = IsWindowed;

	WWDEBUG_SAY(("Attempting Set_Render_Device: name: %s (%s:%s), width: %d, height: %d, windowed: %d",
		_RenderDeviceNameTable[CurRenderDevice].str(),_RenderDeviceDescriptionTable[CurRenderDevice].Get_Driver_Name(),
		_RenderDeviceDescriptionTable[CurRenderDevice].Get_Driver_Version(),ResolutionWidth,ResolutionHeight,(IsWindowed ? 1 : 0)));

#ifdef _WIN32
	// PWG 4/13/2000 - changed so that if you say to resize the window it resizes
	// regardless of whether its windowed or not as OpenGL resizes its self around
	// the caption and edges of the window type you provide, so its important to
	// push the client area to be the size you really want.
	// if ( resize_window && windowed ) {
	if (resize_window) {
		Resize_And_Position_Window();
	}
#endif
	//must be either resetting existing device or creating a new one.
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT(reset_device || !StandaloneDeviceCreated);
#else
	WWASSERT(reset_device || D3DDevice == nullptr);
#endif

	/*
	** Initialize values for present parameters.
	*/
	::ZeroMemory(&_PresentParameters, sizeof(_PresentParameters));

	_PresentParameters.BackBufferWidth = ResolutionWidth;
	_PresentParameters.BackBufferHeight = ResolutionHeight;
	_PresentParameters.BackBufferCount = IsWindowed ? 1 : 2;

	//I changed this to discard all the time (even when full-screen) since that the most efficient. 07-16-03 MW:
	_PresentParameters.SwapEffect = Legacy_Swap_Effect(1);// Discard in windowed and full-screen modes.
	_PresentParameters.hDeviceWindow = _Hwnd;
	_PresentParameters.Windowed = IsWindowed;

	_PresentParameters.EnableAutoDepthStencil = TRUE;				// Driver will attempt to match Z-buffer depth
	_PresentParameters.Flags=0;											// We're not going to lock the backbuffer

	_PresentParameters.FullScreen_PresentationInterval = 0;
	_PresentParameters.FullScreen_RefreshRateInHz = 0;

	/*
	** Set up the buffer formats.  Several issues here:
	** - if in windowed mode, the backbuffer must use the current display format.
	** - the depth buffer must use
	*/
	if (IsWindowed) {

		D3DDISPLAYMODE desktop_mode;
		::ZeroMemory(&desktop_mode, sizeof(desktop_mode));
#if defined(GGC_RENDER_BACKEND_BGFX)
		desktop_mode.Width = ResolutionWidth;
		desktop_mode.Height = ResolutionHeight;
		desktop_mode.RefreshRate = 60;
		desktop_mode.Format = D3DFMT_A8R8G8B8;
#else
		D3DInterface->GetAdapterDisplayMode( CurRenderDevice, &desktop_mode );
#endif

		_PresentParameters.BackBufferFormat = desktop_mode.Format;
		DisplayFormat=static_cast<unsigned>(desktop_mode.Format);

		// In windowed mode, define the bitdepth from desktop mode (as it can't be changed)
		switch (static_cast<unsigned>(_PresentParameters.BackBufferFormat)) {
		case 20:
		case 21:
		case 22: BitDepth=32; break;
		case 23:
		case 25:
		case 26: BitDepth=16; break;
		case 28:
		case 41:
		case 50: BitDepth=8; break;
		default:
			// Unknown backbuffer format probably means the device can't do windowed
			return false;
		}

#if defined(GGC_RENDER_BACKEND_BGFX)
		_PresentParameters.AutoDepthStencilFormat = D3DFMT_D24S8;
#else
		if (BitDepth==32 && D3DInterface->CheckDeviceType(0,WW3D_DEVTYPE,desktop_mode.Format,Legacy_Format(21), TRUE) == S_OK)
		{	//promote 32-bit modes to include destination alpha
			_PresentParameters.BackBufferFormat = Legacy_Format(21);
		}

		/*
		** Find a appropriate Z buffer
		*/
		unsigned z_format = static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat);
		if (Find_Z_Mode(DisplayFormat,static_cast<unsigned>(_PresentParameters.BackBufferFormat),&z_format))
		{
			_PresentParameters.AutoDepthStencilFormat=Legacy_Format(z_format);
		}
		else
		{
			// If opening 32 bit mode failed, try 16 bit, even if the desktop happens to be 32 bit
			if (BitDepth==32) {
				BitDepth=16;
				_PresentParameters.BackBufferFormat=Legacy_Format(23);
				z_format = static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat);
				if (!Find_Z_Mode(static_cast<unsigned>(_PresentParameters.BackBufferFormat),static_cast<unsigned>(_PresentParameters.BackBufferFormat),&z_format)) {
					_PresentParameters.AutoDepthStencilFormat=Legacy_Format(0);
				} else {
					_PresentParameters.AutoDepthStencilFormat=Legacy_Format(z_format);
				}
			}
			else {
				_PresentParameters.AutoDepthStencilFormat=Legacy_Format(0);
			}
		}
#endif

	} else {

		/*
		** Try to find a mode that matches the user's desired bit-depth.
		*/
		unsigned display_format = DisplayFormat;
		unsigned backbuffer_format = static_cast<unsigned>(_PresentParameters.BackBufferFormat);
		unsigned depth_format = static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat);
		Find_Color_And_Z_Mode(ResolutionWidth,ResolutionHeight,BitDepth,&display_format,
			&backbuffer_format,&depth_format);
		DisplayFormat = display_format;
		_PresentParameters.BackBufferFormat = Legacy_Format(backbuffer_format);
		_PresentParameters.AutoDepthStencilFormat = Legacy_Format(depth_format);
	}

	/*
	** Set default for depth stencil format if auto Z buffer failed.
	*/
	if (static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat)==0) {
		if (BitDepth==32) {
			_PresentParameters.AutoDepthStencilFormat=Legacy_Format(71);
		}
		else {
			_PresentParameters.AutoDepthStencilFormat=Legacy_Format(80);
		}
	}

	/*
	** Check the devices support for the requested MSAA mode then setup the multi sample type
	*/
	if (MultiSampleAntiAliasing > 0) {

#if defined(GGC_RENDER_BACKEND_BGFX)
		_PresentParameters.MultiSampleType = Legacy_Multisample_Type(MultiSampleAntiAliasing);
#else
		HRESULT hrBack = D3DInterface->CheckDeviceMultiSampleType(
			CurRenderDevice,
			WW3D_DEVTYPE,
			_PresentParameters.BackBufferFormat,
			IsWindowed,
			Legacy_Multisample_Type(MultiSampleAntiAliasing)
		);

		HRESULT hrDepth = D3DInterface->CheckDeviceMultiSampleType(
			CurRenderDevice,
			WW3D_DEVTYPE,
			_PresentParameters.AutoDepthStencilFormat,
			IsWindowed,
			Legacy_Multisample_Type(MultiSampleAntiAliasing)
		);

		if (FAILED(hrBack) || FAILED(hrDepth)) {
			// IF we fail then disable MSAA entirely.
			// External code needs to retrieve the configured MSAA mode after device creation
			WWDEBUG_SAY(("Requested MSAA Mode Not Supported"));
			MultiSampleAntiAliasing = 0;
		}
#endif
	}

	_PresentParameters.MultiSampleType = Legacy_Multisample_Type(MultiSampleAntiAliasing);

	/*
	** Time to actually create the device.
	*/
	StringClass displayFormat;
	StringClass backbufferFormat;

#if !defined(GGC_RENDER_BACKEND_BGFX)
	Get_Format_Name(Legacy_Format(DisplayFormat),&displayFormat);
	Get_Format_Name(_PresentParameters.BackBufferFormat,&backbufferFormat);
#else
	displayFormat.Format("%u", DisplayFormat);
	backbufferFormat.Format("%u", static_cast<unsigned>(_PresentParameters.BackBufferFormat));
#endif

	WWDEBUG_SAY(("Using Display/BackBuffer Formats: %s/%s",displayFormat.str(),backbufferFormat.str()));

	bool ret;

	if (reset_device)
	{
		WWDEBUG_SAY(("DX8Wrapper::Set_Render_Device is resetting the device."));
		ret = Reset_Device(restore_assets);	//reset device without restoring data - we're likely switching out of the app.
	}
	else
		ret = Create_Device();

	WWDEBUG_SAY(("Reset/Create_Device done, reset_device=%d, restore_assets=%d", reset_device, restore_assets));

	if (ret)
	{
		Render2DClass::Set_Screen_Resolution( RectClass( 0, 0, ResolutionWidth, ResolutionHeight ) );
	}

	return ret;
}

bool DX8Wrapper::Set_Next_Render_Device()
{
	int new_dev = (CurRenderDevice + 1) % _RenderDeviceNameTable.Count();
	return Set_Render_Device(new_dev);
}

bool DX8Wrapper::Toggle_Windowed()
{
#ifdef WW3D_DX8
	// State OK?
	assert (IsInitted);
	if (IsInitted) {

		// Get information about the current render device's resolutions
		const RenderDeviceDescClass &render_device = Get_Render_Device_Desc ();
		const DynamicVectorClass<ResolutionDescClass> &resolutions = render_device.Enumerate_Resolutions ();

		// Loop through all the resolutions supported by the current device.
		// If we aren't currently running under one of these resolutions,
		// then we should probably		 to the closest resolution before
		// toggling the windowed state.
		int curr_res = -1;
		for (int res = 0;
		     (res < resolutions.Count ()) && (curr_res == -1);
			  res ++) {

			// Is this the resolution we are looking for?
			if ((resolutions[res].Width == ResolutionWidth) &&
				 (resolutions[res].Height == ResolutionHeight) &&
				 (resolutions[res].BitDepth == BitDepth)) {
				curr_res = res;
			}
		}

		if (curr_res == -1) {

			// We don't match any of the standard resolutions,
			// so set the first resolution and toggle the windowed state.
			return Set_Device_Resolution (resolutions[0].Width,
								 resolutions[0].Height,
								 resolutions[0].BitDepth,
								 !IsWindowed, true);
		} else {

			// Toggle the windowed state
			return Set_Device_Resolution (-1, -1, -1, !IsWindowed, true);
		}
	}
#endif //WW3D_DX8

	return false;
}

void DX8Wrapper::Set_Swap_Interval(int swap)
{
	switch (swap) {
		case 0: _PresentParameters.FullScreen_PresentationInterval = 0x80000000L; break;
		case 1: _PresentParameters.FullScreen_PresentationInterval = 0x00000001L; break;
		case 2: _PresentParameters.FullScreen_PresentationInterval = 0x00000002L; break;
		case 3: _PresentParameters.FullScreen_PresentationInterval = 0x00000004L; break;
		default: _PresentParameters.FullScreen_PresentationInterval = 0x00000001L; break;
	}

	WWDEBUG_SAY(("DX8Wrapper::Set_Swap_Interval is resetting the device."));
	Reset_Device();
}

int DX8Wrapper::Get_Swap_Interval()
{
	return _PresentParameters.FullScreen_PresentationInterval;
}

bool DX8Wrapper::Has_Stencil()
{
	const unsigned depth_format = static_cast<unsigned>(_PresentParameters.AutoDepthStencilFormat);
	bool has_stencil = (depth_format == 75 ||
						depth_format == 79);
	return has_stencil;
}

int DX8Wrapper::Get_Render_Device_Count()
{
	return _RenderDeviceNameTable.Count();

}
int DX8Wrapper::Get_Render_Device()
{
	assert(IsInitted);
	return CurRenderDevice;
}

const RenderDeviceDescClass & DX8Wrapper::Get_Render_Device_Desc(int deviceidx)
{
	WWASSERT(IsInitted);

	if ((deviceidx == -1) && (CurRenderDevice == -1)) {
		CurRenderDevice = 0;
	}

	// if the device index is -1 then we want the current device
	if (deviceidx == -1) {
		WWASSERT(CurRenderDevice >= 0);
		WWASSERT(CurRenderDevice < _RenderDeviceNameTable.Count());
		return _RenderDeviceDescriptionTable[CurRenderDevice];
	}

	// We can only ask for multiple device information if the devices
	// have been detected.
	WWASSERT(deviceidx >= 0);
	WWASSERT(deviceidx < _RenderDeviceNameTable.Count());
	return _RenderDeviceDescriptionTable[deviceidx];
}

const char * DX8Wrapper::Get_Render_Device_Name(int device_index)
{
	device_index = device_index % _RenderDeviceShortNameTable.Count();
	return _RenderDeviceShortNameTable[device_index];
}

bool DX8Wrapper::Set_Device_Resolution(int width,int height,int bits,int windowed, bool resize_window)
{
	if (D3DDevice != nullptr
#if defined(GGC_RENDER_BACKEND_BGFX)
		|| StandaloneDeviceCreated
#endif
	) {

		if (width != -1) {
			_PresentParameters.BackBufferWidth = ResolutionWidth = width;
		}
		if (height != -1) {
			_PresentParameters.BackBufferHeight = ResolutionHeight = height;
		}
		if (resize_window)
		{
			Resize_And_Position_Window();
		}
#pragma message("TODO: support changing windowed status and changing the bit depth")
		WWDEBUG_SAY(("DX8Wrapper::Set_Device_Resolution is resetting the device."));
		return Reset_Device();
	} else {
		return false;
	}
}

void DX8Wrapper::Get_Device_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed)
{
	WWASSERT(IsInitted);

	set_w = ResolutionWidth;
	set_h = ResolutionHeight;
	set_bits = BitDepth;
	set_windowed = IsWindowed;
}

void DX8Wrapper::Get_Render_Target_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed)
{
	WWASSERT(IsInitted);

#if defined(GGC_RENDER_BACKEND_BGFX)
	Get_Device_Resolution (set_w, set_h, set_bits, set_windowed);
#else
	if (CurrentRenderTarget != nullptr) {
		D3DSURFACE_DESC info;
		CurrentRenderTarget->GetDesc (&info);

		set_w				= info.Width;
		set_h				= info.Height;
		set_bits			= BitDepth;		// should we get the actual bit depth of the target?
		set_windowed	= IsWindowed;	// this doesn't really make sense for render targets (shouldn't matter)...

	} else {
		Get_Device_Resolution (set_w, set_h, set_bits, set_windowed);
	}
#endif
}

bool DX8Wrapper::Registry_Save_Render_Device( const char * sub_key )
{
	int	width, height, depth;
	bool	windowed;
	Get_Device_Resolution(width, height, depth, windowed);
	return Registry_Save_Render_Device(sub_key, CurRenderDevice, ResolutionWidth, ResolutionHeight, BitDepth, IsWindowed, TextureBitDepth);
}

bool DX8Wrapper::Registry_Save_Render_Device( const char *sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth)
{
	RegistryClass * registry = W3DNEW RegistryClass( sub_key );
	WWASSERT( registry );

	if ( !registry->Is_Valid() ) {
		delete registry;
		WWDEBUG_SAY(( "Error getting Registry" ));
		return false;
	}

	registry->Set_String( VALUE_NAME_RENDER_DEVICE_NAME,
		_RenderDeviceShortNameTable[device] );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_WIDTH,	width );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_HEIGHT, height );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_DEPTH, depth );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_WINDOWED, windowed );
	registry->Set_Int( VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH, texture_depth );

	delete registry;
	return true;
}

bool DX8Wrapper::Registry_Load_Render_Device( const char * sub_key, bool resize_window )
{
	char	name[ 200 ];
	int	width,height,depth,windowed;

	if (	Registry_Load_Render_Device(	sub_key,
													name,
													sizeof(name),
													width,
													height,
													depth,
													windowed,
													TextureBitDepth) &&
			(*name != 0))
	{
		WWDEBUG_SAY(( "Device %s (%d X %d) %d bit windowed:%d", name,width,height,depth,windowed));

		if (TextureBitDepth==16 || TextureBitDepth==32) {
//			WWDEBUG_SAY(( "Texture depth %d", TextureBitDepth));
		} else {
			WWDEBUG_SAY(( "Invalid texture depth %d, switching to 16 bits", TextureBitDepth));
			TextureBitDepth=16;
		}

		if ( Set_Render_Device( name, width,height,depth,windowed, resize_window ) != true) {
			if (depth==16) depth=32;
			else depth=16;
			if ( Set_Render_Device( name, width,height,depth,windowed, resize_window ) == true) {
				return true;
			}
			if (depth==16) depth=32;
			else depth=16;
			// we'll test resolutions down, so if start is 640, increase to begin with...
			if (width==640) {
				width=1024;
				height=768;
			}
			for(;;) {
				if (width>2048) {
					width=2048;
					height=1536;
				}
				else if (width>1920) {
					width=1920;
					height=1440;
				}
				else if (width>1600) {
					width=1600;
					height=1200;
				}
				else if (width>1280) {
					width=1280;
					height=1024;
				}
				else if (width>1024) {
					width=1024;
					height=768;
				}
				else if (width>800) {
					width=800;
					height=600;
				}
				else if (width!=640) {
					width=640;
					height=480;
				}
				else {
					return Set_Any_Render_Device();
				}
				for (int i=0;i<2;++i) {
					if ( Set_Render_Device( name, width,height,depth,windowed, resize_window ) == true) {
						return true;
					}
					if (depth==16) depth=32;
					else depth=16;
				}
			}
		}

		return true;
	}

	WWDEBUG_SAY(( "Error getting Registry" ));

	return Set_Any_Render_Device();
}

bool DX8Wrapper::Registry_Load_Render_Device( const char * sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int &texture_depth)
{
	RegistryClass registry( sub_key );

	if ( registry.Is_Valid() ) {
		registry.Get_String( VALUE_NAME_RENDER_DEVICE_NAME,
			device, device_len);

		width =		registry.Get_Int( VALUE_NAME_RENDER_DEVICE_WIDTH, -1 );
		height =		registry.Get_Int( VALUE_NAME_RENDER_DEVICE_HEIGHT, -1 );
		depth =		registry.Get_Int( VALUE_NAME_RENDER_DEVICE_DEPTH, -1 );
		windowed =	registry.Get_Int( VALUE_NAME_RENDER_DEVICE_WINDOWED, -1 );
		texture_depth = registry.Get_Int( VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH, -1 );
		return true;
	}
	*device=0;
	width=-1;
	height=-1;
	depth=-1;
	windowed=-1;
	texture_depth=-1;
	return false;
}


bool DX8Wrapper::Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,unsigned * set_colorbuffer,unsigned * set_backbuffer,unsigned * set_zmode)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)resx;
	(void)resy;
	const unsigned color_format = bitdepth == 16 ? D3DFMT_R5G6B5 : D3DFMT_A8R8G8B8;
	*set_colorbuffer = color_format;
	*set_backbuffer = color_format;
	*set_zmode = bitdepth == 16 ? D3DFMT_D16 : D3DFMT_D24S8;
	return true;
#else
	static unsigned _formats16[] =
	{
		23,
		24,
		25
	};

	static unsigned _formats32[] =
	{
		21,
		22,
		20,
	};

	/*
	** Select the table that we're going to use to search for a valid backbuffer format
	*/
	unsigned * format_table = nullptr;
	int format_count = 0;

	if (BitDepth == 16) {
		format_table = _formats16;
		format_count = sizeof(_formats16) / sizeof(unsigned);
	} else {
		format_table = _formats32;
		format_count = sizeof(_formats32) / sizeof(unsigned);
	}

	/*
	** now search for a valid format
	*/
	bool found = false;
	unsigned int mode = 0;

	int format_index=0;
	for (; format_index < format_count; format_index++) {
		found |= Find_Color_Mode(format_table[format_index],resx,resy,&mode);
		if (found) break;
	}

	if (!found) {
		return false;
	} else {
		*set_backbuffer=*set_colorbuffer = format_table[format_index];
	}

	if (bitdepth==32 && *set_colorbuffer == 22 && D3DInterface->CheckDeviceType(0,WW3D_DEVTYPE,Legacy_Format(*set_colorbuffer),Legacy_Format(21), TRUE) == S_OK)
	{	//promote 32-bit modes to include destination alpha when supported
		*set_backbuffer = 21;
	}

	/*
	** We found a backbuffer format, now find a zbuffer format
	*/
	return Find_Z_Mode(*set_colorbuffer,*set_backbuffer, set_zmode);
#endif
};


// find the resolution mode with at least resx,resy with the highest supported
// refresh rate
bool DX8Wrapper::Find_Color_Mode(unsigned colorbuffer, int resx, int resy, UINT *mode)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)colorbuffer;
	(void)resx;
	(void)resy;
	*mode = 0;
	return true;
#else
	UINT i,j,modemax;
	UINT rx,ry;
	D3DDISPLAYMODE dmode;
	::ZeroMemory(&dmode, sizeof(dmode));

	rx=(unsigned int) resx;
	ry=(unsigned int) resy;

	bool found=false;

	modemax=D3DInterface->GetAdapterModeCount(0);

	i=0;

	while (i<modemax && !found)
	{
		D3DInterface->EnumAdapterModes(0, i, &dmode);
		if (dmode.Width==rx && dmode.Height==ry && static_cast<unsigned>(dmode.Format)==colorbuffer) {
			WWDEBUG_SAY(("Found valid color mode.  Width = %d Height = %d Format = %d",dmode.Width,dmode.Height,dmode.Format));
			found=true;
		}
		i++;
	}

	i--; // this is the first valid mode

	// no match
	if (!found) {
		WWDEBUG_SAY(("Failed to find a valid color mode"));
		return false;
	}

	// go to the highest refresh rate in this mode
	bool stillok=true;

	j=i;
	while (j<modemax && stillok)
	{
		D3DInterface->EnumAdapterModes(0, j, &dmode);
		if (dmode.Width==rx && dmode.Height==ry && static_cast<unsigned>(dmode.Format)==colorbuffer)
			stillok=true; else stillok=false;
		j++;
	}

	if (stillok==false) *mode=j-2;
	else *mode=i;

	return true;
#endif
}

// Helper function to find a Z buffer mode for the colorbuffer
// Will look for greatest Z precision
bool DX8Wrapper::Find_Z_Mode(unsigned colorbuffer,unsigned backbuffer, unsigned *zmode)
{
	//MW: Swapped the next 2 tests so that Stencil modes get tested first.
	if (Test_Z_Mode(colorbuffer,backbuffer,75))
	{
		*zmode=75;
		WWDEBUG_SAY(("Found zbuffer mode 75"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,71))
	{
		*zmode=71;
		WWDEBUG_SAY(("Found zbuffer mode 71"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,77))
	{
		*zmode=77;
		WWDEBUG_SAY(("Found zbuffer mode 77"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,79))
	{
		*zmode=79;
		WWDEBUG_SAY(("Found zbuffer mode 79"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,80))
	{
		*zmode=80;
		WWDEBUG_SAY(("Found zbuffer mode 80"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,73))
	{
		*zmode=73;
		WWDEBUG_SAY(("Found zbuffer mode 73"));
		return true;
	}

	// can't find a match
	WWDEBUG_SAY(("Failed to find a valid zbuffer mode"));
	return false;
}

bool DX8Wrapper::Test_Z_Mode(unsigned colorbuffer,unsigned backbuffer, unsigned zmode)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)colorbuffer;
	(void)backbuffer;
	(void)zmode;
	return true;
#else
	// See if we have this mode first
	if (FAILED(D3DInterface->CheckDeviceFormat(0,WW3D_DEVTYPE,
		Legacy_Format(colorbuffer),2,Legacy_Resource_Type(1),Legacy_Format(zmode))))
	{
		WWDEBUG_SAY(("CheckDeviceFormat failed.  Colorbuffer format = %d  Zbufferformat = %d",colorbuffer,zmode));
		return false;
	}

	// Then see if it matches the color buffer
	if(FAILED(D3DInterface->CheckDepthStencilMatch(0, WW3D_DEVTYPE,
		Legacy_Format(colorbuffer),Legacy_Format(backbuffer),Legacy_Format(zmode))))
	{
		WWDEBUG_SAY(("CheckDepthStencilMatch failed.  Colorbuffer format = %d  Backbuffer format = %d Zbufferformat = %d",colorbuffer,backbuffer,zmode));
		return false;
	}
	return true;
#endif
}


void DX8Wrapper::Reset_Statistics()
{
	FrameStatistics = DX8FrameStatistics();
	LastFrameStatistics = DX8FrameStatistics();
}

void DX8Wrapper::Begin_Statistics()
{
	FrameStatistics = DX8FrameStatistics();
}

void DX8Wrapper::End_Statistics()
{
	LastFrameStatistics = FrameStatistics;
}

const DX8FrameStatistics& DX8Wrapper::Get_Last_Frame_Statistics()
{
	return LastFrameStatistics;
}

unsigned long DX8Wrapper::Get_FrameCount() {return FrameCount;}

void DX8_Assert()
{
#if !defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT(DX8Wrapper::_Get_D3D8());
#endif
	DX8_THREAD_ASSERT();
}

void DX8Wrapper::Begin_Scene()
{
	DX8_THREAD_ASSERT();

#if !defined(GGC_RENDER_BACKEND_BGFX)
#if ENABLE_EMBEDDED_BROWSER
	DX8WebBrowser::Update();
#endif

	DX8CALL(BeginScene());

	DX8WebBrowser::Update();
#endif
}

void DX8Wrapper::End_Scene(bool flip_frames)
{
	DX8_THREAD_ASSERT();
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(EndScene());

	DX8WebBrowser::Render(0);

	if (flip_frames) {
		DX8_Assert();
		HRESULT hr;
		{
			WWPROFILE("DX8Device::Present()");
			hr=_Get_D3D_Device8()->Present(nullptr, nullptr, nullptr, nullptr);
		}

		DX8_RECORD_DX8_CALLS();

		if (SUCCEEDED(hr)) {
#ifdef EXTENDED_STATS
			if (stats.m_sleepTime) {
				::Sleep(stats.m_sleepTime);
			}
#endif
			IsDeviceLost=false;
			FrameCount++;
		}
		else {
			IsDeviceLost=true;
		}

		// If the device was lost we need to check for cooperative level and possibly reset the device
		if (hr==D3DERR_DEVICELOST) {
			hr=_Get_D3D_Device8()->TestCooperativeLevel();
			if (hr==D3DERR_DEVICENOTRESET) {
				WWDEBUG_SAY(("DX8Wrapper::End_Scene is resetting the device."));
				Reset_Device();
			}
			else {
				// Sleep it not active
				ThreadClass::Sleep_Ms(200);
			}
		}
		else {
			DX8_ErrorCode(hr);
		}
	}
#endif

	// Each frame, release all of the buffers and textures.
	Set_Vertex_Buffer(nullptr);
	Set_Index_Buffer(nullptr,0);
	for (int i=0;i<CurrentCaps->Get_Max_Textures_Per_Pass();++i) Commit_Fixed_Function_Texture(i,nullptr);
	FixedFunctionState::Set_Material(nullptr);
}


void DX8Wrapper::Flip_To_Primary()
{
#if !defined(GGC_RENDER_BACKEND_BGFX)
	// If we are fullscreen and the current frame is odd then we need
	// to force a page flip to ensure that the first buffer in the flipping
	// chain is the one visible.
	if (!IsWindowed) {
		DX8_Assert();

		int numBuffers = (_PresentParameters.BackBufferCount + 1);
		int visibleBuffer = (FrameCount % numBuffers);
		int flipCount = ((numBuffers - visibleBuffer) % numBuffers);
		int resetAttempts = 0;

		while ((flipCount > 0) && (resetAttempts < 3)) {
			HRESULT hr = _Get_D3D_Device8()->TestCooperativeLevel();

			if (FAILED(hr)) {
				WWDEBUG_SAY(("TestCooperativeLevel Failed!"));

				if (D3DERR_DEVICELOST == hr) {
					IsDeviceLost=true;
					WWDEBUG_SAY(("DEVICELOST: Cannot flip to primary."));
					return;
				}
				IsDeviceLost=false;

				if (D3DERR_DEVICENOTRESET == hr) {
					WWDEBUG_SAY(("DEVICENOTRESET"));
					Reset_Device();
					resetAttempts++;
				}
			} else {
				WWDEBUG_SAY(("Flipping: %ld", FrameCount));
				hr = _Get_D3D_Device8()->Present(nullptr, nullptr, nullptr, nullptr);

				if (SUCCEEDED(hr)) {
					IsDeviceLost=false;
					FrameCount++;
					WWDEBUG_SAY(("Flip to primary succeeded %ld", FrameCount));
				}
				else {
					IsDeviceLost=true;
				}
			}

			--flipCount;
		}
	}
#endif
}


//**********************************************************************************************
//! Clear current render device
/*! KM
/* 5/17/02 KM Fixed support for render to texture with depth/stencil buffers
*/
void DX8Wrapper::Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha, float z, unsigned int stencil)
{
	DX8_THREAD_ASSERT();

#if !defined(GGC_RENDER_BACKEND_BGFX)
	// If we try to clear a stencil buffer which is not there, the entire call will fail
	// KJM fixed this to get format from back buffer (incase render to texture is used)
	/*bool has_stencil = (	_PresentParameters.AutoDepthStencilFormat == D3DFMT_D15S1 ||
								_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24S8 ||
								_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24X4S4);*/
	bool has_stencil=false;
	IDirect3DSurface8* depthbuffer;

	_Get_D3D_Device8()->GetDepthStencilSurface(&depthbuffer);
	DX8_RECORD_DX8_CALLS();

	if (depthbuffer)
	{
		D3DSURFACE_DESC desc;
		depthbuffer->GetDesc(&desc);
		has_stencil=
		(
			desc.Format==D3DFMT_D15S1 ||
			desc.Format==D3DFMT_D24S8 ||
			desc.Format==D3DFMT_D24X4S4
		);

		// release ref
		depthbuffer->Release();
	}

	DWORD flags = 0;
	if (clear_color) flags |= D3DCLEAR_TARGET;
	if (clear_z_stencil) flags |= D3DCLEAR_ZBUFFER;
	if (clear_z_stencil && has_stencil) flags |= D3DCLEAR_STENCIL;
	if (flags)
	{
		DX8CALL(Clear(0, nullptr, flags, Convert_Color(color,dest_alpha), z, stencil));
	}
#endif
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
void DX8Wrapper::Set_Viewport(CONST D3DVIEWPORT8* pViewport)
{
	DX8_THREAD_ASSERT();
	DX8CALL(SetViewport(pViewport));

#if defined(GGC_RENDER_BACKEND_BGFX)
	// TheSuperHackers @fix bobtista 19/04/2026 Notify g_renderBackend so bgfx
	// view rects stay in sync with the D3D8 viewport. CameraClass::Apply()
	// calls this directly, bypassing g_renderBackend->Set_Viewport().
	if (g_renderBackend != nullptr && pViewport != nullptr)
	{
		RenderBackendViewport rbvp;
		rbvp.x      = pViewport->X;
		rbvp.y      = pViewport->Y;
		rbvp.width  = pViewport->Width;
		rbvp.height = pViewport->Height;
		rbvp.min_z  = pViewport->MinZ;
		rbvp.max_z  = pViewport->MaxZ;
		g_renderBackend->Set_Viewport(rbvp);
	}
#endif
}
#endif

// ----------------------------------------------------------------------------
//
// Set vertex buffer. A reference to previous vertex buffer is released and
// this one is assigned the current vertex buffer. The DX8 vertex buffer will
// actually be set in Apply() which is called by Draw_Indexed_Triangles().
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream)
{
	FixedFunctionState::Set_Vertex_Buffer(vb, stream);
}

// ----------------------------------------------------------------------------
//
// Set index buffer. A reference to previous index buffer is released and
// this one is assigned the current index buffer. The DX8 index buffer will
// actually be set in Apply() which is called by Draw_Indexed_Triangles().
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset)
{
	FixedFunctionState::Set_Index_Buffer(ib, index_base_offset);
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer using dynamic access object.
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Vertex_Buffer(const DynamicVBAccessClass& vba_)
{
	FixedFunctionState::Set_Vertex_Buffer(vba_);
}

// ----------------------------------------------------------------------------
//
// Set index buffer using dynamic access object.
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Index_Buffer(const DynamicIBAccessClass& iba_,unsigned short index_base_offset)
{
	FixedFunctionState::Set_Index_Buffer(iba_, index_base_offset);
}

// ----------------------------------------------------------------------------
//
// Private function for the special case of rendering polygons from sorting
// index and vertex buffers.
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Sorting_IB_VB(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	WWASSERT(FixedFunctionState::Render_State().vertex_buffer_types[0]==BUFFER_TYPE_SORTING || FixedFunctionState::Render_State().vertex_buffer_types[0]==BUFFER_TYPE_DYNAMIC_SORTING);
	WWASSERT(FixedFunctionState::Render_State().index_buffer_type==BUFFER_TYPE_SORTING || FixedFunctionState::Render_State().index_buffer_type==BUFFER_TYPE_DYNAMIC_SORTING);

	// Fill dynamic vertex buffer with sorting vertex buffer vertices
	DynamicVBAccessClass dyn_vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,vertex_count);
	{
		DynamicVBAccessClass::WriteLockClass lock(&dyn_vb_access);
		VertexFormatXYZNDUV2* src = static_cast<SortingVertexBufferClass*>(FixedFunctionState::Render_State().vertex_buffers[0])->VertexBuffer;
		VertexFormatXYZNDUV2* dest= lock.Get_Formatted_Vertex_Array();
		src += FixedFunctionState::Render_State().vba_offset + FixedFunctionState::Render_State().index_base_offset + min_vertex_index;
		unsigned  size = dyn_vb_access.FVF_Info().Get_FVF_Size()*vertex_count/sizeof(unsigned);
		unsigned *dest_u =(unsigned*) dest;
		unsigned *src_u = (unsigned*) src;

		for (unsigned i=0;i<size;++i) {
			*dest_u++=*src_u++;
		}
	}

#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetStreamSource(
		0,
		Legacy_Vertex_Buffer(dyn_vb_access.VertexBuffer),
		dyn_vb_access.FVF_Info().Get_FVF_Size()));
#endif
#if !defined(GGC_RENDER_BACKEND_BGFX)
	unsigned fvf=dyn_vb_access.FVF_Info().Get_FVF();
	if (fvf!=0) {
		DX8CALL(SetVertexShader(fvf));
	}
#endif
	DX8_RECORD_VERTEX_BUFFER_CHANGE();

	unsigned index_count=0;
	switch (primitive_type) {
	case 4: index_count=polygon_count*3; break;
	case 5: index_count=polygon_count+2; break;
	case 6: index_count=polygon_count+2; break;
	default: WWASSERT(0); break; // Unsupported primitive type
	}

	// Fill dynamic index buffer with sorting index buffer vertices
	DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC,index_count);
	{
		DynamicIBAccessClass::WriteLockClass lock(&dyn_ib_access);
		unsigned short* dest=lock.Get_Index_Array();
		unsigned short* src=nullptr;
		src=static_cast<SortingIndexBufferClass*>(FixedFunctionState::Render_State().index_buffer)->index_buffer;
		src+=FixedFunctionState::Render_State().iba_offset+start_index;

		for (unsigned short i=0;i<index_count;++i) {
			unsigned short index=*src++;
			index-=min_vertex_index;
			WWASSERT(index<vertex_count);
			*dest++=index;
		}
	}

#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(SetIndices(
		Legacy_Index_Buffer(dyn_ib_access.IndexBuffer),
		dyn_vb_access.VertexBufferOffset));
#endif
	DX8_RECORD_INDEX_BUFFER_CHANGE();

	DX8_RECORD_DRAW_CALLS();
#if !defined(GGC_RENDER_BACKEND_BGFX)
	DX8CALL(DrawIndexedPrimitive(
		static_cast<D3DPRIMITIVETYPE>(4),
		0,		// start vertex
		vertex_count,
		dyn_ib_access.IndexBufferOffset,
		polygon_count));
#endif

	DX8_RECORD_RENDER(polygon_count,vertex_count,FixedFunctionState::Render_State().shader);

	// TheSuperHackers @refactor bobtista 11/04/2026 Hand the
	// internal dynamic VB/IB to the render backend so a bgfx co-resident
	// can submit the same draw using its transient captures of these
	// inner buffers. The Write locks above already fired the backend's
	// Capture_Dynamic_* hooks, so pending transients are keyed by
	// &dyn_vb_access / &dyn_ib_access. The backend remaps the outer
	// Draw_Triangles args (start_index=0, min_vertex_index=0) and sets
	// an internal skip flag so the outer BgfxBackend::Draw_Triangles
	// does not emit a stale second submit. No-op on DX8Backend.
	if (g_renderBackend != nullptr) {
		g_renderBackend->Submit_Sorted_Draw(dyn_vb_access, dyn_ib_access,
			polygon_count, vertex_count);
	}
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (DrawPolygonLowBoundLimit && DrawPolygonLowBoundLimit>=polygon_count) return;

	DX8_THREAD_ASSERT();
	SNAPSHOT_SAY(("DX8 - draw"));

	Commit_Deferred_Render_State_Changes();

	// Debug feature to disable triangle drawing...
	if (!_Is_Triangle_Draw_Enabled()) return;

#if defined(MESH_RENDER_SNAPSHOT_ENABLED) && !defined(GGC_RENDER_BACKEND_BGFX)
	if (WW3D::Is_Snapshot_Activated()) {
		DWORD passes=0;
		SNAPSHOT_SAY(("ValidateDevice:"));
		HRESULT res=D3DDevice->ValidateDevice(&passes);
		switch (res) {
		case D3D_OK:
			SNAPSHOT_SAY(("OK"));
			break;

		case D3DERR_CONFLICTINGTEXTUREFILTER:
			SNAPSHOT_SAY(("D3DERR_CONFLICTINGTEXTUREFILTER"));
			break;
		case D3DERR_CONFLICTINGTEXTUREPALETTE:
			SNAPSHOT_SAY(("D3DERR_CONFLICTINGTEXTUREPALETTE"));
			break;
		case D3DERR_DEVICELOST:
			SNAPSHOT_SAY(("D3DERR_DEVICELOST"));
			break;
		case D3DERR_TOOMANYOPERATIONS:
			SNAPSHOT_SAY(("D3DERR_TOOMANYOPERATIONS"));
			break;
		case D3DERR_UNSUPPORTEDALPHAARG:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDALPHAARG"));
			break;
		case D3DERR_UNSUPPORTEDALPHAOPERATION:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDALPHAOPERATION"));
			break;
		case D3DERR_UNSUPPORTEDCOLORARG:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDCOLORARG"));
			break;
		case D3DERR_UNSUPPORTEDCOLOROPERATION:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDCOLOROPERATION"));
			break;
		case D3DERR_UNSUPPORTEDFACTORVALUE:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDFACTORVALUE"));
			break;
		case D3DERR_UNSUPPORTEDTEXTUREFILTER:
			SNAPSHOT_SAY(("D3DERR_UNSUPPORTEDTEXTUREFILTER"));
			break;
		case D3DERR_WRONGTEXTUREFORMAT:
			SNAPSHOT_SAY(("D3DERR_WRONGTEXTUREFORMAT"));
			break;
		default:
			SNAPSHOT_SAY(("UNKNOWN Error"));
			break;
		}
	}
#endif	// MESH_RENDER_SNAPSHOT_ENABLED


	SNAPSHOT_SAY(("DX8 - draw %d polygons (%d vertices)",polygon_count,vertex_count));

	if (vertex_count<3) {
		min_vertex_index=0;
		switch (FixedFunctionState::Render_State().vertex_buffer_types[0]) {
		case BUFFER_TYPE_STATIC:
		case BUFFER_TYPE_SORTING:
			vertex_count=FixedFunctionState::Render_State().vertex_buffers[0]->Get_Vertex_Count()-FixedFunctionState::Render_State().index_base_offset-FixedFunctionState::Render_State().vba_offset-min_vertex_index;
			break;
		case BUFFER_TYPE_DYNAMIC:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			vertex_count=FixedFunctionState::Render_State().vba_count;
			break;
		}
	}

	switch (FixedFunctionState::Render_State().vertex_buffer_types[0]) {
	case BUFFER_TYPE_STATIC:
	case BUFFER_TYPE_DYNAMIC:
		switch (FixedFunctionState::Render_State().index_buffer_type) {
		case BUFFER_TYPE_STATIC:
		case BUFFER_TYPE_DYNAMIC:
			{
/*				if ((start_index+FixedFunctionState::Render_State().iba_offset+polygon_count*3) > FixedFunctionState::Render_State().index_buffer->Get_Index_Count())
				{	WWASSERT_PRINT(0,"OVERFLOWING INDEX BUFFER");
					///@todo: MUST FIND OUT WHY THIS HAPPENS WITH LOTS OF PARTICLES ON BIG FIGHT!  -MW
					break;
				}*/
				DX8_RECORD_RENDER(polygon_count,vertex_count,FixedFunctionState::Render_State().shader);
				DX8_RECORD_DRAW_CALLS();
#if !defined(GGC_RENDER_BACKEND_BGFX)
				DX8CALL(DrawIndexedPrimitive(
					(D3DPRIMITIVETYPE)primitive_type,
					min_vertex_index,
					vertex_count,
					start_index+FixedFunctionState::Render_State().iba_offset,
					polygon_count));
#endif
			}
			break;
		case BUFFER_TYPE_SORTING:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			WWASSERT_PRINT(0,"VB and IB must of same type (sorting or dx8)");
			break;
		case BUFFER_TYPE_INVALID:
			WWASSERT(0);
			break;
		}
		break;
	case BUFFER_TYPE_SORTING:
	case BUFFER_TYPE_DYNAMIC_SORTING:
		switch (FixedFunctionState::Render_State().index_buffer_type) {
		case BUFFER_TYPE_STATIC:
		case BUFFER_TYPE_DYNAMIC:
			WWASSERT_PRINT(0,"VB and IB must of same type (sorting or dx8)");
			break;
		case BUFFER_TYPE_SORTING:
		case BUFFER_TYPE_DYNAMIC_SORTING:
			Draw_Sorting_IB_VB(primitive_type,start_index,polygon_count,min_vertex_index,vertex_count);
			break;
		case BUFFER_TYPE_INVALID:
			WWASSERT(0);
			break;
		}
		break;
	case BUFFER_TYPE_INVALID:
		WWASSERT(0);
		break;
	}
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Triangles(
	unsigned buffer_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (buffer_type==BUFFER_TYPE_SORTING || buffer_type==BUFFER_TYPE_DYNAMIC_SORTING) {
		SortingRendererClass::Insert_Triangles(start_index,polygon_count,min_vertex_index,vertex_count);
	}
	else {
		Draw(4,start_index,polygon_count,min_vertex_index,vertex_count);
	}
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Triangles(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw(4,start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Strip(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw(5,start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Commit_Deferred_Render_State_Changes()
{
	SNAPSHOT_SAY(("DX8Wrapper::Commit_Deferred_Render_State_Changes()"));

	if (!FixedFunctionState::Changed_Mask()) return;
	if (FixedFunctionState::Changed_Mask()&SHADER_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply shader"));
		FixedFunctionState::Render_State().shader.Apply();
	}

	unsigned mask=TEXTURE0_CHANGED;
	int i=0;
	for (;i<CurrentCaps->Get_Max_Textures_Per_Pass();++i,mask<<=1)
	{
		if (FixedFunctionState::Changed_Mask()&mask)
		{
			SNAPSHOT_SAY(("DX8 - apply texture %d (%s)",i,FixedFunctionState::Render_State().Textures[i] ? FixedFunctionState::Render_State().Textures[i]->Get_Full_Path().str() : "null"));

			if (FixedFunctionState::Render_State().Textures[i])
			{
				FixedFunctionState::Render_State().Textures[i]->Apply(i);
			}
			else
			{
				TextureBaseClass::Apply_Null(i);
			}
		}
	}

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (FixedFunctionState::Changed_Mask()&MATERIAL_CHANGED)
	{
		SNAPSHOT_SAY(("DX8 - apply material"));
		VertexMaterialClass* material=const_cast<VertexMaterialClass*>(FixedFunctionState::Render_State().material);
		if (material)
		{
			material->Apply();
		}
		else VertexMaterialClass::Apply_Null();
	}

	if (FixedFunctionState::Changed_Mask()&LIGHTS_CHANGED)
	{
		unsigned mask=LIGHT0_CHANGED;
		for (unsigned index=0;index<4;++index,mask<<=1) {
			if (FixedFunctionState::Changed_Mask()&mask) {
				SNAPSHOT_SAY(("DX8 - apply light %d",index));
				if (FixedFunctionState::Render_State().LightEnable[index]) {
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
					if ( WW3D::Is_Snapshot_Activated() ) {
						const LegacyFixedFunctionLight * light = &(FixedFunctionState::Render_State().Lights[index]);
						static const char * _light_types[] = { "Unknown", "Point","Spot", "Directional" };
						WWASSERT((light->Type >= 0) && (light->Type <= 3));

						SNAPSHOT_SAY((" type = %s amb = %4.2f,%4.2f,%4.2f  diff = %4.2f,%4.2f,%4.2f spec = %4.2f, %4.2f, %4.2f",
							_light_types[light->Type],
							light->Ambient.r,light->Ambient.g,light->Ambient.b,
							light->Diffuse.r,light->Diffuse.g,light->Diffuse.b,
							light->Specular.r,light->Specular.g,light->Specular.b ));
						SNAPSHOT_SAY((" pos = %f, %f, %f  dir = %f, %f, %f",
							light->Position.x, light->Position.y, light->Position.z,
							light->Direction.x, light->Direction.y, light->Direction.z ));
					}
#endif

					D3DLIGHT8 light = To_D3D_Light(FixedFunctionState::Render_State().Lights[index]);
					Set_DX8_Light(index,&light);
				}
				else {
					Set_DX8_Light(index,nullptr);
					SNAPSHOT_SAY((" clearing light to null"));
				}
			}
		}
	}
#endif

	if (FixedFunctionState::Changed_Mask()&WORLD_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply world matrix"));
		Commit_Fixed_Function_Transform(256,FixedFunctionState::Render_State().world);
	}
	if (FixedFunctionState::Changed_Mask()&VIEW_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply view matrix"));
		Commit_Fixed_Function_Transform(2,FixedFunctionState::Render_State().view);
	}
	if (FixedFunctionState::Changed_Mask()&VERTEX_BUFFER_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply vb change"));
		for (i=0;i<MAX_VERTEX_STREAMS;++i) {
			if (FixedFunctionState::Render_State().vertex_buffers[i]) {
				switch (FixedFunctionState::Render_State().vertex_buffer_types[i]) {//->Type()) {
				case BUFFER_TYPE_STATIC:
				case BUFFER_TYPE_DYNAMIC:
#if !defined(GGC_RENDER_BACKEND_BGFX)
					DX8CALL(SetStreamSource(
						i,
						Legacy_Vertex_Buffer(FixedFunctionState::Render_State().vertex_buffers[i]),
						FixedFunctionState::Render_State().vertex_buffers[i]->FVF_Info().Get_FVF_Size()));
#endif
					DX8_RECORD_VERTEX_BUFFER_CHANGE();
					{
						// If the VB format is FVF, set the FVF as a vertex shader
						unsigned fvf=FixedFunctionState::Render_State().vertex_buffers[i]->FVF_Info().Get_FVF();
						if (fvf!=0) {
							Commit_Vertex_Shader_Value(fvf);
						}
					}
					break;
				case BUFFER_TYPE_SORTING:
				case BUFFER_TYPE_DYNAMIC_SORTING:
					break;
				default:
					WWASSERT(0);
				}
			} else {
#if !defined(GGC_RENDER_BACKEND_BGFX)
				DX8CALL(SetStreamSource(i,nullptr,0));
#endif
				DX8_RECORD_VERTEX_BUFFER_CHANGE();
			}
		}
	}
	if (FixedFunctionState::Changed_Mask()&INDEX_BUFFER_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply ib change"));
		if (FixedFunctionState::Render_State().index_buffer) {
			switch (FixedFunctionState::Render_State().index_buffer_type) {//->Type()) {
			case BUFFER_TYPE_STATIC:
			case BUFFER_TYPE_DYNAMIC:
#if !defined(GGC_RENDER_BACKEND_BGFX)
				DX8CALL(SetIndices(
					Legacy_Index_Buffer(FixedFunctionState::Render_State().index_buffer),
					FixedFunctionState::Render_State().index_base_offset+FixedFunctionState::Render_State().vba_offset));
#endif
				DX8_RECORD_INDEX_BUFFER_CHANGE();
				break;
			case BUFFER_TYPE_SORTING:
			case BUFFER_TYPE_DYNAMIC_SORTING:
				break;
			default:
				WWASSERT(0);
			}
		}
		else {
#if !defined(GGC_RENDER_BACKEND_BGFX)
			DX8CALL(SetIndices(
				nullptr,
				0));
#endif
			DX8_RECORD_INDEX_BUFFER_CHANGE();
		}
	}

	FixedFunctionState::Changed_Mask()&=((unsigned)WORLD_IDENTITY|(unsigned)VIEW_IDENTITY);

	SNAPSHOT_SAY(("DX8Wrapper::Commit_Deferred_Render_State_Changes() - finished"));
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
void DX8Wrapper::Apply_Render_State_Changes()
{
	Commit_Deferred_Render_State_Changes();
}
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
IDirect3DTexture8 * DX8Wrapper::_Create_DX8_Texture
(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool,
	bool rendertarget
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture8 *texture = nullptr;

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.

	// Render target may return NOTAVAILABLE, in
	// which case we return null.
	if (rendertarget) {
		unsigned ret=D3DXCreateTexture(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			height,
			mip_level_count,
			D3DUSAGE_RENDERTARGET,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture);

		if (ret==D3DERR_NOTAVAILABLE) {
			Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
			return nullptr;
		}

		// If ran out of texture ram, try invalidating some textures and mesh cache.
		if (ret==D3DERR_OUTOFVIDEOMEMORY) {
			WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets..."));
			// Free all textures that haven't been used in the last 5 seconds
			TextureClass::Invalidate_Old_Unused_Textures(5000);

			// Invalidate the mesh cache
			WW3D::_Invalidate_Mesh_Cache();

			ret=D3DXCreateTexture(
				DX8Wrapper::_Get_D3D_Device8(),
				width,
				height,
				mip_level_count,
				D3DUSAGE_RENDERTARGET,
				WW3DFormat_To_D3DFormat(format),
				pool,
				&texture);

			if (SUCCEEDED(ret)) {
				WWDEBUG_SAY(("...Render target creation successful."));
			}
			else {
				WWDEBUG_SAY(("...Render target creation failed."));
			}
			if (ret==D3DERR_OUTOFVIDEOMEMORY) {
				Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
				return nullptr;
			}
		}

		DX8_ErrorCode(ret);
		// Just return the texture, no reduction
		// allowed for render targets.
		return texture;
	}

	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateTexture(
		DX8Wrapper::_Get_D3D_Device8(),
		width,
		height,
		mip_level_count,
		0,
		WW3DFormat_To_D3DFormat(format),
		pool,
		&texture);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY) {
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateTexture(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			height,
			mip_level_count,
			0,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture);
		if (SUCCEEDED(ret)) {
			WWDEBUG_SAY(("...Texture creation successful."));
		}
		else {
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d",width,height,format_name.str(),mip_level_count));
		}

	}
	DX8_ErrorCode(ret);

	return texture;
}

#if defined(GGC_RENDER_BACKEND_BGFX)
// TheSuperHackers @refactor bobtista 22/04/2026 In standalone we replace
// D3DXCreateTextureFromFileExA with a direct
// Targa decoder + stub-device CreateTexture + LockRect write. The goal
// is to (a) remove D3DX as a black-box in the standalone pixel path so
// remaining visual bugs don't depend on D3DX internals interacting with
// our stub and (b) start the work of dropping d3dx8.lib from the link.
// The bgfx ownership path is blocked before reaching this helper.
static IDirect3DTexture8 * LoadTextureStandalone_TGA(
	const char * filename,
	MipCountType mip_level_count)
{
	Targa targa;
	if (targa.Open(filename, TGA_READMODE) != 0)
		return nullptr;

	// W3D uses Y-flipped TGA (D3D texels top-down).
	targa.Header.ImageDescriptor ^= TGAIDF_YORIGIN;

	WW3DFormat src_format = WW3D_FORMAT_UNKNOWN;
	unsigned src_bpp = 0;
	Get_WW3D_Format(src_format, src_bpp, targa);
	if (src_format == WW3D_FORMAT_UNKNOWN)
		return nullptr;

	const unsigned src_w = targa.Header.Width;
	const unsigned src_h = targa.Header.Height;
	if (src_w == 0 || src_h == 0)
		return nullptr;

	// Decide destination format: 32-bit TGA gets A8R8G8B8, 24-bit gets X8R8G8B8.
	// All other cases up-convert to A8R8G8B8 so the stub scratch layout (width*4)
	// matches what EnsureBgfxTexture expects.
	const bool has_alpha = (targa.Header.PixelDepth == 32)
		|| (src_format == WW3D_FORMAT_A8R8G8B8)
		|| (src_format == WW3D_FORMAT_A4R4G4B4)
		|| (src_format == WW3D_FORMAT_A1R5G5B5);
	const D3DFORMAT d3d_fmt = has_alpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;

	// How many mip levels do we actually produce? If caller asked for
	// MIP_LEVELS_1, just one; otherwise full chain down to 1x1.
	DWORD levels = 1;
	if (mip_level_count != MIP_LEVELS_1)
	{
		unsigned m = src_w > src_h ? src_w : src_h;
		while (m > 1) { m >>= 1; ++levels; }
	}

	IDirect3DTexture8 * texture = nullptr;
	HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->CreateTexture(
		src_w, src_h, levels, 0, d3d_fmt, D3DPOOL_MANAGED, &texture);
	if (hr != D3D_OK || texture == nullptr)
		return nullptr;

	// Decode file into an internally-allocated buffer owned by targa.
	// TGA class flips Y-orientation itself based on ImageDescriptor.
	if (targa.Load(filename, TGAF_IMAGE, false) != 0)
	{
		texture->Release();
		return nullptr;
	}

	const uint8_t * src = reinterpret_cast<const uint8_t *>(targa.GetImage());
	if (src == nullptr)
	{
		texture->Release();
		return nullptr;
	}

	D3DLOCKED_RECT locked = { 0 };
	if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
	{
		texture->Release();
		return nullptr;
	}

	// Convert/copy source pixels into the texture's level-0 scratch.
	// Targa memory layout is the same little-endian BGRA byte order as
	// D3D8 A8R8G8B8/X8R8G8B8 so we can straight-copy for 32-bit and
	// fill alpha=0xFF for 24-bit.
	const unsigned dst_pitch = static_cast<unsigned>(locked.Pitch);
	uint8_t * dst = static_cast<uint8_t *>(locked.pBits);
	if (src_bpp == 4)
	{
		for (unsigned y = 0; y < src_h; ++y)
		{
			std::memcpy(dst + y * dst_pitch, src + y * src_w * 4, src_w * 4);
		}
	}
	else if (src_bpp == 3)
	{
		for (unsigned y = 0; y < src_h; ++y)
		{
			const uint8_t * s = src + y * src_w * 3;
			uint8_t * d = dst + y * dst_pitch;
			for (unsigned x = 0; x < src_w; ++x)
			{
				d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 0xFF;
				s += 3; d += 4;
			}
		}
	}
	else
	{
		// Other bit depths (16-bit, paletted) — reject; D3DX fallback
		// will handle these rarer cases.
		texture->UnlockRect(0);
		texture->Release();
		return nullptr;
	}
	texture->UnlockRect(0);

	// Generate mip levels via 2x2 box filter (per channel independent).
	UINT prev_w = src_w;
	UINT prev_h = src_h;
	for (DWORD level = 1; level < levels; ++level)
	{
		UINT lw = prev_w >> 1; if (lw == 0) lw = 1;
		UINT lh = prev_h >> 1; if (lh == 0) lh = 1;

		D3DLOCKED_RECT src_l = { 0 };
		D3DLOCKED_RECT dst_l = { 0 };
		if (FAILED(texture->LockRect(level - 1, &src_l, nullptr, 0))) break;
		if (FAILED(texture->LockRect(level, &dst_l, nullptr, 0)))
		{
			texture->UnlockRect(level - 1);
			break;
		}

		// Clamp the second sample coordinate so 1D parent mips (width==1
		// or height==1) don't read past their row/column. Matches the
		// edge-aware box filter in StandaloneLegacyTextureOps.cpp.
		const UINT parent_w = prev_w;
		const UINT parent_h = prev_h;
		const uint8_t * spx = static_cast<const uint8_t *>(src_l.pBits);
		uint8_t * dpx = static_cast<uint8_t *>(dst_l.pBits);
		for (UINT y = 0; y < lh; ++y)
		{
			const UINT y0 = 2 * y;
			const UINT y1 = (y0 + 1 < parent_h) ? (y0 + 1) : y0;
			for (UINT x = 0; x < lw; ++x)
			{
				const UINT x0 = 2 * x;
				const UINT x1 = (x0 + 1 < parent_w) ? (x0 + 1) : x0;
				const uint8_t * p00 = spx + y0 * src_l.Pitch + x0 * 4;
				const uint8_t * p10 = spx + y0 * src_l.Pitch + x1 * 4;
				const uint8_t * p01 = spx + y1 * src_l.Pitch + x0 * 4;
				const uint8_t * p11 = spx + y1 * src_l.Pitch + x1 * 4;
				uint8_t * d = dpx + y * dst_l.Pitch + x * 4;
				d[0] = static_cast<uint8_t>((p00[0] + p10[0] + p01[0] + p11[0] + 2) >> 2);
				d[1] = static_cast<uint8_t>((p00[1] + p10[1] + p01[1] + p11[1] + 2) >> 2);
				d[2] = static_cast<uint8_t>((p00[2] + p10[2] + p01[2] + p11[2] + 2) >> 2);
				d[3] = static_cast<uint8_t>((p00[3] + p10[3] + p01[3] + p11[3] + 2) >> 2);
			}
		}
		texture->UnlockRect(level);
		texture->UnlockRect(level - 1);
		prev_w = lw;
		prev_h = lh;
	}

	return texture;
}

static bool HasTgaExtension(const char * filename)
{
	if (filename == nullptr) return false;
	const size_t n = std::strlen(filename);
	if (n < 4) return false;
	const char * ext = filename + n - 4;
	return (ext[0] == '.') &&
		(ext[1] == 't' || ext[1] == 'T') &&
		(ext[2] == 'g' || ext[2] == 'G') &&
		(ext[3] == 'a' || ext[3] == 'A');
}
#endif // GGC_RENDER_BACKEND_BGFX

static HRESULT Create_Legacy_Cube_Texture_Compat(
	LPDIRECT3DDEVICE8 device,
	UINT size,
	UINT mip_level_count,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	LPDIRECT3DCUBETEXTURE8 *out_texture)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	if (device == nullptr || out_texture == nullptr)
	{
		return E_POINTER;
	}
	if (mip_level_count == D3DX_DEFAULT)
	{
		mip_level_count = 0;
	}
	if (format == D3DFMT_UNKNOWN)
	{
		format = D3DFMT_A8R8G8B8;
	}
	return device->CreateCubeTexture(size, mip_level_count, usage, format, pool, out_texture);
#else
	return D3DXCreateCubeTexture(device, size, mip_level_count, usage, format, pool, out_texture);
#endif
}

static HRESULT Create_Legacy_Volume_Texture_Compat(
	LPDIRECT3DDEVICE8 device,
	UINT width,
	UINT height,
	UINT depth,
	UINT mip_level_count,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	LPDIRECT3DVOLUMETEXTURE8 *out_texture)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	if (device == nullptr || out_texture == nullptr)
	{
		return E_POINTER;
	}
	if (mip_level_count == D3DX_DEFAULT)
	{
		mip_level_count = 0;
	}
	if (format == D3DFMT_UNKNOWN)
	{
		format = D3DFMT_A8R8G8B8;
	}
	return device->CreateVolumeTexture(width, height, depth, mip_level_count, usage, format, pool, out_texture);
#else
	return D3DXCreateVolumeTexture(device, width, height, depth, mip_level_count, usage, format, pool, out_texture);
#endif
}

IDirect3DTexture8 * DX8Wrapper::_Create_DX8_Texture
(
	const char *filename,
	MipCountType mip_level_count
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture8 *texture = nullptr;

#if defined(GGC_RENDER_BACKEND_BGFX)
	// Bypass D3DX for TGA files in standalone. The D3DX upload path
	// occasionally produced bad pixel data against our stub device
	// (unknown internal cause) which showed as dark bands / black
	// regions on terrain. Direct TGA -> stub LockRect is deterministic.
	if (HasTgaExtension(filename))
	{
		texture = LoadTextureStandalone_TGA(filename, mip_level_count);
		if (texture != nullptr)
		{
			D3DSURFACE_DESC desc;
			texture->GetLevelDesc(0, &desc);
			if (desc.Format == D3DFMT_P8) {
				Log_Missing_Texture_File("paletted TGA", filename);
				texture->Release();
				return Get_Legacy_Missing_Texture();
			}
			return texture;
		}
	}

	WWASSERT_PRINT(
		false,
		"DX8Wrapper::_Create_DX8_Texture(file): standalone bgfx legacy texture path cannot load this file; no D3DX fallback is available");
	Log_Missing_Texture_File("standalone legacy texture loader", filename);
	return Get_Legacy_Missing_Texture();
#else

	// NOTE: If the original image format is not supported as a texture format, it will
	// automatically be converted to an appropriate format.
	// NOTE: It is possible to get the size and format of the original image file from this
	// function as well, so if we later want to second-guess D3DX's format conversion decisions
	// we can do so after this function is called..
	unsigned result = D3DXCreateTextureFromFileExA(
		_Get_D3D_Device8(),
		filename,
		D3DX_DEFAULT,
		D3DX_DEFAULT,
		mip_level_count,//create_mipmaps ? 0 : 1,
		0,
		D3DFMT_UNKNOWN,
		D3DPOOL_MANAGED,
		D3DX_FILTER_BOX,
		D3DX_FILTER_BOX,
		0,
		nullptr,
		nullptr,
		&texture);

	if (result != D3D_OK) {
		Log_Missing_Texture_File("D3DX fallback", filename);
		return Get_Legacy_Missing_Texture();
	}

	// Make sure texture wasn't paletted!
	D3DSURFACE_DESC desc;
	texture->GetLevelDesc(0,&desc);
	if (desc.Format==D3DFMT_P8) {
		Log_Missing_Texture_File("paletted D3DX", filename);
		texture->Release();
		return Get_Legacy_Missing_Texture();
	}
	return texture;
#endif
}

IDirect3DTexture8 * DX8Wrapper::_Create_DX8_Texture
(
	IDirect3DSurface8 *surface,
	MipCountType mip_level_count
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture8 *texture = nullptr;

	D3DSURFACE_DESC surface_desc;
	::ZeroMemory(&surface_desc, sizeof(D3DSURFACE_DESC));
	surface->GetDesc(&surface_desc);

	// This function will create a texture with a different (but similar) format if the surface is
	// not in a supported texture format.
	WW3DFormat format=D3DFormat_To_WW3DFormat(surface_desc.Format);
	texture = _Create_DX8_Texture(surface_desc.Width, surface_desc.Height, format, mip_level_count);

	// Copy the surface to the texture
	IDirect3DSurface8 *tex_surface = nullptr;
	texture->GetSurfaceLevel(0, &tex_surface);
	DX8_ErrorCode(Copy_Legacy_Surface_Compat(tex_surface, nullptr, surface, nullptr, D3DX_FILTER_BOX));
	tex_surface->Release();

	// Create mipmaps if needed
	if (mip_level_count!=MIP_LEVELS_1)
	{
		DX8_ErrorCode(Filter_Legacy_Texture_Mips_Compat(texture, 0));
	}

	return texture;

}

/*!
 * KJM create depth stencil texture
 */
IDirect3DTexture8 * DX8Wrapper::_Create_DX8_ZTexture
(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	D3DPOOL pool
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture8* texture = nullptr;

	D3DFORMAT zfmt=WW3DZFormat_To_D3DFormat(zformat);

	unsigned ret=DX8Wrapper::_Get_D3D_Device8()->CreateTexture
	(
		width,
		height,
		mip_level_count,
		D3DUSAGE_DEPTHSTENCIL,
		zfmt,
		pool,
		&texture
	);

	if (ret==D3DERR_NOTAVAILABLE)
	{
		Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
		return nullptr;
	}

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY)
	{
		WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=DX8Wrapper::_Get_D3D_Device8()->CreateTexture
		(
			width,
			height,
			mip_level_count,
			D3DUSAGE_DEPTHSTENCIL,
			zfmt,
			pool,
			&texture
		);

		if (SUCCEEDED(ret))
		{
			WWDEBUG_SAY(("...Render target creation successful."));
		}
		else
		{
			WWDEBUG_SAY(("...Render target creation failed."));
		}
		if (ret==D3DERR_OUTOFVIDEOMEMORY)
		{
			Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
			return nullptr;
		}
	}

	DX8_ErrorCode(ret);

	texture->AddRef(); // don't release this texture

	// Just return the texture, no reduction
	// allowed for render targets.

	return texture;
}

/*!
 * KJM create cube map texture
 */
IDirect3DCubeTexture8* DX8Wrapper::_Create_DX8_Cube_Texture
(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool,
	bool rendertarget
)
{
	WWASSERT(width==height);
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DCubeTexture8* texture=nullptr;

#if defined(GGC_RENDER_BACKEND_BGFX)
	if (Blocks_Legacy_Texture_Create())
	{
		WWASSERT_PRINT(
			false,
			"DX8Wrapper::_Create_DX8_Cube_Texture: BGFX texture ownership is enabled; no fake-D3D cube texture fallback is allowed");
		return nullptr;
	}
#endif

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.

	// Render target may return NOTAVAILABLE, in
	// which case we return null.
	if (rendertarget)
	{
		unsigned ret=Create_Legacy_Cube_Texture_Compat(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			mip_level_count,
			D3DUSAGE_RENDERTARGET,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture
		);

		if (ret==D3DERR_NOTAVAILABLE)
		{
			Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
			return nullptr;
		}

		// If ran out of texture ram, try invalidating some textures and mesh cache.
		if (ret==D3DERR_OUTOFVIDEOMEMORY)
		{
			WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets..."));
			// Free all textures that haven't been used in the last 5 seconds
			TextureClass::Invalidate_Old_Unused_Textures(5000);

			// Invalidate the mesh cache
			WW3D::_Invalidate_Mesh_Cache();

			ret=Create_Legacy_Cube_Texture_Compat(
				DX8Wrapper::_Get_D3D_Device8(),
				width,
				mip_level_count,
				D3DUSAGE_RENDERTARGET,
				WW3DFormat_To_D3DFormat(format),
				pool,
				&texture
			);

			if (SUCCEEDED(ret))
			{
				WWDEBUG_SAY(("...Render target creation successful."));
			}
			else
			{
				WWDEBUG_SAY(("...Render target creation failed."));
			}
			if (ret==D3DERR_OUTOFVIDEOMEMORY)
			{
				Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
				return nullptr;
			}
		}

		DX8_ErrorCode(ret);
		// Just return the texture, no reduction
		// allowed for render targets.
		return texture;
	}

	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=Create_Legacy_Cube_Texture_Compat(
		DX8Wrapper::_Get_D3D_Device8(),
		width,
		mip_level_count,
		0,
		WW3DFormat_To_D3DFormat(format),
		pool,
		&texture
	);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY)
	{
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=Create_Legacy_Cube_Texture_Compat(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			mip_level_count,
			0,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture
		);
		if (SUCCEEDED(ret))
		{
			WWDEBUG_SAY(("...Texture creation successful."));
		}
		else
		{
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d",width,height,format_name.str(),mip_level_count));
		}

	}
	DX8_ErrorCode(ret);

	return texture;
}

/*!
 * KJM create volume texture
 */
IDirect3DVolumeTexture8* DX8Wrapper::_Create_DX8_Volume_Texture
(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DVolumeTexture8* texture=nullptr;

#if defined(GGC_RENDER_BACKEND_BGFX)
	if (Blocks_Legacy_Texture_Create())
	{
		WWASSERT_PRINT(
			false,
			"DX8Wrapper::_Create_DX8_Volume_Texture: BGFX texture ownership is enabled; no fake-D3D volume texture fallback is allowed");
		return nullptr;
	}
#endif

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.


	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=Create_Legacy_Volume_Texture_Compat(
		DX8Wrapper::_Get_D3D_Device8(),
		width,
		height,
		depth,
		mip_level_count,
		0,
		WW3DFormat_To_D3DFormat(format),
		pool,
		&texture
	);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY)
	{
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets..."));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=Create_Legacy_Volume_Texture_Compat(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			height,
			depth,
			mip_level_count,
			0,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture
		);
		if (SUCCEEDED(ret))
		{
			WWDEBUG_SAY(("...Texture creation successful."));
		}
		else
		{
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d",width,height,format_name.str(),mip_level_count));
		}

	}
	DX8_ErrorCode(ret);

	return texture;
}


IDirect3DSurface8 * DX8Wrapper::_Create_DX8_Surface(unsigned int width, unsigned int height, WW3DFormat format)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();

	IDirect3DSurface8 *surface = nullptr;

	// Paletted surfaces not supported!
	WWASSERT(format!=D3DFMT_P8);

	DX8CALL(CreateImageSurface(width, height, WW3DFormat_To_D3DFormat(format), &surface));

	return surface;
}

IDirect3DSurface8 * DX8Wrapper::_Create_DX8_Surface(const char *filename_)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();

	// Note: Since there is no "D3DXCreateSurfaceFromFile" and no "GetSurfaceInfoFromFile" (the
	// latter is supposed to be added to D3DX in a future version), we create a texture from the
	// file (w/o mipmaps), check that its surface is equal to the original file data (which it
	// will not be if the file is not in a texture-supported format or size). If so, copy its
	// surface (we might be able to just get its surface and add a ref to it but I'm not sure so
	// I'm not going to risk it) and release the texture. If not, create a surface according to
	// the file data and use D3DXLoadSurfaceFromFile. This is a horrible hack, but it saves us
	// having to write file loaders. Will fix this when D3DX provides us with the right functions.
	// Create a surface the size of the file image data
	IDirect3DSurface8 *surface = nullptr;

	{

		file_auto_ptr myfile(_TheFileFactory,filename_);
		// If file not found, create a surface with missing texture in it

		if (!myfile->Is_Available()) {
			// If file not found, try the dds format
			// else create a surface with missing texture in it
			char compressed_name[200];
			strlcpy(compressed_name,filename_, sizeof(compressed_name));
			char *ext = strstr(compressed_name, ".");
			if ( ext && (strlen(ext)==4) &&
				  ( (ext[1] == 't') || (ext[1] == 'T') ) &&
				  ( (ext[2] == 'g') || (ext[2] == 'G') ) &&
				  ( (ext[3] == 'a') || (ext[3] == 'A') ) ) {
				ext[1]='d';
				ext[2]='d';
				ext[3]='s';
			}
			file_auto_ptr myfile2(_TheFileFactory,compressed_name);
			if (!myfile2->Is_Available()) {
				Log_Missing_Texture_File("surface file", filename_);
				return Create_Legacy_Missing_Surface();
			}
		}
	}

	StringClass filename_string(filename_,true);
	surface=Load_Legacy_Surface_Immediate(
		filename_string,
		WW3D_FORMAT_UNKNOWN,
		true);
	return surface;
}
#endif


/***********************************************************************************************
 * DX8Wrapper::_Update_Texture -- Copies a texture from system memory to video memory          *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/26/2001  hy : Created.                                                                  *
 *=============================================================================================*/
#if !defined(GGC_RENDER_BACKEND_BGFX)
void DX8Wrapper::_Update_Texture(TextureClass *system, TextureClass *video)
{
	WWASSERT(system);
	WWASSERT(video);
	WWASSERT(system->Get_Pool()==TextureClass::POOL_SYSTEMMEM);
	WWASSERT(video->Get_Pool()==TextureClass::POOL_DEFAULT);
	DX8CALL(UpdateTexture(Peek_Legacy_Base_Texture(*system),Peek_Legacy_Base_Texture(*video)));
}
#endif

void DX8Wrapper::Compute_Caps(WW3DFormat display_format)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	delete CurrentCaps;
#if defined(GGC_RENDER_BACKEND_BGFX)
	D3DCAPS8 caps;
	Fill_Standalone_DX8_Caps(caps);
	CurrentCaps=new DX8Caps(nullptr,static_cast<const void*>(&caps),display_format,&CurrentAdapterIdentifier);
#else
	CurrentCaps=new DX8Caps(D3DInterface,D3DDevice,display_format,&CurrentAdapterIdentifier);
#endif
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
void DX8Wrapper::Set_Light(unsigned index, const D3DLIGHT8* light)
{
	if (light) {
		FixedFunctionState::Render_State().Lights[index]=To_Legacy_Light(*light);
		FixedFunctionState::Render_State().LightEnable[index]=true;
	}
	else {
		FixedFunctionState::Render_State().LightEnable[index]=false;
	}
	FixedFunctionState::Changed_Mask()|=(LIGHT0_CHANGED<<index);
}

void DX8Wrapper::Set_Light(unsigned index,const LightClass &light)
{
	D3DLIGHT8 dlight;
	Vector3 temp;
	memset(&dlight,0,sizeof(D3DLIGHT8));

	switch (light.Get_Type())
	{
	case LightClass::POINT:
		{
			dlight.Type=D3DLIGHT_POINT;
		}
		break;
	case LightClass::DIRECTIONAL:
		{
			dlight.Type=D3DLIGHT_DIRECTIONAL;
		}
		break;
	case LightClass::SPOT:
		{
			dlight.Type=D3DLIGHT_SPOT;
		}
		break;
	}

	light.Get_Diffuse(&temp);
	temp*=light.Get_Intensity();
	dlight.Diffuse.r=temp.X;
	dlight.Diffuse.g=temp.Y;
	dlight.Diffuse.b=temp.Z;
	dlight.Diffuse.a=1.0f;

	light.Get_Specular(&temp);
	temp*=light.Get_Intensity();
	dlight.Specular.r=temp.X;
	dlight.Specular.g=temp.Y;
	dlight.Specular.b=temp.Z;
	dlight.Specular.a=1.0f;

	light.Get_Ambient(&temp);
	temp*=light.Get_Intensity();
	dlight.Ambient.r=temp.X;
	dlight.Ambient.g=temp.Y;
	dlight.Ambient.b=temp.Z;
	dlight.Ambient.a=1.0f;

	temp=light.Get_Position();
	dlight.Position=*(D3DVECTOR*) &temp;

	light.Get_Spot_Direction(temp);
	dlight.Direction=*(D3DVECTOR*) &temp;

	dlight.Range=light.Get_Attenuation_Range();
	dlight.Falloff=light.Get_Spot_Exponent();
	dlight.Theta=light.Get_Spot_Angle();
	dlight.Phi=light.Get_Spot_Angle();

	// Inverse linear light 1/(1+D)
	double a,b;
	light.Get_Far_Attenuation_Range(a,b);
	dlight.Attenuation0=1.0f;
	if (fabs(a-b)<1e-5)
		// if the attenuation range is too small assume uniform with cutoff
		dlight.Attenuation1=0.0f;
	else
		// this will cause the light to drop to half intensity at the first far attenuation
		dlight.Attenuation1=(float) 1.0/a;
	dlight.Attenuation2=0.0f;

	Set_Light(index,&dlight);
}

//**********************************************************************************************
//! Set the light environment. This is a lighting model which used up to four
//! directional lights to produce the lighting.
/*! 5/27/02 KJM Added shader light environment support
*/
void DX8Wrapper::Set_Light_Environment(LightEnvironmentClass* light_env)
{
	// Shader light environment support															*
	Light_Environment=light_env;

	if (light_env)
	{
		if (std::getenv("GGC_LIGHT_ENV_DIAG") != nullptr)
		{
			static unsigned s_leDiag = 0;
			if (s_leDiag < 5)
			{
				s_leDiag++;
				const Vector3 & eqa = light_env->Get_Equivalent_Ambient();
				std::fprintf(stderr, "LIGHT_ENV: eqAmb=[%.3f %.3f %.3f] lights=%d\n",
					eqa.X, eqa.Y, eqa.Z, light_env->Get_Light_Count());
			}
		}
		int light_count = light_env->Get_Light_Count();
		unsigned int color=Convert_Color(light_env->Get_Equivalent_Ambient(),0.0f);
		if (RenderStateCache::Get_Render_State(RS::AMBIENT)!=color)
		{
			Commit_Fixed_Function_Render_Value(RS::AMBIENT,color);
//buggy Radeon 9700 driver doesn't apply new ambient unless the material also changes.
#if 1
			FixedFunctionState::Changed_Mask()|=MATERIAL_CHANGED;
#endif
		}

		D3DLIGHT8 light;
		int l=0;
		for (;l<light_count;++l) {

			::ZeroMemory(&light, sizeof(D3DLIGHT8));

			light.Type=D3DLIGHT_DIRECTIONAL;
			(Vector3&)light.Diffuse=light_env->Get_Light_Diffuse(l);
			Vector3 dir=-light_env->Get_Light_Direction(l);
			light.Direction=(const D3DVECTOR&)(dir);

			// (gth) TODO: put specular into LightEnvironment?  Much work to be done on lights :-)'
			if (l==0) {
				light.Specular.r = light.Specular.g = light.Specular.b = 1.0f;
			}

			if (light_env->isPointLight(l)) {
				light.Type = D3DLIGHT_POINT;
				(Vector3&)light.Diffuse=light_env->getPointDiffuse(l);
				(Vector3&)light.Ambient=light_env->getPointAmbient(l);
				light.Position = (const D3DVECTOR&)light_env->getPointCenter(l);
				light.Range = light_env->getPointOrad(l);

				// Inverse linear light 1/(1+D)
				double a,b;
				b = light_env->getPointOrad(l);
				a = light_env->getPointIrad(l);

//(gth) CNC3 Generals code for the attenuation factors is causing the lights to over-brighten
//I'm changing the Attenuation0 parameter to 1.0 to avoid this problem.
#if 0
				light.Attenuation0=0.01f;
#else
				light.Attenuation0=1.0f;
#endif
				if (fabs(a-b)<1e-5)
					// if the attenuation range is too small assume uniform with cutoff
					light.Attenuation1=0.0f;
				else
					// this will cause the light to drop to half intensity at the first far attenuation
					light.Attenuation1=(float) 0.1/a;

				light.Attenuation2=8.0f/(b*b);
			}

			Set_Light(l,&light);
		}

		for (;l<4;++l) {
			Set_Light(l,nullptr);
		}
	}
/*	else {
		for (int l=0;l<4;++l) {
			Set_Light(l,nullptr);
		}
	}
*/
}
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
IDirect3DSurface8 * DX8Wrapper::_Get_DX8_Front_Buffer()
{
	DX8_THREAD_ASSERT();
	D3DDISPLAYMODE mode;

	DX8CALL(GetDisplayMode(&mode));

	IDirect3DSurface8 * fb=nullptr;

	DX8CALL(CreateImageSurface(mode.Width,mode.Height,D3DFMT_A8R8G8B8,&fb));

	DX8CALL(GetFrontBuffer(fb));
	return fb;
}

SurfaceClass * DX8Wrapper::_Get_DX8_Back_Buffer(unsigned int num)
{
	DX8_THREAD_ASSERT();

	IDirect3DSurface8 * bb;
	SurfaceClass *surf=nullptr;
	DX8CALL(GetBackBuffer(num,D3DBACKBUFFER_TYPE_MONO,&bb));
	if (bb)
	{
		surf=Create_Legacy_Surface_Wrapper(bb);
		bb->Release();
	}

	return surf;
}


TextureClass *
DX8Wrapper::Create_Render_Target (int width, int height, WW3DFormat format)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	DX8_RECORD_DX8_CALLS();

	// Use the current display format if format isn't specified
	if (format==WW3D_FORMAT_UNKNOWN) {
		D3DDISPLAYMODE mode;
		DX8CALL(GetDisplayMode(&mode));
		format=D3DFormat_To_WW3DFormat(mode.Format);
	}

	// If render target format isn't supported return null
	if (!Get_Current_Caps()->Support_Render_To_Texture_Format(format)) {
		WWDEBUG_SAY(("DX8Wrapper - Render target format is not supported"));
		return nullptr;
	}

	//
	//	Note: We're going to force the width and height to be powers of two and equal
	//
	const DX8Caps* dx8caps=Get_Current_Caps();
	float poweroftwosize = width;
	if (height > 0 && height < width) {
		poweroftwosize = height;
	}
	poweroftwosize = ::Find_POT (poweroftwosize);

	if (poweroftwosize>dx8caps->Get_Max_Texture_Width()) {
		poweroftwosize=dx8caps->Get_Max_Texture_Width();
	}
	if (poweroftwosize>dx8caps->Get_Max_Texture_Height()) {
		poweroftwosize=dx8caps->Get_Max_Texture_Height();
	}

	width = height = poweroftwosize;

	//
	//	Attempt to create the render target
	//
	TextureClass * tex = NEW_REF(TextureClass,(width,height,format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT,true));

	// 3dfx drivers are lying in the CheckDeviceFormat call and claiming
	// that they support render targets!
	if (Peek_Legacy_Base_Texture(*tex) == nullptr)
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target creation failed!"));
		REF_PTR_RELEASE(tex);
	}

	return tex;
}

//**********************************************************************************************
//! Create render target with associated depth stencil buffer
/*! KJM
*/
void DX8Wrapper::Create_Render_Target
(
	int width,
	int height,
	WW3DFormat format,
	WW3DZFormat zformat,
	TextureClass** target,
	ZTextureClass** depth_buffer
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	DX8_RECORD_DX8_CALLS();

	// Use the current display format if format isn't specified
	if (format==WW3D_FORMAT_UNKNOWN)
	{
		*target=nullptr;
		*depth_buffer=nullptr;
		return;
/*		D3DDISPLAYMODE mode;
		DX8CALL(GetDisplayMode(&mode));
		format=D3DFormat_To_WW3DFormat(mode.Format);*/
	}

	// If render target format isn't supported return null
	if (!Get_Current_Caps()->Support_Render_To_Texture_Format(format) ||
		 !Get_Current_Caps()->Support_Depth_Stencil_Format(zformat))
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target with depth format is not supported"));
		return;
	}

	//	Note: We're going to force the width and height to be powers of two and equal
	const DX8Caps* dx8caps=Get_Current_Caps();
	float poweroftwosize = width;
	if (height > 0 && height < width)
	{
		poweroftwosize = height;
	}
	poweroftwosize = ::Find_POT (poweroftwosize);

	if (poweroftwosize>dx8caps->Get_Max_Texture_Width())
	{
		poweroftwosize=dx8caps->Get_Max_Texture_Width();
	}

	if (poweroftwosize>dx8caps->Get_Max_Texture_Height())
	{
		poweroftwosize=dx8caps->Get_Max_Texture_Height();
	}

	width = height = poweroftwosize;

	//	Attempt to create the render target
	TextureClass* tex=NEW_REF(TextureClass,(width,height,format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT,true));

	// 3dfx drivers are lying in the CheckDeviceFormat call and claiming
	// that they support render targets!
	if (Peek_Legacy_Base_Texture(*tex) == nullptr)
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target creation failed!"));
		REF_PTR_RELEASE(tex);
	}

	*target=tex;

	// attempt to create the depth stencil buffer
	*depth_buffer=NEW_REF
	(
		ZTextureClass,
		(
			width,
			height,
			zformat,
			MIP_LEVELS_1,
			TextureClass::POOL_DEFAULT
		)
	);
}

/*!
 * Set render target
 * KM Added optional custom z target
 */
void DX8Wrapper::Set_Render_Target_With_Z
(
	TextureClass* texture,
	ZTextureClass* ztexture
)
{
	WWASSERT(texture!=nullptr);
	IDirect3DSurface8 * d3d_surf = Get_Native_Compatibility_Surface_Level(*texture);
	WWASSERT(d3d_surf != nullptr);

	IDirect3DSurface8* d3d_zbuf=nullptr;
	if (ztexture!=nullptr)
	{

		d3d_zbuf=Get_Native_Compatibility_Surface_Level(*ztexture);
		WWASSERT(d3d_zbuf!=nullptr);
		Set_Render_Target(d3d_surf,d3d_zbuf);
		d3d_zbuf->Release();
	}
	else
	{
		Set_Render_Target(d3d_surf,true);
	}
	d3d_surf->Release();

	IsRenderToTexture = true;
}

void
DX8Wrapper::Set_Render_Target(IDirect3DSwapChain8 *swap_chain)
{
	DX8_THREAD_ASSERT();
	WWASSERT (swap_chain != nullptr);

	//
	//	Get the back buffer for the swap chain
	//
	LPDIRECT3DSURFACE8 render_target = nullptr;
	swap_chain->GetBackBuffer (0, D3DBACKBUFFER_TYPE_MONO, &render_target);

	//
	//	Set this back buffer as the render target
	//
	Set_Render_Target (render_target, true);

	//
	//	Release our hold on the back buffer
	//
	if (render_target != nullptr) {
		render_target->Release ();
		render_target = nullptr;
	}

	IsRenderToTexture = false;
}

void
DX8Wrapper::Set_Render_Target(IDirect3DSurface8 *render_target, bool use_default_depth_buffer)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();

	//
	//	Should we restore the default render target set a new one?
	//
	if (render_target == nullptr || render_target == DefaultRenderTarget)
	{
		// If there is currently a custom render target, default must NOT be null.
		if (CurrentRenderTarget)
		{
			WWASSERT(DefaultRenderTarget!=nullptr);
		}

		//
		//	Restore the default render target
		//
		if (DefaultRenderTarget != nullptr)
		{
			DX8CALL(SetRenderTarget (DefaultRenderTarget, DefaultDepthBuffer));
			DefaultRenderTarget->Release ();
			DefaultRenderTarget = nullptr;
			if (DefaultDepthBuffer)
			{
				DefaultDepthBuffer->Release ();
				DefaultDepthBuffer = nullptr;
			}
		}

		//
		//	Release our hold on the "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}

	}
	else if (render_target != CurrentRenderTarget)
	{
		WWASSERT(DefaultRenderTarget==nullptr);

		//
		//	We'll need the depth buffer later...
		//
		if (DefaultDepthBuffer == nullptr)
		{
//		IDirect3DSurface8 *depth_buffer = nullptr;
			DX8CALL(GetDepthStencilSurface (&DefaultDepthBuffer));
		}

		//
		//	Get a pointer to the default render target (if necessary)
		//
		if (DefaultRenderTarget == nullptr)
		{
			DX8CALL(GetRenderTarget (&DefaultRenderTarget));
		}

		//
		//	Release our hold on the old "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}

		//
		//	Keep a copy of the current render target (for housekeeping)
		//
		CurrentRenderTarget = render_target;
		WWASSERT (CurrentRenderTarget != nullptr);
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->AddRef ();

			//
			//	Switch render targets
			//
			if (use_default_depth_buffer)
			{
				DX8CALL(SetRenderTarget (CurrentRenderTarget, DefaultDepthBuffer));
			}
			else
			{
				DX8CALL(SetRenderTarget (CurrentRenderTarget, nullptr));
			}
		}
	}

	//
	//	Free our hold on the depth buffer
	//
//	if (depth_buffer != nullptr) {
//		depth_buffer->Release ();
//		depth_buffer = nullptr;
//	}

	IsRenderToTexture = false;
}


//**********************************************************************************************
//! Set render target with depth stencil buffer
/*! KJM
*/
void DX8Wrapper::Set_Render_Target
(
	IDirect3DSurface8* render_target,
	IDirect3DSurface8* depth_buffer
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();

	//
	//	Should we restore the default render target set a new one?
	//
	if (render_target == nullptr || render_target == DefaultRenderTarget)
	{
		// If there is currently a custom render target, default must NOT be null.
		if (CurrentRenderTarget)
		{
			WWASSERT(DefaultRenderTarget!=nullptr);
		}

		//
		//	Restore the default render target
		//
		if (DefaultRenderTarget != nullptr)
		{
			DX8CALL(SetRenderTarget (DefaultRenderTarget, DefaultDepthBuffer));
			DefaultRenderTarget->Release ();
			DefaultRenderTarget = nullptr;
			if (DefaultDepthBuffer)
			{
				DefaultDepthBuffer->Release ();
				DefaultDepthBuffer = nullptr;
			}
		}

		//
		//	Release our hold on the "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}
	}
	else if (render_target != CurrentRenderTarget)
	{
		WWASSERT(DefaultRenderTarget==nullptr);

		//
		//	We'll need the depth buffer later...
		//
		if (DefaultDepthBuffer == nullptr)
		{
//		IDirect3DSurface8 *depth_buffer = nullptr;
			DX8CALL(GetDepthStencilSurface (&DefaultDepthBuffer));
		}

		//
		//	Get a pointer to the default render target (if necessary)
		//
		if (DefaultRenderTarget == nullptr)
		{
			DX8CALL(GetRenderTarget (&DefaultRenderTarget));
		}

		//
		//	Release our hold on the old "current" render target
		//
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = nullptr;
		}

		if (CurrentDepthBuffer!=nullptr)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=nullptr;
		}

		//
		//	Keep a copy of the current render target (for housekeeping)
		//
		CurrentRenderTarget = render_target;
		CurrentDepthBuffer = depth_buffer;
		WWASSERT (CurrentRenderTarget != nullptr);
		if (CurrentRenderTarget != nullptr)
		{
			CurrentRenderTarget->AddRef ();
			CurrentDepthBuffer->AddRef();

			//
			//	Switch render targets
			//
			DX8CALL(SetRenderTarget (CurrentRenderTarget, CurrentDepthBuffer));
		}
	}

	IsRenderToTexture=true;
}


void DX8Wrapper::Flush_DX8_Resource_Manager(unsigned int bytes)
{
	DX8_Assert();
	DX8CALL(ResourceManagerDiscardBytes(bytes));
}

unsigned int DX8Wrapper::Get_Free_Texture_RAM()
{
	DX8_Assert();
	DX8_RECORD_DX8_CALLS();
	return DX8Wrapper::_Get_D3D_Device8()->GetAvailableTextureMem();
}
#endif

// Converts a linear gamma ramp to one that is controlled by:
// Gamma - controls the curvature of the middle of the curve
// Bright - controls the minimum value of the curve
// Contrast - controls the difference between the maximum and the minimum of the curve
void DX8Wrapper::Set_Gamma(float gamma,float bright,float contrast,bool calibrate,bool uselimit)
{
#if !defined(GGC_RENDER_BACKEND_BGFX)
	gamma=Bound(gamma,0.6f,6.0f);
	bright=Bound(bright,-0.5f,0.5f);
	contrast=Bound(contrast,0.5f,2.0f);
	float oo_gamma=1.0f/gamma;

	DX8_Assert();
	DX8_RECORD_DX8_CALLS();

	DWORD flag=(calibrate?D3DSGR_CALIBRATE:D3DSGR_NO_CALIBRATION);

	D3DGAMMARAMP ramp;
	float			 limit;

	// IML: I'm not really sure what the intent of the 'limit' variable is. It does not produce useful results for my purposes.
	if (uselimit) {
		limit=(contrast-1)/2*contrast;
	} else {
		limit = 0.0f;
	}

	// HY - arrived at this equation after much trial and error.
	for (int i=0; i<256; i++) {
		float in,out;
		in=i/256.0f;
		float x=in-limit;
		x=Bound(x,0.0f,1.0f);
		x=powf(x,oo_gamma);
		out=contrast*x+bright;
		out=Bound(out,0.0f,1.0f);
		ramp.red[i]=(WORD) (out*65535);
		ramp.green[i]=(WORD) (out*65535);
		ramp.blue[i]=(WORD) (out*65535);
	}

	if (Get_Current_Caps()->Support_Gamma())	{
		DX8Wrapper::_Get_D3D_Device8()->SetGammaRamp(flag,&ramp);
	} else {
		HWND hwnd = GetDesktopWindow();
		HDC hdc = GetDC(hwnd);
		if (hdc)
		{
			SetDeviceGammaRamp (hdc, &ramp);
			ReleaseDC (hwnd, hdc);
		}
	}
#endif
}

void DX8Wrapper::Set_World_Identity()
{
	FixedFunctionState::Set_World_Identity();
}

void DX8Wrapper::Set_View_Identity()
{
	FixedFunctionState::Set_View_Identity();
}

//**********************************************************************************************
//! Resets render device to default state
/*!
*/
void DX8Wrapper::Apply_Default_State()
{
	SNAPSHOT_SAY(("DX8Wrapper::Apply_Default_State()"));

	// only set states used in game
	Commit_Fixed_Function_Render_Value(7, TRUE);
	Commit_Fixed_Function_Render_Value(9, 2);
	Commit_Fixed_Function_Render_Value(14, TRUE);
	Commit_Fixed_Function_Render_Value(15, FALSE);
	Commit_Fixed_Function_Render_Value(19, 2);
	Commit_Fixed_Function_Render_Value(20, 1);
	Commit_Fixed_Function_Render_Value(22, 2);
	Commit_Fixed_Function_Render_Value(23, 4);
	Commit_Fixed_Function_Render_Value(24, 0);
	Commit_Fixed_Function_Render_Value(25, 4);
	Commit_Fixed_Function_Render_Value(26, FALSE);
	Commit_Fixed_Function_Render_Value(27, FALSE);
	Commit_Fixed_Function_Render_Value(28, FALSE);
	Commit_Fixed_Function_Render_Value(29, FALSE);
	Commit_Fixed_Function_Render_Value(47, 0);
	Commit_Fixed_Function_Render_Value(52, FALSE);
	Commit_Fixed_Function_Render_Value(53, 1);
	Commit_Fixed_Function_Render_Value(54, 1);
	Commit_Fixed_Function_Render_Value(55, 1);
	Commit_Fixed_Function_Render_Value(56, 8);
	Commit_Fixed_Function_Render_Value(57, 0);
	Commit_Fixed_Function_Render_Value(58, 0xffffffff);
	Commit_Fixed_Function_Render_Value(59, 0xffffffff);
	Commit_Fixed_Function_Render_Value(60, 0);
	Commit_Fixed_Function_Render_Value(136, TRUE);
	Commit_Fixed_Function_Render_Value(137, FALSE);
	Commit_Fixed_Function_Render_Value(141, TRUE);
	Commit_Fixed_Function_Render_Value(153, FALSE);
	Commit_Fixed_Function_Render_Value(168, 0x0000000f);
	Commit_Fixed_Function_Render_Value(171, 1);

	// disable TSS stages
	int i;
	for (i=0; i<CurrentCaps->Get_Max_Textures_Per_Pass(); i++)
	{
		Commit_Fixed_Function_Texture_Stage_Value(i, 1, 1);
		Commit_Fixed_Function_Texture_Stage_Value(i, 2, 2);
		Commit_Fixed_Function_Texture_Stage_Value(i, 3, 0);

		Commit_Fixed_Function_Texture_Stage_Value(i, 4, 1);
		Commit_Fixed_Function_Texture_Stage_Value(i, 5, 2);
		Commit_Fixed_Function_Texture_Stage_Value(i, 6, 0);

		Commit_Fixed_Function_Texture_Stage_Value(i, 11, i);

		Commit_Fixed_Function_Texture_Stage_Value(i, 13, 1);
		Commit_Fixed_Function_Texture_Stage_Value(i, 14, 1);
		Commit_Fixed_Function_Texture_Stage_Value(i, 15, 0);

		Commit_Fixed_Function_Texture_Stage_Value(i, 24, 0);
		Commit_Fixed_Function_Texture(i,nullptr);
	}

	VertexMaterialClass::Apply_Null();

#if !defined(GGC_RENDER_BACKEND_BGFX)
	for (unsigned index=0;index<4;++index) {
		SNAPSHOT_SAY(("Clearing light %d to null",index));
		Set_DX8_Light(index,nullptr);
	}
#endif

	// set up simple default TSS
	Vector4 vconst[MAX_VERTEX_SHADER_CONSTANTS];
	memset(vconst,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
	Commit_Vertex_Shader_Constants(0, vconst, MAX_VERTEX_SHADER_CONSTANTS);

	Vector4 pconst[MAX_PIXEL_SHADER_CONSTANTS];
	memset(pconst,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
	Commit_Pixel_Shader_Constants(0, pconst, MAX_PIXEL_SHADER_CONSTANTS);

	Commit_Vertex_Shader_Value(DX8_FVF_XYZNDUV2);
	Commit_Pixel_Shader_Value(0);

	ShaderClass::Invalidate();
}

#if !defined(GGC_RENDER_BACKEND_BGFX)
const char* DX8Wrapper::Get_DX8_Render_State_Name(D3DRENDERSTATETYPE state)
{
	switch (state) {
	case D3DRS_ZENABLE                       : return "D3DRS_ZENABLE";
	case D3DRS_FILLMODE                      : return "D3DRS_FILLMODE";
	case D3DRS_SHADEMODE                     : return "D3DRS_SHADEMODE";
	case D3DRS_LINEPATTERN                   : return "D3DRS_LINEPATTERN";
	case D3DRS_ZWRITEENABLE                  : return "D3DRS_ZWRITEENABLE";
	case D3DRS_ALPHATESTENABLE               : return "D3DRS_ALPHATESTENABLE";
	case D3DRS_LASTPIXEL                     : return "D3DRS_LASTPIXEL";
	case D3DRS_SRCBLEND                      : return "D3DRS_SRCBLEND";
	case D3DRS_DESTBLEND                     : return "D3DRS_DESTBLEND";
	case D3DRS_CULLMODE                      : return "D3DRS_CULLMODE";
	case D3DRS_ZFUNC                         : return "D3DRS_ZFUNC";
	case D3DRS_ALPHAREF                      : return "D3DRS_ALPHAREF";
	case D3DRS_ALPHAFUNC                     : return "D3DRS_ALPHAFUNC";
	case D3DRS_DITHERENABLE                  : return "D3DRS_DITHERENABLE";
	case D3DRS_ALPHABLENDENABLE              : return "D3DRS_ALPHABLENDENABLE";
	case D3DRS_FOGENABLE                     : return "D3DRS_FOGENABLE";
	case D3DRS_SPECULARENABLE                : return "D3DRS_SPECULARENABLE";
	case D3DRS_ZVISIBLE                      : return "D3DRS_ZVISIBLE";
	case D3DRS_FOGCOLOR                      : return "D3DRS_FOGCOLOR";
	case D3DRS_FOGTABLEMODE                  : return "D3DRS_FOGTABLEMODE";
	case D3DRS_FOGSTART                      : return "D3DRS_FOGSTART";
	case D3DRS_FOGEND                        : return "D3DRS_FOGEND";
	case D3DRS_FOGDENSITY                    : return "D3DRS_FOGDENSITY";
	case D3DRS_EDGEANTIALIAS                 : return "D3DRS_EDGEANTIALIAS";
	case D3DRS_ZBIAS                         : return "D3DRS_ZBIAS";
	case D3DRS_RANGEFOGENABLE                : return "D3DRS_RANGEFOGENABLE";
	case D3DRS_STENCILENABLE                 : return "D3DRS_STENCILENABLE";
	case D3DRS_STENCILFAIL                   : return "D3DRS_STENCILFAIL";
	case D3DRS_STENCILZFAIL                  : return "D3DRS_STENCILZFAIL";
	case D3DRS_STENCILPASS                   : return "D3DRS_STENCILPASS";
	case D3DRS_STENCILFUNC                   : return "D3DRS_STENCILFUNC";
	case D3DRS_STENCILREF                    : return "D3DRS_STENCILREF";
	case D3DRS_STENCILMASK                   : return "D3DRS_STENCILMASK";
	case D3DRS_STENCILWRITEMASK              : return "D3DRS_STENCILWRITEMASK";
	case D3DRS_TEXTUREFACTOR                 : return "D3DRS_TEXTUREFACTOR";
	case D3DRS_WRAP0                         : return "D3DRS_WRAP0";
	case D3DRS_WRAP1                         : return "D3DRS_WRAP1";
	case D3DRS_WRAP2                         : return "D3DRS_WRAP2";
	case D3DRS_WRAP3                         : return "D3DRS_WRAP3";
	case D3DRS_WRAP4                         : return "D3DRS_WRAP4";
	case D3DRS_WRAP5                         : return "D3DRS_WRAP5";
	case D3DRS_WRAP6                         : return "D3DRS_WRAP6";
	case D3DRS_WRAP7                         : return "D3DRS_WRAP7";
	case D3DRS_CLIPPING                      : return "D3DRS_CLIPPING";
	case D3DRS_LIGHTING                      : return "D3DRS_LIGHTING";
	case D3DRS_AMBIENT                       : return "D3DRS_AMBIENT";
	case D3DRS_FOGVERTEXMODE                 : return "D3DRS_FOGVERTEXMODE";
	case D3DRS_COLORVERTEX                   : return "D3DRS_COLORVERTEX";
	case D3DRS_LOCALVIEWER                   : return "D3DRS_LOCALVIEWER";
	case D3DRS_NORMALIZENORMALS              : return "D3DRS_NORMALIZENORMALS";
	case D3DRS_DIFFUSEMATERIALSOURCE         : return "D3DRS_DIFFUSEMATERIALSOURCE";
	case D3DRS_SPECULARMATERIALSOURCE        : return "D3DRS_SPECULARMATERIALSOURCE";
	case D3DRS_AMBIENTMATERIALSOURCE         : return "D3DRS_AMBIENTMATERIALSOURCE";
	case D3DRS_EMISSIVEMATERIALSOURCE        : return "D3DRS_EMISSIVEMATERIALSOURCE";
	case D3DRS_VERTEXBLEND                   : return "D3DRS_VERTEXBLEND";
	case D3DRS_CLIPPLANEENABLE               : return "D3DRS_CLIPPLANEENABLE";
	case D3DRS_SOFTWAREVERTEXPROCESSING      : return "D3DRS_SOFTWAREVERTEXPROCESSING";
	case D3DRS_POINTSIZE                     : return "D3DRS_POINTSIZE";
	case D3DRS_POINTSIZE_MIN                 : return "D3DRS_POINTSIZE_MIN";
	case D3DRS_POINTSPRITEENABLE             : return "D3DRS_POINTSPRITEENABLE";
	case D3DRS_POINTSCALEENABLE              : return "D3DRS_POINTSCALEENABLE";
	case D3DRS_POINTSCALE_A                  : return "D3DRS_POINTSCALE_A";
	case D3DRS_POINTSCALE_B                  : return "D3DRS_POINTSCALE_B";
	case D3DRS_POINTSCALE_C                  : return "D3DRS_POINTSCALE_C";
	case D3DRS_MULTISAMPLEANTIALIAS          : return "D3DRS_MULTISAMPLEANTIALIAS";
	case D3DRS_MULTISAMPLEMASK               : return "D3DRS_MULTISAMPLEMASK";
	case D3DRS_PATCHEDGESTYLE                : return "D3DRS_PATCHEDGESTYLE";
	case D3DRS_PATCHSEGMENTS                 : return "D3DRS_PATCHSEGMENTS";
	case D3DRS_DEBUGMONITORTOKEN             : return "D3DRS_DEBUGMONITORTOKEN";
	case D3DRS_POINTSIZE_MAX                 : return "D3DRS_POINTSIZE_MAX";
	case D3DRS_INDEXEDVERTEXBLENDENABLE      : return "D3DRS_INDEXEDVERTEXBLENDENABLE";
	case D3DRS_COLORWRITEENABLE              : return "D3DRS_COLORWRITEENABLE";
	case D3DRS_TWEENFACTOR                   : return "D3DRS_TWEENFACTOR";
	case D3DRS_BLENDOP                       : return "D3DRS_BLENDOP";
//	case D3DRS_POSITIONORDER                 : return "D3DRS_POSITIONORDER";
//	case D3DRS_NORMALORDER                   : return "D3DRS_NORMALORDER";
	default											  : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state)
{
	switch (state) {
	case D3DTSS_COLOROP                   : return "D3DTSS_COLOROP";
	case D3DTSS_COLORARG1                 : return "D3DTSS_COLORARG1";
	case D3DTSS_COLORARG2                 : return "D3DTSS_COLORARG2";
	case D3DTSS_ALPHAOP                   : return "D3DTSS_ALPHAOP";
	case D3DTSS_ALPHAARG1                 : return "D3DTSS_ALPHAARG1";
	case D3DTSS_ALPHAARG2                 : return "D3DTSS_ALPHAARG2";
	case D3DTSS_BUMPENVMAT00              : return "D3DTSS_BUMPENVMAT00";
	case D3DTSS_BUMPENVMAT01              : return "D3DTSS_BUMPENVMAT01";
	case D3DTSS_BUMPENVMAT10              : return "D3DTSS_BUMPENVMAT10";
	case D3DTSS_BUMPENVMAT11              : return "D3DTSS_BUMPENVMAT11";
	case D3DTSS_TEXCOORDINDEX             : return "D3DTSS_TEXCOORDINDEX";
	case D3DTSS_ADDRESSU                  : return "D3DTSS_ADDRESSU";
	case D3DTSS_ADDRESSV                  : return "D3DTSS_ADDRESSV";
	case D3DTSS_BORDERCOLOR               : return "D3DTSS_BORDERCOLOR";
	case D3DTSS_MAGFILTER                 : return "D3DTSS_MAGFILTER";
	case D3DTSS_MINFILTER                 : return "D3DTSS_MINFILTER";
	case D3DTSS_MIPFILTER                 : return "D3DTSS_MIPFILTER";
	case D3DTSS_MIPMAPLODBIAS             : return "D3DTSS_MIPMAPLODBIAS";
	case D3DTSS_MAXMIPLEVEL               : return "D3DTSS_MAXMIPLEVEL";
	case D3DTSS_MAXANISOTROPY             : return "D3DTSS_MAXANISOTROPY";
	case D3DTSS_BUMPENVLSCALE             : return "D3DTSS_BUMPENVLSCALE";
	case D3DTSS_BUMPENVLOFFSET            : return "D3DTSS_BUMPENVLOFFSET";
	case D3DTSS_TEXTURETRANSFORMFLAGS     : return "D3DTSS_TEXTURETRANSFORMFLAGS";
	case D3DTSS_ADDRESSW                  : return "D3DTSS_ADDRESSW";
	case D3DTSS_COLORARG0                 : return "D3DTSS_COLORARG0";
	case D3DTSS_ALPHAARG0                 : return "D3DTSS_ALPHAARG0";
	case D3DTSS_RESULTARG                 : return "D3DTSS_RESULTARG";
	default										  : return "UNKNOWN";
	}
}

void DX8Wrapper::Get_DX8_Render_State_Value_Name(StringClass& name, D3DRENDERSTATETYPE state, unsigned value)
{
	switch (state) {
	case D3DRS_ZENABLE:
		name=Get_DX8_ZBuffer_Type_Name(value);
		break;

	case D3DRS_FILLMODE:
		name=Get_DX8_Fill_Mode_Name(value);
		break;

	case D3DRS_SHADEMODE:
		name=Get_DX8_Shade_Mode_Name(value);
		break;

	case D3DRS_LINEPATTERN:
	case D3DRS_FOGCOLOR:
	case D3DRS_ALPHAREF:
	case D3DRS_STENCILMASK:
	case D3DRS_STENCILWRITEMASK:
	case D3DRS_TEXTUREFACTOR:
	case D3DRS_AMBIENT:
	case D3DRS_CLIPPLANEENABLE:
	case D3DRS_MULTISAMPLEMASK:
		name.Format("0x%x",value);
		break;

	case D3DRS_ZWRITEENABLE:
	case D3DRS_ALPHATESTENABLE:
	case D3DRS_LASTPIXEL:
	case D3DRS_DITHERENABLE:
	case D3DRS_ALPHABLENDENABLE:
	case D3DRS_FOGENABLE:
	case D3DRS_SPECULARENABLE:
	case D3DRS_STENCILENABLE:
	case D3DRS_RANGEFOGENABLE:
	case D3DRS_EDGEANTIALIAS:
	case D3DRS_CLIPPING:
	case D3DRS_LIGHTING:
	case D3DRS_COLORVERTEX:
	case D3DRS_LOCALVIEWER:
	case D3DRS_NORMALIZENORMALS:
	case D3DRS_SOFTWAREVERTEXPROCESSING:
	case D3DRS_POINTSPRITEENABLE:
	case D3DRS_POINTSCALEENABLE:
	case D3DRS_MULTISAMPLEANTIALIAS:
	case D3DRS_INDEXEDVERTEXBLENDENABLE:
		name=value ? "TRUE" : "FALSE";
		break;

	case D3DRS_SRCBLEND:
	case D3DRS_DESTBLEND:
		name=Get_DX8_Blend_Name(value);
		break;

	case D3DRS_CULLMODE:
		name=Get_DX8_Cull_Mode_Name(value);
		break;

	case D3DRS_ZFUNC:
	case D3DRS_ALPHAFUNC:
	case D3DRS_STENCILFUNC:
		name=Get_DX8_Cmp_Func_Name(value);
		break;

	case D3DRS_ZVISIBLE:
		name="NOTSUPPORTED";
		break;

	case D3DRS_FOGTABLEMODE:
	case D3DRS_FOGVERTEXMODE:
		name=Get_DX8_Fog_Mode_Name(value);
		break;

	case D3DRS_FOGSTART:
	case D3DRS_FOGEND:
	case D3DRS_FOGDENSITY:
	case D3DRS_POINTSIZE:
	case D3DRS_POINTSIZE_MIN:
	case D3DRS_POINTSCALE_A:
	case D3DRS_POINTSCALE_B:
	case D3DRS_POINTSCALE_C:
	case D3DRS_PATCHSEGMENTS:
	case D3DRS_POINTSIZE_MAX:
	case D3DRS_TWEENFACTOR:
		name.Format("%f",*(float*)&value);
		break;

	case D3DRS_ZBIAS:
	case D3DRS_STENCILREF:
		name.Format("%d",value);
		break;

	case D3DRS_STENCILFAIL:
	case D3DRS_STENCILZFAIL:
	case D3DRS_STENCILPASS:
		name=Get_DX8_Stencil_Op_Name(value);
		break;

	case D3DRS_WRAP0:
	case D3DRS_WRAP1:
	case D3DRS_WRAP2:
	case D3DRS_WRAP3:
	case D3DRS_WRAP4:
	case D3DRS_WRAP5:
	case D3DRS_WRAP6:
	case D3DRS_WRAP7:
		name="0";
		if (value&D3DWRAP_U) name+="|D3DWRAP_U";
		if (value&D3DWRAP_V) name+="|D3DWRAP_V";
		if (value&D3DWRAP_W) name+="|D3DWRAP_W";
		break;

	case D3DRS_DIFFUSEMATERIALSOURCE:
	case D3DRS_SPECULARMATERIALSOURCE:
	case D3DRS_AMBIENTMATERIALSOURCE:
	case D3DRS_EMISSIVEMATERIALSOURCE:
		name=Get_DX8_Material_Source_Name(value);
		break;

	case D3DRS_VERTEXBLEND:
		name=Get_DX8_Vertex_Blend_Flag_Name(value);
		break;

	case D3DRS_PATCHEDGESTYLE:
		name=Get_DX8_Patch_Edge_Style_Name(value);
		break;

	case D3DRS_DEBUGMONITORTOKEN:
		name=Get_DX8_Debug_Monitor_Token_Name(value);
		break;

	case D3DRS_COLORWRITEENABLE:
		name="0";
		if (value&D3DCOLORWRITEENABLE_RED) name+="|D3DCOLORWRITEENABLE_RED";
		if (value&D3DCOLORWRITEENABLE_GREEN) name+="|D3DCOLORWRITEENABLE_GREEN";
		if (value&D3DCOLORWRITEENABLE_BLUE) name+="|D3DCOLORWRITEENABLE_BLUE";
		if (value&D3DCOLORWRITEENABLE_ALPHA) name+="|D3DCOLORWRITEENABLE_ALPHA";
		break;
	case D3DRS_BLENDOP:
		name=Get_DX8_Blend_Op_Name(value);
		break;
	default:
		name.Format("UNKNOWN (%d)",value);
		break;
	}
}

void DX8Wrapper::Get_DX8_Texture_Stage_State_Value_Name(StringClass& name, D3DTEXTURESTAGESTATETYPE state, unsigned value)
{
	switch (state) {
	case D3DTSS_COLOROP:
	case D3DTSS_ALPHAOP:
		name=Get_DX8_Texture_Op_Name(value);
		break;

	case D3DTSS_COLORARG0:
	case D3DTSS_COLORARG1:
	case D3DTSS_COLORARG2:
	case D3DTSS_ALPHAARG0:
	case D3DTSS_ALPHAARG1:
	case D3DTSS_ALPHAARG2:
	case D3DTSS_RESULTARG:
		name=Get_DX8_Texture_Arg_Name(value);
		break;

	case D3DTSS_ADDRESSU:
	case D3DTSS_ADDRESSV:
	case D3DTSS_ADDRESSW:
		name=Get_DX8_Texture_Address_Name(value);
		break;

	case D3DTSS_MAGFILTER:
	case D3DTSS_MINFILTER:
	case D3DTSS_MIPFILTER:
		name=Get_DX8_Texture_Filter_Name(value);
		break;

	case D3DTSS_TEXTURETRANSFORMFLAGS:
		name=Get_DX8_Texture_Transform_Flag_Name(value);
		break;

	// Floating point values
	case D3DTSS_MIPMAPLODBIAS:
	case D3DTSS_BUMPENVMAT00:
	case D3DTSS_BUMPENVMAT01:
	case D3DTSS_BUMPENVMAT10:
	case D3DTSS_BUMPENVMAT11:
	case D3DTSS_BUMPENVLSCALE:
	case D3DTSS_BUMPENVLOFFSET:
		name.Format("%f",*(float*)&value);
		break;

	case D3DTSS_TEXCOORDINDEX:
		if ((value&0xffff0000)==D3DTSS_TCI_CAMERASPACENORMAL) {
			name.Format("D3DTSS_TCI_CAMERASPACENORMAL|%d",value&0xffff);
		}
		else if ((value&0xffff0000)==D3DTSS_TCI_CAMERASPACEPOSITION) {
			name.Format("D3DTSS_TCI_CAMERASPACEPOSITION|%d",value&0xffff);
		}
		else if ((value&0xffff0000)==D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR) {
			name.Format("D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR|%d",value&0xffff);
		}
		else {
			name.Format("%d",value);
		}
		break;

	// Integer value
	case D3DTSS_MAXMIPLEVEL:
	case D3DTSS_MAXANISOTROPY:
		name.Format("%d",value);
		break;
	// Hex values
	case D3DTSS_BORDERCOLOR:
		name.Format("0x%x",value);
		break;

	default:
		name.Format("UNKNOWN (%d)",value);
		break;
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Op_Name(unsigned value)
{
	switch (value) {
	case D3DTOP_DISABLE                      : return "D3DTOP_DISABLE";
	case D3DTOP_SELECTARG1                   : return "D3DTOP_SELECTARG1";
	case D3DTOP_SELECTARG2                   : return "D3DTOP_SELECTARG2";
	case D3DTOP_MODULATE                     : return "D3DTOP_MODULATE";
	case D3DTOP_MODULATE2X                   : return "D3DTOP_MODULATE2X";
	case D3DTOP_MODULATE4X                   : return "D3DTOP_MODULATE4X";
	case D3DTOP_ADD                          : return "D3DTOP_ADD";
	case D3DTOP_ADDSIGNED                    : return "D3DTOP_ADDSIGNED";
	case D3DTOP_ADDSIGNED2X                  : return "D3DTOP_ADDSIGNED2X";
	case D3DTOP_SUBTRACT                     : return "D3DTOP_SUBTRACT";
	case D3DTOP_ADDSMOOTH                    : return "D3DTOP_ADDSMOOTH";
	case D3DTOP_BLENDDIFFUSEALPHA            : return "D3DTOP_BLENDDIFFUSEALPHA";
	case D3DTOP_BLENDTEXTUREALPHA            : return "D3DTOP_BLENDTEXTUREALPHA";
	case D3DTOP_BLENDFACTORALPHA             : return "D3DTOP_BLENDFACTORALPHA";
	case D3DTOP_BLENDTEXTUREALPHAPM          : return "D3DTOP_BLENDTEXTUREALPHAPM";
	case D3DTOP_BLENDCURRENTALPHA            : return "D3DTOP_BLENDCURRENTALPHA";
	case D3DTOP_PREMODULATE                  : return "D3DTOP_PREMODULATE";
	case D3DTOP_MODULATEALPHA_ADDCOLOR       : return "D3DTOP_MODULATEALPHA_ADDCOLOR";
	case D3DTOP_MODULATECOLOR_ADDALPHA       : return "D3DTOP_MODULATECOLOR_ADDALPHA";
	case D3DTOP_MODULATEINVALPHA_ADDCOLOR    : return "D3DTOP_MODULATEINVALPHA_ADDCOLOR";
	case D3DTOP_MODULATEINVCOLOR_ADDALPHA    : return "D3DTOP_MODULATEINVCOLOR_ADDALPHA";
	case D3DTOP_BUMPENVMAP                   : return "D3DTOP_BUMPENVMAP";
	case D3DTOP_BUMPENVMAPLUMINANCE          : return "D3DTOP_BUMPENVMAPLUMINANCE";
	case D3DTOP_DOTPRODUCT3                  : return "D3DTOP_DOTPRODUCT3";
	case D3DTOP_MULTIPLYADD                  : return "D3DTOP_MULTIPLYADD";
	case D3DTOP_LERP                         : return "D3DTOP_LERP";
	default										     : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Arg_Name(unsigned value)
{
	switch (value) {
	case D3DTA_CURRENT			: return "D3DTA_CURRENT";
	case D3DTA_DIFFUSE			: return "D3DTA_DIFFUSE";
	case D3DTA_SELECTMASK		: return "D3DTA_SELECTMASK";
	case D3DTA_SPECULAR			: return "D3DTA_SPECULAR";
	case D3DTA_TEMP				: return "D3DTA_TEMP";
	case D3DTA_TEXTURE			: return "D3DTA_TEXTURE";
	case D3DTA_TFACTOR			: return "D3DTA_TFACTOR";
	case D3DTA_ALPHAREPLICATE	: return "D3DTA_ALPHAREPLICATE";
	case D3DTA_COMPLEMENT		: return "D3DTA_COMPLEMENT";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Filter_Name(unsigned value)
{
	switch (value) {
	case D3DTEXF_NONE				: return "D3DTEXF_NONE";
	case D3DTEXF_POINT			: return "D3DTEXF_POINT";
	case D3DTEXF_LINEAR			: return "D3DTEXF_LINEAR";
	case D3DTEXF_ANISOTROPIC	: return "D3DTEXF_ANISOTROPIC";
	case D3DTEXF_FLATCUBIC		: return "D3DTEXF_FLATCUBIC";
	case D3DTEXF_GAUSSIANCUBIC	: return "D3DTEXF_GAUSSIANCUBIC";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Address_Name(unsigned value)
{
	switch (value) {
	case D3DTADDRESS_WRAP		: return "D3DTADDRESS_WRAP";
	case D3DTADDRESS_MIRROR		: return "D3DTADDRESS_MIRROR";
	case D3DTADDRESS_CLAMP		: return "D3DTADDRESS_CLAMP";
	case D3DTADDRESS_BORDER		: return "D3DTADDRESS_BORDER";
	case D3DTADDRESS_MIRRORONCE: return "D3DTADDRESS_MIRRORONCE";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Transform_Flag_Name(unsigned value)
{
	switch (value) {
	case D3DTTFF_DISABLE			: return "D3DTTFF_DISABLE";
	case D3DTTFF_COUNT1			: return "D3DTTFF_COUNT1";
	case D3DTTFF_COUNT2			: return "D3DTTFF_COUNT2";
	case D3DTTFF_COUNT3			: return "D3DTTFF_COUNT3";
	case D3DTTFF_COUNT4			: return "D3DTTFF_COUNT4";
	case D3DTTFF_PROJECTED		: return "D3DTTFF_PROJECTED";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_ZBuffer_Type_Name(unsigned value)
{
	switch (value) {
	case D3DZB_FALSE				: return "D3DZB_FALSE";
	case D3DZB_TRUE				: return "D3DZB_TRUE";
	case D3DZB_USEW				: return "D3DZB_USEW";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Fill_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DFILL_POINT			: return "D3DFILL_POINT";
	case D3DFILL_WIREFRAME		: return "D3DFILL_WIREFRAME";
	case D3DFILL_SOLID			: return "D3DFILL_SOLID";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Shade_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DSHADE_FLAT			: return "D3DSHADE_FLAT";
	case D3DSHADE_GOURAUD		: return "D3DSHADE_GOURAUD";
	case D3DSHADE_PHONG			: return "D3DSHADE_PHONG";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Blend_Name(unsigned value)
{
	switch (value) {
	case D3DBLEND_ZERO                : return "D3DBLEND_ZERO";
	case D3DBLEND_ONE                 : return "D3DBLEND_ONE";
	case D3DBLEND_SRCCOLOR            : return "D3DBLEND_SRCCOLOR";
	case D3DBLEND_INVSRCCOLOR         : return "D3DBLEND_INVSRCCOLOR";
	case D3DBLEND_SRCALPHA            : return "D3DBLEND_SRCALPHA";
	case D3DBLEND_INVSRCALPHA         : return "D3DBLEND_INVSRCALPHA";
	case D3DBLEND_DESTALPHA           : return "D3DBLEND_DESTALPHA";
	case D3DBLEND_INVDESTALPHA        : return "D3DBLEND_INVDESTALPHA";
	case D3DBLEND_DESTCOLOR           : return "D3DBLEND_DESTCOLOR";
	case D3DBLEND_INVDESTCOLOR        : return "D3DBLEND_INVDESTCOLOR";
	case D3DBLEND_SRCALPHASAT         : return "D3DBLEND_SRCALPHASAT";
	case D3DBLEND_BOTHSRCALPHA        : return "D3DBLEND_BOTHSRCALPHA";
	case D3DBLEND_BOTHINVSRCALPHA     : return "D3DBLEND_BOTHINVSRCALPHA";
	default									 : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Cull_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DCULL_NONE				: return "D3DCULL_NONE";
	case D3DCULL_CW				: return "D3DCULL_CW";
	case D3DCULL_CCW				: return "D3DCULL_CCW";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Cmp_Func_Name(unsigned value)
{
	switch (value) {
	case D3DCMP_NEVER          : return "D3DCMP_NEVER";
	case D3DCMP_LESS           : return "D3DCMP_LESS";
	case D3DCMP_EQUAL          : return "D3DCMP_EQUAL";
	case D3DCMP_LESSEQUAL      : return "D3DCMP_LESSEQUAL";
	case D3DCMP_GREATER        : return "D3DCMP_GREATER";
	case D3DCMP_NOTEQUAL       : return "D3DCMP_NOTEQUAL";
	case D3DCMP_GREATEREQUAL   : return "D3DCMP_GREATEREQUAL";
	case D3DCMP_ALWAYS         : return "D3DCMP_ALWAYS";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Fog_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DFOG_NONE				: return "D3DFOG_NONE";
	case D3DFOG_EXP				: return "D3DFOG_EXP";
	case D3DFOG_EXP2				: return "D3DFOG_EXP2";
	case D3DFOG_LINEAR			: return "D3DFOG_LINEAR";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Stencil_Op_Name(unsigned value)
{
	switch (value) {
	case D3DSTENCILOP_KEEP		: return "D3DSTENCILOP_KEEP";
	case D3DSTENCILOP_ZERO		: return "D3DSTENCILOP_ZERO";
	case D3DSTENCILOP_REPLACE	: return "D3DSTENCILOP_REPLACE";
	case D3DSTENCILOP_INCRSAT	: return "D3DSTENCILOP_INCRSAT";
	case D3DSTENCILOP_DECRSAT	: return "D3DSTENCILOP_DECRSAT";
	case D3DSTENCILOP_INVERT	: return "D3DSTENCILOP_INVERT";
	case D3DSTENCILOP_INCR		: return "D3DSTENCILOP_INCR";
	case D3DSTENCILOP_DECR		: return "D3DSTENCILOP_DECR";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Material_Source_Name(unsigned value)
{
	switch (value) {
	case D3DMCS_MATERIAL			: return "D3DMCS_MATERIAL";
	case D3DMCS_COLOR1			: return "D3DMCS_COLOR1";
	case D3DMCS_COLOR2			: return "D3DMCS_COLOR2";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Vertex_Blend_Flag_Name(unsigned value)
{
	switch (value) {
	case D3DVBF_DISABLE			: return "D3DVBF_DISABLE";
	case D3DVBF_1WEIGHTS			: return "D3DVBF_1WEIGHTS";
	case D3DVBF_2WEIGHTS			: return "D3DVBF_2WEIGHTS";
	case D3DVBF_3WEIGHTS			: return "D3DVBF_3WEIGHTS";
	case D3DVBF_TWEENING			: return "D3DVBF_TWEENING";
	case D3DVBF_0WEIGHTS			: return "D3DVBF_0WEIGHTS";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Patch_Edge_Style_Name(unsigned value)
{
	switch (value) {
	case D3DPATCHEDGE_DISCRETE	: return "D3DPATCHEDGE_DISCRETE";
   case D3DPATCHEDGE_CONTINUOUS:return "D3DPATCHEDGE_CONTINUOUS";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Debug_Monitor_Token_Name(unsigned value)
{
	switch (value) {
	case D3DDMT_ENABLE			: return "D3DDMT_ENABLE";
	case D3DDMT_DISABLE			: return "D3DDMT_DISABLE";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Blend_Op_Name(unsigned value)
{
	switch (value) {
	case D3DBLENDOP_ADD			: return "D3DBLENDOP_ADD";
	case D3DBLENDOP_SUBTRACT	: return "D3DBLENDOP_SUBTRACT";
	case D3DBLENDOP_REVSUBTRACT: return "D3DBLENDOP_REVSUBTRACT";
	case D3DBLENDOP_MIN			: return "D3DBLENDOP_MIN";
	case D3DBLENDOP_MAX			: return "D3DBLENDOP_MAX";
		default							: return "UNKNOWN";
	}
}
#endif


//============================================================================
// DX8Wrapper::getBackBufferFormat
//============================================================================

WW3DFormat	DX8Wrapper::getBackBufferFormat()
{
	return D3DFormat_To_WW3DFormat( _PresentParameters.BackBufferFormat );
}
