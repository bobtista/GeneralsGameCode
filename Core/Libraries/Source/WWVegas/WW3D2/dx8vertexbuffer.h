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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8vertexbuffer.h                      $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/26/02 5:06p                                             $*
 *                                                                                             *
 *                    $Revision:: 26                                                          $*
 *                                                                                             *
 * 06/26/02 KM VB Vertex format size update for shaders                                       *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#include "wwdebug.h"
#include "dx8fvf.h"
#include "vertexbuffer.h"

class DX8Wrapper;
class SortingRendererClass;
class Vector2;
class Vector3;
class Vector4;
class StringClass;
class FVFInfoClass;

#if !defined(GGC_BGFX_STANDALONE)
class DX8VertexBufferClass;

/**
** DX8VertexBufferClass
** This class wraps a DX8 vertex buffer.  Use the lock objects to modify or append to the vertex buffer.
*/
class DX8VertexBufferClass : public VertexBufferClass
{
	W3DMPO_CODE(DX8VertexBufferClass)
protected:
	virtual ~DX8VertexBufferClass() override;
public:
	enum UsageType {
		USAGE_DEFAULT=0,
		USAGE_DYNAMIC=1,
		USAGE_SOFTWAREPROCESSING=2,
		USAGE_NPATCHES=4
	};

	DX8VertexBufferClass(unsigned FVF, unsigned short VertexCount, UsageType usage=USAGE_DEFAULT);
	DX8VertexBufferClass(const Vector3* vertices, const Vector3* normals, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);
	DX8VertexBufferClass(const Vector3* vertices, const Vector3* normals, const Vector4* diffuse, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);
	DX8VertexBufferClass(const Vector3* vertices, const Vector4* diffuse, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);
	DX8VertexBufferClass(const Vector3* vertices, const Vector2* tex_coords, unsigned short VertexCount,UsageType usage=USAGE_DEFAULT);

#if !defined(GGC_BGFX_STANDALONE)
	void *Get_Legacy_Vertex_Buffer() { return VertexBuffer; }
#endif

	void Copy(const Vector3* loc, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector2* uv, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector3* norm, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector3* norm, const Vector2* uv, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector3* norm, const Vector2* uv, const Vector4* diffuse, unsigned first_vertex, unsigned count);
	void Copy(const Vector3* loc, const Vector2* uv, const Vector4* diffuse, unsigned first_vertex, unsigned count);

protected:
	void *VertexBuffer;

	void Create_Vertex_Buffer(UsageType usage);
};
#endif
