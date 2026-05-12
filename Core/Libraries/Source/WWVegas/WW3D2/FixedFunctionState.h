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

#include "d3d8.h"
#include "shader.h"
#include "texture.h"
#include "vertmaterial.h"

const unsigned MAX_TEXTURE_STAGES=8;
const unsigned MAX_VERTEX_STREAMS=2;

class IndexBufferClass;
class VertexBufferClass;

struct RenderStateStruct
{
	ShaderClass shader;
	VertexMaterialClass* material;
	TextureBaseClass * Textures[MAX_TEXTURE_STAGES];
	D3DLIGHT8 Lights[4];
	bool LightEnable[4];
	D3DMATRIX world;
	D3DMATRIX view;
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
	static RenderStateStruct & Render_State();
	static const RenderStateStruct & Peek_Render_State();
	static unsigned & Changed_Mask();

	static void Clear_Raw();
	static void Capture_Render_State(RenderStateStruct & state);
	static void Restore_Render_State(const RenderStateStruct & state);
	static void Release_Render_State();
};
