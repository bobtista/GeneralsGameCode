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

// TheSuperHackers @refactor bobtista 08/10/2026 The shroud overlay, moved from the stage 0 shroud shader of
// W3DShaderManager. The overlay redraws geometry with a multiplicative blend at equal depth.

#include "DX8Backend.h"

#include "WW3D2/vertmaterial.h"

void DX8Backend::Begin_Shroud_Overlay(const RenderBackendShroud & shroud)
{
    //force WW3D2 system to set it's states so it won't later overwrite our custom settings.
    VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
    Set_Material(vmat);
    REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
    Bind_Texture_Immediate(0, shroud.texture);

    Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
    Apply_Render_State_Changes();
    Set_Depth_Func(RB_CMP_EQUAL);

    Set_World_Texture_Projection(0, shroud.projection);
}

void DX8Backend::End_Shroud_Overlay()
{
    Set_Texture(0, nullptr);
    Set_Depth_Func(RB_CMP_LESS_EQUAL);
    Clear_Texture_Projection(0);
}
