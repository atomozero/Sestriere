/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ElevationService.cpp — Open-Meteo elevation API client implementation
 */

#include "ElevationService.h"

#include <Message.h>
#include <String.h>

#include <curl/curl.h>
#include <private/shared/Json.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>


static const char* kElevationApiUrl =
	"https://api.open-meteo.com/v1/elevation";
static const int32 kMaxCoordsPerRequest = 100;


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


// Fetch a single batch of up to kMaxCoordsPerRequest elevations
static status_t
_FetchBatch(const double* lats, const double* lons, int32 count,
	double* outElevations)
{
	if (count <= 0 || count > kMaxCoordsPerRequest)
		return B_BAD_VALUE;

	// Build comma-separated lat and lon strings
	BString latStr, lonStr;
	for (int32 i = 0; i < count; i++) {
		if (i > 0) {
			latStr.Append(",");
			lonStr.Append(",");
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "%.6f", lats[i]);
		latStr.Append(buf);
		snprintf(buf, sizeof(buf), "%.6f", lons[i]);
		lonStr.Append(buf);
	}

	BString url;
	url.SetToFormat("%s?latitude=%s&longitude=%s",
		kElevationApiUrl, latStr.String(), lonStr.String());

	// Fetch via curl
	CurlBuffer curlBuf;
	curlBuf.data = (uint8*)malloc(4096);
	curlBuf.size = 0;
	curlBuf.capacity = 4096;

	if (curlBuf.data == NULL)
		return B_NO_MEMORY;

	CURL* curl = curl_easy_init();
	if (curl == NULL) {
		free(curlBuf.data);
		return B_ERROR;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.String());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlBuf);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Sestriere/1.0");

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK || curlBuf.size == 0) {
		free(curlBuf.data);
		return B_ERROR;
	}

	// Null-terminate
	uint8* tmp = (uint8*)realloc(curlBuf.data, curlBuf.size + 1);
	if (tmp == NULL) {
		free(curlBuf.data);
		return B_NO_MEMORY;
	}
	curlBuf.data = tmp;
	curlBuf.data[curlBuf.size] = '\0';

	// Parse JSON: {"elevation": [38.0, 1250.5, ...]}
	BMessage root;
	BString jsonString((const char*)curlBuf.data);
	free(curlBuf.data);

	status_t status = BJson::Parse(jsonString, root);
	if (status != B_OK)
		return status;

	// BJson stores arrays as BMessage with string keys "0","1","2"...
	BMessage elevArray;
	if (root.FindMessage("elevation", &elevArray) != B_OK)
		return B_ERROR;

	for (int32 i = 0; i < count; i++) {
		char indexStr[16];
		snprintf(indexStr, sizeof(indexStr), "%ld", (long)i);

		double val = 0;
		if (elevArray.FindDouble(indexStr, &val) != B_OK) {
			// Try float fallback (BJson may parse integers as float)
			float fval = 0;
			if (elevArray.FindFloat(indexStr, &fval) == B_OK)
				val = (double)fval;
			else
				val = 0;
		}
		outElevations[i] = val;
	}

	return B_OK;
}


status_t
ElevationService::FetchElevations(const double* lats, const double* lons,
	int32 count, double* outElevations)
{
	if (lats == NULL || lons == NULL || outElevations == NULL || count <= 0)
		return B_BAD_VALUE;

	// Batch in chunks of kMaxCoordsPerRequest
	for (int32 offset = 0; offset < count;
		offset += kMaxCoordsPerRequest) {
		int32 batchSize = count - offset;
		if (batchSize > kMaxCoordsPerRequest)
			batchSize = kMaxCoordsPerRequest;

		status_t status = _FetchBatch(
			lats + offset, lons + offset, batchSize,
			outElevations + offset);
		if (status != B_OK)
			return status;
	}

	return B_OK;
}


status_t
ElevationService::BuildTerrainProfile(double lat1, double lon1,
	double lat2, double lon2, int32 numSamples,
	TerrainPoint* outPoints, int32* outCount)
{
	if (outPoints == NULL || outCount == NULL || numSamples < 2)
		return B_BAD_VALUE;

	// Interpolate coordinates along great circle
	double* lats = new(std::nothrow) double[numSamples];
	double* lons = new(std::nothrow) double[numSamples];
	double* elevations = new(std::nothrow) double[numSamples];

	if (lats == NULL || lons == NULL || elevations == NULL) {
		delete[] lats;
		delete[] lons;
		delete[] elevations;
		return B_NO_MEMORY;
	}

	InterpolatePoints(lat1, lon1, lat2, lon2, lats, lons, numSamples);

	// Fetch elevations
	status_t status = FetchElevations(lats, lons, numSamples, elevations);
	if (status != B_OK) {
		delete[] lats;
		delete[] lons;
		delete[] elevations;
		return status;
	}

	// Build terrain points with cumulative distance
	double totalDist = HaversineDistance(lat1, lon1, lat2, lon2);

	for (int32 i = 0; i < numSamples; i++) {
		outPoints[i].latitude = lats[i];
		outPoints[i].longitude = lons[i];
		outPoints[i].elevation = elevations[i];
		outPoints[i].distance = (totalDist * i) / (numSamples - 1);
	}

	*outCount = numSamples;

	delete[] lats;
	delete[] lons;
	delete[] elevations;
	return B_OK;
}
