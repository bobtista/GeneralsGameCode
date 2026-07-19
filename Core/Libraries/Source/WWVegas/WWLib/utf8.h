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

// UTF-8 <-> wide-character transcoding, hand-rolled per RFC 3629, using no platform text APIs.
// The wide side is wchar_t, whose width is platform-dependent: on Windows it is a 16-bit UTF-16
// code unit (astral codepoints use surrogate pairs); on most other platforms it is a 32-bit
// UTF-32 codepoint. Both are handled transparently based on the width of wchar_t.

// Returns true if the length bytes at str are well-formed UTF-8 per RFC 3629. Rejects overlong
// encodings, codepoints above U+10FFFF, and UTF-16 surrogate codepoints. An empty range is valid.
bool Utf8_Validate(const char* str, size_t length);

// Returns the number of UTF-8 bytes needed for the UTF-8 representation of srcLen wide characters
// from src, not counting a null terminator. Returns 0 if srcLen is 0.
size_t Utf16Le_To_Utf8_Len(const wchar_t* src, size_t srcLen);

// Returns the number of wide characters needed for the wide representation of srcLen bytes from the
// UTF-8 string src, not counting a null terminator. Returns 0 on invalid UTF-8 or if srcLen is 0.
size_t Utf8_To_Utf16Le_Len(const char* src, size_t srcLen);

// Converts srcLen wide characters from src to UTF-8.
// destLen is the destination buffer capacity in bytes. Caller must ensure destLen is large enough
// by querying Utf16Le_To_Utf8_Len first. Writes a null terminator if room remains, otherwise not.
// Returns the number of bytes written, or 0 if srcLen is 0.
size_t Utf16Le_To_Utf8(char* dest, size_t destLen, const wchar_t* src, size_t srcLen);

// Converts srcLen bytes from the UTF-8 string src to wide characters.
// destLen is the destination buffer capacity in wide characters. Caller must ensure destLen is
// large enough by querying Utf8_To_Utf16Le_Len first. Writes a null terminator if room remains,
// otherwise not. Returns the number of wide characters written, or 0 on invalid UTF-8 or if srcLen
// is 0. On failure, dest[0] is set to L'\0' if destLen > 0.
size_t Utf8_To_Utf16Le(wchar_t* dest, size_t destLen, const char* src, size_t srcLen);
