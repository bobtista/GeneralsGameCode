#pragma once

#include "chunkserializer.h"
#include <string>
#include <fstream>

class JSONReader {
public:
	explicit JSONReader(const char* filename);
	
	bool ReadW3DFile(std::vector<std::unique_ptr<ChunkData>>& chunks);
	
private:
	std::unique_ptr<ChunkData> ParseChunk();
	std::string ReadUntil(char delimiter);
	std::string ReadString();
	std::string ReadValue();
	void SkipWhitespace();
	bool Expect(char c);
	bool ExpectString(const char* str);
	uint32_t ParseHex(const std::string& hex);
	std::vector<uint8_t> HexToBytes(const std::string& hex);
	std::string UnescapeString(const std::string& str);
	
	std::ifstream m_file;
	bool m_error;
};

