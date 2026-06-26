#pragma once

// TheSuperHackers @build bobtista 29/04/2026 winsock.h compatibility shim for
// non-Windows builds. Maps the Win32 BSD-derived socket API to native POSIX
// equivalents so WWDownload (FTP client) at least compiles.

#include "windows.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef int SOCKET;
typedef struct hostent HOSTENT;
typedef HOSTENT *PHOSTENT;
typedef HOSTENT *LPHOSTENT;

#ifndef MAKEWORD
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))
#endif

typedef struct WSAData
{
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char *lpVendorInfo;
} WSADATA, *LPWSADATA;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

#ifndef closesocket
#define closesocket(s) (::close(s))
#endif

#ifndef ioctlsocket
#define ioctlsocket(s, cmd, argp) (::ioctl((s), (cmd), (argp)))
#endif

#ifndef WSAGetLastError
#define WSAGetLastError() (errno)
#endif

#ifndef WSAStartup
#define WSAStartup(wVer, lpData) (0)
#endif

#ifndef WSACleanup
#define WSACleanup() (0)
#endif

#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK EAGAIN
#endif

#ifndef WSAEINTR
#define WSAEINTR EINTR
#endif

#ifndef WSAEBADF
#define WSAEBADF EBADF
#endif

#ifndef WSAEACCES
#define WSAEACCES EACCES
#endif

#ifndef WSAEFAULT
#define WSAEFAULT EFAULT
#endif

#ifndef WSAEINVAL
#define WSAEINVAL EINVAL
#endif

#ifndef WSAEMFILE
#define WSAEMFILE EMFILE
#endif

#ifndef WSAEINPROGRESS
#define WSAEINPROGRESS EINPROGRESS
#endif

#ifndef WSAEALREADY
#define WSAEALREADY EALREADY
#endif

#ifndef WSAENOTSOCK
#define WSAENOTSOCK ENOTSOCK
#endif

#ifndef WSAEDESTADDRREQ
#define WSAEDESTADDRREQ EDESTADDRREQ
#endif

#ifndef WSAEMSGSIZE
#define WSAEMSGSIZE EMSGSIZE
#endif

#ifndef WSAEPROTOTYPE
#define WSAEPROTOTYPE EPROTOTYPE
#endif

#ifndef WSAENOPROTOOPT
#define WSAENOPROTOOPT ENOPROTOOPT
#endif

#ifndef WSAEPROTONOSUPPORT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#endif

#ifndef WSAESOCKTNOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#endif

#ifndef WSAEOPNOTSUPP
#define WSAEOPNOTSUPP EOPNOTSUPP
#endif

#ifndef WSAEPFNOSUPPORT
#define WSAEPFNOSUPPORT EPFNOSUPPORT
#endif

#ifndef WSAEAFNOSUPPORT
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#endif

#ifndef WSAEADDRINUSE
#define WSAEADDRINUSE EADDRINUSE
#endif

#ifndef WSAEADDRNOTAVAIL
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#endif

#ifndef WSAENETDOWN
#define WSAENETDOWN ENETDOWN
#endif

#ifndef WSAENETUNREACH
#define WSAENETUNREACH ENETUNREACH
#endif

#ifndef WSAENETRESET
#define WSAENETRESET ENETRESET
#endif

#ifndef WSAECONNABORTED
#define WSAECONNABORTED ECONNABORTED
#endif

#ifndef WSAECONNRESET
#define WSAECONNRESET ECONNRESET
#endif

#ifndef WSAENOBUFS
#define WSAENOBUFS ENOBUFS
#endif

#ifndef WSAEISCONN
#define WSAEISCONN EISCONN
#endif

#ifndef WSAENOTCONN
#define WSAENOTCONN ENOTCONN
#endif

#ifndef WSAESHUTDOWN
#define WSAESHUTDOWN ESHUTDOWN
#endif

#ifndef WSAETOOMANYREFS
#define WSAETOOMANYREFS ETOOMANYREFS
#endif

#ifndef WSAETIMEDOUT
#define WSAETIMEDOUT ETIMEDOUT
#endif

#ifndef WSAECONNREFUSED
#define WSAECONNREFUSED ECONNREFUSED
#endif

#ifndef WSAELOOP
#define WSAELOOP ELOOP
#endif

#ifndef WSAENAMETOOLONG
#define WSAENAMETOOLONG ENAMETOOLONG
#endif

#ifndef WSAEHOSTDOWN
#define WSAEHOSTDOWN EHOSTDOWN
#endif

#ifndef WSAEHOSTUNREACH
#define WSAEHOSTUNREACH EHOSTUNREACH
#endif

#ifndef WSAENOTEMPTY
#define WSAENOTEMPTY ENOTEMPTY
#endif

#ifndef WSAEPROCLIM
#ifdef EPROCLIM
#define WSAEPROCLIM EPROCLIM
#else
#define WSAEPROCLIM 10067
#endif
#endif

#ifndef WSAEUSERS
#define WSAEUSERS EUSERS
#endif

#ifndef WSAEDQUOT
#define WSAEDQUOT EDQUOT
#endif

#ifndef WSAESTALE
#define WSAESTALE ESTALE
#endif

#ifndef WSAEREMOTE
#define WSAEREMOTE EREMOTE
#endif

#ifndef WSABASEERR
#define WSABASEERR 10000
#endif

#ifndef WSAEDISCON
#define WSAEDISCON 10101
#endif

#ifndef WSASYSNOTREADY
#define WSASYSNOTREADY 10091
#endif

#ifndef WSAVERNOTSUPPORTED
#define WSAVERNOTSUPPORTED 10092
#endif

#ifndef WSANOTINITIALISED
#define WSANOTINITIALISED 10093
#endif

#ifndef WSAHOST_NOT_FOUND
#define WSAHOST_NOT_FOUND 11001
#endif

#ifndef WSATRY_AGAIN
#define WSATRY_AGAIN 11002
#endif

#ifndef WSANO_RECOVERY
#define WSANO_RECOVERY 11003
#endif

#ifndef WSANO_DATA
#define WSANO_DATA 11004
#endif
