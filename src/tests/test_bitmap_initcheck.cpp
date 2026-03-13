/*
 * test_bitmap_initcheck.cpp — Verify BBitmap InitCheck guards
 * in ChatView::InitiateDrag() for both copy and thumbnail bitmaps.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int gPassed = 0;
static int gFailed = 0;

static void Check(bool cond, const char* name) {
	if (cond) {
		printf("  PASS: %s\n", name);
		gPassed++;
	} else {
		printf("  FAIL: %s\n", name);
		gFailed++;
	}
}

static bool FileContainsBetween(const char* path,
	const char* startMarker, const char* endMarker, const char* needle)
{
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	const char* start = strstr(buf, startMarker);
	if (!start) { free(buf); return false; }
	const char* end = strstr(start + strlen(startMarker), endMarker);
	if (!end) { free(buf); return false; }

	size_t regionLen = end - start;
	char* region = (char*)malloc(regionLen + 1);
	memcpy(region, start, regionLen);
	region[regionLen] = 0;
	bool found = strstr(region, needle) != NULL;
	free(region);
	free(buf);
	return found;
}

static int CountOccurrences(const char* path, const char* startMarker,
	const char* endMarker, const char* needle)
{
	FILE* f = fopen(path, "r");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	const char* start = strstr(buf, startMarker);
	if (!start) { free(buf); return 0; }
	const char* end = strstr(start + strlen(startMarker), endMarker);
	if (!end) { free(buf); return 0; }

	int count = 0;
	const char* p = start;
	size_t needleLen = strlen(needle);
	while (p < end) {
		p = strstr(p, needle);
		if (p == NULL || p >= end) break;
		count++;
		p += needleLen;
	}
	free(buf);
	return count;
}


int main() {
	printf("=== BBitmap InitCheck Guards Test ===\n\n");

	// Test: copy bitmap has InitCheck guard
	Check(FileContainsBetween(
		"../ChatView.cpp",
		"::InitiateDrag",
		"BBitmapStream stream",
		"copy->InitCheck()"),
		"Copy bitmap checked with InitCheck() before BBitmapStream");

	// Test: copy is deleted on InitCheck failure (before stream takes ownership)
	Check(FileContainsBetween(
		"../ChatView.cpp",
		"copy->InitCheck()",
		"BBitmapStream stream",
		"delete copy"),
		"Copy bitmap deleted on InitCheck failure (before stream ownership)");

	// Test: thumb bitmap has InitCheck guard
	Check(FileContainsBetween(
		"../ChatView.cpp",
		"B_RGBA32, true",
		"BView(thumb->Bounds",
		"thumb->InitCheck()"),
		"Thumb bitmap checked with InitCheck() before use");

	// Test: thumb is deleted on InitCheck failure
	Check(FileContainsBetween(
		"../ChatView.cpp",
		"thumb->InitCheck()",
		"BView(thumb->Bounds",
		"delete thumb"),
		"Thumb bitmap deleted on InitCheck failure");

	// Test: stream ownership documented
	Check(FileContainsBetween(
		"../ChatView.cpp",
		"::InitiateDrag",
		"DragMessage",
		"stream now owns copy"),
		"BBitmapStream ownership documented in comment");

	// Test: two InitCheck calls in InitiateDrag (copy + thumb)
	int count = CountOccurrences(
		"../ChatView.cpp",
		"::InitiateDrag",
		"DragMessage",
		"InitCheck()");
	Check(count >= 2,
		"At least 2 InitCheck() calls in InitiateDrag (copy + thumb)");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
