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

#pragma once

#ifdef RTS_HAS_JSON_CHUNK

#include "Common/AsciiString.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class Script;
class ScriptGroup;
class ScriptList;
class ScriptAction;
class Condition;
class OrCondition;
class Parameter;
class ActionTemplate;
class ConditionTemplate;

// Semantic JSON format for scripts - produces clean, human-readable JSON
// Format example:
// {
//   "name": "TestScript",
//   "isActive": true,
//   "if": [
//     [{ "condition_true": {} }]  // OR clauses containing AND conditions
//   ],
//   "then": [
//     { "setMillisecondTimer": { "counter": "Timer", "value": 10.0 } }
//   ]
// }

class SemanticScriptWriter
{
public:
	SemanticScriptWriter();

	nlohmann::ordered_json writeScriptsFile(ScriptList** scriptLists, int numPlayers);
	nlohmann::ordered_json writeScriptList(ScriptList* scriptList);
	nlohmann::ordered_json writeScriptGroup(ScriptGroup* group);
	nlohmann::ordered_json writeScript(Script* script);
	nlohmann::ordered_json writeConditions(OrCondition* orCondition);
	nlohmann::ordered_json writeCondition(Condition* condition);
	nlohmann::ordered_json writeActions(ScriptAction* action);
	nlohmann::ordered_json writeAction(ScriptAction* action);
	std::string getJSONString(int indent = 2) const;

	const std::vector<std::string>& getWarnings() const { return m_warnings; }
	void clearWarnings() { m_warnings.clear(); }
	bool hasWarnings() const { return !m_warnings.empty(); }

private:
	void writeParameter(nlohmann::ordered_json& params, Parameter* param, int paramIndex,
	                    const ActionTemplate* actionTemplate);
	void writeParameter(nlohmann::ordered_json& params, Parameter* param, int paramIndex,
	                    const ConditionTemplate* conditionTemplate);
	std::string toCamelCase(const AsciiString& internalName);
	std::string getParameterName(int paramIndex, const ActionTemplate* tmpl);
	std::string getParameterName(int paramIndex, const ConditionTemplate* tmpl);
	void addWarning(const std::string& message) { m_warnings.push_back(message); }

	nlohmann::ordered_json m_root;
	std::vector<std::string> m_warnings;
};

class SemanticScriptReader
{
public:
	SemanticScriptReader();

	bool parseScriptsFile(const char* jsonData, size_t length, ScriptList** outScriptLists, int* outNumPlayers);
	ScriptList* parseScriptList(const nlohmann::ordered_json& json);
	ScriptGroup* parseScriptGroup(const nlohmann::ordered_json& json);
	Script* parseScript(const nlohmann::ordered_json& json);
	OrCondition* parseConditions(const nlohmann::ordered_json& json);
	Condition* parseCondition(const nlohmann::ordered_json& json);
	ScriptAction* parseActions(const nlohmann::ordered_json& json);
	ScriptAction* parseAction(const nlohmann::ordered_json& json);

	const std::string& getLastError() const { return m_lastError; }
	const std::vector<std::string>& getWarnings() const { return m_warnings; }
	void clearWarnings() { m_warnings.clear(); }
	bool hasWarnings() const { return !m_warnings.empty(); }

private:
	int findActionType(const std::string& name);
	int findConditionType(const std::string& name);
	Parameter* parseParameter(const nlohmann::ordered_json& json, int expectedType);
	std::string fromCamelCase(const std::string& camelCase);
	void addWarning(const std::string& message) { m_warnings.push_back(message); }

	std::string m_lastError;
	std::vector<std::string> m_warnings;
};

#endif // RTS_HAS_JSON_CHUNK
