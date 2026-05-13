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

#include "IRenderBackend.h"
#include "RenderBackend.h"
#include "surfaceclass.h"
#include "texture.h"
#include "ww3d.h"

IDirect3DBaseTexture8 *DX8TextureInterop::Peek_Legacy_Base_Texture(const TextureBaseClass &texture)
{
	texture.LastAccessed=WW3D::Get_Sync_Time();
	return texture.D3DTexture;
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

	if (texture.D3DTexture != nullptr) {
		texture.D3DTexture->Release();
	}
	texture.D3DTexture = native_texture;
	if (texture.D3DTexture != nullptr) {
		texture.D3DTexture->AddRef();
	}
	texture.Capture_CPU_Texture_Snapshot(texture.D3DTexture);

	// Populate the backend-neutral handle after the legacy texture loader
	// finished creating the compatibility texture. The backend either stores a
	// wrapper around the legacy pointer or creates a parallel bgfx texture via
	// the peek path. Skip when native_texture is null; that's a release, not a
	// load.
	if (texture.D3DTexture != nullptr && g_renderBackend != nullptr) {
		if (texture.m_backendHandle != kInvalidRenderResource) {
			g_renderBackend->Destroy_Resource(texture.m_backendHandle);
		}
		texture.m_backendHandle = g_renderBackend->Register_Loaded_Texture(&texture);
	} else if (texture.D3DTexture == nullptr && texture.m_backendHandle != kInvalidRenderResource && g_renderBackend != nullptr) {
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
	texture.D3DTexture = native_texture;
}

IDirect3DSurface8 *DX8TextureInterop::Peek_Legacy_Surface(const SurfaceClass &surface)
{
	return surface.D3DSurface;
}

SurfaceClass *DX8TextureInterop::Create_Legacy_Surface_Wrapper(IDirect3DSurface8 *surface)
{
	return new SurfaceClass(surface);
}
