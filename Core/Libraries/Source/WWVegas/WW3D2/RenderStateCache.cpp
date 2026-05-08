/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "RenderStateCache.h"

#include <string.h>

unsigned RenderStateCache::RenderStates[RenderStateCache::RENDER_STATE_COUNT];
unsigned RenderStateCache::TextureStageStates[RenderStateCache::TEXTURE_STAGE_COUNT][RenderStateCache::TEXTURE_STAGE_STATE_COUNT];
D3DMATRIX RenderStateCache::Transforms[RenderStateCache::TRANSFORM_COUNT];

void RenderStateCache::Clear()
{
	memset(RenderStates, 0, sizeof(RenderStates));
	memset(TextureStageStates, 0, sizeof(TextureStageStates));
	memset(Transforms, 0, sizeof(Transforms));
}

void RenderStateCache::Invalidate()
{
	unsigned state;
	for (state = 0; state < RENDER_STATE_COUNT; ++state) {
		RenderStates[state] = INVALID_STATE_VALUE;
	}

	unsigned stage;
	for (stage = 0; stage < TEXTURE_STAGE_COUNT; ++stage) {
		for (state = 0; state < TEXTURE_STAGE_STATE_COUNT; ++state) {
			TextureStageStates[stage][state] = INVALID_STATE_VALUE;
		}
	}

	memset(Transforms, 0, sizeof(Transforms));
}

unsigned RenderStateCache::Get_Render_State(unsigned state)
{
	if (state >= RENDER_STATE_COUNT) {
		return INVALID_STATE_VALUE;
	}

	return RenderStates[state];
}

bool RenderStateCache::Set_Render_State(unsigned state, unsigned value)
{
	if (state >= RENDER_STATE_COUNT) {
		return false;
	}

	if (RenderStates[state] == value) {
		return false;
	}

	RenderStates[state] = value;
	return true;
}

unsigned RenderStateCache::Get_Texture_Stage_State(unsigned stage, unsigned state)
{
	if (stage >= TEXTURE_STAGE_COUNT || state >= TEXTURE_STAGE_STATE_COUNT) {
		return INVALID_STATE_VALUE;
	}

	return TextureStageStates[stage][state];
}

bool RenderStateCache::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
	if (stage >= TEXTURE_STAGE_COUNT || state >= TEXTURE_STAGE_STATE_COUNT) {
		return false;
	}

	if (TextureStageStates[stage][state] == value) {
		return false;
	}

	TextureStageStates[stage][state] = value;
	return true;
}

void RenderStateCache::Get_Transform(unsigned transform, D3DMATRIX & matrix)
{
	if (transform >= TRANSFORM_COUNT) {
		memset(&matrix, 0, sizeof(matrix));
		return;
	}

	matrix = Transforms[transform];
}

bool RenderStateCache::Set_Transform(unsigned transform, const D3DMATRIX & matrix)
{
	if (transform >= TRANSFORM_COUNT) {
		return false;
	}

	Transforms[transform] = matrix;
	return true;
}
