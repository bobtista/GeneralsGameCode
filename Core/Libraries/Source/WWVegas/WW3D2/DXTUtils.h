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

// TheSuperHackers @refactor bobtista 26/04/2026 Shared DXT block-compressed
// texture math used by StubD3D8Device and BgfxBackendTextures. Keeping these
// in one place prevents the pitch/row calculations from diverging.

#pragma once

// TheSuperHackers @info bobtista 26/04/2026 DXT block dimensions are always
// 4x4 pixels. DXT1 uses 8 bytes per block, DXT2-5 use 16 bytes per block.

inline unsigned DXT_SurfacePitch(unsigned width, unsigned blockBytes)
{
	return ((width + 3) / 4) * blockBytes;
}

inline unsigned DXT_SurfaceRows(unsigned height)
{
	return (height + 3) / 4;
}

inline unsigned DXT_SurfaceStorageSize(unsigned width, unsigned height, unsigned blockBytes)
{
	return DXT_SurfacePitch(width, blockBytes) * DXT_SurfaceRows(height);
}
