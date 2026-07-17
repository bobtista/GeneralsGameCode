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

// W3DDynamicLight.h
// Class to generate texture for terrain.
// Author: John Ahlquist, April 2001

#pragma once

#include "WW3D2/light.h"
#include "Lib/BaseType.h"
class HeightMapRenderObjClass;

/*************************************************************************
**                             W3DDynamicLight
***************************************************************************/
class W3DDynamicLight : public LightClass
{
friend class BaseHeightMapRenderObjClass;
friend class HeightMapRenderObjClass;
protected:
	/// Values used by HeightMapRenderObjClass to update the height map.
	Bool		m_priorEnable;
	Bool		m_processMe;


	Int			m_prevMinX, m_prevMinY, m_prevMaxX, m_prevMaxY;
	Int			m_minX, m_minY, m_maxX, m_maxY;

	Bool		m_enabled;

	// TheSuperHackers @feature bobtista 23/06/2026 Opt-in: this dynamic light is applied as a
	// dedicated coloured point light in the uber shader (objects + terrain). m_shadowStrength controls
	// how strongly it darkens occluded surfaces: 0 = glow only (no shadow map rendered), >0 = casts a
	// perspective shadow that darkens by that amount.
	Bool		m_castsShadows;
	Real		m_shadowBias;
	Real		m_shadowStrength;
	Real		m_targetShadowStrength;

	Bool		m_decayRange;
	Bool		m_decayColor;
	UnsignedInt m_curDecayFrameCount;
	UnsignedInt m_curIncreaseFrameCount;
	UnsignedInt m_decayFrameCount;
	UnsignedInt m_increaseFrameCount;
	// TheSuperHackers @bugfix bobtista 17/07/2026 Shadow-casting pulses advance their fade once per
	// logic frame instead of once per rendered frame, so the ramp/decay is framerate-independent.
	// Without this a load-settle or high-fps burst plays the whole pulse in a few render frames,
	// flashing the cast shadow on and off.
	UnsignedInt m_lastFadeLogicFrame;
	Real		m_targetRange;
	Vector3 m_targetAmbient;
	Vector3 m_targetDiffuse;


public:
	W3DDynamicLight();
	virtual ~W3DDynamicLight() override;

public:
	virtual void					On_Frame_Update() override;

	void setEnabled(Bool enabled) { m_enabled = enabled; m_decayRange = false; m_decayFrameCount = 0; m_decayColor = false; m_increaseFrameCount = 0;};
	Bool isEnabled() {return m_enabled;};

	void setCastsShadows(Bool b) { m_castsShadows = b; }
	Bool getCastsShadows() const { return m_castsShadows; }
	void setShadowBias(Real b) { m_shadowBias = b; }
	Real getShadowBias() const { return m_shadowBias; }
	void setShadowStrength(Real s) { m_shadowStrength = s; m_targetShadowStrength = s; }
	Real getShadowStrength() const { return m_shadowStrength; }


	/// 0 frameIncreaseTime means it starts out full size/intensity, 0 decay time means it lasts forever.
	void setFrameFade(UnsignedInt frameIncreaseTime, UnsignedInt decayFrameTime);
	void setDecayRange() {m_decayRange = true;};
	void setDecayColor() {m_decayColor = true;};
	// Cull returns true if the terrain vertex at x,y is outside of the light's influence.
	Bool cull(Int x, Int y ) {return (x<m_minX||y<m_minY||x>m_maxX||y>m_maxY);}
};
