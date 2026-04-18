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

// TheSuperHackers @refactor bobtista 10/04/2026 Render backend global
// owner. Holds the single g_renderBackend pointer and constructs/destroys
// the concrete backend instance. See RENDER_BACKEND.md.

#include "RenderBackend.h"
#include "DX8Backend.h"

IRenderBackend * g_renderBackend = nullptr;

void Init_Render_Backend()
{
    if (g_renderBackend != nullptr)
    {
        return;
    }

    // Phase 1: the DX8 backend is the only option. Phase 2 will introduce
    // a compile-time flag to pick between DX8, bgfx, and Diligent.
    g_renderBackend = new DX8Backend();
}

void Shutdown_Render_Backend()
{
    if (g_renderBackend == nullptr)
    {
        return;
    }
    delete g_renderBackend;
    g_renderBackend = nullptr;
}
