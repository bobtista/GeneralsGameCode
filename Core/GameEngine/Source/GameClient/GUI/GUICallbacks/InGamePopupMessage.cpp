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

// FILE: InGamePopupMessage.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Jul 2002
//
//	Filename: 	InGamePopupMessage.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	Init, input, and system for the in game message popup
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/version.h"
#include "Common/MessageStream.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GUI/GUICallbacks/Menus/InGamePopupMessageActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/InGamePopupMessageData.h"

//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

static NameKeyType parentID = NAMEKEY_INVALID;
static NameKeyType staticTextMessageID = NAMEKEY_INVALID;
static NameKeyType buttonOkID = NAMEKEY_INVALID;


static GameWindow *parent = nullptr;
static GameWindow *staticTextMessage = nullptr;
static GameWindow *buttonOk = nullptr;

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** Initialize the InGamePopupMessageInit menu */
//-------------------------------------------------------------------------------------------------
void InGamePopupMessageInit( WindowLayout *layout, void *userData )
{

	parentID = TheNameKeyGenerator->nameToKey("InGamePopupMessage.wnd:InGamePopupMessageParent");
	parent = TheWindowManager->winGetWindowFromId(nullptr, parentID);

	staticTextMessageID = TheNameKeyGenerator->nameToKey("InGamePopupMessage.wnd:StaticTextMessage");
	staticTextMessage = TheWindowManager->winGetWindowFromId(parent, staticTextMessageID);
	buttonOkID = TheNameKeyGenerator->nameToKey("InGamePopupMessage.wnd:ButtonOk");
	buttonOk = TheWindowManager->winGetWindowFromId(parent, buttonOkID);

	if(!InGamePopupMessageActions::open(parent))
		return;

	const InGamePopupMessageData &data = InGamePopupMessageData::instance();

	DisplayString *tempString = TheDisplayStringManager->newDisplayString();
	tempString->setText(data.m_message);
	tempString->setFont(staticTextMessage->winGetFont());
	tempString->setWordWrap(data.m_width - 14);
	Int width, height;
	tempString->getSize(&width, &height);
	TheDisplayStringManager->freeDisplayString(tempString);

	GadgetStaticTextSetText(staticTextMessage, data.m_message);
	// set the positions/sizes
	Int widthOk, heightOk;
	buttonOk->winGetSize(&widthOk, &heightOk);
	parent->winSetPosition( data.m_x, data.m_y);
	parent->winSetSize( data.m_width, height + 7 + 2 + 2 + heightOk + 2 );
	staticTextMessage->winSetPosition(  2,  2);
	staticTextMessage->winSetSize( data.m_width - 4, height + 7);
	buttonOk->winSetPosition(data.m_width - widthOk - 2, height + 7 + 2 + 2);
	staticTextMessage->winSetEnabledTextColors(data.m_textColor, 0);

	TheWindowManager->winSetFocus( parent );

	parent->winHide(FALSE);
	parent->winBringToTop();
}

//-------------------------------------------------------------------------------------------------
/** InGamePopupMessageInput callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType InGamePopupMessageInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{

		switch( msg )
		{

			// --------------------------------------------------------------------------------------------
			case GWM_CHAR:
			{
				UnsignedByte key = mData1;
				UnsignedByte state = mData2;
	//			if (buttonPushed)
	//				break;

				// Enter and Escape are the OK button; don't let them fall through anywhere else
				if( InGamePopupMessageActions::key( key, state ) )
					return MSG_HANDLED;

			}

		}
		return MSG_IGNORED;


}

//-------------------------------------------------------------------------------------------------
/** InGamePopupMessageSystem callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType InGamePopupMessageSystem( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{

  switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CREATE:
		{

			break;

		}
    //---------------------------------------------------------------------------------------------
		case GWM_DESTROY:
		{

			break;

		}

    //----------------------------------------------------------------------------------------------
    case GWM_INPUT_FOCUS:
		{

			// if we're givin the opportunity to take the keyboard focus we must say we want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = TRUE;

			break;

		}
    //---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

      if( controlID == buttonOkID )
			{
				InGamePopupMessageActions::ok();
			}
			break;
		}
		default:
			return MSG_IGNORED;

	}


	return MSG_HANDLED;

}

//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
