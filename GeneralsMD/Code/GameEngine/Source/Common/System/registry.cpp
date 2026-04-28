/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// Registry.cpp
// Simple interface for storing/retrieving registry values
// Author: Matthew D. Campbell, December 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Registry.h"

#ifdef _UNIX
#include "registryini.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// TheSuperHackers @feature bobtista 28/04/2026 Non-Windows registry path.
// Adapted from fbraz3/GeneralsX (felipebraz). Persisted reads
// and writes go through RegistryIni; install paths and language can also be
// overridden via env vars (CNC_ZH_<KEY>, CNC_GENERALS_<KEY>) or auto-detected
// from the on-disk asset layout.

#ifndef HKEY_LOCAL_MACHINE
typedef void *HKEY;
#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002)
#define HKEY_CURRENT_USER  ((HKEY)(uintptr_t)0x80000001)
#endif

static const char *getRegistryIniRoot(HKEY root)
{
	return root == HKEY_LOCAL_MACHINE ? RegistryIni::LocalMachineRoot() : RegistryIni::CurrentUserRoot();
}

Bool getStringFromRegistry(HKEY root, AsciiString path, AsciiString key, AsciiString& val)
{
	std::string storedValue;
	if (!RegistryIni::ReadString(getRegistryIniRoot(root), path.str(), key.str(), storedValue))
	{
		return FALSE;
	}

	val = storedValue.c_str();
	return TRUE;
}

Bool getUnsignedIntFromRegistry(HKEY root, AsciiString path, AsciiString key, UnsignedInt& val)
{
	unsigned int storedValue = 0;
	if (!RegistryIni::ReadUnsignedInt(getRegistryIniRoot(root), path.str(), key.str(), storedValue))
	{
		return FALSE;
	}

	val = storedValue;
	return TRUE;
}

Bool setStringInRegistry(HKEY root, AsciiString path, AsciiString key, AsciiString val)
{
	return RegistryIni::WriteString(getRegistryIniRoot(root), path.str(), key.str(), val.str()) ? TRUE : FALSE;
}

Bool setUnsignedIntInRegistry(HKEY root, AsciiString path, AsciiString key, UnsignedInt val)
{
	return RegistryIni::WriteUnsignedInt(getRegistryIniRoot(root), path.str(), key.str(), val) ? TRUE : FALSE;
}

// Look up an environment variable formed from a fixed prefix plus the
// upper-cased key name. Example: getEnvVar("CNC_ZH_", "InstallPath", val) reads
// $CNC_ZH_INSTALLPATH. Adapted from GeneralsX (felipebraz 25/02/2026).
static Bool getEnvVar(const char *prefix, AsciiString key, AsciiString& val)
{
	char envName[256] = { 0 };
	const char *keyStr = key.str();
	const std::size_t prefixLen = strlen(prefix);
	const std::size_t keyLen = strlen(keyStr);

	if (prefixLen + keyLen + 1 > sizeof(envName))
	{
		return FALSE;
	}

	strcpy(envName, prefix);
	for (std::size_t i = 0; i < keyLen; ++i)
	{
		envName[prefixLen + i] = static_cast<char>(toupper(static_cast<unsigned char>(keyStr[i])));
	}
	envName[prefixLen + keyLen] = '\0';

	const char *envValue = getenv(envName);
	if (envValue != nullptr && envValue[0] != '\0')
	{
		val = envValue;
		DEBUG_LOG(("getEnvVar - found %s = %s", envName, envValue));
		return TRUE;
	}

	return FALSE;
}

// Auto-detect base Generals install path. Steam Zero Hour standalone puts the
// base Generals assets in ./ZH_Generals/; a dev split install puts them in
// ../Generals/. Adapted from GeneralsX (felipebraz 18/02/2026).
static Bool tryRelativeGeneralsPath(AsciiString& val)
{
	struct stat st;

	const char *steamPath = "ZH_Generals/";
	if (stat(steamPath, &st) == 0 && S_ISDIR(st.st_mode))
	{
		val = steamPath;
		DEBUG_LOG(("tryRelativeGeneralsPath - found base Generals at %s (Steam standalone)", steamPath));
		return TRUE;
	}

	const char *devPath = "../Generals/";
	if (stat(devPath, &st) == 0 && S_ISDIR(st.st_mode))
	{
		val = devPath;
		DEBUG_LOG(("tryRelativeGeneralsPath - found base Generals at %s (dev layout)", devPath));
		return TRUE;
	}

	return FALSE;
}

static Bool doesLanguageBigExist(const char *rootPath, const char *bigFile)
{
	struct stat st;
	char lowerBigFile[256] = { 0 };

	for (Int i = 0; bigFile[i] != '\0' && i < static_cast<Int>(sizeof(lowerBigFile) - 1); ++i)
	{
		lowerBigFile[i] = static_cast<char>(tolower(static_cast<unsigned char>(bigFile[i])));
	}

	if (rootPath == nullptr || rootPath[0] == '\0')
	{
		if (stat(bigFile, &st) == 0 || stat(lowerBigFile, &st) == 0)
		{
			return TRUE;
		}
		return FALSE;
	}

	AsciiString fullPath = rootPath;
	const Int len = fullPath.getLength();
	if (len > 0)
	{
		const char lastChar = fullPath.getCharAt(len - 1);
		if (lastChar != '/' && lastChar != '\\')
		{
			fullPath.concat('/');
		}
	}
	fullPath.concat(bigFile);

	if (stat(fullPath.str(), &st) == 0)
	{
		return TRUE;
	}

	AsciiString lowerPath = rootPath;
	const Int lowerLen = lowerPath.getLength();
	if (lowerLen > 0)
	{
		const char lastChar = lowerPath.getCharAt(lowerLen - 1);
		if (lastChar != '/' && lastChar != '\\')
		{
			lowerPath.concat('/');
		}
	}
	lowerPath.concat(lowerBigFile);

	return stat(lowerPath.str(), &st) == 0 ? TRUE : FALSE;
}

// Detect the localized language by probing for the matching language-specific
// .big file in the configured asset roots, mirroring the Windows installer's
// language selection. Adapted from fbraz3/GeneralsX (felipebraz).
static Bool tryAutoDetectLanguage(AsciiString& val)
{
	const struct
	{
		const char *bigFile;
		const char *language;
	} candidates[] = {
		{ "BrazilianZH.big", "brazilian" },
		{ "EnglishZH.big",   "english"   },
		{ "GermanZH.big",    "german"    },
		{ "FrenchZH.big",    "french"    },
		{ "SpanishZH.big",   "spanish"   },
		{ "ChineseZH.big",   "chinese"   },
		{ "KoreanZH.big",    "korean"    },
		{ "PolishZH.big",    "polish"    },
		{ nullptr,           nullptr     }
	};

	const char *searchRoots[] = {
		getenv("CNC_GENERALS_ZH_PATH"),
		getenv("CNC_GENERALS_PATH"),
		getenv("CNC_ZH_INSTALLPATH"),
		nullptr
	};

	for (Int i = 0; candidates[i].bigFile != nullptr; ++i)
	{
		for (std::size_t rootIndex = 0; rootIndex < sizeof(searchRoots) / sizeof(searchRoots[0]); ++rootIndex)
		{
			if (doesLanguageBigExist(searchRoots[rootIndex], candidates[i].bigFile))
			{
				val = candidates[i].language;
				DEBUG_LOG(("tryAutoDetectLanguage - detected language '%s' from %s (root=%s)",
					candidates[i].language,
					candidates[i].bigFile,
					searchRoots[rootIndex] != nullptr ? searchRoots[rootIndex] : "<cwd>"));
				return TRUE;
			}
		}
	}

	return FALSE;
}

Bool GetStringFromGeneralsRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	if (getEnvVar("CNC_GENERALS_", key, val))
	{
		return TRUE;
	}

	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Generals";
	fullPath.concat(path);
	DEBUG_LOG(("GetStringFromGeneralsRegistry - looking in %s for key %s", fullPath.str(), key.str()));
	if (getStringFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	if (getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	if (key == "InstallPath")
	{
		if (tryRelativeGeneralsPath(val))
		{
			return TRUE;
		}
	}

	return FALSE;
}

Bool GetStringFromRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	if (getEnvVar("CNC_ZH_", key, val))
	{
		return TRUE;
	}

	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";
	fullPath.concat(path);
	DEBUG_LOG(("GetStringFromRegistry - looking in %s for key %s", fullPath.str(), key.str()));
	if (getStringFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	if (getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	if (key == "Language")
	{
		if (tryAutoDetectLanguage(val))
		{
			return TRUE;
		}
	}

	return FALSE;
}

Bool GetUnsignedIntFromRegistry(AsciiString path, AsciiString key, UnsignedInt& val)
{
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";
	fullPath.concat(path);
	DEBUG_LOG(("GetUnsignedIntFromRegistry - looking in %s for key %s", fullPath.str(), key.str()));
	if (getUnsignedIntFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getUnsignedIntFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

#else // _UNIX

Bool  getStringFromRegistry(HKEY root, AsciiString path, AsciiString key, AsciiString& val)
{
	HKEY handle;
	unsigned char buffer[256];
	unsigned long size = 256;
	unsigned long type;
	int returnValue;

	if ((returnValue = RegOpenKeyEx( root, path.str(), 0, KEY_READ, &handle )) == ERROR_SUCCESS)
	{
		returnValue = RegQueryValueEx(handle, key.str(), nullptr, &type, (unsigned char *) &buffer, &size);
		RegCloseKey( handle );
	}

	if (returnValue == ERROR_SUCCESS)
	{
		val = (char *)buffer;
		return TRUE;
	}

	return FALSE;
}

Bool getUnsignedIntFromRegistry(HKEY root, AsciiString path, AsciiString key, UnsignedInt& val)
{
	HKEY handle;
	unsigned char buffer[4];
	unsigned long size = 4;
	unsigned long type;
	int returnValue;

	if ((returnValue = RegOpenKeyEx( root, path.str(), 0, KEY_READ, &handle )) == ERROR_SUCCESS)
	{
		returnValue = RegQueryValueEx(handle, key.str(), nullptr, &type, (unsigned char *) &buffer, &size);
		RegCloseKey( handle );
	}

	if (returnValue == ERROR_SUCCESS)
	{
		val = *(UnsignedInt *)buffer;
		return TRUE;
	}

	return FALSE;
}

Bool setStringInRegistry( HKEY root, AsciiString path, AsciiString key, AsciiString val)
{
	HKEY handle;
	unsigned long type;
	unsigned long returnValue;
	int size;
	char lpClass[] = "REG_NONE";

	if ((returnValue = RegCreateKeyEx( root, path.str(), 0, lpClass, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &handle, nullptr )) == ERROR_SUCCESS)
	{
		type = REG_SZ;
		size = val.getLength()+1;
		returnValue = RegSetValueEx(handle, key.str(), 0, type, (unsigned char *)val.str(), size);
		RegCloseKey( handle );
	}

	return (returnValue == ERROR_SUCCESS);
}

Bool setUnsignedIntInRegistry( HKEY root, AsciiString path, AsciiString key, UnsignedInt val)
{
	HKEY handle;
	unsigned long type;
	unsigned long returnValue;
	int size;
	char lpClass[] = "REG_NONE";

	if ((returnValue = RegCreateKeyEx( root, path.str(), 0, lpClass, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &handle, nullptr )) == ERROR_SUCCESS)
	{
		type = REG_DWORD;
		size = 4;
		returnValue = RegSetValueEx(handle, key.str(), 0, type, (unsigned char *)&val, size);
		RegCloseKey( handle );
	}

	return (returnValue == ERROR_SUCCESS);
}

Bool GetStringFromGeneralsRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Generals";

	fullPath.concat(path);
	DEBUG_LOG(("GetStringFromRegistry - looking in %s for key %s", fullPath.str(), key.str()));
	if (getStringFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

Bool GetStringFromRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
#if RTS_GENERALS
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Generals";
#elif RTS_ZEROHOUR
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";
#endif

	fullPath.concat(path);
	DEBUG_LOG(("GetStringFromRegistry - looking in %s for key %s", fullPath.str(), key.str()));
	if (getStringFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

Bool GetUnsignedIntFromRegistry(AsciiString path, AsciiString key, UnsignedInt& val)
{
#if RTS_GENERALS
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Generals";
#elif RTS_ZEROHOUR
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";
#endif

	fullPath.concat(path);
	DEBUG_LOG(("GetUnsignedIntFromRegistry - looking in %s for key %s", fullPath.str(), key.str()));
	if (getUnsignedIntFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getUnsignedIntFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

#endif // _UNIX

AsciiString GetRegistryLanguage()
{
	static Bool cached = FALSE;
	// NOTE: static causes a memory leak, but we have to keep it because the value is cached.
	static AsciiString val = "english";
	if (cached) {
		return val;
	} else {
		cached = TRUE;
	}

	GetStringFromRegistry("", "Language", val);
	return val;
}

AsciiString GetRegistryGameName()
{
	AsciiString val = "GeneralsMPTest";
	GetStringFromRegistry("", "SKU", val);
	return val;
}

UnsignedInt GetRegistryVersion()
{
	UnsignedInt val = 65536;
	GetUnsignedIntFromRegistry("", "Version", val);
	return val;
}

UnsignedInt GetRegistryMapPackVersion()
{
	UnsignedInt val = 65536;
	GetUnsignedIntFromRegistry("", "MapPackVersion", val);
	return val;
}
