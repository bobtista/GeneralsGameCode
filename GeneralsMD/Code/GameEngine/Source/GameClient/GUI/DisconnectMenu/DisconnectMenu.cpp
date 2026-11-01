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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/DisconnectMenu.h"
#include "GameClient/GUICallbacks.h"
#include "GameNetwork/NetworkInterface.h"
#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuActions.h"

DisconnectMenu *TheDisconnectMenu = nullptr;

DisconnectMenu::DisconnectMenu() {
	m_disconnectManager = nullptr;
}

DisconnectMenu::~DisconnectMenu() {
}

void DisconnectMenu::init() {
	m_disconnectManager = nullptr;
	DisconnectMenuActions::init();
	HideDisconnectWindow();
	m_menuState = DISCONNECTMENUSTATETYPE_SCREENOFF;
}

void DisconnectMenu::attachDisconnectManager(DisconnectManager *disconnectManager) {
	m_disconnectManager = disconnectManager;
}

void DisconnectMenu::showScreen() {
	HideDiplomacy();
	HideInGameChat();
	HideQuitMenu();
	DisconnectMenuActions::show();
	ShowDisconnectWindow();
	m_menuState = DISCONNECTMENUSTATETYPE_SCREENON;
}

void DisconnectMenu::hideScreen() {
	HideDisconnectWindow();
	m_menuState = DISCONNECTMENUSTATETYPE_SCREENOFF;
}

// The reports below only change the shared data; the .wnd copies it into its windows here, the
// RmlUi screen picks it up on its own every frame.
void DisconnectMenu::setPlayerName(Int playerNum, UnicodeString name) {
	DisconnectMenuActions::setPlayerName(playerNum, name);
	SyncDisconnectWindow();
}

void DisconnectMenu::setPlayerTimeoutTime(Int playerNum, time_t newTime) {
	DisconnectMenuActions::setPlayerTimeout(playerNum, newTime);
	SyncDisconnectWindow();
}

void DisconnectMenu::showPlayerControls(Int slot) {
	DisconnectMenuActions::showPlayerControls(slot);
	SyncDisconnectWindow();
}

void DisconnectMenu::hidePlayerControls(Int slot) {
	DisconnectMenuActions::hidePlayerControls(slot);
	SyncDisconnectWindow();
}

void DisconnectMenu::showPacketRouterTimeout() {
	DisconnectMenuActions::showPacketRouterTimeout();
	SyncDisconnectWindow();
}

void DisconnectMenu::hidePacketRouterTimeout() {
	DisconnectMenuActions::hidePacketRouterTimeout();
	SyncDisconnectWindow();
}

void DisconnectMenu::setPacketRouterTimeoutTime(time_t newTime) {
	DisconnectMenuActions::setPacketRouterTimeout(newTime);
	SyncDisconnectWindow();
}

void DisconnectMenu::sendChat(UnicodeString text) {
	TheNetwork->sendDisconnectChat(text);
}

void DisconnectMenu::showChat(UnicodeString text) {
	DisconnectMenuActions::showChat(text);
	SyncDisconnectWindow();
}

void DisconnectMenu::quitGame() {
	TheNetwork->quitGame();
}

void DisconnectMenu::removePlayer(Int slot, UnicodeString playerName) {
	DisconnectMenuActions::removePlayer(slot, playerName);
	SyncDisconnectWindow();
}

void DisconnectMenu::voteForPlayer(Int slot) {
	DEBUG_LOG(("Casting vote for disconnect slot %d", slot));
	TheNetwork->voteForPlayerDisconnect(slot); // Do this next.
}

void DisconnectMenu::updateVotes(Int slot, Int votes) {
	DisconnectMenuActions::updateVotes(slot, votes);
	SyncDisconnectWindow();
}
