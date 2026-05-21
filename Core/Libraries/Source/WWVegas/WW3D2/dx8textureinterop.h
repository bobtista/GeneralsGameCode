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

#pragma once

struct IDirect3DBaseTexture8;
struct IDirect3DTexture8;
struct IDirect3DCubeTexture8;
struct IDirect3DVolumeTexture8;
struct IDirect3DSurface8;
class SurfaceClass;
class TextureBaseClass;

class DX8TextureInterop
{
public:
	static IDirect3DBaseTexture8 *Peek_Legacy_Base_Texture(const TextureBaseClass &texture);
	static IDirect3DTexture8 *Peek_Legacy_Texture2D(const TextureBaseClass &texture);
	static IDirect3DCubeTexture8 *Peek_Legacy_Cube_Texture(const TextureBaseClass &texture);
	static IDirect3DVolumeTexture8 *Peek_Legacy_Volume_Texture(const TextureBaseClass &texture);
	static void Set_Legacy_Base_Texture(TextureBaseClass &texture, IDirect3DBaseTexture8 *native_texture);
	static void Share_Legacy_Texture_With(TextureBaseClass &texture, const TextureBaseClass *source);
	static void Poke_Legacy_Texture(TextureBaseClass &texture, IDirect3DBaseTexture8 *native_texture);

	static IDirect3DSurface8 *Peek_Legacy_Surface(const SurfaceClass &surface);
};

inline IDirect3DBaseTexture8 *Peek_Legacy_Base_Texture(const TextureBaseClass &texture)
{
	return DX8TextureInterop::Peek_Legacy_Base_Texture(texture);
}

inline IDirect3DTexture8 *Peek_Legacy_Texture2D(const TextureBaseClass &texture)
{
	return DX8TextureInterop::Peek_Legacy_Texture2D(texture);
}

inline IDirect3DCubeTexture8 *Peek_Legacy_Cube_Texture(const TextureBaseClass &texture)
{
	return DX8TextureInterop::Peek_Legacy_Cube_Texture(texture);
}

inline IDirect3DVolumeTexture8 *Peek_Legacy_Volume_Texture(const TextureBaseClass &texture)
{
	return DX8TextureInterop::Peek_Legacy_Volume_Texture(texture);
}

inline void Set_Legacy_Base_Texture(TextureBaseClass &texture, IDirect3DBaseTexture8 *native_texture)
{
	DX8TextureInterop::Set_Legacy_Base_Texture(texture, native_texture);
}

inline void Poke_Legacy_Texture(TextureBaseClass &texture, IDirect3DBaseTexture8 *native_texture)
{
	DX8TextureInterop::Poke_Legacy_Texture(texture, native_texture);
}

inline IDirect3DSurface8 *Peek_Legacy_Surface(const SurfaceClass &surface)
{
	return DX8TextureInterop::Peek_Legacy_Surface(surface);
}
