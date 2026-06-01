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

#include "always.h"
#include "dx8list.h"
#include "multilist.h"
#include "texture.h"

class TextureTrackerClass : public MultiListObjectClass
{
public:
	TextureTrackerClass
	(
		unsigned int w,
		unsigned int h,
		MipCountType count,
		TextureBaseClass *tex
	)
	: Width(w),
	  Height(h),
	  Mip_level_count(count),
	  Texture(tex)
	{
	}

	virtual ~TextureTrackerClass() = default;
	virtual void Release() const = 0;
	virtual void Recreate() const = 0;

	TextureBaseClass *Get_Texture() const { return Texture; }

protected:
	unsigned int Width;
	unsigned int Height;
	MipCountType Mip_level_count;
	TextureBaseClass *Texture;
};

class TextureResourceManagerClass
{
public:
	static void Shutdown();
	static void Add(TextureTrackerClass *track);
	static void Remove(TextureBaseClass *tex);
	static void Release_Textures();
	static void Recreate_Textures();

private:
	static TextureTrackerList Managed_Textures;
};
