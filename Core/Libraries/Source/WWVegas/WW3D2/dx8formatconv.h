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

#include "ww3dformat.h"
#include <d3d8.h>

/*
** Legacy format conversion boundary. Keep this header out of neutral
** renderer-facing code; callers that include it are explicitly translating to
** or from native legacy format values.
*/

D3DFORMAT WW3DFormat_To_D3DFormat(WW3DFormat ww3d_format);
WW3DFormat D3DFormat_To_WW3DFormat(D3DFORMAT d3d_format);

D3DFORMAT WW3DZFormat_To_D3DFormat(WW3DZFormat ww3d_zformat);
WW3DZFormat D3DFormat_To_WW3DZFormat(D3DFORMAT d3d_format);

void Init_D3D_To_WW3_Conversion();
