/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "FixedFunctionState.h"

#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"

#include <string.h>

namespace
{
	RenderStateStruct s_renderState;
	unsigned s_changedMask;
	IDirect3DBaseTexture8 * s_rawTextures[MAX_TEXTURE_STAGES];
}

RenderStateStruct & FixedFunctionState::Render_State()
{
	return s_renderState;
}

const RenderStateStruct & FixedFunctionState::Peek_Render_State()
{
	return s_renderState;
}

unsigned & FixedFunctionState::Changed_Mask()
{
	return s_changedMask;
}

void FixedFunctionState::Clear_Raw()
{
	memset(&s_renderState, 0, sizeof(s_renderState));
	memset(s_rawTextures, 0, sizeof(s_rawTextures));
	s_changedMask = 0;
}

void FixedFunctionState::Capture_Render_State(RenderStateStruct & state)
{
	state = s_renderState;
}

void FixedFunctionState::Restore_Render_State(const RenderStateStruct & state)
{
	int i;

	if (s_renderState.index_buffer) {
		s_renderState.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i)
	{
		if (s_renderState.vertex_buffers[i])
		{
			s_renderState.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	s_renderState=state;
	s_changedMask=0xffffffff;

	if (s_renderState.index_buffer) {
		s_renderState.index_buffer->Add_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i)
	{
		if (s_renderState.vertex_buffers[i])
		{
			s_renderState.vertex_buffers[i]->Add_Engine_Ref();
		}
	}
}

void FixedFunctionState::Release_Render_State()
{
	int i;

	if (s_renderState.index_buffer) {
		s_renderState.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		if (s_renderState.vertex_buffers[i]) {
			s_renderState.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(s_renderState.vertex_buffers[i]);
	}
	REF_PTR_RELEASE(s_renderState.index_buffer);
	REF_PTR_RELEASE(s_renderState.material);

	for (i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_RELEASE(s_renderState.Textures[i]);
	}
}

IDirect3DBaseTexture8 * FixedFunctionState::Raw_Texture(unsigned stage)
{
	if (stage >= MAX_TEXTURE_STAGES) {
		return nullptr;
	}

	return s_rawTextures[stage];
}

bool FixedFunctionState::Set_Raw_Texture(unsigned stage, IDirect3DBaseTexture8 * texture)
{
	if (stage >= MAX_TEXTURE_STAGES) {
		return false;
	}

	if (s_rawTextures[stage] == texture) {
		return false;
	}

	if (s_rawTextures[stage]) {
		s_rawTextures[stage]->Release();
	}
	s_rawTextures[stage] = texture;
	if (s_rawTextures[stage]) {
		s_rawTextures[stage]->AddRef();
	}
	return true;
}

void FixedFunctionState::Release_Raw_Textures()
{
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage) {
		if (s_rawTextures[stage]) {
			s_rawTextures[stage]->Release();
			s_rawTextures[stage] = nullptr;
		}
	}
}

RenderStateStruct::RenderStateStruct()
	:
	material(0),
	index_buffer(0)
{
	unsigned i;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) vertex_buffers[i]=0;
	for (i=0;i<MAX_TEXTURE_STAGES;++i) Textures[i]=0;
}

RenderStateStruct::~RenderStateStruct()
{
	unsigned i;
	REF_PTR_RELEASE(material);
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(vertex_buffers[i]);
	}
	REF_PTR_RELEASE(index_buffer);

	for (i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_RELEASE(Textures[i]);
	}
}

RenderStateStruct& RenderStateStruct::operator= (const RenderStateStruct& src)
{
	unsigned i;
	REF_PTR_SET(material,src.material);
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_SET(vertex_buffers[i],src.vertex_buffers[i]);
	}
	REF_PTR_SET(index_buffer,src.index_buffer);

	for (i=0;i<MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_SET(Textures[i],src.Textures[i]);
	}

	LightEnable[0]=src.LightEnable[0];
	LightEnable[1]=src.LightEnable[1];
	LightEnable[2]=src.LightEnable[2];
	LightEnable[3]=src.LightEnable[3];
	if (LightEnable[0]) {
		Lights[0]=src.Lights[0];
		if (LightEnable[1]) {
			Lights[1]=src.Lights[1];
			if (LightEnable[2]) {
				Lights[2]=src.Lights[2];
				if (LightEnable[3]) {
					Lights[3]=src.Lights[3];
				}
			}
		}
	}

	shader=src.shader;
	world=src.world;
	view=src.view;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		vertex_buffer_types[i]=src.vertex_buffer_types[i];
	}
	index_buffer_type=src.index_buffer_type;
	vba_offset=src.vba_offset;
	vba_count=src.vba_count;
	iba_offset=src.iba_offset;
	index_base_offset=src.index_base_offset;

	return *this;
}
