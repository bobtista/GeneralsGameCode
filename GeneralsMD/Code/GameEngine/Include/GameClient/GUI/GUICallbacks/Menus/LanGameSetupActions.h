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
// FILE: LanGameSetupActions.h //////////////////////////////////////////////////
// Widget-agnostic LAN game setup logic: slot/option mutation on TheLAN's current
// LANGameInfo, with the same host-authoritative/client-request network traffic
// LanGameOptionsMenu.cpp's handle*Selection() functions already send, so a future
// non-.wnd front end (RmlLanGameSetupScreen) can drive the same game through
// these functions instead of duplicating the request/validation logic. Moved
// from LanGameOptionsMenu.cpp's handle*Selection()/StartPressed()/
// getNextSelectablePlayer()/getFirstSelectablePlayer() and the ButtonMapStartPosition
// GBM_SELECTED/GBM_SELECTED_RIGHT cases. Widget-specific work (combo box
// population, listbox text) stays at the .wnd call site; only network request +
// GameSlot mutation moves here.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameMemory.h"
#include "GameNetwork/LANGameInfo.h"

class AsciiString;
class Money;
class UnicodeString;

namespace LanGameSetupActions
{
	// Result of validateStart(): whether the host's game is ready to request a start.
	enum StartValidationResult
	{
		STARTVALIDATION_READY,
		STARTVALIDATION_TOO_MANY_PLAYERS,
		STARTVALIDATION_NEED_HUMAN_PLAYERS,
		STARTVALIDATION_NEED_MORE_PLAYERS,
		STARTVALIDATION_NEED_MORE_TEAMS,
		STARTVALIDATION_WAITING_ON_ACCEPTS // not ready: still sent RequestAccept(), same as StartPressed()'s else branch
	};

	// getNextSelectablePlayer/getFirstSelectablePlayer: same rules as
	// SkirmishSetupActions' equivalents, ported from LanGameOptionsMenu.cpp's file-local
	// helpers of the same name (LAN's version is host-only and looks at LANGameSlot).
	Int getNextSelectablePlayer( LANGameInfo *game, Int start );
	Int getFirstSelectablePlayer( const LANGameInfo *game );

	// ComboBoxPlayer[i] (host only): move slot i to state (open/closed/AI difficulty), or
	// take/leave the local human's own slot. Ported from LanGameOptionsMenu.cpp's
	// GCM_SELECTED comboBoxPlayerID case. Returns TRUE if slot i's isAI() flipped, so the
	// caller knows whether to repopulate that slot's faction dropdown (observer is only
	// offered to non-AI slots), same as the .wnd path's PopulatePlayerTemplateComboBox call.
	Bool selectPlayerState( LANGameInfo *game, Int slotIndex, SlotState state, Bool *outIsAIChanged );

	// ComboBoxColor[i]: set slot i's color, unless another slot already has it. Sends the
	// host's full option string or the client's single-field request, same as
	// handleColorSelection(). isIniting suppresses the network request during setup
	// (matches s_isIniting).
	Bool selectColor( LANGameInfo *game, Int slotIndex, Int color, Bool isIniting );

	// ComboBoxPlayerTemplate[i]: set slot i's faction. Returns TRUE if the slot became or
	// stopped being an observer (caller repopulates color/team to the "random only" default,
	// same as handlePlayerTemplateSelection()).
	Bool selectPlayerTemplate( LANGameInfo *game, Int slotIndex, Int playerTemplate, Bool isIniting, Bool *outObserverChanged );

	// ComboBoxTeam[i]: set slot i's team.
	Bool selectTeam( LANGameInfo *game, Int slotIndex, Int team, Bool isIniting );

	// ButtonMapStartPosition/map preview marker click for slot i: set slot i's start position,
	// unless another slot already occupies it.
	Bool selectStartPosition( LANGameInfo *game, Int slotIndex, Int position, Bool isIniting );

	// Map preview start-position marker click: move whichever slot the local host can move out
	// of position, and move the next selectable player into it. Ported from
	// LanGameOptionsMenu.cpp's ButtonMapStartPosition GBM_SELECTED case.
	void handleStartPositionMarkerClick( LANGameInfo *game, Int position );

	// Map preview start-position marker right-click: clear whichever slot the local host can
	// move out of position. Ported from the GBM_SELECTED_RIGHT case.
	void handleStartPositionMarkerRightClick( LANGameInfo *game, Int position );

	// ComboBoxStartingCash (host only): set the game's starting cash.
	void setStartingCash( LANGameInfo *game, const Money &startingCash, Bool isIniting );

	// CheckboxLimitSuperweapons (host only): set/clear the superweapon restriction.
	void setSuperweaponRestriction( LANGameInfo *game, Bool restricted, Bool isIniting );

	// ButtonStart (host): whether RequestGameStart()/RequestGameStartTimer() can be sent, same
	// checks as StartPressed(). Ready slots are not closed and RequestGameStart(Timer) is not
	// sent here -- see startGame().
	StartValidationResult validateStart( LANGameInfo *game );

	// ButtonStart (host), once validateStart() returns something other than
	// STARTVALIDATION_WAITING_ON_ACCEPTS: closes open slots and requests the countdown/start,
	// same as StartPressed()'s isReady branch. Caller still owns LANEnableStartButton(false)
	// and closed-slot combo box repopulation (widget work).
	void startGame( LANGameInfo *game );

	// ButtonStart (client)/Accept: send RequestAccept(), same as the non-host ButtonStart case.
	void requestAccept( LANGameInfo *game );

	// ButtonBack: leave the game, same as the buttonBack GBM_SELECTED case (minus destroying
	// the .wnd's own mapSelectLayout, which stays at the .wnd call site).
	void leaveGame();

	// LanMapSelectMenu.wnd's ButtonOK, folded together with the SEND_GAME_OPTS branch of
	// PostToLanGameOptions() it always triggers on a successful pick (map/CRC/size, slot 0's
	// map availability, resetStartSpots()/adjustSlotsForMap(), every slot's start position
	// cleared, resetAccepted(), then the widget-safe updateGameOptions()/lanUpdateSlotList()
	// refresh hooks and the host's RequestGameOptions() broadcast). No-op if game is null.
	void applySelectedMap( LANGameInfo *game, const AsciiString &mapName );
}
