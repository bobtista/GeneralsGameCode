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

#ifdef RTS_HAS_JSON_CHUNK

#include "Common/SemanticMapJSON.h"
#include "Common/Dict.h"
#include "Common/DataChunk.h"
#include "Common/NameKeyGenerator.h"
#include "Common/GameType.h"
#include "Common/GameMemory.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/Scripts.h"
#include "Common/SemanticScriptJSON.h"

// Object flag bit definitions (from MapObject.h)
static const int FLAG_DRAWS_IN_MIRROR    = 0x00000001;
static const int FLAG_ROAD_POINT1        = 0x00000002;
static const int FLAG_ROAD_POINT2        = 0x00000004;
static const int FLAG_ROAD_FLAGS         = (FLAG_ROAD_POINT1|FLAG_ROAD_POINT2);
static const int FLAG_ROAD_CORNER_ANGLED = 0x00000008;
static const int FLAG_BRIDGE_POINT1      = 0x00000010;
static const int FLAG_BRIDGE_POINT2      = 0x00000020;
static const int FLAG_BRIDGE_FLAGS       = (FLAG_BRIDGE_POINT1|FLAG_BRIDGE_POINT2);
static const int FLAG_ROAD_CORNER_TIGHT  = 0x00000040;
static const int FLAG_ROAD_JOIN          = 0x00000080;
static const int FLAG_DONT_RENDER        = 0x00000100;

static const char* s_timeOfDayNames[] = {
	"invalid", "morning", "afternoon", "evening", "night"
};

static const char* s_weatherNames[] = {
	"NORMAL", "SNOWY"
};
static const int s_weatherCount = 2;

static const char* s_compressionNames[] = {
	"none", "refpack", "noxlzh", "zlib1", "zlib2", "zlib3", "zlib4", "zlib5", "zlib6", "zlib7", "zlib8", "zlib9", "btree", "huff"
};
static const int s_compressionCount = 14;

// ============================================================================
// MapData Implementation
// ============================================================================

MapData::MapData()
	: worldInfo(NULL)
	, timeOfDay(0)
	, shadowColor(0xFFFFFFFF)
	, numPlayers(0)
	, heightMapVersion(4)
	, blendTileVersion(8)
{
	for (int i = 0; i < 16; i++)
		scriptLists[i] = NULL;
	for (int i = 0; i < 4; i++)
		memset(&lighting[i], 0, sizeof(LightingInfo));
}

MapData::~MapData()
{
	clear();
}

void MapData::clear()
{
	if (worldInfo)
	{
		delete worldInfo;
		worldInfo = NULL;
	}
	objects.clear();
	waypointLinks.clear();
	for (int i = 0; i < 16; i++)
	{
		if (scriptLists[i])
		{
			deleteInstance(scriptLists[i]);
			scriptLists[i] = NULL;
		}
	}
	numPlayers = 0;
	heightMapData.clear();
	blendTileData.clear();
	heightMapVersion = 4;
	blendTileVersion = 8;
}

// ============================================================================
// SemanticMapWriter Implementation
// ============================================================================

SemanticMapWriter::SemanticMapWriter()
	: m_root(nlohmann::ordered_json::object())
{
}

std::string SemanticMapWriter::timeOfDayToString(int tod)
{
	if (tod >= 0 && tod < 5)
		return s_timeOfDayNames[tod];
	return "unknown";
}

std::string SemanticMapWriter::objectFlagsToString(int flags)
{
	std::string result;
	if (flags & FLAG_DRAWS_IN_MIRROR) result += "drawsInMirror,";
	if (flags & FLAG_ROAD_POINT1) result += "roadPoint1,";
	if (flags & FLAG_ROAD_POINT2) result += "roadPoint2,";
	if (flags & FLAG_ROAD_CORNER_ANGLED) result += "roadCornerAngled,";
	if (flags & FLAG_BRIDGE_POINT1) result += "bridgePoint1,";
	if (flags & FLAG_BRIDGE_POINT2) result += "bridgePoint2,";
	if (flags & FLAG_ROAD_CORNER_TIGHT) result += "roadCornerTight,";
	if (flags & FLAG_ROAD_JOIN) result += "roadJoin,";
	if (flags & FLAG_DONT_RENDER) result += "dontRender,";
	if (!result.empty() && result.back() == ',')
		result.pop_back();
	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeDict(const Dict* dict)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::object();
	if (!dict)
		return result;

	int count = dict->getPairCount();
	for (int i = 0; i < count; i++)
	{
		NameKeyType key = dict->getNthKey(i);
		if (key == NAMEKEY_INVALID)
			continue;

		AsciiString keyNameStr = TheNameKeyGenerator->keyToName(key);
		const char* keyName = keyNameStr.str();
		if (!keyName || !*keyName)
			continue;

		Dict::DataType type = dict->getNthType(i);
		switch (type)
		{
			case Dict::DICT_BOOL:
				result[keyName] = dict->getBool(key);
				break;
			case Dict::DICT_INT:
				result[keyName] = dict->getInt(key);
				break;
			case Dict::DICT_REAL:
				result[keyName] = dict->getReal(key);
				break;
			case Dict::DICT_ASCIISTRING:
				result[keyName] = dict->getAsciiString(key).str() ? dict->getAsciiString(key).str() : "";
				break;
			case Dict::DICT_UNICODESTRING:
			{
				UnicodeString ustr = dict->getUnicodeString(key);
				if (ustr.str())
				{
					std::string utf8;
					for (const wchar_t* p = ustr.str(); *p; ++p)
					{
						if (*p < 0x80)
							utf8 += static_cast<char>(*p);
						else
							utf8 += '?';
					}
					result[keyName] = utf8;
				}
				else
					result[keyName] = "";
				break;
			}
			default:
				break;
		}
	}
	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeWorldInfo(const Dict* dict)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::object();
	if (!dict)
		return result;

	int count = dict->getPairCount();
	for (int i = 0; i < count; i++)
	{
		NameKeyType key = dict->getNthKey(i);
		if (key == NAMEKEY_INVALID)
			continue;

		AsciiString keyNameStr = TheNameKeyGenerator->keyToName(key);
		const char* keyName = keyNameStr.str();
		if (!keyName || !*keyName)
			continue;

		Dict::DataType type = dict->getNthType(i);

		// Special handling for known enum keys
		if (type == Dict::DICT_INT)
		{
			int value = dict->getInt(key);

			if (strcmp(keyName, "weather") == 0)
			{
				if (value >= 0 && value < s_weatherCount)
					result[keyName] = s_weatherNames[value];
				else
					result[keyName] = value;
				continue;
			}
			else if (strcmp(keyName, "compression") == 0)
			{
				if (value >= 0 && value < s_compressionCount)
					result[keyName] = s_compressionNames[value];
				else
					result[keyName] = value;
				continue;
			}
		}

		// Default handling for other keys
		switch (type)
		{
			case Dict::DICT_BOOL:
				result[keyName] = dict->getBool(key);
				break;
			case Dict::DICT_INT:
				result[keyName] = dict->getInt(key);
				break;
			case Dict::DICT_REAL:
				result[keyName] = dict->getReal(key);
				break;
			case Dict::DICT_ASCIISTRING:
				result[keyName] = dict->getAsciiString(key).str() ? dict->getAsciiString(key).str() : "";
				break;
			case Dict::DICT_UNICODESTRING:
			{
				UnicodeString ustr = dict->getUnicodeString(key);
				if (ustr.str())
				{
					std::string utf8;
					for (const wchar_t* p = ustr.str(); *p; ++p)
					{
						if (*p < 0x80)
							utf8 += static_cast<char>(*p);
						else
							utf8 += '?';
					}
					result[keyName] = utf8;
				}
				else
					result[keyName] = "";
				break;
			}
			default:
				break;
		}
	}
	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeObject(const MapObjectInfo& obj)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	result["name"] = obj.name;
	result["position"] = nlohmann::ordered_json::object();
	result["position"]["x"] = obj.x;
	result["position"]["y"] = obj.y;
	result["position"]["z"] = obj.z;
	result["angle"] = obj.angle;

	if (obj.flags != 0)
	{
		std::string flagStr = objectFlagsToString(obj.flags);
		if (!flagStr.empty())
			result["flags"] = flagStr;
		else
			result["flags"] = obj.flags;
	}

	if (obj.properties)
	{
		nlohmann::ordered_json props = writeDict(obj.properties);
		if (!props.empty())
			result["properties"] = props;
	}

	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeObjects(const std::vector<MapObjectInfo>& objects)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();
	for (const auto& obj : objects)
	{
		result.push_back(writeObject(obj));
	}
	return result;
}

nlohmann::ordered_json SemanticMapWriter::writePolygonTrigger(const PolygonTrigger* trigger)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::object();
	if (!trigger)
		return result;

	result["name"] = trigger->getTriggerName().str() ? trigger->getTriggerName().str() : "";
	result["id"] = trigger->getID();

	AsciiString layerName = trigger->getLayerName();
	if (!layerName.isEmpty())
		result["layer"] = layerName.str();

	if (trigger->isWaterArea())
		result["isWater"] = true;
	if (trigger->isRiver())
	{
		result["isRiver"] = true;
		result["riverStart"] = trigger->getRiverStart();
	}

	nlohmann::ordered_json points = nlohmann::ordered_json::array();
	for (int i = 0; i < trigger->getNumPoints(); i++)
	{
		const ICoord3D* pt = trigger->getPoint(i);
		nlohmann::ordered_json point = nlohmann::ordered_json::object();
		point["x"] = pt->x;
		point["y"] = pt->y;
		point["z"] = pt->z;
		points.push_back(point);
	}
	result["points"] = points;

	return result;
}

nlohmann::ordered_json SemanticMapWriter::writePolygonTriggers()
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();
	for (PolygonTrigger* trigger = PolygonTrigger::getFirstPolygonTrigger();
	     trigger;
	     trigger = trigger->getNext())
	{
		result.push_back(writePolygonTrigger(trigger));
	}
	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeWaypointLinks(const std::vector<WaypointLinkInfo>& links)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();
	for (const auto& link : links)
	{
		nlohmann::ordered_json linkJson = nlohmann::ordered_json::object();
		linkJson["from"] = link.waypoint1;
		linkJson["to"] = link.waypoint2;
		result.push_back(linkJson);
	}
	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeLighting(int timeOfDay, const LightingInfo lighting[4], unsigned int shadowColor)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	result["timeOfDay"] = timeOfDayToString(timeOfDay);
	result["shadowColor"] = shadowColor;

	const char* periods[] = {"morning", "afternoon", "evening", "night"};

	for (int i = 0; i < 4; i++)
	{
		nlohmann::ordered_json period = nlohmann::ordered_json::object();

		nlohmann::ordered_json terrainArray = nlohmann::ordered_json::array();
		for (int j = 0; j < 3; j++)
		{
			nlohmann::ordered_json light = nlohmann::ordered_json::object();
			light["ambient"] = nlohmann::ordered_json::object();
			light["ambient"]["r"] = lighting[i].terrainLights[j].ambientR;
			light["ambient"]["g"] = lighting[i].terrainLights[j].ambientG;
			light["ambient"]["b"] = lighting[i].terrainLights[j].ambientB;
			light["diffuse"] = nlohmann::ordered_json::object();
			light["diffuse"]["r"] = lighting[i].terrainLights[j].diffuseR;
			light["diffuse"]["g"] = lighting[i].terrainLights[j].diffuseG;
			light["diffuse"]["b"] = lighting[i].terrainLights[j].diffuseB;
			light["position"] = nlohmann::ordered_json::object();
			light["position"]["x"] = lighting[i].terrainLights[j].posX;
			light["position"]["y"] = lighting[i].terrainLights[j].posY;
			light["position"]["z"] = lighting[i].terrainLights[j].posZ;
			terrainArray.push_back(light);
		}
		period["terrain"] = terrainArray;

		nlohmann::ordered_json objects = nlohmann::ordered_json::array();
		for (int j = 0; j < 3; j++)
		{
			nlohmann::ordered_json light = nlohmann::ordered_json::object();
			light["ambient"] = nlohmann::ordered_json::object();
			light["ambient"]["r"] = lighting[i].objectLights[j].ambientR;
			light["ambient"]["g"] = lighting[i].objectLights[j].ambientG;
			light["ambient"]["b"] = lighting[i].objectLights[j].ambientB;
			light["diffuse"] = nlohmann::ordered_json::object();
			light["diffuse"]["r"] = lighting[i].objectLights[j].diffuseR;
			light["diffuse"]["g"] = lighting[i].objectLights[j].diffuseG;
			light["diffuse"]["b"] = lighting[i].objectLights[j].diffuseB;
			light["position"] = nlohmann::ordered_json::object();
			light["position"]["x"] = lighting[i].objectLights[j].posX;
			light["position"]["y"] = lighting[i].objectLights[j].posY;
			light["position"]["z"] = lighting[i].objectLights[j].posZ;
			objects.push_back(light);
		}
		period["objects"] = objects;

		result[periods[i]] = period;
	}

	return result;
}

nlohmann::ordered_json SemanticMapWriter::writeMapFile(const MapData& mapData)
{
	m_root = nlohmann::ordered_json::object();

	if (mapData.worldInfo)
		m_root["worldInfo"] = writeWorldInfo(mapData.worldInfo);

	m_root["lighting"] = writeLighting(mapData.timeOfDay, mapData.lighting, mapData.shadowColor);

	if (!mapData.objects.empty())
		m_root["objects"] = writeObjects(mapData.objects);

	nlohmann::ordered_json triggers = writePolygonTriggers();
	if (!triggers.empty())
		m_root["polygonTriggers"] = triggers;

	if (!mapData.waypointLinks.empty())
		m_root["waypointLinks"] = writeWaypointLinks(mapData.waypointLinks);

	if (mapData.numPlayers > 0)
	{
		SemanticScriptWriter scriptWriter;
		m_root["scripts"] = scriptWriter.writeScriptsFile(const_cast<ScriptList**>(mapData.scriptLists), mapData.numPlayers);

		for (const auto& warning : scriptWriter.getWarnings())
			addWarning(warning);
	}

	return m_root;
}

std::string SemanticMapWriter::getJSONString(int indent) const
{
	return m_root.dump(indent);
}

// ============================================================================
// SemanticMapReader Implementation
// ============================================================================

SemanticMapReader::SemanticMapReader()
{
}

int SemanticMapReader::stringToTimeOfDay(const std::string& str)
{
	for (int i = 0; i < 5; i++)
	{
		if (str == s_timeOfDayNames[i])
			return i;
	}
	return 0;
}

int SemanticMapReader::stringToObjectFlags(const std::string& str)
{
	int flags = 0;
	if (str.find("drawsInMirror") != std::string::npos) flags |= FLAG_DRAWS_IN_MIRROR;
	if (str.find("roadPoint1") != std::string::npos) flags |= FLAG_ROAD_POINT1;
	if (str.find("roadPoint2") != std::string::npos) flags |= FLAG_ROAD_POINT2;
	if (str.find("roadCornerAngled") != std::string::npos) flags |= FLAG_ROAD_CORNER_ANGLED;
	if (str.find("bridgePoint1") != std::string::npos) flags |= FLAG_BRIDGE_POINT1;
	if (str.find("bridgePoint2") != std::string::npos) flags |= FLAG_BRIDGE_POINT2;
	if (str.find("roadCornerTight") != std::string::npos) flags |= FLAG_ROAD_CORNER_TIGHT;
	if (str.find("roadJoin") != std::string::npos) flags |= FLAG_ROAD_JOIN;
	if (str.find("dontRender") != std::string::npos) flags |= FLAG_DONT_RENDER;
	return flags;
}

bool SemanticMapReader::parseDict(const nlohmann::ordered_json& json, Dict* outDict)
{
	if (!outDict || !json.is_object())
		return false;

	outDict->clear();

	for (auto it = json.begin(); it != json.end(); ++it)
	{
		const std::string& keyName = it.key();
		NameKeyType key = TheNameKeyGenerator->nameToKey(keyName.c_str());

		if (it->is_boolean())
			outDict->setBool(key, it->get<bool>());
		else if (it->is_number_integer())
			outDict->setInt(key, it->get<int>());
		else if (it->is_number_float())
			outDict->setReal(key, it->get<float>());
		else if (it->is_string())
			outDict->setAsciiString(key, AsciiString(it->get<std::string>().c_str()));
	}

	return true;
}

static int stringToWeather(const std::string& str)
{
	for (int i = 0; i < s_weatherCount; i++)
	{
		if (str == s_weatherNames[i])
			return i;
	}
	// Try lowercase
	std::string lower = str;
	for (char& c : lower) c = std::tolower(c);
	if (lower == "normal") return 0;
	if (lower == "snowy") return 1;
	return 0;
}

static int stringToCompression(const std::string& str)
{
	for (int i = 0; i < s_compressionCount; i++)
	{
		if (str == s_compressionNames[i])
			return i;
	}
	return 1; // Default to refpack
}

bool SemanticMapReader::parseWorldInfo(const nlohmann::ordered_json& json, Dict* outDict)
{
	if (!outDict || !json.is_object())
		return false;

	outDict->clear();

	for (auto it = json.begin(); it != json.end(); ++it)
	{
		const std::string& keyName = it.key();
		NameKeyType key = TheNameKeyGenerator->nameToKey(keyName.c_str());

		// Special handling for known enum keys that may be strings
		if (keyName == "weather")
		{
			if (it->is_string())
				outDict->setInt(key, stringToWeather(it->get<std::string>()));
			else if (it->is_number_integer())
				outDict->setInt(key, it->get<int>());
			continue;
		}
		else if (keyName == "compression")
		{
			if (it->is_string())
				outDict->setInt(key, stringToCompression(it->get<std::string>()));
			else if (it->is_number_integer())
				outDict->setInt(key, it->get<int>());
			continue;
		}

		// Default handling
		if (it->is_boolean())
			outDict->setBool(key, it->get<bool>());
		else if (it->is_number_integer())
			outDict->setInt(key, it->get<int>());
		else if (it->is_number_float())
			outDict->setReal(key, it->get<float>());
		else if (it->is_string())
			outDict->setAsciiString(key, AsciiString(it->get<std::string>().c_str()));
	}

	return true;
}

bool SemanticMapReader::parseObject(const nlohmann::ordered_json& json, MapObjectInfo& outObj)
{
	if (!json.is_object())
		return false;

	outObj.name = json.value("name", "");
	outObj.angle = json.value("angle", 0.0f);

	if (json.contains("position") && json["position"].is_object())
	{
		outObj.x = json["position"].value("x", 0.0f);
		outObj.y = json["position"].value("y", 0.0f);
		outObj.z = json["position"].value("z", 0.0f);
	}
	else
	{
		outObj.x = outObj.y = outObj.z = 0.0f;
	}

	if (json.contains("flags"))
	{
		if (json["flags"].is_string())
			outObj.flags = stringToObjectFlags(json["flags"].get<std::string>());
		else if (json["flags"].is_number_integer())
			outObj.flags = json["flags"].get<int>();
		else
			outObj.flags = 0;
	}
	else
	{
		outObj.flags = 0;
	}

	outObj.properties = NULL;
	if (json.contains("properties") && json["properties"].is_object())
	{
		outObj.properties = new Dict();
		parseDict(json["properties"], outObj.properties);
	}

	return true;
}

bool SemanticMapReader::parseObjects(const nlohmann::ordered_json& json, std::vector<MapObjectInfo>& outObjects)
{
	if (!json.is_array())
		return false;

	outObjects.clear();
	for (const auto& objJson : json)
	{
		MapObjectInfo obj;
		if (parseObject(objJson, obj))
			outObjects.push_back(obj);
	}
	return true;
}

bool SemanticMapReader::parsePolygonTrigger(const nlohmann::ordered_json& json)
{
	if (!json.is_object())
		return false;

	int numPoints = 0;
	if (json.contains("points") && json["points"].is_array())
		numPoints = static_cast<int>(json["points"].size());

	PolygonTrigger* trigger = newInstance(PolygonTrigger)(numPoints + 1);

	if (json.contains("name") && json["name"].is_string())
		trigger->setTriggerName(AsciiString(json["name"].get<std::string>().c_str()));

	if (json.contains("layer") && json["layer"].is_string())
		trigger->setLayerName(AsciiString(json["layer"].get<std::string>().c_str()));

	trigger->setWaterArea(json.value("isWater", false));
	trigger->setRiver(json.value("isRiver", false));
	trigger->setRiverStart(json.value("riverStart", 0));

	if (json.contains("points") && json["points"].is_array())
	{
		for (const auto& ptJson : json["points"])
		{
			ICoord3D pt;
			pt.x = ptJson.value("x", 0);
			pt.y = ptJson.value("y", 0);
			pt.z = ptJson.value("z", 0);
			trigger->addPoint(pt);
		}
	}

	PolygonTrigger::addPolygonTrigger(trigger);
	return true;
}

bool SemanticMapReader::parsePolygonTriggers(const nlohmann::ordered_json& json)
{
	if (!json.is_array())
		return false;

	PolygonTrigger::deleteTriggers();

	for (const auto& triggerJson : json)
	{
		if (!parsePolygonTrigger(triggerJson))
		{
			addWarning("Failed to parse polygon trigger");
		}
	}
	return true;
}

bool SemanticMapReader::parseWaypointLinks(const nlohmann::ordered_json& json, std::vector<WaypointLinkInfo>& outLinks)
{
	if (!json.is_array())
		return false;

	outLinks.clear();
	for (const auto& linkJson : json)
	{
		if (linkJson.is_object())
		{
			WaypointLinkInfo link;
			link.waypoint1 = linkJson.value("from", 0);
			link.waypoint2 = linkJson.value("to", 0);
			outLinks.push_back(link);
		}
	}
	return true;
}

bool SemanticMapReader::parseLighting(const nlohmann::ordered_json& json, int& outTimeOfDay, LightingInfo outLighting[4], unsigned int& outShadowColor)
{
	if (!json.is_object())
		return false;

	if (json.contains("timeOfDay"))
	{
		if (json["timeOfDay"].is_string())
			outTimeOfDay = stringToTimeOfDay(json["timeOfDay"].get<std::string>());
		else if (json["timeOfDay"].is_number_integer())
			outTimeOfDay = json["timeOfDay"].get<int>();
	}

	if (json.contains("shadowColor"))
	{
		if (json["shadowColor"].is_number_unsigned())
			outShadowColor = json["shadowColor"].get<unsigned int>();
		else if (json["shadowColor"].is_number_integer())
			outShadowColor = static_cast<unsigned int>(json["shadowColor"].get<int>());
	}

	const char* periods[] = {"morning", "afternoon", "evening", "night"};

	for (int i = 0; i < 4; i++)
	{
		if (!json.contains(periods[i]))
			continue;

		const auto& period = json[periods[i]];

		if (period.contains("terrain"))
		{
			if (period["terrain"].is_array())
			{
				int j = 0;
				for (const auto& light : period["terrain"])
				{
					if (j >= 3) break;
					if (light.contains("ambient"))
					{
						outLighting[i].terrainLights[j].ambientR = light["ambient"].value("r", 0.0f);
						outLighting[i].terrainLights[j].ambientG = light["ambient"].value("g", 0.0f);
						outLighting[i].terrainLights[j].ambientB = light["ambient"].value("b", 0.0f);
					}
					if (light.contains("diffuse"))
					{
						outLighting[i].terrainLights[j].diffuseR = light["diffuse"].value("r", 0.0f);
						outLighting[i].terrainLights[j].diffuseG = light["diffuse"].value("g", 0.0f);
						outLighting[i].terrainLights[j].diffuseB = light["diffuse"].value("b", 0.0f);
					}
					if (light.contains("position"))
					{
						outLighting[i].terrainLights[j].posX = light["position"].value("x", 0.0f);
						outLighting[i].terrainLights[j].posY = light["position"].value("y", 0.0f);
						outLighting[i].terrainLights[j].posZ = light["position"].value("z", 0.0f);
					}
					j++;
				}
			}
			else if (period["terrain"].is_object())
			{
				const auto& terrain = period["terrain"];
				if (terrain.contains("ambient"))
				{
					outLighting[i].terrainLights[0].ambientR = terrain["ambient"].value("r", 0.0f);
					outLighting[i].terrainLights[0].ambientG = terrain["ambient"].value("g", 0.0f);
					outLighting[i].terrainLights[0].ambientB = terrain["ambient"].value("b", 0.0f);
				}
				if (terrain.contains("diffuse"))
				{
					outLighting[i].terrainLights[0].diffuseR = terrain["diffuse"].value("r", 0.0f);
					outLighting[i].terrainLights[0].diffuseG = terrain["diffuse"].value("g", 0.0f);
					outLighting[i].terrainLights[0].diffuseB = terrain["diffuse"].value("b", 0.0f);
				}
				if (terrain.contains("position"))
				{
					outLighting[i].terrainLights[0].posX = terrain["position"].value("x", 0.0f);
					outLighting[i].terrainLights[0].posY = terrain["position"].value("y", 0.0f);
					outLighting[i].terrainLights[0].posZ = terrain["position"].value("z", 0.0f);
				}
			}
		}

		if (period.contains("objects") && period["objects"].is_array())
		{
			int j = 0;
			for (const auto& light : period["objects"])
			{
				if (j >= 3) break;
				if (light.contains("ambient"))
				{
					outLighting[i].objectLights[j].ambientR = light["ambient"].value("r", 0.0f);
					outLighting[i].objectLights[j].ambientG = light["ambient"].value("g", 0.0f);
					outLighting[i].objectLights[j].ambientB = light["ambient"].value("b", 0.0f);
				}
				if (light.contains("diffuse"))
				{
					outLighting[i].objectLights[j].diffuseR = light["diffuse"].value("r", 0.0f);
					outLighting[i].objectLights[j].diffuseG = light["diffuse"].value("g", 0.0f);
					outLighting[i].objectLights[j].diffuseB = light["diffuse"].value("b", 0.0f);
				}
				if (light.contains("position"))
				{
					outLighting[i].objectLights[j].posX = light["position"].value("x", 0.0f);
					outLighting[i].objectLights[j].posY = light["position"].value("y", 0.0f);
					outLighting[i].objectLights[j].posZ = light["position"].value("z", 0.0f);
				}
				j++;
			}
		}
	}

	return true;
}

bool SemanticMapReader::parseMapFile(const char* jsonData, size_t length, MapData& outMapData)
{
	outMapData.clear();

	nlohmann::ordered_json root;
	try
	{
		root = nlohmann::ordered_json::parse(jsonData, jsonData + length);
	}
	catch (const std::exception& e)
	{
		m_lastError = std::string("JSON parse error: ") + e.what();
		return false;
	}

	if (!root.is_object())
	{
		m_lastError = "Root must be a JSON object";
		return false;
	}

	if (root.contains("worldInfo"))
	{
		outMapData.worldInfo = new Dict();
		parseWorldInfo(root["worldInfo"], outMapData.worldInfo);
	}

	if (root.contains("lighting"))
		parseLighting(root["lighting"], outMapData.timeOfDay, outMapData.lighting, outMapData.shadowColor);

	if (root.contains("objects"))
		parseObjects(root["objects"], outMapData.objects);

	if (root.contains("polygonTriggers"))
		parsePolygonTriggers(root["polygonTriggers"]);

	if (root.contains("waypointLinks"))
		parseWaypointLinks(root["waypointLinks"], outMapData.waypointLinks);

	if (root.contains("scripts"))
	{
		SemanticScriptReader scriptReader;
		if (!scriptReader.parseScriptsFile(
			root["scripts"].dump().c_str(),
			root["scripts"].dump().length(),
			outMapData.scriptLists,
			&outMapData.numPlayers))
		{
			addWarning("Failed to parse scripts: " + scriptReader.getLastError());
		}
		for (const auto& warning : scriptReader.getWarnings())
			addWarning(warning);
	}

	return true;
}

// ============================================================================
// SemanticMapFileWriter Implementation
// ============================================================================

void SemanticMapFileWriter::writeHeightMapChunk(DataChunkOutput& output, const MapData& data)
{
	output.openDataChunk("HeightMapData", data.heightMapVersion);
	output.writeArrayOfBytes((char*)data.heightMapData.data(), (Int)data.heightMapData.size());
	output.closeDataChunk();
}

void SemanticMapFileWriter::writeBlendTileChunk(DataChunkOutput& output, const MapData& data)
{
	output.openDataChunk("BlendTileData", data.blendTileVersion);
	output.writeArrayOfBytes((char*)data.blendTileData.data(), (Int)data.blendTileData.size());
	output.closeDataChunk();
}

void SemanticMapFileWriter::writeWorldInfoChunk(DataChunkOutput& output, const Dict* dict)
{
	output.openDataChunk("WorldInfo", 1);
	if (dict)
		output.writeDict(*dict);
	else
		output.writeDict(Dict());  // Always write a Dict, even if empty
	output.closeDataChunk();
}

void SemanticMapFileWriter::writeSidesListChunk(DataChunkOutput& output, ScriptList* scripts[], int numPlayers)
{
	output.openDataChunk("SidesList", 3);
	output.writeInt(0);  // numSides - not storing player data in JSON yet
	output.writeInt(0);  // numTeams - not storing team data in JSON yet
	ScriptList::WriteScriptsDataChunk(output, scripts, numPlayers);
	output.closeDataChunk();
}

void SemanticMapFileWriter::writeObjectsListChunk(DataChunkOutput& output, const std::vector<MapObjectInfo>& objects)
{
	output.openDataChunk("ObjectsList", 3);
	for (const auto& obj : objects)
	{
		output.openDataChunk("Object", 1);
		output.writeReal(obj.x);
		output.writeReal(obj.y);
		output.writeReal(obj.z);
		output.writeReal(obj.angle);
		output.writeInt(obj.flags);
		output.writeAsciiString(AsciiString(obj.name.c_str()));
		if (obj.properties)
			output.writeDict(*obj.properties);
		else
			output.writeDict(Dict());
		output.closeDataChunk();
	}
	output.closeDataChunk();
}

void SemanticMapFileWriter::writePolygonTriggersChunk(DataChunkOutput& output)
{
	PolygonTrigger::WritePolygonTriggersDataChunk(output);
}

void SemanticMapFileWriter::writeGlobalLightingChunk(DataChunkOutput& output, int timeOfDay, const LightingInfo lighting[4], unsigned int shadowColor)
{
	output.openDataChunk("GlobalLighting", 3);
	output.writeInt(timeOfDay);

	for (int i = 0; i < 4; i++)
	{
		// Terrain light [0]
		output.writeReal(lighting[i].terrainLights[0].ambientR);
		output.writeReal(lighting[i].terrainLights[0].ambientG);
		output.writeReal(lighting[i].terrainLights[0].ambientB);
		output.writeReal(lighting[i].terrainLights[0].diffuseR);
		output.writeReal(lighting[i].terrainLights[0].diffuseG);
		output.writeReal(lighting[i].terrainLights[0].diffuseB);
		output.writeReal(lighting[i].terrainLights[0].posX);
		output.writeReal(lighting[i].terrainLights[0].posY);
		output.writeReal(lighting[i].terrainLights[0].posZ);

		// Object light [0]
		output.writeReal(lighting[i].objectLights[0].ambientR);
		output.writeReal(lighting[i].objectLights[0].ambientG);
		output.writeReal(lighting[i].objectLights[0].ambientB);
		output.writeReal(lighting[i].objectLights[0].diffuseR);
		output.writeReal(lighting[i].objectLights[0].diffuseG);
		output.writeReal(lighting[i].objectLights[0].diffuseB);
		output.writeReal(lighting[i].objectLights[0].posX);
		output.writeReal(lighting[i].objectLights[0].posY);
		output.writeReal(lighting[i].objectLights[0].posZ);

		// Additional object lights [1-2] (version 3)
		for (int j = 1; j < 3; j++)
		{
			output.writeReal(lighting[i].objectLights[j].ambientR);
			output.writeReal(lighting[i].objectLights[j].ambientG);
			output.writeReal(lighting[i].objectLights[j].ambientB);
			output.writeReal(lighting[i].objectLights[j].diffuseR);
			output.writeReal(lighting[i].objectLights[j].diffuseG);
			output.writeReal(lighting[i].objectLights[j].diffuseB);
			output.writeReal(lighting[i].objectLights[j].posX);
			output.writeReal(lighting[i].objectLights[j].posY);
			output.writeReal(lighting[i].objectLights[j].posZ);
		}

		// Additional terrain lights [1-2] (version 3)
		for (int j = 1; j < 3; j++)
		{
			output.writeReal(lighting[i].terrainLights[j].ambientR);
			output.writeReal(lighting[i].terrainLights[j].ambientG);
			output.writeReal(lighting[i].terrainLights[j].ambientB);
			output.writeReal(lighting[i].terrainLights[j].diffuseR);
			output.writeReal(lighting[i].terrainLights[j].diffuseG);
			output.writeReal(lighting[i].terrainLights[j].diffuseB);
			output.writeReal(lighting[i].terrainLights[j].posX);
			output.writeReal(lighting[i].terrainLights[j].posY);
			output.writeReal(lighting[i].terrainLights[j].posZ);
		}
	}

	output.writeInt(shadowColor);
	output.closeDataChunk();
}

void SemanticMapFileWriter::writeWaypointsListChunk(DataChunkOutput& output, const std::vector<WaypointLinkInfo>& links)
{
	output.openDataChunk("WaypointsList", 1);
	output.writeInt((Int)links.size());
	for (const auto& link : links)
	{
		output.writeInt(link.waypoint1);
		output.writeInt(link.waypoint2);
	}
	output.closeDataChunk();
}

bool SemanticMapFileWriter::writeMapFile(const MapData& mapData, DataChunkOutput& output)
{
	// Write chunks in required order
	if (!mapData.heightMapData.empty())
		writeHeightMapChunk(output, mapData);

	if (!mapData.blendTileData.empty())
		writeBlendTileChunk(output, mapData);

	writeWorldInfoChunk(output, mapData.worldInfo);

	writeSidesListChunk(output, const_cast<ScriptList**>(mapData.scriptLists), mapData.numPlayers);

	writeObjectsListChunk(output, mapData.objects);

	writePolygonTriggersChunk(output);

	writeGlobalLightingChunk(output, mapData.timeOfDay, mapData.lighting, mapData.shadowColor);

	writeWaypointsListChunk(output, mapData.waypointLinks);

	return true;
}

#endif // RTS_HAS_JSON_CHUNK
