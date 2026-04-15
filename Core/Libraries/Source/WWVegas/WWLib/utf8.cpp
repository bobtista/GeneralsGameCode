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

#include <string.h>

#ifdef _WIN32
#include <windows.h>

static bool Is_Trail_Byte(char c)
{
	return (c & 0xC0) == 0x80;
}

size_t Utf8_Num_Bytes(char lead)
{
	if ((lead & 0x80) == 0x00) return 1;
	if ((lead & 0xE0) == 0xC0) return 2;
	if ((lead & 0xF0) == 0xE0) return 3;
	if ((lead & 0xF8) == 0xF0) return 4;
	return 0;
}

size_t Utf8_Trailing_Invalid_Bytes(const char* str, size_t length)
{
	if (length == 0)
		return 0;

	size_t i = length;
	while (i > 0 && Is_Trail_Byte(str[i - 1]))
		--i;

	if (i == 0)
		return length;

	size_t claimed = Utf8_Num_Bytes(str[i - 1]);
	size_t actual = length - (i - 1);

	if (claimed == 0 || claimed != actual)
		return actual;

	return 0;
}

bool Utf8_Validate(const char* str)
{
	return Utf8_Validate(str, strlen(str));
}

bool Utf8_Validate(const char* str, size_t length)
{
	const unsigned char* s = (const unsigned char*)str;
	size_t i = 0;
	while (i < length)
	{
		size_t bytes = Utf8_Num_Bytes(str[i]);
		if (bytes == 0)
			return false;
		if (i + bytes > length)
			return false;
		for (size_t j = 1; j < bytes; ++j)
		{
			if (!Is_Trail_Byte(str[i + j]))
				return false;
		}
		// Reject overlong encodings per RFC 3629
		if (bytes == 2 && s[i] < 0xC2)
			return false;
		if (bytes == 3 && s[i] == 0xE0 && s[i + 1] < 0xA0)
			return false;
		if (bytes == 4 && s[i] == 0xF0 && s[i + 1] < 0x90)
			return false;
		// Reject codepoints above U+10FFFF
		if (bytes == 4 && s[i] > 0xF4)
			return false;
		if (bytes == 4 && s[i] == 0xF4 && s[i + 1] > 0x8F)
			return false;
		i += bytes;
	}
	return true;
}

size_t Utf16Le_To_Utf8_Len(const wchar_t* src, size_t srcLen)
{
	int bytes = WideCharToMultiByte(CP_UTF8, 0, src, (int)srcLen, nullptr, 0, nullptr, nullptr);
	return (bytes > 0) ? (size_t)bytes : 0;
}

size_t Utf8_To_Utf16Le_Len(const char* src, size_t srcLen)
{
	int wchars = MultiByteToWideChar(CP_UTF8, 0, src, (int)srcLen, nullptr, 0);
	return (wchars > 0) ? (size_t)wchars : 0;
}

size_t Utf16Le_To_Utf8(char* dest, size_t destLen, const wchar_t* src, size_t srcLen)
{
	int required = WideCharToMultiByte(CP_UTF8, 0, src, (int)srcLen, nullptr, 0, nullptr, nullptr);
	if (required <= 0)
	{
		if (destLen > 0)
			dest[0] = '\0';
		return 0;
	}
	if ((size_t)required > destLen)
	{
		if (destLen > 0)
			dest[0] = '\0';
		return (size_t)required;
	}
	int written = WideCharToMultiByte(CP_UTF8, 0, src, (int)srcLen, dest, (int)destLen, nullptr, nullptr);
	if (written == 0)
	{
		if (destLen > 0)
			dest[0] = '\0';
		return 0;
	}
	if ((size_t)written < destLen)
		dest[written] = '\0';
	return (size_t)written;
}

size_t Utf8_To_Utf16Le(wchar_t* dest, size_t destLen, const char* src, size_t srcLen)
{
	int required = MultiByteToWideChar(CP_UTF8, 0, src, (int)srcLen, nullptr, 0);
	if (required <= 0)
	{
		if (destLen > 0)
			dest[0] = L'\0';
		return 0;
	}
	if ((size_t)required > destLen)
	{
		if (destLen > 0)
			dest[0] = L'\0';
		return (size_t)required;
	}
	int written = MultiByteToWideChar(CP_UTF8, 0, src, (int)srcLen, dest, (int)destLen);
	if (written == 0)
	{
		if (destLen > 0)
			dest[0] = L'\0';
		return 0;
	}
	if ((size_t)written < destLen)
		dest[written] = L'\0';
	return (size_t)written;
}

#else
#error "Not implemented"
#endif
