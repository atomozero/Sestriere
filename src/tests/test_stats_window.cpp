/*
 * Test: StatsWindow structural integrity
 * Verifies theme-aware colors, Constants.h usage, no hardcoded colors.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>


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
// Test 1: No hardcoded label/value/header rgb_color constants
// ============================================================================

static void
TestNoHardcodedTextColors()
{
	FILE* fp = OpenSource("StatsWindow.cpp");
	assert(fp != NULL);

	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		// Skip comments
		if (strstr(line, "//") != NULL && strstr(line, "//") < strstr(line, "rgb_color"))
			continue;
		// Check for hardcoded label/value/header colors
		if (strstr(line, "kLabelColor") != NULL
			|| strstr(line, "kValueColor") != NULL
			|| strstr(line, "kHeaderColor") != NULL) {
			fprintf(stderr, "FAIL: Found hardcoded text color: %s", line);
			fclose(fp);
			assert(0);
		}
	}
	fclose(fp);

	printf("  PASS: No hardcoded label/value/header colors\n");
}


// ============================================================================
// Test 2: Uses ui_color() for theme awareness
// ============================================================================

static void
TestUsesUiColor()
{
	FILE* fp = OpenSource("StatsWindow.cpp");
	assert(fp != NULL);

	char line[1024];
	bool foundUiColor = false;
	bool foundTintColor = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "ui_color(") != NULL)
			foundUiColor = true;
		if (strstr(line, "tint_color(") != NULL)
			foundTintColor = true;
	}
	fclose(fp);

	assert(foundUiColor && "Must use ui_color() for theme-aware colors");
	assert(foundTintColor && "Must use tint_color() for derived colors");

	printf("  PASS: Uses ui_color() and tint_color() for theme awareness\n");
}


// ============================================================================
// Test 3: Uses Constants.h named color constants
// ============================================================================

static void
TestUsesNamedConstants()
{
	FILE* fp = OpenSource("StatsWindow.cpp");
	assert(fp != NULL);

	char line[1024];
	bool usesColorGood = false;
	bool usesColorBad = false;
	bool usesBattConsts = false;
	bool includesConstants = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "#include \"Constants.h\"") != NULL)
			includesConstants = true;
		if (strstr(line, "kColorGood") != NULL)
			usesColorGood = true;
		if (strstr(line, "kColorBad") != NULL)
			usesColorBad = true;
		if (strstr(line, "kBattMinMv") != NULL
			|| strstr(line, "kBattRangeMv") != NULL
			|| strstr(line, "BatteryPercent") != NULL)
			usesBattConsts = true;
	}
	fclose(fp);

	assert(includesConstants && "Must include Constants.h");
	assert(usesColorGood && "Must use kColorGood");
	assert(usesColorBad && "Must use kColorBad");
	assert(usesBattConsts && "Must use kBattMinMv/kBattRangeMv or BatteryPercent()");

	printf("  PASS: Uses Constants.h named color and battery constants\n");
}


// ============================================================================
// Test 4: MQTT password is masked
// ============================================================================

static void
TestMqttPasswordMasked()
{
	FILE* fp = OpenSource("SettingsWindow.cpp");
	assert(fp != NULL);

	char line[1024];
	bool foundHideTyping = false;
	bool foundIsTypingHidden = false;
	bool foundShowToggle = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "HideTyping(true)") != NULL)
			foundHideTyping = true;
		if (strstr(line, "IsTypingHidden()") != NULL)
			foundIsTypingHidden = true;
		if (strstr(line, "show_pass") != NULL
			|| strstr(line, "TogglePasswordVis") != NULL)
			foundShowToggle = true;
	}
	fclose(fp);

	assert(foundHideTyping && "MQTT password field must use HideTyping(true)");
	assert(foundIsTypingHidden && "Must check IsTypingHidden() for toggle");
	assert(foundShowToggle && "Must have show/hide password toggle");

	printf("  PASS: MQTT password masked with show/hide toggle\n");
}


// ============================================================================
// Test 5: No Italian strings in ContactItem
// ============================================================================

static void
TestNoItalianStrings()
{
	FILE* fp = OpenSource("ContactItem.cpp");
	assert(fp != NULL);

	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "\"Ieri\"") != NULL) {
			fprintf(stderr, "FAIL: Found Italian string 'Ieri' in ContactItem.cpp\n");
			fclose(fp);
			assert(0);
		}
	}
	fclose(fp);

	printf("  PASS: No Italian strings in ContactItem\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== UX/UI Quality Tests ===\n\n");

	TestNoHardcodedTextColors();
	TestUsesUiColor();
	TestUsesNamedConstants();
	TestMqttPasswordMasked();
	TestNoItalianStrings();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
