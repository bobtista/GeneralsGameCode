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
#include <map>

struct JSONChunkInfo
{
	AsciiString label;
	AsciiString parentLabel;
	DataChunkVersionType version;
	Int dataSize;
};

class JSONChunkInput;

typedef Bool (*JSONChunkParserPtr)(JSONChunkInput& file, JSONChunkInfo* info, void* userData);

class JSONChunkInput
{
private:
	struct ParserEntry
	{
		JSONChunkParserPtr parser;
		AsciiString label;
		AsciiString parentLabel;
		void* userData;
	};

	struct ChunkContext
	{
		const nlohmann::ordered_json* chunk;
		std::string name;
		DataChunkVersionType version;
		size_t valueIndex;
	};

	nlohmann::ordered_json m_root;
	std::vector<ChunkContext> m_chunkStack;
	std::vector<ParserEntry> m_parsers;
	bool m_valid;

public:
	void* m_currentObject;
	void* m_userData;

	JSONChunkInput(const char* jsonData, size_t length)
		: m_valid(false), m_currentObject(nullptr), m_userData(nullptr)
	{
		try
		{
			m_root = nlohmann::ordered_json::parse(jsonData, jsonData + length);
			m_valid = true;
		}
		catch (...)
		{
			m_valid = false;
		}
	}

	Bool isValidFileType() const
	{
		return m_valid && m_root.is_object();
	}

	void registerParser(const AsciiString& label, const AsciiString& parentLabel,
	                    JSONChunkParserPtr parser, void* userData = nullptr)
	{
		ParserEntry entry;
		entry.parser = parser;
		entry.label = label;
		entry.parentLabel = parentLabel;
		entry.userData = userData;
		m_parsers.push_back(entry);
	}

	Bool parse(void* userData = nullptr)
	{
		m_userData = userData;
		return parseChunks(m_root, AsciiString::TheEmptyString);
	}

	AsciiString openDataChunk(DataChunkVersionType* ver)
	{
		if (m_chunkStack.empty())
			return AsciiString::TheEmptyString;

		ChunkContext& ctx = m_chunkStack.back();
		if (ver)
			*ver = ctx.version;
		return AsciiString(ctx.name.c_str());
	}

	void closeDataChunk()
	{
		if (!m_chunkStack.empty())
			m_chunkStack.pop_back();
	}

	Bool atEndOfChunk() const
	{
		if (m_chunkStack.empty())
			return true;
		const ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values"))
			return true;
		return ctx.valueIndex >= (*ctx.chunk)["_values"].size();
	}

	AsciiString getChunkLabel() const
	{
		if (m_chunkStack.empty())
			return AsciiString::TheEmptyString;
		return AsciiString(m_chunkStack.back().name.c_str());
	}

	DataChunkVersionType getChunkVersion() const
	{
		if (m_chunkStack.empty())
			return 0;
		return m_chunkStack.back().version;
	}

	Real readReal()
	{
		if (m_chunkStack.empty())
			return 0.0f;
		ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values") || ctx.valueIndex >= (*ctx.chunk)["_values"].size())
			return 0.0f;
		return (*ctx.chunk)["_values"][ctx.valueIndex++].get<Real>();
	}

	Int readInt()
	{
		if (m_chunkStack.empty())
			return 0;
		ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values") || ctx.valueIndex >= (*ctx.chunk)["_values"].size())
			return 0;
		return (*ctx.chunk)["_values"][ctx.valueIndex++].get<Int>();
	}

	Byte readByte()
	{
		if (m_chunkStack.empty())
			return 0;
		ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values") || ctx.valueIndex >= (*ctx.chunk)["_values"].size())
			return 0;
		return static_cast<Byte>((*ctx.chunk)["_values"][ctx.valueIndex++].get<int>());
	}

	AsciiString readAsciiString()
	{
		if (m_chunkStack.empty())
			return AsciiString::TheEmptyString;
		ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values") || ctx.valueIndex >= (*ctx.chunk)["_values"].size())
			return AsciiString::TheEmptyString;
		std::string s = (*ctx.chunk)["_values"][ctx.valueIndex++].get<std::string>();
		return AsciiString(s.c_str());
	}

	UnicodeString readUnicodeString()
	{
		AsciiString ascii = readAsciiString();
		UnicodeString uni;
		uni.translate(ascii);
		return uni;
	}

	void readArrayOfBytes(char* ptr, Int len)
	{
		if (m_chunkStack.empty())
			return;
		ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values") || ctx.valueIndex >= (*ctx.chunk)["_values"].size())
			return;

		std::string hexStr = (*ctx.chunk)["_values"][ctx.valueIndex++].get<std::string>();
		for (Int i = 0; i < len && i * 2 + 1 < static_cast<Int>(hexStr.size()); i++)
		{
			unsigned char hi = hexCharToNibble(hexStr[i * 2]);
			unsigned char lo = hexCharToNibble(hexStr[i * 2 + 1]);
			ptr[i] = static_cast<char>((hi << 4) | lo);
		}
	}

	Dict readDict()
	{
		Dict d;
		if (m_chunkStack.empty())
			return d;
		ChunkContext& ctx = m_chunkStack.back();
		if (!ctx.chunk->contains("_values") || ctx.valueIndex >= (*ctx.chunk)["_values"].size())
			return d;

		const nlohmann::ordered_json& dictJson = (*ctx.chunk)["_values"][ctx.valueIndex++];
		if (!dictJson.is_object())
			return d;

		for (auto& [key, value] : dictJson.items())
		{
			NameKeyType nameKey = TheNameKeyGenerator->nameToKey(key.c_str());
			if (value.is_boolean())
				d.setBool(nameKey, value.get<bool>());
			else if (value.is_number_integer())
				d.setInt(nameKey, value.get<Int>());
			else if (value.is_number_float())
				d.setReal(nameKey, value.get<Real>());
			else if (value.is_string())
				d.setAsciiString(nameKey, AsciiString(value.get<std::string>().c_str()));
		}
		return d;
	}

	NameKeyType readNameKey()
	{
		AsciiString name = readAsciiString();
		return TheNameKeyGenerator->nameToKey(name);
	}

private:
	static unsigned char hexCharToNibble(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return 0;
	}

	Bool parseChunks(const nlohmann::ordered_json& parent, const AsciiString& parentLabel)
	{
		if (!parent.contains("_children"))
			return true;

		const nlohmann::ordered_json& children = parent["_children"];
		if (!children.is_array())
			return true;

		for (const auto& childWrapper : children)
		{
			if (!childWrapper.is_object())
				continue;

			for (auto& [chunkName, chunkData] : childWrapper.items())
			{
				DataChunkVersionType version = 0;
				if (chunkData.contains("_version"))
					version = chunkData["_version"].get<DataChunkVersionType>();

				const nlohmann::ordered_json* dataPtr = &chunkData;
				if (chunkData.contains("_data"))
					dataPtr = &chunkData["_data"];

				// Find matching parser
				JSONChunkParserPtr parser = nullptr;
				void* parserUserData = nullptr;
				for (const auto& entry : m_parsers)
				{
					if (entry.label == chunkName.c_str() &&
					    (entry.parentLabel.isEmpty() || entry.parentLabel == parentLabel))
					{
						parser = entry.parser;
						parserUserData = entry.userData;
						break;
					}
				}

				if (parser)
				{
					// Push chunk context
					ChunkContext ctx;
					ctx.chunk = dataPtr;
					ctx.name = chunkName;
					ctx.version = version;
					ctx.valueIndex = 0;
					m_chunkStack.push_back(ctx);

					JSONChunkInfo info;
					info.label = chunkName.c_str();
					info.parentLabel = parentLabel;
					info.version = version;
					info.dataSize = 0;

					Bool result = parser(*this, &info, parserUserData ? parserUserData : m_userData);

					m_chunkStack.pop_back();

					if (!result)
						return false;
				}
			}
		}
		return true;
	}
};

#endif // RTS_HAS_JSON_CHUNK
