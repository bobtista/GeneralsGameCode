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

// TheSuperHackers @feature Semantic JSON serialization for scripts

#ifdef RTS_HAS_JSON_CHUNK

#include "Common/SemanticScriptJSON.h"
#include "Common/GameCommon.h"
#include "Common/KindOf.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptEngine.h"
#include "Common/GameMemory.h"
#include <cctype>
#include <algorithm>
#include <cstdio>

std::string SemanticScriptReader_getParameterName(int paramIndex, const Template* tmpl);

static const char* s_comparisonNames[] = {
	"lessThan", "lessEqual", "equal", "greaterEqual", "greater", "notEqual"
};

static const char* s_relationNames[] = { "enemy", "neutral", "friend" };

static const char* s_buildableNames[] = { "yes", "ignorePrerequisites", "no", "onlyByAI" };

// 1-indexed: 1=Ground, 2=Air, 3=GroundOrAir
static const char* s_surfaceNames[] = { "ground", "air", "groundOrAir" };

static const char* s_shakeNames[] = {
	"subtle", "normal", "strong", "severe", "cineExtreme", "cineInsane"
};

static const char* s_radarEventNames[] = {
	"invalid", "construction", "upgrade", "underAttack", "information",
	"beaconPulse", "infiltration", "battlePlan", "stealthDiscovered",
	"stealthNeutralized", "fake"
};

// Values -2 to 2, stored offset by 2 in array
static const char* s_aiMoodNames[] = { "sleep", "passive", "normal", "alert", "aggressive" };

static const char* s_evacNames[] = { "invalid", "left", "right", "burstFromCenter" };

SemanticScriptWriter::SemanticScriptWriter()
{
	m_root = nlohmann::ordered_json::object();
}

std::string SemanticScriptWriter::toCamelCase(const AsciiString& internalName)
{
	if (internalName.isEmpty())
		return "";

	std::string result;
	const char* str = internalName.str();
	bool capitalizeNext = false;
	bool firstChar = true;

	while (*str)
	{
		char c = *str++;
		if (c == '_')
		{
			capitalizeNext = true;
		}
		else
		{
			if (firstChar)
			{
				result += static_cast<char>(std::tolower(c));
				firstChar = false;
			}
			else if (capitalizeNext)
			{
				result += static_cast<char>(std::toupper(c));
				capitalizeNext = false;
			}
			else
			{
				result += static_cast<char>(std::tolower(c));
			}
		}
	}
	return result;
}

std::string SemanticScriptWriter::getParameterName(int paramIndex, const ActionTemplate* tmpl)
{
	if (!tmpl || paramIndex < 0 || paramIndex >= tmpl->m_numUiStrings)
	{
		return "param" + std::to_string(paramIndex);
	}

	AsciiString uiStr = tmpl->m_uiStrings[paramIndex];
	if (uiStr.isEmpty())
		return "param" + std::to_string(paramIndex);

	std::string name = uiStr.str();
	while (!name.empty() && (name.back() == ':' || name.back() == ' '))
		name.pop_back();

	std::string result;
	bool capitalizeNext = false;
	bool firstChar = true;
	for (char c : name)
	{
		if (c == ' ' || c == '_')
		{
			capitalizeNext = true;
		}
		else if (std::isalnum(c))
		{
			if (firstChar)
			{
				result += static_cast<char>(std::tolower(c));
				firstChar = false;
			}
			else if (capitalizeNext)
			{
				result += static_cast<char>(std::toupper(c));
				capitalizeNext = false;
			}
			else
			{
				result += static_cast<char>(std::tolower(c));
			}
		}
	}

	return result.empty() ? "param" + std::to_string(paramIndex) : result;
}

std::string SemanticScriptWriter::getParameterName(int paramIndex, const ConditionTemplate* tmpl)
{
	if (!tmpl || paramIndex < 0 || paramIndex >= tmpl->m_numUiStrings)
	{
		return "param" + std::to_string(paramIndex);
	}

	AsciiString uiStr = tmpl->m_uiStrings[paramIndex];
	if (uiStr.isEmpty())
		return "param" + std::to_string(paramIndex);

	std::string name = uiStr.str();
	while (!name.empty() && (name.back() == ':' || name.back() == ' '))
		name.pop_back();

	std::string result;
	bool capitalizeNext = false;
	bool firstChar = true;
	for (char c : name)
	{
		if (c == ' ' || c == '_')
		{
			capitalizeNext = true;
		}
		else if (std::isalnum(c))
		{
			if (firstChar)
			{
				result += static_cast<char>(std::tolower(c));
				firstChar = false;
			}
			else if (capitalizeNext)
			{
				result += static_cast<char>(std::toupper(c));
				capitalizeNext = false;
			}
			else
			{
				result += static_cast<char>(std::tolower(c));
			}
		}
	}

	return result.empty() ? "param" + std::to_string(paramIndex) : result;
}

void SemanticScriptWriter::writeParameter(nlohmann::ordered_json& params, Parameter* param,
                                          int paramIndex, const ActionTemplate* actionTemplate)
{
	if (!param)
		return;

	std::string paramName = getParameterName(paramIndex, actionTemplate);

	switch (param->getParameterType())
	{
		case Parameter::INT:
		case Parameter::SIDE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::BOUNDARY:
			params[paramName] = param->getInt();
			break;

		case Parameter::BOOLEAN:
			params[paramName] = (param->getInt() != 0);
			break;

		case Parameter::COLOR:
		{
			int color = param->getInt();
			char hexColor[12];
			snprintf(hexColor, sizeof(hexColor), "#%08X", static_cast<unsigned int>(color));
			params[paramName] = hexColor;
			break;
		}

		case Parameter::KIND_OF_PARAM:
		{
			int kindOfIndex = param->getInt();
			const char* const* kindOfNames = KindOfMaskType::getBitNames();
			if (kindOfIndex >= 0 && kindOfNames && kindOfNames[kindOfIndex])
				params[paramName] = kindOfNames[kindOfIndex];
			else
			{
				params[paramName] = kindOfIndex;
				addWarning("Unknown KIND_OF_PARAM value " + std::to_string(kindOfIndex) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;
		}

		case Parameter::COMPARISON:
			if (param->getInt() >= 0 && param->getInt() < 6)
				params[paramName] = s_comparisonNames[param->getInt()];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown COMPARISON value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::RELATION:
			if (param->getInt() >= 0 && param->getInt() < 3)
				params[paramName] = s_relationNames[param->getInt()];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown RELATION value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::BUILDABLE:
			if (param->getInt() >= 0 && param->getInt() < 4)
				params[paramName] = s_buildableNames[param->getInt()];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown BUILDABLE value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::SURFACES_ALLOWED:
			if (param->getInt() >= 1 && param->getInt() <= 3)
				params[paramName] = s_surfaceNames[param->getInt() - 1];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown SURFACES_ALLOWED value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::SHAKE_INTENSITY:
			if (param->getInt() >= 0 && param->getInt() < 6)
				params[paramName] = s_shakeNames[param->getInt()];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown SHAKE_INTENSITY value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::RADAR_EVENT_TYPE:
			if (param->getInt() >= 0 && param->getInt() < 11)
				params[paramName] = s_radarEventNames[param->getInt()];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown RADAR_EVENT_TYPE value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::AI_MOOD:
			if (param->getInt() >= -2 && param->getInt() <= 2)
				params[paramName] = s_aiMoodNames[param->getInt() + 2];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown AI_MOOD value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::LEFT_OR_RIGHT:
			if (param->getInt() >= 0 && param->getInt() < 4)
				params[paramName] = s_evacNames[param->getInt()];
			else
			{
				params[paramName] = param->getInt();
				addWarning("Unknown LEFT_OR_RIGHT value " + std::to_string(param->getInt()) + " for parameter '" + paramName + "', using integer fallback");
			}
			break;

		case Parameter::REAL:
		case Parameter::ANGLE:
		case Parameter::PERCENT:
			params[paramName] = param->getReal();
			break;

		case Parameter::COORD3D:
		{
			Coord3D coord;
			param->getCoord3D(&coord);
			params[paramName] = nlohmann::ordered_json::object();
			params[paramName]["x"] = coord.x;
			params[paramName]["y"] = coord.y;
			params[paramName]["z"] = coord.z;
			break;
		}

		case Parameter::SCRIPT:
		case Parameter::TEAM:
		case Parameter::COUNTER:
		case Parameter::FLAG:
		case Parameter::WAYPOINT:
		case Parameter::TRIGGER_AREA:
		case Parameter::TEXT_STRING:
		case Parameter::SOUND:
		case Parameter::SCRIPT_SUBROUTINE:
		case Parameter::UNIT:
		case Parameter::OBJECT_TYPE:
		case Parameter::TEAM_STATE:
		case Parameter::DIALOG:
		case Parameter::MUSIC:
		case Parameter::MOVIE:
		case Parameter::WAYPOINT_PATH:
		case Parameter::LOCALIZED_TEXT:
		case Parameter::BRIDGE:
		case Parameter::ATTACK_PRIORITY_SET:
		case Parameter::SPECIAL_POWER:
		case Parameter::SCIENCE:
		case Parameter::UPGRADE:
		case Parameter::COMMAND_BUTTON:
		case Parameter::FONT_NAME:
		case Parameter::OBJECT_STATUS:
		case Parameter::COMMANDBUTTON_ALL_ABILITIES:
		case Parameter::SKIRMISH_WAYPOINT_PATH:
		case Parameter::EMOTICON:
		case Parameter::OBJECT_PANEL_FLAG:
		case Parameter::FACTION_NAME:
		case Parameter::OBJECT_TYPE_LIST:
		case Parameter::REVEALNAME:
		case Parameter::SCIENCE_AVAILABILITY:
		default:
			if (!param->getString().isEmpty())
				params[paramName] = param->getString().str();
			else
				params[paramName] = "";
			break;
	}
}

void SemanticScriptWriter::writeParameter(nlohmann::ordered_json& params, Parameter* param,
                                          int paramIndex, const ConditionTemplate* conditionTemplate)
{
	writeParameter(params, param, paramIndex, reinterpret_cast<const ActionTemplate*>(conditionTemplate));
}

nlohmann::ordered_json SemanticScriptWriter::writeAction(ScriptAction* action)
{
	if (!action)
		return nlohmann::ordered_json();

	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	const ActionTemplate* tmpl = TheScriptEngine->getActionTemplate(action->getActionType());
	std::string typeName;
	if (tmpl && !tmpl->m_internalName.isEmpty())
	{
		typeName = toCamelCase(tmpl->m_internalName);
	}
	else
	{
		typeName = "unknownAction" + std::to_string(static_cast<int>(action->getActionType()));
	}

	nlohmann::ordered_json params = nlohmann::ordered_json::object();
	Int numParams = action->getNumParameters();
	for (Int i = 0; i < numParams; i++)
	{
		Parameter* param = action->getParameter(i);
		writeParameter(params, param, i, tmpl);
	}

	result[typeName] = params;
	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeActions(ScriptAction* action)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();

	while (action)
	{
		result.push_back(writeAction(action));
		action = action->getNext();
	}

	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeCondition(Condition* condition)
{
	if (!condition)
		return nlohmann::ordered_json();

	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	const ConditionTemplate* tmpl = TheScriptEngine->getConditionTemplate(condition->getConditionType());
	std::string typeName;
	if (tmpl && !tmpl->m_internalName.isEmpty())
	{
		typeName = toCamelCase(tmpl->m_internalName);
	}
	else
	{
		typeName = "unknownCondition" + std::to_string(static_cast<int>(condition->getConditionType()));
	}

	nlohmann::ordered_json params = nlohmann::ordered_json::object();

	Int numParams = condition->getNumParameters();
	for (Int i = 0; i < numParams; i++)
	{
		Parameter* param = condition->getParameter(i);
		writeParameter(params, param, i, tmpl);
	}

	result[typeName] = params;
	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeConditions(OrCondition* orCondition)
{
	// Format: array of OR clauses, each containing array of AND conditions
	// [
	//   [ {cond1}, {cond2} ],  // (cond1 AND cond2) OR
	//   [ {cond3} ]           // (cond3)
	// ]
	nlohmann::ordered_json result = nlohmann::ordered_json::array();

	while (orCondition)
	{
		nlohmann::ordered_json andClause = nlohmann::ordered_json::array();
		Condition* cond = orCondition->getFirstAndCondition();
		while (cond)
		{
			andClause.push_back(writeCondition(cond));
			cond = cond->getNext();
		}
		result.push_back(andClause);
		orCondition = orCondition->getNextOrCondition();
	}

	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeScript(Script* script)
{
	if (!script)
		return nlohmann::ordered_json();

	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	result["name"] = script->getName().str() ? script->getName().str() : "";

	if (!script->getComment().isEmpty())
		result["comment"] = script->getComment().str();
	if (!script->getConditionComment().isEmpty())
		result["conditionComment"] = script->getConditionComment().str();
	if (!script->getActionComment().isEmpty())
		result["actionComment"] = script->getActionComment().str();

	result["isActive"] = script->isActive();
	result["isOneShot"] = script->isOneShot();
	result["isSubroutine"] = script->isSubroutine();

	nlohmann::ordered_json difficulty = nlohmann::ordered_json::object();
	difficulty["easy"] = script->isEasy();
	difficulty["normal"] = script->isNormal();
	difficulty["hard"] = script->isHard();
	result["difficulty"] = difficulty;

	if (script->getDelayEvalSeconds() > 0)
		result["evaluationDelay"] = script->getDelayEvalSeconds();

	if (script->getOrCondition())
		result["if"] = writeConditions(script->getOrCondition());

	if (script->getAction())
		result["then"] = writeActions(script->getAction());

	if (script->getFalseAction())
	{
		result["else"] = writeActions(script->getFalseAction());
	}

	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeScriptGroup(ScriptGroup* group)
{
	if (!group)
		return nlohmann::ordered_json();

	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	result["name"] = group->getName().str() ? group->getName().str() : "";
	result["isActive"] = group->isActive();
	result["isSubroutine"] = group->isSubroutine();

	nlohmann::ordered_json scripts = nlohmann::ordered_json::array();
	Script* script = group->getScript();
	while (script)
	{
		scripts.push_back(writeScript(script));
		script = script->getNext();
	}
	result["scripts"] = scripts;

	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeScriptList(ScriptList* scriptList)
{
	if (!scriptList)
		return nlohmann::ordered_json();

	nlohmann::ordered_json result = nlohmann::ordered_json::object();

	nlohmann::ordered_json scripts = nlohmann::ordered_json::array();
	Script* script = scriptList->getScript();
	while (script)
	{
		scripts.push_back(writeScript(script));
		script = script->getNext();
	}
	if (!scripts.empty())
		result["scripts"] = scripts;

	nlohmann::ordered_json groups = nlohmann::ordered_json::array();
	ScriptGroup* group = scriptList->getScriptGroup();
	while (group)
	{
		groups.push_back(writeScriptGroup(group));
		group = group->getNext();
	}
	if (!groups.empty())
		result["groups"] = groups;

	return result;
}

nlohmann::ordered_json SemanticScriptWriter::writeScriptsFile(ScriptList** scriptLists, int numPlayers)
{
	m_root = nlohmann::ordered_json::object();
	m_root["version"] = 1;

	nlohmann::ordered_json players = nlohmann::ordered_json::array();
	for (int i = 0; i < numPlayers; i++)
	{
		if (scriptLists[i])
		{
			players.push_back(writeScriptList(scriptLists[i]));
		}
		else
		{
			players.push_back(nlohmann::ordered_json::object());
		}
	}
	m_root["players"] = players;

	return m_root;
}

std::string SemanticScriptWriter::getJSONString(int indent) const
{
	return m_root.dump(indent);
}

// ============================================================================
// SemanticScriptReader Implementation
// ============================================================================

SemanticScriptReader::SemanticScriptReader()
{
}

std::string SemanticScriptReader::fromCamelCase(const std::string& camelCase)
{
	std::string result;
	for (size_t i = 0; i < camelCase.size(); i++)
	{
		char c = camelCase[i];
		if (std::isupper(c) && i > 0)
		{
			result += '_';
		}
		result += static_cast<char>(std::toupper(c));
	}
	return result;
}

int SemanticScriptReader::findActionType(const std::string& name)
{
	std::string upperName = fromCamelCase(name);

	for (int i = 0; i < ScriptAction::NUM_ITEMS; i++)
	{
		const ActionTemplate* tmpl = TheScriptEngine->getActionTemplate(i);
		if (tmpl && !tmpl->m_internalName.isEmpty())
		{
			if (upperName == tmpl->m_internalName.str())
			{
				return i;
			}
		}
	}
	return -1;
}

int SemanticScriptReader::findConditionType(const std::string& name)
{
	std::string upperName = fromCamelCase(name);

	for (int i = 0; i < Condition::NUM_ITEMS; i++)
	{
		const ConditionTemplate* tmpl = TheScriptEngine->getConditionTemplate(i);
		if (tmpl && !tmpl->m_internalName.isEmpty())
		{
			if (upperName == tmpl->m_internalName.str())
			{
				return i;
			}
		}
	}
	return -1;
}

Parameter* SemanticScriptReader::parseParameter(const nlohmann::ordered_json& json, int expectedType)
{
	Parameter* param = newInstance(Parameter)(static_cast<Parameter::ParameterType>(expectedType));

	if (json.is_null())
		return param;

	switch (expectedType)
	{
		case Parameter::INT:
		case Parameter::SIDE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::BOUNDARY:
			if (json.is_number_integer())
				param->setInt(json.get<int>());
			else if (json.is_boolean())
				param->setInt(json.get<bool>() ? 1 : 0);
			break;

		case Parameter::BOOLEAN:
			if (json.is_boolean())
				param->setInt(json.get<bool>() ? 1 : 0);
			else if (json.is_number_integer())
				param->setInt(json.get<int>());
			break;

		case Parameter::COLOR:
			if (json.is_string())
			{
				// Parse hex string "#AARRGGBB" or "AARRGGBB"
				std::string val = json.get<std::string>();
				if (!val.empty() && val[0] == '#')
					val = val.substr(1);
				unsigned int color = 0;
				if (sscanf(val.c_str(), "%x", &color) == 1)
					param->setInt(static_cast<int>(color));
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::KIND_OF_PARAM:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				const char* const* kindOfNames = KindOfMaskType::getBitNames();
				bool found = false;
				if (kindOfNames)
				{
					for (int i = 0; kindOfNames[i]; ++i)
					{
						if (val == kindOfNames[i])
						{
							param->setInt(i);
							found = true;
							break;
						}
					}
				}
				if (!found)
					addWarning("Unrecognized KIND_OF_PARAM value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::COMPARISON:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 6; i++)
				{
					if (val == s_comparisonNames[i])
					{
						param->setInt(i);
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized COMPARISON value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::RELATION:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 3; i++)
				{
					if (val == s_relationNames[i])
					{
						param->setInt(i);
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized RELATION value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::BUILDABLE:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 4; i++)
				{
					if (val == s_buildableNames[i])
					{
						param->setInt(i);
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized BUILDABLE value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::SURFACES_ALLOWED:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 3; i++)
				{
					if (val == s_surfaceNames[i])
					{
						param->setInt(i + 1);  // 1-indexed
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized SURFACES_ALLOWED value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::SHAKE_INTENSITY:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 6; i++)
				{
					if (val == s_shakeNames[i])
					{
						param->setInt(i);
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized SHAKE_INTENSITY value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::RADAR_EVENT_TYPE:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 11; i++)
				{
					if (val == s_radarEventNames[i])
					{
						param->setInt(i);
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized RADAR_EVENT_TYPE value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::AI_MOOD:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 5; i++)
				{
					if (val == s_aiMoodNames[i])
					{
						param->setInt(i - 2);  // offset by -2 (sleep=-2, passive=-1, etc.)
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized AI_MOOD value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::LEFT_OR_RIGHT:
			if (json.is_string())
			{
				std::string val = json.get<std::string>();
				bool found = false;
				for (int i = 0; i < 4; i++)
				{
					if (val == s_evacNames[i])
					{
						param->setInt(i);
						found = true;
						break;
					}
				}
				if (!found)
					addWarning("Unrecognized LEFT_OR_RIGHT value '" + val + "', parameter may have incorrect value");
			}
			else if (json.is_number_integer())
			{
				param->setInt(json.get<int>());
			}
			break;

		case Parameter::REAL:
		case Parameter::ANGLE:
		case Parameter::PERCENT:
			if (json.is_number())
				param->setReal(json.get<float>());
			break;

		case Parameter::COORD3D:
			if (json.is_object())
			{
				Coord3D coord;
				coord.x = json.value("x", 0.0f);
				coord.y = json.value("y", 0.0f);
				coord.z = json.value("z", 0.0f);
				param->setCoord3D(&coord);
			}
			break;

		default:
			if (json.is_string())
				param->setString(AsciiString(json.get<std::string>().c_str()));
			break;
	}

	return param;
}

ScriptAction* SemanticScriptReader::parseAction(const nlohmann::ordered_json& json)
{
	if (!json.is_object() || json.empty())
		return NULL;

	auto it = json.begin();
	std::string typeName = it.key();
	const nlohmann::ordered_json& params = it.value();

	int actionType = findActionType(typeName);
	if (actionType < 0)
	{
		m_lastError = "Unknown action type: " + typeName;
		return NULL;
	}

	ScriptAction* action = newInstance(ScriptAction)();
	action->setActionType(static_cast<ScriptAction::ScriptActionType>(actionType));

	const ActionTemplate* tmpl = TheScriptEngine->getActionTemplate(actionType);
	if (!tmpl)
		return action;

	for (int i = 0; i < tmpl->m_numParameters; i++)
	{
		std::string paramName = SemanticScriptReader_getParameterName(i, tmpl);
		if (params.contains(paramName))
		{
			Parameter* param = parseParameter(params[paramName], tmpl->m_parameters[i]);
			action->setParameter(i, param);
		}
		else
		{
			Parameter* param = newInstance(Parameter)(tmpl->m_parameters[i]);
			action->setParameter(i, param);
		}
	}
	action->setNumParameters(tmpl->m_numParameters);

	return action;
}

ScriptAction* SemanticScriptReader::parseActions(const nlohmann::ordered_json& json)
{
	if (!json.is_array() || json.empty())
		return NULL;

	ScriptAction* head = NULL;
	ScriptAction* tail = NULL;

	for (const auto& actionJson : json)
	{
		ScriptAction* action = parseAction(actionJson);
		if (action)
		{
			if (!head)
			{
				head = action;
				tail = action;
			}
			else
			{
				tail->setNextAction(action);
				tail = action;
			}
		}
	}

	return head;
}

Condition* SemanticScriptReader::parseCondition(const nlohmann::ordered_json& json)
{
	if (!json.is_object() || json.empty())
		return NULL;

	auto it = json.begin();
	std::string typeName = it.key();
	const nlohmann::ordered_json& params = it.value();

	int conditionType = findConditionType(typeName);
	if (conditionType < 0)
	{
		m_lastError = "Unknown condition type: " + typeName;
		return NULL;
	}

	Condition* condition = newInstance(Condition)();
	condition->setConditionType(static_cast<Condition::ConditionType>(conditionType));

	const ConditionTemplate* tmpl = TheScriptEngine->getConditionTemplate(conditionType);
	if (!tmpl)
		return condition;

	for (int i = 0; i < tmpl->m_numParameters; i++)
	{
		std::string paramName = SemanticScriptReader_getParameterName(i, tmpl);
		if (params.contains(paramName))
		{
			Parameter* param = parseParameter(params[paramName], tmpl->m_parameters[i]);
			condition->setParameter(i, param);
		}
		else
		{
			Parameter* param = newInstance(Parameter)(tmpl->m_parameters[i]);
			condition->setParameter(i, param);
		}
	}
	condition->setNumParameters(tmpl->m_numParameters);

	return condition;
}

OrCondition* SemanticScriptReader::parseConditions(const nlohmann::ordered_json& json)
{
	// Format: array of OR clauses, each containing array of AND conditions
	if (!json.is_array() || json.empty())
		return NULL;

	OrCondition* head = NULL;
	OrCondition* tail = NULL;

	for (const auto& orClauseJson : json)
	{
		if (!orClauseJson.is_array())
			continue;

		OrCondition* orCond = newInstance(OrCondition)();

		Condition* andHead = NULL;
		Condition* andTail = NULL;

		for (const auto& condJson : orClauseJson)
		{
			Condition* cond = parseCondition(condJson);
			if (cond)
			{
				if (!andHead)
				{
					andHead = cond;
					andTail = cond;
				}
				else
				{
					andTail->setNextCondition(cond);
					andTail = cond;
				}
			}
		}

		orCond->setFirstAndCondition(andHead);

		if (!head)
		{
			head = orCond;
			tail = orCond;
		}
		else
		{
			tail->setNextOrCondition(orCond);
			tail = orCond;
		}
	}

	return head;
}

Script* SemanticScriptReader::parseScript(const nlohmann::ordered_json& json)
{
	if (!json.is_object())
		return NULL;

	Script* script = newInstance(Script)();

	if (json.contains("name"))
		script->setName(AsciiString(json["name"].get<std::string>().c_str()));
	if (json.contains("comment"))
		script->setComment(AsciiString(json["comment"].get<std::string>().c_str()));
	if (json.contains("conditionComment"))
		script->setConditionComment(AsciiString(json["conditionComment"].get<std::string>().c_str()));
	if (json.contains("actionComment"))
		script->setActionComment(AsciiString(json["actionComment"].get<std::string>().c_str()));

	script->setActive(json.value("isActive", true));
	script->setOneShot(json.value("isOneShot", false));
	script->setSubroutine(json.value("isSubroutine", false));

	if (json.contains("difficulty"))
	{
		const auto& diff = json["difficulty"];
		script->setEasy(diff.value("easy", true));
		script->setNormal(diff.value("normal", true));
		script->setHard(diff.value("hard", true));
	}
	else
	{
		script->setEasy(true);
		script->setNormal(true);
		script->setHard(true);
	}

	script->setDelayEvalSeconds(json.value("evaluationDelay", 0));

	if (json.contains("if"))
	{
		script->setOrCondition(parseConditions(json["if"]));
	}

	if (json.contains("then"))
	{
		script->setAction(parseActions(json["then"]));
	}

	if (json.contains("else"))
	{
		script->setFalseAction(parseActions(json["else"]));
	}

	return script;
}

ScriptGroup* SemanticScriptReader::parseScriptGroup(const nlohmann::ordered_json& json)
{
	if (!json.is_object())
		return NULL;

	ScriptGroup* group = newInstance(ScriptGroup)();

	if (json.contains("name"))
		group->setName(AsciiString(json["name"].get<std::string>().c_str()));

	group->setActive(json.value("isActive", true));
	group->setSubroutine(json.value("isSubroutine", false));

	if (json.contains("scripts") && json["scripts"].is_array())
	{
		int scriptIndex = 0;
		for (const auto& scriptJson : json["scripts"])
		{
			Script* script = parseScript(scriptJson);
			if (script)
			{
				group->addScript(script, scriptIndex++);
			}
		}
	}

	return group;
}

ScriptList* SemanticScriptReader::parseScriptList(const nlohmann::ordered_json& json)
{
	if (!json.is_object())
		return NULL;

	ScriptList* list = newInstance(ScriptList)();

	if (json.contains("scripts") && json["scripts"].is_array())
	{
		int scriptIndex = 0;
		for (const auto& scriptJson : json["scripts"])
		{
			Script* script = parseScript(scriptJson);
			if (script)
			{
				list->addScript(script, scriptIndex++);
			}
		}
	}

	if (json.contains("groups") && json["groups"].is_array())
	{
		int groupIndex = 0;
		for (const auto& groupJson : json["groups"])
		{
			ScriptGroup* group = parseScriptGroup(groupJson);
			if (group)
			{
				list->addGroup(group, groupIndex++);
			}
		}
	}

	return list;
}

bool SemanticScriptReader::parseScriptsFile(const char* jsonData, size_t length,
                                            ScriptList** outScriptLists, int* outNumPlayers)
{
	try
	{
		nlohmann::ordered_json root = nlohmann::ordered_json::parse(jsonData, jsonData + length);

		if (!root.is_object())
		{
			m_lastError = "Root is not an object";
			return false;
		}

		int version = root.value("version", 1);
		if (version != 1)
		{
			m_lastError = "Unsupported version: " + std::to_string(version);
			return false;
		}

		if (!root.contains("players") || !root["players"].is_array())
		{
			m_lastError = "Missing or invalid 'players' array";
			return false;
		}

		const auto& players = root["players"];
		*outNumPlayers = static_cast<int>(players.size());

		for (int i = 0; i < *outNumPlayers && i < MAX_PLAYER_COUNT; i++)
		{
			outScriptLists[i] = parseScriptList(players[i]);
		}

		return true;
	}
	catch (const std::exception& e)
	{
		m_lastError = e.what();
		return false;
	}
}

// Helper function to get parameter name - uses the same logic as writer
std::string SemanticScriptReader_getParameterName(int paramIndex, const Template* tmpl)
{
	if (!tmpl || paramIndex < 0 || paramIndex >= tmpl->m_numUiStrings)
	{
		return "param" + std::to_string(paramIndex);
	}

	AsciiString uiStr = tmpl->m_uiStrings[paramIndex];
	if (uiStr.isEmpty())
		return "param" + std::to_string(paramIndex);

	std::string name = uiStr.str();
	while (!name.empty() && (name.back() == ':' || name.back() == ' '))
		name.pop_back();

	std::string result;
	bool capitalizeNext = false;
	bool firstChar = true;
	for (char c : name)
	{
		if (c == ' ' || c == '_')
		{
			capitalizeNext = true;
		}
		else if (std::isalnum(c))
		{
			if (firstChar)
			{
				result += static_cast<char>(std::tolower(c));
				firstChar = false;
			}
			else if (capitalizeNext)
			{
				result += static_cast<char>(std::toupper(c));
				capitalizeNext = false;
			}
			else
			{
				result += static_cast<char>(std::tolower(c));
			}
		}
	}

	return result.empty() ? "param" + std::to_string(paramIndex) : result;
}

#endif // RTS_HAS_JSON_CHUNK
