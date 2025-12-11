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
//  Utils.h
//
//  Module containing usefull misc. utility functions
//

#pragma once

#include <string>  // For std::string stubs on non-Windows
#include <cstring>  // For strlen, strcat

// Include Qt headers early to get HWND, HBITMAP, etc. definitions
#ifdef QT_VERSION
#include <QtGui/qwindowdefs_win.h>  // For HWND, HBITMAP on Windows
#endif
// Include windows.h on Windows if Qt isn't available
#ifdef _WIN32
#ifndef QT_VERSION
#include <windows.h>  // For HWND, HBITMAP on Windows (non-Qt builds)
#endif
// On Windows, HWND and HBITMAP are already defined by Qt or windows.h - don't redefine
#else
// Define Windows types for non-Windows platforms
typedef void* HWND;
typedef void* HBITMAP;
#endif

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    // WWVegas types are available via WWVegas includes
    #include "wwstring.h"  // For StringClass
    #include "vector.h"  // For DynamicVectorClass
    #ifdef _WIN32
    #include <tchar.h>  // For _T macro
    #endif
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
    
    // Vector template classes (needed for Core build when WWVegas is not available)
    template<typename T> class Vector {
    public:
        T* Get_Array() { return nullptr; }
        int Get_Count() { return 0; }
    };
    template<typename T> class DynamicVectorClass {
    public:
        void Add(const T& item) {}
        void Delete_All() {}
        int Get_Count() const { return 0; }
        int Count() const { return 0; }  // Added for EmitterInstanceList
        T& operator[](int index) { static T dummy; return dummy; }
        const T& operator[](int index) const { static T dummy; return dummy; }
    };
#endif // !HAVE_WWVEGAS

// Windows type stubs for Core build
// LPCTSTR is defined by windows.h - only define if not already defined
// When HAVE_WWVEGAS is defined, windows.h is included via WWVegas headers, so don't define it
#if !defined(HAVE_WWVEGAS) && !defined(LPCTSTR) && !defined(_WINDEF_) && !defined(_WINUSER_) && !defined(_WINDOWS_) && !defined(_WINNT_)
typedef const char* LPCTSTR;
#endif
// CString: when HAVE_WWVEGAS is defined, use StringClass; otherwise use QString or std::string
// TheSuperHackers @refactor bobtista 01/01/2025 MSVC 2022 compatibility: MFC defines CString when _AFXDLL is defined
// When _AFXDLL is defined, we need to include afx.h to get CString, but we're in a Qt build, so we use StringClass
#ifdef _AFXDLL
    // MFC is available - but we're in a Qt build, so we should use StringClass instead
    // However, if MFC headers are included, CString will be defined
    // For now, we'll use StringClass when HAVE_WWVEGAS is defined
    #ifdef HAVE_WWVEGAS
        class StringClass;
        typedef StringClass CString;
    #else
        // If MFC is available but WWVegas is not, we'd use MFC's CString
        // But in a Qt build, this shouldn't happen
        #include <QString>
        typedef QString CString;
    #endif
#elif !defined(CString)
#ifdef HAVE_WWVEGAS
    // StringClass is defined in WWVegas
    class StringClass;
    typedef StringClass CString;
#elif defined(QT_VERSION)
    #include <QString>  // For QString
    typedef QString CString;  // Use QString as CString replacement
#else
    #include <string>
    typedef std::string CString;
#endif
#endif
#ifndef UCHAR
typedef unsigned char UCHAR;
#endif
// LPFILETIME is defined by windows.h - only define if not already defined
// TheSuperHackers @refactor bobtista 01/01/2025 MSVC 2022 compatibility: Don't redefine types that windows.h defines
#if !defined(LPFILETIME) && !defined(_WINDEF_) && !defined(_WINUSER_) && !defined(_WINNT_)
typedef void* LPFILETIME;
#endif
#ifndef BYTE
typedef unsigned char BYTE;
#endif
#ifndef UINT
typedef unsigned int UINT;
#endif
// HBITMAP and HWND are already defined by Qt's qwindowdefs_win.h when Qt headers are included - don't redefine them

// Forward declarations
class RenderObjClass;


/////////////////////////////////////////////////////////////////////////////
//
// Macros
//
#define SAFE_DELETE(pobject) { delete pobject; pobject = NULL; }
#define SAFE_DELETE_ARRAY(pobject) { delete [] pobject; pobject = NULL; }

#define COM_RELEASE(pobject)					\
			if (pobject) {							\
				pobject->Release ();				\
			}											\
			pobject = NULL;						\

#define SAFE_CLOSE(handle)								\
			if (handle != INVALID_HANDLE_VALUE) {	\
				::CloseHandle (handle);					\
				handle = INVALID_HANDLE_VALUE;		\
			}													\

#define SANITY_CHECK(expr)		\
			ASSERT (expr);			\
			if (!expr)

/////////////////////////////////////////////////////////////////////////////
//
// Inlines
//
/////////////////////////////////////////////////////////////////////////////

// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
// LPTSTR is defined by windows.h - only define if not already defined
#if !defined(LPTSTR) && !defined(_WINDEF_) && !defined(_WINUSER_)
typedef char* LPTSTR;
#endif
#include <cstring>  // For strlen, strcat

inline void Delimit_Path (LPTSTR path)
{
	// TheSuperHackers @refactor bobtista 01/01/2025 LPTSTR might be wchar_t* on Windows (Unicode)
	// Cast to char* for strlen/strcat compatibility
	char* path_char = reinterpret_cast<char*>(path);
	size_t len = strlen(path_char);
	if (len > 0 && path_char[len - 1] != '\\') {
		strcat(path_char, "\\");
	}
}

inline void Delimit_Path (CString &path)
{
#ifdef HAVE_WWVEGAS
	// CString is StringClass - use StringClass methods
	// str() returns const TCHAR*, check length and append if needed
	const TCHAR* buffer = path.str();  // str() returns const TCHAR*
	if (buffer && path.Get_Length() > 0) {
		int len = path.Get_Length();
		if (buffer[len - 1] != _T('\\')) {
			// StringClass has operator+= for const TCHAR*
			path += _T("\\");
		}
	}
#elif defined(QT_VERSION)
	// CString is QString
	QString qpath = path;
	if (qpath.length() > 0 && qpath[qpath.length() - 1] != '\\') {
		qpath += "\\";
		path = qpath;
	}
#else
	// CString is std::string
	std::string& spath = path;
	if (spath.length() > 0 && spath[spath.length() - 1] != '\\') {
		spath += "\\";
	}
#endif
}


// Forward declarations
class TextureClass;
class CGraphicView;


/////////////////////////////////////////////////////////////////////////////
//
// Prototypes
//
class CW3DViewDoc *	GetCurrentDocument (void);
CGraphicView *			Get_Graphic_View (void);
// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
void Paint_Gradient (HWND hWnd, BYTE baseRed, BYTE baseGreen, BYTE baseBlue);

//
// Dialog routines
//
// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
// HWND, BYTE, UINT already defined above (or by Qt/Windows headers)
class CSpinButtonCtrl {};  // Stub class

void						SetDlgItemFloat (HWND hdlg, UINT child_id, float value);
float						GetDlgItemFloat (HWND hdlg, UINT child_id);
void						SetWindowFloat (HWND hwnd, float value);
float						GetWindowFloat (HWND hwnd);
void						Initialize_Spinner (CSpinButtonCtrl &ctrl, float pos = 0, float min = 0, float max = 1);
void						Update_Spinner_Buddy (CSpinButtonCtrl &ctrl);
void						Update_Spinner_Buddy (HWND hspinner, int delta);
void						Enable_Dialog_Controls (HWND dlg,bool onoff);

//
//	String manipulation routines
//
CString					Get_Filename_From_Path (LPCTSTR path);
CString					Strip_Filename_From_Path (LPCTSTR path);
CString					Asset_Name_From_Filename (LPCTSTR filename);
CString					Filename_From_Asset_Name (LPCTSTR asset_name);

//
//	File routines
//
bool						Get_File_Time (LPCTSTR path, LPFILETIME pcreation_time, LPFILETIME paccess_time = NULL, LPFILETIME pwrite_time = NULL);
bool						Are_Glide_Drivers_Acceptable (void);
bool						Copy_File (LPCTSTR existing_filename, LPCTSTR new_filename, bool bforce_copy = false);

//
//	Texture routines
//
HBITMAP					Create_DIB_Section (UCHAR **pbits, int width, int height);
HBITMAP					Make_Bitmap_From_Texture (TextureClass &texture, int width, int height);
CString					Get_Texture_Name (TextureClass &texture);
TextureClass *			Load_RC_Texture (LPCTSTR resource_name);
void						Find_Missing_Textures (DynamicVectorClass<StringClass> &list, LPCTSTR filename, int frame_count = 1);  // TheSuperHackers @refactor bobtista 01/01/2025 Use StringClass directly when HAVE_WWVEGAS


// Emitter routines
// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
void Build_Emitter_List (RenderObjClass &render_obj, DynamicVectorClass<StringClass> &list);  // TheSuperHackers @refactor bobtista 01/01/2025 Use StringClass directly when HAVE_WWVEGAS

// Identification routines
// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
bool Is_Aggregate (const char *asset_name);
bool Is_Real_LOD (const char *asset_name);

// Prototype routines
void Rename_Aggregate_Prototype (const char *old_name, const char *new_name);
