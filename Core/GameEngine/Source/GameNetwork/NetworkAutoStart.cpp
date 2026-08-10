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

#include "GameClient/ClientInstance.h"
#include "GameClient/MapUtil.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/NetworkAutoStart.h"
#include "GameNetwork/networkutil.h"

namespace {
NetworkAutoStart::Mode s_mode = NetworkAutoStart::MODE_NONE;
NetworkAutoStart::Role s_role = NetworkAutoStart::ROLE_NONE;
Int s_expectedPlayers = 0;
AsciiString s_hostAddress;
UnsignedInt s_localAddress = 0;
AsciiString s_playerName;
AsciiString s_mapName;
UnsignedInt s_timeoutMilliseconds = 30000;
UnsignedInt s_startTime = 0;
UnsignedInt s_lastActionTime = 0;
Bool s_hasArguments = false;
Bool s_directConnectOpened = false;
Bool s_actionPending = false;
Bool s_startRequested = false;
Bool s_gameStarted = false;
Bool s_failed = false;

const UnsignedInt ACTION_RETRY_MILLISECONDS = 1000;

Bool CanAcceptMap(LANGameInfo *game, LANGameSlot *slot) {
  if (slot->hasMap())
    return true;

  const MapMetaData *mapData = TheMapCache->findMap(game->getMap());
  if (mapData != nullptr)
    return !mapData->m_isOfficial;

  return WouldMapTransfer(game->getMap());
}
} // namespace

Bool NetworkAutoStart::setMode(AsciiString mode) {
  s_hasArguments = true;
  if (mode.compareNoCase("direct") == 0) {
    s_mode = MODE_DIRECT_CONNECT;
    rts::ClientInstance::setMultiInstance(true);
    return true;
  }

  return false;
}

Bool NetworkAutoStart::setHost(Int expectedPlayers) {
  s_hasArguments = true;
  if (s_role == ROLE_JOIN || expectedPlayers < 2 || expectedPlayers > MAX_SLOTS)
    return false;

  s_role = ROLE_HOST;
  s_expectedPlayers = expectedPlayers;
  rts::ClientInstance::setMultiInstance(true);
  return true;
}

Bool NetworkAutoStart::setJoin(AsciiString hostAddress) {
  s_hasArguments = true;
  if (s_role == ROLE_HOST || hostAddress.isEmpty())
    return false;

  s_role = ROLE_JOIN;
  s_hostAddress = hostAddress;
  rts::ClientInstance::setMultiInstance(true);
  return true;
}

Bool NetworkAutoStart::setLocalAddress(AsciiString localAddress) {
  s_hasArguments = true;
  localAddress.trim();
  if (localAddress.isEmpty())
    return false;

  s_localAddress = ResolveIP(localAddress);
  return s_localAddress != 0;
}

Bool NetworkAutoStart::setPlayerName(AsciiString playerName) {
  s_hasArguments = true;
  playerName.trim();
  if (playerName.isEmpty())
    return false;

  s_playerName = playerName;
  return true;
}

Bool NetworkAutoStart::setMapName(AsciiString mapName) {
  s_hasArguments = true;
  mapName.trim();
  if (mapName.isEmpty())
    return false;

  s_mapName = mapName;
  return true;
}

Bool NetworkAutoStart::setTimeoutSeconds(Int seconds) {
  s_hasArguments = true;
  if (seconds < 1)
    return false;

  s_timeoutMilliseconds = (UnsignedInt)seconds * 1000;
  return true;
}

Bool NetworkAutoStart::hasArguments() { return s_hasArguments; }

Bool NetworkAutoStart::isEnabled() {
  return !s_failed && s_mode != MODE_NONE && s_role != ROLE_NONE;
}

Bool NetworkAutoStart::validateConfiguration() {
  if (s_failed)
    return false;

  if (s_mode == MODE_NONE) {
    fail("-autoNetworkMode direct is required");
    return false;
  }

  if (s_role == ROLE_NONE) {
    fail("either -autoNetworkHost or -autoNetworkJoin is required");
    return false;
  }

  return true;
}

Bool NetworkAutoStart::shouldOpenDirectConnect() {
  return s_mode == MODE_DIRECT_CONNECT && !s_directConnectOpened &&
         validateConfiguration();
}

void NetworkAutoStart::markDirectConnectOpened() {
  s_directConnectOpened = true;
  if (s_startTime == 0)
    s_startTime = timeGetTime();
}

AsciiString NetworkAutoStart::getMapName() { return s_mapName; }

UnsignedInt NetworkAutoStart::getLocalAddress() { return s_localAddress; }

UnicodeString NetworkAutoStart::getPlayerName() {
  UnicodeString name;
  if (s_playerName.isNotEmpty()) {
    name.translate(s_playerName);
  } else {
    name.format(L"AutoNet%02u", rts::ClientInstance::getInstanceId());
  }
  name.truncateTo(g_lanPlayerNameLength);
  return name;
}

Bool NetworkAutoStart::checkTimeout() {
  if (s_failed || s_gameStarted)
    return true;

  const UnsignedInt now = timeGetTime();
  if (s_startTime != 0 && now - s_startTime >= s_timeoutMilliseconds) {
    fail("network match startup timed out");
    return true;
  }

  return false;
}

void NetworkAutoStart::fail(const char *message) {
  if (s_failed)
    return;

  s_failed = true;
  s_actionPending = false;
  DEBUG_LOG(("NetworkAutoStart failed: %s", message));
  printf("NetworkAutoStart failed: %s\n", message);
}

void NetworkAutoStart::updateDirectConnect() {
  if (!isEnabled() || s_mode != MODE_DIRECT_CONNECT || checkTimeout() ||
      TheLAN == nullptr)
    return;

  const UnsignedInt now = timeGetTime();
  if (s_actionPending || (s_lastActionTime != 0 &&
                          now - s_lastActionTime < ACTION_RETRY_MILLISECONDS))
    return;

  TheLAN->RequestSetName(getPlayerName());
  s_lastActionTime = now;
  s_actionPending = true;

  if (s_role == ROLE_HOST) {
    DEBUG_LOG(("NetworkAutoStart creating Direct Connect game for %d players",
               s_expectedPlayers));
    TheLAN->RequestGameCreate(UnicodeString::TheEmptyString, true);
  } else {
    const UnsignedInt hostIP = ResolveIP(s_hostAddress);
    if (hostIP == 0) {
      s_actionPending = false;
      fail("could not resolve Direct Connect host address");
      return;
    }

    DEBUG_LOG(("NetworkAutoStart joining Direct Connect host %s",
               s_hostAddress.str()));
    TheLAN->RequestGameJoinDirectConnect(hostIP);
  }
}

void NetworkAutoStart::updateGameOptions() {
  if (!isEnabled() || checkTimeout() || TheLAN == nullptr ||
      TheLAN->GetMyGame() == nullptr)
    return;

  LANGameInfo *game = TheLAN->GetMyGame();
  if (s_role == ROLE_JOIN) {
    const Int localSlot = game->getLocalSlotNum();
    if (localSlot < 0)
      return;

    LANGameSlot *slot = game->getLANSlot(localSlot);
    const UnsignedInt now = timeGetTime();
    if (slot != nullptr && !slot->isAccepted() &&
        (s_lastActionTime == 0 ||
         now - s_lastActionTime >= ACTION_RETRY_MILLISECONDS)) {
      TheLAN->RequestHasMap();
      if (!CanAcceptMap(game, slot)) {
        fail("required map is unavailable and cannot be transferred");
        return;
      }

      TheLAN->RequestAccept();
      s_lastActionTime = now;
    }
    return;
  }

  if (s_startRequested)
    return;

  const MapMetaData *mapData = TheMapCache->findMap(game->getMap());
  if (mapData == nullptr) {
    fail("selected map was not found");
    return;
  }
  if (mapData->m_numPlayers < s_expectedPlayers) {
    fail("selected map has fewer slots than -autoNetworkHost requires");
    return;
  }

  Int humanPlayers = 0;
  for (Int i = 0; i < MAX_SLOTS; ++i) {
    LANGameSlot *slot = game->getLANSlot(i);
    if (slot != nullptr && slot->isHuman())
      ++humanPlayers;
  }

  if (humanPlayers != s_expectedPlayers)
    return;

  game->getLANSlot(0)->setAccept();
  for (Int i = 0; i < MAX_SLOTS; ++i) {
    LANGameSlot *slot = game->getLANSlot(i);
    if (slot != nullptr && slot->isHuman() && !slot->isAccepted())
      return;
  }

  const UnsignedInt now = timeGetTime();
  if (s_lastActionTime == 0 ||
      now - s_lastActionTime >= ACTION_RETRY_MILLISECONDS) {
    DEBUG_LOG(("NetworkAutoStart starting Direct Connect game with %d players",
               humanPlayers));
    s_lastActionTime = now;
    s_startRequested = true;
    StartPressed();
  }
}

void NetworkAutoStart::onGameCreate(LANAPIInterface::ReturnType result) {
  if (!isEnabled())
    return;

  if (result == LANAPIInterface::RET_OK)
    return;

  s_actionPending = false;
  fail("could not create Direct Connect game");
}

void NetworkAutoStart::onGameJoin(LANAPIInterface::ReturnType result) {
  if (!isEnabled())
    return;

  if (result == LANAPIInterface::RET_OK)
    return;

  s_actionPending = false;
  if (result == LANAPIInterface::RET_TIMEOUT ||
      result == LANAPIInterface::RET_GAME_GONE) {
    DEBUG_LOG(
        ("NetworkAutoStart will retry Direct Connect join after result %d",
         result));
    return;
  }

  fail("Direct Connect join was rejected");
}

void NetworkAutoStart::onGameStart() {
  if (!isEnabled())
    return;

  s_gameStarted = true;
  DEBUG_LOG(("NetworkAutoStart entered the network game"));
  printf("NetworkAutoStart entered the network game\n");
}

#endif
