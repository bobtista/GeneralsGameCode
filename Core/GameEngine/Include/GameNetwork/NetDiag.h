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

// Temporary diagnostics for the network stall investigation. Header-only so it
// needs no build wiring, and every output is gated behind -setDebugLevel NET.

#pragma once

#include "Lib/BaseType.h"
#include "Common/Debug.h"

#ifdef DEBUG_LOGGING

// Tracks a FRAMEINFO from the moment it is queued to the moment its ack lands, so
// the self-inflicted queueing delay can be separated from real round trip time.
class NetDiag
{
public:
	enum { SlotCount = 256 };

	struct FrameInfoRecord
	{
		UnsignedInt queuedMs;
		UnsignedInt sentMs;
		UnsignedInt frame;
		UnsignedShort commandId;
		Bool inUse;
		Bool sent;
	};

	static NetDiag& get()
	{
		static NetDiag theInstance;
		return theInstance;
	}

	void reset()
	{
		for (Int i = 0; i < SlotCount; ++i)
		{
			m_records[i].inUse = FALSE;
		}
		m_stalling = FALSE;
		m_stallStartMs = 0;
		m_stallFrame = 0;
		m_stallCount = 0;
		m_stallTotalMs = 0;
		m_logicFrames = 0;
		m_renderFrames = 0;
		m_lastPollMs = 0;
		m_pollGapTotal = 0;
		m_pollGapCount = 0;
		m_pollGapMax = 0;
		m_rateWindowStartMs = timeGetTime();
	}

	// FRAMEINFO lifecycle ----------------------------------------------------

	void onFrameInfoQueued(UnsignedInt frame, UnsignedShort commandId)
	{
		FrameInfoRecord& rec = m_records[commandId % SlotCount];
		rec.queuedMs = timeGetTime();
		rec.sentMs = 0;
		rec.frame = frame;
		rec.commandId = commandId;
		rec.inUse = TRUE;
		rec.sent = FALSE;
	}

	void onFrameInfoSent(UnsignedShort commandId)
	{
		FrameInfoRecord& rec = m_records[commandId % SlotCount];
		if (rec.inUse && rec.commandId == commandId && !rec.sent)
		{
			rec.sentMs = timeGetTime();
			rec.sent = TRUE;
		}
	}

	// Emits the queue/wire/ack split for one acked FRAMEINFO.
	void onFrameInfoAcked(UnsignedShort commandId, Int frameGroupingMs, Real smoothedLatency)
	{
		FrameInfoRecord& rec = m_records[commandId % SlotCount];
		if (!rec.inUse || rec.commandId != commandId)
		{
			return;
		}

		const UnsignedInt nowMs = timeGetTime();
		const UnsignedInt queueDelayMs = rec.sent ? (rec.sentMs - rec.queuedMs) : 0;
		const UnsignedInt wireDelayMs = rec.sent ? (nowMs - rec.sentMs) : 0;
		const UnsignedInt totalMs = nowMs - rec.queuedMs;

		DEBUG_LOG_LEVEL(DEBUG_LEVEL_NET, ("NETDIAG lat frame=%d id=%d queuedToWire=%dms wireToAck=%dms total=%dms grouping=%dms smoothedAvg=%.4fs sent=%d",
			rec.frame, commandId, queueDelayMs, wireDelayMs, totalMs, frameGroupingMs, smoothedLatency, rec.sent ? 1 : 0));

		rec.inUse = FALSE;
	}

	// Stall lifecycle --------------------------------------------------------

	void onFrameNotReady(UnsignedInt frame, Int waitingOnPlayer, Int runAhead, Int cushion)
	{
		if (!m_stalling)
		{
			m_stalling = TRUE;
			m_stallStartMs = timeGetTime();
			m_stallFrame = frame;
			m_stallWaitingOn = waitingOnPlayer;
			m_stallRunAhead = runAhead;
			m_stallCushion = cushion;
		}
	}

	void onFrameReady(UnsignedInt frame)
	{
		if (m_stalling)
		{
			const UnsignedInt waitedMs = timeGetTime() - m_stallStartMs;
			++m_stallCount;
			m_stallTotalMs += waitedMs;
			DEBUG_LOG_LEVEL(DEBUG_LEVEL_NET, ("NETDIAG stall frame=%d waited=%dms waitingOnPlayer=%d runAhead=%d cushion=%d totalStalls=%d totalStalledMs=%d",
				m_stallFrame, waitedMs, m_stallWaitingOn, m_stallRunAhead, m_stallCushion, m_stallCount, m_stallTotalMs));
			m_stalling = FALSE;
		}
	}

	// Rate accounting --------------------------------------------------------

	void onLogicFrame() { ++m_logicFrames; }
	void onRenderFrame() { ++m_renderFrames; }

	// How often the socket is actually drained. Anything a packet spends waiting here is latency
	// the engine adds on top of the wire, and it is paid on both hops of every round trip.
	void onRecvPoll()
	{
		const UnsignedInt nowMs = timeGetTime();
		if (m_lastPollMs != 0)
		{
			const UnsignedInt gap = nowMs - m_lastPollMs;
			m_pollGapTotal += gap;
			++m_pollGapCount;
			if (gap > m_pollGapMax) { m_pollGapMax = gap; }
		}
		m_lastPollMs = nowMs;
	}

	void reportPollGap()
	{
		if (m_pollGapCount == 0) { return; }
		DEBUG_LOG_LEVEL(DEBUG_LEVEL_NET, ("NETDIAG poll gaps=%d meanMs=%.2f maxMs=%d",
			m_pollGapCount, (Real)m_pollGapTotal / (Real)m_pollGapCount, m_pollGapMax));
		m_pollGapCount = 0; m_pollGapTotal = 0; m_pollGapMax = 0;
	}

	// Emits measured logic and render rates once per second. Everything here is
	// counted directly rather than inferred from runahead command spacing.
	void reportRates(Real displayAvgFps, Real displayInstFps, Int metricsAvgFps,
		Int runAhead, Int frameRate, Int cushion, Bool fpsLimitEnabled, Int fpsLimit)
	{
		const UnsignedInt nowMs = timeGetTime();
		const UnsignedInt elapsedMs = nowMs - m_rateWindowStartMs;
		if (elapsedMs < 1000)
		{
			return;
		}

		const Real logicFps = (Real)m_logicFrames * 1000.0f / (Real)elapsedMs;
		const Real renderFps = (Real)m_renderFrames * 1000.0f / (Real)elapsedMs;

		DEBUG_LOG_LEVEL(DEBUG_LEVEL_NET, ("NETDIAG rate window=%dms logicFrames=%d logicFps=%.1f renderFrames=%d renderFps=%.1f displayAvgFps=%.1f displayInstFps=%.1f metricsAvgFps=%d runAhead=%d frameRate=%d cushion=%d fpsLimitEnabled=%d fpsLimit=%d stallsThisGame=%d stalledMs=%d",
			elapsedMs, m_logicFrames, logicFps, m_renderFrames, renderFps, displayAvgFps, displayInstFps,
			metricsAvgFps, runAhead, frameRate, cushion, fpsLimitEnabled ? 1 : 0, fpsLimit, m_stallCount, m_stallTotalMs));

		m_logicFrames = 0;
		m_renderFrames = 0;
		m_rateWindowStartMs = nowMs;
	}

private:
	NetDiag() { reset(); }

	FrameInfoRecord m_records[SlotCount];

	Bool m_stalling;
	UnsignedInt m_stallStartMs;
	UnsignedInt m_stallFrame;
	Int m_stallWaitingOn;
	Int m_stallRunAhead;
	Int m_stallCushion;
	Int m_stallCount;
	UnsignedInt m_stallTotalMs;

	Int m_logicFrames;
	Int m_renderFrames;
	UnsignedInt m_lastPollMs;
	UnsignedInt m_pollGapTotal;
	Int m_pollGapCount;
	UnsignedInt m_pollGapMax;
	UnsignedInt m_rateWindowStartMs;
};

#define NETDIAG_CALL(expr) do { NetDiag::get().expr; } while (0)

#else

#define NETDIAG_CALL(expr) ((void)0)

#endif // DEBUG_LOGGING
