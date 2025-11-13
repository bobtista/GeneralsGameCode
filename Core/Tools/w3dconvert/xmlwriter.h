#pragma once

#include "chunkserializer.h"
#include <fstream>

class XMLWriter {
public:
	explicit XMLWriter(const char* filename);
	~XMLWriter();
	
	bool WriteW3DFile(const std::vector<std::unique_ptr<ChunkData>>& chunks);
	
private:
	void WriteChunk(const ChunkData& chunk, int indent);
	void WriteIndent(int level);
	void WriteData(const ChunkData& chunk, int indent);
	std::string BytesToHex(const std::vector<uint8_t>& data);
	
	std::ofstream m_file;
};

