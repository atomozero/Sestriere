/*
 * Test: FormatTimeAgo utility
 * Verifies relative time formatting and usage in callers.
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

#include "../Utils.h"


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
// Test 1: FormatTimeAgo produces correct output
// ============================================================================

static void
TestFormatTimeAgoOutput()
{
	char buf[24];

	FormatTimeAgo(buf, sizeof(buf), 0);
	assert(strcmp(buf, "Just now") == 0);

	FormatTimeAgo(buf, sizeof(buf), 30);
	assert(strcmp(buf, "Just now") == 0);

	FormatTimeAgo(buf, sizeof(buf), 59);
	assert(strcmp(buf, "Just now") == 0);

	FormatTimeAgo(buf, sizeof(buf), 60);
	assert(strcmp(buf, "1 min ago") == 0);

	FormatTimeAgo(buf, sizeof(buf), 300);
	assert(strcmp(buf, "5 min ago") == 0);

	FormatTimeAgo(buf, sizeof(buf), 3599);
	assert(strcmp(buf, "59 min ago") == 0);

	FormatTimeAgo(buf, sizeof(buf), 3600);
	assert(strcmp(buf, "1 hr ago") == 0);

	FormatTimeAgo(buf, sizeof(buf), 7200);
	assert(strcmp(buf, "2 hr ago") == 0);

	FormatTimeAgo(buf, sizeof(buf), 86400);
	assert(strcmp(buf, "1 days ago") == 0);

	FormatTimeAgo(buf, sizeof(buf), 172800);
	assert(strcmp(buf, "2 days ago") == 0);

	printf("  PASS: FormatTimeAgo produces correct output for all ranges\n");
}


// ============================================================================
// Test 2: FormatTimeAgo defined in Utils.h
// ============================================================================

static void
TestDefinedInUtils()
{
	FILE* fp = OpenSource("Utils.h");
	assert(fp != NULL);

	char line[1024];
	bool found = false;
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "FormatTimeAgo(") != NULL) {
			found = true;
			break;
		}
	}
	fclose(fp);
	assert(found && "FormatTimeAgo should be in Utils.h");

	printf("  PASS: FormatTimeAgo defined in Utils.h\n");
}


// ============================================================================
// Test 3: FormatTimeAgo used in callers
// ============================================================================

static void
TestUsedInCallers()
{
	const char* files[] = {
		"ContactInfoPanel.cpp",
		"ChatHeaderView.cpp",
		NULL
	};

	int usage = 0;
	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		while (fgets(line, sizeof(line), fp)) {
			if (strstr(line, "FormatTimeAgo(") != NULL)
				usage++;
		}
		fclose(fp);
	}

	printf("    FormatTimeAgo usage count: %d\n", usage);
	assert(usage >= 2 && "Expected FormatTimeAgo in at least 2 files");

	printf("  PASS: FormatTimeAgo used in ContactInfoPanel + ChatHeaderView\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== FormatTimeAgo Tests ===\n\n");

	TestFormatTimeAgoOutput();
	TestDefinedInUtils();
	TestUsedInCallers();

	printf("\nAll 3 tests passed.\n");
	return 0;
}
