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

// FILE: OpenALAudioCache.h //////////////////////////////////////////////////////////////////////////
// OpenALAudioCache implementation
// Author: Stephan Vedder, March 2025
#pragma once

// #ifndef SAGE_USE_FFMPEG
// #error "SAGE_USE_FFMPEG must be defined to use the OpenALAudioCache."
// #endif

#include "OpenALAudioDevice/OpenALAudioManager.h"
#include "VideoDevice/FFmpeg/FFmpegFile.h"

#include <unordered_map>

struct PlayingAudio
{
	ALuint m_source = 0;
	OpenALAudioStream* m_stream = nullptr;
	FFmpegFile* m_ffmpegFile = nullptr;

	PlayingAudioType m_type;
	AudioEventRTS* m_audioEventRTS;

	// The created OpenAL buffer handle for this file
	ALuint m_bufferHandle = 0;
	// TheSuperHackers @feature bobtista 18/07/2026 A gapless multi-sample loop: buffers are queued
	// and refilled per update (attack -> body loop -> decay) instead of the stop/reload path, so its
	// cleanup is queue-based, not a single m_bufferHandle. Opt-in via GGC_AUDIO_QUEUED_LOOP.
	Bool m_queuedLoop = false;
	Bool m_requestStop;
	Bool m_cleanupAudioEventRTS;
	Int m_framesFaded;

	PlayingAudio() :
		m_type(PAT_INVALID),
		m_audioEventRTS(NULL),
		m_requestStop(false),
		m_cleanupAudioEventRTS(true),
		m_source(0),
		m_framesFaded(0)
	{ }
};

struct OpenAudioFile
{
	ALuint m_buffer = 0;
	FFmpegFile* m_ffmpegFile = NULL;
	UnsignedInt m_openCount = 0;
	UnsignedInt m_fileSize = 0;
	UnsignedInt m_channels = 0;
	UnsignedInt m_bitsPerSample = 0;
	UnsignedInt m_freq = 0;

	// Note: OpenAudioFile does not own this m_eventInfo, and should not delete it.
	const AudioEventInfo* m_eventInfo;	// Not mutable, unlike the one on AudioEventRTS.
	int m_totalSamples = 0;
	float m_duration = 0.0f;
};

struct OpenFileInfo
{
	AsciiString* filename;
	AudioEventRTS* event;

	OpenFileInfo(AsciiString *filename) : filename(filename),
																				event(NULL)
	{
	}

	OpenFileInfo(AudioEventRTS *event) : filename(NULL),
																			 event(event)
	{
	}
};

typedef std::unordered_map< AsciiString, OpenAudioFile, rts::hash<AsciiString>, rts::equal_to<AsciiString> > OpenFilesHash;
typedef OpenFilesHash::iterator OpenFilesHashIt;

// TheSuperHackers @tweak bobtista 05/06/2026 Decoded-PCM cache sizing. 14 MiB is the
// bare ctor default; the 64 MiB floor (applied in OpenALAudioManager) overrides a
// smaller INI/default cap so the decoded working set fits without re-decoding per frame.
static const UnsignedInt DEFAULT_AUDIO_CACHE_BYTES = 14u * 1024u * 1024u;
static const UnsignedInt MIN_DECODED_PCM_CACHE_BYTES = 64u * 1024u * 1024u;

class OpenALAudioFileCache
{
public:
	OpenALAudioFileCache();

	virtual ~OpenALAudioFileCache();
	ALuint getBufferForFile(const OpenFileInfo& fileToOpenFrom);
	void closeBuffer(ALuint bufferToClose);
	void setMaxSize(UnsignedInt size);
	float getBufferLength(ALuint handle);

	// Note: These accessors are unsynchronized snapshots for informational use only and
	// should be treated as a rough estimate.
	UnsignedInt getCurrentlyUsedSize() const { return m_currentlyUsedSize; }
	UnsignedInt getMaxSize() const { return m_maxSize; }

protected:
	void releaseOpenAudioFile(OpenAudioFile* fileToRelease);

	// This function will return TRUE if it was able to free enough space, and FALSE otherwise.
	Bool freeEnoughSpaceForSample(const OpenAudioFile& sampleThatNeedsSpace);

	// FFmpeg related
	Bool decodeFFmpeg(OpenAudioFile* fileToDecode);

	OpenFilesHash m_openFiles;
	UnsignedInt m_currentlyUsedSize;
	UnsignedInt m_maxSize;
};
