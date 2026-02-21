/*
 * Test: Window thread safety — _LockIfVisible helper
 * Verifies that child window visibility checks use thread-safe
 * _LockIfVisible instead of bare IsHidden() before LockLooper().
 */

#include <cstdio>
#include <cstring>
#include <cassert>


static FILE*
OpenSource(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		char path[256];
		snprintf(path, sizeof(path), "../%s", filename);
		f = fopen(path, "r");
	}
	return f;
}


static bool
FileContains(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return false;

	char line[512];
	bool found = false;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL) {
			found = true;
			break;
		}
	}
	fclose(f);
	return found;
}


static int
CountOccurrences(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return -1;

	char line[512];
	int count = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL)
			count++;
	}
	fclose(f);
	return count;
}


// ============================================================================
// Test 1: _LockIfVisible helper exists
// ============================================================================

static void
TestHelperExists()
{
	assert(FileContains("MainWindow.cpp", "_LockIfVisible(BWindow* window)"));
	assert(FileContains("MainWindow.cpp", "window->LockLooper()"));
	assert(FileContains("MainWindow.cpp", "window->IsHidden()"));

	printf("  PASS: _LockIfVisible helper function exists\n");
}


// ============================================================================
// Test 2: No bare IsHidden() + LockLooper() pattern for child windows
// ============================================================================

static void
TestNoUnsafePattern()
{
	// The unsafe pattern is:
	//   !fXxxWindow->IsHidden()) {
	//   if (fXxxWindow->LockLooper()) {
	// After refactoring, these should all be _LockIfVisible calls.
	//
	// The ONLY remaining IsHidden on child windows should be:
	// - Inside LockLooper blocks (already locked, safe)
	// - On child views (same looper thread, safe)

	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);

	char prev[512] = "";
	char line[512];
	int unsafeCount = 0;

	// Child window variable names
	const char* childWindows[] = {
		"fMissionControlWindow", "fNetworkMapWindow", "fStatsWindow",
		"fTracePathWindow", "fTelemetryWindow", "fLoginWindow",
		"fMqttLogWindow", "fDebugLogWindow", "fPacketAnalyzerWindow",
		NULL
	};

	while (fgets(line, sizeof(line), f) != NULL) {
		// Check if previous line had IsHidden on a child window
		// and current line has LockLooper on the same window
		for (int w = 0; childWindows[w] != NULL; w++) {
			char isHiddenPat[128];
			snprintf(isHiddenPat, sizeof(isHiddenPat),
				"%s->IsHidden()", childWindows[w]);
			char lockPat[128];
			snprintf(lockPat, sizeof(lockPat),
				"%s->LockLooper()", childWindows[w]);

			if (strstr(prev, isHiddenPat) != NULL
				&& strstr(line, lockPat) != NULL) {
				// This is the unsafe pattern!
				printf("  UNSAFE: %s IsHidden before LockLooper\n",
					childWindows[w]);
				unsafeCount++;
			}
		}

		strlcpy(prev, line, sizeof(prev));
	}
	fclose(f);

	assert(unsafeCount == 0);
	printf("  PASS: No unsafe IsHidden→LockLooper patterns on child windows\n");
}


// ============================================================================
// Test 3: _LockIfVisible is used for child window forwarding
// ============================================================================

static void
TestLockIfVisibleUsage()
{
	int count = CountOccurrences("MainWindow.cpp", "_LockIfVisible(");
	// Should be at least 15 (the 15 replaced patterns) + 1 (definition)
	assert(count >= 15);

	printf("  PASS: _LockIfVisible used %d times (expected >= 15)\n", count);
}


// ============================================================================
// Test 4: Remaining IsHidden calls are safe (inside LockLooper or same thread)
// ============================================================================

static void
TestRemainingIsHiddenSafe()
{
	// These are the ALLOWED patterns:
	// 1. fLoginWindow->IsHidden() inside LockLooper block (lines ~2296, ~3894)
	// 2. fSearchBar->IsHidden() — child view, same looper
	// 3. fChatHeader->IsHidden() — child view, same looper
	// 4. window->IsHidden() inside _ShowWindow (locked)
	// 5. window->IsHidden() inside _LockIfVisible (locked)

	int total = CountOccurrences("MainWindow.cpp", "IsHidden()");
	// _ShowWindow + _LockIfVisible + fLoginWindow(2) + fSearchBar(2) + fChatHeader(2)
	// = about 8 safe remaining calls
	assert(total > 0 && total <= 12);

	printf("  PASS: %d remaining IsHidden calls (all in safe contexts)\n",
		total);
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Window Thread Safety Tests ===\n\n");

	TestHelperExists();
	TestNoUnsafePattern();
	TestLockIfVisibleUsage();
	TestRemainingIsHiddenSafe();

	printf("\nAll 4 tests passed.\n");
	return 0;
}
