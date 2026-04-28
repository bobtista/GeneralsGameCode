/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "OpenALAudioDevice/OpenALAudioManager.h"

#if defined(SAGE_USE_OPENAL)

#include "Common/AudioHandleSpecialValues.h"

#include <AL/al.h>
#include <AL/alc.h>

OpenALAudioManager::OpenALAudioManager() :
	m_device(NULL),
	m_context(NULL),
	m_selectedProvider(0),
	m_speakerType(0),
	m_musicTrackName(AsciiString::TheEmptyString)
{
}

OpenALAudioManager::~OpenALAudioManager()
{
	closeDevice();
	TheAudio = NULL;
}

#if defined(RTS_DEBUG)
void OpenALAudioManager::audioDebugDisplay(DebugDisplayInterface *, void *, FILE *)
{
}
#endif

void OpenALAudioManager::init()
{
	openDevice();
	AudioManager::init();
}

void OpenALAudioManager::reset()
{
	AudioManager::reset();
}

void OpenALAudioManager::update()
{
	AudioManager::update();
}

void OpenALAudioManager::stopAudio(AudioAffect)
{
	alSourceStop(0);
}

void OpenALAudioManager::pauseAudio(AudioAffect)
{
}

void OpenALAudioManager::resumeAudio(AudioAffect)
{
}

void OpenALAudioManager::pauseAmbient(Bool)
{
}

void OpenALAudioManager::killAudioEventImmediately(AudioHandle)
{
}

void OpenALAudioManager::nextMusicTrack()
{
	m_musicTrackName = nextTrackName(m_musicTrackName);
}

void OpenALAudioManager::prevMusicTrack()
{
	m_musicTrackName = prevTrackName(m_musicTrackName);
}

Bool OpenALAudioManager::isMusicPlaying() const
{
	return false;
}

Bool OpenALAudioManager::hasMusicTrackCompleted(const AsciiString&, Int) const
{
	return true;
}

AsciiString OpenALAudioManager::getMusicTrackName() const
{
	return m_musicTrackName;
}

void OpenALAudioManager::openDevice()
{
	if (m_device != NULL)
	{
		return;
	}

	m_device = alcOpenDevice(NULL);
	if (m_device != NULL)
	{
		m_context = alcCreateContext(m_device, NULL);
		if (m_context != NULL)
		{
			alcMakeContextCurrent(m_context);
		}
	}
}

void OpenALAudioManager::closeDevice()
{
	if (m_context != NULL)
	{
		alcMakeContextCurrent(NULL);
		alcDestroyContext(m_context);
		m_context = NULL;
	}
	if (m_device != NULL)
	{
		alcCloseDevice(m_device);
		m_device = NULL;
	}
}

void *OpenALAudioManager::getDevice()
{
	return m_device;
}

void OpenALAudioManager::notifyOfAudioCompletion(UnsignedInt, UnsignedInt)
{
}

UnsignedInt OpenALAudioManager::getProviderCount() const
{
	return 1;
}

AsciiString OpenALAudioManager::getProviderName(UnsignedInt providerNum) const
{
	if (providerNum == 0)
	{
		return AsciiString("OpenAL Soft");
	}
	return AsciiString::TheEmptyString;
}

UnsignedInt OpenALAudioManager::getProviderIndex(AsciiString providerName) const
{
	if (providerName.compareNoCase("OpenAL Soft") == 0)
	{
		return 0;
	}
	return PROVIDER_ERROR;
}

void OpenALAudioManager::selectProvider(UnsignedInt providerNdx)
{
	m_selectedProvider = providerNdx;
}

void OpenALAudioManager::unselectProvider()
{
	m_selectedProvider = PROVIDER_ERROR;
}

UnsignedInt OpenALAudioManager::getSelectedProvider() const
{
	return m_selectedProvider;
}

void OpenALAudioManager::setSpeakerType(UnsignedInt speakerType)
{
	m_speakerType = speakerType;
}

UnsignedInt OpenALAudioManager::getSpeakerType()
{
	return m_speakerType;
}

UnsignedInt OpenALAudioManager::getNum2DSamples() const
{
	return 0;
}

UnsignedInt OpenALAudioManager::getNum3DSamples() const
{
	return 0;
}

UnsignedInt OpenALAudioManager::getNumStreams() const
{
	return 0;
}

Bool OpenALAudioManager::doesViolateLimit(AudioEventRTS *) const
{
	return false;
}

Bool OpenALAudioManager::isPlayingLowerPriority(AudioEventRTS *) const
{
	return false;
}

Bool OpenALAudioManager::isPlayingAlready(AudioEventRTS *) const
{
	return false;
}

Bool OpenALAudioManager::isObjectPlayingVoice(UnsignedInt) const
{
	return false;
}

void OpenALAudioManager::adjustVolumeOfPlayingAudio(AsciiString, Real)
{
}

void OpenALAudioManager::removePlayingAudio(AsciiString)
{
}

void OpenALAudioManager::removeAllDisabledAudio()
{
}

Bool OpenALAudioManager::has3DSensitiveStreamsPlaying() const
{
	return false;
}

void *OpenALAudioManager::getHandleForBink()
{
	return NULL;
}

void OpenALAudioManager::releaseHandleForBink()
{
}

void OpenALAudioManager::friend_forcePlayAudioEventRTS(const AudioEventRTS*)
{
}

void OpenALAudioManager::setPreferredProvider(AsciiString)
{
}

void OpenALAudioManager::setPreferredSpeaker(AsciiString)
{
}

Real OpenALAudioManager::getFileLengthMS(AsciiString) const
{
	return 0.0f;
}

void OpenALAudioManager::closeAnySamplesUsingFile(const void *)
{
}

void OpenALAudioManager::setDeviceListenerPosition()
{
	alListener3f(AL_POSITION, m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z);
}

#endif
