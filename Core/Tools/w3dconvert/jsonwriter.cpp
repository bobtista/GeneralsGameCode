#include "jsonwriter.h"
#include <iomanip>
#include <sstream>

JSONWriter::JSONWriter(const char* filename) : m_firstChunk(true) {
	m_file.open(filename);
}

JSONWriter::~JSONWriter() {
	if (m_file.is_open()) {
		m_file.close();
	}
}

bool JSONWriter::WriteW3DFile(const std::vector<std::unique_ptr<ChunkData>>& chunks) {
	if (!m_file.is_open()) {
		return false;
	}
	
	m_file << "{\n";
	WriteIndent(1);
	m_file << "\"format\": \"W3J\",\n";
	WriteIndent(1);
	m_file << "\"version\": \"1.0\",\n";
	WriteIndent(1);
	m_file << "\"chunks\": [\n";
	
	m_firstChunk = true;
	for (const auto& chunk : chunks) {
		if (!m_firstChunk) {
			m_file << ",\n";
		}
		WriteChunk(*chunk, 2);
		m_firstChunk = false;
	}
	
	m_file << "\n";
	WriteIndent(1);
	m_file << "]\n";
	m_file << "}\n";
	return true;
}

void JSONWriter::WriteIndent(int level) {
	for (int i = 0; i < level; ++i) {
		m_file << "  ";
	}
}

void JSONWriter::WriteChunk(const ChunkData& chunk, int indent) {
	WriteIndent(indent);
	m_file << "{\n";
	
	WriteIndent(indent + 1);
	m_file << "\"id\": \"0x" << std::hex << std::setw(8) << std::setfill('0') 
	       << chunk.chunkId << "\",\n" << std::dec;
	
	WriteIndent(indent + 1);
	m_file << "\"name\": \"" << chunk.chunkName << "\",\n";
	
	WriteIndent(indent + 1);
	m_file << "\"size\": " << chunk.chunkSize;
	
	if (!chunk.subChunks.empty()) {
		m_file << ",\n";
		WriteIndent(indent + 1);
		m_file << "\"chunks\": [\n";
		
		bool first = true;
		for (const auto& subChunk : chunk.subChunks) {
			if (!first) {
				m_file << ",\n";
			}
			WriteChunk(*subChunk, indent + 2);
			first = false;
		}
		
		m_file << "\n";
		WriteIndent(indent + 1);
		m_file << "]";
	} else if (!chunk.rawData.empty()) {
		m_file << ",\n";
		
		if (chunk.dataType == ChunkDataType::STRING) {
			WriteIndent(indent + 1);
			m_file << "\"string\": \"";
			for (size_t i = 0; i < chunk.rawData.size() && chunk.rawData[i] != 0; ++i) {
				char c = chunk.rawData[i];
				switch (c) {
					case '\\': m_file << "\\\\"; break;
					case '"': m_file << "\\\""; break;
					case '\n': m_file << "\\n"; break;
					case '\r': m_file << "\\r"; break;
					case '\t': m_file << "\\t"; break;
					default: m_file << c;
				}
			}
			m_file << "\"";
		} else {
			WriteIndent(indent + 1);
			m_file << "\"data\": \"" << BytesToHex(chunk.rawData) << "\"";
		}
	}
	
	m_file << "\n";
	WriteIndent(indent);
	m_file << "}";
}

std::string JSONWriter::BytesToHex(const std::vector<uint8_t>& data) {
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (uint8_t byte : data) {
		oss << std::setw(2) << static_cast<int>(byte);
	}
	return oss.str();
}

