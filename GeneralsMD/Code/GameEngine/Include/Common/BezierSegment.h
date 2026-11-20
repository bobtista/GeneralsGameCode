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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// BezierSegment.h ////////////////////////////////////////////////////////////////////////////////
// John K McDonald, Jr.
// September 2002
// DO NOT DISTRIBUTE

#pragma once

#ifdef _WIN32
#include <d3dx8math.h>
#else
// D3DX is not available here. These mirror the D3DX8 types and functions the Bezier code uses,
// evaluating in the same order as the D3DX8 reference formulas.
struct D3DXVECTOR4
{
	D3DXVECTOR4() {}
	D3DXVECTOR4(float fx, float fy, float fz, float fw) : x(fx), y(fy), z(fz), w(fw) {}
	float x, y, z, w;
};

struct D3DXMATRIX
{
	D3DXMATRIX() {}
	D3DXMATRIX(float f11, float f12, float f13, float f14,
		float f21, float f22, float f23, float f24,
		float f31, float f32, float f33, float f34,
		float f41, float f42, float f43, float f44)
		: _11(f11), _12(f12), _13(f13), _14(f14)
		, _21(f21), _22(f22), _23(f23), _24(f24)
		, _31(f31), _32(f32), _33(f33), _34(f34)
		, _41(f41), _42(f42), _43(f43), _44(f44) {}
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;
	float _41, _42, _43, _44;
};

inline D3DXVECTOR4* D3DXVec4Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM)
{
	D3DXVECTOR4 v;
	v.x = (pV->x * pM->_11 + pV->y * pM->_21) + (pV->z * pM->_31 + pV->w * pM->_41);
	v.y = (pV->x * pM->_12 + pV->y * pM->_22) + (pV->z * pM->_32 + pV->w * pM->_42);
	v.z = (pV->x * pM->_13 + pV->y * pM->_23) + (pV->z * pM->_33 + pV->w * pM->_43);
	v.w = (pV->x * pM->_14 + pV->y * pM->_24) + (pV->z * pM->_34 + pV->w * pM->_44);
	*pOut = v;
	return pOut;
}

inline float D3DXVec4Dot(const D3DXVECTOR4 *pV1, const D3DXVECTOR4 *pV2)
{
	return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z + pV1->w * pV2->w;
}
#endif
#include "Common/STLTypedefs.h"

#define USUAL_TOLERANCE 1.0f

class BezierSegment
{
	protected:
		static const D3DXMATRIX s_bezBasisMatrix;
		Coord3D m_controlPoints[4];

	public:	// Constructors
		BezierSegment();
		BezierSegment(Real x0, Real y0, Real z0,
									Real x1, Real y1, Real z1,
									Real x2, Real y2, Real z2,
									Real x3, Real y3, Real z3);

		BezierSegment(Real cp[12]);


		BezierSegment(const Coord3D& cp0,
									const Coord3D& cp1,
									const Coord3D& cp2,
									const Coord3D& cp3);

		BezierSegment(Coord3D cp[4]);

	public:
		void evaluateBezSegmentAtT(Real tValue, Coord3D *outResult) const;
		void getSegmentPoints(Int numSegments, VecCoord3D *outResult) const;
		Real getApproximateLength(Real withinTolerance = USUAL_TOLERANCE) const;

		void splitSegmentAtT(Real tValue, BezierSegment &outSeg1, BezierSegment &outSeg2) const;

	public:	// He get's friendly access.
		friend class BezFwdIterator;
};
