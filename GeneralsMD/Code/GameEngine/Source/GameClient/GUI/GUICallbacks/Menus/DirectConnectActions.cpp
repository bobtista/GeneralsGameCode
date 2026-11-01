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
// FILE: DirectConnectActions.cpp ////////////////////////////////////////////////
// See DirectConnectActions.h.

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/DirectConnectActions.h"

#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "Common/QuotedPrintable.h"
#include "Common/UnicodeString.h"
#include "Common/UserPreferences.h"
#include "GameClient/GameText.h"
#include "GameClient/Shell.h"
#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/LANAPI.h"
#include "GameNetwork/LANAPICallbacks.h"

UnicodeString DirectConnectActions::enterDirectConnect()
{
	if ( TheLAN == nullptr )
	{
		TheLAN = NEW LANAPI();
		TheLAN->init();
	}
	TheLAN->reset();

	TheShell->showShellMap( TRUE );

	LANPreferences userprefs;
	UnicodeString name = userprefs.getUserName();
	if ( name.isEmpty() )
		name = TheGameText->fetch( "GUI:Player" );

	// Same delete+recreate dance NetworkDirectConnectInit() has always done to resolve the local IP.
	delete TheLAN;
	TheLAN = nullptr;

	if ( TheLAN == nullptr )
	{
		TheLAN = NEW LANAPI();

		OptionPreferences prefs;
		UnsignedInt IP = prefs.getOnlineIPAddress();

		IPEnumeration IPs;
		EnumeratedIP *IPlist = IPs.getAddresses();
		DEBUG_ASSERTCRASH( IPlist, ("No IP addresses found!") );

		Bool foundIP = FALSE;
		EnumeratedIP *tempIP = IPlist;
		while ( (tempIP != nullptr) && (foundIP == FALSE) )
		{
			if ( IP == tempIP->getIP() )
				foundIP = TRUE;
			tempIP = tempIP->getNext();
		}

		if ( foundIP == FALSE && IPlist )
			IP = IPlist->getIP();

		TheLAN->init();
		TheLAN->SetLocalIP( IP );
	}

	TheLAN->RequestLobbyLeave( true );

	return name;
}

UnicodeString DirectConnectActions::localIPString()
{
	if ( !TheLAN )
		return UnicodeString::TheEmptyString;

	UnsignedInt ip = TheLAN->GetLocalIP();
	UnicodeString ipstr;
	ipstr.format( L"%d.%d.%d.%d", PRINTF_IP_AS_4_INTS(ip) );
	return ipstr;
}

void DirectConnectActions::hostGame( const UnicodeString &playerName )
{
	DEBUG_ASSERTCRASH( TheLAN != nullptr, ("TheLAN is null!") );
	if ( !TheLAN )
		TheLAN = NEW LANAPI();

	UnsignedInt localIP = TheLAN->GetLocalIP();
	UnicodeString localIPStr;
	localIPStr.format( L"%d.%d.%d.%d", PRINTF_IP_AS_4_INTS(localIP) );

	UnicodeString name = playerName;

	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable( name );
	prefs.write();

	name.truncateTo( g_lanPlayerNameLength );
	TheLAN->RequestSetName( name );
	TheLAN->RequestGameCreate( localIPStr, TRUE );
}

void DirectConnectActions::joinGame( const UnicodeString &remoteIPEntry, const std::vector<UnicodeString> &comboEntries,
	int currentSelection, const UnicodeString &playerName )
{
	if ( !TheLAN )
		TheLAN = NEW LANAPI();

	UnsignedInt ipaddress = 0;
	AsciiString asciientry;
	asciientry.translate( remoteIPEntry );

	AsciiString ipstring;
	asciientry.nextToken( &ipstring, "(" );

	Int ip1, ip2, ip3, ip4;
	Int numFields = sscanf( ipstring.str(), "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4 );
	(void)numFields; DEBUG_ASSERTCRASH( numFields == 4, ("JoinDirectConnectGame - invalid IP address format: %s", ipstring.str()) );

	DEBUG_LOG( ("JoinDirectConnectGame - joining at %d.%d.%d.%d", ip1, ip2, ip3, ip4) );

	ipaddress = (ip1 << 24) + (ip2 << 16) + (ip3 << 8) + ip4;

	UnicodeString name = playerName;
	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable( name );
	prefs.write();

	updateRemoteIPList( comboEntries, currentSelection, remoteIPEntry );

	name.truncateTo( g_lanPlayerNameLength );
	TheLAN->RequestSetName( name );

	TheLAN->RequestGameJoinDirectConnect( ipaddress );
}

void DirectConnectActions::updateRemoteIPList( const std::vector<UnicodeString> &comboEntries, int currentSelection,
	const UnicodeString &currentText )
{
	Int n1[4], n2[4];
	LANPreferences prefs;
	Int numEntries = (Int)comboEntries.size();

	AsciiString sel;
	sel.translate( currentText );

	UnicodeString newEntry = currentText;
	UnicodeString newIP;
	newEntry.nextToken( &newIP, L":" );
	Int numFields = swscanf( newIP.str(), L"%d.%d.%d.%d", &(n1[0]), &(n1[1]), &(n1[2]), &(n1[3]) );

	if ( numFields != 4 )
	{
		// this is not a properly formatted IP, don't change a thing.
		return;
	}

	prefs["RemoteIP0"] = sel;

	Int currentINIEntry = 1;

	for ( Int i = 0; i < numEntries; ++i )
	{
		if ( i != currentSelection )
		{
			UnicodeString uni = comboEntries[i];
			AsciiString ascii;
			ascii.translate( uni );

			// prevent more than one copy of an IP address from being put in the list.
			if ( currentSelection == -1 )
			{
				UnicodeString oldEntry = uni;
				UnicodeString oldIP;
				oldEntry.nextToken( &oldIP, L":" );

				swscanf( oldIP.str(), L"%d.%d.%d.%d", &(n2[0]), &(n2[1]), &(n2[2]), &(n2[3]) );

				Bool isEqual = TRUE;
				for ( Int j = 0; (j < 4) && (isEqual == TRUE); ++j )
				{
					if ( n1[j] != n2[j] )
						isEqual = FALSE;
				}
				// check to see if this is a duplicate or if this is not a properly formatted IP address.
				if ( isEqual == TRUE )
				{
					--numEntries;
					continue;
				}
			}
			AsciiString temp;
			temp.format( "RemoteIP%d", currentINIEntry );
			++currentINIEntry;
			prefs[temp.str()] = ascii;
		}
	}

	if ( currentSelection == -1 )
	{
		++numEntries;
	}

	AsciiString numRemoteIPs;
	numRemoteIPs.format( "%d", numEntries );

	prefs["NumRemoteIPs"] = numRemoteIPs;

	prefs.write();
}

void DirectConnectActions::commitPlayerName( const UnicodeString &playerName )
{
	UnicodeString name = playerName;

	LANPreferences prefs;
	prefs["UserName"] = UnicodeStringToQuotedPrintable( name );
	prefs.write();

	name.truncateTo( g_lanPlayerNameLength );
	TheLAN->RequestSetName( name );
}
