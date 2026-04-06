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

#pragma once

#include <stddef.h>
#include <wchar.h>

// Returns the number of bytes in a UTF-8 character based on its lead byte.
// Returns 0 if the lead byte is invalid.
size_t Utf8_Num_Bytes(char lead);

// Returns the number of invalid bytes at the end of the string due to an
// incomplete multi-byte sequence. Returns 0 if the string ends on a complete sequence.
size_t Utf8_Trailing_Invalid_Bytes(const char* str, size_t length);

// Returns true if the null-terminated string is valid UTF-8, false otherwise.
bool Utf8_Validate(const char* str);
bool Utf8_Validate(const char* str, size_t length);

// Returns the number of bytes in the UTF-8 representation of srcLen wide characters
// from src. Returns 0 on failure or if srcLen is 0.
size_t Get_Utf8_Size(const wchar_t* src, size_t srcLen);

// Returns the number of wchar_t elements in the wide character representation of
// srcLen bytes from the UTF-8 string src. Returns 0 on failure or if srcLen is 0.
size_t Get_Unicode_Size(const char* src, size_t srcLen);

// Converts srcLen wide characters from src to UTF-8. destSize is in bytes.
// Does not write a null terminator. Caller must allocate destSize + 1 and
// write the terminator if one is needed. Returns true on success, false on failure.
// On failure, dest[0] is set to '\0'.
bool Unicode_To_Utf8(char* dest, const wchar_t* src, size_t srcLen, size_t destSize);

// Converts srcLen bytes from the UTF-8 string src to wide characters. destSize is in wchar_t elements.
// Does not write a null terminator. Caller must allocate destSize + 1 and
// write the terminator if one is needed. Returns true on success, false on failure.
// On failure, dest[0] is set to L'\0'.
bool Utf8_To_Unicode(wchar_t* dest, const char* src, size_t srcLen, size_t destSize);
