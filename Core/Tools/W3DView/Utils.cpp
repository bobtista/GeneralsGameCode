/*
**	Command & Conquer Renegade(tm)
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

/////////////////////////////////////////////////////////////////////////////
//
//  Utils.cpp
//
//  Module containing usefull misc. utility functions
//


// Include Qt headers FIRST to get HWND, HBITMAP, etc. definitions
#include <QApplication>
#include <QtCore/QtGlobal>  // For Q_UNUSED
#ifdef QT_VERSION
#include <QtGui/qwindowdefs_win.h>  // For HWND, HBITMAP on Windows
#endif

#include "Utils.h"
#include "W3DViewDoc_Qt.h"
#include "MainFrm_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "texture.h"
    #include "assetmgr.h"
    #include "agg_def.h"
    #include "hlod.h"
    #include <VFW.h>
    #include "rcfile.h"
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
#endif

// Windows API type stubs for Core build (only when HAVE_WWVEGAS is NOT defined)
// When HAVE_WWVEGAS is defined, Windows headers are included, so don't define stubs
#ifndef HAVE_WWVEGAS
// HWND is already defined by Qt's qwindowdefs_win.h - don't redefine it
#ifndef UINT
typedef unsigned int UINT;
#endif
#ifndef BOOL
typedef int BOOL;
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef LPARAM
typedef long LPARAM;
#endif
#ifndef WPARAM
typedef unsigned int WPARAM;
#endif
#ifndef LRESULT
typedef long LRESULT;
#endif
#ifndef LONG
typedef long LONG;
#endif
#ifndef LPCSTR
typedef const char* LPCSTR;
#endif
#ifndef UDM_GETRANGE32
#define UDM_GETRANGE32 0
#endif
#ifndef UDM_GETBUDDY
#define UDM_GETBUDDY 0
#endif
#ifndef GW_CHILD
#define GW_CHILD 0
#endif
#ifndef GW_HWNDNEXT
#define GW_HWNDNEXT 0
#endif
#ifndef GWL_STYLE
#define GWL_STYLE 0
#endif
#ifndef UDS_SETBUDDYINT
#define UDS_SETBUDDYINT 0
#endif

// Windows API function stubs
inline LRESULT SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) { return 0; }
inline HWND GetWindow(HWND hWnd, UINT uCmd) { return nullptr; }
inline BOOL IsWindow(HWND hWnd) { return FALSE; }
inline LONG GetWindowLong(HWND hWnd, int nIndex) { return 0; }
inline int GetClassName(HWND hWnd, char* lpClassName, int nMaxCount) { return 0; }
inline BOOL EnableWindow(HWND hWnd, BOOL bEnable) { return FALSE; }
inline BOOL SetWindowText(HWND hWnd, LPCSTR lpString) { return FALSE; }
inline int GetWindowText(HWND hWnd, char* lpString, int nMaxCount) { return 0; }
#ifndef RECT
struct RECT { long left, top, right, bottom; };
#endif
// HDC is already defined by Qt's qwindowdefs_win.h - don't redefine it
inline HDC GetDC(HWND hWnd) { Q_UNUSED(hWnd); return nullptr; }
inline int ReleaseDC(HWND hWnd, HDC hDC) { Q_UNUSED(hWnd); Q_UNUSED(hDC); return 0; }
inline BOOL ValidateRect(HWND hWnd, const RECT* lpRect) { Q_UNUSED(hWnd); Q_UNUSED(lpRect); return FALSE; }
inline BOOL GetClientRect(HWND hWnd, RECT* lpRect) { Q_UNUSED(hWnd); if(lpRect) { lpRect->left=lpRect->top=lpRect->right=lpRect->bottom=0; } return FALSE; }
#endif // !HAVE_WWVEGAS
#ifndef RGB
#define RGB(r,g,b) ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))
#endif
// MFC CDC stub
class CDC {
public:
    void Attach(HDC hDC) { Q_UNUSED(hDC); }
    void Detach() {}
    void FillSolidRect(int x, int y, int cx, int cy, unsigned long color) { Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(cx); Q_UNUSED(cy); Q_UNUSED(color); }
};
// TCHAR is defined by windows.h or tchar.h when HAVE_WWVEGAS is defined - don't redefine
#ifndef HAVE_WWVEGAS
#ifndef TCHAR
typedef char TCHAR;
#endif
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
// strrchr is already defined in string.h - don't redefine it
#ifndef lstrcpy
inline char* lstrcpy(char* dest, const char* src) { return strcpy(dest, src); }
#endif


////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentDocument
//
////////////////////////////////////////////////////////////////////////////
CW3DViewDoc *
GetCurrentDocument (void)
{
    // Assume failure
    CW3DViewDoc *pCDoc = NULL;

    // Get a pointer to the main window using Qt
    QWidget* mainWidget = QApplication::activeWindow();
    if (!mainWidget) {
        QWidgetList widgets = QApplication::topLevelWidgets();
        for (QWidget* widget : widgets) {
            CMainFrame* mainFrame = qobject_cast<CMainFrame*>(widget);
            if (mainFrame) {
                pCDoc = mainFrame->GetActiveDocument();
                break;
            }
        }
    } else {
        CMainFrame* pCMainWnd = qobject_cast<CMainFrame*>(mainWidget);
        if (pCMainWnd) {
            pCDoc = pCMainWnd->GetActiveDocument();
        }
    }

    // Return the doc pointer
    return pCDoc;
}

/////////////////////////////////////////////////////////////
//
//  Paint_Gradient
//
void
Paint_Gradient
(
	HWND hWnd,
	BYTE baseRed,
	BYTE baseGreen,
	BYTE baseBlue
)
{
    // TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
    Q_UNUSED(hWnd);
    Q_UNUSED(baseRed);
    Q_UNUSED(baseGreen);
    Q_UNUSED(baseBlue);
    // Stub - MFC CDC and Windows GDI not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  SetDlgItemFloat, GetDlgItemFloat, Initialize_Spinner, Update_Spinner_Buddy
//  Stub implementations for Core build (Windows API not available)
//
void
SetDlgItemFloat
(
	HWND hdlg,
	UINT child_id,
	float value
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(hdlg);
	Q_UNUSED(child_id);
	Q_UNUSED(value);
	// Stub - Windows API not available in Core build
}

float
GetDlgItemFloat
(
	HWND hdlg,
	UINT child_id
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(hdlg);
	Q_UNUSED(child_id);
	return 0.0f;  // Stub - Windows API not available in Core build
}

void
Initialize_Spinner
(
	CSpinButtonCtrl &ctrl,
	float pos,
	float min,
	float max
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(ctrl);
	Q_UNUSED(pos);
	Q_UNUSED(min);
	Q_UNUSED(max);
	// Stub - MFC controls not available in Core build
}

void
Update_Spinner_Buddy (CSpinButtonCtrl &ctrl, int delta)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(ctrl);
	Q_UNUSED(delta);
	// Stub - MFC controls not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  Update_Spinner_Buddy (HWND version), Enable_Dialog_Controls, SetWindowFloat, GetWindowFloat
//  Stub implementations for Core build
//
void
Update_Spinner_Buddy (HWND hspinner, int delta)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(hspinner);
	Q_UNUSED(delta);
	// Stub - Windows API not available in Core build
}

#if 0  // Disabled - using stubs above
#ifdef _WIN32
void
Update_Spinner_Buddy_OLD (HWND hspinner, int delta)
{
	//
	//	Only perform this service if the spinner isn't an auto buddy
	//
	if ((::GetWindowLong (hspinner, GWL_STYLE) & UDS_SETBUDDYINT) == 0) {
		HWND hbuddy_wnd = (HWND)SendMessage (hspinner, UDM_GETBUDDY, 0, 0L);
		if (::IsWindow (hbuddy_wnd)) {

			// Get the current value, increment it, and put it back into the control
			float value = ::GetWindowFloat (hbuddy_wnd);
			value += (((float)(delta)) / 100.0F);

			//
			//	Validate the new position
			//
			int int_min = 0;
			int int_max = 0;
			SendMessage (hspinner, UDM_GETRANGE32, (WPARAM)&int_min, (LPARAM)&int_max);
			float float_min = ((float)int_min) / 100;
			float float_max = ((float)int_max) / 100;
			value = std::max (float_min, value);
			value = std::min (float_max, value);

			// Pass the value onto the buddy window
			::SetWindowFloat (hbuddy_wnd, value);
		}
	}

	return ;
}
#endif  // _WIN32
#endif  // End of disabled block (#if 0)

void
Enable_Dialog_Controls (HWND dlg,bool onoff)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(dlg);
	Q_UNUSED(onoff);
	// Stub - Windows API not available in Core build
}

void
SetWindowFloat
(
	HWND hwnd,
	float value
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(hwnd);
	Q_UNUSED(value);
	// Stub - Windows API not available in Core build
}

float
GetWindowFloat (HWND hwnd)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(hwnd);
	return 0.0f;  // Stub - Windows API not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  Asset_Name_From_Filename, Filename_From_Asset_Name, Get_Filename_From_Path, Strip_Filename_From_Path
//  Stub implementations for Core build (use CString which is QString)
//
CString
Asset_Name_From_Filename (LPCTSTR filename)
{
	// Get the filename from this path
	CString asset_name = ::Get_Filename_From_Path (filename);

#ifdef HAVE_WWVEGAS
	// CString is StringClass - use StringClass methods
	// Find the index of the extension (if exists)
	int extension = asset_name.Get_Length() - 1;
	while (extension >= 0 && asset_name[extension] != '.') {
		extension--;
	}
	// Strip off the extension
	if (extension >= 0) {
		asset_name.Erase(extension, asset_name.Get_Length() - extension);
	}
#else
	// CString is QString
	QString qname = asset_name;
	int extension = qname.lastIndexOf ('.');
	// Strip off the extension
	if (extension != -1) {
		qname = qname.left (extension);
		asset_name = qname;
	}
#endif

	// Return the name of the asset
	return asset_name;
}

CString
Filename_From_Asset_Name (LPCTSTR asset_name)
{
#ifdef HAVE_WWVEGAS
	// CString is StringClass - use StringClass methods
	CString filename(asset_name);
	filename += ".w3d";
	return filename;
#else
	// CString is QString
	QString qname = QString::fromLocal8Bit(asset_name);
	CString filename = qname + ".w3d";
	return filename;
#endif
}

CString
Get_Filename_From_Path (LPCTSTR path)
{
#ifdef HAVE_WWVEGAS
	// CString is StringClass - use StringClass methods
	// In Unicode builds, LPCTSTR is const wchar_t*, so convert to const char* first
	// For now, assume ANSI build (LPCTSTR is const char*)
	const char* path_char = (const char*)path;  // Cast for strrchr
	const char* filename = strrchr (path_char, '\\');
	if (filename == NULL) {
		filename = strrchr (path_char, '/');  // Try forward slash too
	}
	if (filename != NULL) {
		// Increment past the directory deliminator
		filename ++;
	} else {
		filename = path_char;
	}
	return CString(filename);  // Directly construct StringClass from const char*
#else
	// CString is QString
	QString qpath = QString::fromLocal8Bit(path);
	int lastSlash = qpath.lastIndexOf('\\');
	if (lastSlash == -1) {
		lastSlash = qpath.lastIndexOf('/');
	}
	if (lastSlash != -1) {
		return qpath.mid(lastSlash + 1);
	}
	return qpath;
#endif
}

CString
Strip_Filename_From_Path (LPCTSTR path)
{
#ifdef HAVE_WWVEGAS
	// CString is StringClass - use StringClass methods directly
	CString result(path);
	// Find the last occurrence of the directory delimiter
	int lastSlash = result.Get_Length() - 1;
	while (lastSlash >= 0 && result[lastSlash] != '\\' && result[lastSlash] != '/') {
		lastSlash--;
	}
	// Strip off the filename
	if (lastSlash >= 0) {
		result.Erase(lastSlash + 1, result.Get_Length() - lastSlash - 1);
	}
	return result;
#else
	// CString is QString - use QString for path manipulation
	QString qpath = QString::fromLocal8Bit(path);
	
	// Find the last occurrence of the directory delimiter
	int lastSlash = qpath.lastIndexOf('\\');
	if (lastSlash == -1) {
		lastSlash = qpath.lastIndexOf('/');  // Try forward slash too
	}
	
	// Strip off the filename
	if (lastSlash != -1) {
		qpath = qpath.left(lastSlash);
	}

	// Return the path only
	return CString (qpath);
#endif
}


////////////////////////////////////////////////////////////////////////////
//
//  Create_DIB_Section, Make_Bitmap_From_Texture, Get_Texture_Name, Build_Emitter_List,
//  Is_Aggregate, Rename_Aggregate_Prototype, Is_Real_LOD
//  Stub implementations for Core build (game engine not available)
//
HBITMAP
Create_DIB_Section
(
	UCHAR **pbits,
	int width,
	int height
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(pbits);
	Q_UNUSED(width);
	Q_UNUSED(height);
	return nullptr;  // Stub - Windows GDI not available in Core build
}

HBITMAP
Make_Bitmap_From_Texture (TextureClass &texture, int width, int height)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(texture);
	Q_UNUSED(width);
	Q_UNUSED(height);
	return nullptr;  // Stub - game engine not available in Core build
}

CString
Get_Texture_Name (TextureClass &texture)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(texture);
	return CString();  // Stub - game engine not available in Core build
}

void
Build_Emitter_List
(
	RenderObjClass &render_obj,
	DynamicVectorClass<CString> &list
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(render_obj);
	Q_UNUSED(list);
	// Stub - game engine not available in Core build
}

bool
Is_Aggregate (const char *asset_name)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(asset_name);
	return false;  // Stub - game engine not available in Core build
}

void
Rename_Aggregate_Prototype
(
	const char *old_name,
	const char *new_name
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(old_name);
	Q_UNUSED(new_name);
	// Stub - game engine not available in Core build
}

bool
Is_Real_LOD (const char *asset_name)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(asset_name);
	return false;  // Stub - game engine not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  Get_File_Time
//
// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
bool
Get_File_Time
(
	LPCTSTR path,
	LPFILETIME pcreation_time,
	LPFILETIME paccess_time,
	LPFILETIME pwrite_time
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(path);
	Q_UNUSED(pcreation_time);
	Q_UNUSED(paccess_time);
	Q_UNUSED(pwrite_time);
	return false;  // Stub - Windows API not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  Are_Glide_Drivers_Acceptable
//
// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
bool
Are_Glide_Drivers_Acceptable (void)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	return false;  // Stub - Windows API not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  Load_RC_Texture, Resolve_Path, Find_Missing_Textures, Copy_File
//  Stub implementations for Core build
//
TextureClass *
Load_RC_Texture (LPCTSTR resource_name)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(resource_name);
	return nullptr;  // Stub - game engine not available in Core build
}

void
Resolve_Path (CString &filename)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(filename);
	// Stub - Windows API not available in Core build
}

void
Find_Missing_Textures
(
	DynamicVectorClass<StringClass> &	list,
	LPCTSTR								name,
	int									frame_count
)
{
#ifdef HAVE_WWVEGAS
	// Real implementation using WWVegas AssetManager
	// Note: Find_Missing_Textures may not exist in WW3DAssetManager - stub for now
	// TODO: Implement or find the correct method name
	Q_UNUSED(list);
	Q_UNUSED(name);
	Q_UNUSED(frame_count);
#else
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(list);
	Q_UNUSED(name);
	Q_UNUSED(frame_count);
	// Stub - Windows API not available in Core build
#endif
}

// TheSuperHackers @refactor bobtista 01/01/2025 Add overload for wchar_t* to match linker expectations
#ifdef _WIN32
#ifndef HAVE_WWVEGAS
// Unicode overload for Qt builds (when HAVE_WWVEGAS is not defined, CString is QString)
void
Find_Missing_Textures
(
	DynamicVectorClass<CString> &	list,
	const wchar_t*					name,
	int								frame_count
)
{
	// Convert wchar_t* to const char* for the main implementation
	QString qname = QString::fromWCharArray(name);
	Find_Missing_Textures(list, qname.toLocal8Bit().constData(), frame_count);
}
#endif
#endif

bool
Copy_File
(
	LPCTSTR	existing_filename,
	LPCTSTR	new_filename,
	bool		force_copy
)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementation for Core build
	Q_UNUSED(existing_filename);
	Q_UNUSED(new_filename);
	Q_UNUSED(force_copy);
	return false;  // Stub - Windows API not available in Core build
}


////////////////////////////////////////////////////////////////////////////
//
//  Get_Graphic_View
//
////////////////////////////////////////////////////////////////////////////
CGraphicView *
Get_Graphic_View (void)
{
	CGraphicView *view = NULL;

	//
	//	Get the view from the current document
	//
	CW3DViewDoc *doc = GetCurrentDocument ();
	if (doc != NULL) {
		view = doc->GetGraphicView ();
	}

	return view;
}

// TheSuperHackers @refactor bobtista 01/01/2025 Define shader preset to avoid multiple definition
#ifdef HAVE_WWVEGAS
#include "shader.h"  // For ShaderClass
ShaderClass* g_PresetATestBlend2DShader = nullptr;
#else
// Stub for Core build
#include "GameEngineStubs.h"
ShaderClass* g_PresetATestBlend2DShader = nullptr;
#endif
