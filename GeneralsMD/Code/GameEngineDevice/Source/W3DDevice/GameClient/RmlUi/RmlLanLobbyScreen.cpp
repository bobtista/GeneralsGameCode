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
#include "W3DDevice/GameClient/RmlUi/RmlLanLobbyScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlChatInput.h"

#include "Common/AsciiString.h"
#include "Common/Debug.h"
#include "Common/UnicodeString.h"
#include "Common/UnicodeUtf8.h"
#include "GameClient/GameText.h"
#include "GameClient/GUI/GUICallbacks/Menus/LanLobbyActions.h"
#include "GameClient/MessageBox.h"
#include "GameClient/Shell.h"
#include "W3DDevice/GameClient/RmlUi/RmlTransitionSounds.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/LANGameInfo.h"
#include "GameNetwork/LANPlayer.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cstdio>
#include <windows.h>

extern Bool LANbuttonPushed; // LanLobbyMenu.cpp; see RmlLanLobbyScreen::update()
extern Bool LANSocketErrorDetected; // ditto -- set by LANAPI::update() on transport failure

namespace
{
	Rml::String rgbToHex(UnsignedInt rgb)
	{
		char hex[8];
		_snprintf_s(hex, sizeof(hex), _TRUNCATE, "#%02X%02X%02X", (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
		return Rml::String(hex);
	}

	// "3/8", or just "3" when the map is not installed and its size is unknown.
	Rml::String playersText(Int numPlayers, Int maxPlayers)
	{
		char text[16];
		if (maxPlayers > 0)
			_snprintf_s(text, sizeof(text), _TRUNCATE, "%d/%d", numPlayers, maxPlayers);
		else
			_snprintf_s(text, sizeof(text), _TRUNCATE, "%d", numPlayers);
		return Rml::String(text);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RmlLanLobbyScreen &RmlLanLobbyScreen::instance()
{
	static RmlLanLobbyScreen s_screen;
	return s_screen;
}

void RmlLanLobbyScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("lanlobby");
	if (constructor)
	{
		m_hq.bind(constructor);
		Rml::StructHandle<PlayerRowModel> playerHandle = constructor.RegisterStruct<PlayerRowModel>();
		if (playerHandle)
		{
			playerHandle.RegisterMember("name", &PlayerRowModel::name);
			playerHandle.RegisterMember("tooltip", &PlayerRowModel::tooltip);
			playerHandle.RegisterMember("used", &PlayerRowModel::used);
		}
		constructor.RegisterArray<Rml::Vector<PlayerRowModel>>();

		Rml::StructHandle<GameRowModel> gameHandle = constructor.RegisterStruct<GameRowModel>();
		if (gameHandle)
		{
			gameHandle.RegisterMember("index", &GameRowModel::index);
			gameHandle.RegisterMember("display_name", &GameRowModel::displayName);
			gameHandle.RegisterMember("map_name", &GameRowModel::mapName);
			gameHandle.RegisterMember("players_text", &GameRowModel::playersText);
			gameHandle.RegisterMember("in_progress", &GameRowModel::inProgress);
			gameHandle.RegisterMember("is_selected", &GameRowModel::isSelected);
			gameHandle.RegisterMember("used", &GameRowModel::used);
		}
		constructor.RegisterArray<Rml::Vector<GameRowModel>>();

		Rml::StructHandle<GameDetailSlotModel> slotHandle = constructor.RegisterStruct<GameDetailSlotModel>();
		if (slotHandle)
		{
			slotHandle.RegisterMember("occupied", &GameDetailSlotModel::occupied);
			slotHandle.RegisterMember("is_human", &GameDetailSlotModel::isHuman);
			slotHandle.RegisterMember("is_observer", &GameDetailSlotModel::isObserver);
			slotHandle.RegisterMember("is_random_faction", &GameDetailSlotModel::isRandomFaction);
			slotHandle.RegisterMember("label", &GameDetailSlotModel::label);
			slotHandle.RegisterMember("side_icon_image", &GameDetailSlotModel::sideIconImage);
			slotHandle.RegisterMember("show_side_icon", &GameDetailSlotModel::showSideIcon);
			slotHandle.RegisterMember("color_hex", &GameDetailSlotModel::colorHex);
		}
		constructor.RegisterArray<Rml::Vector<GameDetailSlotModel>>();
		constructor.RegisterArray<Rml::Vector<Rml::String>>();
		// Ronin @bugfix 31/10/2026 DX9: chat lines carry their colour (emotes, player colours)
		Rml::StructHandle<ChatLineModel> chatLineHandle = constructor.RegisterStruct<ChatLineModel>();
		if (chatLineHandle)
		{
			chatLineHandle.RegisterMember("text", &ChatLineModel::text);
			chatLineHandle.RegisterMember("color", &ChatLineModel::color);
		}
		constructor.RegisterArray<Rml::Vector<ChatLineModel>>();

		constructor.Bind("players", &m_model.players);
		constructor.Bind("games", &m_model.games);
		constructor.Bind("player_count", &m_model.playerCount);
		constructor.Bind("game_count", &m_model.gameCount);
		constructor.Bind("selected_game_index", &m_model.selectedGameIndex);
		constructor.Bind("details_valid", &m_model.detailsValid);
		constructor.Bind("details_game_name", &m_model.detailsGameName);
		constructor.Bind("details_map_display_name", &m_model.detailsMapDisplayName);
		constructor.Bind("details_map_path", &m_model.detailsMapPath);
		constructor.Bind("details_players_text", &m_model.detailsPlayersText);
		constructor.Bind("details_in_progress", &m_model.detailsInProgress);
		constructor.Bind("detail_slots", &m_model.detailSlots);
		constructor.Bind("lan_name", &m_model.playerName);
		constructor.Bind("chat_entry_text", &m_model.chatEntryText);
		constructor.Bind("chat_lines", &m_model.chatLines);

		constructor.BindEventCallback("host_game", &RmlLanLobbyScreen::onHostGame, this);
		constructor.BindEventCallback("join_selected", &RmlLanLobbyScreen::onJoinSelected, this);
		constructor.BindEventCallback("game_row_clicked", &RmlLanLobbyScreen::onGameRowClicked, this);
		constructor.BindEventCallback("game_row_activated", &RmlLanLobbyScreen::onGameRowActivated, this);
		constructor.BindEventCallback("direct_connect", &RmlLanLobbyScreen::onDirectConnect, this);
		constructor.BindEventCallback("back", &RmlLanLobbyScreen::onBackPressed, this);
		constructor.BindEventCallback("name_changed", &RmlLanLobbyScreen::onNameChanged, this);
		constructor.BindEventCallback("clear_name", &RmlLanLobbyScreen::onClearName, this);
		constructor.BindEventCallback("chat_entry_committed", &RmlLanLobbyScreen::onChatEntryCommitted, this);
		constructor.BindEventCallback("send_chat", &RmlLanLobbyScreen::onSendChat, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/LanLobby.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
void RmlLanLobbyScreen::show()
{
	if (!m_document)
		return;

	// Mark every row unused rather than clear() -- storage stays grow-only for this screen's whole
	// open lifetime (see RmlGrowOnlyList.h), including across a hide()/show() reopen.
	m_playerRows.beginUpdate();
	m_playerRows.endUpdate();
	m_gameRows.beginUpdate();
	m_gameRows.endUpdate();
	m_model.playerCount = 0;
	m_model.gameCount = 0;
	m_model.selectedGameIndex = -1;
	clearSelection();
	m_model.chatLines.clear();
	m_model.chatEntryText.clear();

	Bool socketError = FALSE;
	m_defaultName = LanLobbyActions::enterLobby(socketError);
	m_model.playerName = unicodeToUtf8(m_defaultName);

	if (m_modelHandle)
		m_modelHandle.DirtyAllVariables();

	m_connections.disconnect();
	m_connections.add(LanLobbySignals::playerList().connect([this](LANPlayer *playerList) { onPlayerListChanged(playerList); }));
	m_connections.add(LanLobbySignals::gameList().connect([this](LANGameInfo *gameList) { onGameListChanged(gameList); }));
	m_connections.add(LanLobbySignals::chatLine().connect([this](const UnicodeString &line, Color color) { onChatLine(unicodeToUtf8(line), RmlChatColorHex(color)); }));

	m_hq.refresh(m_modelHandle, true);
	m_document->Show();

	// LanLobbyMenuUpdate()'s entrance group, and LanLobbyMenuShutdown()'s reverse in hide().
	TransitionSounds::play("LanLobbyFade");

	// Mirrors LanLobbyMenuUpdate()'s LANSocketErrorDetected handling timing: raised here so it isn't
	// missed if SetLocalIP() failed before the signals/document were even up (see update()'s own check
	// for the same flag on every later frame).
	if (socketError)
		LANSocketErrorDetected = TRUE;
}

void RmlLanLobbyScreen::hide()
{
	if (m_document && m_document->IsVisible())
		TransitionSounds::play("LanLobbyFade", TRUE);
	if (m_document)
		m_document->Hide();

	m_connections.disconnect();

	LanLobbyActions::leaveLobby(utf8ToUnicode(m_model.playerName));
}

bool RmlLanLobbyScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

void RmlLanLobbyScreen::onBack()
{
	// Same as ButtonBack (see LanLobbyMenu.cpp's GBM_SELECTED): pop first (synchronously runs
	// hide()/leaveLobby() via RmlUiScreenRegistry::close(), same ordering the .wnd path relies on --
	// see LanLobbyMenu.cpp's Back handler comment), then free TheLAN.
	LANbuttonPushed = true;
	TheShell->pop();
	delete TheLAN;
	TheLAN = nullptr;
}

void RmlLanLobbyScreen::update()
{
	m_hq.refresh(m_modelHandle);
	// LanLobbyMenuUpdate() never runs for a registry-routed screen (see header comment); this is
	// its replacement for the two things that matter once the transition is already resolved (this
	// screen has no .wnd-style fade to gate on).
	if (!LANbuttonPushed && TheLAN)
		TheLAN->update();

	if (LANSocketErrorDetected == TRUE)
	{
		LANSocketErrorDetected = FALSE;
		DEBUG_LOG(("SOCKET ERROR!  BAILING!"));
		MessageBoxOk(TheGameText->fetch("GUI:NetworkError"), TheGameText->fetch("GUI:SocketError"), nullptr);
		onBack();
	}
}

//-------------------------------------------------------------------------------------------------
void RmlLanLobbyScreen::clearSelection()
{
	m_model.detailsValid = false;
	m_model.detailsGameName.clear();
	m_model.detailsMapDisplayName.clear();
	m_model.detailsMapPath.clear();
	m_model.detailsPlayersText.clear();
	m_model.detailsInProgress = false;
	m_model.detailSlots.clear();
	for (Int i = 0; i < MAX_SLOTS; ++i)
		m_model.detailSlots.push_back(GameDetailSlotModel());
}

// Rebuilds detail_slots in place (fixed MAX_SLOTS size, see GameDetailSlotModel) from the game at
// m_model.selectedGameIndex, mirroring GameInfoWindow.cpp's RefreshGameInfoWindow() by way of
// LanLobbyData::buildGameDetails().
void RmlLanLobbyScreen::refreshSelectedGameDetails()
{
	if (m_model.selectedGameIndex < 0 || !TheLAN)
	{
		clearSelection();
		if (m_modelHandle)
		{
			m_modelHandle.DirtyVariable("details_valid");
			m_modelHandle.DirtyVariable("details_map_path");
			m_modelHandle.DirtyVariable("detail_slots");
		}
		return;
	}

	LANGameInfo *game = TheLAN->LookupGameByListOffset(m_model.selectedGameIndex);
	LanLobbyGameDetails details = LanLobbyData::buildGameDetails(game);

	m_model.detailsValid = details.m_valid == TRUE;
	m_model.detailsGameName = unicodeToUtf8(details.m_gameName);
	m_model.detailsMapDisplayName = unicodeToUtf8(details.m_mapDisplayName);
	m_model.detailsMapPath = details.m_mapName.str();
	m_model.detailsPlayersText = playersText(details.m_numPlayers, details.m_maxPlayers);
	m_model.detailsInProgress = details.m_inProgress == TRUE;

	m_model.detailSlots.clear();
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		const LanLobbyGameDetailSlot &src = details.m_slots[i];
		GameDetailSlotModel slot;
		slot.occupied = src.m_occupied == TRUE;
		slot.isHuman = src.m_isHuman == TRUE;
		slot.isObserver = src.m_isObserver == TRUE;
		slot.isRandomFaction = src.m_isRandomFaction == TRUE;
		slot.label = unicodeToUtf8(src.m_label);
		slot.sideIconImage = src.m_sideIconImage.str();
		slot.showSideIcon = src.m_sideIconImage.isNotEmpty();
		if (src.m_colorIndex >= 0)
			slot.colorHex = rgbToHex(src.m_rgb);
		m_model.detailSlots.push_back(slot);
	}

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("details_valid");
		m_modelHandle.DirtyVariable("details_game_name");
		m_modelHandle.DirtyVariable("details_map_display_name");
		m_modelHandle.DirtyVariable("details_map_path");
		m_modelHandle.DirtyVariable("details_players_text");
		m_modelHandle.DirtyVariable("details_in_progress");
		m_modelHandle.DirtyVariable("detail_slots");
	}
}

//-------------------------------------------------------------------------------------------------
// LanLobbySignals::playerList target (see LANAPI::OnPlayerList()). Full snapshot each call, same as
// the .wnd path's GadgetListBoxReset()+repopulate -- rebuilt in one step, one Dirty*() call, per
// the growing/shrinking-list rule (see RmlSkirmishSetupScreen.h's start_markers comment for why an
// in-place resize is unsafe; here the array is simply rebuilt fresh, which RmlUi tolerates).
void RmlLanLobbyScreen::onPlayerListChanged(LANPlayer *playerList)
{
	std::vector<LanLobbyPlayerRow> rows = LanLobbyData::buildPlayerRows(playerList);

	m_playerRows.beginUpdate();
	for (const LanLobbyPlayerRow &row : rows)
	{
		PlayerRowModel &player = m_playerRows.next();
		player.name = unicodeToUtf8(row.m_name);
		player.tooltip = unicodeToUtf8(row.m_tooltip);
	}
	m_playerRows.endUpdate();
	m_model.playerCount = (int)rows.size();

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("players");
		m_modelHandle.DirtyVariable("player_count");
	}
}

// LanLobbySignals::gameList target (see LANAPI::OnGameList()). Same full-snapshot-rebuild reasoning as
// onPlayerListChanged(). Preserves the current selection by list offset (matches the .wnd path's
// GLM_SELECTED-driven listboxGames, which also just keys off list offset, not a stable game handle).
void RmlLanLobbyScreen::onGameListChanged(LANGameInfo *gameList)
{
	std::vector<LanLobbyGameRow> rows = LanLobbyData::buildGameRows(gameList);

	m_gameRows.beginUpdate();
	Int i = 0;
	for (const LanLobbyGameRow &row : rows)
	{
		GameRowModel &game = m_gameRows.next();
		game.index = i;
		game.displayName = unicodeToUtf8(row.m_displayName);
		game.mapName = unicodeToUtf8(row.m_mapDisplayName);
		game.playersText = playersText(row.m_numPlayers, row.m_maxPlayers);
		game.inProgress = row.m_inProgress == TRUE;
		game.isSelected = (i == m_model.selectedGameIndex);
		++i;
	}
	m_gameRows.endUpdate();
	m_model.gameCount = (int)rows.size();

	if (m_model.selectedGameIndex >= (Int)rows.size())
	{
		m_model.selectedGameIndex = -1;
		refreshSelectedGameDetails();
	}
	else if (m_model.selectedGameIndex >= 0)
	{
		refreshSelectedGameDetails(); // game at this offset may have changed shape (slots/map)
	}

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("games");
		m_modelHandle.DirtyVariable("game_count");
		m_modelHandle.DirtyVariable("selected_game_index");
	}
}

// Ronin @bugfix 31/10/2026 DX9: each line in the colour the game gives it (RmlChatColorHex()).
// LanLobbySignals::chatLine target (see LANAPI::OnChat()). Sending your own chat also arrives back through this
// signal (LAN chat is a broadcast the sender receives too), so onSendChat()/onChatEntryCommitted()
// below do not echo locally -- same as the .wnd path, whose GEM_EDIT_DONE/ButtonEmote handlers never
// write to listboxChatWindow themselves either.
void RmlLanLobbyScreen::onChatLine(const Rml::String &line, const Rml::String &color)
{
	ChatLineModel row;
	row.text = line;
	row.color = color;
	m_model.chatLines.push_back(row);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_lines");
}

//-------------------------------------------------------------------------------------------------
void RmlLanLobbyScreen::onHostGame(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	LanLobbyActions::hostGame();
}

void RmlLanLobbyScreen::onJoinSelected(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	// Mirrors ButtonJoin's GBM_SELECTED handler exactly, including the "no game selected" chat line.
	if (m_model.selectedGameIndex >= 0 && TheLAN)
	{
		LANGameInfo *game = TheLAN->LookupGameByListOffset(m_model.selectedGameIndex);
		LanLobbyActions::joinGame(game);
	}
	else
	{
		onChatLine(unicodeToUtf8(TheGameText->fetch("LAN:ErrorNoGameSelected")));
	}
}

void RmlLanLobbyScreen::onGameRowClicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	// Mirrors GLM_SELECTED: select the row and refresh the details panel, or hide it if the click
	// somehow carries an out-of-range index.
	int index = args[0].Get<int>();

	for (GameRowModel &game : m_model.games)
		game.isSelected = game.used && (game.index == index);

	m_model.selectedGameIndex = (index >= 0 && index < m_gameRows.liveCount()) ? index : -1;
	refreshSelectedGameDetails();

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("games");
		m_modelHandle.DirtyVariable("selected_game_index");
	}
}

void RmlLanLobbyScreen::onGameRowActivated(Rml::DataModelHandle constructorHandle, Rml::Event &event, const Rml::VariantList &args)
{
	// Mirrors GLM_DOUBLE_CLICKED: select (so the details panel/Join button agree) then join.
	onGameRowClicked(constructorHandle, event, args);
	if (m_model.selectedGameIndex >= 0 && TheLAN)
	{
		LANGameInfo *game = TheLAN->LookupGameByListOffset(m_model.selectedGameIndex);
		LanLobbyActions::joinGame(game);
	}
}

void RmlLanLobbyScreen::onDirectConnect(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	// Mirrors buttonDirectConnectID's handler. TheShell->push() routes to RmlNetworkDirectConnectScreen
	// automatically since NetworkDirectConnect.wnd is registered in RmlUiScreenRegistry (see
	// RmlUiManager::init()); no different call needed here than the .wnd path always made.
	LanLobbyActions::directConnect();
	TheShell->push("Menus/NetworkDirectConnect.wnd");
}

void RmlLanLobbyScreen::onBackPressed(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	onBack();
}

void RmlLanLobbyScreen::onNameChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	// Mirrors GEM_UPDATE_TEXT: sanitize, send, and write the sanitized text back into the field.
	UnicodeString sanitized = LanLobbyActions::sanitizeName(utf8ToUnicode(m_model.playerName));
	LanLobbyActions::setName(sanitized, m_defaultName);
	m_model.playerName = unicodeToUtf8(sanitized);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("lan_name");
}

void RmlLanLobbyScreen::onClearName(Rml::DataModelHandle handle, Rml::Event &event, const Rml::VariantList &args)
{
	// Mirrors ButtonClear: clear the field, then run the same commit logic (falls back to defaultName).
	m_model.playerName.clear();
	onNameChanged(handle, event, args);
}

void RmlLanLobbyScreen::onChatEntryCommitted(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	// Change fires on every edit; only Enter (linebreak) commits, like the .wnd's GEM_EDIT_DONE.
	if (!ev.GetParameter<bool>("linebreak", false))
		return;

	LanLobbyActions::sendChatEntry(utf8ToUnicode(m_model.chatEntryText));
	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");
}

void RmlLanLobbyScreen::onSendChat(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	// Despite the .wnd's ButtonEmote name, this sends normal chat (LANCHAT_NORMAL) -- see
	// LanLobbyActions.h's sendChatButton() comment. Kept for exact parity.
	LanLobbyActions::sendChatButton(utf8ToUnicode(m_model.chatEntryText));
	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");
}

//-------------------------------------------------------------------------------------------------
void OpenRmlLanLobbyScreen()
{
	if (!TheRmlUiManager)
		return;
	// Ronin @feature 31/10/2026 DX9: our shell's push / pop closes the screen before; no current-screen slot here.
	RmlLanLobbyScreen &screen = RmlLanLobbyScreen::instance();
	screen.load(TheRmlUiManager->getContext());
	screen.show();
}

void CloseRmlLanLobbyScreen()
{
	if (RmlLanLobbyScreen::instance().isVisible())
		RmlLanLobbyScreen::instance().hide();
}
