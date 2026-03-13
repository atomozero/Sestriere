/*
 * test_window_locker.cpp — Verify WindowLocker RAII class exists,
 * _ShowWindow has null check, and WindowLocker is used in MainWindow.
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

static int CountOccurrences(const char* path, const char* needle) {
	FILE* f = fopen(path, "r");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	int count = 0;
	const char* p = buf;
	size_t needleLen = strlen(needle);
	while ((p = strstr(p, needle)) != NULL) {
		count++;
		p += needleLen;
	}
	free(buf);
	return count;
}


int main() {
	printf("=== WindowLocker RAII & _ShowWindow Test ===\n\n");

	// --- WindowLocker class structure ---
	printf("--- WindowLocker class ---\n");

	Check(FileContains(
		"../MainWindow.cpp",
		"class WindowLocker"),
		"WindowLocker class defined in MainWindow.cpp");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"class WindowLocker",
		"};",
		"fWindow->LockLooper()"),
		"Constructor calls LockLooper() on non-null window");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"class WindowLocker",
		"};",
		"fWindow != NULL"),
		"Constructor checks for NULL before locking");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"class WindowLocker",
		"};",
		"~WindowLocker()"),
		"Destructor defined for RAII cleanup");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"~WindowLocker()",
		"};",
		"UnlockLooper()"),
		"Destructor calls UnlockLooper() if locked");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"class WindowLocker",
		"};",
		"bool IsLocked()"),
		"IsLocked() method available for conditional blocks");

	// --- _ShowWindow null check ---
	printf("\n--- _ShowWindow null check ---\n");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"_ShowWindow(BWindow* window)",
		"Activate",
		"window == NULL"),
		"_ShowWindow checks for NULL pointer");

	// --- WindowLocker usage ---
	printf("\n--- WindowLocker usage ---\n");

	int usages = CountOccurrences("../MainWindow.cpp", "WindowLocker ");
	// class definition + at least 5 uses
	Check(usages >= 6,
		"WindowLocker used at least 5 times (plus class definition)");

	Check(FileContains(
		"../MainWindow.cpp",
		"WindowLocker locker(fSettingsWindow)"),
		"WindowLocker used for fSettingsWindow");

	Check(FileContains(
		"../MainWindow.cpp",
		"WindowLocker logLock(fMqttLogWindow)"),
		"WindowLocker used for fMqttLogWindow");

	Check(FileContains(
		"../MainWindow.cpp",
		"WindowLocker mapLock(fNetworkMapWindow)"),
		"WindowLocker used for fNetworkMapWindow");

	Check(FileContains(
		"../MainWindow.cpp",
		"WindowLocker monLock(fSerialMonitorWindow)"),
		"WindowLocker used for fSerialMonitorWindow");

	Check(FileContains(
		"../MainWindow.cpp",
		"WindowLocker teleLock(fTelemetryWindow)"),
		"WindowLocker used for fTelemetryWindow");

	// _ShowWindow uses WindowLocker
	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"_ShowWindow(BWindow* window)",
		"Activate",
		"WindowLocker"),
		"_ShowWindow uses WindowLocker internally");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
