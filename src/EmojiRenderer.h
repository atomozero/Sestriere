/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * EmojiRenderer.h — Inline colored emoji rendering via Twemoji
 */

#ifndef EMOJIRENDERER_H
#define EMOJIRENDERER_H

#include <Font.h>
#include <Handler.h>
#include <SupportDefs.h>

class BBitmap;
class BView;


class EmojiRenderer {
public:
	// Decode next UTF-8 codepoint, return bytes consumed
	static	uint32		DecodeUTF8(const char* str, int32* bytesConsumed);

	// Check if codepoint is an emoji
	static	bool		IsEmoji(uint32 codepoint);

	// Parse emoji sequence at text, write hex code (e.g. "1f600").
	// Returns byte length of sequence, or 0 if not emoji.
	static	int32		NextEmojiSequence(const char* text,
							char* outHex, size_t hexSize);

	// Get cached emoji bitmap (NULL if not available yet)
	static	BBitmap*	GetBitmap(const char* hexCode);

	// Request async download if not cached, post MSG_EMOJI_LOADED
	static	void		RequestIfNeeded(const char* hexCode,
							BHandler* target);

	// Scan text for emoji and request downloads for all
	static	void		RequestEmoji(const char* text, BHandler* target);

	// Draw text line with inline emoji bitmaps
	static	void		DrawLine(BView* owner, const char* text,
							BPoint pos, float emojiSize);

	// Measure text width accounting for emoji
	static	float		MeasureLine(const BFont* font, const char* text,
							float emojiSize);
};

#endif // EMOJIRENDERER_H
