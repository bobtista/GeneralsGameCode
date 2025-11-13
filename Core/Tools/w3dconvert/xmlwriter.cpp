#include "xmlwriter.h"
#include <iomanip>
#include <sstream>
#include <cctype>

XMLWriter::XMLWriter(const char* filename) {
	m_file.open(filename);
}

XMLWriter::~XMLWriter() {
	if (m_file.is_open()) {
		m_file.close();
	}
}

bool XMLWriter::WriteW3DFile(const std::vector<std::unique_ptr<ChunkData>>& chunks) {
	if (!m_file.is_open()) {
		return false;
	}
	
	m_file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	m_file << "<W3D>\n";
	
	for (const auto& chunk : chunks) {
		WriteChunk(*chunk, 1);
	}
	
	m_file << "</W3D>\n";
	return true;
}

void XMLWriter::WriteIndent(int level) {
	for (int i = 0; i < level; ++i) {
		m_file << "  ";
	}
}

void XMLWriter::WriteChunk(const ChunkData& chunk, int indent) {
	WriteIndent(indent);
	m_file << "<Chunk id=\"0x" << std::hex << std::setw(8) << std::setfill('0') 
	       << chunk.chunkId << "\" name=\"" << chunk.chunkName 
	       << "\" size=\"" << std::dec << chunk.chunkSize << "\"";
	
	if (chunk.subChunks.empty() && chunk.rawData.empty()) {
		m_file << "/>\n";
		return;
	}
	
	m_file << ">\n";
	
	if (!chunk.subChunks.empty()) {
		for (const auto& subChunk : chunk.subChunks) {
			WriteChunk(*subChunk, indent + 1);
		}
	} else if (!chunk.rawData.empty()) {
		WriteData(chunk, indent + 1);
	}
	
	WriteIndent(indent);
	m_file << "</Chunk>\n";
}

void XMLWriter::WriteData(const ChunkData& chunk, int indent) {
	WriteIndent(indent);
	
	if (chunk.dataType == ChunkDataType::STRING) {
		m_file << "<String>";
		for (size_t i = 0; i < chunk.rawData.size() && chunk.rawData[i] != 0; ++i) {
			char c = chunk.rawData[i];
			switch (c) {
				case '<': m_file << "&lt;"; break;
				case '>': m_file << "&gt;"; break;
				case '&': m_file << "&amp;"; break;
				case '"': m_file << "&quot;"; break;
				case '\'': m_file << "&apos;"; break;
				default:
					if (std::isprint(static_cast<unsigned char>(c))) {
						m_file << c;
					} else {
						m_file << "&#" << static_cast<int>(static_cast<unsigned char>(c)) << ";";
					}
			}
		}
		m_file << "</String>\n";
	} else {
		m_file << "<Data encoding=\"hex\">";
		m_file << BytesToHex(chunk.rawData);
		m_file << "</Data>\n";
	}
}

std::string XMLWriter::BytesToHex(const std::vector<uint8_t>& data) {
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (uint8_t byte : data) {
		oss << std::setw(2) << static_cast<int>(byte);
	}
	return oss.str();
}

