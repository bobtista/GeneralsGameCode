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
// FILE: RmlDisconnectScreen.h ////////////////////////////////////////////////
// RmlUi view of Menus/DisconnectScreen.wnd, the panel over a running network game that lists the
// players who stopped answering (vote to drop them, quit, chat). All the rules live in
// DisconnectMenuActions/DisconnectMenuData; this only mirrors DisconnectMenuData into a data model
// once per frame.
//
// DisconnectMenu decides when the screen is up (isScreenVisible() is polled all over the game): its
// showScreen()/hideScreen() open and close this through the registry, and this never opens itself.
// It is an overlay: the registry entry does not capture input, so the HUD keeps the mouse everywhere
// but over the panel, and the keyboard belongs to RmlUi only while the chat entry has the focus.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "W3DDevice/GameClient/RmlUi/RmlGrowOnlyList.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
class RmlDisconnectScreen
{
public:
	static RmlDisconnectScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	void open();
	void close();
	bool isVisible() const;

	// Per-frame pump; see RmlUiManager::update().
	static void tick();

private:
	RmlDisconnectScreen();

	void load(Rml::Context *context);
	void refresh();

	void onVote(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onQuit(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onChatEntryCommitted(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);

	struct PlayerModel
	{
		int index = 0;
		Rml::String top; ///< the row's place in the panel, in percent
		Rml::String name;
		Rml::String timeout;
		Rml::String votes;
		bool visible = false;
		bool voteEnabled = true;
	};

	struct ChatLineModel
	{
		Rml::String text;
		Rml::String color = "#FFFFFF";
		bool used = true; // grow-only storage, see RmlGrowOnlyList.h; hidden via data-if when false
	};

	struct Model
	{
		Rml::Vector<PlayerModel> players; // always one per slot
		Rml::Vector<ChatLineModel> chatLines;
		Rml::String chatEntryText;
		bool routerVisible = false;
		Rml::String routerTimeout;
		bool quitEnabled = true;
	};

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;

	Model m_model;
	RmlGrowOnlyList<ChatLineModel> m_chatRows;
	unsigned int m_shownVersion = 0;
	unsigned int m_shownChatVersion = 0;
	bool m_focused = false; ///< the chat entry got the focus once, like the .wnd's on creation
};

// Registry entry points (see RmlUiManager::init()).
void OpenRmlDisconnectScreen();
void CloseRmlDisconnectScreen();
