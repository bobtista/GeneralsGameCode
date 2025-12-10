#include "Babylon_Qt.h"
#include "BabylonDlg_Qt.h"
#include "XLStuff.h"
#include <QApplication>
#include <QMessageBox>
#include <cstring>
#ifdef _WIN32
    #include <direct.h>
#else
    #include <unistd.h>
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

static BOOL CALLBACK EnumAllWindowsProcExact(HWND hWnd, LPARAM lParam);
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
	
#ifdef _WIN32
	if (!AfxOleInit()) {
		QMessageBox::critical(nullptr, "Error", "OLE initialization failed");
		return 1;
	}
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
	if (GetWindowText(hWnd, windowTitle, sizeof(windowTitle))) {
		if (strstr(windowTitle, szSearchTitle)) {
			FoundWindow = hWnd;
			*(BOOL*)lParam = TRUE;
			return FALSE;
		}
	}
	return TRUE;
}
#endif


