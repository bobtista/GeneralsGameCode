/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#if defined(SAGE_USE_OPENAL)

#include "Common/GameAudio.h"

struct ALCdevice;
struct ALCcontext;

class OpenALAudioManager : public AudioManager
{
public:
	OpenALAudioManager();
	virtual ~OpenALAudioManager() override;

#if defined(RTS_DEBUG)
	virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *userData, FILE *fp = NULL) override;
#endif

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;
	virtual void stopAudio(AudioAffect which) override;
	virtual void pauseAudio(AudioAffect which) override;
	virtual void resumeAudio(AudioAffect which) override;
	virtual void pauseAmbient(Bool shouldPause) override;
	virtual void killAudioEventImmediately(AudioHandle audioEvent) override;
	virtual void nextMusicTrack() override;
	virtual void prevMusicTrack() override;
	virtual Bool isMusicPlaying() const override;
	virtual Bool hasMusicTrackCompleted(const AsciiString& trackName, Int numberOfTimes) const override;
	virtual AsciiString getMusicTrackName() const override;
	virtual void openDevice() override;
	virtual void closeDevice() override;
	virtual void *getDevice() override;
	virtual void notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags) override;
	virtual UnsignedInt getProviderCount() const override;
	virtual AsciiString getProviderName(UnsignedInt providerNum) const override;
	virtual UnsignedInt getProviderIndex(AsciiString providerName) const override;
	virtual void selectProvider(UnsignedInt providerNdx) override;
	virtual void unselectProvider() override;
	virtual UnsignedInt getSelectedProvider() const override;
	virtual void setSpeakerType(UnsignedInt speakerType) override;
	virtual UnsignedInt getSpeakerType() override;
	virtual UnsignedInt getNum2DSamples() const override;
	virtual UnsignedInt getNum3DSamples() const override;
	virtual UnsignedInt getNumStreams() const override;
	virtual Bool doesViolateLimit(AudioEventRTS *event) const override;
	virtual Bool isPlayingLowerPriority(AudioEventRTS *event) const override;
	virtual Bool isPlayingAlready(AudioEventRTS *event) const override;
	virtual Bool isObjectPlayingVoice(UnsignedInt objID) const override;
	virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume) override;
	virtual void removePlayingAudio(AsciiString eventName) override;
	virtual void removeAllDisabledAudio() override;
	virtual Bool has3DSensitiveStreamsPlaying() const override;
	virtual void *getHandleForBink() override;
	virtual void releaseHandleForBink() override;
	virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay) override;
	virtual void setPreferredProvider(AsciiString providerNdx) override;
	virtual void setPreferredSpeaker(AsciiString speakerType) override;
	virtual Real getFileLengthMS(AsciiString strToLoad) const override;
	virtual void closeAnySamplesUsingFile(const void *fileToClose) override;

protected:
	virtual void setDeviceListenerPosition() override;

private:
	ALCdevice *m_device;
	ALCcontext *m_context;
	UnsignedInt m_selectedProvider;
	UnsignedInt m_speakerType;
	AsciiString m_musicTrackName;
};

#endif
