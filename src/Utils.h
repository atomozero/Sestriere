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
