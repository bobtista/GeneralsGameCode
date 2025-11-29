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

// TableOfContents.h
// String-to-ID mapping table for DataChunk library
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#pragma once

#include "DataChunk/Types.h"
#include "DataChunk/Stream.h"
#include <string>

namespace DataChunk {

//----------------------------------------------------------------------
// Mapping
//----------------------------------------------------------------------
/** Internal structure for string-to-ID mapping. */
struct Mapping
{
	Mapping* next;
	ChunkString name;
	ChunkUInt id;

	Mapping() : next(NULL), id(0) {}
};

//----------------------------------------------------------------------
// DataChunkTableOfContents
//----------------------------------------------------------------------
/** Manages the string table that maps chunk type names to integer IDs.
    This is written at the start of chunk files and allows chunk types
    to be identified by name rather than hardcoded IDs. */
class DataChunkTableOfContents
{
	Mapping* m_list;
	ChunkInt m_listLength;
	ChunkUInt m_nextID;
	bool m_headerOpened;

	Mapping* findMapping(const ChunkString& name);

public:
	DataChunkTableOfContents();
	~DataChunkTableOfContents();

	/** Get ID for a name (must exist).
	    @param name String name to look up
	    @return Integer ID for the name */
	ChunkUInt getID(const ChunkString& name);

	/** Get name for an ID (must exist).
	    @param id Integer ID to look up
	    @return String name for the ID */
	ChunkString getName(ChunkUInt id);

	/** Allocate or get existing ID for a name.
	    @param name String name
	    @return Integer ID (existing or newly allocated) */
	ChunkUInt allocateID(const ChunkString& name);

	/** Check if table was opened for reading.
	    @return true if table was read from file */
	bool isOpenedForRead() const { return m_headerOpened; }

	/** Write table to output stream.
	    @param out Output stream to write to */
	void write(DataChunkOutputStream& out);

	/** Read table from input stream.
	    @param in Input stream to read from */
	void read(DataChunkInputStream& in);
};

} // namespace DataChunk

