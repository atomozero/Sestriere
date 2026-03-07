/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * AudioEngine.cpp — Audio recording and playback for voice messages
 *
 * On construction, creates a BMediaRecorder that registers as a
 * Cortex media node named "Sestriere".  This allows users to see
 * Sestriere in Cortex and route any audio source to it.
 *
 * Recording flow:
 *   1. StartRecording() — if not already connected (via Cortex),
 *      tries auto-connect to system audio input.
 *   2. Buffers flow into _RecordBuffer() callback.
 *   3. StopRecording() — stops accepting buffers but keeps the
 *      Cortex connection alive for future recordings.
 */

#include "AudioEngine.h"

#include <Looper.h>
#include <MediaRoster.h>
#include <Message.h>
#include <TimeSource.h>

#include <Autolock.h>

#include <cstdio>
#include <cstring>
#include <new>

#include "Constants.h"


// Audio format: 8kHz mono 16-bit signed, matching Codec2 input
static const uint32 kSampleRate = 8000;
static const uint32 kBufferSize = 2048;  // bytes per BSoundPlayer buffer

// Max recording: 30 seconds at 8kHz = 240000 samples
static const size_t kMaxRecordSamples = 30 * kSampleRate;
static const size_t kInitialRecordCapacity = kSampleRate * 5;  // 5 seconds


AudioEngine::AudioEngine()
	:
	fInitErr(B_NO_INIT),
	fPlayer(NULL),
	fRecorder(NULL),
	fRecording(false),
	fPlaying(false),
	fRecordBuffer(NULL),
	fRecordSize(0),
	fRecordCapacity(0),
	fPlayBuffer(NULL),
	fPlaySize(0),
	fPlayPosition(0),
	fPlayNotify(NULL)
{
	// Create recorder — this registers a Cortex consumer node
	// named "Sestriere" so it appears in Cortex immediately.
	fRecorder = new(std::nothrow) BMediaRecorder("Sestriere",
		B_MEDIA_RAW_AUDIO);
	if (fRecorder == NULL || fRecorder->InitCheck() != B_OK) {
		fprintf(stderr, "[AudioEngine] BMediaRecorder init failed — "
			"media_server may not be running\n");
		delete fRecorder;
		fRecorder = NULL;
		fInitErr = B_ERROR;
		return;
	}

	// Set accepted format: 8kHz mono 16-bit signed
	media_format format = {};
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio.frame_rate = kSampleRate;
	format.u.raw_audio.channel_count = 1;
	format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.u.raw_audio.byte_order =
		B_HOST_IS_LENDIAN ? B_MEDIA_LITTLE_ENDIAN : B_MEDIA_BIG_ENDIAN;
	format.u.raw_audio.buffer_size = kBufferSize;

	fRecorder->SetAcceptedFormat(format);
	fRecorder->SetHooks(_RecordBuffer, NULL, this);

	fInitErr = B_OK;
}


AudioEngine::~AudioEngine()
{
	Stop();
	if (fRecording)
		StopRecording(NULL, NULL);

	if (fRecorder != NULL) {
		if (fRecorder->IsConnected())
			fRecorder->Disconnect();
		delete fRecorder;
	}

	delete fPlayer;
	delete[] fRecordBuffer;
}


// =============================================================================
// Recording
// =============================================================================


status_t
AudioEngine::StartRecording()
{
	BAutolock lock(fLock);

	if (fRecording)
		return B_BUSY;

	if (fRecorder == NULL)
		return B_NO_INIT;

	// Allocate recording buffer
	delete[] fRecordBuffer;
	fRecordCapacity = kInitialRecordCapacity;
	fRecordBuffer = new(std::nothrow) int16[fRecordCapacity];
	if (fRecordBuffer == NULL)
		return B_NO_MEMORY;
	fRecordSize = 0;

	// If not already connected (via Cortex routing), try auto-connect
	// to the system audio input
	if (!fRecorder->IsConnected()) {
		media_format format = {};
		format.type = B_MEDIA_RAW_AUDIO;
		format.u.raw_audio.frame_rate = kSampleRate;
		format.u.raw_audio.channel_count = 1;
		format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
		format.u.raw_audio.byte_order =
			B_HOST_IS_LENDIAN ? B_MEDIA_LITTLE_ENDIAN : B_MEDIA_BIG_ENDIAN;
		format.u.raw_audio.buffer_size = kBufferSize;

		status_t err = fRecorder->Connect(format);
		if (err != B_OK) {
			fprintf(stderr, "[AudioEngine] No audio input — "
				"connect a source in Cortex or plug in a microphone\n");
			delete[] fRecordBuffer;
			fRecordBuffer = NULL;
			return err;
		}
	}

	// Start recording
	status_t err = fRecorder->Start();
	if (err != B_OK) {
		fprintf(stderr, "[AudioEngine] Failed to start recording: %s\n",
			strerror(err));
		delete[] fRecordBuffer;
		fRecordBuffer = NULL;
		return err;
	}

	fRecording = true;
	return B_OK;
}


status_t
AudioEngine::StopRecording(int16** outPcm, size_t* outSamples)
{
	BAutolock lock(fLock);

	if (!fRecording)
		return B_ERROR;

	fRecording = false;

	if (fRecorder != NULL) {
		fRecorder->Stop();
		// Don't disconnect — keep the Cortex node connection alive
		// so the user doesn't have to re-route in Cortex each time
	}

	// Return the recorded buffer
	if (outPcm != NULL && outSamples != NULL && fRecordSize > 0) {
		*outPcm = fRecordBuffer;
		*outSamples = fRecordSize;
		fRecordBuffer = NULL;  // Caller owns the buffer now
	} else {
		delete[] fRecordBuffer;
	}
	fRecordBuffer = NULL;
	fRecordSize = 0;
	fRecordCapacity = 0;

	return B_OK;
}


void
AudioEngine::_RecordBuffer(void* cookie, bigtime_t timestamp,
	void* data, size_t size, const media_format& format)
{
	(void)timestamp;
	AudioEngine* engine = (AudioEngine*)cookie;
	if (engine == NULL || !engine->fRecording)
		return;

	BAutolock lock(engine->fLock);
	if (!engine->fRecording || engine->fRecordBuffer == NULL)
		return;

	// Convert to samples
	size_t samples = size / sizeof(int16);
	if (samples == 0)
		return;

	const int16* incoming = (const int16*)data;

	// Resample if needed (simple nearest-neighbor for non-8kHz input)
	float srcRate = format.u.raw_audio.frame_rate;
	uint32 srcChannels = format.u.raw_audio.channel_count;

	if (srcRate != kSampleRate || srcChannels != 1) {
		// Calculate output samples
		float ratio = (float)kSampleRate / srcRate;
		size_t srcFrames = size / (srcChannels * sizeof(int16));
		size_t outSamples = (size_t)(srcFrames * ratio);

		// Grow buffer if needed
		size_t needed = engine->fRecordSize + outSamples;
		if (needed > kMaxRecordSamples)
			outSamples = kMaxRecordSamples - engine->fRecordSize;
		if (outSamples == 0) {
			engine->fRecording = false;
			return;
		}

		if (needed > engine->fRecordCapacity) {
			size_t newCap = engine->fRecordCapacity * 2;
			if (newCap < needed) newCap = needed;
			if (newCap > kMaxRecordSamples) newCap = kMaxRecordSamples;
			int16* newBuf = new(std::nothrow) int16[newCap];
			if (newBuf == NULL) return;
			memcpy(newBuf, engine->fRecordBuffer,
				engine->fRecordSize * sizeof(int16));
			delete[] engine->fRecordBuffer;
			engine->fRecordBuffer = newBuf;
			engine->fRecordCapacity = newCap;
		}

		// Resample + downmix
		for (size_t i = 0; i < outSamples; i++) {
			size_t srcIdx = (size_t)(i / ratio);
			if (srcIdx >= srcFrames) srcIdx = srcFrames - 1;
			int32 sum = 0;
			for (uint32 ch = 0; ch < srcChannels; ch++)
				sum += incoming[srcIdx * srcChannels + ch];
			engine->fRecordBuffer[engine->fRecordSize++] =
				(int16)(sum / (int32)srcChannels);
		}
	} else {
		// Direct copy — format matches
		size_t needed = engine->fRecordSize + samples;
		if (needed > kMaxRecordSamples)
			samples = kMaxRecordSamples - engine->fRecordSize;
		if (samples == 0) {
			engine->fRecording = false;
			return;
		}

		if (needed > engine->fRecordCapacity) {
			size_t newCap = engine->fRecordCapacity * 2;
			if (newCap < needed) newCap = needed;
			if (newCap > kMaxRecordSamples) newCap = kMaxRecordSamples;
			int16* newBuf = new(std::nothrow) int16[newCap];
			if (newBuf == NULL) return;
			memcpy(newBuf, engine->fRecordBuffer,
				engine->fRecordSize * sizeof(int16));
			delete[] engine->fRecordBuffer;
			engine->fRecordBuffer = newBuf;
			engine->fRecordCapacity = newCap;
		}

		memcpy(engine->fRecordBuffer + engine->fRecordSize,
			incoming, samples * sizeof(int16));
		engine->fRecordSize += samples;
	}
}


// =============================================================================
// Playback
// =============================================================================


status_t
AudioEngine::Play(const int16* pcmData, size_t sampleCount,
	BHandler* notifyTarget)
{
	if (fPlaying)
		Stop();

	if (pcmData == NULL || sampleCount == 0)
		return B_BAD_VALUE;

	// Make an owned copy so caller can free their buffer immediately
	delete[] fPlayBuffer;
	fPlayBuffer = new(std::nothrow) int16[sampleCount];
	if (fPlayBuffer == NULL)
		return B_NO_MEMORY;
	memcpy(fPlayBuffer, pcmData, sampleCount * sizeof(int16));
	fPlaySize = sampleCount;
	fPlayPosition = 0;
	fPlayNotify = notifyTarget;

	// Create sound player with 8kHz mono format
	media_raw_audio_format format;
	format.frame_rate = kSampleRate;
	format.channel_count = 1;
	format.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.byte_order = B_HOST_IS_LENDIAN
		? B_MEDIA_LITTLE_ENDIAN : B_MEDIA_BIG_ENDIAN;
	format.buffer_size = kBufferSize;

	delete fPlayer;
	fPlayer = new(std::nothrow) BSoundPlayer(&format, "Sestriere Voice",
		_PlayBuffer, _PlayNotify, this);

	if (fPlayer == NULL || fPlayer->InitCheck() != B_OK) {
		fprintf(stderr, "[AudioEngine] BSoundPlayer init failed\n");
		delete fPlayer;
		fPlayer = NULL;
		return B_ERROR;
	}

	fPlaying = true;
	fPlayer->SetHasData(true);
	fPlayer->Start();

	return B_OK;
}


void
AudioEngine::Stop()
{
	if (!fPlaying)
		return;

	fPlaying = false;
	if (fPlayer != NULL) {
		fPlayer->Stop(true, true);
		delete fPlayer;
		fPlayer = NULL;
	}

	delete[] fPlayBuffer;
	fPlayBuffer = NULL;
	fPlaySize = 0;
	fPlayPosition = 0;
}


void
AudioEngine::_PlayBuffer(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& fmt)
{
	(void)fmt;
	AudioEngine* engine = (AudioEngine*)cookie;
	if (engine == NULL || !engine->fPlaying) {
		memset(buffer, 0, size);
		return;
	}

	int16* out = (int16*)buffer;
	size_t samplesNeeded = size / sizeof(int16);
	size_t remaining = engine->fPlaySize - engine->fPlayPosition;

	if (remaining == 0) {
		memset(buffer, 0, size);
		engine->fPlaying = false;
		return;
	}

	size_t toCopy = (remaining < samplesNeeded) ? remaining : samplesNeeded;
	memcpy(out, engine->fPlayBuffer + engine->fPlayPosition,
		toCopy * sizeof(int16));

	// Zero-fill remainder if at end
	if (toCopy < samplesNeeded)
		memset(out + toCopy, 0, (samplesNeeded - toCopy) * sizeof(int16));

	engine->fPlayPosition += toCopy;

	// Check if playback finished
	if (engine->fPlayPosition >= engine->fPlaySize) {
		engine->fPlaying = false;
		// Notify target directly — BSoundPlayer won't fire B_STOPPED
		// until Stop() is called, so we send the notification here
		if (engine->fPlayNotify != NULL) {
			BLooper* looper = engine->fPlayNotify->Looper();
			if (looper != NULL) {
				BMessage msg(MSG_VOICE_PLAY_DONE);
				looper->PostMessage(&msg, engine->fPlayNotify);
			}
			engine->fPlayNotify = NULL;  // send once
		}
	}
}


void
AudioEngine::_PlayNotify(void* cookie,
	BSoundPlayer::sound_player_notification what, ...)
{
	AudioEngine* engine = (AudioEngine*)cookie;
	if (engine == NULL)
		return;

	if (what == BSoundPlayer::B_STOPPED && !engine->fPlaying) {
		// Playback finished — notify target
		if (engine->fPlayNotify != NULL) {
			BLooper* looper = engine->fPlayNotify->Looper();
			if (looper != NULL) {
				BMessage msg(MSG_VOICE_PLAY_DONE);
				looper->PostMessage(&msg, engine->fPlayNotify);
			}
		}
	}
}
