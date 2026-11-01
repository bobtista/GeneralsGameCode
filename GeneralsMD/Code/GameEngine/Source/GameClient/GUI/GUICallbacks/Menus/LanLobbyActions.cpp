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
// FILE: LanLobbyActions.cpp ////////////////////////////////////////////////////
// See LanLobbyActions.h.

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/LanLobbyActions.h"

#include "Common/GlobalData.h"
#include "Common/QuotedPrintable.h"
#include "Common/UnicodeString.h"
#include "Common/UserPreferences.h"
#include "GameClient/Shell.h"
#include "GameClient/ShellHooks.h"
#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/LANAPI.h"
#include "GameNetwork/LANAPICallbacks.h"

// Independent of LanLobbyMenu.cpp's own static (see enterLobby()/leaveLobby()): same duplication
// precedent as SkirmishSetupActions vs the .wnd SkirmishGameOptionsMenuInit()/its own statics.
static Bool s_useFpsLimit = FALSE;

void LanLobbyActions::hostGame()
{
	TheLAN->RequestGameCreate( L"", FALSE );
}

void LanLobbyActions::joinGame( LANGameInfo *game )
{
	if ( game )
		TheLAN->RequestGameJoin( game );
}

void LanLobbyActions::directConnect()
{
	TheLAN->RequestLobbyLeave( false );
}

UnicodeString LanLobbyActions::sanitizeName( const UnicodeString &rawInput )
{
	UnicodeString txtInput = rawInput;

	// Skip leading whitespace (see GEM_UPDATE_TEXT).
	const WideChar *c = txtInput.str();
	while ( c && iswspace(*c) )
		c++;

	if ( c )
		txtInput = UnicodeString(c);
	else
		txtInput = UnicodeString::TheEmptyString;

	txtInput.truncateTo(g_lanPlayerNameLength);

	// strtok delimiters can't be in names.
	if ( !txtInput.isEmpty() && txtInput.getCharAt(txtInput.getLength()-1) == L',' )
		txtInput.removeLastChar();

	if ( !txtInput.isEmpty() && txtInput.getCharAt(txtInput.getLength()-1) == L':' )
		txtInput.removeLastChar();

	if ( !txtInput.isEmpty() && txtInput.getCharAt(txtInput.getLength()-1) == L';' )
		txtInput.removeLastChar();

	return txtInput;
}

void LanLobbyActions::setName( const UnicodeString &sanitizedName, const UnicodeString &defaultName )
{
	if ( !sanitizedName.isEmpty() )
		TheLAN->RequestSetName( sanitizedName );
	else
		TheLAN->RequestSetName( defaultName );
}

UnicodeString LanLobbyActions::sendChatEntry( const UnicodeString &rawInput )
{
	UnicodeString txtInput = rawInput;

	// Trim leading whitespace only (see GEM_EDIT_DONE).
	while ( !txtInput.isEmpty() && iswspace(txtInput.getCharAt(0)) )
		txtInput = UnicodeString(txtInput.str()+1);

	if ( !txtInput.isEmpty() )
		TheLAN->RequestPlayerChat( txtInput );	// Ronin @bugfix 31/10/2026 DX9: as upstream's .wnd screens: player chat, a line starting "/me " goes as an emote

	return txtInput;
}

UnicodeString LanLobbyActions::sendChatButton( const UnicodeString &rawInput )
{
	UnicodeString txtInput = rawInput;
	txtInput.trim(); // both ends (see ButtonEmote's GBM_SELECTED handler)

	if ( !txtInput.isEmpty() )
		TheLAN->RequestPlayerChat( txtInput );	// Ronin @bugfix 31/10/2026 DX9: as upstream's .wnd screens: player chat, a line starting "/me " goes as an emote

	return txtInput;
}

UnicodeString LanLobbyActions::sendEmote( const UnicodeString &rawInput )
{
	UnicodeString txtInput = rawInput;
	txtInput.trim();

	if ( !txtInput.isEmpty() )
		TheLAN->RequestChat( txtInput, LANAPIInterface::LANCHAT_EMOTE );

	return txtInput;
}

UnicodeString LanLobbyActions::enterLobby( Bool &socketError )
{
	socketError = FALSE;

	if ( !TheLAN )
	{
		TheLAN = NEW LANAPI();
		s_useFpsLimit = TheGlobalData->m_useFpsLimit;
	}
	else
	{
		TheWritableGlobalData->m_useFpsLimit = s_useFpsLimit;
		TheLAN->reset();
	}

	UnsignedInt IP = TheGlobalData->m_defaultIP;
	if ( !IP )
	{
		IPEnumeration IPs;
		EnumeratedIP *IPlist = IPs.getAddresses();
		if ( IPlist )
			IP = IPlist->getIP();
	}

	TheLAN->init();
	if ( TheLAN->SetLocalIP( IP ) == FALSE )
		socketError = TRUE;

	LANPreferences prefs;
	UnicodeString defaultName = prefs.getUserName();
	defaultName.truncateTo( g_lanPlayerNameLength );

	TheLAN->RequestSetName( defaultName );
	TheLAN->RequestLocations();

	TheShell->showShellMap( TRUE );
	TheLAN->checkMOTD();

	SignalUIInteraction( SHELL_SCRIPT_HOOK_LAN_OPENED );

	return defaultName;
}

void LanLobbyActions::leaveLobby( const UnicodeString &playerName )
{
	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable( playerName );
	prefs.write();

	if ( TheLAN )
		TheLAN->RequestLobbyLeave( true );

	TheWritableGlobalData->m_useFpsLimit = s_useFpsLimit;

	SignalUIInteraction( SHELL_SCRIPT_HOOK_LAN_CLOSED );
}
