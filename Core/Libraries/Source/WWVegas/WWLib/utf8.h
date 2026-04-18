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

// NOTE: The current implementation is Windows-only and treats wchar_t as UTF-16LE.
// On non-Windows platforms wchar_t is typically UTF-32, so a future cross-platform
// implementation should migrate the wide parameters to uint16_t / char16_t.

// Returns the number of bytes in a UTF-8 character based on its lead byte.
// Returns 0 if the lead byte is invalid.
size_t Utf8_Num_Bytes(char lead);

// Returns the number of invalid bytes at the end of the string due to an
// incomplete multi-byte sequence. Returns 0 if the string ends on a complete sequence.
size_t Utf8_Trailing_Invalid_Bytes(const char* str, size_t length);

// Returns true if the null-terminated string is valid UTF-8, false otherwise.
bool Utf8_Validate(const char* str);
bool Utf8_Validate(const char* str, size_t length);

// Returns the number of bytes needed for the UTF-8 representation of srcLen UTF-16LE
// characters from src, not counting a null terminator. Returns 0 on failure or if srcLen is 0.
size_t Utf16Le_To_Utf8_Len(const wchar_t* src, size_t srcLen);

// Returns the number of UTF-16LE elements needed for the UTF-16LE representation
// of srcLen bytes from the UTF-8 string src, not counting a null terminator.
// Returns 0 on failure or if srcLen is 0.
size_t Utf8_To_Utf16Le_Len(const char* src, size_t srcLen);

// Converts srcLen UTF-16LE characters from src to UTF-8.
// destLen is the destination buffer capacity in bytes. Caller must ensure destLen is large enough
// by querying Utf16Le_To_Utf8_Len first. Writes a null terminator if room remains, otherwise not.
// Returns the number of bytes written on success, or 0 on failure.
// On failure, dest[0] is set to '\0' if destLen > 0.
size_t Utf16Le_To_Utf8(char* dest, size_t destLen, const wchar_t* src, size_t srcLen);

// Converts srcLen bytes from the UTF-8 string src to UTF-16LE characters.
// destLen is the destination buffer capacity in wchar_t elements. Caller must ensure destLen is
// large enough by querying Utf8_To_Utf16Le_Len first. Writes a null terminator if room remains,
// otherwise not. Returns the number of wchar_t elements written on success, or 0 on failure.
// On failure, dest[0] is set to L'\0' if destLen > 0.
size_t Utf8_To_Utf16Le(wchar_t* dest, size_t destLen, const char* src, size_t srcLen);
