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
// FILE: RmlInGamePopupScreen.h ///////////////////////////////////////////////
// RmlUi view of InGamePopupMessage.wnd, the message a map script pops up over the game (text in the
// script's color, at the script's position and width, and an OK button). All the rules live in
// InGamePopupMessageActions/InGamePopupMessageData; this only mirrors the data into a data model when it
// opens.
//
// InGameUI::popupMessage() creates the .wnd through winCreateLayout(), which hands back a placeholder
// layout for a registered path: its runInit() opens this through the registry and clearPopupMessageData()
// destroying it closes it. It is an overlay: the registry entry does not capture input, so the HUD keeps the
// mouse everywhere but over the panel. The .wnd took the keyboard focus for its Enter and Escape keys
// (OK), so the registry offers those two keys to onKey() ahead of the game while it is up. A popup that
// paused the game makes the placeholder window modal, which is what stopped the other windows taking the
// mouse in the .wnd.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
class RmlInGamePopupScreen
{
public:
	static RmlInGamePopupScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	void open();
	void close();
	bool isVisible() const;
	bool onKey(unsigned char key, unsigned short state); ///< Enter and Escape: same as InGamePopupMessageInput

private:
	RmlInGamePopupScreen() {}

	void load(Rml::Context *context);
	void refresh();

	void onOk(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);

	struct Model
	{
		Rml::String message;
		Rml::String textColor = "#FFFFFF";
		Rml::String leftPx = "0px"; ///< the panel's place, in pixels like the script's
		Rml::String topPx = "0px";
		Rml::String widthPx = "50px";
	};

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;

	Model m_model;
};

// Registry entry points (see RmlUiManager::init()).
void OpenRmlInGamePopupScreen();
void CloseRmlInGamePopupScreen();
