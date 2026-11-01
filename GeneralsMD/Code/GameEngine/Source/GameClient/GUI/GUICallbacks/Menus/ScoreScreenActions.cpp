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
// FILE: ScoreScreenActions.cpp ////////////////////////////////////////////////
// See ScoreScreenActions.h. Bodies moved verbatim out of ScoreScreen.cpp's
// GBM_SELECTED/GEM_EDIT_DONE handlers; no behavior change.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/ScoreScreenActions.h"

#include "Common/AudioEventRTS.h"
#include "Common/BattleHonors.h"
#include "Common/GameEngine.h"
#include "Common/GameLOD.h"
#include "Common/GameState.h"
#include "Common/ReplaySimulation.h"
#include "Common/SkirmishBattleHonors.h"
#include "Common/UnicodeString.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/ChallengeGenerals.h"
#include "GameClient/Display.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameClient/RmlUiScreenRegistry.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Shell.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/WindowLayout.h"
#include "GameLogic/FPUControl.h"
#include "GameNetwork/GameSpyOverlay.h"
#include "GameNetwork/LANAPICallbacks.h"
// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
#if defined(GENERALS_ONLINE)
#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_Init.h"
#endif

namespace ScoreScreenActions
{

//-------------------------------------------------------------------------------------------------
void pressOk()
{
	GameSpyCloseOverlay(GSOVERLAY_BUDDY);
	TheShell->pop();
	TheCampaignManager->setCampaign(AsciiString::TheEmptyString);

	if ( ReplaySimulation::getReplayCount() > 0 )
	{
		ReplaySimulation::stop();
		TheGameEngine->setQuitting(TRUE);
	}
}

//-------------------------------------------------------------------------------------------------
Bool pressContinue(ScoreScreenModeType mode, Bool buttonIsFinishCampaign)
{
	Bool replayWasPressed = FALSE;

	if( ReplaySimulation::getReplayCount() > 0 )
	{
		TheGameEngine->setQuitting(TRUE);
	}
	else
	{
		if(!buttonIsFinishCampaign)
			replayWasPressed = TRUE;
		if( mode == SCORESCREENMODE_SINGLEPLAYER)
		{
			AsciiString mapName = TheCampaignManager->getCurrentMap();

			if( mapName.isEmpty() )
			{
				replayWasPressed = FALSE;
				TheShell->pop();
			}
			else
			{

			}
		}
#if defined(GENERALS_ONLINE)	// Ronin @feature 31/10/2026 DX9: Generals Online only; 2003 has no internet Continue
		else if (mode == SCORESCREENMODE_INTERNET)
		{
			NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
			if (pLobbyInterface != nullptr)
			{
				uint64_t currentMatchID = pLobbyInterface->GetCurrentMatchID();

				if (currentMatchID != 0)
				{
					AsciiString strMatchURL;
#if defined(USE_TEST_ENV)
					strMatchURL.format("https://www.playgenerals.online/viewmatch?match=%" PRIu64 "&env=test", currentMatchID);
#else
					strMatchURL.format("https://strata.gamereplays.org/zh/match/%" PRIu64, currentMatchID);
#endif
					ShellExecuteA(NULL, "open", strMatchURL.str(), NULL, NULL, SW_SHOWNORMAL);
				}
			}
		}
#endif
	}

	return replayWasPressed;
}

//-------------------------------------------------------------------------------------------------
void toggleBuddyOverlay()
{
	GameSpyToggleOverlay( GSOVERLAY_BUDDY );
}

//-------------------------------------------------------------------------------------------------
void startSaveReplayFlow()
{
	if (RmlUiScreenRegistry::routesToRmlUi("Menus/PopupReplay.wnd"))
	{
		RmlUiScreenRegistry::open("Menus/PopupReplay.wnd");
		return;
	}

	WindowLayout *saveReplayLayout = TheShell->getPopupReplayLayout();
	DEBUG_ASSERTCRASH( saveReplayLayout, ("Unable to get save replay menu layout.") );
	saveReplayLayout->runInit();
	saveReplayLayout->hide( FALSE );
	saveReplayLayout->bringForward();
}

//-------------------------------------------------------------------------------------------------
void sendChat(const UnicodeString &text, Bool isEmote)
{
	if (!text.isEmpty())
		if (TheLAN)
			TheLAN->RequestPlayerChat(text);	// Ronin @bugfix 31/10/2026 DX9: as upstream's .wnd screens: player chat, a line starting "/me " goes as an emote (both buttons)
			//add the gamespy chat request here
}

//-------------------------------------------------------------------------------------------------
// Decisions moved verbatim out of ScoreScreen.cpp's finishSinglePlayerInit(); no GameWindow calls,
// no behavior change. See ScoreScreenCampaignFinish.
ScoreScreenCampaignFinish finishSinglePlayer()
{
	ScoreScreenCampaignFinish result;
	result.m_victorious = TheCampaignManager->isVictorious();

	Bool isChallengeCampaign = TheCampaignManager->getCurrentCampaign()
		&& TheCampaignManager->getCurrentCampaign()->isChallengeCampaign();

	if (result.m_victorious)
	{
		if (isChallengeCampaign)
		{
			AsciiString name = TheCampaignManager->getCurrentMission()->m_generalName;
			const GeneralPersona *general = TheChallengeGenerals->getGeneralByGeneralName(name);
			result.m_showChallengeSplash = TRUE;
			result.m_challengePortrait = general->getImageDefeated();
			result.m_challengeRemarksText = TheGameText->fetch(general->getStringDefeated());
			result.m_challengeHeaderText.format( TheGameText->fetch("GUI:ChallengeWinText"), TheGameText->fetch(name).str() );

			AudioEventRTS event( general->getWinSound() );
			TheAudio->addAudioEvent( &event );
			TheAudio->update();
		}

		TheCampaignManager->gotoNextMission();

		if (TheCampaignManager->getCurrentMap().isEmpty())
		{
			result.m_continueButtonCaption = TheGameText->fetch("GUI:EndCampaign");
			result.m_campaignComplete = TRUE;

			// mark us as having completed the campaign
			Campaign* campaign = TheCampaignManager->getCurrentCampaign();
			if (campaign)
			{
				GameDifficulty difficulty = TheCampaignManager->getGameDifficulty();
				SkirmishBattleHonors stats;
				if (campaign->m_name.compareNoCase("USA") == 0)
				{
					stats.setUSACampaignComplete(difficulty);
					stats.setHonors(BATTLE_HONOR_CAMPAIGN_USA);
				}

				if (campaign->m_name.compareNoCase("China") == 0)
				{
					stats.setCHINACampaignComplete(difficulty);
					stats.setHonors(BATTLE_HONOR_CAMPAIGN_CHINA);
				}

				if (campaign->m_name.compareNoCase("GLA") == 0)
				{
					stats.setGLACampaignComplete(difficulty);
					stats.setHonors(BATTLE_HONOR_CAMPAIGN_GLA);
				}

				if (campaign->m_name.compareNoCase("GLA") == 0)
				{
					stats.setGLACampaignComplete(difficulty);
					stats.setHonors(BATTLE_HONOR_CAMPAIGN_GLA);
				}

				for (int i = 0; i < MAX_GLOBAL_GENERAL_TYPES; ++i)
				{
					char campaignName[128];
					sprintf(campaignName, "CHALLENGE_%d", i);
					if (campaign->m_name.compareNoCase(campaignName) == 0)
					{
						stats.setChallengeCampaignComplete(i, difficulty);
						stats.setHonors(BATTLE_HONOR_CHALLENGE_MODE);
					}
				}

				stats.write();

				if (campaign->getFinalVictoryMovie().isNotEmpty())
				{
					AsciiString vidName = campaign->getFinalVictoryMovie();
					Bool useLowRes = FALSE;
					if (TheGameLODManager) {
						if (!TheGameLODManager->didMemPass()) {
							useLowRes = TRUE;
						}
						if (TheGameLODManager->getRecommendedStaticLODLevel()==STATIC_GAME_LOD_LOW) {
							useLowRes = TRUE;
						}
						if (TheGameLODManager->getStaticLODLevel()==STATIC_GAME_LOD_LOW) {
							useLowRes = TRUE;
						}
					}
					if (!useLowRes)
						result.m_campaignCompletionMovie = vidName;
				}
			}
		}
		else
		{
			result.m_continueButtonCaption = TheGameText->fetch("GUI:SaveAndContinue");

			// auto save game
			TheGameState->missionSave();
			result.m_showSaveGameText = TRUE;
		}
	}
	else
	{
		if (isChallengeCampaign)
		{
			AsciiString name = TheCampaignManager->getCurrentMission()->m_generalName;
			const GeneralPersona *general = TheChallengeGenerals->getGeneralByGeneralName(name);
			result.m_showChallengeSplash = TRUE;
			result.m_challengePortrait = general->getImageVictorious();
			result.m_challengeRemarksText = TheGameText->fetch(general->getStringVictorious());
			result.m_challengeHeaderText.format( TheGameText->fetch("GUI:ChallengeLossText"), TheGameText->fetch(name).str() );

			AudioEventRTS event( general->getLossSound() );
			TheAudio->addAudioEvent( &event );
			TheAudio->update();
		}

		result.m_continueButtonCaption = TheGameText->fetch("GUI:Retry");
	}

	TheInGameUI->freeMessageResources();

	return result;
}

//-------------------------------------------------------------------------------------------------
void playCampaignCompletionMovie(const AsciiString &movieName)
{
	if (movieName.isEmpty())
		return;

	WindowLayout *blankLayout = TheWindowManager->winCreateLayout("Menus/BlankWindow.wnd");
	DEBUG_ASSERTCRASH(blankLayout,("We Couldn't Load Menus/BlankWindow.wnd"));
	if (!blankLayout)
		return;
	blankLayout->hide(FALSE);
	blankLayout->bringForward();
	blankLayout->getFirstWindow()->winClearStatus(WIN_STATUS_IMAGE);

	VideoStreamInterface *videoStream = TheVideoPlayer->open( movieName );
	if ( videoStream == nullptr )
	{
		blankLayout->destroyWindows();
		deleteInstance(blankLayout);
		return;
	}

	VideoBuffer *videoBuffer = TheDisplay->createVideoBuffer();
	if (	videoBuffer == nullptr ||
				!videoBuffer->allocate(	videoStream->width(), videoStream->height()) )
	{
		delete videoBuffer;
		videoStream->close();
		blankLayout->destroyWindows();
		deleteInstance(blankLayout);
		return;
	}

	GameWindow *movieWindow = blankLayout->getFirstWindow();
	TheWritableGlobalData->m_loadScreenRender = TRUE;
	while (videoStream->frameIndex() < videoStream->frameCount() - 1)
	{
		// User can skip the movie by pressing ESC, same as ScoreScreen.cpp's PlayMovieAndBlock()
		if (TheKeyboard)
		{
			TheKeyboard->UPDATE();
			KeyboardIO *io = TheKeyboard->findKey(KEY_ESC, KeyboardIO::STATUS_UNUSED);
			if (io && BitIsSet(io->state, KEY_STATE_DOWN))
			{
				io->setUsed();
				break;
			}
		}

		TheGameEngine->serviceWindowsOS();

		if(!videoStream->isFrameReady())
		{
			Sleep(1);
			continue;
		}

		videoStream->frameDecompress();
		videoStream->frameRender(videoBuffer);
		videoStream->frameNext();

		movieWindow->winGetInstanceData()->setVideoBuffer(videoBuffer);

		TheDisplay->draw();
	}
	TheWritableGlobalData->m_loadScreenRender = FALSE;
	movieWindow->winGetInstanceData()->setVideoBuffer(nullptr);

	delete videoBuffer;
	videoStream->close();

	setFPMode();

	blankLayout->destroyWindows();
	deleteInstance(blankLayout);
}

}
