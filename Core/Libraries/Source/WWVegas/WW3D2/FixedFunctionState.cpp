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

#if !defined(GGC_BGFX_STANDALONE)
#include "d3d8.h"
#endif
#include "indexbuffer.h"
#include "RenderStateDefs.h"
#include "vertexbuffer.h"

#include <string.h>

namespace
{
	RenderStateStruct s_renderState;
	unsigned s_changedMask;
	LegacyRawTexture * s_rawTextures[MAX_TEXTURE_STAGES];
	unsigned s_renderStates[FixedFunctionState::RENDER_STATE_COUNT];
	unsigned s_textureStageStates[FixedFunctionState::TEXTURE_STAGE_COUNT][FixedFunctionState::TEXTURE_STAGE_STATE_COUNT];
	LegacyTransformMatrix s_transforms[FixedFunctionState::TRANSFORM_COUNT];

	struct SemanticRenderState
	{
		bool cullModeValid;
		unsigned cullMode;
		bool lightingValid;
		bool lightingEnabled;
		bool fogColorValid;
		unsigned fogColor;
		bool colorWriteMaskValid;
		unsigned colorWriteMask;
		bool ambientColorValid;
		unsigned ambientColor;
		bool ambientMaterialSourceValid;
		unsigned ambientMaterialSource;
		bool diffuseMaterialSourceValid;
		unsigned diffuseMaterialSource;
		bool emissiveMaterialSourceValid;
		unsigned emissiveMaterialSource;
		bool sourceBlendFactorValid;
		unsigned sourceBlendFactor;
		bool destinationBlendFactorValid;
		unsigned destinationBlendFactor;
		bool blendOpValid;
		unsigned blendOp;
		bool alphaBlendValid;
		bool alphaBlendEnabled;
		bool alphaTestValid;
		bool alphaTestEnabled;
		bool alphaTestReferenceValid;
		unsigned alphaTestReference;
		bool alphaTestFunctionValid;
		unsigned alphaTestFunction;
	};

	SemanticRenderState s_semanticRenderState;

	void D3DMatrixIdentity(LegacyTransformMatrix * dxm)
	{
		memset(dxm, 0, sizeof(*dxm));
		dxm->_11 = 1.0f;
		dxm->_22 = 1.0f;
		dxm->_33 = 1.0f;
		dxm->_44 = 1.0f;
	}

	void ClearSemanticRenderState()
	{
		s_semanticRenderState.cullModeValid = true;
		s_semanticRenderState.cullMode = 0;
		s_semanticRenderState.lightingValid = true;
		s_semanticRenderState.lightingEnabled = false;
		s_semanticRenderState.fogColorValid = true;
		s_semanticRenderState.fogColor = 0;
		s_semanticRenderState.colorWriteMaskValid = true;
		s_semanticRenderState.colorWriteMask = 0;
		s_semanticRenderState.ambientColorValid = true;
		s_semanticRenderState.ambientColor = 0;
		s_semanticRenderState.ambientMaterialSourceValid = true;
		s_semanticRenderState.ambientMaterialSource = 0;
		s_semanticRenderState.diffuseMaterialSourceValid = true;
		s_semanticRenderState.diffuseMaterialSource = 0;
		s_semanticRenderState.emissiveMaterialSourceValid = true;
		s_semanticRenderState.emissiveMaterialSource = 0;
		s_semanticRenderState.sourceBlendFactorValid = true;
		s_semanticRenderState.sourceBlendFactor = 0;
		s_semanticRenderState.destinationBlendFactorValid = true;
		s_semanticRenderState.destinationBlendFactor = 0;
		s_semanticRenderState.blendOpValid = true;
		s_semanticRenderState.blendOp = 0;
		s_semanticRenderState.alphaBlendValid = true;
		s_semanticRenderState.alphaBlendEnabled = false;
		s_semanticRenderState.alphaTestValid = true;
		s_semanticRenderState.alphaTestEnabled = false;
		s_semanticRenderState.alphaTestReferenceValid = true;
		s_semanticRenderState.alphaTestReference = 0;
		s_semanticRenderState.alphaTestFunctionValid = true;
		s_semanticRenderState.alphaTestFunction = 0;
	}

	void InvalidateSemanticRenderState()
	{
		s_semanticRenderState.cullModeValid = false;
		s_semanticRenderState.cullMode = 0;
		s_semanticRenderState.lightingValid = false;
		s_semanticRenderState.lightingEnabled = false;
		s_semanticRenderState.fogColorValid = false;
		s_semanticRenderState.fogColor = 0;
		s_semanticRenderState.colorWriteMaskValid = false;
		s_semanticRenderState.colorWriteMask = 0;
		s_semanticRenderState.ambientColorValid = false;
		s_semanticRenderState.ambientColor = 0;
		s_semanticRenderState.ambientMaterialSourceValid = false;
		s_semanticRenderState.ambientMaterialSource = 0;
		s_semanticRenderState.diffuseMaterialSourceValid = false;
		s_semanticRenderState.diffuseMaterialSource = 0;
		s_semanticRenderState.emissiveMaterialSourceValid = false;
		s_semanticRenderState.emissiveMaterialSource = 0;
		s_semanticRenderState.sourceBlendFactorValid = false;
		s_semanticRenderState.sourceBlendFactor = 0;
		s_semanticRenderState.destinationBlendFactorValid = false;
		s_semanticRenderState.destinationBlendFactor = 0;
		s_semanticRenderState.blendOpValid = false;
		s_semanticRenderState.blendOp = 0;
		s_semanticRenderState.alphaBlendValid = false;
		s_semanticRenderState.alphaBlendEnabled = false;
		s_semanticRenderState.alphaTestValid = false;
		s_semanticRenderState.alphaTestEnabled = false;
		s_semanticRenderState.alphaTestReferenceValid = false;
		s_semanticRenderState.alphaTestReference = 0;
		s_semanticRenderState.alphaTestFunctionValid = false;
		s_semanticRenderState.alphaTestFunction = 0;
	}

	void MirrorSemanticRenderState(unsigned state, unsigned value)
	{
		switch (state) {
			case RS::CULLMODE:
				s_semanticRenderState.cullModeValid = true;
				s_semanticRenderState.cullMode = value;
				break;
			case RS::LIGHTING:
				s_semanticRenderState.lightingValid = true;
				s_semanticRenderState.lightingEnabled = (value != 0);
				break;
			case RS::FOGCOLOR:
				s_semanticRenderState.fogColorValid = true;
				s_semanticRenderState.fogColor = value;
				break;
			case RS::COLORWRITEENABLE:
				s_semanticRenderState.colorWriteMaskValid = true;
				s_semanticRenderState.colorWriteMask = value;
				break;
			case RS::AMBIENT:
				s_semanticRenderState.ambientColorValid = true;
				s_semanticRenderState.ambientColor = value;
				break;
			case RS::AMBIENTMATERIALSOURCE:
				s_semanticRenderState.ambientMaterialSourceValid = true;
				s_semanticRenderState.ambientMaterialSource = value;
				break;
			case RS::DIFFUSEMATERIALSOURCE:
				s_semanticRenderState.diffuseMaterialSourceValid = true;
				s_semanticRenderState.diffuseMaterialSource = value;
				break;
			case RS::EMISSIVEMATERIALSOURCE:
				s_semanticRenderState.emissiveMaterialSourceValid = true;
				s_semanticRenderState.emissiveMaterialSource = value;
				break;
			case RS::SRCBLEND:
				s_semanticRenderState.sourceBlendFactorValid = true;
				s_semanticRenderState.sourceBlendFactor = value;
				break;
			case RS::DESTBLEND:
				s_semanticRenderState.destinationBlendFactorValid = true;
				s_semanticRenderState.destinationBlendFactor = value;
				break;
			case RS::BLENDOP:
				s_semanticRenderState.blendOpValid = true;
				s_semanticRenderState.blendOp = value;
				break;
			case RS::ALPHABLENDENABLE:
				s_semanticRenderState.alphaBlendValid = true;
				s_semanticRenderState.alphaBlendEnabled = (value != 0);
				break;
			case RS::ALPHATESTENABLE:
				s_semanticRenderState.alphaTestValid = true;
				s_semanticRenderState.alphaTestEnabled = (value != 0);
				break;
			case RS::ALPHAREF:
				s_semanticRenderState.alphaTestReferenceValid = true;
				s_semanticRenderState.alphaTestReference = value;
				break;
			case RS::ALPHAFUNC:
				s_semanticRenderState.alphaTestFunctionValid = true;
				s_semanticRenderState.alphaTestFunction = value;
				break;
			default:
				break;
		}
	}
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
	Clear_Cached_State();
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

bool FixedFunctionState::Set_Shader(const ShaderClass & shader, bool shader_dirty)
{
	if (!shader_dirty && ((unsigned &)shader == (unsigned &)s_renderState.shader)) {
		return false;
	}

	s_renderState.shader = shader;
	s_changedMask |= SHADER_CHANGED;
	return true;
}

void FixedFunctionState::Set_Material(const VertexMaterialClass * material)
{
	REF_PTR_SET(s_renderState.material, const_cast<VertexMaterialClass *>(material));
	s_changedMask |= MATERIAL_CHANGED;
}

bool FixedFunctionState::Set_Texture(unsigned stage, TextureBaseClass * texture)
{
	if (stage >= MAX_TEXTURE_STAGES) {
		return false;
	}

	if (texture == s_renderState.Textures[stage]) {
		return false;
	}

	REF_PTR_SET(s_renderState.Textures[stage], texture);
	s_changedMask |= (TEXTURE0_CHANGED << stage);
	return true;
}

void FixedFunctionState::Set_Vertex_Buffer(const VertexBufferClass * vertex_buffer, unsigned stream)
{
	s_renderState.vba_offset = 0;
	s_renderState.vba_count = 0;
	if (s_renderState.vertex_buffers[stream]) {
		s_renderState.vertex_buffers[stream]->Release_Engine_Ref();
	}
	REF_PTR_SET(s_renderState.vertex_buffers[stream], const_cast<VertexBufferClass *>(vertex_buffer));
	if (vertex_buffer) {
		vertex_buffer->Add_Engine_Ref();
		s_renderState.vertex_buffer_types[stream] = vertex_buffer->Type();
	} else {
		s_renderState.vertex_buffer_types[stream] = BUFFER_TYPE_INVALID;
	}
	s_changedMask |= VERTEX_BUFFER_CHANGED;
}

void FixedFunctionState::Set_Vertex_Buffer(const DynamicVBAccessClass & vertex_buffer_access)
{
	for (int i = 1; i < MAX_VERTEX_STREAMS; ++i) {
		Set_Vertex_Buffer(nullptr, i);
	}

	if (s_renderState.vertex_buffers[0]) {
		s_renderState.vertex_buffers[0]->Release_Engine_Ref();
	}

	s_renderState.vertex_buffer_types[0] = vertex_buffer_access.Get_Type();
	s_renderState.vba_offset = vertex_buffer_access.Get_Vertex_Buffer_Offset();
	s_renderState.vba_count = vertex_buffer_access.Get_Vertex_Count();
	REF_PTR_SET(s_renderState.vertex_buffers[0], vertex_buffer_access.Get_Vertex_Buffer());
	s_renderState.vertex_buffers[0]->Add_Engine_Ref();
	s_changedMask |= VERTEX_BUFFER_CHANGED;
	s_changedMask |= INDEX_BUFFER_CHANGED;
}

void FixedFunctionState::Set_Index_Buffer(const IndexBufferClass * index_buffer, unsigned short index_base_offset)
{
	s_renderState.iba_offset = 0;
	if (s_renderState.index_buffer) {
		s_renderState.index_buffer->Release_Engine_Ref();
	}
	REF_PTR_SET(s_renderState.index_buffer, const_cast<IndexBufferClass *>(index_buffer));
	s_renderState.index_base_offset = index_base_offset;
	if (index_buffer) {
		index_buffer->Add_Engine_Ref();
		s_renderState.index_buffer_type = index_buffer->Type();
	} else {
		s_renderState.index_buffer_type = BUFFER_TYPE_INVALID;
	}
	s_changedMask |= INDEX_BUFFER_CHANGED;
}

void FixedFunctionState::Set_Index_Buffer(const DynamicIBAccessClass & index_buffer_access, unsigned short index_base_offset)
{
	if (s_renderState.index_buffer) {
		s_renderState.index_buffer->Release_Engine_Ref();
	}

	s_renderState.index_base_offset = index_base_offset;
	s_renderState.index_buffer_type = index_buffer_access.Get_Type();
	s_renderState.iba_offset = index_buffer_access.Get_Index_Buffer_Offset();
	REF_PTR_SET(s_renderState.index_buffer, index_buffer_access.Get_Index_Buffer());
	s_renderState.index_buffer->Add_Engine_Ref();
	s_changedMask |= INDEX_BUFFER_CHANGED;
}

void FixedFunctionState::Set_World_Identity()
{
	if (s_changedMask & WORLD_IDENTITY) {
		return;
	}

	D3DMatrixIdentity(&s_renderState.world);
	s_changedMask |= WORLD_CHANGED | WORLD_IDENTITY;
}

void FixedFunctionState::Set_View_Identity()
{
	if (s_changedMask & VIEW_IDENTITY) {
		return;
	}

	D3DMatrixIdentity(&s_renderState.view);
	s_changedMask |= VIEW_CHANGED | VIEW_IDENTITY;
}

bool FixedFunctionState::Is_World_Identity()
{
	return !!(s_changedMask & WORLD_IDENTITY);
}

bool FixedFunctionState::Is_View_Identity()
{
	return !!(s_changedMask & VIEW_IDENTITY);
}

LegacyRawTexture * FixedFunctionState::Raw_Texture(unsigned stage)
{
	if (stage >= MAX_TEXTURE_STAGES) {
		return nullptr;
	}

	return s_rawTextures[stage];
}

bool FixedFunctionState::Set_Raw_Texture(unsigned stage, LegacyRawTexture * texture)
{
	if (stage >= MAX_TEXTURE_STAGES) {
		return false;
	}

	if (s_rawTextures[stage] == texture) {
		return false;
	}

#if defined(GGC_BGFX_STANDALONE)
	WWASSERT_PRINT(
		s_rawTextures[stage] == nullptr && texture == nullptr,
		"FixedFunctionState::Set_Raw_Texture: standalone bgfx cannot own fake-D3D raw textures");
	s_rawTextures[stage] = nullptr;
#else
	if (s_rawTextures[stage]) {
		s_rawTextures[stage]->Release();
	}
	s_rawTextures[stage] = texture;
	if (s_rawTextures[stage]) {
		s_rawTextures[stage]->AddRef();
	}
#endif
	return true;
}

void FixedFunctionState::Release_Raw_Textures()
{
	for (unsigned stage = 0; stage < MAX_TEXTURE_STAGES; ++stage) {
		if (s_rawTextures[stage]) {
#if defined(GGC_BGFX_STANDALONE)
			WWASSERT_PRINT(
				false,
				"FixedFunctionState::Release_Raw_Textures: standalone bgfx cannot release fake-D3D raw textures");
#else
			s_rawTextures[stage]->Release();
#endif
			s_rawTextures[stage] = nullptr;
		}
	}
}

void FixedFunctionState::Clear_Cached_State()
{
	memset(s_renderStates, 0, sizeof(s_renderStates));
	memset(s_textureStageStates, 0, sizeof(s_textureStageStates));
	memset(s_transforms, 0, sizeof(s_transforms));
	ClearSemanticRenderState();
}

void FixedFunctionState::Invalidate_Cached_State()
{
	unsigned state;
	for (state = 0; state < RENDER_STATE_COUNT; ++state) {
		s_renderStates[state] = INVALID_STATE_VALUE;
	}

	unsigned stage;
	for (stage = 0; stage < TEXTURE_STAGE_COUNT; ++stage) {
		for (state = 0; state < TEXTURE_STAGE_STATE_COUNT; ++state) {
			s_textureStageStates[stage][state] = INVALID_STATE_VALUE;
		}
	}

	memset(s_transforms, 0, sizeof(s_transforms));
	InvalidateSemanticRenderState();
}

unsigned FixedFunctionState::Cached_Render_State(unsigned state)
{
	if (state >= RENDER_STATE_COUNT) {
		return INVALID_STATE_VALUE;
	}

	return s_renderStates[state];
}

bool FixedFunctionState::Set_Cached_Render_State(unsigned state, unsigned value)
{
	if (state >= RENDER_STATE_COUNT) {
		return false;
	}

	if (s_renderStates[state] == value) {
		return false;
	}

	s_renderStates[state] = value;
	MirrorSemanticRenderState(state, value);
	return true;
}

unsigned FixedFunctionState::Cached_Texture_Stage_State(unsigned stage, unsigned state)
{
	if (stage >= TEXTURE_STAGE_COUNT || state >= TEXTURE_STAGE_STATE_COUNT) {
		return INVALID_STATE_VALUE;
	}

	return s_textureStageStates[stage][state];
}

bool FixedFunctionState::Set_Cached_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
	if (stage >= TEXTURE_STAGE_COUNT || state >= TEXTURE_STAGE_STATE_COUNT) {
		return false;
	}

	if (s_textureStageStates[stage][state] == value) {
		return false;
	}

	s_textureStageStates[stage][state] = value;
	return true;
}

void FixedFunctionState::Cached_Transform(unsigned transform, LegacyTransformMatrix & matrix)
{
	if (transform >= TRANSFORM_COUNT) {
		memset(&matrix, 0, sizeof(matrix));
		return;
	}

	matrix = s_transforms[transform];
}

bool FixedFunctionState::Set_Cached_Transform(unsigned transform, const LegacyTransformMatrix & matrix)
{
	if (transform >= TRANSFORM_COUNT) {
		return false;
	}

	s_transforms[transform] = matrix;
	return true;
}

unsigned FixedFunctionState::Cull_Mode(unsigned default_value)
{
	return s_semanticRenderState.cullModeValid ? s_semanticRenderState.cullMode : default_value;
}

bool FixedFunctionState::Set_Cull_Mode(unsigned value)
{
	return Set_Cached_Render_State(RS::CULLMODE, value);
}

bool FixedFunctionState::Lighting_Enabled(bool default_value)
{
	return s_semanticRenderState.lightingValid ? s_semanticRenderState.lightingEnabled : default_value;
}

bool FixedFunctionState::Set_Lighting_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::LIGHTING, enabled ? 1U : 0U);
}

unsigned FixedFunctionState::Fog_Color(unsigned default_value)
{
	return s_semanticRenderState.fogColorValid ? s_semanticRenderState.fogColor : default_value;
}

bool FixedFunctionState::Set_Fog_Color(unsigned value)
{
	return Set_Cached_Render_State(RS::FOGCOLOR, value);
}

unsigned FixedFunctionState::Color_Write_Mask(unsigned default_value)
{
	return s_semanticRenderState.colorWriteMaskValid ? s_semanticRenderState.colorWriteMask : default_value;
}

bool FixedFunctionState::Set_Color_Write_Mask(unsigned value)
{
	return Set_Cached_Render_State(RS::COLORWRITEENABLE, value);
}

unsigned FixedFunctionState::Ambient_Color(unsigned default_value)
{
	return s_semanticRenderState.ambientColorValid ? s_semanticRenderState.ambientColor : default_value;
}

bool FixedFunctionState::Set_Ambient_Color(unsigned value)
{
	return Set_Cached_Render_State(RS::AMBIENT, value);
}

unsigned FixedFunctionState::Ambient_Material_Source(unsigned default_value)
{
	return s_semanticRenderState.ambientMaterialSourceValid ? s_semanticRenderState.ambientMaterialSource : default_value;
}

unsigned FixedFunctionState::Diffuse_Material_Source(unsigned default_value)
{
	return s_semanticRenderState.diffuseMaterialSourceValid ? s_semanticRenderState.diffuseMaterialSource : default_value;
}

unsigned FixedFunctionState::Emissive_Material_Source(unsigned default_value)
{
	return s_semanticRenderState.emissiveMaterialSourceValid ? s_semanticRenderState.emissiveMaterialSource : default_value;
}

bool FixedFunctionState::Set_Material_Color_Sources(unsigned ambient_source, unsigned diffuse_source, unsigned emissive_source)
{
	bool changed = false;
	changed |= Set_Cached_Render_State(RS::AMBIENTMATERIALSOURCE, ambient_source);
	changed |= Set_Cached_Render_State(RS::DIFFUSEMATERIALSOURCE, diffuse_source);
	changed |= Set_Cached_Render_State(RS::EMISSIVEMATERIALSOURCE, emissive_source);
	return changed;
}

unsigned FixedFunctionState::Source_Blend_Factor(unsigned default_value)
{
	return s_semanticRenderState.sourceBlendFactorValid ? s_semanticRenderState.sourceBlendFactor : default_value;
}

unsigned FixedFunctionState::Destination_Blend_Factor(unsigned default_value)
{
	return s_semanticRenderState.destinationBlendFactorValid ? s_semanticRenderState.destinationBlendFactor : default_value;
}

bool FixedFunctionState::Set_Blend_Factors(unsigned source_factor, unsigned destination_factor)
{
	bool changed = false;
	changed |= Set_Cached_Render_State(RS::SRCBLEND, source_factor);
	changed |= Set_Cached_Render_State(RS::DESTBLEND, destination_factor);
	return changed;
}

unsigned FixedFunctionState::Blend_Op(unsigned default_value)
{
	return s_semanticRenderState.blendOpValid ? s_semanticRenderState.blendOp : default_value;
}

bool FixedFunctionState::Set_Blend_Op(unsigned value)
{
	return Set_Cached_Render_State(RS::BLENDOP, value);
}

bool FixedFunctionState::Alpha_Blend_Enabled(bool default_value)
{
	return s_semanticRenderState.alphaBlendValid ? s_semanticRenderState.alphaBlendEnabled : default_value;
}

bool FixedFunctionState::Set_Alpha_Blend_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::ALPHABLENDENABLE, enabled ? 1U : 0U);
}

bool FixedFunctionState::Alpha_Test_Enabled(bool default_value)
{
	return s_semanticRenderState.alphaTestValid ? s_semanticRenderState.alphaTestEnabled : default_value;
}

unsigned FixedFunctionState::Alpha_Test_Reference(unsigned default_value)
{
	return s_semanticRenderState.alphaTestReferenceValid ? s_semanticRenderState.alphaTestReference : default_value;
}

unsigned FixedFunctionState::Alpha_Test_Function(unsigned default_value)
{
	return s_semanticRenderState.alphaTestFunctionValid ? s_semanticRenderState.alphaTestFunction : default_value;
}

bool FixedFunctionState::Set_Alpha_Test_State(bool enabled, unsigned reference, unsigned function)
{
	bool changed = false;
	changed |= Set_Cached_Render_State(RS::ALPHATESTENABLE, enabled ? 1U : 0U);
	changed |= Set_Cached_Render_State(RS::ALPHAREF, reference);
	changed |= Set_Cached_Render_State(RS::ALPHAFUNC, function);
	return changed;
}

void FixedFunctionState::Transform_Matrix(unsigned transform, LegacyTransformMatrix & matrix)
{
	Cached_Transform(transform, matrix);
}

bool FixedFunctionState::Set_Transform_Matrix(unsigned transform, const LegacyTransformMatrix & matrix)
{
	return Set_Cached_Transform(transform, matrix);
}

RenderStateStruct::RenderStateStruct()
	:
	material(0),
	index_buffer(0),
	sorted_draw_flags(0)
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

	for (i=0; i<4; ++i) {
		LightEnable[i]=src.LightEnable[i];
		if (LightEnable[i]) {
			Lights[i]=src.Lights[i];
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
	sorted_draw_flags=src.sorted_draw_flags;

	return *this;
}
