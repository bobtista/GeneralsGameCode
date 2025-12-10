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

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
#include "Vector.h"
#else
// Stub for non-Windows
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
#endif

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

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific functions
#ifdef _WIN32
__inline void Delimit_Path (LPTSTR path)
{
	if (::lstrlen (path) > 0 && path[::lstrlen (path) - 1] != '\\') {
		::lstrcat (path, "\\");
	}
	return ;
}

__inline void Delimit_Path (CString &path)
{
	if (path[::lstrlen (path) - 1] != '\\') {
		path += CString ("\\");
	}
	return ;
}
#else
// Stubs for non-Windows
inline void Delimit_Path (char* path) {
	if (strlen(path) > 0 && path[strlen(path) - 1] != '/') {
		strcat(path, "/");
	}
}
#endif


// Forward declarations
class TextureClass;
class CGraphicView;


/////////////////////////////////////////////////////////////////////////////
//
// Prototypes
//
class CW3DViewDoc *	GetCurrentDocument (void);
CGraphicView *			Get_Graphic_View (void);
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific functions
#ifdef _WIN32
void						Paint_Gradient (HWND hWnd, BYTE baseRed, BYTE baseGreen, BYTE baseBlue);
#else
// Stubs for non-Windows
typedef void* HWND;
typedef unsigned char BYTE;
void Paint_Gradient (HWND hWnd, BYTE baseRed, BYTE baseGreen, BYTE baseBlue);
#endif

//
// Dialog routines
//
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific functions
#ifdef _WIN32
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
#else
// Stubs for non-Windows - these functions are Windows-only and not used on Mac
// Declarations only (implementations are Windows-only in Utils.cpp)
typedef unsigned int UINT;
void SetDlgItemFloat (HWND hdlg, UINT child_id, float value);
float GetDlgItemFloat (HWND hdlg, UINT child_id);
void SetWindowFloat (HWND hwnd, float value);
float GetWindowFloat (HWND hwnd);
void Initialize_Spinner (void* ctrl, float pos = 0, float min = 0, float max = 1);
void Update_Spinner_Buddy (void* ctrl);
void Update_Spinner_Buddy (HWND hspinner, int delta);
void Enable_Dialog_Controls (HWND dlg, bool onoff);

// String manipulation - not used on Mac
std::string Get_Filename_From_Path (const char* path);
std::string Strip_Filename_From_Path (const char* path);
std::string Asset_Name_From_Filename (const char* filename);
std::string Filename_From_Asset_Name (const char* asset_name);

// File routines - not used on Mac
bool Get_File_Time (const char* path, void* pcreation_time, void* paccess_time = NULL, void* pwrite_time = NULL);
bool Are_Glide_Drivers_Acceptable (void);
bool Copy_File (const char* existing_filename, const char* new_filename, bool bforce_copy = false);

// Texture routines
void* Create_DIB_Section (void** pbits, int width, int height);
void* Make_Bitmap_From_Texture (TextureClass &texture, int width, int height);
std::string Get_Texture_Name (TextureClass &texture);
TextureClass * Load_RC_Texture (const char* resource_name);
void Find_Missing_Textures (void* list, const char* filename, int frame_count = 1);
#endif


// Emitter routines
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific functions
#ifdef _WIN32
void						Build_Emitter_List (RenderObjClass &render_obj, DynamicVectorClass<CString> &list);
#else
void Build_Emitter_List (RenderObjClass &render_obj, void* list);  // Declaration only, not used on Mac
#endif

// Identification routines
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific functions
#ifdef _WIN32
bool						Is_Aggregate (const char *asset_name);
bool						Is_Real_LOD (const char *asset_name);

// Prototype routines
void						Rename_Aggregate_Prototype (const char *old_name, const char *new_name);
#else
// Stubs for non-Windows (not used on Mac)
bool Is_Aggregate (const char *asset_name);
bool Is_Real_LOD (const char *asset_name);
void Rename_Aggregate_Prototype (const char *old_name, const char *new_name);
#endif
