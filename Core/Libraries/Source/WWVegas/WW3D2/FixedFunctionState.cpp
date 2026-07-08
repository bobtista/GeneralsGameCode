/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "FixedFunctionState.h"

#if !defined(GGC_RENDER_BACKEND_BGFX)
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
		bool zBiasValid;
		unsigned zBias;
		bool fillModeValid;
		unsigned fillMode;
		bool shadeModeValid;
		unsigned shadeMode;
		bool depthTestValid;
		bool depthTestEnabled;
		bool depthWriteValid;
		bool depthWriteEnabled;
		bool depthFunctionValid;
		unsigned depthFunction;
		bool fogEnableValid;
		bool fogEnabled;
		bool specularEnableValid;
		bool specularEnabled;
		bool patchSegmentsValid;
		unsigned patchSegments;
		bool normalizeNormalsValid;
		bool normalizeNormalsEnabled;
		bool textureFactorValid;
		unsigned textureFactor;
		bool pointSpriteValid;
		bool pointSpriteEnabled;
		bool pointScaleValid;
		bool pointScaleEnabled;
		bool pointSizeValid;
		unsigned pointSize;
		unsigned pointSizeMin;
		unsigned pointSizeMax;
		bool pointScaleValuesValid;
		unsigned pointScaleA;
		unsigned pointScaleB;
		unsigned pointScaleC;
		bool stencilEnableValid;
		bool stencilEnabled;
		bool stencilFunctionValid;
		unsigned stencilFunction;
		bool stencilReferenceValid;
		unsigned stencilReference;
		bool stencilReadMaskValid;
		unsigned stencilReadMask;
		bool stencilWriteMaskValid;
		unsigned stencilWriteMask;
		bool stencilOpsValid;
		unsigned stencilPassOp;
		unsigned stencilFailOp;
		unsigned stencilZFailOp;
	};

	SemanticRenderState s_semanticRenderState;

	void LegacyMatrixIdentity(LegacyTransformMatrix * dxm)
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
		s_semanticRenderState.zBiasValid = true;
		s_semanticRenderState.zBias = 0;
		s_semanticRenderState.fillModeValid = true;
		s_semanticRenderState.fillMode = 0;
		s_semanticRenderState.shadeModeValid = true;
		s_semanticRenderState.shadeMode = 0;
		s_semanticRenderState.depthTestValid = true;
		s_semanticRenderState.depthTestEnabled = false;
		s_semanticRenderState.depthWriteValid = true;
		s_semanticRenderState.depthWriteEnabled = false;
		s_semanticRenderState.depthFunctionValid = true;
		s_semanticRenderState.depthFunction = 0;
		s_semanticRenderState.fogEnableValid = true;
		s_semanticRenderState.fogEnabled = false;
		s_semanticRenderState.specularEnableValid = true;
		s_semanticRenderState.specularEnabled = false;
		s_semanticRenderState.patchSegmentsValid = true;
		s_semanticRenderState.patchSegments = 0;
		s_semanticRenderState.normalizeNormalsValid = true;
		s_semanticRenderState.normalizeNormalsEnabled = false;
		s_semanticRenderState.textureFactorValid = true;
		s_semanticRenderState.textureFactor = 0;
		s_semanticRenderState.pointSpriteValid = true;
		s_semanticRenderState.pointSpriteEnabled = false;
		s_semanticRenderState.pointScaleValid = true;
		s_semanticRenderState.pointScaleEnabled = false;
		s_semanticRenderState.pointSizeValid = true;
		s_semanticRenderState.pointSize = 0;
		s_semanticRenderState.pointSizeMin = 0;
		s_semanticRenderState.pointSizeMax = 0;
		s_semanticRenderState.pointScaleValuesValid = true;
		s_semanticRenderState.pointScaleA = 0;
		s_semanticRenderState.pointScaleB = 0;
		s_semanticRenderState.pointScaleC = 0;
		s_semanticRenderState.stencilEnableValid = true;
		s_semanticRenderState.stencilEnabled = false;
		s_semanticRenderState.stencilFunctionValid = true;
		s_semanticRenderState.stencilFunction = 0;
		s_semanticRenderState.stencilReferenceValid = true;
		s_semanticRenderState.stencilReference = 0;
		s_semanticRenderState.stencilReadMaskValid = true;
		s_semanticRenderState.stencilReadMask = 0;
		s_semanticRenderState.stencilWriteMaskValid = true;
		s_semanticRenderState.stencilWriteMask = 0;
		s_semanticRenderState.stencilOpsValid = true;
		s_semanticRenderState.stencilPassOp = 0;
		s_semanticRenderState.stencilFailOp = 0;
		s_semanticRenderState.stencilZFailOp = 0;
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
		s_semanticRenderState.zBiasValid = false;
		s_semanticRenderState.zBias = 0;
		s_semanticRenderState.fillModeValid = false;
		s_semanticRenderState.fillMode = 0;
		s_semanticRenderState.shadeModeValid = false;
		s_semanticRenderState.shadeMode = 0;
		s_semanticRenderState.depthTestValid = false;
		s_semanticRenderState.depthTestEnabled = false;
		s_semanticRenderState.depthWriteValid = false;
		s_semanticRenderState.depthWriteEnabled = false;
		s_semanticRenderState.depthFunctionValid = false;
		s_semanticRenderState.depthFunction = 0;
		s_semanticRenderState.fogEnableValid = false;
		s_semanticRenderState.fogEnabled = false;
		s_semanticRenderState.specularEnableValid = false;
		s_semanticRenderState.specularEnabled = false;
		s_semanticRenderState.patchSegmentsValid = false;
		s_semanticRenderState.patchSegments = 0;
		s_semanticRenderState.normalizeNormalsValid = false;
		s_semanticRenderState.normalizeNormalsEnabled = false;
		s_semanticRenderState.textureFactorValid = false;
		s_semanticRenderState.textureFactor = 0;
		s_semanticRenderState.pointSpriteValid = false;
		s_semanticRenderState.pointSpriteEnabled = false;
		s_semanticRenderState.pointScaleValid = false;
		s_semanticRenderState.pointScaleEnabled = false;
		s_semanticRenderState.pointSizeValid = false;
		s_semanticRenderState.pointSize = 0;
		s_semanticRenderState.pointSizeMin = 0;
		s_semanticRenderState.pointSizeMax = 0;
		s_semanticRenderState.pointScaleValuesValid = false;
		s_semanticRenderState.pointScaleA = 0;
		s_semanticRenderState.pointScaleB = 0;
		s_semanticRenderState.pointScaleC = 0;
		s_semanticRenderState.stencilEnableValid = false;
		s_semanticRenderState.stencilEnabled = false;
		s_semanticRenderState.stencilFunctionValid = false;
		s_semanticRenderState.stencilFunction = 0;
		s_semanticRenderState.stencilReferenceValid = false;
		s_semanticRenderState.stencilReference = 0;
		s_semanticRenderState.stencilReadMaskValid = false;
		s_semanticRenderState.stencilReadMask = 0;
		s_semanticRenderState.stencilWriteMaskValid = false;
		s_semanticRenderState.stencilWriteMask = 0;
		s_semanticRenderState.stencilOpsValid = false;
		s_semanticRenderState.stencilPassOp = 0;
		s_semanticRenderState.stencilFailOp = 0;
		s_semanticRenderState.stencilZFailOp = 0;
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
			case RS::ZBIAS:
				s_semanticRenderState.zBiasValid = true;
				s_semanticRenderState.zBias = value;
				break;
			case RS::FILLMODE:
				s_semanticRenderState.fillModeValid = true;
				s_semanticRenderState.fillMode = value;
				break;
			case RS::SHADEMODE:
				s_semanticRenderState.shadeModeValid = true;
				s_semanticRenderState.shadeMode = value;
				break;
			case RS::ZENABLE:
				s_semanticRenderState.depthTestValid = true;
				s_semanticRenderState.depthTestEnabled = (value != 0);
				break;
			case RS::ZWRITEENABLE:
				s_semanticRenderState.depthWriteValid = true;
				s_semanticRenderState.depthWriteEnabled = (value != 0);
				break;
			case RS::ZFUNC:
				s_semanticRenderState.depthFunctionValid = true;
				s_semanticRenderState.depthFunction = value;
				break;
			case RS::FOGENABLE:
				s_semanticRenderState.fogEnableValid = true;
				s_semanticRenderState.fogEnabled = (value != 0);
				break;
			case RS::SPECULARENABLE:
				s_semanticRenderState.specularEnableValid = true;
				s_semanticRenderState.specularEnabled = (value != 0);
				break;
			case RS::PATCHSEGMENTS:
				s_semanticRenderState.patchSegmentsValid = true;
				s_semanticRenderState.patchSegments = value;
				break;
			case RS::NORMALIZENORMALS:
				s_semanticRenderState.normalizeNormalsValid = true;
				s_semanticRenderState.normalizeNormalsEnabled = (value != 0);
				break;
			case RS::TEXTUREFACTOR:
				s_semanticRenderState.textureFactorValid = true;
				s_semanticRenderState.textureFactor = value;
				break;
			case RS::POINTSPRITEENABLE:
				s_semanticRenderState.pointSpriteValid = true;
				s_semanticRenderState.pointSpriteEnabled = (value != 0);
				break;
			case RS::POINTSCALEENABLE:
				s_semanticRenderState.pointScaleValid = true;
				s_semanticRenderState.pointScaleEnabled = (value != 0);
				break;
			case RS::POINTSIZE:
				s_semanticRenderState.pointSizeValid = true;
				s_semanticRenderState.pointSize = value;
				break;
			case RS::POINTSIZEMIN:
				s_semanticRenderState.pointSizeValid = true;
				s_semanticRenderState.pointSizeMin = value;
				break;
			case RS::POINTSIZEMAX:
				s_semanticRenderState.pointSizeValid = true;
				s_semanticRenderState.pointSizeMax = value;
				break;
			case RS::POINTSCALE_A:
				s_semanticRenderState.pointScaleValuesValid = true;
				s_semanticRenderState.pointScaleA = value;
				break;
			case RS::POINTSCALE_B:
				s_semanticRenderState.pointScaleValuesValid = true;
				s_semanticRenderState.pointScaleB = value;
				break;
			case RS::POINTSCALE_C:
				s_semanticRenderState.pointScaleValuesValid = true;
				s_semanticRenderState.pointScaleC = value;
				break;
			case RS::STENCILENABLE:
				s_semanticRenderState.stencilEnableValid = true;
				s_semanticRenderState.stencilEnabled = (value != 0);
				break;
			case RS::STENCILFUNC:
				s_semanticRenderState.stencilFunctionValid = true;
				s_semanticRenderState.stencilFunction = value;
				break;
			case RS::STENCILREF:
				s_semanticRenderState.stencilReferenceValid = true;
				s_semanticRenderState.stencilReference = value;
				break;
			case RS::STENCILMASK:
				s_semanticRenderState.stencilReadMaskValid = true;
				s_semanticRenderState.stencilReadMask = value;
				break;
			case RS::STENCILWRITEMASK:
				s_semanticRenderState.stencilWriteMaskValid = true;
				s_semanticRenderState.stencilWriteMask = value;
				break;
			case RS::STENCILPASS:
				s_semanticRenderState.stencilOpsValid = true;
				s_semanticRenderState.stencilPassOp = value;
				break;
			case RS::STENCILFAIL:
				s_semanticRenderState.stencilOpsValid = true;
				s_semanticRenderState.stencilFailOp = value;
				break;
			case RS::STENCILZFAIL:
				s_semanticRenderState.stencilOpsValid = true;
				s_semanticRenderState.stencilZFailOp = value;
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

	LegacyMatrixIdentity(&s_renderState.world);
	s_changedMask |= WORLD_CHANGED | WORLD_IDENTITY;
}

void FixedFunctionState::Set_View_Identity()
{
	if (s_changedMask & VIEW_IDENTITY) {
		return;
	}

	LegacyMatrixIdentity(&s_renderState.view);
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

#if defined(GGC_RENDER_BACKEND_BGFX)
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
#if defined(GGC_RENDER_BACKEND_BGFX)
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

bool FixedFunctionState::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
	return Set_Cached_Texture_Stage_State(stage, state, value);
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

int FixedFunctionState::Z_Bias(int default_value)
{
	return s_semanticRenderState.zBiasValid ? static_cast<int>(s_semanticRenderState.zBias) : default_value;
}

bool FixedFunctionState::Set_Z_Bias(int value)
{
	return Set_Cached_Render_State(RS::ZBIAS, static_cast<unsigned>(value));
}

unsigned FixedFunctionState::Fill_Mode(unsigned default_value)
{
	return s_semanticRenderState.fillModeValid ? s_semanticRenderState.fillMode : default_value;
}

bool FixedFunctionState::Set_Fill_Mode(unsigned value)
{
	return Set_Cached_Render_State(RS::FILLMODE, value);
}

unsigned FixedFunctionState::Shade_Mode(unsigned default_value)
{
	return s_semanticRenderState.shadeModeValid ? s_semanticRenderState.shadeMode : default_value;
}

bool FixedFunctionState::Set_Shade_Mode(unsigned value)
{
	return Set_Cached_Render_State(RS::SHADEMODE, value);
}

bool FixedFunctionState::Depth_Test_Enabled(bool default_value)
{
	return s_semanticRenderState.depthTestValid ? s_semanticRenderState.depthTestEnabled : default_value;
}

bool FixedFunctionState::Set_Depth_Test_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::ZENABLE, enabled ? 1U : 0U);
}

bool FixedFunctionState::Depth_Write_Enabled(bool default_value)
{
	return s_semanticRenderState.depthWriteValid ? s_semanticRenderState.depthWriteEnabled : default_value;
}

bool FixedFunctionState::Set_Depth_Write_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::ZWRITEENABLE, enabled ? 1U : 0U);
}

unsigned FixedFunctionState::Depth_Function(unsigned default_value)
{
	return s_semanticRenderState.depthFunctionValid ? s_semanticRenderState.depthFunction : default_value;
}

bool FixedFunctionState::Set_Depth_Function(unsigned value)
{
	return Set_Cached_Render_State(RS::ZFUNC, value);
}

bool FixedFunctionState::Fog_Enabled(bool default_value)
{
	return s_semanticRenderState.fogEnableValid ? s_semanticRenderState.fogEnabled : default_value;
}

bool FixedFunctionState::Set_Fog_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::FOGENABLE, enabled ? 1U : 0U);
}

bool FixedFunctionState::Specular_Enabled(bool default_value)
{
	return s_semanticRenderState.specularEnableValid ? s_semanticRenderState.specularEnabled : default_value;
}

bool FixedFunctionState::Set_Specular_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::SPECULARENABLE, enabled ? 1U : 0U);
}

unsigned FixedFunctionState::Patch_Segments_Bits(unsigned default_value)
{
	return s_semanticRenderState.patchSegmentsValid ? s_semanticRenderState.patchSegments : default_value;
}

bool FixedFunctionState::Set_Patch_Segments_Bits(unsigned value)
{
	return Set_Cached_Render_State(RS::PATCHSEGMENTS, value);
}

bool FixedFunctionState::Normalize_Normals_Enabled(bool default_value)
{
	return s_semanticRenderState.normalizeNormalsValid ? s_semanticRenderState.normalizeNormalsEnabled : default_value;
}

bool FixedFunctionState::Set_Normalize_Normals_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::NORMALIZENORMALS, enabled ? 1U : 0U);
}

unsigned FixedFunctionState::Texture_Factor(unsigned default_value)
{
	return s_semanticRenderState.textureFactorValid ? s_semanticRenderState.textureFactor : default_value;
}

bool FixedFunctionState::Set_Texture_Factor(unsigned value)
{
	return Set_Cached_Render_State(RS::TEXTUREFACTOR, value);
}

bool FixedFunctionState::Point_Sprite_Enabled(bool default_value)
{
	return s_semanticRenderState.pointSpriteValid ? s_semanticRenderState.pointSpriteEnabled : default_value;
}

bool FixedFunctionState::Set_Point_Sprite_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::POINTSPRITEENABLE, enabled ? 1U : 0U);
}

bool FixedFunctionState::Point_Scale_Enabled(bool default_value)
{
	return s_semanticRenderState.pointScaleValid ? s_semanticRenderState.pointScaleEnabled : default_value;
}

bool FixedFunctionState::Set_Point_Scale_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::POINTSCALEENABLE, enabled ? 1U : 0U);
}

bool FixedFunctionState::Set_Point_Size_Bits(unsigned size, unsigned min_size, unsigned max_size)
{
	bool changed = false;
	changed |= Set_Cached_Render_State(RS::POINTSIZE, size);
	changed |= Set_Cached_Render_State(RS::POINTSIZEMIN, min_size);
	changed |= Set_Cached_Render_State(RS::POINTSIZEMAX, max_size);
	return changed;
}

bool FixedFunctionState::Set_Point_Scale_Bits(unsigned a, unsigned b, unsigned c)
{
	bool changed = false;
	changed |= Set_Cached_Render_State(RS::POINTSCALE_A, a);
	changed |= Set_Cached_Render_State(RS::POINTSCALE_B, b);
	changed |= Set_Cached_Render_State(RS::POINTSCALE_C, c);
	return changed;
}

bool FixedFunctionState::Stencil_Enabled(bool default_value)
{
	return s_semanticRenderState.stencilEnableValid ? s_semanticRenderState.stencilEnabled : default_value;
}

bool FixedFunctionState::Set_Stencil_Enabled(bool enabled)
{
	return Set_Cached_Render_State(RS::STENCILENABLE, enabled ? 1U : 0U);
}

unsigned FixedFunctionState::Stencil_Function(unsigned default_value)
{
	return s_semanticRenderState.stencilFunctionValid ? s_semanticRenderState.stencilFunction : default_value;
}

bool FixedFunctionState::Set_Stencil_Function(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILFUNC, value);
}

unsigned FixedFunctionState::Stencil_Reference(unsigned default_value)
{
	return s_semanticRenderState.stencilReferenceValid ? s_semanticRenderState.stencilReference : default_value;
}

bool FixedFunctionState::Set_Stencil_Reference(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILREF, value);
}

unsigned FixedFunctionState::Stencil_Read_Mask(unsigned default_value)
{
	return s_semanticRenderState.stencilReadMaskValid ? s_semanticRenderState.stencilReadMask : default_value;
}

bool FixedFunctionState::Set_Stencil_Read_Mask(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILMASK, value);
}

unsigned FixedFunctionState::Stencil_Write_Mask(unsigned default_value)
{
	return s_semanticRenderState.stencilWriteMaskValid ? s_semanticRenderState.stencilWriteMask : default_value;
}

bool FixedFunctionState::Set_Stencil_Write_Mask(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILWRITEMASK, value);
}

bool FixedFunctionState::Set_Stencil_Pass_Op(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILPASS, value);
}

bool FixedFunctionState::Set_Stencil_Fail_Op(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILFAIL, value);
}

bool FixedFunctionState::Set_Stencil_ZFail_Op(unsigned value)
{
	return Set_Cached_Render_State(RS::STENCILZFAIL, value);
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
	sorted_draw_flags(0),
	sorted_array_page(-1),
	sorted_array_layer(-1),
	sorted_array_scale_u(1.0f),
	sorted_array_scale_v(1.0f)
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
	sorted_array_page=src.sorted_array_page;
	sorted_array_layer=src.sorted_array_layer;
	sorted_array_scale_u=src.sorted_array_scale_u;
	sorted_array_scale_v=src.sorted_array_scale_v;

	return *this;
}
