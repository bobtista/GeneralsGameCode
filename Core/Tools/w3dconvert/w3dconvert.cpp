#include "chunkserializer.h"
#include "jsonwriter.h"
#include "jsonreader.h"
#include "wwfile.h"
#include <iostream>
#include <cstring>

void PrintUsage(const char* programName) {
	std::cout << "W3D Converter - Convert between W3D binary and W3J JSON formats\n";
	std::cout << "Usage:\n";
	std::cout << "  " << programName << " --to-json <input.w3d> <output.w3j>\n";
	std::cout << "  " << programName << " --to-w3d <input.w3j> <output.w3d>\n";
	std::cout << "\nOptions:\n";
	std::cout << "  --to-json    Convert W3D binary file to W3J JSON format\n";
	std::cout << "  --to-w3d     Convert W3J JSON file to W3D binary format\n";
}

bool ConvertToJSON(const char* inputPath, const char* outputPath) {
	FileClass file(inputPath);
	if (!file.Open()) {
		std::cerr << "Error: Could not open input file: " << inputPath << "\n";
		return false;
	}
	
	ChunkLoadClass cload(&file);
	std::vector<std::unique_ptr<ChunkData>> chunks;
	
	while (cload.Open_Chunk()) {
		auto chunk = ChunkSerializer::ReadChunk(cload);
		if (chunk) {
			chunks.push_back(std::move(chunk));
		}
		cload.Close_Chunk();
	}
	
	file.Close();
	
	if (chunks.empty()) {
		std::cerr << "Error: No chunks found in input file\n";
		return false;
	}
	
	JSONWriter writer(outputPath);
	if (!writer.WriteW3DFile(chunks)) {
		std::cerr << "Error: Failed to write JSON file: " << outputPath << "\n";
		return false;
	}
	
	std::cout << "Successfully converted " << inputPath << " to " << outputPath << "\n";
	std::cout << "Processed " << chunks.size() << " top-level chunk(s)\n";
	return true;
}

bool ConvertToW3D(const char* inputPath, const char* outputPath) {
	JSONReader reader(inputPath);
	std::vector<std::unique_ptr<ChunkData>> chunks;
	
	if (!reader.ReadW3DFile(chunks)) {
		std::cerr << "Error: Failed to read JSON file: " << inputPath << "\n";
		return false;
	}
	
	if (chunks.empty()) {
		std::cerr << "Error: No chunks found in input file\n";
		return false;
	}
	
	FileClass file(outputPath);
	if (!file.Open(2)) {
		std::cerr << "Error: Could not create output file: " << outputPath << "\n";
		return false;
	}
	
	ChunkSaveClass csave(&file);
	
	for (const auto& chunk : chunks) {
		if (!ChunkSerializer::WriteChunk(csave, *chunk)) {
			std::cerr << "Error: Failed to write chunk to W3D file\n";
			file.Close();
			return false;
		}
	}
	
	file.Close();
	
	std::cout << "Successfully converted " << inputPath << " to " << outputPath << "\n";
	std::cout << "Processed " << chunks.size() << " top-level chunk(s)\n";
	return true;
}

int main(int argc, char* argv[]) {
	if (argc != 4) {
		PrintUsage(argv[0]);
		return 1;
	}
	
	const char* mode = argv[1];
	const char* inputPath = argv[2];
	const char* outputPath = argv[3];
	
	bool success = false;
	
	if (std::strcmp(mode, "--to-json") == 0) {
		success = ConvertToJSON(inputPath, outputPath);
	} else if (std::strcmp(mode, "--to-w3d") == 0) {
		success = ConvertToW3D(inputPath, outputPath);
	} else {
		std::cerr << "Error: Unknown mode '" << mode << "'\n\n";
		PrintUsage(argv[0]);
		return 1;
	}
	
	return success ? 0 : 1;
}

