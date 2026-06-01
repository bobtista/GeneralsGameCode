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

#include "ww3dformat.h"
#if defined(GGC_BGFX_STANDALONE)
#include "dx8standalonetypes.h"
#else
#include <d3d8.h>
#endif

/*
** Legacy format conversion boundary. Keep this header out of neutral
** renderer-facing code; callers that include it are explicitly translating to
** or from native legacy format values.
*/

#ifdef GGC_BGFX_STANDALONE

inline D3DFORMAT WW3DFormat_To_D3DFormat(WW3DFormat ww3d_format)
{
	switch (ww3d_format) {
	case WW3D_FORMAT_R8G8B8: return D3DFMT_R8G8B8;
	case WW3D_FORMAT_A8R8G8B8: return D3DFMT_A8R8G8B8;
	case WW3D_FORMAT_X8R8G8B8: return D3DFMT_X8R8G8B8;
	case WW3D_FORMAT_R5G6B5: return D3DFMT_R5G6B5;
	case WW3D_FORMAT_X1R5G5B5: return D3DFMT_X1R5G5B5;
	case WW3D_FORMAT_A1R5G5B5: return D3DFMT_A1R5G5B5;
	case WW3D_FORMAT_A4R4G4B4: return D3DFMT_A4R4G4B4;
	case WW3D_FORMAT_R3G3B2: return D3DFMT_R3G3B2;
	case WW3D_FORMAT_A8: return D3DFMT_A8;
	case WW3D_FORMAT_A8R3G3B2: return D3DFMT_A8R3G3B2;
	case WW3D_FORMAT_X4R4G4B4: return D3DFMT_X4R4G4B4;
	case WW3D_FORMAT_A8P8: return D3DFMT_A8P8;
	case WW3D_FORMAT_P8: return D3DFMT_P8;
	case WW3D_FORMAT_L8: return D3DFMT_L8;
	case WW3D_FORMAT_A8L8: return D3DFMT_A8L8;
	case WW3D_FORMAT_A4L4: return D3DFMT_A4L4;
	case WW3D_FORMAT_U8V8: return D3DFMT_V8U8;
	case WW3D_FORMAT_L6V5U5: return D3DFMT_L6V5U5;
	case WW3D_FORMAT_X8L8V8U8: return D3DFMT_X8L8V8U8;
	case WW3D_FORMAT_DXT1: return D3DFMT_DXT1;
	case WW3D_FORMAT_DXT2: return D3DFMT_DXT2;
	case WW3D_FORMAT_DXT3: return D3DFMT_DXT3;
	case WW3D_FORMAT_DXT4: return D3DFMT_DXT4;
	case WW3D_FORMAT_DXT5: return D3DFMT_DXT5;
	default: return D3DFMT_UNKNOWN;
	}
}

inline WW3DFormat D3DFormat_To_WW3DFormat(D3DFORMAT d3d_format)
{
	switch (d3d_format) {
	case D3DFMT_R8G8B8: return WW3D_FORMAT_R8G8B8;
	case D3DFMT_A8R8G8B8: return WW3D_FORMAT_A8R8G8B8;
	case D3DFMT_X8R8G8B8: return WW3D_FORMAT_X8R8G8B8;
	case D3DFMT_R5G6B5: return WW3D_FORMAT_R5G6B5;
	case D3DFMT_X1R5G5B5: return WW3D_FORMAT_X1R5G5B5;
	case D3DFMT_A1R5G5B5: return WW3D_FORMAT_A1R5G5B5;
	case D3DFMT_A4R4G4B4: return WW3D_FORMAT_A4R4G4B4;
	case D3DFMT_R3G3B2: return WW3D_FORMAT_R3G3B2;
	case D3DFMT_A8: return WW3D_FORMAT_A8;
	case D3DFMT_A8R3G3B2: return WW3D_FORMAT_A8R3G3B2;
	case D3DFMT_X4R4G4B4: return WW3D_FORMAT_X4R4G4B4;
	case D3DFMT_A8P8: return WW3D_FORMAT_A8P8;
	case D3DFMT_P8: return WW3D_FORMAT_P8;
	case D3DFMT_L8: return WW3D_FORMAT_L8;
	case D3DFMT_A8L8: return WW3D_FORMAT_A8L8;
	case D3DFMT_A4L4: return WW3D_FORMAT_A4L4;
	case D3DFMT_V8U8: return WW3D_FORMAT_U8V8;
	case D3DFMT_L6V5U5: return WW3D_FORMAT_L6V5U5;
	case D3DFMT_X8L8V8U8: return WW3D_FORMAT_X8L8V8U8;
	case D3DFMT_DXT1: return WW3D_FORMAT_DXT1;
	case D3DFMT_DXT2: return WW3D_FORMAT_DXT2;
	case D3DFMT_DXT3: return WW3D_FORMAT_DXT3;
	case D3DFMT_DXT4: return WW3D_FORMAT_DXT4;
	case D3DFMT_DXT5: return WW3D_FORMAT_DXT5;
	default: return WW3D_FORMAT_UNKNOWN;
	}
}

inline D3DFORMAT WW3DZFormat_To_D3DFormat(WW3DZFormat ww3d_zformat)
{
	switch (ww3d_zformat) {
	case WW3D_ZFORMAT_D16_LOCKABLE: return D3DFMT_D16_LOCKABLE;
	case WW3D_ZFORMAT_D32: return D3DFMT_D32;
	case WW3D_ZFORMAT_D15S1: return D3DFMT_D15S1;
	case WW3D_ZFORMAT_D24S8: return D3DFMT_D24S8;
	case WW3D_ZFORMAT_D16: return D3DFMT_D16;
	case WW3D_ZFORMAT_D24X8: return D3DFMT_D24X8;
	case WW3D_ZFORMAT_D24X4S4: return D3DFMT_D24X4S4;
	default: return D3DFMT_UNKNOWN;
	}
}

inline WW3DZFormat D3DFormat_To_WW3DZFormat(D3DFORMAT d3d_format)
{
	switch (d3d_format) {
	case D3DFMT_D16_LOCKABLE: return WW3D_ZFORMAT_D16_LOCKABLE;
	case D3DFMT_D32: return WW3D_ZFORMAT_D32;
	case D3DFMT_D15S1: return WW3D_ZFORMAT_D15S1;
	case D3DFMT_D24S8: return WW3D_ZFORMAT_D24S8;
	case D3DFMT_D16: return WW3D_ZFORMAT_D16;
	case D3DFMT_D24X8: return WW3D_ZFORMAT_D24X8;
	case D3DFMT_D24X4S4: return WW3D_ZFORMAT_D24X4S4;
	default: return WW3D_ZFORMAT_UNKNOWN;
	}
}

inline void Init_D3D_To_WW3_Conversion()
{
}

#else

D3DFORMAT WW3DFormat_To_D3DFormat(WW3DFormat ww3d_format);
WW3DFormat D3DFormat_To_WW3DFormat(D3DFORMAT d3d_format);

D3DFORMAT WW3DZFormat_To_D3DFormat(WW3DZFormat ww3d_zformat);
WW3DZFormat D3DFormat_To_WW3DZFormat(D3DFORMAT d3d_format);

void Init_D3D_To_WW3_Conversion();

#endif
