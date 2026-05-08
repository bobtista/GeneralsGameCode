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

class RenderStateCache
{
public:
	enum
	{
		RENDER_STATE_COUNT = 256,
		TEXTURE_STAGE_COUNT = 8,
		TEXTURE_STAGE_STATE_COUNT = 32,
		TRANSFORM_COUNT = D3DTS_WORLD + 1,
		INVALID_STATE_VALUE = 0x12345678
	};

	static void Clear();
	static void Invalidate();

	static unsigned Get_Render_State(unsigned state);
	static bool Set_Render_State(unsigned state, unsigned value);

	static unsigned Get_Texture_Stage_State(unsigned stage, unsigned state);
	static bool Set_Texture_Stage_State(unsigned stage, unsigned state, unsigned value);

	static void Get_Transform(unsigned transform, D3DMATRIX & matrix);
	static bool Set_Transform(unsigned transform, const D3DMATRIX & matrix);

private:
	static unsigned RenderStates[RENDER_STATE_COUNT];
	static unsigned TextureStageStates[TEXTURE_STAGE_COUNT][TEXTURE_STAGE_STATE_COUNT];
	static D3DMATRIX Transforms[TRANSFORM_COUNT];
};
