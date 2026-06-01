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

///////// StdLocalFileSystem.cpp /////////////////////////
// Stephan Vedder, April 2025
////////////////////////////////////////////////////////////

#include "Common/AsciiString.h"
#include "Common/GameMemory.h"
#include "Common/PerfTimer.h"
#include "StdDevice/Common/StdLocalFileSystem.h"
#include "StdDevice/Common/StdLocalFile.h"

#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// TheSuperHackers @fix bobtista 30/06/2026 Convert between narrow strings and
// std::filesystem::path without throwing. MSVC's narrow path constructor and
// path::string() convert against the system ANSI codepage with the
// *_ERR_INVALID_CHARS flag set, which throws std::system_error
// (ERROR_NO_UNICODE_TRANSLATION) on machines whose codepage cannot map a byte
// (e.g. the "Use Unicode UTF-8" beta or a DBCS system locale). The unhandled
// throw terminates the game at startup. Convert here without that flag so
// unmappable bytes are substituted instead of crashing.
namespace
{
#ifdef _WIN32
	std::filesystem::path narrowToPath(const std::string &str)
	{
		if (str.empty())
		{
			return std::filesystem::path();
		}
		int wlen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), nullptr, 0);
		std::wstring wide(wlen, L'\0');
		if (wlen > 0)
		{
			MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), &wide[0], wlen);
		}
		return std::filesystem::path(std::move(wide));
	}

	std::string pathToString(const std::filesystem::path &path)
	{
		const std::wstring &wide = path.native();
		if (wide.empty())
		{
			return std::string();
		}
		int len = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
		std::string narrow(len, '\0');
		if (len > 0)
		{
			WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), &narrow[0], len, nullptr, nullptr);
		}
		return narrow;
	}
#else
	inline std::filesystem::path narrowToPath(const std::string &str)
	{
		return std::filesystem::path(str);
	}

	inline std::string pathToString(const std::filesystem::path &path)
	{
		return path.string();
	}
#endif
}

//DECLARE_PERF_TIMER(StdLocalFileSystem_openFile)
StdLocalFileSystem::StdLocalFileSystem() = default;

StdLocalFileSystem::~StdLocalFileSystem() = default;

static std::filesystem::path fixFilenameFromWindowsPath(const Char *filename, Int access)
{
	std::string fixedFilename(filename);

#ifndef _WIN32
	// Replace backslashes with forward slashes on unix
	std::replace(fixedFilename.begin(), fixedFilename.end(), '\\', '/');
#endif

	// Convert the filename to a std::filesystem::path and pass that
	std::filesystem::path path = narrowToPath(fixedFilename);

#ifndef _WIN32
	// check if the file exists to see if fixup is required
	// if it's not found try to match disregarding case sensitivity
	// For cases where a write is happening, we should check if the parent path exists, if so, let it through, since the file may not exist yet.
	std::error_code ec;
	if (!std::filesystem::exists(path, ec) &&
		((!(access & File::WRITE)) || ((access & File::WRITE) && !std::filesystem::exists(path.parent_path(), ec))))
	{
		// Traverse path to try and match case-insensitively
		std::filesystem::path parent = path.parent_path();

		std::filesystem::path pathFixed;
		std::filesystem::path pathCurrent;
		// TheSuperHackers @build bobtista 29/04/2026 std::filesystem::path's
		// iterator dereferences to a temporary on Apple Clang's libc++; use
		// auto-by-value (or const ref) instead of non-const ref.
		for (auto p : path)
		{
			std::filesystem::path pathFixedPart;

			if (std::filesystem::exists(pathCurrent / p, ec))
			{
				pathFixedPart = p;
			}
			else if (std::filesystem::exists(pathFixed / p, ec))
			{
				pathFixedPart = p;
			}
			else
			{
				// Check if the subpath exists using case-insensitive comparison.
				// TheSuperHackers @bugfix bobtista 29/07/2026 Scan the working
				// directory for the leading component of a relative path. It used to
				// be copied through verbatim, so on a case-sensitive filesystem a
				// literal such as "data\Scripts\SkirmishScripts.scb" never matched an
				// on-disk "Data" and the open failed, leaving skirmish games with no
				// scripts and no teams on Linux. Windows and macOS hid the bug behind
				// a case-insensitive filesystem.
				const std::filesystem::path scanDir = pathFixed.empty() ? std::filesystem::path(".") : pathFixed;
				for (auto& entry : std::filesystem::directory_iterator(scanDir, ec))
				{
					if (strcasecmp(entry.path().filename().string().c_str(), p.string().c_str()) == 0)
					{
						pathFixedPart = entry.path().filename();
						break;
					}
				}
			}

			if (pathFixedPart.empty())
			{
				// Required to allow creation of new files
				if (!(access & File::WRITE))
				{
					return std::filesystem::path();
				}

				// Use the last known good path
				pathFixed = p;
			}

			// Copy of the current path to mirror the current depth
			pathFixed /= pathFixedPart;
			pathCurrent /= p;
		}
		path = pathFixed;
	}
#endif

	return path;
}

File * StdLocalFileSystem::openFile(const Char *filename, Int access, size_t bufferSize)
{
	//USE_PERF_TIMER(StdLocalFileSystem_openFile)

	// sanity check
	if (strlen(filename) <= 0) {
		return nullptr;
	}

	std::filesystem::path path = fixFilenameFromWindowsPath(filename, access);

	if (path.empty()) {
		return nullptr;
	}

	if (access & File::WRITE) {
		// if opening the file for writing, we need to make sure the directory is there
		// before we try to create the file.
		std::filesystem::path dir = path.parent_path();
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec) || ec) {
			if(!std::filesystem::create_directories(dir, ec) || ec) {
				DEBUG_LOG(("StdLocalFileSystem::openFile - Error creating directory %s", pathToString(dir).c_str()));
				return nullptr;
			}
		}
	}

	StdLocalFile *file = newInstance( StdLocalFile );

	if (file->open(pathToString(path).c_str(), access, bufferSize) == FALSE) {
		deleteInstance(file);
		file = nullptr;
	} else {
		file->deleteOnClose();
	}

// this will also need to play nice with the STREAMING type that I added, if we ever enable this

// srj sez: this speeds up INI loading, but makes BIG files unusable.
// don't enable it without further tweaking.
//
// unless you like running really slowly.
//	if (!(access&File::WRITE)) {
//		// Return a ramfile.
//		RAMFile *ramFile = newInstance( RAMFile );
//		if (ramFile->open(file)) {
//			file->close(); // is deleteonclose, so should delete.
//			ramFile->deleteOnClose();
//			return ramFile;
//		}	else {
//			ramFile->close();
//			deleteInstance(ramFile);
//		}
//	}

	return file;
}

void StdLocalFileSystem::update()
{
}

void StdLocalFileSystem::init()
{
}

void StdLocalFileSystem::reset()
{
}

//DECLARE_PERF_TIMER(StdLocalFileSystem_doesFileExist)
Bool StdLocalFileSystem::doesFileExist(const Char *filename) const
{
	std::filesystem::path path = fixFilenameFromWindowsPath(filename, 0);
	if(path.empty()) {
		return FALSE;
	}

	std::error_code ec;
	return std::filesystem::exists(path, ec);
}

void StdLocalFileSystem::getFileListInDirectory(const AsciiString& currentDirectory, const AsciiString& originalDirectory, const AsciiString& searchName, FilenameList & filenameList, Bool searchSubdirectories) const
{

	AsciiString asciisearch;
	asciisearch = originalDirectory;
	asciisearch.concat(currentDirectory);
	auto searchExt = narrowToPath(searchName.str()).extension();
	if (asciisearch.isEmpty()) {
		asciisearch = ".";
	}

	std::string fixedDirectory(asciisearch.str());

#ifndef _WIN32
	// Replace backslashes with forward slashes on unix
	std::replace(fixedDirectory.begin(), fixedDirectory.end(), '\\', '/');
#endif

	Bool done = FALSE;
	std::error_code ec;

	auto iter = std::filesystem::directory_iterator(narrowToPath(fixedDirectory), ec);
	// The default iterator constructor creates an end iterator
	done = iter == std::filesystem::directory_iterator();

	if (ec) {
		// TheSuperHackers @tweak bobtista 08/07/2026 The engine probes many optional
		// override directories that rarely exist (Data/INI subfolders), so an absent
		// directory is routine; only log real I/O errors.
		if (ec != std::errc::no_such_file_or_directory && ec != std::errc::not_a_directory) {
			DEBUG_LOG(("StdLocalFileSystem::getFileListInDirectory - Error opening directory %s (%s)", fixedDirectory.c_str(), ec.message().c_str()));
		}
		return;
	}

	while (!done)	{
		std::string filenameStr = pathToString(iter->path().filename());
		if (!iter->is_directory() && iter->path().extension() == searchExt &&
			(strcmp(filenameStr.c_str(), ".") != 0 && strcmp(filenameStr.c_str(), "..") != 0)) {
			// if we haven't already, add this filename to the list.
			// a stl set should only allow one copy of each filename
			AsciiString newFilename;
			newFilename = originalDirectory;
			newFilename.concat(currentDirectory);
			newFilename.concat(filenameStr.c_str());
			if (filenameList.find(newFilename) == filenameList.end()) {
				filenameList.insert(newFilename);
			}
		}

		iter++;
		done = iter == std::filesystem::directory_iterator();
	}

	if (searchSubdirectories) {
		auto iter = std::filesystem::directory_iterator(narrowToPath(fixedDirectory), ec);

		if (ec) {
			DEBUG_LOG(("StdLocalFileSystem::getFileListInDirectory - Error opening subdirectory %s", fixedDirectory.c_str()));
			return;
		}

		// The default iterator constructor creates an end iterator
		done = iter == std::filesystem::directory_iterator();

		while (!done) {
			std::string filenameStr = pathToString(iter->path().filename());
			if(iter->is_directory() &&
				(strcmp(filenameStr.c_str(), ".") != 0 && strcmp(filenameStr.c_str(), "..") != 0)) {
				AsciiString tempsearchstr;
				tempsearchstr.concat(currentDirectory);
				tempsearchstr.concat(filenameStr.c_str());
				tempsearchstr.concat('\\');

				// recursively add files in subdirectories if required.
				getFileListInDirectory(tempsearchstr, originalDirectory, searchName, filenameList, searchSubdirectories);
			}

			iter++;
			done = iter == std::filesystem::directory_iterator();
		}
	}
}

Bool StdLocalFileSystem::getFileInfo(const AsciiString& filename, FileInfo *fileInfo) const
{
	std::filesystem::path path = fixFilenameFromWindowsPath(filename.str(), 0);

	if(path.empty()) {
		return FALSE;
	}

	std::error_code ec;
	auto file_size = std::filesystem::file_size(path, ec);
	if (ec)
	{
		return FALSE;
	}

	auto write_time = std::filesystem::last_write_time(path, ec);
	if (ec)
	{
		return FALSE;
	}

	// TODO: fix this to be win compatible (time since 1601)
	auto time = write_time.time_since_epoch().count();
	fileInfo->timestampHigh = time >> 32;
	fileInfo->timestampLow = time & UINT32_MAX;
	fileInfo->sizeHigh      = file_size >> 32;
	fileInfo->sizeLow  = file_size & UINT32_MAX;

	return TRUE;
}

Bool StdLocalFileSystem::createDirectory(AsciiString directory)
{
	bool result = FALSE;

	std::string fixedDirectory(directory.str());

#ifndef _WIN32
	// Replace backslashes with forward slashes on unix
	std::replace(fixedDirectory.begin(), fixedDirectory.end(), '\\', '/');
#endif

	if ((!fixedDirectory.empty()) && (fixedDirectory.length() < _MAX_DIR)) {
		// Convert to host path
		std::filesystem::path path = narrowToPath(fixedDirectory);

		std::error_code ec;
		result = std::filesystem::create_directory(path, ec);
		if (ec) {
			result = FALSE;
		}
	}
	return result;
}

AsciiString StdLocalFileSystem::normalizePath(const AsciiString& filePath) const
{
	std::string nonNormalized(filePath.str());
#ifndef _WIN32
	// Replace backslashes with forward slashes on non-Windows platforms
	std::replace(nonNormalized.begin(), nonNormalized.end(), '\\', '/');
#endif
	std::filesystem::path pathNonNormalized = narrowToPath(nonNormalized);
	return AsciiString(pathToString(pathNonNormalized.lexically_normal()).c_str());
}
