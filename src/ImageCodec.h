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
	// Compress an image file to grayscale JPEG suitable for LoRa transmission.
	// Loads any format supported by Translation Kit, resizes to maxDim,
	// converts to grayscale, and compresses as JPEG.
	// Caller must free(*outData) when done.
	static	status_t		CompressImageFile(const char* path,
								uint8** outData, size_t* outSize,
								int32* outWidth, int32* outHeight,
								int32 maxDim = 128, int32 quality = 65);

	// Decompress JPEG data into a BBitmap.
	// Caller owns the returned bitmap.
	static	BBitmap*		DecompressImageData(const uint8* jpegData,
								size_t size);
};

#endif // IMAGECODEC_H
