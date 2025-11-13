#pragma once

#include "chunkserializer.h"
#include <fstream>
#include <string>

class XMLReader {
public:
	explicit XMLReader(const char* filename);
	
	bool ReadW3DFile(std::vector<std::unique_ptr<ChunkData>>& chunks);
	
private:
	std::unique_ptr<ChunkData> ParseChunk(const std::string& line);
	bool ReadChunkContent(std::unique_ptr<ChunkData>& chunk);
	std::string ReadLine();
	std::string Trim(const std::string& str);
	std::string GetAttribute(const std::string& line, const char* attrName);
	uint32_t ParseHex(const std::string& hex);
	std::vector<uint8_t> HexToBytes(const std::string& hex);
	
	std::ifstream m_file;
	bool m_error;
};

