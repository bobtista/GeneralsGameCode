/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "PreRTS.h"

#include "Common/GameUtility.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/Radar.h"

#include "GameClient/ControlBar.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ParticleSys.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/GhostObject.h"
#include "GameLogic/PartitionManager.h"

#include <stdio.h>
#include <stdlib.h>

namespace rts
{

static Bool shouldLogPlayerContext()
{
	return std::getenv("GGC_PLAYER_CONTEXT_DIAG") != nullptr;
}

static int playerIndexOrMinusOne(Player *player)
{
	return player != nullptr ? player->getPlayerIndex() : -1;
}

static const char *playerSideOrNull(Player *player)
{
	return player != nullptr ? player->getSide().str() : "<null>";
}

static void logPlayerContext(const char *event, Player *subject)
{
	if (!shouldLogPlayerContext())
		return;

	Player *local = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
	Player *observed = TheControlBar != nullptr ? TheControlBar->getObservedPlayer() : nullptr;
	Player *lookAt = TheControlBar != nullptr ? TheControlBar->getObserverLookAtPlayer() : nullptr;
	Player *effective = observed != nullptr ? observed : local;

	if (FILE *diag = std::fopen("ggc_player_context_diag.txt", "a"))
	{
		std::fprintf(diag,
			"%s frame=%u subject=%d/%s local=%d/%s observed=%d/%s lookAt=%d/%s effective=%d/%s\n",
			event,
			TheGameLogic != nullptr ? TheGameLogic->getFrame() : 0,
			playerIndexOrMinusOne(subject),
			playerSideOrNull(subject),
			playerIndexOrMinusOne(local),
			playerSideOrNull(local),
			playerIndexOrMinusOne(observed),
			playerSideOrNull(observed),
			playerIndexOrMinusOne(lookAt),
			playerSideOrNull(lookAt),
			playerIndexOrMinusOne(effective),
			playerSideOrNull(effective));
		std::fclose(diag);
	}
}

namespace detail
{
static void changePlayerCommon(Player* player)
{
	logPlayerContext("changePlayerCommon:before", player);
	TheParticleSystemManager->setLocalPlayerIndex(player->getPlayerIndex());
	ThePartitionManager->refreshShroudForLocalPlayer();
	TheGhostObjectManager->setLocalPlayerIndex(player->getPlayerIndex());
	TheGameClient->updateFakeDrawables();
	TheRadar->refreshObjects();
	TheInGameUI->deselectAllDrawables();
	logPlayerContext("changePlayerCommon:after", player);
}

} // namespace detail

bool localPlayerHasRadar()
{
	// Using "local" instead of "observed or local" player because as an observer we prefer
	// the radar to be turned on when observing a player that has no radar.
	const Player* player = ThePlayerList->getLocalPlayer();
	const PlayerIndex index = player->getPlayerIndex();

	if (TheRadar->isRadarForced(index))
		return true;

	if (!TheRadar->isRadarHidden(index) && player->hasRadar())
		return true;

	return false;
}

Player* getObservedOrLocalPlayer()
{
	DEBUG_ASSERTCRASH(TheControlBar != nullptr, ("TheControlBar is null"));
	Player* player = TheControlBar->getObservedPlayer();
	if (player == nullptr)
	{
		DEBUG_ASSERTCRASH(ThePlayerList != nullptr, ("ThePlayerList is null"));
		player = ThePlayerList->getLocalPlayer();
	}
	static Player *lastPlayer = nullptr;
	if (player != lastPlayer)
	{
		logPlayerContext("effectivePlayerChanged", player);
		lastPlayer = player;
	}
	return player;
}

Player* getObservedOrLocalPlayer_Safe()
{
	Player* player = nullptr;

	if (TheControlBar != nullptr)
		player = TheControlBar->getObservedPlayer();

	if (player == nullptr)
		if (ThePlayerList != nullptr)
			player = ThePlayerList->getLocalPlayer();

	return player;
}

PlayerIndex getObservedOrLocalPlayerIndex_Safe()
{
	if (Player* player = getObservedOrLocalPlayer_Safe())
		return player->getPlayerIndex();

	return 0;
}

void changeLocalPlayer(Player* player)
{
	DEBUG_ASSERTCRASH(player != nullptr, ("Player is null"));

	logPlayerContext("changeLocalPlayer:before", player);
	ThePlayerList->setLocalPlayer(player);
	TheControlBar->setObserverLookAtPlayer(nullptr);
	TheControlBar->setObservedPlayer(nullptr);
	TheControlBar->setControlBarSchemeByPlayer(player);
	TheControlBar->initSpecialPowershortcutBar(player);

	detail::changePlayerCommon(player);
	logPlayerContext("changeLocalPlayer:after", player);
}

void changeObservedPlayer(Player* player)
{
	logPlayerContext("changeObservedPlayer:before", player);
	TheControlBar->setObserverLookAtPlayer(player);

	const Bool canBeginObservePlayer = TheGlobalData->m_enablePlayerObserver && TheGhostObjectManager->trackAllPlayers();
	const Bool canEndObservePlayer = TheControlBar->getObservedPlayer() != nullptr && TheControlBar->getObserverLookAtPlayer() == nullptr;

	if (canBeginObservePlayer || canEndObservePlayer)
	{
		TheControlBar->setObservedPlayer(player);

		Player *becomePlayer = player;
		if (becomePlayer == nullptr)
			becomePlayer = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey("ReplayObserver"));
		detail::changePlayerCommon(becomePlayer);
	}
	logPlayerContext("changeObservedPlayer:after", player);
}

} // namespace rts
