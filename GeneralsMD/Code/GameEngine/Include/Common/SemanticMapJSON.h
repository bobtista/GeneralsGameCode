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

// TheSuperHackers @feature Semantic JSON serialization for map files

#pragma once

#ifdef RTS_HAS_JSON_CHUNK

#include "Common/AsciiString.h"
#include "Common/GameType.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class Dict;
class PolygonTrigger;
class ScriptList;
class DataChunkOutput;

// Stored map object data (since MapObject is WorldBuilder-only)
struct MapObjectInfo
{
	std::string name;
	float x, y, z;
	float angle;
	int flags;
	Dict* properties;
};

// Stored waypoint link data
struct WaypointLinkInfo
{
	int waypoint1;
	int waypoint2;
};

// Stored lighting data per time-of-day
struct LightingInfo
{
	struct Light
	{
		float ambientR, ambientG, ambientB;
		float diffuseR, diffuseG, diffuseB;
		float posX, posY, posZ;
	};

	Light terrainLights[3];
	Light objectLights[3];
};

// Container for all parsed map data
struct MapData
{
	// WorldInfo
	Dict* worldInfo;

	// Objects
	std::vector<MapObjectInfo> objects;

	// Polygon triggers (stored separately, not in objects)
	// PolygonTrigger data is accessed via static list

	// Waypoint links
	std::vector<WaypointLinkInfo> waypointLinks;

	// Lighting
	int timeOfDay;
	LightingInfo lighting[4];  // morning, afternoon, evening, night

	// Scripts (per player)
	ScriptList* scriptLists[16];
	int numPlayers;

	// Raw binary chunks (preserved from base map for round-trip)
	std::vector<unsigned char> heightMapData;
	std::vector<unsigned char> blendTileData;
	int heightMapVersion;
	int blendTileVersion;

	MapData();
	~MapData();
	void clear();
};

// Semantic JSON writer for map files
class SemanticMapWriter
{
public:
	SemanticMapWriter();

	nlohmann::ordered_json writeMapFile(const MapData& mapData);
	nlohmann::ordered_json writeWorldInfo(const Dict* dict);
	nlohmann::ordered_json writeObjects(const std::vector<MapObjectInfo>& objects);
	nlohmann::ordered_json writeObject(const MapObjectInfo& obj);
	nlohmann::ordered_json writePolygonTriggers();
	nlohmann::ordered_json writePolygonTrigger(const PolygonTrigger* trigger);
	nlohmann::ordered_json writeWaypointLinks(const std::vector<WaypointLinkInfo>& links);
	nlohmann::ordered_json writeLighting(int timeOfDay, const LightingInfo lighting[4]);
	nlohmann::ordered_json writeDict(const Dict* dict);

	std::string getJSONString(int indent = 2) const;

	const std::vector<std::string>& getWarnings() const { return m_warnings; }
	void clearWarnings() { m_warnings.clear(); }
	bool hasWarnings() const { return !m_warnings.empty(); }

private:
	void addWarning(const std::string& message) { m_warnings.push_back(message); }
	std::string timeOfDayToString(int tod);
	std::string objectFlagsToString(int flags);

	nlohmann::ordered_json m_root;
	std::vector<std::string> m_warnings;
};

// Semantic JSON reader for map files
class SemanticMapReader
{
public:
	SemanticMapReader();

	bool parseMapFile(const char* jsonData, size_t length, MapData& outMapData);
	bool parseWorldInfo(const nlohmann::ordered_json& json, Dict* outDict);
	bool parseObjects(const nlohmann::ordered_json& json, std::vector<MapObjectInfo>& outObjects);
	bool parseObject(const nlohmann::ordered_json& json, MapObjectInfo& outObj);
	bool parsePolygonTriggers(const nlohmann::ordered_json& json);
	bool parsePolygonTrigger(const nlohmann::ordered_json& json);
	bool parseWaypointLinks(const nlohmann::ordered_json& json, std::vector<WaypointLinkInfo>& outLinks);
	bool parseLighting(const nlohmann::ordered_json& json, int& outTimeOfDay, LightingInfo outLighting[4]);
	bool parseDict(const nlohmann::ordered_json& json, Dict* outDict);

	const std::string& getLastError() const { return m_lastError; }
	const std::vector<std::string>& getWarnings() const { return m_warnings; }
	void clearWarnings() { m_warnings.clear(); }
	bool hasWarnings() const { return !m_warnings.empty(); }

private:
	void addWarning(const std::string& message) { m_warnings.push_back(message); }
	int stringToTimeOfDay(const std::string& str);
	int stringToObjectFlags(const std::string& str);

	std::string m_lastError;
	std::vector<std::string> m_warnings;
};

// Map file writer (binary output)
class SemanticMapFileWriter
{
public:
	bool writeMapFile(const MapData& mapData, DataChunkOutput& output);

private:
	void writeHeightMapChunk(DataChunkOutput& output, const MapData& data);
	void writeBlendTileChunk(DataChunkOutput& output, const MapData& data);
	void writeWorldInfoChunk(DataChunkOutput& output, const Dict* dict);
	void writeSidesListChunk(DataChunkOutput& output, ScriptList* scripts[], int numPlayers);
	void writeObjectsListChunk(DataChunkOutput& output, const std::vector<MapObjectInfo>& objects);
	void writePolygonTriggersChunk(DataChunkOutput& output);
	void writeGlobalLightingChunk(DataChunkOutput& output, int timeOfDay, const LightingInfo lighting[4]);
	void writeWaypointsListChunk(DataChunkOutput& output, const std::vector<WaypointLinkInfo>& links);
};

#endif // RTS_HAS_JSON_CHUNK
