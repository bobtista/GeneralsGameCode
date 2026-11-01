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
#include "W3DDevice/GameClient/RmlUi/RmlNetworkDirectConnectScreen.h"

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "Common/UnicodeUtf8.h"
#include "GameClient/GUI/GUICallbacks/Menus/DirectConnectActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/DirectConnectData.h"
#include "GameClient/Shell.h"
#include "W3DDevice/GameClient/RmlUi/RmlTransitionSounds.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <windows.h>

extern Bool LANbuttonPushed; // NetworkDirectConnect.cpp; see RmlNetworkDirectConnectScreen::onBack()

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RmlNetworkDirectConnectScreen &RmlNetworkDirectConnectScreen::instance()
{
	static RmlNetworkDirectConnectScreen s_screen;
	return s_screen;
}

void RmlNetworkDirectConnectScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("networkdirectconnect");
	if (constructor)
	{
		m_hq.bind(constructor);
		Rml::StructHandle<RemoteIpRowModel> rowHandle = constructor.RegisterStruct<RemoteIpRowModel>();
		if (rowHandle)
		{
			rowHandle.RegisterMember("index", &RemoteIpRowModel::index);
			rowHandle.RegisterMember("text", &RemoteIpRowModel::text);
			rowHandle.RegisterMember("is_selected", &RemoteIpRowModel::isSelected);
			rowHandle.RegisterMember("used", &RemoteIpRowModel::used);
		}
		constructor.RegisterArray<Rml::Vector<RemoteIpRowModel>>();

		constructor.Bind("lan_name", &m_model.playerName);
		constructor.Bind("local_ip", &m_model.localIp);
		constructor.Bind("remote_ip_text", &m_model.remoteIpText);
		constructor.Bind("selected_history_index", &m_model.selectedHistoryIndex);
		constructor.Bind("remote_ip_history", &m_model.remoteIpHistory);

		constructor.BindEventCallback("host_game", &RmlNetworkDirectConnectScreen::onHostGame, this);
		constructor.BindEventCallback("join_game", &RmlNetworkDirectConnectScreen::onJoinGame, this);
		constructor.BindEventCallback("back", &RmlNetworkDirectConnectScreen::onBackPressed, this);
		constructor.BindEventCallback("select_remote_ip", &RmlNetworkDirectConnectScreen::onSelectRemoteIp, this);
		constructor.BindEventCallback("remote_ip_text_changed", &RmlNetworkDirectConnectScreen::onRemoteIpTextChanged, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/NetworkDirectConnect.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
void RmlNetworkDirectConnectScreen::show()
{
	if (!m_document)
		return;

	m_model.playerName = unicodeToUtf8(DirectConnectActions::enterDirectConnect());
	m_model.localIp = unicodeToUtf8(DirectConnectActions::localIPString());
	refreshHistory(); // also sets remote_ip_text/selected_history_index (see comment there)

	if (m_modelHandle)
		m_modelHandle.DirtyAllVariables();

	m_hq.refresh(m_modelHandle, true);
	m_document->Show();

	// NetworkDirectConnectInit()'s entrance group, and NetworkDirectConnectShutdown()'s reverse in hide().
	TransitionSounds::play("NetworkDirectConnectFade");
}

void RmlNetworkDirectConnectScreen::hide()
{
	if (m_document && m_document->IsVisible())
		TransitionSounds::play("NetworkDirectConnectFade", TRUE);
	if (m_document)
		m_document->Hide();

	// No unconditional name-commit/TheLAN teardown here: the .wnd path never does either on a plain
	// hide -- only Host/Join/Back write the UserName pref (see DirectConnectActions.h), and
	// NetworkDirectConnectShutdown() never touches prefs or TheLAN. Unlike RmlLanLobbyScreen::hide(),
	// there is no leaveLobby()-equivalent to call.
}

bool RmlNetworkDirectConnectScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

void RmlNetworkDirectConnectScreen::onBack()
{
	// Mirrors ButtonBack's GBM_SELECTED handler exactly: commit the name, then pop. Unlike
	// RmlLanLobbyScreen::onBack(), the .wnd path never deletes TheLAN here.
	DirectConnectActions::commitPlayerName(utf8ToUnicode(m_model.playerName));
	LANbuttonPushed = true;
	TheShell->pop();
}

//-------------------------------------------------------------------------------------------------
// Mirrors PopulateRemoteIPComboBox(): full rebuild from LANPreferences, first entry preselected.
void RmlNetworkDirectConnectScreen::refreshHistory()
{
	std::vector<UnicodeString> history = DirectConnectData::loadRemoteIPHistory();

	m_historyRows.beginUpdate();
	int i = 0;
	for (const UnicodeString &entry : history)
	{
		RemoteIpRowModel &row = m_historyRows.next();
		row.index = i;
		row.text = unicodeToUtf8(entry);
		row.isSelected = (i == 0);
		++i;
	}
	m_historyRows.endUpdate();

	if (!history.empty())
	{
		m_model.remoteIpText = unicodeToUtf8(history[0]);
		m_model.selectedHistoryIndex = 0;
	}
	else
	{
		m_model.remoteIpText.clear();
		m_model.selectedHistoryIndex = -1;
	}

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("remote_ip_history");
		m_modelHandle.DirtyVariable("remote_ip_text");
		m_modelHandle.DirtyVariable("selected_history_index");
	}
}

//-------------------------------------------------------------------------------------------------
void RmlNetworkDirectConnectScreen::onHostGame(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	DirectConnectActions::hostGame(utf8ToUnicode(m_model.playerName));
}

void RmlNetworkDirectConnectScreen::onJoinGame(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	// comboEntries mirrors what the .wnd path's own combobox-enumeration loop would have built (see
	// NetworkDirectConnect.cpp's currentRemoteIPComboEntries()): every currently-live history row, in
	// order.
	std::vector<UnicodeString> comboEntries;
	for (const RemoteIpRowModel &row : m_model.remoteIpHistory)
	{
		if (row.used)
			comboEntries.push_back(utf8ToUnicode(row.text));
	}

	DirectConnectActions::joinGame(utf8ToUnicode(m_model.remoteIpText), comboEntries,
		m_model.selectedHistoryIndex, utf8ToUnicode(m_model.playerName));

	refreshHistory();
}

void RmlNetworkDirectConnectScreen::onBackPressed(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	onBack();
}

void RmlNetworkDirectConnectScreen::onSelectRemoteIp(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	// Mirrors clicking/selecting an entry in the .wnd's editable combobox.
	int index = args[0].Get<int>();
	if (index < 0 || index >= m_historyRows.liveCount())
		return;

	m_model.remoteIpText = m_model.remoteIpHistory[index].text;
	m_model.selectedHistoryIndex = index;

	for (RemoteIpRowModel &row : m_model.remoteIpHistory)
		row.isSelected = row.used && (row.index == index);

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("remote_ip_text");
		m_modelHandle.DirtyVariable("selected_history_index");
		m_modelHandle.DirtyVariable("remote_ip_history");
	}
}

void RmlNetworkDirectConnectScreen::onRemoteIpTextChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	// Mirrors the native editable combobox falling back to "no selection" (GadgetComboBoxGetSelectedPos()
	// returns -1) once the displayed text no longer matches the selected entry's text.
	if (m_model.selectedHistoryIndex >= 0 && m_model.selectedHistoryIndex < (int)m_model.remoteIpHistory.size()
		&& m_model.remoteIpHistory[m_model.selectedHistoryIndex].text == m_model.remoteIpText)
		return;

	m_model.selectedHistoryIndex = -1;
	for (RemoteIpRowModel &row : m_model.remoteIpHistory)
		row.isSelected = false;

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("selected_history_index");
		m_modelHandle.DirtyVariable("remote_ip_history");
	}
}

//-------------------------------------------------------------------------------------------------
void OpenRmlNetworkDirectConnectScreen()
{
	if (!TheRmlUiManager)
		return;
	// Ronin @feature 31/10/2026 DX9: our shell's push / pop closes the screen before; no current-screen slot here.
	RmlNetworkDirectConnectScreen &screen = RmlNetworkDirectConnectScreen::instance();
	screen.load(TheRmlUiManager->getContext());
	screen.show();
}

void CloseRmlNetworkDirectConnectScreen()
{
	if (RmlNetworkDirectConnectScreen::instance().isVisible())
		RmlNetworkDirectConnectScreen::instance().hide();
}

//-------------------------------------------------------------------------------------------------
void RmlNetworkDirectConnectScreen::update()
{
	m_hq.refresh(m_modelHandle);
}
