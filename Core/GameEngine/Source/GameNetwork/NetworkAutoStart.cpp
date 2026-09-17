/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#if defined(RTS_DEBUG)

#include <limits.h>

#include "Common/GameEngine.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameNetwork/NetworkInterface.h"
#include "GameClient/ClientInstance.h"
#include "GameClient/MapUtil.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/NetworkAutoStart.h"
#include "GameNetwork/networkutil.h"

namespace
{
enum {
	DefaultStartupTimeoutMilliseconds = 30000,
	ActionRetryMilliseconds = 1000,
	MillisecondsPerSecond = 1000,
};

NetworkAutoStart::Mode s_mode = NetworkAutoStart::MODE_NONE;
NetworkAutoStart::Role s_role = NetworkAutoStart::ROLE_NONE;
Int s_expectedPlayers = 0;
Int s_aiPlayers = 0;
Bool s_teamGame = false;
Bool s_teamGameApplied = false;
Bool s_convertHumansToAI = false;
Int s_garrisonFrame = -1;
Int s_sellTunnelsFrame = -1;
Int s_surrenderFrame = -1;
Bool s_garrisonDone = false;
Bool s_surrenderDone = false;
Int s_lastSellFrame = -1;
const char *const TunnelTemplateName = "GLATunnelNetwork";

enum
{
	GarrisonRetryFrames = 300,
	SellIntervalFrames = 600,
	MaxGarrisonUnits = 4,
};

Int findPlayerTemplateBySide(const char *side)
{
	for (Int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i)
	{
		const PlayerTemplate *pt = ThePlayerTemplateStore->getNthPlayerTemplate(i);
		if (pt != nullptr && pt->getSide().compareNoCase(side) == 0)
		{
			return i;
		}
	}
	return -1;
}

Bool isCompletedTunnel(const Object *obj)
{
	return obj->getTemplate()->getName().compare(TunnelTemplateName) == 0 &&
		!obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
		!obj->getStatusBits().test(OBJECT_STATUS_SOLD) &&
		!obj->isEffectivelyDead();
}
UnsignedInt s_hostAddress = 0;
UnsignedInt s_localAddress = 0;
AsciiString s_playerName;
AsciiString s_mapName;
UnsignedInt s_timeoutMilliseconds = DefaultStartupTimeoutMilliseconds;
UnsignedInt s_startTime = 0;
UnsignedInt s_lastActionTime = 0;
Bool s_hasArguments = false;
Bool s_lobbyOpened = false;
Bool s_directConnectOpened = false;
Bool s_actionPending = false;
Bool s_startRequested = false;
Bool s_gameStarted = false;
Bool s_failed = false;

} // namespace

Bool NetworkAutoStart::setMode(const AsciiString &mode)
{
	s_hasArguments = true;
	if (mode.compareNoCase("direct") == 0)
	{
		s_mode = MODE_DIRECT_CONNECT;
		rts::ClientInstance::setMultiInstance(true);
		rts::ClientInstance::skipPrimaryInstance();
		return true;
	}

	return false;
}

Bool NetworkAutoStart::setHost(Int expectedPlayers)
{
	s_hasArguments = true;
	if (s_role == ROLE_JOIN || expectedPlayers < MIN_EXPECTED_PLAYERS || expectedPlayers > MAX_SLOTS)
	{
		return false;
	}

	s_role = ROLE_HOST;
	s_expectedPlayers = expectedPlayers;
	return true;
}

Bool NetworkAutoStart::setAICount(Int aiPlayers)
{
	s_hasArguments = true;
	if (aiPlayers < 1 || aiPlayers >= MAX_SLOTS)
	{
		return false;
	}

	s_aiPlayers = aiPlayers;
	return true;
}

Bool NetworkAutoStart::setTeamGame()
{
	s_hasArguments = true;
	s_teamGame = true;
	return true;
}

Bool NetworkAutoStart::setConvertHumansToAI()
{
	s_hasArguments = true;
	s_convertHumansToAI = true;
	return true;
}

Bool NetworkAutoStart::setGarrisonFrame(Int frame)
{
	s_hasArguments = true;
	if (frame < 1)
	{
		return false;
	}
	s_garrisonFrame = frame;
	return true;
}

Bool NetworkAutoStart::setSellTunnelsFrame(Int frame)
{
	s_hasArguments = true;
	if (frame < 1)
	{
		return false;
	}
	s_sellTunnelsFrame = frame;
	return true;
}

Bool NetworkAutoStart::setSurrenderFrame(Int frame)
{
	s_hasArguments = true;
	if (frame < 1)
	{
		return false;
	}
	s_surrenderFrame = frame;
	return true;
}

Bool NetworkAutoStart::shouldConvertHumansToAI()
{
	return s_convertHumansToAI;
}

Bool NetworkAutoStart::setJoin(AsciiString hostAddress)
{
	s_hasArguments = true;
	hostAddress.trim();
	if (s_role == ROLE_HOST || hostAddress.isEmpty())
	{
		return false;
	}

	UnsignedInt resolvedAddress = 0;
	if (!ParseIPv4Address(hostAddress, resolvedAddress))
	{
		return false;
	}

	s_role = ROLE_JOIN;
	s_hostAddress = resolvedAddress;
	return true;
}

Bool NetworkAutoStart::setLocalAddress(AsciiString localAddress)
{
	s_hasArguments = true;
	localAddress.trim();
	if (localAddress.isEmpty())
	{
		return false;
	}

	return ParseIPv4Address(localAddress, s_localAddress);
}

Bool NetworkAutoStart::setPlayerName(AsciiString playerName)
{
	s_hasArguments = true;
	playerName.trim();
	if (playerName.isEmpty())
	{
		return false;
	}

	s_playerName = playerName;
	return true;
}

Bool NetworkAutoStart::setMapName(AsciiString mapName)
{
	s_hasArguments = true;
	mapName.trim();
	if (mapName.isEmpty())
	{
		return false;
	}

	s_mapName = mapName;
	return true;
}

Bool NetworkAutoStart::setTimeoutSeconds(Int seconds)
{
	s_hasArguments = true;
	if (seconds < 1 || (UnsignedInt)seconds > UINT_MAX / MillisecondsPerSecond)
	{
		return false;
	}

	s_timeoutMilliseconds = (UnsignedInt)seconds * MillisecondsPerSecond;
	return true;
}

Bool NetworkAutoStart::isEnabled()
{
	return !s_failed && s_mode != MODE_NONE && s_role != ROLE_NONE;
}

Bool NetworkAutoStart::validateConfiguration()
{
	if (s_failed)
	{
		return false;
	}

	if (s_mode == MODE_NONE)
	{
		fail("-autoNetworkMode direct is required");
		return false;
	}

	if (s_role == ROLE_NONE)
	{
		fail("either -autoNetworkHost or -autoNetworkJoin is required");
		return false;
	}

	if (s_role == ROLE_JOIN && s_aiPlayers > 0)
	{
		fail("-autoNetworkAI is only valid with -autoNetworkHost");
		return false;
	}

	return true;
}

Bool NetworkAutoStart::hasFailed()
{
	return s_failed;
}

Bool NetworkAutoStart::shouldOpenLobby()
{
	return s_hasArguments && !s_lobbyOpened && validateConfiguration();
}

void NetworkAutoStart::markLobbyOpened()
{
	s_lobbyOpened = true;
	s_startTime = timeGetTime();
}

Bool NetworkAutoStart::shouldOpenDirectConnect()
{
	if (!s_hasArguments || s_directConnectOpened || !validateConfiguration())
	{
		return false;
	}

	return s_mode == MODE_DIRECT_CONNECT;
}

void NetworkAutoStart::markDirectConnectOpened()
{
	s_directConnectOpened = true;
	if (s_startTime == 0)
	{
		s_startTime = timeGetTime();
	}
}

AsciiString NetworkAutoStart::getMapName()
{
	return s_mapName;
}

UnsignedInt NetworkAutoStart::getLocalAddress()
{
	return s_localAddress;
}

UnicodeString NetworkAutoStart::getPlayerName()
{
	UnicodeString name;
	if (s_playerName.isNotEmpty())
	{
		name.translate(s_playerName);
	}
	else
	{
		name.format(L"AutoNet%02u", rts::ClientInstance::getInstanceId());
	}
	name.truncateTo(g_lanPlayerNameLength);
	return name;
}

Bool NetworkAutoStart::checkTimeout()
{
	if (s_failed || s_gameStarted)
	{
		return true;
	}

	const UnsignedInt now = timeGetTime();
	if (s_startTime != 0 && now - s_startTime >= s_timeoutMilliseconds)
	{
		fail("network match startup timed out");
		return true;
	}

	return false;
}

void NetworkAutoStart::fail(const char *message)
{
	if (s_failed)
	{
		return;
	}

	s_failed = true;
	s_actionPending = false;
	DEBUG_LOG(("NetworkAutoStart failed: %s", message));
	printf("NetworkAutoStart failed: %s\n", message);
	fflush(stdout);
	TheGameEngine->setQuitting(true);
}

void NetworkAutoStart::updateDirectConnect()
{
	if (!isEnabled() || s_mode != MODE_DIRECT_CONNECT || checkTimeout() || TheLAN == nullptr)
	{
		return;
	}

	const UnsignedInt now = timeGetTime();
	if (s_actionPending || (s_lastActionTime != 0 && now - s_lastActionTime < ActionRetryMilliseconds))
	{
		return;
	}

	TheLAN->RequestSetName(getPlayerName());
	s_lastActionTime = now;
	s_actionPending = true;

	if (s_role == ROLE_HOST)
	{
		DEBUG_LOG(("NetworkAutoStart creating Direct Connect game for %d players", s_expectedPlayers));
		TheLAN->RequestGameCreate(UnicodeString::TheEmptyString, true);
	}
	else
	{
		DEBUG_LOG(("NetworkAutoStart joining Direct Connect host 0x%08X", s_hostAddress));
		TheLAN->RequestGameJoinDirectConnect(s_hostAddress);
	}
}

void NetworkAutoStart::updateGameOptions()
{
	if (!isEnabled() || checkTimeout() || TheLAN == nullptr || TheLAN->GetMyGame() == nullptr)
	{
		return;
	}

	LANGameInfo *game = TheLAN->GetMyGame();
	if (s_role == ROLE_JOIN)
	{
		const Int localSlot = game->getLocalSlotNum();
		if (localSlot < 0)
		{
			return;
		}

		LANGameSlot *slot = game->getLANSlot(localSlot);
		const UnsignedInt now = timeGetTime();
		if (slot != nullptr && !slot->isAccepted() &&
				(s_lastActionTime == 0 || now - s_lastActionTime >= ActionRetryMilliseconds))
		{
			TheLAN->RequestHasMap();
			if (!slot->hasMap() && !CanTransferMap(game->getMap()))
			{
				fail("required map is unavailable and cannot be transferred");
				return;
			}

			TheLAN->RequestAccept();
			s_lastActionTime = now;
		}
		return;
	}

	if (s_startRequested)
	{
		return;
	}

	const MapMetaData *mapData = TheMapCache->findMap(game->getMap());
	if (mapData == nullptr)
	{
		fail("selected map was not found");
		return;
	}
	if (mapData->m_numPlayers < s_expectedPlayers + s_aiPlayers)
	{
		fail("selected map has fewer slots than -autoNetworkHost and -autoNetworkAI require");
		return;
	}

	Int humanPlayers = 0;
	Int aiPlayers = 0;
	for (Int humanIndex = 0; humanIndex < MAX_SLOTS; ++humanIndex)
	{
		LANGameSlot *slot = game->getLANSlot(humanIndex);
		if (slot != nullptr && slot->isHuman())
		{
			++humanPlayers;
		}
		if (slot != nullptr && slot->isAI())
		{
			++aiPlayers;
		}
	}

	if (humanPlayers > s_expectedPlayers)
	{
		fail("more players joined than -autoNetworkHost expects");
		return;
	}
	if (humanPlayers != s_expectedPlayers)
	{
		return;
	}

	if (aiPlayers < s_aiPlayers)
	{
		const UnsignedInt aiNow = timeGetTime();
		if (s_lastActionTime == 0 || aiNow - s_lastActionTime >= ActionRetryMilliseconds)
		{
			Int aiToAdd = s_aiPlayers - aiPlayers;
			for (Int aiIndex = 0; aiIndex < MAX_SLOTS && aiToAdd > 0; ++aiIndex)
			{
				LANGameSlot *slot = game->getLANSlot(aiIndex);
				if (slot != nullptr && slot->getState() == SLOT_OPEN)
				{
					slot->setState(SLOT_BRUTAL_AI);
					--aiToAdd;
				}
			}
			DEBUG_LOG(("NetworkAutoStart filling %d slots with hard AI", s_aiPlayers - aiPlayers));
			game->resetAccepted();
			TheLAN->RequestGameOptions(GenerateGameOptionsString(), true);
			lanUpdateSlotList();
			s_lastActionTime = aiNow;
		}
		return;
	}

	if (s_teamGame && !s_teamGameApplied)
	{
		const Int glaTemplate = findPlayerTemplateBySide("GLA");
		const Int chinaTemplate = findPlayerTemplateBySide("China");
		if (glaTemplate < 0 || chinaTemplate < 0)
		{
			fail("GLA or China player template not found for -autoNetworkTeamGame");
			return;
		}
		for (Int teamIndex = 0; teamIndex < MAX_SLOTS; ++teamIndex)
		{
			LANGameSlot *slot = game->getLANSlot(teamIndex);
			if (slot == nullptr)
			{
				continue;
			}
			if (slot->isHuman())
			{
				slot->setTeamNumber(0);
				slot->setPlayerTemplate(teamIndex == 0 ? glaTemplate : chinaTemplate);
			}
			else if (slot->isAI())
			{
				slot->setTeamNumber(1);
			}
		}
		DEBUG_LOG(("NetworkAutoStart arranged a team game: humans on team 0 (host GLA, others China), AI on team 1"));
		s_teamGameApplied = true;
		game->resetAccepted();
		TheLAN->RequestGameOptions(GenerateGameOptionsString(), true);
		lanUpdateSlotList();
		s_lastActionTime = timeGetTime();
		return;
	}

	LANGameSlot *hostSlot = game->getLANSlot(0);
	if (hostSlot == nullptr)
	{
		fail("host slot is unavailable");
		return;
	}
	hostSlot->setAccept();
	for (Int acceptedIndex = 0; acceptedIndex < MAX_SLOTS; ++acceptedIndex)
	{
		LANGameSlot *slot = game->getLANSlot(acceptedIndex);
		if (slot != nullptr && slot->isHuman() && !slot->isAccepted())
		{
			return;
		}
	}

	const UnsignedInt now = timeGetTime();
	if (s_lastActionTime == 0 || now - s_lastActionTime >= ActionRetryMilliseconds)
	{
		DEBUG_LOG(("NetworkAutoStart starting Direct Connect game with %d players", humanPlayers));
		s_lastActionTime = now;
		s_startRequested = StartLANGame();
		if (!s_startRequested)
		{
			fail("LAN start validation rejected the match; see LAN system messages");
		}
	}
}

void NetworkAutoStart::onGameCreate(LANAPIInterface::ReturnType result)
{
	if (!isEnabled())
	{
		return;
	}

	if (result == LANAPIInterface::RET_OK)
	{
		return;
	}

	s_actionPending = false;
	fail("could not create Direct Connect game");
}

void NetworkAutoStart::onGameJoin(LANAPIInterface::ReturnType result)
{
	if (!isEnabled())
	{
		return;
	}

	if (result == LANAPIInterface::RET_OK)
	{
		return;
	}

	s_actionPending = false;
	if (result == LANAPIInterface::RET_TIMEOUT || result == LANAPIInterface::RET_GAME_GONE)
	{
		DEBUG_LOG(("NetworkAutoStart will retry Direct Connect join after result %d", result));
		return;
	}

	fail("Direct Connect join was rejected");
}

void NetworkAutoStart::onLocalAddressSet(Bool result)
{
	if (isEnabled() && !result)
	{
		fail("could not bind the Direct Connect local address");
	}
}

void NetworkAutoStart::onGameStartFailure()
{
	if (isEnabled())
	{
		fail("required map could not be transferred");
	}
}

void NetworkAutoStart::onGameStart()
{
	if (!isEnabled())
	{
		return;
	}

	s_gameStarted = true;
	DEBUG_LOG(("NetworkAutoStart requested network game startup"));
	printf("NetworkAutoStart requested network game startup\n");
	fflush(stdout);
}


void NetworkAutoStart::updateInGame()
{
	if (!s_hasArguments || TheGameLogic == nullptr || !TheGameLogic->isInGame() || TheNetwork == nullptr || ThePlayerList == nullptr)
	{
		return;
	}

	Player *local = ThePlayerList->getLocalPlayer();
	if (local == nullptr)
	{
		return;
	}

	const Int frame = static_cast<Int>(TheGameLogic->getFrame());

	if (s_garrisonFrame > 0 && !s_garrisonDone && frame >= s_garrisonFrame && (frame - s_garrisonFrame) % GarrisonRetryFrames == 0)
	{
		Object *tunnel = nullptr;
		std::vector<ObjectID> riders;
		for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
		{
			if (obj->getControllingPlayer() != local || obj->isEffectivelyDead())
			{
				continue;
			}
			if (tunnel == nullptr && isCompletedTunnel(obj))
			{
				tunnel = obj;
			}
			else if (riders.size() < MaxGarrisonUnits && !obj->isContained() && !obj->isKindOf(KINDOF_AIRCRAFT) &&
				!obj->isKindOf(KINDOF_DOZER) && !obj->isKindOf(KINDOF_HARVESTER) &&
				(obj->isKindOf(KINDOF_INFANTRY) || obj->isKindOf(KINDOF_VEHICLE)))
			{
				riders.push_back(obj->getID());
			}
		}
		if (tunnel != nullptr && !riders.empty())
		{
			GameMessage *teamMsg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
			teamMsg->appendBooleanArgument(TRUE);
			for (size_t i = 0; i < riders.size(); ++i)
			{
				teamMsg->appendObjectIDArgument(riders[i]);
			}
			GameMessage *enterMsg = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
			enterMsg->appendObjectIDArgument(INVALID_ID);
			enterMsg->appendObjectIDArgument(tunnel->getID());
			DEBUG_LOG(("NetworkAutoStart frame %d: ordering %u units into tunnel id %u", frame, static_cast<UnsignedInt>(riders.size()), tunnel->getID()));
			printf("NetworkAutoStart frame %d: ordering %u units into tunnel id %u\n", frame, static_cast<UnsignedInt>(riders.size()), tunnel->getID());
			fflush(stdout);
			s_garrisonDone = true;
		}
		else
		{
			DEBUG_LOG(("NetworkAutoStart frame %d: no completed tunnel or no units to garrison yet", frame));
			printf("NetworkAutoStart frame %d: no completed tunnel or no units to garrison yet\n", frame);
			fflush(stdout);
		}
	}

	if (s_sellTunnelsFrame > 0 && frame >= s_sellTunnelsFrame && (s_lastSellFrame < 0 || frame - s_lastSellFrame >= SellIntervalFrames))
	{
		for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
		{
			if (obj->getControllingPlayer() == local && isCompletedTunnel(obj))
			{
				GameMessage *teamMsg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
				teamMsg->appendBooleanArgument(TRUE);
				teamMsg->appendObjectIDArgument(obj->getID());
				TheMessageStream->appendMessage(GameMessage::MSG_SELL);
				DEBUG_LOG(("NetworkAutoStart frame %d: selling tunnel id %u", frame, obj->getID()));
				printf("NetworkAutoStart frame %d: selling tunnel id %u\n", frame, obj->getID());
				fflush(stdout);
				break;
			}
		}
		s_lastSellFrame = frame;
	}

	if (s_surrenderFrame > 0 && !s_surrenderDone && frame >= s_surrenderFrame)
	{
		GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_SELF_DESTRUCT);
		msg->appendBooleanArgument(TRUE);
		if (TheInGameUI != nullptr)
		{
			TheInGameUI->setClientQuiet(TRUE);
		}
		DEBUG_LOG(("NetworkAutoStart frame %d: surrendering with asset transfer", frame));
		printf("NetworkAutoStart frame %d: surrendering with asset transfer\n", frame);
		fflush(stdout);
		s_surrenderDone = true;
	}
}

#endif
