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
// FILE: RmlLoadScreen.h //////////////////////////////////////////////////////
// RmlUi view of the load screens (Menus/MapTransferScreen.wnd, Menus/MultiplayerLoadScreen.wnd,
// Menus/GameSpyLoadScreen.wnd, Menus/ShellGameLoadScreen.wnd, Menus/SinglePlayerLoadScreen.wnd,
// Menus/ChallengeLoadScreen.wnd). The LoadScreen classes keep all the logic and write a LoadScreenData; this only mirrors that
// into a data model whenever its version changes. One instance per document,
// created by winCreateFromScript() through RmlUiScreenRegistry and closed when
// the LoadScreen destroys its placeholder window. RmlUiManager::update() calls
// tick(): load screens block the main loop, so nothing else would refresh them.
// The LoadScreen decodes the movies; their buffers are published to <video> elements as sources named
// loadscreen-<slot> (see refresh()).
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; }

//-------------------------------------------------------------------------------------------------
class RmlLoadScreen
{
public:
	enum Kind { KIND_MAP_TRANSFER, KIND_MULTIPLAYER, KIND_ONLINE, KIND_SHELL, KIND_SINGLE_PLAYER, KIND_CHALLENGE, KIND_COUNT };

	static RmlLoadScreen &instance(Kind kind);
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }
	static void tick(); ///< refreshes every open instance whose LoadScreenData changed

	void open();
	void close();
	bool isVisible() const;

	RmlLoadScreen(const char *document, const char *modelName) : m_documentPath(document), m_modelName(modelName) {}

private:
	void load(Rml::Context *context);
	void refresh();

	struct RowModel
	{
		Rml::String name;
		Rml::String side;
		Rml::String team;
		Rml::String winLoss;
		Rml::String disconnects;
		Rml::String status;
		Rml::String rankImage;
		Rml::String medalImage;
		Rml::String sideImage;
		bool hasSideImage = false;
		Rml::String colorHex = "#FFFFFF";
		Rml::String progressStyle = "0%"; ///< "42%", bound via data-style-width
		bool used = false; ///< the array is always MAX_SLOTS long; unused rows are hidden
		bool showProgress = true;
		bool showStats = true;
		bool hasRank = false;
		bool hasMedal = false;
	};

	struct MarkerModel
	{
		Rml::String xStyle = "0%"; ///< "12.500%", bound via data-style-left
		Rml::String yStyle = "0%";
		Rml::String label;
		Rml::String colorHex = "#FFFFFF";
		bool used = false;
		bool hasLabel = false;
	};

	struct UnitModel
	{
		Rml::String name;
		bool shown = false;
	};

	struct GeneralModel
	{
		Rml::String bigName;
		Rml::String name;
		Rml::String rank;
		Rml::String strategy;
		Rml::String portrait;
		bool hasPortrait = false;
	};

	const char *m_documentPath;
	const char *m_modelName;
	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;
	unsigned int m_seenVersion = 0;

	Rml::Vector<RowModel> m_rows;
	Rml::Vector<MarkerModel> m_markers;
	Rml::String m_localName;
	Rml::String m_localFeatures;
	Rml::String m_localPortrait;
	Rml::String m_mapName;
	Rml::String m_mapDisplayName;
	Rml::String m_gameMode;
	Rml::String m_currentFile;
	Rml::String m_timeout;
	Rml::String m_progressStyle = "0%";
	bool m_titleScreen = false;
	bool m_hasPortrait = false;
	bool m_hasMap = false;

	// single player
	Rml::String m_backgroundImage;
	bool m_hasBackground = false;
	bool m_hasMovie = false; ///< the backdrop movie is up
	int m_barColor = -1;
	bool m_showObjectives = false;
	Rml::Vector<Rml::String> m_objectiveLines;
	Rml::Vector<UnitModel> m_units;
	Rml::String m_location;
	bool m_showLocation = false;

	// challenge
	Rml::Vector<GeneralModel> m_generals;
	bool m_showBioTitles = false;
	bool m_showBioEntries = false;
	bool m_showPortraitMovies = false;
	bool m_showPortraits = false;
	bool m_showOuterCircle = false;
	bool m_showInnerCircle = false;
	bool m_showVersusBackdrop = false;
	bool m_showVersus = false;
};

// Registry entry points (see RmlUiManager::init()).
void OpenRmlMapTransferScreen();
void CloseRmlMapTransferScreen();
void OpenRmlMultiplayerLoadScreen();
void CloseRmlMultiplayerLoadScreen();
void OpenRmlOnlineLoadScreen();
void CloseRmlOnlineLoadScreen();
void OpenRmlShellLoadScreen();
void CloseRmlShellLoadScreen();
void OpenRmlSinglePlayerLoadScreen();
void CloseRmlSinglePlayerLoadScreen();
void OpenRmlChallengeLoadScreen();
void CloseRmlChallengeLoadScreen();
