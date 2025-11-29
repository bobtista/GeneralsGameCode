/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 TheSuperHackers
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

// StreamAdapters.h
// FILE* stream adapters for DataChunk library (for use in tools)
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#pragma once

#include "DataChunk/Stream.h"
#include <cstdio>

namespace DataChunk {

//----------------------------------------------------------------------
// FileOutputStream
//----------------------------------------------------------------------
/** Adapter that wraps a FILE* for writing chunk data.
    Use this in standalone tools that work with FILE* streams. */
class FileOutputStream : public DataChunkOutputStream
{
	FILE* m_file;

public:
	FileOutputStream(FILE* file) : m_file(file) {}

	virtual unsigned int write(const void* data, unsigned int numBytes)
	{
		if (m_file)
		{
			return (unsigned int)fwrite(data, 1, numBytes, m_file);
		}
		return 0;
	}
};

//----------------------------------------------------------------------
// FileInputStream
//----------------------------------------------------------------------
/** Adapter that wraps a FILE* for reading chunk data.
    Use this in standalone tools that work with FILE* streams. */
class FileInputStream : public DataChunkInputStream
{
	FILE* m_file;

public:
	FileInputStream(FILE* file) : m_file(file) {}

	virtual unsigned int read(void* data, unsigned int numBytes)
	{
		if (m_file)
		{
			return (unsigned int)fread(data, 1, numBytes, m_file);
		}
		return 0;
	}

	virtual unsigned int tell()
	{
		if (m_file)
		{
			return (unsigned int)ftell(m_file);
		}
		return 0;
	}

	virtual bool seek(unsigned int pos)
	{
		if (m_file)
		{
			return fseek(m_file, (long)pos, SEEK_SET) == 0;
		}
		return false;
	}

	virtual bool eof()
	{
		if (m_file)
		{
			return feof(m_file) != 0;
		}
		return true;
	}
};

} // namespace DataChunk

