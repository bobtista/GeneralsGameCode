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

// TheSuperHackers @refactor bobtista 22/04/2026 Phase 5.2 Stage 1
// No-op implementations of the Direct3D 8 COM interfaces. These stubs allow
// DX8Wrapper to be driven against a synthetic Direct3D 8 device without
// loading d3d8.dll at runtime. Every method returns S_OK / D3D_OK and does
// nothing; resource creation returns ref-counted stub objects; Lock methods
// hand out heap-allocated scratch buffers. Compiled only under the
// GGC_BGFX_STANDALONE build configuration used by the bgfx-only renderer.

#include "StubD3D8Device.h"

#if defined(GGC_BGFX_STANDALONE)

#include "DXTUtils.h"
#include "wwdebug.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace
{

// TheSuperHackers @refactor bobtista 22/04/2026 Phase 5.2 — scratch buffers
// must bypass the game's overridden global operator new[] (see
// GameMemory.cpp: new[] routes through TheDynamicMemoryAllocator). Stub
// surfaces can be tens of MB (e.g. a 4096x4096 shadow map = 64 MB) and
// allocating them through the pool corrupts pool metadata. std::calloc is
// not overridden when RTS_MEMORYPOOL_OVERRIDE_MALLOC=OFF (the default).
struct StubAllocDeleter
{
	void operator()(uint8_t* p) const noexcept { std::free(p); }
};
using StubScratch = std::unique_ptr<uint8_t[], StubAllocDeleter>;

static StubScratch AllocScratch(size_t bytes)
{
	if (bytes == 0) bytes = 4;
	void* p = std::calloc(bytes, 1);
	return StubScratch(static_cast<uint8_t*>(p));
}

// TheSuperHackers @bugfix bobtista 22/04/2026 Phase 5.2 — stub Lock/Desc
// previously reported pitch = width*4 regardless of format. Terrain uses
// D3DFMT_A1R5G5B5 (2 bytes/pixel); upload paths (EnsureBgfxTexture,
// D3DXFilterTexture) trust the reported pitch as source stride, so a
// bogus 4bpp pitch caused them to read every other row and run off the
// end of the written data — visible as dark horizontal/diagonal bands
// on terrain in standalone. Resolve per-format bytes per pixel for the
// formats the engine actually creates; anything else falls back to 4.
static UINT BytesPerPixel(D3DFORMAT fmt)
{
	switch (fmt)
	{
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_G16R16:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_Q8W8V8U8:
	case D3DFMT_V16U16:
	case D3DFMT_W11V11U10:
	case D3DFMT_A2W10V10U10:
	case D3DFMT_D32:
	case D3DFMT_D24S8:
	case D3DFMT_D24X8:
	case D3DFMT_D24X4S4:
		return 4;
	case D3DFMT_R8G8B8:
		return 3;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_X4R4G4B4:
	case D3DFMT_A8L8:
	case D3DFMT_A8P8:
	case D3DFMT_A8R3G3B2:
	case D3DFMT_V8U8:
	case D3DFMT_L6V5U5:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
	case D3DFMT_D16:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
	case D3DFMT_INDEX16:
		return 2;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8:
	case D3DFMT_L8:
	case D3DFMT_P8:
	case D3DFMT_A4L4:
		return 1;
	case D3DFMT_INDEX32:
		return 4;
	default:
		return 4;
	}
}

// TheSuperHackers @bugfix bobtista 27/04/2026 Compressed D3D textures lock
// by block rows, not pixel rows. Reporting them as width*4 made bgfx upload
// skip chunks of DXT atlas data and fill the gaps with black.
static bool IsCompressedFormat(D3DFORMAT fmt)
{
	switch (fmt)
	{
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return true;
	default:
		return false;
	}
}

static UINT BlockBytes(D3DFORMAT fmt)
{
	return (fmt == D3DFMT_DXT1) ? 8 : 16;
}

static UINT SurfacePitch(D3DFORMAT fmt, UINT width)
{
	if (IsCompressedFormat(fmt))
	{
		return DXT_SurfacePitch(width, BlockBytes(fmt));
	}
	return width * BytesPerPixel(fmt);
}

static UINT SurfaceRows(D3DFORMAT fmt, UINT height)
{
	if (IsCompressedFormat(fmt))
	{
		return DXT_SurfaceRows(height);
	}
	return height;
}

static UINT SurfaceStorageSize(D3DFORMAT fmt, UINT width, UINT height)
{
	if (IsCompressedFormat(fmt))
	{
		return DXT_SurfaceStorageSize(width, height, BlockBytes(fmt));
	}
	return SurfacePitch(fmt, width) * SurfaceRows(fmt, height);
}

class StubD3D8Device;

static void FillCaps(D3DCAPS8& caps)
{
	std::memset(&caps, 0, sizeof(caps));
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
	// TheSuperHackers @bugfix bobtista 22/04/2026 Phase 5.2 — DO NOT set
	// D3DPTEXTURECAPS_POW2 or D3DPTEXTURECAPS_SQUAREONLY here; those are
	// RESTRICTIONS, not features. D3DXCreateTexture reads these and rounds
	// rectangular textures down to the largest legal square (e.g. 2048x1
	// collapsed to 1x1) — which wedged the terrain alpha-edge texture.
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

// ---------------------------------------------------------------------------
// Surface
// ---------------------------------------------------------------------------
class StubD3D8Surface final : public IDirect3DSurface8
{
public:
	// Owning ctor — allocates its own scratch buffer (for render targets,
	// depth stencil surfaces, image surfaces).
	StubD3D8Surface(IDirect3DDevice8* device, IUnknown* container, UINT width, UINT height, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_container(container), m_width(width), m_height(height), m_format(format),
		  m_ownedScratch(AllocScratch(SurfaceStorageSize(format, width, height))),
		  m_scratchPtr(m_ownedScratch.get())
	{
	}
	// Borrowing ctor — reuses the container's scratch buffer. Used when a
	// texture's GetSurfaceLevel(0) hands back a surface view; the game's
	// writes through the surface must land in the same memory that the
	// bgfx backend later reads via the texture's LockRect. Real D3D8
	// aliases level 0 this way; the owning/borrowing split mirrors that.
	StubD3D8Surface(IDirect3DDevice8* device, IUnknown* container, UINT width, UINT height, D3DFORMAT format, uint8_t* borrowedScratch)
		: m_refCount(1), m_device(device), m_container(container), m_width(width), m_height(height), m_format(format),
		  m_ownedScratch(),
		  m_scratchPtr(borrowedScratch)
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DSurface8 || riid == IID_IUnknown) { *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr) return E_POINTER;
		*ppDevice = m_device;
		if (m_device) m_device->AddRef();
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD(GetContainer)(REFIID, void** ppContainer) override
	{
		if (ppContainer == nullptr) return E_POINTER;
		*ppContainer = m_container;
		if (m_container) m_container->AddRef();
		return D3D_OK;
	}
	STDMETHOD(GetDesc)(D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = SurfaceStorageSize(m_format, m_width, m_height);
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		return D3D_OK;
	}
	STDMETHOD(LockRect)(D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr) return E_POINTER;
		pLockedRect->Pitch = static_cast<INT>(SurfacePitch(m_format, m_width));
		pLockedRect->pBits = m_scratchPtr;
		return D3D_OK;
	}
	STDMETHOD(UnlockRect)() override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	IUnknown* m_container;
	UINT m_width;
	UINT m_height;
	D3DFORMAT m_format;
	StubScratch m_ownedScratch;
	uint8_t* m_scratchPtr;
};

// ---------------------------------------------------------------------------
// Vertex Buffer
// ---------------------------------------------------------------------------
class StubD3D8VertexBuffer final : public IDirect3DVertexBuffer8
{
public:
	StubD3D8VertexBuffer(IDirect3DDevice8* device, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_length(length), m_usage(usage), m_fvf(fvf), m_pool(pool),
		  m_scratch(AllocScratch(length))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DVertexBuffer8 || riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr) return E_POINTER;
		*ppDevice = m_device;
		if (m_device) m_device->AddRef();
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_VERTEXBUFFER; }
	STDMETHOD(Lock)(UINT OffsetToLock, UINT, BYTE** ppbData, DWORD) override
	{
		if (ppbData == nullptr) return E_POINTER;
		*ppbData = m_scratch.get() + OffsetToLock;
		return D3D_OK;
	}
	STDMETHOD(Unlock)() override { return D3D_OK; }
	STDMETHOD(GetDesc)(D3DVERTEXBUFFER_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		pDesc->Format = D3DFMT_VERTEXDATA;
		pDesc->Type = D3DRTYPE_VERTEXBUFFER;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = m_length;
		pDesc->FVF = m_fvf;
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_length;
	DWORD m_usage;
	DWORD m_fvf;
	D3DPOOL m_pool;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Index Buffer
// ---------------------------------------------------------------------------
class StubD3D8IndexBuffer final : public IDirect3DIndexBuffer8
{
public:
	StubD3D8IndexBuffer(IDirect3DDevice8* device, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_length(length), m_usage(usage), m_format(format), m_pool(pool),
		  m_scratch(AllocScratch(length))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DIndexBuffer8 || riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr) return E_POINTER;
		*ppDevice = m_device;
		if (m_device) m_device->AddRef();
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_INDEXBUFFER; }
	STDMETHOD(Lock)(UINT OffsetToLock, UINT, BYTE** ppbData, DWORD) override
	{
		if (ppbData == nullptr) return E_POINTER;
		*ppbData = m_scratch.get() + OffsetToLock;
		return D3D_OK;
	}
	STDMETHOD(Unlock)() override { return D3D_OK; }
	STDMETHOD(GetDesc)(D3DINDEXBUFFER_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_INDEXBUFFER;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = m_length;
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_length;
	DWORD m_usage;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Texture (2D)
// ---------------------------------------------------------------------------
static DWORD ComputeFullMipLevels(UINT w, UINT h)
{
	DWORD levels = 1;
	UINT m = w > h ? w : h;
	while (m > 1) { m >>= 1; ++levels; }
	return levels;
}

class StubD3D8Texture final : public IDirect3DTexture8
{
public:
	StubD3D8Texture(IDirect3DDevice8* device, UINT width, UINT height, UINT requestedLevels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_width(width), m_height(height), m_usage(usage), m_format(format), m_pool(pool)
	{
		// D3D8 convention: Levels == 0 means "all mips down to 1x1".
		m_levels = requestedLevels == 0 ? ComputeFullMipLevels(width, height) : requestedLevels;
		if (m_levels > 16) m_levels = 16;
		m_levelScratch.reset(new StubScratch[m_levels]);
		m_surfaces.reset(new IDirect3DSurface8*[m_levels]);
		for (DWORD i = 0; i < m_levels; ++i)
		{
			UINT lw = width  >> i; if (lw == 0) lw = 1;
			UINT lh = height >> i; if (lh == 0) lh = 1;
			m_levelScratch[i] = AllocScratch(SurfaceStorageSize(format, lw, lh));
			m_surfaces[i] = nullptr;
		}
	}
	~StubD3D8Texture()
	{
		for (DWORD i = 0; i < m_levels; ++i)
		{
			if (m_surfaces[i]) m_surfaces[i]->Release();
		}
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DTexture8 || riid == IID_IDirect3DBaseTexture8 ||
			riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr) return E_POINTER;
		*ppDevice = m_device;
		if (m_device) m_device->AddRef();
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_TEXTURE; }
	STDMETHOD_(DWORD, SetLOD)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetLOD)() override { return 0; }
	STDMETHOD_(DWORD, GetLevelCount)() override { return m_levels; }
	STDMETHOD(GetLevelDesc)(UINT level, D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		if (level >= m_levels) level = m_levels - 1;
		UINT lw = m_width  >> level; if (lw == 0) lw = 1;
		UINT lh = m_height >> level; if (lh == 0) lh = 1;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = SurfaceStorageSize(m_format, lw, lh);
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = lw;
		pDesc->Height = lh;
		return D3D_OK;
	}
	STDMETHOD(GetSurfaceLevel)(UINT level, IDirect3DSurface8** ppSurfaceLevel) override
	{
		if (ppSurfaceLevel == nullptr) return E_POINTER;
		if (level >= m_levels) level = m_levels - 1;
		if (m_surfaces[level] == nullptr)
		{
			// Real D3D8 aliases surface level N with the texture's level-N
			// storage. Writes via the surface must be visible through a
			// subsequent texture-level LockRect. EnsureBgfxTexture relies
			// on this aliasing when it uploads texture pixels to bgfx.
			UINT lw = m_width  >> level; if (lw == 0) lw = 1;
			UINT lh = m_height >> level; if (lh == 0) lh = 1;
			m_surfaces[level] = new StubD3D8Surface(m_device, static_cast<IDirect3DTexture8*>(this), lw, lh, m_format, m_levelScratch[level].get());
		}
		m_surfaces[level]->AddRef();
		*ppSurfaceLevel = m_surfaces[level];
		return D3D_OK;
	}
	STDMETHOD(LockRect)(UINT level, D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr) return E_POINTER;
		if (level >= m_levels) level = m_levels - 1;
		UINT lw = m_width >> level; if (lw == 0) lw = 1;
		pLockedRect->Pitch = static_cast<INT>(SurfacePitch(m_format, lw));
		pLockedRect->pBits = m_levelScratch[level].get();
		return D3D_OK;
	}
	STDMETHOD(UnlockRect)(UINT) override { return D3D_OK; }
	STDMETHOD(AddDirtyRect)(CONST RECT*) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_width;
	UINT m_height;
	DWORD m_usage;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	DWORD m_levels;
	std::unique_ptr<StubScratch[]> m_levelScratch;
	std::unique_ptr<IDirect3DSurface8*[]> m_surfaces;
};

// ---------------------------------------------------------------------------
// Cube Texture
// ---------------------------------------------------------------------------
class StubD3D8CubeTexture final : public IDirect3DCubeTexture8
{
public:
	StubD3D8CubeTexture(IDirect3DDevice8* device, UINT edge, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_edge(edge), m_format(format),
		  m_scratch(AllocScratch(SurfaceStorageSize(format, edge, edge)))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DCubeTexture8 || riid == IID_IDirect3DBaseTexture8 ||
			riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr) return E_POINTER;
		*ppDevice = m_device;
		if (m_device) m_device->AddRef();
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_CUBETEXTURE; }
	STDMETHOD_(DWORD, SetLOD)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetLOD)() override { return 0; }
	STDMETHOD_(DWORD, GetLevelCount)() override { return 1; }
	STDMETHOD(GetLevelDesc)(UINT, D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = SurfaceStorageSize(m_format, m_edge, m_edge);
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_edge;
		pDesc->Height = m_edge;
		return D3D_OK;
	}
	STDMETHOD(GetCubeMapSurface)(D3DCUBEMAP_FACES, UINT, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr) return E_POINTER;
		*ppSurface = new StubD3D8Surface(m_device, static_cast<IDirect3DCubeTexture8*>(this), m_edge, m_edge, m_format, m_scratch.get());
		return D3D_OK;
	}
	STDMETHOD(LockRect)(D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr) return E_POINTER;
		pLockedRect->Pitch = static_cast<INT>(SurfacePitch(m_format, m_edge));
		pLockedRect->pBits = m_scratch.get();
		return D3D_OK;
	}
	STDMETHOD(UnlockRect)(D3DCUBEMAP_FACES, UINT) override { return D3D_OK; }
	STDMETHOD(AddDirtyRect)(D3DCUBEMAP_FACES, CONST RECT*) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_edge;
	D3DFORMAT m_format;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Volume Texture
// ---------------------------------------------------------------------------
class StubD3D8VolumeTexture final : public IDirect3DVolumeTexture8
{
public:
	StubD3D8VolumeTexture(IDirect3DDevice8* device, UINT width, UINT height, UINT depth, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_width(width), m_height(height), m_depth(depth), m_format(format),
		  m_scratch(AllocScratch(static_cast<size_t>(width) * height * depth * BytesPerPixel(format)))
	{
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DVolumeTexture8 || riid == IID_IDirect3DBaseTexture8 ||
			riid == IID_IDirect3DResource8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override
	{
		if (ppDevice == nullptr) return E_POINTER;
		*ppDevice = m_device;
		if (m_device) m_device->AddRef();
		return D3D_OK;
	}
	STDMETHOD(SetPrivateData)(REFGUID, CONST void*, DWORD, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPrivateData)(REFGUID, void*, DWORD*) override { return D3D_OK; }
	STDMETHOD(FreePrivateData)(REFGUID) override { return D3D_OK; }
	STDMETHOD_(DWORD, SetPriority)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetPriority)() override { return 0; }
	STDMETHOD_(void, PreLoad)() override {}
	STDMETHOD_(D3DRESOURCETYPE, GetType)() override { return D3DRTYPE_VOLUMETEXTURE; }
	STDMETHOD_(DWORD, SetLOD)(DWORD) override { return 0; }
	STDMETHOD_(DWORD, GetLOD)() override { return 0; }
	STDMETHOD_(DWORD, GetLevelCount)() override { return 1; }
	STDMETHOD(GetLevelDesc)(UINT, D3DVOLUME_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		const UINT bpp = BytesPerPixel(m_format);
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_VOLUME;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = m_width * m_height * m_depth * bpp;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		pDesc->Depth = m_depth;
		return D3D_OK;
	}
	STDMETHOD(GetVolumeLevel)(UINT, IDirect3DVolume8** ppVolumeLevel) override
	{
		if (ppVolumeLevel == nullptr) return E_POINTER;
		*ppVolumeLevel = nullptr;
		return D3D_OK;
	}
	STDMETHOD(LockBox)(UINT, D3DLOCKED_BOX* pLockedVolume, CONST D3DBOX*, DWORD) override
	{
		if (pLockedVolume == nullptr) return E_POINTER;
		const UINT bpp = BytesPerPixel(m_format);
		pLockedVolume->RowPitch = static_cast<INT>(m_width * bpp);
		pLockedVolume->SlicePitch = static_cast<INT>(m_width * m_height * bpp);
		pLockedVolume->pBits = m_scratch.get();
		return D3D_OK;
	}
	STDMETHOD(UnlockBox)(UINT) override { return D3D_OK; }
	STDMETHOD(AddDirtyBox)(CONST D3DBOX*) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	UINT m_width;
	UINT m_height;
	UINT m_depth;
	D3DFORMAT m_format;
	StubScratch m_scratch;
};

// ---------------------------------------------------------------------------
// Swap Chain
// ---------------------------------------------------------------------------
class StubD3D8SwapChain final : public IDirect3DSwapChain8
{
public:
	StubD3D8SwapChain(IDirect3DDevice8* device, IDirect3DSurface8* backBuffer)
		: m_refCount(1), m_device(device), m_backBuffer(backBuffer)
	{
		if (m_backBuffer) m_backBuffer->AddRef();
	}
	~StubD3D8SwapChain()
	{
		if (m_backBuffer) m_backBuffer->Release();
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DSwapChain8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}
	STDMETHOD(Present)(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
	STDMETHOD(GetBackBuffer)(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer) override
	{
		if (ppBackBuffer == nullptr) return E_POINTER;
		*ppBackBuffer = m_backBuffer;
		if (m_backBuffer) m_backBuffer->AddRef();
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
	IDirect3DDevice8* m_device;
	IDirect3DSurface8* m_backBuffer;
};

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------
class StubD3D8Device final : public IDirect3DDevice8
{
	struct StageStateKey
	{
		DWORD stage;
		DWORD type;

		bool operator==(const StageStateKey & rhs) const
		{
			return stage == rhs.stage && type == rhs.type;
		}
	};
	struct StageStateKeyHash
	{
		size_t operator()(const StageStateKey & key) const
		{
			return (static_cast<size_t>(key.stage) << 8) ^ static_cast<size_t>(key.type);
		}
	};

public:
	StubD3D8Device(IDirect3D8* parent, HWND focusWindow, UINT width, UINT height)
		: m_refCount(1), m_parent(parent), m_focusWindow(focusWindow), m_width(width), m_height(height),
		  m_backBuffer(nullptr), m_depthStencil(nullptr)
	{
		if (m_parent) m_parent->AddRef();
		m_backBuffer = new StubD3D8Surface(this, nullptr, width, height, D3DFMT_A8R8G8B8);
		m_depthStencil = new StubD3D8Surface(this, nullptr, width, height, D3DFMT_D24S8);
	}
	~StubD3D8Device()
	{
		if (m_depthStencil) m_depthStencil->Release();
		if (m_backBuffer) m_backBuffer->Release();
		if (m_parent) m_parent->Release();
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3DDevice8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}

	STDMETHOD(TestCooperativeLevel)() override { return D3D_OK; }
	STDMETHOD_(UINT, GetAvailableTextureMem)() override { return 512u * 1024u * 1024u; }
	STDMETHOD(ResourceManagerDiscardBytes)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetDirect3D)(IDirect3D8** ppD3D8) override
	{
		if (ppD3D8 == nullptr) return E_POINTER;
		*ppD3D8 = m_parent;
		if (m_parent) m_parent->AddRef();
		return D3D_OK;
	}
	STDMETHOD(GetDeviceCaps)(D3DCAPS8* pCaps) override
	{
		if (pCaps == nullptr) return E_POINTER;
		FillCaps(*pCaps);
		return D3D_OK;
	}
	STDMETHOD(GetDisplayMode)(D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr) return E_POINTER;
		pMode->Width = m_width;
		pMode->Height = m_height;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters) override
	{
		if (pParameters == nullptr) return E_POINTER;
		pParameters->AdapterOrdinal = 0;
		pParameters->DeviceType = D3DDEVTYPE_HAL;
		pParameters->hFocusWindow = m_focusWindow;
		pParameters->BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		return D3D_OK;
	}
	STDMETHOD(SetCursorProperties)(UINT, UINT, IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD_(void, SetCursorPosition)(UINT, UINT, DWORD) override {}
	STDMETHOD_(BOOL, ShowCursor)(BOOL) override { return TRUE; }
	STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8** pSwapChain) override
	{
		if (pSwapChain == nullptr) return E_POINTER;
		*pSwapChain = new StubD3D8SwapChain(this, m_backBuffer);
		return D3D_OK;
	}
	STDMETHOD(Reset)(D3DPRESENT_PARAMETERS*) override { return D3D_OK; }
	STDMETHOD(Present)(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
	STDMETHOD(GetBackBuffer)(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer) override
	{
		if (ppBackBuffer == nullptr) return E_POINTER;
		*ppBackBuffer = m_backBuffer;
		if (m_backBuffer) m_backBuffer->AddRef();
		return D3D_OK;
	}
	STDMETHOD(GetRasterStatus)(D3DRASTER_STATUS* pRasterStatus) override
	{
		if (pRasterStatus) { pRasterStatus->InVBlank = FALSE; pRasterStatus->ScanLine = 0; }
		return D3D_OK;
	}
	STDMETHOD_(void, SetGammaRamp)(DWORD, CONST D3DGAMMARAMP*) override {}
	STDMETHOD_(void, GetGammaRamp)(D3DGAMMARAMP*) override {}

	STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) override
	{
		if (ppTexture == nullptr) return E_POINTER;
		*ppTexture = new StubD3D8Texture(this, Width, Height, Levels, Usage, Format, Pool);
		return D3D_OK;
	}
	STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DVolumeTexture8** ppVolumeTexture) override
	{
		if (ppVolumeTexture == nullptr) return E_POINTER;
		*ppVolumeTexture = new StubD3D8VolumeTexture(this, Width, Height, Depth, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DCubeTexture8** ppCubeTexture) override
	{
		if (ppCubeTexture == nullptr) return E_POINTER;
		*ppCubeTexture = new StubD3D8CubeTexture(this, EdgeLength, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) override
	{
		if (ppVertexBuffer == nullptr) return E_POINTER;
		*ppVertexBuffer = new StubD3D8VertexBuffer(this, Length, Usage, FVF, Pool);
		return D3D_OK;
	}
	STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) override
	{
		if (ppIndexBuffer == nullptr) return E_POINTER;
		*ppIndexBuffer = new StubD3D8IndexBuffer(this, Length, Usage, Format, Pool);
		return D3D_OK;
	}
	STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, BOOL, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr) return E_POINTER;
		*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr) return E_POINTER;
		*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format);
		return D3D_OK;
	}
	STDMETHOD(CreateImageSurface)(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr) return E_POINTER;
		*ppSurface = new StubD3D8Surface(this, nullptr, Width, Height, Format);
		return D3D_OK;
	}

	// TheSuperHackers @bugfix bobtista 22/04/2026 Phase 5.2 — D3DX8 loads
	// texture files via Create (MANAGED) + Create (SYSTEMMEM scratch) +
	// UpdateTexture(scratch → managed). A no-op UpdateTexture leaves
	// the managed texture empty, which is why infantry / fonts / HUD
	// textures were rendering black. Lock both sides and memcpy.
	STDMETHOD(CopyRects)(IDirect3DSurface8* src, CONST RECT* srcRects, UINT count, IDirect3DSurface8* dst, CONST POINT* dstPts) override
	{
		if (src == nullptr || dst == nullptr) return D3D_OK;
		D3DSURFACE_DESC sd, dd;
		if (FAILED(src->GetDesc(&sd)) || FAILED(dst->GetDesc(&dd))) return D3D_OK;
		D3DLOCKED_RECT sl = {}, dl = {};
		if (FAILED(src->LockRect(&sl, nullptr, 0))) return D3D_OK;
		if (FAILED(dst->LockRect(&dl, nullptr, 0))) { src->UnlockRect(); return D3D_OK; }
		if (count == 0 || srcRects == nullptr)
		{
			const UINT h = SurfaceRows(sd.Format, sd.Height) < SurfaceRows(dd.Format, dd.Height)
				? SurfaceRows(sd.Format, sd.Height) : SurfaceRows(dd.Format, dd.Height);
			const UINT rowBytes = SurfacePitch(sd.Format, sd.Width) < SurfacePitch(dd.Format, dd.Width)
				? SurfacePitch(sd.Format, sd.Width) : SurfacePitch(dd.Format, dd.Width);
			for (UINT y = 0; y < h; ++y)
			{
				std::memcpy(
					static_cast<uint8_t*>(dl.pBits) + y * dl.Pitch,
					static_cast<const uint8_t*>(sl.pBits) + y * sl.Pitch,
					rowBytes);
			}
		}
		else
		{
			if (IsCompressedFormat(sd.Format) || IsCompressedFormat(dd.Format))
			{
				WWDEBUG_SAY(("[StubD3D8] CopyRects: compressed rect copy not implemented, skipping"));
				dst->UnlockRect();
				src->UnlockRect();
				return E_NOTIMPL;
			}
			const UINT bpp = BytesPerPixel(sd.Format);
			for (UINT i = 0; i < count; ++i)
			{
				const RECT& r = srcRects[i];
				const LONG dx = dstPts ? dstPts[i].x : r.left;
				const LONG dy = dstPts ? dstPts[i].y : r.top;
				const LONG rw = r.right - r.left;
				const LONG rh = r.bottom - r.top;
				for (LONG y = 0; y < rh; ++y)
				{
					std::memcpy(
						static_cast<uint8_t*>(dl.pBits) + (dy + y) * dl.Pitch + dx * bpp,
						static_cast<const uint8_t*>(sl.pBits) + (r.top + y) * sl.Pitch + r.left * bpp,
						rw * bpp);
				}
			}
		}
		dst->UnlockRect();
		src->UnlockRect();
		return D3D_OK;
	}
	STDMETHOD(UpdateTexture)(IDirect3DBaseTexture8* src, IDirect3DBaseTexture8* dst) override
	{
		if (src == nullptr || dst == nullptr) return D3D_OK;
		if (src->GetType() != D3DRTYPE_TEXTURE || dst->GetType() != D3DRTYPE_TEXTURE) return D3D_OK;
		IDirect3DTexture8* st = static_cast<IDirect3DTexture8*>(src);
		IDirect3DTexture8* dt = static_cast<IDirect3DTexture8*>(dst);
		DWORD srcLevels = st->GetLevelCount();
		DWORD dstLevels = dt->GetLevelCount();
		DWORD levels = srcLevels < dstLevels ? srcLevels : dstLevels;
		for (DWORD i = 0; i < levels; ++i)
		{
			D3DSURFACE_DESC sd, dd;
			if (FAILED(st->GetLevelDesc(i, &sd)) || FAILED(dt->GetLevelDesc(i, &dd))) break;
			D3DLOCKED_RECT sl = {}, dl = {};
			if (FAILED(st->LockRect(i, &sl, nullptr, 0))) break;
			if (FAILED(dt->LockRect(i, &dl, nullptr, 0))) { st->UnlockRect(i); break; }
			const UINT h = SurfaceRows(sd.Format, sd.Height) < SurfaceRows(dd.Format, dd.Height)
				? SurfaceRows(sd.Format, sd.Height) : SurfaceRows(dd.Format, dd.Height);
			const UINT rowBytes = SurfacePitch(sd.Format, sd.Width) < SurfacePitch(dd.Format, dd.Width)
				? SurfacePitch(sd.Format, sd.Width) : SurfacePitch(dd.Format, dd.Width);
			for (UINT y = 0; y < h; ++y)
			{
				std::memcpy(
					static_cast<uint8_t*>(dl.pBits) + y * dl.Pitch,
					static_cast<const uint8_t*>(sl.pBits) + y * sl.Pitch,
					rowBytes);
			}
			dt->UnlockRect(i);
			st->UnlockRect(i);
		}
		return D3D_OK;
	}
	STDMETHOD(GetFrontBuffer)(IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD(SetRenderTarget)(IDirect3DSurface8*, IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD(GetRenderTarget)(IDirect3DSurface8** ppRenderTarget) override
	{
		if (ppRenderTarget == nullptr) return E_POINTER;
		*ppRenderTarget = m_backBuffer;
		if (m_backBuffer) m_backBuffer->AddRef();
		return D3D_OK;
	}
	STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface8** ppZStencilSurface) override
	{
		if (ppZStencilSurface == nullptr) return E_POINTER;
		*ppZStencilSurface = m_depthStencil;
		if (m_depthStencil) m_depthStencil->AddRef();
		return D3D_OK;
	}
	STDMETHOD(BeginScene)() override { return D3D_OK; }
	STDMETHOD(EndScene)() override { return D3D_OK; }
	STDMETHOD(Clear)(DWORD, CONST D3DRECT*, DWORD, D3DCOLOR, float, DWORD) override { return D3D_OK; }
	STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* m) override
	{
		// TheSuperHackers @bugfix bobtista 22/04/2026 Phase 5.2 —
		// W3DWater and W3DTreeBuffer read back the current view/world
		// transform via DX8Wrapper::_Get_DX8_Transform, compute inverses
		// and feed shader constants. A GetTransform that returns zero
		// yields a singular matrix (det = 0), producing NaN shader
		// inputs and visible banding artifacts on terrain water/tree
		// passes. Record what the game sets so GetTransform can return
		// the real matrix.
		if (m) m_transforms[static_cast<DWORD>(state)] = *m;
		return D3D_OK;
	}
	STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE state, D3DMATRIX* pMatrix) override
	{
		if (pMatrix == nullptr) return E_POINTER;
		auto it = m_transforms.find(static_cast<DWORD>(state));
		if (it != m_transforms.end())
		{
			*pMatrix = it->second;
		}
		else
		{
			// Identity default.
			std::memset(pMatrix, 0, sizeof(*pMatrix));
			pMatrix->_11 = pMatrix->_22 = pMatrix->_33 = pMatrix->_44 = 1.0f;
		}
		return D3D_OK;
	}
	STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override { return D3D_OK; }
	STDMETHOD(SetViewport)(CONST D3DVIEWPORT8*) override { return D3D_OK; }
	STDMETHOD(GetViewport)(D3DVIEWPORT8* pViewport) override
	{
		if (pViewport)
		{
			pViewport->X = 0; pViewport->Y = 0;
			pViewport->Width = m_width; pViewport->Height = m_height;
			pViewport->MinZ = 0.0f; pViewport->MaxZ = 1.0f;
		}
		return D3D_OK;
	}
	STDMETHOD(SetMaterial)(CONST D3DMATERIAL8*) override { return D3D_OK; }
	STDMETHOD(GetMaterial)(D3DMATERIAL8* pMaterial) override
	{
		if (pMaterial) std::memset(pMaterial, 0, sizeof(*pMaterial));
		return D3D_OK;
	}
	STDMETHOD(SetLight)(DWORD, CONST D3DLIGHT8*) override { return D3D_OK; }
	STDMETHOD(GetLight)(DWORD, D3DLIGHT8* pLight) override
	{
		if (pLight) std::memset(pLight, 0, sizeof(*pLight));
		return D3D_OK;
	}
	STDMETHOD(LightEnable)(DWORD, BOOL) override { return D3D_OK; }
	STDMETHOD(GetLightEnable)(DWORD, BOOL* pEnable) override
	{
		if (pEnable) *pEnable = FALSE;
		return D3D_OK;
	}
	STDMETHOD(SetClipPlane)(DWORD, CONST float*) override { return D3D_OK; }
	STDMETHOD(GetClipPlane)(DWORD, float* pPlane) override
	{
		if (pPlane) std::memset(pPlane, 0, 4 * sizeof(float));
		return D3D_OK;
	}
	STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE state, DWORD value) override
	{
		m_renderStates[static_cast<DWORD>(state)] = value;
		return D3D_OK;
	}
	STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE state, DWORD* pValue) override
	{
		if (pValue)
		{
			auto it = m_renderStates.find(static_cast<DWORD>(state));
			*pValue = (it != m_renderStates.end()) ? it->second : 0;
		}
		return D3D_OK;
	}
	STDMETHOD(BeginStateBlock)() override { return D3D_OK; }
	STDMETHOD(EndStateBlock)(DWORD* pToken) override
	{
		if (pToken) *pToken = 0;
		return D3D_OK;
	}
	STDMETHOD(ApplyStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(CaptureStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(DeleteStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE, DWORD* pToken) override
	{
		if (pToken) *pToken = 0;
		return D3D_OK;
	}
	STDMETHOD(SetClipStatus)(CONST D3DCLIPSTATUS8*) override { return D3D_OK; }
	STDMETHOD(GetClipStatus)(D3DCLIPSTATUS8* pClipStatus) override
	{
		if (pClipStatus) std::memset(pClipStatus, 0, sizeof(*pClipStatus));
		return D3D_OK;
	}
	STDMETHOD(GetTexture)(DWORD, IDirect3DBaseTexture8** ppTexture) override
	{
		if (ppTexture) *ppTexture = nullptr;
		return D3D_OK;
	}
	STDMETHOD(SetTexture)(DWORD, IDirect3DBaseTexture8*) override { return D3D_OK; }
	STDMETHOD(GetTextureStageState)(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* pValue) override
	{
		if (pValue == nullptr)
		{
			return E_POINTER;
		}
		const StageStateKey key = { stage, static_cast<DWORD>(type) };
		std::unordered_map<StageStateKey, DWORD, StageStateKeyHash>::const_iterator it = m_textureStageStates.find(key);
		*pValue = (it != m_textureStageStates.end()) ? it->second : 0;
		return D3D_OK;
	}
	STDMETHOD(SetTextureStageState)(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
	{
		const StageStateKey key = { stage, static_cast<DWORD>(type) };
		m_textureStageStates[key] = value;
		return D3D_OK;
	}
	STDMETHOD(ValidateDevice)(DWORD* pNumPasses) override
	{
		if (pNumPasses) *pNumPasses = 1;
		return D3D_OK;
	}
	STDMETHOD(GetInfo)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(SetPaletteEntries)(UINT, CONST PALETTEENTRY*) override { return D3D_OK; }
	STDMETHOD(GetPaletteEntries)(UINT, PALETTEENTRY*) override { return D3D_OK; }
	STDMETHOD(SetCurrentTexturePalette)(UINT) override { return D3D_OK; }
	STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) override
	{
		if (PaletteNumber) *PaletteNumber = 0;
		return D3D_OK;
	}
	STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE, UINT, UINT) override { return D3D_OK; }
	STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT) override { return D3D_OK; }
	STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE, UINT, CONST void*, UINT) override { return D3D_OK; }
	STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE, UINT, UINT, UINT, CONST void*, D3DFORMAT, CONST void*, UINT) override { return D3D_OK; }
	STDMETHOD(ProcessVertices)(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) override { return D3D_OK; }
	STDMETHOD(CreateVertexShader)(CONST DWORD*, CONST DWORD*, DWORD* pHandle, DWORD) override
	{
		if (pHandle) *pHandle = 1;
		return D3D_OK;
	}
	STDMETHOD(SetVertexShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShader)(DWORD* pHandle) override
	{
		if (pHandle) *pHandle = 0;
		return D3D_OK;
	}
	STDMETHOD(DeleteVertexShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(SetVertexShaderConstant)(DWORD, CONST void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShaderConstant)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShaderDeclaration)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData) *pSizeOfData = 0;
		return D3D_OK;
	}
	STDMETHOD(GetVertexShaderFunction)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData) *pSizeOfData = 0;
		return D3D_OK;
	}
	STDMETHOD(SetStreamSource)(UINT, IDirect3DVertexBuffer8*, UINT) override { return D3D_OK; }
	STDMETHOD(GetStreamSource)(UINT, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride) override
	{
		if (ppStreamData) *ppStreamData = nullptr;
		if (pStride) *pStride = 0;
		return D3D_OK;
	}
	STDMETHOD(SetIndices)(IDirect3DIndexBuffer8*, UINT) override { return D3D_OK; }
	STDMETHOD(GetIndices)(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex) override
	{
		if (ppIndexData) *ppIndexData = nullptr;
		if (pBaseVertexIndex) *pBaseVertexIndex = 0;
		return D3D_OK;
	}
	STDMETHOD(CreatePixelShader)(CONST DWORD*, DWORD* pHandle) override
	{
		if (pHandle) *pHandle = 1;
		return D3D_OK;
	}
	STDMETHOD(SetPixelShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShader)(DWORD* pHandle) override
	{
		if (pHandle) *pHandle = 0;
		return D3D_OK;
	}
	STDMETHOD(DeletePixelShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(SetPixelShaderConstant)(DWORD, CONST void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShaderConstant)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShaderFunction)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData) *pSizeOfData = 0;
		return D3D_OK;
	}
	STDMETHOD(DrawRectPatch)(UINT, CONST float*, CONST D3DRECTPATCH_INFO*) override { return D3D_OK; }
	STDMETHOD(DrawTriPatch)(UINT, CONST float*, CONST D3DTRIPATCH_INFO*) override { return D3D_OK; }
	STDMETHOD(DeletePatch)(UINT) override { return D3D_OK; }

private:
	std::atomic<ULONG> m_refCount;
	IDirect3D8* m_parent;
	HWND m_focusWindow;
	UINT m_width;
	UINT m_height;
	IDirect3DSurface8* m_backBuffer;
	IDirect3DSurface8* m_depthStencil;
	std::unordered_map<DWORD, D3DMATRIX> m_transforms;
	std::unordered_map<DWORD, DWORD> m_renderStates;
	std::unordered_map<StageStateKey, DWORD, StageStateKeyHash> m_textureStageStates;
};

// ---------------------------------------------------------------------------
// Direct3D 8 factory interface
// ---------------------------------------------------------------------------
class StubD3D8Interface final : public IDirect3D8
{
public:
	StubD3D8Interface() : m_refCount(1) {}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr) return E_POINTER;
		if (riid == IID_IDirect3D8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0) delete this;
		return r;
	}

	STDMETHOD(RegisterSoftwareDevice)(void*) override { return D3D_OK; }
	STDMETHOD_(UINT, GetAdapterCount)() override { return 1; }
	STDMETHOD(GetAdapterIdentifier)(UINT, DWORD, D3DADAPTER_IDENTIFIER8* pIdentifier) override
	{
		if (pIdentifier == nullptr) return E_POINTER;
		std::memset(pIdentifier, 0, sizeof(*pIdentifier));
		std::strncpy(pIdentifier->Driver, "StubD3D8", sizeof(pIdentifier->Driver) - 1);
		std::strncpy(pIdentifier->Description, "Generals bgfx standalone stub", sizeof(pIdentifier->Description) - 1);
		pIdentifier->DriverVersion.QuadPart = 0;
		pIdentifier->VendorId = 0;
		pIdentifier->DeviceId = 0;
		pIdentifier->SubSysId = 0;
		pIdentifier->Revision = 0;
		pIdentifier->WHQLLevel = 0;
		return D3D_OK;
	}
	STDMETHOD_(UINT, GetAdapterModeCount)(UINT) override { return 1; }
	STDMETHOD(EnumAdapterModes)(UINT, UINT, D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr) return E_POINTER;
		pMode->Width = 1920;
		pMode->Height = 1080;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(GetAdapterDisplayMode)(UINT, D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr) return E_POINTER;
		pMode->Width = 1920;
		pMode->Height = 1080;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(CheckDeviceType)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) override { return S_OK; }
	STDMETHOD(CheckDeviceFormat)(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override { return S_OK; }
	STDMETHOD(CheckDeviceMultiSampleType)(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE) override { return S_OK; }
	STDMETHOD(CheckDepthStencilMatch)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override { return S_OK; }
	STDMETHOD(GetDeviceCaps)(UINT, D3DDEVTYPE, D3DCAPS8* pCaps) override
	{
		if (pCaps == nullptr) return E_POINTER;
		FillCaps(*pCaps);
		return D3D_OK;
	}
	STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT) override { return NULL; }
	STDMETHOD(CreateDevice)(UINT, D3DDEVTYPE, HWND hFocusWindow, DWORD, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) override
	{
		if (ppReturnedDeviceInterface == nullptr) return E_POINTER;
		UINT width = 1920;
		UINT height = 1080;
		if (pPresentationParameters)
		{
			if (pPresentationParameters->BackBufferWidth) width = pPresentationParameters->BackBufferWidth;
			if (pPresentationParameters->BackBufferHeight) height = pPresentationParameters->BackBufferHeight;
		}
		*ppReturnedDeviceInterface = new StubD3D8Device(this, hFocusWindow, width, height);
		return D3D_OK;
	}

private:
	std::atomic<ULONG> m_refCount;
};

} // namespace

IDirect3D8* CreateStubD3D8Interface()
{
	return new StubD3D8Interface();
}

#endif // GGC_BGFX_STANDALONE
