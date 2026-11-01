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
// FILE: LoadScreenView.cpp ///////////////////////////////////////////////////
// Applies LoadScreenData to the gadgets of the .wnd load screens.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/NameKeyGenerator.h"
#include "GameClient/GadgetProgressBar.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "Common/UnicodeString.h"
#include "GameClient/LoadScreenView.h"
#include "GameClient/MapUtil.h"

// Row colors are stored without alpha; the gadgets want it opaque.
static Color opaqueColor( UnsignedInt rgb )
{
	return 0xFF000000 | (rgb & 0xFFFFFF);
}

void positionStartSpots( GameInfo *myGame, GameWindow *buttonMapStartPositions[], GameWindow *mapWindow);
void updateMapStartSpots( GameInfo *myGame, GameWindow *buttonMapStartPositions[], Bool onLoadScreen = FALSE );

static const Image *findImage( const AsciiString &name )
{
	return name.isEmpty() ? nullptr : TheMappedImageCollection->findImageByName( name );
}

// House colored bar: image on Zero Hour, bar color on the original.
static void applyHouseBar( GameWindow *bar, const LoadScreenPlayerRow &row )
{
#if RTS_GENERALS
	GadgetProgressBarSetEnabledBarColor(bar, opaqueColor( row.m_color ) );
#else
	AsciiString imageName;
	imageName.format("LoadingBar_ProgressCenter%d", row.m_colorIndex);
	const Image *houseImage = TheMappedImageCollection->findImageByName(imageName);
	if (! houseImage)
		houseImage = TheMappedImageCollection->findImageByName("LoadingBar_Progress");
	bar->winSetEnabledImage( 6, houseImage );
#endif
}

// RosterLoadScreenView ///////////////////////////////////////////////////////
RosterLoadScreenView::RosterLoadScreenView( const char *wndName ) : m_wndName( wndName )
{
	m_mapPreview = nullptr;
	m_portraitLocalGeneral = nullptr;
	m_featuresLocalGeneral = nullptr;
	m_nameLocalGeneral = nullptr;
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_progressBars[i] = nullptr;
		m_playerNames[i] = nullptr;
		m_playerSide[i] = nullptr;
		m_buttonMapStartPosition[i] = nullptr;
		m_appliedProgress[i] = 0;
	}
}

RosterLoadScreenView::~RosterLoadScreenView()
{
	if(m_mapPreview)
	{
		m_mapPreview->winSetUserData(nullptr);
	}
}

void RosterLoadScreenView::bindCommon( GameWindow *root )
{
	AsciiString winName;
	winName.format( "%s:WinMapPreview", m_wndName.str() );
	m_mapPreview = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
}

void RosterLoadScreenView::applyCommon( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
#if RTS_GENERALS
	if (const Image *loadScreenImage = findImage( data.m_backgroundImage ))
		root->winSetEnabledImage(0, loadScreenImage);
#else
	// portrait, features, and name of the local player's general
	AsciiString winName;
	winName.format( "%s:LocalGeneralPortrait", m_wndName.str() );
	m_portraitLocalGeneral = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
	m_portraitLocalGeneral->winSetEnabledImage( 0, findImage( data.m_localPortrait ));
	winName.format( "%s:LocalGeneralFeatures", m_wndName.str() );
	m_featuresLocalGeneral = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
	GadgetStaticTextSetText( m_featuresLocalGeneral, data.m_localFeatures );
	winName.format( "%s:LocalGeneralName", m_wndName.str() );
	m_nameLocalGeneral = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
	GadgetStaticTextSetText( m_nameLocalGeneral, data.m_localName );
#endif
}

void RosterLoadScreenView::applyMapPreview( GameInfo *game, const LoadScreenData &data )
{
	if(m_mapPreview)
	{
		const MapMetaData *mmd = TheMapCache->findMap(data.m_mapName);
		Image *image = getMapPreviewImage(data.m_mapName);
		m_mapPreview->winSetUserData((void *)mmd);

		positionStartSpots( game, m_buttonMapStartPosition, m_mapPreview);
		updateMapStartSpots( game, m_buttonMapStartPosition, TRUE );
		if(image)
		{
			m_mapPreview->winSetStatus(WIN_STATUS_IMAGE);
			m_mapPreview->winSetEnabledImage(0, image);
		}
		else
		{
			m_mapPreview->winClearStatus(WIN_STATUS_IMAGE);
		}
	}
}

void RosterLoadScreenView::update( const LoadScreenData &data )
{
	for (Int i = 0; i < data.m_rowCount; ++i)
	{
		if (m_appliedProgress[i] != data.m_rows[i].m_progress)
		{
			m_appliedProgress[i] = data.m_rows[i].m_progress;
			if (m_progressBars[i])
				GadgetProgressBarSetProgress(m_progressBars[i], data.m_rows[i].m_progress );
		}
	}
}

void RosterLoadScreenView::reset()
{
	for(Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_progressBars[i] = nullptr;
		m_playerNames[i] = nullptr;
		m_playerSide[i] = nullptr;
	}
}

// MultiPlayerLoadScreenView //////////////////////////////////////////////////
MultiPlayerLoadScreenView::MultiPlayerLoadScreenView() : RosterLoadScreenView( "MultiplayerLoadScreen.wnd" )
{
}

void MultiPlayerLoadScreenView::init( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
	root->winHide(FALSE);
	root->winBringToTop();
	bindCommon( root );
	applyCommon( root, game, data );

	GameWindow *teamWin[MAX_SLOTS];
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		AsciiString winName;
		winName.format( "MultiplayerLoadScreen.wnd:ProgressLoad%d",i);
		m_progressBars[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_progressBars[i], ("Can't initialize the progressbars for the Multiplayer loadscreen"));
		// set the progressbar to zero
		GadgetProgressBarSetProgress(m_progressBars[i], 0 );

		winName.format( "MultiplayerLoadScreen.wnd:ButtonMapStartPosition%d",i);
		m_buttonMapStartPosition[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_buttonMapStartPosition[i], ("Can't initialize the MapStart Positions for the MultiplayerLoadScreen loadscreen"));

		winName.format( "MultiplayerLoadScreen.wnd:StaticTextPlayer%d",i);
		m_playerNames[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerNames[i], ("Can't initialize the Names for the Multiplayer loadscreen"));

		winName.format( "MultiplayerLoadScreen.wnd:StaticTextSide%d",i);
		m_playerSide[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerSide[i], ("Can't initialize the Sides for the Multiplayer loadscreen"));

		winName.format( "MultiplayerLoadScreen.wnd:StaticTextTeam%d",i);
		teamWin[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));

		m_appliedProgress[i] = 0;

		if (i >= data.m_rowCount)
		{
			m_progressBars[i]->winHide(TRUE);
			m_playerNames[i]->winHide(TRUE);
			m_playerSide[i]->winHide(TRUE);
			if (teamWin[i])
				teamWin[i]->winHide(TRUE);
			continue;
		}

		const LoadScreenPlayerRow &row = data.m_rows[i];
		Color houseColor = opaqueColor( row.m_color );
		applyHouseBar( m_progressBars[i], row );

		GadgetStaticTextSetText(m_playerNames[i], row.m_name );
		m_playerNames[i]->winSetEnabledTextColors(houseColor, m_playerNames[i]->winGetEnabledTextBorderColor());

		GadgetStaticTextSetText(m_playerSide[i], row.m_side );
		m_playerSide[i]->winSetEnabledTextColors(houseColor, m_playerSide[i]->winGetEnabledTextBorderColor());

		if (!row.m_showProgress)
			m_progressBars[i]->winHide(TRUE);

		if (teamWin[i])
		{
			GadgetStaticTextSetText(teamWin[i], row.m_team);
			teamWin[i]->winSetEnabledTextColors(houseColor, m_playerNames[i]->winGetEnabledTextBorderColor());
		}
	}

	applyMapPreview( game, data );
}

// GameSpyLoadScreenView //////////////////////////////////////////////////////
GameSpyLoadScreenView::GameSpyLoadScreenView() : RosterLoadScreenView( "GameSpyLoadScreen.wnd" )
{
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_playerWin[i] = nullptr;
		m_playerTotalDisconnects[i] = nullptr;
		m_playerWinLosses[i] = nullptr;
		m_playerRank[i] = nullptr;
		m_playerOfficerMedal[i] = nullptr;
	}
}

void GameSpyLoadScreenView::init( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
	root->winHide(FALSE);
	root->winBringToTop();
	bindCommon( root );
	applyCommon( root, game, data );

	GameWindow *teamWin[MAX_SLOTS];
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		AsciiString winName;
		winName.format( "GameSpyLoadScreen.wnd:ProgressLoad%d",i);
		m_progressBars[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_progressBars[i], ("Can't initialize the progressbars for the GameSpyLoadScreen loadscreen"));
		// set the progressbar to zero
		GadgetProgressBarSetProgress(m_progressBars[i], 0 );

		winName.format( "GameSpyLoadScreen.wnd:StaticTextPlayer%d",i);
		m_playerNames[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerNames[i], ("Can't initialize the Names for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:ButtonMapStartPosition%d",i);
		m_buttonMapStartPosition[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_buttonMapStartPosition[i], ("Can't initialize the MapStart Positions for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:StaticTextSide%d",i);
		m_playerSide[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerSide[i], ("Can't initialize the Sides for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:WinPlayer%d",i);
		m_playerWin[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerWin[i], ("Can't initialize the WinPlayer for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:StaticTextTotalDisconnects%d",i);
		m_playerTotalDisconnects[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerTotalDisconnects[i], ("Can't initialize the m_playerTotalDisconnects for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:StaticTextWinLoss%d",i);
		m_playerWinLosses[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerWinLosses[i], ("Can't initialize the m_playerWinLosses for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:WinRank%d",i);
		m_playerRank[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerRank[i], ("Can't initialize the m_playerRank for the GameSpyLoadScreen loadscreen"));

		winName.format( "GameSpyLoadScreen.wnd:WinOfficer%d",i);
		m_playerOfficerMedal[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerOfficerMedal[i], ("Can't initialize the m_playerOfficerMedal for the GameSpyLoadScreen loadscreen"));

		winName.format( "MultiplayerLoadScreen.wnd:StaticTextTeam%d",i);
		teamWin[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));

		m_appliedProgress[i] = 0;

		if (i >= data.m_rowCount)
		{
			m_playerWin[i]->winHide(TRUE);
			continue;
		}

		const LoadScreenPlayerRow &row = data.m_rows[i];
		Color houseColor = opaqueColor( row.m_color );
		applyHouseBar( m_progressBars[i], row );

		GadgetStaticTextSetText(m_playerNames[i], row.m_name );
		m_playerNames[i]->winSetEnabledTextColors(houseColor, m_playerNames[i]->winGetEnabledTextBorderColor());

		m_playerOfficerMedal[i]->winSetEnabledImage(0, findImage( row.m_medalImage ));
		m_playerRank[i]->winSetEnabledImage(0, findImage( row.m_rankImage ));

		GadgetStaticTextSetText(m_playerWinLosses[i], row.m_winLoss);
		m_playerWinLosses[i]->winSetEnabledTextColors(houseColor, m_playerWinLosses[i]->winGetEnabledTextBorderColor());

		GadgetStaticTextSetText(m_playerTotalDisconnects[i], row.m_disconnects);
		m_playerTotalDisconnects[i]->winSetEnabledTextColors(houseColor, m_playerTotalDisconnects[i]->winGetEnabledTextBorderColor());

		GadgetStaticTextSetText(m_playerSide[i], row.m_side);
		m_playerSide[i]->winSetEnabledTextColors(houseColor, m_playerSide[i]->winGetEnabledTextBorderColor());

		if (!row.m_showProgress)
			m_progressBars[i]->winHide(TRUE);
		if (!row.m_showStats)
		{
			m_playerTotalDisconnects[i]->winHide(TRUE);
			m_playerWinLosses[i]->winHide(TRUE);
			m_playerRank[i]->winHide(TRUE);
			m_playerOfficerMedal[i]->winHide(TRUE);
		}

		if (teamWin[i])
		{
			GadgetStaticTextSetText(teamWin[i], row.m_team);
			teamWin[i]->winSetEnabledTextColors(houseColor, m_playerNames[i]->winGetEnabledTextBorderColor());
		}
	}

	applyMapPreview( game, data );
}

// MapTransferLoadScreenView //////////////////////////////////////////////////
MapTransferLoadScreenView::MapTransferLoadScreenView()
{
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_progressBars[i] = nullptr;
		m_playerNames[i] = nullptr;
		m_progressText[i] = nullptr;
		m_appliedProgress[i] = 0;
	}
	m_fileNameText = nullptr;
	m_timeoutText = nullptr;
}

void MapTransferLoadScreenView::init( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
	root->winHide(FALSE);
	root->winBringToTop();

	AsciiString winName;

	// Load the Filename Text
	m_fileNameText = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( "MapTransferScreen.wnd:StaticTextCurrentFile" ));
	DEBUG_ASSERTCRASH(m_fileNameText, ("Can't initialize the filename for the map transfer loadscreen"));

	// Load the Timeout Text
	m_timeoutText = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( "MapTransferScreen.wnd:StaticTextTimeout" ));
	DEBUG_ASSERTCRASH(m_timeoutText, ("Can't initialize the timeout for the map transfer loadscreen"));

	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		// Load the Progress Bar
		winName.format( "MapTransferScreen.wnd:ProgressLoad%d",i);
		m_progressBars[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_progressBars[i], ("Can't initialize the progressbars for the map transfer loadscreen"));
		// set the progressbar to zero
		GadgetProgressBarSetProgress(m_progressBars[i], 0 );

		// Load the Player's name
		winName.format( "MapTransferScreen.wnd:StaticTextPlayer%d",i);
		m_playerNames[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_playerNames[i], ("Can't initialize the Names for the map transfer loadscreen"));

		// Load the Progress Text
		winName.format( "MapTransferScreen.wnd:StaticTextProgress%d",i);
		m_progressText[i] = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( winName ));
		DEBUG_ASSERTCRASH(m_progressText[i], ("Can't initialize the progress text for the map transfer loadscreen"));

		m_appliedProgress[i] = 0;

		if (i >= data.m_rowCount)
		{
			m_progressBars[i]->winHide(TRUE);
			m_playerNames[i]->winHide(TRUE);
			m_progressText[i]->winHide(TRUE);
			continue;
		}

		const LoadScreenPlayerRow &row = data.m_rows[i];
		Color houseColor = opaqueColor( row.m_color );
		GadgetProgressBarSetEnabledBarColor(m_progressBars[i], houseColor );

		GadgetStaticTextSetText(m_playerNames[i], row.m_name );
		m_playerNames[i]->winSetEnabledTextColors(houseColor, m_playerNames[i]->winGetEnabledTextBorderColor());

		GadgetStaticTextSetText(m_progressText[i], UnicodeString::TheEmptyString );
		m_progressText[i]->winSetEnabledTextColors(houseColor, m_progressText[i]->winGetEnabledTextBorderColor());

		if (!row.m_showProgress)
			m_progressBars[i]->winHide(TRUE);
	}
}

void MapTransferLoadScreenView::update( const LoadScreenData &data )
{
	for (Int i = 0; i < data.m_rowCount; ++i)
	{
		const LoadScreenPlayerRow &row = data.m_rows[i];
		if (m_appliedProgress[i] != row.m_progress)
		{
			m_appliedProgress[i] = row.m_progress;
			if (m_progressBars[i])
				GadgetProgressBarSetProgress(m_progressBars[i], row.m_progress );
		}
		if (m_appliedStatus[i] != row.m_status)
		{
			m_appliedStatus[i] = row.m_status;
			if (m_progressText[i])
				GadgetStaticTextSetText(m_progressText[i], row.m_status);
		}
	}
	if (m_appliedFile != data.m_currentFile)
	{
		m_appliedFile = data.m_currentFile;
		if (m_fileNameText)
			GadgetStaticTextSetText(m_fileNameText, data.m_currentFile);
	}
	if (m_appliedTimeout != data.m_timeout)
	{
		m_appliedTimeout = data.m_timeout;
		if (m_timeoutText)
			GadgetStaticTextSetText(m_timeoutText, data.m_timeout);
	}
}

void MapTransferLoadScreenView::reset()
{
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_progressBars[i] = nullptr;
		m_playerNames[i] = nullptr;
		m_progressText[i] = nullptr;
	}
	m_fileNameText = nullptr;
	m_timeoutText = nullptr;
}

// ShellLoadScreenView ////////////////////////////////////////////////////////
ShellLoadScreenView::ShellLoadScreenView()
{
	m_progressBar = nullptr;
}

void ShellLoadScreenView::init( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
	root->winHide(FALSE);
	root->winBringToTop();

	// Store the pointer to the progress bar on the loadscreen
	m_progressBar = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( "ShellGameLoadScreen.wnd:ProgressLoad" ));
	DEBUG_ASSERTCRASH(m_progressBar, ("Can't initialize the progressbar for the single player loadscreen"));
	GadgetProgressBarSetProgress(m_progressBar, 0 );
	m_progressBar->winHide(TRUE);

	if (data.m_titleScreen)
	{
		root->winSetEnabledImage(0, TheMappedImageCollection->findImageByName("TitleScreen"));

		GameWindow *win = TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( "ShellGameLoadScreen.wnd:StaticTextLegal" ));
		if(win)
			win->winHide(FALSE);
	}
	m_progressBar->winHide(FALSE);
}

void ShellLoadScreenView::update( const LoadScreenData &data )
{
	if (m_progressBar)
		GadgetProgressBarSetProgress(m_progressBar, data.m_progress);
}

void ShellLoadScreenView::reset()
{
	m_progressBar = nullptr;
}

static GameWindow *findChild( GameWindow *root, const char *name )
{
	return TheWindowManager->winGetWindowFromId( root, TheNameKeyGenerator->nameToKey( name ));
}

// SinglePlayerLoadScreenView /////////////////////////////////////////////////
SinglePlayerLoadScreenView::SinglePlayerLoadScreenView()
{
	reset();
}

void SinglePlayerLoadScreenView::init( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
	m_root = root;
	root->winHide(FALSE);
	root->winBringToTop();

	// Store the pointer to the progress bar on the loadscreen
	m_progressBar = findChild( root, "SinglePlayerLoadScreen.wnd:ProgressLoad" );
	DEBUG_ASSERTCRASH(m_progressBar, ("Can't initialize the progressbar for the single player loadscreen"));
	GadgetProgressBarSetProgress(m_progressBar, 0 );

	m_percent = findChild( root, "SinglePlayerLoadScreen.wnd:Percent" );
	DEBUG_ASSERTCRASH(m_percent, ("Can't initialize the m_percent for the single player loadscreen"));
	GadgetStaticTextSetText(m_percent,L"0%");
	m_percent->winHide(TRUE);

	m_objectiveWin = findChild( root, "SinglePlayerLoadScreen.wnd:ObjectivesWin" );
	DEBUG_ASSERTCRASH(m_objectiveWin, ("Can't initialize the m_objectiveWin for the single player loadscreen"));
	m_objectiveWin->winHide(TRUE);

	AsciiString lineName;
	Int i = 0;
	for(; i < MAX_OBJECTIVE_LINES; ++i)
	{
		lineName.format("SinglePlayerLoadScreen.wnd:StaticTextLine%d",i);
		m_objectiveLines[i] = findChild( root, lineName.str() );
		DEBUG_ASSERTCRASH(m_objectiveLines[i], ("Can't initialize the m_objectiveLines[%d] for the single player loadscreen", i));
		GadgetStaticTextSetText(m_objectiveLines[i],UnicodeString::TheEmptyString);
	}

	for(i = 0; i < MAX_DISPLAYED_UNITS; ++i)
	{
		lineName.format("SinglePlayerLoadScreen.wnd:StaticTextCameoText%d",i);
		m_unitDesc[i] = findChild( root, lineName.str() );
		DEBUG_ASSERTCRASH(m_unitDesc[i], ("Can't initialize the m_objectiveLines[%d] for the single player loadscreen", i));
		GadgetStaticTextSetText(m_unitDesc[i], data.m_unitNames[i]);
		m_unitDesc[i]->winHide(TRUE);
	}
	m_location = findChild( root, "SinglePlayerLoadScreen.wnd:StaticTextCameoText3" );
	DEBUG_ASSERTCRASH(m_location, ("Can't initialize the m_objectiveWin for the single player loadscreen"));
	m_location->winHide(TRUE);
	GadgetStaticTextSetText(m_location, data.m_location);

	m_background = findChild( root, "SinglePlayerLoadScreen.wnd:ParentSinglePlayerLoadScreen" );

	update( data );
}

void SinglePlayerLoadScreenView::update( const LoadScreenData &data )
{
	if (!m_root)
		return;

	if (m_appliedProgress != data.m_progress)
	{
		m_appliedProgress = data.m_progress;
		UnicodeString per;
		per.format(L"%d%%", data.m_progress);
		GadgetProgressBarSetProgress(m_progressBar, data.m_progress);
		GadgetStaticTextSetText(m_percent, per);
	}

	if (m_appliedVideo != data.m_videos[LOAD_VIDEO_BACKGROUND])
	{
		m_appliedVideo = data.m_videos[LOAD_VIDEO_BACKGROUND];
		m_root->winGetInstanceData()->setVideoBuffer( m_appliedVideo );
	}

	// the campaign's art, and its bar: USA to blue, GLA to green, China to red
	if (m_appliedBackground != data.m_backgroundImage)
	{
		m_appliedBackground = data.m_backgroundImage;
		if (const Image *image = findImage( data.m_backgroundImage ))
			m_background->winSetEnabledImage( 0, image );
	}
	if (m_appliedBarColor != data.m_barColorIndex)
	{
		m_appliedBarColor = data.m_barColorIndex;
		AsciiString imageName;
		imageName.format("LoadingBar_ProgressCenter%d", data.m_barColorIndex);
		if (const Image *image = TheMappedImageCollection->findImageByName( imageName ))
			m_progressBar->winSetEnabledImage( 6, image );
	}

	if (m_appliedShowObjectives != data.m_showObjectives)
	{
		m_appliedShowObjectives = data.m_showObjectives;
		m_objectiveWin->winHide( !data.m_showObjectives );
	}
	for (Int i = 0; i < MAX_OBJECTIVE_LINES; ++i)
	{
		if (m_appliedObjectiveLines[i] != data.m_objectiveLines[i])
		{
			m_appliedObjectiveLines[i] = data.m_objectiveLines[i];
			GadgetStaticTextSetText(m_objectiveLines[i], data.m_objectiveLines[i]);
		}
	}
	for (Int i = 0; i < MAX_DISPLAYED_UNITS; ++i)
	{
		if (m_appliedShowUnit[i] != data.m_showUnit[i])
		{
			m_appliedShowUnit[i] = data.m_showUnit[i];
			m_unitDesc[i]->winHide( !data.m_showUnit[i] );
		}
	}
	if (m_appliedShowLocation != data.m_showLocation)
	{
		m_appliedShowLocation = data.m_showLocation;
		m_location->winHide( !data.m_showLocation );
	}
}

void SinglePlayerLoadScreenView::reset()
{
	m_root = nullptr;
	m_background = nullptr;
	m_progressBar = nullptr;
	m_percent = nullptr;
	m_objectiveWin = nullptr;
	m_location = nullptr;
	for (Int i = 0; i < MAX_OBJECTIVE_LINES; ++i)
	{
		m_objectiveLines[i] = nullptr;
		m_appliedObjectiveLines[i].clear();
	}
	for (Int i = 0; i < MAX_DISPLAYED_UNITS; ++i)
	{
		m_unitDesc[i] = nullptr;
		m_appliedShowUnit[i] = FALSE;
	}

	// what init() leaves on screen
	m_appliedProgress = 0;
	m_appliedVideo = nullptr;
	m_appliedBackground.clear();
	m_appliedBarColor = -1;
	m_appliedShowObjectives = FALSE;
	m_appliedShowLocation = FALSE;
}

// ChallengeLoadScreenView ////////////////////////////////////////////////////
ChallengeLoadScreenView::ChallengeLoadScreenView()
{
	reset();
}

void ChallengeLoadScreenView::init( GameWindow *root, GameInfo *game, const LoadScreenData &data )
{
	static const char *const sides[2] = { "Left", "Right" };
	static const char *const titles[BIO_TITLE_COUNT] = { "BioName", "BioBirthplace", "BioStrategy" };
	static const char *const entries[BIO_ENTRY_COUNT] = { "BigNameEntry", "BioNameEntry", "BioBirthplaceEntry", "BioStrategyEntry" };

	m_root = root;
	root->winHide(FALSE);
	root->winBringToTop();

	// Store the pointer to the progress bar on the loadscreen
	m_progressBar = findChild( root, "ChallengeLoadScreen.wnd:ProgressLoad" );
	DEBUG_ASSERTCRASH(m_progressBar, ("Can't initialize the progressbar for the single player loadscreen"));
	GadgetProgressBarSetProgress(m_progressBar, 0 );

	AsciiString name;
	for (Int side = 0; side < 2; ++side)
	{
		name.format("ChallengeLoadScreen.wnd:Portrait%s", sides[side]);
		m_portraits[side] = findChild( root, name.str() );
		name.format("ChallengeLoadScreen.wnd:PortraitMovie%s", sides[side]);
		m_portraitMovies[side] = findChild( root, name.str() );
		for (Int i = 0; i < BIO_TITLE_COUNT; ++i)
		{
			name.format("ChallengeLoadScreen.wnd:%s%s", titles[i], sides[side]);
			m_bioTitles[side][i] = findChild( root, name.str() );
		}
		for (Int i = 0; i < BIO_ENTRY_COUNT; ++i)
		{
			name.format("ChallengeLoadScreen.wnd:%s%s", entries[i], sides[side]);
			m_bioEntries[side][i] = findChild( root, name.str() );
		}
	}
	m_outerCircle = findChild( root, "ChallengeLoadScreen.wnd:CircleAlphaOuter" );
	m_innerCircle = findChild( root, "ChallengeLoadScreen.wnd:CircleAlphaInner" );
	m_versusBackdrop = findChild( root, "ChallengeLoadScreen.wnd:VersusBackdrop" );
	m_versus = findChild( root, "ChallengeLoadScreen.wnd:OverlayVs" );

	apply( data, TRUE );
}

void ChallengeLoadScreenView::update( const LoadScreenData &data )
{
	apply( data, FALSE );
}

// force: the entries still hold the .wnd's placeholder text, so write what the data holds (hidden until shown).
void ChallengeLoadScreenView::apply( const LoadScreenData &data, Bool force )
{
	if (!m_root)
		return;

	if (m_appliedProgress != data.m_progress)
	{
		m_appliedProgress = data.m_progress;
		GadgetProgressBarSetProgress(m_progressBar, data.m_progress);
	}

	GameWindow *const videoWindows[LOAD_VIDEO_COUNT] = { m_root, m_portraitMovies[0], m_portraitMovies[1], m_versus };
	for (Int i = 0; i < LOAD_VIDEO_COUNT; ++i)
	{
		if (m_appliedVideos[i] != data.m_videos[i])
		{
			m_appliedVideos[i] = data.m_videos[i];
			videoWindows[i]->winGetInstanceData()->setVideoBuffer( data.m_videos[i] );
		}
	}

	for (Int side = 0; side < 2; ++side)
	{
		const LoadScreenGeneral &general = data.m_generals[side];
		LoadScreenGeneral &applied = m_appliedGenerals[side];
		const UnicodeString *texts[BIO_ENTRY_COUNT] = { &general.m_bigName, &general.m_name, &general.m_rank, &general.m_strategy };
		UnicodeString *appliedTexts[BIO_ENTRY_COUNT] = { &applied.m_bigName, &applied.m_name, &applied.m_rank, &applied.m_strategy };
		for (Int i = 0; i < BIO_ENTRY_COUNT; ++i)
		{
			if (force || *appliedTexts[i] != *texts[i])
			{
				*appliedTexts[i] = *texts[i];
				GadgetStaticTextSetText( m_bioEntries[side][i], *texts[i] );
			}
		}
		if (applied.m_portrait != general.m_portrait)
		{
			applied.m_portrait = general.m_portrait;
			m_portraits[side]->winSetEnabledImage( 0, findImage( general.m_portrait ) );
		}
	}

	if (m_appliedShowBioTitles != data.m_showBioTitles)
	{
		m_appliedShowBioTitles = data.m_showBioTitles;
		for (Int side = 0; side < 2; ++side)
			for (Int i = 0; i < BIO_TITLE_COUNT; ++i)
				m_bioTitles[side][i]->winHide( !data.m_showBioTitles );
	}
	if (m_appliedShowBioEntries != data.m_showBioEntries)
	{
		m_appliedShowBioEntries = data.m_showBioEntries;
		for (Int side = 0; side < 2; ++side)
			for (Int i = 0; i < BIO_ENTRY_COUNT; ++i)
				m_bioEntries[side][i]->winHide( !data.m_showBioEntries );
	}
	if (m_appliedShowPortraitMovies != data.m_showPortraitMovies)
	{
		m_appliedShowPortraitMovies = data.m_showPortraitMovies;
		m_portraitMovies[0]->winHide( !data.m_showPortraitMovies );
		m_portraitMovies[1]->winHide( !data.m_showPortraitMovies );
	}
	if (m_appliedShowPortraits != data.m_showPortraits)
	{
		m_appliedShowPortraits = data.m_showPortraits;
		m_portraits[0]->winHide( !data.m_showPortraits );
		m_portraits[1]->winHide( !data.m_showPortraits );
	}
	if (m_appliedShowOuterCircle != data.m_showOuterCircle)
	{
		m_appliedShowOuterCircle = data.m_showOuterCircle;
		m_outerCircle->winHide( !data.m_showOuterCircle );
	}
	if (m_appliedShowInnerCircle != data.m_showInnerCircle)
	{
		m_appliedShowInnerCircle = data.m_showInnerCircle;
		m_innerCircle->winHide( !data.m_showInnerCircle );
	}
	if (m_appliedShowVersusBackdrop != data.m_showVersusBackdrop)
	{
		m_appliedShowVersusBackdrop = data.m_showVersusBackdrop;
		m_versusBackdrop->winHide( !data.m_showVersusBackdrop );
	}
	if (m_appliedShowVersus != data.m_showVersus)
	{
		m_appliedShowVersus = data.m_showVersus;
		m_versus->winHide( !data.m_showVersus );
	}
}

void ChallengeLoadScreenView::reset()
{
	m_root = nullptr;
	m_progressBar = nullptr;
	for (Int side = 0; side < 2; ++side)
	{
		for (Int i = 0; i < BIO_TITLE_COUNT; ++i)
			m_bioTitles[side][i] = nullptr;
		for (Int i = 0; i < BIO_ENTRY_COUNT; ++i)
			m_bioEntries[side][i] = nullptr;
		m_portraits[side] = nullptr;
		m_portraitMovies[side] = nullptr;
		m_appliedGenerals[side] = LoadScreenGeneral();
	}
	m_outerCircle = nullptr;
	m_innerCircle = nullptr;
	m_versusBackdrop = nullptr;
	m_versus = nullptr;

	// what the .wnd shows before anything is applied
	m_appliedProgress = 0;
	for (Int i = 0; i < LOAD_VIDEO_COUNT; ++i)
		m_appliedVideos[i] = nullptr;
	m_appliedShowBioTitles = FALSE;
	m_appliedShowBioEntries = FALSE;
	m_appliedShowPortraitMovies = FALSE;
	m_appliedShowPortraits = FALSE;
	m_appliedShowOuterCircle = TRUE;
	m_appliedShowInnerCircle = TRUE;
	m_appliedShowVersusBackdrop = TRUE;
	m_appliedShowVersus = FALSE;
}
