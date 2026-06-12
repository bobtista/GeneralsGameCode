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

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/networkutil.h"
#include "GameClient/ClientInstance.h"

#ifndef _WIN32
#include <ifaddrs.h>
#include <net/if.h>
#else
// TheSuperHackers @build bobtista 12/06/2026 INTERFACE_INFO / SIO_GET_INTERFACE_LIST (used by
// getSubnetBroadcastAddress) are declared in ws2ipdef.h, which winsock2.h does not include by default.
#include <ws2ipdef.h>
#endif

IPEnumeration::IPEnumeration()
{
	m_IPlist = nullptr;
	m_isWinsockInitialized = false;
}

IPEnumeration::~IPEnumeration()
{
	if (m_isWinsockInitialized)
	{
		WSACleanup();
		m_isWinsockInitialized = false;
	}

	EnumeratedIP *ip = m_IPlist;
	while (ip)
	{
		ip = ip->getNext();
		deleteInstance(m_IPlist);
		m_IPlist = ip;
	}
}

EnumeratedIP * IPEnumeration::getAddresses()
{
	if (m_IPlist)
		return m_IPlist;

	if (!m_isWinsockInitialized)
	{
		// TheSuperHackers @bugfix bobtista 09/06/2026 Only validate the Winsock
		// version on Windows. On other platforms WSAStartup is a no-op that never
		// fills wsadata, so reading wsadata.wVersion returns uninitialized memory
		// and this check spuriously fails, leaving the machine with no IP list.
#ifdef _WIN32
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return nullptr;
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return nullptr;
		}
#endif
		m_isWinsockInitialized = true;
	}

	// TheSuperHackers @feature Add one unique local host IP address for each multi client instance.
	if (rts::ClientInstance::isMultiInstance())
	{
		const UnsignedInt id = rts::ClientInstance::getInstanceId();
		addNewIP(
			127,
			(UnsignedByte)(id >> 16),
			(UnsignedByte)(id >> 8),
			(UnsignedByte)(id));
	}

#ifdef _WIN32
	// get the local machine's host name
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)))
	{
		DEBUG_LOG(("Failed call to gethostname; WSAGetLastError returned %d", WSAGetLastError()));
		return nullptr;
	}
	DEBUG_LOG(("Hostname is '%s'", hostname));

	// get host information from the host name
	HOSTENT* hostEnt = gethostbyname(hostname);
	if (hostEnt == nullptr)
	{
		DEBUG_LOG(("Failed call to gethostbyname; WSAGetLastError returned %d", WSAGetLastError()));
		return nullptr;
	}

	// sanity-check the length of the IP adress
	if (hostEnt->h_length != 4)
	{
		DEBUG_LOG(("gethostbyname returns oddly-sized IP addresses!"));
		return nullptr;
	}

	// construct a list of addresses
	int numAddresses = 0;
	char *entry;
	while ( (entry = hostEnt->h_addr_list[numAddresses++]) != nullptr )
	{
		addNewIP(
			(UnsignedByte)entry[0],
			(UnsignedByte)entry[1],
			(UnsignedByte)entry[2],
			(UnsignedByte)entry[3]);
	}
#else
	// TheSuperHackers @feature bobtista 09/06/2026 Enumerate every local IPv4 interface via
	// getifaddrs. gethostbyname(hostname) only returns the addresses the host name resolves to
	// on non-Windows, which omits VPN/tunnel adapters (Hamachi, ZeroTier, utun) that are needed
	// to host or join an internet "LAN" or direct-connect game. Loopback is skipped so it is
	// never offered as the local IP.
	struct ifaddrs *ifaddrList = nullptr;
	if (getifaddrs(&ifaddrList) == 0)
	{
		for (struct ifaddrs *ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
			{
				continue;
			}
			if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0)
			{
				continue;
			}
			const struct sockaddr_in *sin = (const struct sockaddr_in *)ifa->ifa_addr;
			const UnsignedInt addr = ntohl(sin->sin_addr.s_addr);
			addNewIP(
				(UnsignedByte)(addr >> 24),
				(UnsignedByte)(addr >> 16),
				(UnsignedByte)(addr >> 8),
				(UnsignedByte)(addr));
		}
		freeifaddrs(ifaddrList);
	}
#endif

	return m_IPlist;
}

// TheSuperHackers @bugfix bobtista 12/06/2026 The LAN protocol broadcasts discovery announces and
// JOIN_ACCEPT to the limited broadcast 255.255.255.255, which only egresses the single default-route
// interface. On a multi-homed host (a ZeroTier/VPN overlay adapter alongside Wi-Fi) those packets
// never reach peers on the overlay subnet, so machines can't see each other in the LAN lobby and a
// direct-connect joiner times out waiting for its (broadcast) accept even though the host has already
// added it. Sending to the subnet-directed broadcast of the selected local IP instead routes the
// packet out the interface that owns that subnet. Returns host byte order; falls back to the limited
// broadcast when the netmask can't be resolved (preserving the original behavior).
UnsignedInt IPEnumeration::getSubnetBroadcastAddress( UnsignedInt localIP )
{
	if (localIP == 0)
	{
		return INADDR_BROADCAST;
	}

#ifdef _WIN32
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock != INVALID_SOCKET)
	{
		INTERFACE_INFO ifList[32];
		DWORD bytesReturned = 0;
		if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, nullptr, 0, ifList, sizeof(ifList), &bytesReturned, nullptr, nullptr) == 0)
		{
			const int count = (int)(bytesReturned / sizeof(INTERFACE_INFO));
			for (int i = 0; i < count; ++i)
			{
				const UnsignedInt ifaceIP = ntohl(((struct sockaddr_in *)&ifList[i].iiAddress)->sin_addr.s_addr);
				if (ifaceIP == localIP)
				{
					const UnsignedInt mask = ntohl(((struct sockaddr_in *)&ifList[i].iiNetmask)->sin_addr.s_addr);
					closesocket(sock);
					return (localIP & mask) | (~mask);
				}
			}
		}
		closesocket(sock);
	}
#else
	struct ifaddrs *ifaddrList = nullptr;
	if (getifaddrs(&ifaddrList) == 0)
	{
		for (struct ifaddrs *ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET || ifa->ifa_netmask == nullptr)
			{
				continue;
			}
			const UnsignedInt ifaceIP = ntohl(((const struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr);
			if (ifaceIP == localIP)
			{
				const UnsignedInt mask = ntohl(((const struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr);
				freeifaddrs(ifaddrList);
				return (localIP & mask) | (~mask);
			}
		}
		freeifaddrs(ifaddrList);
	}
#endif

	return INADDR_BROADCAST;
}

void IPEnumeration::addNewIP( UnsignedByte a, UnsignedByte b, UnsignedByte c, UnsignedByte d )
{
	EnumeratedIP *newIP = newInstance(EnumeratedIP);

	AsciiString str;
	str.format("%d.%d.%d.%d", (int)a, (int)b, (int)c, (int)d);

	UnsignedInt ip = AssembleIp(a, b, c, d);

	newIP->setIPstring(str);
	newIP->setIP(ip);

	DEBUG_LOG(("IP: 0x%8.8X (%s)", ip, str.str()));

	// Add the IP to the list in ascending order
	if (!m_IPlist)
	{
		m_IPlist = newIP;
		newIP->setNext(nullptr);
	}
	else
	{
		if (newIP->getIP() < m_IPlist->getIP())
		{
			newIP->setNext(m_IPlist);
			m_IPlist = newIP;
		}
		else
		{
			EnumeratedIP *p = m_IPlist;
			while (p->getNext() && p->getNext()->getIP() < newIP->getIP())
			{
				p = p->getNext();
			}
			newIP->setNext(p->getNext());
			p->setNext(newIP);
		}
	}
}

AsciiString IPEnumeration::getMachineName()
{
	if (!m_isWinsockInitialized)
	{
		// TheSuperHackers @bugfix bobtista 09/06/2026 Only validate the Winsock
		// version on Windows. On other platforms WSAStartup is a no-op that never
		// fills wsadata, so reading wsadata.wVersion returns uninitialized memory.
#ifdef _WIN32
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return "";
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return "";
		}
#endif
		m_isWinsockInitialized = true;
	}

	// get the local machine's host name
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)))
	{
		DEBUG_LOG(("Failed call to gethostname; WSAGetLastError returned %d", WSAGetLastError()));
		return "";
	}

	return AsciiString(hostname);
}


