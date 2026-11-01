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
// FILE: DirectConnectData.h //////////////////////////////////////////////////////
// Widget-agnostic remote-IP history content: the ordered list PopulateRemoteIPComboBox()
// has always read from LANPreferences, with no GadgetComboBox coupling, so a non-.wnd
// front end (RmlNetworkDirectConnectScreen) can rebuild the same combobox contents.
// Mirrors LanLobbyData.h's role for the LAN lobby.
///////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/UnicodeString.h"

#include <vector>

namespace DirectConnectData
{
	// Mirrors PopulateRemoteIPComboBox()'s LANPreferences read (minus GadgetComboBoxReset/AddEntry).
	// Entry 0 (if any) is the caller's default selection, same as the .wnd path's SetSelectedPos(0).
	std::vector<UnicodeString> loadRemoteIPHistory();
}
