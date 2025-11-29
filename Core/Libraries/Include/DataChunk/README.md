# DataChunk Library

Platform-neutral library for reading and writing chunk-based data files (SCB format).

## Overview

This library provides the core chunk I/O functionality extracted from the game engine, making it available for use in both the engine and standalone tools. It maintains 100% binary compatibility with retail SCB files.

## Features

- ✅ VC6 compatible (no C++11 features)
- ✅ Platform-neutral (works on Windows, macOS, Linux)
- ✅ Binary compatible with retail SCB format
- ✅ No engine dependencies
- ✅ Simple stream-based interface

## Usage Example

### Reading a Chunk File

```cpp
#include "DataChunk/DataChunk.h"
#include "DataChunk/StreamAdapters.h"
#include <cstdio>

using namespace DataChunk;

bool parseScript(DataChunkInput& file, DataChunkInfo* info, void* userData)
{
    ChunkString scriptName = file.readAsciiString();
    ChunkString comment = file.readAsciiString();
    // ... read more fields ...
    return true;
}

int main()
{
    FILE* fp = fopen("script.scb", "rb");
    if (!fp) return 1;

    FileInputStream stream(fp);
    DataChunkInput chunkInput(&stream);

    chunkInput.registerParser("Script", "", parseScript);
    chunkInput.parse();

    fclose(fp);
    return 0;
}
```

### Writing a Chunk File

```cpp
#include "DataChunk/DataChunk.h"
#include "DataChunk/StreamAdapters.h"
#include <cstdio>

using namespace DataChunk;

int main()
{
    FILE* fp = fopen("output.scb", "wb");
    if (!fp) return 1;

    FileOutputStream stream(fp);
    DataChunkOutput chunkOutput(&stream);

    chunkOutput.openDataChunk("Script", 2);
    chunkOutput.writeAsciiString("MyScript");
    chunkOutput.writeAsciiString("Comment");
    // ... write more fields ...
    chunkOutput.closeDataChunk();

    fclose(fp);
    return 0;
}
```

## Architecture

- **Stream.h**: Abstract stream interfaces
- **Types.h**: Type definitions (VC6 compatible)
- **TableOfContents.h/cpp**: String-to-ID mapping
- **DataChunk.h/cpp**: Main I/O classes
- **StreamAdapters.h**: FILE* adapters for tools

## Binary Format

The library maintains exact binary compatibility with the engine's chunk format:

- Chunk header: 10 bytes (4-byte ID + 2-byte version + 4-byte size)
- String table: "CkMp" magic + count + entries
- Strings: Length-prefixed (2 bytes) + data (no null terminator)
- All integers: Little-endian, 4 bytes
- All floats: IEEE 754, 4 bytes

## VC6 Compatibility Notes

- Uses `std::string` (VC6 STL compatible)
- Uses `NULL` instead of `nullptr`
- No C++11 features
- Raw pointers (no smart pointers)
- Standard C++98/C++03

