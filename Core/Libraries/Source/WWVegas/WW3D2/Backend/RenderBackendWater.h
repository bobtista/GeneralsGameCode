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

#pragma once

#include "WW3D2/IRenderBackend.h"

class TextureClass;

struct RenderBackendWaterShaders
{
    unsigned long water_pixel_shader;
    unsigned long river_pixel_shader;
    TextureClass * white_texture;
};

void Create_Water_Shaders(IRenderBackend & backend, RenderBackendWaterShaders & shaders);
void Release_Water_Shaders(IRenderBackend & backend, RenderBackendWaterShaders & shaders);

// Draws water with texture stages and the legacy water pixel shaders. Shared by the backends that express the
// water effect through their stage state.
void Draw_Water_Stages(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
                       const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count,
                       unsigned short min_vertex_index, unsigned short vertex_count);
