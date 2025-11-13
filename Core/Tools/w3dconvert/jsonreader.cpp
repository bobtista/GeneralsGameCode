#include "jsonreader.h"
#include <cctype>

JSONReader::JSONReader(const char* filename) : m_error(false) {
	m_file.open(filename);
}

bool JSONReader::ReadW3DFile(std::vector<std::unique_ptr<ChunkData>>& chunks) {
	if (!m_file.is_open()) {
		return false;
	}
	
	SkipWhitespace();
	if (!Expect('{')) return false;
	
	while (!m_error && m_file.peek() != '}') {
		SkipWhitespace();
		if (m_file.peek() == '}') break;
		
		std::string key = ReadString();
		SkipWhitespace();
		if (!Expect(':')) return false;
		SkipWhitespace();
		
		if (key == "chunks") {
			if (!Expect('[')) return false;
			
			while (!m_error && m_file.peek() != ']') {
				SkipWhitespace();
				if (m_file.peek() == ']') break;
				
				auto chunk = ParseChunk();
				if (chunk) {
					chunks.push_back(std::move(chunk));
				}
				
				SkipWhitespace();
				if (m_file.peek() == ',') {
					m_file.get();
				}
			}
			
			if (!Expect(']')) return false;
		} else {
			ReadValue();
		}
		
		SkipWhitespace();
		if (m_file.peek() == ',') {
			m_file.get();
		}
	}
	
	return !m_error;
}

std::unique_ptr<ChunkData> JSONReader::ParseChunk() {
	SkipWhitespace();
	if (!Expect('{')) return nullptr;
	
	uint32_t id = 0;
	std::string name;
	uint32_t size = 0;
	std::vector<std::unique_ptr<ChunkData>> subChunks;
	std::vector<uint8_t> data;
	ChunkDataType dataType = ChunkDataType::UNKNOWN;
	
	while (!m_error && m_file.peek() != '}') {
		SkipWhitespace();
		if (m_file.peek() == '}') break;
		
		std::string key = ReadString();
		SkipWhitespace();
		if (!Expect(':')) return nullptr;
		SkipWhitespace();
		
		if (key == "id") {
			std::string idStr = ReadString();
			id = ParseHex(idStr);
		} else if (key == "name") {
			name = ReadString();
		} else if (key == "size") {
			std::string sizeStr = ReadValue();
			size = std::stoul(sizeStr);
		} else if (key == "chunks") {
			if (!Expect('[')) return nullptr;
			
			while (!m_error && m_file.peek() != ']') {
				SkipWhitespace();
				if (m_file.peek() == ']') break;
				
				auto subChunk = ParseChunk();
				if (subChunk) {
					subChunks.push_back(std::move(subChunk));
				}
				
				SkipWhitespace();
				if (m_file.peek() == ',') {
					m_file.get();
				}
			}
			
			if (!Expect(']')) return nullptr;
		} else if (key == "string") {
			std::string str = ReadString();
			str = UnescapeString(str);
			data.assign(str.begin(), str.end());
			data.push_back(0);
			dataType = ChunkDataType::STRING;
		} else if (key == "data") {
			std::string hex = ReadString();
			data = HexToBytes(hex);
			dataType = ChunkDataType::ARRAY;
		}
		
		SkipWhitespace();
		if (m_file.peek() == ',') {
			m_file.get();
		}
	}
	
	if (!Expect('}')) return nullptr;
	
	auto chunk = std::make_unique<ChunkData>(id, size, name.c_str());
	chunk->subChunks = std::move(subChunks);
	chunk->rawData = std::move(data);
	chunk->dataType = dataType;
	
	return chunk;
}

void JSONReader::SkipWhitespace() {
	while (m_file && std::isspace(m_file.peek())) {
		m_file.get();
	}
}

bool JSONReader::Expect(char c) {
	SkipWhitespace();
	if (m_file.peek() == c) {
		m_file.get();
		return true;
	}
	m_error = true;
	return false;
}

std::string JSONReader::ReadString() {
	SkipWhitespace();
	if (m_file.peek() != '"') {
		m_error = true;
		return "";
	}
	m_file.get();
	
	std::string result;
	char c;
	while (m_file.get(c) && c != '"') {
		if (c == '\\') {
			if (m_file.get(c)) {
				result += '\\';
				result += c;
			}
		} else {
			result += c;
		}
	}
	
	return result;
}

std::string JSONReader::ReadValue() {
	SkipWhitespace();
	std::string result;
	char c;
	
	while (m_file.peek() != ',' && m_file.peek() != '}' && m_file.peek() != ']') {
		if (!m_file.get(c)) break;
		if (std::isspace(c)) break;
		result += c;
	}
	
	return result;
}

uint32_t JSONReader::ParseHex(const std::string& hex) {
	std::string clean = hex;
	if (clean.find("0x") == 0 || clean.find("0X") == 0) {
		clean = clean.substr(2);
	}
	return std::stoul(clean, nullptr, 16);
}

std::vector<uint8_t> JSONReader::HexToBytes(const std::string& hex) {
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i < hex.length(); i += 2) {
		if (i + 1 >= hex.length()) break;
		std::string byteStr = hex.substr(i, 2);
		uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
		bytes.push_back(byte);
	}
	return bytes;
}

std::string JSONReader::UnescapeString(const std::string& str) {
	std::string result;
	for (size_t i = 0; i < str.length(); ++i) {
		if (str[i] == '\\' && i + 1 < str.length()) {
			switch (str[i + 1]) {
				case 'n': result += '\n'; i++; break;
				case 'r': result += '\r'; i++; break;
				case 't': result += '\t'; i++; break;
				case '\\': result += '\\'; i++; break;
				case '"': result += '"'; i++; break;
				default: result += str[i];
			}
		} else {
			result += str[i];
		}
	}
	return result;
}

