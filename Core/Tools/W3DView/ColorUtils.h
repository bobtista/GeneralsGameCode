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

//////////////////////////////////////////////////////////////////////////////////////////
//
//	ColorUtils.h
//
//

#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 Define Windows types for Qt builds
// Include Qt headers early to get HWND definition
#ifdef QT_VERSION
#include <QtGui/qwindowdefs_win.h>  // For HWND on Windows
#endif
// Include windows.h if available to get RECT, HWND, HINSTANCE definitions
#ifdef _WIN32
#ifndef QT_VERSION
#include <windows.h>  // For RECT, HWND, HINSTANCE on Windows (non-Qt builds)
#endif
#endif
// Define types that might not be available in Qt builds
#ifndef UCHAR
typedef unsigned char UCHAR;
#endif
// RECT is defined by windows.h - only define if not already defined
#if !defined(RECT) && !defined(_WINDEF_) && !defined(_WINUSER_)
struct RECT { long left, top, right, bottom; };
#endif
#ifndef COLORREF
typedef unsigned long COLORREF;
#endif
#ifndef BOOL
typedef int BOOL;
#endif
// HWND is already defined by Qt's qwindowdefs_win.h or windows.h - don't redefine it
#if !defined(HWND) && !defined(_WINDEF_) && !defined(_WINUSER_)
typedef void* HWND;
#endif
// HINSTANCE is defined by windows.h - only define if not already defined
#if !defined(HINSTANCE) && !defined(_WINDEF_) && !defined(_WINUSER_)
typedef void* HINSTANCE;
#endif

typedef void (*WWCTRL_COLORCALLBACK)(int,int,int,void*);

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////
void		Frame_Rect (UCHAR *pbits, const RECT &rect, COLORREF color, int scanline_size);
void		Draw_Vert_Line (UCHAR *pbits, int x, int y, int len, COLORREF color, int scanline_size);
void		Draw_Horz_Line (UCHAR *pbits, int x, int y, int len, COLORREF color, int scanline_size);
void		Draw_Sunken_Rect (UCHAR *pbits, const RECT &rect, int scanline_size);
void		Draw_Raised_Rect (UCHAR *pbits, const RECT &rect, int scanline_size);
BOOL		Show_Color_Picker (int *red, int *green, int *blue);
HWND		Create_Color_Picker_Form (HWND parent, int red, int green, int blue);
BOOL		Get_Form_Color (HWND form_wnd, int *red, int *green, int *blue);
BOOL		Set_Form_Color (HWND form_wnd, int red, int green, int blue);
BOOL		Set_Form_Original_Color (HWND form_wnd, int red, int green, int blue);
BOOL		Set_Update_Callback (HWND form_wnd, WWCTRL_COLORCALLBACK callback, void *arg=nullptr);
void		RegisterColorPicker (HINSTANCE hinst);
void		RegisterColorBar (HINSTANCE hinst);

// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementations for Qt builds
#ifdef QT_VERSION
// Qt build - provide stub implementations
inline void Frame_Rect (UCHAR *pbits, const RECT &rect, COLORREF color, int scanline_size) { Q_UNUSED(pbits); Q_UNUSED(rect); Q_UNUSED(color); Q_UNUSED(scanline_size); }
inline void Draw_Vert_Line (UCHAR *pbits, int x, int y, int len, COLORREF color, int scanline_size) { Q_UNUSED(pbits); Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(len); Q_UNUSED(color); Q_UNUSED(scanline_size); }
inline void Draw_Horz_Line (UCHAR *pbits, int x, int y, int len, COLORREF color, int scanline_size) { Q_UNUSED(pbits); Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(len); Q_UNUSED(color); Q_UNUSED(scanline_size); }
inline BOOL Show_Color_Picker (int *red, int *green, int *blue) { Q_UNUSED(red); Q_UNUSED(green); Q_UNUSED(blue); return 0; }
inline HWND Create_Color_Picker_Form (HWND parent, int red, int green, int blue) { Q_UNUSED(parent); Q_UNUSED(red); Q_UNUSED(green); Q_UNUSED(blue); return nullptr; }
inline BOOL Get_Form_Color (HWND form_wnd, int *red, int *green, int *blue) { Q_UNUSED(form_wnd); Q_UNUSED(red); Q_UNUSED(green); Q_UNUSED(blue); return 0; }
inline BOOL Set_Form_Color (HWND form_wnd, int red, int green, int blue) { Q_UNUSED(form_wnd); Q_UNUSED(red); Q_UNUSED(green); Q_UNUSED(blue); return 0; }
inline BOOL Set_Form_Original_Color (HWND form_wnd, int red, int green, int blue) { Q_UNUSED(form_wnd); Q_UNUSED(red); Q_UNUSED(green); Q_UNUSED(blue); return 0; }
inline BOOL Set_Update_Callback (HWND form_wnd, WWCTRL_COLORCALLBACK callback, void *arg=nullptr) { Q_UNUSED(form_wnd); Q_UNUSED(callback); Q_UNUSED(arg); return 0; }
inline void RegisterColorPicker (HINSTANCE hinst) { Q_UNUSED(hinst); }
inline void RegisterColorBar (HINSTANCE hinst) { Q_UNUSED(hinst); }
#endif

#if 0  // Disabled - using unified definitions above
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific types
#ifdef _WIN32
#else
// Stubs for non-Windows
typedef void (*WWCTRL_COLORCALLBACK)(int,int,int,void*);

void Frame_Rect (UCHAR *pbits, const RECT &rect, COLORREF color, int scanline_size) {}
void Draw_Vert_Line (UCHAR *pbits, int x, int y, int len, COLORREF color, int scanline_size) {}
void Draw_Horz_Line (UCHAR *pbits, int x, int y, int len, COLORREF color, int scanline_size) {}
// Note: Draw_Sunken_Rect and Draw_Raised_Rect are implemented in ColorUtils.cpp (conditionally compiled)
BOOL Show_Color_Picker (int *red, int *green, int *blue) { return 0; }
HWND Create_Color_Picker_Form (HWND parent, int red, int green, int blue) { return nullptr; }
BOOL Get_Form_Color (HWND form_wnd, int *red, int *green, int *blue) { return 0; }
BOOL Set_Form_Color (HWND form_wnd, int red, int green, int blue) { return 0; }
BOOL Set_Form_Original_Color (HWND form_wnd, int red, int green, int blue) { return 0; }
BOOL Set_Update_Callback (HWND form_wnd, WWCTRL_COLORCALLBACK callback, void *arg=nullptr) { return 0; }
void RegisterColorPicker (HINSTANCE hinst) {}
void RegisterColorBar (HINSTANCE hinst) {}
#endif
#endif  // End of disabled block (#if 0)
