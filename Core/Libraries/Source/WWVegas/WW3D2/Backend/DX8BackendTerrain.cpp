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

// TheSuperHackers @refactor bobtista 08/10/2026 The terrain effect, moved from the terrain shaders of
// W3DShaderManager. Terrain draws in one pass with the terrain pixel shaders when they are allowed.
// Otherwise it draws with texture stages in two passes, plus one for the cloud and light map.
// Every vertex buffer is drawn once per pass, before the next pass starts.

#include "DX8Backend.h"

#include "WW3D2/dx8wrapper.h"
#include "WW3D2/texture.h"

static void Set_Terrain_Filters(RenderBackendTerrainFiltering filtering)
{
    if (filtering != RB_TERRAIN_FILTER_POINT) {
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    } else {
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    }
    // TheSuperHackers @info bobtista 08/10/2026 Without trilinear filtering, retail mips stage 0 with point
    // and stage 1 with linear filtering.
    if (filtering == RB_TERRAIN_FILTER_TRILINEAR) {
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
    } else {
        DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
        DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
    }
}

static void Draw_Terrain_Vertex_Buffers(DX8Backend & backend, VertexBufferClass * const * vertex_buffers,
                                        unsigned int vertex_buffer_count, unsigned short polygon_count, unsigned short vertex_count)
{
    for (unsigned int i = 0; i < vertex_buffer_count; ++i)
    {
        backend.Set_Vertex_Buffer(vertex_buffers[i], 0);
        backend.Draw_Triangles(0, polygon_count, 0, vertex_count);
    }
}

void DX8Backend::Draw_Terrain(const RenderBackendTerrainDraw & terrain, VertexBufferClass * const * vertex_buffers,
                              unsigned int vertex_buffer_count, unsigned short polygon_count, unsigned short vertex_count)
{
    if (TerrainPixelShader)
    {
        Set_Terrain_Pixel_Shader_State(terrain);
        Draw_Terrain_Vertex_Buffers(*this, vertex_buffers, vertex_buffer_count, polygon_count, vertex_count);

        Bind_Texture_Immediate(2, nullptr);	//release reference to any texture
        Bind_Texture_Immediate(3, nullptr);	//release reference to any texture

        DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(0);	//turn off pixel shader

        Bind_Texture_Immediate(0, nullptr);
        Bind_Texture_Immediate(1, nullptr);

        Clear_Texture_Projection(0);
        Clear_Texture_Projection(1);
        Clear_Texture_Projection(2);
        Clear_Texture_Projection(3);

        Invalidate_Cached_Render_States();
        return;
    }

    const bool has_map = terrain.maps.cloud_texture || terrain.maps.light_map_texture;
    const int pass_count = has_map ? 3 : 2;
    for (int pass = 0; pass < pass_count; ++pass)
    {
        Set_Terrain_Stage_State(terrain, pass);
        Draw_Terrain_Vertex_Buffers(*this, vertex_buffers, vertex_buffer_count, polygon_count, vertex_count);
    }

    ShaderClass::Invalidate();

    //Free references to textures
    Bind_Texture_Immediate(0, nullptr);
    Bind_Texture_Immediate(1, nullptr);

    Clear_Texture_Projection(0);
    Clear_Texture_Projection(1);
}

void DX8Backend::Set_Terrain_Pixel_Shader_State(const RenderBackendTerrainDraw & terrain)
{
    //force WW3D2 system to set it's states so it won't later overwrite our custom settings.
    Apply_Render_State_Changes();

    //setup base pass
    Bind_Texture_Immediate(0, terrain.atlas_texture);
    Bind_Texture_Immediate(1, terrain.atlas_texture);

    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

    //tell pixel shader which UV set to use for each stage
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
    DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_TEXCOORDINDEX, 1 );

    Set_Terrain_Filters(terrain.filtering);

    if (terrain.maps.cloud_texture || terrain.maps.light_map_texture)
    {
        RenderBackendWorldProjection projection;

        DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
        DX8Wrapper::Set_DX8_Texture_Stage_State(2,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

        if (terrain.maps.cloud_texture && terrain.maps.light_map_texture)
        {
            //full shader
            DX8Wrapper::Set_DX8_Texture_Stage_State(3,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
            DX8Wrapper::Set_DX8_Texture_Stage_State(3,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
            Bind_Texture_Immediate(2, terrain.maps.cloud_texture);
            Bind_Texture_Immediate(3, terrain.maps.light_map_texture);
            DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(TerrainTwoMapsPixelShader);

            DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
            DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

            DX8Wrapper::Set_DX8_Texture_Stage_State(3, D3DTSS_MINFILTER, D3DTEXF_POINT);
            DX8Wrapper::Set_DX8_Texture_Stage_State(3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

            Set_World_Texture_Projection(2, terrain.maps.cloud_projection);

            Set_World_Texture_Projection(3, terrain.maps.light_map_projection);
        }
        else
        {
            //single noise texture shader
            DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(TerrainOneMapPixelShader);

            if (terrain.maps.cloud_texture)
            {
                //cloud map
                Bind_Texture_Immediate(2, terrain.maps.cloud_texture);
                projection = terrain.maps.cloud_projection;
                DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
            }
            else
            {
                //light map
                Bind_Texture_Immediate(2, terrain.maps.light_map_texture);
                projection = terrain.maps.light_map_projection;
                DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MINFILTER, D3DTEXF_POINT);
                DX8Wrapper::Set_DX8_Texture_Stage_State(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
            }
            Set_World_Texture_Projection(2, projection);
        }
    }
    else
    {
        //just base texturing
        DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(TerrainPixelShader);
    }
}

void DX8Backend::Set_Terrain_Stage_State(const RenderBackendTerrainDraw & terrain, int pass)
{
    //force WW3D2 system to set it's states so it won't later overwrite our custom settings.
    Apply_Render_State_Changes();

    Set_Terrain_Filters(terrain.filtering);

    switch (pass)
    {
        case 0:
            Bind_Texture_Immediate(0, terrain.atlas_texture);
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

            // Modulate the diffuse color with the texture as lighting comes from diffuse.
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
            DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,false);
            break;
        case 1:
            Bind_Texture_Immediate(0, terrain.atlas_texture);
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

            // Modulate the diffuse color with the texture as lighting comes from diffuse.
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 1 );
            // Blend the result using the alpha. (came from diffuse mod texture)
            DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);
            DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
            DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            // Disable stage 2.
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
            break;
        case 2:
            // Noise/cloud pass
            RenderBackendWorldProjection projection;

            //these states apply to all noise/cloud combination passes
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

            DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
            DX8Wrapper::Set_DX8_Texture_Stage_State(0,  D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

            //blend into frame buffer
            DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE,true);
            DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,D3DBLEND_DESTCOLOR);
            DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND,D3DBLEND_ZERO);

            if (terrain.maps.cloud_texture && terrain.maps.light_map_texture)
            {
                //setup cloud pass
                Bind_Texture_Immediate(0, terrain.maps.cloud_texture);

                Set_World_Texture_Projection(0, terrain.maps.cloud_projection);
                //clouds always need bilinear filtering
                DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

                //setup noise pass
                Bind_Texture_Immediate(1, terrain.maps.light_map_texture);

                Set_World_Texture_Projection(1, terrain.maps.light_map_projection);
                //noise always needs point/linear filtering.  Why point!?
                DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
                DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);

                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
            }
            else
            {
                //only 1 noise or cloud texture
                // Now setup the texture pipeline.
                if (terrain.maps.cloud_texture)
                {
                    //setup cloud pass
                    Bind_Texture_Immediate(0, terrain.maps.cloud_texture);
                    projection = terrain.maps.cloud_projection;
                    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                    DX8Wrapper::Set_DX8_Texture_Stage_State(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                }
                else
                {
                    //setup noise pass
                    Bind_Texture_Immediate(0, terrain.maps.light_map_texture);
                    projection = terrain.maps.light_map_projection;
                    // TheSuperHackers @info bobtista 08/10/2026 Retail sets the light map filter on stage 1 instead
                    // of stage 0, so the light map is sampled with the terrain filter.
                    DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
                    DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                }

                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
                DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
                Set_World_Texture_Projection(0, projection);
            }
            break;
    }
}
