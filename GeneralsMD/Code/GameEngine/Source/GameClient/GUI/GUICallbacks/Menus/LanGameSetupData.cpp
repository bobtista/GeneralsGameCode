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
// FILE: LanGameSetupData.cpp ///////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/LanGameSetupData.h"

LanGameSetupData LanGameSetupData::build( LANGameInfo *game, Bool startButtonEnabled )
{
	LanGameSetupData data;

	if( !game )
		return data;

	GameSetupData setup = GameSetupData::build( game, TRUE );
	data.m_options = setup.m_options;
	data.m_isHost = game->amIHost();
	data.m_startButtonEnabled = startButtonEnabled;

	data.m_slots.resize( MAX_SLOTS );
	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		LanGameSetupSlotRow &row = data.m_slots[i];
		row.m_base = setup.m_slots[i];

		const GameSlot *slot = game->getConstSlot( i );
		if( !slot )
			continue;

		row.m_accepted = slot->isAccepted();
		row.m_hasMap = slot->hasMap();
	}

	return data;
}
