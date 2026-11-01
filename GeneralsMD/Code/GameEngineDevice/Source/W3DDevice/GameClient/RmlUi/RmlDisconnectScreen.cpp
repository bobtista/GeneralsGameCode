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
#include "W3DDevice/GameClient/RmlUi/RmlDisconnectScreen.h"
#include "Common/UnicodeUtf8.h"
#include "W3DDevice/GameClient/RmlUi/RmlChatInput.h"

#include "GameClient/DisconnectMenu.h"
#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuData.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cstdio>
#include <vector>
#include <windows.h>

namespace
{
	Rml::String rgbToHex(UnsignedInt rgb)
	{
		char hex[8];
		_snprintf_s(hex, sizeof(hex), _TRUNCATE, "#%02X%02X%02X", (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
		return Rml::String(hex);
	}

	// Row tops in percent of the panel: the .wnd's rows start at y 96 and are 32 apart, the panel
	// (DisconnectWindow) is 532 units tall and starts at y 52.
	Rml::String rowTop(int slot)
	{
		char text[32];
		_snprintf_s(text, sizeof(text), _TRUNCATE, "%.3f%%", (96 - 52 + 32 * slot) * 100.0f / 532.0f);
		return Rml::String(text);
	}
}

//-------------------------------------------------------------------------------------------------
RmlDisconnectScreen::RmlDisconnectScreen() : m_chatRows(m_model.chatLines)
{
}

RmlDisconnectScreen &RmlDisconnectScreen::instance()
{
	static RmlDisconnectScreen s_screen;
	return s_screen;
}

void RmlDisconnectScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	m_model.players.resize(DisconnectMenuData::PLAYER_SLOTS);
	for (int i = 0; i < DisconnectMenuData::PLAYER_SLOTS; ++i)
	{
		m_model.players[i].index = i;
		m_model.players[i].top = rowTop(i);
	}

	Rml::DataModelConstructor constructor = context->CreateDataModel("disconnectscreen");
	if (constructor)
	{
		if (Rml::StructHandle<PlayerModel> playerHandle = constructor.RegisterStruct<PlayerModel>())
		{
			playerHandle.RegisterMember("index", &PlayerModel::index);
			playerHandle.RegisterMember("top", &PlayerModel::top);
			playerHandle.RegisterMember("name", &PlayerModel::name);
			playerHandle.RegisterMember("timeout", &PlayerModel::timeout);
			playerHandle.RegisterMember("votes", &PlayerModel::votes);
			playerHandle.RegisterMember("visible", &PlayerModel::visible);
			playerHandle.RegisterMember("vote_enabled", &PlayerModel::voteEnabled);
		}
		constructor.RegisterArray<Rml::Vector<PlayerModel>>();

		if (Rml::StructHandle<ChatLineModel> chatHandle = constructor.RegisterStruct<ChatLineModel>())
		{
			chatHandle.RegisterMember("text", &ChatLineModel::text);
			chatHandle.RegisterMember("color", &ChatLineModel::color);
			chatHandle.RegisterMember("used", &ChatLineModel::used);
		}
		constructor.RegisterArray<Rml::Vector<ChatLineModel>>();

		constructor.Bind("players", &m_model.players);
		constructor.Bind("chat_lines", &m_model.chatLines);
		constructor.Bind("chat_entry_text", &m_model.chatEntryText);
		constructor.Bind("router_visible", &m_model.routerVisible);
		constructor.Bind("router_timeout", &m_model.routerTimeout);
		constructor.Bind("quit_enabled", &m_model.quitEnabled);

		constructor.BindEventCallback("vote", &RmlDisconnectScreen::onVote, this);
		constructor.BindEventCallback("quit", &RmlDisconnectScreen::onQuit, this);
		constructor.BindEventCallback("chat_entry_committed", &RmlDisconnectScreen::onChatEntryCommitted, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/DisconnectScreen.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
// Copies the shared data into the model. The chat entry is left alone: dirtying it would wipe
// what the player is typing every time a timeout ticks.
void RmlDisconnectScreen::refresh()
{
	const DisconnectMenuData &data = DisconnectMenuData::instance();

	for (int i = 0; i < DisconnectMenuData::PLAYER_SLOTS; ++i)
	{
		const DisconnectMenuData::Player &src = data.m_players[i];
		PlayerModel &player = m_model.players[i];
		player.name = unicodeToUtf8(src.m_name);
		player.timeout = unicodeToUtf8(src.m_timeout);
		player.votes = unicodeToUtf8(src.m_votes);
		player.visible = src.m_visible != FALSE;
		player.voteEnabled = src.m_voteEnabled != FALSE;
	}

	m_model.routerVisible = data.m_routerVisible != FALSE;
	m_model.routerTimeout = unicodeToUtf8(data.m_routerTimeout);
	m_model.quitEnabled = data.m_quitEnabled != FALSE;

	if (m_shownChatVersion != data.m_chatVersion)
	{
		m_shownChatVersion = data.m_chatVersion;

		m_chatRows.beginUpdate();
		for (size_t i = 0; i < data.m_chat.size(); ++i)
		{
			ChatLineModel &line = m_chatRows.next();
			line.text = unicodeToUtf8(data.m_chat[i].m_text);
			line.color = rgbToHex(data.m_chat[i].m_rgb);
		}
		m_chatRows.endUpdate();
	}
	m_shownVersion = data.m_version;

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("players");
		m_modelHandle.DirtyVariable("chat_lines");
		m_modelHandle.DirtyVariable("router_visible");
		m_modelHandle.DirtyVariable("router_timeout");
		m_modelHandle.DirtyVariable("quit_enabled");
	}
}

void RmlDisconnectScreen::open()
{
	if (!TheRmlUiManager)
		return;

	load(TheRmlUiManager->getContext());
	if (!m_document)
		return;

	refresh();

	// no focus for the document: the game keeps the keyboard until the chat entry is clicked
	m_document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);

	if (!m_focused)
	{
		m_focused = true;
		if (Rml::Element *entry = m_document->QuerySelector(".dc-entry"))
			entry->Focus();
	}
}

void RmlDisconnectScreen::close()
{
	if (m_document)
		m_document->Hide();
}

bool RmlDisconnectScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

void RmlDisconnectScreen::tick()
{
	RmlDisconnectScreen &screen = instance();
	if (!screen.isVisible())
		return;

	// The network game is over (DisconnectMenu is gone with it): nothing owns this panel any more.
	if (!TheDisconnectMenu)
	{
		screen.close();
		return;
	}

	const DisconnectMenuData &data = DisconnectMenuData::instance();
	if (screen.m_shownVersion != data.m_version || screen.m_shownChatVersion != data.m_chatVersion)
		screen.refresh();
}

//-------------------------------------------------------------------------------------------------
void RmlDisconnectScreen::onVote(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	if (args.empty() || !TheDisconnectMenu)
		return;

	DisconnectMenuActions::vote(args[0].Get<int>());
	refresh();
}

void RmlDisconnectScreen::onQuit(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	if (!TheDisconnectMenu)
		return;

	DisconnectMenuActions::quit();
	refresh();
}

// Enter in the entry (or leaving it): echo and send what was typed, then clear the line.
void RmlDisconnectScreen::onChatEntryCommitted(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	// Change fires on every edit; only Enter (linebreak) commits, like the .wnd's GEM_EDIT_DONE.
	if (!ev.GetParameter<bool>("linebreak", false))
		return;

	const UnicodeString text = utf8ToUnicode(m_model.chatEntryText);

	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");

	if (TheDisconnectMenu)
		DisconnectMenuActions::sendChat(text);
}

//-------------------------------------------------------------------------------------------------
void OpenRmlDisconnectScreen() { RmlDisconnectScreen::instance().open(); }
void CloseRmlDisconnectScreen() { RmlDisconnectScreen::instance().close(); }
