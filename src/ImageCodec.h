/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ImageCodec.h — Image compression/decompression for LoRa image sharing
 */

#ifndef IMAGECODEC_H
#define IMAGECODEC_H

#include <Bitmap.h>
#include <SupportDefs.h>


class ImageCodec {
public:
	// Compress an image file to color WebP suitable for LoRa transmission.
	// Loads any format supported by Translation Kit, resizes to maxDim,
	// and compresses as WebP.
	// Caller must free(*outData) when done.
	static	status_t		CompressImageFile(const char* path,
								uint8** outData, size_t* outSize,
								int32* outWidth, int32* outHeight,
								int32 maxDim = 192, int32 quality = 50);

	// Decompress image data (JPEG, WebP, etc.) into a BBitmap.
	// Caller owns the returned bitmap.
	static	BBitmap*		DecompressImageData(const uint8* jpegData,
								size_t size);

	// Decompress GIF into array of frames.
	// outFrames and outDurations allocated with new[] — caller deletes.
	// Each BBitmap* in outFrames must be deleted by caller.
	static	status_t		DecompressGifFrames(const uint8* gifData,
								size_t size, BBitmap*** outFrames,
								uint32** outDurations,
								int32* outFrameCount,
								int32 maxFrames = 32);
};

#endif // IMAGECODEC_H
