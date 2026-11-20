/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#ifdef EXTENDED_STATS

struct RenderDebugStats
{
	RenderDebugStats() :
		m_showingStats(false),
		m_disableTerrain(false),
		m_disableWater(false),
		m_disableObjects(false),
		m_disableOverhead(false),
		m_disableConsole(false),
		m_debugLinesToShow(-1),
		m_sleepTime(0)
	{
	}

	bool m_showingStats;
	bool m_disableTerrain;
	bool m_disableWater;
	bool m_disableObjects;
	bool m_disableOverhead;
	bool m_disableConsole;
	int  m_debugLinesToShow;
	int  m_sleepTime;
};

extern RenderDebugStats g_renderDebugStats;

#endif
