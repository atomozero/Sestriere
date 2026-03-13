/*
 * test_tuning_response.cpp — Verify RSP_TUNING_PARAMS (code 23) handling
 *
 * Build: g++ -o test_tuning_response test_tuning_response.cpp -I../ -lm
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>


static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  %-55s ", name); \
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
// Constants.h
// ============================================================================

static void
test_rsp_tuning_params_defined()
{
	TEST("Constants.h: RSP_TUNING_PARAMS defined");
	if (FileContains("../Constants.h", "RSP_TUNING_PARAMS"))
		PASS();
	else
		FAIL("missing RSP_TUNING_PARAMS constant");
}

static void
test_rsp_tuning_params_value()
{
	TEST("Constants.h: RSP_TUNING_PARAMS = 23");
	if (FileContains("../Constants.h", "RSP_TUNING_PARAMS = 23"))
		PASS();
	else
		FAIL("RSP_TUNING_PARAMS should be 23");
}

static void
test_cmd_get_tuning_params_exists()
{
	TEST("Constants.h: CMD_GET_TUNING_PARAMS = 43");
	if (FileContains("../Constants.h", "CMD_GET_TUNING_PARAMS = 43"))
		PASS();
	else
		FAIL("CMD_GET_TUNING_PARAMS should be 43");
}


// ============================================================================
// MainWindow.h
// ============================================================================

static void
test_handler_declared()
{
	TEST("MainWindow.h: _HandleTuningParams declared");
	if (FileContains("../MainWindow.h", "_HandleTuningParams"))
		PASS();
	else
		FAIL("missing _HandleTuningParams declaration");
}


// ============================================================================
// MainWindow.cpp — _ParseFrame dispatch
// ============================================================================

static void
test_parse_frame_case()
{
	TEST("MainWindow.cpp: case RSP_TUNING_PARAMS in _ParseFrame");
	if (FileContains("../MainWindow.cpp", "case RSP_TUNING_PARAMS:"))
		PASS();
	else
		FAIL("missing case RSP_TUNING_PARAMS in _ParseFrame");
}

static void
test_parse_frame_calls_handler()
{
	TEST("MainWindow.cpp: _HandleTuningParams called from _ParseFrame");
	if (FileContains("../MainWindow.cpp", "_HandleTuningParams(data, length)"))
		PASS();
	else
		FAIL("missing _HandleTuningParams call");
}


// ============================================================================
// MainWindow.cpp — _HandleTuningParams implementation
// ============================================================================

static void
test_handler_exists()
{
	TEST("MainWindow.cpp: _HandleTuningParams implemented");
	if (FileContains("../MainWindow.cpp", "::_HandleTuningParams("))
		PASS();
	else
		FAIL("missing _HandleTuningParams implementation");
}

static void
test_handler_length_check()
{
	TEST("MainWindow.cpp: _HandleTuningParams checks length");
	if (FileContains("../MainWindow.cpp", "length < 9"))
		PASS();
	else
		FAIL("missing length check in _HandleTuningParams");
}

static void
test_handler_reads_rx_delay()
{
	TEST("MainWindow.cpp: reads rxDelayBase via ReadLE32");
	if (FileContains("../MainWindow.cpp", "ReadLE32(data + 1)"))
		PASS();
	else
		FAIL("missing rxDelayBase ReadLE32");
}

static void
test_handler_reads_airtime()
{
	TEST("MainWindow.cpp: reads airtimeFactor via ReadLE32");
	if (FileContains("../MainWindow.cpp", "ReadLE32(data + 5)"))
		PASS();
	else
		FAIL("missing airtimeFactor ReadLE32");
}

static void
test_handler_logs_values()
{
	TEST("MainWindow.cpp: _HandleTuningParams logs values");
	if (FileContains("../MainWindow.cpp", "Tuning params: rxDelay="))
		PASS();
	else
		FAIL("missing log message");
}

static void
test_handler_forwards_to_settings()
{
	TEST("MainWindow.cpp: forwards to SettingsWindow");
	if (FileContains("../MainWindow.cpp", "SetTuningParams(rxDelayBase"))
		PASS();
	else
		FAIL("missing SetTuningParams forwarding");
}

static void
test_handler_lock_guard()
{
	TEST("MainWindow.cpp: LockLooper guard on SettingsWindow");
	if (FileContains("../MainWindow.cpp",
		"fSettingsWindow->LockLooper()"))
		PASS();
	else
		FAIL("missing LockLooper guard");
}


// ============================================================================
// ProtocolHandler — SendGetTuningParams exists
// ============================================================================

static void
test_send_get_tuning_params()
{
	TEST("ProtocolHandler.cpp: SendGetTuningParams exists");
	if (FileContains("../ProtocolHandler.cpp", "SendGetTuningParams"))
		PASS();
	else
		FAIL("missing SendGetTuningParams");
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== RSP_TUNING_PARAMS Tests ===\n\n");

	// Constants
	test_rsp_tuning_params_defined();
	test_rsp_tuning_params_value();
	test_cmd_get_tuning_params_exists();

	// Header
	test_handler_declared();

	// _ParseFrame
	test_parse_frame_case();
	test_parse_frame_calls_handler();

	// Handler implementation
	test_handler_exists();
	test_handler_length_check();
	test_handler_reads_rx_delay();
	test_handler_reads_airtime();
	test_handler_logs_values();
	test_handler_forwards_to_settings();
	test_handler_lock_guard();

	// ProtocolHandler
	test_send_get_tuning_params();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
