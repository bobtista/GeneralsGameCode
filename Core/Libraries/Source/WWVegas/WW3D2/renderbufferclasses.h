/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "always.h"

class DX8VertexBufferClass;

#if defined(GGC_BGFX_STANDALONE)

#include "dx8indexbuffer.h"

class RenderIndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(RenderIndexBufferClass)

public:
	enum UsageType {
		USAGE_DEFAULT = 0,
		USAGE_DYNAMIC = 1,
		USAGE_SOFTWAREPROCESSING = 2,
		USAGE_NPATCHES = 4
	};

	RenderIndexBufferClass(unsigned short index_count, UsageType usage = USAGE_DEFAULT);
	virtual ~RenderIndexBufferClass() override;
};

#else

class DX8IndexBufferClass;
using RenderIndexBufferClass = DX8IndexBufferClass;

#endif

// Transitional neutral names for runtime code that only needs WW3D render
// buffers, not raw Direct3D buffer objects. The concrete implementation is
// still DX8VertexBufferClass until the vertex-buffer compatibility class
// split is complete. RenderIndexBufferClass has a standalone bgfx
// implementation and aliases DX8IndexBufferClass for DX8 reference builds.
using RenderVertexBufferClass = DX8VertexBufferClass;
