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

// FILE: OpenALAudioStream.h //////////////////////////////////////////////////////////////////////////
// OpenALAudioStream implementation
// Author: Stephan Vedder, March 2025
#pragma once

#include "always.h"
#include <AL/al.h>
#include <stdint.h>
#include <functional>

#define AL_STREAM_BUFFER_COUNT 32

class OpenALAudioStream final
{
public:
    OpenALAudioStream();
    ~OpenALAudioStream();

    void setRequireDataCallback(std::function<void()> callback) { m_requireDataCallback = callback; }
    ALuint getSource() const { return m_source; }

    bool bufferData(uint8_t *data, size_t data_size, ALenum format, int samplerate);
    bool isPlaying();
    void update();
    void reset();

    // TheSuperHackers @bugfix bobtista 05/06/2026 Marked once the decoder reaches end-of-file so
    // update() stops restarting a finished stream (which otherwise replays its last buffer forever).
    void markEndOfStream() { m_reachedEof = true; }
    bool reachedEndOfStream() const { return m_reachedEof; }

    void play() { m_stopRequested = false; alSourcePlay(m_source); }
    void pause() { alSourcePause(m_source); }
    // TheSuperHackers @bugfix bobtista 13/07/2026 Remember an intentional stop so update() does not
    // treat the stopped source as an underrun and restart it; the drained stream is then released
    // by processPlayingList, matching the Miles behavior of parking an explicitly stopped stream.
    void stop() { m_stopRequested = true; alSourceStop(m_source); }
    bool isStopRequested() const { return m_stopRequested; }

    void setVolume(float vol) { alSourcef(m_source, AL_GAIN, vol); }

protected:
    std::function<void()> m_requireDataCallback = nullptr;
    ALuint m_source = 0;
    ALuint m_buffers[AL_STREAM_BUFFER_COUNT] = {};
    unsigned int m_current_buffer_idx = 0;
    bool m_reachedEof = false;
    bool m_stopRequested = false;
};