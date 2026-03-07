/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * VoiceSession.h — Voice message session management for LoRa voice sharing
 *
 * Compatible with meshcore-sar VE2 voice envelope protocol.
 * Uses CMD_SEND_RAW_DATA (magic 0x56) for voice data fragments and
 * CMD_SEND_BINARY_REQ (magic 0x72) for voice fetch requests.
 */

#ifndef VOICESESSION_H
#define VOICESESSION_H

#include <OS.h>
#include <String.h>
#include <SupportDefs.h>

#include <cstdlib>
#include <cstring>

#include "Compat.h"
#include "Types.h"


// Protocol constants
const uint8 kVoiceMagic = 0x56;       // 'V' — voice data packet
const uint8 kVoiceFetchMagic = 0x72;  // 'r' — voice fetch request
const size_t kVoiceHeaderSize = 8;    // [magic:1][sid:4][mode:1][idx:1][total:1]
const size_t kMaxVoiceFragmentPayload = 152;
const bigtime_t kVoiceSessionTTL = 15 * 60 * 1000000LL; // 15 minutes

// Max recording duration in seconds
const uint32 kMaxVoiceRecordSeconds = 30;

// Codec2 mode IDs (meshcore-sar compatible)
enum VoicePacketMode {
	VOICE_MODE_700C  = 0,
	VOICE_MODE_1200  = 1,
	VOICE_MODE_2400  = 2,
	VOICE_MODE_1300  = 3,   // default — 1300 bps
	VOICE_MODE_1400  = 4,
	VOICE_MODE_1600  = 5,
	VOICE_MODE_3200  = 6
};

// Session states
enum VoiceSessionState {
	VOICE_PENDING   = 0,    // Envelope received, no fragments yet
	VOICE_LOADING   = 1,    // Receiving fragments
	VOICE_COMPLETE  = 2,    // All fragments received
	VOICE_FAILED    = 3,    // Transfer failed or timed out
	VOICE_SENDING   = 4     // Outgoing, transmitting fragments
};


// Single voice fragment
struct VoiceFragment {
	uint8   data[kMaxVoiceFragmentPayload];
	uint16  length;
	bool    received;

	VoiceFragment() : length(0), received(false) {
		memset(data, 0, sizeof(data));
	}
};


// Voice transfer session
struct VoiceSession {
	uint32              sessionId;
	VoicePacketMode     mode;
	uint8               totalFragments;
	uint8               receivedCount;
	uint32              durationSec;    // Audio duration in seconds
	uint32              totalBytes;     // Total Codec2 data size
	uint8               senderKey[6];   // Sender pubkey prefix
	uint32              timestamp;      // Creation timestamp
	VoiceSessionState   state;
	bigtime_t           createdTime;    // system_time() at creation
	VoiceFragment       fragments[255];
	uint8*              codec2Data;     // Reassembled Codec2 data (owned)
	size_t              codec2Size;

	VoiceSession()
		: sessionId(0), mode(VOICE_MODE_1300), totalFragments(0),
		  receivedCount(0), durationSec(0), totalBytes(0),
		  timestamp(0), state(VOICE_PENDING), createdTime(0),
		  codec2Data(NULL), codec2Size(0)
	{
		memset(senderKey, 0, sizeof(senderKey));
	}

	~VoiceSession() {
		free(codec2Data);
	}

	bool IsComplete() const {
		return receivedCount >= totalFragments && totalFragments > 0;
	}

	bool IsExpired() const {
		return (system_time() - createdTime) > kVoiceSessionTTL;
	}

	float Progress() const {
		if (totalFragments == 0) return 0.0f;
		return (float)receivedCount / totalFragments;
	}

	// Reassemble fragments into contiguous Codec2 data.
	// Returns true on success.
	bool Reassemble() {
		if (!IsComplete())
			return false;

		size_t total = 0;
		for (uint8 i = 0; i < totalFragments; i++)
			total += fragments[i].length;

		uint8* buf = (uint8*)malloc(total);
		if (buf == NULL)
			return false;

		size_t offset = 0;
		for (uint8 i = 0; i < totalFragments; i++) {
			memcpy(buf + offset, fragments[i].data, fragments[i].length);
			offset += fragments[i].length;
		}

		free(codec2Data);
		codec2Data = buf;
		codec2Size = total;
		return true;
	}

private:
	// Non-copyable
	VoiceSession(const VoiceSession&);
	VoiceSession& operator=(const VoiceSession&);
};


// Manages active voice transfer sessions
class VoiceSessionManager {
public:
							VoiceSessionManager();
							~VoiceSessionManager();

	// Create outgoing session: fragments the Codec2 data for transmission.
	// Returns session ID.
	uint32				CreateOutgoing(const uint8* codec2Data, size_t size,
							VoicePacketMode mode, uint32 durationSec,
							const uint8* selfKeyPrefix);

	// Create incoming session from VE2 envelope text.
	// Returns session pointer or NULL on parse failure.
	VoiceSession*		CreateFromEnvelope(const char* ve2Text);

	// Add a received fragment. Returns true if session is now complete.
	bool				AddFragment(uint32 sessionId, uint8 index,
							const uint8* data, uint16 length);

	// Find session by ID. Returns NULL if not found.
	VoiceSession*		FindSession(uint32 sessionId);

	// Remove expired sessions.
	void				PurgeExpired();

	// --- Static protocol helpers ---

	// Format VE2 envelope text for a session.
	static BString		FormatEnvelope(uint32 sessionId, VoicePacketMode mode,
							uint8 totalFragments, uint32 durationSec,
							const uint8* senderKey, uint32 timestamp);

	// Parse VE2 envelope text. Returns true on success.
	static bool			ParseEnvelope(const char* text, uint32* outSid,
							VoicePacketMode* outMode, uint8* outTotal,
							uint32* outDuration, uint8* outSenderKey,
							uint32* outTimestamp);

	// Check if a text message is a VE2 voice envelope.
	static bool			IsVoiceEnvelope(const char* text);

	// Build a fragment packet with header for CMD_SEND_RAW_DATA.
	// Returns total packet size written to outPacket.
	static size_t		BuildFragmentPacket(uint8* outPacket,
							uint32 sessionId, VoicePacketMode mode,
							uint8 index, uint8 total,
							const uint8* fragData, uint16 fragLen);

	// Build a fetch request packet for CMD_SEND_BINARY_REQ.
	// Returns total packet size written to outPacket.
	static size_t		BuildFetchRequest(uint8* outPacket,
							uint32 sessionId, uint8 flags,
							const uint8* senderKey, uint32 timestamp,
							const uint8* missingIndices, uint8 count);

	// --- Base-36 helpers (meshcore-sar compatible) ---
	static BString		ToBase36(uint32 value);
	static uint32		FromBase36(const char* str);

private:
	OwningObjectList<VoiceSession>	fSessions;
	uint32							fNextSessionId;
};

#endif // VOICESESSION_H
