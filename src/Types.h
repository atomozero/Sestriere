/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Types.h — Basic types and radio presets
 */

#ifndef TYPES_H
#define TYPES_H

#include "Compat.h"
#include <SupportDefs.h>

#include <cstring>

// Radio preset information
struct RadioPresetInfo {
	const char* name;
	uint32 frequency;    // Hz
	uint32 bandwidth;    // Hz
	uint8 spreadingFactor;
	uint8 codingRate;
};

enum RadioPreset {
	PRESET_CUSTOM = 0,
	PRESET_MESHCORE_DEFAULT,    // 906.875 MHz, 250kHz, SF11, CR5
	PRESET_USA_CANADA_NARROW,   // 910.525 MHz, 62.5kHz, SF7, CR5
	PRESET_USA_FAST,            // 906.875 MHz, 500kHz, SF7, CR5
	PRESET_EU_UK_NARROW,        // 869.618 MHz, 62.5kHz, SF8, CR8
	PRESET_EU_868_WIDE,         // 868.0 MHz, 125kHz, SF9, CR8
	PRESET_EU_433,              // 433.875 MHz, 125kHz, SF9, CR5
	PRESET_ANZ_915,             // 915.0 MHz, 125kHz, SF9, CR8
	PRESET_NZ_915,              // 915.0 MHz, 250kHz, SF11, CR5
	PRESET_LONG_RANGE,          // 868.0 MHz, 62.5kHz, SF12, CR8
	PRESET_MEDIUM_RANGE,        // 868.0 MHz, 125kHz, SF10, CR5
	PRESET_FAST,                // 868.0 MHz, 250kHz, SF7, CR5
	PRESET_COUNT
};

static const RadioPresetInfo kRadioPresets[] = {
	{ "Custom",              0,         0,      0,  0 },
	{ "MeshCore Default",    906875000, 250000, 11, 5 },
	{ "USA/Canada Narrow",   910525000,  62500,  7, 5 },
	{ "USA Fast",            906875000, 500000,  7, 5 },
	{ "EU/UK 868 Narrow",    869618000,  62500,  8, 8 },
	{ "EU 868 Wide",         868000000, 125000,  9, 8 },
	{ "EU 433 MHz",          433875000, 125000,  9, 5 },
	{ "ANZ 915",             915000000, 125000,  9, 8 },
	{ "NZ 915",              915000000, 250000, 11, 5 },
	{ "Long Range",          868000000,  62500, 12, 8 },
	{ "Medium Range",        868000000, 125000, 10, 5 },
	{ "Fast",                868000000, 250000,  7, 5 },
};

// Size constants
static const size_t kPubKeySize = 32;
static const size_t kPubKeyPrefixSize = 6;
static const uint8 kPathLenDirect = 0xFF;	// Direct path (no hops)

// Delivery status for outgoing messages
enum DeliveryStatus {
	DELIVERY_PENDING	= 0,	// Waiting for radio to transmit
	DELIVERY_SENT		= 1,	// Radio transmitted (RSP_SENT)
	DELIVERY_CONFIRMED	= 2		// Recipient ACKed (PUSH_SEND_CONFIRMED)
};

// Chat message for display in ChatView
struct ChatMessage {
	uint8	pubKeyPrefix[kPubKeyPrefixSize];
	uint8	pathLen;			// kPathLenDirect = direct, otherwise hop count
	int8	snr;				// Signal-to-noise ratio
	uint32	timestamp;			// Sender timestamp
	char	text[256];			// Message content
	bool	isOutgoing;			// True if we sent this message
	bool	isChannel;			// True if channel message (vs DM)
	uint8	deliveryStatus;		// DeliveryStatus enum value
	uint32	roundTripMs;		// Round-trip time from PUSH_SEND_CONFIRMED
	uint8	txtType;			// 0=plain, 1=CLI data

	ChatMessage() : pathLen(kPathLenDirect), snr(0), timestamp(0),
					isOutgoing(false), isChannel(false),
					deliveryStatus(DELIVERY_SENT), roundTripMs(0),
					txtType(0) {
		memset(pubKeyPrefix, 0, sizeof(pubKeyPrefix));
		memset(text, 0, sizeof(text));
	}
};

// Contact information
struct ContactInfo {
	uint8	publicKey[kPubKeySize];
	uint8	type;			// ADV_TYPE: 0=NONE, 1=CHAT, 2=REPEATER, 3=ROOM
	uint8	flags;
	int8	outPathLen;		// Outbound path length (-1 = unknown, 0xFF = direct)
	uint32	lastSeen;		// timestamp
	int32	latitude;		// GPS latitude (1e-7 degrees, 0 = unknown)
	int32	longitude;		// GPS longitude (1e-7 degrees, 0 = unknown)
	char	name[64];
	bool	isValid;

	// Message history for this contact (owning = true)
	OwningObjectList<ChatMessage>	messages;

	ContactInfo() : type(0), flags(0), outPathLen(0), lastSeen(0),
					latitude(0), longitude(0), isValid(false) {
		memset(publicKey, 0, sizeof(publicKey));
		memset(name, 0, sizeof(name));
	}

	bool HasGPS() const { return latitude != 0 || longitude != 0; }
};

// Channel information (private/custom channels)
struct ChannelInfo {
	uint8	index;				// Channel slot index (0-based)
	char	name[32];			// Channel name (null-terminated)
	uint8	secret[16];			// PSK encryption key

	// Message history for this channel (owning = true)
	OwningObjectList<ChatMessage>	messages;

	ChannelInfo() : index(0) {
		memset(name, 0, sizeof(name));
		memset(secret, 0, sizeof(secret));
	}

	bool IsEmpty() const { return name[0] == '\0'; }
};

// Captured raw radio packet for Packet Analyzer
struct CapturedPacket {
	uint32		index;				// Sequential packet number
	uint32		timestamp;			// Capture time (real_time_clock, seconds)
	bigtime_t	captureTime;		// Capture time (system_time, microseconds)
	uint8		code;				// MeshCore response/push code
	int8		snr;				// Signal-to-noise ratio
	int8		rssi;				// Signal strength
	uint8		pathLen;			// Hop count
	uint16		payloadSize;		// Raw payload size
	uint8		payload[512];		// Raw payload data
	char		typeStr[32];		// Human-readable type name
	char		sourceStr[80];		// Source identifier (name + hex key prefix)
	char		summary[128];		// Decoded summary text

	CapturedPacket() : index(0), timestamp(0), captureTime(0), code(0),
					   snr(0), rssi(0), pathLen(0), payloadSize(0) {
		memset(payload, 0, sizeof(payload));
		memset(typeStr, 0, sizeof(typeStr));
		memset(sourceStr, 0, sizeof(sourceStr));
		memset(summary, 0, sizeof(summary));
	}
};


// Incoming message
struct IncomingMessage {
	uint8	senderPubKey[kPubKeyPrefixSize];
	uint8	type;				// Message type
	int8	rssi;				// Signal strength
	int8	snr;				// Signal to noise ratio
	char	text[256];			// Message content
	uint32	timestamp;			// When received

	IncomingMessage() : type(0), rssi(0), snr(0), timestamp(0) {
		memset(senderPubKey, 0, sizeof(senderPubKey));
		memset(text, 0, sizeof(text));
	}
};

#endif // TYPES_H
