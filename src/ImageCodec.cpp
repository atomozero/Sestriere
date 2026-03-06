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

	// Create grayscale bitmap in B_RGB32 (R=G=B=Y for each pixel)
	BBitmap* grayscale = new BBitmap(BRect(0, 0, dstW - 1, dstH - 1),
		B_RGB32);
	if (grayscale->InitCheck() != B_OK) {
		delete source;
		delete grayscale;
		return B_NO_MEMORY;
	}

	// Ensure source is B_RGB32 for pixel access
	if (source->ColorSpace() != B_RGB32 && source->ColorSpace() != B_RGBA32) {
		BBitmap* converted = new BBitmap(srcBounds, B_RGB32);
		if (converted->InitCheck() != B_OK) {
			delete source;
			delete grayscale;
			delete converted;
			return B_NO_MEMORY;
		}
		converted->ImportBits(source);
		delete source;
		source = converted;
	}

	// Scale and convert to grayscale using nearest-neighbor sampling
	uint8* srcBits = (uint8*)source->Bits();
	uint8* dstBits = (uint8*)grayscale->Bits();
	int32 srcBPR = source->BytesPerRow();
	int32 dstBPR = grayscale->BytesPerRow();

	for (int32 y = 0; y < dstH; y++) {
		int32 srcY = y * srcH / dstH;
		if (srcY >= srcH) srcY = srcH - 1;
		uint8* srcRow = srcBits + srcY * srcBPR;
		uint8* dstRow = dstBits + y * dstBPR;

		for (int32 x = 0; x < dstW; x++) {
			int32 srcX = x * srcW / dstW;
			if (srcX >= srcW) srcX = srcW - 1;

			uint8* sp = srcRow + srcX * 4;  // B_RGB32: B,G,R,A
			uint8 b = sp[0], g = sp[1], r = sp[2];

			// ITU-R BT.601 luma
			uint8 luma = (uint8)(0.299f * r + 0.587f * g + 0.114f * b);

			uint8* dp = dstRow + x * 4;
			dp[0] = luma;  // B
			dp[1] = luma;  // G
			dp[2] = luma;  // R
			dp[3] = 255;   // A
		}
	}

	delete source;

	// Compress to JPEG using Translation Kit
	BBitmapStream stream(grayscale);  // stream takes ownership of grayscale

	// Find JPEG translator
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	translator_id* translators = NULL;
	int32 numTranslators = 0;
	roster->GetAllTranslators(&translators, &numTranslators);

	translator_id jpegTranslator = 0;
	bool found = false;
	for (int32 i = 0; i < numTranslators; i++) {
		const translation_format* formats = NULL;
		int32 numFormats = 0;
		if (roster->GetOutputFormats(translators[i], &formats, &numFormats)
			== B_OK) {
			for (int32 j = 0; j < numFormats; j++) {
				if (formats[j].type == B_JPEG_FORMAT) {
					jpegTranslator = translators[i];
					found = true;
					break;
				}
			}
		}
		if (found) break;
	}
	delete[] translators;

	if (!found) {
		// stream destructor deletes grayscale
		return B_ERROR;
	}

	// Set JPEG quality via translator settings
	BMessage settings;
	settings.AddInt32("quality", quality);
	roster->MakeConfigurationView(jpegTranslator, &settings, NULL, NULL);

	BMallocIO output;
	status_t status = roster->Translate(jpegTranslator, &stream, NULL,
		&output, B_JPEG_FORMAT);
	if (status != B_OK)
		return status;

	// Copy output data
	size_t jpegSize = output.BufferLength();
	uint8* jpegData = (uint8*)malloc(jpegSize);
	if (jpegData == NULL)
		return B_NO_MEMORY;

	memcpy(jpegData, output.Buffer(), jpegSize);

	*outData = jpegData;
	*outSize = jpegSize;
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
