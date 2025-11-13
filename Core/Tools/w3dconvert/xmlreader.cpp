#include "xmlreader.h"
#include <algorithm>
#include <sstream>

XMLReader::XMLReader(const char* filename) : m_error(false) {
	m_file.open(filename);
}

bool XMLReader::ReadW3DFile(std::vector<std::unique_ptr<ChunkData>>& chunks) {
	if (!m_file.is_open()) {
		return false;
	}
	
	std::string line;
	while (std::getline(m_file, line)) {
		line = Trim(line);
		
		if (line.find("<Chunk ") != std::string::npos) {
			auto chunk = ParseChunk(line);
			if (chunk) {
				if (!ReadChunkContent(chunk)) {
					return false;
				}
				chunks.push_back(std::move(chunk));
			}
		} else if (line == "</W3D>") {
			break;
		}
	}
	
	return !m_error;
}

std::unique_ptr<ChunkData> XMLReader::ParseChunk(const std::string& line) {
	std::string idStr = GetAttribute(line, "id");
	std::string name = GetAttribute(line, "name");
	std::string sizeStr = GetAttribute(line, "size");
	
	if (idStr.empty() || name.empty() || sizeStr.empty()) {
		m_error = true;
		return nullptr;
	}
	
	uint32_t id = ParseHex(idStr);
	uint32_t size = std::stoul(sizeStr);
	
	return std::make_unique<ChunkData>(id, size, name.c_str());
}

bool XMLReader::ReadChunkContent(std::unique_ptr<ChunkData>& chunk) {
	std::string line;
	
	while (std::getline(m_file, line)) {
		line = Trim(line);
		
		if (line.find("<Chunk ") != std::string::npos) {
			auto subChunk = ParseChunk(line);
			if (!subChunk) {
				return false;
			}
			if (!ReadChunkContent(subChunk)) {
				return false;
			}
			chunk->subChunks.push_back(std::move(subChunk));
		} else if (line.find("<String>") != std::string::npos) {
			size_t start = line.find("<String>") + 8;
			size_t end = line.find("</String>");
			if (end != std::string::npos) {
				std::string str = line.substr(start, end - start);
				chunk->rawData.assign(str.begin(), str.end());
				chunk->rawData.push_back(0);
				chunk->dataType = ChunkDataType::STRING;
			}
		} else if (line.find("<Data encoding=\"hex\">") != std::string::npos) {
			size_t start = line.find(">") + 1;
			size_t end = line.find("</Data>");
			if (end != std::string::npos) {
				std::string hex = line.substr(start, end - start);
				chunk->rawData = HexToBytes(hex);
				chunk->dataType = ChunkDataType::ARRAY;
			}
		} else if (line == "</Chunk>") {
			break;
		} else if (line.find("/>") != std::string::npos) {
			break;
		}
	}
	
	return true;
}

std::string XMLReader::Trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return "";
	}
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, last - first + 1);
}

std::string XMLReader::GetAttribute(const std::string& line, const char* attrName) {
	std::string attr = std::string(attrName) + "=\"";
	size_t pos = line.find(attr);
	if (pos == std::string::npos) {
		return "";
	}
	
	size_t start = pos + attr.length();
	size_t end = line.find("\"", start);
	if (end == std::string::npos) {
		return "";
	}
	
	return line.substr(start, end - start);
}

uint32_t XMLReader::ParseHex(const std::string& hex) {
	std::string clean = hex;
	if (clean.find("0x") == 0 || clean.find("0X") == 0) {
		clean = clean.substr(2);
	}
	
	return std::stoul(clean, nullptr, 16);
}

std::vector<uint8_t> XMLReader::HexToBytes(const std::string& hex) {
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i < hex.length(); i += 2) {
		std::string byteStr = hex.substr(i, 2);
		uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
		bytes.push_back(byte);
	}
	return bytes;
}

