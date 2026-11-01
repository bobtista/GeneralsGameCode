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
// FILE: InGamePopupMessageActions.cpp ///////////////////////////////////////
// See InGamePopupMessageActions.h. Bodies moved out of InGamePopupMessage.cpp; no behavior change.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/InGamePopupMessageActions.h"

#include "Common/MessageStream.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/GUI/GUICallbacks/Menus/InGamePopupMessageData.h"

namespace InGamePopupMessageActions
{

static InGamePopupMessageData &data()
{
	return InGamePopupMessageData::instance();
}

Bool open( GameWindow *window )
{
	PopupMessageData *pMData = TheInGameUI->getPopupMessageData();

	if(!pMData)
	{
		DEBUG_ASSERTCRASH(pMData, ("We're in InGamePopupMessage without a pointer to pMData") );
		///< @todo: add a call to the close this bitch method when I implement it CLH
		return FALSE;
	}

	data().m_message = pMData->message;
	data().m_x = pMData->x;
	data().m_y = pMData->y;
	data().m_width = pMData->width;
	data().m_textColor = pMData->textColor;
	data().m_pause = pMData->pause;

	if(pMData->pause)
		TheWindowManager->winSetModal( window );

	return TRUE;
}

void ok()
{
	if(!data().m_pause)
		TheMessageStream->appendMessage( GameMessage::MSG_CLEAR_INGAME_POPUP_MESSAGE );
	else
		TheInGameUI->clearPopupMessageData();
}

Bool key( UnsignedByte key, UnsignedByte state )
{
	if( key != KEY_ENTER && key != KEY_ESC )
		return FALSE;

	if( BitIsSet( state, KEY_STATE_UP ) )
		ok();

	return TRUE;
}

}
