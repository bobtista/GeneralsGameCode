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
// FILE: DownloadMenuActions.cpp /////////////////////////////////////////////
// See DownloadMenuActions.h. Bodies moved out of DownloadMenu.cpp, where the download manager wrote
// its progress straight into the gadgets; no behavior change.
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GUI/GUICallbacks/Menus/DownloadMenuActions.h"

#include "Common/GameEngine.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/MessageBox.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/GUI/GUICallbacks/Menus/DownloadMenuData.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/DownloadManager.h"
#include "GameNetwork/GameSpy/MainMenuUtils.h"

#include <ctime>

namespace DownloadMenuActions
{

static DownloadMenuData &data()
{
	return DownloadMenuData::instance();
}

static void changed()
{
	DownloadMenuSignals::changed().emit();
}

static WindowLayout *s_layout = nullptr;

static void closeDownloadWindow()
{
	DEBUG_ASSERTCRASH(s_layout, ("No Layout"));
	if (!s_layout)
		return;

	WindowLayout *menuLayout = s_layout;
	s_layout = nullptr;

	menuLayout->runShutdown();
	menuLayout->destroyWindows();
	deleteInstance(menuLayout);
	menuLayout = nullptr;

	GameWindow *mainWin = TheWindowManager->winGetWindowFromId( nullptr, NAMEKEY("MainMenu.wnd:MainMenuParent") );
	if (mainWin)
		TheWindowManager->winSetFocus( mainWin );
}

static void errorCallback()
{
	HandleCanceledDownload();
	closeDownloadWindow();
}

static void successQuitCallback()
{
	TheGameEngine->setQuitting( TRUE );
	closeDownloadWindow();

	// Clean up game data.  No crashy-crash for you!
	if (TheGameLogic->isInGame())
		TheMessageStream->appendMessage( GameMessage::MSG_CLEAR_GAME_DATA );
}

static void successNoQuitCallback()
{
	HandleCanceledDownload();
	closeDownloadWindow();
}

static time_t lastUpdate = 0;
static Int timeLeft = 0;

#if !defined(GENERALS_ONLINE)
static UnicodeString timeLeftText( Int seconds )
{
	UnicodeString timeString;
	if (seconds)
	{
		DEBUG_ASSERTCRASH(seconds > 0, ("Time left is negative!"));
		seconds = max(1, seconds);
		Int takenHour, takenMin, takenSec;
		takenHour = seconds / 60 / 60;
		takenMin = seconds / 60;
		takenSec = seconds % 60;
		timeString.format(TheGameText->fetch("GUI:DownloadTimeLeft"), takenHour, takenMin, takenSec);
	}
	else
	{
		timeString = TheGameText->fetch("GUI:DownloadUnknownTime");
	}
	return timeString;
}
#endif

static void showFileName( const AsciiString &file )
{
	// just get the filename, not the pathname; Ronin @feature 31/10/2026 DX9: as upstream's DownloadMenu.cpp reads it now
	const char *bob = getFileName(file.str());

	data().m_file.translate(bob);
	changed();
}

class DownloadManagerMunkee : public DownloadManager
{
public:
	DownloadManagerMunkee() : m_shouldQuitOnSuccess(false) {}
	virtual HRESULT OnError( Int error ) override;
	virtual HRESULT OnEnd() override;
	virtual HRESULT OnProgressUpdate( Int bytesread, Int totalsize, Int timetaken, Int timeleft ) override;
	virtual HRESULT OnStatusUpdate( Int status ) override;
	virtual HRESULT downloadFile( AsciiString server, AsciiString username, AsciiString password, AsciiString file, AsciiString localfile, AsciiString regkey, Bool tryResume ) override;

	virtual HRESULT SetFileName(AsciiString file);	// Ronin @feature 31/10/2026 DX9: Generals Online's; our DownloadManager has no base

private:
	Bool m_shouldQuitOnSuccess;
};

HRESULT DownloadManagerMunkee::downloadFile( AsciiString server, AsciiString username, AsciiString password, AsciiString file, AsciiString localfile, AsciiString regkey, Bool tryResume )
{
	// see if we'll need to restart
	if (strstr(localfile.str(), "patches\\") != nullptr)
	{
		m_shouldQuitOnSuccess = true;
	}

	showFileName(file);

	password.format("-%s", password.str());
	return DownloadManager::downloadFile( server, username, password, file, localfile, regkey, tryResume );
}

HRESULT DownloadManagerMunkee::SetFileName(AsciiString file)
{
	showFileName(file);
	return S_OK;
}

HRESULT DownloadManagerMunkee::OnError( Int error )
{
	HRESULT ret = DownloadManager::OnError( error );

	MessageBoxOk(TheGameText->fetch("GUI:DownloadErrorTitle"), getErrorString(), errorCallback);
	return ret;
}
HRESULT DownloadManagerMunkee::OnEnd()
{
	HRESULT ret = DownloadManager::OnEnd();

	if (isFileQueuedForDownload())
	{
		return downloadNextQueuedFile();
	}
	if (m_shouldQuitOnSuccess)
		MessageBoxOk(TheGameText->fetch("GUI:DownloadSuccessTitle"), TheGameText->fetch("GUI:DownloadSuccessMustQuit"), successQuitCallback);
	else
		MessageBoxOk(TheGameText->fetch("GUI:DownloadSuccessTitle"), TheGameText->fetch("GUI:DownloadSuccess"), successNoQuitCallback);
	return ret;
}

HRESULT DownloadManagerMunkee::OnProgressUpdate( Int bytesread, Int totalsize, Int timetaken, Int timeleft )
{
	HRESULT ret = DownloadManager::OnProgressUpdate( bytesread, totalsize, timetaken, timeleft );

	if (totalsize > 0)
	{
#if !defined(GENERALS_ONLINE)
		data().m_percent = bytesread * 100 / totalsize;
#else
		data().m_percent = 100.f*((float)bytesread/(float)totalsize);
#endif
	}

	data().m_size.format(TheGameText->fetch("GUI:DownloadBytesRatio"), bytesread, totalsize);

	timeLeft = timeleft;
#if !defined(GENERALS_ONLINE)
	if (data().m_time.isEmpty()) // only update immediately the first time
	{
		lastUpdate = time(nullptr);
		data().m_time = timeLeftText(timeleft);
	}
#endif
	changed();
	return ret;
}

HRESULT DownloadManagerMunkee::OnStatusUpdate( Int status )
{
	HRESULT ret = DownloadManager::OnStatusUpdate( status );

	data().m_status = getStatusString();
	changed();
	return ret;
}

void open( WindowLayout *layout )
{
	s_layout = layout;
	lastUpdate = 0;
	timeLeft = 0;
	data().reset();
	changed();

	DEBUG_ASSERTCRASH(!TheDownloadManager, ("Download manager already exists"));

	delete TheDownloadManager;
	TheDownloadManager = NEW DownloadManagerMunkee;
}

void close()
{
	delete TheDownloadManager;
	TheDownloadManager = nullptr;
}

void cancel()
{
	HandleCanceledDownload();
	closeDownloadWindow();
}

void update()
{
	if (data().m_time.isEmpty())
		return;

	time_t now = time(nullptr);
	if (now <= lastUpdate)
		return;

	lastUpdate = now;

#if !defined(GENERALS_ONLINE)
	data().m_time = timeLeftText(timeLeft);
	changed();
#endif
}

}
