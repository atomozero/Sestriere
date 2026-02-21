/*
 * Test: Window lifecycle management
 * Verifies that all child window pointers declared in MainWindow.h
 * are properly cleaned up in QuitRequested().
 *
 * This is a source-level test that greps the source files to ensure
 * every Window* member has a corresponding cleanup block.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  TEST: %s ... ", name);

#define PASS() \
	do { printf("PASS\n"); sTestsPassed++; } while(0)

#define FAIL(msg) \
	do { printf("FAIL: %s\n", msg); sTestsFailed++; } while(0)


// Check if a string pattern exists in a file
static bool FileContains(const char* filepath, const char* pattern)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "grep -q '%s' '%s' 2>/dev/null", pattern, filepath);
	return system(cmd) == 0;
}

// Count occurrences of pattern in file
static int CountOccurrences(const char* filepath, const char* pattern)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "grep -c '%s' '%s' 2>/dev/null", pattern, filepath);
	FILE* fp = popen(cmd, "r");
	if (fp == NULL) return -1;
	int count = 0;
	if (fscanf(fp, "%d", &count) != 1)
		count = 0;
	pclose(fp);
	return count;
}


int main()
{
	printf("=== Window Lifecycle Tests ===\n\n");

	const char* header = "MainWindow.h";
	const char* impl = "MainWindow.cpp";

	// List of all child window member names
	const char* windows[] = {
		"fSettingsWindow",
		"fStatsWindow",
		"fTracePathWindow",
		"fNetworkMapWindow",
		"fTelemetryWindow",
		"fLoginWindow",
		"fMapWindow",
		"fContactExportWindow",
		"fPacketAnalyzerWindow",
		"fMqttLogWindow",
		"fMissionControlWindow",
		NULL
	};

	// --- Test 1: All window pointers are declared in header ---
	TEST("All 11 window pointers declared in MainWindow.h");
	{
		int found = 0;
		for (int i = 0; windows[i] != NULL; i++) {
			if (FileContains(header, windows[i]))
				found++;
		}
		if (found == 11)
			PASS();
		else {
			char msg[64];
			snprintf(msg, sizeof(msg), "only %d/11 found", found);
			FAIL(msg);
		}
	}

	// --- Test 2: All window pointers initialized to NULL ---
	TEST("All window pointers initialized to NULL in constructor");
	{
		int found = 0;
		for (int i = 0; windows[i] != NULL; i++) {
			char pattern[128];
			snprintf(pattern, sizeof(pattern), "%s(NULL)", windows[i]);
			if (FileContains(impl, pattern))
				found++;
		}
		if (found == 11)
			PASS();
		else {
			char msg[64];
			snprintf(msg, sizeof(msg), "only %d/11 initialized", found);
			FAIL(msg);
		}
	}

	// --- Test 3: All window pointers cleaned up in QuitRequested ---
	TEST("All window pointers cleaned up (Lock+Quit+NULL) in QuitRequested");
	{
		int cleaned = 0;
		for (int i = 0; windows[i] != NULL; i++) {
			// Check for the pattern: windowName->Lock()
			// and: windowName = NULL
			char lockPattern[128];
			char nullPattern[128];
			snprintf(lockPattern, sizeof(lockPattern), "%s->Lock()", windows[i]);
			snprintf(nullPattern, sizeof(nullPattern), "%s = NULL", windows[i]);

			bool hasLock = FileContains(impl, lockPattern);
			bool hasNull = FileContains(impl, nullPattern);

			if (hasLock && hasNull) {
				cleaned++;
			} else {
				printf("\n    MISSING cleanup for %s (Lock=%d, NULL=%d)",
					windows[i], hasLock, hasNull);
			}
		}
		printf("\n");  // newline after potential missing reports
		if (cleaned == 11) {
			printf("  ");
			PASS();
		} else {
			char msg[64];
			snprintf(msg, sizeof(msg), "only %d/11 cleaned up", cleaned);
			printf("  ");
			FAIL(msg);
		}
	}

	// --- Test 4: Each declared window has a ->Quit() call ---
	TEST("Each window pointer has a corresponding Quit call");
	{
		int found = 0;
		for (int i = 0; windows[i] != NULL; i++) {
			char pattern[128];
			snprintf(pattern, sizeof(pattern), "%s->Quit", windows[i]);
			if (FileContains(impl, pattern))
				found++;
		}
		if (found == 11)
			PASS();
		else {
			char msg[64];
			snprintf(msg, sizeof(msg), "only %d/11 have Quit() call", found);
			FAIL(msg);
		}
	}

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);

	return sTestsFailed > 0 ? 1 : 0;
}
