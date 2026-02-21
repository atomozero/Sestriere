/*
 * Test: MissionControlWindow BMessageRunner lifecycle
 * Verifies that timer runners are stored as member variables
 * and deleted in the destructor.
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


// Search within a specific function body
static bool
FunctionContains(const char* filename, const char* funcSig,
	const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return false;

	char line[512];
	bool inFunc = false;
	int braceDepth = 0;
	bool found = false;

	while (fgets(line, sizeof(line), f) != NULL) {
		if (!inFunc) {
			if (strstr(line, funcSig) != NULL) {
				inFunc = true;
				braceDepth = 0;
			}
			continue;
		}

		for (const char* p = line; *p != '\0'; p++) {
			if (*p == '{') braceDepth++;
			else if (*p == '}') braceDepth--;
		}

		if (strstr(line, pattern) != NULL) {
			found = true;
			break;
		}

		if (inFunc && braceDepth == 0 && strchr(line, '}') != NULL)
			break;
	}

	fclose(f);
	return found;
}


// ============================================================================
// Test 1: Timer member variables declared in header
// ============================================================================

static void
TestTimerMembersDeclared()
{
	const char* header = "MissionControlWindow.h";

	assert(FileContains(header, "fRefreshTimer"));
	assert(FileContains(header, "fPulseTimer"));
	assert(FileContains(header, "fAlertFlashTimer"));
	assert(FileContains(header, "BMessageRunner*"));

	printf("  PASS: Timer member variables declared in header\n");
}


// ============================================================================
// Test 2: Timers stored in constructor (not leaked)
// ============================================================================

static void
TestTimersStoredInConstructor()
{
	const char* cpp = "MissionControlWindow.cpp";

	// Must assign to member variable, not just `new BMessageRunner(...)`
	assert(FunctionContains(cpp, "MissionControlWindow::MissionControlWindow",
		"fRefreshTimer ="));
	assert(FunctionContains(cpp, "MissionControlWindow::MissionControlWindow",
		"fPulseTimer ="));
	assert(FunctionContains(cpp, "MissionControlWindow::MissionControlWindow",
		"fAlertFlashTimer ="));

	printf("  PASS: Timers stored in member variables in constructor\n");
}


// ============================================================================
// Test 3: Timers deleted in destructor
// ============================================================================

static void
TestTimersDeletedInDestructor()
{
	const char* cpp = "MissionControlWindow.cpp";

	assert(FunctionContains(cpp, "MissionControlWindow::~MissionControlWindow",
		"delete fRefreshTimer"));
	assert(FunctionContains(cpp, "MissionControlWindow::~MissionControlWindow",
		"delete fPulseTimer"));
	assert(FunctionContains(cpp, "MissionControlWindow::~MissionControlWindow",
		"delete fAlertFlashTimer"));

	printf("  PASS: Timers deleted in destructor\n");
}


// ============================================================================
// Test 4: No orphan `new BMessageRunner` (all stored)
// ============================================================================

static void
TestNoOrphanRunners()
{
	FILE* f = OpenSource("MissionControlWindow.cpp");
	assert(f != NULL);

	char line[512];
	int orphanCount = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		// Look for lines with `new BMessageRunner` that don't have `=`
		if (strstr(line, "new BMessageRunner") != NULL
			&& strstr(line, "=") == NULL) {
			orphanCount++;
		}
	}
	fclose(f);

	if (orphanCount > 0)
		printf("  FAIL: %d orphan BMessageRunner allocations found\n",
			orphanCount);
	assert(orphanCount == 0);

	printf("  PASS: No orphan BMessageRunner allocations\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== MissionControlWindow BMessageRunner Lifecycle Tests ===\n\n");

	TestTimerMembersDeclared();
	TestTimersStoredInConstructor();
	TestTimersDeletedInDestructor();
	TestNoOrphanRunners();

	printf("\nAll 4 tests passed.\n");
	return 0;
}
