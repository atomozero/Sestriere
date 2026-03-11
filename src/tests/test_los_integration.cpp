/*
 * test_los_integration.cpp — Integration pattern tests for LoS feature
 *
 * Build: g++ -o test_los_integration test_los_integration.cpp -I../ -lm
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>


static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  %-50s ", name); \
	fflush(stdout);

#define PASS() \
	do { printf("[PASS]\n"); sTestsPassed++; } while (0)

#define FAIL(msg) \
	do { printf("[FAIL] %s\n", msg); sTestsFailed++; } while (0)


static bool
FileContains(const char* path, const char* needle)
{
	FILE* f = fopen(path, "r");
	if (f == NULL) return false;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = (char*)malloc(size + 1);
	if (buf == NULL) { fclose(f); return false; }

	fread(buf, 1, size, f);
	buf[size] = '\0';
	fclose(f);

	bool found = strstr(buf, needle) != NULL;
	free(buf);
	return found;
}


// ============================================================================
// Constants.h tests
// ============================================================================

static void
test_msg_show_los_constant()
{
	TEST("Constants.h: MSG_SHOW_LOS defined");
	if (FileContains("../Constants.h", "MSG_SHOW_LOS"))
		PASS();
	else
		FAIL("missing MSG_SHOW_LOS constant");
}

static void
test_msg_show_los_value()
{
	TEST("Constants.h: MSG_SHOW_LOS = 'slos'");
	if (FileContains("../Constants.h", "MSG_SHOW_LOS = 'slos'"))
		PASS();
	else
		FAIL("wrong MSG_SHOW_LOS value");
}


// ============================================================================
// MainWindow.h tests
// ============================================================================

static void
test_forward_decl()
{
	TEST("MainWindow.h: LoSWindow forward declaration");
	if (FileContains("../MainWindow.h", "class LoSWindow"))
		PASS();
	else
		FAIL("missing LoSWindow forward declaration");
}

static void
test_member_variable()
{
	TEST("MainWindow.h: fLoSWindow member");
	if (FileContains("../MainWindow.h", "fLoSWindow"))
		PASS();
	else
		FAIL("missing fLoSWindow member");
}


// ============================================================================
// MainWindow.cpp tests
// ============================================================================

static void
test_include_los_window()
{
	TEST("MainWindow.cpp: includes LoSWindow.h");
	if (FileContains("../MainWindow.cpp", "#include \"LoSWindow.h\""))
		PASS();
	else
		FAIL("missing LoSWindow.h include");
}

static void
test_init_null()
{
	TEST("MainWindow.cpp: fLoSWindow(NULL) in init list");
	if (FileContains("../MainWindow.cpp", "fLoSWindow(NULL)"))
		PASS();
	else
		FAIL("missing fLoSWindow(NULL) initialization");
}

static void
test_quit_cleanup()
{
	TEST("MainWindow.cpp: fLoSWindow cleanup in QuitRequested");
	if (FileContains("../MainWindow.cpp", "fLoSWindow->Lock()"))
		PASS();
	else
		FAIL("missing fLoSWindow cleanup");
}

static void
test_msg_show_los_handler()
{
	TEST("MainWindow.cpp: MSG_SHOW_LOS handler");
	if (FileContains("../MainWindow.cpp", "case MSG_SHOW_LOS:"))
		PASS();
	else
		FAIL("missing MSG_SHOW_LOS handler");
}

static void
test_context_menu_item()
{
	TEST("MainWindow.cpp: Line of Sight menu item");
	if (FileContains("../MainWindow.cpp", "\"Line of Sight\""))
		PASS();
	else
		FAIL("missing Line of Sight menu item");
}

static void
test_gps_gate_self()
{
	TEST("MainWindow.cpp: GPS gate for self position");
	if (FileContains("../MainWindow.cpp", "fMqttSettings.latitude"))
		PASS();
	else
		FAIL("missing self GPS check");
}

static void
test_gps_gate_contact()
{
	TEST("MainWindow.cpp: GPS gate for contact");
	if (FileContains("../MainWindow.cpp", "HasGPS()"))
		PASS();
	else
		FAIL("missing contact GPS check");
}

static void
test_set_endpoints_call()
{
	TEST("MainWindow.cpp: SetEndpoints called");
	if (FileContains("../MainWindow.cpp", "SetEndpoints("))
		PASS();
	else
		FAIL("missing SetEndpoints call");
}

static void
test_set_frequency_call()
{
	TEST("MainWindow.cpp: SetFrequency called");
	if (FileContains("../MainWindow.cpp", "SetFrequency("))
		PASS();
	else
		FAIL("missing SetFrequency call");
}

static void
test_start_analysis_call()
{
	TEST("MainWindow.cpp: StartAnalysis called");
	if (FileContains("../MainWindow.cpp", "StartAnalysis()"))
		PASS();
	else
		FAIL("missing StartAnalysis call");
}

static void
test_lock_looper_guard()
{
	TEST("MainWindow.cpp: LockLooper guard on LoS window");
	// Check that LockLooper is called when accessing LoSWindow
	if (FileContains("../MainWindow.cpp", "fLoSWindow->LockLooper()"))
		PASS();
	else
		FAIL("missing LockLooper guard");
}


// ============================================================================
// Makefile tests
// ============================================================================

static void
test_makefile_elevation_service()
{
	TEST("Makefile: ElevationService.cpp in SRCS");
	if (FileContains("../Makefile", "ElevationService.cpp"))
		PASS();
	else
		FAIL("missing ElevationService.cpp in Makefile");
}

static void
test_makefile_los_window()
{
	TEST("Makefile: LoSWindow.cpp in SRCS");
	if (FileContains("../Makefile", "LoSWindow.cpp"))
		PASS();
	else
		FAIL("missing LoSWindow.cpp in Makefile");
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== LoS Integration Tests ===\n\n");

	// Constants
	test_msg_show_los_constant();
	test_msg_show_los_value();

	// MainWindow.h
	test_forward_decl();
	test_member_variable();

	// MainWindow.cpp
	test_include_los_window();
	test_init_null();
	test_quit_cleanup();
	test_msg_show_los_handler();
	test_context_menu_item();
	test_gps_gate_self();
	test_gps_gate_contact();
	test_set_endpoints_call();
	test_set_frequency_call();
	test_start_analysis_call();
	test_lock_looper_guard();

	// Makefile
	test_makefile_elevation_service();
	test_makefile_los_window();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
