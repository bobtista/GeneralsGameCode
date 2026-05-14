/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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
		MATERIAL_CHANGED = 1 << 14,
		SHADER_CHANGED = 1 << 15,
		VERTEX_BUFFER_CHANGED = 1 << 16,
		INDEX_BUFFER_CHANGED = 1 << 17,
		WORLD_IDENTITY = 1 << 18,
		VIEW_IDENTITY = 1 << 19,

		TEXTURES_CHANGED =
			TEXTURE0_CHANGED | TEXTURE1_CHANGED | TEXTURE2_CHANGED | TEXTURE3_CHANGED,
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

	static void Cached_Transform(unsigned transform, LegacyTransformMatrix & matrix);
	static bool Set_Cached_Transform(unsigned transform, const LegacyTransformMatrix & matrix);
};
