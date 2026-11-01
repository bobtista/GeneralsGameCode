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
#include "W3DDevice/GameClient/RmlUi/RmlLoadScreen.h"
#include "Common/UnicodeUtf8.h"

#include "GameClient/LoadScreenData.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiElements.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cstdio>
#include <windows.h>

//-------------------------------------------------------------------------------------------------
static Rml::String rgbToHex(UnsignedInt rgb)
{
	char hex[8];
	_snprintf_s(hex, sizeof(hex), _TRUNCATE, "#%02X%02X%02X", (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
	return Rml::String(hex);
}

static Rml::String percentStyle(float percent)
{
	char buf[16];
	_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.3f%%", percent);
	return Rml::String(buf);
}

//-------------------------------------------------------------------------------------------------
RmlLoadScreen &RmlLoadScreen::instance(Kind kind)
{
	static RmlLoadScreen s_screens[KIND_COUNT] = {
		RmlLoadScreen("Data/UI/MapTransfer.rml", "maptransfer"),
		RmlLoadScreen("Data/UI/MultiplayerLoad.rml", "multiplayerload"),
		RmlLoadScreen("Data/UI/GameSpyLoad.rml", "onlineload"),
		RmlLoadScreen("Data/UI/ShellGameLoad.rml", "shellload"),
		RmlLoadScreen("Data/UI/SinglePlayerLoadScreen.rml", "singleplayerload"),
		RmlLoadScreen("Data/UI/ChallengeLoadScreen.rml", "challengeload"),
	};
	return s_screens[kind];
}

// <video source="..."> names of the LoadScreenData movies, by LoadScreenVideo slot.
static const char *const s_videoSources[LOAD_VIDEO_COUNT] = {
	"loadscreen-background",
	"loadscreen-portrait-left",
	"loadscreen-portrait-right",
	"loadscreen-versus",
};

void RmlLoadScreen::tick()
{
	for (int i = 0; i < KIND_COUNT; ++i)
	{
		RmlLoadScreen &screen = instance((Kind)i);
		if (screen.isVisible() && screen.m_seenVersion != LoadScreenData::instance().m_version)
			screen.refresh();
	}
}

void RmlLoadScreen::load(Rml::Context *context)
{
	if (m_document)
		return;

	m_context = context;
	m_rows.assign(MAX_SLOTS, RowModel());
	m_markers.assign(MAX_SLOTS, MarkerModel());
	m_objectiveLines.assign(MAX_OBJECTIVE_LINES, Rml::String());
	m_units.assign(MAX_DISPLAYED_UNITS, UnitModel());
	m_generals.assign(2, GeneralModel());

	Rml::DataModelConstructor constructor = context->CreateDataModel(m_modelName);
	if (constructor)
	{
		if (Rml::StructHandle<RowModel> rowHandle = constructor.RegisterStruct<RowModel>())
		{
			rowHandle.RegisterMember("name", &RowModel::name);
			rowHandle.RegisterMember("side", &RowModel::side);
			rowHandle.RegisterMember("team", &RowModel::team);
			rowHandle.RegisterMember("win_loss", &RowModel::winLoss);
			rowHandle.RegisterMember("disconnects", &RowModel::disconnects);
			rowHandle.RegisterMember("status", &RowModel::status);
			rowHandle.RegisterMember("rank_image", &RowModel::rankImage);
			rowHandle.RegisterMember("medal_image", &RowModel::medalImage);
			rowHandle.RegisterMember("side_image", &RowModel::sideImage);
			rowHandle.RegisterMember("has_side_image", &RowModel::hasSideImage);
			rowHandle.RegisterMember("color_hex", &RowModel::colorHex);
			rowHandle.RegisterMember("progress_style", &RowModel::progressStyle);
			rowHandle.RegisterMember("used", &RowModel::used);
			rowHandle.RegisterMember("show_progress", &RowModel::showProgress);
			rowHandle.RegisterMember("show_stats", &RowModel::showStats);
			rowHandle.RegisterMember("has_rank", &RowModel::hasRank);
			rowHandle.RegisterMember("has_medal", &RowModel::hasMedal);
		}
		if (Rml::StructHandle<MarkerModel> markerHandle = constructor.RegisterStruct<MarkerModel>())
		{
			markerHandle.RegisterMember("x_style", &MarkerModel::xStyle);
			markerHandle.RegisterMember("y_style", &MarkerModel::yStyle);
			markerHandle.RegisterMember("label", &MarkerModel::label);
			markerHandle.RegisterMember("color_hex", &MarkerModel::colorHex);
			markerHandle.RegisterMember("used", &MarkerModel::used);
			markerHandle.RegisterMember("has_label", &MarkerModel::hasLabel);
		}
		if (Rml::StructHandle<UnitModel> unitHandle = constructor.RegisterStruct<UnitModel>())
		{
			unitHandle.RegisterMember("name", &UnitModel::name);
			unitHandle.RegisterMember("shown", &UnitModel::shown);
		}
		constructor.RegisterArray<Rml::Vector<RowModel>>();
		constructor.RegisterArray<Rml::Vector<MarkerModel>>();
		constructor.RegisterArray<Rml::Vector<Rml::String>>();
		if (Rml::StructHandle<GeneralModel> generalHandle = constructor.RegisterStruct<GeneralModel>())
		{
			generalHandle.RegisterMember("big_name", &GeneralModel::bigName);
			generalHandle.RegisterMember("name", &GeneralModel::name);
			generalHandle.RegisterMember("rank", &GeneralModel::rank);
			generalHandle.RegisterMember("strategy", &GeneralModel::strategy);
			generalHandle.RegisterMember("portrait", &GeneralModel::portrait);
			generalHandle.RegisterMember("has_portrait", &GeneralModel::hasPortrait);
		}
		constructor.RegisterArray<Rml::Vector<UnitModel>>();
		constructor.RegisterArray<Rml::Vector<GeneralModel>>();

		constructor.Bind("rows", &m_rows);
		constructor.Bind("markers", &m_markers);
		constructor.Bind("local_name", &m_localName);
		constructor.Bind("local_features", &m_localFeatures);
		constructor.Bind("local_portrait", &m_localPortrait);
		constructor.Bind("has_portrait", &m_hasPortrait);
		constructor.Bind("map_name", &m_mapName);
		constructor.Bind("map_display_name", &m_mapDisplayName);
		constructor.Bind("game_mode", &m_gameMode);
		constructor.Bind("has_map", &m_hasMap);
		constructor.Bind("current_file", &m_currentFile);
		constructor.Bind("timeout", &m_timeout);
		constructor.Bind("progress_style", &m_progressStyle);
		constructor.Bind("title_screen", &m_titleScreen);
		constructor.Bind("background_image", &m_backgroundImage);
		constructor.Bind("has_background", &m_hasBackground);
		constructor.Bind("has_movie", &m_hasMovie);
		constructor.Bind("bar_color", &m_barColor);
		constructor.Bind("show_objectives", &m_showObjectives);
		constructor.Bind("objective_lines", &m_objectiveLines);
		constructor.Bind("units", &m_units);
		constructor.Bind("location", &m_location);
		constructor.Bind("show_location", &m_showLocation);
		constructor.Bind("generals", &m_generals);
		constructor.Bind("show_bio_titles", &m_showBioTitles);
		constructor.Bind("show_bio_entries", &m_showBioEntries);
		constructor.Bind("show_portrait_movies", &m_showPortraitMovies);
		constructor.Bind("show_portraits", &m_showPortraits);
		constructor.Bind("show_outer_circle", &m_showOuterCircle);
		constructor.Bind("show_inner_circle", &m_showInnerCircle);
		constructor.Bind("show_versus_backdrop", &m_showVersusBackdrop);
		constructor.Bind("show_versus", &m_showVersus);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen(m_documentPath);	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

void RmlLoadScreen::refresh()
{
	const LoadScreenData &data = LoadScreenData::instance();
	m_seenVersion = data.m_version;

	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		const LoadScreenPlayerRow &src = data.m_rows[i];
		RowModel &row = m_rows[i];
		row.used = i < data.m_rowCount;
		row.name = unicodeToUtf8(src.m_name);
		row.side = unicodeToUtf8(src.m_side);
		row.team = unicodeToUtf8(src.m_team);
		row.winLoss = unicodeToUtf8(src.m_winLoss);
		row.disconnects = unicodeToUtf8(src.m_disconnects);
		row.status = unicodeToUtf8(src.m_status);
		row.rankImage = src.m_rankImage.str();
		row.medalImage = src.m_medalImage.str();
		row.hasRank = !src.m_rankImage.isEmpty();
		row.hasMedal = !src.m_medalImage.isEmpty();
		row.sideImage = src.m_sideImage.str();
		row.hasSideImage = !src.m_sideImage.isEmpty();
		row.colorHex = rgbToHex(src.m_color);
		row.progressStyle = percentStyle((float)src.m_progress);
		row.showProgress = src.m_showProgress == TRUE;
		row.showStats = src.m_showStats == TRUE;

		const LoadScreenStartMarker &srcMarker = data.m_markers[i];
		MarkerModel &marker = m_markers[i];
		marker.used = srcMarker.m_used == TRUE;
		marker.xStyle = percentStyle(srcMarker.m_x * 100.0f);
		marker.yStyle = percentStyle(srcMarker.m_y * 100.0f);
		marker.hasLabel = srcMarker.m_slotNumber > 0;
		marker.colorHex = rgbToHex(srcMarker.m_color);
		char number[8];
		_snprintf_s(number, sizeof(number), _TRUNCATE, "%d", srcMarker.m_slotNumber);
		marker.label = marker.hasLabel ? number : "";
	}

	m_localName = unicodeToUtf8(data.m_localName);
	m_localFeatures = unicodeToUtf8(data.m_localFeatures);
	m_localPortrait = data.m_localPortrait.str();
	m_hasPortrait = !data.m_localPortrait.isEmpty();
	m_mapName = data.m_mapName.str();
	m_hasMap = !data.m_mapName.isEmpty();
	m_mapDisplayName = unicodeToUtf8(data.m_mapDisplayName);
	m_gameMode = unicodeToUtf8(data.m_gameMode);
	m_currentFile = unicodeToUtf8(data.m_currentFile);
	m_timeout = unicodeToUtf8(data.m_timeout);
	m_progressStyle = percentStyle((float)data.m_progress);
	m_titleScreen = data.m_titleScreen == TRUE;

	m_backgroundImage = data.m_backgroundImage.str();
	m_hasBackground = !data.m_backgroundImage.isEmpty();
	m_hasMovie = data.m_videos[LOAD_VIDEO_BACKGROUND] != nullptr;
	m_barColor = data.m_barColorIndex;
	m_showObjectives = data.m_showObjectives == TRUE;
	for (Int i = 0; i < MAX_OBJECTIVE_LINES; ++i)
		m_objectiveLines[i] = unicodeToUtf8(data.m_objectiveLines[i]);
	for (Int i = 0; i < MAX_DISPLAYED_UNITS; ++i)
	{
		m_units[i].name = unicodeToUtf8(data.m_unitNames[i]);
		m_units[i].shown = data.m_showUnit[i] == TRUE;
	}
	m_location = unicodeToUtf8(data.m_location);
	m_showLocation = data.m_showLocation == TRUE;

	for (Int i = 0; i < 2; ++i)
	{
		const LoadScreenGeneral &src = data.m_generals[i];
		GeneralModel &general = m_generals[i];
		general.bigName = unicodeToUtf8(src.m_bigName);
		general.name = unicodeToUtf8(src.m_name);
		general.rank = unicodeToUtf8(src.m_rank);
		general.strategy = unicodeToUtf8(src.m_strategy);
		general.portrait = src.m_portrait.str();
		general.hasPortrait = !src.m_portrait.isEmpty();
	}
	m_showBioTitles = data.m_showBioTitles == TRUE;
	m_showBioEntries = data.m_showBioEntries == TRUE;
	m_showPortraitMovies = data.m_showPortraitMovies == TRUE;
	m_showPortraits = data.m_showPortraits == TRUE;
	m_showOuterCircle = data.m_showOuterCircle == TRUE;
	m_showInnerCircle = data.m_showInnerCircle == TRUE;
	m_showVersusBackdrop = data.m_showVersusBackdrop == TRUE;
	m_showVersus = data.m_showVersus == TRUE;

	for (Int i = 0; i < LOAD_VIDEO_COUNT; ++i)
		RmlVideoElement::setSource(s_videoSources[i], data.m_videos[i]);

	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("rows");
		m_modelHandle.DirtyVariable("markers");
		m_modelHandle.DirtyVariable("local_name");
		m_modelHandle.DirtyVariable("local_features");
		m_modelHandle.DirtyVariable("local_portrait");
		m_modelHandle.DirtyVariable("has_portrait");
		m_modelHandle.DirtyVariable("map_name");
		m_modelHandle.DirtyVariable("has_map");
		m_modelHandle.DirtyVariable("map_display_name");
		m_modelHandle.DirtyVariable("game_mode");
		m_modelHandle.DirtyVariable("current_file");
		m_modelHandle.DirtyVariable("timeout");
		m_modelHandle.DirtyVariable("progress_style");
		m_modelHandle.DirtyVariable("title_screen");
		m_modelHandle.DirtyVariable("background_image");
		m_modelHandle.DirtyVariable("has_background");
		m_modelHandle.DirtyVariable("has_movie");
		m_modelHandle.DirtyVariable("bar_color");
		m_modelHandle.DirtyVariable("show_objectives");
		m_modelHandle.DirtyVariable("objective_lines");
		m_modelHandle.DirtyVariable("units");
		m_modelHandle.DirtyVariable("location");
		m_modelHandle.DirtyVariable("show_location");
		m_modelHandle.DirtyVariable("generals");
		m_modelHandle.DirtyVariable("show_bio_titles");
		m_modelHandle.DirtyVariable("show_bio_entries");
		m_modelHandle.DirtyVariable("show_portrait_movies");
		m_modelHandle.DirtyVariable("show_portraits");
		m_modelHandle.DirtyVariable("show_outer_circle");
		m_modelHandle.DirtyVariable("show_inner_circle");
		m_modelHandle.DirtyVariable("show_versus_backdrop");
		m_modelHandle.DirtyVariable("show_versus");
	}
}

void RmlLoadScreen::open()
{
	if (!TheRmlUiManager)
		return;

	load(TheRmlUiManager->getContext());
	if (!m_document)
		return;

	refresh();
	m_document->Show();
}

void RmlLoadScreen::close()
{
	// the LoadScreen deletes its movies right after; a later load screen publishes its own
	for (Int i = 0; i < LOAD_VIDEO_COUNT; ++i)
		RmlVideoElement::setSource(s_videoSources[i], nullptr);

	if (m_document)
		m_document->Hide();
}

bool RmlLoadScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

//-------------------------------------------------------------------------------------------------
void OpenRmlMapTransferScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_MAP_TRANSFER).open(); }
void CloseRmlMapTransferScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_MAP_TRANSFER).close(); }
void OpenRmlMultiplayerLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_MULTIPLAYER).open(); }
void CloseRmlMultiplayerLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_MULTIPLAYER).close(); }
void OpenRmlOnlineLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_ONLINE).open(); }
void CloseRmlOnlineLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_ONLINE).close(); }
void OpenRmlShellLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_SHELL).open(); }
void CloseRmlShellLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_SHELL).close(); }
void OpenRmlSinglePlayerLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_SINGLE_PLAYER).open(); }
void CloseRmlSinglePlayerLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_SINGLE_PLAYER).close(); }
void OpenRmlChallengeLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_CHALLENGE).open(); }
void CloseRmlChallengeLoadScreen() { RmlLoadScreen::instance(RmlLoadScreen::KIND_CHALLENGE).close(); }
