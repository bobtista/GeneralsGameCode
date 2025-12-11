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
#else
// Define Windows types if Qt isn't available
typedef void* HWND;
typedef void* HBITMAP;
#endif

// TheSuperHackers @refactor bobtista 01/01/2025 Use GameEngineStubs for all platforms (Core build)
#include "GameEngineStubs.h"

// Vector template classes (needed for Core build)
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

// Windows type stubs for Core build
#ifndef LPCTSTR
typedef const char* LPCTSTR;
#endif
#ifndef CString
#include <QString>  // For QString
typedef QString CString;  // Use QString as CString replacement
#endif
#ifndef UCHAR
typedef unsigned char UCHAR;
#endif
#ifndef LPFILETIME
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
#ifndef LPTSTR
typedef char* LPTSTR;
#endif
#include <cstring>  // For strlen, strcat

inline void Delimit_Path (LPTSTR path)
{
	size_t len = strlen(path);
	if (len > 0 && path[len - 1] != '\\') {
		strcat(path, "\\");
	}
}

inline void Delimit_Path (CString &path)
{
	QString qpath = path;
	if (qpath.length() > 0 && qpath[qpath.length() - 1] != '\\') {
		qpath += "\\";
		path = qpath;
	}
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
void						Find_Missing_Textures (DynamicVectorClass<CString> &list, LPCTSTR filename, int frame_count = 1);


// Emitter routines
// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
void Build_Emitter_List (RenderObjClass &render_obj, DynamicVectorClass<CString> &list);

// Identification routines
// TheSuperHackers @refactor bobtista 01/01/2025 Use stubs for Core build
bool Is_Aggregate (const char *asset_name);
bool Is_Real_LOD (const char *asset_name);

// Prototype routines
void Rename_Aggregate_Prototype (const char *old_name, const char *new_name);
