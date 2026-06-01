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

#if defined(GGC_BGFX_STANDALONE)
#include "texturecompatibilitytypes.h"
#else
#include "dx8texturelegacyd3dtypes.h"
#endif

#if defined(GGC_BGFX_STANDALONE)
using LegacyBaseTexture = NativeCompatibilityBaseTexture;
using LegacySurface = NativeCompatibilitySurface;
using NativeCompatibilityTextureSurface = NativeCompatibilitySurface;
using LegacySurfaceDesc = NativeCompatibilitySurfaceDesc;
using LegacyVolumeDesc = NativeCompatibilityVolumeDesc;
using LegacyLockedRect = NativeCompatibilityLockedRect;

using LegacyLoaderTexture = NativeCompatibilityTexture2D;
using LegacyLoaderSurface = NativeCompatibilitySurface;
using LegacyLoaderCubeTexture = NativeCompatibilityCubeTexture;
using LegacyLoaderVolumeTexture = NativeCompatibilityVolumeTexture;
using LegacyLoaderLockedRect = NativeCompatibilityLockedRect;
using LegacyLoaderLockedBox = NativeCompatibilityLockedBox;
using LegacyLoaderCubeFace = NativeCompatibilityCubeFace;
#endif
