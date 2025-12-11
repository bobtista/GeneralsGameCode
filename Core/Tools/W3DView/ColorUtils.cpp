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
//	Utils.cpp
//
//

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include StdAfx.h (Windows-only)
#ifdef _WIN32
#include "StdAfx.h"
// GetSysColor might not be available in Qt builds - add stub if needed
#ifndef GetSysColor
#include <windows.h>  // For GetSysColor
#endif
#else
// Windows macros for COLORREF
#ifndef GetRValue
#define GetRValue(rgb) ((UCHAR)(rgb))
#endif
#ifndef GetGValue
#define GetGValue(rgb) ((UCHAR)(((WORD)(rgb)) >> 8))
#endif
#ifndef GetBValue
#define GetBValue(rgb) ((UCHAR)((rgb)>>16))
#endif
// Windows API stubs
#ifndef GetSysColor
inline COLORREF GetSysColor(int nIndex) { Q_UNUSED(nIndex); return RGB(192, 192, 192); }  // Default gray
#endif
#ifndef COLOR_3DSHADOW
#define COLOR_3DSHADOW 16
#endif
#ifndef COLOR_3DHIGHLIGHT
#define COLOR_3DHIGHLIGHT 20
#endif
#endif
#include "ColorUtils.h"


/////////////////////////////////////////////////////////////////////////////
//
// Draw_Sunken_Rect
//
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile Windows-specific implementations
#ifdef _WIN32
void
Draw_Sunken_Rect
(
	UCHAR *pbits,
	const RECT &rect,
	int scanline_size
)
{
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	// Draw the 4 lines that compose the rectangle
	::Draw_Vert_Line (pbits, rect.left,		rect.top,		height,	::GetSysColor (COLOR_3DSHADOW),		scanline_size);
	::Draw_Vert_Line (pbits, rect.right-1,	rect.top,		height,	::GetSysColor (COLOR_3DHIGHLIGHT),	scanline_size);
	::Draw_Horz_Line (pbits, rect.left,		rect.top,		width,	::GetSysColor (COLOR_3DSHADOW),		scanline_size);
	::Draw_Horz_Line (pbits, rect.left,		rect.bottom-1, width,	::GetSysColor (COLOR_3DHIGHLIGHT),	scanline_size);
	return ;
}


/////////////////////////////////////////////////////////////////////////////
//
// Draw_Raised_Rect
//
void
Draw_Raised_Rect
(
	UCHAR *pbits,
	const RECT &rect,
	int scanline_size
)
{
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	// Draw the 4 lines that compose the rectangle
	::Draw_Vert_Line (pbits, rect.left,		rect.top,		height,	::GetSysColor (COLOR_3DHIGHLIGHT),	scanline_size);
	::Draw_Vert_Line (pbits, rect.right-1,	rect.top,		height,	::GetSysColor (COLOR_3DSHADOW),		scanline_size);
	::Draw_Horz_Line (pbits, rect.left,		rect.top,		width,	::GetSysColor (COLOR_3DHIGHLIGHT),	scanline_size);
	::Draw_Horz_Line (pbits, rect.left,		rect.bottom-1, width,	::GetSysColor (COLOR_3DSHADOW),		scanline_size);
	return ;
}


/////////////////////////////////////////////////////////////////////////////
//
// Frame_Rect
//
void
Frame_Rect
(
	UCHAR *pbits,
	const RECT &rect,
	COLORREF color,
	int scanline_size
)
{
	UCHAR red = GetRValue (color);
	UCHAR green = GetGValue (color);
	UCHAR blue = GetBValue (color);

	int index = (rect.top * scanline_size) + (rect.left * 3);
	int col;
	for (col = rect.left; col < rect.right; col ++) {
		pbits[index++] = blue;
		pbits[index++] = green;
		pbits[index++] = red;
	}

	index = ((rect.bottom-1) * scanline_size) + (rect.left * 3);
	for (col = rect.left; col < rect.right; col ++) {
		pbits[index++] = blue;
		pbits[index++] = green;
		pbits[index++] = red;
	}

	index = (rect.top * scanline_size) + (rect.left * 3);
	int row;
	for (row = rect.top; row < rect.bottom; row ++) {
		pbits[index]		= blue;
		pbits[index + 1]	= green;
		pbits[index + 2]	= red;
		index += scanline_size;
	}

	index = (rect.top * scanline_size) + ((rect.right-1) * 3);
	for (row = rect.top; row < rect.bottom; row ++) {
		pbits[index]		= blue;
		pbits[index + 1]	= green;
		pbits[index + 2]	= red;
		index += scanline_size;
	}

	return ;
}

void
Draw_Vert_Line
(
	UCHAR *pbits,
	int x,
	int y,
	int len,
	COLORREF color,
	int scanline_size
)
{
	UCHAR red = GetRValue (color);
	UCHAR green = GetGValue (color);
	UCHAR blue = GetBValue (color);

	int index = (y * scanline_size) + (x * 3);
	for (int row = y; row < len; row ++) {
		pbits[index]		= blue;
		pbits[index + 1]	= green;
		pbits[index + 2]	= red;
		index += scanline_size;
	}

	return ;
}

void
Draw_Horz_Line
(
	UCHAR *pbits,
	int x,
	int y,
	int len,
	COLORREF color,
	int scanline_size
)
{
	UCHAR red = GetRValue (color);
	UCHAR green = GetGValue (color);
	UCHAR blue = GetBValue (color);

	int index = (y * scanline_size) + (x * 3);
	for (int col = x; col < len; col ++) {
		pbits[index++] = blue;
		pbits[index++] = green;
		pbits[index++] = red;
	}

	return ;
}
#else
// Stubs for non-Windows (implementations are in header)
#endif
