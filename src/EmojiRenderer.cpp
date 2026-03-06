/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * EmojiRenderer.cpp — Inline colored emoji via Twemoji CDN
 */

#include "EmojiRenderer.h"

#include <Autolock.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <Locker.h>
#include <Messenger.h>
#include <String.h>
#include <TranslatorRoster.h>
#include <View.h>

#include <map>
#include <set>
#include <string>

#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "GiphyClient.h"


static const char* kTwemojiBase =
	"https://cdn.jsdelivr.net/gh/jdecked/twemoji@latest/assets/72x72/";
static const char* kEmojiCacheDir =
	"/boot/home/config/settings/Sestriere/emoji_cache";

static BLocker sLock("emoji_cache");
static std::map<std::string, BBitmap*> sCache;
static std::set<std::string> sPending;
static std::set<std::string> sFailed;


uint32
EmojiRenderer::DecodeUTF8(const char* s, int32* bytesConsumed)
{
	if (s == NULL || s[0] == '\0') {
		*bytesConsumed = 0;
		return 0;
	}

	uint8 c = (uint8)s[0];

	if (c < 0x80) {
		*bytesConsumed = 1;
		return c;
	} else if ((c & 0xE0) == 0xC0) {
		*bytesConsumed = 2;
		return ((c & 0x1F) << 6) | (s[1] & 0x3F);
	} else if ((c & 0xF0) == 0xE0) {
		*bytesConsumed = 3;
		return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6)
			| (s[2] & 0x3F);
	} else if ((c & 0xF8) == 0xF0) {
		*bytesConsumed = 4;
		return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12)
			| ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
	}

	*bytesConsumed = 1;
	return 0xFFFD;  // replacement character
}


bool
EmojiRenderer::IsEmoji(uint32 cp)
{
	if (cp >= 0x1F000 && cp <= 0x1FFFF) return true;
	if (cp >= 0x2600 && cp <= 0x27BF) return true;
	if (cp >= 0x2300 && cp <= 0x23FF) return true;
	if (cp >= 0x2190 && cp <= 0x21FF) return true;
	if (cp == 0x200D) return true;  // ZWJ
	if (cp == 0x20E3) return true;  // keycap
	if (cp >= 0xFE00 && cp <= 0xFE0F) return true;  // VS
	if (cp == 0x00A9 || cp == 0x00AE) return true;
	if (cp == 0x203C || cp == 0x2049) return true;
	if (cp == 0x2139) return true;
	if (cp >= 0x2B05 && cp <= 0x2B07) return true;
	if (cp == 0x2B1B || cp == 0x2B1C) return true;
	if (cp == 0x2B50 || cp == 0x2B55) return true;
	if (cp == 0x3030 || cp == 0x303D) return true;
	if (cp == 0x3297 || cp == 0x3299) return true;
	return false;
}


int32
EmojiRenderer::NextEmojiSequence(const char* text, char* outHex,
	size_t hexSize)
{
	if (text == NULL || text[0] == '\0')
		return 0;

	int32 bytes = 0;
	uint32 cp = DecodeUTF8(text, &bytes);

	if (!IsEmoji(cp))
		return 0;

	// Don't treat ZWJ/VS16/keycap alone as emoji
	if (cp == 0x200D || cp == 0x20E3 || (cp >= 0xFE00 && cp <= 0xFE0F))
		return 0;

	char hex[256];
	int32 hexLen = snprintf(hex, sizeof(hex), "%x", cp);
	int32 totalBytes = bytes;

	// Regional indicator pairs (flags)
	if (cp >= 0x1F1E6 && cp <= 0x1F1FF) {
		int32 nb = 0;
		uint32 next = DecodeUTF8(text + totalBytes, &nb);
		if (next >= 0x1F1E6 && next <= 0x1F1FF) {
			hexLen += snprintf(hex + hexLen, sizeof(hex) - hexLen,
				"-%x", next);
			totalBytes += nb;
		}
		strlcpy(outHex, hex, hexSize);
		return totalBytes;
	}

	// Consume modifiers: VS16, ZWJ sequences, skin tones
	while (text[totalBytes] != '\0') {
		int32 nb = 0;
		uint32 next = DecodeUTF8(text + totalBytes, &nb);

		if (next == 0xFE0F) {
			hexLen += snprintf(hex + hexLen, sizeof(hex) - hexLen,
				"-fe0f");
			totalBytes += nb;
		} else if (next == 0x200D) {
			hexLen += snprintf(hex + hexLen, sizeof(hex) - hexLen,
				"-200d");
			totalBytes += nb;
			// Consume the joined emoji
			next = DecodeUTF8(text + totalBytes, &nb);
			if (next != 0) {
				hexLen += snprintf(hex + hexLen,
					sizeof(hex) - hexLen, "-%x", next);
				totalBytes += nb;
			}
		} else if (next >= 0x1F3FB && next <= 0x1F3FF) {
			hexLen += snprintf(hex + hexLen, sizeof(hex) - hexLen,
				"-%x", next);
			totalBytes += nb;
		} else if (next == 0x20E3) {
			hexLen += snprintf(hex + hexLen, sizeof(hex) - hexLen,
				"-20e3");
			totalBytes += nb;
		} else {
			break;
		}
	}

	strlcpy(outHex, hex, hexSize);
	return totalBytes;
}


BBitmap*
EmojiRenderer::GetBitmap(const char* hexCode)
{
	BAutolock lock(sLock);

	auto it = sCache.find(hexCode);
	if (it != sCache.end())
		return it->second;

	lock.Unlock();

	// Try disk cache
	BString path;
	path.SetToFormat("%s/%s.png", kEmojiCacheDir, hexCode);

	BEntry entry(path.String());
	if (!entry.Exists())
		return NULL;

	off_t fileSize;
	entry.GetSize(&fileSize);
	if (fileSize <= 0)
		return NULL;

	uint8* pngData = (uint8*)malloc(fileSize);
	if (pngData == NULL)
		return NULL;

	BFile file(path.String(), B_READ_ONLY);
	if (file.Read(pngData, fileSize) != fileSize) {
		free(pngData);
		return NULL;
	}

	// Decode PNG
	BMemoryIO input(pngData, fileSize);
	BBitmapStream output;
	BBitmap* bitmap = NULL;

	if (BTranslatorRoster::Default()->Translate(&input, NULL, NULL,
		&output, B_TRANSLATOR_BITMAP) == B_OK)
		output.DetachBitmap(&bitmap);

	free(pngData);

	if (bitmap != NULL) {
		BAutolock cacheLock(sLock);
		sCache[hexCode] = bitmap;
	}

	return bitmap;
}


struct EmojiDownloadCtx {
	char		hex[128];
	BHandler*	target;
};


static int32
_EmojiDownloadThread(void* data)
{
	EmojiDownloadCtx* ctx = (EmojiDownloadCtx*)data;

	// Download from Twemoji CDN
	BString url;
	url.SetToFormat("%s%s.png", kTwemojiBase, ctx->hex);

	uint8* pngData = NULL;
	size_t pngSize = 0;
	status_t status = GiphyClient::DownloadData(url.String(),
		&pngData, &pngSize);

	// Fallback: try without -fe0f
	if (status != B_OK || pngSize == 0) {
		free(pngData);
		pngData = NULL;
		pngSize = 0;

		BString altHex(ctx->hex);
		altHex.ReplaceAll("-fe0f", "");
		if (altHex != BString(ctx->hex)) {
			url.SetToFormat("%s%s.png", kTwemojiBase,
				altHex.String());
			GiphyClient::DownloadData(url.String(),
				&pngData, &pngSize);
		}
	}

	BBitmap* bitmap = NULL;
	if (pngData != NULL && pngSize > 0) {
		// Decode PNG
		BMemoryIO input(pngData, pngSize);
		BBitmapStream output;
		if (BTranslatorRoster::Default()->Translate(&input, NULL, NULL,
			&output, B_TRANSLATOR_BITMAP) == B_OK)
			output.DetachBitmap(&bitmap);

		// Save to disk cache
		if (bitmap != NULL) {
			create_directory(kEmojiCacheDir, 0755);
			BString cachePath;
			cachePath.SetToFormat("%s/%s.png", kEmojiCacheDir,
				ctx->hex);
			BFile cacheFile(cachePath.String(),
				B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
			if (cacheFile.InitCheck() == B_OK)
				cacheFile.Write(pngData, pngSize);
		}
	}
	free(pngData);

	{
		BAutolock lock(sLock);
		sPending.erase(ctx->hex);
		if (bitmap != NULL)
			sCache[ctx->hex] = bitmap;
		else
			sFailed.insert(ctx->hex);
	}

	if (bitmap != NULL)
		BMessenger(ctx->target).SendMessage(MSG_EMOJI_LOADED);

	delete ctx;
	return 0;
}


void
EmojiRenderer::RequestIfNeeded(const char* hexCode, BHandler* target)
{
	BAutolock lock(sLock);
	if (sCache.count(hexCode) || sPending.count(hexCode)
		|| sFailed.count(hexCode))
		return;

	sPending.insert(hexCode);
	lock.Unlock();

	EmojiDownloadCtx* ctx = new EmojiDownloadCtx;
	strlcpy(ctx->hex, hexCode, sizeof(ctx->hex));
	ctx->target = target;

	thread_id tid = spawn_thread(_EmojiDownloadThread, "emoji_dl",
		B_LOW_PRIORITY, ctx);
	if (tid >= 0)
		resume_thread(tid);
	else {
		BAutolock failLock(sLock);
		sPending.erase(hexCode);
		delete ctx;
	}
}


void
EmojiRenderer::RequestEmoji(const char* text, BHandler* target)
{
	const char* p = text;
	while (*p != '\0') {
		char hex[128];
		int32 seqLen = NextEmojiSequence(p, hex, sizeof(hex));
		if (seqLen > 0) {
			RequestIfNeeded(hex, target);
			p += seqLen;
		} else {
			int32 bytes = 0;
			DecodeUTF8(p, &bytes);
			if (bytes == 0) bytes = 1;
			p += bytes;
		}
	}
}


void
EmojiRenderer::DrawLine(BView* owner, const char* text, BPoint pos,
	float emojiSize)
{
	float x = pos.x;
	const char* p = text;

	while (*p != '\0') {
		char hex[128];
		int32 seqLen = NextEmojiSequence(p, hex, sizeof(hex));

		if (seqLen > 0) {
			BBitmap* bmp = GetBitmap(hex);
			if (bmp != NULL) {
				float size = emojiSize * 0.85f;
				font_height fh;
				owner->GetFontHeight(&fh);
				float ey = pos.y - fh.ascent
					+ (emojiSize - size) / 2;
				BRect src = bmp->Bounds();
				BRect dst(x, ey, x + size - 1, ey + size - 1);
				owner->SetDrawingMode(B_OP_ALPHA);
				owner->SetBlendingMode(B_PIXEL_ALPHA,
					B_ALPHA_OVERLAY);
				owner->DrawBitmap(bmp, src, dst);
				owner->SetDrawingMode(B_OP_COPY);
				x += size;
			} else {
				// Emoji not yet cached — draw the raw bytes
				BString seg;
				seg.SetTo(p, seqLen);
				owner->DrawString(seg.String(), BPoint(x, pos.y));
				x += owner->StringWidth(seg.String());
			}
			p += seqLen;
		} else {
			// Accumulate non-emoji text
			const char* start = p;
			int32 bytes = 0;
			DecodeUTF8(p, &bytes);
			if (bytes == 0) bytes = 1;
			p += bytes;

			while (*p != '\0') {
				char tempHex[128];
				if (NextEmojiSequence(p, tempHex,
					sizeof(tempHex)) > 0)
					break;
				DecodeUTF8(p, &bytes);
				if (bytes == 0) bytes = 1;
				p += bytes;
			}

			BString seg;
			seg.SetTo(start, p - start);
			owner->DrawString(seg.String(), BPoint(x, pos.y));
			x += owner->StringWidth(seg.String());
		}
	}
}


float
EmojiRenderer::MeasureLine(const BFont* font, const char* text,
	float emojiSize)
{
	float width = 0;
	const char* p = text;

	while (*p != '\0') {
		char hex[128];
		int32 seqLen = NextEmojiSequence(p, hex, sizeof(hex));

		if (seqLen > 0) {
			width += emojiSize * 0.85f;
			p += seqLen;
		} else {
			const char* start = p;
			int32 bytes = 0;
			DecodeUTF8(p, &bytes);
			if (bytes == 0) bytes = 1;
			p += bytes;

			while (*p != '\0') {
				char tempHex[128];
				if (NextEmojiSequence(p, tempHex,
					sizeof(tempHex)) > 0)
					break;
				DecodeUTF8(p, &bytes);
				if (bytes == 0) bytes = 1;
				p += bytes;
			}

			BString seg;
			seg.SetTo(start, p - start);
			width += font->StringWidth(seg.String());
		}
	}

	return width;
}
