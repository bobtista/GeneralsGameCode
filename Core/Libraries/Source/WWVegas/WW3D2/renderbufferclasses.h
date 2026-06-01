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
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "always.h"

class DX8VertexBufferClass;

#if defined(GGC_BGFX_STANDALONE)

#include "indexbufferbase.h"
#include "vertexbufferbase.h"

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

class RenderVertexBufferClass : public VertexBufferClass
{
	W3DMPO_CODE(RenderVertexBufferClass)

public:
	enum UsageType {
		USAGE_DEFAULT = 0,
		USAGE_DYNAMIC = 1,
		USAGE_SOFTWAREPROCESSING = 2,
		USAGE_NPATCHES = 4
	};

	RenderVertexBufferClass(unsigned FVF, unsigned short vertex_count, UsageType usage = USAGE_DEFAULT);
	virtual ~RenderVertexBufferClass() override;
};

#else

// TheSuperHackers @build bobtista 01/06/2026 Pull the complete DX8 buffer
// class definitions in here so the aliases below resolve to a complete type
// in every translation unit that includes this header. Forward declarations
// alone leave RenderIndexBufferClass / RenderVertexBufferClass usable only as
// pointer-to-incomplete in code shared with the bgfx backend (USAGE_* enums,
// WriteLockClass nested type, NEW_REF, static_cast to base IndexBufferClass /
// VertexBufferClass all require the full definition).
#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
using RenderIndexBufferClass = DX8IndexBufferClass;
using RenderVertexBufferClass = DX8VertexBufferClass;

#endif

template <typename BufferClass>
constexpr typename BufferClass::UsageType Render_Buffer_Usage_Default()
{
	return BufferClass::USAGE_DEFAULT;
}

template <typename BufferClass>
constexpr typename BufferClass::UsageType Render_Buffer_Usage_Dynamic()
{
	return BufferClass::USAGE_DYNAMIC;
}

// Transitional neutral names for runtime code that only needs WW3D render
// buffers, not raw Direct3D buffer objects. RenderVertexBufferClass and
// RenderIndexBufferClass have standalone bgfx implementations and alias the
// DX8 classes for DX8 reference builds.
