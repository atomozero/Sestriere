/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * AudioEngine.h — Audio recording and playback for voice messages
 *
 * Uses BSoundPlayer for playback (8kHz 16-bit mono PCM).
 * Uses BMediaRecorder for recording audio input.
 */

#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <Handler.h>
#include <MediaDefs.h>
#include <MediaRecorder.h>
#include <SoundPlayer.h>
#include <SupportDefs.h>


class AudioEngine {
public:
						AudioEngine();
						~AudioEngine();

	// Check if audio input is available
	static bool			IsInputAvailable();

	// Recording
	status_t			StartRecording();
	status_t			StopRecording(int16** outPcm, size_t* outSamples);
	bool				IsRecording() const { return fRecording; }

	// Playback
	status_t			Play(const int16* pcmData, size_t sampleCount,
							BHandler* notifyTarget = NULL);
	void				Stop();
	bool				IsPlaying() const { return fPlaying; }

private:
	// BSoundPlayer callback for playback
	static void			_PlayBuffer(void* cookie, void* buffer,
							size_t size,
							const media_raw_audio_format& fmt);

	// BSoundPlayer notification callback
	static void			_PlayNotify(void* cookie,
							BSoundPlayer::sound_player_notification what,
							...);

	// BMediaRecorder callback for recording
	static void			_RecordBuffer(void* cookie, bigtime_t timestamp,
							void* data, size_t size,
							const media_format& format);

	BSoundPlayer*		fPlayer;
	BMediaRecorder*		fRecorder;
	bool				fRecording;
	bool				fPlaying;

	// Record buffer (growing)
	int16*				fRecordBuffer;
	size_t				fRecordSize;     // samples written
	size_t				fRecordCapacity; // samples allocated

	// Play buffer
	const int16*		fPlayBuffer;
	size_t				fPlaySize;       // total samples
	size_t				fPlayPosition;   // current sample offset
	BHandler*			fPlayNotify;     // receives MSG_VOICE_PLAY_DONE
};


#endif // AUDIOENGINE_H
