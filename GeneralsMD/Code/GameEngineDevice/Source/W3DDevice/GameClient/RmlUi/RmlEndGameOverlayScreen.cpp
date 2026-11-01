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
#include "W3DDevice/GameClient/RmlUi/RmlEndGameOverlayScreen.h"

#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

//-------------------------------------------------------------------------------------------------
RmlEndGameOverlayScreen &RmlEndGameOverlayScreen::instance()
{
	static RmlEndGameOverlayScreen s_screen;
	return s_screen;
}

void RmlEndGameOverlayScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("endgameoverlay");
	if (constructor)
	{
		constructor.Bind("image_name", &m_imageName);
		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/EndGameOverlay.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

void RmlEndGameOverlayScreen::open(const char *mappedImageName)
{
	if (!TheRmlUiManager)
		return;

	load(TheRmlUiManager->getContext());
	if (!m_document)
		return;

	m_imageName = mappedImageName ? mappedImageName : "";
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("image_name");

	m_document->Show();
}

void RmlEndGameOverlayScreen::close()
{
	if (m_document)
		m_document->Hide();
}

bool RmlEndGameOverlayScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

//-------------------------------------------------------------------------------------------------
// Registry entry points: one open per .wnd path, all sharing the one document/instance. Image
// names match each .wnd's ENABLEDDRAWDATA IMAGE field (see Data/INI/MappedImages).
void OpenRmlVictoriousScreen() { RmlEndGameOverlayScreen::instance().open("Victorious"); }
void CloseRmlVictoriousScreen() { RmlEndGameOverlayScreen::instance().close(); }

void OpenRmlDefeatScreen() { RmlEndGameOverlayScreen::instance().open("Defeated"); }
void CloseRmlDefeatScreen() { RmlEndGameOverlayScreen::instance().close(); }

void OpenRmlLocalDefeatScreen() { RmlEndGameOverlayScreen::instance().open("Defeated"); }
void CloseRmlLocalDefeatScreen() { RmlEndGameOverlayScreen::instance().close(); }

void OpenRmlObserverQuitScreen() { RmlEndGameOverlayScreen::instance().open("GameOver"); }
void CloseRmlObserverQuitScreen() { RmlEndGameOverlayScreen::instance().close(); }
