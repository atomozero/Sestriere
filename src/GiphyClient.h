/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GiphyClient.h — GIPHY API client for GIF search and download
 */

#ifndef GIPHYCLIENT_H
#define GIPHYCLIENT_H

#include <SupportDefs.h>


struct GiphyResult {
	char	id[64];
	char	title[128];
	char	previewUrl[256];
	char	gifUrl[256];
	int32	width;
	int32	height;
};


class GiphyClient {
public:
	static	int32		Search(const char* query, GiphyResult* results,
							int32 maxResults = 20);

	static	int32		Trending(GiphyResult* results,
							int32 maxResults = 20);

	static	status_t	DownloadData(const char* url, uint8** outData,
							size_t* outSize);

	static	void		BuildGifUrl(const char* gifId, char* outUrl,
							size_t urlSize);

	static	bool		IsGifMessage(const char* text);

	static	bool		ExtractGifId(const char* text, char* outId,
							size_t idSize);
};

#endif // GIPHYCLIENT_H
