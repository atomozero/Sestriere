/*
 * Test: FormatUptime helper in Utils.h
 * Verifies uptime formatting and usage across files.
 */

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

#include "../Utils.h"


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
// Test 1: FormatUptime produces correct output
// ============================================================================

static void
TestFormatUptimeOutput()
{
	char buf[32];

	// Minutes only
	FormatUptime(buf, sizeof(buf), 300);
	assert(strcmp(buf, "5m") == 0);

	// Hours and minutes
	FormatUptime(buf, sizeof(buf), 3660);
	assert(strcmp(buf, "1h 1m") == 0);

	// Days, hours, minutes
	FormatUptime(buf, sizeof(buf), 90060);
	assert(strcmp(buf, "1d 1h 1m") == 0);

	// Zero
	FormatUptime(buf, sizeof(buf), 0);
	assert(strcmp(buf, "0m") == 0);

	// Large uptime
	FormatUptime(buf, sizeof(buf), 86400 * 7 + 3600 * 3 + 60 * 45);
	assert(strcmp(buf, "7d 3h 45m") == 0);

	printf("  PASS: FormatUptime produces correct output\n");
}


// ============================================================================
// Test 2: FormatUptime defined in Utils.h
// ============================================================================

static void
TestFormatUptimeInUtils()
{
	assert(FileContains("Utils.h", "FormatUptime"));
	assert(FileContains("Utils.h", "86400"));
	assert(FileContains("Utils.h", "3600"));

	printf("  PASS: FormatUptime defined in Utils.h\n");
}


// ============================================================================
// Test 3: FormatUptime used in callers
// ============================================================================

static void
TestFormatUptimeUsed()
{
	assert(FileContains("ContactInfoPanel.cpp", "FormatUptime("));
	assert(FileContains("MissionControlWindow.cpp", "FormatUptime("));

	printf("  PASS: FormatUptime used in ContactInfoPanel + MissionControl\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== FormatUptime Tests ===\n\n");

	TestFormatUptimeOutput();
	TestFormatUptimeInUtils();
	TestFormatUptimeUsed();

	printf("\nAll 3 tests passed.\n");
	return 0;
}
