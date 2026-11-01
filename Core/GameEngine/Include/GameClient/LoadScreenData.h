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
// FILE: LoadScreenData.h /////////////////////////////////////////////////////
// Widget-agnostic content of the load screens (map transfer, multiplayer, online, shell,
// single player and challenge). The LoadScreen classes fill this and never touch a GameWindow;
// the legacy .wnd view and the RmlUi load screen both draw it.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "GameClient/CampaignManager.h"
#include "GameNetwork/GameInfo.h"

class VideoBuffer;

// Movies of the single player and challenge screens. The LoadScreen owns and decodes them; a view
// only draws the buffer published for a slot, null while that movie shows nothing.
enum LoadScreenVideo
{
	LOAD_VIDEO_BACKGROUND, ///< briefing or challenge backdrop over the whole screen
	LOAD_VIDEO_PORTRAIT_LEFT, ///< challenge: the player's general
	LOAD_VIDEO_PORTRAIT_RIGHT, ///< challenge: the opponent
	LOAD_VIDEO_VERSUS, ///< challenge: the "vs" animation, .wnd view only (the RmlUi screen draws its own)
	LOAD_VIDEO_COUNT
};

// One side of the challenge screen. The entries are typed out a character at a time, so these hold
// what is shown so far, not the full text.
struct LoadScreenGeneral
{
	UnicodeString m_bigName;
	UnicodeString m_name;
	UnicodeString m_rank;
	UnicodeString m_strategy;
	AsciiString m_portrait; ///< still portrait shown instead of the movie on min spec
};

struct LoadScreenPlayerRow
{
	UnicodeString m_name;
	UnicodeString m_side;
	UnicodeString m_team;
	UnicodeString m_winLoss;
	UnicodeString m_disconnects;
	UnicodeString m_status; ///< map transfer state text
	AsciiString m_rankImage; ///< mapped image names, empty for none
	AsciiString m_medalImage;
	AsciiString m_sideImage; ///< faction badge (the army's general image, else its side icon), empty for random
	UnsignedInt m_color = 0xFFFFFF; ///< 0x00RRGGBB
	Int m_colorIndex = 0; ///< slot's apparent color index, for the legacy house-colored bar
	Int m_progress = 0; ///< 0..100
	Bool m_showProgress = TRUE;
	Bool m_showStats = TRUE; ///< FALSE for AI: no rank, win/loss or disconnects
};

// Start position marker over the map preview, laid out as fractions of the preview.
struct LoadScreenStartMarker
{
	Real m_x = 0.0f;
	Real m_y = 0.0f;
	Int m_slotNumber = 0; ///< 1-based number of the slot starting here
	UnsignedInt m_color = 0xFFFFFF;
	Bool m_used = FALSE;
};

struct LoadScreenData
{
	static LoadScreenData &instance()
	{
		static LoadScreenData s_data;
		return s_data;
	}

	void reset()
	{
		UnsignedInt version = m_version;
		*this = LoadScreenData();
		m_version = version + 1;
	}
	void touch() { ++m_version; } ///< bump after any change so the view refreshes

	Int m_rowCount = 0;
	LoadScreenPlayerRow m_rows[MAX_SLOTS];

	UnicodeString m_localName; ///< local player's general or faction
	UnicodeString m_localFeatures;
	AsciiString m_localPortrait; ///< mapped image name

	AsciiString m_backgroundImage; ///< mapped image behind the screen, empty for none (original Generals)

	AsciiString m_mapName;
	UnicodeString m_mapDisplayName; ///< the map's name as the map list shows it
	UnicodeString m_gameMode; ///< skirmish, network or online, over the map name
	LoadScreenStartMarker m_markers[MAX_SLOTS];

	UnicodeString m_currentFile; ///< map transfer only
	UnicodeString m_timeout;

	Int m_progress = 0; ///< shell, single player and challenge load bar, 0..100
	Bool m_titleScreen = FALSE; ///< first shell load: title art and copyright line

	// Single player and challenge -----------------------------------------------------------
	VideoBuffer *m_videos[LOAD_VIDEO_COUNT] = {};
	Int m_barColorIndex = -1; ///< fill is LoadingBar_ProgressCenter<n>, -1 for the .wnd's own

	// Single player briefing. The objectives, unit and location texts only ever show on the original Generals.
	Bool m_showObjectives = FALSE;
	UnicodeString m_objectiveLines[MAX_OBJECTIVE_LINES]; ///< typed out so far
	UnicodeString m_unitNames[MAX_DISPLAYED_UNITS];
	Bool m_showUnit[MAX_DISPLAYED_UNITS] = {};
	UnicodeString m_location;
	Bool m_showLocation = FALSE;

	// Challenge: [0] is the player's general on the left, [1] the opponent. The circles and the
	// versus backdrop start out shown, as in the .wnd, until the movies are known to play.
	LoadScreenGeneral m_generals[2];
	Bool m_showBioTitles = FALSE;
	Bool m_showBioEntries = FALSE;
	Bool m_showPortraitMovies = FALSE;
	Bool m_showPortraits = FALSE; ///< the still portraits
	Bool m_showOuterCircle = TRUE;
	Bool m_showInnerCircle = TRUE;
	Bool m_showVersusBackdrop = TRUE;
	Bool m_showVersus = FALSE;

	UnsignedInt m_version = 0;
};
