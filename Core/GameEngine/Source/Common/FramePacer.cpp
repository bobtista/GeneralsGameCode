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
#include "PreRTS.h"

#include "Common/FramePacer.h"

#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#include "GameNetwork/NetworkDefs.h"
#include "GameNetwork/NetworkInterface.h"


FramePacer* TheFramePacer = nullptr;

FramePacer::FramePacer()
{
	// Set the time slice size to 1 ms.
	timeBeginPeriod(1);

	m_maxFPS = BaseFps;
	m_logicTimeScaleFPS = LOGICFRAMES_PER_SECOND;
	m_updateTime = 1.0f / (Real)BaseFps; // initialized to something to avoid division by zero on first use
	m_enableFpsLimit = FALSE;
	m_enableLogicTimeScale = FALSE;
	m_isTimeFrozen = FALSE;
	m_isGameHalted = FALSE;
	m_enablePerformanceLog = FALSE;
	m_performanceLogFrameCount = 0;
	m_performanceLogWindowFrames = 0;
	m_performanceLogElapsedSeconds = 0.0f;
	m_performanceLogWindowSeconds = 0.0f;
	m_performanceLogWindowMinMs = 0.0f;
	m_performanceLogWindowMaxMs = 0.0f;
}

FramePacer::~FramePacer()
{
	flushPerformanceLogWindow();

	// Restore the previous time slice for Windows.
	timeEndPeriod(1);
}

void FramePacer::update()
{
	// TheSuperHackers @bugfix xezon 05/08/2025 Re-implements the frame rate limiter
	// with higher resolution counters to cap the frame rate more accurately to the desired limit.
	const UnsignedInt maxFps = getActualFramesPerSecondLimit();// allowFpsLimit ? getFramesPerSecondLimit() : RenderFpsPreset::UncappedFpsValue;
	m_updateTime = m_frameRateLimit.wait(maxFps);
	updatePerformanceLog();
}

void FramePacer::enablePerformanceLog(Bool enable)
{
	m_enablePerformanceLog = enable;
	m_performanceLogFrameCount = 0;
	m_performanceLogWindowFrames = 0;
	m_performanceLogElapsedSeconds = 0.0f;
	m_performanceLogWindowSeconds = 0.0f;
	m_performanceLogWindowMinMs = 0.0f;
	m_performanceLogWindowMaxMs = 0.0f;

	if (m_enablePerformanceLog)
	{
		FILE *file = fopen("PerfLog_FrameTimes.csv", "wt");
		if (file != nullptr)
		{
			fprintf(file, "elapsed_seconds,total_frames,window_frames,avg_ms,min_ms,max_ms,fps\n");
			fclose(file);
		}
		else
		{
			DEBUG_LOG(("FramePacer::enablePerformanceLog() - failed to open PerfLog_FrameTimes.csv"));
		}
	}
}

void FramePacer::flushPerformanceLogWindow()
{
	if (!m_enablePerformanceLog || m_performanceLogWindowFrames == 0)
	{
		return;
	}

	FILE *file = fopen("PerfLog_FrameTimes.csv", "at");
	if (file != nullptr)
	{
		const Real avgMs = (m_performanceLogWindowSeconds * 1000.0f) / (Real)m_performanceLogWindowFrames;
		const Real fps = (Real)m_performanceLogWindowFrames / m_performanceLogWindowSeconds;
		fprintf(file, "%.3f,%u,%u,%.3f,%.3f,%.3f,%.3f\n",
			m_performanceLogElapsedSeconds,
			m_performanceLogFrameCount,
			m_performanceLogWindowFrames,
			avgMs,
			m_performanceLogWindowMinMs,
			m_performanceLogWindowMaxMs,
			fps);
		fclose(file);
	}

	m_performanceLogWindowFrames = 0;
	m_performanceLogWindowSeconds = 0.0f;
	m_performanceLogWindowMinMs = 0.0f;
	m_performanceLogWindowMaxMs = 0.0f;
}

void FramePacer::updatePerformanceLog()
{
	if (!m_enablePerformanceLog)
	{
		return;
	}

	const Real frameMs = m_updateTime * 1000.0f;
	++m_performanceLogFrameCount;
	++m_performanceLogWindowFrames;
	m_performanceLogElapsedSeconds += m_updateTime;
	m_performanceLogWindowSeconds += m_updateTime;

	if (m_performanceLogWindowFrames == 1)
	{
		m_performanceLogWindowMinMs = frameMs;
		m_performanceLogWindowMaxMs = frameMs;
	}
	else
	{
		m_performanceLogWindowMinMs = min(m_performanceLogWindowMinMs, frameMs);
		m_performanceLogWindowMaxMs = max(m_performanceLogWindowMaxMs, frameMs);
	}

	if (m_performanceLogWindowSeconds >= 1.0f)
	{
		flushPerformanceLogWindow();
	}
}

void FramePacer::setFramesPerSecondLimit( Int fps )
{
	DEBUG_LOG(("FramePacer::setFramesPerSecondLimit() - setting max fps to %d (TheGlobalData->m_useFpsLimit == %d)", fps, TheGlobalData->m_useFpsLimit));
	m_maxFPS = fps;
}

Int FramePacer::getFramesPerSecondLimit()  const
{
	return m_maxFPS;
}

void FramePacer::enableFramesPerSecondLimit( Bool enable )
{
	m_enableFpsLimit = enable;
}

Bool FramePacer::isFramesPerSecondLimitEnabled() const
{
	return m_enableFpsLimit;
}

Bool FramePacer::isActualFramesPerSecondLimitEnabled() const
{
	Bool allowFpsLimit = true;

	if (TheTacticalView != nullptr)
	{
		allowFpsLimit &= TheTacticalView->getTimeMultiplier()<=1 && !TheScriptEngine->isTimeFast();
	}

	if (TheGameLogic != nullptr)
	{
#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode);
#else	//always allow this cheat key if we're in a replay game.
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame());
#endif
	}

	allowFpsLimit &= TheGlobalData->m_useFpsLimit;
	allowFpsLimit &= isFramesPerSecondLimitEnabled();

	return allowFpsLimit;
}

Int FramePacer::getActualFramesPerSecondLimit() const
{
	return isActualFramesPerSecondLimitEnabled() ? getFramesPerSecondLimit() : RenderFpsPreset::UncappedFpsValue;
}

Real FramePacer::getUpdateTime()  const
{
	return m_updateTime;
}

Real FramePacer::getUpdateFps()  const
{
	return 1.0f / m_updateTime;
}

Real FramePacer::getBaseOverUpdateFpsRatio(Real minUpdateFps)
{
	// Update fps is floored to default 5 fps, 200 ms.
	// Useful to prevent insane ratios on frame spikes/stalls.
	return (Real)BaseFps / std::max(getUpdateFps(), minUpdateFps);
}

void FramePacer::setTimeFrozen(Bool frozen)
{
	m_isTimeFrozen = frozen;
}

void FramePacer::setGameHalted(Bool halted)
{
	m_isGameHalted = halted;
}

Bool FramePacer::isTimeFrozen() const
{
	return m_isTimeFrozen;
}

Bool FramePacer::isGameHalted() const
{
	return m_isGameHalted;
}

void FramePacer::setLogicTimeScaleFps( Int fps )
{
	m_logicTimeScaleFPS = fps;
}

Int FramePacer::getLogicTimeScaleFps() const
{
	return m_logicTimeScaleFPS;
}

void FramePacer::enableLogicTimeScale( Bool enable )
{
	m_enableLogicTimeScale = enable;
}

Bool FramePacer::isLogicTimeScaleEnabled() const
{
	return m_enableLogicTimeScale;
}

Int FramePacer::getActualLogicTimeScaleFps(LogicTimeQueryFlags flags) const
{
	if (m_isTimeFrozen && (flags & IgnoreFrozenTime) == 0)
	{
		return 0;
	}

	if (m_isGameHalted && (flags & IgnoreHaltedGame) == 0)
	{
		return 0;
	}

	if (TheNetwork != nullptr)
	{
		return TheNetwork->getFrameRate();
	}

	if (isLogicTimeScaleEnabled())
	{
		return getLogicTimeScaleFps();
	}

	// Returns uncapped value to align with the render update as per the original game behavior.
	return RenderFpsPreset::UncappedFpsValue;
}

Real FramePacer::getActualLogicTimeScaleRatio(LogicTimeQueryFlags flags) const
{
	return (Real)getActualLogicTimeScaleFps(flags) / LOGICFRAMES_PER_SECONDS_REAL;
}

Real FramePacer::getActualLogicTimeScaleOverFpsRatio(LogicTimeQueryFlags flags) const
{
	// TheSuperHackers @info Clamps ratio to min 1, because the logic
	// frame rate is currently capped by the render frame rate.
	return min(1.0f, (Real)getActualLogicTimeScaleFps(flags) / getUpdateFps());
}

Real FramePacer::getLogicTimeStepSeconds(LogicTimeQueryFlags flags) const
{
	return SECONDS_PER_LOGICFRAME_REAL * getActualLogicTimeScaleOverFpsRatio(flags);
}

Real FramePacer::getLogicTimeStepMilliseconds(LogicTimeQueryFlags flags) const
{
	return MSEC_PER_LOGICFRAME_REAL * getActualLogicTimeScaleOverFpsRatio(flags);
}
