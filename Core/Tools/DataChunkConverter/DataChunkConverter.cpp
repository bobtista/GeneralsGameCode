/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
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

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

static void printUsage(const char *programName)
{
	std::cerr << "DataChunkConverter - Convert between binary DataChunk (SCB) and JSON formats\n\n";
	std::cerr << "Usage:\n";
	std::cerr << "  " << programName << " --to-json <input.scb> <output.json>\n";
	std::cerr << "  " << programName << " --to-binary <input.json> <output.scb>\n";
	std::cerr << "\nOptions:\n";
	std::cerr << "  --to-json     Convert binary DataChunk file to JSON\n";
	std::cerr << "  --to-binary   Convert JSON file to binary DataChunk\n";
	std::cerr << "  --help        Show this help message\n";
}

// Dict data types (from Dict.h)
enum DictDataType
{
	DICT_BOOL = 0,
	DICT_INT = 1,
	DICT_REAL = 2,
	DICT_ASCIISTRING = 3,
	DICT_UNICODESTRING = 4
};

class BinaryReader
{
	const uint8_t *m_data;
	size_t m_size;
	size_t m_pos;

public:
	BinaryReader(const uint8_t *data, size_t size) : m_data(data), m_size(size), m_pos(0) {}

	bool hasData(size_t bytes) const { return m_pos + bytes <= m_size; }
	size_t pos() const { return m_pos; }
	size_t remaining() const { return m_size - m_pos; }
	void skip(size_t bytes) { m_pos += bytes; }

	uint8_t readByte()
	{
		if (!hasData(1)) return 0;
		return m_data[m_pos++];
	}

	int32_t readInt()
	{
		if (!hasData(4)) return 0;
		int32_t val;
		std::memcpy(&val, m_data + m_pos, 4);
		m_pos += 4;
		return val;
	}

	uint16_t readUShort()
	{
		if (!hasData(2)) return 0;
		uint16_t val;
		std::memcpy(&val, m_data + m_pos, 2);
		m_pos += 2;
		return val;
	}

	uint32_t readUInt()
	{
		if (!hasData(4)) return 0;
		uint32_t val;
		std::memcpy(&val, m_data + m_pos, 4);
		m_pos += 4;
		return val;
	}

	float readFloat()
	{
		if (!hasData(4)) return 0.0f;
		float val;
		std::memcpy(&val, m_data + m_pos, 4);
		m_pos += 4;
		return val;
	}

	std::string readLenString()
	{
		uint16_t len = readUShort();
		if (!hasData(len)) return "";
		// Convert from Latin-1/Windows-1252 to UTF-8
		std::string utf8;
		utf8.reserve(len * 2); // worst case
		for (uint16_t i = 0; i < len; i++) {
			uint8_t ch = m_data[m_pos + i];
			if (ch < 0x80) {
				utf8.push_back(static_cast<char>(ch));
			} else {
				// Latin-1 high bytes -> UTF-8 two-byte sequence
				utf8.push_back(static_cast<char>(0xC0 | (ch >> 6)));
				utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
			}
		}
		m_pos += len;
		return utf8;
	}

	std::u16string readLenUnicodeString()
	{
		uint16_t len = readUShort();
		if (!hasData(len * 2)) return u"";
		std::u16string str;
		str.reserve(len);
		for (uint16_t i = 0; i < len; i++) {
			uint16_t ch;
			std::memcpy(&ch, m_data + m_pos, 2);
			m_pos += 2;
			str.push_back(static_cast<char16_t>(ch));
		}
		return str;
	}

	std::string readFixedString(size_t len)
	{
		if (!hasData(len)) return "";
		std::string str(reinterpret_cast<const char*>(m_data + m_pos), len);
		m_pos += len;
		return str;
	}
};

class BinaryWriter
{
	std::vector<uint8_t> m_data;

public:
	void writeByte(uint8_t val) { m_data.push_back(val); }

	void writeInt(int32_t val)
	{
		size_t pos = m_data.size();
		m_data.resize(pos + 4);
		std::memcpy(m_data.data() + pos, &val, 4);
	}

	void writeUShort(uint16_t val)
	{
		size_t pos = m_data.size();
		m_data.resize(pos + 2);
		std::memcpy(m_data.data() + pos, &val, 2);
	}

	void writeUInt(uint32_t val)
	{
		size_t pos = m_data.size();
		m_data.resize(pos + 4);
		std::memcpy(m_data.data() + pos, &val, 4);
	}

	void writeFloat(float val)
	{
		size_t pos = m_data.size();
		m_data.resize(pos + 4);
		std::memcpy(m_data.data() + pos, &val, 4);
	}

	void writeLenString(const std::string &utf8)
	{
		// Convert from UTF-8 to Latin-1
		std::vector<uint8_t> latin1;
		latin1.reserve(utf8.size());
		size_t i = 0;
		while (i < utf8.size()) {
			uint8_t c = static_cast<uint8_t>(utf8[i]);
			if ((c & 0x80) == 0) {
				// ASCII
				latin1.push_back(c);
				i += 1;
			} else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
				// Two-byte UTF-8 sequence -> Latin-1
				uint8_t ch = ((c & 0x1F) << 6) | (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
				latin1.push_back(ch);
				i += 2;
			} else {
				// Skip invalid or higher UTF-8 sequences
				latin1.push_back('?');
				i += 1;
			}
		}
		writeUShort(static_cast<uint16_t>(latin1.size()));
		m_data.insert(m_data.end(), latin1.begin(), latin1.end());
	}

	void writeLenUnicodeString(const std::u16string &str)
	{
		writeUShort(static_cast<uint16_t>(str.size()));
		for (char16_t ch : str) {
			uint16_t val = static_cast<uint16_t>(ch);
			size_t pos = m_data.size();
			m_data.resize(pos + 2);
			std::memcpy(m_data.data() + pos, &val, 2);
		}
	}

	void writeBytes(const std::vector<uint8_t> &bytes)
	{
		m_data.insert(m_data.end(), bytes.begin(), bytes.end());
	}

	void writeBytes(const uint8_t *data, size_t len)
	{
		m_data.insert(m_data.end(), data, data + len);
	}

	const std::vector<uint8_t>& data() const { return m_data; }
	size_t size() const { return m_data.size(); }
};

// String table for ID <-> name mapping
class StringTable
{
	std::map<uint32_t, std::string> m_idToName;
	std::map<std::string, uint32_t> m_nameToId;
	std::vector<uint32_t> m_order;  // Preserves original entry order
	uint32_t m_nextId = 1;

public:
	void addMapping(const std::string &name, uint32_t id)
	{
		if (m_idToName.find(id) == m_idToName.end()) {
			m_order.push_back(id);  // Track insertion order
		}
		m_idToName[id] = name;
		m_nameToId[name] = id;
		if (id >= m_nextId) {
			m_nextId = id + 1;
		}
	}

	uint32_t getOrCreateId(const std::string &name)
	{
		auto it = m_nameToId.find(name);
		if (it != m_nameToId.end()) {
			return it->second;
		}
		uint32_t id = m_nextId++;
		addMapping(name, id);
		return id;
	}

	std::string getName(uint32_t id) const
	{
		auto it = m_idToName.find(id);
		if (it != m_idToName.end()) {
			return it->second;
		}
		return "<unknown:" + std::to_string(id) + ">";
	}

	const std::map<uint32_t, std::string>& mappings() const { return m_idToName; }
	const std::vector<uint32_t>& order() const { return m_order; }
};

struct ChunkHeader
{
	std::string label;
	uint16_t version;
	uint32_t dataSize;
};

static bool readStringTable(BinaryReader &reader, StringTable &table)
{
	// Check for CkMp header
	if (!reader.hasData(4)) return false;
	std::string tag = reader.readFixedString(4);
	if (tag != "CkMp") {
		std::cerr << "Error: Invalid file header (expected 'CkMp', got '" << tag << "')\n";
		return false;
	}

	// Read number of entries
	int32_t count = reader.readInt();

	// Read each entry: [1-byte len][string][4-byte ID]
	for (int32_t i = 0; i < count; i++) {
		uint8_t len = reader.readByte();
		std::string name = reader.readFixedString(len);
		uint32_t id = reader.readUInt();
		table.addMapping(name, id);
	}

	return true;
}

static void writeStringTable(BinaryWriter &writer, const StringTable &table)
{
	// Write CkMp header
	writer.writeBytes(reinterpret_cast<const uint8_t*>("CkMp"), 4);

	// Write count
	const auto &mappings = table.mappings();
	const auto &order = table.order();
	writer.writeInt(static_cast<int32_t>(mappings.size()));

	// Write entries in original order
	for (uint32_t id : order) {
		auto it = mappings.find(id);
		if (it != mappings.end()) {
			const std::string &name = it->second;
			writer.writeByte(static_cast<uint8_t>(name.size()));
			writer.writeBytes(reinterpret_cast<const uint8_t*>(name.data()), name.size());
			writer.writeUInt(id);
		}
	}
}

static bool readChunkHeader(BinaryReader &reader, const StringTable &table, ChunkHeader &header)
{
	if (!reader.hasData(10)) return false; // 4 (id) + 2 (version) + 4 (size)

	uint32_t id = reader.readUInt();
	header.label = table.getName(id);
	header.version = reader.readUShort();
	header.dataSize = reader.readUInt();
	return true;
}

static void writeChunkHeader(BinaryWriter &writer, StringTable &table, const std::string &label, uint16_t version, uint32_t dataSize)
{
	uint32_t id = table.getOrCreateId(label);
	writer.writeUInt(id);
	writer.writeUShort(version);
	writer.writeUInt(dataSize);
}

// Convert UTF-16 to UTF-8
static std::string utf16ToUtf8(const std::u16string &utf16)
{
	std::string utf8;
	for (char16_t ch : utf16) {
		if (ch < 0x80) {
			utf8.push_back(static_cast<char>(ch));
		} else if (ch < 0x800) {
			utf8.push_back(static_cast<char>(0xC0 | (ch >> 6)));
			utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
		} else {
			utf8.push_back(static_cast<char>(0xE0 | (ch >> 12)));
			utf8.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
			utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
		}
	}
	return utf8;
}

// Convert UTF-8 to UTF-16
static std::u16string utf8ToUtf16(const std::string &utf8)
{
	std::u16string utf16;
	size_t i = 0;
	while (i < utf8.size()) {
		uint8_t c = static_cast<uint8_t>(utf8[i]);
		char16_t ch;
		if ((c & 0x80) == 0) {
			ch = c;
			i += 1;
		} else if ((c & 0xE0) == 0xC0) {
			ch = (c & 0x1F) << 6;
			if (i + 1 < utf8.size()) ch |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
			i += 2;
		} else if ((c & 0xF0) == 0xE0) {
			ch = (c & 0x0F) << 12;
			if (i + 1 < utf8.size()) ch |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6;
			if (i + 2 < utf8.size()) ch |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
			i += 3;
		} else {
			ch = '?';
			i += 1;
		}
		utf16.push_back(ch);
	}
	return utf16;
}

// Read a Dict object from binary
static json readDict(BinaryReader &reader, const StringTable &table)
{
	json dict = json::object();
	uint16_t count = reader.readUShort();

	for (uint16_t i = 0; i < count; i++) {
		int32_t keyAndType = reader.readInt();
		DictDataType type = static_cast<DictDataType>(keyAndType & 0xFF);
		uint32_t keyId = static_cast<uint32_t>(keyAndType >> 8);
		std::string keyName = table.getName(keyId);

		json entry = json::object();
		entry["_type"] = static_cast<int>(type);

		switch (type) {
			case DICT_BOOL:
				entry["value"] = reader.readByte() != 0;
				break;
			case DICT_INT:
				entry["value"] = reader.readInt();
				break;
			case DICT_REAL:
				entry["value"] = reader.readFloat();
				break;
			case DICT_ASCIISTRING:
				entry["value"] = reader.readLenString();
				break;
			case DICT_UNICODESTRING:
				entry["value"] = utf16ToUtf8(reader.readLenUnicodeString());
				break;
			default:
				entry["value"] = nullptr;
				break;
		}

		dict[keyName] = entry;
	}

	return dict;
}

// Write a Dict object to binary
static void writeDict(BinaryWriter &writer, StringTable &table, const json &dict)
{
	// Count valid entries
	uint16_t count = 0;
	for (auto it = dict.begin(); it != dict.end(); ++it) {
		if (it.value().is_object() && it.value().contains("_type")) {
			count++;
		}
	}

	writer.writeUShort(count);

	for (auto it = dict.begin(); it != dict.end(); ++it) {
		const std::string &key = it.key();
		const json &value = it.value();
		if (!value.is_object() || !value.contains("_type")) continue;

		uint32_t keyId = table.getOrCreateId(key);
		int typeInt = value["_type"].get<int>();
		DictDataType type = static_cast<DictDataType>(typeInt);
		int32_t keyAndType = (static_cast<int32_t>(keyId) << 8) | (type & 0xFF);
		writer.writeInt(keyAndType);

		switch (type) {
			case DICT_BOOL: {
				bool bval = value["value"].get<bool>();
				writer.writeByte(bval ? 1 : 0);
				break;
			}
			case DICT_INT: {
				int32_t ival = value["value"].get<int32_t>();
				writer.writeInt(ival);
				break;
			}
			case DICT_REAL: {
				float fval = value["value"].get<float>();
				writer.writeFloat(fval);
				break;
			}
			case DICT_ASCIISTRING: {
				std::string sval = value["value"].get<std::string>();
				writer.writeLenString(sval);
				break;
			}
			case DICT_UNICODESTRING: {
				std::string sval = value["value"].get<std::string>();
				writer.writeLenUnicodeString(utf8ToUtf16(sval));
				break;
			}
			default:
				break;
		}
	}
}

// Read multiple Dict objects until end of chunk
static json readDictArray(BinaryReader &reader, const StringTable &table, size_t endPos)
{
	json dicts = json::array();
	while (reader.pos() < endPos && reader.remaining() > 0) {
		dicts.push_back(readDict(reader, table));
	}
	return dicts;
}

// Write multiple Dict objects
static void writeDictArray(BinaryWriter &writer, StringTable &table, const json &dicts)
{
	for (const auto &dict : dicts) {
		writeDict(writer, table, dict);
	}
}

static json readParameter(BinaryReader &reader)
{
	json param = json::object();
	int32_t paramType = reader.readInt();
	param["type"] = paramType;

	// Parameter format from game code:
	// - type (int)
	// - if COORD3D (16): x, y, z (floats)
	// - else: int, real, string (ALL THREE always)
	if (paramType == 16) { // COORD3D
		param["x"] = reader.readFloat();
		param["y"] = reader.readFloat();
		param["z"] = reader.readFloat();
	} else {
		param["int"] = reader.readInt();
		param["real"] = reader.readFloat();
		param["string"] = reader.readLenString();
	}
	return param;
}

static void writeParameter(BinaryWriter &writer, const json &param)
{
	int32_t paramType = param.value("type", 0);
	writer.writeInt(paramType);

	// Parameter format from game code:
	// - type (int)
	// - if COORD3D (16): x, y, z (floats)
	// - else: int, real, string (ALL THREE always)
	if (paramType == 16) { // COORD3D
		writer.writeFloat(param.value("x", 0.0f));
		writer.writeFloat(param.value("y", 0.0f));
		writer.writeFloat(param.value("z", 0.0f));
	} else {
		writer.writeInt(param.value("int", 0));
		writer.writeFloat(param.value("real", 0.0f));
		writer.writeLenString(param.value("string", ""));
	}
}

static json parseChunks(BinaryReader &reader, const StringTable &table, size_t endPos);

static json parseCondition(BinaryReader &reader, const StringTable &table, uint16_t version)
{
	json cond = json::object();
	cond["conditionType"] = reader.readInt();
	// Version 4+ has a nameKey between conditionType and numParms
	if (version >= 4) {
		int32_t keyAndType = reader.readInt();
		uint32_t nameKeyId = static_cast<uint32_t>(keyAndType >> 8);
		cond["nameKey"] = table.getName(nameKeyId);
	}
	int32_t numParms = reader.readInt();
	cond["numParms"] = numParms;

	json params = json::array();
	for (int i = 0; i < numParms; i++) {
		params.push_back(readParameter(reader));
	}
	cond["parameters"] = params;
	return cond;
}

static json parseScriptAction(BinaryReader &reader, const StringTable &table, uint16_t version)
{
	json action = json::object();
	action["actionType"] = reader.readInt();
	// Version 2+ has a nameKey between actionType and numParms
	if (version >= 2) {
		int32_t keyAndType = reader.readInt();
		uint32_t nameKeyId = static_cast<uint32_t>(keyAndType >> 8);
		action["nameKey"] = table.getName(nameKeyId);
	}
	int32_t numParms = reader.readInt();
	action["numParms"] = numParms;

	json params = json::array();
	for (int i = 0; i < numParms; i++) {
		params.push_back(readParameter(reader));
	}
	action["parameters"] = params;
	return action;
}

static json parseScript(BinaryReader &reader, const StringTable &table, uint16_t version, size_t endPos)
{
	json script = json::object();
	script["scriptName"] = reader.readLenString();
	script["comment"] = reader.readLenString();
	script["conditionComment"] = reader.readLenString();
	script["actionComment"] = reader.readLenString();
	script["isActive"] = reader.readByte();
	script["deactivateUponSuccess"] = reader.readByte();
	script["easy"] = reader.readByte();
	script["normal"] = reader.readByte();
	script["hard"] = reader.readByte();
	script["isSubroutine"] = reader.readByte();
	if (version >= 2) {
		script["delayEvaluationSeconds"] = reader.readInt();
	}
	script["_children"] = parseChunks(reader, table, endPos);
	return script;
}

static json parseScriptGroup(BinaryReader &reader, const StringTable &table, uint16_t version, size_t endPos)
{
	json group = json::object();
	group["groupName"] = reader.readLenString();
	group["isGroupActive"] = reader.readByte();
	if (version >= 2) {
		group["isGroupSubroutine"] = reader.readByte();
	}
	group["_children"] = parseChunks(reader, table, endPos);
	return group;
}

static json parseOrCondition(BinaryReader &reader, const StringTable &table, size_t endPos)
{
	json orCond = json::object();
	orCond["_children"] = parseChunks(reader, table, endPos);
	return orCond;
}

// ScriptsPlayers has custom format: readDicts flag (version 2+), numNames, then name+optional dict pairs
static json parseScriptsPlayers(BinaryReader &reader, const StringTable &table, uint16_t version, size_t endPos)
{
	json data = json::object();
	int32_t readDicts = 0;
	if (version >= 2) {
		readDicts = reader.readInt();
		data["readDicts"] = readDicts;
	}
	int32_t numNames = reader.readInt();
	data["numNames"] = numNames;

	json players = json::array();
	for (int32_t i = 0; i < numNames && reader.pos() < endPos; i++) {
		json player = json::object();
		player["playerName"] = reader.readLenString();
		if (readDicts) {
			player["dict"] = readDict(reader, table);
		}
		players.push_back(player);
	}
	data["players"] = players;
	return data;
}

static json parseChunks(BinaryReader &reader, const StringTable &table, size_t endPos)
{
	json children = json::array();

	while (reader.pos() < endPos && reader.remaining() > 0) {
		ChunkHeader header;
		if (!readChunkHeader(reader, table, header)) break;

		size_t chunkEnd = reader.pos() + header.dataSize;
		if (chunkEnd > endPos) break;

		json child = json::object();
		child["_label"] = header.label;
		child["_version"] = header.version;

		if (header.label == "Script") {
			child["_data"] = parseScript(reader, table, header.version, chunkEnd);
		} else if (header.label == "ScriptGroup") {
			child["_data"] = parseScriptGroup(reader, table, header.version, chunkEnd);
		} else if (header.label == "OrCondition") {
			child["_data"] = parseOrCondition(reader, table, chunkEnd);
		} else if (header.label == "Condition") {
			child["_data"] = parseCondition(reader, table, header.version);
		} else if (header.label == "ScriptAction" || header.label == "ScriptActionFalse") {
			child["_data"] = parseScriptAction(reader, table, header.version);
		} else if (header.label == "ScriptList" || header.label == "PlayerScriptsList") {
			child["_data"] = json::object();
			child["_data"]["_children"] = parseChunks(reader, table, chunkEnd);
		} else if (header.label == "ScriptsPlayers") {
			// ScriptsPlayers has custom format, not just dicts
			child["_data"] = parseScriptsPlayers(reader, table, header.version, chunkEnd);
		} else if (header.label == "ScriptTeams" || header.label == "ObjectsList" ||
		           header.label == "PolygonTriggers" || header.label == "WaypointsList") {
			// Dict-based chunks - parse as array of Dict objects
			child["_data"] = json::object();
			child["_data"]["_dicts"] = readDictArray(reader, table, chunkEnd);
		} else {
			// Unknown chunk - store raw data
			std::vector<uint8_t> rawData(header.dataSize);
			for (size_t i = 0; i < header.dataSize && reader.remaining() > 0; i++) {
				rawData[i] = reader.readByte();
			}
			child["_rawData"] = rawData;
		}

		// Ensure reader is positioned at chunk end for next iteration
		if (reader.pos() < chunkEnd) {
			reader.skip(chunkEnd - reader.pos());
		}

		children.push_back(child);
	}

	return children;
}

static void writeChunks(BinaryWriter &writer, StringTable &table, const json &children);

static void writeCondition(BinaryWriter &writer, StringTable &table, const json &data, uint16_t version)
{
	writer.writeInt(data.value("conditionType", 0));
	// Version 4+ has a nameKey between conditionType and numParms
	if (version >= 4 && data.contains("nameKey")) {
		std::string nameKey = data["nameKey"].get<std::string>();
		uint32_t nameKeyId = table.getOrCreateId(nameKey);
		int32_t keyAndType = (static_cast<int32_t>(nameKeyId) << 8) | 3; // 3 = DICT_ASCIISTRING
		writer.writeInt(keyAndType);
	}
	writer.writeInt(data.value("numParms", 0));
	if (data.contains("parameters")) {
		for (const auto &param : data["parameters"]) {
			writeParameter(writer, param);
		}
	}
}

static void writeScriptAction(BinaryWriter &writer, StringTable &table, const json &data, uint16_t version)
{
	writer.writeInt(data.value("actionType", 0));
	// Version 2+ has a nameKey between actionType and numParms
	if (version >= 2 && data.contains("nameKey")) {
		std::string nameKey = data["nameKey"].get<std::string>();
		uint32_t nameKeyId = table.getOrCreateId(nameKey);
		int32_t keyAndType = (static_cast<int32_t>(nameKeyId) << 8) | 3; // 3 = DICT_ASCIISTRING
		writer.writeInt(keyAndType);
	}
	writer.writeInt(data.value("numParms", 0));
	if (data.contains("parameters")) {
		for (const auto &param : data["parameters"]) {
			writeParameter(writer, param);
		}
	}
}

static void writeScript(BinaryWriter &writer, StringTable &table, const json &data)
{
	writer.writeLenString(data.value("scriptName", ""));
	writer.writeLenString(data.value("comment", ""));
	writer.writeLenString(data.value("conditionComment", ""));
	writer.writeLenString(data.value("actionComment", ""));
	writer.writeByte(data.value("isActive", 0));
	writer.writeByte(data.value("deactivateUponSuccess", 0));
	writer.writeByte(data.value("easy", 1));
	writer.writeByte(data.value("normal", 1));
	writer.writeByte(data.value("hard", 1));
	writer.writeByte(data.value("isSubroutine", 0));
	// Always write version 2 format with delayEvaluationSeconds
	writer.writeInt(data.value("delayEvaluationSeconds", 0));
	if (data.contains("_children")) {
		writeChunks(writer, table, data["_children"]);
	}
}

static void writeScriptGroup(BinaryWriter &writer, StringTable &table, const json &data)
{
	writer.writeLenString(data.value("groupName", ""));
	writer.writeByte(data.value("isGroupActive", 1));
	writer.writeByte(data.value("isGroupSubroutine", 0));
	if (data.contains("_children")) {
		writeChunks(writer, table, data["_children"]);
	}
}

static void writeScriptsPlayers(BinaryWriter &writer, StringTable &table, const json &data, uint16_t version)
{
	// Version 2+ has readDicts flag
	int32_t readDicts = data.value("readDicts", 0);
	if (version >= 2) {
		writer.writeInt(readDicts);
	}
	int32_t numNames = data.value("numNames", 0);
	writer.writeInt(numNames);

	if (data.contains("players")) {
		for (const auto &player : data["players"]) {
			writer.writeLenString(player.value("playerName", ""));
			if (readDicts && player.contains("dict")) {
				writeDict(writer, table, player["dict"]);
			}
		}
	}
}

static void writeChunk(BinaryWriter &writer, StringTable &table, const json &chunk)
{
	std::string label = chunk.value("_label", "");
	uint16_t version = chunk.value("_version", 1);

	BinaryWriter dataWriter;
	if (chunk.contains("_data")) {
		const json &data = chunk["_data"];
		if (label == "Script") {
			writeScript(dataWriter, table, data);
		} else if (label == "ScriptGroup") {
			writeScriptGroup(dataWriter, table, data);
		} else if (label == "OrCondition") {
			if (data.contains("_children")) {
				writeChunks(dataWriter, table, data["_children"]);
			}
		} else if (label == "Condition") {
			writeCondition(dataWriter, table, data, version);
		} else if (label == "ScriptAction" || label == "ScriptActionFalse") {
			writeScriptAction(dataWriter, table, data, version);
		} else if (label == "ScriptList" || label == "PlayerScriptsList") {
			if (data.contains("_children")) {
				writeChunks(dataWriter, table, data["_children"]);
			}
		} else if (label == "ScriptsPlayers") {
			writeScriptsPlayers(dataWriter, table, data, version);
		} else if (label == "ScriptTeams" || label == "ObjectsList" ||
		           label == "PolygonTriggers" || label == "WaypointsList") {
			// Dict-based chunks
			if (data.contains("_dicts")) {
				writeDictArray(dataWriter, table, data["_dicts"]);
			}
		}
	} else if (chunk.contains("_rawData")) {
		std::vector<uint8_t> rawData = chunk["_rawData"].get<std::vector<uint8_t>>();
		dataWriter.writeBytes(rawData);
	}

	writeChunkHeader(writer, table, label, version, static_cast<uint32_t>(dataWriter.size()));
	writer.writeBytes(dataWriter.data());
}

static void writeChunks(BinaryWriter &writer, StringTable &table, const json &children)
{
	for (const auto &child : children) {
		writeChunk(writer, table, child);
	}
}

// First pass: collect all labels and dict keys to build string table
static void collectStrings(const json &node, StringTable &table)
{
	if (node.is_object()) {
		if (node.contains("_label")) {
			std::string label = node["_label"].get<std::string>();
			table.getOrCreateId(label);
		}
		// Collect nameKey for Condition/ScriptAction version compatibility
		if (node.contains("nameKey")) {
			std::string nameKey = node["nameKey"].get<std::string>();
			table.getOrCreateId(nameKey);
		}
		if (node.contains("_data")) {
			collectStrings(node["_data"], table);
		}
		if (node.contains("_children")) {
			collectStrings(node["_children"], table);
		}
		if (node.contains("_dicts")) {
			collectStrings(node["_dicts"], table);
		}
		// Handle ScriptsPlayers players array with dicts
		if (node.contains("players")) {
			for (const auto &player : node["players"]) {
				if (player.contains("dict")) {
					collectStrings(player["dict"], table);
				}
			}
		}
		// Collect dict keys
		for (auto it = node.begin(); it != node.end(); ++it) {
			if (it.value().is_object() && it.value().contains("_type")) {
				table.getOrCreateId(it.key());
			}
		}
	} else if (node.is_array()) {
		for (const auto &child : node) {
			collectStrings(child, table);
		}
	}
}

static bool convertBinaryToJson(const std::string &inputPath, const std::string &outputPath)
{
	std::ifstream inputFile(inputPath, std::ios::binary);
	if (!inputFile) {
		std::cerr << "Error: Cannot open input file: " << inputPath << "\n";
		return false;
	}

	inputFile.seekg(0, std::ios::end);
	size_t fileSize = inputFile.tellg();
	inputFile.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(fileSize);
	inputFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
	inputFile.close();

	BinaryReader reader(buffer.data(), buffer.size());

	// Read string table
	StringTable table;
	if (!readStringTable(reader, table)) {
		return false;
	}

	// Parse chunks
	json result = json::object();

	// Store string table in JSON for round-trip preservation (as array to preserve order)
	json tableJson = json::array();
	for (uint32_t id : table.order()) {
		auto it = table.mappings().find(id);
		if (it != table.mappings().end()) {
			json entry = json::object();
			entry["id"] = id;
			entry["name"] = it->second;
			tableJson.push_back(entry);
		}
	}
	result["_stringTable"] = tableJson;

	result["_children"] = parseChunks(reader, table, fileSize);

	std::ofstream outputFile(outputPath);
	if (!outputFile) {
		std::cerr << "Error: Cannot create output file: " << outputPath << "\n";
		return false;
	}

	outputFile << result.dump(2);
	outputFile.close();

	std::cout << "Converted " << inputPath << " -> " << outputPath << "\n";
	return true;
}

static bool convertJsonToBinary(const std::string &inputPath, const std::string &outputPath)
{
	std::ifstream inputFile(inputPath);
	if (!inputFile) {
		std::cerr << "Error: Cannot open input file: " << inputPath << "\n";
		return false;
	}

	json data;
	try {
		inputFile >> data;
	} catch (const json::parse_error &e) {
		std::cerr << "Error: Invalid JSON: " << e.what() << "\n";
		return false;
	}
	inputFile.close();

	StringTable table;

	// Restore string table from JSON if present (supports both array and object formats)
	if (data.contains("_stringTable")) {
		const json &stringTable = data["_stringTable"];
		if (stringTable.is_array()) {
			// New array format: preserves order
			for (const auto &entry : stringTable) {
				uint32_t id = entry["id"].get<uint32_t>();
				std::string name = entry["name"].get<std::string>();
				table.addMapping(name, id);
			}
		} else {
			// Legacy object format
			for (auto it = stringTable.begin(); it != stringTable.end(); ++it) {
				uint32_t id = std::stoul(it.key());
				std::string name = it.value().get<std::string>();
				table.addMapping(name, id);
			}
		}
	}

	// Collect any new strings not in the table
	collectStrings(data, table);

	// Write string table
	BinaryWriter headerWriter;
	writeStringTable(headerWriter, table);

	// Write chunks
	BinaryWriter chunkWriter;
	if (data.contains("_children")) {
		writeChunks(chunkWriter, table, data["_children"]);
	}

	std::ofstream outputFile(outputPath, std::ios::binary);
	if (!outputFile) {
		std::cerr << "Error: Cannot create output file: " << outputPath << "\n";
		return false;
	}

	outputFile.write(reinterpret_cast<const char*>(headerWriter.data().data()), headerWriter.size());
	outputFile.write(reinterpret_cast<const char*>(chunkWriter.data().data()), chunkWriter.size());
	outputFile.close();

	std::cout << "Converted " << inputPath << " -> " << outputPath << "\n";
	return true;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printUsage(argv[0]);
		return 1;
	}

	std::string arg1 = argv[1];

	if (arg1 == "--help" || arg1 == "-h") {
		printUsage(argv[0]);
		return 0;
	}

	if (argc != 4) {
		std::cerr << "Error: Invalid number of arguments\n\n";
		printUsage(argv[0]);
		return 1;
	}

	std::string inputPath = argv[2];
	std::string outputPath = argv[3];

	bool success = false;
	try {
		if (arg1 == "--to-json") {
			success = convertBinaryToJson(inputPath, outputPath);
		} else if (arg1 == "--to-binary") {
			success = convertJsonToBinary(inputPath, outputPath);
		} else {
			std::cerr << "Error: Unknown option: " << arg1 << "\n\n";
			printUsage(argv[0]);
			return 1;
		}
	} catch (const std::exception &e) {
		std::cerr << "Exception: " << e.what() << "\n";
		return 2;
	} catch (...) {
		std::cerr << "Unknown exception\n";
		return 3;
	}

	return success ? 0 : 1;
}
