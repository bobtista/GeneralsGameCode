/*
**	Command & Conquer Generals Zero Hour(tm)
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

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "Common/Dict.h"
#include "Common/NameKeyGenerator.h"
#include "Common/DataChunk.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <string>

class JSONChunkOutput
{
private:
	nlohmann::ordered_json m_root;
	std::vector<nlohmann::ordered_json*> m_chunkStack;
	std::vector<std::string> m_chunkNames;

	nlohmann::ordered_json& currentChunk()
	{
		if (m_chunkStack.empty())
			return m_root;
		return *m_chunkStack.back();
	}

public:
	JSONChunkOutput() : m_root(nlohmann::ordered_json::object()) {}

	void openDataChunk(const char* name, DataChunkVersionType ver)
	{
		nlohmann::ordered_json& parent = currentChunk();

		nlohmann::ordered_json newChunk = nlohmann::ordered_json::object();
		newChunk["_version"] = ver;
		newChunk["_data"] = nlohmann::ordered_json::object();

		if (!parent.contains("_children"))
			parent["_children"] = nlohmann::ordered_json::array();

		nlohmann::ordered_json wrapper = nlohmann::ordered_json::object();
		wrapper[name] = std::move(newChunk);
		parent["_children"].push_back(std::move(wrapper));

		m_chunkStack.push_back(&(parent["_children"].back()[name]["_data"]));
		m_chunkNames.push_back(name);
	}

	void closeDataChunk()
	{
		if (!m_chunkStack.empty())
		{
			m_chunkStack.pop_back();
			m_chunkNames.pop_back();
		}
	}

	void writeReal(Real r)
	{
		currentChunk()["_values"].push_back(r);
	}

	void writeInt(Int i)
	{
		currentChunk()["_values"].push_back(i);
	}

	void writeByte(Byte b)
	{
		currentChunk()["_values"].push_back(static_cast<int>(b));
	}

	void writeAsciiString(const AsciiString& str)
	{
		currentChunk()["_values"].push_back(str.str() ? str.str() : "");
	}

	void writeUnicodeString(UnicodeString str)
	{
		// Convert Unicode to UTF-8 for JSON
		AsciiString ascii;
		str.translate(ascii);
		currentChunk()["_values"].push_back(ascii.str() ? ascii.str() : "");
	}

	void writeArrayOfBytes(char* ptr, Int len)
	{
		// Encode as hex string for readability
		std::string hexStr;
		hexStr.reserve(len * 2);
		static const char hexChars[] = "0123456789ABCDEF";
		for (Int i = 0; i < len; i++)
		{
			unsigned char c = static_cast<unsigned char>(ptr[i]);
			hexStr += hexChars[c >> 4];
			hexStr += hexChars[c & 0x0F];
		}
		currentChunk()["_values"].push_back(hexStr);
	}

	void writeDict(const Dict& d)
	{
		nlohmann::ordered_json dictJson = nlohmann::ordered_json::object();
		// Dict serialization - iterate through pairs
		for (Int i = 0; i < d.getNumPairs(); i++)
		{
			const Dict::Pair* p = d.getNthPair(i);
			if (p)
			{
				std::string keyName = TheNameKeyGenerator->keyToName(p->key).str();
				switch (p->type)
				{
					case Dict::DICT_BOOL:
						dictJson[keyName] = p->value.boolValue;
						break;
					case Dict::DICT_INT:
						dictJson[keyName] = p->value.intValue;
						break;
					case Dict::DICT_REAL:
						dictJson[keyName] = p->value.realValue;
						break;
					case Dict::DICT_ASCIISTRING:
						dictJson[keyName] = p->value.asciiStringValue ? p->value.asciiStringValue : "";
						break;
					case Dict::DICT_UNICODESTRING:
						{
							AsciiString ascii;
							if (p->value.unicodeStringValue)
							{
								UnicodeString uni(p->value.unicodeStringValue);
								uni.translate(ascii);
							}
							dictJson[keyName] = ascii.str() ? ascii.str() : "";
						}
						break;
				}
			}
		}
		currentChunk()["_values"].push_back(dictJson);
	}

	void writeNameKey(const NameKeyType key)
	{
		AsciiString name = TheNameKeyGenerator->keyToName(key);
		currentChunk()["_values"].push_back(name.str() ? name.str() : "");
	}

	std::string getJSONString(int indent = 2) const
	{
		return m_root.dump(indent);
	}

	const nlohmann::ordered_json& getJSON() const
	{
		return m_root;
	}
};

#endif // RTS_HAS_JSON_CHUNK
