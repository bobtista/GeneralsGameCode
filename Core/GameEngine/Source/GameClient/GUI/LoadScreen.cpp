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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: LoadScreen.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Mar 2002
//
//	Filename: 	LoadScreen.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	Contains each of the different derived LoadClasses for each of the
//						Different kind of games we can have.
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "Common/NameKeyGenerator.h"
#include "Common/AudioAffect.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/GameAudio.h"
#include "Common/GameEngine.h"
#include "Common/GameLOD.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/MultiplayerSettings.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/Display.h"
#include "GameClient/GadgetProgressBar.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/GUI/GUICallbacks/Menus/GameSetupData.h"
#include "GameClient/Keyboard.h"
#include "GameClient/LoadScreen.h"
#include "GameClient/LoadScreenData.h"
#include "GameClient/LoadScreenView.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Mouse.h"
#include "GameClient/RmlUiScreenRegistry.h"
#include "GameClient/Shell.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/ChallengeGenerals.h"
#include "GameLogic/FPUControl.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/PersistentStorageThread.h"
#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/RankPointValue.h"
// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
#if defined(GENERALS_ONLINE)
#include "../OnlineServices_Init.h"
#include "../OnlineServices_StatsInterface.h"
#endif

//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// PRIVATE TYPES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PRIVATE DATA ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PUBLIC DATA ////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#if defined(GENERALS_ONLINE)	// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
bool g_bHasDoneSOGScreenshot = false;
#endif

//-----------------------------------------------------------------------------
// PRIVATE PROTOTYPES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

UnsignedInt GetTeamUiColor(Int teamNumber);

// The .wnd load screen gets a view over LoadScreenData only when the registry does not route the path to RmlUi.
static Bool usesLegacyView( const char *wndPath )
{
	return !RmlUiScreenRegistry::routesToRmlUi( AsciiString( wndPath ) );
}

#if !RTS_GENERALS
// Portrait and name shown for the local player's general (or faction, for the original armies).
static const Image *getLocalGeneralPortrait( const PlayerTemplate *pt, Bool large, UnicodeString &localName )
{
	const GeneralPersona *localGeneral = TheChallengeGenerals->getGeneralByTemplateName( pt->getName() );
	const Image *portrait = nullptr;
	if (localGeneral)
	{
		portrait = localGeneral->getBioPortraitLarge();
		localName = TheGameText->fetch( localGeneral->getBioName() );
	}
	else
	{
		// the main original factions don't have associated generals
		if (pt->getName() == "FactionAmerica")
			portrait = TheMappedImageCollection->findImageByName( large ? "SAFactionLogoLg_US" : "SAFactionLogo144_US" );
		else if (pt->getName() == "FactionGLA")
			portrait = TheMappedImageCollection->findImageByName( large ? "SUFactionLogoLg_GLA" : "SUFactionLogo144_GLA" );
		else if (pt->getName() == "FactionChina")
			portrait = TheMappedImageCollection->findImageByName( large ? "SNFactionLogoLg_China" : "SNFactionLogo144_China" );
		else
			DEBUG_CRASH(("Unexpected player template"));

		localName = pt->getDisplayName();
	}
	return portrait;
}

static void fillLocalGeneral( LoadScreenData &view, const PlayerTemplate *pt, Bool large )
{
	const Image *portrait = getLocalGeneralPortrait( pt, large, view.m_localName );
	AsciiString features = pt->getGeneralFeatures();
	view.m_localFeatures = TheGameText->fetch( features.isEmpty() ? "GUI:PlayerObserver" : pt->getGeneralFeatures() );
	view.m_localPortrait = portrait ? portrait->getName() : AsciiString::TheEmptyString;
}
#endif

// Map preview markers, each numbered and tinted by team for the slot starting there, like updateMapStartSpots().
static void fillMapView( LoadScreenData &view, GameInfo *game )
{
	view.m_mapName = game->getMap();
	const MapMetaData *md = TheMapCache ? TheMapCache->findMap( game->getMap() ) : nullptr;
	if (md)
		view.m_mapDisplayName = md->m_displayName;
	else
		view.m_mapDisplayName.translate( game->getMap() );
	std::vector<GameSetupStartPositionMarker> markers = GameSetupData::computeStartPositionMarkers( game->getMap() );
	for (Int i = 0; i < MAX_SLOTS && i < (Int)markers.size(); ++i)
	{
		view.m_markers[i].m_used = markers[i].m_used;
		view.m_markers[i].m_x = markers[i].m_xFraction;
		view.m_markers[i].m_y = markers[i].m_yFraction;
	}
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		GameSlot *slot = game->getSlot(i);
		if (!slot || !slot->isOccupied())
			continue;
		Int startPos = slot->getApparentStartPos();
		if (startPos < 0 || startPos >= MAX_SLOTS)
			continue;
		view.m_markers[startPos].m_slotNumber = i + 1;
		view.m_markers[startPos].m_color = (slot->getTeamNumber() >= 0 ? GetTeamUiColor( slot->getTeamNumber() ) : 0xFFFFFF) & 0xFFFFFF;
	}
}

// The badge for the side getSlotSideName() names: the general's image, else the side icon.
static AsciiString getSlotSideImage( GameSlot *slot )
{
#if defined(GO_REVEAL_TEAMS)
	Int templateNum = slot->getPlayerTemplate();
#else
	Int templateNum = slot->getApparentPlayerTemplate();
#endif
	const PlayerTemplate *pt = nullptr;
	if (templateNum >= 0)
		pt = ThePlayerTemplateStore->getNthPlayerTemplate( templateNum );
	else if (templateNum == PLAYERTEMPLATE_OBSERVER)
		pt = ThePlayerTemplateStore->findPlayerTemplate( TheNameKeyGenerator->nameToKey( "FactionObserver" ) );
	if (!pt)
		return AsciiString::TheEmptyString;
#if RTS_GENERALS
	const Image *image = pt->getSideIconImage();
#else
	const Image *image = pt->getGeneralImage() ? pt->getGeneralImage() : pt->getSideIconImage();
#endif
	return image ? image->getName() : AsciiString::TheEmptyString;
}

static UnicodeString getSlotSideName( GameSlot *slot )
{
#if defined(GO_REVEAL_TEAMS)
	const PlayerTemplate* pt = ThePlayerTemplateStore->getNthPlayerTemplate(slot->getPlayerTemplate());
	return pt ? pt->getDisplayName() : slot->getApparentPlayerTemplateDisplayName();
#else
	return slot->getApparentPlayerTemplateDisplayName();
#endif
}

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
enum{
FRAME_TITLES_START = 20,
FRAME_TELETYPE_START = 24,
FRAME_FUDGE_ADD = 30,
FRAME_PORTRAITS_START = 35,
FRAME_OUTER_CIRCLE_LINE_SHOW = 50,
FRAME_INNER_CIRCLE_LINE_SHOW = 52,
FRAME_OUTER_CIRCLE_ALPHA_SHOW = 63,
FRAME_INNER_CIRCLE_ALPHA_SHOW = 74,
FRAME_OUTER_CIRCLE_LINE_HIDE = 75,
FRAME_INNER_BACKDROP_ALPHA_SHOW = 80,
FRAME_INNER_CIRCLE_LINE_HIDE = 81,
FRAME_VS_ANIM_START = 98,
FRAME_RIGHT_VOICE = 140,
};

static const Int TELETYPE_UPDATE_FREQ = 2; // how many frames between teletype updates



//-----------------------------------------------------------------------------
// LoadScreen Class
//-----------------------------------------------------------------------------

LoadScreen::LoadScreen()
{
	m_loadScreen = nullptr;
	m_view = nullptr;
}

LoadScreen::~LoadScreen()
{
	delete m_view;
	if(m_loadScreen)
		TheWindowManager->winDestroy( m_loadScreen );
}

void LoadScreen::publishData()
{
	LoadScreenData &data = LoadScreenData::instance();
	data.touch();
	if (m_view)
		m_view->update( data );
}

void LoadScreen::publishProgress( Int percent )
{
	LoadScreenData &data = LoadScreenData::instance();
	if (data.m_progress != percent)
	{
		data.m_progress = percent;
		publishData();
	}
}

void LoadScreen::update( Int percent )
{
	TheGameEngine->serviceWindowsOS();
	if (TheGameEngine->getQuitting() || (TheGameLogic && TheGameLogic->isQuitToDesktopRequested()))
		return;	//don't bother with any of this if the player is exiting game.

	TheWindowManager->update();
	TheDisplay->update();
	// redraw all views, update the GUI
	TheDisplay->draw();

	setFPMode();
}


// SinglePlayerLoadScreen Class ///////////////////////////////////////////////
//-----------------------------------------------------------------------------
SinglePlayerLoadScreen::SinglePlayerLoadScreen()
{
	m_currentObjectiveLine = 0;
	m_currentObjectiveLineCharacter = 0;
	m_finishedObjectiveText = FALSE;
	m_currentObjectiveWidthOffset = 0;
	m_videoStream = nullptr;
	m_videoBuffer = nullptr;
}

SinglePlayerLoadScreen::~SinglePlayerLoadScreen()
{
	// nothing may draw the buffer once it is gone
	LoadScreenData::instance().m_videos[LOAD_VIDEO_BACKGROUND] = nullptr;
	publishData();

	delete m_videoBuffer;

	if ( m_videoStream )
	{
		m_videoStream->close();
	}

	TheAudio->removeAudioEvent( m_ambientLoopHandle );
}

void SinglePlayerLoadScreen::moveWindows( Int frame )
{
	enum{
		STATE_BEGIN = 250,
		STATE_SHOW_LOCATION = 251,
		STATE_BEGIN_BRIEFING = 255,
//		STATE_BEGIN_ANIMATING_TEXT = 250,
		STATE_SHOW_CAMEO_1 = 434,
		STATE_BEGIN_ANIMATING_TEXT = 356,
		STATE_HIDE_CAMEO_1 = 459,
		STATE_SHOW_CAMEO_2 = 464,
		STATE_HIDE_CAMEO_2 = 492,
		STATE_SHOW_CAMEO_3 = 497,
		STATE_HIDE_CAMEO_3 = 524,
//		STATE_END_ANIM_HEAD = 450,
		STATE_END_ANIMATING_TEXT = 730,
		STATE_END = 730
	};
	if(frame < STATE_BEGIN || frame > STATE_END)
		return;

	LoadScreenData &data = LoadScreenData::instance();

	if( frame == STATE_BEGIN_BRIEFING)
	{
		// add sound support here
		TheAudio->friend_forcePlayAudioEventRTS(&TheCampaignManager->getCurrentMission()->m_briefingVoice);
	}

	if( frame == STATE_BEGIN_ANIMATING_TEXT)
	{
		data.m_showObjectives = TRUE;
		// animate the text and stuff
	}

	if( frame > STATE_BEGIN_ANIMATING_TEXT && frame <= STATE_END_ANIMATING_TEXT && !m_finishedObjectiveText)
	{
		if(m_currentObjectiveLineCharacter >= m_unicodeObjectiveLines[m_currentObjectiveLine].getLength() )
		{
			m_currentObjectiveLine++;
			m_currentObjectiveLineCharacter =0;
		}
		if(m_currentObjectiveLine >= MAX_OBJECTIVE_LINES || m_unicodeObjectiveLines[m_currentObjectiveLine].isEmpty())
		{
			m_finishedObjectiveText = TRUE;
		}
		else
		{
			WideChar wChar = m_unicodeObjectiveLines[m_currentObjectiveLine].getCharAt(m_currentObjectiveLineCharacter);
			data.m_objectiveLines[m_currentObjectiveLine].concat(wChar);
		}
		m_currentObjectiveLineCharacter++;
	}
	switch (frame) {

	case STATE_SHOW_LOCATION:
		data.m_showLocation = TRUE;
		break;
	case STATE_SHOW_CAMEO_1:
		data.m_showUnit[0] = TRUE;
		break;
	case STATE_HIDE_CAMEO_1:
		data.m_showUnit[0] = FALSE;
		break;
	case STATE_SHOW_CAMEO_2:
		data.m_showUnit[1] = TRUE;
		break;
	case STATE_HIDE_CAMEO_2:
		data.m_showUnit[1] = FALSE;
		break;
	case STATE_SHOW_CAMEO_3:
		data.m_showUnit[2] = TRUE;
		break;
	case STATE_HIDE_CAMEO_3:
		data.m_showUnit[2] = FALSE;
		break;
	}

	publishData();
}

void SinglePlayerLoadScreen::init( GameInfo *game )
{
	//No music in SinglePlayerLoadScreen

	static const char *wndPath = "Menus/SinglePlayerLoadScreen.wnd";
	LoadScreenData &data = LoadScreenData::instance();
	data.reset();

	Mission *mission = TheCampaignManager->getCurrentMission();
	Int i = 0;
	for(; i < MAX_OBJECTIVE_LINES; ++i)
	{
		// translate the objective lines
		if(mission->m_missionObjectivesLabel[i].isNotEmpty())
			m_unicodeObjectiveLines[i] = TheGameText->fetch(mission->m_missionObjectivesLabel[i]);
	}
	for(i = 0; i < MAX_DISPLAYED_UNITS; ++i)
		data.m_unitNames[i] = TheGameText->fetch(mission->m_unitNames[i]);
	data.m_location = TheGameText->fetch(mission->m_locationNameLabel);

	// create the layout of the load screen
	m_loadScreen = TheWindowManager->winCreateFromScript( wndPath );
	DEBUG_ASSERTCRASH(m_loadScreen, ("Can't initialize the single player loadscreen"));
	if (usesLegacyView( wndPath ))
	{
		m_view = NEW SinglePlayerLoadScreenView;
		m_view->init( m_loadScreen, game, data );
	}
	data.touch();

	m_currentObjectiveLine = 0;
	m_currentObjectiveWidthOffset = 0;
	m_currentObjectiveLineCharacter = 0;
	m_finishedObjectiveText = FALSE;

	m_ambientLoop.setEventName("LoadScreenAmbient");
	// create the new stream
	m_videoStream = TheVideoPlayer->open( mission->m_movieLabel );
	if ( m_videoStream == nullptr )
	{
		return;
	}

	// Create the new buffer
	m_videoBuffer = TheDisplay->createVideoBuffer();
	if (	m_videoBuffer == nullptr ||
				!m_videoBuffer->allocate(	m_videoStream->width(),
													m_videoStream->height())
		)
	{
		delete m_videoBuffer;
		m_videoBuffer = nullptr;

		if ( m_videoStream )
		{
			m_videoStream->close();
			m_videoStream = nullptr;
		}

		return;
	}

	// format the progress bar: USA to blue, GLA to green, China to red
	// and set the background image
	AsciiString campaignName = TheCampaignManager->getCurrentCampaign()->m_name;
	if (campaignName.compareNoCase("USA") == 0)
	{
		data.m_backgroundImage = "MissionLoad_USA";
		data.m_barColorIndex = 2;
	}
	else if (campaignName.compareNoCase("GLA") == 0)
	{
		data.m_backgroundImage = "MissionLoad_GLA";
		data.m_barColorIndex = 3;
	}
	else if (campaignName.compareNoCase("China") == 0)
	{
		data.m_backgroundImage = "MissionLoad_China";
		data.m_barColorIndex = 1;
	}
	// else leave the default background screen
	publishData();


	if(TheGameLODManager && TheGameLODManager->didMemPass())
	{
		// TheSuperHackers @bugfix Originally this movie render loop stopped rendering when the game window was inactive.
		// This either skipped the movie or caused decompression artifacts. Now the video just keeps playing until it done.

		Int progressUpdateCount = m_videoStream->frameCount() / FRAME_FUDGE_ADD;
		Int shiftedPercent = -FRAME_FUDGE_ADD + 1;
		while (m_videoStream->frameIndex() < m_videoStream->frameCount() - 1 )
		{
			if (GameClient::isMovieAbortRequested())
			{
				break;
			}

			if(!m_videoStream->isFrameReady())
			{
				Sleep(1);
				continue;
			}

			m_videoStream->frameDecompress();
			m_videoStream->frameRender(m_videoBuffer);

#if RTS_GENERALS
			moveWindows( m_videoStream->frameIndex());
#endif

			m_videoStream->frameNext();

			if(m_videoBuffer && data.m_videos[LOAD_VIDEO_BACKGROUND] != m_videoBuffer)
			{
				data.m_videos[LOAD_VIDEO_BACKGROUND] = m_videoBuffer;
				publishData();
			}
			if(m_videoStream->frameIndex() % progressUpdateCount == 0)
			{
				shiftedPercent++;
				if(shiftedPercent >0)
					shiftedPercent = 0;
				Int percent = (shiftedPercent + FRAME_FUDGE_ADD)/1.3;
				TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);
				publishProgress( percent );

			}
			TheWindowManager->update();

			// redraw all views, update the GUI
			TheDisplay->draw();
		}

#if !RTS_GENERALS
		// let the background image show through
		m_videoStream->close();
		m_videoStream = nullptr;
		data.m_videos[LOAD_VIDEO_BACKGROUND] = nullptr;
		publishData();
		TheDisplay->draw();
#endif
	}
	else
	{
#if RTS_GENERALS
		// if we're min speced
		m_videoStream->frameGoto(m_videoStream->frameCount()); // zero based
		while(!m_videoStream->isFrameReady())
			Sleep(1);
		m_videoStream->frameDecompress();
		m_videoStream->frameRender(m_videoBuffer);
		if(m_videoBuffer)
			data.m_videos[LOAD_VIDEO_BACKGROUND] = m_videoBuffer;

		data.m_showObjectives = TRUE;
		for(i = 0; i < MAX_DISPLAYED_UNITS; ++i)
			data.m_showUnit[i] = TRUE;
		data.m_showLocation = TRUE;

		// Audio was choppy so, I chopped it out!
		TheAudio->friend_forcePlayAudioEventRTS(&TheCampaignManager->getCurrentMission()->m_briefingVoice);

		for(Int i = 0; i < MAX_OBJECTIVE_LINES; ++i)
		{
			data.m_objectiveLines[i] = m_unicodeObjectiveLines[i];
		}
		publishData();
#else
		// if we're min spec'ed don't play a movie
#endif

		Int delay = mission->m_voiceLength * 1000;
		Int begin = timeGetTime();
		Int currTime = begin;
		Int fudgeFactor = 0;
		while(begin + delay > currTime )
		{
			fudgeFactor = 30 * ((currTime - begin)/ INT_TO_REAL(delay ));
			publishProgress( fudgeFactor );

			if (GameClient::isMovieAbortRequested())
			{
				break;
			}

			TheWindowManager->update();
			TheDisplay->draw();
			Sleep(100);
			currTime = timeGetTime();
		}


		TheWindowManager->update();
		TheDisplay->draw();

	}
	setFPMode();
	m_ambientLoopHandle = TheAudio->addAudioEvent(&m_ambientLoop);

}

void SinglePlayerLoadScreen::reset()
{
	setLoadScreen(nullptr);
	if (m_view)
		m_view->reset();
}

void SinglePlayerLoadScreen::update( Int percent )
{
	percent = (percent + FRAME_FUDGE_ADD)/1.3;
	TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);
	publishProgress( percent );

	// Do this last!
	LoadScreen::update( percent );
}

void SinglePlayerLoadScreen::setProgressRange( Int min, Int max )
{

}

// LoadScreenMovie Class //////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
LoadScreenMovie::LoadScreenMovie()
{
	m_stream = nullptr;
	m_buffer = nullptr;
	m_playType = WINDOW_PLAY_MOVIE_ONCE;
	m_state = WINDOW_VIDEO_STATE_STOP;
}

LoadScreenMovie::~LoadScreenMovie()
{
	release();
}

void LoadScreenMovie::release()
{
	delete m_buffer;
	m_buffer = nullptr;

	if ( m_stream )
	{
		m_stream->close();
		m_stream = nullptr;
	}
	m_state = WINDOW_VIDEO_STATE_STOP;
}

void LoadScreenMovie::play( const AsciiString &movieName, WindowVideoPlayType playType )
{
	// if we already have a movie playing, kill it.
	release();

	// create the new stream
	VideoStreamInterface *videoStream = TheVideoPlayer->open( movieName );
	if ( videoStream == nullptr )
	{
		return;
	}

	// Create the new buffer
	VideoBuffer *videoBuffer = TheDisplay->createVideoBuffer();
	if (	videoBuffer == nullptr ||
				!videoBuffer->allocate(	videoStream->width(),
													videoStream->height())
		)
	{
		delete videoBuffer;
		videoStream->close();
		return;
	}

	m_stream = videoStream;
	m_buffer = videoBuffer;
	m_playType = playType;
	m_state = WINDOW_VIDEO_STATE_PLAY;
}

void LoadScreenMovie::update()
{
	// Only advance the frame if we're playing
	if ( m_state != WINDOW_VIDEO_STATE_PLAY || !m_stream || !m_buffer )
		return;

	if ( m_stream->isFrameReady())
	{
		m_stream->frameDecompress();
		m_stream->frameRender( m_buffer );
		m_stream->frameNext();

		// If we reach frame Index of 0, we might have to pause, or loop.
		if ( m_stream->frameIndex() == 0 )
		{
			if(m_playType == WINDOW_PLAY_MOVIE_ONCE)
				m_state = WINDOW_VIDEO_STATE_STOP;
			else if (m_playType == WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME)
				m_state = WINDOW_VIDEO_STATE_PAUSE;
		}
	}
}

VideoBuffer *LoadScreenMovie::buffer() const
{
	return (m_state == WINDOW_VIDEO_STATE_PLAY || m_state == WINDOW_VIDEO_STATE_PAUSE) ? m_buffer : nullptr;
}

// ChallengeLoadScreen Class ///////////////////////////////////////////////
//-----------------------------------------------------------------------------
ChallengeLoadScreen::ChallengeLoadScreen()
{
	m_videoStream = nullptr;
	m_videoBuffer = nullptr;

	for (Int i = 0; i < 2; ++i)
	{
		m_textPosBigName[i] = 0;
		m_textPosName[i] = 0;
		m_textPosRank[i] = 0;
		m_textPosStrategy[i] = 0;
	}
}

ChallengeLoadScreen::~ChallengeLoadScreen()
{
	// nothing may draw the buffers once they are gone
	LoadScreenData &data = LoadScreenData::instance();
	for (Int i = 0; i < LOAD_VIDEO_COUNT; ++i)
		data.m_videos[i] = nullptr;
	publishData();

	delete m_videoBuffer;

	if ( m_videoStream )
	{
		m_videoStream->close();
	}

	TheAudio->removeAudioEvent( m_ambientLoopHandle );
}

// accepts the number of chars to advance, the text shown so far, the total text for final display, and the current position of the readout
// returns the updated position of the readout
static Int updateTeletypeText( Int num_chars, UnicodeString &shownText, const UnicodeString &full_text, Int current_text_pos )
{
	WideChar wChar;
	for (Int i = 0; i < num_chars; i++)
	{
		if (current_text_pos < full_text.getLength())
		{
			wChar = full_text.getCharAt(current_text_pos);
			shownText.concat(wChar);
			current_text_pos++;
		}
	}
	return current_text_pos;
}

void ChallengeLoadScreen::activatePieces( Int frame, const GeneralPersona *generalPlayer, const GeneralPersona *generalOpponent )
{
	LoadScreenData &data = LoadScreenData::instance();
	Bool changed = TRUE;

	AudioEventRTS eventLeftGeneral( generalPlayer->getNameSound() );
	AudioEventRTS eventVS("Taunts_GCAnnouncer12");
	AudioEventRTS eventRightGeneral( generalOpponent->getNameSound() );

	switch (frame)
	{
		case FRAME_TITLES_START:
			data.m_showBioTitles = TRUE;
			break;
		case FRAME_TELETYPE_START:
			// reinit the readouts for each new load screen
			for (Int i = 0; i < 2; ++i)
			{
				m_textPosBigName[i] = 0;
				m_textPosName[i] = 0;
				m_textPosRank[i] = 0;
				m_textPosStrategy[i] = 0;

				data.m_generals[i].m_bigName.clear();
				data.m_generals[i].m_name.clear();
				data.m_generals[i].m_rank.clear();
				data.m_generals[i].m_strategy.clear();
			}
			data.m_showBioEntries = TRUE;
			break;
		case FRAME_PORTRAITS_START:

			m_portraitMovieLeft.play( generalPlayer->getPortraitMovieLeftName(), WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME);
			m_portraitMovieRight.play( generalOpponent->getPortraitMovieRightName(), WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME);
			data.m_showPortraitMovies = TRUE;

			TheAudio->addAudioEvent( &eventLeftGeneral );

			break;
		case FRAME_OUTER_CIRCLE_ALPHA_SHOW:
			data.m_showOuterCircle = TRUE;
			break;
		case FRAME_INNER_CIRCLE_ALPHA_SHOW:
			data.m_showInnerCircle = TRUE;
			break;
		case FRAME_INNER_BACKDROP_ALPHA_SHOW:
			data.m_showVersusBackdrop = TRUE;
			break;
		case FRAME_VS_ANIM_START:
			// it's time to start the overlay movie
			data.m_showVersusBackdrop = TRUE;
			data.m_showVersus = TRUE;
			// only the .wnd shows the movie; the RmlUi screen animates its own "vs" off m_showVersus
			if (m_view)
				m_versusMovie.play( "VSSmall", WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME);

			// "Verses"
			TheAudio->addAudioEvent( &eventVS );

			break;
		case FRAME_RIGHT_VOICE:
			TheAudio->addAudioEvent( &eventRightGeneral );
			changed = FALSE;
			break;
		default:
			changed = FALSE;
			break;
	}

	// update the teletype readout
	if (frame > FRAME_TELETYPE_START && (frame % TELETYPE_UPDATE_FREQ) == 0)
	{
		for (Int i = 0; i < 2; ++i)
		{
			LoadScreenGeneral &general = data.m_generals[i];
			m_textPosName[i] = updateTeletypeText( 1, general.m_name, m_bioName[i], m_textPosName[i]);
			m_textPosBigName[i] = updateTeletypeText( 1, general.m_bigName, m_bioName[i], m_textPosBigName[i]);
			m_textPosRank[i] = updateTeletypeText( 1, general.m_rank, m_bioRank[i], m_textPosRank[i]);
			m_textPosStrategy[i] = updateTeletypeText( 1, general.m_strategy, m_bioStrategy[i], m_textPosStrategy[i]);
		}
		changed = TRUE;
	}

	if (changed)
		publishData();
}

void ChallengeLoadScreen::activatePiecesMinSpec(const GeneralPersona *generalPlayer, const GeneralPersona *generalOpponent)
{
	LoadScreenData &data = LoadScreenData::instance();
	const Image *portraits[2] = { generalPlayer->getBioPortraitLarge(), generalOpponent->getBioPortraitLarge() };

	data.m_showBioTitles = TRUE;
	data.m_showBioEntries = TRUE;
	for (Int i = 0; i < 2; ++i)
	{
		LoadScreenGeneral &general = data.m_generals[i];
		general.m_bigName = m_bioName[i];
		general.m_name = m_bioName[i];
		general.m_rank = m_bioRank[i];
		general.m_strategy = m_bioStrategy[i];
		general.m_portrait = portraits[i] ? portraits[i]->getName() : AsciiString::TheEmptyString;
	}
	data.m_showPortraits = TRUE;
	data.m_showOuterCircle = TRUE;
	data.m_showInnerCircle = TRUE;
	data.m_showVersusBackdrop = TRUE;
	data.m_showVersus = TRUE;
	if (m_view)
		m_versusMovie.play( "VSSmall", WINDOW_PLAY_MOVIE_SHOW_LAST_FRAME);
	data.m_videos[LOAD_VIDEO_VERSUS] = m_versusMovie.buffer();
	publishData();
}

void ChallengeLoadScreen::updateMovies()
{
	m_portraitMovieLeft.update();
	m_portraitMovieRight.update();
	m_versusMovie.update();

	LoadScreenData &data = LoadScreenData::instance();
	VideoBuffer *left = m_portraitMovieLeft.buffer();
	VideoBuffer *right = m_portraitMovieRight.buffer();
	VideoBuffer *versus = m_versusMovie.buffer();
	if (data.m_videos[LOAD_VIDEO_PORTRAIT_LEFT] != left || data.m_videos[LOAD_VIDEO_PORTRAIT_RIGHT] != right || data.m_videos[LOAD_VIDEO_VERSUS] != versus)
	{
		data.m_videos[LOAD_VIDEO_PORTRAIT_LEFT] = left;
		data.m_videos[LOAD_VIDEO_PORTRAIT_RIGHT] = right;
		data.m_videos[LOAD_VIDEO_VERSUS] = versus;
		publishData();
	}
}


void ChallengeLoadScreen::init( GameInfo *game )
{
	const Campaign *campaign = TheCampaignManager->getCurrentCampaign();
	const Mission *mission = TheCampaignManager->getCurrentMission();

	// the player general is tied to the campaign
	const GeneralPersona* generalPlayer = TheChallengeGenerals->getPlayerGeneralByCampaignName( campaign->m_name );

	// the opponent general is tied to the mission
	DEBUG_ASSERTCRASH(mission->m_generalName.isNotEmpty(), ("No GeneralName associated with this mission, check Campaign.ini"));
	const GeneralPersona* generalOpponent = TheChallengeGenerals->getGeneralByGeneralName( mission->m_generalName );

	static const char *wndPath = "Menus/ChallengeLoadScreen.wnd";
	LoadScreenData &data = LoadScreenData::instance();
	data.reset();

	const GeneralPersona *generals[2] = { generalPlayer, generalOpponent };
	for (Int i = 0; i < 2; ++i)
	{
		m_bioName[i] = TheGameText->fetch(generals[i]->getBioName());
		m_bioRank[i] = TheGameText->fetch(generals[i]->getBioRank());
		m_bioStrategy[i] = TheGameText->fetch(generals[i]->getBioStrategy());
	}

	// create the layout of the load screen
	m_loadScreen = TheWindowManager->winCreateFromScript( wndPath );
	DEBUG_ASSERTCRASH(m_loadScreen, ("Can't initialize the single player loadscreen"));
	if (usesLegacyView( wndPath ))
	{
		m_view = NEW ChallengeLoadScreenView;
		m_view->init( m_loadScreen, game, data );
	}
	data.touch();

	m_ambientLoop.setEventName("LoadScreenAmbient");

	// create the new background video stream
	m_videoStream = TheVideoPlayer->open( TheCampaignManager->getCurrentMission()->m_movieLabel );

	// Create the new buffer
	m_videoBuffer = TheDisplay->createVideoBuffer();
	if (m_videoBuffer == nullptr || m_videoStream == nullptr || !m_videoBuffer->allocate(	m_videoStream->width(), m_videoStream->height() ))
	{
		delete m_videoBuffer;
		m_videoBuffer = nullptr;

		if ( m_videoStream )
		{
			m_videoStream->close();
			m_videoStream = nullptr;
		}

		return;
	}

	// make sure reticle stuff starts out hidden
	data.m_showOuterCircle = FALSE;
	data.m_showInnerCircle = FALSE;
	data.m_showVersusBackdrop = FALSE;
	data.m_showVersus = FALSE;
	publishData();

	if(TheGameLODManager && TheGameLODManager->didMemPass())
	{
		// TheSuperHackers @bugfix Originally this movie render loop stopped rendering when the game window was inactive.
		// This either skipped the movie or caused decompression artifacts. Now the video just keeps playing until it done.

		Int progressUpdateCount = m_videoStream->frameCount() / FRAME_FUDGE_ADD;
		Int shiftedPercent = -FRAME_FUDGE_ADD + 1;
		while (m_videoStream->frameIndex() < m_videoStream->frameCount() - 1 )
		{
			if (GameClient::isMovieAbortRequested())
			{
				break;
			}

			if(!m_videoStream->isFrameReady())
			{
				Sleep(1);
				continue;
			}

			m_videoStream->frameDecompress();
			m_videoStream->frameRender(m_videoBuffer);
			m_videoStream->frameNext();

			if(m_videoBuffer && data.m_videos[LOAD_VIDEO_BACKGROUND] != m_videoBuffer)
			{
				data.m_videos[LOAD_VIDEO_BACKGROUND] = m_videoBuffer;
				publishData();
			}

			Int frame = m_videoStream->frameIndex();
			if(frame % progressUpdateCount == 0)
			{
				shiftedPercent++;
				if(shiftedPercent >0)
					shiftedPercent = 0;
				Int percent = (shiftedPercent + FRAME_FUDGE_ADD)/1.3;
				TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);
				publishProgress( percent );
			}
			TheWindowManager->update();

			activatePieces(frame, generalPlayer, generalOpponent);
			updateMovies();

			// redraw all views, update the GUI
			TheDisplay->draw();

			TheAudio->update();
		}
	}
	else
	{
		// if we're min speced
		m_videoStream->frameGoto(m_videoStream->frameCount()); // zero based
		while(!m_videoStream->isFrameReady())
		{
			if (GameClient::isMovieAbortRequested())
			{
				return;
			}
			Sleep(1);
		}
		m_videoStream->frameDecompress();
		m_videoStream->frameRender(m_videoBuffer);
		if(m_videoBuffer)
			data.m_videos[LOAD_VIDEO_BACKGROUND] = m_videoBuffer;

		activatePiecesMinSpec(generalPlayer, generalOpponent);

		Int delay = mission->m_voiceLength * 1000;
		Int begin = timeGetTime();
		Int currTime = begin;
		Int fudgeFactor = 0;
		while(begin + delay > currTime )
		{
			fudgeFactor = 30 * ((currTime - begin)/ INT_TO_REAL(delay ));
			publishProgress( fudgeFactor );

			if (GameClient::isMovieAbortRequested())
			{
				break;
			}

			TheWindowManager->update();
			TheDisplay->draw();
			Sleep(100);
			currTime = timeGetTime();
		}

		updateMovies();
		TheWindowManager->update();
		TheDisplay->draw();
	}
	setFPMode();


	AudioEventRTS event( generalOpponent->getRandomTauntSound() );
	TheAudio->addAudioEvent( &event );

	m_ambientLoopHandle = TheAudio->addAudioEvent(&m_ambientLoop);
	TheAudio->update();
}

void ChallengeLoadScreen::reset()
{
	setLoadScreen(nullptr);
	if (m_view)
		m_view->reset();
}

void ChallengeLoadScreen::update( Int percent )
{
	percent = (percent + FRAME_FUDGE_ADD)/1.3;
	TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);
	publishProgress( percent );

	// Do this last!
	LoadScreen::update( percent );
}

void ChallengeLoadScreen::setProgressRange( Int min, Int max )
{

}

// ShellGameLoadScreen Class //////////////////////////////////////////////////
//-----------------------------------------------------------------------------
ShellGameLoadScreen::ShellGameLoadScreen()
{
}

ShellGameLoadScreen::~ShellGameLoadScreen()
{
}

void ShellGameLoadScreen::init( GameInfo *game )
{
	static BOOL firstLoad = TRUE;

	static const char *wndPath = "Menus/ShellGameLoadScreen.wnd";
	LoadScreenData &data = LoadScreenData::instance();
	data.reset();

	// create the layout of the load screen
	m_loadScreen = TheWindowManager->winCreateFromScript( wndPath );
	DEBUG_ASSERTCRASH(m_loadScreen, ("Can't initialize the ShellGame loadscreen"));

	if (firstLoad && TheGameLODManager && TheGameLODManager->didMemPass())
	{
		data.m_titleScreen = TRUE;
		TheWritableGlobalData->m_breakTheMovie = FALSE;
		firstLoad = FALSE;
	}

	if (usesLegacyView( wndPath ))
	{
		m_view = NEW ShellLoadScreenView;
		m_view->init( m_loadScreen, game, data );
	}
	data.touch();
}

void ShellGameLoadScreen::reset()
{
	setLoadScreen(nullptr);
	if (m_view)
		m_view->reset();
}

void ShellGameLoadScreen::update( Int percent )
{
	TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);
	publishProgress( percent );

	// Do this last!
	LoadScreen::update( percent );
}

// MultiPlayerLoadScreen Class //////////////////////////////////////////////////
//-----------------------------------------------------------------------------
MultiPlayerLoadScreen::MultiPlayerLoadScreen()
{
	for(Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_playerLookup[i] = -1;
	}
}

MultiPlayerLoadScreen::~MultiPlayerLoadScreen()
{
	TheAudio->removeAudioEvent( AHSV_StopTheMusicFade );
//	TheAudio->stopAudio( AudioAffect_Music );
}

void MultiPlayerLoadScreen::init( GameInfo *game )
{
	static const char *wndPath = "Menus/MultiplayerLoadScreen.wnd";
	LoadScreenData &data = LoadScreenData::instance();
	data.reset();

	// create the layout of the load screen
	m_loadScreen = TheWindowManager->winCreateFromScript( wndPath );
	DEBUG_ASSERTCRASH(m_loadScreen, ("Can't initialize the Multiplayer loadscreen"));
	GameSlot *lSlot = game->getSlot(game->getLocalSlotNum());
	const PlayerTemplate* pt;
	if (lSlot->getPlayerTemplate() >= 0)
		pt = ThePlayerTemplateStore->getNthPlayerTemplate(lSlot->getPlayerTemplate());
	else
		pt = ThePlayerTemplateStore->findPlayerTemplate( TheNameKeyGenerator->nameToKey("FactionObserver") );

#if RTS_GENERALS
	data.m_backgroundImage = pt->getLoadScreen();
#else
	// portrait, features, and name for the local player's general
	fillLocalGeneral( data, pt, TRUE );
#endif

	AsciiString musicName = pt->getLoadScreenMusic();
	if ( ! musicName.isEmpty() )
	{
		TheAudio->removeAudioEvent( AHSV_StopTheMusicFade );
		AudioEventRTS event( musicName );
		event.setShouldFade( TRUE );

		TheAudio->addAudioEvent( &event );
		TheAudio->update();//Since GameEngine::update() is suspended until after I am gone...

	}

	//DEBUG_ASSERTCRASH(TheNetwork, ("Where the Heck is the Network!!!!"));
	//DEBUG_LOG(("NumPlayers %d", TheNetwork->getNumPlayers()));

	Int netSlot = 0;
	for (Int slotNum = 0; slotNum < MAX_SLOTS; ++slotNum)
	{
		GameSlot *slot = game->getSlot(slotNum);
		if (!slot || !slot->isOccupied())
			continue;

		LoadScreenPlayerRow &row = data.m_rows[netSlot];
		row.m_name = slot->getName();
		row.m_colorIndex = slot->getApparentColor();
		row.m_color = TheMultiplayerSettings->getColor(slot->getApparentColor())->getColor() & 0xFFFFFF;
		row.m_side = getSlotSideName( slot );
		row.m_sideImage = getSlotSideImage( slot );
		row.m_showProgress = !slot->isAI();
		AsciiString teamStr;
		teamStr.format("Team:%d", slot->getTeamNumber() + 1);
		row.m_team = TheGameText->fetch(teamStr);
		m_playerLookup[slotNum] = netSlot; // save our mapping so we can update progress correctly
		netSlot++;
	}
	data.m_rowCount = netSlot;
	fillMapView( data, game );
	data.m_gameMode = TheGameText->fetch( TheGameLogic->getGameMode() == GAME_SKIRMISH ? "GUI:Skirmish" : "GUI:Network" );

	if (usesLegacyView( wndPath ))
	{
		m_view = NEW MultiPlayerLoadScreenView;
		m_view->init( m_loadScreen, game, data );
	}
	data.touch();

	TheGameLogic->initTimeOutValues();
}

void MultiPlayerLoadScreen::reset()
{
	setLoadScreen(nullptr);
	if (m_view)
		m_view->reset();
}

void MultiPlayerLoadScreen::update( Int percent )
{
	if (TheNetwork)
	{
		if(percent <= 100)
			TheNetwork->updateLoadProgress( percent );
		TheNetwork->liteupdate();
	}
	else
	{
		if (percent <= 100)
			TheGameLogic->processProgress( TheGameInfo->getLocalSlotNum(), percent );
	}

	//GadgetProgressBarSetProgress(m_progressBars[TheNetwork->getLocalPlayerID()], percent );

	TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);

	// Do this last!
	LoadScreen::update( percent );
}

void MultiPlayerLoadScreen::processProgress(Int playerId, Int percentage)
{

	if( percentage < 0 || percentage > 100 || playerId >= MAX_SLOTS || playerId < 0 || m_playerLookup[playerId] == -1)
	{
		DEBUG_CRASH(("Percentage %d was passed in for Player %d", percentage, playerId));
		return;
	}
	//DEBUG_LOG(("Percentage %d was passed in for Player %d (in loadscreen position %d)", percentage, playerId, m_playerLookup[playerId]));
	LoadScreenData::instance().m_rows[m_playerLookup[playerId]].m_progress = percentage;
	publishData();
}

// GameSpyLoadScreen Class //////////////////////////////////////////////////
//-----------------------------------------------------------------------------
GameSpyLoadScreen::GameSpyLoadScreen()
{
	for(Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_playerLookup[i] = -1;
	}
}

GameSpyLoadScreen::~GameSpyLoadScreen()
{
}

#if !defined(GENERALS_ONLINE)
extern Int GetAdditionalDisconnectsFromUserFile(Int playerID);
#endif

struct GameSpySlotInfo
{
	UnicodeString m_name;
	const Image *m_rankImage;
	const Image *m_medalImage;
	UnicodeString m_winLoss;
	UnicodeString m_disconnects;
};

// Name (with Elo in quick match), rank, officers club medal, win/loss and disconnect counts for a slot.
static void gatherGameSpySlotInfo( GameInfo *game, GameSpyGameSlot *slot, GameSpySlotInfo &info )
{
	// Get the stats for the player
#if defined(GENERALS_ONLINE)
	PSPlayerStats stats = PSPlayerStats();
	NGMP_OnlineServices_StatsInterface* pStatsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_StatsInterface>();
	if (pStatsInterface != nullptr)
	{
		// Data should be in cache from lobby joins, so we can do this synchronously
		pStatsInterface->getPlayerStatsFromCache(slot->getProfileID(), &stats);
	}
#else
	PSPlayerStats stats = TheGameSpyPSMessageQueue->findPlayerStatsByID(slot->getProfileID());
#endif

	info.m_name = slot->getName();

#if defined(GENERALS_ONLINE)
	// if QM, show ELO
	NGMPGame* pNGMPGame = (NGMPGame*)game;
	if (pNGMPGame->isQMGame())
	{
		info.m_name.format(L"%s (Elo: %d)", slot->getName().str(), stats.elo_rating);
	}
#endif

	DEBUG_LOG(("LoadScreen - populating info for %ls(%d) - stats returned id %d",
		slot->getName().str(), slot->getProfileID(), stats.id));

#if defined(GENERALS_ONLINE)
	Bool isPreorder = false;
#else
	Bool isPreorder = TheGameSpyInfo->didPlayerPreorder(stats.id);
#endif
	Int rankPoints = CalculateRank(stats);
	Int favSide = GetFavoriteSide(stats);

	info.m_medalImage = TheMappedImageCollection->findImageByName("OfficersClubsmall");
	if (!isPreorder)
		info.m_medalImage = nullptr;
	info.m_rankImage = LookupSmallRankImage(favSide, rankPoints);

	// pop wins and losses
	Int numLosses = 0;

	PerGeneralMap::iterator it;
	for(it = stats.losses.begin(); it != stats.losses.end(); ++it)
	{
		numLosses += it->second;
	}
	Int numWins = 0;
	for(it =stats.wins.begin(); it != stats.wins.end(); ++it)
	{
		numWins += it->second;
	}
	info.m_winLoss.format(L"%d/%d", numWins, numLosses);

	// disconnects
	Int numDisconnects = 0;

	for(it =stats.discons.begin(); it != stats.discons.end(); ++it)
	{
		numDisconnects += it->second;
	}
	for(it =stats.desyncs.begin(); it != stats.desyncs.end(); ++it)
	{
		numDisconnects += it->second;
	}
#if !defined(GENERALS_ONLINE)
	numDisconnects += GetAdditionalDisconnectsFromUserFile(stats.id);
#endif

	info.m_disconnects.format(L"%d", numDisconnects);
}

void GameSpyLoadScreen::init( GameInfo *game )
{
#if defined(GENERALS_ONLINE)	// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
	g_bHasDoneSOGScreenshot = FALSE;
#endif

	static const char *wndPath = "Menus/GameSpyLoadScreen.wnd";
	LoadScreenData &data = LoadScreenData::instance();
	data.reset();

	// create the layout of the load screen
	m_loadScreen = TheWindowManager->winCreateFromScript( wndPath );
	DEBUG_ASSERTCRASH(m_loadScreen, ("Can't initialize the Multiplayer loadscreen"));
	DEBUG_ASSERTCRASH(TheNetwork, ("Where the Heck is the Network!!!!"));
	DEBUG_LOG(("NumPlayers %d", TheNetwork->getNumPlayers()));
GameSlot *lSlot = game->getSlot(game->getLocalSlotNum());
	const PlayerTemplate* pt;
	if (lSlot->getPlayerTemplate() >= 0)
		pt = ThePlayerTemplateStore->getNthPlayerTemplate(lSlot->getPlayerTemplate());
	else
		pt = ThePlayerTemplateStore->findPlayerTemplate( TheNameKeyGenerator->nameToKey("FactionObserver") );

#if RTS_GENERALS
	data.m_backgroundImage = pt->getLoadScreen();
#else
	// portrait, features, and name for the local player's general
	fillLocalGeneral( data, pt, FALSE );
#endif

	Int netSlot = 0;
	for (Int slotNum = 0; slotNum < MAX_SLOTS; ++slotNum)
	{
		GameSpyGameSlot *slot = (GameSpyGameSlot *)game->getSlot(slotNum);
		if (!slot || !slot->isOccupied())
			continue;

		GameSpySlotInfo info;
		gatherGameSpySlotInfo( game, slot, info );

		LoadScreenPlayerRow &row = data.m_rows[netSlot];
		row.m_name = info.m_name;
		row.m_colorIndex = slot->getApparentColor();
		row.m_color = TheMultiplayerSettings->getColor(slot->getApparentColor())->getColor() & 0xFFFFFF;
		row.m_side = getSlotSideName( slot );
		row.m_sideImage = getSlotSideImage( slot );
		row.m_winLoss = info.m_winLoss;
		row.m_disconnects = info.m_disconnects;
		row.m_rankImage = info.m_rankImage ? info.m_rankImage->getName() : AsciiString::TheEmptyString;
		row.m_medalImage = info.m_medalImage ? info.m_medalImage->getName() : AsciiString::TheEmptyString;
		row.m_showProgress = !slot->isAI();
		row.m_showStats = !slot->isAI();
		AsciiString teamStr;
		teamStr.format("Team:%d", slot->getTeamNumber() + 1);
		if (slot->isAI() && slot->getTeamNumber() == -1)
			teamStr = "Team:AI";
		row.m_team = TheGameText->fetch(teamStr);
		m_playerLookup[slotNum] = netSlot; // save our mapping so we can update progress correctly
		netSlot++;
	}
	data.m_rowCount = netSlot;
	fillMapView( data, game );
	data.m_gameMode = TheGameText->fetch( "GUI:GeneralsOnline" );

	if (usesLegacyView( wndPath ))
	{
		m_view = NEW GameSpyLoadScreenView;
		m_view->init( m_loadScreen, game, data );
	}
	data.touch();

	TheGameLogic->initTimeOutValues();
}

void GameSpyLoadScreen::reset()
{
	setLoadScreen(nullptr);
	if (m_view)
		m_view->reset();
}

void GameSpyLoadScreen::update( Int percent )
{
	if(percent <= 100)
		TheNetwork->updateLoadProgress( percent );
	TheNetwork->liteupdate();

#if defined(GENERALS_ONLINE)	// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
	if (TheNetwork != nullptr)
	{
		if (percent >= 50)
		{
			if (!g_bHasDoneSOGScreenshot)
			{
				g_bHasDoneSOGScreenshot = true;

				NGMP_OnlineServicesManager::GetInstance()->CaptureScreenshotForProbe(EScreenshotType::SCREENSHOT_TYPE_LOADSCREEN, std::string()); // pass no URI here, wait until we have one received from server
			}
		}
	}
#endif

	//GadgetProgressBarSetProgress(m_progressBars[TheNetwork->getLocalPlayerID()], percent );

#if defined(GENERALS_ONLINE)
	// GENERALS ONLINE: this is ticked in game engine, but game engine doesnt tick for MP loads when the host is complete and remotes arent... do a liteupdate like TheNetwork does
	if (NGMP_OnlineServicesManager::GetInstance() != nullptr)
	{
		NGMP_OnlineServicesManager::GetInstance()->Tick();
	}
#endif
	TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);

	// Do this last!
	LoadScreen::update( percent );
}

void GameSpyLoadScreen::processProgress(Int playerId, Int percentage)
{

	if( percentage < 0 || percentage > 100 || playerId >= MAX_SLOTS || playerId < 0 || m_playerLookup[playerId] == -1)
	{
		DEBUG_CRASH(("Percentage %d was passed in for Player %d", percentage, playerId));
		return;
	}
	//DEBUG_LOG(("Percentage %d was passed in for Player %d (in loadscreen position %d)", percentage, playerId, m_playerLookup[playerId]));
	LoadScreenData::instance().m_rows[m_playerLookup[playerId]].m_progress = percentage;
	publishData();
}

// MapTransferLoadScreen Class //////////////////////////////////////////////////
//-----------------------------------------------------------------------------
MapTransferLoadScreen::MapTransferLoadScreen()
{
	m_oldTimeout = 0;
	for(Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_playerLookup[i] = -1;
		m_oldProgress[i] = -1;
	}
}

MapTransferLoadScreen::~MapTransferLoadScreen()
{
}

void MapTransferLoadScreen::init( GameInfo *game )
{
	static const char *wndPath = "Menus/MapTransferScreen.wnd";
	LoadScreenData &data = LoadScreenData::instance();
	data.reset();

	// create the layout of the load screen
	m_loadScreen = TheWindowManager->winCreateFromScript( wndPath );
	DEBUG_ASSERTCRASH(m_loadScreen, ("Can't initialize the map transfer loadscreen"));
	if (!m_loadScreen)
		return;

	DEBUG_ASSERTCRASH(TheNetwork, ("Where the Heck is the Network?!!!!"));
	DEBUG_LOG(("NumPlayers %d", TheNetwork->getNumPlayers()));

	Int netSlot = 0;
	for (Int slotNum = 0; slotNum < MAX_SLOTS; ++slotNum)
	{
		GameSlot *slot = game->getSlot(slotNum);
		if (!slot || !slot->isHuman())
			continue;

		LoadScreenPlayerRow &row = data.m_rows[netSlot];
		row.m_name = slot->getName();
		row.m_color = TheMultiplayerSettings->getColor(slot->getApparentColor())->getColor() & 0xFFFFFF;
		const GameSlot *gameInfoSlot = TheGameInfo->getConstSlot(slotNum);
		row.m_showProgress = !(slotNum == 0 || (gameInfoSlot && gameInfoSlot->isHuman() && gameInfoSlot->hasMap()));
		m_playerLookup[slotNum] = netSlot; // save our mapping so we can update progress correctly
		netSlot++;
	}
	data.m_rowCount = netSlot;

	if (usesLegacyView( wndPath ))
	{
		m_view = NEW MapTransferLoadScreenView;
		m_view->init( m_loadScreen, game, data );
	}
	data.touch();
}

void MapTransferLoadScreen::reset()
{
	setLoadScreen(nullptr);
	if (m_view)
		m_view->reset();
	for(Int i = 0; i < MAX_SLOTS; ++i)
	{
		m_playerLookup[i] = -1;
		m_oldProgress[i] = -1;
	}
}

void MapTransferLoadScreen::update( Int percent )
{
	if (TheNetwork)
	{
		TheNetwork->liteupdate();
	}

#if defined(GENERALS_ONLINE)	// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
	// GENERALS ONLINE: this is ticked in game engine, but game engine doesnt tick for MP loads and map transfers are while(true)... when the host is complete and remotes arent... do a liteupdate like TheNetwork does
	if (NGMP_OnlineServicesManager::GetInstance() != nullptr)
	{
		NGMP_OnlineServicesManager::GetInstance()->Tick();
	}
#endif

	TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);

	// Do this last!
	LoadScreen::update( percent );
}

void MapTransferLoadScreen::processProgress(Int playerId, Int percentage, AsciiString stateStr)
{

	if( percentage < 0 || percentage > 100 || playerId >= MAX_SLOTS || playerId < 0 || m_playerLookup[playerId] == -1)
	{
		DEBUG_CRASH(("Percentage %d was passed in for Player %d", percentage, playerId));
		return;
	}

	if (m_oldProgress[playerId] == percentage)
		return;
	m_oldProgress[playerId] = percentage;

	LoadScreenPlayerRow &row = LoadScreenData::instance().m_rows[m_playerLookup[playerId]];
	row.m_progress = percentage;
	row.m_status = TheGameText->fetch(stateStr);
	publishData();
}

void MapTransferLoadScreen::processTimeout(Int secondsLeft)
{
	if (m_oldTimeout == secondsLeft)
		return;
	m_oldTimeout = secondsLeft;

	UnicodeString txt;
	txt.format(TheGameText->fetch("MapTransfer:Timeout"), (secondsLeft/60), (secondsLeft%60));
	LoadScreenData::instance().m_timeout = txt;
	publishData();
}

void MapTransferLoadScreen::setCurrentFilename(AsciiString filename)
{
	UnicodeString txt;
	txt.translate(TheGameState->getMapLeafName(filename));
	txt.format(TheGameText->fetch("MapTransfer:CurrentFile"), txt.str());
	LoadScreenData::instance().m_currentFile = txt;
	publishData();
}
