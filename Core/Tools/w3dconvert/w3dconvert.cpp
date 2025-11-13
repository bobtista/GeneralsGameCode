#include "chunkserializer.h"
#include "xmlwriter.h"
#include "xmlreader.h"
#include "wwfile.h"
#include <iostream>
#include <cstring>

void PrintUsage(const char* programName) {
	std::cout << "W3D Converter - Convert between W3D binary and W3X XML formats\n";
	std::cout << "Usage:\n";
	std::cout << "  " << programName << " --to-xml <input.w3d> <output.w3x>\n";
	std::cout << "  " << programName << " --to-w3d <input.w3x> <output.w3d>\n";
	std::cout << "\nOptions:\n";
	std::cout << "  --to-xml     Convert W3D binary file to W3X XML format\n";
	std::cout << "  --to-w3d     Convert W3X XML file to W3D binary format\n";
}

bool ConvertToXML(const char* inputPath, const char* outputPath) {
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
	
	XMLWriter writer(outputPath);
	if (!writer.WriteW3DFile(chunks)) {
		std::cerr << "Error: Failed to write XML file: " << outputPath << "\n";
		return false;
	}
	
	std::cout << "Successfully converted " << inputPath << " to " << outputPath << "\n";
	std::cout << "Processed " << chunks.size() << " top-level chunk(s)\n";
	return true;
}

bool ConvertToW3D(const char* inputPath, const char* outputPath) {
	XMLReader reader(inputPath);
	std::vector<std::unique_ptr<ChunkData>> chunks;
	
	if (!reader.ReadW3DFile(chunks)) {
		std::cerr << "Error: Failed to read XML file: " << inputPath << "\n";
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
	
	if (std::strcmp(mode, "--to-xml") == 0) {
		success = ConvertToXML(inputPath, outputPath);
	} else if (std::strcmp(mode, "--to-w3d") == 0) {
		success = ConvertToW3D(inputPath, outputPath);
	} else {
		std::cerr << "Error: Unknown mode '" << mode << "'\n\n";
		PrintUsage(argv[0]);
		return 1;
	}
	
	return success ? 0 : 1;
}

