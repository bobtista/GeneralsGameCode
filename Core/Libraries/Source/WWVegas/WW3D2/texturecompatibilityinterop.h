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

#pragma once

#include "texturecompat.h"
#include "dx8texturelegacytypes.h"
#include "texturefilter.h"
#include "ww3dformat.h"

class StringClass;
class SurfaceClass;
class TextureBaseClass;
class TextureClass;
class ZTextureClass;

class TextureCompatibilityInterop
{
public:
	static LegacyBaseTexture *Peek_Legacy_Base_Texture(const TextureBaseClass &texture);
	static LegacyLoaderTexture *Peek_Legacy_Texture2D(const TextureBaseClass &texture);
	static LegacyLoaderCubeTexture *Peek_Legacy_Cube_Texture(const TextureBaseClass &texture);
	static LegacyLoaderVolumeTexture *Peek_Legacy_Volume_Texture(const TextureBaseClass &texture);
	static void Set_Legacy_Base_Texture(TextureBaseClass &texture, LegacyBaseTexture *native_texture);
	static void Share_Legacy_Texture_With(TextureBaseClass &texture, const TextureBaseClass *source);
	static void Poke_Legacy_Texture(TextureBaseClass &texture, LegacyBaseTexture *native_texture);
	static void Apply_Native_Compatibility_Texture(
		TextureBaseClass &texture,
		LegacyBaseTexture *native_texture,
		bool initialized,
		bool disable_auto_invalidation = false);

	static LegacySurface *Peek_Legacy_Surface(const SurfaceClass &surface, bool intentToWrite = false);
	static SurfaceClass *Create_Legacy_Surface_Wrapper(LegacySurface *surface);
	static LegacySurface *Get_Native_Compatibility_Surface_Level(TextureClass &texture, unsigned int level = 0);
	static LegacySurface *Get_Native_Compatibility_Surface_Level(ZTextureClass &texture, unsigned int level = 0);
	static LegacySurface *Create_Legacy_Surface(
		unsigned int width,
		unsigned int height,
		WW3DFormat format);
	static LegacySurface *Create_Legacy_Surface_From_File(const char *filename);

	static LegacyLoaderTexture *Create_Legacy_Texture(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		int pool,
		bool render_target = false);
	static LegacyLoaderTexture *Create_Legacy_Texture_From_Surface(
		LegacySurface *surface,
		MipCountType mip_level_count);
	static LegacyLoaderTexture *Create_Legacy_ZTexture(
		unsigned int width,
		unsigned int height,
		WW3DZFormat zformat,
		MipCountType mip_level_count,
		int pool);
	static LegacyLoaderCubeTexture *Create_Legacy_Cube_Texture(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		int pool,
		bool render_target = false);
	static LegacyLoaderVolumeTexture *Create_Legacy_Volume_Texture(
		unsigned int width,
		unsigned int height,
		unsigned int depth,
		WW3DFormat format,
		MipCountType mip_level_count,
		int pool);
	static WW3DFormat Legacy_Texture_Format_To_WW3DFormat(unsigned int format);
	static bool Generate_Legacy_Texture_Mips(TextureClass &texture);
};

inline LegacyBaseTexture *Peek_Legacy_Base_Texture(const TextureBaseClass &texture)
{
	return TextureCompatibilityInterop::Peek_Legacy_Base_Texture(texture);
}

inline LegacyLoaderTexture *Peek_Legacy_Texture2D(const TextureBaseClass &texture)
{
	return TextureCompatibilityInterop::Peek_Legacy_Texture2D(texture);
}

inline LegacyLoaderCubeTexture *Peek_Legacy_Cube_Texture(const TextureBaseClass &texture)
{
	return TextureCompatibilityInterop::Peek_Legacy_Cube_Texture(texture);
}

inline LegacyLoaderVolumeTexture *Peek_Legacy_Volume_Texture(const TextureBaseClass &texture)
{
	return TextureCompatibilityInterop::Peek_Legacy_Volume_Texture(texture);
}

inline void Set_Legacy_Base_Texture(TextureBaseClass &texture, LegacyBaseTexture *native_texture)
{
	TextureCompatibilityInterop::Set_Legacy_Base_Texture(texture, native_texture);
}

inline void Poke_Legacy_Texture(TextureBaseClass &texture, LegacyBaseTexture *native_texture)
{
	TextureCompatibilityInterop::Poke_Legacy_Texture(texture, native_texture);
}

inline void Apply_Native_Compatibility_Texture(
	TextureBaseClass &texture,
	LegacyBaseTexture *native_texture,
	bool initialized,
	bool disable_auto_invalidation = false)
{
	TextureCompatibilityInterop::Apply_Native_Compatibility_Texture(texture, native_texture, initialized, disable_auto_invalidation);
}

inline LegacySurface *Peek_Legacy_Surface(const SurfaceClass &surface, bool intentToWrite = false)
{
	return TextureCompatibilityInterop::Peek_Legacy_Surface(surface, intentToWrite);
}

inline SurfaceClass *Create_Legacy_Surface_Wrapper(LegacySurface *surface)
{
	return TextureCompatibilityInterop::Create_Legacy_Surface_Wrapper(surface);
}

inline LegacySurface *Get_Native_Compatibility_Surface_Level(TextureClass &texture, unsigned int level = 0)
{
	return TextureCompatibilityInterop::Get_Native_Compatibility_Surface_Level(texture, level);
}

inline LegacySurface *Get_Native_Compatibility_Surface_Level(ZTextureClass &texture, unsigned int level = 0)
{
	return TextureCompatibilityInterop::Get_Native_Compatibility_Surface_Level(texture, level);
}

inline LegacySurface *Create_Legacy_Surface(
	unsigned int width,
	unsigned int height,
	WW3DFormat format)
{
	return TextureCompatibilityInterop::Create_Legacy_Surface(width, height, format);
}

inline LegacySurface *Create_Legacy_Surface_From_File(const char *filename)
{
	return TextureCompatibilityInterop::Create_Legacy_Surface_From_File(filename);
}

inline LegacyLoaderTexture *Create_Legacy_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target = false)
{
	return TextureCompatibilityInterop::Create_Legacy_Texture(width, height, format, mip_level_count, pool, render_target);
}

inline LegacyLoaderTexture *Create_Legacy_Texture_From_Surface(
	LegacySurface *surface,
	MipCountType mip_level_count)
{
	return TextureCompatibilityInterop::Create_Legacy_Texture_From_Surface(surface, mip_level_count);
}

inline LegacyLoaderTexture *Create_Legacy_ZTexture(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	int pool)
{
	return TextureCompatibilityInterop::Create_Legacy_ZTexture(width, height, zformat, mip_level_count, pool);
}

inline LegacyLoaderCubeTexture *Create_Legacy_Cube_Texture(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool,
	bool render_target = false)
{
	return TextureCompatibilityInterop::Create_Legacy_Cube_Texture(width, height, format, mip_level_count, pool, render_target);
}

inline LegacyLoaderVolumeTexture *Create_Legacy_Volume_Texture(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	int pool)
{
	return TextureCompatibilityInterop::Create_Legacy_Volume_Texture(width, height, depth, format, mip_level_count, pool);
}

inline WW3DFormat Legacy_Texture_Format_To_WW3DFormat(unsigned int format)
{
	return TextureCompatibilityInterop::Legacy_Texture_Format_To_WW3DFormat(format);
}

inline bool Generate_Legacy_Texture_Mips(TextureClass &texture)
{
	return TextureCompatibilityInterop::Generate_Legacy_Texture_Mips(texture);
}

LegacyLoaderTexture *Get_Legacy_Missing_Texture();
LegacySurface *Create_Legacy_Missing_Surface();
void Copy_Legacy_Surface(
	LegacySurface *destination,
	const LegacySurfaceCopyRect &destination_rect,
	LegacySurface *source,
	const LegacySurfaceCopyRect &source_rect,
	unsigned int filter);
LegacySurface *Load_Legacy_Surface_Immediate(
	const StringClass &filename,
	WW3DFormat surface_format,
	bool allow_compression);
