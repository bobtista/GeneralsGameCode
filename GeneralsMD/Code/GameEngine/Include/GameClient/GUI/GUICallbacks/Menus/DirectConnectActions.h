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
// FILE: DirectConnectActions.h /////////////////////////////////////////////////
// Widget-agnostic LAN direct-connect actions: host/join/remote-IP-history/name-
// commit, with the exact behavior NetworkDirectConnect.cpp's callbacks have
// always had. No GameWindow/gadget coupling, so NetworkDirectConnect.cpp and a
// future RmlUi front end drive the same TheLAN calls through these functions
// instead of duplicating the logic. Same duplication precedent as
// LanLobbyActions.h. Moved from HostDirectConnectGame()/JoinDirectConnectGame()/
// UpdateRemoteIPList()/NetworkDirectConnectInit()/NetworkDirectConnectSystem()'s
// ButtonBack case.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

class UnicodeString;

namespace DirectConnectActions
{
	// Mirrors NetworkDirectConnectInit()'s TheLAN create/reset/local-IP-resolve dance (including the
	// delete+recreate step), minus anything GameWindow/gadget-specific (window lookups, combobox
	// population, static-text set, layout show/transition). Returns the player name to show (from
	// prefs, or the default "GUI:Player" name) -- same fallback the .wnd path's EditPlayerName gets.
	UnicodeString enterDirectConnect();

	// Formats TheLAN's local IP the same way the .wnd path's StaticLocalIP does. Empty if TheLAN is
	// null (shouldn't happen after enterDirectConnect(), but mirrors GetLocalIP()'s precondition).
	UnicodeString localIPString();

	// Mirrors HostDirectConnectGame() exactly: writes UserName pref, sends the (length-truncated)
	// name, then RequestGameCreate() with the local IP as the direct-connect game name.
	void hostGame( const UnicodeString &playerName );

	// Mirrors JoinDirectConnectGame() exactly: parses remoteIPEntry as "a.b.c.d[(...)]" (same
	// nextToken-before-'(' + sscanf as the original; an unparsable address still proceeds with
	// whatever sscanf partially filled, matching the .wnd path's DEBUG_ASSERTCRASH-only handling),
	// writes UserName pref, updates the remote-IP history (see updateRemoteIPList()), sends the
	// (length-truncated) name, then RequestGameJoinDirectConnect(). comboEntries/currentSelection are
	// the combobox's entries (in display order) and selection state at call time, exactly as
	// UpdateRemoteIPList() used to read them itself -- see updateRemoteIPList()'s comment.
	void joinGame( const UnicodeString &remoteIPEntry, const std::vector<UnicodeString> &comboEntries,
		int currentSelection, const UnicodeString &playerName );

	// Mirrors UpdateRemoteIPList()'s prefs bookkeeping exactly (dedup-by-IP against the typed entry,
	// re-numbering the "RemoteIPn" keys, writing "NumRemoteIPs"), given the combobox's entries (in
	// display order) and selection state, without any GadgetComboBox coupling. currentSelection is -1
	// when currentText doesn't match any existing entry (a freshly typed address), matching
	// GadgetComboBoxGetSelectedPos()'s -1-on-no-match behavior. A currentText that isn't a valid
	// "a.b.c.d..." address leaves everything unchanged (no prefs write), same as the original's early
	// return. Called by joinGame(); exposed separately since the .wnd path's caller still needs to
	// enumerate its own combobox to build comboEntries.
	void updateRemoteIPList( const std::vector<UnicodeString> &comboEntries, int currentSelection,
		const UnicodeString &currentText );

	// Mirrors ButtonBack's GBM_SELECTED handler: writes UserName pref, sends the (length-truncated)
	// name. Caller still does its own navigation (TheShell->pop()/RmlUiScreenRegistry close, etc.).
	void commitPlayerName( const UnicodeString &playerName );
}
