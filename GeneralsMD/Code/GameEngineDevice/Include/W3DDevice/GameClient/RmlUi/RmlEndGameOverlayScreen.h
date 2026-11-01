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
// FILE: RmlEndGameOverlayScreen.h ///////////////////////////////////////////
// RmlScreen for Data/UI/EndGameOverlay.rml: the static mapped-image splash
// shown by ScriptActions::doVictory()/doDefeat()/doLocalDefeat() (Victorious.wnd,
// Defeat.wnd, LocalDefeat.wnd, ObserverQuit.wnd). All four .wnd files are just a
// bordered panel behind one full-size IMAGE with no buttons/callbacks -- the
// mission ends and this window is destroyed by ScriptEngine's end-game/close-
// window frame timer (see ScriptEngine::update()), never by user input. One
// shared instance covers all four; only the mapped image name differs.
// Not routed through RmlUiManager::showScreen() (same independent-overlay
// reasoning as RmlQuitMenuScreen): this is created via winCreateFromScript()
// while gameplay is still visible behind it, not a shell screen swap.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; }

//-------------------------------------------------------------------------------------------------
class RmlEndGameOverlayScreen
{
public:
	static RmlEndGameOverlayScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	void open(const char *mappedImageName);
	void close();
	bool isVisible() const;

private:
	RmlEndGameOverlayScreen() {}

	void load(Rml::Context *context);

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;
	Rml::String m_imageName;
};

// Registry entry points, one pair per .wnd path (see GameWindowManagerScript.cpp's
// winCreateFromScript() and RmlUiManager::init()).
void OpenRmlVictoriousScreen();
void CloseRmlVictoriousScreen();
void OpenRmlDefeatScreen();
void CloseRmlDefeatScreen();
void OpenRmlLocalDefeatScreen();
void CloseRmlLocalDefeatScreen();
void OpenRmlObserverQuitScreen();
void CloseRmlObserverQuitScreen();
