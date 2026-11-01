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
// FILE: ScoreScreenData.cpp ///////////////////////////////////////////////////
// See ScoreScreenData.h. Data gathering and side effects (battle honor writes,
// online stats posting) moved out of ScoreScreen.cpp's grabMultiPlayerInfo(),
// grabSinglePlayerInfo(), populatePlayerInfo(), populateSideInfo(),
// setObserverWindows() and their helpers; no behavior change.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/ScoreScreenData.h"

#include "Common/AcademyStats.h"
#include "Common/BattleHonors.h"
#include "Common/GameLOD.h"
#include "Common/GameSpyMiscPreferences.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/ReplaySimulation.h"
#include "Common/ScoreKeeper.h"
#include "Common/SkirmishBattleHonors.h"
#include "Common/ThingFactory.h"
#include "GameClient/GameText.h"
#include "GameClient/Image.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/VictoryConditions.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/GameSpy/GameResultsThread.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/PersistentStorageThread.h"
#include "GameNetwork/NetworkDefs.h"
#include "GameNetwork/NetworkInterface.h"
// Ronin @feature 31/10/2026 DX9: Generals Online kept for that merge, off without GENERALS_ONLINE
#if defined(GENERALS_ONLINE)
#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_Init.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_StatsInterface.h"
#endif

#include <map>

namespace
{

enum
{
	USA_FRIEND = 0,
	CHINA_FRIEND,
	GLA_FRIEND,
	USA_ENEMY,		// Keep friends with friends, enemys with enemys
	CHINA_ENEMY,
	GLA_ENEMY,

	MAX_RELATIONS
};

//-------------------------------------------------------------------------------------------------
// Moved verbatim from ScoreScreen.cpp.
Bool isSlotLocalAlly(GameInfo *game, const GameSlot *slot)
{
	const GameSlot *localSlot = game->getConstSlot(game->getLocalSlotNum());
	if (!localSlot)
		return TRUE;

	if (slot == localSlot)
		return TRUE;

	if (slot->getTeamNumber() < 0)
		return FALSE;

	return slot->getTeamNumber() == localSlot->getTeamNumber();
}

//-------------------------------------------------------------------------------------------------
// Moved verbatim from ScoreScreen.cpp.
void updateSkirmishBattleHonors(SkirmishBattleHonors& stats)
{
	DEBUG_LOG(("Updating Skirmish battle honors"));
	Player *localPlayer = ThePlayerList->getLocalPlayer();
	ScoreKeeper *s = localPlayer->getScoreKeeper();

	if (stats.getWinStreak() >= 5)
		stats.setHonors(BATTLE_HONOR_STREAK);

	// For Apocalypse Honor, see if the player has used each category of super weapon.
	const ThingTemplate *pTemplate = TheThingFactory->findTemplate("GLAScudStorm");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltSCUD();
	pTemplate = TheThingFactory->findTemplate("Boss_GLAScudStorm");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltSCUD();
	pTemplate = TheThingFactory->findTemplate("Chem_GLAScudStorm");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltSCUD();
	pTemplate = TheThingFactory->findTemplate("Slth_GLAScudStorm");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltSCUD();
	pTemplate = TheThingFactory->findTemplate("Demo_GLAScudStorm");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltSCUD();

	pTemplate = TheThingFactory->findTemplate("AmericaParticleCannonUplink");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltParticleCannon();
	pTemplate = TheThingFactory->findTemplate("AirF_AmericaParticleCannonUplink");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltParticleCannon();
	pTemplate = TheThingFactory->findTemplate("Lazr_AmericaParticleCannonUplink");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltParticleCannon();
	pTemplate = TheThingFactory->findTemplate("SupW_AmericaParticleCannonUplink");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltParticleCannon();
	pTemplate = TheThingFactory->findTemplate("Boss_ParticleCannonUplink");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltParticleCannon();

	pTemplate = TheThingFactory->findTemplate("ChinaNuclearMissileLauncher");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltNuke();
	pTemplate = TheThingFactory->findTemplate("Boss_NuclearMissileLauncher");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltNuke();
	pTemplate = TheThingFactory->findTemplate("Infa_ChinaNuclearMissileLauncher");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltNuke();
	pTemplate = TheThingFactory->findTemplate("Nuke_ChinaNuclearMissileLauncher");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltNuke();
	pTemplate = TheThingFactory->findTemplate("Tank_ChinaNuclearMissileLauncher");
	if (s->getTotalObjectsBuilt(pTemplate) > 0)
		stats.setBuiltNuke();
	if (stats.builtNuke() && stats.builtParticleCannon() && stats.builtSCUD())
		stats.setHonors(BATTLE_HONOR_APOCALYPSE);

	KindOfMaskType validMask, invalidMask;
	validMask.set(KINDOF_VEHICLE);
	invalidMask.set(KINDOF_AIRCRAFT);
	if (/*TheGameInfo->isSkirmish() &&*/ s->getTotalUnitsBuilt(validMask, invalidMask) >= 50)
		stats.setHonors(BATTLE_HONOR_BATTLE_TANK);

	validMask.clear();
	validMask.set(KINDOF_AIRCRAFT);
	invalidMask.clear();
	if (/*TheGameInfo->isSkirmish() &&*/ s->getTotalUnitsBuilt(validMask, invalidMask) >= 20)
		stats.setHonors(BATTLE_HONOR_AIR_WING);

	if (/*TheGameInfo->isSkirmish() &&*/ (TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND / 60) < 5)
	{
		stats.setHonors(BATTLE_HONOR_BLITZ5);
	}

	if (/*TheGameInfo->isSkirmish() &&*/ (TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND / 60) < 10)
	{
		stats.setHonors(BATTLE_HONOR_BLITZ10);
	}

	if (stats.getNumGamesLoyal() >= 20 && localPlayer->getSide() == "America")
		stats.setHonors(BATTLE_HONOR_LOYALTY_USA);

	if (stats.getNumGamesLoyal() >= 20 && 	localPlayer->getSide() == "China")
		stats.setHonors(BATTLE_HONOR_LOYALTY_CHINA);

	if (stats.getNumGamesLoyal() >= 20 && 	localPlayer->getSide() == "GLA")
		stats.setHonors(BATTLE_HONOR_LOYALTY_GLA);

	// endurance medal(s)
	Int numEasy = 0;
	Int numMedium = 0;
	Int numBrutal = 0;
	Bool anyAlliedAI = FALSE;
	for (Int i=0; i<MAX_SLOTS; ++i)
	{
		const GameSlot *slot = TheGameInfo->getConstSlot(i);
		if (slot->isAI() && !isSlotLocalAlly(TheGameInfo, slot))
		{
			if (TheGameInfo->getConstSlot(i)->getState() == SLOT_EASY_AI)
				++numEasy;
			if (TheGameInfo->getConstSlot(i)->getState() == SLOT_MED_AI)
				++numMedium;
			if (TheGameInfo->getConstSlot(i)->getState() == SLOT_BRUTAL_AI)
				++numBrutal;
		}
		else if (slot->isAI())
		{
			// can't get challenge medals with AI helpers
			anyAlliedAI = TRUE;
		}
	}
	if (/*!anyAlliedAI &&*/ (numEasy || numMedium || numBrutal))
	{
		Int oldEasy = stats.getEnduranceMedal(TheGameInfo->getMap(), SLOT_EASY_AI);
		Int oldMedium = stats.getEnduranceMedal(TheGameInfo->getMap(), SLOT_MED_AI);
		Int oldBrutal = stats.getEnduranceMedal(TheGameInfo->getMap(), SLOT_BRUTAL_AI);
		if (numEasy)
		{
			stats.setEnduranceMedal(TheGameInfo->getMap(), SLOT_EASY_AI, max(oldEasy, numEasy + numMedium + numBrutal));
		}
		if (numMedium)
		{
			stats.setEnduranceMedal(TheGameInfo->getMap(), SLOT_MED_AI, max(oldMedium, numMedium + numBrutal));
		}
		if (numBrutal)
		{
			stats.setEnduranceMedal(TheGameInfo->getMap(), SLOT_BRUTAL_AI, max(oldBrutal, numBrutal));
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Moved verbatim from ScoreScreen.cpp.
void updateMPBattleHonors(Int& honors, PSPlayerStats& stats)
{
	DEBUG_LOG(("Updating MP battle honors"));
	Player *localPlayer = ThePlayerList->getLocalPlayer();
	ScoreKeeper *s = localPlayer->getScoreKeeper();

	//	BATTLE_HONOR_STREAK 				= 0x0000002,
	if (stats.winsInARow >= 5)
		honors |= BATTLE_HONOR_STREAK;

	//	BATTLE_HONOR_LOYALTY_USA		= 0x0000020,
	if (stats.gamesInRowWithLastGeneral >= 20 && 	localPlayer->getSide() == "America")
		honors |= BATTLE_HONOR_LOYALTY_USA;

	//	BATTLE_HONOR_LOYALTY_CHINA	= 0x0000040,
	if (stats.gamesInRowWithLastGeneral >= 20 && 	localPlayer->getSide() == "China")
		honors |= BATTLE_HONOR_LOYALTY_CHINA;

	//	BATTLE_HONOR_LOYALTY_GLA		= 0x0000060,
	if (stats.gamesInRowWithLastGeneral >= 20 && 	localPlayer->getSide() == "GLA")
		honors |= BATTLE_HONOR_LOYALTY_GLA;

	//	BATTLE_HONOR_BATTLE_TANK		= 0x0000080,
	KindOfMaskType validMask, invalidMask;
	validMask.set(KINDOF_VEHICLE);
	invalidMask.set(KINDOF_AIRCRAFT);
	if (/*TheGameInfo->isSkirmish() &&*/ s->getTotalUnitsBuilt(validMask, invalidMask) >= 50)
		honors |= BATTLE_HONOR_BATTLE_TANK;

	//	BATTLE_HONOR_AIR_WING				= 0x0000100,
	validMask.clear();
	validMask.set(KINDOF_AIRCRAFT);
	invalidMask.clear();
	if (/*TheGameInfo->isSkirmish() &&*/ s->getTotalUnitsBuilt(validMask, invalidMask) >= 20)
		honors |= BATTLE_HONOR_AIR_WING;

	if (stats.builtNuke && stats.builtParticleCannon && stats.builtSCUD)
		honors |= BATTLE_HONOR_APOCALYPSE;

	//	BATTLE_HONOR_BLITZ5					= 0x0008000,
	if (/*TheGameInfo->isSkirmish() &&*/ (TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND / 60) < 5)
	{
		honors |= BATTLE_HONOR_BLITZ5;
	}

	//	BATTLE_HONOR_BLITZ10				= 0x0008000,
	if (/*TheGameInfo->isSkirmish() &&*/ (TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND / 60) < 10)
	{
		honors |= BATTLE_HONOR_BLITZ10;
	}

	//	BATTLE_HONOR_GLOBAL_GENERAL
	Int numGlobalChallengeWins = 0;
	for (int i = GLOBAL_GENERAL_BEGIN; i <= GLOBAL_GENERAL_END; ++i)
	{
		PerGeneralMap::const_iterator pit = stats.wins.find(i);
		if (pit != stats.wins.end())
		{
			if (pit->second > 0)
			{
				++numGlobalChallengeWins;
			}
		}
	}

	if (numGlobalChallengeWins >= MAX_GLOBAL_GENERAL_TYPES)
	{
		honors |= BATTLE_HONOR_GLOBAL_GENERAL;
	}

	/*  NOT IMPLEMENTED YET */
	//	BATTLE_HONOR_LADDER_CHAMP		= 0x0000001, (IGNORED HERE)
	//	BATTLE_HONOR_SPECIAL_FORCES	= 0x0000200, (Not Valid)
	//	BATTLE_HONOR_ENDURANCE			= 0x0000400,
	//	BATTLE_HONOR_CAMPAIGN_USA		= 0x0000800,
	//	BATTLE_HONOR_CAMPAIGN_CHINA	= 0x0001000,
	//	BATTLE_HONOR_CAMPAIGN_GLA  	= 0x0002000,
}

// challenge medals are beating 1-7 Brutal AIs
// Moved verbatim from ScoreScreen.cpp.
void updateChallengeMedals(Int& medals)
{
	if (!TheGameInfo->isSkirmish())
		return;

	Int numAIs = 0;
	Int numBrutals = 0;
	for (Int i=0; i<MAX_SLOTS; ++i)
	{
		const GameSlot *slot = TheGameInfo->getConstSlot(i);
		if (slot->isAI() && !isSlotLocalAlly(TheGameInfo, slot))
		{
			++numAIs;
			if (TheGameInfo->getConstSlot(i)->getState() == SLOT_BRUTAL_AI)
				++numBrutals;
		}
		else if (slot->isAI())
		{
			// can't get challenge medals with AI helpers
			return;
		}
	}

	if (numAIs)
	{
		switch(numBrutals)
		{
		case 1:
			medals |= BH_CHALLENGE_MASK_1;
			break;
		case 2:
			medals |= BH_CHALLENGE_MASK_2;
			break;
		case 3:
			medals |= BH_CHALLENGE_MASK_3;
			break;
		case 4:
			medals |= BH_CHALLENGE_MASK_4;
			break;
		case 5:
			medals |= BH_CHALLENGE_MASK_5;
			break;
		case 6:
			medals |= BH_CHALLENGE_MASK_6;
			break;
		case 7:
			medals |= BH_CHALLENGE_MASK_7;
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Moved verbatim from ScoreScreen.cpp.
inline int CheckForApocalypse( ScoreKeeper *s, const char* szWeapon )
{
	const ThingTemplate* pTemplate = TheThingFactory->findTemplate(szWeapon);
	if( s->getTotalObjectsBuilt(pTemplate) > 0 )
		return 1;
	return 0;
}

//-------------------------------------------------------------------------------------------------
// Skirmish battle honor side effect, moved from populatePlayerInfo()'s
// "screenType == SCORESCREEN_SKIRMISH && player->isLocalPlayer()" block.
void runSkirmishHonorsSideEffect(Player *player)
{
	if (!player->isLocalPlayer())
		return;

	if (TheGameInfo->isSandbox() || !(TheVictoryConditions->isLocalAlliedDefeat() || TheVictoryConditions->isLocalAlliedVictory()))
	{
		// If we died, and are watching sparring AIs, we still get the loss.
		if (player->isPlayerActive())
		{
			DEBUG_LOG(("Skipping skirmish stats update: sandbox:%d defeat:%d victory:%d",
				TheGameInfo->isSandbox(), TheVictoryConditions->isLocalAlliedDefeat(), TheVictoryConditions->isLocalAlliedVictory()));
			return;
		}
	}

	SkirmishBattleHonors stats;

	if (TheVictoryConditions->isLocalAlliedVictory())
	{
		stats.setWins(stats.getWins()+1);
		stats.setWinStreak(stats.getWinStreak()+1);
		stats.setBestWinStreak(max(stats.getBestWinStreak(), stats.getWinStreak()));
		updateSkirmishBattleHonors(stats);
		Int challengeMedals = stats.getChallengeMedals();
		updateChallengeMedals(challengeMedals);
		stats.setChallengeMedals(challengeMedals);
	}
	else
	{
		stats.setLosses(stats.getLosses()+1);
		stats.setWinStreak(0);
	}

	AsciiString lastGeneral = stats.getLastGeneral();
	stats.setLastGeneral(player->getPlayerTemplate()->getSide());
	if (lastGeneral != stats.getLastGeneral())
	{
		stats.setNumGamesLoyal(0);
	}
	else
	{
		stats.setNumGamesLoyal(stats.getNumGamesLoyal()+1);
	}

	stats.write();
}

//-------------------------------------------------------------------------------------------------
// Internet stats posting side effect, moved from populatePlayerInfo()'s
// "screenType == SCORESCREEN_INTERNET" block.
void runInternetStatsSideEffect(Player *player)
{
	DEBUG_LOG(("populatePlayerInfo() - SCORESCREEN_INTERNET\n"));

#if defined(GENERALS_ONLINE)
	if (TheNGMPGame == nullptr)
		return;

	if (!TheNGMPGame->getUseStats()
		&& !TheNGMPGame->isQMGame())  //QuickMatch games always record stats
		return;	//the host has requested not to record stats for this game.
#else
	if (TheGameSpyGame && !TheGameSpyGame->getUseStats()
		&& !TheGameSpyGame->isQMGame() )  //QuickMatch games always record stats
		return;	//the host has requested not to record stats for this game.

	Int localID = TheGameSpyInfo->getLocalProfileID();
	if (localID)
#endif

	{
#if defined(GENERALS_ONLINE)
		Int localSlotNum = TheNGMPGame->getLocalSlotNum();
#else
		Int localSlotNum = TheGameSpyGame->getLocalSlotNum();
#endif
		if (player->isLocalPlayer())
		{
#if defined(GENERALS_ONLINE)
			NGMPGameSlot* localSlot = TheNGMPGame->getGameSpySlot(localSlotNum);
#else
			GameSpyGameSlot* localSlot = TheGameSpyGame->getGameSpySlot(localSlotNum);
#endif
			if (localSlot)
			{
				if (TheVictoryConditions->amIObserver())
				{
					// nothing to track
					DEBUG_LOG(("populatePlayerInfo() - not tracking stats for observer\n"));
					return;
				}

#if defined(GENERALS_ONLINE)
				NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
				int64_t localID = pAuthInterface == nullptr ? -1 : pAuthInterface->GetUserID();

				// do this part sync from cache, should have our own stats + up to date
				NGMP_OnlineServices_StatsInterface* pStatsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_StatsInterface>();
				if (pStatsInterface == nullptr)
				{
					return;
				}

				pStatsInterface->findPlayerStatsByID(localID, [=](bool bSuccess, PSPlayerStats stats)
#else
				PSPlayerStats stats = TheGameSpyPSMessageQueue->findPlayerStatsByID(localID);
#endif
				{
					UnsignedInt latestHumanInGame = 0;
					UnsignedInt lastFrameOfGame = 0;
					Bool gameEndedInDisconnect = TRUE;
					Bool sawAnyDisconnects = FALSE;
					Bool anyNonAI = FALSE;
					Bool anyAI = FALSE;
					Int i = 0;
					for (; i < MAX_SLOTS; ++i)
					{
						const GameSlot* slot = TheGameInfo->getConstSlot(i);
						if (slot->isOccupied() && i != localSlotNum)
						{
							if (slot->isAI())
								anyAI = TRUE;
							else
								anyNonAI = TRUE;
						}
						if (slot->isOccupied())
						{
							lastFrameOfGame = max(lastFrameOfGame, slot->lastFrameInGame());
						}
						if (slot->isHuman())
						{
							if (i != localSlotNum)
							{
								latestHumanInGame = max(latestHumanInGame, slot->lastFrameInGame());
							}
						}
					}
					DEBUG_LOG(("Game ended on frame %d - TheGameLogic->getFrame()=%d\n", lastFrameOfGame - 1, TheGameLogic->getFrame() - 1));
					for (i = 0; i < MAX_SLOTS; ++i)
					{
						const GameSlot* slot = TheGameInfo->getConstSlot(i);
						DEBUG_LOG(("latestHumanInGame=%d, slot->isOccupied()=%d, slot->disconnected()=%d, slot->isAI()=%d, slot->lastFrameInGame()=%d\n",
							latestHumanInGame, slot->isOccupied(), slot->disconnected(), slot->isAI(), slot->lastFrameInGame()));
						if (slot->isOccupied() && slot->disconnected())
						{
							DEBUG_LOG(("Marking game as a possible disconnect game\n"));
							sawAnyDisconnects = TRUE;
						}
						if (slot->isOccupied() && !slot->disconnected() && i != localSlotNum &&
							(slot->isAI() || (slot->lastFrameInGame() >= lastFrameOfGame/*TheGameLogic->getFrame()*/ - 1)))
						{
							DEBUG_LOG(("Marking game as not ending in disconnect\n"));
							gameEndedInDisconnect = FALSE;
						}
					}

					if (!sawAnyDisconnects)
					{
						DEBUG_LOG(("Didn't see any disconnects - making gameEndedInDisconnect == FALSE\n"));
						gameEndedInDisconnect = FALSE;
					}

					if (gameEndedInDisconnect)
					{
#if defined(GENERALS_ONLINE)
						// TODO_NGMP we need to detect this on service.
						// Use victory conditions to avoid penolizing the innocent player for now.
						if (!TheVictoryConditions->isLocalAlliedVictory())
						{
							DEBUG_LOG(("[DISC] we disconnected, gameEndedInDisconnect = true\n"));
						}
						else
						{
							DEBUG_LOG(("[DISC] opponent disconnected, gameEndedInDisconnect = false\n"));
							gameEndedInDisconnect = FALSE;
						}
#else
						// Ronin @bugfix 31/10/2026 DX9: the 2003 test, from ScoreScreen.cpp (his had a misspelt call and no ping-frame check)
						if (latestHumanInGame == TheNetwork->getPingFrame())
						{
							// we pinged on the last frame someone was there - i.e. game ended in a disconnect.
							// check if we were to blame.
							if (TheNetwork->getPingsReceived() < max(1, TheNetwork->getPingsSent()/2)) /// @todo: what's a good percent of pings to have gotten?
							{
								DEBUG_LOG(("We were to blame.  Leaving gameEndedInDisconnect = true"));
							}
							else
							{
								DEBUG_LOG(("We were not to blame.  Changing gameEndedInDisconnect = false"));
								gameEndedInDisconnect = FALSE;
							}
						}
						else
						{
							DEBUG_LOG(("gameEndedInDisconnect, and we didn't ping on last frame.  What's up with that?"));
						}
#endif
					}

					ScoreKeeper* s = player->getScoreKeeper();
					if (!anyNonAI)
					{
						// play against all ai players -- no stats to gather.

						// even though we don't want to register stats, we still want to register the match outcome, since we started it on the backend, it needs finishing too

						return;
					}

					if (anyAI)
					{
						// Holy mis-implemented, Batman, You get no stats for _any_ AI, not _all_ AI.
						// No wonder we fired the whole department.

						// even though we don't want to register stats, we still want to register the match outcome, since we started it on the backend, it needs finishing too

						return;
					}

					//Remove the extra disconnection we add to all games when they start.
					DEBUG_LOG(("populatePlayerInfo() - removing extra disconnect\n"));

					// TODO_NGMP_STATS
					//if (TheGameSpyInfo)
						//TheGameSpyInfo->updateAdditionalGameSpyDisconnections(-1);

					Bool sawEndOfGame = FALSE;
					if (TheVictoryConditions->isLocalAlliedDefeat() || TheVictoryConditions->isLocalAlliedVictory())
					{
						sawEndOfGame = TRUE;
					}
					if (TheVictoryConditions->isLocalDefeat())
					{
						sawEndOfGame = TRUE;
					}
					if (TheNetwork->sawCRCMismatch() || gameEndedInDisconnect)
					{
						sawEndOfGame = TRUE;
					}
					if (!sawEndOfGame)
					{
						DEBUG_LOG(("Not sending results - we didn't finish a game. %d\n", TheVictoryConditions->getEndFrame()));

						// even though we don't want to register stats, we still want to register the match outcome, since we started it on the backend, it needs finishing too

						return;
					}

#if !defined(GENERALS_ONLINE)
					// TODO_NGMP_STATS: Impl ladders again, how did these work in the base game?
					// send ladder results (even if we end the game in the first N seconds)

					if (TheGameSpyGame->getLadderPort() && TheGameSpyGame->getLadderIP().isNotEmpty())
					{
						GameResultsRequest gameResReq;
						gameResReq.hostname = TheGameSpyGame->getLadderIP().str();
						gameResReq.port = TheGameSpyGame->getLadderPort();
						gameResReq.results = TheGameSpyGame->generateLadderGameResultsPacket().str();
						DEBUG_ASSERTCRASH(TheGameResultsQueue, ("No Game Results queue!\n"));
						if (TheGameResultsQueue)
						{
							TheGameResultsQueue->addRequest(gameResReq);
						}
					}

					// TODO_NGMP: Why did they do this? dont register short games?
					if (TheVictoryConditions->getEndFrame() < LOGICFRAMES_PER_SECOND * 25 && TheVictoryConditions->getEndFrame())
					{
						return;
					}

					// generate and send a gameres packet
					AsciiString resultsPacket = TheGameSpyGame->generateGameSpyGameResultsPacket();
					DEBUG_LOG(("About to send results packet: %s\n", resultsPacket.str()));
					PSRequest grReq;
					grReq.requestType = PSRequest::PSREQUEST_SENDGAMERESTOGAMESPY;
					grReq.results = resultsPacket.str();
					TheGameSpyPSMessageQueue->addRequest(grReq);
#endif

					Int ptIdx;
					const PlayerTemplate* myTemplate = player->getPlayerTemplate();
					DEBUG_LOG(("myTemplate = %X(%s)\n", myTemplate, myTemplate->getName().str()));
					for (ptIdx = 0; ptIdx < ThePlayerTemplateStore->getPlayerTemplateCount(); ++ptIdx)
					{
						const PlayerTemplate* nthTemplate = ThePlayerTemplateStore->getNthPlayerTemplate(ptIdx);
						DEBUG_LOG(("nthTemplate = %X(%s)\n", nthTemplate, nthTemplate->getName().str()));
						if (nthTemplate == myTemplate)
						{
							break;
						}
					}

					if (stats.id == 0)
					{
						// we haven't gotten stats for ourselves yet.  Bummer.
						// what we'll do is just update our disconnects in the registry if we disconnected.
						// other than that, there's not much we can do.  :P
						if (gameEndedInDisconnect || TheNetwork->sawCRCMismatch())
						{
							/* @todo: this the right way
							UnsignedInt discons = 0;
							UnsignedInt syncs = 0;
							GetUnsignedIntFromRegistry("", "dc", discons);
							GetUnsignedIntFromRegistry("", "se", sync);
							++discons;
							++syncs;
							SetUnsignedIntInRegistry("", "dc", discons);
							SetUnsignedIntInRegistry("", "se", syncs);
							*/
							DEBUG_LOG(("populatePlayerInfo() - need to save off info for disconnect games!\n"));

#if !defined(GENERALS_ONLINE)
							PSRequest req;
							req.requestType = PSRequest::PSREQUEST_UPDATEPLAYERSTATS;
							req.email = TheGameSpyInfo->getLocalEmail().str();
							req.nick = TheGameSpyInfo->getLocalBaseName().str();
							req.password = "";
							req.player = stats;
							req.addDesync = TheNetwork->sawCRCMismatch();
							req.addDiscon = gameEndedInDisconnect;
							req.lastHouse = ptIdx;
							TheGameSpyPSMessageQueue->addRequest(req);
#endif
						}
						DEBUG_CRASH(("populatePlayerInfo() - not tracking stats - we haven't gotten the original stuff yet\n"));
						return;
					}

					if (TheNetwork->sawCRCMismatch())
					{
						++stats.desyncs[ptIdx];
					}
					else if (gameEndedInDisconnect)
					{
						++stats.discons[ptIdx];
					}
					else if (TheVictoryConditions->isLocalAlliedDefeat() || !TheVictoryConditions->getEndFrame())
					{
						++stats.losses[ptIdx];
					}
					else
					{
						++stats.wins[ptIdx];
					}

					stats.buildingsBuilt[ptIdx] += s->getTotalBuildingsBuilt();
					stats.buildingsKilled[ptIdx] += s->getTotalBuildingsDestroyed();
					stats.buildingsLost[ptIdx] += s->getTotalBuildingsLost();

#if defined(GENERALS_ONLINE)
					if (TheNGMPGame->isQMGame())
#else
					if (TheGameSpyGame->isQMGame())
#endif
					{
						stats.QMGames[ptIdx]++;
					}
					else
					{
						stats.customGames[ptIdx]++;
					}

					if (TheNetwork->sawCRCMismatch())
					{
						stats.lossesInARow = 0;
						stats.desyncsInARow++;
						stats.disconsInARow = 0;
						stats.winsInARow = 0;
						stats.maxDesyncsInARow = max(stats.desyncsInARow, stats.maxDesyncsInARow);
					}
					else if (gameEndedInDisconnect)
					{
						stats.lossesInARow = 0;
						stats.desyncsInARow = 0;
						stats.disconsInARow++;
						stats.winsInARow = 0;
						stats.maxDisconsInARow = max(stats.disconsInARow, stats.maxDisconsInARow);
					}
					else if (TheVictoryConditions->isLocalAlliedVictory())
					{
						stats.lossesInARow = 0;
						stats.desyncsInARow = 0;
						stats.disconsInARow = 0;
						stats.winsInARow++;
						stats.maxWinsInARow = max(stats.winsInARow, stats.maxWinsInARow);
					}
					else
					{
						stats.lossesInARow++;
						stats.desyncsInARow = 0;
						stats.disconsInARow = 0;
						stats.winsInARow = 0;
						stats.maxLossesInARow = max(stats.lossesInARow, stats.maxLossesInARow);
					}

					stats.earnings[ptIdx] += s->getTotalMoneyEarned();
					stats.duration[ptIdx] += TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND / 60; // in minutes
					stats.games[ptIdx]++;
					//since we raise this number when game starts, we need to lower it back on completion
					Int disCons;
					disCons = stats.discons[ptIdx];
					disCons -= 1;
					if (disCons >= 0)
						stats.discons[ptIdx] = disCons;

					stats.gamesAsRandom += (localSlot->getOriginalPlayerTemplate() == PLAYERTEMPLATE_RANDOM);

					if (stats.lastGeneral != ptIdx)
						stats.gamesInRowWithLastGeneral = 0;
					stats.gamesInRowWithLastGeneral++;
					stats.lastGeneral = ptIdx;

					Int gameSize = 0;
					for (int i = 0; i < MAX_SLOTS; ++i)
					{
#if defined(GENERALS_ONLINE)
						if (TheNGMPGame->getConstSlot(i)->isOccupied() && TheNGMPGame->getConstSlot(i)->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER)
#else
						if (TheGameSpyGame->getConstSlot(i)->isOccupied() && TheGameSpyGame->getConstSlot(i)->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER)
#endif
							++gameSize;
					}
					switch (gameSize)
					{
					case 2:
						stats.gamesOf2p[ptIdx]++;
						break;
					case 3:
						stats.gamesOf3p[ptIdx]++;
						break;
					case 4:
						stats.gamesOf4p[ptIdx]++;
						break;
					case 5:
						stats.gamesOf5p[ptIdx]++;
						break;
					case 6:
						stats.gamesOf6p[ptIdx]++;
						break;
					case 7:
						stats.gamesOf7p[ptIdx]++;
						break;
					case 8:
						stats.gamesOf8p[ptIdx]++;
						break;
					default:
						return; // nothing to track.
					}

					stats.surrenders[ptIdx] += TheGameInfo->haveWeSurrendered() || !TheVictoryConditions->getEndFrame();

					AsciiString systemSpec;
					systemSpec.format("LOD%d", TheGameLODManager->getRecommendedStaticLODLevel());
					stats.systemSpec = systemSpec.str();

					stats.techCaptured[ptIdx] += s->getTotalTechBuildingsCaptured();

					stats.unitsBuilt[ptIdx] += s->getTotalUnitsBuilt();
					stats.unitsKilled[ptIdx] += s->getTotalUnitsDestroyed();
					stats.unitsLost[ptIdx] += s->getTotalUnitsLost();

					DEBUG_LOG(("Before game built scud:%d, cannon:%d, nuke:%d\n", stats.builtSCUD, stats.builtParticleCannon, stats.builtNuke));
					stats.builtSCUD += CheckForApocalypse(s, "GLAScudStorm");
					stats.builtSCUD += CheckForApocalypse(s, "Chem_GLAScudStorm");
					stats.builtSCUD += CheckForApocalypse(s, "Demo_GLAScudStorm");
					stats.builtSCUD += CheckForApocalypse(s, "Slth_GLAScudStorm");
					stats.builtParticleCannon += CheckForApocalypse(s, "AmericaParticleCannonUplink");
					stats.builtParticleCannon += CheckForApocalypse(s, "AirF_AmericaParticleCannonUplink");
					stats.builtParticleCannon += CheckForApocalypse(s, "Lazr_AmericaParticleCannonUplink");
					stats.builtParticleCannon += CheckForApocalypse(s, "SupW_AmericaParticleCannonUplink");
					stats.builtNuke += CheckForApocalypse(s, "ChinaNuclearMissileLauncher");
					stats.builtNuke += CheckForApocalypse(s, "Nuke_ChinaNuclearMissileLauncher");
					stats.builtNuke += CheckForApocalypse(s, "Infa_ChinaNuclearMissileLauncher");
					stats.builtNuke += CheckForApocalypse(s, "Tank_ChinaNuclearMissileLauncher");
					DEBUG_LOG(("After game built scud:%d, cannon:%d, nuke:%d\n", stats.builtSCUD, stats.builtParticleCannon, stats.builtNuke));

					// TODO_NGMP_STATS: ladders
#if !defined(GENERALS_ONLINE)
					if (TheGameSpyGame->getLadderPort() && TheGameSpyGame->getLadderIP().isNotEmpty())
					{
						stats.lastLadderPort = TheGameSpyGame->getLadderPort();
						stats.lastLadderHost = TheGameSpyGame->getLadderIP().str();
					}
#endif

					if (!TheNetwork->sawCRCMismatch() && !gameEndedInDisconnect && !TheVictoryConditions->isLocalAlliedDefeat() && TheVictoryConditions->getEndFrame())
					{
						updateMPBattleHonors(stats.battleHonors, stats);
						updateChallengeMedals(stats.challengeMedals);
					}

#if !defined(GENERALS_ONLINE)
					// Ronin @bugfix 31/10/2026 DX9: the 2003 line (his had a garbled name, a debug-build error)
					DEBUG_LOG(("populatePlayerInfo() - tracking stats for %s/%s/%s", TheGameSpyInfo->getLocalBaseName().str(), TheGameSpyInfo->getLocalEmail().str(), TheGameSpyInfo->getLocalPassword().str()));

					PSRequest req;
					req.requestType = PSRequest::PSREQUEST_UPDATEPLAYERSTATS;
					req.email = TheGameSpyInfo->getLocalEmail().str();
					req.nick = TheGameSpyInfo->getLocalBaseName().str();
					req.password = TheGameSpyInfo->getLocalPassword().str();
					req.player = stats;
					req.addDesync = TheNetwork->sawCRCMismatch();
					req.addDiscon = gameEndedInDisconnect;
					req.lastHouse = ptIdx;
					TheGameSpyPSMessageQueue->addRequest(req);
					TheGameSpyPSMessageQueue->trackPlayerStats(stats);

					// force an update of our shtuff
					PSResponse newResp;
					newResp.responseType = PSResponse::PSRESPONSE_PLAYERSTATS;
					newResp.player = stats;
					TheGameSpyPSMessageQueue->addResponse(newResp);

					// cache our stuff for easy reading next time
					GameSpyMiscPreferences mPref;
					mPref.setCachedStats(GameSpyPSMessageQueueInterface::formatPlayerKVPairs(stats).c_str());
					mPref.write();
				}
#else

					pStatsInterface->UpdateMyStats(stats);



				}, EStatsRequestPolicy::CACHED_ONLY); // NOTE: we really need the latest stats here, but this could cause a UI delay...
#endif
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Academy advice gathering, moved from populatePlayerInfo()'s local-player block.
void gatherAcademyAdvice(Player *player, std::vector<UnicodeString> &advice)
{
	if (TheGameLogic->isInSkirmishGame() || TheGameLogic->isInInternetGame())
	{
		AcademyAdviceInfo info;
		if (player->getAcademyStats()->calculateAcademyAdvice(&info))
		{
			for (UnsignedInt i = 0; i < info.numTips; i++)
				advice.push_back(info.advice[i]);
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Data + side effects moved from populatePlayerInfo(). overrideDisplayName mirrors
// ScoreScreen.cpp's overidePlayerDisplayName, which is always TRUE for single player rows
// (grabSinglePlayerInfo) and always FALSE for multiplayer rows (grabMultiPlayerInfo).
void gatherPlayerRow(Player *player, ScoreScreenModeType mode, Bool overrideDisplayName, ScoreScreenPlayerRow &row)
{
	ScoreKeeper *scoreKpr = player->getScoreKeeper();
	if (!scoreKpr)
	{
		DEBUG_ASSERTCRASH(FALSE,("Player %s does not have a scoreKeeper", player->getPlayerDisplayName().str()));
		return;
	}

	row.m_displayName = overrideDisplayName ? TheGameText->fetch("GUI:Player") : player->getPlayerDisplayName();
	row.m_textColor = player->getPlayerColor();
	row.m_isObserver = FALSE;
	row.m_isLocalPlayerRow = (player == ThePlayerList->getLocalPlayer());

	row.m_stats.m_totalUnitsBuilt = scoreKpr->getTotalUnitsBuilt();
	row.m_stats.m_totalUnitsLost = scoreKpr->getTotalUnitsLost();
	row.m_stats.m_totalUnitsDestroyed = scoreKpr->getTotalUnitsDestroyed();
	row.m_stats.m_totalBuildingsBuilt = scoreKpr->getTotalBuildingsBuilt();
	row.m_stats.m_totalBuildingsLost = scoreKpr->getTotalBuildingsLost();
	row.m_stats.m_totalBuildingsDestroyed = scoreKpr->getTotalBuildingsDestroyed();
	row.m_stats.m_totalMoneyEarned = scoreKpr->getTotalMoneyEarned();

	if (row.m_isLocalPlayerRow)
		gatherAcademyAdvice(player, row.m_academyAdvice);

	row.m_touchSideIcon = TRUE;
	const PlayerTemplate *fact = player->getPlayerTemplate();
	if (fact != nullptr)
	{
		row.m_sideIconImage = fact->getSideIconImage();
		row.m_badgeImage = fact->getGeneralImage() ? fact->getGeneralImage() : fact->getSideIconImage();
	}

	if (mode != SCORESCREENMODE_SINGLEPLAYER)
		row.m_isVictor = TheVictoryConditions->hasAchievedVictory(player);

	if (mode == SCORESCREENMODE_SKIRMISH)
		runSkirmishHonorsSideEffect(player);
	else if (mode == SCORESCREENMODE_INTERNET)
		runInternetStatsSideEffect(player);
}

//-------------------------------------------------------------------------------------------------
// Moved from setObserverWindows()'s data (the gadget-filling half stays in ScoreScreen.cpp).
void gatherObserverRow(Player *player, ScoreScreenPlayerRow &row)
{
	row.m_displayName = player->getPlayerDisplayName();
	row.m_textColor = 0xffffffff;
	row.m_isObserver = TRUE;
	row.m_touchSideIcon = TRUE;
	const PlayerTemplate *fact = player->getPlayerTemplate();
	if (fact != nullptr)
	{
		row.m_sideIconImage = fact->getSideIconImage();
		row.m_badgeImage = fact->getSideIconImage();
	}
}

//-------------------------------------------------------------------------------------------------
// The outcome, map and length of the game, for every mode.
void gatherMatchInfo(ScoreScreenData &data)
{
	data.m_localVictory = TheVictoryConditions->isLocalAlliedVictory();
	data.m_localDefeat = !data.m_localVictory && TheVictoryConditions->isLocalAlliedDefeat();

	const MapMetaData *md = TheMapCache ? TheMapCache->findMap(TheGlobalData->m_mapName) : nullptr;
	if (md)
		data.m_mapDisplayName = md->m_displayName;

	UnsignedInt frames = TheVictoryConditions->getEndFrame();
	if (frames == 0)
		frames = TheGameLogic->getFrame();
	data.m_gameSeconds = frames / LOGICFRAMES_PER_SECOND;
}

} // namespace

//-------------------------------------------------------------------------------------------------
ScoreScreenData ScoreScreenData::buildForMultiPlayer(ScoreScreenModeType mode)
{
	ScoreScreenData data;
	data.m_mode = mode;
	gatherMatchInfo(data);

	typedef std::multimap<Int, Player *> ScoreMap;
	typedef ScoreMap::reverse_iterator RevScoreMapIt;

	AsciiString playerName;
	Player *player;
	ScoreMap scores;

	player = ThePlayerList->getLocalPlayer();
	if (player)
	{
		const Image *image = TheMappedImageCollection->findImageByName("MutiPlayer_ScoreScreen");
		if (image)
		{
			data.m_backgroundImage = image;
			data.m_hasBackgroundImage = TRUE;
		}
	}

	// Add each player and score to the map. This allows us to sort the players based on score.
	for (Int i = 0; i < MAX_SLOTS; ++i)
	{
		playerName.format("player%d", i);
		player = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(playerName));
		if (player)
			scores.emplace(player->getScoreKeeper()->calculateScore(), player);
	}

	// display the players based on Score
	RevScoreMapIt revIt;
	for (revIt = scores.rbegin(); revIt != scores.rend(); ++revIt)
	{
		Player *p = revIt->second;
		ScoreScreenPlayerRow row;
		if (p->isPlayerObserver())
			gatherObserverRow(p, row);
		else
			gatherPlayerRow(p, mode, FALSE, row);
		data.m_rows.push_back(row);
	}

	return data;
}

//-------------------------------------------------------------------------------------------------
ScoreScreenData ScoreScreenData::buildForSinglePlayer()
{
	ScoreScreenData data;
	data.m_mode = SCORESCREENMODE_SINGLEPLAYER;
	gatherMatchInfo(data);

	Player *player, *localPlayer;
	localPlayer = ThePlayerList->getLocalPlayer();

	if (localPlayer)
	{
		if (!localPlayer->isPlayerObserver())
		{
			ScoreScreenPlayerRow row;
			gatherPlayerRow(localPlayer, SCORESCREENMODE_SINGLEPLAYER, TRUE, row);
			data.m_rows.push_back(row);
		}
		else
		{
			for (Int k = 0; k < MAX_PLAYER_COUNT; ++k)
			{
				localPlayer = ThePlayerList->getNthPlayer(k);
				if (localPlayer->getPlayerType() == PLAYER_HUMAN)
				{
					ScoreScreenPlayerRow row;
					gatherPlayerRow(localPlayer, SCORESCREENMODE_SINGLEPLAYER, TRUE, row);
					data.m_rows.push_back(row);
					break;
				}
				localPlayer = nullptr;
			}
		}

		PlayerTemplate const *fact = ThePlayerList->getLocalPlayer()->getPlayerTemplate();
		if (fact != nullptr)
		{
			const Image *image = TheMappedImageCollection->findImageByName(ThePlayerList->getLocalPlayer()->getPlayerTemplate()->getScoreScreen());
			if (image)
			{
				data.m_backgroundImage = image;
				data.m_hasBackgroundImage = TRUE;
			}
		}
	}

	if (!localPlayer)
		return data;

	// okay, there's all kinds of hard coding going on here.  THe reason why! well,
	// We have no way of telling what sides we have in the game.  Hence, the hardcoding.
	AsciiString side;
	for (Int j = 0; j < MAX_RELATIONS; ++j)
	{
		Bool isFriend = TRUE;

		switch (j) {
		case USA_ENEMY:
			isFriend = FALSE;
			FALLTHROUGH;
		case USA_FRIEND:
			side.set("USA");
			break;
		case CHINA_ENEMY:
			isFriend = FALSE;
			FALLTHROUGH;
		case CHINA_FRIEND:
			side.set("China");
			break;
		case GLA_ENEMY:
			isFriend = FALSE;
			FALLTHROUGH;
		case GLA_FRIEND:
			side.set("GLA");
			break;
		}

		ScoreGather sg;
		Bool populate = FALSE;
		Color color = 0;
		for (Int i = 0; i < MAX_PLAYER_COUNT; ++i)
		{
			player = ThePlayerList->getNthPlayer(i);
			if (player && player != localPlayer &&
				side.compare(player->getBaseSide()) == 0)
			{
				if ((TheGameLogic->isInSinglePlayerGame() == FALSE) || (player->getListInScoreScreen() == TRUE))
				{
					if ((isFriend == TRUE && localPlayer->getRelationship(player->getDefaultTeam()) == ALLIES) ||
							(isFriend == FALSE && localPlayer->getRelationship(player->getDefaultTeam()) == ENEMIES))
					{
						ScoreKeeper *sk = player->getScoreKeeper();
						sg.m_totalBuildingsBuilt += sk->getTotalBuildingsBuilt();
						sg.m_totalBuildingsDestroyed += sk->getTotalBuildingsDestroyed();
						sg.m_totalBuildingsLost += sk->getTotalBuildingsLost();
						sg.m_totalMoneyEarned += sk->getTotalMoneyEarned();
						sg.m_totalMoneySpent += sk->getTotalMoneySpent();
						sg.m_totalUnitsBuilt += sk->getTotalUnitsBuilt();
						sg.m_totalUnitsDestroyed += sk->getTotalUnitsDestroyed();
						sg.m_totalUnitsLost += sk->getTotalUnitsLost();
						sg.m_sideImage = player->getPlayerTemplate()->getSideIconImage();
						color = player->getPlayerColor();
						populate = TRUE;
					}
				}
			}
		}
		if (populate)
		{
			AsciiString label;
			label.set("GUI:");
			label.concat(side);
			if (isFriend)
				label.concat("Allies");
			else
				label.concat("Enemies");

			ScoreScreenPlayerRow row;
			row.m_displayName = TheGameText->fetch(label);
			row.m_textColor = color;
			row.m_stats = sg;
			if (sg.m_sideImage)
			{
				row.m_touchSideIcon = TRUE;
				row.m_sideIconImage = sg.m_sideImage;
				row.m_badgeImage = sg.m_sideImage;
			}
			data.m_rows.push_back(row);
		}
	}

	return data;
}

//-------------------------------------------------------------------------------------------------
Bool ScoreScreenData::replayHasMoreEntries()
{
	bool hasSimulationReplay = ReplaySimulation::getReplayCount() > 0;
	bool isLastSimulationReplay = ReplaySimulation::getCurrentReplayIndex() == ReplaySimulation::getReplayCount()-1;

	return hasSimulationReplay && !isLastSimulationReplay;
}

//-------------------------------------------------------------------------------------------------
// Static per-mode defaults, moved from ScoreScreen.cpp's init*() functions. Callers patch in
// runtime-discovered overrides (internet match info, buddies-button gating) afterward.
ScoreScreenLayout ScoreScreenLayout::forMode(ScoreScreenModeType mode)
{
	ScoreScreenLayout layout;

	switch (mode)
	{
		case SCORESCREENMODE_SINGLEPLAYER:
			// ScoreScreen.cpp's initSinglePlayer() never calls applyScoreScreenLayout() (the .wnd
			// continue button keeps its .wnd default, then finishSinglePlayerInit() unconditionally
			// un-hides it regardless of victorious/defeat/campaign-complete); a non-.wnd front end
			// has no such default, so show it unconditionally here instead.
			layout.m_showContinueButton = TRUE;
			break;

		case SCORESCREENMODE_SKIRMISH:
			// Academy panel left at its .wnd default; skirmish never touches it.
			break;

		case SCORESCREENMODE_LAN:
			layout.m_showChatEntry = TRUE;
			layout.m_showEmoteButton = TRUE;
			layout.m_showChatBoxBorder = TRUE;
			layout.m_showChatLog = TRUE;
			// No academy in LAN.
			layout.m_touchAcademyPanel = TRUE;
			layout.m_showAcademyPanel = FALSE;
			break;

		case SCORESCREENMODE_INTERNET:
			layout.m_showChatBoxBorder = TRUE;
			layout.m_showChatLog = TRUE;
			// Provide academy advice in internet games.
			layout.m_touchAcademyPanel = TRUE;
			layout.m_showAcademyPanel = TRUE;
#if defined(GENERALS_ONLINE)
			layout.m_showContinueButton = TRUE;
#endif
			break;

		case SCORESCREENMODE_REPLAY:
			layout.m_showContinueButton = ScoreScreenData::replayHasMoreEntries();
			layout.m_touchAcademyPanel = TRUE;
			layout.m_showAcademyPanel = FALSE;
			break;

		default:
			break;
	}

	return layout;
}
