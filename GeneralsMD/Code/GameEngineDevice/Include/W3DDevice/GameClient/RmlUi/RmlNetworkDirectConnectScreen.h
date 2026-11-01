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
// FILE: RmlNetworkDirectConnectScreen.h ///////////////////////////////////////
// RmlScreen for Data/UI/NetworkDirectConnect.rml. Registered for
// Menus/NetworkDirectConnect.wnd (RmlLanLobbyScreen::onDirectConnect() pushes
// that path). Same registry-routed pattern as RmlLanLobbyScreen: no
// NetworkDirectConnectInit()/Shutdown()/Update() callback ever runs, so:
//   - show() calls DirectConnectActions::enterDirectConnect() itself (TheLAN
//     create/reset, local IP resolve, default name) instead of
//     NetworkDirectConnectInit().
//   - hide() does nothing extra: the original .wnd path never commits the
//     player name or touches TheLAN on a generic hide either -- only
//     Host/Join/Back do (see onHostGame()/onJoinGame()/onBack()).
//   - update() is unimplemented (default no-op): the original
//     NetworkDirectConnectUpdate() never pumps TheLAN->update() either (unlike
//     LanLobbyMenuUpdate()/RmlLanLobbyScreen::update()) -- this screen only
//     ever navigates away (Host/Join push LanGameOptionsMenu.wnd, which pumps
//     TheLAN itself once it's up) or backs out.
// The remote-IP combobox becomes a plain text input (remote_ip_text) plus a
// clickable history list (remote_ip_history, RmlGrowOnlyList-backed, see
// RmlGrowOnlyList.h) standing in for the .wnd's editable ComboBox: clicking a
// history row fills the text and marks it selected, same as
// GadgetComboBoxSetSelectedPos(); typing anything else clears the selection,
// same as the native combobox falling back to "no selection" once the text no
// longer matches an entry. selected_history_index mirrors
// GadgetComboBoxGetSelectedPos()'s -1-when-typed convention, and is exactly
// what DirectConnectActions::joinGame()/updateRemoteIPList() expect.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "W3DDevice/GameClient/RmlUi/RmlGrowOnlyList.h"
#include "W3DDevice/GameClient/RmlUi/RmlHqStatus.h"
#include "W3DDevice/GameClient/RmlUi/RmlScreen.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
// Ronin @feature 31/10/2026 DX9: standalone, as his other screens are: our RmlScreen has other methods.
class RmlNetworkDirectConnectScreen
{
public:
	static RmlNetworkDirectConnectScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	// RmlScreen ----------------------------------------------------------------------------
	virtual void load(Rml::Context *context);
	virtual void show();
	virtual void hide();
	virtual bool isVisible() const;
	virtual void onBack(); // same as ButtonBack (see NetworkDirectConnect.cpp's GBM_SELECTED)
	virtual void update(); // ticks the .hq-header status

private:
	RmlNetworkDirectConnectScreen() : m_historyRows(m_model.remoteIpHistory) {}

	void refreshHistory(); // rebuilds remote_ip_history from DirectConnectData::loadRemoteIPHistory()

	void onHostGame(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onJoinGame(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onBackPressed(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSelectRemoteIp(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args); // history row click
	void onRemoteIpTextChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // typing clears selection

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;
	RmlHqStatus m_hq; // .hq-header status

	// One row of the remote-IP history list (see refreshHistory()).
	struct RemoteIpRowModel
	{
		int index = 0;
		Rml::String text;
		bool isSelected = false;
		bool used = true; // grow-only storage, see RmlGrowOnlyList.h; hidden via data-if when false
	};

	struct Model
	{
		Rml::String playerName;
		Rml::String localIp;
		Rml::String remoteIpText;
		int selectedHistoryIndex = -1; // -1 = typed text doesn't match any history entry
		Rml::Vector<RemoteIpRowModel> remoteIpHistory;
	} m_model;

	// Grow-only wrapper around m_model.remoteIpHistory (see RmlGrowOnlyList.h); bound directly to the
	// rml remote_ip_history array, so the underlying storage never shrinks while this screen is open.
	RmlGrowOnlyList<RemoteIpRowModel> m_historyRows;
};

// Registry entry point (see RmlUiManager::init()).
void OpenRmlNetworkDirectConnectScreen();
void CloseRmlNetworkDirectConnectScreen();
