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
#include "W3DDevice/GameClient/RmlUi/RmlInGamePopupScreen.h"
#include "Common/UnicodeUtf8.h"

#include "GameClient/GameWindow.h"
#include "GameClient/RmlUiScreenRegistry.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/GUI/GUICallbacks/Menus/InGamePopupMessageActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/InGamePopupMessageData.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <cstdio>
#include <windows.h>

namespace
{
	const char *const kWndPath = "InGamePopupMessage.wnd";

	Rml::String pixels(int value)
	{
		char text[32];
		_snprintf_s(text, sizeof(text), _TRUNCATE, "%dpx", value);
		return Rml::String(text);
	}
}

//-------------------------------------------------------------------------------------------------
RmlInGamePopupScreen &RmlInGamePopupScreen::instance()
{
	static RmlInGamePopupScreen s_screen;
	return s_screen;
}

void RmlInGamePopupScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("ingamepopup");
	if (constructor)
	{
		constructor.Bind("message", &m_model.message);
		constructor.Bind("text_color", &m_model.textColor);
		constructor.Bind("left_px", &m_model.leftPx);
		constructor.Bind("top_px", &m_model.topPx);
		constructor.Bind("width_px", &m_model.widthPx);

		constructor.BindEventCallback("ok", &RmlInGamePopupScreen::onOk, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/InGamePopupMessage.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
void RmlInGamePopupScreen::refresh()
{
	const InGamePopupMessageData &data = InGamePopupMessageData::instance();

	m_model.message = unicodeToUtf8(data.m_message);

	char color[8];
	_snprintf_s(color, sizeof(color), _TRUNCATE, "#%02X%02X%02X", (data.m_textColor >> 16) & 0xFF, (data.m_textColor >> 8) & 0xFF, data.m_textColor & 0xFF);
	m_model.textColor = color;

	m_model.leftPx = pixels(data.m_x);
	m_model.topPx = pixels(data.m_y);
	m_model.widthPx = pixels(data.m_width);

	if (m_modelHandle)
		m_modelHandle.DirtyAllVariables();
}

void RmlInGamePopupScreen::open()
{
	if (!TheRmlUiManager)
		return;

	load(TheRmlUiManager->getContext());
	if (!m_document)
		return;

	WindowLayout *layout = RmlUiScreenRegistry::layoutFor(kWndPath);
	if (!InGamePopupMessageActions::open(layout ? layout->getFirstWindow() : nullptr))
		return;

	refresh();

	// no focus for the document: the keys it wants come through onKey(), the game keeps the rest
	m_document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}

void RmlInGamePopupScreen::close()
{
	if (m_document)
		m_document->Hide();
}

bool RmlInGamePopupScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

//-------------------------------------------------------------------------------------------------
void RmlInGamePopupScreen::onOk(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	InGamePopupMessageActions::ok();
}

bool RmlInGamePopupScreen::onKey(unsigned char key, unsigned short state)
{
	return InGamePopupMessageActions::key(key, (unsigned char)state);
}

//-------------------------------------------------------------------------------------------------
void OpenRmlInGamePopupScreen() { RmlInGamePopupScreen::instance().open(); }
void CloseRmlInGamePopupScreen() { RmlInGamePopupScreen::instance().close(); }
