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
// FILE: RmlDownloadScreen.h //////////////////////////////////////////////////
// RmlUi view of Menus/DownloadMenu.wnd, the panel that shows the Generals Online update download
// (file, size, status, progress, Cancel). All the rules live in DownloadMenuActions/DownloadMenuData;
// this only mirrors DownloadMenuData into a data model whenever DownloadMenuSignals::changed() fires.
//
// The patch check creates the .wnd through winCreateLayout(), which hands back a placeholder layout for
// a registered path: its runInit() opens this through the registry and destroying it (Cancel, or the
// message boxes at the end of the update) closes it. It is modal, like the RmlUi popups; the .wnd only
// took the keyboard focus for its Escape key.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Signal.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
class RmlDownloadScreen
{
public:
	static RmlDownloadScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	void open();
	void close();
	bool isVisible() const;
	void back(); ///< Escape: same as ButtonCancel (KEY_ESC in DownloadMenu.cpp)

	// Per-frame pump while visible; see RmlUiManager::update().
	static void tick();

private:
	RmlDownloadScreen() {}

	void load(Rml::Context *context);
	void refresh();

	void onCancel(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);

	struct Model
	{
		Rml::String fileText;
		Rml::String sizeText;
		Rml::String timeText;
		Rml::String statusText;
		Rml::String progressStyle = "0%"; ///< the bar's width
	};

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;
	SignalConnection m_dataConnection;

	Model m_model;
	bool m_dirty = false; ///< DownloadMenuData changed since the last refresh
};

// Registry entry points (see RmlUiManager::init()).
void OpenRmlDownloadScreen();
void CloseRmlDownloadScreen();
