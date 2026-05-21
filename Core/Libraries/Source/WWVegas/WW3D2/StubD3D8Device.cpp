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

// TheSuperHackers @refactor bobtista 22/04/2026 Stage 1
// No-op implementation of the Direct3D 8 interface/device calls. This allows
// DX8Wrapper to be driven against a synthetic Direct3D 8 device without
// loading d3d8.dll at runtime. Resource creation methods assert and fail so
// standalone bgfx cannot silently fall back to fake D3D objects.

#include "StubD3D8Device.h"

#if defined(GGC_BGFX_STANDALONE)

#include "wwdebug.h"

#include <atomic>
#include <cstring>
#include <unordered_map>

namespace
{


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
	// TheSuperHackers @bugfix bobtista 22/04/2026 DO NOT set
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
		if (m_parent)
		{
			m_parent->AddRef();
		}
	}
	~StubD3D8Device()
	{
		if (m_depthStencil)
		{
			m_depthStencil->Release();
		}
		if (m_backBuffer)
		{
			m_backBuffer->Release();
		}
		if (m_parent)
		{
			m_parent->Release();
		}
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3DDevice8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}

	STDMETHOD(TestCooperativeLevel)() override { return D3D_OK; }
	STDMETHOD_(UINT, GetAvailableTextureMem)() override { return 512u * 1024u * 1024u; }
	STDMETHOD(ResourceManagerDiscardBytes)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetDirect3D)(IDirect3D8** ppD3D8) override
	{
		if (ppD3D8 == nullptr)
		{
			return E_POINTER;
		}
		*ppD3D8 = m_parent;
		if (m_parent)
		{
			m_parent->AddRef();
		}
		return D3D_OK;
	}
	STDMETHOD(GetDeviceCaps)(D3DCAPS8* pCaps) override
	{
		if (pCaps == nullptr)
		{
			return E_POINTER;
		}
		FillCaps(*pCaps);
		return D3D_OK;
	}
	STDMETHOD(GetDisplayMode)(D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr)
		{
			return E_POINTER;
		}
		pMode->Width = m_width;
		pMode->Height = m_height;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters) override
	{
		if (pParameters == nullptr)
		{
			return E_POINTER;
		}
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
		if (pSwapChain == nullptr)
		{
			return E_POINTER;
		}
		*pSwapChain = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateAdditionalSwapChain: standalone bgfx cannot create fake-D3D swap chains");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(Reset)(D3DPRESENT_PARAMETERS*) override { return D3D_OK; }
	STDMETHOD(Present)(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
	STDMETHOD(GetBackBuffer)(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer) override
	{
		if (ppBackBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppBackBuffer = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::GetBackBuffer: standalone bgfx cannot return fake-D3D back buffers");
		return D3DERR_INVALIDCALL;
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
		if (ppTexture == nullptr)
		{
			return E_POINTER;
		}
		*ppTexture = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateTexture: standalone bgfx cannot create fake-D3D textures");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DVolumeTexture8** ppVolumeTexture) override
	{
		if (ppVolumeTexture == nullptr)
		{
			return E_POINTER;
		}
		*ppVolumeTexture = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateVolumeTexture: standalone bgfx cannot create fake-D3D volume textures");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DCubeTexture8** ppCubeTexture) override
	{
		if (ppCubeTexture == nullptr)
		{
			return E_POINTER;
		}
		*ppCubeTexture = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateCubeTexture: standalone bgfx cannot create fake-D3D cube textures");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) override
	{
		if (ppVertexBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppVertexBuffer = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateVertexBuffer: standalone bgfx cannot create fake-D3D vertex buffers");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) override
	{
		if (ppIndexBuffer == nullptr)
		{
			return E_POINTER;
		}
		*ppIndexBuffer = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateIndexBuffer: standalone bgfx cannot create fake-D3D index buffers");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, BOOL, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
		*ppSurface = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateRenderTarget: standalone bgfx cannot create fake-D3D render targets");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
		*ppSurface = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateDepthStencilSurface: standalone bgfx cannot create fake-D3D depth surfaces");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(CreateImageSurface)(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface) override
	{
		if (ppSurface == nullptr)
		{
			return E_POINTER;
		}
		*ppSurface = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::CreateImageSurface: standalone bgfx cannot create fake-D3D image surfaces");
		return D3DERR_INVALIDCALL;
	}

	STDMETHOD(CopyRects)(IDirect3DSurface8* src, CONST RECT* srcRects, UINT count, IDirect3DSurface8* dst, CONST POINT* dstPts) override
	{
		(void)src;
		(void)srcRects;
		(void)count;
		(void)dst;
		(void)dstPts;
		WWASSERT_PRINT(false, "StubD3D8Device::CopyRects: standalone bgfx cannot copy fake-D3D surfaces");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(UpdateTexture)(IDirect3DBaseTexture8* src, IDirect3DBaseTexture8* dst) override
	{
		(void)src;
		(void)dst;
		WWASSERT_PRINT(false, "StubD3D8Device::UpdateTexture: standalone bgfx cannot update fake-D3D textures");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(GetFrontBuffer)(IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD(SetRenderTarget)(IDirect3DSurface8*, IDirect3DSurface8*) override { return D3D_OK; }
	STDMETHOD(GetRenderTarget)(IDirect3DSurface8** ppRenderTarget) override
	{
		if (ppRenderTarget == nullptr)
		{
			return E_POINTER;
		}
		*ppRenderTarget = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::GetRenderTarget: standalone bgfx cannot return fake-D3D render targets");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface8** ppZStencilSurface) override
	{
		if (ppZStencilSurface == nullptr)
		{
			return E_POINTER;
		}
		*ppZStencilSurface = nullptr;
		WWASSERT_PRINT(false, "StubD3D8Device::GetDepthStencilSurface: standalone bgfx cannot return fake-D3D depth surfaces");
		return D3DERR_INVALIDCALL;
	}
	STDMETHOD(BeginScene)() override { return D3D_OK; }
	STDMETHOD(EndScene)() override { return D3D_OK; }
	STDMETHOD(Clear)(DWORD, CONST D3DRECT*, DWORD, D3DCOLOR, float, DWORD) override { return D3D_OK; }
	STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* m) override
	{
		// TheSuperHackers @bugfix bobtista 22/04/2026 W3DWater and
		// W3DTreeBuffer read back the current view/world
		// transform via DX8Wrapper::_Get_DX8_Transform, compute inverses
		// and feed shader constants. A GetTransform that returns zero
		// yields a singular matrix (det = 0), producing NaN shader
		// inputs and visible banding artifacts on terrain water/tree
		// passes. Record what the game sets so GetTransform can return
		// the real matrix.
		if (m)
		{
			m_transforms[static_cast<DWORD>(state)] = *m;
		}
		return D3D_OK;
	}
	STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE state, D3DMATRIX* pMatrix) override
	{
		if (pMatrix == nullptr)
		{
			return E_POINTER;
		}
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
		if (pMaterial)
		{
			std::memset(pMaterial, 0, sizeof(*pMaterial));
		}
		return D3D_OK;
	}
	STDMETHOD(SetLight)(DWORD, CONST D3DLIGHT8*) override { return D3D_OK; }
	STDMETHOD(GetLight)(DWORD, D3DLIGHT8* pLight) override
	{
		if (pLight)
		{
			std::memset(pLight, 0, sizeof(*pLight));
		}
		return D3D_OK;
	}
	STDMETHOD(LightEnable)(DWORD, BOOL) override { return D3D_OK; }
	STDMETHOD(GetLightEnable)(DWORD, BOOL* pEnable) override
	{
		if (pEnable)
		{
			*pEnable = FALSE;
		}
		return D3D_OK;
	}
	STDMETHOD(SetClipPlane)(DWORD, CONST float*) override { return D3D_OK; }
	STDMETHOD(GetClipPlane)(DWORD, float* pPlane) override
	{
		if (pPlane)
		{
			std::memset(pPlane, 0, 4 * sizeof(float));
		}
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
		if (pToken)
		{
			*pToken = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(ApplyStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(CaptureStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(DeleteStateBlock)(DWORD) override { return D3D_OK; }
	STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE, DWORD* pToken) override
	{
		if (pToken)
		{
			*pToken = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(SetClipStatus)(CONST D3DCLIPSTATUS8*) override { return D3D_OK; }
	STDMETHOD(GetClipStatus)(D3DCLIPSTATUS8* pClipStatus) override
	{
		if (pClipStatus)
		{
			std::memset(pClipStatus, 0, sizeof(*pClipStatus));
		}
		return D3D_OK;
	}
	STDMETHOD(GetTexture)(DWORD, IDirect3DBaseTexture8** ppTexture) override
	{
		if (ppTexture)
		{
			*ppTexture = nullptr;
		}
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
		if (pNumPasses)
		{
			*pNumPasses = 1;
		}
		return D3D_OK;
	}
	STDMETHOD(GetInfo)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(SetPaletteEntries)(UINT, CONST PALETTEENTRY*) override { return D3D_OK; }
	STDMETHOD(GetPaletteEntries)(UINT, PALETTEENTRY*) override { return D3D_OK; }
	STDMETHOD(SetCurrentTexturePalette)(UINT) override { return D3D_OK; }
	STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) override
	{
		if (PaletteNumber)
		{
			*PaletteNumber = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE, UINT, UINT) override { return D3D_OK; }
	STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT) override { return D3D_OK; }
	STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE, UINT, CONST void*, UINT) override { return D3D_OK; }
	STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE, UINT, UINT, UINT, CONST void*, D3DFORMAT, CONST void*, UINT) override { return D3D_OK; }
	STDMETHOD(ProcessVertices)(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) override { return D3D_OK; }
	STDMETHOD(CreateVertexShader)(CONST DWORD*, CONST DWORD*, DWORD* pHandle, DWORD) override
	{
		if (pHandle)
		{
			*pHandle = 1;
		}
		return D3D_OK;
	}
	STDMETHOD(SetVertexShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShader)(DWORD* pHandle) override
	{
		if (pHandle)
		{
			*pHandle = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DeleteVertexShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(SetVertexShaderConstant)(DWORD, CONST void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShaderConstant)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetVertexShaderDeclaration)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData)
		{
			*pSizeOfData = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(GetVertexShaderFunction)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData)
		{
			*pSizeOfData = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(SetStreamSource)(UINT, IDirect3DVertexBuffer8*, UINT) override { return D3D_OK; }
	STDMETHOD(GetStreamSource)(UINT, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride) override
	{
		if (ppStreamData)
		{
			*ppStreamData = nullptr;
		}
		if (pStride)
		{
			*pStride = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(SetIndices)(IDirect3DIndexBuffer8*, UINT) override { return D3D_OK; }
	STDMETHOD(GetIndices)(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex) override
	{
		if (ppIndexData)
		{
			*ppIndexData = nullptr;
		}
		if (pBaseVertexIndex)
		{
			*pBaseVertexIndex = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(CreatePixelShader)(CONST DWORD*, DWORD* pHandle) override
	{
		if (pHandle)
		{
			*pHandle = 1;
		}
		return D3D_OK;
	}
	STDMETHOD(SetPixelShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShader)(DWORD* pHandle) override
	{
		if (pHandle)
		{
			*pHandle = 0;
		}
		return D3D_OK;
	}
	STDMETHOD(DeletePixelShader)(DWORD) override { return D3D_OK; }
	STDMETHOD(SetPixelShaderConstant)(DWORD, CONST void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShaderConstant)(DWORD, void*, DWORD) override { return D3D_OK; }
	STDMETHOD(GetPixelShaderFunction)(DWORD, void*, DWORD* pSizeOfData) override
	{
		if (pSizeOfData)
		{
			*pSizeOfData = 0;
		}
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
		if (ppv == nullptr)
		{
			return E_POINTER;
		}
		if (riid == IID_IDirect3D8 || riid == IID_IUnknown)
		{ *ppv = this; AddRef(); return S_OK; }
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return ++m_refCount; }
	STDMETHOD_(ULONG, Release)() override
	{
		ULONG r = --m_refCount;
		if (r == 0)
		{
			delete this;
		}
		return r;
	}

	STDMETHOD(RegisterSoftwareDevice)(void*) override { return D3D_OK; }
	STDMETHOD_(UINT, GetAdapterCount)() override { return 1; }
	STDMETHOD(GetAdapterIdentifier)(UINT, DWORD, D3DADAPTER_IDENTIFIER8* pIdentifier) override
	{
		if (pIdentifier == nullptr)
		{
			return E_POINTER;
		}
		std::memset(pIdentifier, 0, sizeof(*pIdentifier));
		std::strncpy(pIdentifier->Driver, "StubD3D8", sizeof(pIdentifier->Driver) - 1);
		std::strncpy(pIdentifier->Description, "Generals bgfx standalone stub", sizeof(pIdentifier->Description) - 1);
		// TheSuperHackers @build bobtista 29/04/2026 The DX8 SDK declares
		// DriverVersion as LARGE_INTEGER only on _WIN32; non-Windows builds
		// see two DWORDs instead. memset already zeroed both paths so the
		// explicit assignment can be skipped on non-Windows.
#ifdef _WIN32
		pIdentifier->DriverVersion.QuadPart = 0;
#endif
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
		if (pMode == nullptr)
		{
			return E_POINTER;
		}
		pMode->Width = 1920;
		pMode->Height = 1080;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_A8R8G8B8;
		return D3D_OK;
	}
	STDMETHOD(GetAdapterDisplayMode)(UINT, D3DDISPLAYMODE* pMode) override
	{
		if (pMode == nullptr)
		{
			return E_POINTER;
		}
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
		if (pCaps == nullptr)
		{
			return E_POINTER;
		}
		FillCaps(*pCaps);
		return D3D_OK;
	}
	STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT) override { return NULL; }
	STDMETHOD(CreateDevice)(UINT, D3DDEVTYPE, HWND hFocusWindow, DWORD, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) override
	{
		if (ppReturnedDeviceInterface == nullptr)
		{
			return E_POINTER;
		}
		UINT width = 1920;
		UINT height = 1080;
		if (pPresentationParameters)
		{
			if (pPresentationParameters->BackBufferWidth)
			{
				width = pPresentationParameters->BackBufferWidth;
			}
			if (pPresentationParameters->BackBufferHeight)
			{
				height = pPresentationParameters->BackBufferHeight;
			}
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
