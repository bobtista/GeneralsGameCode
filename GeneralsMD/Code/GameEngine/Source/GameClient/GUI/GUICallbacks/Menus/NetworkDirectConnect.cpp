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

///////////////////////////////////////////////////////////////////////////////////////
// FILE: NetworkDirectConnect.cpp
// Author: Bryan Cleveland, November 2001
// Description: Lan Lobby Menu
///////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "gamespy/peer/peer.h"

#include "GameClient/AnimateWindowManager.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/Shell.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/GUI/GUICallbacks/Menus/DirectConnectActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/DirectConnectData.h"

#include "GameNetwork/LANAPI.h"
#include "GameNetwork/LANAPICallbacks.h"

#include <vector>


// window ids ------------------------------------------------------------------------------

// Window Pointers ------------------------------------------------------------------------

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////

extern Bool LANbuttonPushed;
extern Bool LANisShuttingDown;

static Bool isShuttingDown = false;
static Bool buttonPushed = false;

static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType buttonHostID = NAMEKEY_INVALID;
static NameKeyType buttonJoinID = NAMEKEY_INVALID;
static NameKeyType editPlayerNameID = NAMEKEY_INVALID;
static NameKeyType comboboxRemoteIPID = NAMEKEY_INVALID;
static NameKeyType staticLocalIPID = NAMEKEY_INVALID;

static GameWindow *buttonBack = nullptr;
static GameWindow *buttonHost = nullptr;
static GameWindow *buttonJoin = nullptr;
static GameWindow *editPlayerName = nullptr;
static GameWindow *comboboxRemoteIP = nullptr;
static GameWindow *staticLocalIP = nullptr;

void PopulateRemoteIPComboBox()
{
	GadgetComboBoxReset(comboboxRemoteIP);

	std::vector<UnicodeString> history = DirectConnectData::loadRemoteIPHistory();
	Color white = GameMakeColor(255,255,255,255);

	for (const UnicodeString &entry : history)
		GadgetComboBoxAddEntry(comboboxRemoteIP, entry, white);

	if (!history.empty())
	{
		GadgetComboBoxSetSelectedPos(comboboxRemoteIP, 0, TRUE);
	}
}

// Enumerates the combobox's own entries (in display order), same SetSelectedPos(i, FALSE)+GetText()
// dance the original UpdateRemoteIPList() used, so DirectConnectActions::updateRemoteIPList() can do
// the prefs bookkeeping without any GadgetComboBox coupling.
static std::vector<UnicodeString> currentRemoteIPComboEntries()
{
	std::vector<UnicodeString> entries;
	Int numEntries = GadgetComboBoxGetLength(comboboxRemoteIP);

	// Same SetSelectedPos(i, FALSE) dance the original loop used to read each entry's text; like the
	// original, this leaves the widget's selection at the last enumerated position -- harmless since
	// PopulateRemoteIPComboBox() always runs right after and resets/rebuilds the whole combobox.
	for (Int i = 0; i < numEntries; ++i)
	{
		GadgetComboBoxSetSelectedPos(comboboxRemoteIP, i, FALSE);
		entries.push_back(GadgetComboBoxGetText(comboboxRemoteIP));
	}

	return entries;
}

void HostDirectConnectGame()
{
	UnicodeString name = GadgetTextEntryGetText(editPlayerName);
	DirectConnectActions::hostGame(name);
}

void JoinDirectConnectGame()
{
	Int currentSelection = -1;
	GadgetComboBoxGetSelectedPos(comboboxRemoteIP, &currentSelection);
	UnicodeString ipunistring = GadgetComboBoxGetText(comboboxRemoteIP);
	std::vector<UnicodeString> comboEntries = currentRemoteIPComboEntries();

	UnicodeString name = GadgetTextEntryGetText(editPlayerName);

	DirectConnectActions::joinGame(ipunistring, comboEntries, currentSelection, name);

	PopulateRemoteIPComboBox();
}

//-------------------------------------------------------------------------------------------------
/** Initialize the WOL Welcome Menu */
//-------------------------------------------------------------------------------------------------
void NetworkDirectConnectInit( WindowLayout *layout, void *userData )
{
	LANbuttonPushed = false;
	LANisShuttingDown = false;

	buttonPushed = false;
	isShuttingDown = false;
	buttonBackID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ButtonBack" );
	buttonHostID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ButtonHost" );
	buttonJoinID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ButtonJoin" );
	editPlayerNameID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:EditPlayerName" );
	comboboxRemoteIPID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:ComboboxRemoteIP" );
	staticLocalIPID = TheNameKeyGenerator->nameToKey( "NetworkDirectConnect.wnd:StaticLocalIP" );

	buttonBack = TheWindowManager->winGetWindowFromId( nullptr,  buttonBackID);
	buttonHost = TheWindowManager->winGetWindowFromId( nullptr,	buttonHostID);
	buttonJoin = TheWindowManager->winGetWindowFromId( nullptr,	buttonJoinID);
	editPlayerName = TheWindowManager->winGetWindowFromId( nullptr,	editPlayerNameID);
	comboboxRemoteIP = TheWindowManager->winGetWindowFromId( nullptr,	comboboxRemoteIPID);
	staticLocalIP = TheWindowManager->winGetWindowFromId( nullptr, staticLocalIPID);

//	// animate controls
//	TheShell->registerWithAnimateManager(buttonBack, WIN_ANIMATION_SLIDE_LEFT, TRUE, 800);
//	TheShell->registerWithAnimateManager(buttonHost, WIN_ANIMATION_SLIDE_LEFT, TRUE, 600);
//	TheShell->registerWithAnimateManager(buttonJoin, WIN_ANIMATION_SLIDE_LEFT, TRUE, 200);
//

	// TheLAN create/reset/local-IP-resolve dance, see DirectConnectActions::enterDirectConnect().
	UnicodeString name = DirectConnectActions::enterDirectConnect();

	GadgetTextEntrySetText(editPlayerName, name);

	PopulateRemoteIPComboBox();

	GadgetStaticTextSetText(staticLocalIP, DirectConnectActions::localIPString());

	layout->hide(FALSE);
	layout->bringForward();
	TheTransitionHandler->setGroup("NetworkDirectConnectFade");


}

//-------------------------------------------------------------------------------------------------
/** This is called when a shutdown is complete for this menu */
//-------------------------------------------------------------------------------------------------
static void shutdownComplete( WindowLayout *layout )
{

	isShuttingDown = false;

	// hide the layout
	layout->hide( TRUE );

	// our shutdown is complete
	TheShell->shutdownComplete( layout );

}

//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu shutdown method */
//-------------------------------------------------------------------------------------------------
void NetworkDirectConnectShutdown( WindowLayout *layout, void *userData )
{
	isShuttingDown = true;

	// if we are shutting down for an immediate pop, skip the animations
	Bool popImmediate = *(Bool *)userData;
	if( popImmediate )
	{

		shutdownComplete( layout );
		return;

	}

	TheShell->reverseAnimatewindow();

	TheTransitionHandler->reverse("NetworkDirectConnectFade");
}


//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu update method */
//-------------------------------------------------------------------------------------------------
void NetworkDirectConnectUpdate( WindowLayout * layout, void *userData)
{
	// We'll only be successful if we've requested to
	if(isShuttingDown && TheShell->isAnimFinished() && TheTransitionHandler->isFinished())
		shutdownComplete(layout);
}

//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu input callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType NetworkDirectConnectInput( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;
			if (buttonPushed)
				break;

			switch( key )
			{

				// ----------------------------------------------------------------------------------------
				case KEY_ESC:
				{

					//
					// send a simulated selected event to the parent window of the
					// back/exit button
					//
					if( BitIsSet( state, KEY_STATE_UP ) )
					{
						TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,
																							(WindowMsgData)buttonBack, buttonBackID );

					}

					// don't let key fall through anywhere else
					return MSG_HANDLED;

				}

			}

		}

	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
/** WOL Welcome Menu window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType NetworkDirectConnectSystem( GameWindow *window, UnsignedInt msg,
														 WindowMsgData mData1, WindowMsgData mData2 )
{
	UnicodeString txtInput;

	switch( msg )
	{


		case GWM_CREATE:
			{

				break;
			}

		case GWM_DESTROY:
			{
				break;
			}

		case GWM_INPUT_FOCUS:
			{
				// if we're givin the opportunity to take the keyboard focus we must say we want it
				if( mData1 == TRUE )
					*(Bool *)mData2 = TRUE;

				return MSG_HANDLED;
			}

		case GBM_SELECTED:
			{
				if (buttonPushed)
					break;

				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if ( controlID == buttonBackID )
				{
					UnicodeString name = GadgetTextEntryGetText(editPlayerName);
					DirectConnectActions::commitPlayerName(name);

					buttonPushed = true;
					LANbuttonPushed = true;
					TheShell->pop();
				}
				else if (controlID == buttonHostID)
				{
					HostDirectConnectGame();
				}
				else if (controlID == buttonJoinID)
				{
					JoinDirectConnectGame();
				}
				break;
			}

		case GEM_EDIT_DONE:
			{
				break;
			}
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;
}
