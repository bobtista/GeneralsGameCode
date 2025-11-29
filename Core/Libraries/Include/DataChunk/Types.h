/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Types.h
// Type definitions for DataChunk library
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#pragma once

#include <Utility/stdint_adapter.h>
#include <string>

namespace DataChunk {

// Type aliases matching engine types for binary compatibility
typedef uint16_t DataChunkVersionType;  // matches engine's DataChunkVersionType

// String types - using std::string for platform neutrality
typedef std::string ChunkString;
typedef std::wstring ChunkWideString;

// Numeric types matching engine for binary compatibility
typedef int32_t ChunkInt;           // matches engine's Int
typedef uint32_t ChunkUInt;          // matches engine's UnsignedInt
typedef uint16_t ChunkUShort;        // matches engine's UnsignedShort
typedef uint8_t ChunkByte;           // matches engine's Byte
typedef float ChunkReal;             // matches engine's Real

} // namespace DataChunk

