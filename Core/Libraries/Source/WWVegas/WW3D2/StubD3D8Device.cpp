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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

namespace
{

class StubD3D8Device;

static void FillCaps(D3DCAPS8& caps)
{
	std::memset(&caps, 0, sizeof(caps));
	caps.DeviceType = D3DDEVTYPE_HAL;
	caps.AdapterOrdinal = 0;
	caps.Caps = 0;
	caps.Caps2 = D3DCAPS2_CANRENDERWINDOWED | D3DCAPS2_DYNAMICTEXTURES;
	caps.Caps3 = 0;
	caps.PresentationIntervals = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_INTERVAL_IMMEDIATE;
	caps.CursorCaps = 0;
	caps.DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE | D3DDEVCAPS_DRAWPRIMTLVERTEX;
	caps.PrimitiveMiscCaps = 0xFFFFFFFF;
	caps.RasterCaps = 0xFFFFFFFF;
	caps.ZCmpCaps = 0xFF;
	caps.SrcBlendCaps = 0xFFFFFFFF;
	caps.DestBlendCaps = 0xFFFFFFFF;
	caps.AlphaCmpCaps = 0xFF;
	caps.ShadeCaps = 0xFFFFFFFF;
	caps.TextureCaps = 0xFFFFFFFF;
	caps.TextureFilterCaps = 0xFFFFFFFF;
	caps.CubeTextureFilterCaps = 0xFFFFFFFF;
	caps.VolumeTextureFilterCaps = 0xFFFFFFFF;
	caps.TextureAddressCaps = 0xFF;
	caps.VolumeTextureAddressCaps = 0xFF;
	caps.LineCaps = 0xFF;
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
	caps.StencilCaps = 0xFF;
	caps.FVFCaps = 8 | D3DFVFCAPS_PSIZE;
	caps.TextureOpCaps = 0xFFFFFFFF;
	caps.MaxTextureBlendStages = 8;
	caps.MaxSimultaneousTextures = 4;
	caps.VertexProcessingCaps = 0xFFFFFFFF;
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
	StubD3D8Surface(IDirect3DDevice8* device, IUnknown* container, UINT width, UINT height, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_container(container), m_width(width), m_height(height), m_format(format)
	{
		const size_t bytes = static_cast<size_t>(width) * height * 4;
		m_scratch.reset(new uint8_t[bytes > 0 ? bytes : 4]);
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
		pDesc->Size = m_width * m_height * 4;
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		return D3D_OK;
	}
	STDMETHOD(LockRect)(D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr) return E_POINTER;
		pLockedRect->Pitch = static_cast<INT>(m_width * 4);
		pLockedRect->pBits = m_scratch.get();
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
	std::unique_ptr<uint8_t[]> m_scratch;
};

// ---------------------------------------------------------------------------
// Vertex Buffer
// ---------------------------------------------------------------------------
class StubD3D8VertexBuffer final : public IDirect3DVertexBuffer8
{
public:
	StubD3D8VertexBuffer(IDirect3DDevice8* device, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_length(length), m_usage(usage), m_fvf(fvf), m_pool(pool)
	{
		m_scratch.reset(new uint8_t[length > 0 ? length : 4]);
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
	std::unique_ptr<uint8_t[]> m_scratch;
};

// ---------------------------------------------------------------------------
// Index Buffer
// ---------------------------------------------------------------------------
class StubD3D8IndexBuffer final : public IDirect3DIndexBuffer8
{
public:
	StubD3D8IndexBuffer(IDirect3DDevice8* device, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_length(length), m_usage(usage), m_format(format), m_pool(pool)
	{
		m_scratch.reset(new uint8_t[length > 0 ? length : 4]);
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
	std::unique_ptr<uint8_t[]> m_scratch;
};

// ---------------------------------------------------------------------------
// Texture (2D)
// ---------------------------------------------------------------------------
class StubD3D8Texture final : public IDirect3DTexture8
{
public:
	StubD3D8Texture(IDirect3DDevice8* device, UINT width, UINT height, DWORD usage, D3DFORMAT format, D3DPOOL pool)
		: m_refCount(1), m_device(device), m_width(width), m_height(height), m_usage(usage), m_format(format), m_pool(pool), m_surface(nullptr)
	{
		const size_t bytes = static_cast<size_t>(width) * height * 4;
		m_scratch.reset(new uint8_t[bytes > 0 ? bytes : 4]);
	}
	~StubD3D8Texture()
	{
		if (m_surface) m_surface->Release();
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
	STDMETHOD_(DWORD, GetLevelCount)() override { return 1; }
	STDMETHOD(GetLevelDesc)(UINT, D3DSURFACE_DESC* pDesc) override
	{
		if (pDesc == nullptr) return E_POINTER;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = m_width * m_height * 4;
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		return D3D_OK;
	}
	STDMETHOD(GetSurfaceLevel)(UINT, IDirect3DSurface8** ppSurfaceLevel) override
	{
		if (ppSurfaceLevel == nullptr) return E_POINTER;
		if (m_surface == nullptr)
		{
			m_surface = new StubD3D8Surface(m_device, static_cast<IDirect3DTexture8*>(this), m_width, m_height, m_format);
		}
		m_surface->AddRef();
		*ppSurfaceLevel = m_surface;
		return D3D_OK;
	}
	STDMETHOD(LockRect)(UINT, D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr) return E_POINTER;
		pLockedRect->Pitch = static_cast<INT>(m_width * 4);
		pLockedRect->pBits = m_scratch.get();
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
	IDirect3DSurface8* m_surface;
	std::unique_ptr<uint8_t[]> m_scratch;
};

// ---------------------------------------------------------------------------
// Cube Texture
// ---------------------------------------------------------------------------
class StubD3D8CubeTexture final : public IDirect3DCubeTexture8
{
public:
	StubD3D8CubeTexture(IDirect3DDevice8* device, UINT edge, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_edge(edge), m_format(format)
	{
		const size_t bytes = static_cast<size_t>(edge) * edge * 4;
		m_scratch.reset(new uint8_t[bytes > 0 ? bytes : 4]);
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
		pDesc->Format = D3DFMT_A8R8G8B8;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = 256 * 256 * 4;
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = 256;
		pDesc->Height = 256;
		return D3D_OK;
	}
	STDMETHOD(GetCubeMapSurface)(D3DCUBEMAP_FACES, UINT, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr) return E_POINTER;
		*ppSurface = new StubD3D8Surface(m_device, static_cast<IDirect3DCubeTexture8*>(this), m_edge, m_edge, m_format);
		return D3D_OK;
	}
	STDMETHOD(LockRect)(D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT* pLockedRect, CONST RECT*, DWORD) override
	{
		if (pLockedRect == nullptr) return E_POINTER;
		pLockedRect->Pitch = static_cast<INT>(m_edge * 4);
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
	std::unique_ptr<uint8_t[]> m_scratch;
};

// ---------------------------------------------------------------------------
// Volume Texture
// ---------------------------------------------------------------------------
class StubD3D8VolumeTexture final : public IDirect3DVolumeTexture8
{
public:
	StubD3D8VolumeTexture(IDirect3DDevice8* device, UINT width, UINT height, UINT depth, D3DFORMAT format)
		: m_refCount(1), m_device(device), m_width(width), m_height(height), m_depth(depth), m_format(format)
	{
		const size_t bytes = static_cast<size_t>(width) * height * depth * 4;
		m_scratch.reset(new uint8_t[bytes > 0 ? bytes : 4]);
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
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_VOLUME;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_DEFAULT;
		pDesc->Size = m_width * m_height * m_depth * 4;
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
		pLockedVolume->RowPitch = static_cast<INT>(m_width * 4);
		pLockedVolume->SlicePitch = static_cast<INT>(m_width * m_height * 4);
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
	std::unique_ptr<uint8_t[]> m_scratch;
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

	STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) override
	{
		if (ppTexture == nullptr) return E_POINTER;
		*ppTexture = new StubD3D8Texture(this, Width, Height, Usage, Format, Pool);
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

	STDMETHOD(CopyRects)(IDirect3DSurface8*, CONST RECT*, UINT, IDirect3DSurface8*, CONST POINT*) override { return D3D_OK; }
	STDMETHOD(UpdateTexture)(IDirect3DBaseTexture8*, IDirect3DBaseTexture8*) override { return D3D_OK; }
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
	STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override { return D3D_OK; }
	STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE, D3DMATRIX* pMatrix) override
	{
		if (pMatrix) std::memset(pMatrix, 0, sizeof(*pMatrix));
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
	STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE, DWORD) override { return D3D_OK; }
	STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE, DWORD* pValue) override
	{
		if (pValue) *pValue = 0;
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
	STDMETHOD(GetTextureStageState)(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD* pValue) override
	{
		if (pValue) *pValue = 0;
		return D3D_OK;
	}
	STDMETHOD(SetTextureStageState)(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) override { return D3D_OK; }
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
