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

#pragma once

#ifdef RTS_HAS_JSON_CHUNK

#include "Common/GameMemory.h"
#include "Common/Dict.h"
#include <nlohmann/json.hpp>

typedef unsigned short DataChunkVersionType;

class JSONChunkInput;

struct JSONChunkInfo
{
	AsciiString label;
	AsciiString parentLabel;
	DataChunkVersionType version;
	Int dataSize;
};

typedef Bool (*JSONChunkParserPtr)( JSONChunkInput &file, JSONChunkInfo *info, void *userData );

class JSONParser : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JSONParser, "JSONParser")
public:
	JSONParser *next;

	JSONChunkParserPtr parser;
	AsciiString label;
	AsciiString parentLabel;
	void *userData;
};
EMPTY_DTOR(JSONParser)

class JSONInputChunk : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JSONInputChunk, "JSONInputChunk")
public:
	JSONInputChunk* next;
	AsciiString label;
	DataChunkVersionType version;
	nlohmann::json* data;
	size_t childIndex;  // Index into _children array for nested chunks
	size_t itemIndex;   // Index into _items array for reading values
};
EMPTY_DTOR(JSONInputChunk)

class JSONChunkInput
{
protected:
	nlohmann::json m_root;
	JSONParser* m_parserList;
	JSONInputChunk* m_chunkStack;
	std::vector<nlohmann::json*> m_chunkArray;
	size_t m_currentChunkIndex;

	void clearChunkStack( void );

public:
	void *m_currentObject;
	void *m_userData;

public:
	JSONChunkInput( const char* jsonData, size_t jsonSize );
	~JSONChunkInput();

	void registerParser( const AsciiString& label, const AsciiString& parentLabel, JSONChunkParserPtr parser, void *userData = NULL );

	Bool parse( void *userData = NULL );

	Bool isValidFileType(void);
	AsciiString openDataChunk(DataChunkVersionType *ver );
	void closeDataChunk( void );

	Bool atEndOfFile( void );
	Bool atEndOfChunk( void );

	void reset( void );

	AsciiString getChunkLabel( void );
	DataChunkVersionType getChunkVersion( void );
	unsigned int getChunkDataSize( void );
	unsigned int getChunkDataSizeLeft( void );

	// Unnamed reads (for backwards compatibility - reads from positional array)
	Real readReal(void);
	Int readInt(void);
	Byte readByte(void);

	AsciiString readAsciiString(void);
	UnicodeString readUnicodeString(void);
	Dict readDict(void);
	void readArrayOfBytes(char *ptr, Int len);

	NameKeyType readNameKey(void);

	// Named reads (reads from object with field names for human-readable JSON)
	Real readReal(const char* name);
	Int readInt(const char* name);
	Byte readByte(const char* name);

	AsciiString readAsciiString(const char* name);
	UnicodeString readUnicodeString(const char* name);
	Dict readDict(const char* name);
	void readArrayOfBytes(const char* name, char *ptr, Int len);

	NameKeyType readNameKey(const char* name);
};

#endif // RTS_HAS_JSON_CHUNK
