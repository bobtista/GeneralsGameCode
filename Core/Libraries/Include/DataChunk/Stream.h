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

// Stream.h
// Platform-neutral stream interfaces for DataChunk library
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#pragma once

#include <Utility/stdint_adapter.h>

namespace DataChunk {

//----------------------------------------------------------------------
// DataChunkOutputStream
//----------------------------------------------------------------------
/** Virtual interface for writing chunk data. Platform-neutral replacement
    for engine's OutputStream. */
class DataChunkOutputStream
{
public:
	virtual ~DataChunkOutputStream() {}

	/** Write data to the stream.
	    @param data Pointer to data to write
	    @param numBytes Number of bytes to write
	    @return Number of bytes written (should equal numBytes on success) */
	virtual unsigned int write(const void* data, unsigned int numBytes) = 0;
};

//----------------------------------------------------------------------
// DataChunkInputStream
//----------------------------------------------------------------------
/** Virtual interface for reading chunk data. Platform-neutral replacement
    for engine's ChunkInputStream. */
class DataChunkInputStream
{
public:
	virtual ~DataChunkInputStream() {}

	/** Read data from the stream.
	    @param data Buffer to read into
	    @param numBytes Number of bytes to read
	    @return Number of bytes read */
	virtual unsigned int read(void* data, unsigned int numBytes) = 0;

	/** Get current position in stream.
	    @return Current byte offset from start */
	virtual unsigned int tell() = 0;

	/** Seek to absolute position.
	    @param pos Byte offset from start
	    @return true on success, false on failure */
	virtual bool seek(unsigned int pos) = 0;

	/** Check if at end of stream.
	    @return true if at end, false otherwise */
	virtual bool eof() = 0;
};

} // namespace DataChunk

