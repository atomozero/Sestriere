/*
 * Test: Named color constants in Constants.h
 * Verifies color constants are defined and used across files.
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
// Test 1: Color constants defined in Constants.h
// ============================================================================

static void
TestColorConstantsDefined()
{
	assert(FileContains("Constants.h", "kStatusOnline"));
	assert(FileContains("Constants.h", "kStatusRecent"));
	assert(FileContains("Constants.h", "kStatusOffline"));
	assert(FileContains("Constants.h", "kColorGood"));
	assert(FileContains("Constants.h", "kColorFair"));
	assert(FileContains("Constants.h", "kColorPoor"));
	assert(FileContains("Constants.h", "kColorBad"));
	assert(FileContains("Constants.h", "kTypeBadgeRepeater"));
	assert(FileContains("Constants.h", "kTypeBadgeRoom"));
	assert(FileContains("Constants.h", "kAvatarPalette"));

	printf("  PASS: All color constants defined in Constants.h\n");
}


// ============================================================================
// Test 2: No inline quality colors in TopBarView/MissionControl
// ============================================================================

static void
TestNoInlineQualityColors()
{
	const char* files[] = {
		"MissionControlWindow.cpp",
		"TopBarView.cpp",
	};
	const char* patterns[] = {
		"{80, 180, 80",
		"{200, 170, 50",
		"{210, 120, 50",
		"{200, 60, 60",
	};

	int violations = 0;
	for (int f = 0; f < 2; f++) {
		for (int p = 0; p < 4; p++) {
			int count = CountOccurrences(files[f], patterns[p]);
			if (count > 0) {
				printf("    WARN: %s still has %d inline %s\n",
					files[f], count, patterns[p]);
				violations += count;
			}
		}
	}

	assert(violations == 0);
	printf("  PASS: No inline quality colors in TopBarView/MissionControlWindow\n");
}


// ============================================================================
// Test 3: Avatar palette not duplicated
// ============================================================================

static void
TestAvatarPaletteNotDuplicated()
{
	const char* files[] = {
		"ContactItem.cpp",
		"ContactInfoPanel.cpp",
		"ChatHeaderView.cpp",
	};

	int localArrays = 0;
	for (int i = 0; i < 3; i++) {
		int count = CountOccurrences(files[i], "229, 115, 115");
		localArrays += count;
	}

	assert(localArrays == 0);

	for (int i = 0; i < 3; i++) {
		assert(FileContains(files[i], "kAvatarPalette"));
	}

	printf("  PASS: Avatar palette centralized (no duplicates in 3 files)\n");
}


// ============================================================================
// Test 4: Type badge constants used
// ============================================================================

static void
TestTypeBadgeConstantsUsed()
{
	assert(FileContains("ContactItem.cpp", "kTypeBadgeRepeater"));
	assert(FileContains("ContactItem.cpp", "kTypeBadgeRoom"));
	assert(FileContains("ContactInfoPanel.cpp", "kTypeBadgeRepeater"));
	assert(FileContains("ContactInfoPanel.cpp", "kTypeBadgeRoom"));

	int inlineRepeater = CountOccurrences("ContactItem.cpp", "{100, 160, 100");
	int inlineRoom = CountOccurrences("ContactItem.cpp", "{120, 120, 180");
	assert(inlineRepeater == 0);
	assert(inlineRoom == 0);

	printf("  PASS: Type badge constants used (no inline badge colors)\n");
}


// ============================================================================
// Test 5: Status colors use named constants
// ============================================================================

static void
TestStatusColorsUseConstants()
{
	const char* files[] = {
		"ContactItem.cpp",
		"ContactInfoPanel.cpp",
		"ChatHeaderView.cpp",
	};

	for (int i = 0; i < 3; i++) {
		int inlineOnline = CountOccurrences(files[i], "{77, 182, 172");
		assert(inlineOnline == 0);
	}

	printf("  PASS: Status colors centralized (no inline definitions)\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Color Constants Tests ===\n\n");

	TestColorConstantsDefined();
	TestNoInlineQualityColors();
	TestAvatarPaletteNotDuplicated();
	TestTypeBadgeConstantsUsed();
	TestStatusColorsUseConstants();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
