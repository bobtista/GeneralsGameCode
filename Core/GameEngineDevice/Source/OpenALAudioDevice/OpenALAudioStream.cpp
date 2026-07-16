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

#include "OpenALAudioDevice/OpenALAudioStream.h"
#include "OpenALAudioDevice/OpenALAudioManager.h"
#include <AL/alext.h>

OpenALAudioStream::OpenALAudioStream()
{
    alGenSources(1, &m_source);
    alGenBuffers(AL_STREAM_BUFFER_COUNT, m_buffers);

    // GeneralsX @bugfix BenderAI 22/04/2026 Force video stream source to non-positional direct playback.
    alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(m_source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f);
    alSourcef(m_source, AL_GAIN, 1.0f);
    alSourcei(m_source, AL_LOOPING, AL_FALSE);
#ifdef AL_DIRECT_CHANNELS_SOFT
    alSourcei(m_source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
#endif
#ifdef AL_SOURCE_SPATIALIZE_SOFT
    alSourcei(m_source, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
#endif

#ifdef INTENSIVE_AUDIO_DEBUG
    DEBUG_LOG(("OpenALAudioStream created: %i\n", m_source));
#endif
}

OpenALAudioStream::~OpenALAudioStream()
{
#ifdef INTENSIVE_AUDIO_DEBUG
    DEBUG_LOG(("OpenALAudioStream freed: %i\n", m_source));
#endif
    // Unbind the buffers first
    alSourceStop(m_source);
    alSourcei(m_source, AL_BUFFER, 0);
    alDeleteSources(1, &m_source);
    // Now delete the buffers
    alDeleteBuffers(AL_STREAM_BUFFER_COUNT, m_buffers);
}

bool OpenALAudioStream::bufferData(uint8_t *data, size_t data_size, ALenum format, int samplerate)
{
#ifdef INTENSIVE_AUDIO_DEBUG
    DEBUG_LOG(("Buffering %zu bytes of data (samplerate: %i, format: %i)\n", data_size, samplerate, format));
#endif
    ALint num_queued;
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
    if (num_queued >= AL_STREAM_BUFFER_COUNT) {
#ifdef INTENSIVE_AUDIO_DEBUG
        DEBUG_LOG(("Having too many buffers already queued: %i", num_queued));
#endif
        return false;
    }

    ALuint &current_buffer = m_buffers[m_current_buffer_idx];
    // GeneralsX @bugfix BenderAI 22/04/2026 Detect and reject invalid OpenAL buffer/queue operations.
    while (alGetError() != AL_NO_ERROR) {}
    alBufferData(current_buffer, format, data, data_size, samplerate);
    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        DEBUG_LOG(("OpenALAudioStream::bufferData alBufferData failed: err=0x%x format=0x%x size=%zu rate=%d\n",
            (unsigned int)err, (unsigned int)format, data_size, samplerate));
        return false;
    }

    alSourceQueueBuffers(m_source, 1, &current_buffer);
    err = alGetError();
    if (err != AL_NO_ERROR) {
        DEBUG_LOG(("OpenALAudioStream::bufferData alSourceQueueBuffers failed: err=0x%x source=%u buffer=%u\n",
            (unsigned int)err, (unsigned int)m_source, (unsigned int)current_buffer));
        return false;
    }

    // TheSuperHackers @bugfix bobtista 16/07/2026 A buffer queued onto a source that is not
    // playing is reported as processed by a stopped source even though it never played; count it
    // so update() can tell such buffers apart from genuinely played ones.
    ALint sourceState;
    alGetSourcei(m_source, AL_SOURCE_STATE, &sourceState);
    if (sourceState != AL_PLAYING && sourceState != AL_PAUSED) {
        m_buffersQueuedWhileNotPlaying++;
    }

    m_current_buffer_idx++;

    if (m_current_buffer_idx >= AL_STREAM_BUFFER_COUNT)
        m_current_buffer_idx = 0;

    return true;
}

void OpenALAudioStream::update()
{
    // TheSuperHackers @bugfix bobtista 13/07/2026 An intentionally stopped stream must stay stopped
    // so processPlayingList releases it; restarting it here undid stopAudio and stopAudioEvent.
    if (m_stopRequested) {
        return;
    }

    ALint sourceState;
    alGetSourcei(m_source, AL_SOURCE_STATE, &sourceState);

    if (sourceState == AL_PLAYING) {
        // Everything still in the queue of a playing source is scheduled to play.
        m_buffersQueuedWhileNotPlaying = 0;
    }

    ALint processedBeforeUnqueue = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processedBeforeUnqueue);
#ifdef INTENSIVE_AUDIO_DEBUG
    DEBUG_LOG(("%i buffers have been processed\n", processedBeforeUnqueue));
#endif

    // GeneralsX @bugfix BenderAI 22/04/2026 Only unqueue processed data in active playback states.
    // TheSuperHackers @bugfix bobtista 16/07/2026 Also unqueue the played buffers of a source that
    // stopped from starvation. A stopped source reports its whole queue as processed, including
    // buffers queued after the stop that never played, so restarting it without dropping the played
    // buffers replayed the audible part from the beginning (the challenge intro speech cutting out
    // and starting over). Drop exactly the buffers that are not known to be queued-but-unplayed,
    // so the restart below resumes with unheard data instead of rewinding.
    ALint processedToUnqueue = 0;
    if (sourceState == AL_PLAYING || sourceState == AL_PAUSED) {
        processedToUnqueue = processedBeforeUnqueue;
    } else if (sourceState == AL_STOPPED) {
        ALint num_queued = 0;
        alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
        ALint unplayed = m_buffersQueuedWhileNotPlaying < num_queued ? m_buffersQueuedWhileNotPlaying : num_queued;
        processedToUnqueue = num_queued - unplayed;
    }
    while (processedToUnqueue > 0) {
        ALuint buffer;
        alSourceUnqueueBuffers(m_source, 1, &buffer);
        processedToUnqueue--;
    }

    ALint num_queued;
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
#ifdef INTENSIVE_AUDIO_DEBUG
    DEBUG_LOG(("Having %i buffers queued\n", num_queued));
#endif

    if (num_queued < AL_STREAM_BUFFER_COUNT / 2 && m_requireDataCallback) {
        // GeneralsX @bugfix BenderAI 22/04/2026 Do not fake queue growth when callback fails to enqueue data.
        // Ask for more data to be buffered.
        // Only fill up to the half, because some formats can output
        // more than one buffer per decoded frame.
        while (num_queued < AL_STREAM_BUFFER_COUNT / 2) {
            m_requireDataCallback();

            ALint refreshedQueued = 0;
            alGetSourcei(m_source, AL_BUFFERS_QUEUED, &refreshedQueued);
            if (refreshedQueued <= num_queued) {
                break;
            }
            num_queued = refreshedQueued;
        }
    }

    // GeneralsX @bugfix fbraz3 27/04/2026 Restart after refill when a generic speech stream
    // began the frame with an empty queue; otherwise processPlayingList() can release it as
    // stopped before the newly buffered narrator audio ever starts playing.
    // TheSuperHackers @bugfix bobtista 16/07/2026 The played buffers of a stopped source were
    // dropped above, so anything still queued is unheard and safe to (re)start; a finished stream
    // drains to an empty queue and stays stopped for release (the machine-gun loop stays fixed).
    alGetSourcei(m_source, AL_SOURCE_STATE, &sourceState);
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
    if ((sourceState == AL_STOPPED || sourceState == AL_INITIAL) && num_queued > 0) {
        play();
    }
}

void OpenALAudioStream::reset()
{
#ifdef INTENSIVE_AUDIO_DEBUG
    DEBUG_LOG(("Resetting stream\n"));
#endif
    // alSourceStop() marks all queued buffers as processed so they can be
    // unqueued. alSourceRewind() transitions to AL_INITIAL but does NOT move
    // unprocessed buffers to processed state, so the subsequent
    // alSourcei(AL_BUFFER, 0) would fail with AL_INVALID_OPERATION if any
    // buffers were still pending.
    alSourceStop(m_source);
    ALint num_queued;
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
    while (num_queued > 0) {
        ALuint buf;
        alSourceUnqueueBuffers(m_source, 1, &buf);
        num_queued--;
    }
    m_current_buffer_idx = 0;
    m_reachedEof = false;
    m_stopRequested = false;
    m_buffersQueuedWhileNotPlaying = 0;
}

bool OpenALAudioStream::isPlaying()
{
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}