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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/framgrab.h                             $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 1/08/01 10:04a                                              $*
 *                                                                                             *
 *                    $Revision:: 1                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"

#if defined (_MSC_VER)
#pragma warning (push, 3)	// (gth) system headers complain at warning level 4...
#endif

#ifdef _WIN32
#include "windows.h"
#include "windowsx.h"
#include "vfw.h"
#endif

#if defined (_MSC_VER)
#pragma warning (pop)
#endif

// FramGrab.h: interface for the FrameGrabClass class.
//
//////////////////////////////////////////////////////////////////////

class FrameGrabClass
{
public:
	enum MODE {
		RAW,
		AVI
	};

	// depending on which mode you select, it will produce either frames or an AVI.
	FrameGrabClass(const char *filename, MODE mode, int width, int height, int bitdepth, float framerate );

	virtual ~FrameGrabClass();

	void ConvertGrab(void *BitmapPointer);
	void Grab(void *BitmapPointer);

	long * GetBuffer()			{ return Bitmap; }
	float	GetFrameRate()			{ return FrameRate; }

protected:
	const char *Filename;
	float			FrameRate;

	MODE Mode;
	long Counter; // used for incrementing filename cunter, etc.

#ifdef _WIN32
	void GrabAVI(void *BitmapPointer);
	void GrabRawFrame(void *BitmapPointer);

	// avi settings
	PAVIFILE				AVIFile;
	long					*Bitmap;
	PAVISTREAM			Stream;
	AVISTREAMINFO		AVIStreamInfo;
	BITMAPINFOHEADER	BitmapInfoHeader;

	// general purpose cleanup routine
	void CleanupAVI();

	// convert the SR image into AVI byte ordering
	void ConvertFrame(void *BitmapPointer);
#else
	void GrabAVI(void * /*BitmapPointer*/) {}
	void GrabRawFrame(void * /*BitmapPointer*/) {}
	void CleanupAVI() {}
	void ConvertFrame(void * /*BitmapPointer*/) {}

	long *Bitmap = nullptr;
#endif

};

#ifndef _WIN32
inline FrameGrabClass::FrameGrabClass(const char *filename, MODE mode, int width, int height, int bitdepth, float framerate)
    : Filename(filename), FrameRate(framerate), Mode(mode), Counter(0)
{
    (void)bitdepth;
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    Bitmap = (pixel_count > 0) ? new long[pixel_count]{} : nullptr;
}

inline FrameGrabClass::~FrameGrabClass()
{
    delete[] Bitmap;
    Bitmap = nullptr;
}

inline void FrameGrabClass::ConvertGrab(void *BitmapPointer)
{
    (void)BitmapPointer;
}

inline void FrameGrabClass::Grab(void *BitmapPointer)
{
    (void)BitmapPointer;
}
#endif
