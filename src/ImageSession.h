/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ImageSession.h — Image fragment session management for LoRa image sharing
 *
 * Compatible with meshcore-sar IE2 envelope protocol.
 * Uses CMD_SEND_RAW_DATA (magic 0x49) for data fragments and
 * CMD_SEND_BINARY_REQ (magic 0x69) for fetch requests.
 */

#ifndef IMAGESESSION_H
#define IMAGESESSION_H

#include <OS.h>
#include <String.h>
#include <SupportDefs.h>

#include <cstdlib>
#include <cstring>

#include "Compat.h"
#include "Types.h"


// Protocol constants
const uint8 kImageMagic = 0x49;       // 'I' — image data packet
const uint8 kFetchMagic = 0x69;       // 'i' — fetch request
const uint8 kImageFormatJPEG = 1;
const size_t kMaxFragmentPayload = 152;
const size_t kImageHeaderSize = 8;    // [magic:1][sid:4][fmt:1][idx:1][total:1]
const bigtime_t kSessionTTL = 15 * 60 * 1000000LL; // 15 minutes in microseconds

// Session states
enum ImageSessionState {
	IMAGE_PENDING   = 0,    // Envelope received, no fragments yet
	IMAGE_LOADING   = 1,    // Receiving fragments
	IMAGE_COMPLETE  = 2,    // All fragments received
	IMAGE_FAILED    = 3,    // Transfer failed or timed out
	IMAGE_SENDING   = 4     // Outgoing, transmitting fragments
};


// Single image fragment
struct ImageFragment {
	uint8   data[kMaxFragmentPayload];
	uint16  length;
	bool    received;

	ImageFragment() : length(0), received(false) {
		memset(data, 0, sizeof(data));
	}
};


// Image transfer session
struct ImageSession {
	uint32              sessionId;
	uint8               format;         // kImageFormatJPEG
	uint8               totalFragments;
	uint8               receivedCount;
	int32               width;
	int32               height;
	uint32              totalBytes;     // Total JPEG size
	uint8               senderKey[6];   // Sender pubkey prefix
	uint32              timestamp;      // Creation timestamp
	ImageSessionState   state;
	bigtime_t           createdTime;    // system_time() at creation
	ImageFragment       fragments[255];
	uint8*              jpegData;       // Reassembled JPEG (owned)
	size_t              jpegSize;

	ImageSession()
		: sessionId(0), format(kImageFormatJPEG), totalFragments(0),
		  receivedCount(0), width(0), height(0), totalBytes(0),
		  timestamp(0), state(IMAGE_PENDING), createdTime(0),
		  jpegData(NULL), jpegSize(0)
	{
		memset(senderKey, 0, sizeof(senderKey));
	}

	~ImageSession() {
		free(jpegData);
	}

	bool IsComplete() const {
		return receivedCount >= totalFragments && totalFragments > 0;
	}

	bool IsExpired() const {
		return (system_time() - createdTime) > kSessionTTL;
	}

	float Progress() const {
		if (totalFragments == 0) return 0.0f;
		return (float)receivedCount / totalFragments;
	}

	// Reassemble fragments into contiguous JPEG data.
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

		free(jpegData);
		jpegData = buf;
		jpegSize = total;
		return true;
	}

private:
	// Non-copyable
	ImageSession(const ImageSession&);
	ImageSession& operator=(const ImageSession&);
};


// Manages active image transfer sessions
class ImageSessionManager {
public:
							ImageSessionManager();
							~ImageSessionManager();

	// Create outgoing session: fragments the JPEG data for transmission.
	// Returns session ID.
	uint32				CreateOutgoing(const uint8* jpegData, size_t size,
							int32 width, int32 height,
							const uint8* selfKeyPrefix);

	// Create incoming session from IE2 envelope text.
	// Returns session pointer or NULL on parse failure.
	ImageSession*		CreateFromEnvelope(const char* ie2Text);

	// Add a received fragment. Returns true if session is now complete.
	bool				AddFragment(uint32 sessionId, uint8 index,
							const uint8* data, uint16 length);

	// Find session by ID. Returns NULL if not found.
	ImageSession*		FindSession(uint32 sessionId);

	// Remove expired sessions.
	void				PurgeExpired();

	// --- Static protocol helpers ---

	// Format IE2 envelope text for a session.
	static BString		FormatEnvelope(uint32 sessionId, uint8 format,
							uint8 totalFragments, int32 width, int32 height,
							uint32 totalBytes, const uint8* senderKey,
							uint32 timestamp);

	// Parse IE2 envelope text. Returns true on success.
	static bool			ParseEnvelope(const char* text, uint32* outSid,
							uint8* outFormat, uint8* outTotal,
							int32* outWidth, int32* outHeight,
							uint32* outBytes, uint8* outSenderKey,
							uint32* outTimestamp);

	// Check if a text message is an IE2 image envelope.
	static bool			IsImageEnvelope(const char* text);

	// Build a fragment packet with header for CMD_SEND_RAW_DATA.
	// Returns total packet size written to outPacket.
	static size_t		BuildFragmentPacket(uint8* outPacket,
							uint32 sessionId, uint8 format,
							uint8 index, uint8 total,
							const uint8* fragData, uint16 fragLen);

	// Build a fetch request packet for CMD_SEND_BINARY_REQ.
	// missingIndices is an array of fragment indices to request.
	// Returns total packet size written to outPacket.
	static size_t		BuildFetchRequest(uint8* outPacket,
							uint32 sessionId,
							const uint8* missingIndices, uint8 count);

private:
	OwningObjectList<ImageSession>	fSessions;
	uint32							fNextSessionId;
};

#endif // IMAGESESSION_H
