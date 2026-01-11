/*
**	Command & Conquer Generals Zero Hour(tm)
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

// TheSuperHackers @feature Semantic JSON converter for SCB script files and MAP files

// FILE: Main.cpp //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
// Project:    SemanticScriptConverter
//
// Desc:       Command-line tool to convert between SCB/MAP binary and semantic JSON format
//
// Usage:
//   SemanticScriptConverter <input.scb> <output.json>    Convert scripts
//   SemanticScriptConverter <input.map> <output.json>    Extract map data to JSON
//   SemanticScriptConverter <input.json> <output.scb>    Convert JSON to scripts
//
//-----------------------------------------------------------------------------

#ifdef RTS_HAS_JSON_CHUNK

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Lib/BaseType.h"
#include "Common/Debug.h"
#include "Common/GameCommon.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/SubsystemInterface.h"
#include "Common/DataChunk.h"
#include "Common/Dict.h"
#include "Common/SemanticScriptJSON.h"
#include "Common/SemanticMapJSON.h"
#include "Compression.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/PolygonTrigger.h"

#ifdef _WIN32
#include "Win32Device/Common/Win32LocalFileSystem.h"
#else
#include "StdDevice/Common/StdLocalFileSystem.h"
#endif

// External globals needed by engine code
#ifdef _WIN32
HINSTANCE ApplicationHInstance = NULL;
HWND ApplicationHWnd = NULL;
#endif

const char *gAppPrefix = "SSC_";
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";

static SubsystemInterfaceList _TheSubsystemList;

template<class SUBSYSTEM>
void initSubsystem(SUBSYSTEM*& sysref, SUBSYSTEM* sys, const char* path1 = NULL, const char* path2 = NULL)
{
	sysref = sys;
	_TheSubsystemList.initSubsystem(sys, path1, path2, NULL);
}

//-----------------------------------------------------------------------------
// MemoryInputStream - ChunkInputStream that reads from a memory buffer
//-----------------------------------------------------------------------------
class MemoryInputStream : public ChunkInputStream
{
	const unsigned char* m_data;
	size_t m_size;
	size_t m_pos;

public:
	MemoryInputStream(const unsigned char* data, size_t size)
		: m_data(data), m_size(size), m_pos(0) {}

	Int read(void* dst, Int numBytes) override
	{
		if (m_pos >= m_size)
			return 0;
		size_t available = m_size - m_pos;
		size_t toRead = (size_t)numBytes < available ? (size_t)numBytes : available;
		memcpy(dst, m_data + m_pos, toRead);
		m_pos += toRead;
		return (Int)toRead;
	}

	Bool absoluteSeek(UnsignedInt pos) override
	{
		if (pos > m_size)
			m_pos = m_size;
		else
			m_pos = pos;
		return true;
	}

	Bool eof() override { return m_pos >= m_size; }
	UnsignedInt tell() override { return (UnsignedInt)m_pos; }
};

//-----------------------------------------------------------------------------
// MemoryOutputStream - OutputStream that writes to a memory buffer
//-----------------------------------------------------------------------------
class MemoryOutputStream : public OutputStream
{
	std::vector<unsigned char> m_data;

public:
	Int write(const void* pData, Int numBytes) override
	{
		const unsigned char* src = (const unsigned char*)pData;
		m_data.insert(m_data.end(), src, src + numBytes);
		return numBytes;
	}

	const unsigned char* getData() const { return m_data.data(); }
	size_t getDataSize() const { return m_data.size(); }
	void clear() { m_data.clear(); }
};

//-----------------------------------------------------------------------------
// SimpleFileOutputStream - OutputStream that writes to a FILE*
//-----------------------------------------------------------------------------
class SimpleFileOutputStream : public OutputStream
{
	FILE* m_file;
public:
	SimpleFileOutputStream(FILE* file) : m_file(file) {}
	Int write(const void* pData, Int numBytes) override
	{
		return (Int)fwrite(pData, 1, numBytes, m_file);
	}
};

//-----------------------------------------------------------------------------
// decompressIfNeeded - Decompress data if it's compressed (RefPack, etc.)
//-----------------------------------------------------------------------------
static bool decompressIfNeeded(std::string& data)
{
	if (data.size() < 8)
		return true;  // Too small to be compressed

	if (!CompressionManager::isDataCompressed(data.data(), (Int)data.size()))
		return true;  // Not compressed

	Int uncompLen = CompressionManager::getUncompressedSize(data.data(), (Int)data.size());
	if (uncompLen <= 0)
		return false;  // Invalid compression header

	std::vector<char> uncompBuffer(uncompLen);
	Int actualLen = CompressionManager::decompressData(data.data(), (Int)data.size(),
	                                                    uncompBuffer.data(), uncompLen);
	if (actualLen != uncompLen)
	{
		fprintf(stderr, "Warning: Decompression size mismatch (expected %d, got %d)\n",
		        uncompLen, actualLen);
		return false;
	}

	data.assign(uncompBuffer.data(), uncompLen);
	return true;
}

//-----------------------------------------------------------------------------
// Global options
//-----------------------------------------------------------------------------
static std::string g_gameDir;

//-----------------------------------------------------------------------------
// Global team storage for ScriptTeams chunk parsing
//-----------------------------------------------------------------------------
static std::vector<Dict> g_parsedTeams;

// TheSuperHackers @bobtista Parse ScriptTeams chunk - stores team Dicts for JSON export
static Bool ParseScriptTeamsChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	g_parsedTeams.clear();
	while (!file.atEndOfChunk())
	{
		Dict teamDict = file.readDict();
		g_parsedTeams.push_back(teamDict);
	}
	return true;
}

//-----------------------------------------------------------------------------
// printUsage
//-----------------------------------------------------------------------------
static void printUsage(const char* programName)
{
	fprintf(stderr, "Semantic Script/Map Converter - Convert between binary and JSON formats\n\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s [options] <input> <output>       Convert between formats (auto-detected)\n", programName);
	fprintf(stderr, "  %s [options] --validate <file>      Validate SCB, MAP, or JSON file\n", programName);
	fprintf(stderr, "  %s [options] --verify <input>       Roundtrip verification test (.scb or .map)\n", programName);
	fprintf(stderr, "\n");
	fprintf(stderr, "Conversion direction is auto-detected from file extensions:\n");
	fprintf(stderr, "  .scb -> .json    Script binary to semantic JSON\n");
	fprintf(stderr, "  .json -> .scb    Semantic JSON to script binary\n");
	fprintf(stderr, "  .map -> .json    Extract map data to semantic JSON\n");
	fprintf(stderr, "  .json + .map -> .map  Merge JSON data with base map terrain\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --validate         Check if file can be parsed (shows statistics)\n");
	fprintf(stderr, "  --verify           Roundtrip test: binary -> JSON -> binary, compare results\n");
	fprintf(stderr, "  --game-dir <path>  Path to game installation directory\n");
	fprintf(stderr, "                     (defaults to registry lookup, then current directory)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "  %s SkirmishScripts.scb scripts.json\n", programName);
	fprintf(stderr, "  %s scripts.json SkirmishScripts.scb\n", programName);
	fprintf(stderr, "  %s MyMap.map mapdata.json\n", programName);
	fprintf(stderr, "  %s mapdata.json MyMap.map Modified.map\n", programName);
	fprintf(stderr, "  %s --validate SkirmishScripts.scb\n", programName);
	fprintf(stderr, "  %s --verify SkirmishScripts.scb\n", programName);
	fprintf(stderr, "  %s --verify MyMap.map\n", programName);
	fprintf(stderr, "\n");
}

//-----------------------------------------------------------------------------
// hasExtension - Check if filename has given extension (case-insensitive)
//-----------------------------------------------------------------------------
static bool hasExtension(const char* filename, const char* ext)
{
	size_t nameLen = strlen(filename);
	size_t extLen = strlen(ext);
	if (nameLen < extLen)
		return false;

	const char* fileExt = filename + nameLen - extLen;
	for (size_t i = 0; i < extLen; i++)
	{
		if (std::tolower(fileExt[i]) != std::tolower(ext[i]))
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// findGameDirectory - Find game installation directory
//-----------------------------------------------------------------------------
static std::string findGameDirectory()
{
	// Option 1: Use explicitly specified game directory
	if (!g_gameDir.empty())
	{
		return g_gameDir;
	}

#ifdef _WIN32
	// Option 2: Look up in Windows registry
	HKEY hKey;
	const char* registryPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, registryPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
	    RegOpenKeyExA(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		char installPath[MAX_PATH];
		DWORD pathSize = sizeof(installPath);
		DWORD type;

		if (RegQueryValueExA(hKey, "InstallPath", NULL, &type, (LPBYTE)installPath, &pathSize) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			// Strip trailing quotes or backslashes that might be in the registry
			std::string path(installPath);
			while (!path.empty() && (path.back() == '"' || path.back() == '\\'))
				path.pop_back();
			printf("Found game directory in registry: %s\n", path.c_str());
			fflush(stdout);
			return path;
		}

		// Try "ErgcKey" as alternative
		if (RegQueryValueExA(hKey, "ErgcKey", NULL, &type, (LPBYTE)installPath, &pathSize) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			// Strip trailing quotes or backslashes that might be in the registry
			std::string path(installPath);
			while (!path.empty() && (path.back() == '"' || path.back() == '\\'))
				path.pop_back();
			printf("Found game directory in registry (ErgcKey): %s\n", path.c_str());
			fflush(stdout);
			return path;
		}

		RegCloseKey(hKey);
	}
#endif

	// Option 3: Use current working directory
	printf("Using current directory as game directory\n");
	return ".";
}

//-----------------------------------------------------------------------------
// readFileContents
//-----------------------------------------------------------------------------
static bool readFileContents(const char* filename, std::string& outContents)
{
	std::ifstream file(filename, std::ios::binary);
	if (!file)
	{
		printf("Error: Cannot open file '%s' for reading\n", filename);
		fflush(stdout);
		return false;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	outContents = buffer.str();
	return true;
}

//-----------------------------------------------------------------------------
// writeFileContents
//-----------------------------------------------------------------------------
static bool writeFileContents(const char* filename, const std::string& contents)
{
	std::ofstream file(filename, std::ios::binary);
	if (!file)
	{
		printf("Error: Cannot open file '%s' for writing\n", filename);
		fflush(stdout);
		return false;
	}

	file.write(contents.c_str(), contents.size());
	return file.good();
}

//-----------------------------------------------------------------------------
// initMinimalEngine
//-----------------------------------------------------------------------------
static bool initMinimalEngine()
{
	// Find and change to game directory
	std::string gameDir = findGameDirectory();

#ifdef _WIN32
	if (!SetCurrentDirectoryA(gameDir.c_str()))
	{
		fprintf(stderr, "Warning: Could not change to game directory: %s\n", gameDir.c_str());
		fflush(stderr);
	}
	else
	{
		printf("Changed to game directory: %s\n", gameDir.c_str());
		fflush(stdout);
	}
#else
	if (chdir(gameDir.c_str()) != 0)
	{
		fprintf(stderr, "Warning: Could not change to game directory: %s\n", gameDir.c_str());
		fflush(stderr);
	}
	else
	{
		printf("Changed to game directory: %s\n", gameDir.c_str());
		fflush(stdout);
	}
#endif

	// Initialize memory manager
	printf("Initializing memory manager...\n");
	fflush(stdout);
	initMemoryManager();

	// Initialize name key generator (required for engine)
	printf("Initializing name key generator...\n");
	fflush(stdout);
	TheNameKeyGenerator = new NameKeyGenerator;
	TheNameKeyGenerator->init();

	// Initialize file system
	printf("Initializing file system...\n");
	fflush(stdout);
	TheFileSystem = new FileSystem;

#ifdef _WIN32
	initSubsystem(TheLocalFileSystem, (LocalFileSystem*)new Win32LocalFileSystem);
#else
	initSubsystem(TheLocalFileSystem, (LocalFileSystem*)new StdLocalFileSystem);
#endif

	// Initialize GlobalData (required by ScriptEngine::init)
	printf("Initializing global data...\n");
	fflush(stdout);
	TheWritableGlobalData = new GlobalData();

	// Initialize script engine (needed for action/condition templates)
	// Templates are populated in ScriptEngine::init()
	printf("Initializing script engine...\n");
	fflush(stdout);
	initSubsystem(TheScriptEngine, (ScriptEngine*)(new ScriptEngine()));

	// Initialize SidesList (needed for map parsing)
	printf("Initializing sides list...\n");
	fflush(stdout);
	initSubsystem(TheSidesList, new SidesList());

	printf("Post-processing subsystems...\n");
	fflush(stdout);
	_TheSubsystemList.postProcessLoadAll();
	printf("Engine initialization complete.\n");
	fflush(stdout);

	// Verify templates were loaded
	const ActionTemplate* testAction = TheScriptEngine->getActionTemplate(0);
	if (testAction && !testAction->m_internalName.isEmpty())
	{
		printf("Script templates loaded successfully.\n");
		fflush(stdout);
	}
	else
	{
		fprintf(stderr, "Warning: Script templates not loaded. Template names will show as 'unknownAction/Condition'.\n");
		fprintf(stderr, "Make sure the game's Data/INI files are accessible from: %s\n", gameDir.c_str());
		fflush(stderr);
	}

	return true;
}

//-----------------------------------------------------------------------------
// shutdownEngine
//-----------------------------------------------------------------------------
static void shutdownEngine()
{
	_TheSubsystemList.shutdownAll();

	delete TheWritableGlobalData;
	TheWritableGlobalData = NULL;

	delete TheFileSystem;
	TheFileSystem = NULL;

	delete TheNameKeyGenerator;
	TheNameKeyGenerator = NULL;

	shutdownMemoryManager();
}

//-----------------------------------------------------------------------------
// cleanupScriptLists - Helper to clean up script lists with proper memory management
//-----------------------------------------------------------------------------
static void cleanupScriptLists(ScriptList* scriptLists[], int count)
{
	for (int i = 0; i < count; i++)
	{
		if (scriptLists[i])
		{
			deleteInstance(scriptLists[i]);
			scriptLists[i] = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// countPolygonTriggers - Count polygon triggers by iterating the list
//-----------------------------------------------------------------------------
static int countPolygonTriggers()
{
	int count = 0;
	for (PolygonTrigger* t = PolygonTrigger::getFirstPolygonTrigger(); t; t = t->getNext())
		count++;
	return count;
}

//-----------------------------------------------------------------------------
// convertToJSON
//-----------------------------------------------------------------------------
static int convertToJSON(const char* inputFile, const char* outputFile)
{
	printf("Converting %s to JSON...\n", inputFile);
	fflush(stdout);

	// Read the input SCB file
	std::string scbData;
	if (!readFileContents(inputFile, scbData))
	{
		return 1;
	}

	// Parse the SCB binary data using the game's chunk parsing system
	MemoryInputStream memStream((const unsigned char*)scbData.data(), scbData.size());
	DataChunkInput input(&memStream);

	if (!input.isValidFileType())
	{
		fprintf(stderr, "Error: Invalid SCB file format\n");
		return 1;
	}

	// Clear global team storage
	g_parsedTeams.clear();

	// Register parser callbacks
	input.registerParser("PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk);
	input.registerParser("ScriptTeams", AsciiString::TheEmptyString, ParseScriptTeamsChunk);

	// Parse the file - this invokes the callbacks for each chunk type
	if (!input.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse SCB file\n");
		return 1;
	}

	// Get the parsed script lists
	ScriptList* scriptLists[MAX_PLAYER_COUNT];
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		scriptLists[i] = NULL;
	Int numPlayers = ScriptList::getReadScripts(scriptLists);

	// Convert to semantic JSON
	SemanticScriptWriter writer;
	nlohmann::ordered_json jsonOutput = writer.writeScriptsFile(scriptLists, numPlayers, g_parsedTeams);

	// Write JSON to output file
	std::string jsonStr = jsonOutput.dump(2);
	if (!writeFileContents(outputFile, jsonStr))
	{
		cleanupScriptLists(scriptLists, MAX_PLAYER_COUNT);
		return 1;
	}

	printf("Successfully wrote %s\n", outputFile);

	// Print any warnings
	if (writer.hasWarnings())
	{
		printf("\nWarnings (%zu):\n", writer.getWarnings().size());
		for (const std::string& warning : writer.getWarnings())
		{
			printf("  - %s\n", warning.c_str());
		}
	}

	// Clean up
	cleanupScriptLists(scriptLists, MAX_PLAYER_COUNT);

	return 0;
}

//-----------------------------------------------------------------------------
// convertToSCB
//-----------------------------------------------------------------------------
static int convertToSCB(const char* inputFile, const char* outputFile)
{
	printf("Converting %s to SCB...\n", inputFile);

	// Read the input JSON file
	std::string jsonData;
	if (!readFileContents(inputFile, jsonData))
	{
		return 1;
	}

	// Parse JSON and create script objects
	SemanticScriptReader reader;
	ScriptList* scriptLists[MAX_PLAYER_COUNT];
	int numPlayers = 0;

	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
	{
		scriptLists[i] = NULL;
	}

	// Parse teams from JSON if present
	std::vector<Dict> teams;
	if (!reader.parseScriptsFile(jsonData.c_str(), jsonData.size(), scriptLists, &numPlayers, &teams))
	{
		fprintf(stderr, "Error parsing JSON: %s\n", reader.getLastError().c_str());
		return 1;
	}

	// Print any warnings from parsing
	if (reader.hasWarnings())
	{
		printf("\nParsing warnings (%zu):\n", reader.getWarnings().size());
		for (const std::string& warning : reader.getWarnings())
		{
			printf("  - %s\n", warning.c_str());
		}
	}

	if (!teams.empty())
	{
		printf("Parsed %zu teams from JSON\n", teams.size());
	}

	// Write to SCB binary format using the game's chunk writing system
	FILE* outFile = fopen(outputFile, "wb");
	if (!outFile)
	{
		fprintf(stderr, "Error: Cannot open output file '%s'\n", outputFile);
		cleanupScriptLists(scriptLists, MAX_PLAYER_COUNT);
		return 1;
	}

	SimpleFileOutputStream fileStream(outFile);

	// Use a block scope to ensure DataChunkOutput destructor writes before fclose
	{
		DataChunkOutput output(&fileStream);
		ScriptList::WriteScriptsDataChunk(output, scriptLists, numPlayers);

		// Write ScriptTeams chunk if teams were parsed
		if (!teams.empty())
		{
			output.openDataChunk("ScriptTeams", 1);
			for (const Dict& team : teams)
			{
				output.writeDict(team);
			}
			output.closeDataChunk();
		}
	}

	fclose(outFile);

	printf("Successfully wrote %s\n", outputFile);

	// Clean up
	cleanupScriptLists(scriptLists, MAX_PLAYER_COUNT);

	return 0;
}

//-----------------------------------------------------------------------------
// Map chunk parsing callbacks
//-----------------------------------------------------------------------------

static MapData g_mapData;

static Bool parseWorldInfoChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	g_mapData.worldInfo = new Dict();
	Dict d = file.readDict();
	*g_mapData.worldInfo = d;
	return true;
}

static Bool parseObjectsListChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);

static Bool parseObjectChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	MapObjectInfo obj;
	obj.x = file.readReal();
	obj.y = file.readReal();
	obj.z = file.readReal();
	obj.angle = file.readReal();
	obj.flags = file.readInt();

	AsciiString name = file.readAsciiString();
	obj.name = name.str() ? name.str() : "";

	obj.properties = new Dict();
	Dict d = file.readDict();
	*obj.properties = d;

	g_mapData.objects.push_back(obj);
	return true;
}

static Bool parseObjectsListChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	file.registerParser("Object", info->label, parseObjectChunk);
	return true;
}

static Bool parseWaypointsListChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Int numLinks = file.readInt();
	for (Int i = 0; i < numLinks; i++)
	{
		WaypointLinkInfo link;
		link.waypoint1 = file.readInt();
		link.waypoint2 = file.readInt();
		g_mapData.waypointLinks.push_back(link);
	}
	return true;
}

static Bool parseGlobalLightingChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	g_mapData.timeOfDay = file.readInt();

	for (int i = 0; i < 4; i++)
	{
		g_mapData.lighting[i].terrainLights[0].ambientR = file.readReal();
		g_mapData.lighting[i].terrainLights[0].ambientG = file.readReal();
		g_mapData.lighting[i].terrainLights[0].ambientB = file.readReal();
		g_mapData.lighting[i].terrainLights[0].diffuseR = file.readReal();
		g_mapData.lighting[i].terrainLights[0].diffuseG = file.readReal();
		g_mapData.lighting[i].terrainLights[0].diffuseB = file.readReal();
		g_mapData.lighting[i].terrainLights[0].posX = file.readReal();
		g_mapData.lighting[i].terrainLights[0].posY = file.readReal();
		g_mapData.lighting[i].terrainLights[0].posZ = file.readReal();

		g_mapData.lighting[i].objectLights[0].ambientR = file.readReal();
		g_mapData.lighting[i].objectLights[0].ambientG = file.readReal();
		g_mapData.lighting[i].objectLights[0].ambientB = file.readReal();
		g_mapData.lighting[i].objectLights[0].diffuseR = file.readReal();
		g_mapData.lighting[i].objectLights[0].diffuseG = file.readReal();
		g_mapData.lighting[i].objectLights[0].diffuseB = file.readReal();
		g_mapData.lighting[i].objectLights[0].posX = file.readReal();
		g_mapData.lighting[i].objectLights[0].posY = file.readReal();
		g_mapData.lighting[i].objectLights[0].posZ = file.readReal();

		if (info->version >= 3)
		{
			// Additional object lights [1-2]
			for (int j = 1; j < 3; j++)
			{
				g_mapData.lighting[i].objectLights[j].ambientR = file.readReal();
				g_mapData.lighting[i].objectLights[j].ambientG = file.readReal();
				g_mapData.lighting[i].objectLights[j].ambientB = file.readReal();
				g_mapData.lighting[i].objectLights[j].diffuseR = file.readReal();
				g_mapData.lighting[i].objectLights[j].diffuseG = file.readReal();
				g_mapData.lighting[i].objectLights[j].diffuseB = file.readReal();
				g_mapData.lighting[i].objectLights[j].posX = file.readReal();
				g_mapData.lighting[i].objectLights[j].posY = file.readReal();
				g_mapData.lighting[i].objectLights[j].posZ = file.readReal();
			}

			// Additional terrain lights [1-2]
			for (int j = 1; j < 3; j++)
			{
				g_mapData.lighting[i].terrainLights[j].ambientR = file.readReal();
				g_mapData.lighting[i].terrainLights[j].ambientG = file.readReal();
				g_mapData.lighting[i].terrainLights[j].ambientB = file.readReal();
				g_mapData.lighting[i].terrainLights[j].diffuseR = file.readReal();
				g_mapData.lighting[i].terrainLights[j].diffuseG = file.readReal();
				g_mapData.lighting[i].terrainLights[j].diffuseB = file.readReal();
				g_mapData.lighting[i].terrainLights[j].posX = file.readReal();
				g_mapData.lighting[i].terrainLights[j].posY = file.readReal();
				g_mapData.lighting[i].terrainLights[j].posZ = file.readReal();
			}
		}
	}

	// Read shadowColor (version 3)
	if (info->version >= 3)
		g_mapData.shadowColor = file.readInt();

	return true;
}

static Bool captureHeightMapChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	g_mapData.heightMapVersion = info->version;
	Int size = info->dataSize;
	g_mapData.heightMapData.resize(size);
	file.readArrayOfBytes((char*)g_mapData.heightMapData.data(), size);
	return true;
}

static Bool captureBlendTileChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	g_mapData.blendTileVersion = info->version;
	Int size = info->dataSize;
	g_mapData.blendTileData.resize(size);
	file.readArrayOfBytes((char*)g_mapData.blendTileData.data(), size);
	return true;
}

//-----------------------------------------------------------------------------
// convertMapToJSON
//-----------------------------------------------------------------------------
static int convertMapToJSON(const char* inputFile, const char* outputFile)
{
	printf("Converting map %s to JSON...\n", inputFile);

	std::string mapFileData;
	if (!readFileContents(inputFile, mapFileData))
	{
		return 1;
	}

	// Decompress if needed (map files are often RefPack compressed)
	if (!decompressIfNeeded(mapFileData))
	{
		fprintf(stderr, "Error: Failed to decompress map file\n");
		return 1;
	}

	g_mapData.clear();
	PolygonTrigger::deleteTriggers();

	MemoryInputStream memStream((const unsigned char*)mapFileData.data(), mapFileData.size());
	DataChunkInput input(&memStream);

	if (!input.isValidFileType())
	{
		fprintf(stderr, "Error: Invalid map file format\n");
		return 1;
	}

	input.registerParser("WorldInfo", AsciiString::TheEmptyString, parseWorldInfoChunk);
	input.registerParser("ObjectsList", AsciiString::TheEmptyString, parseObjectsListChunk);
	input.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
	input.registerParser("WaypointsList", AsciiString::TheEmptyString, parseWaypointsListChunk);
	input.registerParser("GlobalLighting", AsciiString::TheEmptyString, parseGlobalLightingChunk);
	input.registerParser("SidesList", AsciiString::TheEmptyString, SidesList::ParseSidesDataChunk);
	input.registerParser("PlayerScriptsList", "SidesList", ScriptList::ParseScriptsDataChunk);

	if (!input.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse map file\n");
		return 1;
	}

	g_mapData.numPlayers = ScriptList::getReadScripts(g_mapData.scriptLists);

	printf("  Objects: %zu\n", g_mapData.objects.size());
	printf("  Waypoint links: %zu\n", g_mapData.waypointLinks.size());

	int triggerCount = countPolygonTriggers();
	printf("  Polygon triggers: %d\n", triggerCount);
	printf("  Script players: %d\n", g_mapData.numPlayers);

	SemanticMapWriter writer;
	nlohmann::ordered_json jsonOutput = writer.writeMapFile(g_mapData);

	std::string jsonStr = jsonOutput.dump(2);
	if (!writeFileContents(outputFile, jsonStr))
	{
		return 1;
	}

	printf("Successfully wrote %s (%zu bytes)\n", outputFile, jsonStr.size());

	if (writer.hasWarnings())
	{
		printf("\nWarnings (%zu):\n", writer.getWarnings().size());
		for (const std::string& warning : writer.getWarnings())
		{
			printf("  - %s\n", warning.c_str());
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// convertJSONToMap
//-----------------------------------------------------------------------------
static int convertJSONToMap(const char* jsonFile, const char* baseMapFile, const char* outputFile)
{
	printf("Merging %s with %s to create %s...\n", jsonFile, baseMapFile, outputFile);

	// 1. Read and parse JSON
	std::string jsonData;
	if (!readFileContents(jsonFile, jsonData))
	{
		return 1;
	}

	SemanticMapReader reader;
	MapData mapData;
	if (!reader.parseMapFile(jsonData.c_str(), jsonData.size(), mapData))
	{
		fprintf(stderr, "Error parsing JSON: %s\n", reader.getLastError().c_str());
		return 1;
	}

	printf("  Parsed JSON: %zu objects, %zu waypoint links\n",
	       mapData.objects.size(), mapData.waypointLinks.size());

	// 2. Read binary chunks from base map
	std::string baseData;
	if (!readFileContents(baseMapFile, baseData))
	{
		return 1;
	}

	// Decompress if needed
	if (!decompressIfNeeded(baseData))
	{
		fprintf(stderr, "Error: Failed to decompress base map file\n");
		return 1;
	}

	// Use a separate MapData to capture binary chunks without clearing parsed JSON data
	g_mapData.heightMapData.clear();
	g_mapData.blendTileData.clear();

	MemoryInputStream baseStream((const unsigned char*)baseData.data(), baseData.size());
	DataChunkInput baseInput(&baseStream);

	if (!baseInput.isValidFileType())
	{
		fprintf(stderr, "Error: Invalid base map file format\n");
		return 1;
	}

	baseInput.registerParser("HeightMapData", AsciiString::TheEmptyString, captureHeightMapChunk);
	baseInput.registerParser("BlendTileData", AsciiString::TheEmptyString, captureBlendTileChunk);

	if (!baseInput.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to read base map\n");
		return 1;
	}

	// Transfer binary chunks to mapData
	mapData.heightMapData = std::move(g_mapData.heightMapData);
	mapData.blendTileData = std::move(g_mapData.blendTileData);
	mapData.heightMapVersion = g_mapData.heightMapVersion;
	mapData.blendTileVersion = g_mapData.blendTileVersion;

	if (mapData.heightMapData.empty())
	{
		fprintf(stderr, "Error: HeightMapData not found in base map\n");
		return 1;
	}
	if (mapData.blendTileData.empty())
	{
		fprintf(stderr, "Error: BlendTileData not found in base map\n");
		return 1;
	}

	printf("  Captured HeightMapData: %zu bytes (v%d)\n",
	       mapData.heightMapData.size(), mapData.heightMapVersion);
	printf("  Captured BlendTileData: %zu bytes (v%d)\n",
	       mapData.blendTileData.size(), mapData.blendTileVersion);

	// 3. Write combined output
	FILE* outFile = fopen(outputFile, "wb");
	if (!outFile)
	{
		fprintf(stderr, "Error: Cannot open output file '%s'\n", outputFile);
		return 1;
	}

	SimpleFileOutputStream fileStream(outFile);
	SemanticMapFileWriter writer;

	// Use a block scope to ensure DataChunkOutput destructor writes before fclose
	{
		DataChunkOutput output(&fileStream);

		if (!writer.writeMapFile(mapData, output))
		{
			fprintf(stderr, "Error: Failed to write map data\n");
			fclose(outFile);
			return 1;
		}
	}
	// DataChunkOutput destructor has now written the table of contents and temp file contents

	fclose(outFile);

	printf("Successfully wrote %s\n", outputFile);

	if (reader.hasWarnings())
	{
		printf("\nWarnings (%zu):\n", reader.getWarnings().size());
		for (const std::string& warning : reader.getWarnings())
		{
			printf("  - %s\n", warning.c_str());
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Comparison functions for semantic equivalence testing
//-----------------------------------------------------------------------------

static bool compareParameters(Parameter* p1, Parameter* p2, const char* context)
{
	if (p1 == NULL && p2 == NULL)
		return true;
	if (p1 == NULL || p2 == NULL)
	{
		printf("  DIFF [%s]: One parameter is NULL\n", context);
		return false;
	}

	if (p1->getParameterType() != p2->getParameterType())
	{
		printf("  DIFF [%s]: Parameter type mismatch: %d vs %d\n",
		       context, p1->getParameterType(), p2->getParameterType());
		return false;
	}

	switch (p1->getParameterType())
	{
		case Parameter::INT:
		case Parameter::BOOLEAN:
		case Parameter::COMPARISON:
		case Parameter::RELATION:
		case Parameter::SIDE:
		case Parameter::AI_MOOD:
		case Parameter::KIND_OF_PARAM:
		case Parameter::RADAR_EVENT_TYPE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::BOUNDARY:
		case Parameter::BUILDABLE:
		case Parameter::SURFACES_ALLOWED:
		case Parameter::SHAKE_INTENSITY:
		case Parameter::COLOR:
		case Parameter::LEFT_OR_RIGHT:
			if (p1->getInt() != p2->getInt())
			{
				printf("  DIFF [%s]: Int value mismatch: %d vs %d\n",
				       context, p1->getInt(), p2->getInt());
				return false;
			}
			break;

		case Parameter::REAL:
		case Parameter::ANGLE:
		case Parameter::PERCENT:
			if (std::abs(p1->getReal() - p2->getReal()) > 0.0001f)
			{
				printf("  DIFF [%s]: Real value mismatch: %f vs %f\n",
				       context, p1->getReal(), p2->getReal());
				return false;
			}
			break;

		case Parameter::COORD3D:
		{
			Coord3D c1, c2;
			p1->getCoord3D(&c1);
			p2->getCoord3D(&c2);
			if (std::abs(c1.x - c2.x) > 0.0001f ||
			    std::abs(c1.y - c2.y) > 0.0001f ||
			    std::abs(c1.z - c2.z) > 0.0001f)
			{
				printf("  DIFF [%s]: Coord3D mismatch\n", context);
				return false;
			}
			break;
		}

		default:
			if (p1->getString() != p2->getString())
			{
				printf("  DIFF [%s]: String value mismatch: '%s' vs '%s'\n",
				       context, p1->getString().str(), p2->getString().str());
				return false;
			}
			break;
	}

	return true;
}

static bool compareConditions(Condition* c1, Condition* c2, const char* scriptName);

static bool compareOrConditions(OrCondition* or1, OrCondition* or2, const char* scriptName)
{
	int orIndex = 0;
	while (or1 || or2)
	{
		if (or1 == NULL || or2 == NULL)
		{
			printf("  DIFF [%s]: OR clause count mismatch at index %d\n", scriptName, orIndex);
			return false;
		}

		if (!compareConditions(or1->getFirstAndCondition(), or2->getFirstAndCondition(), scriptName))
			return false;

		or1 = or1->getNextOrCondition();
		or2 = or2->getNextOrCondition();
		orIndex++;
	}
	return true;
}

static bool compareConditions(Condition* c1, Condition* c2, const char* scriptName)
{
	int condIndex = 0;
	while (c1 || c2)
	{
		if (c1 == NULL || c2 == NULL)
		{
			printf("  DIFF [%s]: Condition count mismatch at index %d\n", scriptName, condIndex);
			return false;
		}

		if (c1->getConditionType() != c2->getConditionType())
		{
			printf("  DIFF [%s]: Condition type mismatch at %d: %d vs %d\n",
			       scriptName, condIndex, c1->getConditionType(), c2->getConditionType());
			return false;
		}

		if (c1->getNumParameters() != c2->getNumParameters())
		{
			printf("  DIFF [%s]: Condition param count mismatch at %d: %d vs %d\n",
			       scriptName, condIndex, c1->getNumParameters(), c2->getNumParameters());
			return false;
		}

		char context[256];
		for (Int i = 0; i < c1->getNumParameters(); i++)
		{
			snprintf(context, sizeof(context), "%s/cond%d/param%d", scriptName, condIndex, i);
			if (!compareParameters(c1->getParameter(i), c2->getParameter(i), context))
				return false;
		}

		c1 = c1->getNext();
		c2 = c2->getNext();
		condIndex++;
	}
	return true;
}

static bool compareActions(ScriptAction* a1, ScriptAction* a2, const char* scriptName, const char* actionType)
{
	int actIndex = 0;
	while (a1 || a2)
	{
		if (a1 == NULL || a2 == NULL)
		{
			printf("  DIFF [%s/%s]: Action count mismatch at index %d\n", scriptName, actionType, actIndex);
			return false;
		}

		if (a1->getActionType() != a2->getActionType())
		{
			printf("  DIFF [%s/%s]: Action type mismatch at %d: %d vs %d\n",
			       scriptName, actionType, actIndex, a1->getActionType(), a2->getActionType());
			return false;
		}

		if (a1->getNumParameters() != a2->getNumParameters())
		{
			printf("  DIFF [%s/%s]: Action param count mismatch at %d: %d vs %d\n",
			       scriptName, actionType, actIndex, a1->getNumParameters(), a2->getNumParameters());
			return false;
		}

		char context[256];
		for (Int i = 0; i < a1->getNumParameters(); i++)
		{
			snprintf(context, sizeof(context), "%s/%s/act%d/param%d", scriptName, actionType, actIndex, i);
			if (!compareParameters(a1->getParameter(i), a2->getParameter(i), context))
				return false;
		}

		a1 = a1->getNext();
		a2 = a2->getNext();
		actIndex++;
	}
	return true;
}

static bool compareScripts(Script* s1, Script* s2)
{
	int scriptIndex = 0;
	while (s1 || s2)
	{
		if (s1 == NULL || s2 == NULL)
		{
			printf("  DIFF: Script count mismatch at index %d\n", scriptIndex);
			return false;
		}

		const char* name1 = s1->getName().str() ? s1->getName().str() : "";
		const char* name2 = s2->getName().str() ? s2->getName().str() : "";

		if (strcmp(name1, name2) != 0)
		{
			printf("  DIFF: Script name mismatch at %d: '%s' vs '%s'\n", scriptIndex, name1, name2);
			return false;
		}

		if (s1->isActive() != s2->isActive())
		{
			printf("  DIFF [%s]: isActive mismatch\n", name1);
			return false;
		}
		if (s1->isOneShot() != s2->isOneShot())
		{
			printf("  DIFF [%s]: isOneShot mismatch\n", name1);
			return false;
		}
		if (s1->isSubroutine() != s2->isSubroutine())
		{
			printf("  DIFF [%s]: isSubroutine mismatch\n", name1);
			return false;
		}
		if (s1->isEasy() != s2->isEasy() || s1->isNormal() != s2->isNormal() || s1->isHard() != s2->isHard())
		{
			printf("  DIFF [%s]: difficulty mismatch\n", name1);
			return false;
		}
		if (s1->getDelayEvalSeconds() != s2->getDelayEvalSeconds())
		{
			printf("  DIFF [%s]: evaluationDelay mismatch\n", name1);
			return false;
		}

		// Compare conditions
		if (!compareOrConditions(s1->getOrCondition(), s2->getOrCondition(), name1))
			return false;

		// Compare actions
		if (!compareActions(s1->getAction(), s2->getAction(), name1, "then"))
			return false;

		// Compare false actions
		if (!compareActions(s1->getFalseAction(), s2->getFalseAction(), name1, "else"))
			return false;

		s1 = s1->getNext();
		s2 = s2->getNext();
		scriptIndex++;
	}
	return true;
}

static bool compareScriptGroups(ScriptGroup* g1, ScriptGroup* g2)
{
	int groupIndex = 0;
	while (g1 || g2)
	{
		if (g1 == NULL || g2 == NULL)
		{
			printf("  DIFF: ScriptGroup count mismatch at index %d\n", groupIndex);
			return false;
		}

		const char* name1 = g1->getName().str() ? g1->getName().str() : "";
		const char* name2 = g2->getName().str() ? g2->getName().str() : "";

		if (strcmp(name1, name2) != 0)
		{
			printf("  DIFF: Group name mismatch at %d: '%s' vs '%s'\n", groupIndex, name1, name2);
			return false;
		}

		if (g1->isActive() != g2->isActive())
		{
			printf("  DIFF [group %s]: isActive mismatch\n", name1);
			return false;
		}
		if (g1->isSubroutine() != g2->isSubroutine())
		{
			printf("  DIFF [group %s]: isSubroutine mismatch\n", name1);
			return false;
		}

		// Compare scripts within the group
		if (!compareScripts(g1->getScript(), g2->getScript()))
			return false;

		g1 = g1->getNext();
		g2 = g2->getNext();
		groupIndex++;
	}
	return true;
}

static bool compareScriptLists(ScriptList* l1, ScriptList* l2, int playerIndex)
{
	if (l1 == NULL && l2 == NULL)
		return true;
	if (l1 == NULL || l2 == NULL)
	{
		printf("  DIFF: Player %d ScriptList null mismatch\n", playerIndex);
		return false;
	}

	printf("  Comparing player %d scripts...\n", playerIndex);

	// Compare top-level scripts
	if (!compareScripts(l1->getScript(), l2->getScript()))
		return false;

	// Compare script groups
	if (!compareScriptGroups(l1->getScriptGroup(), l2->getScriptGroup()))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Helper to count scripts in a list
//-----------------------------------------------------------------------------
static void countScriptListContents(ScriptList* list, int& scriptCount, int& groupCount, int& conditionCount, int& actionCount)
{
	if (!list)
		return;

	// Count top-level scripts
	Script* script = list->getScript();
	while (script)
	{
		scriptCount++;

		// Count conditions
		OrCondition* orCond = script->getOrCondition();
		while (orCond)
		{
			Condition* cond = orCond->getFirstAndCondition();
			while (cond)
			{
				conditionCount++;
				cond = cond->getNext();
			}
			orCond = orCond->getNextOrCondition();
		}

		// Count actions
		ScriptAction* action = script->getAction();
		while (action)
		{
			actionCount++;
			action = action->getNext();
		}
		action = script->getFalseAction();
		while (action)
		{
			actionCount++;
			action = action->getNext();
		}

		script = script->getNext();
	}

	// Count groups and their scripts
	ScriptGroup* group = list->getScriptGroup();
	while (group)
	{
		groupCount++;

		Script* groupScript = group->getScript();
		while (groupScript)
		{
			scriptCount++;

			// Count conditions
			OrCondition* orCond = groupScript->getOrCondition();
			while (orCond)
			{
				Condition* cond = orCond->getFirstAndCondition();
				while (cond)
				{
					conditionCount++;
					cond = cond->getNext();
				}
				orCond = orCond->getNextOrCondition();
			}

			// Count actions
			ScriptAction* action = groupScript->getAction();
			while (action)
			{
				actionCount++;
				action = action->getNext();
			}
			action = groupScript->getFalseAction();
			while (action)
			{
				actionCount++;
				action = action->getNext();
			}

			groupScript = groupScript->getNext();
		}

		group = group->getNext();
	}
}

//-----------------------------------------------------------------------------
// Semantic validation - check templates and parameter counts
//-----------------------------------------------------------------------------
struct SemanticIssue
{
	std::string scriptName;
	std::string type;       // "action" or "condition"
	int typeId;
	std::string description;
};

// TheSuperHackers @bobtista Check a single script for semantic issues
static void validateScriptSemantic(Script* script, std::vector<SemanticIssue>& issues)
{
	if (!script)
		return;

	std::string scriptName = script->getName().str();

	// Validate conditions
	OrCondition* orCond = script->getOrCondition();
	while (orCond)
	{
		Condition* cond = orCond->getFirstAndCondition();
		while (cond)
		{
			int condType = cond->getConditionType();
			const ConditionTemplate* tmpl = TheScriptEngine->getConditionTemplate(condType);

			// Templates are always allocated but may be uninitialized (empty name)
			if (!tmpl || tmpl->m_internalName.isEmpty())
			{
				SemanticIssue issue;
				issue.scriptName = scriptName;
				issue.type = "condition";
				issue.typeId = condType;
				issue.description = "No template defined for condition type " + std::to_string(condType);
				issues.push_back(issue);
			}
			else
			{
				int expectedParams = tmpl->getNumParameters();
				int actualParams = cond->getNumParameters();
				if (actualParams != expectedParams)
				{
					SemanticIssue issue;
					issue.scriptName = scriptName;
					issue.type = "condition";
					issue.typeId = condType;
					issue.description = "Parameter count mismatch for condition " + std::to_string(condType) +
					                    " (" + std::string(tmpl->m_internalName.str()) + ")" +
					                    ": expected " + std::to_string(expectedParams) +
					                    ", got " + std::to_string(actualParams);
					issues.push_back(issue);
				}
			}
			cond = cond->getNext();
		}
		orCond = orCond->getNextOrCondition();
	}

	// Validate actions
	ScriptAction* action = script->getAction();
	while (action)
	{
		int actionType = action->getActionType();
		const ActionTemplate* tmpl = TheScriptEngine->getActionTemplate(actionType);

		// Templates are always allocated but may be uninitialized (empty name)
		if (!tmpl || tmpl->m_internalName.isEmpty())
		{
			SemanticIssue issue;
			issue.scriptName = scriptName;
			issue.type = "action";
			issue.typeId = actionType;
			issue.description = "No template defined for action type " + std::to_string(actionType);
			issues.push_back(issue);
		}
		else
		{
			int expectedParams = tmpl->getNumParameters();
			int actualParams = action->getNumParameters();
			if (actualParams != expectedParams)
			{
				SemanticIssue issue;
				issue.scriptName = scriptName;
				issue.type = "action";
				issue.typeId = actionType;
				issue.description = "Parameter count mismatch for action " + std::to_string(actionType) +
				                    " (" + std::string(tmpl->m_internalName.str()) + ")" +
				                    ": expected " + std::to_string(expectedParams) +
				                    ", got " + std::to_string(actualParams);
				issues.push_back(issue);
			}
		}
		action = action->getNext();
	}

	// Validate false actions
	action = script->getFalseAction();
	while (action)
	{
		int actionType = action->getActionType();
		const ActionTemplate* tmpl = TheScriptEngine->getActionTemplate(actionType);

		// Templates are always allocated but may be uninitialized (empty name)
		if (!tmpl || tmpl->m_internalName.isEmpty())
		{
			SemanticIssue issue;
			issue.scriptName = scriptName;
			issue.type = "falseAction";
			issue.typeId = actionType;
			issue.description = "No template defined for action type " + std::to_string(actionType);
			issues.push_back(issue);
		}
		else
		{
			int expectedParams = tmpl->getNumParameters();
			int actualParams = action->getNumParameters();
			if (actualParams != expectedParams)
			{
				SemanticIssue issue;
				issue.scriptName = scriptName;
				issue.type = "falseAction";
				issue.typeId = actionType;
				issue.description = "Parameter count mismatch for action " + std::to_string(actionType) +
				                    " (" + std::string(tmpl->m_internalName.str()) + ")" +
				                    ": expected " + std::to_string(expectedParams) +
				                    ", got " + std::to_string(actualParams);
				issues.push_back(issue);
			}
		}
		action = action->getNext();
	}
}

// TheSuperHackers @bobtista Check all scripts in a list for semantic issues
static void validateScriptListSemantic(ScriptList* list, std::vector<SemanticIssue>& issues)
{
	if (!list)
		return;

	// Validate top-level scripts
	Script* script = list->getScript();
	while (script)
	{
		validateScriptSemantic(script, issues);
		script = script->getNext();
	}

	// Validate scripts in groups
	ScriptGroup* group = list->getScriptGroup();
	while (group)
	{
		Script* groupScript = group->getScript();
		while (groupScript)
		{
			validateScriptSemantic(groupScript, issues);
			groupScript = groupScript->getNext();
		}
		group = group->getNext();
	}
}

// TheSuperHackers @bobtista Perform semantic validation on all script lists
static int performSemanticValidation(ScriptList* scriptLists[], int numPlayers)
{
	std::vector<SemanticIssue> issues;

	for (int i = 0; i < numPlayers; i++)
	{
		if (scriptLists[i])
		{
			validateScriptListSemantic(scriptLists[i], issues);
		}
	}

	if (issues.empty())
	{
		printf("\nSemantic validation: PASSED\n");
		return 0;
	}

	printf("\nSemantic validation: FAILED (%zu issues)\n", issues.size());

	// Count by type
	int missingTemplates = 0;
	int paramMismatches = 0;
	for (const auto& issue : issues)
	{
		if (issue.description.find("No template") != std::string::npos)
			missingTemplates++;
		else
			paramMismatches++;
	}

	if (missingTemplates > 0)
		printf("  Missing templates: %d\n", missingTemplates);
	if (paramMismatches > 0)
		printf("  Parameter mismatches: %d\n", paramMismatches);

	// Print first 10 issues
	int maxToShow = std::min((int)issues.size(), 10);
	printf("\nFirst %d issues:\n", maxToShow);
	for (int i = 0; i < maxToShow; i++)
	{
		const auto& issue = issues[i];
		printf("  [%s] %s: %s\n", issue.scriptName.c_str(), issue.type.c_str(), issue.description.c_str());
	}

	if ((int)issues.size() > maxToShow)
	{
		printf("  ... and %zu more issues\n", issues.size() - maxToShow);
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Validation summary helpers for verify functions
//-----------------------------------------------------------------------------
struct ScriptStats
{
	int numPlayers;
	int totalScripts;
	int totalGroups;
	int totalConditions;
	int totalActions;
};

static ScriptStats getScriptStats(ScriptList* scriptLists[], int numPlayers)
{
	ScriptStats stats = {numPlayers, 0, 0, 0, 0};
	for (int i = 0; i < numPlayers; i++)
	{
		if (scriptLists[i])
		{
			int scripts = 0, groups = 0, conditions = 0, actions = 0;
			countScriptListContents(scriptLists[i], scripts, groups, conditions, actions);
			stats.totalScripts += scripts;
			stats.totalGroups += groups;
			stats.totalConditions += conditions;
			stats.totalActions += actions;
		}
	}
	return stats;
}

static void printScriptValidationSummary(const ScriptStats& stats, const char* label)
{
	printf("  [%s] Players: %d\n", label, stats.numPlayers);
	printf("  [%s] Total: %d scripts, %d groups, %d conditions, %d actions\n",
	       label, stats.totalScripts, stats.totalGroups, stats.totalConditions, stats.totalActions);
}

static bool validateScriptStats(const ScriptStats& original, const ScriptStats& roundtrip)
{
	bool valid = true;

	if (original.numPlayers != roundtrip.numPlayers)
	{
		printf("  VALIDATION ERROR: Player count mismatch: %d vs %d\n",
		       original.numPlayers, roundtrip.numPlayers);
		valid = false;
	}

	if (original.totalScripts != roundtrip.totalScripts)
	{
		printf("  VALIDATION ERROR: Script count mismatch: %d vs %d\n",
		       original.totalScripts, roundtrip.totalScripts);
		valid = false;
	}

	if (original.totalGroups != roundtrip.totalGroups)
	{
		printf("  VALIDATION ERROR: Group count mismatch: %d vs %d\n",
		       original.totalGroups, roundtrip.totalGroups);
		valid = false;
	}

	if (original.totalConditions != roundtrip.totalConditions)
	{
		printf("  VALIDATION ERROR: Condition count mismatch: %d vs %d\n",
		       original.totalConditions, roundtrip.totalConditions);
		valid = false;
	}

	if (original.totalActions != roundtrip.totalActions)
	{
		printf("  VALIDATION ERROR: Action count mismatch: %d vs %d\n",
		       original.totalActions, roundtrip.totalActions);
		valid = false;
	}

	return valid;
}

//-----------------------------------------------------------------------------
// validateFile - Validate an SCB, MAP, or JSON file
//-----------------------------------------------------------------------------
static int validateFile(const char* inputFile)
{
	printf("Validating %s...\n", inputFile);

	std::string fileData;
	if (!readFileContents(inputFile, fileData))
	{
		return 1;
	}

	printf("  File size: %zu bytes\n", fileData.size());

	bool isJson = hasExtension(inputFile, ".json");
	bool isMap = hasExtension(inputFile, ".map");

	if (isMap)
	{
		// Parse as MAP binary
		printf("  Format: MAP binary\n");

		// Decompress if needed
		if (!decompressIfNeeded(fileData))
		{
			fprintf(stderr, "FAILED: Could not decompress map file\n");
			return 1;
		}

		g_mapData.clear();
		PolygonTrigger::deleteTriggers();

		MemoryInputStream memStream((const unsigned char*)fileData.data(), fileData.size());
		DataChunkInput input(&memStream);

		if (!input.isValidFileType())
		{
			fprintf(stderr, "FAILED: Invalid map file format\n");
			return 1;
		}

		input.registerParser("HeightMapData", AsciiString::TheEmptyString, captureHeightMapChunk);
		input.registerParser("BlendTileData", AsciiString::TheEmptyString, captureBlendTileChunk);
		input.registerParser("WorldInfo", AsciiString::TheEmptyString, parseWorldInfoChunk);
		input.registerParser("ObjectsList", AsciiString::TheEmptyString, parseObjectsListChunk);
		input.registerParser("Object", "ObjectsList", parseObjectChunk);
		input.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
		input.registerParser("WaypointsList", AsciiString::TheEmptyString, parseWaypointsListChunk);
		input.registerParser("GlobalLighting", AsciiString::TheEmptyString, parseGlobalLightingChunk);
		input.registerParser("SidesList", AsciiString::TheEmptyString, SidesList::ParseSidesDataChunk);
		input.registerParser("PlayerScriptsList", "SidesList", ScriptList::ParseScriptsDataChunk);

		if (!input.parse(NULL))
		{
			fprintf(stderr, "FAILED: MAP parse error\n");
			return 1;
		}

		// Show map statistics
		printf("\nMap Statistics:\n");
		printf("  HeightMapData: %zu bytes (v%d)\n", g_mapData.heightMapData.size(), g_mapData.heightMapVersion);
		printf("  BlendTileData: %zu bytes (v%d)\n", g_mapData.blendTileData.size(), g_mapData.blendTileVersion);
		printf("  Objects: %zu\n", g_mapData.objects.size());
		printf("  Polygon triggers: %d\n", countPolygonTriggers());
		printf("  Waypoint links: %zu\n", g_mapData.waypointLinks.size());

		// Count waypoints among objects
		int waypointCount = 0;
		for (const auto& obj : g_mapData.objects)
		{
			if (obj.flags & 0x200)  // FLAG_WAYPOINT
				waypointCount++;
		}
		printf("  Waypoints: %d\n", waypointCount);

		// Script statistics
		g_mapData.numPlayers = ScriptList::getReadScripts(g_mapData.scriptLists);
		if (g_mapData.numPlayers > 0)
		{
			printf("\nScript Statistics:\n");
			printf("  Players: %d\n", g_mapData.numPlayers);

			int totalScripts = 0, totalGroups = 0, totalConditions = 0, totalActions = 0;
			for (int i = 0; i < g_mapData.numPlayers; i++)
			{
				if (g_mapData.scriptLists[i])
				{
					int scripts = 0, groups = 0, conditions = 0, actions = 0;
					countScriptListContents(g_mapData.scriptLists[i], scripts, groups, conditions, actions);
					if (scripts > 0 || groups > 0)
					{
						printf("  Player %d: %d scripts, %d groups, %d conditions, %d actions\n",
						       i, scripts, groups, conditions, actions);
					}
					totalScripts += scripts;
					totalGroups += groups;
					totalConditions += conditions;
					totalActions += actions;
				}
			}
			printf("  --------\n");
			printf("  Total: %d scripts, %d groups, %d conditions, %d actions\n",
			       totalScripts, totalGroups, totalConditions, totalActions);

			// Perform semantic validation on map scripts
			int semanticResult = performSemanticValidation(g_mapData.scriptLists, g_mapData.numPlayers);
			if (semanticResult != 0)
			{
				fprintf(stderr, "\nFAILED: Map semantic validation errors found.\n");
				return 1;
			}
		}

		printf("\nSUCCESS: Map file is valid.\n");
		return 0;
	}

	if (isJson)
	{
		// Detect JSON type by parsing and checking for key fields
		try
		{
			nlohmann::ordered_json root = nlohmann::ordered_json::parse(fileData);

			if (!root.is_object())
			{
				fprintf(stderr, "FAILED: JSON root is not an object\n");
				return 1;
			}

			// Check if it's a map JSON (has "objects" or "lighting") or script JSON (has "players")
			bool isMapJson = root.contains("objects") || root.contains("lighting") || root.contains("waypointLinks");
			bool isScriptJson = root.contains("players");

			if (isMapJson)
			{
				printf("  Format: Map JSON\n");

				SemanticMapReader reader;
				MapData mapData;

				if (!reader.parseMapFile(fileData.c_str(), fileData.size(), mapData))
				{
					fprintf(stderr, "FAILED: Map JSON parse error: %s\n", reader.getLastError().c_str());
					return 1;
				}

				// Show map JSON statistics
				printf("\nMap JSON Statistics:\n");
				printf("  Objects: %zu\n", mapData.objects.size());
				printf("  Waypoint links: %zu\n", mapData.waypointLinks.size());

				int triggerCount = countPolygonTriggers();
				printf("  Polygon triggers: %d\n", triggerCount);

				if (mapData.numPlayers > 0)
				{
					printf("  Script players: %d\n", mapData.numPlayers);

					int totalScripts = 0, totalGroups = 0, totalConditions = 0, totalActions = 0;
					for (int i = 0; i < mapData.numPlayers; i++)
					{
						if (mapData.scriptLists[i])
						{
							int scripts = 0, groups = 0, conditions = 0, actions = 0;
							countScriptListContents(mapData.scriptLists[i], scripts, groups, conditions, actions);
							totalScripts += scripts;
							totalGroups += groups;
							totalConditions += conditions;
							totalActions += actions;
						}
					}
					printf("  Total scripts: %d scripts, %d groups, %d conditions, %d actions\n",
					       totalScripts, totalGroups, totalConditions, totalActions);

					// Perform semantic validation on map JSON scripts
					int semanticResult = performSemanticValidation(mapData.scriptLists, mapData.numPlayers);
					if (semanticResult != 0)
					{
						fprintf(stderr, "\nFAILED: Map JSON semantic validation errors found.\n");
						return 1;
					}
				}

				if (reader.hasWarnings())
				{
					printf("\nWarnings (%zu):\n", reader.getWarnings().size());
					for (const std::string& warning : reader.getWarnings())
					{
						printf("  - %s\n", warning.c_str());
					}
				}

				printf("\nSUCCESS: Map JSON file is valid.\n");
				return 0;
			}
			else if (isScriptJson)
			{
				printf("  Format: Script JSON\n");

				SemanticScriptReader reader;
				ScriptList* scriptLists[MAX_PLAYER_COUNT];
				int numPlayers = 0;

				for (int i = 0; i < MAX_PLAYER_COUNT; i++)
					scriptLists[i] = NULL;

				if (!reader.parseScriptsFile(fileData.c_str(), fileData.size(), scriptLists, &numPlayers))
				{
					fprintf(stderr, "FAILED: Script JSON parse error: %s\n", reader.getLastError().c_str());
					return 1;
				}

				// Show statistics
				printf("\nStatistics:\n");
				printf("  Players: %d\n", numPlayers);

				int totalScripts = 0;
				int totalGroups = 0;
				int totalConditions = 0;
				int totalActions = 0;

				for (int i = 0; i < numPlayers; i++)
				{
					if (scriptLists[i])
					{
						int scripts = 0, groups = 0, conditions = 0, actions = 0;
						countScriptListContents(scriptLists[i], scripts, groups, conditions, actions);

						if (scripts > 0 || groups > 0)
						{
							printf("  Player %d: %d scripts, %d groups, %d conditions, %d actions\n",
							       i, scripts, groups, conditions, actions);
						}

						totalScripts += scripts;
						totalGroups += groups;
						totalConditions += conditions;
						totalActions += actions;
					}
				}

				printf("  --------\n");
				printf("  Total: %d scripts, %d groups, %d conditions, %d actions\n",
				       totalScripts, totalGroups, totalConditions, totalActions);

				// Perform semantic validation
				int semanticResult = performSemanticValidation(scriptLists, numPlayers);

				// Clean up
				cleanupScriptLists(scriptLists, MAX_PLAYER_COUNT);

				if (semanticResult != 0)
				{
					fprintf(stderr, "\nFAILED: Script JSON semantic validation errors found.\n");
					return 1;
				}

				printf("\nSUCCESS: Script JSON file is valid.\n");
				return 0;
			}
			else
			{
				fprintf(stderr, "FAILED: Unknown JSON format (missing 'players', 'objects', or 'lighting')\n");
				return 1;
			}
		}
		catch (const std::exception& e)
		{
			fprintf(stderr, "FAILED: JSON parse error: %s\n", e.what());
			return 1;
		}
	}

	// SCB binary validation
	printf("  Format: SCB binary\n");
	MemoryInputStream memStream((const unsigned char*)fileData.data(), fileData.size());
	DataChunkInput input(&memStream);

	if (!input.isValidFileType())
	{
		fprintf(stderr, "FAILED: Invalid SCB file format\n");
		return 1;
	}

	input.registerParser("PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk);

	if (!input.parse(NULL))
	{
		fprintf(stderr, "FAILED: SCB parse error\n");
		return 1;
	}

	ScriptList* scriptLists[MAX_PLAYER_COUNT];
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		scriptLists[i] = NULL;
	int numPlayers = ScriptList::getReadScripts(scriptLists);

	// Show statistics
	printf("\nStatistics:\n");
	printf("  Players: %d\n", numPlayers);

	int totalScripts = 0;
	int totalGroups = 0;
	int totalConditions = 0;
	int totalActions = 0;

	for (int i = 0; i < numPlayers; i++)
	{
		if (scriptLists[i])
		{
			int scripts = 0, groups = 0, conditions = 0, actions = 0;
			countScriptListContents(scriptLists[i], scripts, groups, conditions, actions);

			if (scripts > 0 || groups > 0)
			{
				printf("  Player %d: %d scripts, %d groups, %d conditions, %d actions\n",
				       i, scripts, groups, conditions, actions);
			}

			totalScripts += scripts;
			totalGroups += groups;
			totalConditions += conditions;
			totalActions += actions;
		}
	}

	printf("  --------\n");
	printf("  Total: %d scripts, %d groups, %d conditions, %d actions\n",
	       totalScripts, totalGroups, totalConditions, totalActions);

	// Perform semantic validation
	int semanticResult = performSemanticValidation(scriptLists, numPlayers);

	// Clean up
	cleanupScriptLists(scriptLists, MAX_PLAYER_COUNT);

	if (semanticResult != 0)
	{
		fprintf(stderr, "\nFAILED: Semantic validation errors found.\n");
		return 1;
	}

	printf("\nSUCCESS: File is valid.\n");
	return 0;
}

//-----------------------------------------------------------------------------
// verifyRoundtrip - Test SCB -> JSON -> SCB roundtrip
//-----------------------------------------------------------------------------
static int verifyRoundtrip(const char* inputFile)
{
	printf("Verifying roundtrip for %s...\n", inputFile);

	// Step 1: Read the original SCB file
	std::string scbData;
	if (!readFileContents(inputFile, scbData))
	{
		return 1;
	}

	printf("Step 1: Parsing original SCB file...\n");
	MemoryInputStream memStream1((const unsigned char*)scbData.data(), scbData.size());
	DataChunkInput input1(&memStream1);

	if (!input1.isValidFileType())
	{
		fprintf(stderr, "Error: Invalid SCB file format\n");
		return 1;
	}

	input1.registerParser("PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk);

	if (!input1.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse original SCB file\n");
		return 1;
	}

	ScriptList* originalLists[MAX_PLAYER_COUNT];
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		originalLists[i] = NULL;
	Int numPlayers = ScriptList::getReadScripts(originalLists);

	ScriptStats originalStats = getScriptStats(originalLists, numPlayers);
	printScriptValidationSummary(originalStats, "Original");

	// Step 2: Convert to JSON
	printf("Step 2: Converting to JSON...\n");
	SemanticScriptWriter writer;
	nlohmann::ordered_json jsonOutput = writer.writeScriptsFile(originalLists, numPlayers);
	std::string jsonStr = jsonOutput.dump(2);
	printf("  JSON size: %zu bytes\n", jsonStr.size());

	// Step 3: Parse JSON back to script objects
	printf("Step 3: Parsing JSON back to scripts...\n");
	SemanticScriptReader reader;
	ScriptList* roundtripLists[MAX_PLAYER_COUNT];
	int roundtripNumPlayers = 0;

	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		roundtripLists[i] = NULL;

	if (!reader.parseScriptsFile(jsonStr.c_str(), jsonStr.size(), roundtripLists, &roundtripNumPlayers))
	{
		fprintf(stderr, "Error parsing JSON: %s\n", reader.getLastError().c_str());
		cleanupScriptLists(originalLists, MAX_PLAYER_COUNT);
		return 1;
	}

	ScriptStats roundtripStats = getScriptStats(roundtripLists, roundtripNumPlayers);
	printScriptValidationSummary(roundtripStats, "Roundtrip");

	// Step 4: Validate counts match before detailed comparison
	printf("Step 4: Validating roundtrip data...\n");
	if (!validateScriptStats(originalStats, roundtripStats))
	{
		printf("\nFAILURE: Validation failed - counts don't match.\n");
		cleanupScriptLists(originalLists, MAX_PLAYER_COUNT);
		cleanupScriptLists(roundtripLists, MAX_PLAYER_COUNT);
		return 1;
	}
	printf("  Validation passed - counts match.\n");

	// Step 5: Compare original and roundtrip results in detail
	printf("Step 5: Comparing original and roundtrip results...\n");

	bool allMatch = true;
	for (int i = 0; i < numPlayers; i++)
	{
		if (!compareScriptLists(originalLists[i], roundtripLists[i], i))
		{
			allMatch = false;
		}
	}

	// Clean up
	cleanupScriptLists(originalLists, MAX_PLAYER_COUNT);
	cleanupScriptLists(roundtripLists, MAX_PLAYER_COUNT);

	if (allMatch)
	{
		printf("\nSUCCESS: Roundtrip verification passed! All scripts match.\n");
		return 0;
	}
	else
	{
		printf("\nFAILURE: Roundtrip verification found differences.\n");
		return 1;
	}
}

//-----------------------------------------------------------------------------
// Map comparison functions
//-----------------------------------------------------------------------------

static bool compareWaypointLinks(const std::vector<WaypointLinkInfo>& links1,
                                  const std::vector<WaypointLinkInfo>& links2)
{
	if (links1.size() != links2.size())
	{
		printf("  DIFF: Waypoint link count mismatch: %zu vs %zu\n", links1.size(), links2.size());
		return false;
	}

	for (size_t i = 0; i < links1.size(); i++)
	{
		if (links1[i].waypoint1 != links2[i].waypoint1 ||
		    links1[i].waypoint2 != links2[i].waypoint2)
		{
			printf("  DIFF: Waypoint link %zu mismatch: (%d,%d) vs (%d,%d)\n",
			       i, links1[i].waypoint1, links1[i].waypoint2,
			       links2[i].waypoint1, links2[i].waypoint2);
			return false;
		}
	}
	return true;
}

static bool compareLightValue(float v1, float v2, const char* context)
{
	if (std::abs(v1 - v2) > 0.0001f)
	{
		printf("  DIFF [%s]: Light value mismatch: %f vs %f\n", context, v1, v2);
		return false;
	}
	return true;
}

static bool compareLighting(int tod1, const LightingInfo lighting1[4], unsigned int shadow1,
                             int tod2, const LightingInfo lighting2[4], unsigned int shadow2)
{
	if (tod1 != tod2)
	{
		printf("  DIFF: Time of day mismatch: %d vs %d\n", tod1, tod2);
		return false;
	}

	if (shadow1 != shadow2)
	{
		printf("  DIFF: Shadow color mismatch: 0x%08X vs 0x%08X\n", shadow1, shadow2);
		return false;
	}

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			char ctx[64];
			snprintf(ctx, sizeof(ctx), "lighting[%d].terrain[%d]", i, j);
			if (!compareLightValue(lighting1[i].terrainLights[j].ambientR, lighting2[i].terrainLights[j].ambientR, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].ambientG, lighting2[i].terrainLights[j].ambientG, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].ambientB, lighting2[i].terrainLights[j].ambientB, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].diffuseR, lighting2[i].terrainLights[j].diffuseR, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].diffuseG, lighting2[i].terrainLights[j].diffuseG, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].diffuseB, lighting2[i].terrainLights[j].diffuseB, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].posX, lighting2[i].terrainLights[j].posX, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].posY, lighting2[i].terrainLights[j].posY, ctx) ||
			    !compareLightValue(lighting1[i].terrainLights[j].posZ, lighting2[i].terrainLights[j].posZ, ctx))
				return false;

			snprintf(ctx, sizeof(ctx), "lighting[%d].object[%d]", i, j);
			if (!compareLightValue(lighting1[i].objectLights[j].ambientR, lighting2[i].objectLights[j].ambientR, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].ambientG, lighting2[i].objectLights[j].ambientG, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].ambientB, lighting2[i].objectLights[j].ambientB, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].diffuseR, lighting2[i].objectLights[j].diffuseR, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].diffuseG, lighting2[i].objectLights[j].diffuseG, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].diffuseB, lighting2[i].objectLights[j].diffuseB, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].posX, lighting2[i].objectLights[j].posX, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].posY, lighting2[i].objectLights[j].posY, ctx) ||
			    !compareLightValue(lighting1[i].objectLights[j].posZ, lighting2[i].objectLights[j].posZ, ctx))
				return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// verifyMapRoundtrip - Test MAP -> JSON -> MAP roundtrip
//-----------------------------------------------------------------------------
static int verifyMapRoundtrip(const char* inputFile)
{
	printf("Verifying map roundtrip for %s...\n", inputFile);

	// Step 1: Read and parse the original map file
	printf("Step 1: Parsing original map file...\n");

	std::string mapFileData;
	if (!readFileContents(inputFile, mapFileData))
		return 1;

	if (!decompressIfNeeded(mapFileData))
	{
		fprintf(stderr, "Error: Failed to decompress map file\n");
		return 1;
	}

	g_mapData.clear();
	PolygonTrigger::deleteTriggers();

	MemoryInputStream memStream1((const unsigned char*)mapFileData.data(), mapFileData.size());
	DataChunkInput input1(&memStream1);

	if (!input1.isValidFileType())
	{
		fprintf(stderr, "Error: Invalid map file format\n");
		return 1;
	}

	input1.registerParser("WorldInfo", AsciiString::TheEmptyString, parseWorldInfoChunk);
	input1.registerParser("ObjectsList", AsciiString::TheEmptyString, parseObjectsListChunk);
	input1.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
	input1.registerParser("WaypointsList", AsciiString::TheEmptyString, parseWaypointsListChunk);
	input1.registerParser("GlobalLighting", AsciiString::TheEmptyString, parseGlobalLightingChunk);
	input1.registerParser("SidesList", AsciiString::TheEmptyString, SidesList::ParseSidesDataChunk);
	input1.registerParser("PlayerScriptsList", "SidesList", ScriptList::ParseScriptsDataChunk);

	if (!input1.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse map file\n");
		return 1;
	}

	g_mapData.numPlayers = ScriptList::getReadScripts(g_mapData.scriptLists);

	// Save original data for comparison
	MapData originalData;
	originalData.timeOfDay = g_mapData.timeOfDay;
	originalData.shadowColor = g_mapData.shadowColor;
	memcpy(originalData.lighting, g_mapData.lighting, sizeof(originalData.lighting));
	originalData.waypointLinks = g_mapData.waypointLinks;
	originalData.objects = g_mapData.objects;
	originalData.numPlayers = g_mapData.numPlayers;

	// Save polygon trigger info before we delete them
	// Note: We compare by name since trigger IDs are auto-assigned and not preserved in JSON
	struct TriggerInfo {
		int id;
		std::string name;
		int numPoints;
		bool isWater;
		bool isRiver;
		bool operator<(const TriggerInfo& other) const { return name < other.name; }
	};
	std::vector<TriggerInfo> originalTriggerInfo;
	for (PolygonTrigger* t = PolygonTrigger::getFirstPolygonTrigger(); t; t = t->getNext())
	{
		TriggerInfo info;
		info.id = t->getID();
		info.name = t->getTriggerName().str() ? t->getTriggerName().str() : "";
		info.numPoints = t->getNumPoints();
		info.isWater = t->isWaterArea();
		info.isRiver = t->isRiver();
		originalTriggerInfo.push_back(info);
	}
	std::sort(originalTriggerInfo.begin(), originalTriggerInfo.end());

	printf("  [Original] Waypoint links: %zu\n", originalData.waypointLinks.size());
	printf("  [Original] Polygon triggers: %zu\n", originalTriggerInfo.size());
	printf("  [Original] Objects: %zu\n", originalData.objects.size());

	// Step 2: Convert to JSON
	printf("Step 2: Converting to JSON...\n");
	SemanticMapWriter writer;
	nlohmann::ordered_json jsonOutput = writer.writeMapFile(g_mapData);
	std::string jsonStr = jsonOutput.dump(2);
	printf("  JSON size: %zu bytes\n", jsonStr.size());

	// Step 3: Parse JSON back to map data
	printf("Step 3: Parsing JSON back to map data...\n");

	// Clear global state before parsing
	PolygonTrigger::deleteTriggers();

	SemanticMapReader reader;
	MapData roundtripData;
	if (!reader.parseMapFile(jsonStr.c_str(), jsonStr.size(), roundtripData))
	{
		fprintf(stderr, "Error parsing JSON: %s\n", reader.getLastError().c_str());
		return 1;
	}

	// Gather roundtrip trigger info
	std::vector<TriggerInfo> roundtripTriggerInfo;
	for (PolygonTrigger* t = PolygonTrigger::getFirstPolygonTrigger(); t; t = t->getNext())
	{
		TriggerInfo info;
		info.id = t->getID();
		info.name = t->getTriggerName().str() ? t->getTriggerName().str() : "";
		info.numPoints = t->getNumPoints();
		info.isWater = t->isWaterArea();
		info.isRiver = t->isRiver();
		roundtripTriggerInfo.push_back(info);
	}
	std::sort(roundtripTriggerInfo.begin(), roundtripTriggerInfo.end());

	printf("  [Roundtrip] Waypoint links: %zu\n", roundtripData.waypointLinks.size());
	printf("  [Roundtrip] Polygon triggers: %zu\n", roundtripTriggerInfo.size());
	printf("  [Roundtrip] Objects: %zu\n", roundtripData.objects.size());

	// Step 4: Compare
	printf("Step 4: Comparing original and roundtrip data...\n");

	bool allMatch = true;

	// Compare waypoint links
	printf("  Comparing waypoint links...\n");
	if (!compareWaypointLinks(originalData.waypointLinks, roundtripData.waypointLinks))
		allMatch = false;

	// Compare lighting
	printf("  Comparing lighting...\n");
	if (!compareLighting(originalData.timeOfDay, originalData.lighting, originalData.shadowColor,
	                     roundtripData.timeOfDay, roundtripData.lighting, roundtripData.shadowColor))
		allMatch = false;

	// Compare polygon triggers (sorted by name, IDs are auto-assigned so not compared)
	printf("  Comparing polygon triggers...\n");
	if (originalTriggerInfo.size() != roundtripTriggerInfo.size())
	{
		printf("  DIFF: Polygon trigger count mismatch: %zu vs %zu\n",
		       originalTriggerInfo.size(), roundtripTriggerInfo.size());
		allMatch = false;
	}
	else
	{
		for (size_t i = 0; i < originalTriggerInfo.size(); i++)
		{
			const TriggerInfo& orig = originalTriggerInfo[i];
			const TriggerInfo& rt = roundtripTriggerInfo[i];

			if (orig.name != rt.name)
			{
				printf("  DIFF: Polygon trigger %zu name mismatch: '%s' vs '%s'\n",
				       i, orig.name.c_str(), rt.name.c_str());
				allMatch = false;
			}
			if (orig.numPoints != rt.numPoints)
			{
				printf("  DIFF: Polygon trigger '%s' point count mismatch: %d vs %d\n",
				       orig.name.c_str(), orig.numPoints, rt.numPoints);
				allMatch = false;
			}
			if (orig.isWater != rt.isWater)
			{
				printf("  DIFF: Polygon trigger '%s' isWater mismatch: %d vs %d\n",
				       orig.name.c_str(), orig.isWater, rt.isWater);
				allMatch = false;
			}
			if (orig.isRiver != rt.isRiver)
			{
				printf("  DIFF: Polygon trigger '%s' isRiver mismatch: %d vs %d\n",
				       orig.name.c_str(), orig.isRiver, rt.isRiver);
				allMatch = false;
			}
		}
	}

	// Compare object counts (detailed object comparison is complex due to Dict)
	if (originalData.objects.size() != roundtripData.objects.size())
	{
		printf("  DIFF: Object count mismatch: %zu vs %zu\n",
		       originalData.objects.size(), roundtripData.objects.size());
		allMatch = false;
	}

	if (allMatch)
	{
		printf("\nSUCCESS: Map roundtrip verification passed!\n");
		return 0;
	}
	else
	{
		printf("\nFAILURE: Map roundtrip verification found differences.\n");
		return 1;
	}
}

//-----------------------------------------------------------------------------
// getAbsolutePath - Convert relative path to absolute before we change directory
//-----------------------------------------------------------------------------
static std::string getAbsolutePath(const char* path)
{
	if (!path || path[0] == '\0')
		return std::string();

#ifdef _WIN32
	char absPath[MAX_PATH];
	if (GetFullPathNameA(path, MAX_PATH, absPath, NULL) > 0)
		return std::string(absPath);
#else
	char absPath[PATH_MAX];
	if (realpath(path, absPath) != NULL)
		return std::string(absPath);
#endif

	// Fallback: return original path
	return std::string(path);
}

//-----------------------------------------------------------------------------
// main
//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printUsage(argv[0]);
		return 1;
	}

	// Parse arguments
	bool verify = false;
	bool validate = false;
	const char* inputFile = NULL;
	const char* outputFile = NULL;
	const char* thirdFile = NULL;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--game-dir") == 0)
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "Error: --game-dir requires a path argument\n");
				return 1;
			}
			g_gameDir = argv[++i];
		}
		else if (strcmp(argv[i], "--verify") == 0)
		{
			verify = true;
		}
		else if (strcmp(argv[i], "--validate") == 0)
		{
			validate = true;
		}
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			printUsage(argv[0]);
			return 0;
		}
		else if (argv[i][0] == '-')
		{
			fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
			printUsage(argv[0]);
			return 1;
		}
		else if (inputFile == NULL)
		{
			inputFile = argv[i];
		}
		else if (outputFile == NULL)
		{
			outputFile = argv[i];
		}
		else if (thirdFile == NULL)
		{
			thirdFile = argv[i];
		}
		else
		{
			fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[i]);
			printUsage(argv[0]);
			return 1;
		}
	}

	// Validate arguments
	if (inputFile == NULL)
	{
		fprintf(stderr, "Error: No input file specified\n\n");
		printUsage(argv[0]);
		return 1;
	}

	if ((verify || validate) && outputFile != NULL)
	{
		fprintf(stderr, "Error: --%s does not take an output file\n", verify ? "verify" : "validate");
		printUsage(argv[0]);
		return 1;
	}

	if (!verify && !validate && outputFile == NULL)
	{
		fprintf(stderr, "Error: No output file specified\n\n");
		printUsage(argv[0]);
		return 1;
	}

	// Convert relative paths to absolute paths BEFORE we change to game directory
	// This ensures the tool can find files when run from any directory
	std::string absInputFile = getAbsolutePath(inputFile);
	std::string absOutputFile = outputFile ? getAbsolutePath(outputFile) : "";
	std::string absThirdFile = thirdFile ? getAbsolutePath(thirdFile) : "";

	inputFile = absInputFile.c_str();
	outputFile = absOutputFile.empty() ? NULL : absOutputFile.c_str();
	thirdFile = absThirdFile.empty() ? NULL : absThirdFile.c_str();

	// Determine conversion direction from file extensions
	bool toJson = false;
	bool toScb = false;
	bool mapToJson = false;
	bool jsonToMap = false;

	if (!verify && !validate)
	{
		bool inputIsScb = hasExtension(inputFile, ".scb");
		bool inputIsMap = hasExtension(inputFile, ".map");
		bool inputIsJson = hasExtension(inputFile, ".json");
		bool outputIsScb = hasExtension(outputFile, ".scb");
		bool outputIsMap = hasExtension(outputFile, ".map");
		bool outputIsJson = hasExtension(outputFile, ".json");
		bool thirdIsMap = thirdFile != NULL && hasExtension(thirdFile, ".map");

		if (inputIsScb && outputIsJson)
		{
			toJson = true;
		}
		else if (inputIsMap && outputIsJson)
		{
			mapToJson = true;
		}
		else if (inputIsJson && outputIsScb)
		{
			toScb = true;
		}
		else if (inputIsJson && outputIsMap && thirdIsMap)
		{
			// JSON + base.map + output.map = merge mode
			jsonToMap = true;
		}
		else
		{
			fprintf(stderr, "Error: Cannot determine conversion direction from file extensions.\n");
			fprintf(stderr, "       Use .scb/.map for binary and .json for JSON format.\n");
			fprintf(stderr, "       Input: %s, Output: %s\n", inputFile, outputFile);
			return 1;
		}
	}

	// Initialize minimal engine
	printf("\nInput file: %s\n", inputFile);
	if (outputFile)
		printf("Output file: %s\n", outputFile);
	fflush(stdout);

	if (!initMinimalEngine())
	{
		fprintf(stderr, "Error: Failed to initialize engine\n");
		return 1;
	}

	printf("\nStarting conversion...\n");
	fflush(stdout);

	int result = 0;

	try
	{
		if (toJson)
		{
			result = convertToJSON(inputFile, outputFile);
		}
		else if (mapToJson)
		{
			result = convertMapToJSON(inputFile, outputFile);
		}
		else if (toScb)
		{
			result = convertToSCB(inputFile, outputFile);
		}
		else if (jsonToMap)
		{
			result = convertJSONToMap(inputFile, outputFile, thirdFile);
		}
		else if (verify)
		{
			if (hasExtension(inputFile, ".map"))
				result = verifyMapRoundtrip(inputFile);
			else
				result = verifyRoundtrip(inputFile);
		}
		else if (validate)
		{
			result = validateFile(inputFile);
		}
	}
	catch (const std::exception& e)
	{
		printf("Error: %s\n", e.what());
		fflush(stdout);
		result = 1;
	}
	catch (...)
	{
		printf("Error: Unknown exception\n");
		fflush(stdout);
		result = 1;
	}

	// Shutdown engine
	shutdownEngine();

	// Print final status if no other output was given
	if (result != 0)
	{
		printf("Operation failed.\n");
		fflush(stdout);
	}

	return result;
}

//-----------------------------------------------------------------------------
// WinMain stub - required because StackDump.cpp in z_gameengine references it
//-----------------------------------------------------------------------------
#ifdef _WIN32
Int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */, LPSTR /* lpCmdLine */, Int /* nCmdShow */)
{
	ApplicationHInstance = hInstance;
	return main(__argc, __argv);
}
#endif

#else // RTS_HAS_JSON_CHUNK

#include <cstdio>

int main(int /* argc */, char* /* argv */[])
{
	fprintf(stderr, "Error: This tool requires JSON support (RTS_HAS_JSON_CHUNK)\n");
	fprintf(stderr, "Please rebuild with JSON support enabled.\n");
	return 1;
}

#endif // RTS_HAS_JSON_CHUNK
