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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Lib/BaseType.h"
#include "Common/Debug.h"
#include "Common/GameCommon.h"
#include "Common/GameMemory.h"
#include "Common/NameKeyGenerator.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/SubsystemInterface.h"
#include "Common/DataChunk.h"
#include "Common/SemanticScriptJSON.h"
#include "Common/SemanticMapJSON.h"
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

static SubsystemInterfaceList _TheSubsystemList;

template<class SUBSYSTEM>
void initSubsystem(SUBSYSTEM*& sysref, SUBSYSTEM* sys, const char* path1 = NULL, const char* path2 = NULL)
{
	sysref = sys;
	_TheSubsystemList.initSubsystem(sys, path1, path2, NULL);
}

//-----------------------------------------------------------------------------
// Global options
//-----------------------------------------------------------------------------
static std::string g_gameDir;

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
			printf("Found game directory in registry: %s\n", installPath);
			return std::string(installPath);
		}

		// Try "ErgcKey" as alternative
		if (RegQueryValueExA(hKey, "ErgcKey", NULL, &type, (LPBYTE)installPath, &pathSize) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			printf("Found game directory in registry (ErgcKey): %s\n", installPath);
			return std::string(installPath);
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
		fprintf(stderr, "Error: Cannot open file '%s' for reading\n", filename);
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
		fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
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
	}
	else
	{
		printf("Changed to game directory: %s\n", gameDir.c_str());
	}
#else
	if (chdir(gameDir.c_str()) != 0)
	{
		fprintf(stderr, "Warning: Could not change to game directory: %s\n", gameDir.c_str());
	}
	else
	{
		printf("Changed to game directory: %s\n", gameDir.c_str());
	}
#endif

	// Initialize memory manager
	initMemoryManager();

	// Initialize name key generator (required for engine)
	TheNameKeyGenerator = new NameKeyGenerator;
	TheNameKeyGenerator->init();

	// Initialize file system
	TheFileSystem = new FileSystem;

#ifdef _WIN32
	initSubsystem(TheLocalFileSystem, (LocalFileSystem*)new Win32LocalFileSystem);
#else
	initSubsystem(TheLocalFileSystem, (LocalFileSystem*)new StdLocalFileSystem);
#endif

	// Initialize script engine (needed for action/condition templates)
	// Note: Templates are loaded from Data/INI/Scripts*.ini files during INI parsing
	initSubsystem(TheScriptEngine, (ScriptEngine*)(new ScriptEngine()));

	_TheSubsystemList.postProcessLoadAll();

	// Check if templates were loaded
	const ActionTemplate* testAction = TheScriptEngine->getActionTemplate(0);
	if (!testAction || testAction->m_internalName.isEmpty())
	{
		fprintf(stderr, "Warning: Script templates not loaded. Template names will show as 'unknownAction/Condition'.\n");
		fprintf(stderr, "Make sure the game's Data/INI files are accessible from: %s\n", gameDir.c_str());
	}

	return true;
}

//-----------------------------------------------------------------------------
// shutdownEngine
//-----------------------------------------------------------------------------
static void shutdownEngine()
{
	_TheSubsystemList.shutdownAll();

	delete TheFileSystem;
	TheFileSystem = NULL;

	delete TheNameKeyGenerator;
	TheNameKeyGenerator = NULL;

	shutdownMemoryManager();
}

//-----------------------------------------------------------------------------
// convertToJSON
//-----------------------------------------------------------------------------
static int convertToJSON(const char* inputFile, const char* outputFile)
{
	printf("Converting %s to JSON...\n", inputFile);

	// Read the input SCB file
	std::string scbData;
	if (!readFileContents(inputFile, scbData))
	{
		return 1;
	}

	// Parse the SCB binary data using the game's chunk parsing system
	DataChunkInput input;
	input.open((const unsigned char*)scbData.data(), (Int)scbData.size());

	// Register the scripts parser callback
	input.registerParser("PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk);

	// Parse the file - this invokes the callback for PlayerScriptsList chunks
	if (!input.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse SCB file\n");
		input.close();
		return 1;
	}

	// Get the parsed script lists
	ScriptList* scriptLists[MAX_PLAYER_COUNT];
	Int numPlayers = ScriptList::getReadScripts(scriptLists);

	input.close();

	// Convert to semantic JSON
	SemanticScriptWriter writer;
	nlohmann::ordered_json jsonOutput = writer.writeScriptsFile(scriptLists, numPlayers);

	// Write JSON to output file
	std::string jsonStr = jsonOutput.dump(2);
	if (!writeFileContents(outputFile, jsonStr))
	{
		// Clean up
		for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		{
			if (scriptLists[i])
				delete scriptLists[i];
		}
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
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
	{
		if (scriptLists[i])
			delete scriptLists[i];
	}

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

	if (!reader.parseScriptsFile(jsonData.c_str(), jsonData.size(), scriptLists, &numPlayers))
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

	// Write to SCB binary format using the game's chunk writing system
	DataChunkOutput output;

	// WriteScriptsDataChunk handles the entire PlayerScriptsList chunk structure
	ScriptList::WriteScriptsDataChunk(output, scriptLists, numPlayers);

	// Write to output file
	std::string scbData((const char*)output.getData(), output.getDataSize());
	if (!writeFileContents(outputFile, scbData))
	{
		// Clean up
		for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		{
			if (scriptLists[i])
				delete scriptLists[i];
		}
		return 1;
	}

	printf("Successfully wrote %s\n", outputFile);

	// Clean up
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
	{
		if (scriptLists[i])
			delete scriptLists[i];
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Map chunk parsing callbacks
//-----------------------------------------------------------------------------

static MapData g_mapData;

static Bool parseWorldInfoChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	g_mapData.worldInfo = new Dict();
	file.readDict(g_mapData.worldInfo);
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
	file.readDict(obj.properties);

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
		}
	}
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

	g_mapData.clear();
	PolygonTrigger::deleteTriggers();

	DataChunkInput input;
	input.open((const unsigned char*)mapFileData.data(), (Int)mapFileData.size());

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
		input.close();
		return 1;
	}

	g_mapData.numPlayers = ScriptList::getReadScripts(g_mapData.scriptLists);

	input.close();

	printf("  Objects: %zu\n", g_mapData.objects.size());
	printf("  Waypoint links: %zu\n", g_mapData.waypointLinks.size());

	int triggerCount = 0;
	for (PolygonTrigger* t = PolygonTrigger::getFirstPolygonTrigger(); t; t = t->getNext())
		triggerCount++;
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

	// Use a separate MapData to capture binary chunks without clearing parsed JSON data
	g_mapData.heightMapData.clear();
	g_mapData.blendTileData.clear();

	DataChunkInput baseInput;
	baseInput.open((const unsigned char*)baseData.data(), (Int)baseData.size());
	baseInput.registerParser("HeightMapData", AsciiString::TheEmptyString, captureHeightMapChunk);
	baseInput.registerParser("BlendTileData", AsciiString::TheEmptyString, captureBlendTileChunk);

	if (!baseInput.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to read base map\n");
		baseInput.close();
		return 1;
	}
	baseInput.close();

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
	DataChunkOutput output;
	SemanticMapFileWriter writer;
	if (!writer.writeMapFile(mapData, output))
	{
		fprintf(stderr, "Error: Failed to write map data\n");
		return 1;
	}

	// 4. Save to file
	std::string outputData((const char*)output.getData(), output.getDataSize());
	if (!writeFileContents(outputFile, outputData))
	{
		return 1;
	}

	printf("Successfully wrote %s (%zu bytes)\n", outputFile, outputData.size());

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

static bool validateScriptData(ScriptList* scriptLists[], int numPlayers, const char* label)
{
	bool valid = true;

	for (int p = 0; p < numPlayers; p++)
	{
		if (!scriptLists[p])
			continue;

		// Check top-level scripts
		Script* script = scriptLists[p]->getScript();
		int scriptIndex = 0;
		while (script)
		{
			// Check for empty script name
			if (script->getName().isEmpty())
			{
				printf("  [%s] VALIDATION ERROR: Player %d script %d has empty name\n", label, p, scriptIndex);
				valid = false;
			}

			// Check for invalid condition types
			OrCondition* orCond = script->getOrCondition();
			while (orCond)
			{
				Condition* cond = orCond->getFirstAndCondition();
				while (cond)
				{
					if (cond->getConditionType() < 0)
					{
						printf("  [%s] VALIDATION ERROR: Player %d script '%s' has invalid condition type %d\n",
						       label, p, script->getName().str(), cond->getConditionType());
						valid = false;
					}
					cond = cond->getNext();
				}
				orCond = orCond->getNextOrCondition();
			}

			// Check for invalid action types
			ScriptAction* action = script->getAction();
			while (action)
			{
				if (action->getActionType() < 0)
				{
					printf("  [%s] VALIDATION ERROR: Player %d script '%s' has invalid action type %d\n",
					       label, p, script->getName().str(), action->getActionType());
					valid = false;
				}
				action = action->getNext();
			}

			script = script->getNext();
			scriptIndex++;
		}

		// Check groups
		ScriptGroup* group = scriptLists[p]->getScriptGroup();
		int groupIndex = 0;
		while (group)
		{
			// Check for empty group name
			if (group->getName().isEmpty())
			{
				printf("  [%s] VALIDATION ERROR: Player %d group %d has empty name\n", label, p, groupIndex);
				valid = false;
			}

			// Check scripts within group
			Script* groupScript = group->getScript();
			int gsIndex = 0;
			while (groupScript)
			{
				if (groupScript->getName().isEmpty())
				{
					printf("  [%s] VALIDATION ERROR: Player %d group '%s' script %d has empty name\n",
					       label, p, group->getName().str(), gsIndex);
					valid = false;
				}
				groupScript = groupScript->getNext();
				gsIndex++;
			}

			group = group->getNext();
			groupIndex++;
		}
	}

	return valid;
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

	// Warn if roundtrip has zero content when original had content
	if (original.totalScripts > 0 && roundtrip.totalScripts == 0)
	{
		printf("  VALIDATION ERROR: Roundtrip lost all scripts!\n");
		valid = false;
	}

	return valid;
}

struct MapStats
{
	size_t objectCount;
	size_t waypointLinkCount;
	int triggerCount;
	int waypointCount;
	int timeOfDay;
	ScriptStats scripts;
};

static MapStats getMapStats(const MapData& mapData, int triggerCount)
{
	MapStats stats;
	stats.objectCount = mapData.objects.size();
	stats.waypointLinkCount = mapData.waypointLinks.size();
	stats.triggerCount = triggerCount;
	stats.timeOfDay = mapData.timeOfDay;

	// Count waypoints
	stats.waypointCount = 0;
	for (const auto& obj : mapData.objects)
	{
		if (obj.flags & 0x200)  // FLAG_WAYPOINT
			stats.waypointCount++;
	}

	stats.scripts = getScriptStats(const_cast<ScriptList**>(mapData.scriptLists), mapData.numPlayers);
	return stats;
}

static void printMapValidationSummary(const MapStats& stats, const char* label)
{
	printf("  [%s] Objects: %zu, Waypoint links: %zu, Triggers: %d\n",
	       label, stats.objectCount, stats.waypointLinkCount, stats.triggerCount);
	printf("  [%s] Waypoints: %d, Time of day: %d\n", label, stats.waypointCount, stats.timeOfDay);

	if (stats.scripts.numPlayers > 0)
	{
		printf("  [%s] Scripts: %d scripts, %d groups, %d conditions, %d actions\n",
		       label, stats.scripts.totalScripts, stats.scripts.totalGroups,
		       stats.scripts.totalConditions, stats.scripts.totalActions);
	}
}

static bool validateMapData(const MapData& mapData, const char* label)
{
	bool valid = true;

	// Check for empty object names
	for (size_t i = 0; i < mapData.objects.size(); i++)
	{
		if (mapData.objects[i].name.empty())
		{
			printf("  [%s] VALIDATION ERROR: Object %zu has empty name\n", label, i);
			valid = false;
			break;  // Don't spam errors
		}
	}

	// Check for invalid waypoint links
	for (size_t i = 0; i < mapData.waypointLinks.size(); i++)
	{
		if (mapData.waypointLinks[i].waypoint1 < 0 || mapData.waypointLinks[i].waypoint2 < 0)
		{
			printf("  [%s] VALIDATION ERROR: Waypoint link %zu has negative ID\n", label, i);
			valid = false;
			break;
		}
	}

	// Check time of day is in valid range
	if (mapData.timeOfDay < 0 || mapData.timeOfDay > 3)
	{
		printf("  [%s] VALIDATION WARNING: Unusual time of day value: %d\n", label, mapData.timeOfDay);
	}

	return valid;
}

static bool validateMapStats(const MapStats& original, const MapStats& roundtrip)
{
	bool valid = true;

	if (original.objectCount != roundtrip.objectCount)
	{
		printf("  VALIDATION ERROR: Object count mismatch: %zu vs %zu\n",
		       original.objectCount, roundtrip.objectCount);
		valid = false;
	}

	if (original.waypointLinkCount != roundtrip.waypointLinkCount)
	{
		printf("  VALIDATION ERROR: Waypoint link count mismatch: %zu vs %zu\n",
		       original.waypointLinkCount, roundtrip.waypointLinkCount);
		valid = false;
	}

	if (original.triggerCount != roundtrip.triggerCount)
	{
		printf("  VALIDATION ERROR: Trigger count mismatch: %d vs %d\n",
		       original.triggerCount, roundtrip.triggerCount);
		valid = false;
	}

	if (original.waypointCount != roundtrip.waypointCount)
	{
		printf("  VALIDATION ERROR: Waypoint count mismatch: %d vs %d\n",
		       original.waypointCount, roundtrip.waypointCount);
		valid = false;
	}

	if (original.timeOfDay != roundtrip.timeOfDay)
	{
		printf("  VALIDATION ERROR: Time of day mismatch: %d vs %d\n",
		       original.timeOfDay, roundtrip.timeOfDay);
		valid = false;
	}

	// Warn if roundtrip lost all content
	if (original.objectCount > 0 && roundtrip.objectCount == 0)
	{
		printf("  VALIDATION ERROR: Roundtrip lost all objects!\n");
		valid = false;
	}

	// Validate script stats
	if (!validateScriptStats(original.scripts, roundtrip.scripts))
		valid = false;

	return valid;
}

//-----------------------------------------------------------------------------
// Map data comparison helpers
//-----------------------------------------------------------------------------
static bool floatEqual(float a, float b, float epsilon = 0.001f)
{
	return fabs(a - b) < epsilon;
}

static bool compareMapObjects(const std::vector<MapObjectInfo>& obj1, const std::vector<MapObjectInfo>& obj2)
{
	if (obj1.size() != obj2.size())
	{
		printf("  DIFF: Object count mismatch: %zu vs %zu\n", obj1.size(), obj2.size());
		return false;
	}

	for (size_t i = 0; i < obj1.size(); i++)
	{
		const MapObjectInfo& o1 = obj1[i];
		const MapObjectInfo& o2 = obj2[i];

		if (o1.name != o2.name)
		{
			printf("  DIFF [obj %zu]: Name mismatch: '%s' vs '%s'\n", i, o1.name.c_str(), o2.name.c_str());
			return false;
		}
		if (!floatEqual(o1.x, o2.x) || !floatEqual(o1.y, o2.y) || !floatEqual(o1.z, o2.z))
		{
			printf("  DIFF [obj %zu '%s']: Position mismatch: (%.2f,%.2f,%.2f) vs (%.2f,%.2f,%.2f)\n",
			       i, o1.name.c_str(), o1.x, o1.y, o1.z, o2.x, o2.y, o2.z);
			return false;
		}
		if (!floatEqual(o1.angle, o2.angle))
		{
			printf("  DIFF [obj %zu '%s']: Angle mismatch: %.4f vs %.4f\n", i, o1.name.c_str(), o1.angle, o2.angle);
			return false;
		}
		if (o1.flags != o2.flags)
		{
			printf("  DIFF [obj %zu '%s']: Flags mismatch: 0x%x vs 0x%x\n", i, o1.name.c_str(), o1.flags, o2.flags);
			return false;
		}
	}

	return true;
}

static bool compareWaypointLinks(const std::vector<WaypointLinkInfo>& links1, const std::vector<WaypointLinkInfo>& links2)
{
	if (links1.size() != links2.size())
	{
		printf("  DIFF: Waypoint link count mismatch: %zu vs %zu\n", links1.size(), links2.size());
		return false;
	}

	for (size_t i = 0; i < links1.size(); i++)
	{
		if (links1[i].waypoint1 != links2[i].waypoint1 || links1[i].waypoint2 != links2[i].waypoint2)
		{
			printf("  DIFF [link %zu]: Mismatch: (%d,%d) vs (%d,%d)\n",
			       i, links1[i].waypoint1, links1[i].waypoint2, links2[i].waypoint1, links2[i].waypoint2);
			return false;
		}
	}

	return true;
}

static bool compareLighting(const LightingInfo lighting1[4], const LightingInfo lighting2[4])
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			const LightingInfo::Light& t1 = lighting1[i].terrainLights[j];
			const LightingInfo::Light& t2 = lighting2[i].terrainLights[j];
			if (!floatEqual(t1.ambientR, t2.ambientR) || !floatEqual(t1.ambientG, t2.ambientG) || !floatEqual(t1.ambientB, t2.ambientB) ||
			    !floatEqual(t1.diffuseR, t2.diffuseR) || !floatEqual(t1.diffuseG, t2.diffuseG) || !floatEqual(t1.diffuseB, t2.diffuseB) ||
			    !floatEqual(t1.posX, t2.posX) || !floatEqual(t1.posY, t2.posY) || !floatEqual(t1.posZ, t2.posZ))
			{
				printf("  DIFF [lighting %d terrain %d]: Mismatch\n", i, j);
				return false;
			}

			const LightingInfo::Light& o1 = lighting1[i].objectLights[j];
			const LightingInfo::Light& o2 = lighting2[i].objectLights[j];
			if (!floatEqual(o1.ambientR, o2.ambientR) || !floatEqual(o1.ambientG, o2.ambientG) || !floatEqual(o1.ambientB, o2.ambientB) ||
			    !floatEqual(o1.diffuseR, o2.diffuseR) || !floatEqual(o1.diffuseG, o2.diffuseG) || !floatEqual(o1.diffuseB, o2.diffuseB) ||
			    !floatEqual(o1.posX, o2.posX) || !floatEqual(o1.posY, o2.posY) || !floatEqual(o1.posZ, o2.posZ))
			{
				printf("  DIFF [lighting %d object %d]: Mismatch\n", i, j);
				return false;
			}
		}
	}

	return true;
}

static bool compareMapData(const MapData& map1, const MapData& map2)
{
	bool allMatch = true;

	printf("  Comparing objects...\n");
	if (!compareMapObjects(map1.objects, map2.objects))
		allMatch = false;

	printf("  Comparing waypoint links...\n");
	if (!compareWaypointLinks(map1.waypointLinks, map2.waypointLinks))
		allMatch = false;

	printf("  Comparing lighting...\n");
	if (map1.timeOfDay != map2.timeOfDay)
	{
		printf("  DIFF: Time of day mismatch: %d vs %d\n", map1.timeOfDay, map2.timeOfDay);
		allMatch = false;
	}
	if (!compareLighting(map1.lighting, map2.lighting))
		allMatch = false;

	printf("  Comparing polygon triggers...\n");
	// Polygon triggers are stored in a global list, we compare counts here
	// More detailed comparison would require iterating the static list

	printf("  Comparing scripts...\n");
	if (map1.numPlayers != map2.numPlayers)
	{
		printf("  DIFF: Script player count mismatch: %d vs %d\n", map1.numPlayers, map2.numPlayers);
		allMatch = false;
	}
	else
	{
		for (int i = 0; i < map1.numPlayers; i++)
		{
			if (!compareScriptLists(map1.scriptLists[i], map2.scriptLists[i], i))
				allMatch = false;
		}
	}

	return allMatch;
}

//-----------------------------------------------------------------------------
// verifyMapRoundtrip - Test MAP -> JSON -> MAP roundtrip
//-----------------------------------------------------------------------------
static int verifyMapRoundtrip(const char* inputFile)
{
	printf("Verifying map roundtrip for %s...\n", inputFile);

	// Step 1: Read and parse the original MAP file
	std::string mapData;
	if (!readFileContents(inputFile, mapData))
	{
		return 1;
	}

	printf("Step 1: Parsing original MAP file...\n");

	MapData originalMap;
	originalMap.clear();
	g_mapData.clear();
	PolygonTrigger::deleteAllPolygonTriggers();

	DataChunkInput input1;
	input1.open((const unsigned char*)mapData.data(), (Int)mapData.size());
	input1.registerParser("HeightMapData", AsciiString::TheEmptyString, captureHeightMapChunk);
	input1.registerParser("BlendTileData", AsciiString::TheEmptyString, captureBlendTileChunk);
	input1.registerParser("WorldInfo", AsciiString::TheEmptyString, parseWorldInfoChunk);
	input1.registerParser("ObjectsList", AsciiString::TheEmptyString, parseObjectsListChunk);
	input1.registerParser("Object", "ObjectsList", parseObjectChunk);
	input1.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
	input1.registerParser("WaypointsList", AsciiString::TheEmptyString, parseWaypointsListChunk);
	input1.registerParser("GlobalLighting", AsciiString::TheEmptyString, parseGlobalLightingChunk);
	input1.registerParser("SidesList", AsciiString::TheEmptyString, parseSidesListChunk);
	input1.registerParser("PlayerScriptsList", "SidesList", ScriptList::ParseScriptsDataChunk);

	if (!input1.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse original MAP file\n");
		input1.close();
		return 1;
	}
	input1.close();

	// Copy parsed data to originalMap
	originalMap = g_mapData;
	originalMap.numPlayers = ScriptList::getReadScripts(originalMap.scriptLists);
	int originalTriggerCount = PolygonTrigger::getNumPolygonTriggers();

	MapStats originalStats = getMapStats(originalMap, originalTriggerCount);
	printMapValidationSummary(originalStats, "Original");

	// Validate original data
	if (!validateMapData(originalMap, "Original"))
	{
		printf("  WARNING: Original map has validation issues\n");
	}

	// Step 2: Convert to JSON
	printf("Step 2: Converting to JSON...\n");
	SemanticMapWriter writer;
	nlohmann::ordered_json jsonOutput = writer.writeMapFile(originalMap);
	std::string jsonStr = jsonOutput.dump(2);
	printf("  JSON size: %zu bytes\n", jsonStr.size());

	// Step 3: Parse JSON back to map data
	printf("Step 3: Parsing JSON back to map data...\n");
	SemanticMapReader reader;
	MapData roundtripMap;
	PolygonTrigger::deleteAllPolygonTriggers();

	if (!reader.parseMapFile(jsonStr.c_str(), jsonStr.size(), roundtripMap))
	{
		fprintf(stderr, "Error parsing JSON: %s\n", reader.getLastError().c_str());
		return 1;
	}

	// Transfer binary chunks from original (these wouldn't be in JSON)
	roundtripMap.heightMapData = originalMap.heightMapData;
	roundtripMap.blendTileData = originalMap.blendTileData;
	roundtripMap.heightMapVersion = originalMap.heightMapVersion;
	roundtripMap.blendTileVersion = originalMap.blendTileVersion;

	// Step 4: Write to binary MAP format
	printf("Step 4: Writing to binary MAP format...\n");
	DataChunkOutput output;
	SemanticMapFileWriter mapWriter;
	if (!mapWriter.writeMapFile(roundtripMap, output))
	{
		fprintf(stderr, "Error: Failed to write map data\n");
		return 1;
	}
	printf("  Binary size: %d bytes\n", output.getDataSize());

	// Step 5: Re-parse the roundtripped binary
	printf("Step 5: Re-parsing roundtripped binary...\n");
	MapData reparsedMap;
	reparsedMap.clear();
	g_mapData.clear();
	PolygonTrigger::deleteAllPolygonTriggers();

	DataChunkInput input2;
	input2.open((const unsigned char*)output.getData(), output.getDataSize());
	input2.registerParser("HeightMapData", AsciiString::TheEmptyString, captureHeightMapChunk);
	input2.registerParser("BlendTileData", AsciiString::TheEmptyString, captureBlendTileChunk);
	input2.registerParser("WorldInfo", AsciiString::TheEmptyString, parseWorldInfoChunk);
	input2.registerParser("ObjectsList", AsciiString::TheEmptyString, parseObjectsListChunk);
	input2.registerParser("Object", "ObjectsList", parseObjectChunk);
	input2.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
	input2.registerParser("WaypointsList", AsciiString::TheEmptyString, parseWaypointsListChunk);
	input2.registerParser("GlobalLighting", AsciiString::TheEmptyString, parseGlobalLightingChunk);
	input2.registerParser("SidesList", AsciiString::TheEmptyString, parseSidesListChunk);
	input2.registerParser("PlayerScriptsList", "SidesList", ScriptList::ParseScriptsDataChunk);

	if (!input2.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse roundtripped MAP\n");
		input2.close();
		return 1;
	}
	input2.close();

	reparsedMap = g_mapData;
	reparsedMap.numPlayers = ScriptList::getReadScripts(reparsedMap.scriptLists);
	int reparsedTriggerCount = PolygonTrigger::getNumPolygonTriggers();

	MapStats reparsedStats = getMapStats(reparsedMap, reparsedTriggerCount);
	printMapValidationSummary(reparsedStats, "Roundtrip");

	// Validate roundtrip data
	if (!validateMapData(reparsedMap, "Roundtrip"))
	{
		printf("\nFAILURE: Roundtrip map has validation errors\n");
		return 1;
	}

	// Step 6: Validate counts match before detailed comparison
	printf("Step 6: Validating roundtrip counts...\n");
	if (!validateMapStats(originalStats, reparsedStats))
	{
		printf("\nFAILURE: Validation failed - counts don't match.\n");
		return 1;
	}
	printf("  Validation passed - counts match.\n");

	// Step 7: Compare original and reparsed in detail
	printf("Step 7: Comparing original and roundtripped data in detail...\n");

	bool allMatch = compareMapData(originalMap, reparsedMap);

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

		g_mapData.clear();
		PolygonTrigger::deleteAllPolygonTriggers();

		DataChunkInput input;
		input.open((const unsigned char*)fileData.data(), (Int)fileData.size());
		input.registerParser("HeightMapData", AsciiString::TheEmptyString, captureHeightMapChunk);
		input.registerParser("BlendTileData", AsciiString::TheEmptyString, captureBlendTileChunk);
		input.registerParser("WorldInfo", AsciiString::TheEmptyString, parseWorldInfoChunk);
		input.registerParser("ObjectsList", AsciiString::TheEmptyString, parseObjectsListChunk);
		input.registerParser("Object", "ObjectsList", parseObjectChunk);
		input.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
		input.registerParser("WaypointsList", AsciiString::TheEmptyString, parseWaypointsListChunk);
		input.registerParser("GlobalLighting", AsciiString::TheEmptyString, parseGlobalLightingChunk);
		input.registerParser("SidesList", AsciiString::TheEmptyString, parseSidesListChunk);
		input.registerParser("PlayerScriptsList", "SidesList", ScriptList::ParseScriptsDataChunk);

		if (!input.parse(NULL))
		{
			fprintf(stderr, "FAILED: MAP parse error\n");
			input.close();
			return 1;
		}
		input.close();

		// Show map statistics
		printf("\nMap Statistics:\n");
		printf("  HeightMapData: %zu bytes (v%d)\n", g_mapData.heightMapData.size(), g_mapData.heightMapVersion);
		printf("  BlendTileData: %zu bytes (v%d)\n", g_mapData.blendTileData.size(), g_mapData.blendTileVersion);
		printf("  Objects: %zu\n", g_mapData.objects.size());
		printf("  Polygon triggers: %d\n", PolygonTrigger::getNumPolygonTriggers());
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
		}

		printf("\nSUCCESS: Map file is valid.\n");
		return 0;
	}

	// SCB/JSON validation (script files)
	ScriptList* scriptLists[MAX_PLAYER_COUNT];
	int numPlayers = 0;

	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		scriptLists[i] = NULL;

	if (isJson)
	{
		// Parse as JSON
		printf("  Format: JSON\n");
		SemanticScriptReader reader;

		if (!reader.parseScriptsFile(fileData.c_str(), fileData.size(), scriptLists, &numPlayers))
		{
			fprintf(stderr, "FAILED: JSON parse error: %s\n", reader.getLastError().c_str());
			return 1;
		}
	}
	else
	{
		// Parse as SCB binary
		printf("  Format: SCB binary\n");
		DataChunkInput input;
		input.open((const unsigned char*)fileData.data(), (Int)fileData.size());
		input.registerParser("PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk);

		if (!input.parse(NULL))
		{
			fprintf(stderr, "FAILED: SCB parse error\n");
			input.close();
			return 1;
		}

		numPlayers = ScriptList::getReadScripts(scriptLists);
		input.close();
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

	// Clean up
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
	{
		if (scriptLists[i])
			delete scriptLists[i];
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
	DataChunkInput input1;
	input1.open((const unsigned char*)scbData.data(), (Int)scbData.size());
	input1.registerParser("PlayerScriptsList", AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk);

	if (!input1.parse(NULL))
	{
		fprintf(stderr, "Error: Failed to parse original SCB file\n");
		input1.close();
		return 1;
	}

	ScriptList* originalLists[MAX_PLAYER_COUNT];
	Int numPlayers = ScriptList::getReadScripts(originalLists);
	input1.close();

	ScriptStats originalStats = getScriptStats(originalLists, numPlayers);
	printScriptValidationSummary(originalStats, "Original");

	// Validate original data
	if (!validateScriptData(originalLists, numPlayers, "Original"))
	{
		printf("  WARNING: Original scripts have validation issues\n");
	}

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
		// Clean up original lists
		for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		{
			if (originalLists[i])
				delete originalLists[i];
		}
		return 1;
	}

	ScriptStats roundtripStats = getScriptStats(roundtripLists, roundtripNumPlayers);
	printScriptValidationSummary(roundtripStats, "Roundtrip");

	// Validate roundtrip data
	if (!validateScriptData(roundtripLists, roundtripNumPlayers, "Roundtrip"))
	{
		printf("\nFAILURE: Roundtrip scripts have validation errors\n");
		// Clean up
		for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		{
			if (originalLists[i])
				delete originalLists[i];
			if (roundtripLists[i])
				delete roundtripLists[i];
		}
		return 1;
	}

	// Step 4: Validate counts match before detailed comparison
	printf("Step 4: Validating roundtrip data...\n");
	if (!validateScriptStats(originalStats, roundtripStats))
	{
		printf("\nFAILURE: Validation failed - counts don't match.\n");
		// Clean up
		for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		{
			if (originalLists[i])
				delete originalLists[i];
			if (roundtripLists[i])
				delete roundtripLists[i];
		}
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
	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
	{
		if (originalLists[i])
			delete originalLists[i];
		if (roundtripLists[i])
			delete roundtripLists[i];
	}

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
	if (!initMinimalEngine())
	{
		fprintf(stderr, "Error: Failed to initialize engine\n");
		return 1;
	}

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
		fprintf(stderr, "Error: %s\n", e.what());
		result = 1;
	}
	catch (...)
	{
		fprintf(stderr, "Error: Unknown exception\n");
		result = 1;
	}

	// Shutdown engine
	shutdownEngine();

	return result;
}

#else // RTS_HAS_JSON_CHUNK

#include <cstdio>

int main(int /* argc */, char* /* argv */[])
{
	fprintf(stderr, "Error: This tool requires JSON support (RTS_HAS_JSON_CHUNK)\n");
	fprintf(stderr, "Please rebuild with JSON support enabled.\n");
	return 1;
}

#endif // RTS_HAS_JSON_CHUNK
