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
#include "Common/DataChunk.h"
#include <nlohmann/json.hpp>
#include <string>

class JSONOutputChunk : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(JSONOutputChunk, "JSONOutputChunk")
public:
	JSONOutputChunk* next;
	AsciiString label;
	nlohmann::json* data;
};
EMPTY_DTOR(JSONOutputChunk)

class JSONChunkOutput : public ChunkOutputStream
{
protected:
	nlohmann::json m_root;
	nlohmann::json m_toc;
	JSONOutputChunk* m_chunkStack;
	UnsignedInt m_nextID;

public:
	JSONChunkOutput( void );
	~JSONChunkOutput();

	void openDataChunk( const char *name, DataChunkVersionType ver );
	void closeDataChunk( void );

	// Unnamed writes (for backwards compatibility - writes to positional array)
	void writeReal(Real r);
	void writeInt(Int i);
	void writeByte(Byte b);
	void writeAsciiString(const AsciiString& string);
	void writeUnicodeString(UnicodeString string);
	void writeArrayOfBytes(char *ptr, Int len);
	void writeDict(const Dict& d);
	void writeNameKey(const NameKeyType key);

	// Named writes (writes to object with field names for human-readable JSON)
	void writeReal(const char* name, Real r);
	void writeInt(const char* name, Int i);
	void writeByte(const char* name, Byte b);
	void writeAsciiString(const char* name, const AsciiString& string);
	void writeUnicodeString(const char* name, UnicodeString string);
	void writeArrayOfBytes(const char* name, char *ptr, Int len);
	void writeDict(const char* name, const Dict& d);
	void writeNameKey(const char* name, const NameKeyType key);

	// Binary-only writes (no-op for JSON output)
	void writeBinaryOnlyInt(Int i) { (void)i; }
	void writeBinaryOnlyNameKey(NameKeyType key) { (void)key; }

	// JSON-only writes (writes to JSON field)
	void writeJSONOnlyString(const char* name, const AsciiString& value);
	void writeJSONOnlyInt(const char* name, Int i);
	void writeJSONOnlyBool(const char* name, Bool b);

	// Type-converting writes (JSON writes string to _items)
	void writeParameterType(Int type, const char* typeName);
	void writeBoolAsByte(Bool b);
	void writeBoolAsByte(const char* name, Bool b);
	void writeBoolAsInt(Bool b);  // JSON writes true/false to _items

	// Enum writes (JSON writes string to _items, ignores int value)
	void writeEnumAsInt(Int value, const char* enumStr);

	std::string getJSONString( void );
};

#endif // RTS_HAS_JSON_CHUNK
