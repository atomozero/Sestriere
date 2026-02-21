/*
 * Test: kContactHexSize buffer constant
 * Verifies the named constant replaces magic char[13] declarations.
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
// Test 1: Constant has correct value
// ============================================================================

static void
TestConstantValue()
{
	assert(kContactHexSize == 13);
	assert(kPubKeyHexSize == 65);

	// Verify: 6 bytes * 2 hex chars + null = 13
	char buf[kContactHexSize];
	assert(sizeof(buf) == 13);

	printf("  PASS: kContactHexSize = 13, kPubKeyHexSize = 65\n");
}


// ============================================================================
// Test 2: No magic char[13] hex buffers in production code
// ============================================================================

static void
TestNoMagicChar13()
{
	const char* files[] = {
		"ContactInfoPanel.cpp",
		"DatabaseManager.cpp",
		"MainWindow.cpp",
		NULL
	};

	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		int lineNum = 0;
		int magic13 = 0;
		while (fgets(line, sizeof(line), fp)) {
			lineNum++;
			// Look for "char xxxHex[13]" or "char hex[13]" patterns
			if ((strstr(line, "Hex[13]") != NULL
				|| strstr(line, "hex[13]") != NULL
				|| strstr(line, "hexPrefix[13]") != NULL)
				&& strstr(line, "//") == NULL) {
				fprintf(stderr, "  Magic char[13] at %s:%d: %s",
					files[f], lineNum, line);
				magic13++;
			}
		}
		fclose(fp);

		if (magic13 > 0) {
			fprintf(stderr,
				"FAIL: %d magic char[13] hex buffers in %s\n",
				magic13, files[f]);
			assert(0 && "Magic char[13] hex buffer found");
		}
	}

	printf("  PASS: No magic char[13] hex buffers remain\n");
}


// ============================================================================
// Test 3: kContactHexSize used across files
// ============================================================================

static void
TestConstantUsed()
{
	const char* files[] = {
		"ContactInfoPanel.cpp",
		"DatabaseManager.cpp",
		"MainWindow.cpp",
		NULL
	};

	int total = 0;
	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		while (fgets(line, sizeof(line), fp)) {
			if (strstr(line, "kContactHexSize") != NULL)
				total++;
		}
		fclose(fp);
	}

	printf("    kContactHexSize usage count: %d\n", total);
	assert(total >= 15 && "Expected 15+ usages of kContactHexSize");

	printf("  PASS: kContactHexSize used %d times across 3 files\n", total);
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Contact Hex Buffer Size Tests ===\n\n");

	TestConstantValue();
	TestNoMagicChar13();
	TestConstantUsed();

	printf("\nAll 3 tests passed.\n");
	return 0;
}
