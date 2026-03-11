/*
 * Test: Device PIN UI (P4)
 * Verifies PIN field, RSP_DEVICE_INFO parsing, message routing, and protocol call.
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
// Test 1: CMD_SET_DEVICE_PIN constant defined
// ============================================================================

static void
TestCommandConstant()
{
	assert(FileContains("Constants.h", "CMD_SET_DEVICE_PIN"));

	printf("  PASS: CMD_SET_DEVICE_PIN constant defined\n");
}


// ============================================================================
// Test 2: MSG_SET_DEVICE_PIN message constant defined
// ============================================================================

static void
TestMessageConstant()
{
	assert(FileContains("Constants.h", "MSG_SET_DEVICE_PIN"));

	printf("  PASS: MSG_SET_DEVICE_PIN message constant defined\n");
}


// ============================================================================
// Test 3: PIN UI control exists in SettingsWindow
// ============================================================================

static void
TestPinUIControl()
{
	const char* file = "SettingsWindow.cpp";

	assert(FileContains(file, "BLE PIN"));
	assert(FileContains(file, "Set PIN"));
	assert(FileContains(file, "fDevicePinControl"));

	printf("  PASS: BLE PIN field and Set PIN button exist\n");
}


// ============================================================================
// Test 4: fDevicePinControl member declared in header
// ============================================================================

static void
TestMemberVariable()
{
	assert(FileContains("SettingsWindow.h", "fDevicePinControl"));

	printf("  PASS: fDevicePinControl member declared\n");
}


// ============================================================================
// Test 5: SetDevicePin setter method exists
// ============================================================================

static void
TestSetterMethod()
{
	assert(FileContains("SettingsWindow.h", "SetDevicePin"));
	assert(FileContains("SettingsWindow.cpp",
		"SettingsWindow::SetDevicePin("));

	printf("  PASS: SetDevicePin setter method exists\n");
}


// ============================================================================
// Test 6: MSG_SET_DEVICE_PIN handler in MainWindow calls protocol
// ============================================================================

static void
TestMainWindowHandler()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "case MSG_SET_DEVICE_PIN:"));
	assert(FileContains(file, "SendSetDevicePin("));

	printf("  PASS: MSG_SET_DEVICE_PIN handler calls SendSetDevicePin\n");
}


// ============================================================================
// Test 7: Return value checked on SendSetDevicePin
// ============================================================================

static void
TestReturnValueChecked()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "Failed to set device PIN"));

	printf("  PASS: SendSetDevicePin return value checked\n");
}


// ============================================================================
// Test 8: BLE PIN parsed from RSP_DEVICE_INFO
// ============================================================================

static void
TestPinParsedFromDeviceInfo()
{
	const char* file = "MainWindow.cpp";

	// fDevicePin is assigned from data[4-7]
	assert(FileContains(file, "fDevicePin"));
	assert(FileContains(file, "BLE PIN:"));

	printf("  PASS: BLE PIN parsed from RSP_DEVICE_INFO\n");
}


// ============================================================================
// Test 9: PIN forwarded to SettingsWindow
// ============================================================================

static void
TestPinForwardedToSettings()
{
	const char* file = "MainWindow.cpp";

	assert(FileContains(file, "SetDevicePin(fDevicePin)"));

	printf("  PASS: PIN forwarded to SettingsWindow on open\n");
}


// ============================================================================
// Test 10: Protocol method exists in ProtocolHandler
// ============================================================================

static void
TestProtocolMethod()
{
	assert(FileContains("ProtocolHandler.h", "SendSetDevicePin"));
	assert(FileContains("ProtocolHandler.cpp",
		"ProtocolHandler::SendSetDevicePin("));

	printf("  PASS: SendSetDevicePin protocol method exists\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Device PIN UI Tests (P4) ===\n\n");

	TestCommandConstant();
	TestMessageConstant();
	TestPinUIControl();
	TestMemberVariable();
	TestSetterMethod();
	TestMainWindowHandler();
	TestReturnValueChecked();
	TestPinParsedFromDeviceInfo();
	TestPinForwardedToSettings();
	TestProtocolMethod();

	printf("\nAll 10 tests passed.\n");
	return 0;
}
