/*
 * test_mapview_settings_dir.cpp — Verify MapView uses find_directory()
 * instead of getenv("HOME") for tile cache path.
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


int main() {
	printf("=== MapView Settings Directory Test ===\n\n");

	const char* src = "../MapView.cpp";

	// No getenv("HOME") in MapView
	Check(!FileContains(src, "getenv(\"HOME\")"),
		"No getenv(\"HOME\") in MapView.cpp");

	// Uses find_directory with B_USER_SETTINGS_DIRECTORY
	Check(FileContains(src, "find_directory(B_USER_SETTINGS_DIRECTORY"),
		"Uses find_directory(B_USER_SETTINGS_DIRECTORY)");

	// Includes FindDirectory.h
	Check(FileContains(src, "#include <FindDirectory.h>"),
		"Includes FindDirectory.h header");

	// Includes Path.h
	Check(FileContains(src, "#include <Path.h>"),
		"Includes Path.h header");

	// Appends Sestriere/tiles path
	Check(FileContains(src, "Sestriere/tiles"),
		"Appends Sestriere/tiles to settings path");

	// No other getenv in the file
	Check(!FileContains(src, "getenv"),
		"No getenv calls remaining in MapView.cpp");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
