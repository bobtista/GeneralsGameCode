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
// FILE: LanGameSetupActions.cpp /////////////////////////////////////////////////
// See LanGameSetupActions.h. The two host-local text notices StartPressed() used to
// write straight into listboxChatWindowLanGame (a per-player "doesn't have the map"
// line, and "notified players of your intent to start") are now sent through
// TheLAN->OnChat(SYSTEM) like every other validation message in this function --
// same visible text/color, but it now also reaches LanGameSetupSignals::chatLine instead of
// requiring a GameWindow.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/LanGameSetupActions.h"

#include "Common/GlobalData.h"
#include "Common/Money.h"
#include "Common/MultiplayerSettings.h"
#include "Common/PlayerTemplate.h"
#include "Common/QuotedPrintable.h"
#include "GameClient/GameText.h"
#include "GameClient/MapUtil.h"
#include "GameNetwork/GUIUtil.h"
#include "GameNetwork/LANAPICallbacks.h"

#include <set>

Int LanGameSetupActions::getNextSelectablePlayer( LANGameInfo *game, Int start )
{
	if( !game || !game->amIHost() )
		return -1;

	for( Int j = start; j < MAX_SLOTS; ++j )
	{
		LANGameSlot *slot = game->getLANSlot( j );
		if( slot && slot->getStartPos() == -1 &&
			( ( j == game->getLocalSlotNum() && game->getConstSlot( j )->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER )
			|| slot->isAI() ) )
		{
			return j;
		}
	}
	return -1;
}

Int LanGameSetupActions::getFirstSelectablePlayer( const LANGameInfo *game )
{
	const GameSlot *slot = game->getConstSlot( game->getLocalSlotNum() );
	if( !game->amIHost() || ( slot && slot->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER ) )
		return game->getLocalSlotNum();

	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		slot = game->getConstSlot( i );
		if( slot && slot->isAI() )
			return i;
	}

	return game->getLocalSlotNum();
}

Bool LanGameSetupActions::selectPlayerState( LANGameInfo *game, Int slotIndex, SlotState state, Bool *outIsAIChanged )
{
	if( outIsAIChanged )
		*outIsAIChanged = FALSE;

	if( !game || !game->amIHost() || slotIndex == game->getLocalSlotNum() )
		return FALSE;

	if( state == SLOT_PLAYER || state < 0 )
		return FALSE;

	LANGameSlot *slot = game->getLANSlot( slotIndex );
	if( !slot )
		return FALSE;

	if( slot->getState() == SLOT_PLAYER )
	{
		UnicodeString name = game->getPlayerName( slotIndex );
		slot->setState( state );
		game->resetAccepted();
		TheLAN->OnPlayerLeave( name ); // triggers its own game-options resync
		return TRUE;
	}
	else if( slot->getState() != state )
	{
		Bool wasAI = slot->isAI();
		slot->setState( state );
		Bool isAI = slot->isAI();
		if( wasAI || isAI )
			game->resetAccepted();
		if( outIsAIChanged )
			*outIsAIChanged = ( wasAI != isAI );

		TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
		lanUpdateSlotList();
		return TRUE;
	}
	return FALSE;
}

Bool LanGameSetupActions::selectColor( LANGameInfo *game, Int slotIndex, Int color, Bool isIniting )
{
	if( !game )
		return FALSE;

	LANGameSlot *slot = game->getLANSlot( slotIndex );
	if( !slot || color == slot->getColor() )
		return FALSE;

	if( color < -1 || color >= TheMultiplayerSettings->getNumColors() )
		return FALSE;

	if( color != -1 )
	{
		for( Int i = 0; i < MAX_SLOTS; ++i )
		{
			LANGameSlot *checkSlot = game->getLANSlot( i );
			if( checkSlot != slot && color == checkSlot->getColor() )
				return FALSE;
		}
	}

	slot->setColor( color );

	if( game->amIHost() )
	{
		if( !isIniting )
		{
			TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
			lanUpdateSlotList();
		}
	}
	else
	{
		if( !slot->isLocalPlayer() || !AreSlotListUpdatesEnabled() )
			return FALSE;

		AsciiString options;
		options.format( "Color=%d", color );
		TheLAN->RequestGameOptions( options, true );
	}
	return TRUE;
}

Bool LanGameSetupActions::selectPlayerTemplate( LANGameInfo *game, Int slotIndex, Int playerTemplate, Bool isIniting, Bool *outObserverChanged )
{
	if( outObserverChanged )
		*outObserverChanged = FALSE;

	if( !game )
		return FALSE;

	LANGameSlot *slot = game->getLANSlot( slotIndex );
	if( !slot || playerTemplate == slot->getPlayerTemplate() )
		return FALSE;

	Int oldTemplate = slot->getPlayerTemplate();
	slot->setPlayerTemplate( playerTemplate );

	if( oldTemplate == PLAYERTEMPLATE_OBSERVER || playerTemplate == PLAYERTEMPLATE_OBSERVER )
	{
		// Becoming, or ceasing to be, an observer: reset color/team to random, same as the
		// .wnd path's GadgetComboBoxSetSelectedPos(comboBoxColor[index]/comboBoxTeam[index], 0).
		slot->setColor( -1 );
		slot->setTeamNumber( -1 );
		slot->setStartPos( -1 );
		if( outObserverChanged )
			*outObserverChanged = TRUE;
	}

	game->resetAccepted();

	if( game->amIHost() )
	{
		if( !isIniting )
		{
			TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
			lanUpdateSlotList();
		}
	}
	else if( AreSlotListUpdatesEnabled() )
	{
		AsciiString options;
		options.format( "PlayerTemplate=%d", playerTemplate );
		TheLAN->RequestGameOptions( options, true );
	}
	return TRUE;
}

Bool LanGameSetupActions::selectTeam( LANGameInfo *game, Int slotIndex, Int team, Bool isIniting )
{
	if( !game )
		return FALSE;

	LANGameSlot *slot = game->getLANSlot( slotIndex );
	if( !slot || team == slot->getTeamNumber() )
		return FALSE;

	slot->setTeamNumber( team );
	game->resetAccepted();

	if( game->amIHost() )
	{
		if( !isIniting )
		{
			TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
			lanUpdateSlotList();
		}
	}
	else if( AreSlotListUpdatesEnabled() )
	{
		AsciiString options;
		options.format( "Team=%d", team );
		TheLAN->RequestGameOptions( options, true );
	}
	return TRUE;
}

Bool LanGameSetupActions::selectStartPosition( LANGameInfo *game, Int slotIndex, Int position, Bool isIniting )
{
	if( !game )
		return FALSE;

	LANGameSlot *slot = game->getLANSlot( slotIndex );
	if( !slot || position == slot->getStartPos() )
		return FALSE;

	if( position >= 0 )
	{
		for( Int i = 0; i < MAX_SLOTS; ++i )
		{
			if( i != slotIndex && game->getSlot( i )->getStartPos() == position )
				return FALSE;
		}
	}
	slot->setStartPos( position );

	if( game->amIHost() )
	{
		if( !isIniting )
		{
			game->resetAccepted();
			TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
			lanUpdateSlotList();
		}
	}
	else if( AreSlotListUpdatesEnabled() )
	{
		AsciiString options;
		options.format( "StartPos=%d", slot->getStartPos() );
		TheLAN->RequestGameOptions( options, true );
	}
	return TRUE;
}

void LanGameSetupActions::handleStartPositionMarkerClick( LANGameInfo *game, Int position )
{
	if( !game )
		return;

	Int playerIdxInPos = -1;
	for( Int j = 0; j < MAX_SLOTS; ++j )
	{
		LANGameSlot *slot = game->getLANSlot( j );
		if( slot && slot->getStartPos() == position )
		{
			playerIdxInPos = j;
			break;
		}
	}

	if( playerIdxInPos >= 0 )
	{
		LANGameSlot *slot = game->getLANSlot( playerIdxInPos );
		if( playerIdxInPos == game->getLocalSlotNum() || ( game->amIHost() && slot && slot->isAI() ) )
		{
			Int nextPlayer = getNextSelectablePlayer( game, playerIdxInPos + 1 );
			selectStartPosition( game, playerIdxInPos, -1, FALSE );
			if( nextPlayer >= 0 )
				selectStartPosition( game, nextPlayer, position, FALSE );
		}
	}
	else
	{
		Int nextPlayer = getNextSelectablePlayer( game, 0 );
		if( nextPlayer < 0 )
			nextPlayer = getFirstSelectablePlayer( game );
		selectStartPosition( game, nextPlayer, position, FALSE );
	}
}

void LanGameSetupActions::handleStartPositionMarkerRightClick( LANGameInfo *game, Int position )
{
	if( !game )
		return;

	Int playerIdxInPos = -1;
	for( Int j = 0; j < MAX_SLOTS; ++j )
	{
		LANGameSlot *slot = game->getLANSlot( j );
		if( slot && slot->getStartPos() == position )
		{
			playerIdxInPos = j;
			break;
		}
	}

	if( playerIdxInPos >= 0 )
	{
		LANGameSlot *slot = game->getLANSlot( playerIdxInPos );
		if( playerIdxInPos == game->getLocalSlotNum() || ( game->amIHost() && slot && slot->isAI() ) )
			selectStartPosition( game, playerIdxInPos, -1, FALSE );
	}
}

void LanGameSetupActions::setStartingCash( LANGameInfo *game, const Money &startingCash, Bool isIniting )
{
	if( !game )
		return;

	game->setStartingCash( startingCash );
	game->resetAccepted();

	if( game->amIHost() && !isIniting )
	{
		TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
		lanUpdateSlotList();
	}
}

void LanGameSetupActions::setSuperweaponRestriction( LANGameInfo *game, Bool restricted, Bool isIniting )
{
	if( !game )
		return;

	game->setSuperweaponRestriction( restricted ? 1 : 0 );
	game->resetAccepted();

	if( game->amIHost() && !isIniting )
	{
		TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
		lanUpdateSlotList();
	}
}

LanGameSetupActions::StartValidationResult LanGameSetupActions::validateStart( LANGameInfo *game )
{
	if( !game )
		return STARTVALIDATION_WAITING_ON_ACCEPTS;

	game->getLANSlot( 0 )->setAccept(); // cause we are, of course!

	Int numUsers = 0, numHumans = 0;
	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		GameSlot *slot = game->getSlot( i );
		if( slot && slot->isOccupied() && slot->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER )
		{
			if( slot->isHuman() )
				numHumans++;
			numUsers++;
		}
	}

	const MapMetaData *md = TheMapCache->findMap( game->getMap() );
	if( !md || md->m_numPlayers < numUsers )
	{
		if( game->amIHost() )
		{
			UnicodeString text;
			text.format( TheGameText->fetch( "LAN:TooManyPlayers" ), ( md ) ? md->m_numPlayers : 0 );
			TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), text, LANAPI::LANCHAT_SYSTEM );
		}
		return STARTVALIDATION_TOO_MANY_PLAYERS;
	}

	if( TheGlobalData->m_netMinPlayers && !numHumans )
	{
		if( game->amIHost() )
		{
			UnicodeString text = TheGameText->fetch( "GUI:NeedHumanPlayers" );
			TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), text, LANAPI::LANCHAT_SYSTEM );
		}
		return STARTVALIDATION_NEED_HUMAN_PLAYERS;
	}

	if( numUsers < TheGlobalData->m_netMinPlayers )
	{
		if( game->amIHost() )
		{
			UnicodeString text;
			text.format( TheGameText->fetch( "LAN:NeedMorePlayers" ), numUsers );
			TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), text, LANAPI::LANCHAT_SYSTEM );
		}
		return STARTVALIDATION_NEED_MORE_PLAYERS;
	}

	Int numRandom = 0;
	std::set<Int> teams;
	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		GameSlot *slot = game->getSlot( i );
		if( slot && slot->isOccupied() && slot->getPlayerTemplate() != PLAYERTEMPLATE_OBSERVER )
		{
			if( slot->getTeamNumber() >= 0 )
				teams.insert( slot->getTeamNumber() );
			else
				++numRandom;
		}
	}
	if( numRandom + (Int)teams.size() < TheGlobalData->m_netMinPlayers )
	{
		if( game->amIHost() )
		{
			UnicodeString text;
			text.format( TheGameText->fetch( "LAN:NeedMoreTeams" ) );
			TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), text, LANAPI::LANCHAT_SYSTEM );
		}
		return STARTVALIDATION_NEED_MORE_TEAMS;
	}

	if( numRandom + (Int)teams.size() < 2 )
	{
		UnicodeString text;
		text.format( TheGameText->fetch( "GUI:SandboxMode" ) );
		TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), text, LANAPI::LANCHAT_SYSTEM );
	}

	Bool isReady = true;
	Bool allHaveMap = true;
	const MapMetaData *mapData = TheMapCache->findMap( game->getMap() );
	UnicodeString mapDisplayName;
	Bool willTransfer = TRUE;
	if( mapData )
	{
		mapDisplayName.format( L"%ls", mapData->m_displayName.str() );
		willTransfer = !mapData->m_isOfficial;
	}
	else
	{
		mapDisplayName.translate( game->getMap().str() );
		willTransfer = WouldMapTransfer( game->getMap() );
	}

	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		LANGameSlot *slot = game->getLANSlot( i );
		if( slot->isHuman() && !slot->isAccepted() )
		{
			isReady = false;
			if( !willTransfer && !slot->hasMap() )
			{
				UnicodeString msg;
				msg.format( TheGameText->fetch( "GUI:PlayerNoMap" ), slot->getName().str(), mapDisplayName.str() );
				TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), msg, LANAPI::LANCHAT_SYSTEM );
				allHaveMap = false;
			}
		}
	}

	if( isReady )
		return STARTVALIDATION_READY;

	if( allHaveMap )
	{
		TheLAN->OnChat( L"SYSTEM", TheLAN->GetLocalIP(), TheGameText->fetch( "GUI:NotifiedStartIntent" ), LANAPI::LANCHAT_SYSTEM );
		TheLAN->RequestAccept();
	}
	return STARTVALIDATION_WAITING_ON_ACCEPTS;
}

void LanGameSetupActions::startGame( LANGameInfo *game )
{
	if( !game )
		return;

	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		LANGameSlot *slot = game->getLANSlot( i );
		if( slot && slot->isOpen() )
			slot->setState( SLOT_CLOSED );
	}

	Int seconds = TheMultiplayerSettings->getStartCountdownTimerSeconds();
	if( seconds )
		TheLAN->RequestGameStartTimer( seconds );
	else
		TheLAN->RequestGameStart();
}

void LanGameSetupActions::requestAccept( LANGameInfo *game )
{
	TheLAN->RequestAccept();
}

void LanGameSetupActions::leaveGame()
{
	TheLAN->RequestGameLeave();
}

void LanGameSetupActions::applySelectedMap( LANGameInfo *game, const AsciiString &mapName )
{
	if( !game )
		return;

	game->setMap( mapName );

	AsciiString lowerMap = mapName;
	lowerMap.toLower();
	const MapMetaData *md = TheMapCache ? TheMapCache->findMap( lowerMap ) : nullptr;
	if( md )
	{
		game->getSlot( 0 )->setMapAvailability( true );
		game->setMapCRC( md->m_CRC );
		game->setMapSize( md->m_filesize );

		game->resetStartSpots();
		game->adjustSlotsForMap();
	}

	game->resetAccepted();
	for( Int i = 0; i < MAX_SLOTS; ++i )
		game->getSlot( i )->setStartPos( -1 );

	updateGameOptions();
	lanUpdateSlotList();

	TheLAN->RequestGameOptions( GenerateGameOptionsString(), true );
}
