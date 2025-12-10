#pragma once

#include <QApplication>
#include "PlatformTypes.h"  // TheSuperHackers @refactor bobtista 01/01/2025 Add cross-platform types
#include "TransDB.h"  // For TransDB and LangID

class CBabylonDlg;

extern char AppTitle[200];
extern CBabylonDlg* MainDLG;
extern TransDB* BabylonstrDB;
extern TransDB* MainDB;
extern char BabylonstrFilename[_MAX_PATH];
extern char MainXLSFilename[_MAX_PATH];
extern char RootPath[_MAX_PATH];
extern char DialogPath[_MAX_PATH];
extern LangID CurrentLanguage;
extern int ViewChanges;

int ExcelRunning(void);

int main(int argc, char* argv[]);


