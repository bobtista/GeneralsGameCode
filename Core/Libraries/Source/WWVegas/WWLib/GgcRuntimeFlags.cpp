/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "GgcRuntimeFlags.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

struct GgcFlagInfo
{
	const char *name;
	const char *alias;
	unsigned char tier;
	unsigned char type;
	int defaultInt;
	float defaultFloat;
	const char *help;
};

const GgcFlagInfo s_table[GgcFlagCount] =
{
#define GGC_RUNTIME_FLAG_TABLE_ENTRY(id, name, alias, tier, type, defaultInt, defaultFloat, help) \
	{ name, alias, (unsigned char)(tier), (unsigned char)(type), defaultInt, defaultFloat, help },
	GGC_RUNTIME_FLAG_LIST(GGC_RUNTIME_FLAG_TABLE_ENTRY)
#undef GGC_RUNTIME_FLAG_TABLE_ENTRY
};

// Storage is deliberately destructor-free: GgcFlag_StrictPoolShutdown is read
// from ObjectPoolClass destructors during static destruction at process exit.
struct GgcFlagState
{
	int enabled;
	int intValue;
	float floatValue;
	const char *strValue;
};

GgcFlagState s_state[GgcFlagCount];
std::atomic<int> s_resolved[GgcFlagCount];

// Diagnostic- and probe-tier flags resolve as unset when the build does not
// compile the diagnostic tools in, so they cannot be enabled in such builds.
bool TierResolvable(unsigned char tier)
{
#if defined(GGC_DIAGNOSTIC_TOOLS)
	(void)tier;
	return true;
#else
	return tier != (unsigned char)GgcTier_Diagnostic && tier != (unsigned char)GgcTier_Probe;
#endif
}

bool EqualsIgnoreCase(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		char ca = *a;
		char cb = *b;
		if (ca >= 'A' && ca <= 'Z')
		{
			ca = (char)(ca - 'A' + 'a');
		}
		if (cb >= 'A' && cb <= 'Z')
		{
			cb = (char)(cb - 'A' + 'a');
		}
		if (ca != cb)
		{
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

void StoreResolvedValue(GgcFlagId id, const char *value)
{
	GgcFlagState &state = s_state[id];
	const GgcFlagInfo &info = s_table[id];

	state.strValue = value;
	state.intValue = (value != NULL) ? std::atoi(value) : info.defaultInt;
	state.floatValue = (value != NULL) ? (float)std::atof(value) : info.defaultFloat;

	switch (info.type)
	{
	case GgcFlagType_Truthy:
		state.enabled = (value != NULL && (std::strcmp(value, "1") == 0 || EqualsIgnoreCase(value, "true"))) ? 1 : 0;
		break;
	case GgcFlagType_ZeroOff:
		// On unless explicitly set to a non-empty value that parses to zero.
		state.enabled = (value != NULL && *value != '\0' && std::atoi(value) == 0) ? 0 : 1;
		break;
	default:
		state.enabled = (value != NULL) ? 1 : 0;
		break;
	}

	// Concurrent first reads compute identical values from the same
	// environment, so racing writers are benign - same guarantee the
	// per-site function-local statics gave before the registry.
	s_resolved[id].store(1, std::memory_order_release);
}

const GgcFlagState &Resolve(GgcFlagId id)
{
	GgcFlagState &state = s_state[id];
	if (s_resolved[id].load(std::memory_order_acquire) != 0)
	{
		return state;
	}

	const GgcFlagInfo &info = s_table[id];
	const char *value = NULL;
	if (TierResolvable(info.tier))
	{
		value = std::getenv(info.name);
		if (value == NULL && info.alias != NULL)
		{
			value = std::getenv(info.alias);
		}
	}

	StoreResolvedValue(id, value);
	return state;
}

} // namespace

namespace GgcFlags
{

bool Enabled(GgcFlagId id)
{
	return Resolve(id).enabled != 0;
}

int IntValue(GgcFlagId id)
{
	return Resolve(id).intValue;
}

float FloatValue(GgcFlagId id)
{
	return Resolve(id).floatValue;
}

const char *StringValue(GgcFlagId id)
{
	return Resolve(id).strValue;
}

void SetOverride(GgcFlagId id, const char *value)
{
	if (!TierResolvable(s_table[id].tier))
	{
		return;
	}
	const char *copy = NULL;
	if (value != NULL)
	{
		// Deliberately never freed: flag values live for the whole process,
		// matching the lifetime getenv pointers had before the registry.
		const size_t size = std::strlen(value) + 1;
		char *owned = (char *)std::malloc(size);
		std::memcpy(owned, value, size);
		copy = owned;
	}
	StoreResolvedValue(id, copy);
}

void DumpTableIfRequested()
{
	if (std::getenv("GGC_LIST_FLAGS") == NULL)
	{
		return;
	}

	static const char *const tierNames[] = { "setting", "kill-switch", "diagnostic", "probe", "harness", "workaround" };
	static const char *const typeNames[] = { "presence", "truthy", "zero-off", "int", "float", "string" };

	std::fprintf(stderr, "[ggc] %d runtime flags (GGC_LIST_FLAGS):\n", (int)GgcFlagCount);
	for (int i = 0; i < (int)GgcFlagCount; ++i)
	{
		const GgcFlagInfo &info = s_table[i];
		const GgcFlagState &state = Resolve((GgcFlagId)i);
		const char *value = state.strValue;
		char valueText[64];
		if (!TierResolvable(info.tier))
		{
			std::snprintf(valueText, sizeof(valueText), "(compiled out)");
		}
		else if (value == NULL)
		{
			std::snprintf(valueText, sizeof(valueText), "(unset)");
		}
		else
		{
			std::snprintf(valueText, sizeof(valueText), "\"%s\"", value);
		}
		std::fprintf(stderr, "[ggc]   %-11s %-8s %-48s %-16s %s\n",
			tierNames[info.tier], typeNames[info.type], info.name, valueText, info.help);
		if (info.alias != NULL)
		{
			std::fprintf(stderr, "[ggc]   %-11s %-8s %-48s (alias of %s)\n",
				tierNames[info.tier], typeNames[info.type], info.alias, info.name);
		}
	}
	std::fflush(stderr);
}

}
