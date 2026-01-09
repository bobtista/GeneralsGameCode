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

// TheSuperHackers @feature Semantic JSON converter for SCB script files

// FILE: Main.cpp //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
// Project:    SemanticScriptConverter
//
// Desc:       Command-line tool to convert between SCB binary and semantic JSON format
//
// Usage:
//   SemanticScriptConverter --to-json <input.scb> <output.json>
//   SemanticScriptConverter --to-scb <input.json> <output.scb>
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
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptEngine.h"

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
	fprintf(stderr, "Semantic Script Converter - Convert between SCB binary and JSON format\n\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s [options] <input> <output>       Convert between formats (auto-detected)\n", programName);
	fprintf(stderr, "  %s [options] --validate <file>      Validate SCB or JSON file\n", programName);
	fprintf(stderr, "  %s [options] --verify <input.scb>   Roundtrip verification test\n", programName);
	fprintf(stderr, "\n");
	fprintf(stderr, "Conversion direction is auto-detected from file extensions:\n");
	fprintf(stderr, "  .scb -> .json    Binary to semantic JSON\n");
	fprintf(stderr, "  .json -> .scb    Semantic JSON to binary\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --validate         Check if file can be parsed (shows statistics)\n");
	fprintf(stderr, "  --verify           Roundtrip test: SCB -> JSON -> SCB, compare results\n");
	fprintf(stderr, "  --game-dir <path>  Path to game installation directory\n");
	fprintf(stderr, "                     (defaults to registry lookup, then current directory)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "  %s SkirmishScripts.scb scripts.json\n", programName);
	fprintf(stderr, "  %s scripts.json SkirmishScripts.scb\n", programName);
	fprintf(stderr, "  %s --validate SkirmishScripts.scb\n", programName);
	fprintf(stderr, "  %s --verify SkirmishScripts.scb\n", programName);
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
// validateFile - Validate an SCB or JSON file
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

	ScriptList* scriptLists[MAX_PLAYER_COUNT];
	int numPlayers = 0;

	for (int i = 0; i < MAX_PLAYER_COUNT; i++)
		scriptLists[i] = NULL;

	bool isJson = hasExtension(inputFile, ".json");

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

	printf("  Found %d player script lists\n", numPlayers);

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

	// Step 4: Compare original and roundtrip results
	printf("Step 4: Comparing original and roundtrip results...\n");

	bool allMatch = true;

	if (numPlayers != roundtripNumPlayers)
	{
		printf("  DIFF: Player count mismatch: %d vs %d\n", numPlayers, roundtripNumPlayers);
		allMatch = false;
	}
	else
	{
		for (int i = 0; i < numPlayers; i++)
		{
			if (!compareScriptLists(originalLists[i], roundtripLists[i], i))
			{
				allMatch = false;
			}
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

	if (!verify && !validate)
	{
		bool inputIsScb = hasExtension(inputFile, ".scb");
		bool inputIsJson = hasExtension(inputFile, ".json");
		bool outputIsScb = hasExtension(outputFile, ".scb");
		bool outputIsJson = hasExtension(outputFile, ".json");

		if (inputIsScb && outputIsJson)
		{
			toJson = true;
		}
		else if (inputIsJson && outputIsScb)
		{
			toScb = true;
		}
		else
		{
			fprintf(stderr, "Error: Cannot determine conversion direction from file extensions.\n");
			fprintf(stderr, "       Use .scb for binary and .json for JSON format.\n");
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
		else if (toScb)
		{
			result = convertToSCB(inputFile, outputFile);
		}
		else if (verify)
		{
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
