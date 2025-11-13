#pragma once

#include "always.h"
#include "chunkio.h"
#include "w3d_file.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

enum class ChunkDataType {
	UNKNOWN,
	STRUCT,
	ARRAY,
	STRING
};

struct ChunkData {
	uint32_t chunkId;
	uint32_t chunkSize;
	std::string chunkName;
	ChunkDataType dataType;
	
	std::vector<uint8_t> rawData;
	std::vector<std::unique_ptr<ChunkData>> subChunks;
	
	ChunkData(uint32_t id, uint32_t size, const char* name)
		: chunkId(id), chunkSize(size), chunkName(name), dataType(ChunkDataType::UNKNOWN) {}
};

class ChunkSerializer {
public:
	static std::unique_ptr<ChunkData> ReadChunk(ChunkLoadClass& cload);
	static bool WriteChunk(ChunkSaveClass& csave, const ChunkData& chunk);
	
	static const char* GetChunkName(uint32_t chunkId);
	
private:
	static void ReadChunkData(ChunkLoadClass& cload, ChunkData& chunk);
	static bool HasSubChunks(uint32_t chunkId);
};

