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

// TheSuperHackers @refactor bobtista 08/10/2026 Pixel shaders of the DX8 effects. They are created and
// released with the W3DShaderManager resources, which also follow device resets.

#include "DX8Backend.h"

#include "WW3D2/dx8wrapper.h"
#include "WWLib/ffactory.h"
#include "WWLib/wwfile.h"

#include <vector>

static DWORD Load_Pixel_Shader(const char * filename)
{
    file_auto_ptr file(_TheFileFactory, filename);
    if (!file->Is_Available() || !file->Open())
    {
        return 0;
    }

    const int size = file->Size();
    std::vector<DWORD> code((size + sizeof(DWORD) - 1) / sizeof(DWORD));
    const int read = code.empty() ? 0 : file->Read(&code[0], size);
    file->Close();
    if (read != size || code.empty())
    {
        return 0;
    }

    DWORD handle = 0;
    if (FAILED(DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(&code[0], &handle)))
    {
        return 0;
    }
    return handle;
}

static void Delete_Pixel_Shader(unsigned long & handle)
{
    if (handle)
    {
        DX8Wrapper::_Get_D3D_Device8()->DeletePixelShader(handle);
        handle = 0;
    }
}

void DX8Backend::Create_Effect_Resources(bool allow_pixel_shaders)
{
    Release_Effect_Resources();

    if (allow_pixel_shaders)
    {
        RoadPixelShader = Load_Pixel_Shader("shaders\\roadnoise2.pso");

        TerrainPixelShader = Load_Pixel_Shader("shaders\\terrain.pso");
        TerrainOneMapPixelShader = Load_Pixel_Shader("shaders\\terrainnoise.pso");
        TerrainTwoMapsPixelShader = Load_Pixel_Shader("shaders\\terrainnoise2.pso");
        if (!TerrainPixelShader || !TerrainOneMapPixelShader || !TerrainTwoMapsPixelShader)
        {
            Release_Terrain_Pixel_Shaders();
        }

        Create_Water_Shaders(*this, WaterShaders);
    }
}

void DX8Backend::Release_Effect_Resources()
{
    Delete_Pixel_Shader(RoadPixelShader);
    Release_Terrain_Pixel_Shaders();
    Release_Water_Shaders(*this, WaterShaders);
}

void DX8Backend::Release_Terrain_Pixel_Shaders()
{
    Delete_Pixel_Shader(TerrainPixelShader);
    Delete_Pixel_Shader(TerrainOneMapPixelShader);
    Delete_Pixel_Shader(TerrainTwoMapsPixelShader);
}

void DX8Backend::Draw_Water(const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count,
                            unsigned short min_vertex_index, unsigned short vertex_count)
{
    Draw_Water_Stages(*this, WaterShaders, water, start_index, polygon_count, min_vertex_index, vertex_count);
}
