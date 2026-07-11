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

//////// BIGFileSystem.cpp ///////////////////////////
// Bryan Cleveland, August 2002
/////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "Common/BIGFileSystem.h"

#include "Common/AudioAffect.h"
#include "Common/ArchiveFile.h"
#include "Common/file.h"
#include "Common/GameAudio.h"
#include "Common/GameMemory.h"
#include "Common/LocalFileSystem.h"

#if RTS_ZEROHOUR
#include "Common/Registry.h"
#endif

#include "Utility/endian_compat.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static const char BIGFileIdentifier[] = { 'B', 'I', 'G', 'F' };

// Zero Hour can run with both Generals and ZH archives in one directory.
// Load the higher-priority archives first because ArchiveFileSystem keeps the
// first copy of a path when overwrite is false.
static AsciiString GetBigFilename(AsciiString filename)
{
	const char *path = filename.str();
	const char *slash = strrchr(path, '\\');
	const char *forwardSlash = strrchr(path, '/');
	if (forwardSlash != nullptr && (slash == nullptr || forwardSlash > slash))
	{
		slash = forwardSlash;
	}

	AsciiString result = slash != nullptr ? slash + 1 : path;
	result.toLower();
	return result;
}

static Int GetBIGLoadPriority(AsciiString filename)
{
	AsciiString baseName = GetBigFilename(filename);

	if (baseName.compareNoCase("patchzh.big") == 0)
	{
		return 10;
	}
	if (baseName.endsWithNoCase("zh.big"))
	{
		return 20;
	}
	if (baseName.compareNoCase("patch.big") == 0
		|| baseName.compareNoCase("patchdata.big") == 0
		|| baseName.compareNoCase("patchini.big") == 0)
	{
		return 30;
	}
	if (baseName.compareNoCase("audio.big") == 0
		|| baseName.compareNoCase("audioenglish.big") == 0
		|| baseName.compareNoCase("english.big") == 0
		|| baseName.compareNoCase("gensec.big") == 0
		|| baseName.compareNoCase("ini.big") == 0
		|| baseName.compareNoCase("maps.big") == 0
		|| baseName.compareNoCase("music.big") == 0
		|| baseName.compareNoCase("shaders.big") == 0
		|| baseName.compareNoCase("speech.big") == 0
		|| baseName.compareNoCase("speechenglish.big") == 0
		|| baseName.compareNoCase("terrain.big") == 0
		|| baseName.compareNoCase("textures.big") == 0
		|| baseName.compareNoCase("w3d.big") == 0
		|| baseName.compareNoCase("window.big") == 0)
	{
		return 40;
	}

	return 0;
}

struct BIGFileHeader {
	char identifier[sizeof(BIGFileIdentifier)];
	UnsignedInt archiveFileSize; // little endian, retail reads it as Int so 2 GB max
	UnsignedInt numFiles; // big endian
	UnsignedInt firstEntryOffset; // big endian
};

struct BIGFileEntryHeader {
	UnsignedInt fileOffset; // big endian
	UnsignedInt fileSize; // big endian
};

static_assert(sizeof(BIGFileHeader) == 16, "Incorrect BIG header size");
static_assert(sizeof(BIGFileEntryHeader) == 8, "Incorrect BIG entry header size");

// Returns the length of the string, or -1 if it is truncated or does not fit the buffer.
static Int readNullTerminatedString(File *fp, char *buffer, Int bufferSize) {
	for (Int i = 0; i < bufferSize; ++i) {
		if (fp->read(buffer + i, 1) != 1) {
			return -1;
		}
		if (buffer[i] == 0) {
			return i;
		}
	}
	return -1;
}

BIGFileSystem::BIGFileSystem() : ArchiveFileSystem() {
}

BIGFileSystem::~BIGFileSystem() {
}

void BIGFileSystem::init() {
	DEBUG_ASSERTCRASH(TheLocalFileSystem != nullptr, ("TheLocalFileSystem must be initialized before TheArchiveFileSystem."));
	if (TheLocalFileSystem == nullptr) {
		return;
	}

	loadArchiveFilesFromDirectory("", "*.big");

#if RTS_ZEROHOUR
#if defined(_UNIX)
    // TheSuperHackers @feature bobtista 09/06/2026 On non-Windows the engine binary often
    // lives apart from the game data, so honor $CNC_ZH_INSTALLPATH (resolved through
    // GetStringFromRegistry) as an additional Zero Hour archive root alongside the working
    // directory, mirroring the base Generals load below. Lets a build elsewhere point at an
    // existing install without copying the .big files next to the executable.
    AsciiString zhInstallPath;
    GetStringFromRegistry("", "InstallPath", zhInstallPath );
    if (!zhInstallPath.isEmpty())
    {
      loadBigFilesFromDirectory(zhInstallPath, "*.big");
    }
#endif

    // load original Generals assets
    AsciiString installPath;
    GetStringFromGeneralsRegistry("", "InstallPath", installPath );
    //@todo this will need to be ramped up to a crash for release
    DEBUG_ASSERTCRASH(!installPath.isEmpty(), ("Be 1337! Go install Generals!"));
    if (!installPath.isEmpty())
      loadArchiveFilesFromDirectory(installPath, "*.big");
#endif
}

void BIGFileSystem::reset() {
}

void BIGFileSystem::update() {
}

void BIGFileSystem::postProcessLoad() {
}

ArchiveFile * BIGFileSystem::openArchiveFile(const Char *filename) {
	// TheSuperHackers @bugfix bobtista 11/07/2026 Retry transient archive-open
	// failures. Archives mount exactly once at startup and a failed mount was
	// silently skipped in release builds (the crash above is debug-only),
	// leaving every asset in that archive unavailable for the whole run —
	// observed on Windows as save-restored objects loading with no visible
	// models when the game launches during heavy disk activity (antivirus
	// scan or a previous process still tearing down holds the file briefly).
	File *fp = nullptr;
	for (Int attempt = 0; attempt < 10; ++attempt) {
		fp = TheLocalFileSystem->openFile(filename, File::READ | File::BINARY);
		if (fp != nullptr) {
			break;
		}
		std::fprintf(stderr, "[ggc] archive open failed (attempt %d/10): %s\n", attempt + 1, filename);
		std::fflush(stderr);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	AsciiString archiveFileName;
	archiveFileName = filename;
	archiveFileName.toLower();
	UnsignedInt archiveFileSize = 0;
	UnsignedInt numLittleFiles = 0;
	BIGFileHeader header;

	DEBUG_LOG(("BIGFileSystem::openArchiveFile - opening BIG file %s", filename));

	if (fp == nullptr) {
		DEBUG_CRASH(("Could not open archive file %s for parsing", filename));
		return nullptr;
	}

	AsciiString asciibuf;
	char buffer[_MAX_PATH];
	if (fp->read(&header, sizeof(header)) != sizeof(header) || memcmp(header.identifier, BIGFileIdentifier, sizeof(BIGFileIdentifier)) != 0) {
		DEBUG_CRASH(("Error reading BIG file header in file %s", filename));
		fp->close();
		fp = nullptr;
		return nullptr;
	}

	archiveFileSize = letoh(header.archiveFileSize);
	numLittleFiles = betoh(header.numFiles);

	DEBUG_LOG(("BIGFileSystem::openArchiveFile - size of archive file is %u bytes", archiveFileSize));

//	char t;

	DEBUG_LOG(("BIGFileSystem::openArchiveFile - %u are contained in archive", numLittleFiles));
//	for (Int i = 0; i < 2; ++i) {
//		t = buffer[i];
//		buffer[i] = buffer[(4-i)-1];
//		buffer[(4-i)-1] = t;
//	}

	// read in each directory listing.
	ArchivedFileInfo fileInfo;
	ArchiveFile *archiveFile = NEW ArchiveFile(filename, AsciiString::TheEmptyString);

	Bool corrupt = FALSE;
	for (UnsignedInt i = 0; i < numLittleFiles; ++i) {
		BIGFileEntryHeader entry;
		if (fp->read(&entry, sizeof(entry)) != sizeof(entry)) {
			corrupt = TRUE;
			break;
		}

		fileInfo.m_archiveFilename = archiveFileName;
		fileInfo.m_offset = betoh(entry.fileOffset);
		fileInfo.m_size = betoh(entry.fileSize);

		// read in the path name of the file.
		const Int pathIndex = readNullTerminatedString(fp, buffer, sizeof(buffer));
		if (pathIndex < 0) {
			corrupt = TRUE;
			break;
		}

		char *leafName = getFileName(buffer);

		fileInfo.m_filename = leafName;
		fileInfo.m_filename.toLower();
		*leafName = 0;

		AsciiString path;
		path = buffer;

		AsciiString debugpath;
		debugpath = path;
		debugpath.concat(fileInfo.m_filename);
//		DEBUG_LOG(("BIGFileSystem::openArchiveFile - adding file %s to archive file %s, file number %d", debugpath.str(), fileInfo.m_archiveFilename.str(), i));

		archiveFile->addFile(path, &fileInfo);
	}

	if (corrupt) {
		DEBUG_CRASH(("Truncated or corrupt directory in BIG file %s", filename));
		delete archiveFile;
		fp->close();
		fp = nullptr;
		return nullptr;
	}

	archiveFile->attachFile(fp);

	// leave fp open as the archive file will be using it.

	return archiveFile;
}

void BIGFileSystem::closeArchiveFile(const Char *filename) {
	// Need to close the specified big file
	ArchiveFileMap::iterator it =  m_archiveFileMap.find(filename);
	if (it == m_archiveFileMap.end()) {
		return;
	}

	if (stricmp(filename, MUSIC_BIG) == 0) {
		// Stop the current audio
		TheAudio->stopAudio(AudioAffect_Music);

		// No need to turn off other audio, as the lookups will just fail.
	}
	DEBUG_ASSERTCRASH(stricmp(filename, MUSIC_BIG) == 0, ("Attempting to close Archive file '%s', need to add code to handle its shutdown correctly.", filename));

	// may need to do some other processing here first.

	delete (it->second);
	m_archiveFileMap.erase(it);
}

void BIGFileSystem::closeAllArchiveFiles() {
}

void BIGFileSystem::closeAllFiles() {
}

Bool BIGFileSystem::loadArchiveFilesFromDirectory(AsciiString dir, AsciiString fileMask, Bool overwrite) {

	FilenameList filenameList;
	TheLocalFileSystem->getFileListInDirectory(dir, "", fileMask, filenameList, TRUE);
	std::vector<AsciiString> sortedFiles(filenameList.begin(), filenameList.end());
	std::sort(sortedFiles.begin(), sortedFiles.end(), [](const AsciiString& a, const AsciiString& b) {
		Int priorityA = GetBIGLoadPriority(a);
		Int priorityB = GetBIGLoadPriority(b);
		if (priorityA != priorityB)
		{
			return priorityA < priorityB;
		}
		return a.compareNoCase(b) < 0;
	});

	Bool actuallyAdded = FALSE;
	std::vector<AsciiString>::iterator it = sortedFiles.begin();
	while (it != sortedFiles.end()) {
#if RTS_ZEROHOUR
		// TheSuperHackers @bugfix bobtista 18/11/2025 Skip duplicate INIZH.big in Data\INI to prevent CRC mismatches.
		// English, Chinese, and Korean SKUs shipped with two INIZH.big files (one in Run directory, one in Run\Data\INI).
		// The DeleteFile cleanup doesn't work on EA App/Origin installs because the folder is not writable, so we skip loading it instead.
		if (it->endsWithNoCase("Data\\INI\\INIZH.big") || it->endsWithNoCase("Data/INI/INIZH.big")) {
			it++;
			continue;
		}
#endif

		ArchiveFile *archiveFile = openArchiveFile((*it).str());

		if (archiveFile != nullptr) {
			DEBUG_LOG(("BIGFileSystem::loadArchiveFilesFromDirectory - loading %s into the directory tree.", (*it).str()));
			loadIntoDirectoryTree(archiveFile, overwrite);
			m_archiveFileMap[(*it)] = archiveFile;
			DEBUG_LOG(("BIGFileSystem::loadArchiveFilesFromDirectory - %s inserted into the archive file map.", (*it).str()));
			actuallyAdded = TRUE;
		}
		else {
			// TheSuperHackers @bugfix bobtista 11/07/2026 A failed mount used to be
			// skipped silently in release builds; every asset in the archive then
			// stayed unavailable for the whole run. Say so where a release build
			// can see it.
			std::fprintf(stderr, "[ggc] ARCHIVE MOUNT FAILED, contents unavailable this run: %s\n", (*it).str());
			std::fflush(stderr);
		}

		it++;
	}

	return actuallyAdded;
}

void BIGFileSystem::loadMods()
{
	if (TheGlobalData->m_modBIG.isNotEmpty())
	{
		ArchiveFile *archiveFile = openArchiveFile(TheGlobalData->m_modBIG.str());

		if (archiveFile != nullptr) {
			DEBUG_LOG(("BIGFileSystem::loadMods - loading %s into the directory tree.", TheGlobalData->m_modBIG.str()));
			loadIntoDirectoryTree(archiveFile, TRUE);
			m_archiveFileMap[TheGlobalData->m_modBIG] = archiveFile;
			DEBUG_LOG(("BIGFileSystem::loadMods - %s inserted into the archive file map.", TheGlobalData->m_modBIG.str()));
		}
		else
		{
			DEBUG_LOG(("BIGFileSystem::loadMods - could not openArchiveFile(%s)", TheGlobalData->m_modBIG.str()));
		}
	}

	if (TheGlobalData->m_modDir.isNotEmpty())
	{
		MAYBE_UNUSED Bool ret = loadArchiveFilesFromDirectory(TheGlobalData->m_modDir, "*.big", TRUE);
		(void)ret;
		DEBUG_ASSERTLOG(ret, ("loadArchiveFilesFromDirectory(%s) returned FALSE!", TheGlobalData->m_modDir.str()));
	}
}
