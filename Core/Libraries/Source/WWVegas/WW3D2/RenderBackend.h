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

// TheSuperHackers @refactor bobtista 10/04/2026 Backend-agnostic access point
// for the global IRenderBackend instance. Engine-side code should include
// this header (not IRenderBackend.h or DX8Backend.h directly) to use the
// render backend.

#pragma once

#include "IRenderBackend.h"

// The active rendering backend. Set by Init_Render_Backend() during
// WW3D device initialization and cleared by Shutdown_Render_Backend()
// during device teardown. Never null between those two calls.
extern IRenderBackend * g_renderBackend;

// Create the render backend. Called by DX8Wrapper::Do_Onetime_Device_Dependent_Inits
// after the D3D device has been successfully created.
//
// Phase 1 always creates a DX8Backend. Phase 2 will add a compile-time
// option to select between DX8Backend, BgfxBackend, and DiligentBackend.
void Init_Render_Backend();

// Destroy the render backend. Called by DX8Wrapper::Do_Onetime_Device_Dependent_Shutdowns
// before the D3D device is released.
void Shutdown_Render_Backend();
