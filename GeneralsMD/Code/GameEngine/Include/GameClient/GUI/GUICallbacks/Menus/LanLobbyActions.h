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
// FILE: LanLobbyActions.h //////////////////////////////////////////////////////
// Widget-agnostic LAN lobby actions: host/join/direct-connect/name-change/chat,
// with the exact validation the .wnd LanLobbyMenu.cpp callbacks have always used
// (name sanitizing/truncation, chat trimming). No GameWindow/gadget coupling, so
// LanLobbyMenu.cpp and a future RmlUi front end drive the same TheLAN calls
// through these functions instead of duplicating the logic. Moved from
// LanLobbyMenuSystem()'s GBM_SELECTED/GEM_UPDATE_TEXT/GEM_EDIT_DONE cases.
///////////////////////////////////////////////////////////////////////////////

#pragma once

class LANGameInfo;
class UnicodeString;

namespace LanLobbyActions
{
	// Host/Join/DirectConnect/Back button handlers.
	void hostGame();
	void joinGame( LANGameInfo *game ); ///< no-op if game is null (mirrors "no game selected" checks)
	void directConnect(); ///< leaves the lobby without a forced disconnect; caller still pushes NetworkDirectConnect.wnd

	// Player name: sanitizeName() mirrors GEM_UPDATE_TEXT's exact trim/truncate/trailing-punctuation
	// rules (','/':'/':' are strtok delimiters and can't be in names). setName() sends the sanitized
	// name, or defaultName if sanitizing left it empty -- same fallback as the .wnd handler.
	UnicodeString sanitizeName( const UnicodeString &rawInput );
	void setName( const UnicodeString &sanitizedName, const UnicodeString &defaultName );

	// Chat: sendChatEntry() mirrors GEM_EDIT_DONE's chat entry line (trims leading whitespace only).
	// sendChatButton() mirrors ButtonEmote's handler, which despite its name sends LANCHAT_NORMAL,
	// not LANCHAT_EMOTE (trims both leading and trailing whitespace) -- both intentionally differ in
	// trimming to match the .wnd's existing, slightly inconsistent, behavior exactly.
	UnicodeString sendChatEntry( const UnicodeString &rawInput );
	UnicodeString sendChatButton( const UnicodeString &rawInput );

	// Real emote support for a front end that wants it (the .wnd's "emote" button is actually a
	// second Send button -- see sendChatButton()). Trims both ends like sendChatButton().
	UnicodeString sendEmote( const UnicodeString &rawInput );

	// Mirrors LanLobbyMenuInit()'s engine-state setup (TheLAN create/reset, IP selection, default
	// player name, MOTD check) minus anything GameWindow/gadget-specific (listbox reset, tooltip,
	// GameInfoWindow) -- same duplication precedent as SkirmishSetupActions::enterSkirmishSetup()
	// vs SkirmishGameOptionsMenuInit(). Returns the sanitized default player name (already sent via
	// RequestSetName()); callers show it in their own player-name field. socketError is set TRUE if
	// SetLocalIP() failed (mirrors LanLobbyMenu.cpp's LANSocketErrorDetected), so a non-.wnd caller
	// can raise the same "GUI:SocketError" message box itself.
	UnicodeString enterLobby( Bool &socketError );

	// Mirrors LanLobbyMenuShutdown()'s engine-state teardown (user name pref write, RequestLobbyLeave,
	// fps-limit restore) minus DestroyGameInfoWindow() (a GameWindow-only gadget window). playerName
	// is whatever the caller's player-name field currently holds (saved into UserPreferences).
	void leaveLobby( const UnicodeString &playerName );
}
