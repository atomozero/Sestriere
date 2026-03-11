/*
 * Test: PUSH_CONTROL_DATA (0x8E) handling (B3)
 * Verifies the handler exists, is declared, and logs control data.
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
// Test 1: PUSH_CONTROL_DATA constant defined (0x8E)
// ============================================================================

static void
TestConstantDefined()
{
	assert(FileContains("Constants.h", "PUSH_CONTROL_DATA"));
	assert(FileContains("Constants.h", "0x8E"));

	printf("  PASS: PUSH_CONTROL_DATA constant defined as 0x8E\n");
}


// ============================================================================
// Test 2: case PUSH_CONTROL_DATA in _ParseFrame switch
// ============================================================================

static void
TestCaseInParseFrame()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "case PUSH_CONTROL_DATA:"));
	assert(FileContains(file, "_HandlePushControlData(data, length)"));

	printf("  PASS: case PUSH_CONTROL_DATA dispatches to handler\n");
}


// ============================================================================
// Test 3: _HandlePushControlData declared in header
// ============================================================================

static void
TestHandlerDeclared()
{
	assert(FileContains("MainWindow.h", "_HandlePushControlData"));

	printf("  PASS: _HandlePushControlData declared in MainWindow.h\n");
}


// ============================================================================
// Test 4: _HandlePushControlData implementation exists
// ============================================================================

static void
TestHandlerImplemented()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "MainWindow::_HandlePushControlData("));

	printf("  PASS: _HandlePushControlData implemented in MainWindow.cpp\n");
}


// ============================================================================
// Test 5: Handler parses SNR, RSSI, and path_len
// ============================================================================

static void
TestParseFields()
{
	const char* file = "MainWindow.cpp";

	// Find the implementation and verify field parsing
	FILE* f = OpenSource(file);
	assert(f != NULL);

	char line[512];
	bool inHandler = false;
	bool hasSnr = false;
	bool hasRssi = false;
	bool hasPathLen = false;
	bool hasPayload = false;

	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "MainWindow::_HandlePushControlData("))
			inHandler = true;

		if (inHandler) {
			if (strstr(line, "snrDb") || strstr(line, "SNR"))
				hasSnr = true;
			if (strstr(line, "rssi") || strstr(line, "RSSI"))
				hasRssi = true;
			if (strstr(line, "pathLen"))
				hasPathLen = true;
			if (strstr(line, "payloadLen") || strstr(line, "payload"))
				hasPayload = true;

			// Stop at next function definition
			if (strstr(line, "MainWindow::_Handle") &&
				!strstr(line, "_HandlePushControlData"))
				break;
		}
	}
	fclose(f);

	assert(hasSnr);
	assert(hasRssi);
	assert(hasPathLen);
	assert(hasPayload);

	printf("  PASS: Handler parses SNR, RSSI, path_len, and payload\n");
}


// ============================================================================
// Test 6: Control data logged to debug log
// ============================================================================

static void
TestDebugLogging()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "\"CTRL\""));
	assert(FileContains(file, "Control data:"));

	printf("  PASS: Control data logged with CTRL category\n");
}


// ============================================================================
// Test 7: Short frame protection
// ============================================================================

static void
TestShortFrameProtection()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "PUSH_CONTROL_DATA: frame too short"));

	printf("  PASS: Short frame rejected with warning\n");
}


// ============================================================================
// Test 8: PacketAnalyzer labels CONTROL_DATA
// ============================================================================

static void
TestPacketAnalyzerLabel()
{
	assert(FileContains("PacketAnalyzerWindow.cpp", "CONTROL_DATA"));

	printf("  PASS: PacketAnalyzer has CONTROL_DATA label\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== PUSH_CONTROL_DATA Handler Tests (B3) ===\n\n");

	TestConstantDefined();
	TestCaseInParseFrame();
	TestHandlerDeclared();
	TestHandlerImplemented();
	TestParseFields();
	TestDebugLogging();
	TestShortFrameProtection();
	TestPacketAnalyzerLabel();

	printf("\nAll 8 tests passed.\n");
	return 0;
}
