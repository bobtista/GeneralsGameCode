# W3D Converter Tool

A command-line tool to convert between W3D binary format and W3X XML format.

## Overview

This tool enables bidirectional conversion between Westwood 3D (W3D) binary files and a human-readable XML format (W3X). This is useful for:

- Version control friendly storage of 3D assets
- Manual editing of model properties  
- Integration with modern 3D modeling tools
- Understanding W3D file structure
- Debugging model issues

## Building

The tool is built as part of the GeneralsGameCode project. Follow the standard build instructions in the main repository README.

On Windows with Visual Studio 2022:
```bash
cmake --preset vs2022
cmake --build build
```

The executable will be located at: `build/Core/Tools/w3dconvert/w3dconvert.exe`

## Usage

### Convert W3D to XML

```bash
w3dconvert --to-xml input.w3d output.w3x
```

This reads the binary W3D file and generates a human-readable XML file containing all chunk data.

### Convert XML back to W3D

```bash
w3dconvert --to-w3d input.w3x output.w3d
```

This reads the XML file and generates a binary W3D file that can be used by the game engine.

## XML Format (W3X)

The W3X format is a straightforward XML representation of W3D chunks:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<W3D>
  <Chunk id="0x00000000" name="W3D_CHUNK_MESH" size="12345">
    <Chunk id="0x0000001F" name="W3D_CHUNK_MESH_HEADER3" size="256">
      <Data encoding="hex">0a0b0c0d...</Data>
    </Chunk>
    <Chunk id="0x00000002" name="W3D_CHUNK_VERTICES" size="1440">
      <Data encoding="hex">3f800000...</Data>
    </Chunk>
    <Chunk id="0x0000000C" name="W3D_CHUNK_MESH_USER_TEXT" size="32">
      <String>MyModel.Mesh01</String>
    </Chunk>
  </Chunk>
</W3D>
```

- String data is stored as readable text with XML escaping
- Binary data is stored as hexadecimal for round-trip fidelity
- Chunk hierarchy is preserved through nested XML elements
- Chunk IDs and names are both included for readability

## Testing

Test the converter with sample W3D files:

```bash
# Convert a W3D file to XML
w3dconvert --to-xml Core/Libraries/Source/WWVegas/WW3D2/RequiredAssets/ShatterPlanes0.w3d test.w3x

# Convert back to W3D
w3dconvert --to-w3d test.w3x test.w3d

# Verify the files are identical
diff Core/Libraries/Source/WWVegas/WW3D2/RequiredAssets/ShatterPlanes0.w3d test.w3d
```

## Supported Chunk Types

The converter currently supports all chunk types used in Generals and Zero Hour, including:

- Meshes (geometry, materials, textures)
- Hierarchies (skeletons, bones)
- Animations (keyframes, channels)
- Collision data
- Particle emitters
- Aggregate objects
- And more...

## Implementation Notes

The converter is built on the existing W3D chunk I/O infrastructure (`ChunkLoadClass`/`ChunkSaveClass`) and uses modern C++ with smart pointers for memory safety.

Key design decisions:
- All chunk data is preserved byte-for-byte for lossless conversion
- XML format prioritizes human readability over file size
- Memory ownership is explicit using `std::unique_ptr`
- Recursive chunk traversal handles arbitrary nesting depth

## Future Enhancements

Potential improvements:
- Structured data serialization (parse known struct types into XML attributes)
- Chunk sorting/normalization for consistent output
- Validation against W3D specification
- Support for Blender/3DS Max intermediate formats
- Compression support for large models

## Contributing

When adding support for new chunk types:
1. Add the chunk ID constant to the switch in `ChunkSerializer::GetChunkName()`
2. If the chunk contains sub-chunks, add it to `ChunkSerializer::HasSubChunks()`
3. Test with actual W3D files containing the new chunk type

