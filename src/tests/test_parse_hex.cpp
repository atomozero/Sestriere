/*
 * Test: ParseHexPrefix and ParseHexPubKey utilities
 * Verifies hex parsing helpers work correctly and replaced inline loops.
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
// Test 1: ParseHexPrefix correctness
// ============================================================================

static void
TestParseHexPrefix()
{
	uint8 prefix[6];
	memset(prefix, 0, sizeof(prefix));

	assert(ParseHexPrefix(prefix, "aabbccddeeff") == true);
	assert(prefix[0] == 0xAA);
	assert(prefix[1] == 0xBB);
	assert(prefix[2] == 0xCC);
	assert(prefix[3] == 0xDD);
	assert(prefix[4] == 0xEE);
	assert(prefix[5] == 0xFF);

	// Mixed case
	memset(prefix, 0, sizeof(prefix));
	assert(ParseHexPrefix(prefix, "0a1B2c3D4e5F") == true);
	assert(prefix[0] == 0x0A);
	assert(prefix[5] == 0x5F);

	// Zero prefix
	memset(prefix, 0xFF, sizeof(prefix));
	assert(ParseHexPrefix(prefix, "000000000000") == true);
	assert(prefix[0] == 0);
	assert(prefix[5] == 0);

	printf("  PASS: ParseHexPrefix produces correct byte values\n");
}


// ============================================================================
// Test 2: ParseHexPubKey correctness
// ============================================================================

static void
TestParseHexPubKey()
{
	uint8 key[32];
	memset(key, 0, sizeof(key));

	const char* hex =
		"0102030405060708090a0b0c0d0e0f10"
		"1112131415161718191a1b1c1d1e1f20";

	assert(ParseHexPubKey(key, hex) == true);
	assert(key[0] == 0x01);
	assert(key[15] == 0x10);
	assert(key[16] == 0x11);
	assert(key[31] == 0x20);

	printf("  PASS: ParseHexPubKey produces correct 32-byte key\n");
}


// ============================================================================
// Test 3: No inline hex parsing loops remain in DatabaseManager.cpp
// ============================================================================

static void
TestNoInlineHexLoops()
{
	FILE* fp = OpenSource("DatabaseManager.cpp");
	assert(fp != NULL);

	char line[1024];
	int inlineLoopCount = 0;
	int parseHexCount = 0;

	while (fgets(line, sizeof(line), fp)) {
		// Count remaining inline sscanf(%2x) hex parsing loops
		if (strstr(line, "sscanf") != NULL && strstr(line, "%2x") != NULL)
			inlineLoopCount++;
		// Count ParseHexPrefix/ParseHexPubKey usage
		if (strstr(line, "ParseHexPrefix(") != NULL
			|| strstr(line, "ParseHexPubKey(") != NULL)
			parseHexCount++;
	}
	fclose(fp);

	printf("    Inline hex loops: %d, ParseHex* calls: %d\n",
		inlineLoopCount, parseHexCount);

	assert(inlineLoopCount == 0 && "Inline hex parsing loops should be replaced");
	assert(parseHexCount >= 5 && "Expected at least 5 ParseHex* calls");

	printf("  PASS: All hex parsing loops replaced with ParseHex* utilities\n");
}


// ============================================================================
// Test 4: ParseHexPrefix defined in Utils.h
// ============================================================================

static void
TestDefinedInUtils()
{
	FILE* fp = OpenSource("Utils.h");
	assert(fp != NULL);

	char line[1024];
	bool foundPrefix = false;
	bool foundPubKey = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "ParseHexPrefix(") != NULL)
			foundPrefix = true;
		if (strstr(line, "ParseHexPubKey(") != NULL)
			foundPubKey = true;
	}
	fclose(fp);

	assert(foundPrefix && "ParseHexPrefix should be in Utils.h");
	assert(foundPubKey && "ParseHexPubKey should be in Utils.h");

	printf("  PASS: ParseHexPrefix and ParseHexPubKey defined in Utils.h\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== ParseHex Utility Tests ===\n\n");

	TestParseHexPrefix();
	TestParseHexPubKey();
	TestNoInlineHexLoops();
	TestDefinedInUtils();

	printf("\nAll 4 tests passed.\n");
	return 0;
}
