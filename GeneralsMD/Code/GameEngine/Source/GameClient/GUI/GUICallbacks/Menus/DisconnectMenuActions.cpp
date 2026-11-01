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
// FILE: DisconnectMenuActions.cpp ///////////////////////////////////////////
// See DisconnectMenuActions.h. Bodies moved out of DisconnectMenu.cpp and DisconnectWindow.cpp;
// no behavior change.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuActions.h"

#include "GameClient/DisconnectMenu.h"
#include "GameClient/GameText.h"
#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuData.h"

namespace DisconnectMenuActions
{

static const UnsignedInt CHAT_RGB = 0xFF0000;
static const UnsignedInt NOTICE_RGB = 0xFFFFFF;

static DisconnectMenuData &data()
{
	return DisconnectMenuData::instance();
}

static Bool validSlot( Int slot )
{
	return slot >= 0 && slot < DisconnectMenuData::PLAYER_SLOTS;
}

static UnicodeString number( Int value )
{
	UnicodeString text;
	text.format( L"%d", value );
	return text;
}

void init()
{
	data().reset();
}

void show()
{
	for( Int i = 0; i < DisconnectMenuData::PLAYER_SLOTS; ++i )
		data().m_players[i].m_voteEnabled = TRUE;
	data().m_quitEnabled = TRUE;

	data().m_chat.clear();
	data().addChat( TheGameText->fetch( "GUI:InternetDisconnectionMenuBody1" ), NOTICE_RGB );
}

void setPlayerName( Int slot, const UnicodeString &name )
{
	if( !validSlot( slot ) )
		return;

	if( !name.isEmpty() )
	{
		data().m_players[slot].m_name = name;
		data().m_players[slot].m_timeout.clear();
		showPlayerControls( slot );
	}
	else
	{
		hidePlayerControls( slot );
	}
}

void setPlayerTimeout( Int slot, time_t seconds )
{
	if( !validSlot( slot ) )
		return;

	data().m_players[slot].m_timeout = number( (Int)seconds );
	data().touch();
}

void showPlayerControls( Int slot )
{
	if( !validSlot( slot ) )
		return;

	data().m_players[slot].m_visible = TRUE;
	data().m_players[slot].m_voteEnabled = TRUE;
	data().touch();
}

void hidePlayerControls( Int slot )
{
	if( !validSlot( slot ) )
		return;

	data().m_players[slot].m_visible = FALSE;
	data().m_players[slot].m_voteEnabled = TRUE;
	data().touch();
}

void updateVotes( Int slot, Int votes )
{
	if( !validSlot( slot ) )
		return;

	data().m_players[slot].m_votes = number( votes );
	data().touch();
}

void showPacketRouterTimeout()
{
	data().m_routerTimeout.clear(); // start it off with a blank string
	data().m_routerVisible = TRUE;
	data().touch();
}

void hidePacketRouterTimeout()
{
	data().m_routerVisible = FALSE;
	data().touch();
}

void setPacketRouterTimeout( time_t seconds )
{
	data().m_routerTimeout = number( (Int)seconds );
	data().touch();
}

void showChat( const UnicodeString &text )
{
	data().addChat( text, CHAT_RGB );
}

void removePlayer( Int slot, const UnicodeString &name )
{
	hidePlayerControls( slot );

	UnicodeString text;
	text.format( TheGameText->fetch( "Network:PlayerLeftGame" ), name.str() );
	data().addChat( text, CHAT_RGB );
}

void vote( Int slot )
{
	if( !validSlot( slot ) )
		return;

	TheDisconnectMenu->voteForPlayer( slot );
	data().m_players[slot].m_voteEnabled = FALSE;
	data().touch();
}

void quit()
{
	TheDisconnectMenu->quitGame();
	data().m_quitEnabled = FALSE;
	data().touch();
}

void sendChat( UnicodeString text )
{
	// clean up the text (remove leading/trailing chars, etc)
	text.trim();
	if( !text.isEmpty() )
		TheDisconnectMenu->sendChat( text );
}

}
