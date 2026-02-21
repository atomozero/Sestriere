/*
 * Test: LoginWindow uses non-blocking close instead of snooze()
 * Verifies no blocking snooze() call in LoginWindow UI thread.
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
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL) {
			fclose(f);
			return true;
		}
	}
	fclose(f);
	return false;
}


// ============================================================================
// Test 1: No snooze() in LoginWindow
// ============================================================================

static void
TestNoSnooze()
{
	assert(!FileContains("LoginWindow.cpp", "snooze("));
	printf("  PASS: No blocking snooze() in LoginWindow\n");
}


// ============================================================================
// Test 2: Uses BMessageRunner for delayed close
// ============================================================================

static void
TestUsesMessageRunner()
{
	assert(FileContains("LoginWindow.cpp", "fCloseRunner"));
	assert(FileContains("LoginWindow.cpp", "kMsgDelayedClose"));
	assert(FileContains("LoginWindow.cpp", "BMessageRunner"));

	printf("  PASS: Uses BMessageRunner for non-blocking delayed close\n");
}


// ============================================================================
// Test 3: fCloseRunner lifecycle (init/delete)
// ============================================================================

static void
TestCloseRunnerLifecycle()
{
	// Must be initialized to NULL
	assert(FileContains("LoginWindow.cpp", "fCloseRunner(NULL)"));

	// Must be deleted in destructor
	assert(FileContains("LoginWindow.cpp", "delete fCloseRunner"));

	// Must be declared in header
	assert(FileContains("LoginWindow.h", "fCloseRunner"));

	printf("  PASS: fCloseRunner has proper lifecycle (init/delete/declared)\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== LoginWindow Snooze Fix Tests ===\n\n");

	TestNoSnooze();
	TestUsesMessageRunner();
	TestCloseRunnerLifecycle();

	printf("\nAll 3 tests passed.\n");
	return 0;
}
