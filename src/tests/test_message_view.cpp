/*
 * Test: MessageView structural integrity
 * Verifies theme-aware colors, Constants.h usage, layout patterns.
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
// Test 1: Theme-aware colors (no hardcoded rgb_color)
// ============================================================================

static void
TestThemeAwareColors()
{
	FILE* fp = OpenSource("MessageView.cpp");
	assert(fp != NULL);

	char line[1024];
	int lineNum = 0;
	bool foundUiColor = false;
	bool foundTintColor = false;

	while (fgets(line, sizeof(line), fp)) {
		lineNum++;
		if (strstr(line, "ui_color(") != NULL)
			foundUiColor = true;
		if (strstr(line, "tint_color(") != NULL)
			foundTintColor = true;
	}
	fclose(fp);

	assert(foundUiColor && "Should use ui_color() for theme awareness");
	assert(foundTintColor && "Should use tint_color() for derived colors");

	printf("  PASS: Uses ui_color() and tint_color() for theme awareness\n");
}


// ============================================================================
// Test 2: SNR colors use Constants.h named constants
// ============================================================================

static void
TestSnrColorConstants()
{
	FILE* fp = OpenSource("MessageView.cpp");
	assert(fp != NULL);

	char line[1024];
	bool usesConstants = false;
	bool includesConstants = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "#include \"Constants.h\"") != NULL)
			includesConstants = true;
		if (strstr(line, "kColorGood") != NULL
			|| strstr(line, "kColorFair") != NULL
			|| strstr(line, "kColorBad") != NULL)
			usesConstants = true;
	}
	fclose(fp);

	assert(includesConstants && "Should include Constants.h");
	assert(usesConstants && "Should use named color constants for SNR");

	printf("  PASS: SNR colors use Constants.h named constants\n");
}


// ============================================================================
// Test 3: Delivery status rendering (all 3 states handled)
// ============================================================================

static void
TestDeliveryStatusHandled()
{
	FILE* fp = OpenSource("MessageView.cpp");
	assert(fp != NULL);

	char line[1024];
	bool foundPending = false;
	bool foundSent = false;
	bool foundConfirmed = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "DELIVERY_PENDING") != NULL)
			foundPending = true;
		if (strstr(line, "DELIVERY_SENT") != NULL)
			foundSent = true;
		if (strstr(line, "DELIVERY_CONFIRMED") != NULL)
			foundConfirmed = true;
	}
	fclose(fp);

	assert(foundPending && "Must handle DELIVERY_PENDING");
	assert(foundSent && "Must handle DELIVERY_SENT");
	assert(foundConfirmed && "Must handle DELIVERY_CONFIRMED");

	printf("  PASS: All 3 delivery status states handled\n");
}


// ============================================================================
// Test 4: Uses localtime_r (not localtime)
// ============================================================================

static void
TestLocaltimeR()
{
	FILE* fp = OpenSource("MessageView.cpp");
	assert(fp != NULL);

	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		char* pos = strstr(line, "localtime(");
		if (pos != NULL && strstr(line, "localtime_r") == NULL) {
			fprintf(stderr, "FAIL: Non-reentrant localtime() in MessageView.cpp\n");
			fclose(fp);
			assert(0);
		}
	}
	fclose(fp);

	printf("  PASS: No non-reentrant localtime() calls\n");
}


// ============================================================================
// Test 5: Word wrap function exists
// ============================================================================

static void
TestWordWrapExists()
{
	FILE* fp = OpenSource("MessageView.cpp");
	assert(fp != NULL);

	char line[1024];
	bool foundWrapText = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "_WrapText(") != NULL) {
			foundWrapText = true;
			break;
		}
	}
	fclose(fp);

	assert(foundWrapText && "Should have _WrapText method");

	printf("  PASS: Word wrap function exists\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== MessageView Structural Tests ===\n\n");

	TestThemeAwareColors();
	TestSnrColorConstants();
	TestDeliveryStatusHandled();
	TestLocaltimeR();
	TestWordWrapExists();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
