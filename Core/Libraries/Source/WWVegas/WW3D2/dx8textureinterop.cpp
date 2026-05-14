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
**	MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dx8textureinterop.h"

#include <d3d8.h>
#include <d3dx8tex.h>

#include "dx8formatconv.h"
#include "dx8wrapper.h"
#include "ffactory.h"
#include "IRenderBackend.h"
#include "missingtexture.h"
#include "RenderBackend.h"
#include "surfaceclass.h"
#include "texture.h"
#include "textureloader.h"
#include "ww3d.h"

namespace
{
#if defined(GGC_BGFX_STANDALONE)
	D3DPOOL Legacy_Pool_To_D3D(int pool)
	{
		switch (pool)
		{
		case LEGACY_TEXTURE_POOL_DEFAULT: return D3DPOOL_DEFAULT;
		case LEGACY_TEXTURE_POOL_MANAGED: return D3DPOOL_MANAGED;
		case LEGACY_TEXTURE_POOL_SYSTEMMEM: return D3DPOOL_SYSTEMMEM;
		default:
			WWASSERT(0);
			return D3DPOOL_MANAGED;
		}
	}

	IDirect3DDevice8 *Legacy_Device()
	{
		DX8_Assert();
		return DX8_Call_Device();
	}

	void Release_Unused_Assets_For_Texture_Retry()
	{
		TextureClass::Invalidate_Old_Unused_Textures(5000);
		WW3D::_Invalidate_Mesh_Cache();
	}

	bool Should_Retry_Texture_Create(HRESULT result)
	{
		return result == D3DERR_OUTOFVIDEOMEMORY;
	}
#else
	IDirect3DDevice8 *Legacy_Device()
	{
		DX8_Assert();
		return DX8_Call_Device();
	}
#endif

	IDirect3DTexture8 *s_missingTexture = nullptr;
	constexpr unsigned kLegacyMipFilterBox = 5;
}

IDirect3DBaseTexture8 *DX8TextureInterop::Peek_Legacy_Base_Texture(const TextureBaseClass &texture)
{
	texture.LastAccessed=WW3D::Get_Sync_Time();
	return static_cast<IDirect3DBaseTexture8 *>(texture.LegacyTexture);
}

IDirect3DTexture8 *DX8TextureInterop::Peek_Legacy_Texture2D(const TextureBaseClass &texture)
{
	return static_cast<IDirect3DTexture8 *>(Peek_Legacy_Base_Texture(texture));
}

IDirect3DCubeTexture8 *DX8TextureInterop::Peek_Legacy_Cube_Texture(const TextureBaseClass &texture)
{
	return static_cast<IDirect3DCubeTexture8 *>(Peek_Legacy_Base_Texture(texture));
}

IDirect3DVolumeTexture8 *DX8TextureInterop::Peek_Legacy_Volume_Texture(const TextureBaseClass &texture)
{
	return static_cast<IDirect3DVolumeTexture8 *>(Peek_Legacy_Base_Texture(texture));
}

void DX8TextureInterop::Set_Legacy_Base_Texture(TextureBaseClass &texture, IDirect3DBaseTexture8 *native_texture)
{
	// (gth) Generals does stuff directly with the native texture pointer so lets
	// reset the access timer whenever someone messes with this pointer.
	texture.LastAccessed=WW3D::Get_Sync_Time();

	IDirect3DBaseTexture8 *old_texture = static_cast<IDirect3DBaseTexture8 *>(texture.LegacyTexture);
	if (old_texture != nullptr) {
		old_texture->Release();
	}
	texture.LegacyTexture = native_texture;
	if (native_texture != nullptr) {
		native_texture->AddRef();
	}
	texture.Capture_CPU_Texture_Snapshot(texture.LegacyTexture);

	// Populate the backend-neutral handle after the legacy texture loader
	// finished creating the compatibility texture. The backend either stores a
	// wrapper around the legacy pointer or creates a parallel bgfx texture via
	// the peek path. Skip when native_texture is null; that's a release, not a
	// load.
	if (texture.LegacyTexture != nullptr && g_renderBackend != nullptr) {
		if (texture.m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(texture.m_backendHandle);
		}
		texture.m_backendHandle = g_renderBackend->Register_Loaded_Texture(&texture);
	} else if (texture.LegacyTexture == nullptr && texture.m_backendHandle != kInvalidRenderResource && g_renderBackend != nullptr) {
		g_renderBackend->Destroy_Resource(texture.m_backendHandle);
		texture.m_backendHandle = kInvalidRenderResource;
	}
}

void DX8TextureInterop::Share_Legacy_Texture_With(TextureBaseClass &texture, const TextureBaseClass *source)
{
	Set_Legacy_Base_Texture(texture, source != nullptr ? Peek_Legacy_Base_Texture(*source) : nullptr);
}

void DX8TextureInterop::Poke_Legacy_Texture(TextureBaseClass &texture, IDirect3DBaseTexture8 *native_texture)
{
	texture.LegacyTexture = native_texture;
}

void DX8TextureInterop::Apply_Legacy_Surface(
	TextureBaseClass &texture,
	IDirect3DBaseTexture8 *native_texture,
	bool initialized,
	bool disable_auto_invalidation)
{
	texture.Apply_Legacy_Surface(native_texture, initialized, disable_auto_invalidation);
}

IDirect3DSurface8 *DX8TextureInterop::Peek_Legacy_Surface(const SurfaceClass &surface)
{
	return static_cast<IDirect3DSurface8 *>(surface.D3DSurface);
}

SurfaceClass *DX8TextureInterop::Create_Legacy_Surface_Wrapper(IDirect3DSurface8 *surface)
{
	return new SurfaceClass(surface);
}

IDirect3DSurface8 *DX8TextureInterop::Get_Legacy_Surface_Level(TextureClass &texture, unsigned int level)
{
	return static_cast<IDirect3DSurface8 *>(texture.Get_Legacy_Surface_Level(level));
}

IDirect3DSurface8 *DX8TextureInterop::Get_Legacy_Surface_Level(ZTextureClass &texture, unsigned int level)
{
	return static_cast<IDirect3DSurface8 *>(texture.Get_Legacy_Surface_Level(level));
}

IDirect3DSurface8 *DX8TextureInterop::Create_Legacy_Surface(
	unsigned int width,
	unsigned int height,
	WW3DFormat format)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	IDirect3DSurface8 *surface = nullptr;
	WWASSERT(format != WW3D_FORMAT_P8);
	DX8_ErrorCode(Legacy_Device()->CreateImageSurface(width, height, WW3DFormat_To_D3DFormat(format), &surface));
	return surface;
#else
	return DX8Wrapper::_Create_DX8_Surface(width, height, format);
#endif
}

IDirect3DSurface8 *DX8TextureInterop::Create_Legacy_Surface_From_File(const char *filename)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	StringClass filename_string(filename, true);
	return Load_Legacy_Surface_Immediate(filename_string, WW3D_FORMAT_UNKNOWN, true);
#else
	return DX8Wrapper::_Create_DX8_Surface(filename);
#endif
}

IDirect3DTexture8 *DX8TextureInterop::Create_Legacy_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	WWASSERT(format != WW3D_FORMAT_P8);

	IDirect3DTexture8 *texture = nullptr;
	const DWORD usage = render_target ? D3DUSAGE_RENDERTARGET : 0;
	const D3DFORMAT native_format = WW3DFormat_To_D3DFormat(format);
	const D3DPOOL native_pool = Legacy_Pool_To_D3D(pool);
	HRESULT result = D3DXCreateTexture(
		Legacy_Device(),
		width,
		height,
		mip_level_count,
		usage,
		native_format,
		native_pool,
		&texture);

	if (render_target && result == D3DERR_NOTAVAILABLE) {
		return nullptr;
	}
	if (Should_Retry_Texture_Create(result)) {
		Release_Unused_Assets_For_Texture_Retry();
		result = D3DXCreateTexture(
			Legacy_Device(),
			width,
			height,
			mip_level_count,
			usage,
			native_format,
			native_pool,
			&texture);
	}

	DX8_ErrorCode(result);
	return texture;
#else
	return DX8Wrapper::_Create_DX8_Texture(width, height, format, mip_level_count, static_cast<D3DPOOL>(pool), render_target);
#endif
}

IDirect3DTexture8 *DX8TextureInterop::Create_Legacy_Texture_From_Surface(
	IDirect3DSurface8 *surface,
	MipCountType mip_level_count)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	DX8_Assert();

	D3DSURFACE_DESC surface_desc;
	::ZeroMemory(&surface_desc, sizeof(surface_desc));
	DX8_ErrorCode(surface->GetDesc(&surface_desc));

	IDirect3DTexture8 *texture = Create_Legacy_Texture(
		surface_desc.Width,
		surface_desc.Height,
		D3DFormat_To_WW3DFormat(surface_desc.Format),
		mip_level_count,
		LEGACY_TEXTURE_POOL_MANAGED);

	IDirect3DSurface8 *tex_surface = nullptr;
	DX8_ErrorCode(texture->GetSurfaceLevel(0, &tex_surface));
	DX8_ErrorCode(D3DXLoadSurfaceFromSurface(tex_surface, nullptr, nullptr, surface, nullptr, nullptr, D3DX_FILTER_BOX, 0));
	tex_surface->Release();

	if (mip_level_count != MIP_LEVELS_1) {
		DX8_ErrorCode(D3DXFilterTexture(texture, nullptr, 0, D3DX_FILTER_BOX));
	}

	return texture;
#else
	return DX8Wrapper::_Create_DX8_Texture(surface, mip_level_count);
#endif
}

IDirect3DTexture8 *DX8TextureInterop::Create_Legacy_ZTexture(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	int pool)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	IDirect3DTexture8 *texture = nullptr;
	const D3DFORMAT native_format = WW3DZFormat_To_D3DFormat(zformat);
	const D3DPOOL native_pool = Legacy_Pool_To_D3D(pool);
	HRESULT result = Legacy_Device()->CreateTexture(
		width,
		height,
		mip_level_count,
		D3DUSAGE_DEPTHSTENCIL,
		native_format,
		native_pool,
		&texture);

	if (result == D3DERR_NOTAVAILABLE) {
		return nullptr;
	}
	if (Should_Retry_Texture_Create(result)) {
		Release_Unused_Assets_For_Texture_Retry();
		result = Legacy_Device()->CreateTexture(
			width,
			height,
			mip_level_count,
			D3DUSAGE_DEPTHSTENCIL,
			native_format,
			native_pool,
			&texture);
	}

	DX8_ErrorCode(result);
	if (texture != nullptr) {
		texture->AddRef();
	}
	return texture;
#else
	return DX8Wrapper::_Create_DX8_ZTexture(width, height, zformat, mip_level_count, static_cast<D3DPOOL>(pool));
#endif
}

IDirect3DCubeTexture8 *DX8TextureInterop::Create_Legacy_Cube_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	WWASSERT(width == height);
	WWASSERT(format != WW3D_FORMAT_P8);

	IDirect3DCubeTexture8 *texture = nullptr;
	const DWORD usage = render_target ? D3DUSAGE_RENDERTARGET : 0;
	const D3DFORMAT native_format = WW3DFormat_To_D3DFormat(format);
	const D3DPOOL native_pool = Legacy_Pool_To_D3D(pool);
	HRESULT result = D3DXCreateCubeTexture(
		Legacy_Device(),
		width,
		mip_level_count,
		usage,
		native_format,
		native_pool,
		&texture);

	if (render_target && result == D3DERR_NOTAVAILABLE) {
		return nullptr;
	}
	if (Should_Retry_Texture_Create(result)) {
		Release_Unused_Assets_For_Texture_Retry();
		result = D3DXCreateCubeTexture(
			Legacy_Device(),
			width,
			mip_level_count,
			usage,
			native_format,
			native_pool,
			&texture);
	}

	DX8_ErrorCode(result);
	return texture;
#else
	return DX8Wrapper::_Create_DX8_Cube_Texture(width, height, format, mip_level_count, static_cast<D3DPOOL>(pool), render_target);
#endif
}

IDirect3DVolumeTexture8 *DX8TextureInterop::Create_Legacy_Volume_Texture(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool)
{
#if defined(GGC_BGFX_STANDALONE)
	DX8_THREAD_ASSERT();
	WWASSERT(format != WW3D_FORMAT_P8);

	IDirect3DVolumeTexture8 *texture = nullptr;
	const D3DFORMAT native_format = WW3DFormat_To_D3DFormat(format);
	const D3DPOOL native_pool = Legacy_Pool_To_D3D(pool);
	HRESULT result = D3DXCreateVolumeTexture(
		Legacy_Device(),
		width,
		height,
		depth,
		mip_level_count,
		0,
		native_format,
		native_pool,
		&texture);

	if (Should_Retry_Texture_Create(result)) {
		Release_Unused_Assets_For_Texture_Retry();
		result = D3DXCreateVolumeTexture(
			Legacy_Device(),
			width,
			height,
			depth,
			mip_level_count,
			0,
			native_format,
			native_pool,
			&texture);
	}

	DX8_ErrorCode(result);
	return texture;
#else
	return DX8Wrapper::_Create_DX8_Volume_Texture(width, height, depth, format, mip_level_count, static_cast<D3DPOOL>(pool));
#endif
}

IDirect3DTexture8 *Get_Legacy_Missing_Texture()
{
	WWASSERT(s_missingTexture);
	s_missingTexture->AddRef();
	return s_missingTexture;
}

IDirect3DSurface8 *Create_Legacy_Missing_Surface()
{
	IDirect3DSurface8 *texture_surface = nullptr;
	DX8_ErrorCode(s_missingTexture->GetSurfaceLevel(0, &texture_surface));
	D3DSURFACE_DESC texture_surface_desc;
	::ZeroMemory(&texture_surface_desc, sizeof(texture_surface_desc));
	DX8_ErrorCode(texture_surface->GetDesc(&texture_surface_desc));

	IDirect3DSurface8 *surface = nullptr;
	DX8_ErrorCode(Legacy_Device()->CreateImageSurface(
		texture_surface_desc.Width,
		texture_surface_desc.Height,
		texture_surface_desc.Format,
		&surface));

	D3DLOCKED_RECT locked_rect;
	::ZeroMemory(&locked_rect, sizeof(locked_rect));
	DX8_ErrorCode(surface->LockRect(&locked_rect, nullptr, 0));

	for (unsigned int y = 0; y < texture_surface_desc.Height; ++y)
	{
		unsigned int *buffer = reinterpret_cast<unsigned int *>(
			static_cast<unsigned char *>(locked_rect.pBits) + locked_rect.Pitch * y);
		for (unsigned int x = 0; x < texture_surface_desc.Width; ++x)
		{
			*buffer++ = 0x7FFF00FF;
		}
	}

	DX8_ErrorCode(surface->UnlockRect());
	texture_surface->Release();
	return surface;
}

void Init_Legacy_Missing_Texture(
	unsigned int width,
	unsigned int height,
	const unsigned int *pixels)
{
	WWASSERT(!s_missingTexture);

	IDirect3DTexture8 *texture = Create_Legacy_Texture(
		width,
		height,
		WW3D_FORMAT_A8R8G8B8,
		MIP_LEVELS_ALL,
		LEGACY_TEXTURE_POOL_MANAGED);

	D3DLOCKED_RECT locked_rect;
	RECT rect;
	rect.left=0;
	rect.right=width;
	rect.top=0;
	rect.bottom=height;
	DX8_ErrorCode(texture->LockRect(0, &locked_rect, &rect, 0));

	unsigned *buffer=static_cast<unsigned *>(locked_rect.pBits);
	for (unsigned y=0;y<height;y++)
	{
		for (unsigned x=0; x<width; x++)
		{
			//*buffer++=missing_image_palette[*pixels++];
			*buffer++=0x7FFF00FF;
			++pixels;
		}
		buffer=static_cast<unsigned *>(locked_rect.pBits);
		buffer+=locked_rect.Pitch/sizeof(unsigned)*y;
	}

	DX8_ErrorCode(texture->UnlockRect(0));

	for (unsigned i=1;i<texture->GetLevelCount();++i) {
		IDirect3DSurface8 *src,*dst;
		DX8_ErrorCode(texture->GetSurfaceLevel(i-1,&src));
		DX8_ErrorCode(texture->GetSurfaceLevel(i,&dst));

		DX8_ErrorCode(D3DXLoadSurfaceFromSurface(
			dst,
			nullptr,
			nullptr,
			src,
			nullptr,
			nullptr,
			kLegacyMipFilterBox,
			0));

		src->Release();
		dst->Release();
	}

	s_missingTexture=texture;
}

void Release_Legacy_Missing_Texture()
{
	if (s_missingTexture != nullptr) {
		s_missingTexture->Release();
		s_missingTexture=nullptr;
	}
}
