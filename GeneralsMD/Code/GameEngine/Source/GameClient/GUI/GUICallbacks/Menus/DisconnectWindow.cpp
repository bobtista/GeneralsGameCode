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

// FILE: DisconnectControls.cpp ///////////////////////////////////////////////////////////////////////
// Author: Bryan Cleveland - March 2001
// Desc: GUI menu for network disconnects
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/GameWindow.h"
#include "GameClient/GameText.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GameClient.h"
#include "GameClient/DisconnectMenu.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/RmlUiScreenRegistry.h"
#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/DisconnectMenuData.h"
#include "Common/NameKeyGenerator.h"

// Private Data -----------------------------
static WindowLayout *disconnectMenuLayout;

static const Int SLOTS = DisconnectMenuData::PLAYER_SLOTS;

static NameKeyType textEntryID = NAMEKEY_INVALID;
static NameKeyType textDisplayID = NAMEKEY_INVALID;

static GameWindow *textEntryWindow = nullptr;
static GameWindow *textDisplayWindow = nullptr;

static NameKeyType buttonQuitID = NAMEKEY_INVALID;
static GameWindow *buttonQuitWindow = nullptr;

static NameKeyType buttonVotePlayerID[SLOTS] = { NAMEKEY_INVALID };

static GameWindow *buttonVotePlayerWindow[SLOTS] = { nullptr };
static GameWindow *playerNameWindow[SLOTS] = { nullptr };
static GameWindow *playerTimeoutWindow[SLOTS] = { nullptr };
static GameWindow *playerVotesWindow[SLOTS] = { nullptr };
static GameWindow *packetRouterTimeoutWindow = nullptr;
static GameWindow *packetRouterTimeoutLabelWindow = nullptr;

static UnsignedInt shownVersion = 0;
static UnsignedInt shownChatVersion = 0;

// Names of the per-player controls in the window, one per slot.
static const char *const playerNameControlNames[SLOTS] = {
	"DisconnectScreen.wnd:StaticPlayer1Name",
	"DisconnectScreen.wnd:StaticPlayer2Name",
	"DisconnectScreen.wnd:StaticPlayer3Name",
	"DisconnectScreen.wnd:StaticPlayer4Name",
	"DisconnectScreen.wnd:StaticPlayer5Name",
	"DisconnectScreen.wnd:StaticPlayer6Name",
	"DisconnectScreen.wnd:StaticPlayer7Name"
};

static const char *const playerTimeoutControlNames[SLOTS] = {
	"DisconnectScreen.wnd:StaticPlayer1Timeout",
	"DisconnectScreen.wnd:StaticPlayer2Timeout",
	"DisconnectScreen.wnd:StaticPlayer3Timeout",
	"DisconnectScreen.wnd:StaticPlayer4Timeout",
	"DisconnectScreen.wnd:StaticPlayer5Timeout",
	"DisconnectScreen.wnd:StaticPlayer6Timeout",
	"DisconnectScreen.wnd:StaticPlayer7Timeout"
};

static const char *const playerVoteButtonControlNames[SLOTS] = {
	"DisconnectScreen.wnd:ButtonKickPlayer1",
	"DisconnectScreen.wnd:ButtonKickPlayer2",
	"DisconnectScreen.wnd:ButtonKickPlayer3",
	"DisconnectScreen.wnd:ButtonKickPlayer4",
	"DisconnectScreen.wnd:ButtonKickPlayer5",
	"DisconnectScreen.wnd:ButtonKickPlayer6",
	"DisconnectScreen.wnd:ButtonKickPlayer7"
};

static const char *const playerVoteCountControlNames[SLOTS] = {
	"DisconnectScreen.wnd:StaticPlayer1Votes",
	"DisconnectScreen.wnd:StaticPlayer2Votes",
	"DisconnectScreen.wnd:StaticPlayer3Votes",
	"DisconnectScreen.wnd:StaticPlayer4Votes",
	"DisconnectScreen.wnd:StaticPlayer5Votes",
	"DisconnectScreen.wnd:StaticPlayer6Votes",
	"DisconnectScreen.wnd:StaticPlayer7Votes"
};

static const char *const packetRouterTimeoutControlName = "DisconnectScreen.wnd:StaticPacketRouterTimeout";
static const char *const packetRouterTimeoutLabelControlName = "DisconnectScreen.wnd:StaticPacketRouterTimeoutLabel";

// The RmlUi screen replaces the .wnd, unless -wnd was given; its layout only holds a placeholder.
static Bool routedToRmlUi()
{
	return RmlUiScreenRegistry::routesToRmlUi( AsciiString( "Menus/DisconnectScreen.wnd" ) );
}

static GameWindow *findWindow( const char *name )
{
	return TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( name ) );
}

static void InitDisconnectWindow() {
	textEntryID = TheNameKeyGenerator->nameToKey( "DisconnectScreen.wnd:TextEntry");
	textDisplayID = TheNameKeyGenerator->nameToKey( "DisconnectScreen.wnd:ListboxTextDisplay");

	textEntryWindow = TheWindowManager->winGetWindowFromId(nullptr, textEntryID);
	textDisplayWindow = TheWindowManager->winGetWindowFromId(nullptr, textDisplayID);

	if (textEntryWindow != nullptr) {
		GadgetTextEntrySetText(textEntryWindow, UnicodeString::TheEmptyString);
		TheWindowManager->winSetFocus(textEntryWindow);
	}

	buttonQuitID = TheNameKeyGenerator->nameToKey( "DisconnectScreen.wnd:ButtonQuitGame");
	buttonQuitWindow = TheWindowManager->winGetWindowFromId(nullptr, buttonQuitID);

	for (Int i = 0; i < SLOTS; ++i) {
		buttonVotePlayerID[i] = TheNameKeyGenerator->nameToKey( playerVoteButtonControlNames[i] );
		buttonVotePlayerWindow[i] = TheWindowManager->winGetWindowFromId(nullptr, buttonVotePlayerID[i]);
		playerNameWindow[i] = findWindow( playerNameControlNames[i] );
		playerTimeoutWindow[i] = findWindow( playerTimeoutControlNames[i] );
		playerVotesWindow[i] = findWindow( playerVoteCountControlNames[i] );
	}

	packetRouterTimeoutWindow = findWindow( packetRouterTimeoutControlName );
	packetRouterTimeoutLabelWindow = findWindow( packetRouterTimeoutLabelControlName );

	shownVersion = 0;
	shownChatVersion = 0;
}

//------------------------------------------------------
/** Make the windows show the disconnect data. DisconnectMenu calls this after every change. */
//------------------------------------------------------
void SyncDisconnectWindow()
{
	// not created yet, or the RmlUi screen shows the data
	if( disconnectMenuLayout == nullptr || textDisplayWindow == nullptr )
		return;

	const DisconnectMenuData &data = DisconnectMenuData::instance();

	if( shownVersion != data.m_version )
	{
		shownVersion = data.m_version;

		for( Int i = 0; i < SLOTS; ++i )
		{
			const DisconnectMenuData::Player &player = data.m_players[i];

			if( !player.m_name.isEmpty() )
				GadgetStaticTextSetText( playerNameWindow[i], player.m_name );
			GadgetStaticTextSetText( playerTimeoutWindow[i], player.m_timeout );
			GadgetStaticTextSetText( playerVotesWindow[i], player.m_votes );

			playerNameWindow[i]->winHide( !player.m_visible );
			playerTimeoutWindow[i]->winHide( !player.m_visible );
			playerVotesWindow[i]->winHide( !player.m_visible );
			buttonVotePlayerWindow[i]->winHide( !player.m_visible );
			buttonVotePlayerWindow[i]->winEnable( player.m_voteEnabled );
		}

		packetRouterTimeoutLabelWindow->winHide( !data.m_routerVisible );
		GadgetStaticTextSetText( packetRouterTimeoutWindow, data.m_routerTimeout );
		packetRouterTimeoutWindow->winHide( !data.m_routerVisible );

		buttonQuitWindow->winEnable( data.m_quitEnabled );
	}

	if( shownChatVersion != data.m_chatVersion )
	{
		shownChatVersion = data.m_chatVersion;

		GadgetListBoxReset( textDisplayWindow );
		for( size_t i = 0; i < data.m_chat.size(); ++i )
		{
			const UnsignedInt rgb = data.m_chat[i].m_rgb;
			GadgetListBoxAddEntryText( textDisplayWindow, data.m_chat[i].m_text,
				GameMakeColor( (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 255 ), -1 );
		}
	}
}

//------------------------------------------------------
/** Show the Disconnect Screen */
//------------------------------------------------------
void ShowDisconnectWindow()
{

	// load the quit menu from the layout file if needed
	if( disconnectMenuLayout == nullptr )
	{

		// load layout from disk
		disconnectMenuLayout = TheWindowManager->winCreateLayout( "Menus/DisconnectScreen.wnd" );

		// init it
		if( !routedToRmlUi() )
			InitDisconnectWindow();

		// show it
		disconnectMenuLayout->hide( FALSE );

	}
	else
	{

		disconnectMenuLayout->hide( FALSE );

	}

	SyncDisconnectWindow();
	disconnectMenuLayout->bringForward();

}

//------------------------------------------------------
/** Hide the Disconnect Screen */
//------------------------------------------------------
void HideDisconnectWindow()
{

	// load the quit menu from the layout file if needed
	if( disconnectMenuLayout == nullptr )
	{

		// load layout from disk
		disconnectMenuLayout = TheWindowManager->winCreateLayout( "Menus/DisconnectScreen.wnd" );

		// init it
		if( !routedToRmlUi() )
			InitDisconnectWindow();

		// show it
		disconnectMenuLayout->hide( TRUE );

	}
	else
	{

		disconnectMenuLayout->hide( TRUE );

	}

}

//-------------------------------------------------------------------------------------------------
/** Input callback for the control bar parent */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType DisconnectControlInput( GameWindow *window, UnsignedInt msg,
																						WindowMsgData mData1, WindowMsgData mData2 )
{

	return MSG_IGNORED;

}

//-------------------------------------------------------------------------------------------------
/** System callback for the control bar parent */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType DisconnectControlSystem( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{

			GameWindow *control = (GameWindow *) mData1;
			Int controlID = control->winGetWindowId();

			if (controlID == buttonQuitID) {
				DisconnectMenuActions::quit();
			} else {
				for (Int i = 0; i < SLOTS; ++i) {
					if (controlID == buttonVotePlayerID[i]) {
						DisconnectMenuActions::vote(i);
						break;
					}
				}
			}
			SyncDisconnectWindow();

			break;

		}

		case GEM_EDIT_DONE:
		{
//			DEBUG_LOG(("DisconnectControlSystem - got GEM_EDIT_DONE."));
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

			// Take the user's input and echo it into the chat window as well as
			// send it to the other clients on the lan
			if ( controlID == textEntryID )
			{
				UnicodeString txtInput;

//				DEBUG_LOG(("DisconnectControlSystem - GEM_EDIT_DONE was from the text entry control."));

				// read the user's input
				txtInput.set(GadgetTextEntryGetText( textEntryWindow ));
				// Clear the text entry line
				GadgetTextEntrySetText(textEntryWindow, UnicodeString::TheEmptyString);
				DisconnectMenuActions::sendChat(txtInput);

			}
			break;
		}

		//---------------------------------------------------------------------------------------------
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}
