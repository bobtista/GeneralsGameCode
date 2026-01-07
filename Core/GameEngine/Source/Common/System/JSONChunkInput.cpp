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

#include "Common/JSONChunkInput.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Errors.h"
#include "Common/UnicodeString.h"
#include "Common/AsciiString.h"
#ifdef WIN32
#include <windows.h>
#endif

JSONChunkInput::JSONChunkInput( const char* jsonData, size_t jsonSize ) :
	m_parserList(NULL),
	m_chunkStack(NULL),
	m_currentChunkIndex(0),
	m_currentObject(NULL),
	m_userData(NULL)
{
	try {
		m_root = nlohmann::json::parse(jsonData, jsonData + jsonSize);
		if (m_root.contains("chunks") && m_root["chunks"].is_array()) {
			for (auto& chunk : m_root["chunks"]) {
				m_chunkArray.push_back(&chunk);
			}
		}
	} catch (...) {
		m_root = nlohmann::json();
	}
}

JSONChunkInput::~JSONChunkInput()
{
	clearChunkStack();

	JSONParser *p, *next;
	for (p=m_parserList; p; p=next) {
		next = p->next;
		deleteInstance(p);
	}
}

void JSONChunkInput::clearChunkStack( void )
{
	JSONInputChunk *c, *next;

	for (c=m_chunkStack; c; c=next) {
		next = c->next;
		deleteInstance(c);
	}

	m_chunkStack = NULL;
}

Bool JSONChunkInput::isValidFileType(void)
{
	return m_root.contains("chunks") && m_root["chunks"].is_array();
}

void JSONChunkInput::registerParser( const AsciiString& label, const AsciiString& parentLabel, JSONChunkParserPtr parser, void *userData )
{
	JSONParser *p = newInstance(JSONParser);
	p->parser = parser;
	p->label = label;
	p->parentLabel = parentLabel;
	p->userData = userData;
	p->next = m_parserList;
	m_parserList = p;
}

AsciiString JSONChunkInput::openDataChunk(DataChunkVersionType *ver )
{
	// If we're inside a chunk, read nested chunks from _children array
	if (m_chunkStack && m_chunkStack->data) {
		nlohmann::json* data = m_chunkStack->data;

		// Check for _children array in the chunk object
		if (data->is_object() && data->contains("_children") && (*data)["_children"].is_array()) {
			nlohmann::json& children = (*data)["_children"];
			if (m_chunkStack->childIndex < children.size()) {
				nlohmann::json& item = children[m_chunkStack->childIndex++];
				if (item.is_object() && item.contains("label") && item.contains("version")) {
					JSONInputChunk *c = newInstance(JSONInputChunk);
					c->label = AsciiString(item["label"].get<std::string>().c_str());
					c->version = item["version"].get<DataChunkVersionType>();
					// Point directly to chunk object (no "data" wrapper)
					c->data = &item;
					c->childIndex = 0;
					c->itemIndex = 0;
					c->next = m_chunkStack;
					m_chunkStack = c;

					*ver = c->version;
					return c->label;
				}
			}
			// No more nested chunks
			*ver = 0;
			return AsciiString::TheEmptyString;
		}

		// No more nested chunks in current chunk
		*ver = 0;
		return AsciiString::TheEmptyString;
	}

	// Top-level chunk - read from m_chunkArray
	if (m_currentChunkIndex >= m_chunkArray.size()) {
		*ver = 0;
		return AsciiString::TheEmptyString;
	}

	nlohmann::json* chunkJson = m_chunkArray[m_currentChunkIndex++];

	if (!chunkJson->contains("label") || !chunkJson->contains("version")) {
		*ver = 0;
		return AsciiString::TheEmptyString;
	}

	JSONInputChunk *c = newInstance(JSONInputChunk);
	c->label = AsciiString((*chunkJson)["label"].get<std::string>().c_str());
	c->version = (*chunkJson)["version"].get<DataChunkVersionType>();
	// Point directly to chunk object (no "data" wrapper)
	c->data = chunkJson;
	c->childIndex = 0;
	c->itemIndex = 0;
	c->next = m_chunkStack;
	m_chunkStack = c;

	*ver = c->version;
	return c->label;
}

void JSONChunkInput::closeDataChunk( void )
{
	if (m_chunkStack == NULL) {
		return;
	}

	JSONInputChunk *c = m_chunkStack;
	m_chunkStack = m_chunkStack->next;
	deleteInstance(c);
}

Bool JSONChunkInput::atEndOfFile( void )
{
	return m_currentChunkIndex >= m_chunkArray.size() && m_chunkStack == NULL;
}

Bool JSONChunkInput::atEndOfChunk( void )
{
	if (m_chunkStack == NULL) {
		return true;
	}

	nlohmann::json* data = m_chunkStack->data;

	// New format: object with _children array
	if (data->is_object()) {
		if (data->contains("_children") && (*data)["_children"].is_array()) {
			return m_chunkStack->childIndex >= (*data)["_children"].size();
		}
		// Object without _children means no nested chunks, so we're at end
		return true;
	}

	// Old format: data is an array
	if (data->is_array()) {
		return m_chunkStack->childIndex >= data->size();
	}

	return true;
}

void JSONChunkInput::reset( void )
{
	clearChunkStack();
	m_currentChunkIndex = 0;
}

AsciiString JSONChunkInput::getChunkLabel( void )
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return AsciiString::TheEmptyString;
	}

	return m_chunkStack->label;
}

DataChunkVersionType JSONChunkInput::getChunkVersion( void )
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	return m_chunkStack->version;
}

unsigned int JSONChunkInput::getChunkDataSize( void )
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	nlohmann::json* data = m_chunkStack->data;

	// New format: object with _children array
	if (data->is_object() && data->contains("_children") && (*data)["_children"].is_array()) {
		return (*data)["_children"].size();
	}

	// Old format: data is an array
	if (data->is_array()) {
		return data->size();
	}

	return 0;
}

unsigned int JSONChunkInput::getChunkDataSizeLeft( void )
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	nlohmann::json* data = m_chunkStack->data;

	// New format: object with _children array
	if (data->is_object() && data->contains("_children") && (*data)["_children"].is_array()) {
		return (*data)["_children"].size() - m_chunkStack->childIndex;
	}

	// Old format: data is an array
	if (data->is_array()) {
		return data->size() - m_chunkStack->childIndex;
	}

	return 0;
}

Bool JSONChunkInput::parse( void *userData )
{
	m_userData = userData;

	while (!atEndOfFile()) {
		if (m_chunkStack && atEndOfChunk()) {
			break;
		}

		DataChunkVersionType ver;
		AsciiString label = openDataChunk(&ver);
		if (atEndOfFile()) {
			break;
		}

		AsciiString parentLabel = AsciiString::TheEmptyString;
		if (m_chunkStack && m_chunkStack->next) {
			parentLabel = m_chunkStack->next->label;
		}

		Bool scopeOK = false;
		JSONParser* parser;
		for (parser=m_parserList; parser; parser=parser->next) {
			if (parser->label == label) {
				scopeOK = true;
				if (parentLabel != parser->parentLabel) {
					scopeOK = false;
				}

				if (scopeOK) {
					JSONChunkInfo info;
					info.label = label;
					info.parentLabel = parentLabel;
					info.version = ver;
					info.dataSize = getChunkDataSize();

					if (parser->parser(*this, &info, userData) == false) {
						return false;
					}
					break;
				}
			}
		}

		closeDataChunk();
	}

	return true;
}

// Helper to get the items array (either _items in object, or the array itself for old format)
static nlohmann::json* getItemsArray(nlohmann::json* data) {
	if (data->is_object() && data->contains("_items") && (*data)["_items"].is_array()) {
		return &(*data)["_items"];
	}
	if (data->is_array()) {
		return data;
	}
	return NULL;
}

Real JSONChunkInput::readReal(void)
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return 0.0f;
	}

	nlohmann::json* items = getItemsArray(m_chunkStack->data);
	if (!items) {
		DEBUG_CRASH(("Bad."));
		return 0.0f;
	}

	if (m_chunkStack->itemIndex >= items->size()) {
		DEBUG_CRASH(("Read past end of chunk."));
		return 0.0f;
	}

	Real r = (*items)[m_chunkStack->itemIndex++].get<Real>();
	return r;
}

Int JSONChunkInput::readInt(void)
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	nlohmann::json* items = getItemsArray(m_chunkStack->data);
	if (!items) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	if (m_chunkStack->itemIndex >= items->size()) {
		DEBUG_CRASH(("Read past end of chunk."));
		return 0;
	}

	Int i = (*items)[m_chunkStack->itemIndex++].get<Int>();
	return i;
}

Byte JSONChunkInput::readByte(void)
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	nlohmann::json* items = getItemsArray(m_chunkStack->data);
	if (!items) {
		DEBUG_CRASH(("Bad."));
		return 0;
	}

	if (m_chunkStack->itemIndex >= items->size()) {
		DEBUG_CRASH(("Read past end of chunk."));
		return 0;
	}

	Byte b = (*items)[m_chunkStack->itemIndex++].get<Byte>();
	return b;
}

void JSONChunkInput::readArrayOfBytes(char *ptr, Int len)
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return;
	}

	nlohmann::json* items = getItemsArray(m_chunkStack->data);
	if (!items) {
		DEBUG_CRASH(("Bad."));
		return;
	}

	if (m_chunkStack->itemIndex >= items->size()) {
		DEBUG_CRASH(("Read past end of chunk."));
		return;
	}

	std::string bytesStr = (*items)[m_chunkStack->itemIndex++].get<std::string>();

	if (bytesStr.length() < (size_t)len) {
		DEBUG_CRASH(("Not enough bytes."));
		return;
	}

	memcpy(ptr, bytesStr.c_str(), len);
}

AsciiString JSONChunkInput::readAsciiString(void)
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return AsciiString::TheEmptyString;
	}

	nlohmann::json* items = getItemsArray(m_chunkStack->data);
	if (!items) {
		DEBUG_CRASH(("Bad."));
		return AsciiString::TheEmptyString;
	}

	if (m_chunkStack->itemIndex >= items->size()) {
		DEBUG_CRASH(("Read past end of chunk."));
		return AsciiString::TheEmptyString;
	}

	std::string str = (*items)[m_chunkStack->itemIndex++].get<std::string>();
	return AsciiString(str.c_str());
}

UnicodeString JSONChunkInput::readUnicodeString(void)
{
	if (m_chunkStack == NULL) {
		DEBUG_CRASH(("Bad."));
		return UnicodeString::TheEmptyString;
	}

	nlohmann::json* items = getItemsArray(m_chunkStack->data);
	if (!items) {
		DEBUG_CRASH(("Bad."));
		return UnicodeString::TheEmptyString;
	}

	if (m_chunkStack->itemIndex >= items->size()) {
		DEBUG_CRASH(("Read past end of chunk."));
		return UnicodeString::TheEmptyString;
	}

	std::string str = (*items)[m_chunkStack->itemIndex++].get<std::string>();
	if (str.empty()) {
		return UnicodeString::TheEmptyString;
	}

#ifdef WIN32
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	if (wideLen <= 0) {
		DEBUG_CRASH(("Failed to convert UTF-8 string to wide string."));
		return UnicodeString::TheEmptyString;
	}
	WideChar* wideStr = (WideChar*)TheDynamicMemoryAllocator->allocateBytes(wideLen * sizeof(WideChar), "JSONChunkInput::readUnicodeString");
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, wideLen);
	UnicodeString result(wideStr);
	TheDynamicMemoryAllocator->freeBytes(wideStr);
	return result;
#else
	AsciiString asciiStr(str.c_str());
	UnicodeString result;
	result.translate(asciiStr);
	return result;
#endif
}

NameKeyType JSONChunkInput::readNameKey(void)
{
	Int keyAndType = readInt();
#ifdef DEBUG_CRASHING
	Dict::DataType t = (Dict::DataType)(keyAndType & 0xff);
	DEBUG_ASSERTCRASH(t==Dict::DICT_ASCIISTRING,("Invalid key data."));
#endif
	keyAndType >>= 8;

	std::string knameStr;
	if (m_root.contains("toc") && m_root["toc"].is_object()) {
		std::string idStr = std::to_string(keyAndType);
		if (m_root["toc"].contains(idStr)) {
			knameStr = m_root["toc"][idStr].get<std::string>();
		}
	}

	if (knameStr.empty()) {
		knameStr = std::to_string(keyAndType);
	}

	AsciiString kname(knameStr.c_str());
	NameKeyType k = TheNameKeyGenerator->nameToKey(kname);
	return k;
}

Dict JSONChunkInput::readDict()
{
	UnsignedShort len = readInt();
	Dict d(len);

	for (int i = 0; i < len; i++)
	{
		Int keyAndType = readInt();
		Dict::DataType t = (Dict::DataType)(keyAndType & 0xff);
		keyAndType >>= 8;

		std::string knameStr;
		if (m_root.contains("toc") && m_root["toc"].is_object()) {
			std::string idStr = std::to_string(keyAndType);
			if (m_root["toc"].contains(idStr)) {
				knameStr = m_root["toc"][idStr].get<std::string>();
			}
		}

		if (knameStr.empty()) {
			knameStr = std::to_string(keyAndType);
		}

		AsciiString kname(knameStr.c_str());
		NameKeyType k = TheNameKeyGenerator->nameToKey(kname);

		switch(t)
		{
			case Dict::DICT_BOOL:
				d.setBool(k, readByte() ? true : false);
				break;
			case Dict::DICT_INT:
				d.setInt(k, readInt());
				break;
			case Dict::DICT_REAL:
				d.setReal(k, readReal());
				break;
			case Dict::DICT_ASCIISTRING:
				d.setAsciiString(k, readAsciiString());
				break;
			case Dict::DICT_UNICODESTRING:
				d.setUnicodeString(k, readUnicodeString());
				break;
			default:
				throw ERROR_CORRUPT_FILE_FORMAT;
				break;
		}
	}

	return d;
}

// Named read implementations
Real JSONChunkInput::readReal(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return 0.0f;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return 0.0f;
	}

	return (*m_chunkStack->data)[name].get<Real>();
}

Int JSONChunkInput::readInt(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return 0;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return 0;
	}

	return (*m_chunkStack->data)[name].get<Int>();
}

Byte JSONChunkInput::readByte(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return 0;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return 0;
	}

	return (*m_chunkStack->data)[name].get<Byte>();
}

AsciiString JSONChunkInput::readAsciiString(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return AsciiString::TheEmptyString;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return AsciiString::TheEmptyString;
	}

	std::string str = (*m_chunkStack->data)[name].get<std::string>();
	return AsciiString(str.c_str());
}

UnicodeString JSONChunkInput::readUnicodeString(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return UnicodeString::TheEmptyString;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return UnicodeString::TheEmptyString;
	}

	std::string str = (*m_chunkStack->data)[name].get<std::string>();
	if (str.empty()) {
		return UnicodeString::TheEmptyString;
	}

#ifdef WIN32
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	if (wideLen <= 0) {
		DEBUG_CRASH(("Failed to convert UTF-8 string to wide string."));
		return UnicodeString::TheEmptyString;
	}
	WideChar* wideStr = (WideChar*)TheDynamicMemoryAllocator->allocateBytes(wideLen * sizeof(WideChar), "JSONChunkInput::readUnicodeString");
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, wideLen);
	UnicodeString result(wideStr);
	TheDynamicMemoryAllocator->freeBytes(wideStr);
	return result;
#else
	AsciiString asciiStr(str.c_str());
	UnicodeString result;
	result.translate(asciiStr);
	return result;
#endif
}

void JSONChunkInput::readArrayOfBytes(const char* name, char *ptr, Int len)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return;
	}

	std::string bytesStr = (*m_chunkStack->data)[name].get<std::string>();

	if (bytesStr.length() < (size_t)len) {
		DEBUG_CRASH(("Not enough bytes."));
		return;
	}

	memcpy(ptr, bytesStr.c_str(), len);
}

NameKeyType JSONChunkInput::readNameKey(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return NAMEKEY_INVALID;
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return NAMEKEY_INVALID;
	}

	std::string knameStr = (*m_chunkStack->data)[name].get<std::string>();
	AsciiString kname(knameStr.c_str());
	return TheNameKeyGenerator->nameToKey(kname);
}

Dict JSONChunkInput::readDict(const char* name)
{
	if (m_chunkStack == NULL || !m_chunkStack->data->is_object()) {
		DEBUG_CRASH(("Bad - expected object data for named read."));
		return Dict();
	}

	if (!m_chunkStack->data->contains(name)) {
		DEBUG_CRASH(("Field not found: %s", name));
		return Dict();
	}

	nlohmann::json& dictObj = (*m_chunkStack->data)[name];
	if (!dictObj.is_object()) {
		DEBUG_CRASH(("Expected object for dict field."));
		return Dict();
	}

	Dict d(dictObj.size());

	for (auto& [key, value] : dictObj.items()) {
		NameKeyType k = TheNameKeyGenerator->nameToKey(AsciiString(key.c_str()));

		if (value.is_boolean()) {
			d.setBool(k, value.get<bool>());
		} else if (value.is_number_integer()) {
			d.setInt(k, value.get<Int>());
		} else if (value.is_number_float()) {
			d.setReal(k, value.get<Real>());
		} else if (value.is_string()) {
			d.setAsciiString(k, AsciiString(value.get<std::string>().c_str()));
		}
	}

	return d;
}
