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

#include "dx8fvf.h"
#include "wwstring.h"

static unsigned Get_FVF_Texcoord_Component_Count(unsigned FVF, unsigned coord_index)
{
	const unsigned format = (FVF >> (coord_index * 2 + 16)) & 0x3;

	switch (format) {
	case 3:
		return 1;
	case 0:
		return 2;
	case 1:
		return 3;
	case 2:
		return 4;
	default:
		return 2;
	}
}

static unsigned Get_FVF_Position_Size(unsigned FVF)
{
	switch (FVF & DX8_FVF_POSITION_MASK) {
	case DX8_FVF_FLAG_XYZ:
		return 3 * sizeof(float);
	case DX8_FVF_FLAG_XYZRHW:
		return 4 * sizeof(float);
	case DX8_FVF_FLAG_XYZB1:
		return 4 * sizeof(float);
	case DX8_FVF_FLAG_XYZB2:
		return 5 * sizeof(float);
	case DX8_FVF_FLAG_XYZB3:
		return 6 * sizeof(float);
	case DX8_FVF_FLAG_XYZB4:
		return ((FVF & DX8_FVF_LASTBETA_UBYTE4) == DX8_FVF_LASTBETA_UBYTE4)
			? 6 * sizeof(float) + sizeof(unsigned)
			: 7 * sizeof(float);
	case DX8_FVF_FLAG_XYZB5:
		return 8 * sizeof(float);
	default:
		return 0;
	}
}

static unsigned Get_FVF_Vertex_Size(unsigned FVF)
{
	unsigned size = Get_FVF_Position_Size(FVF);

	if ((FVF & DX8_FVF_FLAG_NORMAL) == DX8_FVF_FLAG_NORMAL) {
		size += 3 * sizeof(float);
	}
	if ((FVF & DX8_FVF_FLAG_DIFFUSE) == DX8_FVF_FLAG_DIFFUSE) {
		size += sizeof(unsigned);
	}
	if ((FVF & DX8_FVF_FLAG_SPECULAR) == DX8_FVF_FLAG_SPECULAR) {
		size += sizeof(unsigned);
	}

	const unsigned tex_count = (FVF & DX8_FVF_TEXCOUNT_MASK) >> DX8_FVF_TEXCOUNT_SHIFT;
	for (unsigned i = 0; i < tex_count && i < DX8_FVF_MAX_TEXCOORD; ++i) {
		size += Get_FVF_Texcoord_Component_Count(FVF, i) * sizeof(float);
	}

	return size;
}

FVFInfoClass::FVFInfoClass(unsigned FVF_)
	:
	FVF(FVF_),
	fvf_size(Get_FVF_Vertex_Size(FVF))
{
	location_offset=0;
	blend_offset=location_offset;

	if ((FVF&DX8_FVF_FLAG_XYZ)==DX8_FVF_FLAG_XYZ) blend_offset+=3*sizeof(float);
	normal_offset=blend_offset;

	if ( ((FVF&DX8_FVF_FLAG_XYZB4)==DX8_FVF_FLAG_XYZB4) &&
		  ((FVF&DX8_FVF_LASTBETA_UBYTE4)==DX8_FVF_LASTBETA_UBYTE4) ) normal_offset+=3*sizeof(float)+sizeof(unsigned);
	diffuse_offset=normal_offset;

	if ((FVF&DX8_FVF_FLAG_NORMAL)==DX8_FVF_FLAG_NORMAL) diffuse_offset+=3*sizeof(float);
	specular_offset=diffuse_offset;

	if ((FVF&DX8_FVF_FLAG_DIFFUSE)==DX8_FVF_FLAG_DIFFUSE) specular_offset+=sizeof(unsigned);
	texcoord_offset[0]=specular_offset;

	if ((FVF&DX8_FVF_FLAG_SPECULAR)==DX8_FVF_FLAG_SPECULAR) texcoord_offset[0]+=sizeof(unsigned);

	for (unsigned int i=1; i<DX8_FVF_MAX_TEXCOORD; i++)
	{
		texcoord_offset[i]=texcoord_offset[i-1];
		texcoord_offset[i]+=Get_FVF_Texcoord_Component_Count(FVF, i - 1) * sizeof(float);
	}
}

unsigned FVFInfoClass::Get_UV_Channel_Count() const
{
	return (FVF & DX8_FVF_TEXCOUNT_MASK) >> DX8_FVF_TEXCOUNT_SHIFT;
}

bool FVFInfoClass::Has_Normal() const
{
	return (FVF & DX8_FVF_FLAG_NORMAL) == DX8_FVF_FLAG_NORMAL;
}

bool FVFInfoClass::Has_Diffuse() const
{
	return (FVF & DX8_FVF_FLAG_DIFFUSE) == DX8_FVF_FLAG_DIFFUSE;
}

bool FVFInfoClass::Has_Specular() const
{
	return (FVF & DX8_FVF_FLAG_SPECULAR) == DX8_FVF_FLAG_SPECULAR;
}

unsigned FVFInfoClass::Build_FVF(bool has_normal, bool has_diffuse, bool has_specular, unsigned tex_coord_count)
{
	unsigned fvf = DX8_FVF_FLAG_XYZ;

	if (has_normal) {
		fvf |= DX8_FVF_FLAG_NORMAL;
	}
	if (has_diffuse) {
		fvf |= DX8_FVF_FLAG_DIFFUSE;
	}
	if (has_specular) {
		fvf |= DX8_FVF_FLAG_SPECULAR;
	}

	if (tex_coord_count <= DX8_FVF_MAX_TEXCOORD) {
		fvf |= tex_coord_count << DX8_FVF_TEXCOUNT_SHIFT;
	}

	return fvf;
}

void FVFInfoClass::Get_FVF_Name(StringClass& fvfname) const
{
	switch (Get_FVF()) {
	case DX8_FVF_XYZ: fvfname="FVF_XYZ"; break;
	case DX8_FVF_XYZN: fvfname="FVF_XYZ|NORMAL"; break;
	case DX8_FVF_XYZNUV1: fvfname="FVF_XYZ|NORMAL|TEX1"; break;
	case DX8_FVF_XYZNUV2: fvfname="FVF_XYZ|NORMAL|TEX2"; break;
	case DX8_FVF_XYZNDUV1: fvfname="FVF_XYZ|NORMAL|TEX1|DIFFUSE"; break;
	case DX8_FVF_XYZNDUV2: fvfname="FVF_XYZ|NORMAL|TEX2|DIFFUSE"; break;
	case DX8_FVF_XYZDUV1: fvfname="FVF_XYZ|TEX1|DIFFUSE"; break;
	case DX8_FVF_XYZDUV2: fvfname="FVF_XYZ|TEX2|DIFFUSE"; break;
	case DX8_FVF_XYZUV1: fvfname="FVF_XYZ|TEX1"; break;
	case DX8_FVF_XYZUV2: fvfname="FVF_XYZ|TEX2"; break;
	case DX8_FVF_XYZNDUV1TG3 : fvfname="FVF_XYZ|NORMAL|DIFFUSE|TEX4|TEXCOORDSIZE2(0)|TEXCOORDSIZE3(1)|TEXCOORDSIZE3(2)|TEXCOORDSIZE3(3)"; break;
	case DX8_FVF_XYZNUV2DMAP :	fvfname="FVF_XYZ|NORMAL|TEX3|TEXCOORDSIZE1(0)|TEXCOORDSIZE4(1)|TEXCOORDSIZE2(2)"; break;
	case DX8_FVF_XYZNDCUBEMAP : fvfname="FVF_XYZ|NORMAL|DIFFUSE"; break;
	default: fvfname="Unknown!";
	}
}
