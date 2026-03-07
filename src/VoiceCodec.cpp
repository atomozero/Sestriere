/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * VoiceCodec.cpp — Codec2 encoding/decoding wrapper for voice messages
 */

#include "VoiceCodec.h"

#include <cstdlib>
#include <cstring>

#include <codec2/codec2.h>


int
VoiceCodec::Codec2ModeId(VoicePacketMode mode)
{
	// Map meshcore-sar VoicePacketMode to codec2 library constants.
	// meshcore-sar mode IDs differ from codec2 library mode constants.
	switch (mode) {
		case VOICE_MODE_700C:  return CODEC2_MODE_700C;
		case VOICE_MODE_1200:  return CODEC2_MODE_1200;
		case VOICE_MODE_2400:  return CODEC2_MODE_2400;
		case VOICE_MODE_1300:  return CODEC2_MODE_1300;
		case VOICE_MODE_1400:  return CODEC2_MODE_1400;
		case VOICE_MODE_1600:  return CODEC2_MODE_1600;
		case VOICE_MODE_3200:  return CODEC2_MODE_3200;
		default:               return CODEC2_MODE_1300;
	}
}


int32
VoiceCodec::BytesPerFrame(VoicePacketMode mode)
{
	struct CODEC2* c2 = codec2_create(Codec2ModeId(mode));
	if (c2 == NULL)
		return 7;  // fallback for mode 1300
	int32 bytes = codec2_bytes_per_frame(c2);
	codec2_destroy(c2);
	return bytes;
}


int32
VoiceCodec::SamplesPerFrame(VoicePacketMode mode)
{
	struct CODEC2* c2 = codec2_create(Codec2ModeId(mode));
	if (c2 == NULL)
		return 320;  // fallback for mode 1300
	int32 samples = codec2_samples_per_frame(c2);
	codec2_destroy(c2);
	return samples;
}


uint32
VoiceCodec::DurationSec(size_t codec2Size, VoicePacketMode mode)
{
	int32 bpf = BytesPerFrame(mode);
	int32 spf = SamplesPerFrame(mode);
	if (bpf <= 0 || spf <= 0)
		return 0;

	size_t numFrames = codec2Size / bpf;
	size_t totalSamples = numFrames * spf;
	return (uint32)(totalSamples / 8000);  // 8000 Hz sample rate
}


status_t
VoiceCodec::Encode(const int16* pcmData, size_t sampleCount,
	VoicePacketMode mode, uint8** outCodec2, size_t* outSize)
{
	if (pcmData == NULL || sampleCount == 0
		|| outCodec2 == NULL || outSize == NULL)
		return B_BAD_VALUE;

	struct CODEC2* c2 = codec2_create(Codec2ModeId(mode));
	if (c2 == NULL)
		return B_ERROR;

	int samplesPerFrame = codec2_samples_per_frame(c2);
	int bytesPerFrame = codec2_bytes_per_frame(c2);

	size_t numFrames = sampleCount / samplesPerFrame;
	if (numFrames == 0) {
		codec2_destroy(c2);
		return B_BAD_VALUE;
	}

	size_t totalBytes = numFrames * bytesPerFrame;
	uint8* encoded = (uint8*)malloc(totalBytes);
	if (encoded == NULL) {
		codec2_destroy(c2);
		return B_NO_MEMORY;
	}

	for (size_t i = 0; i < numFrames; i++) {
		codec2_encode(c2, encoded + i * bytesPerFrame,
			(short*)(pcmData + i * samplesPerFrame));
	}

	codec2_destroy(c2);

	*outCodec2 = encoded;
	*outSize = totalBytes;
	return B_OK;
}


status_t
VoiceCodec::Decode(const uint8* codec2Data, size_t size,
	VoicePacketMode mode, int16** outPcm, size_t* outSampleCount)
{
	if (codec2Data == NULL || size == 0
		|| outPcm == NULL || outSampleCount == NULL)
		return B_BAD_VALUE;

	struct CODEC2* c2 = codec2_create(Codec2ModeId(mode));
	if (c2 == NULL)
		return B_ERROR;

	int samplesPerFrame = codec2_samples_per_frame(c2);
	int bytesPerFrame = codec2_bytes_per_frame(c2);

	size_t numFrames = size / bytesPerFrame;
	if (numFrames == 0) {
		codec2_destroy(c2);
		return B_BAD_VALUE;
	}

	size_t totalSamples = numFrames * samplesPerFrame;
	int16* decoded = new(std::nothrow) int16[totalSamples];
	if (decoded == NULL) {
		codec2_destroy(c2);
		return B_NO_MEMORY;
	}

	for (size_t i = 0; i < numFrames; i++) {
		codec2_decode(c2, decoded + i * samplesPerFrame,
			codec2Data + i * bytesPerFrame);
	}

	codec2_destroy(c2);

	*outPcm = decoded;
	*outSampleCount = totalSamples;
	return B_OK;
}
