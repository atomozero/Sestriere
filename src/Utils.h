/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Utils.h — Common utility functions for protocol parsing
 */

#ifndef UTILS_H
#define UTILS_H

#include <SupportDefs.h>

#include <cstdio>


// Little-endian integer reading helpers
// These replace the duplicated data[x] | (data[x+1] << 8) ... patterns.

inline uint16
ReadLE16(const uint8* data)
{
	return (uint16)(data[0] | (data[1] << 8));
}


inline int16
ReadLE16Signed(const uint8* data)
{
	return (int16)(data[0] | (data[1] << 8));
}


inline uint32
ReadLE32(const uint8* data)
{
	return (uint32)data[0] | ((uint32)data[1] << 8)
		| ((uint32)data[2] << 16) | ((uint32)data[3] << 24);
}


inline int32
ReadLE32Signed(const uint8* data)
{
	return (int32)ReadLE32(data);
}


// Hex formatting helpers
// These replace the duplicated snprintf(..., "%02X", ...) loop patterns.

inline void
FormatHexBytes(char* dest, const uint8* src, size_t count)
{
	for (size_t i = 0; i < count; i++)
		snprintf(dest + i * 2, 3, "%02X", src[i]);
}


inline void
FormatPubKeyPrefix(char* dest, const uint8* prefix)
{
	FormatHexBytes(dest, prefix, 6);
	dest[12] = '\0';
}


inline void
FormatPubKeyFull(char* dest, const uint8* pubkey)
{
	FormatHexBytes(dest, pubkey, 32);
	dest[64] = '\0';
}


// Lowercase hex for database contact keys (12-char hex string from 6-byte prefix)
inline void
FormatContactKey(char* dest, const uint8* prefix)
{
	for (size_t i = 0; i < 6; i++)
		snprintf(dest + i * 2, 3, "%02x", prefix[i]);
	dest[12] = '\0';
}


// Hex string to byte array parsing helpers
// These replace the duplicated sscanf(..., "%2x", ...) loop patterns.

// Parse 6 hex bytes (12 chars) into a pubkey prefix. Returns true on success.
inline bool
ParseHexPrefix(uint8* dest, const char* hexStr)
{
	for (int i = 0; i < 6; i++) {
		if (hexStr[i * 2] == '\0')
			return false;
		unsigned int byte;
		if (sscanf(hexStr + i * 2, "%2x", &byte) != 1)
			return false;
		dest[i] = (uint8)byte;
	}
	return true;
}


// Parse 32 hex bytes (64 chars) into a full public key. Returns true on success.
inline bool
ParseHexPubKey(uint8* dest, const char* hexStr)
{
	for (int i = 0; i < 32; i++) {
		unsigned int byte;
		if (sscanf(hexStr + i * 2, "%2x", &byte) != 1)
			return false;
		dest[i] = (uint8)byte;
	}
	return true;
}


// Relative time formatting: "Just now" / "N min ago" / "N hr ago" / "N days ago"
// dest must be at least 24 bytes. Returns dest for convenience.
inline char*
FormatTimeAgo(char* dest, size_t size, uint32 ageSeconds)
{
	if (ageSeconds < 60)
		snprintf(dest, size, "Just now");
	else if (ageSeconds < 3600)
		snprintf(dest, size, "%u min ago", (unsigned)(ageSeconds / 60));
	else if (ageSeconds < 86400)
		snprintf(dest, size, "%u hr ago", (unsigned)(ageSeconds / 3600));
	else
		snprintf(dest, size, "%u days ago", (unsigned)(ageSeconds / 86400));
	return dest;
}


// Uptime formatting: "Xd Yh Zm" / "Yh Zm" / "Zm"
// dest must be at least 16 bytes. Returns dest for convenience.
inline char*
FormatUptime(char* dest, size_t size, uint32 totalSeconds)
{
	uint32 d = totalSeconds / 86400;
	uint32 h = (totalSeconds % 86400) / 3600;
	uint32 m = (totalSeconds % 3600) / 60;
	if (d > 0)
		snprintf(dest, size, "%ud %uh %um", (unsigned)d, (unsigned)h, (unsigned)m);
	else if (h > 0)
		snprintf(dest, size, "%uh %um", (unsigned)h, (unsigned)m);
	else
		snprintf(dest, size, "%um", (unsigned)m);
	return dest;
}


#endif // UTILS_H
