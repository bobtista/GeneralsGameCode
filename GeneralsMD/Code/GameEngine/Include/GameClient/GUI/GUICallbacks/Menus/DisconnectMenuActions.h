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
// FILE: DisconnectMenuActions.h /////////////////////////////////////////////
// Widget-agnostic disconnect screen logic: what the disconnect manager reports (names, timeouts,
// votes, chat) and what the player does about it (vote, quit, chat), changing DisconnectMenuData.
// DisconnectMenu feeds the reports in; DisconnectWindow.cpp's .wnd callbacks and
// RmlDisconnectScreen call the player's actions instead of duplicating the rules.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/UnicodeString.h"
#include "Lib/BaseType.h"

#include <ctime>

namespace DisconnectMenuActions
{
	void init(); ///< a network game starts: forget everything
	void show(); ///< the screen comes up: all buttons enabled, the chat starts over with the notice

	// Reports of the disconnect manager. The slots do not include the local player.
	void setPlayerName( Int slot, const UnicodeString &name ); ///< an empty name hides the slot
	void setPlayerTimeout( Int slot, time_t seconds );
	void showPlayerControls( Int slot );
	void hidePlayerControls( Int slot );
	void updateVotes( Int slot, Int votes );
	void showPacketRouterTimeout();
	void hidePacketRouterTimeout();
	void setPacketRouterTimeout( time_t seconds );
	void showChat( const UnicodeString &text );
	void removePlayer( Int slot, const UnicodeString &name );

	// What the player does.
	void vote( Int slot ); ///< votes to drop that player; the button stays disabled until the screen comes up again
	void quit(); ///< leaves the game
	void sendChat( UnicodeString text ); ///< trims it and sends it to the others unless it is empty
}
