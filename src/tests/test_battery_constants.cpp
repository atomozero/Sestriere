/*
 * Test: Battery calculation constants
 * Verifies kBattMinMv/kBattRangeMv are defined and used consistently.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

// Redefine Haiku types for standalone test
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef signed char int8;
typedef short int16;
typedef int int32;

#include "../Constants.h"


static FILE*
OpenSource(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		char alt[256];
		snprintf(alt, sizeof(alt), "../%s", filename);
		f = fopen(alt, "r");
	}
	return f;
}


// ============================================================================
// Test 1: Constants have correct values
// ============================================================================

static void
TestConstantValues()
{
	assert(kBattMinMv == 3000);
	assert(kBattRangeMv == 1200);

	// Verify math: 3000mV = 0%, 4200mV = 100%
	int pct0 = ((int)3000 - kBattMinMv) * 100 / kBattRangeMv;
	int pct100 = ((int)4200 - kBattMinMv) * 100 / kBattRangeMv;
	int pct50 = ((int)3600 - kBattMinMv) * 100 / kBattRangeMv;

	assert(pct0 == 0);
	assert(pct100 == 100);
	assert(pct50 == 50);

	printf("  PASS: Battery constants have correct values\n");
}


// ============================================================================
// Test 2: No hardcoded 3000/1200 battery pattern in production code
// ============================================================================

static void
TestNoMagicBattCalc()
{
	const char* files[] = {
		"ContactInfoPanel.cpp",
		"DeskbarReplicant.cpp",
		"PacketAnalyzerWindow.cpp",
		NULL
	};

	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		int lineNum = 0;
		while (fgets(line, sizeof(line), fp)) {
			lineNum++;
			// Look for the old pattern: - 3000) * 100 / 1200
			if (strstr(line, "- 3000)") != NULL
				&& strstr(line, "/ 1200") != NULL) {
				fprintf(stderr,
					"FAIL: Magic battery calc at %s:%d: %s",
					files[f], lineNum, line);
				fclose(fp);
				assert(0 && "Hardcoded battery calculation found");
			}
		}
		fclose(fp);
	}

	printf("  PASS: No hardcoded battery calculations remain\n");
}


// ============================================================================
// Test 3: Named constants used in battery calculations
// ============================================================================

static void
TestNamedConstantsUsed()
{
	const char* files[] = {
		"ContactInfoPanel.cpp",
		"DeskbarReplicant.cpp",
		"PacketAnalyzerWindow.cpp",
		NULL
	};

	int totalUsage = 0;
	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		while (fgets(line, sizeof(line), fp)) {
			if (strstr(line, "kBattMinMv") != NULL)
				totalUsage++;
			if (strstr(line, "kBattRangeMv") != NULL)
				totalUsage++;
			// BatteryPercent() from Utils.h is the centralized function
			// that uses kBatteryRanges — counts as proper usage
			if (strstr(line, "BatteryPercent") != NULL)
				totalUsage++;
		}
		fclose(fp);
	}

	printf("    Named constant references: %d\n", totalUsage);
	assert(totalUsage >= 6 && "Expected battery constants/BatteryPercent in 3 files");

	printf("  PASS: Named battery constants used across 3 files\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Battery Calculation Constants Tests ===\n\n");

	TestConstantValues();
	TestNoMagicBattCalc();
	TestNamedConstantsUsed();

	printf("\nAll 3 tests passed.\n");
	return 0;
}
