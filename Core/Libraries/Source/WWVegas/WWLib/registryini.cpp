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

// TheSuperHackers @feature bobtista 28/04/2026 INI-file backed Windows
// registry replacement for non-Windows builds. Adapted from fbraz3/GeneralsX
// (felipebraz). Persistence lives at:
//   macOS  : ~/Library/Application Support/TheSuperHackers/registry.ini
//   Linux  : $XDG_CONFIG_HOME/TheSuperHackers/registry.ini
//            (falls back to ~/.config/TheSuperHackers/registry.ini)
//   Windows: %APPDATA%\TheSuperHackers\registry.ini (only used by
//            consumers that explicitly opt in; the real Win registry
//            remains the default).
// Sections mirror the Windows registry path scheme, e.g.
//   [hkey_current_user\software\electronic arts\ea games\command and conquer generals zero hour]
// so the same ReadString/WriteString call shape works on every platform.

#include "registryini.h"

#include <ctype.h>
#include <errno.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

namespace
{
	typedef std::map<std::string, std::string> RegistryIniSection;
	typedef std::map<std::string, RegistryIniSection> RegistryIniData;

	const char *kCurrentUserRoot = "HKEY_CURRENT_USER";
	const char *kLocalMachineRoot = "HKEY_LOCAL_MACHINE";
	const char *kDefaultValueKey = "@";
	// TheSuperHackers @tweak bobtista 05/06/2026 Name the vendor dir and store filename
	// once so the registry location is defined in a single place.
	const char *kVendorDirName = "TheSuperHackers";
	const char *kRegistryIniFileName = "registry.ini";

	std::string TrimCopy(const std::string &value)
	{
		std::string::size_type start = 0;
		while (start < value.length() && isspace(static_cast<unsigned char>(value[start])))
		{
			++start;
		}

		std::string::size_type end = value.length();
		while (end > start && isspace(static_cast<unsigned char>(value[end - 1])))
		{
			--end;
		}

		return value.substr(start, end - start);
	}

	// TheSuperHackers @bugfix bobtista 05/06/2026 Escape backslash/newline/CR so
	// stored values survive a round-trip. Without this, a value containing a
	// newline split into a bogus second line on load, and leading/trailing
	// spaces were silently stripped because the value was Trim-copied on read.
	std::string EscapeValue(const std::string &value)
	{
		std::string out;
		out.reserve(value.length());
		for (std::string::size_type i = 0; i < value.length(); ++i)
		{
			const char c = value[i];
			switch (c)
			{
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				default: out += c; break;
			}
		}
		return out;
	}

	std::string UnescapeValue(const std::string &value)
	{
		std::string out;
		out.reserve(value.length());
		for (std::string::size_type i = 0; i < value.length(); ++i)
		{
			if (value[i] == '\\' && i + 1 < value.length())
			{
				const char next = value[i + 1];
				if (next == '\\')
				{
					out += '\\';
					++i;
					continue;
				}
				if (next == 'n')
				{
					out += '\n';
					++i;
					continue;
				}
				if (next == 'r')
				{
					out += '\r';
					++i;
					continue;
				}
			}
			out += value[i];
		}
		return out;
	}

	std::string ToLowerAscii(std::string value)
	{
		for (std::string::size_type i = 0; i < value.length(); ++i)
		{
			value[i] = static_cast<char>(tolower(static_cast<unsigned char>(value[i])));
		}

		return value;
	}

	bool StartsWith(const std::string &value, const char *prefix)
	{
		const std::string::size_type prefixLength = strlen(prefix);
		return value.length() >= prefixLength && value.compare(0, prefixLength, prefix) == 0;
	}

	bool EnsureDirectoryRecursive(const std::string &directory);

	std::string GetRegistryIniDirectory()
	{
#ifdef _WIN32
		const char *appData = getenv("APPDATA");
		if (appData != nullptr && appData[0] != '\0')
		{
			return std::string(appData) + "\\" + kVendorDirName;
		}

		const char *userProfile = getenv("USERPROFILE");
		if (userProfile != nullptr && userProfile[0] != '\0')
		{
			return std::string(userProfile) + "\\AppData\\Roaming\\" + kVendorDirName;
		}
#else
		const char *home = getenv("HOME");
#ifdef __APPLE__
		if (home != nullptr && home[0] != '\0')
		{
			return std::string(home) + "/Library/Application Support/" + kVendorDirName;
		}
#else
		const char *xdgConfigHome = getenv("XDG_CONFIG_HOME");
		if (xdgConfigHome != nullptr && xdgConfigHome[0] != '\0')
		{
			return std::string(xdgConfigHome) + "/" + kVendorDirName;
		}

		if (home != nullptr && home[0] != '\0')
		{
			return std::string(home) + "/.config/" + kVendorDirName;
		}
#endif
#endif

		return kVendorDirName;
	}

	std::string GetRegistryIniPath()
	{
#ifdef _WIN32
		return GetRegistryIniDirectory() + "\\" + kRegistryIniFileName;
#else
		return GetRegistryIniDirectory() + "/" + kRegistryIniFileName;
#endif
	}

	int MakeDirectory(const char *path)
	{
#ifdef _WIN32
		return _mkdir(path);
#else
		return mkdir(path, 0755);
#endif
	}

	bool EnsureDirectoryRecursive(const std::string &directory)
	{
		if (directory.empty())
		{
			return false;
		}

		std::string current;
		for (std::string::size_type i = 0; i < directory.length(); ++i)
		{
			const char currentChar = directory[i];
			current += currentChar;

			if (currentChar != '/' && currentChar != '\\')
			{
				continue;
			}

			if (current.length() <= 1)
			{
				continue;
			}

			// Skip drive letter root (e.g. "C:\") on Windows.
			if (current.length() == 3 && current[1] == ':')
			{
				continue;
			}

			if (MakeDirectory(current.c_str()) != 0 && errno != EEXIST)
			{
				return false;
			}
		}

		// TheSuperHackers @bugfix bobtista 28/05/2026 Only call MakeDirectory
		// for the trailing path component when the input lacked a separator.
		// When the loop already created it (path ended with '/' or '\\'),
		// a second call here would re-stat EEXIST or fail spuriously.
		if (!directory.empty())
		{
			const char lastChar = directory[directory.length() - 1];
			if (lastChar != '/' && lastChar != '\\')
			{
				if (MakeDirectory(directory.c_str()) != 0 && errno != EEXIST)
				{
					return false;
				}
			}
		}

		return true;
	}

	bool EnsureRegistryStorageExists()
	{
		const std::string directory = GetRegistryIniDirectory();
		if (!EnsureDirectoryRecursive(directory))
		{
			return false;
		}

		FILE *file = fopen(GetRegistryIniPath().c_str(), "a");
		if (file == nullptr)
		{
			return false;
		}

		fclose(file);
		return true;
	}

	std::string NormalizeRoot(const char *root)
	{
		const std::string lowered = ToLowerAscii(TrimCopy(root == nullptr ? "" : root));
		if (lowered == "hkey_local_machine" || lowered == "hklm")
		{
			return ToLowerAscii(kLocalMachineRoot);
		}

		if (lowered == "hkey_current_user" || lowered == "hkcu" || lowered.empty())
		{
			return ToLowerAscii(kCurrentUserRoot);
		}

		return lowered;
	}

	void StripKnownRootPrefix(std::string &path, std::string &root)
	{
		const std::string lowered = ToLowerAscii(path);
		if (StartsWith(lowered, "hkey_local_machine\\"))
		{
			root = ToLowerAscii(kLocalMachineRoot);
			path.erase(0, strlen("HKEY_LOCAL_MACHINE\\"));
		}
		else if (StartsWith(lowered, "hklm\\"))
		{
			root = ToLowerAscii(kLocalMachineRoot);
			path.erase(0, strlen("HKLM\\"));
		}
		else if (StartsWith(lowered, "hkey_current_user\\"))
		{
			root = ToLowerAscii(kCurrentUserRoot);
			path.erase(0, strlen("HKEY_CURRENT_USER\\"));
		}
		else if (StartsWith(lowered, "hkcu\\"))
		{
			root = ToLowerAscii(kCurrentUserRoot);
			path.erase(0, strlen("HKCU\\"));
		}
	}

	std::string NormalizeRegistryPath(const char *path)
	{
		std::string normalized = TrimCopy(path == nullptr ? "" : path);
		for (std::string::size_type i = 0; i < normalized.length(); ++i)
		{
			if (normalized[i] == '/')
			{
				normalized[i] = '\\';
			}
		}

		normalized = ToLowerAscii(normalized);
		while (!normalized.empty() && normalized[0] == '\\')
		{
			normalized.erase(0, 1);
		}
		while (!normalized.empty() && normalized[normalized.length() - 1] == '\\')
		{
			normalized.erase(normalized.length() - 1);
		}

		std::vector<std::string> parts;
		std::string current;
		for (std::string::size_type i = 0; i < normalized.length(); ++i)
		{
			if (normalized[i] == '\\')
			{
				if (!current.empty())
				{
					if (current != "wow6432node")
					{
						parts.push_back(current);
					}
					current.clear();
				}
				continue;
			}

			current += normalized[i];
		}

		if (!current.empty() && current != "wow6432node")
		{
			parts.push_back(current);
		}

		// Map common product-name variations to a canonical form so mods and
		// variants that write to slightly different paths still find each
		// other's keys. Adapted from fbraz3/GeneralsX.
		if (!parts.empty())
		{
			static const struct Alias
			{
				const char *from;
				const char *to;
			} aliases[] = {
				{ "zero hour", "command and conquer generals zero hour" },
				{ "generals zero hour", "command and conquer generals zero hour" },
				{ "command & conquer generals zero hour", "command and conquer generals zero hour" },
				{ "command and conquer generals", "command and conquer generals" },
				{ "command and conquer generals zh", "command and conquer generals zero hour" },
				{ "cnc_generals_zh", "command and conquer generals zero hour" },
			};

			for (std::size_t i = 0; i < parts.size(); ++i)
			{
				for (std::size_t aliasTry = 0; aliasTry < 6 && i + aliasTry < parts.size(); ++aliasTry)
				{
					std::string combined = parts[i];
					for (std::size_t k = 1; k <= aliasTry; ++k)
					{
						combined += " ";
						combined += parts[i + k];
					}

					bool matched = false;
					for (std::size_t a = 0; a < sizeof(aliases) / sizeof(aliases[0]); ++a)
					{
						if (combined == aliases[a].from)
						{
							parts[i] = std::string(aliases[a].to);
							if (aliasTry > 0)
							{
								parts.erase(parts.begin() + i + 1, parts.begin() + i + 1 + aliasTry);
							}
							matched = true;
							break;
						}
					}

					if (matched)
					{
						break;
					}
				}
			}
		}

		normalized.clear();
		for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++it)
		{
			if (!normalized.empty())
			{
				normalized += "\\";
			}
			normalized += *it;
		}

		return normalized;
	}

	std::string NormalizeSectionName(const char *root, const char *path)
	{
		std::string normalizedRoot = NormalizeRoot(root);
		std::string rawPath = TrimCopy(path == nullptr ? "" : path);
		StripKnownRootPrefix(rawPath, normalizedRoot);
		const std::string normalizedPath = NormalizeRegistryPath(rawPath.c_str());

		if (normalizedPath.empty())
		{
			return normalizedRoot;
		}

		return normalizedRoot + "\\" + normalizedPath;
	}

	std::string NormalizeStoredSectionName(const std::string &section)
	{
		return NormalizeSectionName(nullptr, section.c_str());
	}

	std::string BuildSectionName(const char *root, const char *path)
	{
		return NormalizeSectionName(root, path);
	}

	std::string BuildKeyName(const char *key)
	{
		const std::string normalized = ToLowerAscii(TrimCopy(key == nullptr ? "" : key));
		if (normalized.empty() || normalized == "@" || normalized == "@default" || normalized == "(default)" || normalized == "default")
		{
			return kDefaultValueKey;
		}

		return normalized;
	}

	bool LoadRegistryIni(RegistryIniData &data)
	{
		if (!EnsureRegistryStorageExists())
		{
			return false;
		}

		FILE *file = fopen(GetRegistryIniPath().c_str(), "r");
		if (file == nullptr)
		{
			return false;
		}

		char line[4096];
		std::string currentSection;
		bool firstLine = true;

		while (fgets(line, sizeof(line), file) != nullptr)
		{
			std::string currentLine = line;
			// Skip UTF-8 BOM if present on the very first line.
			if (firstLine && currentLine.length() >= 3
				&& static_cast<unsigned char>(currentLine[0]) == 0xEF
				&& static_cast<unsigned char>(currentLine[1]) == 0xBB
				&& static_cast<unsigned char>(currentLine[2]) == 0xBF)
			{
				currentLine.erase(0, 3);
			}
			firstLine = false;

			while (!currentLine.empty()
				&& (currentLine[currentLine.length() - 1] == '\n'
					|| currentLine[currentLine.length() - 1] == '\r'))
			{
				currentLine.erase(currentLine.length() - 1);
			}

			const std::string trimmedLine = TrimCopy(currentLine);
			if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#')
			{
				continue;
			}

			if (trimmedLine[0] == '[' && trimmedLine[trimmedLine.length() - 1] == ']')
			{
				currentSection = NormalizeStoredSectionName(trimmedLine.substr(1, trimmedLine.length() - 2));
				continue;
			}

			const std::string::size_type separator = currentLine.find('=');
			if (separator == std::string::npos || currentSection.empty())
			{
				continue;
			}

			const std::string key = BuildKeyName(currentLine.substr(0, separator).c_str());
			// TheSuperHackers @bugfix bobtista 05/06/2026 Unescape and do NOT trim
			// the value so leading/trailing spaces and escaped characters round-trip.
			const std::string value = UnescapeValue(currentLine.substr(separator + 1));
			data[currentSection][key] = value;
		}

		// TheSuperHackers @bugfix bobtista 05/06/2026 Distinguish a real read error
		// from EOF so callers do not overwrite a store they failed to fully read.
		if (ferror(file))
		{
			fclose(file);
			return false;
		}

		fclose(file);
		return true;
	}

	bool SaveRegistryIni(const RegistryIniData &data)
	{
		if (!EnsureRegistryStorageExists())
		{
			return false;
		}

		// TheSuperHackers @bugfix bobtista 28/05/2026 Write to a temporary file
		// then rename it over the target so a crash mid-write cannot leave the
		// registry truncated or empty. POSIX rename() is atomic on the same
		// filesystem.
		const std::string finalPath = GetRegistryIniPath();
		const std::string tempPath = finalPath + ".tmp";

		FILE *file = fopen(tempPath.c_str(), "w");
		if (file == nullptr)
		{
			return false;
		}

		for (RegistryIniData::const_iterator sectionIt = data.begin(); sectionIt != data.end(); ++sectionIt)
		{
			fprintf(file, "[%s]\n", sectionIt->first.c_str());
			for (RegistryIniSection::const_iterator valueIt = sectionIt->second.begin(); valueIt != sectionIt->second.end(); ++valueIt)
			{
				fprintf(file, "%s=%s\n",
					valueIt->first == kDefaultValueKey ? "@" : valueIt->first.c_str(),
					EscapeValue(valueIt->second).c_str());
			}
			fprintf(file, "\n");
		}

		if (fflush(file) != 0)
		{
			fclose(file);
			remove(tempPath.c_str());
			return false;
		}

		if (fclose(file) != 0)
		{
			remove(tempPath.c_str());
			return false;
		}

#ifdef _WIN32
		// Windows rename() fails if the destination exists; remove it first.
		remove(finalPath.c_str());
#endif
		if (rename(tempPath.c_str(), finalPath.c_str()) != 0)
		{
			remove(tempPath.c_str());
			return false;
		}

		return true;
	}
}

namespace RegistryIni
{
	const char *CurrentUserRoot()
	{
		return kCurrentUserRoot;
	}

	const char *LocalMachineRoot()
	{
		return kLocalMachineRoot;
	}

	bool ReadString(const char *root, const char *path, const char *key, std::string &value)
	{
		RegistryIniData data;
		LoadRegistryIni(data);

		const RegistryIniData::const_iterator sectionIt = data.find(BuildSectionName(root, path));
		if (sectionIt == data.end())
		{
			return false;
		}

		const RegistryIniSection::const_iterator valueIt = sectionIt->second.find(BuildKeyName(key));
		if (valueIt == sectionIt->second.end())
		{
			return false;
		}

		value = valueIt->second;
		return true;
	}

	bool ReadUnsignedInt(const char *root, const char *path, const char *key, unsigned int &value)
	{
		std::string storedValue;
		if (!ReadString(root, path, key, storedValue))
		{
			return false;
		}

		char *end = nullptr;
		const unsigned long parsedValue = strtoul(storedValue.c_str(), &end, 0);
		if (end == storedValue.c_str() || *end != '\0')
		{
			return false;
		}

		value = static_cast<unsigned int>(parsedValue);
		return true;
	}

	bool WriteString(const char *root, const char *path, const char *key, const char *value)
	{
		RegistryIniData data;
		// TheSuperHackers @bugfix bobtista 05/06/2026 Bail on a failed load rather
		// than overwriting the store with partial/empty data.
		if (!LoadRegistryIni(data))
		{
			return false;
		}
		data[BuildSectionName(root, path)][BuildKeyName(key)] = value == nullptr ? "" : value;
		return SaveRegistryIni(data);
	}

	bool WriteUnsignedInt(const char *root, const char *path, const char *key, unsigned int value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%u", value);
		return WriteString(root, path, key, buffer);
	}

	bool SectionExists(const char *root, const char *path)
	{
		RegistryIniData data;
		LoadRegistryIni(data);
		return data.find(BuildSectionName(root, path)) != data.end();
	}

	bool DeleteValue(const char *root, const char *path, const char *key)
	{
		RegistryIniData data;
		if (!LoadRegistryIni(data))
		{
			return false;
		}

		RegistryIniData::iterator sectionIt = data.find(BuildSectionName(root, path));
		if (sectionIt == data.end())
		{
			return false;
		}

		sectionIt->second.erase(BuildKeyName(key));
		if (sectionIt->second.empty())
		{
			data.erase(sectionIt);
		}

		return SaveRegistryIni(data);
	}

	bool ClearSection(const char *root, const char *path)
	{
		RegistryIniData data;
		if (!LoadRegistryIni(data))
		{
			return false;
		}
		data.erase(BuildSectionName(root, path));
		return SaveRegistryIni(data);
	}

	bool ListValues(const char *root, const char *path, std::vector<std::string> &keys)
	{
		keys.clear();
		RegistryIniData data;
		LoadRegistryIni(data);

		const RegistryIniData::const_iterator sectionIt = data.find(BuildSectionName(root, path));
		if (sectionIt == data.end())
		{
			return false;
		}

		for (RegistryIniSection::const_iterator valueIt = sectionIt->second.begin(); valueIt != sectionIt->second.end(); ++valueIt)
		{
			keys.push_back(valueIt->first == kDefaultValueKey ? "" : valueIt->first);
		}

		return true;
	}
}
