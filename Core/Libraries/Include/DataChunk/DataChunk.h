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

// DataChunk.h
// Main public API for DataChunk library
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#pragma once

#include "DataChunk/Types.h"
#include "DataChunk/Stream.h"
#include "DataChunk/TableOfContents.h"
#include <string>

namespace DataChunk {

//----------------------------------------------------------------------
// OutputChunk
//----------------------------------------------------------------------
/** Internal structure for tracking open output chunks. */
struct OutputChunk
{
	OutputChunk* next;
	ChunkUInt id;           // chunk symbol type from table of contents
	ChunkInt filepos;       // position of file at start of data offset

	OutputChunk() : next(NULL), id(0), filepos(0) {}
};

//----------------------------------------------------------------------
// InputChunk
//----------------------------------------------------------------------
/** Internal structure for tracking open input chunks. */
struct InputChunk
{
	InputChunk* next;
	ChunkUInt id;                    // chunk symbol type from table of contents
	DataChunkVersionType version;     // version of data
	ChunkInt chunkStart;              // position of the start of chunk data (past header)
	ChunkInt dataSize;                 // total data size of chunk
	ChunkInt dataLeft;                 // data left to read in this chunk

	InputChunk() : next(NULL), id(0), version(0), chunkStart(0), dataSize(0), dataLeft(0) {}
};

//----------------------------------------------------------------------
// DataChunkInfo
//----------------------------------------------------------------------
/** Information about a chunk being parsed. */
struct DataChunkInfo
{
	ChunkString label;
	ChunkString parentLabel;
	DataChunkVersionType version;
	ChunkInt dataSize;
};

//----------------------------------------------------------------------
// DataChunkParserPtr
//----------------------------------------------------------------------
/** Function pointer type for parsing chunks. */
typedef bool (*DataChunkParserPtr)(class DataChunkInput& file, DataChunkInfo* info, void* userData);

//----------------------------------------------------------------------
// UserParser
//----------------------------------------------------------------------
/** Internal structure for registered parsers. */
struct UserParser
{
	UserParser* next;
	DataChunkParserPtr parser;        // the user parsing function
	ChunkString label;                 // the data chunk label to match
	ChunkString parentLabel;            // the parent chunk's label (the scope)
	void* userData;                     // user data pointer

	UserParser() : next(NULL), parser(NULL), userData(NULL) {}
};

//----------------------------------------------------------------------
// DataChunkOutput
//----------------------------------------------------------------------
/** Class for writing chunk-based data files.
    Platform-neutral replacement for engine's DataChunkOutput. */
class DataChunkOutput
{
	DataChunkOutputStream* m_pOut;        // The actual output stream
	DataChunkTableOfContents m_contents;   // table of contents of data chunk types
	OutputChunk* m_chunkStack;             // current stack of open data chunks

	// Internal buffer for writing (replaces temp file)
	char* m_buffer;
	unsigned int m_bufferSize;
	unsigned int m_bufferPos;

	void growBuffer(unsigned int needed);

public:
	DataChunkOutput(DataChunkOutputStream* pOut);
	~DataChunkOutput();

	/** Open a new data chunk.
	    @param name Chunk type name (will be added to string table)
	    @param ver Version number for this chunk */
	void openDataChunk(const char* name, DataChunkVersionType ver);

	/** Close the current data chunk. */
	void closeDataChunk();

	/** Write a float value. */
	void writeReal(ChunkReal r);

	/** Write an integer value. */
	void writeInt(ChunkInt i);

	/** Write a byte value. */
	void writeByte(ChunkByte b);

	/** Write an ASCII string (length-prefixed, no null terminator). */
	void writeAsciiString(const ChunkString& string);

	/** Write a Unicode string (length-prefixed, no null terminator). */
	void writeUnicodeString(const ChunkWideString& string);

	/** Write an array of bytes. */
	void writeArrayOfBytes(const char* ptr, ChunkInt len);

	/** Allocate or get existing ID for a chunk type name.
	    @param name Chunk type name
	    @return Integer ID (existing or newly allocated) */
	ChunkUInt allocateID(const ChunkString& name);
};

//----------------------------------------------------------------------
// DataChunkInput
//----------------------------------------------------------------------
/** Class for reading chunk-based data files.
    Platform-neutral replacement for engine's DataChunkInput. */
class DataChunkInput
{
	enum { CHUNK_HEADER_BYTES = 4 };  // 2 shorts in chunk file header

	DataChunkInputStream* m_file;              // input file stream
	DataChunkTableOfContents m_contents;      // table of contents of data chunk types
	ChunkInt m_fileposOfFirstChunk;            // seek position of first data chunk
	UserParser* m_parserList;                   // list of all registered parsers
	InputChunk* m_chunkStack;                   // current stack of open data chunks

	void clearChunkStack();
	void decrementDataLeft(ChunkInt size);

public:
	void* m_currentObject;  // user parse routines can use this
	void* m_userData;        // user data hook

	DataChunkInput(DataChunkInputStream* pStream);
	~DataChunkInput();

	/** Register a parser function for data chunks.
	    @param label Chunk label to match
	    @param parentLabel Parent chunk label (or empty for global scope)
	    @param parser Parser function to call
	    @param userData Optional user data to pass to parser */
	void registerParser(const ChunkString& label, const ChunkString& parentLabel,
	                    DataChunkParserPtr parser, void* userData = NULL);

	/** Parse the chunk stream using registered parsers.
	    @param userData Optional user data to pass to parsers
	    @return true on success, false on failure */
	bool parse(void* userData = NULL);

	/** Check if file has valid chunk format.
	    @return true if valid format */
	bool isValidFileType();

	/** Open the next data chunk.
	    @param ver Output parameter for chunk version
	    @return Chunk label name */
	ChunkString openDataChunk(DataChunkVersionType* ver);

	/** Close the current chunk and move to next. */
	void closeDataChunk();

	/** Check if at end of file.
	    @return true if at end */
	bool atEndOfFile();

	/** Check if at end of current chunk.
	    @return true if all data read from chunk */
	bool atEndOfChunk();

	/** Reset to just-opened state. */
	void reset();

	/** Get label of current chunk.
	    @return Chunk label name */
	ChunkString getChunkLabel();

	/** Get version of current chunk.
	    @return Chunk version number */
	DataChunkVersionType getChunkVersion();

	/** Get size of data in current chunk.
	    @return Data size in bytes */
	ChunkUInt getChunkDataSize();

	/** Get size of data left to read in current chunk.
	    @return Remaining data size in bytes */
	ChunkUInt getChunkDataSizeLeft();

	/** Read a float value. */
	ChunkReal readReal();

	/** Read an integer value. */
	ChunkInt readInt();

	/** Read a byte value. */
	ChunkByte readByte();

	/** Read an ASCII string. */
	ChunkString readAsciiString();

	/** Read a Unicode string. */
	ChunkWideString readUnicodeString();

	/** Read an array of bytes.
	    @param ptr Buffer to read into
	    @param len Number of bytes to read */
	void readArrayOfBytes(char* ptr, ChunkInt len);
};

} // namespace DataChunk

