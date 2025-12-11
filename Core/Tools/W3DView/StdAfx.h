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

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 StdAfx.h is MFC-only - stub for Qt builds
// Check if we're building for Qt by checking if Qt headers are available
#if defined(QT_VERSION) || defined(QT_WIDGETS_LIB) || defined(QT_GUI_LIB) || defined(QT_CORE_LIB)
// Qt build - stub always.h and MFC headers
// always.h is a game engine header - not available in Core build
// MFC headers not needed for Qt build
#else
// MFC build - include full headers
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#include "always.h"

// TheSuperHackers @build jlallas384 05/04/2025 Prevent afxwin.h from loading d3d9types.h, colliding with our own DirectX library.
#define _d3d9TYPES_H_

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.
