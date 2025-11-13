#pragma once

#include "chunkserializer.h"
#include <fstream>

class JSONWriter {
public:
	explicit JSONWriter(const char* filename);
	~JSONWriter();
	
	bool WriteW3DFile(const std::vector<std::unique_ptr<ChunkData>>& chunks);
	
private:
	void WriteChunk(const ChunkData& chunk, int indent);
	void WriteIndent(int level);
	void WriteString(const std::string& str);
	std::string BytesToHex(const std::vector<uint8_t>& data);
	
	std::ofstream m_file;
	bool m_firstChunk;
};

