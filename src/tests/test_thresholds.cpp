/*
 * Test: Named threshold constants
 * Verifies battery, RSSI, and SNR thresholds are defined and used.
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


static bool
FileContains(const char* filename, const char* pattern)
{
	return CountOccurrences(filename, pattern) > 0;
}


// ============================================================================
// Test 1: Threshold constants defined in Constants.h
// ============================================================================

static void
TestThresholdsDefined()
{
	assert(FileContains("Constants.h", "kBattGoodMv"));
	assert(FileContains("Constants.h", "kBattFairMv"));
	assert(FileContains("Constants.h", "kBattLowMv"));
	assert(FileContains("Constants.h", "kRssiGood"));
	assert(FileContains("Constants.h", "kRssiFair"));
	assert(FileContains("Constants.h", "kRssiPoor"));
	assert(FileContains("Constants.h", "kSnrExcellent"));
	assert(FileContains("Constants.h", "kSnrGood"));
	assert(FileContains("Constants.h", "kSnrFair"));
	assert(FileContains("Constants.h", "kSnrPoor"));

	printf("  PASS: All threshold constants defined in Constants.h\n");
}


// ============================================================================
// Test 2: No magic battery numbers in color functions
// ============================================================================

static void
TestNoBatteryMagicNumbers()
{
	const char* files[] = {
		"MissionControlWindow.cpp",
		"TopBarView.cpp",
	};

	for (int i = 0; i < 2; i++) {
		// Should not have inline 3900/3600/3400 in battery comparison context
		// (3600 may appear in time calculations, so check for "mv >= 3900" pattern)
		assert(!FileContains(files[i], "mv >= 3900"));
		assert(!FileContains(files[i], "mv >= 3600"));
		assert(!FileContains(files[i], "mv >= 3400"));
		assert(!FileContains(files[i], "Mv < 3400"));
	}

	printf("  PASS: No magic battery numbers in MissionControl/TopBarView\n");
}


// ============================================================================
// Test 3: No magic RSSI numbers in color functions
// ============================================================================

static void
TestNoRssiMagicNumbers()
{
	const char* files[] = {
		"MissionControlWindow.cpp",
		"TopBarView.cpp",
	};

	for (int i = 0; i < 2; i++) {
		assert(!FileContains(files[i], "rssi >= -60)"));
		assert(!FileContains(files[i], "rssi >= -80)"));
		assert(!FileContains(files[i], "rssi >= -90)"));
	}

	printf("  PASS: No magic RSSI numbers in MissionControl/TopBarView\n");
}


// ============================================================================
// Test 4: Named constants used in color functions
// ============================================================================

static void
TestConstantsUsed()
{
	// Both files should use the named constants
	assert(FileContains("MissionControlWindow.cpp", "kBattGoodMv"));
	assert(FileContains("MissionControlWindow.cpp", "kRssiGood"));
	assert(FileContains("MissionControlWindow.cpp", "kSnrExcellent"));
	assert(FileContains("TopBarView.cpp", "kBattGoodMv"));
	assert(FileContains("TopBarView.cpp", "kRssiGood"));

	printf("  PASS: Named threshold constants used in color functions\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Threshold Constants Tests ===\n\n");

	TestThresholdsDefined();
	TestNoBatteryMagicNumbers();
	TestNoRssiMagicNumbers();
	TestConstantsUsed();

	printf("\nAll 4 tests passed.\n");
	return 0;
}
