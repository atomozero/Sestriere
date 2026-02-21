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


#endif // UTILS_H
