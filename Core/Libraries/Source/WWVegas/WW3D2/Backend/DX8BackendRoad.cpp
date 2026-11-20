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

// TheSuperHackers @refactor bobtista 07/10/2026 The road effect, moved from the road shaders of
// W3DShaderManager. Roads with both cloud and light map draw in one pass with roadnoise2.pso when
// pixel shaders are allowed. Every other road draws with texture stages, in two passes when it has
// both cloud and light map.

#include "DX8Backend.h"

#include "WW3D2/dx8wrapper.h"

void DX8Backend::Draw_Road(const RenderBackendRoadDraw & road, unsigned short start_index, unsigned short polygon_count,
                           unsigned short min_vertex_index, unsigned short vertex_count)
{
    const bool has_cloud_and_light_map = road.maps.cloud_texture && road.maps.light_map_texture;

    if (RoadPixelShader && has_cloud_and_light_map)
    {
        Set_Road_Pixel_Shader_State(road);
        Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

        DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(0);
        Clear_Texture_Projection(0);
        Clear_Texture_Projection(1);
        Clear_Texture_Projection(2);
        Clear_Texture_Projection(3);
        Invalidate_Cached_Render_States();
        return;
    }

    const int pass_count = has_cloud_and_light_map ? 2 : 1;
    for (int pass = 0; pass < pass_count; ++pass)
    {
        Set_Road_Stage_State(road, pass);
        Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
    }

    ShaderClass::Invalidate();
    Clear_Texture_Projection(0);
    Clear_Texture_Projection(1);
}

void DX8Backend::Set_Road_Pixel_Shader_State(const RenderBackendRoadDraw & road)
{
    //force WW3D2 system to set it's states so it won't later overwrite our custom settings.
    Apply_Render_State_Changes();

    //tell pixel shader which UV set to use for each stage
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );

    Set_Depth_Func(RB_CMP_LESS_EQUAL);
    Set_Depth_Write_Enable(false);
    Set_Lighting_Enable(false);

    Set_Alpha_Blend_Enable(true);	//blend roads into terrain
    Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);

    if (road.trilinear_filtering)
    {
        Set_Texture_Mip_Filter(0, RB_TEXTURE_SAMPLE_LINEAR);
        Set_Texture_Mip_Filter(1, RB_TEXTURE_SAMPLE_LINEAR);
    }
    else
    {
        Set_Texture_Mip_Filter(0, RB_TEXTURE_SAMPLE_POINT);
        Set_Texture_Mip_Filter(1, RB_TEXTURE_SAMPLE_POINT);
    }

    Set_Texture_Address_Mode(1, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP);

    Set_Texture_Address_Mode(2, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP);

    Set_Texture(1, road.maps.cloud_texture);
    Set_Texture(2, road.maps.light_map_texture);

    DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(RoadPixelShader);

    Set_Texture_Min_Mag_Filter(1, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);

    Set_Texture_Min_Mag_Filter(2, RB_TEXTURE_SAMPLE_POINT, RB_TEXTURE_SAMPLE_LINEAR);

    Set_World_Texture_Projection(1, road.maps.cloud_projection);

    Set_World_Texture_Projection(2, road.maps.light_map_projection);
}

void DX8Backend::Set_Road_Stage_State(const RenderBackendRoadDraw & road, int pass)
{
    //Force system to apply world/view transforms.
    Apply_Render_State_Changes();

    Set_Depth_Func(RB_CMP_LESS_EQUAL);
    Set_Depth_Write_Enable(false);
    Set_Lighting_Enable(false);

    // Modulate the diffuse color with the texture as lighting comes from diffuse.
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );

    DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_TEXCOORDINDEX, 0 );
    Set_Alpha_Blend_Enable(true);	//blend roads into terrain

    if (pass == 0)
    {
        Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);

        if (road.maps.cloud_texture || road.maps.light_map_texture)
        {
            //second texture unit will contain a noise pass
            if (road.trilinear_filtering)
                Set_Texture_Mip_Filter(1, RB_TEXTURE_SAMPLE_LINEAR);
            else
                Set_Texture_Mip_Filter(1, RB_TEXTURE_SAMPLE_POINT);

            Set_Texture_Address_Mode(1, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP);

            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );

            if (road.maps.cloud_texture)
            {
                //cloud map, the light map follows in pass 1 when there is one.
                Set_Texture(1, road.maps.cloud_texture);
                Set_Texture_Min_Mag_Filter(1, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
                Set_World_Texture_Projection(1, road.maps.cloud_projection);
            }
            else
            {
                //light map
                Set_Texture(1, road.maps.light_map_texture);
                Set_Texture_Min_Mag_Filter(1, RB_TEXTURE_SAMPLE_POINT, RB_TEXTURE_SAMPLE_LINEAR);
                Set_World_Texture_Projection(1, road.maps.light_map_projection);
            }
        }
        else
        {
            //just base texturing
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
            DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
        }
    }
    else
    {
        //pass 1, apply additional noise pass
        if (road.trilinear_filtering)
            Set_Texture_Mip_Filter(1, RB_TEXTURE_SAMPLE_LINEAR);
        else
            Set_Texture_Mip_Filter(1, RB_TEXTURE_SAMPLE_POINT);

        Set_Texture(1, road.maps.light_map_texture);

        Set_Texture_Min_Mag_Filter(1, RB_TEXTURE_SAMPLE_POINT, RB_TEXTURE_SAMPLE_LINEAR);

        // TheSuperHackers @info bobtista 06/10/2026 Retail set the light map matrix on stage 0 instead of
        // stage 1, so stage 1 keeps the cloud projection from pass 0.
        Set_World_Texture_Projection(1, road.maps.cloud_projection);

        Set_Texture_Address_Mode(1, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP, RB_TEXTURE_ADDRESS_WRAP);

        //Copy alpha channel into stage 1 but mask out color channel by replacing with white.
        DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        //Force color channel to white by copying the alpha into RGB
        DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE|D3DTA_ALPHAREPLICATE);
        DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
        DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );

        DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
        DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_BLENDCURRENTALPHA);
        DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
        DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

        //Modulate into existing roads with clouds applied. - only apply where roads are transparent by
        //using road texture as a mask.
        Set_Blend_Factors(RB_BLEND_ZERO, RB_BLEND_SRC_COLOR);
    }
}
