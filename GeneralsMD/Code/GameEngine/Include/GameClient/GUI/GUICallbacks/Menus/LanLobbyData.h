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
// FILE: LanLobbyData.h ////////////////////////////////////////////////////////
// Widget-agnostic LAN lobby content: the player list, game list, and a selected
// game's details, with no GameWindow/gadget coupling. Built from the raw
// LANPlayer*/LANGameInfo* linked lists LANAPI::OnPlayerList()/OnGameList() and
// LANDisplayGameList()/RefreshGameInfoWindow() already walk, so a non-.wnd front
// end (RmlLanLobbyScreen) can render the same lists the .wnd path's
// listboxPlayers/listboxGames/GameInfoWindow build, without depending on
// GameWindow/GadgetListBox. Mirrors GameSetupData.h's role for skirmish setup.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/UnicodeString.h"
#include "GameNetwork/LANGameInfo.h"
#include "GameNetwork/LANPlayer.h"

#include <vector>

// One row of the lobby player list.
struct LanLobbyPlayerRow
{
	UnicodeString m_name;
	UnsignedInt m_ip = 0;
	UnicodeString m_tooltip; // buildPlayerTooltip(): login/host, +IP in RTS_DEBUG
};

// One row of the lobby game list. m_game is only valid for the lifetime of the LANGameInfo list
// LANAPI just handed to OnGameList()/LanLobbySignals::gameList -- do not retain across callbacks,
// same lifetime rule the .wnd path already relies on via GadgetListBoxSetItemData.
struct LanLobbyGameRow
{
	LANGameInfo *m_game = nullptr;
	UnicodeString m_displayName; // host's player name, bracketed like "[Name]" while in progress
	Bool m_inProgress = FALSE;
	UnicodeString m_mapDisplayName;
	Int m_numPlayers = 0;
	Int m_maxPlayers = 0;
};

// One slot of a selected game's details panel (mirrors GameInfoWindow.cpp's RefreshGameInfoWindow).
struct LanLobbyGameDetailSlot
{
	Bool m_occupied = FALSE;
	Bool m_isHuman = FALSE;
	Bool m_isObserver = FALSE;
	Bool m_isRandomFaction = FALSE; // occupied, non-observer, but no valid PlayerTemplate side icon
	UnicodeString m_label; // player name, or the AI difficulty's localized label
	AsciiString m_sideIconImage; // side icon image name; empty when none applies (random/observer)
	Int m_colorIndex = -1;
	UnsignedInt m_rgb = 0; // 0x00RRGGBB of m_colorIndex, 0 when it has none
};

// A selected game's details (mirrors GameInfoWindow.cpp's RefreshGameInfoWindow contents).
struct LanLobbyGameDetails
{
	Bool m_valid = FALSE;
	UnicodeString m_gameName;
	UnicodeString m_mapDisplayName;
	AsciiString m_mapName; // the map's path, for its preview
	Bool m_inProgress = FALSE;
	Int m_numPlayers = 0;
	Int m_maxPlayers = 0;
	LanLobbyGameDetailSlot m_slots[MAX_SLOTS];
};

namespace LanLobbyData
{
	// The lobby player list's tooltip text: login/host (plus the IP in debug builds), empty if the
	// player has neither. Shared with the .wnd's cursor tooltip (setLANPlayerTooltip()).
	UnicodeString buildPlayerTooltip( LANPlayer *player );
	std::vector<LanLobbyPlayerRow> buildPlayerRows( LANPlayer *playerList );
	std::vector<LanLobbyGameRow> buildGameRows( LANGameInfo *gameList );
	LanLobbyGameDetails buildGameDetails( LANGameInfo *game );
}
