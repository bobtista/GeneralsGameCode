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

// FILE: QuotedPrintable.cpp /////////////////////////////////////////////////////////
// Author: Matt Campbell, February 2002
// Description: Quoted-printable encode/decode
////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/QuotedPrintable.h"

#define MAGIC_CHAR '_'

// takes an integer and returns an ASCII representation
static char intToHexDigit(int num)
{
	if (num<0 || num >15) return '\0';
	if (num<10)
	{
		return '0' + num;
	}
	return 'A' + (num-10);
}

// convert an ASCII representation of a hex digit into the digit itself
static int hexDigitToInt(char c)
{
	if (c <= '9' && c >= '0') return (c - '0');
	if (c <= 'f' && c >= 'a') return (c - 'a' + 10);
	if (c <= 'F' && c >= 'A') return (c - 'A' + 10);
	return 0;
}

// Convert unicode strings into ascii quoted-printable strings
// TheSuperHackers @bugfix bobtista 09/06/2026 Encode each WideChar as two
// little-endian bytes (the retail UTF-16 format) instead of walking the raw
// wchar_t buffer two bytes at a time. wchar_t is 4 bytes on macOS/Linux, so the
// old code saw the 0x0000 upper half of the first character as a string
// terminator and truncated every name to its first letter (e.g. "Macbook" -> "M_00").
AsciiString UnicodeStringToQuotedPrintable(UnicodeString original)
{
	static char dest[1024];
	const WideChar *src = original.str();
	int i=0;
	while ( *src != 0 && i < (int)sizeof(dest)-7 )
	{
		const unsigned char bytes[2] = { (unsigned char)((*src)&0xff), (unsigned char)(((*src)>>8)&0xff) };
		for (int b=0; b<2; ++b)
		{
			if (!isalnum(bytes[b]))
			{
				dest[i++] = MAGIC_CHAR;
				dest[i++] = intToHexDigit(bytes[b]>>4);
				dest[i++] = intToHexDigit(bytes[b]&0xf);
			}
			else
			{
				dest[i++] = bytes[b];
			}
		}
		src ++;
	}
	dest[i] = '\0';

	return dest;
}

// Convert ascii strings into ascii quoted-printable strings
AsciiString AsciiStringToQuotedPrintable(AsciiString original)
{
	static char dest[1024];
	const unsigned char *src = reinterpret_cast<const unsigned char *>(original.str());
	int i=0;
	while ( src[0]!='\0' && i<1021 )
	{
		if (!isalnum(*src))
		{
			dest[i++] = MAGIC_CHAR;
			dest[i++] = intToHexDigit((*src)>>4);
			dest[i++] = intToHexDigit((*src)&0xf);
		}
		else
		{
			dest[i++] = *src;
		}
		src ++;
	}
	dest[i] = '\0';

	return dest;
}

// Convert ascii quoted-printable strings into unicode strings
// TheSuperHackers @bugfix bobtista 09/06/2026 Reassemble decoded bytes into
// little-endian 16-bit code units before storing them as WideChar. wchar_t is
// 4 bytes on macOS/Linux, so writing decoded bytes straight into the wchar_t
// buffer (as the old code did) produced garbage on those platforms.
UnicodeString QuotedPrintableToUnicodeString(AsciiString original)
{
	static WideChar dest[1024];
	int di=0;

	const unsigned char *src = reinterpret_cast<const unsigned char *>(original.str());

	unsigned char lowByte=0;
	Bool haveLow=FALSE;
	while (*src && di<1023)
	{
		unsigned char value;
		if (*src == MAGIC_CHAR)
		{
			if (src[1] == '\0')
			{
				// string ends with MAGIC_CHAR
				break;
			}
			value = hexDigitToInt(src[1]);
			src++;
			if (src[1] != '\0')
			{
				value = (value<<4) | hexDigitToInt(src[1]);
				src++;
			}
		}
		else
		{
			value = *src;
		}
		src++;

		if (!haveLow)
		{
			lowByte = value;
			haveLow = TRUE;
		}
		else
		{
			dest[di++] = (WideChar)(lowByte | (value << 8));
			haveLow = FALSE;
		}
	}

	if (haveLow && di<1023)
	{
		dest[di++] = (WideChar)lowByte;
	}

	dest[di] = 0;

	return dest;
}

// Convert ascii quoted-printable strings into ascii strings
AsciiString QuotedPrintableToAsciiString(AsciiString original)
{
	static char dest[1024];
	int i=0;

	unsigned char *c = reinterpret_cast<unsigned char *>(dest);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(original.str());

	while (*src && i<1023)
	{
		if (*src == MAGIC_CHAR)
		{
			if (src[1] == '\0')
			{
				// string ends with MAGIC_CHAR
				break;
			}
			*c = hexDigitToInt(src[1]);
			src++;
			if (src[1] != '\0')
			{
				*c = *c<<4;
				*c = *c | hexDigitToInt(src[1]);
				src++;
			}
		}
		else
		{
			*c = *src;
		}
		src++;
		c++;
	}

	*c = 0;

	return dest;
}

