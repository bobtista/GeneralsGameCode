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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ThreadUtils.cpp //////////////////////////////////////////////////////
// GameSpy thread utils
// Author: Matthew D. Campbell, July 2002

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "utf8.h"

//-------------------------------------------------------------------------

// TheSuperHackers @refactor bobtista 02/04/2026 Use WWLib UTF-8 functions instead of raw Win32 API calls
std::wstring MultiByteToWideCharSingleLine( const char *orig )
{
	size_t srcLen = strlen(orig);
	size_t len = Get_Unicode_Len(orig, srcLen);
	if (len == 0)
		return std::wstring();
	std::wstring ret;
	ret.resize(len);
	Utf8_To_Unicode(&ret[0], len, orig, srcLen);
	WideChar *c = nullptr;
	do
	{
		c = wcschr(&ret[0], L'\n');
		if (c)
		{
			*c = L' ';
		}
	}
	while ( c != nullptr );
	do
	{
		c = wcschr(&ret[0], L'\r');
		if (c)
		{
			*c = L' ';
		}
	}
	while ( c != nullptr );

	return ret;
}

std::string WideCharStringToMultiByte( const WideChar *orig )
{
	size_t srcLen = wcslen(orig);
	size_t len = Get_Utf8_Len(orig, srcLen);
	if (len == 0)
		return std::string();
	std::string ret;
	ret.resize(len);
	Unicode_To_Utf8(&ret[0], len, orig, srcLen);
	return ret;
}

//-------------------------------------------------------------------------

