/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * VoiceCodec.h — Codec2 encoding/decoding wrapper for voice messages
 */

#ifndef VOICECODEC_H
#define VOICECODEC_H

#include <SupportDefs.h>

#include "VoiceSession.h"


class VoiceCodec {
public:
	// Encode PCM -> Codec2. Caller must free(*outCodec2).
	static status_t		Encode(const int16* pcmData, size_t sampleCount,
							VoicePacketMode mode,
							uint8** outCodec2, size_t* outSize);

	// Decode Codec2 -> PCM. Caller must delete[] *outPcm.
	static status_t		Decode(const uint8* codec2Data, size_t size,
							VoicePacketMode mode,
							int16** outPcm, size_t* outSampleCount);

	// Calculate duration in seconds from Codec2 data size.
	static uint32		DurationSec(size_t codec2Size, VoicePacketMode mode);

	// Returns bytes per frame for the specified mode.
	static int32		BytesPerFrame(VoicePacketMode mode);

	// Returns samples per frame for the specified mode.
	static int32		SamplesPerFrame(VoicePacketMode mode);

	// Maps VoicePacketMode to codec2 library mode constant.
	static int			Codec2ModeId(VoicePacketMode mode);
};


#endif // VOICECODEC_H
