/*
 * Test: FormatContactKey + bounds check on _SelectContact
 * Verifies lowercase hex DB key generation and index validation.
 */

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>

typedef uint8_t uint8;


// Inline copy of FormatContactKey for standalone test
static void
FormatContactKey(char* dest, const uint8* prefix)
{
	for (size_t i = 0; i < 6; i++)
		snprintf(dest + i * 2, 3, "%02x", prefix[i]);
	dest[12] = '\0';
}


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


// ============================================================================
// Test 1: FormatContactKey produces correct lowercase hex
// ============================================================================

static void
TestFormatContactKey()
{
	uint8 prefix[6] = {0xAA, 0xBB, 0xCC, 0x01, 0x23, 0xFF};
	char result[13];
	FormatContactKey(result, prefix);

	assert(strcmp(result, "aabbcc0123ff") == 0);
	assert(strlen(result) == 12);

	// All zeros
	uint8 zeros[6] = {0, 0, 0, 0, 0, 0};
	FormatContactKey(result, zeros);
	assert(strcmp(result, "000000000000") == 0);

	printf("  PASS: FormatContactKey produces correct lowercase hex\n");
}


// ============================================================================
// Test 2: FormatContactKey exists in Utils.h
// ============================================================================

static void
TestFormatContactKeyInUtils()
{
	assert(FileContains("Utils.h", "FormatContactKey"));
	assert(FileContains("Utils.h", "%02x"));

	printf("  PASS: FormatContactKey defined in Utils.h\n");
}


// ============================================================================
// Test 3: MainWindow uses FormatContactKey (not manual loops)
// ============================================================================

static void
TestMainWindowUsesHelper()
{
	int manualCount = 0;
	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);

	char line[512];
	while (fgets(line, sizeof(line), f) != NULL) {
		// Manual pattern: snprintf(XXXHex + i * 2, 3, "%02x",
		if (strstr(line, "Hex + i * 2") != NULL
			&& strstr(line, "%02x") != NULL)
			manualCount++;
		if (strstr(line, "Hex + j * 2") != NULL
			&& strstr(line, "%02x") != NULL)
			manualCount++;
	}
	fclose(f);

	// Only the 32-byte full key loop should remain
	assert(manualCount <= 1);

	assert(FileContains("MainWindow.cpp", "FormatContactKey("));

	printf("  PASS: MainWindow uses FormatContactKey (%d manual loops remain)\n",
		manualCount);
}


// ============================================================================
// Test 4: _SelectContact has bounds check
// ============================================================================

static void
TestSelectContactBoundsCheck()
{
	assert(FileContains("MainWindow.cpp",
		"if (index < 0 || index >= fContactList->CountItems())"));

	printf("  PASS: _SelectContact has bounds validation\n");
}


// ============================================================================
// Test 5: MSG_CONTACT_SELECTED checks CurrentSelection result
// ============================================================================

static void
TestContactSelectedGuard()
{
	// Should have: if (index >= 0)
	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);

	char prev[512] = "";
	char line[512];
	bool foundGuard = false;

	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(prev, "CurrentSelection()") != NULL
			&& strstr(line, "if (index >= 0)") != NULL) {
			foundGuard = true;
			break;
		}
		strlcpy(prev, line, sizeof(prev));
	}
	fclose(f);

	assert(foundGuard);
	printf("  PASS: MSG_CONTACT_SELECTED guards against -1 index\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Contact Key & Bounds Check Tests ===\n\n");

	TestFormatContactKey();
	TestFormatContactKeyInUtils();
	TestMainWindowUsesHelper();
	TestSelectContactBoundsCheck();
	TestContactSelectedGuard();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
