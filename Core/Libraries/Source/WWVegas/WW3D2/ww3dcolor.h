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

#pragma once

#include "vector3.h"
#include "vector4.h"

namespace WW3DColor
{
WWINLINE Vector4 From_ARGB(unsigned int color)
{
	Vector4 converted;
	converted[3] = ((color & 0xff000000) >> 24) / 255.0f;
	converted[0] = ((color & 0x00ff0000) >> 16) / 255.0f;
	converted[1] = ((color & 0x0000ff00) >> 8) / 255.0f;
	converted[2] = ((color & 0x000000ff) >> 0) / 255.0f;
	return converted;
}

WWINLINE unsigned int To_ARGB(const Vector3 & color, float alpha)
{
	return color.Convert_To_ARGB(alpha);
}

WWINLINE unsigned int To_ARGB(const Vector4 & color)
{
	return To_ARGB(reinterpret_cast<const Vector3 &>(color), color[3]);
}

WWINLINE void Clamp(Vector4 & color)
{
	for (int i = 0; i < 4; ++i)
	{
		const float nonnegative = (color[i] < 0.0f) ? 0.0f : color[i];
		color[i] = (nonnegative > 1.0f) ? 1.0f : nonnegative;
	}
}

WWINLINE unsigned int To_ARGB_Clamp(const Vector4 & color)
{
	Vector4 clamped = color;
	Clamp(clamped);
	return To_ARGB(clamped);
}

WWINLINE void Set_Alpha(float alpha, unsigned int & color)
{
	color &= 0x00FFFFFF;
	color |= (static_cast<unsigned int>(alpha * 255.0f) << 24);
}
}
