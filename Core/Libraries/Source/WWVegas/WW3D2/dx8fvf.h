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
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8fvf.h                               $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/26/02 5:06p                                             $*
 *                                                                                             *
 *                    $Revision:: 7                                                          $*
 *                                                                                             *
 * 06/26/02 KM VB Vertex format update for shaders                                       *
 * 07/17/02 KM VB Vertex format update for displacement mapping                               *
 * 08/01/02 KM VB Vertex format update for cube mapping                               *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#ifdef WWDEBUG
#include "wwdebug.h"
#endif

class StringClass;

static constexpr unsigned DX8_FVF_MAX_TEXCOORD = 8;
static constexpr unsigned DX8_FVF_POSITION_MASK = 0x00e;
static constexpr unsigned DX8_FVF_FLAG_XYZ = 0x002;
static constexpr unsigned DX8_FVF_FLAG_XYZRHW = 0x004;
static constexpr unsigned DX8_FVF_FLAG_XYZB1 = 0x006;
static constexpr unsigned DX8_FVF_FLAG_XYZB2 = 0x008;
static constexpr unsigned DX8_FVF_FLAG_XYZB3 = 0x00a;
static constexpr unsigned DX8_FVF_FLAG_XYZB4 = 0x00c;
static constexpr unsigned DX8_FVF_FLAG_XYZB5 = 0x00e;
static constexpr unsigned DX8_FVF_FLAG_NORMAL = 0x010;
static constexpr unsigned DX8_FVF_FLAG_DIFFUSE = 0x040;
static constexpr unsigned DX8_FVF_FLAG_SPECULAR = 0x080;
static constexpr unsigned DX8_FVF_TEXCOUNT_MASK = 0xf00;
static constexpr unsigned DX8_FVF_TEXCOUNT_SHIFT = 8;
static constexpr unsigned DX8_FVF_TEX0 = 0u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX1 = 1u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX2 = 2u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX3 = 3u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX4 = 4u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX5 = 5u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX6 = 6u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX7 = 7u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_TEX8 = 8u << DX8_FVF_TEXCOUNT_SHIFT;
static constexpr unsigned DX8_FVF_LASTBETA_UBYTE4 = 0x1000;

static constexpr unsigned DX8_FVF_TEXCOORDSIZE1(unsigned coord_index)
{
	return 3u << (coord_index * 2 + 16);
}

static constexpr unsigned DX8_FVF_TEXCOORDSIZE2(unsigned)
{
	return 0u;
}

static constexpr unsigned DX8_FVF_TEXCOORDSIZE3(unsigned coord_index)
{
	return 1u << (coord_index * 2 + 16);
}

static constexpr unsigned DX8_FVF_TEXCOORDSIZE4(unsigned coord_index)
{
	return 2u << (coord_index * 2 + 16);
}

enum {
	DX8_FVF_XYZ				= DX8_FVF_FLAG_XYZ,
	DX8_FVF_XYZN			= DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL,
	DX8_FVF_XYZNUV1		= DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_TEX1,
	DX8_FVF_XYZNUV2		= DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_TEX2,
	DX8_FVF_XYZNDUV1		= DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_TEX1|DX8_FVF_FLAG_DIFFUSE,
	DX8_FVF_XYZNDUV2		= DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_TEX2|DX8_FVF_FLAG_DIFFUSE,
	DX8_FVF_XYZDUV1		= DX8_FVF_FLAG_XYZ|DX8_FVF_TEX1|DX8_FVF_FLAG_DIFFUSE,
	DX8_FVF_XYZDUV2		= DX8_FVF_FLAG_XYZ|DX8_FVF_TEX2|DX8_FVF_FLAG_DIFFUSE,
	DX8_FVF_XYZUV1			= DX8_FVF_FLAG_XYZ|DX8_FVF_TEX1,
	DX8_FVF_XYZUV2			= DX8_FVF_FLAG_XYZ|DX8_FVF_TEX2,
	DX8_FVF_XYZNDUV1TG3	= (DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_FLAG_DIFFUSE|DX8_FVF_TEX4|DX8_FVF_TEXCOORDSIZE2(0)|DX8_FVF_TEXCOORDSIZE3(1)|DX8_FVF_TEXCOORDSIZE3(2)|DX8_FVF_TEXCOORDSIZE3(3)),
	DX8_FVF_XYZNUV2DMAP	= (DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_TEX3 | DX8_FVF_TEXCOORDSIZE1(0) | DX8_FVF_TEXCOORDSIZE4(1) | DX8_FVF_TEXCOORDSIZE2(2) ),
	DX8_FVF_XYZNDCUBEMAP	= DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_FLAG_DIFFUSE
};

static constexpr unsigned RENDER_VERTEX_FORMAT_XYZ = DX8_FVF_XYZ;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZD = DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_DIFFUSE;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZUV1 = DX8_FVF_XYZUV1;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZUV2 = DX8_FVF_XYZUV2;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZDUV1 = DX8_FVF_XYZDUV1;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZDUV2 = DX8_FVF_XYZDUV2;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZN = DX8_FVF_XYZN;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZND = DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_FLAG_DIFFUSE;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZNUV1 = DX8_FVF_XYZNUV1;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZNUV2 = DX8_FVF_XYZNUV2;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZNDUV1 = DX8_FVF_XYZNDUV1;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZNDUV2 = DX8_FVF_XYZNDUV2;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZRHW = DX8_FVF_FLAG_XYZRHW;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZRHWD = DX8_FVF_FLAG_XYZRHW|DX8_FVF_FLAG_DIFFUSE;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZRHWUV1 = DX8_FVF_FLAG_XYZRHW|DX8_FVF_TEX1;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZRHWUV2 = DX8_FVF_FLAG_XYZRHW|DX8_FVF_TEX2;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZRHWDUV1 = DX8_FVF_FLAG_XYZRHW|DX8_FVF_FLAG_DIFFUSE|DX8_FVF_TEX1;
static constexpr unsigned RENDER_VERTEX_FORMAT_XYZRHWDUV2 = DX8_FVF_FLAG_XYZRHW|DX8_FVF_FLAG_DIFFUSE|DX8_FVF_TEX2;

// ----------------------------------------------------------------------------
//
// Util structures for vertex buffer handling. Cast the void pointer returned
// by the vertex buffer to one of these structures.
//
// ----------------------------------------------------------------------------

struct VertexFormatXYZ
{
	float x;
	float y;
	float z;
};

struct VertexFormatXYZNUV1
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u1;
	float v1;
};

struct VertexFormatXYZNUV2
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZN
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
};

struct VertexFormatXYZNDUV1
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
};

struct VertexFormatXYZNDUV2
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZDUV1
{
	float x;
	float y;
	float z;
	unsigned diffuse;
	float u1;
	float v1;
};

struct VertexFormatXYZDUV2
{
	float x;
	float y;
	float z;
	unsigned diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZUV1
{
	float x;
	float y;
	float z;
	float u1;
	float v1;
};

struct VertexFormatXYZUV2
{
	float x;
	float y;
	float z;
	float u1;
	float v1;
	float u2;
	float v2;
};

// todo KJM compress
struct VertexFormatXYZNDUV1TG3
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
	float Sx;
	float Sy;
	float Sz;
	float Tx;
	float Ty;
	float Tz;
	float SxTx;
	float SxTy;
	float SxTz;
};


// displacement mapping format
struct VertexFormatXYZNUV2DMAP
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float T1x;
	float T1y;
	float T1z;
	float T1w;
	float T2x;
	float T2y;
};

// cube map format (texcoords are normally generated)
struct VertexFormatXYZNDCUBEMAP
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
//	float u1;
//	float v1;
//	float w1;
};

// FVF info class can be created for any legal FVF. It constructs information
// of offsets to various elements in the vertex buffer.

class FVFInfoClass
{
	W3DMPO_CODE(FVFInfoClass)

	mutable unsigned						FVF;
	mutable unsigned						fvf_size;

	unsigned							location_offset;
	unsigned							normal_offset;
	unsigned							blend_offset;
	unsigned							texcoord_offset[DX8_FVF_MAX_TEXCOORD];
	unsigned							diffuse_offset;
	unsigned							specular_offset;
public:
	FVFInfoClass(unsigned FVF);

	unsigned Get_Location_Offset() const { return location_offset; }
	unsigned Get_Normal_Offset() const { return normal_offset; }
#ifdef WWDEBUG
	inline unsigned Get_Tex_Offset(unsigned int n) const { WWASSERT(n<DX8_FVF_MAX_TEXCOORD); return texcoord_offset[n]; }
#else
	unsigned Get_Tex_Offset(unsigned int n) const { return texcoord_offset[n]; }
#endif

	unsigned Get_Diffuse_Offset() const { return diffuse_offset; }
	unsigned Get_Specular_Offset() const { return specular_offset; }
	unsigned Get_FVF() const { return FVF; }
	unsigned Get_FVF_Size() const { return fvf_size; }
	unsigned Get_UV_Channel_Count() const;
	bool Has_Normal() const;
	bool Has_Diffuse() const;
	bool Has_Specular() const;

	void Get_FVF_Name(StringClass& fvfname) const;	// For debug purposes
	static unsigned Build_FVF(bool has_normal, bool has_diffuse, bool has_specular, unsigned tex_coord_count);

	// for enabling vertex shaders
	void Set_FVF(unsigned fvf) const { FVF=fvf; }
	void Set_FVF_Size(unsigned size) const { fvf_size=size; }
};
