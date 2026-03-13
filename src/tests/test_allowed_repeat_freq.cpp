/*
 * test_allowed_repeat_freq.cpp — Verify CMD/RSP allowed repeat freq (60/26)
 *
 * Build: g++ -o test_allowed_repeat_freq test_allowed_repeat_freq.cpp -I../ -lm
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
test_cmd_defined()
{
	TEST("Constants.h: CMD_GET_ALLOWED_REPEAT_FREQ defined");
	if (FileContains("../Constants.h", "CMD_GET_ALLOWED_REPEAT_FREQ"))
		PASS();
	else
		FAIL("missing CMD_GET_ALLOWED_REPEAT_FREQ");
}

static void
test_cmd_value()
{
	TEST("Constants.h: CMD_GET_ALLOWED_REPEAT_FREQ = 60");
	if (FileContains("../Constants.h", "CMD_GET_ALLOWED_REPEAT_FREQ = 60"))
		PASS();
	else
		FAIL("CMD_GET_ALLOWED_REPEAT_FREQ should be 60");
}

static void
test_rsp_defined()
{
	TEST("Constants.h: RSP_ALLOWED_REPEAT_FREQ defined");
	if (FileContains("../Constants.h", "RSP_ALLOWED_REPEAT_FREQ"))
		PASS();
	else
		FAIL("missing RSP_ALLOWED_REPEAT_FREQ");
}

static void
test_rsp_value()
{
	TEST("Constants.h: RSP_ALLOWED_REPEAT_FREQ = 26");
	if (FileContains("../Constants.h", "RSP_ALLOWED_REPEAT_FREQ = 26"))
		PASS();
	else
		FAIL("RSP_ALLOWED_REPEAT_FREQ should be 26");
}


// ============================================================================
// ProtocolHandler
// ============================================================================

static void
test_send_method_header()
{
	TEST("ProtocolHandler.h: SendGetAllowedRepeatFreq declared");
	if (FileContains("../ProtocolHandler.h", "SendGetAllowedRepeatFreq"))
		PASS();
	else
		FAIL("missing SendGetAllowedRepeatFreq declaration");
}

static void
test_send_method_impl()
{
	TEST("ProtocolHandler.cpp: SendGetAllowedRepeatFreq implemented");
	if (FileContains("../ProtocolHandler.cpp",
		"::SendGetAllowedRepeatFreq"))
		PASS();
	else
		FAIL("missing SendGetAllowedRepeatFreq implementation");
}

static void
test_send_method_uses_cmd()
{
	TEST("ProtocolHandler.cpp: uses CMD_GET_ALLOWED_REPEAT_FREQ");
	if (FileContains("../ProtocolHandler.cpp",
		"CMD_GET_ALLOWED_REPEAT_FREQ"))
		PASS();
	else
		FAIL("missing CMD_GET_ALLOWED_REPEAT_FREQ usage");
}


// ============================================================================
// MainWindow.h
// ============================================================================

static void
test_handler_declared()
{
	TEST("MainWindow.h: _HandleAllowedRepeatFreq declared");
	if (FileContains("../MainWindow.h", "_HandleAllowedRepeatFreq"))
		PASS();
	else
		FAIL("missing _HandleAllowedRepeatFreq declaration");
}


// ============================================================================
// MainWindow.cpp — _ParseFrame
// ============================================================================

static void
test_parse_frame_case()
{
	TEST("MainWindow.cpp: case RSP_ALLOWED_REPEAT_FREQ in _ParseFrame");
	if (FileContains("../MainWindow.cpp", "case RSP_ALLOWED_REPEAT_FREQ:"))
		PASS();
	else
		FAIL("missing case in _ParseFrame");
}

static void
test_parse_frame_calls_handler()
{
	TEST("MainWindow.cpp: _HandleAllowedRepeatFreq called");
	if (FileContains("../MainWindow.cpp",
		"_HandleAllowedRepeatFreq(data, length)"))
		PASS();
	else
		FAIL("missing handler call");
}


// ============================================================================
// MainWindow.cpp — Handler implementation
// ============================================================================

static void
test_handler_exists()
{
	TEST("MainWindow.cpp: _HandleAllowedRepeatFreq implemented");
	if (FileContains("../MainWindow.cpp",
		"::_HandleAllowedRepeatFreq("))
		PASS();
	else
		FAIL("missing handler implementation");
}

static void
test_handler_length_check()
{
	TEST("MainWindow.cpp: handler checks minimum length");
	if (FileContains("../MainWindow.cpp", "length < 9"))
		PASS();
	else
		FAIL("missing length check");
}

static void
test_handler_reads_freq_pairs()
{
	TEST("MainWindow.cpp: handler reads freq pairs via ReadLE32");
	if (FileContains("../MainWindow.cpp", "lowerKHz") &&
		FileContains("../MainWindow.cpp", "upperKHz"))
		PASS();
	else
		FAIL("missing frequency pair parsing");
}

static void
test_handler_logs_mhz()
{
	TEST("MainWindow.cpp: handler logs frequencies in MHz");
	if (FileContains("../MainWindow.cpp", "Allowed repeat frequencies:"))
		PASS();
	else
		FAIL("missing MHz log output");
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== Allowed Repeat Freq Tests ===\n\n");

	// Constants
	test_cmd_defined();
	test_cmd_value();
	test_rsp_defined();
	test_rsp_value();

	// ProtocolHandler
	test_send_method_header();
	test_send_method_impl();
	test_send_method_uses_cmd();

	// MainWindow.h
	test_handler_declared();

	// _ParseFrame
	test_parse_frame_case();
	test_parse_frame_calls_handler();

	// Handler
	test_handler_exists();
	test_handler_length_check();
	test_handler_reads_freq_pairs();
	test_handler_logs_mhz();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
