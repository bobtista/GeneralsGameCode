/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "always.h"
#include "utf8.h"

#ifdef _WIN32
#include <windows.h>

size_t Utf16Le_To_Utf8_Len(const wchar_t* src, size_t srcLen)
{
	const int bytes = WideCharToMultiByte(CP_UTF8, 0, src, (int)srcLen, nullptr, 0, nullptr, nullptr);
	return (bytes >= 0) ? (size_t)bytes : 0;
}

size_t Utf8_To_Utf16Le_Len(const char* src, size_t srcLen)
{
	const int wchars = MultiByteToWideChar(CP_UTF8, 0, src, (int)srcLen, nullptr, 0);
	return (wchars >= 0) ? (size_t)wchars : 0;
}

size_t Utf16Le_To_Utf8(char* dest, size_t destLen, const wchar_t* src, size_t srcLen)
{
	const int written = WideCharToMultiByte(CP_UTF8, 0, src, (int)srcLen, dest, (int)destLen, nullptr, nullptr);
	WWASSERT(written >= 0 && (size_t)written <= destLen);
	if ((size_t)written < destLen)
	{
		dest[written] = '\0';
	}
	return (size_t)written;
}

size_t Utf8_To_Utf16Le(wchar_t* dest, size_t destLen, const char* src, size_t srcLen)
{
	const int written = MultiByteToWideChar(CP_UTF8, 0, src, (int)srcLen, dest, (int)destLen);
	WWASSERT(written >= 0 && (size_t)written <= destLen);
	if ((size_t)written < destLen)
	{
		dest[written] = L'\0';
	}
	return (size_t)written;
}

#else
#error "Not implemented"
#endif
