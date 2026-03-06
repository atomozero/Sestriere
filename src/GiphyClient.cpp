/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GiphyClient.cpp — GIPHY API client implementation using libcurl and BJson
 */

#include "GiphyClient.h"

#include <Message.h>
#include <String.h>

#include <curl/curl.h>
#include <private/shared/Json.h>

#include <cstdlib>
#include <cstring>


static const char* kGiphyApiKey = "sXpGFDGZs0Dv1mmNFvYaGUvYwKX0PWIh";
static const char* kGiphySearchUrl = "https://api.giphy.com/v1/gifs/search";
static const char* kGiphyTrendingUrl = "https://api.giphy.com/v1/gifs/trending";
static const char* kGiphyMediaBase = "https://media.giphy.com/media/";


struct CurlBuffer {
	uint8*	data;
	size_t	size;
	size_t	capacity;
};


static size_t
_CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t totalSize = size * nmemb;
	CurlBuffer* buf = (CurlBuffer*)userp;

	if (buf->size + totalSize >= buf->capacity) {
		size_t newCap = (buf->capacity + totalSize) * 2;
		uint8* newData = (uint8*)realloc(buf->data, newCap);
		if (newData == NULL)
			return 0;
		buf->data = newData;
		buf->capacity = newCap;
	}

	memcpy(buf->data + buf->size, contents, totalSize);
	buf->size += totalSize;
	return totalSize;
}


static int32
_ParseGiphyResults(const char* jsonStr, GiphyResult* results, int32 maxResults)
{
	BMessage root;
	BString jsonString(jsonStr);
	status_t status = BJson::Parse(jsonString, root);
	if (status != B_OK)
		return 0;

	// BJson stores arrays as BMessage with string keys "0","1","2"...
	BMessage dataArray;
	if (root.FindMessage("data", &dataArray) != B_OK)
		return 0;

	int32 count = 0;
	for (int32 i = 0; i < maxResults; i++) {
		char indexStr[16];
		snprintf(indexStr, sizeof(indexStr), "%ld", (long)i);

		BMessage item;
		if (dataArray.FindMessage(indexStr, &item) != B_OK)
			break;

		GiphyResult& r = results[count];
		memset(&r, 0, sizeof(GiphyResult));

		// ID
		const char* idStr = NULL;
		if (item.FindString("id", &idStr) == B_OK)
			strlcpy(r.id, idStr, sizeof(r.id));

		// Title
		const char* titleStr = NULL;
		if (item.FindString("title", &titleStr) == B_OK)
			strlcpy(r.title, titleStr, sizeof(r.title));

		// Images → fixed_width_small_still → url (static preview)
		BMessage images;
		if (item.FindMessage("images", &images) == B_OK) {
			BMessage still;
			if (images.FindMessage("fixed_width_small_still", &still)
				== B_OK) {
				const char* url = NULL;
				if (still.FindString("url", &url) == B_OK)
					strlcpy(r.previewUrl, url, sizeof(r.previewUrl));
			}

			// Get dimensions from fixed_width
			BMessage fixedWidth;
			if (images.FindMessage("fixed_width", &fixedWidth) == B_OK) {
				const char* wStr = NULL;
				const char* hStr = NULL;
				if (fixedWidth.FindString("width", &wStr) == B_OK)
					r.width = atoi(wStr);
				if (fixedWidth.FindString("height", &hStr) == B_OK)
					r.height = atoi(hStr);
			}
		}

		// Build GIF URL from ID
		GiphyClient::BuildGifUrl(r.id, r.gifUrl, sizeof(r.gifUrl));

		count++;
	}

	return count;
}


static int32
_FetchAndParse(const char* url, GiphyResult* results, int32 maxResults)
{
	CurlBuffer buf;
	buf.data = (uint8*)malloc(4096);
	buf.size = 0;
	buf.capacity = 4096;

	if (buf.data == NULL)
		return 0;

	CURL* curl = curl_easy_init();
	if (curl == NULL) {
		free(buf.data);
		return 0;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Sestriere/1.0");

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK || buf.size == 0) {
		free(buf.data);
		return 0;
	}

	// Null-terminate for JSON parsing
	uint8* tmp = (uint8*)realloc(buf.data, buf.size + 1);
	if (tmp == NULL) {
		free(buf.data);
		return 0;
	}
	buf.data = tmp;
	buf.data[buf.size] = '\0';

	int32 count = _ParseGiphyResults((const char*)buf.data, results, maxResults);
	free(buf.data);
	return count;
}


int32
GiphyClient::Search(const char* query, GiphyResult* results, int32 maxResults)
{
	if (query == NULL || results == NULL || maxResults <= 0)
		return 0;

	// URL-encode the query
	CURL* curl = curl_easy_init();
	if (curl == NULL)
		return 0;

	char* escaped = curl_easy_escape(curl, query, 0);
	curl_easy_cleanup(curl);

	if (escaped == NULL)
		return 0;

	BString url;
	url.SetToFormat("%s?api_key=%s&q=%s&limit=%ld&rating=g",
		kGiphySearchUrl, kGiphyApiKey, escaped, (long)maxResults);

	curl_free(escaped);

	return _FetchAndParse(url.String(), results, maxResults);
}


int32
GiphyClient::Trending(GiphyResult* results, int32 maxResults)
{
	if (results == NULL || maxResults <= 0)
		return 0;

	BString url;
	url.SetToFormat("%s?api_key=%s&limit=%ld&rating=g",
		kGiphyTrendingUrl, kGiphyApiKey, (long)maxResults);

	return _FetchAndParse(url.String(), results, maxResults);
}


status_t
GiphyClient::DownloadData(const char* url, uint8** outData, size_t* outSize)
{
	if (url == NULL || outData == NULL || outSize == NULL)
		return B_BAD_VALUE;

	CurlBuffer buf;
	buf.data = (uint8*)malloc(65536);
	buf.size = 0;
	buf.capacity = 65536;

	if (buf.data == NULL)
		return B_NO_MEMORY;

	CURL* curl = curl_easy_init();
	if (curl == NULL) {
		free(buf.data);
		return B_ERROR;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Sestriere/1.0");

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		free(buf.data);
		return B_ERROR;
	}

	*outData = buf.data;
	*outSize = buf.size;
	return B_OK;
}


void
GiphyClient::BuildGifUrl(const char* gifId, char* outUrl, size_t urlSize)
{
	snprintf(outUrl, urlSize, "%s%s/giphy.gif", kGiphyMediaBase, gifId);
}


bool
GiphyClient::IsGifMessage(const char* text)
{
	if (text == NULL || text[0] != 'g' || text[1] != ':')
		return false;

	const char* p = text + 2;
	if (*p == '\0')
		return false;

	while (*p != '\0') {
		char c = *p;
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return false;
		p++;
	}
	return true;
}


bool
GiphyClient::ExtractGifId(const char* text, char* outId, size_t idSize)
{
	if (!IsGifMessage(text))
		return false;

	strlcpy(outId, text + 2, idSize);
	return true;
}
