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

#include "FixedFunctionState.h"
#include "RenderStateDefs.h"

class RenderStateCache
{
public:
	enum
	{
		RENDER_STATE_COUNT = FixedFunctionState::RENDER_STATE_COUNT,
		TEXTURE_STAGE_COUNT = FixedFunctionState::TEXTURE_STAGE_COUNT,
		TEXTURE_STAGE_STATE_COUNT = FixedFunctionState::TEXTURE_STAGE_STATE_COUNT,
		TRANSFORM_COUNT = FixedFunctionState::TRANSFORM_COUNT,
		INVALID_STATE_VALUE = FixedFunctionState::INVALID_STATE_VALUE
	};

	static void Clear();
	static void Invalidate();

	static unsigned Get_Render_State(unsigned state);
	static bool Set_Render_State(unsigned state, unsigned value);

	static unsigned Get_Texture_Stage_State(unsigned stage, unsigned state);
	static bool Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value);

	static void Get_Transform(unsigned transform, LegacyTransformMatrix & matrix);
	static bool Set_Transform(unsigned transform, const LegacyTransformMatrix & matrix);
};
