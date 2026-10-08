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

// TheSuperHackers @refactor bobtista 08/10/2026 The water effect, moved from the standing water, river and
// water grid shader setup of WaterRenderObjClass.

#include "RenderBackendWater.h"

#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/vertmaterial.h"
#include "WWMath/matrix4.h"

#define WATER_MESH_OPACITY		0.5f
#define REFLECTION_FACTOR 0.1f

static void Fill_White_Texture(TextureClass *texture)
{
	if (texture == nullptr)
	{
		return;
	}

	TextureClass::MutableTextureMipView mip = texture->Begin_Mip_Write(0);
	if (!mip.Is_Valid())
	{
		return;
	}

	if (mip.Format == WW3D_FORMAT_A4R4G4B4)
	{
		*reinterpret_cast<unsigned short *>(mip.Data) = 0xffff;
	}
	else if (mip.Format == WW3D_FORMAT_A8R8G8B8)
	{
		*reinterpret_cast<unsigned int *>(mip.Data) = 0xffffffff;
	}
	texture->End_Mip_Write(0);
}

void Create_Water_Shaders(IRenderBackend & backend, RenderBackendWaterShaders & shaders)
{
	unsigned long legacyHandle = 0;
	if (backend.Create_Legacy_Pixel_Shader(RB_LEGACY_PIXEL_SHADER_RIVER_WATER, &legacyHandle))
		shaders.river_pixel_shader = legacyHandle;
	if (backend.Create_Legacy_Pixel_Shader(RB_LEGACY_PIXEL_SHADER_TRAPEZOID_WATER, &legacyHandle))
		shaders.water_pixel_shader = legacyHandle;

	//For some reason setting a null texture does not result in 0xffffffff for pixel shaders so using explicit "white" texture.
	shaders.white_texture=NEW_REF(TextureClass,(1, 1, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1));
	Fill_White_Texture(shaders.white_texture);
}

void Release_Water_Shaders(IRenderBackend & backend, RenderBackendWaterShaders & shaders)
{
	if (shaders.water_pixel_shader)
		backend.Delete_Pixel_Shader(shaders.water_pixel_shader);
	if (shaders.river_pixel_shader)
		backend.Delete_Pixel_Shader(shaders.river_pixel_shader);
	shaders.water_pixel_shader = 0;
	shaders.river_pixel_shader = 0;
	REF_PTR_RELEASE(shaders.white_texture);
}

static inline Matrix4x4 Make_Scale_Texture_Matrix(float sx, float sy, float sz)
{
	return Matrix4x4(
		sx, 0.0f, 0.0f, 0.0f,
		0.0f, sy, 0.0f, 0.0f,
		0.0f, 0.0f, sz, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

static inline Matrix4x4 Make_Translation_Texture_Matrix(float x, float y, float z)
{
	return Matrix4x4(
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f);
}

static void Set_Noise_Texture_Transform(IRenderBackend & backend, unsigned stage,
	const RenderBackendWorldProjection & projection)
{
	Matrix4x4 view;
	backend.Get_Transform(RB_TRANSFORM_VIEW, view);
	const Matrix4x4 destMatrix =
		Make_Translation_Texture_Matrix(projection.bias_u, projection.bias_v, 0.0f) *
		Make_Scale_Texture_Matrix(projection.scale_x, projection.scale_y, 1.0f) *
		Make_Translation_Texture_Matrix(projection.offset_x, projection.offset_y, 0.0f) *
		view.Inverse();
	backend.Set_Texture_Transform(stage, destMatrix);
}

static void Set_Camera_Space_Texcoord2(IRenderBackend & backend, unsigned stage, unsigned uv_index)
{
	backend.Set_Texture_Coord_Source(stage, RB_TEXCOORD_CAMERA_SPACE_POSITION, uv_index);
	backend.Set_Texture_Transform_Mode(stage, 2, false);
}

static void Set_Stage_Address_2D(IRenderBackend & backend, unsigned stage, RenderBackendTextureAddressMode address_mode)
{
	backend.Set_Texture_Address_Mode(stage, address_mode, address_mode, RB_TEXTURE_ADDRESS_WRAP);
}

static void Set_Linear_Filter(IRenderBackend & backend, unsigned stage)
{
	backend.Set_Texture_Min_Mag_Filter(stage, RB_TEXTURE_SAMPLE_LINEAR, RB_TEXTURE_SAMPLE_LINEAR);
}

static void Set_Water_Material(IRenderBackend & backend, const RenderBackendWaterDraw & water)
{
	if (!water.additive)
		backend.Set_Shader(ShaderClass::_PresetAlphaShader);
	else
		backend.Set_Shader(ShaderClass::_PresetAdditiveShader);

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	backend.Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	water.water_texture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	water.water_texture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
	water.water_texture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);

	backend.Apply_Render_State_Changes();	//force update of view and projection matrices
}

static void Set_Noise_Stages(IRenderBackend & backend, const RenderBackendWaterDraw & water)
{
	if (!water.sparkle_texture->Is_Initialized())
		water.sparkle_texture->Init();
	backend.Bind_Texture_Immediate(1, water.sparkle_texture);

	if (!water.noise_texture->Is_Initialized())
		water.noise_texture->Init();
	backend.Bind_Texture_Immediate(2, water.noise_texture);

	Set_Stage_Address_2D(backend, 1, RB_TEXTURE_ADDRESS_WRAP);

	// Two output coordinates are used.
	Set_Camera_Space_Texcoord2(backend, 2, 0);
	Set_Stage_Address_2D(backend, 2, RB_TEXTURE_ADDRESS_WRAP);

	Set_Noise_Texture_Transform(backend, 2, water.noise_projection);
}

static void Set_Pixel_Shader_With_Reflection(IRenderBackend & backend, unsigned long pixel_shader)
{
	const float reflectionFactor[4] = { REFLECTION_FACTOR, REFLECTION_FACTOR, REFLECTION_FACTOR, 1.0f };
	backend.Set_Pixel_Shader_Constant(0, &reflectionFactor, 1);
	backend.Set_Pixel_Shader(pixel_shader);
}

//Setup a texture projection in the given stage that applies the shroud.
static void Set_Shroud_Stage(IRenderBackend & backend, const RenderBackendShroud & shroud, unsigned stage)
{
	backend.Override_Terrain_Blend(false);

	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	backend.Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
	backend.Bind_Texture_Immediate(stage, shroud.texture);
	backend.Set_Shroud_Texture_Pass_Active(true, stage);

	backend.Apply_Render_State_Changes();
	backend.Set_Depth_Func(RB_CMP_EQUAL);

	backend.Set_World_Texture_Projection(stage, shroud.projection);
}

static void Reset_Shroud_Stage(IRenderBackend & backend, unsigned stage)
{
	backend.Set_Shroud_Texture_Pass_Active(false, stage);
	backend.Set_Texture(stage,nullptr);
	backend.Set_Depth_Func(RB_CMP_LESS_EQUAL);
	backend.Clear_Texture_Projection(stage);
}

static void Set_Water_Stage_State(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
	const RenderBackendWaterDraw & water)
{
	backend.Set_Texture(0,water.water_texture);
	Set_Water_Material(backend, water);

	//Setup shroud to render in same pass as water
	if (shaders.water_pixel_shader)
	{	if (water.shroud.texture)
		{
			//Use stage 3 to apply the shroud
			Set_Shroud_Stage(backend, water.shroud, 3);
			backend.Bind_Texture_Immediate(3, water.shroud.texture);
			//Shroud shader uses z-compare of EQUAL which wouldn't work on water because it doesn't
			//write to the zbuffer.  Change to LESSEQUAL.
			backend.Set_Depth_Func(RB_CMP_LESS_EQUAL);
		}
		else
		{
			//Assume no shroud, so stage 3 will be null texture but using actual white because
			//pixel shader on GF4 generates random colors with SetTexture(3,nullptr).
			if (!shaders.white_texture->Is_Initialized())
			{	shaders.white_texture->Init();
				Fill_White_Texture(shaders.white_texture);
			}
			backend.Bind_Texture_Immediate(3, shaders.white_texture);
		}
	}

	backend.Set_Texture_Alpha_Operation(0, RB_TEXOP_ADD);
	backend.Set_Texture_Coord_Source(0, RB_TEXCOORD_MESH_UV, 0);
	Set_Stage_Address_2D(backend, 0, RB_TEXTURE_ADDRESS_WRAP);
	backend.Set_Texture_Coord_Source(1, RB_TEXCOORD_MESH_UV, 0);

	if (shaders.water_pixel_shader)
	{
		Set_Noise_Stages(backend, water);
	}
	Set_Linear_Filter(backend, 0);
	Set_Linear_Filter(backend, 1);
	Set_Linear_Filter(backend, 2);
	if (shaders.water_pixel_shader)
	{
		Set_Pixel_Shader_With_Reflection(backend, shaders.water_pixel_shader);
	}
}

static void Set_River_Stage_State(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
	const RenderBackendWaterDraw & water)
{
	Set_Water_Material(backend, water);
	backend.Set_Texture_Alpha_Operation(0, RB_TEXOP_ADD);
	if (!water.edge_texture->Is_Initialized())
		water.edge_texture->Init();
	backend.Bind_Texture_Immediate(3, water.edge_texture);
	Set_Stage_Address_2D(backend, 3, RB_TEXTURE_ADDRESS_WRAP);
	backend.Set_Texture_Coord_Source(0, RB_TEXCOORD_MESH_UV, 0);
	backend.Set_Texture_Coord_Source(1, RB_TEXCOORD_MESH_UV, 0);
	backend.Set_Texture_Coord_Source(3, RB_TEXCOORD_MESH_UV, 1);

	if (shaders.river_pixel_shader)
	{
		Set_Noise_Stages(backend, water);
	}
	Set_Linear_Filter(backend, 0);
	Set_Linear_Filter(backend, 1);
	Set_Linear_Filter(backend, 2);
	Set_Linear_Filter(backend, 3);
	if (shaders.river_pixel_shader)
	{
		Set_Pixel_Shader_With_Reflection(backend, shaders.river_pixel_shader);
	}
}

static void Set_Water_Draw_Shader(IRenderBackend & backend)
{
	ShaderClass waterShader = ShaderClass::_PresetAlphaShader;
	waterShader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	waterShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
	backend.Set_Shader(waterShader);
}

static void Restore_Blend_Factors(IRenderBackend & backend, const RenderBackendWaterDraw & water)
{
	if (!water.additive)
	{
		backend.Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);
	}
	else
	{
		backend.Set_Blend_Factors(RB_BLEND_ONE, RB_BLEND_ONE);
	}
}

static void Draw_Standing_Water(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
	const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count,
	unsigned short min_vertex_index, unsigned short vertex_count)
{
	Set_Water_Stage_State(backend, shaders, water);
	Set_Water_Draw_Shader(backend);
	if (shaders.water_pixel_shader)
	{
		backend.Set_Pixel_Shader(shaders.water_pixel_shader);
	}
	backend.Override_Alpha_Blend_Enable(true);
	backend.Override_Material_Opacity(WATER_MESH_OPACITY);

	// TheSuperHackers @bugfix bobtista 22/06/2026 The shoreline pass authors the
	// back-buffer alpha gradient on the dx8 reference (renderShoreLines), so the
	// soft-water DESTALPHA edge reads a real gradient there just like the original.
	// Keep bgfx on source-alpha water: the dest-alpha mask is heightmap-only and
	// treats mesh rocks as deep water, which paints opaque blue collars around them.
	if (water.soft_edge && !backend.Has_Shader_Pipeline())
	{
		if (water.additive)
		{
			backend.Set_Blend_Factors(RB_BLEND_DEST_ALPHA, RB_BLEND_ONE);
		}
		else
		{
			backend.Set_Blend_Factors(RB_BLEND_DEST_ALPHA, RB_BLEND_INV_DEST_ALPHA);
		}
	}

	CullMode cull = backend.Get_Cull_Mode();
	backend.Set_Cull_Mode(RB_CULL_NONE);

	backend.Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

	if (shaders.water_pixel_shader)
	{
		backend.Set_Pixel_Shader(0);
	}
	//Restore alpha blend to default values since we may have changed them to feather edges.
	Restore_Blend_Factors(backend, water);

	if (water.shroud.texture)
	{
		if (shaders.water_pixel_shader)
		{
			Reset_Shroud_Stage(backend, 3);
			backend.Bind_Texture_Immediate(3, nullptr);
			backend.Set_Depth_Func(RB_CMP_EQUAL);
		}
		else
		{
			backend.Begin_Shroud_Overlay(water.shroud);
			backend.Set_Cull_Mode(RB_CULL_NONE);
			backend.Set_Depth_Func(RB_CMP_LESS_EQUAL);
			backend.Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
			backend.Set_Depth_Func(RB_CMP_EQUAL);
			backend.End_Shroud_Overlay();
		}
	}
	backend.Set_Cull_Mode(cull);
}

static void Draw_River_Water(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
	const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count,
	unsigned short min_vertex_index, unsigned short vertex_count)
{
	backend.Set_Texture(0,water.water_texture);

	Set_River_Stage_State(backend, shaders, water);
	Set_Water_Draw_Shader(backend);
	backend.Override_Alpha_Blend_Enable(true);
	backend.Override_Material_Opacity(WATER_MESH_OPACITY);

	//In additive blending we need to use the alpha at the edges of river to darken
	//rgb instead.
	if (water.additive)
		backend.Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_ONE);

	if (shaders.river_pixel_shader) backend.Set_Pixel_Shader(shaders.river_pixel_shader);
	CullMode cull = backend.Get_Cull_Mode();
	backend.Set_Cull_Mode(RB_CULL_NONE);

	backend.Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);

	if (shaders.river_pixel_shader) backend.Set_Pixel_Shader(0);

	//restore blend mode to what W3D expects.
	// TheSuperHackers @fix bobtista 20/04/2026 The flat water path below
	// resets blend factors for both additive and non-additive modes, but
	// this JBA path only reset for additive. On bgfx the DESTALPHA blend
	// that Override_Material_Opacity() sets then leaked into subsequent
	// draws (e.g. the small faction-emblem quad on the command-center
	// bib), producing a black rectangle there. Match the flat water path
	// so non-additive JBA water restores SRC_ALPHA/INV_SRC_ALPHA.
	if (water.additive)
		backend.Set_Blend_Factors(RB_BLEND_ONE, RB_BLEND_ONE);
	else
		backend.Set_Blend_Factors(RB_BLEND_SRC_ALPHA, RB_BLEND_INV_SRC_ALPHA);

	backend.Set_Cull_Mode(cull);
}

static void Draw_Water_Grid(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
	const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count,
	unsigned short min_vertex_index, unsigned short vertex_count)
{
	Set_Water_Stage_State(backend, shaders, water);

	// TheSuperHackers @bugfix bobtista 17/07/2026 Mark the deforming grid mesh as a water
	// draw like the trapezoid and river paths do. Retail applied WATER_MESH_OPACITY to this
	// very material; on the shader pipeline the override doubles as the routing marker that
	// sends the draw to the water view, so without it the grid rendered in the earlier
	// engine view and the flat trapezoid layer and wakes composited over it.
	backend.Override_Alpha_Blend_Enable(true);
	backend.Override_Material_Opacity(WATER_MESH_OPACITY);

	if (water.shroud.texture && !shaders.water_pixel_shader)
	{
		//we have a shroud to apply and can't do it inside the pixel shader.
		//so do it in stage1
		Set_Shroud_Stage(backend, water.shroud, 1);

		//modulate with shroud texture
		backend.Set_Texture_Color_Argument(1, 1, RB_TEXARG_TEXTURE);	//stage 1 texture
		backend.Set_Texture_Color_Argument(1, 2, RB_TEXARG_CURRENT);	//previous stage texture
		backend.Set_Texture_Color_Operation(1, RB_TEXOP_MODULATE);
		backend.Set_Texture_Alpha_Operation(1, RB_TEXOP_MODULATE);

		//Shroud shader uses z-compare of EQUAL which wouldn't work on water because it doesn't
		//write to the zbuffer.  Change to LESSEQUAL.
		backend.Set_Depth_Func(RB_CMP_LESS_EQUAL);
		backend.Draw_Strip(start_index, polygon_count, min_vertex_index, vertex_count);
		backend.Set_Depth_Func(RB_CMP_EQUAL);
		Reset_Shroud_Stage(backend, 1);
	}
	else
	{
		backend.Draw_Strip(start_index, polygon_count, min_vertex_index, vertex_count);
	}

	if (shaders.water_pixel_shader) backend.Set_Pixel_Shader(0);

	backend.Set_Texture(0,nullptr);
	backend.Set_Texture(1,nullptr);
	ShaderClass::Invalidate();
}

void Draw_Water_Stages(IRenderBackend & backend, const RenderBackendWaterShaders & shaders,
                       const RenderBackendWaterDraw & water, unsigned short start_index, unsigned short polygon_count,
                       unsigned short min_vertex_index, unsigned short vertex_count)
{
	switch (water.surface)
	{
		case RB_WATER_STANDING:
			Draw_Standing_Water(backend, shaders, water, start_index, polygon_count, min_vertex_index, vertex_count);
			break;
		case RB_WATER_RIVER:
			Draw_River_Water(backend, shaders, water, start_index, polygon_count, min_vertex_index, vertex_count);
			break;
		case RB_WATER_GRID:
			Draw_Water_Grid(backend, shaders, water, start_index, polygon_count, min_vertex_index, vertex_count);
			break;
	}
}
