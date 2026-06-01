/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "RenderStateCache.h"

void RenderStateCache::Clear()
{
	FixedFunctionState::Clear_Cached_State();
}

void RenderStateCache::Invalidate()
{
	FixedFunctionState::Invalidate_Cached_State();
}

unsigned RenderStateCache::Get_Render_State(unsigned state)
{
	return FixedFunctionState::Cached_Render_State(state);
}

bool RenderStateCache::Set_Render_State(unsigned state, unsigned value)
{
	return FixedFunctionState::Set_Cached_Render_State(state, value);
}

unsigned RenderStateCache::Get_Texture_Stage_State(unsigned stage, unsigned state)
{
	return FixedFunctionState::Cached_Texture_Stage_State(stage, state);
}

bool RenderStateCache::Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value)
{
	return FixedFunctionState::Set_Cached_Texture_Stage_State(stage, state, value);
}

void RenderStateCache::Get_Transform(unsigned transform, LegacyTransformMatrix & matrix)
{
	FixedFunctionState::Cached_Transform(transform, matrix);
}

bool RenderStateCache::Set_Transform(unsigned transform, const LegacyTransformMatrix & matrix)
{
	return FixedFunctionState::Set_Cached_Transform(transform, matrix);
}
