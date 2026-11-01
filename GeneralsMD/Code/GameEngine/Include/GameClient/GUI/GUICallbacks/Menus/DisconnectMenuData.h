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
// FILE: DisconnectMenuData.h ////////////////////////////////////////////////
// Widget-agnostic state of the disconnect screen (DisconnectScreen.wnd and its RmlUi replacement):
// the other players' names, timeouts and votes, the packet router timeout and the chat. Whether
// the screen is up stays with DisconnectMenu. DisconnectMenuActions changes this data; the
// screens only draw it.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/UnicodeString.h"
#include "GameNetwork/GameInfo.h"

#include <vector>

struct DisconnectMenuData
{
	static DisconnectMenuData &instance()
	{
		static DisconnectMenuData s_data;
		return s_data;
	}

	static const Int PLAYER_SLOTS = MAX_SLOTS - 1; ///< everybody but the local player
	static const Int MAX_CHAT_LINES = 100; ///< the .wnd's listbox length; older lines are purged

	struct Player
	{
		UnicodeString m_name;
		UnicodeString m_timeout; ///< seconds since the player was last heard of
		UnicodeString m_votes;
		Bool m_visible = FALSE; ///< the name, timeout, vote count and vote button
		Bool m_voteEnabled = TRUE;
	};

	struct ChatLine
	{
		UnicodeString m_text;
		UnsignedInt m_rgb = 0xFFFFFF;
	};

	void reset();
	void touch() { ++m_version; } ///< bump after any change so the view refreshes
	void addChat( const UnicodeString &text, UnsignedInt rgb );

	Player m_players[PLAYER_SLOTS];

	Bool m_routerVisible = FALSE; ///< the packet router timeout and its label
	UnicodeString m_routerTimeout;

	std::vector<ChatLine> m_chat;
	Bool m_quitEnabled = TRUE;

	UnsignedInt m_version = 0;
	UnsignedInt m_chatVersion = 0; ///< changes whenever a chat line was added or the lines were cleared
};
