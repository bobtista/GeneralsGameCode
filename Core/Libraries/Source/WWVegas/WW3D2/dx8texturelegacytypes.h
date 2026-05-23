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

#if defined(GGC_BGFX_STANDALONE)
#include "dx8standalonetypes.h"
#else
#include "d3d8.h"
#endif

using LegacyBaseTexture = IDirect3DBaseTexture8;
using LegacySurface = IDirect3DSurface8;
using LegacyTextureSurface = IDirect3DSurface8;
using LegacySurfaceDesc = D3DSURFACE_DESC;
using LegacyVolumeDesc = D3DVOLUME_DESC;
using LegacyLockedRect = D3DLOCKED_RECT;

using LegacyLoaderTexture = IDirect3DTexture8;
using LegacyLoaderSurface = IDirect3DSurface8;
using LegacyLoaderCubeTexture = IDirect3DCubeTexture8;
using LegacyLoaderLockedRect = D3DLOCKED_RECT;
using LegacyLoaderLockedBox = D3DLOCKED_BOX;
using LegacyLoaderCubeFace = D3DCUBEMAP_FACES;
