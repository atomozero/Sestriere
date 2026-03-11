/*
 * Test: Tuning Parameters UI (P3)
 * Verifies UI controls, message routing, and protocol calls.
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
// Test 1: CMD_SET_TUNING_PARAMS and CMD_GET_TUNING_PARAMS constants defined
// ============================================================================

static void
TestCommandConstants()
{
	assert(FileContains("Constants.h", "CMD_SET_TUNING_PARAMS"));
	assert(FileContains("Constants.h", "CMD_GET_TUNING_PARAMS"));

	printf("  PASS: CMD_SET/GET_TUNING_PARAMS constants defined\n");
}


// ============================================================================
// Test 2: MSG_SET_TUNING_PARAMS message constant defined
// ============================================================================

static void
TestMessageConstant()
{
	assert(FileContains("Constants.h", "MSG_SET_TUNING_PARAMS"));

	printf("  PASS: MSG_SET_TUNING_PARAMS message constant defined\n");
}


// ============================================================================
// Test 3: UI controls exist in SettingsWindow
// ============================================================================

static void
TestUIControls()
{
	const char* file = "SettingsWindow.cpp";

	assert(FileContains(file, "RX Delay Base"));
	assert(FileContains(file, "Airtime Factor"));
	assert(FileContains(file, "Apply Tuning"));

	printf("  PASS: RX Delay Base, Airtime Factor controls and Apply button\n");
}


// ============================================================================
// Test 4: Member variables declared in header
// ============================================================================

static void
TestMemberVariables()
{
	const char* file = "SettingsWindow.h";

	assert(FileContains(file, "fRxDelayBaseControl"));
	assert(FileContains(file, "fAirtimeFactorControl"));

	printf("  PASS: Tuning param member variables declared\n");
}


// ============================================================================
// Test 5: SetTuningParams setter method exists
// ============================================================================

static void
TestSetterMethod()
{
	assert(FileContains("SettingsWindow.h", "SetTuningParams"));
	assert(FileContains("SettingsWindow.cpp",
		"SettingsWindow::SetTuningParams("));

	printf("  PASS: SetTuningParams setter method exists\n");
}


// ============================================================================
// Test 6: MSG_SET_TUNING_PARAMS handler in MainWindow calls protocol
// ============================================================================

static void
TestMainWindowHandler()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "case MSG_SET_TUNING_PARAMS:"));
	assert(FileContains(file, "SendSetTuningParams("));

	printf("  PASS: MSG_SET_TUNING_PARAMS handler calls SendSetTuningParams\n");
}


// ============================================================================
// Test 7: Return value checked on SendSetTuningParams
// ============================================================================

static void
TestReturnValueChecked()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "SendSetTuningParams(rxDelay,"));
	assert(FileContains(file, "Failed to set tuning parameters"));

	printf("  PASS: SendSetTuningParams return value checked\n");
}


// ============================================================================
// Test 8: Protocol methods exist in ProtocolHandler
// ============================================================================

static void
TestProtocolMethods()
{
	assert(FileContains("ProtocolHandler.h", "SendGetTuningParams"));
	assert(FileContains("ProtocolHandler.h", "SendSetTuningParams"));
	assert(FileContains("ProtocolHandler.cpp",
		"ProtocolHandler::SendSetTuningParams("));

	printf("  PASS: Protocol methods for tuning params exist\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Tuning Parameters UI Tests (P3) ===\n\n");

	TestCommandConstants();
	TestMessageConstant();
	TestUIControls();
	TestMemberVariables();
	TestSetterMethod();
	TestMainWindowHandler();
	TestReturnValueChecked();
	TestProtocolMethods();

	printf("\nAll 8 tests passed.\n");
	return 0;
}
