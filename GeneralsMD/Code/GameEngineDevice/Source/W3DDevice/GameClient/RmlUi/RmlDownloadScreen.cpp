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
#include "W3DDevice/GameClient/RmlUi/RmlDownloadScreen.h"
#include "Common/UnicodeUtf8.h"

#include "GameClient/RmlUiScreenRegistry.h"
#include "GameClient/GUI/GUICallbacks/Menus/DownloadMenuActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/DownloadMenuData.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <cstdio>
#include <windows.h>

namespace
{
	const char *const kWndPath = "Menus/DownloadMenu.wnd";
}

//-------------------------------------------------------------------------------------------------
RmlDownloadScreen &RmlDownloadScreen::instance()
{
	static RmlDownloadScreen s_screen;
	return s_screen;
}

void RmlDownloadScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("downloadscreen");
	if (constructor)
	{
		constructor.Bind("file_text", &m_model.fileText);
		constructor.Bind("size_text", &m_model.sizeText);
		constructor.Bind("time_text", &m_model.timeText);
		constructor.Bind("status_text", &m_model.statusText);
		constructor.Bind("progress_style", &m_model.progressStyle);

		constructor.BindEventCallback("cancel", &RmlDownloadScreen::onCancel, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/DownloadScreen.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
void RmlDownloadScreen::refresh()
{
	const DownloadMenuData &data = DownloadMenuData::instance();

	m_model.fileText = unicodeToUtf8(data.m_file);
	m_model.sizeText = unicodeToUtf8(data.m_size);
	m_model.timeText = unicodeToUtf8(data.m_time);
	m_model.statusText = unicodeToUtf8(data.m_status);

	// the bar cannot draw past its ends
	char percent[16];
	_snprintf_s(percent, sizeof(percent), _TRUNCATE, "%d%%", data.m_percent < 0 ? 0 : (data.m_percent > 100 ? 100 : data.m_percent));
	m_model.progressStyle = percent;

	m_dirty = false;

	if (m_modelHandle)
		m_modelHandle.DirtyAllVariables();
}

void RmlDownloadScreen::open()
{
	if (!TheRmlUiManager)
		return;

	// A placeholder layout opens the screen from runInit(), hide(FALSE) and bringForward() alike.
	if (isVisible())
		return;

	load(TheRmlUiManager->getContext());
	if (!m_document)
		return;

	m_dataConnection = DownloadMenuSignals::changed().connect([this]() { m_dirty = true; });
	DownloadMenuActions::open(RmlUiScreenRegistry::layoutFor(kWndPath));
	refresh();

	// modal, but the keyboard stays with the document: only Escape does anything, as in the .wnd
	m_document->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
}

void RmlDownloadScreen::close()
{
	if (!isVisible())
		return;

	m_document->Hide();
	m_dataConnection.disconnect();
	DownloadMenuActions::close();
}

bool RmlDownloadScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

void RmlDownloadScreen::tick()
{
	RmlDownloadScreen &screen = instance();
	if (!screen.isVisible())
		return;

	DownloadMenuActions::update();
	if (screen.m_dirty)
		screen.refresh();
}

//-------------------------------------------------------------------------------------------------
void RmlDownloadScreen::onCancel(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	DownloadMenuActions::cancel();
}

void RmlDownloadScreen::back()
{
	DownloadMenuActions::cancel();
}

//-------------------------------------------------------------------------------------------------
void OpenRmlDownloadScreen() { RmlDownloadScreen::instance().open(); }
void CloseRmlDownloadScreen() { RmlDownloadScreen::instance().close(); }
