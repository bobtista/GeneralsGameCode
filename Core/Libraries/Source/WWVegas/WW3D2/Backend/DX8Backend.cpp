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

// TheSuperHackers @refactor bobtista 10/04/2026 DX8Backend forwarding adapter.
// Every method in this file is a one-line trampoline to the existing
// DX8Wrapper static API. Keep it that way — if behavior needs to change it
// should change in DX8Wrapper, not here.

#include "DX8Backend.h"
#include "RenderBackend.h"

#include "WW3D2/dx8wrapper.h"
#include "WWMath/vector3.h"
#include "WW3D2/lightenvironment.h"

DX8Backend::DX8Backend()
{
}

DX8Backend::~DX8Backend()
{
}

IRenderBackend *Create_Render_Backend()
{
    return new DX8Backend();
}

void DX8Backend::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    DX8Wrapper::Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

void DX8Backend::Begin_Scene()
{
    DX8Wrapper::Begin_Scene();
}

void DX8Backend::End_Scene(bool flip_frame)
{
    DX8Wrapper::End_Scene(flip_frame);
}

void DX8Backend::Flip_To_Primary()
{
    DX8Wrapper::Flip_To_Primary();
}

void DX8Backend::Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha, float z, unsigned int stencil)
{
    DX8Wrapper::Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void DX8Backend::Set_Viewport(const RenderBackendViewport & viewport)
{
    D3DVIEWPORT8 vp;
    vp.X      = viewport.x;
    vp.Y      = viewport.y;
    vp.Width  = viewport.width;
    vp.Height = viewport.height;
    vp.MinZ   = viewport.min_z;
    vp.MaxZ   = viewport.max_z;
    DX8Wrapper::Set_Viewport(&vp);
}

void DX8Backend::Invalidate_Cached_Render_States()
{
    DX8Wrapper::Invalidate_Cached_Render_States();
}

void DX8Backend::Set_Ambient(const Vector3 & color)
{
    DX8Wrapper::Set_Ambient(color);
}

void DX8Backend::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    DX8Wrapper::Set_Light_Environment(light_env);
}
