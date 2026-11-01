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

// Ronin @feature 31/10/2026 DX9: tintinhamans' (GeneralsGameCode, arctic/go/feat/rmlui), ported as he wrote it.
// FILE: LanGameSetupData.h ////////////////////////////////////////////////////
// Widget-agnostic LAN game setup content: layers LAN-only per-slot/per-screen
// state (accepted flag, map availability, host-vs-client start control) on top
// of GameSetupData, the same slot/options snapshot skirmish setup already
// shares. Built from a LANGameInfo (a GameInfo) so LanGameOptionsMenu.cpp's
// .wnd path and a future RmlUi screen render the same thing.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/GUI/GUICallbacks/Menus/GameSetupData.h"
#include "GameNetwork/LANGameInfo.h"

#include <vector>

// One slot row: the shared GameSetupSlotRow fields, plus the two bits that only
// exist for a networked LAN game (SkirmishSetupActions has no equivalent).
struct LanGameSetupSlotRow
{
	GameSetupSlotRow m_base;
	Bool m_accepted = FALSE; // GameSlot::isAccepted(); non-human slots are always accepted
	Bool m_hasMap = TRUE;    // GameSlot::hasMap(); only meaningful for human slots
};

struct LanGameSetupData
{
	std::vector<LanGameSetupSlotRow> m_slots; // always MAX_SLOTS entries
	GameSetupOptionsData m_options;
	Bool m_isHost = FALSE;

	// ButtonStart: host sees "Start", client sees "Accept" (buttonStart->winSetText() in
	// LanGameOptionsMenuInit()); this is who I am, not a live state, so the label never
	// changes for the lifetime of the screen.
	Bool m_startButtonEnabled = TRUE;

	// Snapshot the current slots and map/options state out of game, plus the LAN-only bits
	// GameSetupData::build() doesn't know about. startButtonEnabled mirrors the caller's own
	// LANEnableStartButton()/LanGameSetupSignals::startButton state (see LanGameOptionsMenu.cpp),
	// since that toggle has no equivalent read back off LANGameInfo itself.
	static LanGameSetupData build( LANGameInfo *game, Bool startButtonEnabled );
};
