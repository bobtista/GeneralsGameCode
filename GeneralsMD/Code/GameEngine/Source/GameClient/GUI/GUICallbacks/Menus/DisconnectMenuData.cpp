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
// FILE: DisconnectMenuData.cpp //////////////////////////////////////////////
// See DisconnectMenuData.h.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuData.h"

void DisconnectMenuData::reset()
{
	for( Int i = 0; i < PLAYER_SLOTS; ++i )
	{
		Player &player = m_players[i];
		player = Player();
		player.m_votes = L"0";
	}

	m_routerVisible = FALSE;
	m_routerTimeout.clear();
	m_chat.clear();
	m_quitEnabled = TRUE;

	++m_chatVersion;
	touch();
}

void DisconnectMenuData::addChat( const UnicodeString &text, UnsignedInt rgb )
{
	ChatLine line;
	line.m_text = text;
	line.m_rgb = rgb;
	m_chat.push_back( line );

	if( (Int)m_chat.size() > MAX_CHAT_LINES )
		m_chat.erase( m_chat.begin() );

	++m_chatVersion;
	touch();
}
