/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Community.cpp — Community HMAC-SHA256 PSK derivation and JSON parsing
 */

#include "Community.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Minimal SHA-256 implementation (same as used in MainWindow for channel PSK)
// For HMAC-SHA256 we need the raw hash function.

static const uint32 kSha256K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};


static inline uint32
RightRotate(uint32 value, uint32 count)
{
	return (value >> count) | (value << (32 - count));
}


static void
Sha256Block(const uint8* block, uint32* state)
{
	uint32 w[64];
	for (int i = 0; i < 16; i++) {
		w[i] = ((uint32)block[i * 4] << 24)
			| ((uint32)block[i * 4 + 1] << 16)
			| ((uint32)block[i * 4 + 2] << 8)
			| (uint32)block[i * 4 + 3];
	}
	for (int i = 16; i < 64; i++) {
		uint32 s0 = RightRotate(w[i - 15], 7)
			^ RightRotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32 s1 = RightRotate(w[i - 2], 17)
			^ RightRotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	uint32 a = state[0], b = state[1], c = state[2], d = state[3];
	uint32 e = state[4], f = state[5], g = state[6], h = state[7];

	for (int i = 0; i < 64; i++) {
		uint32 S1 = RightRotate(e, 6) ^ RightRotate(e, 11)
			^ RightRotate(e, 25);
		uint32 ch = (e & f) ^ (~e & g);
		uint32 temp1 = h + S1 + ch + kSha256K[i] + w[i];
		uint32 S0 = RightRotate(a, 2) ^ RightRotate(a, 13)
			^ RightRotate(a, 22);
		uint32 maj = (a & b) ^ (a & c) ^ (b & c);
		uint32 temp2 = S0 + maj;

		h = g; g = f; f = e; e = d + temp1;
		d = c; c = b; b = a; a = temp1 + temp2;
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}


static void
Sha256(const uint8* data, size_t len, uint8* outHash)
{
	uint32 state[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};

	size_t totalLen = len;
	size_t offset = 0;

	while (offset + 64 <= len) {
		Sha256Block(data + offset, state);
		offset += 64;
	}

	// Padding
	uint8 block[64];
	size_t remaining = len - offset;
	memcpy(block, data + offset, remaining);
	block[remaining] = 0x80;
	remaining++;

	if (remaining > 56) {
		memset(block + remaining, 0, 64 - remaining);
		Sha256Block(block, state);
		memset(block, 0, 56);
	} else {
		memset(block + remaining, 0, 56 - remaining);
	}

	uint64 bitLen = (uint64)totalLen * 8;
	for (int i = 7; i >= 0; i--)
		block[56 + (7 - i)] = (uint8)(bitLen >> (i * 8));
	Sha256Block(block, state);

	for (int i = 0; i < 8; i++) {
		outHash[i * 4] = (uint8)(state[i] >> 24);
		outHash[i * 4 + 1] = (uint8)(state[i] >> 16);
		outHash[i * 4 + 2] = (uint8)(state[i] >> 8);
		outHash[i * 4 + 3] = (uint8)state[i];
	}
}


static void
HmacSha256(const uint8* key, size_t keyLen, const uint8* data,
	size_t dataLen, uint8* outHash)
{
	uint8 keyBlock[64];
	memset(keyBlock, 0, sizeof(keyBlock));

	if (keyLen > 64) {
		Sha256(key, keyLen, keyBlock);
	} else {
		memcpy(keyBlock, key, keyLen);
	}

	// Inner hash: SHA256((key XOR ipad) || data)
	uint8 innerBuf[64 + 256];
	for (int i = 0; i < 64; i++)
		innerBuf[i] = keyBlock[i] ^ 0x36;
	memcpy(innerBuf + 64, data, dataLen);

	uint8 innerHash[32];
	Sha256(innerBuf, 64 + dataLen, innerHash);

	// Outer hash: SHA256((key XOR opad) || innerHash)
	uint8 outerBuf[64 + 32];
	for (int i = 0; i < 64; i++)
		outerBuf[i] = keyBlock[i] ^ 0x5c;
	memcpy(outerBuf + 64, innerHash, 32);

	Sha256(outerBuf, 96, outHash);
}


// Base64url decode (no padding)
static int
Base64UrlDecode(const char* input, size_t inputLen, uint8* output,
	size_t maxOut)
{
	static const int kDecTable[128] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
		52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
		41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
	};

	size_t outIdx = 0;
	uint32 accum = 0;
	int bits = 0;

	for (size_t i = 0; i < inputLen; i++) {
		uint8 ch = (uint8)input[i];
		if (ch >= 128)
			return -1;
		int val = kDecTable[ch];
		if (val < 0) {
			if (ch == '=')
				continue;
			return -1;
		}
		accum = (accum << 6) | (uint32)val;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (outIdx >= maxOut)
				return -1;
			output[outIdx++] = (uint8)(accum >> bits);
		}
	}
	return (int)outIdx;
}


// Base64url encode (no padding)
static int
Base64UrlEncode(const uint8* input, size_t inputLen, char* output,
	size_t maxOut)
{
	static const char kEncTable[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		"0123456789-_";

	size_t outIdx = 0;
	for (size_t i = 0; i < inputLen; i += 3) {
		uint32 n = (uint32)input[i] << 16;
		if (i + 1 < inputLen) n |= (uint32)input[i + 1] << 8;
		if (i + 2 < inputLen) n |= (uint32)input[i + 2];

		if (outIdx + 1 >= maxOut) return -1;
		output[outIdx++] = kEncTable[(n >> 18) & 0x3F];
		output[outIdx++] = kEncTable[(n >> 12) & 0x3F];
		if (i + 1 < inputLen) {
			if (outIdx >= maxOut) return -1;
			output[outIdx++] = kEncTable[(n >> 6) & 0x3F];
		}
		if (i + 2 < inputLen) {
			if (outIdx >= maxOut) return -1;
			output[outIdx++] = kEncTable[n & 0x3F];
		}
	}
	if (outIdx >= maxOut) return -1;
	output[outIdx] = '\0';
	return (int)outIdx;
}


void
DeriveCommunityPublicPsk(const uint8* secret, uint8* outPsk)
{
	const char* label = "channel:v1:__public__";
	uint8 hmac[32];
	HmacSha256(secret, 32, (const uint8*)label, strlen(label), hmac);
	memcpy(outPsk, hmac, 16);
}


void
DeriveCommunityHashtagPsk(const uint8* secret, const char* hashtag,
	uint8* outPsk)
{
	// Normalize: strip leading #, lowercase, trim
	char normalized[64];
	const char* src = hashtag;
	if (src[0] == '#')
		src++;
	size_t len = strlen(src);
	if (len > 63)
		len = 63;
	for (size_t i = 0; i < len; i++)
		normalized[i] = (char)tolower((unsigned char)src[i]);
	normalized[len] = '\0';

	// Trim trailing whitespace
	while (len > 0 && normalized[len - 1] == ' ')
		normalized[--len] = '\0';

	char label[128];
	snprintf(label, sizeof(label), "channel:v1:%s", normalized);

	uint8 hmac[32];
	HmacSha256(secret, 32, (const uint8*)label, strlen(label), hmac);
	memcpy(outPsk, hmac, 16);
}


void
ComputeCommunityId(const uint8* secret, char* outHex)
{
	const char* prefix = "community:v1";
	size_t prefixLen = strlen(prefix);

	uint8 data[44];  // 12 + 32
	memcpy(data, prefix, prefixLen);
	memcpy(data + prefixLen, secret, 32);

	uint8 hash[32];
	Sha256(data, prefixLen + 32, hash);

	for (int i = 0; i < 32; i++)
		snprintf(outHex + i * 2, 3, "%02x", hash[i]);
	outHex[64] = '\0';
}


bool
ParseCommunityJson(const char* json, CommunityInfo* outInfo)
{
	if (json == NULL || outInfo == NULL)
		return false;

	// Simple JSON parsing for {"v":1,"type":"meshcore_community","name":"...","k":"..."}
	if (strstr(json, "\"meshcore_community\"") == NULL)
		return false;
	if (strstr(json, "\"v\":1") == NULL
		&& strstr(json, "\"v\": 1") == NULL)
		return false;

	// Extract name
	const char* nameKey = strstr(json, "\"name\"");
	if (nameKey == NULL)
		return false;
	const char* nameVal = strchr(nameKey + 6, '"');
	if (nameVal == NULL)
		return false;
	nameVal++;  // skip opening quote
	const char* nameEnd = strchr(nameVal, '"');
	if (nameEnd == NULL)
		return false;
	size_t nameLen = nameEnd - nameVal;
	if (nameLen >= sizeof(outInfo->name))
		nameLen = sizeof(outInfo->name) - 1;
	memcpy(outInfo->name, nameVal, nameLen);
	outInfo->name[nameLen] = '\0';

	// Extract k (base64url secret)
	const char* kKey = strstr(json, "\"k\"");
	if (kKey == NULL)
		return false;
	const char* kVal = strchr(kKey + 3, '"');
	if (kVal == NULL)
		return false;
	kVal++;
	const char* kEnd = strchr(kVal, '"');
	if (kEnd == NULL)
		return false;
	size_t kLen = kEnd - kVal;

	int decoded = Base64UrlDecode(kVal, kLen, outInfo->secret, 32);
	if (decoded != 32)
		return false;

	ComputeCommunityId(outInfo->secret, outInfo->communityId);
	return true;
}


bool
FormatCommunityJson(const CommunityInfo* info, char* outJson, size_t outSize)
{
	if (info == NULL || outJson == NULL || outSize < 128)
		return false;

	char b64Secret[64];
	int b64Len = Base64UrlEncode(info->secret, 32, b64Secret,
		sizeof(b64Secret));
	if (b64Len < 0)
		return false;

	snprintf(outJson, outSize,
		"{\"v\":1,\"type\":\"meshcore_community\","
		"\"name\":\"%s\",\"k\":\"%s\"}",
		info->name, b64Secret);
	return true;
}
