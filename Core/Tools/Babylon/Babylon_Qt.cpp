// Include Windows headers FIRST, before Qt, to avoid conflicts
#ifdef _WIN32
    // Undefine any Qt-defined types that might conflict
    #ifdef SHORT
    #undef SHORT
    #endif
    // Ensure we use ANSI functions, not Unicode
    #ifdef UNICODE
    #undef UNICODE
    #endif
    #ifdef _UNICODE
    #undef _UNICODE
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <direct.h>
#else
    #include <unistd.h>
#endif

// Now include Qt headers - they should be safe after windows.h
#include <QApplication>
#include <QMessageBox>
#include <cstring>

// Include our headers
#include "Babylon_Qt.h"
#include "BabylonDlg_Qt.h"
#include "XLStuff.h"
#include "PlatformTypes.h"  // For Windows types and CALLBACK

// Undefine Windows macros that conflict with Qt
#ifdef _WIN32
    #ifdef connect
    #undef connect
    #endif
    #ifdef SendMessage
    #undef SendMessage
    #endif
    #ifdef GetMessage
    #undef GetMessage
    #endif
#endif

char AppTitle[200];
CBabylonDlg* MainDLG = NULL;

TransDB* BabylonstrDB = NULL;
TransDB* MainDB = NULL;
char BabylonstrFilename[_MAX_PATH];
char MainXLSFilename[_MAX_PATH];
char RootPath[_MAX_PATH];
char DialogPath[_MAX_PATH];
LangID CurrentLanguage = LANGID_UNKNOWN;
int ViewChanges = FALSE;

static const char* AppName = "Babylon:";
static HWND FoundWindow = NULL;

#ifdef _WIN32
static BOOL CALLBACK EnumAllWindowsProcExact(HWND hWnd, LPARAM lParam);
#endif
static const char* szSearchTitle;

static int AlreadyRunning(void)
{
	BOOL found = FALSE;
	szSearchTitle = AppName;
#ifdef _WIN32
	EnumWindows((WNDENUMPROC)EnumAllWindowsProcExact, (LPARAM)&found);
#endif
	return found;
}

int ExcelRunning(void)
{
	BOOL found = FALSE;
	szSearchTitle = "Microsoft Excel";
#ifdef _WIN32
	EnumWindows((WNDENUMPROC)EnumAllWindowsProcExact, (LPARAM)&found);
#endif
	return found;
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	
	// OLE initialization is MFC-specific - skip for Qt build
	// Excel COM integration will be stubbed/disabled in Qt version
#ifdef _WIN32
	// For Qt build, we don't use MFC's AfxOleInit
	// If Excel integration is needed, use CoInitialize/CoUninitialize instead
	// But for now, Excel integration is disabled in Qt build
#endif
	
	MainDB = new TransDB("Main");
	BabylonstrDB = new TransDB("Generals.str");
	MainDB->EnableIDs();
	
	sprintf(AppTitle, "%s Built on %s - %s", AppName, __DATE__, __TIME__);
	
	#ifdef _WIN32
		if (!_getcwd(RootPath, _MAX_PATH)) {
			QMessageBox::warning(nullptr, "Error", "Failed to obtain current working directory!\n\nSet directory path to \"c:\\Babylon\".");
			strcpy(RootPath, "c:\\Babylon");
		}
	#else
		// Unix: Use getcwd from unistd.h
		#include <unistd.h>
		if (!getcwd(RootPath, _MAX_PATH)) {
			QMessageBox::warning(nullptr, "Error", "Failed to obtain current working directory!");
			strcpy(RootPath, "/tmp/Babylon");
		}
	#endif
	#ifdef _WIN32
		strlwr(RootPath);
		strcpy(BabylonstrFilename, RootPath);
		strcat(BabylonstrFilename, "\\Data\\Generals.str");
		strcpy(MainXLSFilename, RootPath);
		strcat(MainXLSFilename, "\\Data\\main.db");
		strcpy(DialogPath, RootPath);
		strcat(DialogPath, "\\dialog");
	#else
		// Unix: Use forward slashes
		strcpy(BabylonstrFilename, RootPath);
		strcat(BabylonstrFilename, "/Data/Generals.str");
		strcpy(MainXLSFilename, RootPath);
		strcat(MainXLSFilename, "/Data/main.db");
		strcpy(DialogPath, RootPath);
		strcat(DialogPath, "/dialog");
	#endif
	
	if (AlreadyRunning()) {
#ifdef _WIN32
		if (FoundWindow) {
			SetForegroundWindow(FoundWindow);
		}
#endif
		return 0;
	}
	
	// On non-Windows, OpenExcel() returns FALSE, but we still want to show the dialog
	// Excel integration is optional - the dialog can work without it
	#ifdef _WIN32
		if (OpenExcel()) {
	#else
		// On non-Windows, always show the dialog (Excel integration not available)
		{
	#endif
			CBabylonDlg dlg;
			MainDLG = &dlg;
			
			dlg.exec();
			
			#ifdef _WIN32
				CloseExcel();
			#endif
		}
	
	delete BabylonstrDB;
	delete MainDB;
	
	return 0;
}

#ifdef _WIN32
static BOOL CALLBACK EnumAllWindowsProcExact(HWND hWnd, LPARAM lParam)
{
	char windowTitle[256];
	// Use GetWindowTextA for ANSI strings
	// GetWindowTextA is defined in winuser.h (included via windows.h)
	// LPSTR is char* - the array decays to pointer, so this should work
	int textLen = GetWindowTextA(hWnd, (LPSTR)windowTitle, sizeof(windowTitle));
	if (textLen > 0) {
		if (strstr(windowTitle, szSearchTitle)) {
			FoundWindow = hWnd;
			*(BOOL*)lParam = TRUE;
			return FALSE;
		}
	}
	return TRUE;
}
#endif
