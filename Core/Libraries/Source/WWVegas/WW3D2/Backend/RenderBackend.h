/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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
// for the global IRenderBackend instance. Engine-side code should include this
// header (not IRenderBackend.h or DX8Backend.h directly) to use the backend.

#pragma once

#include "WW3D2/IRenderBackend.h"

// The active rendering backend. Set by Init_Render_Backend() and cleared by
// Shutdown_Render_Backend(); never null between those two calls.
extern IRenderBackend * g_renderBackend;

// Create the render backend. Must be called after the render device is ready.
void Init_Render_Backend();

// Destroy the render backend. Must be called before the render device is released.
void Shutdown_Render_Backend();
