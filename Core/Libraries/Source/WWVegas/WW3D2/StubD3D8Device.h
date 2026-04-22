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

// TheSuperHackers @refactor bobtista 22/04/2026 Phase 5.2 Stage 1
// No-op implementations of the Direct3D 8 COM interfaces. These stubs allow
// DX8Wrapper to be driven against a synthetic Direct3D 8 device without
// loading d3d8.dll at runtime. Every method returns S_OK / D3D_OK and does
// nothing; resource creation returns ref-counted stub objects; Lock methods
// hand out heap-allocated scratch buffers. Compiled only under the
// GGC_BGFX_STANDALONE build configuration used by the bgfx-only renderer.

#pragma once

#if defined(GGC_BGFX_STANDALONE)

#include <d3d8.h>

// Entry point. Returns a newly AddRef'd IDirect3D8 stub. Caller releases.
IDirect3D8* CreateStubD3D8Interface();

#endif // GGC_BGFX_STANDALONE
