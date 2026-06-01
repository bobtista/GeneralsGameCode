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

#include "legacyd3dmatrix.h"

#if !defined(GGC_BGFX_STANDALONE)
struct IDirect3DBaseTexture8;
#endif

struct LegacyFixedFunctionColor
{
	float r;
	float g;
	float b;
	float a;
};

struct LegacyFixedFunctionVector3
{
	float x;
	float y;
	float z;
};

struct LegacyFixedFunctionLight
{
	unsigned int Type;
	LegacyFixedFunctionColor Diffuse;
	LegacyFixedFunctionColor Specular;
	LegacyFixedFunctionColor Ambient;
	LegacyFixedFunctionVector3 Position;
	LegacyFixedFunctionVector3 Direction;
	float Range;
	float Falloff;
	float Attenuation0;
	float Attenuation1;
	float Attenuation2;
	float Theta;
	float Phi;
};

#if defined(GGC_BGFX_STANDALONE)
using LegacyRawTexture = void;
#else
using LegacyRawTexture = IDirect3DBaseTexture8;
#endif
using LegacyTransformMatrix = D3DMATRIX;

constexpr unsigned LEGACY_D3DTS_WORLD = 256;
constexpr unsigned LEGACY_FIXED_FUNCTION_TRANSFORM_COUNT = LEGACY_D3DTS_WORLD + 1;
