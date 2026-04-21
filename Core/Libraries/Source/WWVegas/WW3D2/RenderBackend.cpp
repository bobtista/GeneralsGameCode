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

// TheSuperHackers @refactor bobtista 10/04/2026 Render backend global
// owner. Holds the single g_renderBackend pointer and constructs/destroys
// the concrete backend instance.
//
// The concrete backend is selected at compile time via the GGC_RENDER_BACKEND
// CMake flag which sets one of:
//   GGC_RENDER_BACKEND_DX8      - DirectX 8 (default, VC6-compatible, Windows only)
//   GGC_RENDER_BACKEND_BGFX     - bgfx (DX11/Vulkan/Metal/GL, cross-platform)
//   GGC_RENDER_BACKEND_DILIGENT - Diligent Engine (DX11/Vulkan/Metal, cross-platform)
//
// Exactly one of these is defined in any given build. If none are defined
// (a legacy build that hasn't included render-backend.cmake) we default to
// DX8 so the legacy path keeps working unchanged.
//

#include "RenderBackend.h"

#if defined(GGC_RENDER_BACKEND_BGFX)
#include "BgfxBackend.h"
#elif defined(GGC_RENDER_BACKEND_DILIGENT)
#include "DiligentBackend.h"
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
#elif defined(GGC_RENDER_BACKEND_DILIGENT)
    g_renderBackend = new DiligentBackend();
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
