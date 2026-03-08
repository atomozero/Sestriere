/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ImageCodec.cpp — Image compression/decompression for LoRa image sharing
 */

#include "ImageCodec.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <File.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <gif_lib.h>

#include <cstdlib>
#include <cstring>


status_t
ImageCodec::CompressImageFile(const char* path, uint8** outData,
	size_t* outSize, int32* outWidth, int32* outHeight, int32 maxDim,
	int32 quality)
{
	if (path == NULL || outData == NULL || outSize == NULL)
		return B_BAD_VALUE;

	// Load image using Translation Kit (supports PNG, JPEG, BMP, etc.)
	BBitmap* source = BTranslationUtils::GetBitmap(path);
	if (source == NULL)
		return B_ERROR;

	BRect srcBounds = source->Bounds();
	int32 srcW = (int32)(srcBounds.Width() + 1);
	int32 srcH = (int32)(srcBounds.Height() + 1);

	// Calculate scaled dimensions preserving aspect ratio
	int32 dstW, dstH;
	if (srcW >= srcH) {
		dstW = (srcW > maxDim) ? maxDim : srcW;
		dstH = (int32)((float)srcH * dstW / srcW);
		if (dstH < 1) dstH = 1;
	} else {
		dstH = (srcH > maxDim) ? maxDim : srcH;
		dstW = (int32)((float)srcW * dstH / srcH);
		if (dstW < 1) dstW = 1;
	}

	// Create scaled color bitmap in B_RGB32
	BBitmap* scaled = new BBitmap(BRect(0, 0, dstW - 1, dstH - 1),
		B_RGB32);
	if (scaled->InitCheck() != B_OK) {
		delete source;
		delete scaled;
		return B_NO_MEMORY;
	}

	// Ensure source is B_RGB32 for pixel access
	if (source->ColorSpace() != B_RGB32 && source->ColorSpace() != B_RGBA32) {
		BBitmap* converted = new BBitmap(srcBounds, B_RGB32);
		if (converted->InitCheck() != B_OK) {
			delete source;
			delete scaled;
			delete converted;
			return B_NO_MEMORY;
		}
		converted->ImportBits(source);
		delete source;
		source = converted;
	}

	// Scale using nearest-neighbor sampling (preserving color)
	uint8* srcBits = (uint8*)source->Bits();
	uint8* dstBits = (uint8*)scaled->Bits();
	int32 srcBPR = source->BytesPerRow();
	int32 dstBPR = scaled->BytesPerRow();

	for (int32 y = 0; y < dstH; y++) {
		int32 srcY = y * srcH / dstH;
		if (srcY >= srcH) srcY = srcH - 1;
		uint8* srcRow = srcBits + srcY * srcBPR;
		uint8* dstRow = dstBits + y * dstBPR;

		for (int32 x = 0; x < dstW; x++) {
			int32 srcX = x * srcW / dstW;
			if (srcX >= srcW) srcX = srcW - 1;

			uint8* sp = srcRow + srcX * 4;  // B_RGB32: B,G,R,A
			uint8* dp = dstRow + x * 4;
			dp[0] = sp[0];  // B
			dp[1] = sp[1];  // G
			dp[2] = sp[2];  // R
			dp[3] = 255;    // A
		}
	}

	delete source;

	// Compress to WebP using Translation Kit
	BBitmapStream stream(scaled);  // stream takes ownership of scaled

	// Find WebP translator
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	translator_id* translators = NULL;
	int32 numTranslators = 0;
	roster->GetAllTranslators(&translators, &numTranslators);

	translator_id webpTranslator = 0;
	bool found = false;
	for (int32 i = 0; i < numTranslators; i++) {
		const translation_format* formats = NULL;
		int32 numFormats = 0;
		if (roster->GetOutputFormats(translators[i], &formats, &numFormats)
			== B_OK) {
			for (int32 j = 0; j < numFormats; j++) {
				if (formats[j].type == B_WEBP_FORMAT) {
					webpTranslator = translators[i];
					found = true;
					break;
				}
			}
		}
		if (found) break;
	}
	delete[] translators;

	if (!found) {
		// stream destructor deletes scaled
		return B_ERROR;
	}

	// Set WebP quality via translator settings
	BMessage settings;
	settings.AddInt32("quality", quality);
	roster->MakeConfigurationView(webpTranslator, &settings, NULL, NULL);

	BMallocIO output;
	status_t status = roster->Translate(webpTranslator, &stream, NULL,
		&output, B_WEBP_FORMAT);
	if (status != B_OK)
		return status;

	// Copy output data
	size_t webpSize = output.BufferLength();
	uint8* webpData = (uint8*)malloc(webpSize);
	if (webpData == NULL)
		return B_NO_MEMORY;

	memcpy(webpData, output.Buffer(), webpSize);

	*outData = webpData;
	*outSize = webpSize;
	if (outWidth != NULL) *outWidth = dstW;
	if (outHeight != NULL) *outHeight = dstH;

	return B_OK;
}


BBitmap*
ImageCodec::DecompressImageData(const uint8* jpegData, size_t size)
{
	if (jpegData == NULL || size == 0)
		return NULL;

	BMemoryIO input(jpegData, size);
	BBitmap* bitmap = NULL;

	BTranslatorRoster* roster = BTranslatorRoster::Default();
	BBitmapStream output;

	status_t status = roster->Translate(&input, NULL, NULL, &output,
		B_TRANSLATOR_BITMAP);
	if (status != B_OK)
		return NULL;

	output.DetachBitmap(&bitmap);
	return bitmap;
}


// --- GIF frame decoding with giflib ---

struct GifReadContext {
	const uint8*	data;
	size_t			size;
	size_t			offset;
};


static int
_GifReadFunc(GifFileType* gif, GifByteType* buf, int count)
{
	GifReadContext* ctx = (GifReadContext*)gif->UserData;
	size_t avail = ctx->size - ctx->offset;
	size_t toRead = (size_t)count < avail ? (size_t)count : avail;
	memcpy(buf, ctx->data + ctx->offset, toRead);
	ctx->offset += toRead;
	return (int)toRead;
}


status_t
ImageCodec::DecompressGifFrames(const uint8* gifData, size_t size,
	BBitmap*** outFrames, uint32** outDurations, int32* outFrameCount,
	int32 maxFrames)
{
	if (gifData == NULL || size == 0 || outFrames == NULL
		|| outDurations == NULL || outFrameCount == NULL)
		return B_BAD_VALUE;

	*outFrames = NULL;
	*outDurations = NULL;
	*outFrameCount = 0;

	GifReadContext ctx;
	ctx.data = gifData;
	ctx.size = size;
	ctx.offset = 0;

	int error = 0;
	GifFileType* gif = DGifOpen(&ctx, _GifReadFunc, &error);
	if (gif == NULL)
		return B_ERROR;

	if (DGifSlurp(gif) != GIF_OK) {
		DGifCloseFile(gif, &error);
		return B_ERROR;
	}

	int32 frameCount = gif->ImageCount;
	if (frameCount <= 0) {
		DGifCloseFile(gif, &error);
		return B_ERROR;
	}
	if (frameCount > maxFrames)
		frameCount = maxFrames;

	int32 canvasW = gif->SWidth;
	int32 canvasH = gif->SHeight;

	BBitmap** frames = new BBitmap*[frameCount];
	uint32* durations = new uint32[frameCount];
	memset(frames, 0, sizeof(BBitmap*) * frameCount);
	memset(durations, 0, sizeof(uint32) * frameCount);

	// Canvas for frame compositing (persistent across frames)
	uint8* canvas = (uint8*)calloc(canvasW * canvasH * 4, 1);
	if (canvas == NULL) {
		delete[] frames;
		delete[] durations;
		DGifCloseFile(gif, &error);
		return B_NO_MEMORY;
	}

	// Fill canvas with transparent background
	ColorMapObject* globalMap = gif->SColorMap;
	GifColorType bgColor = {0, 0, 0};
	if (globalMap != NULL && gif->SBackGroundColor < globalMap->ColorCount)
		bgColor = globalMap->Colors[gif->SBackGroundColor];

	// Canvas starts fully transparent (calloc zeroed it)

	// Save canvas state for dispose-to-previous
	uint8* prevCanvas = (uint8*)malloc(canvasW * canvasH * 4);
	if (prevCanvas != NULL)
		memcpy(prevCanvas, canvas, canvasW * canvasH * 4);

	int32 validFrames = 0;

	for (int32 i = 0; i < frameCount; i++) {
		SavedImage* si = &gif->SavedImages[i];
		GifImageDesc* desc = &si->ImageDesc;

		int32 fLeft = desc->Left;
		int32 fTop = desc->Top;
		int32 fW = desc->Width;
		int32 fH = desc->Height;

		ColorMapObject* colorMap = desc->ColorMap
			? desc->ColorMap : globalMap;
		if (colorMap == NULL)
			continue;

		// Extract GCE (Graphics Control Extension)
		int32 delay = 100;  // default 100ms
		int32 disposal = 0;
		int32 transIndex = -1;

		for (int32 e = 0; e < si->ExtensionBlockCount; e++) {
			ExtensionBlock* ext = &si->ExtensionBlocks[e];
			if (ext->Function == GRAPHICS_EXT_FUNC_CODE
				&& ext->ByteCount >= 4) {
				uint8 packed = ext->Bytes[0];
				disposal = (packed >> 2) & 7;
				if (packed & 1)
					transIndex = (uint8)ext->Bytes[3];
				int32 d = (uint8)ext->Bytes[1]
					| ((uint8)ext->Bytes[2] << 8);
				delay = d * 10;  // centiseconds → ms
				if (delay == 0)
					delay = 100;
			}
		}

		// Save canvas before rendering (for dispose-to-previous)
		if (disposal == 3 && prevCanvas != NULL)
			memcpy(prevCanvas, canvas, canvasW * canvasH * 4);

		// Render frame pixels onto canvas
		uint8* raster = si->RasterBits;
		for (int32 y = 0; y < fH; y++) {
			int32 cy = fTop + y;
			if (cy < 0 || cy >= canvasH)
				continue;
			for (int32 x = 0; x < fW; x++) {
				int32 cx = fLeft + x;
				if (cx < 0 || cx >= canvasW)
					continue;

				uint8 idx = raster[y * fW + x];
				if (idx == transIndex)
					continue;
				if (idx >= colorMap->ColorCount)
					continue;

				GifColorType* c = &colorMap->Colors[idx];
				int32 off = (cy * canvasW + cx) * 4;
				canvas[off + 0] = c->Blue;
				canvas[off + 1] = c->Green;
				canvas[off + 2] = c->Red;
				canvas[off + 3] = 255;
			}
		}

		// Create BBitmap from canvas (RGBA for transparency support)
		BBitmap* bmp = new BBitmap(BRect(0, 0, canvasW - 1, canvasH - 1),
			B_RGBA32);
		if (bmp->InitCheck() != B_OK) {
			delete bmp;
			continue;
		}

		// Copy canvas to bitmap
		int32 bpr = bmp->BytesPerRow();
		uint8* bits = (uint8*)bmp->Bits();
		for (int32 y = 0; y < canvasH; y++)
			memcpy(bits + y * bpr, canvas + y * canvasW * 4, canvasW * 4);

		frames[validFrames] = bmp;
		durations[validFrames] = (uint32)delay;
		validFrames++;

		// Apply disposal
		if (disposal == 2) {
			// Restore to background (transparent)
			for (int32 y = fTop; y < fTop + fH && y < canvasH; y++) {
				for (int32 x = fLeft; x < fLeft + fW && x < canvasW; x++) {
					int32 off = (y * canvasW + x) * 4;
					canvas[off + 0] = 0;
					canvas[off + 1] = 0;
					canvas[off + 2] = 0;
					canvas[off + 3] = 0;
				}
			}
		} else if (disposal == 3 && prevCanvas != NULL) {
			// Restore to previous
			memcpy(canvas, prevCanvas, canvasW * canvasH * 4);
		}
	}

	free(canvas);
	free(prevCanvas);
	DGifCloseFile(gif, &error);

	if (validFrames == 0) {
		delete[] frames;
		delete[] durations;
		return B_ERROR;
	}

	*outFrames = frames;
	*outDurations = durations;
	*outFrameCount = validFrames;
	return B_OK;
}
