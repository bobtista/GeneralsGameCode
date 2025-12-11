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

/////////////////////////////////////////////////////////////////////
//
//	Globals.cpp
//
//	Module containing global variable initialization.
//
//

// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include StdAfx.h (Windows-only)
#ifdef _WIN32
#include "StdAfx.h"
#endif

#include "Globals.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    // StringClass is defined in WWVegas headers
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
#endif
#include "ViewerAssetMgr.h"

// Main asset manager for the application.
ViewerAssetMgrClass *_TheAssetMgr = nullptr;  // TheSuperHackers @refactor bobtista 01/01/2025 Use nullptr instead of NULL


int g_iDeviceIndex      = -1;//DEFAULT_DEVICEINDEX;
int g_iBitsPerPixel     = -1;//DEFAULT_BITSPERPIX;
int g_iWidth				= 640;
int g_iHeight				= 480;
