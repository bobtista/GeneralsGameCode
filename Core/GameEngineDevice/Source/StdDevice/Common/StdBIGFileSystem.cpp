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

//////// StdBIGFileSystem.h ///////////////////////////
// Stephan Vedder, April 2025
/////////////////////////////////////////////////////////////

#include "Common/AudioAffect.h"
#include "Common/ArchiveFile.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/file.h"
#include "Common/GameAudio.h"
#include "Common/GameMemory.h"
#include "Common/LocalFileSystem.h"

#if RTS_ZEROHOUR
#include "Common/Registry.h"
#endif

#include "StdDevice/Common/StdBIGFile.h"
#include "StdDevice/Common/StdBIGFileSystem.h"
#include "Utility/endian_compat.h"

#include <algorithm>
#include <cstring>
#include <vector>

static const char *BIGFileIdentifier = "BIGF";

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

StdBIGFileSystem::StdBIGFileSystem() : ArchiveFileSystem() {
}

StdBIGFileSystem::~StdBIGFileSystem() {
}

void StdBIGFileSystem::init() {
	DEBUG_ASSERTCRASH(TheLocalFileSystem != nullptr, ("TheLocalFileSystem must be initialized before TheArchiveFileSystem."));
	if (TheLocalFileSystem == nullptr) {
		return;
	}

	loadBigFilesFromDirectory("", "*.big");

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
      loadBigFilesFromDirectory(installPath, "*.big");
#endif
}

void StdBIGFileSystem::reset() {
}

void StdBIGFileSystem::update() {
}

void StdBIGFileSystem::postProcessLoad() {
}

ArchiveFile * StdBIGFileSystem::openArchiveFile(const Char *filename) {
	File *fp = TheLocalFileSystem->openFile(filename, File::READ | File::BINARY);
	AsciiString archiveFileName;
	archiveFileName = filename;
	archiveFileName.toLower();
	Int archiveFileSize = 0;
	Int numLittleFiles = 0;

	DEBUG_LOG(("StdBIGFileSystem::openArchiveFile - opening BIG file %s", filename));

	if (fp == nullptr) {
		DEBUG_CRASH(("Could not open archive file %s for parsing", filename));
		return nullptr;
	}

	AsciiString asciibuf;
	char buffer[_MAX_PATH];
	fp->read(buffer, 4); // read the "BIG" at the beginning of the file.
	buffer[4] = 0;
	if (strcmp(buffer, BIGFileIdentifier) != 0) {
		DEBUG_CRASH(("Error reading BIG file identifier in file %s", filename));
		fp->close();
		fp = nullptr;
		return nullptr;
	}

	// read in the file size.
	fp->read(&archiveFileSize, 4);

	DEBUG_LOG(("StdBIGFileSystem::openArchiveFile - size of archive file is %d bytes", archiveFileSize));

//	char t;

	// read in the number of files contained in this BIG file.
	// change the order of the bytes cause the file size is in reverse byte order for some reason.
	fp->read(&numLittleFiles, 4);
	numLittleFiles = betoh(numLittleFiles);

	DEBUG_LOG(("StdBIGFileSystem::openArchiveFile - %d are contained in archive", numLittleFiles));
//	for (Int i = 0; i < 2; ++i) {
//		t = buffer[i];
//		buffer[i] = buffer[(4-i)-1];
//		buffer[(4-i)-1] = t;
//	}

	// seek to the beginning of the directory listing.
	fp->seek(0x10, File::START);
	// read in each directory listing.
	ArchivedFileInfo *fileInfo = NEW ArchivedFileInfo;
	// TheSuperHackers @fix bobtista 12/06/2026 Allocate the archive file only after the early-return
	// validity checks above (mirrors the Win32BIGFileSystem fix) so a missing/invalid BIG doesn't leak it.
	ArchiveFile *archiveFile = NEW StdBIGFile(filename, AsciiString::TheEmptyString);

	for (Int i = 0; i < numLittleFiles; ++i) {
		Int filesize = 0;
		Int fileOffset = 0;
		fp->read(&fileOffset, 4);
		fp->read(&filesize, 4);

		filesize = betoh(filesize);
		fileOffset = betoh(fileOffset);

		fileInfo->m_archiveFilename = archiveFileName;
		fileInfo->m_offset = fileOffset;
		fileInfo->m_size = filesize;

		// read in the path name of the file.
		Int pathIndex = -1;
		do {
			++pathIndex;
			fp->read(buffer + pathIndex, 1);
		} while (buffer[pathIndex] != 0);

		Int filenameIndex = pathIndex;
		while ((filenameIndex >= 0) && (buffer[filenameIndex] != '\\') && (buffer[filenameIndex] != '/')) {
			--filenameIndex;
		}

		fileInfo->m_filename = (char *)(buffer + filenameIndex + 1);
		fileInfo->m_filename.toLower();
		buffer[filenameIndex + 1] = 0;

		AsciiString path;
		path = buffer;

		AsciiString debugpath;
		debugpath = path;
		debugpath.concat(fileInfo->m_filename);
//		DEBUG_LOG(("StdBIGFileSystem::openArchiveFile - adding file %s to archive file %s, file number %d", debugpath.str(), fileInfo->m_archiveFilename.str(), i));

		archiveFile->addFile(path, fileInfo);
	}

	archiveFile->attachFile(fp);

	delete fileInfo;
	fileInfo = nullptr;

	// leave fp open as the archive file will be using it.

	return archiveFile;
}

void StdBIGFileSystem::closeArchiveFile(const Char *filename) {
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

void StdBIGFileSystem::closeAllArchiveFiles() {
}

void StdBIGFileSystem::closeAllFiles() {
}

Bool StdBIGFileSystem::loadBigFilesFromDirectory(AsciiString dir, AsciiString fileMask, Bool overwrite) {

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
			DEBUG_LOG(("StdBIGFileSystem::loadBigFilesFromDirectory - loading %s into the directory tree.", (*it).str()));
			loadIntoDirectoryTree(archiveFile, overwrite);
			m_archiveFileMap[(*it)] = archiveFile;
			DEBUG_LOG(("StdBIGFileSystem::loadBigFilesFromDirectory - %s inserted into the archive file map.", (*it).str()));
			actuallyAdded = TRUE;
		}

		it++;
	}

	return actuallyAdded;
}
