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

// FILE: W3DSnow.h /////////////////////////////////////////////////////////

#pragma once

#include "GameClient/Snow.h"

class RenderInfoClass;
// TheSuperHackers @build bobtista 01/06/2026 RenderIndexBufferClass is a type
// alias on the dx8 backend; include the shared header.
#include "WW3D2/renderbufferclasses.h"
class TextureClass;

class W3DSnowManager : public SnowManager
{
  public :

	W3DSnowManager();
	virtual ~W3DSnowManager() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update () override;
	virtual void updateIniSettings() override;

	void	render(RenderInfoClass &rinfo);
	void	renderAsQuads(RenderInfoClass &rinfo, Int cubeOriginX, Int cubeOriginY, Int cubeDimX, Int cubeDimY);
	void	ReleaseResources();
	Bool	ReAcquireResources();

private:
	RenderIndexBufferClass	*m_indexBuffer;
	TextureClass *m_snowTexture;
	Real m_snowCeiling;	///<height at the top of the cube with camera at center.
	Real m_heightTraveled;	///<height that snow flake traveled this frame.
	Int m_totalRendered;	///<total number of snow particles rendered this frame - only for profiling.
};
