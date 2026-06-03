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

#include "texturecompatibilityinterop.h"

#if defined(GGC_RENDER_BACKEND_BGFX)
#include "WWLib/win.h"
#else
#include <d3d8.h>
#include <d3dx8tex.h>
#endif

#if !defined(GGC_RENDER_BACKEND_BGFX)
#include "dx8formatconv.h"
#include "dx8wrapper.h"
#endif
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
#if defined(GGC_RENDER_BACKEND_BGFX)
#else
	IDirect3DDevice8 *Legacy_Device()
	{
		DX8_Assert();
		return DX8_Call_Device();
	}
#endif

	LegacyLoaderTexture *s_missingTexture = nullptr;
	constexpr unsigned kLegacyMipFilterBox = 5;

	HRESULT Filter_Legacy_Texture_Mips_Compat(LegacyBaseTexture *base_texture, unsigned int src_level)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		(void)base_texture;
		(void)src_level;
		WWASSERT_PRINT(
			false,
			"Filter_Legacy_Texture_Mips_Compat: standalone bgfx cannot filter fake-D3D texture mips");
		return E_FAIL;
#else
		return D3DXFilterTexture(base_texture, nullptr, src_level, kLegacyMipFilterBox);
#endif
	}

	HRESULT Copy_Legacy_Surface_Compat(
		LegacySurface *destination,
		const RECT *destination_rect,
		LegacySurface *source,
		const RECT *source_rect,
		unsigned int filter)
	{
#if defined(GGC_RENDER_BACKEND_BGFX)
		(void)destination;
		(void)destination_rect;
		(void)source;
		(void)source_rect;
		(void)filter;
		WWASSERT_PRINT(
			false,
			"Copy_Legacy_Surface_Compat: standalone bgfx cannot copy fake-D3D surfaces");
		return E_FAIL;
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
}

LegacyBaseTexture *TextureCompatibilityInterop::Peek_Legacy_Base_Texture(const TextureBaseClass &texture)
{
	texture.LastAccessed=WW3D::Get_Sync_Time();
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		texture.Get_Native_Compatibility_Texture() == nullptr,
		"Peek_Legacy_Base_Texture: standalone bgfx cannot expose fake-D3D textures");
	return nullptr;
#else
	return static_cast<LegacyBaseTexture *>(texture.Get_Native_Compatibility_Texture());
#endif
}

LegacyLoaderTexture *TextureCompatibilityInterop::Peek_Legacy_Texture2D(const TextureBaseClass &texture)
{
	return reinterpret_cast<LegacyLoaderTexture *>(Peek_Legacy_Base_Texture(texture));
}

LegacyLoaderCubeTexture *TextureCompatibilityInterop::Peek_Legacy_Cube_Texture(const TextureBaseClass &texture)
{
	return reinterpret_cast<LegacyLoaderCubeTexture *>(Peek_Legacy_Base_Texture(texture));
}

LegacyLoaderVolumeTexture *TextureCompatibilityInterop::Peek_Legacy_Volume_Texture(const TextureBaseClass &texture)
{
	return reinterpret_cast<LegacyLoaderVolumeTexture *>(Peek_Legacy_Base_Texture(texture));
}

void TextureCompatibilityInterop::Set_Legacy_Base_Texture(TextureBaseClass &texture, LegacyBaseTexture *native_texture)
{
	// (gth) Generals does stuff directly with the native texture pointer so lets
	// reset the access timer whenever someone messes with this pointer.
	texture.LastAccessed=WW3D::Get_Sync_Time();

	LegacyBaseTexture *old_texture = static_cast<LegacyBaseTexture *>(texture.Get_Native_Compatibility_Texture());
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		old_texture == nullptr && native_texture == nullptr,
		"Set_Legacy_Base_Texture: standalone bgfx cannot own fake-D3D textures");
#else
	if (old_texture != nullptr) {
		old_texture->Release();
	}
#endif
#if defined(GGC_RENDER_BACKEND_BGFX)
	texture.Set_Native_Compatibility_Texture(nullptr);
#else
	texture.Set_Native_Compatibility_Texture(native_texture);
#endif
#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (native_texture != nullptr) {
		native_texture->AddRef();
	}
#endif
	bool preserve_cpu_snapshot = false;
#if defined(GGC_RENDER_BACKEND_BGFX)
	preserve_cpu_snapshot =
		texture.Has_CPU_Texture_Mips()
		&& texture.PreserveCPUTextureSnapshotOnNextLegacySet;
#endif
	if (!preserve_cpu_snapshot) {
		texture.Capture_CPU_Texture_Snapshot(texture.Get_Native_Compatibility_Texture());
	}
	texture.PreserveCPUTextureSnapshotOnNextLegacySet = false;

	// Populate the backend-neutral handle after the legacy texture loader
	// finished creating the compatibility texture. The backend either stores a
	// wrapper around the legacy pointer or creates a parallel bgfx texture via
	// the peek path. Skip when native_texture is null; that's a release, not a
	// load.
	if (texture.Get_Native_Compatibility_Texture() != nullptr && g_renderBackend != nullptr) {
		if (texture.m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(texture.m_backendHandle);
		}
		texture.m_backendHandle = g_renderBackend->Register_Texture_Resource(&texture);
	} else if (texture.Get_Native_Compatibility_Texture() == nullptr && g_renderBackend != nullptr) {
		if (texture.m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(texture.m_backendHandle);
			texture.m_backendHandle = kInvalidRenderResource;
		}
		g_renderBackend->Release_Cached_Texture(&texture);
	}
}

void TextureCompatibilityInterop::Share_Legacy_Texture_With(TextureBaseClass &texture, const TextureBaseClass *source)
{
	// TheSuperHackers @bugfix bobtista 28/05/2026 Only bump CPUTextureRevision when Set_Legacy_Base_Texture below will actually consume the preserved snapshot (i.e. the source has a non-null legacy texture to share). Bumping unconditionally invalidates downstream caches even when no real share happens.
	LegacyBaseTexture *shared_legacy = source != nullptr ? Peek_Legacy_Base_Texture(*source) : nullptr;
#if defined(GGC_RENDER_BACKEND_BGFX)
	if (source != nullptr
		&& shared_legacy != nullptr
		&& source->Has_CPU_Texture_Mips()) {
		texture.CPUTextureMips = source->CPUTextureMips;
		texture.PreserveCPUTextureSnapshotOnNextLegacySet = true;
		++texture.CPUTextureRevision;
	}
#endif
	Set_Legacy_Base_Texture(texture, shared_legacy);
}

void Share_Legacy_Texture_With(TextureBaseClass &texture, const TextureBaseClass *source)
{
	TextureCompatibilityInterop::Share_Legacy_Texture_With(texture, source);
}

void TextureCompatibilityInterop::Poke_Legacy_Texture(TextureBaseClass &texture, LegacyBaseTexture *native_texture)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		native_texture == nullptr,
		"Poke_Legacy_Texture: standalone bgfx cannot store fake-D3D textures");
	texture.Set_Native_Compatibility_Texture(nullptr);
#else
	texture.Set_Native_Compatibility_Texture(native_texture);
#endif
}

void TextureCompatibilityInterop::Apply_Native_Compatibility_Texture(
	TextureBaseClass &texture,
	LegacyBaseTexture *native_texture,
	bool initialized,
	bool disable_auto_invalidation)
{
	texture.Apply_Native_Compatibility_Texture(native_texture, initialized, disable_auto_invalidation);
}

LegacySurface *TextureCompatibilityInterop::Peek_Legacy_Surface(const SurfaceClass &surface, bool intentToWrite)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		surface.Get_Native_Compatibility_Surface() == nullptr,
		"Peek_Legacy_Surface: standalone bgfx cannot expose fake-D3D surfaces");
	return nullptr;
#else
	// TheSuperHackers @bugfix bobtista 28/05/2026 Only mark the CPU snapshot stale when the caller intends to write; read-only peeks (capture/back-buffer copy, ObjectPreview, etc.) leave the snapshot valid.
	if (intentToWrite)
	{
		const_cast<SurfaceClass &>(surface).Mark_CPU_Surface_Snapshot_Stale();
	}
	return static_cast<LegacySurface *>(surface.Get_Native_Compatibility_Surface());
#endif
}

SurfaceClass *TextureCompatibilityInterop::Create_Legacy_Surface_Wrapper(LegacySurface *surface)
{
	return new SurfaceClass(surface);
}

LegacySurface *TextureCompatibilityInterop::Get_Native_Compatibility_Surface_Level(TextureClass &texture, unsigned int level)
{
	return static_cast<LegacySurface *>(texture.Get_Native_Compatibility_Surface_Level(level));
}

LegacySurface *TextureCompatibilityInterop::Get_Native_Compatibility_Surface_Level(ZTextureClass &texture, unsigned int level)
{
	return static_cast<LegacySurface *>(texture.Get_Native_Compatibility_Surface_Level(level));
}

LegacySurface *TextureCompatibilityInterop::Create_Legacy_Surface(
	unsigned int width,
	unsigned int height,
	WW3DFormat format)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Surface: standalone bgfx cannot create fake-D3D surfaces");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_Surface(width, height, format);
#endif
}

LegacySurface *TextureCompatibilityInterop::Create_Legacy_Surface_From_File(const char *filename)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Surface_From_File: standalone bgfx cannot create fake-D3D surfaces");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_Surface(filename);
#endif
}

LegacyLoaderTexture *TextureCompatibilityInterop::Create_Legacy_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Texture: standalone bgfx cannot create fake-D3D textures");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_Texture(width, height, format, mip_level_count, static_cast<D3DPOOL>(pool), render_target);
#endif
}

LegacyLoaderTexture *TextureCompatibilityInterop::Create_Legacy_Texture_From_Surface(
	LegacySurface *surface,
	MipCountType mip_level_count)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Texture_From_Surface: standalone bgfx cannot create fake-D3D textures");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_Texture(surface, mip_level_count);
#endif
}

LegacyLoaderTexture *TextureCompatibilityInterop::Create_Legacy_ZTexture(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	int pool)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_ZTexture: standalone bgfx cannot create fake-D3D depth textures");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_ZTexture(width, height, zformat, mip_level_count, static_cast<D3DPOOL>(pool));
#endif
}

LegacyLoaderCubeTexture *TextureCompatibilityInterop::Create_Legacy_Cube_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Cube_Texture: standalone bgfx cannot create fake-D3D cube textures");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_Cube_Texture(width, height, format, mip_level_count, static_cast<D3DPOOL>(pool), render_target);
#endif
}

LegacyLoaderVolumeTexture *TextureCompatibilityInterop::Create_Legacy_Volume_Texture(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Volume_Texture: standalone bgfx cannot create fake-D3D volume textures");
	return nullptr;
#else
	return DX8Wrapper::_Create_DX8_Volume_Texture(width, height, depth, format, mip_level_count, static_cast<D3DPOOL>(pool));
#endif
}

WW3DFormat TextureCompatibilityInterop::Legacy_Texture_Format_To_WW3DFormat(unsigned int format)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)format;
	WWASSERT_PRINT(
		false,
		"Legacy_Texture_Format_To_WW3DFormat: standalone bgfx cannot decode D3D texture formats");
	return WW3D_FORMAT_UNKNOWN;
#else
	return D3DFormat_To_WW3DFormat(static_cast<D3DFORMAT>(format));
#endif
}

bool TextureCompatibilityInterop::Generate_Legacy_Texture_Mips(TextureClass &texture)
{
	LegacyLoaderTexture *native_texture = Peek_Legacy_Texture2D(texture);
	if (native_texture == nullptr)
	{
		return false;
	}

	return SUCCEEDED(Filter_Legacy_Texture_Mips_Compat(reinterpret_cast<LegacyBaseTexture *>(native_texture), 0));
}

LegacyLoaderTexture *Get_Legacy_Missing_Texture()
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Get_Legacy_Missing_Texture: standalone bgfx cannot return fake-D3D missing textures");
	return nullptr;
#else
	WWASSERT(s_missingTexture);
	s_missingTexture->AddRef();
	return s_missingTexture;
#endif
}

LegacySurface *Create_Legacy_Missing_Surface()
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Create_Legacy_Missing_Surface: standalone bgfx cannot create fake-D3D missing surfaces");
	return nullptr;
#else
	LegacySurface *texture_surface = nullptr;
	DX8_ErrorCode(s_missingTexture->GetSurfaceLevel(0, &texture_surface));
	LegacySurfaceDesc texture_surface_desc;
	::ZeroMemory(&texture_surface_desc, sizeof(texture_surface_desc));
	DX8_ErrorCode(texture_surface->GetDesc(&texture_surface_desc));

	LegacySurface *surface = nullptr;
	DX8_ErrorCode(Legacy_Device()->CreateImageSurface(
		texture_surface_desc.Width,
		texture_surface_desc.Height,
		texture_surface_desc.Format,
		&surface));

	LegacyLockedRect locked_rect;
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
#endif
}

void Copy_Legacy_Surface(
	LegacySurface *destination,
	const LegacySurfaceCopyRect &destination_rect,
	LegacySurface *source,
	const LegacySurfaceCopyRect &source_rect,
	unsigned int filter)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
	(void)destination;
	(void)destination_rect;
	(void)source;
	(void)source_rect;
	(void)filter;
	WWASSERT_PRINT(
		false,
		"Copy_Legacy_Surface: standalone bgfx cannot copy fake-D3D surfaces");
#else
	RECT destination_native_rect;
	destination_native_rect.left = destination_rect.left;
	destination_native_rect.top = destination_rect.top;
	destination_native_rect.right = destination_rect.right;
	destination_native_rect.bottom = destination_rect.bottom;
	RECT source_native_rect;
	source_native_rect.left = source_rect.left;
	source_native_rect.top = source_rect.top;
	source_native_rect.right = source_rect.right;
	source_native_rect.bottom = source_rect.bottom;
	DX8_ErrorCode(Copy_Legacy_Surface_Compat(
		destination,
		&destination_native_rect,
		source,
		&source_native_rect,
		filter));
#endif
}

void Init_Legacy_Missing_Texture(
	unsigned int width,
	unsigned int height,
	const unsigned int *pixels)
{
	WWASSERT(!s_missingTexture);

#if defined(GGC_RENDER_BACKEND_BGFX)
	WWASSERT_PRINT(
		false,
		"Init_Legacy_Missing_Texture: standalone bgfx cannot create fake-D3D missing textures");
	return;
#else
	LegacyLoaderTexture *texture = Create_Legacy_Texture(
		width,
		height,
		WW3D_FORMAT_A8R8G8B8,
		MIP_LEVELS_ALL,
		LEGACY_TEXTURE_POOL_MANAGED);

	LegacyLockedRect locked_rect;
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
		buffer+=locked_rect.Pitch/sizeof(unsigned)*(y+1);
	}

	DX8_ErrorCode(texture->UnlockRect(0));

	for (unsigned i=1;i<texture->GetLevelCount();++i) {
		LegacySurface *src,*dst;
		DX8_ErrorCode(texture->GetSurfaceLevel(i-1,&src));
		DX8_ErrorCode(texture->GetSurfaceLevel(i,&dst));

		DX8_ErrorCode(Copy_Legacy_Surface_Compat(
			dst,
			nullptr,
			src,
			nullptr,
			kLegacyMipFilterBox));

		src->Release();
		dst->Release();
	}

	s_missingTexture=texture;
#endif
}

void Release_Legacy_Missing_Texture()
{
	if (s_missingTexture != nullptr) {
#if defined(GGC_RENDER_BACKEND_BGFX)
		WWASSERT_PRINT(
			false,
			"Release_Legacy_Missing_Texture: standalone bgfx cannot release fake-D3D missing textures");
#else
		s_missingTexture->Release();
#endif
		s_missingTexture=nullptr;
	}
}
