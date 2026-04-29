/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// TheSuperHackers @build bobtista 29/04/2026 Non-Windows stub TU. The real
// FTP/HTTP download path uses winsock + Win registry APIs; this stub gives
// the engine the symbols it needs to link without compiling the full FTP.cpp
// and Download.cpp on macOS/Linux. Every method returns E_FAIL so callers
// see "no download possible" cleanly. Real cross-platform implementation
// can replace this later.

#include "WWDownload/Download.h"
#include "WWDownload/ftp.h"
#include "WWDownload/Registry.h"

bool GetStringFromRegistry(std::string /*path*/, std::string /*key*/, std::string & /*val*/) { return false; }
bool GetUnsignedIntFromRegistry(std::string /*path*/, std::string /*key*/, unsigned int & /*val*/) { return false; }
bool SetStringInRegistry(std::string /*path*/, std::string /*key*/, std::string /*val*/) { return false; }
bool SetUnsignedIntInRegistry(std::string /*path*/, std::string /*key*/, unsigned int /*val*/) { return false; }

Cftp::Cftp()
	: m_iCommandSocket(-1)
	, m_iDataSocket(-1)
	, m_iFilePos(0)
	, m_iBytesRead(0)
	, m_iFileSize(0)
	, m_pfLocalFile(nullptr)
	, m_iStatus(0)
	, m_sendNewPortStatus(0)
	, m_findStart(0)
{
	m_szRemoteFilePath[0] = '\0';
	m_szRemoteFileName[0] = '\0';
	m_szLocalFilePath[0] = '\0';
	m_szLocalFileName[0] = '\0';
	m_szServerName[0] = '\0';
	m_szUserName[0] = '\0';
	m_szPassword[0] = '\0';
}

Cftp::~Cftp()
{
}

HRESULT Cftp::ConnectToServer(LPCSTR /*szServerName*/) { return E_FAIL; }
HRESULT Cftp::DisconnectFromServer() { return E_FAIL; }
HRESULT Cftp::LoginToServer(LPCSTR /*szUserName*/, LPCSTR /*szPassword*/) { return E_FAIL; }
HRESULT Cftp::LogoffFromServer() { return E_FAIL; }
HRESULT Cftp::FindFile(LPCSTR /*szRemoteFileName*/, int * /*piSize*/) { return E_FAIL; }
HRESULT Cftp::FileRecoveryPosition(LPCSTR /*szLocalFileName*/, LPCSTR /*szRegistryRoot*/) { return E_FAIL; }
HRESULT Cftp::GetNextFileBlock(LPCSTR /*szLocalFileName*/, int * /*piTotalRead*/) { return E_FAIL; }
HRESULT Cftp::RecvReply(LPCSTR /*pReplyBuffer*/, int /*iSize*/, int * /*piRetCode*/) { return E_FAIL; }
HRESULT Cftp::SendCommand(LPCSTR /*pCommand*/, int /*iSize*/) { return E_FAIL; }

int  Cftp::SendData(char * /*pData*/, int /*iSize*/) { return -1; }
int  Cftp::RecvData(char * /*pData*/, int /*iSize*/) { return -1; }
int  Cftp::SendNewPort() { return -1; }
int  Cftp::OpenDataConnection() { return -1; }
void Cftp::CloseDataConnection() {}
int  Cftp::AsyncGetHostByName(char * /*szName*/, struct sockaddr_in & /*address*/) { return -1; }
void Cftp::GetDownloadFilename(const char * /*localname*/, char * /*downloadname*/, size_t /*downloadname_size*/) {}
void Cftp::CloseSockets() {}
void Cftp::ZeroStuff() {}

HRESULT CDownload::PumpMessages() { return E_FAIL; }
HRESULT CDownload::Abort() { return E_FAIL; }
HRESULT CDownload::DownloadFile(LPCSTR /*server*/, LPCSTR /*username*/, LPCSTR /*password*/,
	LPCSTR /*file*/, LPCSTR /*localfile*/, LPCSTR /*regkey*/, bool /*tryresume*/)
{
	return E_FAIL;
}
HRESULT CDownload::GetLastLocalFile(char * /*local_file*/, int /*maxlen*/) { return E_FAIL; }
