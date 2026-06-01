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

// TheSuperHackers @refactor bobtista 10/04/2026 Render backend global owner: holds the
// single g_renderBackend pointer and constructs/destroys the concrete backend, selected
// at compile time via GGC_RENDER_BACKEND_DX8 (default) or GGC_RENDER_BACKEND_BGFX.

#include "RenderBackend.h"

#if defined(GGC_RENDER_BACKEND_BGFX)
#include "BgfxBackend.h"
#else
#include "DX8Backend.h"
#endif

IRenderBackend * g_renderBackend = nullptr;

void Init_Render_Backend()
{
    if (g_renderBackend != nullptr)
    {
        return;
    }
#if defined(GGC_RENDER_BACKEND_BGFX)
    g_renderBackend = new BgfxBackend();
#else
    g_renderBackend = new DX8Backend();
#endif
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
