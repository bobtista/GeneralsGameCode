/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#include "fixedfunctionlegacytypes.h"
#include "shader.h"
#include "texture.h"
#include "vertmaterial.h"

const unsigned MAX_TEXTURE_STAGES=8;
const unsigned MAX_VERTEX_STREAMS=2;

class IndexBufferClass;
class VertexBufferClass;
class DynamicIBAccessClass;
class DynamicVBAccessClass;

struct RenderStateStruct
{
	ShaderClass shader;
	VertexMaterialClass* material;
	TextureBaseClass * Textures[MAX_TEXTURE_STAGES];
	LegacyFixedFunctionLight Lights[4];
	bool LightEnable[4];
	LegacyTransformMatrix world;
	LegacyTransformMatrix view;
	unsigned vertex_buffer_types[MAX_VERTEX_STREAMS];
	unsigned index_buffer_type;
	unsigned short vba_offset;
	unsigned short vba_count;
	unsigned short iba_offset;
	VertexBufferClass* vertex_buffers[MAX_VERTEX_STREAMS];
	IndexBufferClass* index_buffer;
	unsigned short index_base_offset;
	unsigned int sorted_draw_flags;

	RenderStateStruct();
	~RenderStateStruct();

	RenderStateStruct& operator= (const RenderStateStruct& src);
};

class FixedFunctionState
{
public:
	enum
	{
		WORLD_CHANGED = 1 << 0,
		VIEW_CHANGED = 1 << 1,
		LIGHT0_CHANGED = 1 << 2,
		LIGHT1_CHANGED = 1 << 3,
		LIGHT2_CHANGED = 1 << 4,
		LIGHT3_CHANGED = 1 << 5,
		TEXTURE0_CHANGED = 1 << 6,
		TEXTURE1_CHANGED = 1 << 7,
		TEXTURE2_CHANGED = 1 << 8,
		TEXTURE3_CHANGED = 1 << 9,
		TEXTURE4_CHANGED = 1 << 10,
		TEXTURE5_CHANGED = 1 << 11,
		TEXTURE6_CHANGED = 1 << 12,
		TEXTURE7_CHANGED = 1 << 13,
		MATERIAL_CHANGED = 1 << 14,
		SHADER_CHANGED = 1 << 15,
		VERTEX_BUFFER_CHANGED = 1 << 16,
		INDEX_BUFFER_CHANGED = 1 << 17,
		WORLD_IDENTITY = 1 << 18,
		VIEW_IDENTITY = 1 << 19,

		TEXTURES_CHANGED =
			TEXTURE0_CHANGED | TEXTURE1_CHANGED | TEXTURE2_CHANGED | TEXTURE3_CHANGED |
			TEXTURE4_CHANGED | TEXTURE5_CHANGED | TEXTURE6_CHANGED | TEXTURE7_CHANGED,
		LIGHTS_CHANGED =
			LIGHT0_CHANGED | LIGHT1_CHANGED | LIGHT2_CHANGED | LIGHT3_CHANGED,

		RENDER_STATE_COUNT = 256,
		TEXTURE_STAGE_COUNT = 8,
		TEXTURE_STAGE_STATE_COUNT = 32,
		TRANSFORM_COUNT = LEGACY_FIXED_FUNCTION_TRANSFORM_COUNT,
		INVALID_STATE_VALUE = 0x12345678
	};

	static RenderStateStruct & Render_State();
	static const RenderStateStruct & Peek_Render_State();
	static unsigned & Changed_Mask();

	static void Clear_Raw();
	static void Capture_Render_State(RenderStateStruct & state);
	static void Restore_Render_State(const RenderStateStruct & state);
	static void Release_Render_State();
	static bool Set_Shader(const ShaderClass & shader, bool shader_dirty = false);
	static void Set_Material(const VertexMaterialClass * material);
	static bool Set_Texture(unsigned stage, TextureBaseClass * texture);
	static void Set_Vertex_Buffer(const VertexBufferClass * vertex_buffer, unsigned stream);
	static void Set_Vertex_Buffer(const DynamicVBAccessClass & vertex_buffer_access);
	static void Set_Index_Buffer(const IndexBufferClass * index_buffer, unsigned short index_base_offset);
	static void Set_Index_Buffer(const DynamicIBAccessClass & index_buffer_access, unsigned short index_base_offset);
	static void Set_World_Identity();
	static void Set_View_Identity();
	static bool Is_World_Identity();
	static bool Is_View_Identity();

	static LegacyRawTexture * Raw_Texture(unsigned stage);
	static bool Set_Raw_Texture(unsigned stage, LegacyRawTexture * texture);
	static void Release_Raw_Textures();

	static void Clear_Cached_State();
	static void Invalidate_Cached_State();

	static unsigned Cached_Render_State(unsigned state);
	static bool Set_Cached_Render_State(unsigned state, unsigned value);

	static unsigned Cached_Texture_Stage_State(unsigned stage, unsigned state);
	static bool Set_Cached_Texture_Stage_State(unsigned stage, unsigned state, unsigned value);
	static bool Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value);

	static void Cached_Transform(unsigned transform, LegacyTransformMatrix & matrix);
	static bool Set_Cached_Transform(unsigned transform, const LegacyTransformMatrix & matrix);

	static unsigned Cull_Mode(unsigned default_value);
	static bool Set_Cull_Mode(unsigned value);
	static bool Lighting_Enabled(bool default_value);
	static bool Set_Lighting_Enabled(bool enabled);
	static unsigned Fog_Color(unsigned default_value);
	static bool Set_Fog_Color(unsigned value);
	static unsigned Color_Write_Mask(unsigned default_value);
	static bool Set_Color_Write_Mask(unsigned value);
	static unsigned Ambient_Color(unsigned default_value);
	static bool Set_Ambient_Color(unsigned value);
	static unsigned Ambient_Material_Source(unsigned default_value);
	static unsigned Diffuse_Material_Source(unsigned default_value);
	static unsigned Emissive_Material_Source(unsigned default_value);
	static bool Set_Material_Color_Sources(unsigned ambient_source, unsigned diffuse_source, unsigned emissive_source);
	static unsigned Source_Blend_Factor(unsigned default_value);
	static unsigned Destination_Blend_Factor(unsigned default_value);
	static bool Set_Blend_Factors(unsigned source_factor, unsigned destination_factor);
	static unsigned Blend_Op(unsigned default_value);
	static bool Set_Blend_Op(unsigned value);
	static bool Alpha_Blend_Enabled(bool default_value);
	static bool Set_Alpha_Blend_Enabled(bool enabled);
	static bool Alpha_Test_Enabled(bool default_value);
	static unsigned Alpha_Test_Reference(unsigned default_value);
	static unsigned Alpha_Test_Function(unsigned default_value);
	static bool Set_Alpha_Test_State(bool enabled, unsigned reference, unsigned function);
	static int Z_Bias(int default_value);
	static bool Set_Z_Bias(int value);
	static unsigned Fill_Mode(unsigned default_value);
	static bool Set_Fill_Mode(unsigned value);
	static unsigned Shade_Mode(unsigned default_value);
	static bool Set_Shade_Mode(unsigned value);
	static bool Depth_Test_Enabled(bool default_value);
	static bool Set_Depth_Test_Enabled(bool enabled);
	static bool Depth_Write_Enabled(bool default_value);
	static bool Set_Depth_Write_Enabled(bool enabled);
	static unsigned Depth_Function(unsigned default_value);
	static bool Set_Depth_Function(unsigned value);
	static bool Fog_Enabled(bool default_value);
	static bool Set_Fog_Enabled(bool enabled);
	static bool Specular_Enabled(bool default_value);
	static bool Set_Specular_Enabled(bool enabled);
	static unsigned Patch_Segments_Bits(unsigned default_value);
	static bool Set_Patch_Segments_Bits(unsigned value);
	static bool Normalize_Normals_Enabled(bool default_value);
	static bool Set_Normalize_Normals_Enabled(bool enabled);
	static unsigned Texture_Factor(unsigned default_value);
	static bool Set_Texture_Factor(unsigned value);
	static bool Point_Sprite_Enabled(bool default_value);
	static bool Set_Point_Sprite_Enabled(bool enabled);
	static bool Point_Scale_Enabled(bool default_value);
	static bool Set_Point_Scale_Enabled(bool enabled);
	static bool Set_Point_Size_Bits(unsigned size, unsigned min_size, unsigned max_size);
	static bool Set_Point_Scale_Bits(unsigned a, unsigned b, unsigned c);
	static bool Stencil_Enabled(bool default_value);
	static bool Set_Stencil_Enabled(bool enabled);
	static unsigned Stencil_Function(unsigned default_value);
	static bool Set_Stencil_Function(unsigned value);
	static unsigned Stencil_Reference(unsigned default_value);
	static bool Set_Stencil_Reference(unsigned value);
	static unsigned Stencil_Read_Mask(unsigned default_value);
	static bool Set_Stencil_Read_Mask(unsigned value);
	static unsigned Stencil_Write_Mask(unsigned default_value);
	static bool Set_Stencil_Write_Mask(unsigned value);
	static bool Set_Stencil_Pass_Op(unsigned value);
	static bool Set_Stencil_Fail_Op(unsigned value);
	static bool Set_Stencil_ZFail_Op(unsigned value);
	static void Transform_Matrix(unsigned transform, LegacyTransformMatrix & matrix);
	static bool Set_Transform_Matrix(unsigned transform, const LegacyTransformMatrix & matrix);
};
