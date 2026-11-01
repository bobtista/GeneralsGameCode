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
// FILE: RmlLanMapSelectScreen.h /////////////////////////////////////////////////
// RmlScreen for Data/UI/LanMapSelect.rml. Registered for Menus/LanMapSelectMenu.wnd,
// opened by RmlLanGameSetupScreen's "Select Map" button (RmlUiScreenRegistry::open()),
// same as LanGameOptionsMenu.cpp's ButtonSelectMap -> winCreateLayout() flow. Not
// routed through RmlUiManager::showScreen()'s single-current-screen swap -- like
// RmlSkirmishMapSelectScreen it stays independent of the screen underneath, which it
// hides while open and re-shows (refreshed, not reset) on OK/Back via
// ReturnToRmlLanGameSetupScreen(). The map list itself is widget-agnostic already
// (MapUtil.h's buildFilteredMapList()); the filter default lives in MapSelectActions
// (shared with SkirmishMapSelectMenu.cpp/RmlSkirmishMapSelectScreen), and the OK
// write-back into TheLAN's LANGameInfo lives in LanGameSetupActions::applySelectedMap,
// shared with LanMapSelectMenu.cpp's .wnd ButtonOK handler.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "W3DDevice/GameClient/RmlUi/RmlGrowOnlyList.h"
#include "W3DDevice/GameClient/RmlUi/RmlHqStatus.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
class RmlLanMapSelectScreen
{
public:
	static RmlLanMapSelectScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	void open();
	void close();
	bool isVisible() const;
	void back(); ///< Escape: same as ButtonBack (KEY_ESC in LanMapSelectMenu.cpp)

private:
	RmlLanMapSelectScreen() : m_mapRows(m_model.maps) {}

	void load(Rml::Context *context);
	void refreshMapList(); // rebuilds m_model.maps (buildFilteredMapList()) for the current filter
	void refreshStartMarkers(); // rebuilds m_model.startMarkers for m_model.selectedMapName
	void selectMap(const Rml::String &mapName); // OK/double-click: write back + return to setup

	void onFilterChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onPlayerFilterChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // (players), 0 = any
	void refreshVisibleCount(); // applies the player filter and the search, and counts what is left
	void onSearchChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // search box, every keystroke
	void onSortChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // (1: by players, 0: by name)
	void onMapSelected(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onMapActivated(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // double-click: select + OK
	void onOk(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onBack(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;
	RmlHqStatus m_hq; // .hq-header status

	// One row in the map-list (see GameSetup.rcss's reusable "map-row" markup block).
	struct MapEntryModel
	{
		Rml::String mapName; // map cache key, also the click/select/OK value
		Rml::String displayName;
		int numPlayers = 0;
		bool hasStarImage = false;
		Rml::String starImage; // TheMappedImageCollection name, valid only if hasStarImage
		bool isSelected = false;
		bool used = true; // grow-only storage, see RmlGrowOnlyList.h; hidden via data-if when false
		bool visible = true; // passes the player filter and the search
	};

	// One start-position marker on the preview (see GameSetupStartPositionMarker); no occupant/color
	// fields here -- positionStartSpots() draws the same generic marker image for every start spot
	// on the browse preview, unlike the setup screen's per-slot-colored markers.
	struct StartMarkerModel
	{
		int position = 0;
		Rml::String xStyle; // e.g. "12.500%", bound via data-style-left
		Rml::String yStyle; // e.g. "34.200%", bound via data-style-top
		bool used = false; // MAX_SLOTS entries always; see GameSetupStartPositionMarker::m_used
	};

	struct Model
	{
		Rml::Vector<MapEntryModel> maps;
		bool useSystemMaps = true;

		Rml::String selectedMapName;
		Rml::String selectedDisplayName;
		bool hasSelection = false;
		int selectedNumPlayers = 0;

		Rml::Vector<StartMarkerModel> startMarkers;
		int playerFilter = 0; // show only maps for this many players; 0 = any
		int visibleCount = 0; // live maps that pass playerFilter and the search, for the empty state
		Rml::String searchText; // the search box: a case-insensitive part of the name
		bool sortByPlayers = false; // order by player count instead of name
		Rml::String mapCountText; // "n of N maps"
	} m_model;

	// Grow-only wrapper around m_model.maps (see RmlGrowOnlyList.h): the filter toggle can rebuild
	// this list with a different count while the document stays open, so its storage never shrinks.
	RmlGrowOnlyList<MapEntryModel> m_mapRows;
};

// Registry entry point (see RmlUiManager::init()).
void OpenRmlLanMapSelectScreen();
void CloseRmlLanMapSelectScreen();
