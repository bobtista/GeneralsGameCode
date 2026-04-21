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

// TheSuperHackers @refactor bobtista 10/04/2026 Render backend selection
// point. Constructs the concrete backend that WW3D owns for the lifetime of
// the device.
//
// The concrete backend is selected at compile time via the GGC_RENDER_BACKEND
// CMake flag which sets one of:
//   GGC_RENDER_BACKEND_DX8      - DirectX 8 (default, VC6-compatible, Windows only)
//   GGC_RENDER_BACKEND_BGFX     - bgfx (DX11/Vulkan/Metal/GL, cross-platform)
//
// Exactly one of these is defined in any given build. If none are defined
// (a legacy build that hasn't included render-backend.cmake) we default to
// DX8 so the legacy path keeps working unchanged.
//
// See RENDER_BACKEND.md.

#include "Backend/RenderBackend.h"

#include "IRenderBackend.h"

#if defined(GGC_RENDER_BACKEND_BGFX)
#include "Backend/BgfxBackend.h"
#else
#include "Backend/DX8Backend.h"
#endif

IRenderBackend * Create_Render_Backend(void * window, bool lite)
{
#if defined(GGC_RENDER_BACKEND_BGFX)
    (void)window;
    (void)lite;
    return new BgfxBackend();
#else
    return DX8Backend::Create(window, lite);
#endif
}

// TheSuperHackers @refactor bobtista 29/10/2026 Bridge from the upstream static
// Renderer class to the active IRenderBackend. Renderer owns the backend instance
// and forwards to it; device queries go to DX8Wrapper as they did before the
// Renderer class existed.

#include "Renderer.h"
#include "dx8wrapper.h"
#include "ww3d.h"

static IRenderBackend * TheRenderBackend = nullptr;

IRenderBackend * WW3D::Get_Render_Backend()
{
    return TheRenderBackend;
}

bool Renderer::Init(void * window, bool lite)
{
    WWASSERT(TheRenderBackend == nullptr);
    if (TheRenderBackend == nullptr)
    {
        TheRenderBackend = Create_Render_Backend(window, lite);
    }
    return TheRenderBackend != nullptr;
}

void Renderer::Shutdown()
{
    delete TheRenderBackend;
    TheRenderBackend = nullptr;
}

int Renderer::Get_Render_Device_Count()
{
    return DX8Wrapper::Get_Render_Device_Count();
}

int Renderer::Get_Render_Device()
{
    return DX8Wrapper::Get_Render_Device();
}

const char * Renderer::Get_Render_Device_Name(int device_index)
{
    return DX8Wrapper::Get_Render_Device_Name(device_index);
}

void Renderer::Get_Device_Resolution(int & width, int & height, int & bits, bool & windowed)
{
    DX8Wrapper::Get_Device_Resolution(width, height, bits, windowed);
}

void Renderer::Get_Render_Target_Resolution(int & width, int & height, int & bits, bool & windowed)
{
    DX8Wrapper::Get_Render_Target_Resolution(width, height, bits, windowed);
}

int Renderer::Get_Device_Resolution_Width()
{
    return DX8Wrapper::Get_Device_Resolution_Width();
}

int Renderer::Get_Device_Resolution_Height()
{
    return DX8Wrapper::Get_Device_Resolution_Height();
}

bool Renderer::Is_Windowed()
{
    return DX8Wrapper::Is_Windowed();
}

int Renderer::Get_Texture_Bitdepth()
{
    return DX8Wrapper::Get_Texture_Bitdepth();
}

int Renderer::Get_Swap_Interval()
{
    return DX8Wrapper::Get_Swap_Interval();
}

bool Renderer::Has_Stencil()
{
    return TheRenderBackend->Has_Stencil();
}

WW3DFormat Renderer::Get_Back_Buffer_Format()
{
    return TheRenderBackend->Get_Back_Buffer_Format();
}

void Renderer::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    TheRenderBackend->Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

void Renderer::Begin_Scene()
{
    TheRenderBackend->Begin_Scene();
}

void Renderer::End_Scene(bool flip_frame)
{
    TheRenderBackend->End_Scene(flip_frame);
}

void Renderer::Flip_To_Primary()
{
    TheRenderBackend->Flip_To_Primary();
}

void Renderer::Clear(bool clear_color, bool clear_z_stencil, const Vector3 & color, float dest_alpha, float z, unsigned int stencil)
{
    TheRenderBackend->Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void Renderer::Set_Viewport(const RenderViewport & viewport)
{
    RenderBackendViewport vp;
    vp.x = viewport.x;
    vp.y = viewport.y;
    vp.width = viewport.width;
    vp.height = viewport.height;
    vp.min_z = viewport.min_z;
    vp.max_z = viewport.max_z;
    TheRenderBackend->Set_Viewport(vp);
}

void Renderer::Invalidate_Cached_Render_States()
{
    TheRenderBackend->Invalidate_Cached_Render_States();
}

void Renderer::Set_Ambient(const Vector3 & color)
{
    TheRenderBackend->Set_Ambient(color);
}

void Renderer::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    TheRenderBackend->Set_Light_Environment(light_env);
}
