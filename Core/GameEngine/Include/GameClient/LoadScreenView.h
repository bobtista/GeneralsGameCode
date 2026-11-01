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
// FILE: LoadScreenView.h /////////////////////////////////////////////////////
// Legacy .wnd presentation of LoadScreenData. A LoadScreen owns one only when the registry
// does not route its screen to RmlUi; RmlLoadScreen is the other reader of the same data.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/GameWindow.h"
#include "GameClient/LoadScreenData.h"

class GameInfo;

class LoadScreenView
{
public:
	virtual ~LoadScreenView() {}

	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) = 0; ///< bind the gadgets and apply everything
	virtual void update( const LoadScreenData &data ) = 0; ///< apply what changes while loading
	virtual void reset() = 0; ///< the window is going away, drop the gadget pointers
};

// Per-player transfer bars, state texts, current file and timeout.
class MapTransferLoadScreenView : public LoadScreenView
{
public:
	MapTransferLoadScreenView();

	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) override;
	virtual void update( const LoadScreenData &data ) override;
	virtual void reset() override;

private:
	GameWindow *m_progressBars[MAX_SLOTS];
	GameWindow *m_playerNames[MAX_SLOTS];
	GameWindow *m_progressText[MAX_SLOTS];
	GameWindow *m_fileNameText;
	GameWindow *m_timeoutText;
	Int m_appliedProgress[MAX_SLOTS]; ///< so publishing every frame does not touch the gadgets
	UnicodeString m_appliedStatus[MAX_SLOTS];
	UnicodeString m_appliedFile;
	UnicodeString m_appliedTimeout;
};

// Rows, local general and map preview shared by the multiplayer and online screens.
class RosterLoadScreenView : public LoadScreenView
{
public:
	RosterLoadScreenView( const char *wndName );
	virtual ~RosterLoadScreenView() override;

	virtual void update( const LoadScreenData &data ) override;
	virtual void reset() override;

protected:
	void bindCommon( GameWindow *root ); ///< map preview window
	void applyCommon( GameWindow *root, GameInfo *game, const LoadScreenData &data ); ///< background and local general
	void applyMapPreview( GameInfo *game, const LoadScreenData &data ); ///< after the start spot buttons are bound

	AsciiString m_wndName; ///< prefix of the gadget names, e.g. MultiplayerLoadScreen.wnd
	GameWindow *m_progressBars[MAX_SLOTS];
	GameWindow *m_playerNames[MAX_SLOTS];
	GameWindow *m_playerSide[MAX_SLOTS];
	GameWindow *m_mapPreview;
	GameWindow *m_buttonMapStartPosition[MAX_SLOTS];
	GameWindow *m_portraitLocalGeneral;
	GameWindow *m_featuresLocalGeneral;
	GameWindow *m_nameLocalGeneral;
	Int m_appliedProgress[MAX_SLOTS]; ///< so publishing every frame does not touch the gadgets
};

class MultiPlayerLoadScreenView : public RosterLoadScreenView
{
public:
	MultiPlayerLoadScreenView();
	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) override;
};

// Adds the online stats: rank, officer medal, win/loss and disconnects.
class GameSpyLoadScreenView : public RosterLoadScreenView
{
public:
	GameSpyLoadScreenView();
	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) override;

private:
	GameWindow *m_playerWin[MAX_SLOTS];
	GameWindow *m_playerTotalDisconnects[MAX_SLOTS];
	GameWindow *m_playerWinLosses[MAX_SLOTS];
	GameWindow *m_playerRank[MAX_SLOTS];
	GameWindow *m_playerOfficerMedal[MAX_SLOTS];
};

// Title art, legal line and progress bar of the shell load.
class ShellLoadScreenView : public LoadScreenView
{
public:
	ShellLoadScreenView();

	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) override;
	virtual void update( const LoadScreenData &data ) override;
	virtual void reset() override;

private:
	GameWindow *m_progressBar;
};

// Briefing movie over the campaign art, progress bar, and on the original Generals the objectives,
// units and location.
class SinglePlayerLoadScreenView : public LoadScreenView
{
public:
	SinglePlayerLoadScreenView();

	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) override;
	virtual void update( const LoadScreenData &data ) override;
	virtual void reset() override;

private:
	GameWindow *m_root;
	GameWindow *m_background;
	GameWindow *m_progressBar;
	GameWindow *m_percent;
	GameWindow *m_objectiveWin;
	GameWindow *m_objectiveLines[MAX_OBJECTIVE_LINES];
	GameWindow *m_unitDesc[MAX_DISPLAYED_UNITS];
	GameWindow *m_location;

	// what the gadgets show, so publishing every frame does not touch them
	Int m_appliedProgress;
	VideoBuffer *m_appliedVideo;
	AsciiString m_appliedBackground;
	Int m_appliedBarColor;
	Bool m_appliedShowObjectives;
	UnicodeString m_appliedObjectiveLines[MAX_OBJECTIVE_LINES];
	Bool m_appliedShowUnit[MAX_DISPLAYED_UNITS];
	Bool m_appliedShowLocation;
};

// Backdrop movie, the two generals' portraits and typed out bios, the versus animation and the bar.
class ChallengeLoadScreenView : public LoadScreenView
{
public:
	ChallengeLoadScreenView();

	virtual void init( GameWindow *root, GameInfo *game, const LoadScreenData &data ) override;
	virtual void update( const LoadScreenData &data ) override;
	virtual void reset() override;

private:
	enum { BIO_TITLE_COUNT = 3, BIO_ENTRY_COUNT = 4 };

	void apply( const LoadScreenData &data, Bool force );

	GameWindow *m_root;
	GameWindow *m_progressBar;
	GameWindow *m_bioTitles[2][BIO_TITLE_COUNT]; ///< name, rank and strategy labels
	GameWindow *m_bioEntries[2][BIO_ENTRY_COUNT]; ///< big name, name, rank, strategy
	GameWindow *m_portraits[2];
	GameWindow *m_portraitMovies[2];
	GameWindow *m_outerCircle;
	GameWindow *m_innerCircle;
	GameWindow *m_versusBackdrop;
	GameWindow *m_versus;

	Int m_appliedProgress;
	VideoBuffer *m_appliedVideos[LOAD_VIDEO_COUNT];
	LoadScreenGeneral m_appliedGenerals[2];
	Bool m_appliedShowBioTitles;
	Bool m_appliedShowBioEntries;
	Bool m_appliedShowPortraitMovies;
	Bool m_appliedShowPortraits;
	Bool m_appliedShowOuterCircle;
	Bool m_appliedShowInnerCircle;
	Bool m_appliedShowVersusBackdrop;
	Bool m_appliedShowVersus;
};
