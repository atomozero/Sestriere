/*
 * test_gif_thread_leak.cpp — Verify GifPickerWindow thread lifecycle:
 * fSearchThread not reset to -1 prematurely, _LoadTrending saves thread_id,
 * kMsgSearchDone resets fSearchThread to allow new searches.
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

static bool FileContains(const char* path, const char* needle) {
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);
	bool found = strstr(buf, needle) != NULL;
	free(buf);
	return found;
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

static int CountOccurrencesBetween(const char* path,
	const char* startMarker, const char* endMarker, const char* needle)
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

	size_t regionLen = end - start;
	char* region = (char*)malloc(regionLen + 1);
	memcpy(region, start, regionLen);
	region[regionLen] = 0;

	int count = 0;
	const char* p = region;
	size_t needleLen = strlen(needle);
	while ((p = strstr(p, needle)) != NULL) {
		count++;
		p += needleLen;
	}
	free(region);
	free(buf);
	return count;
}


int main() {
	printf("=== GifPickerWindow Thread Leak Test ===\n\n");

	const char* src = "../GifPickerWindow.cpp";

	// --- _Search: no premature fSearchThread = -1 ---
	printf("--- _Search thread lifecycle ---\n");

	// fSearchThread should NOT be set to -1 at end of _Search
	int resetCount = CountOccurrencesBetween(src,
		"GifPickerWindow::_Search",
		"GifPickerWindow::_LoadTrending",
		"fSearchThread = -1");
	Check(resetCount == 0,
		"_Search does not reset fSearchThread to -1 after spawn");

	// fSearchThread should be assigned from spawn_thread
	Check(FileContainsBetween(src,
		"GifPickerWindow::_Search",
		"GifPickerWindow::_LoadTrending",
		"fSearchThread = spawn_thread"),
		"_Search assigns fSearchThread from spawn_thread");

	// Guard at top of _Search should check fSearchThread >= 0
	Check(FileContainsBetween(src,
		"GifPickerWindow::_Search",
		"SearchContext",
		"fSearchThread >= 0"),
		"_Search guards against concurrent searches");

	// --- _LoadTrending: saves thread_id ---
	printf("\n--- _LoadTrending thread lifecycle ---\n");

	Check(FileContainsBetween(src,
		"GifPickerWindow::_LoadTrending",
		"_SearchThread",
		"fSearchThread = spawn_thread"),
		"_LoadTrending saves thread_id to fSearchThread");

	// No local tid variable (should use fSearchThread directly)
	Check(!FileContainsBetween(src,
		"GifPickerWindow::_LoadTrending",
		"_SearchThread",
		"thread_id tid"),
		"_LoadTrending does not use local tid variable");

	// --- kMsgSearchDone resets fSearchThread ---
	printf("\n--- kMsgSearchDone handler ---\n");

	Check(FileContainsBetween(src,
		"kMsgSearchDone:",
		"FindInt32",
		"fSearchThread = -1"),
		"kMsgSearchDone resets fSearchThread to -1");

	// --- Destructor waits for thread ---
	printf("\n--- Destructor ---\n");

	Check(FileContainsBetween(src,
		"~GifPickerWindow()",
		"}",
		"wait_for_thread(fSearchThread"),
		"Destructor waits for fSearchThread if running");

	Check(FileContainsBetween(src,
		"~GifPickerWindow()",
		"}",
		"fSearchThread >= 0"),
		"Destructor checks fSearchThread >= 0 before waiting");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
