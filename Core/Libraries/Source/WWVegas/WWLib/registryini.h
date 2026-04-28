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
// (felipebraz). Stores HKLM/HKCU emulated key/value pairs in a
// platform-appropriate config file so the engine's existing registry callers
// keep working unchanged on Linux and macOS.

#pragma once

#include <string>
#include <vector>

namespace RegistryIni
{
	const char *CurrentUserRoot();
	const char *LocalMachineRoot();

	bool ReadString(const char *root, const char *path, const char *key, std::string &value);
	bool ReadUnsignedInt(const char *root, const char *path, const char *key, unsigned int &value);
	bool WriteString(const char *root, const char *path, const char *key, const char *value);
	bool WriteUnsignedInt(const char *root, const char *path, const char *key, unsigned int value);
	bool SectionExists(const char *root, const char *path);
	bool DeleteValue(const char *root, const char *path, const char *key);
	bool ClearSection(const char *root, const char *path);
	bool ListValues(const char *root, const char *path, std::vector<std::string> &keys);
}
