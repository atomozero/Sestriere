/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Reactions.h — Message reactions (meshcore-open compatible)
 *
 * Wire format: "r:HASH:INDEX"
 *   HASH  = 4 hex chars, lower 16 bits of Dart VM hashCode
 *   INDEX = 2 hex chars, emoji table index
 *
 * Hash input: "$timestamp$senderName$first5" (channel)
 *             "$timestamp$first5" (DM, senderName=NULL)
 */

#ifndef REACTIONS_H
#define REACTIONS_H

#include <stdio.h>
#include <string.h>


// Reaction emoji table (matches meshcore-open EmojiPicker order:
// quickEmojis + smileys + gestures + hearts + objects)
static const char* kReactionEmojis[] = {
	// quickEmojis [0-5]
	"\xF0\x9F\x98\x8D",       // 00: 😍
	"\xF0\x9F\xA4\x94\xF0\x9F\xA4\xB7", // 01: 🤔🤷
	"\xF0\x9F\x98\x82",       // 02: 😂
	"\xF0\x9F\x98\x80",       // 03: 😀
	"\xF0\x9F\x98\x8F",       // 04: 😏
	"\xF0\x9F\x98\xA4",       // 05: 😤
	// smileys [6-72]
	"\xF0\x9F\x98\x80",       // 06: 😀
	"\xF0\x9F\x98\x83",       // 07: 😃
	"\xF0\x9F\x98\x84",       // 08: 😄
	"\xF0\x9F\x98\x81",       // 09: 😁
	"\xF0\x9F\x98\x86",       // 0a: 😆
	"\xF0\x9F\x98\x85",       // 0b: 😅
	"\xF0\x9F\xA4\xA3",       // 0c: 🤣
	"\xF0\x9F\x98\x82",       // 0d: 😂
	"\xF0\x9F\x99\x82",       // 0e: 🙂
	"\xF0\x9F\x99\x83",       // 0f: 🙃
	"\xF0\x9F\x98\x89",       // 10: 😉
	"\xF0\x9F\x98\x8A",       // 11: 😊
	"\xF0\x9F\x98\x87",       // 12: 😇
	"\xF0\x9F\xA5\xB0",       // 13: 🥰
	"\xF0\x9F\x98\x8D",       // 14: 😍
	"\xF0\x9F\x98\x98",       // 15: 😘
	"\xF0\x9F\x98\x97",       // 16: 😗
	"\xF0\x9F\x98\x9A",       // 17: 😚
	"\xF0\x9F\x98\x99",       // 18: 😙
	"\xF0\x9F\xA5\xB2",       // 19: 🥲
	"\xF0\x9F\x98\x8B",       // 1a: 😋
	"\xF0\x9F\x98\x9B",       // 1b: 😛
	"\xF0\x9F\xA4\x93",       // 1c: 🤓
	"\xF0\x9F\xA4\xA8",       // 1d: 🤨
	"\xF0\x9F\x98\x8E",       // 1e: 😎
	"\xF0\x9F\xA5\xB8",       // 1f: 🥸
	"\xF0\x9F\xA4\xA9",       // 20: 🤩
	"\xF0\x9F\xA5\xB3",       // 21: 🥳
	"\xF0\x9F\x98\x8F",       // 22: 😏
	"\xF0\x9F\x98\x92",       // 23: 😒
	"\xF0\x9F\x98\x9E",       // 24: 😞
	"\xF0\x9F\x98\x94",       // 25: 😔
	"\xF0\x9F\x98\x9F",       // 26: 😟
	"\xF0\x9F\xA5\xBA",       // 27: 🥺
	"\xF0\x9F\x98\x95",       // 28: 😕
	"\xF0\x9F\x99\x81",       // 29: 🙁
	"\xE2\x98\xB9\xEF\xB8\x8F", // 2a: ☹️
	"\xF0\x9F\x98\xB2",       // 2b: 😲
	"\xF0\x9F\x98\x96",       // 2c: 😖
	"\xF0\x9F\x98\xA2",       // 2d: 😢
	"\xF0\x9F\x98\xAD",       // 2e: 😭
	"\xF0\x9F\x98\xB1",       // 2f: 😱
	"\xF0\x9F\x98\x96",       // 30: 😖
	"\xF0\x9F\x98\xA3",       // 31: 😣
	"\xF0\x9F\x98\x9E",       // 32: 😞
	"\xF0\x9F\x98\x93",       // 33: 😓
	"\xF0\x9F\x98\xA9",       // 34: 😩
	"\xF0\x9F\x98\xAB",       // 35: 😫
	"\xF0\x9F\xA5\xB1",       // 36: 🥱
	"\xF0\x9F\x98\xA4",       // 37: 😤
	"\xF0\x9F\x98\xA1",       // 38: 😡
	"\xF0\x9F\x98\xA0",       // 39: 😠
	"\xF0\x9F\xA4\xAC",       // 3a: 🤬
	"\xF0\x9F\x98\x88",       // 3b: 😈
	"\xF0\x9F\x91\xBF",       // 3c: 👿
	"\xF0\x9F\x92\x80",       // 3d: 💀
	"\xE2\x98\xA0\xEF\xB8\x8F", // 3e: ☠️
	// gestures [73-101] (subset)
	"\xF0\x9F\x91\x8D",       // 3f: 👍
	"\xF0\x9F\x91\x8E",       // 40: 👎
	"\xF0\x9F\x91\x8B",       // 41: 👋
	"\xF0\x9F\xA4\x9A",       // 42: 🤚
	"\xF0\x9F\x96\x90",       // 43: 🖐
	"\xF0\x9F\x91\x8C",       // 44: 👌
};

static const int kReactionEmojiCount =
	sizeof(kReactionEmojis) / sizeof(kReactionEmojis[0]);


// Dart VM String.hashCode — Jenkins-like hash seeded with string length.
// Dart strings are UTF-16 code units. For the reaction hash inputs
// (timestamp digits + ASCII name + first 5 text chars), this is
// equivalent to iterating bytes since all values are < 128.
inline uint32
DartStringHash(const char* str)
{
	size_t len = strlen(str);
	uint32 hash = (uint32)len;

	for (size_t i = 0; i < len; i++) {
		hash += (uint8)str[i];
		hash += hash << 10;
		hash ^= hash >> 6;
	}

	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;

	return hash;
}


// Compute reaction hash for a message
// For channel messages, senderName is provided
// For DMs, senderName is NULL
inline uint16
ComputeReactionHash(uint32 timestamp, const char* senderName,
	const char* text)
{
	char input[128];
	char first5[6];
	size_t textLen = strlen(text);
	size_t copyLen = textLen < 5 ? textLen : 5;
	memcpy(first5, text, copyLen);
	first5[copyLen] = '\0';

	if (senderName != NULL)
		snprintf(input, sizeof(input), "%u%s%s", timestamp,
			senderName, first5);
	else
		snprintf(input, sizeof(input), "%u%s", timestamp, first5);

	return (uint16)(DartStringHash(input) & 0xFFFF);
}


// Format a reaction message: "r:HASH:INDEX"
inline bool
FormatReaction(char* out, size_t outSize, uint16 hash, uint8 emojiIdx)
{
	if (emojiIdx >= kReactionEmojiCount || outSize < 10)
		return false;
	snprintf(out, outSize, "r:%04x:%02x", hash, emojiIdx);
	return true;
}


// Check if a message is a reaction
inline bool
IsReactionMessage(const char* text)
{
	// Must match: r:XXXX:XX (at least 9 chars, ignore trailing whitespace/null)
	if (text == NULL)
		return false;
	size_t len = strlen(text);
	// Trim trailing whitespace/control chars
	while (len > 0 && (uint8)text[len - 1] <= ' ')
		len--;
	if (len != 9)
		return false;
	if (text[0] != 'r' || text[1] != ':' || text[6] != ':')
		return false;
	// Validate hex chars
	for (int i = 2; i < 6; i++) {
		char c = text[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return false;
	}
	for (int i = 7; i < 9; i++) {
		char c = text[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return false;
	}
	return true;
}


// Parse a reaction message, returns emoji UTF-8 string or NULL
inline const char*
ParseReaction(const char* text, uint16* outHash)
{
	if (!IsReactionMessage(text))
		return NULL;

	unsigned int hash, idx;
	if (sscanf(text + 2, "%4x:%2x", &hash, &idx) != 2)
		return NULL;
	if (idx >= (unsigned int)kReactionEmojiCount)
		return NULL;

	if (outHash != NULL)
		*outHash = (uint16)hash;

	return kReactionEmojis[idx];
}


#endif	// REACTIONS_H
