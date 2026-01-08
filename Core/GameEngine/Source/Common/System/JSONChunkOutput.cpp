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

#include "PreRTS.h"

#include "Common/JSONChunkOutput.h"
#include "Common/NameKeyGenerator.h"

JSONChunkOutput::JSONChunkOutput( void ) :
	m_chunkStack(NULL),
	m_nextID(1)
{
	m_root["chunks"] = nlohmann::json::array();
	m_toc = nlohmann::json::object();
}

JSONChunkOutput::~JSONChunkOutput()
{
	while (m_chunkStack) {
		JSONOutputChunk *c = m_chunkStack;
		m_chunkStack = m_chunkStack->next;
		deleteInstance(c);
	}

	if (!m_toc.empty()) {
		m_root["toc"] = m_toc;
	}
}

void JSONChunkOutput::openDataChunk( const char *name, DataChunkVersionType ver )
{
	AsciiString nameStr(name);
	UnsignedInt id = m_nextID++;
	m_toc[std::to_string(id)] = nameStr.str();

	nlohmann::json chunk;
	chunk["label"] = nameStr.str();
	chunk["version"] = ver;
	// No "data" wrapper - fields go directly in chunk object

	// If we're inside a chunk (nested), add to parent's _children array
	if (m_chunkStack && m_chunkStack->data) {
		// Nested chunk - add to parent's _children array
		if (!m_chunkStack->data->contains("_children")) {
			(*m_chunkStack->data)["_children"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_children"].push_back(chunk);
	} else {
		// Top-level chunk - add to root chunks array
		m_root["chunks"].push_back(chunk);
	}

	JSONOutputChunk *c = newInstance(JSONOutputChunk);
	c->label = nameStr;
	// Point directly to the chunk object (no "data" wrapper)
	if (m_chunkStack && m_chunkStack->data) {
		c->data = &((*m_chunkStack->data)["_children"].back());
	} else {
		c->data = &(m_root["chunks"].back());
	}
	c->next = m_chunkStack;
	m_chunkStack = c;
}

void JSONChunkOutput::closeDataChunk( void )
{
	if (m_chunkStack == NULL) {
		return;
	}

	JSONOutputChunk *c = m_chunkStack;
	m_chunkStack = m_chunkStack->next;
	deleteInstance(c);
}

void JSONChunkOutput::writeReal( Real r )
{
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(r);
	}
}

void JSONChunkOutput::writeInt( Int i )
{
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(i);
	}
}

void JSONChunkOutput::writeByte( Byte b )
{
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(b);
	}
}

void JSONChunkOutput::writeArrayOfBytes(char *ptr, Int len)
{
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		std::string bytesStr(ptr, len);
		(*m_chunkStack->data)["_items"].push_back(bytesStr);
	}
}

void JSONChunkOutput::writeAsciiString( const AsciiString& theString )
{
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(theString.str());
	}
}

void JSONChunkOutput::writeUnicodeString( UnicodeString theString )
{
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(theString.str());
	}
}

void JSONChunkOutput::writeNameKey( const NameKeyType key )
{
	// For unnamed writes, store as encoded int for backwards compat
	AsciiString kname = TheNameKeyGenerator->keyToName(key);
	UnsignedInt id = m_nextID++;
	m_toc[std::to_string(id)] = kname.str();
	Int keyAndType = id;
	keyAndType <<= 8;
	Dict::DataType t = Dict::DICT_ASCIISTRING;
	keyAndType |= (t & 0xff);
	writeInt(keyAndType);
}

void JSONChunkOutput::writeDict( const Dict& d )
{
	UnsignedShort len = d.getPairCount();
	writeInt(len);
	for (int i = 0; i < len; i++)
	{
		NameKeyType k = d.getNthKey(i);
		AsciiString kname = TheNameKeyGenerator->keyToName(k);

		UnsignedInt id = m_nextID++;
		m_toc[std::to_string(id)] = kname.str();
		Int keyAndType = id;
		keyAndType <<= 8;
		Dict::DataType t = d.getNthType(i);
		keyAndType |= (t & 0xff);
		writeInt(keyAndType);

		switch(t)
		{
			case Dict::DICT_BOOL:
				writeByte(d.getNthBool(i)?1:0);
				break;
			case Dict::DICT_INT:
				writeInt(d.getNthInt(i));
				break;
			case Dict::DICT_REAL:
				writeReal(d.getNthReal(i));
				break;
			case Dict::DICT_ASCIISTRING:
				writeAsciiString(d.getNthAsciiString(i));
				break;
			case Dict::DICT_UNICODESTRING:
				writeUnicodeString(d.getNthUnicodeString(i));
				break;
			default:
				DEBUG_CRASH(("impossible"));
				break;
		}
	}
}

std::string JSONChunkOutput::getJSONString( void )
{
	if (!m_toc.empty()) {
		m_root["toc"] = m_toc;
	}
	return m_root.dump(2);
}

// Named write implementations
void JSONChunkOutput::writeReal(const char* name, Real r)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = r;
	}
}

void JSONChunkOutput::writeInt(const char* name, Int i)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = i;
	}
}

void JSONChunkOutput::writeByte(const char* name, Byte b)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = b;
	}
}

void JSONChunkOutput::writeAsciiString(const char* name, const AsciiString& theString)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = theString.str();
	}
}

void JSONChunkOutput::writeUnicodeString(const char* name, UnicodeString theString)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = theString.str();
	}
}

void JSONChunkOutput::writeArrayOfBytes(const char* name, char *ptr, Int len)
{
	if (m_chunkStack && m_chunkStack->data) {
		std::string bytesStr(ptr, len);
		(*m_chunkStack->data)[name] = bytesStr;
	}
}

void JSONChunkOutput::writeNameKey(const char* name, const NameKeyType key)
{
	AsciiString kname = TheNameKeyGenerator->keyToName(key);
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = kname.str();
	}
}

void JSONChunkOutput::writeDict(const char* name, const Dict& d)
{
	if (!m_chunkStack || !m_chunkStack->data) {
		return;
	}

	nlohmann::json dictObj = nlohmann::json::object();
	UnsignedShort len = d.getPairCount();

	for (int i = 0; i < len; i++)
	{
		NameKeyType k = d.getNthKey(i);
		AsciiString kname = TheNameKeyGenerator->keyToName(k);
		Dict::DataType t = d.getNthType(i);

		switch(t)
		{
			case Dict::DICT_BOOL:
				dictObj[kname.str()] = d.getNthBool(i);
				break;
			case Dict::DICT_INT:
				dictObj[kname.str()] = d.getNthInt(i);
				break;
			case Dict::DICT_REAL:
				dictObj[kname.str()] = d.getNthReal(i);
				break;
			case Dict::DICT_ASCIISTRING:
				dictObj[kname.str()] = d.getNthAsciiString(i).str();
				break;
			case Dict::DICT_UNICODESTRING:
				dictObj[kname.str()] = d.getNthUnicodeString(i).str();
				break;
			default:
				DEBUG_CRASH(("impossible"));
				break;
		}
	}

	(*m_chunkStack->data)[name] = dictObj;
}

// JSON-only write implementations
void JSONChunkOutput::writeJSONOnlyString(const char* name, const AsciiString& value)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = value.str();
	}
}

void JSONChunkOutput::writeJSONOnlyInt(const char* name, Int i)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = i;
	}
}

void JSONChunkOutput::writeJSONOnlyBool(const char* name, Bool b)
{
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = (bool)b;
	}
}

void JSONChunkOutput::writeParameterType(Int type, const char* typeName)
{
	// Write the string type name to _items array for human-readable JSON
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(typeName);
	}
}

void JSONChunkOutput::writeBoolAsByte(Bool b)
{
	// Write as true/false boolean to _items array
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back((bool)b);
	}
}

void JSONChunkOutput::writeBoolAsByte(const char* name, Bool b)
{
	// Write as true/false boolean to named field
	if (m_chunkStack && m_chunkStack->data) {
		(*m_chunkStack->data)[name] = (bool)b;
	}
}

void JSONChunkOutput::writeBoolAsInt(Bool b)
{
	// Write as true/false boolean to _items array (same as writeBoolAsByte for JSON)
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back((bool)b);
	}
}

void JSONChunkOutput::writeEnumAsInt(Int value, const char* enumStr)
{
	// Write the string enum name to _items array (ignores int value for JSON)
	(void)value;
	if (m_chunkStack && m_chunkStack->data) {
		if (!m_chunkStack->data->contains("_items")) {
			(*m_chunkStack->data)["_items"] = nlohmann::json::array();
		}
		(*m_chunkStack->data)["_items"].push_back(enumStr);
	}
}
