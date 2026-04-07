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

// Returns the number of bytes needed for the UTF-8 representation of srcLen wide
// characters from src, not counting a null terminator. Returns 0 on failure or if srcLen is 0.
size_t Get_Utf8_Len(const wchar_t* src, size_t srcLen);

// Returns the number of wchar_t elements needed for the wide character representation
// of srcLen bytes from the UTF-8 string src, not counting a null terminator.
// Returns 0 on failure or if srcLen is 0.
size_t Get_Unicode_Len(const char* src, size_t srcLen);

// Converts srcLen wide characters from src to UTF-8.
// destLen is the destination buffer capacity in bytes, not counting a null terminator.
// Returns the number of bytes written on success, or 0 on failure.
// Writes a null terminator if destLen > bytes written. Does not write one if destLen
// equals bytes written (exact fit). On failure, dest[0] is set to '\0' if destLen > 0.
size_t Unicode_To_Utf8(char* dest, size_t destLen, const wchar_t* src, size_t srcLen);

// Converts srcLen bytes from the UTF-8 string src to wide characters.
// destLen is the destination buffer capacity in wchar_t elements, not counting a null terminator.
// Returns the number of wchar_t elements written on success, or 0 on failure.
// Writes a null terminator if destLen > elements written. Does not write one if destLen
// equals elements written (exact fit). On failure, dest[0] is set to L'\0' if destLen > 0.
size_t Utf8_To_Unicode(wchar_t* dest, size_t destLen, const char* src, size_t srcLen);
