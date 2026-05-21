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

#include "texturecompat.h"
#include "texturefilter.h"
#include "ww3dformat.h"

struct IDirect3DBaseTexture8;
struct IDirect3DTexture8;
struct IDirect3DCubeTexture8;
struct IDirect3DVolumeTexture8;
struct IDirect3DSurface8;
class StringClass;
class SurfaceClass;
class TextureBaseClass;
class TextureClass;
class ZTextureClass;

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
	static void Apply_Legacy_Surface(
		TextureBaseClass &texture,
		IDirect3DBaseTexture8 *native_texture,
		bool initialized,
		bool disable_auto_invalidation = false);

		static IDirect3DSurface8 *Peek_Legacy_Surface(const SurfaceClass &surface);
		static SurfaceClass *Create_Legacy_Surface_Wrapper(IDirect3DSurface8 *surface);
		static IDirect3DSurface8 *Get_Legacy_Surface_Level(TextureClass &texture, unsigned int level = 0);
		static IDirect3DSurface8 *Get_Legacy_Surface_Level(ZTextureClass &texture, unsigned int level = 0);
		static IDirect3DSurface8 *Create_Legacy_Surface(
			unsigned int width,
			unsigned int height,
			WW3DFormat format);
		static IDirect3DSurface8 *Create_Legacy_Surface_From_File(const char *filename);

		static IDirect3DTexture8 *Create_Legacy_Texture(
			unsigned int width,
			unsigned int height,
			WW3DFormat format,
			MipCountType mip_level_count,
			int pool,
			bool render_target = false);
		static IDirect3DTexture8 *Create_Legacy_Texture_From_Surface(
			IDirect3DSurface8 *surface,
			MipCountType mip_level_count);
		static IDirect3DTexture8 *Create_Legacy_ZTexture(
			unsigned int width,
			unsigned int height,
			WW3DZFormat zformat,
			MipCountType mip_level_count,
			int pool);
		static IDirect3DCubeTexture8 *Create_Legacy_Cube_Texture(
			unsigned int width,
			unsigned int height,
			WW3DFormat format,
			MipCountType mip_level_count,
			int pool,
			bool render_target = false);
		static IDirect3DVolumeTexture8 *Create_Legacy_Volume_Texture(
			unsigned int width,
			unsigned int height,
			unsigned int depth,
			WW3DFormat format,
			MipCountType mip_level_count,
			int pool);
		static WW3DFormat Legacy_Texture_Format_To_WW3DFormat(unsigned int format);
		static bool Generate_Legacy_Texture_Mips(TextureClass &texture);
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

inline void Apply_Legacy_Surface(
	TextureBaseClass &texture,
	IDirect3DBaseTexture8 *native_texture,
	bool initialized,
	bool disable_auto_invalidation = false)
{
	DX8TextureInterop::Apply_Legacy_Surface(texture, native_texture, initialized, disable_auto_invalidation);
}

inline IDirect3DSurface8 *Peek_Legacy_Surface(const SurfaceClass &surface)
{
	return DX8TextureInterop::Peek_Legacy_Surface(surface);
}

inline SurfaceClass *Create_Legacy_Surface_Wrapper(IDirect3DSurface8 *surface)
{
	return DX8TextureInterop::Create_Legacy_Surface_Wrapper(surface);
}

inline IDirect3DSurface8 *Get_Legacy_Surface_Level(TextureClass &texture, unsigned int level = 0)
{
	return DX8TextureInterop::Get_Legacy_Surface_Level(texture, level);
}

inline IDirect3DSurface8 *Get_Legacy_Surface_Level(ZTextureClass &texture, unsigned int level = 0)
{
	return DX8TextureInterop::Get_Legacy_Surface_Level(texture, level);
}

inline IDirect3DSurface8 *Create_Legacy_Surface(
	unsigned int width,
	unsigned int height,
	WW3DFormat format)
{
	return DX8TextureInterop::Create_Legacy_Surface(width, height, format);
}

inline IDirect3DSurface8 *Create_Legacy_Surface_From_File(const char *filename)
{
	return DX8TextureInterop::Create_Legacy_Surface_From_File(filename);
}

inline IDirect3DTexture8 *Create_Legacy_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target = false)
{
	return DX8TextureInterop::Create_Legacy_Texture(width, height, format, mip_level_count, pool, render_target);
}

inline IDirect3DTexture8 *Create_Legacy_Texture_From_Surface(
	IDirect3DSurface8 *surface,
	MipCountType mip_level_count)
{
	return DX8TextureInterop::Create_Legacy_Texture_From_Surface(surface, mip_level_count);
}

inline IDirect3DTexture8 *Create_Legacy_ZTexture(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	int pool)
{
	return DX8TextureInterop::Create_Legacy_ZTexture(width, height, zformat, mip_level_count, pool);
}

inline IDirect3DCubeTexture8 *Create_Legacy_Cube_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target = false)
{
	return DX8TextureInterop::Create_Legacy_Cube_Texture(width, height, format, mip_level_count, pool, render_target);
}

inline IDirect3DVolumeTexture8 *Create_Legacy_Volume_Texture(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool)
{
	return DX8TextureInterop::Create_Legacy_Volume_Texture(width, height, depth, format, mip_level_count, pool);
}

inline WW3DFormat Legacy_Texture_Format_To_WW3DFormat(unsigned int format)
{
	return DX8TextureInterop::Legacy_Texture_Format_To_WW3DFormat(format);
}

inline bool Generate_Legacy_Texture_Mips(TextureClass &texture)
{
	return DX8TextureInterop::Generate_Legacy_Texture_Mips(texture);
}

IDirect3DTexture8 *Get_Legacy_Missing_Texture();
IDirect3DSurface8 *Create_Legacy_Missing_Surface();
void Copy_Legacy_Surface(
	IDirect3DSurface8 *destination,
	const LegacySurfaceCopyRect &destination_rect,
	IDirect3DSurface8 *source,
	const LegacySurfaceCopyRect &source_rect,
	unsigned int filter);
IDirect3DSurface8 *Load_Legacy_Surface_Immediate(
	const StringClass &filename,
	WW3DFormat surface_format,
	bool allow_compression);
