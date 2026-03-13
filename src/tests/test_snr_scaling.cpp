/*
 * test_snr_scaling.cpp — Verify SNR ×4 scaling in all protocol handlers
 *
 * The MeshCore V3 protocol stores SNR as int8 × 4 (Q6.2 fixed-point).
 * All handlers must divide by 4 before use/display/storage.
 *
 * Build: g++ -o test_snr_scaling test_snr_scaling.cpp -I../ -lm
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


// Check that a specific pattern appears between two marker strings in a file
// This verifies the fix is in the right function, not somewhere else
static bool
FileContainsBetween(const char* path, const char* startMarker,
	const char* endMarker, const char* needle)
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

	bool found = false;
	char* start = strstr(buf, startMarker);
	if (start != NULL) {
		char* end = strstr(start + strlen(startMarker), endMarker);
		if (end != NULL) {
			// Temporarily null-terminate the region
			char saved = *end;
			*end = '\0';
			found = strstr(start, needle) != NULL;
			*end = saved;
		}
	}

	free(buf);
	return found;
}


// ============================================================================
// V3 DM SNR scaling
// ============================================================================

static void
test_v3_dm_snr_offset_defined()
{
	TEST("Constants.h: kV3DmSnrOffset defined");
	if (FileContains("../Constants.h", "kV3DmSnrOffset"))
		PASS();
	else
		FAIL("missing kV3DmSnrOffset");
}

static void
test_v3_dm_snr_divided()
{
	TEST("MainWindow.cpp: V3 DM SNR divided by 4");
	if (FileContainsBetween("../MainWindow.cpp",
		"::_HandleContactMsgRecv",
		"::_HandleChannelMsgRecv",
		"kV3DmSnrOffset] / 4"))
		PASS();
	else
		FAIL("V3 DM SNR not divided by 4 in _HandleContactMsgRecv");
}

static void
test_v3_dm_snr_has_comment()
{
	TEST("MainWindow.cpp: V3 DM SNR division has comment");
	if (FileContainsBetween("../MainWindow.cpp",
		"::_HandleContactMsgRecv",
		"::_HandleChannelMsgRecv",
		"V3 SNR is stored"))
		PASS();
	else
		FAIL("missing comment explaining SNR ×4 scaling");
}


// ============================================================================
// V3 Channel SNR scaling
// ============================================================================

static void
test_v3_ch_snr_offset_defined()
{
	TEST("Constants.h: kV3ChSnrOffset defined");
	if (FileContains("../Constants.h", "kV3ChSnrOffset"))
		PASS();
	else
		FAIL("missing kV3ChSnrOffset");
}

static void
test_v3_ch_snr_divided()
{
	TEST("MainWindow.cpp: V3 Channel SNR divided by 4");
	if (FileContainsBetween("../MainWindow.cpp",
		"::_HandleChannelMsgRecv",
		"::_HandleBattAndStorage",
		"kV3ChSnrOffset] / 4"))
		PASS();
	else
		FAIL("V3 Channel SNR not divided by 4 in _HandleChannelMsgRecv");
}

static void
test_v3_ch_snr_has_comment()
{
	TEST("MainWindow.cpp: V3 Channel SNR division has comment");
	if (FileContainsBetween("../MainWindow.cpp",
		"::_HandleChannelMsgRecv",
		"::_HandleBattAndStorage",
		"V3 SNR is stored"))
		PASS();
	else
		FAIL("missing comment explaining SNR ×4 scaling");
}


// ============================================================================
// Other handlers: confirm they still divide correctly (regression)
// ============================================================================

static void
test_push_raw_data_snr_divided()
{
	TEST("MainWindow.cpp: PUSH_RAW_DATA SNR divided by 4");
	if (FileContains("../MainWindow.cpp", "snr / 4.0f"))
		PASS();
	else
		FAIL("PUSH_RAW_DATA SNR division missing");
}

static void
test_push_control_data_snr_divided()
{
	TEST("MainWindow.cpp: PUSH_CONTROL_DATA SNR divided by 4");
	if (FileContains("../MainWindow.cpp", "data[1] / 4.0f"))
		PASS();
	else
		FAIL("PUSH_CONTROL_DATA SNR division missing");
}

static void
test_push_advert_snr_divided()
{
	TEST("MainWindow.cpp: PUSH_ADVERT SNR divided by 4");
	if (FileContains("../MainWindow.cpp", "snr / 4"))
		PASS();
	else
		FAIL("PUSH_ADVERT SNR division missing");
}

static void
test_status_response_snr_divided()
{
	TEST("MainWindow.cpp: Status response SNR divided by 4");
	if (FileContains("../MainWindow.cpp", "snrRaw / 4"))
		PASS();
	else
		FAIL("Status response SNR division missing");
}


// ============================================================================
// V2 handlers: confirm SNR is 0 (no SNR in V2)
// ============================================================================

static void
test_v2_dm_snr_zero()
{
	TEST("MainWindow.cpp: V2 DM SNR set to 0");
	if (FileContainsBetween("../MainWindow.cpp",
		"::_HandleContactMsgRecv",
		"::_HandleChannelMsgRecv",
		"snr = 0;  // V2 does not include SNR"))
		PASS();
	else
		FAIL("V2 DM should explicitly set snr = 0");
}

static void
test_v2_ch_snr_zero()
{
	TEST("MainWindow.cpp: V2 Channel SNR set to 0");
	if (FileContainsBetween("../MainWindow.cpp",
		"::_HandleChannelMsgRecv",
		"::_HandleBattAndStorage",
		"snr = 0;  // V2 does not include SNR"))
		PASS();
	else
		FAIL("V2 Channel should explicitly set snr = 0");
}


// ============================================================================
// Consistency: no raw SNR stored without division
// ============================================================================

static void
test_no_raw_snr_in_dm_handler()
{
	TEST("MainWindow.cpp: No raw kV3DmSnrOffset without /4");
	// The pattern "kV3DmSnrOffset];" (semicolon right after bracket)
	// would indicate reading without division — should NOT exist
	if (!FileContainsBetween("../MainWindow.cpp",
		"::_HandleContactMsgRecv",
		"::_HandleChannelMsgRecv",
		"kV3DmSnrOffset];"))
		PASS();
	else
		FAIL("found raw SNR read (no /4) in DM handler");
}

static void
test_no_raw_snr_in_ch_handler()
{
	TEST("MainWindow.cpp: No raw kV3ChSnrOffset without /4");
	if (!FileContainsBetween("../MainWindow.cpp",
		"::_HandleChannelMsgRecv",
		"::_HandleBattAndStorage",
		"kV3ChSnrOffset];"))
		PASS();
	else
		FAIL("found raw SNR read (no /4) in Channel handler");
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== SNR ×4 Scaling Tests ===\n\n");

	// V3 DM
	test_v3_dm_snr_offset_defined();
	test_v3_dm_snr_divided();
	test_v3_dm_snr_has_comment();

	// V3 Channel
	test_v3_ch_snr_offset_defined();
	test_v3_ch_snr_divided();
	test_v3_ch_snr_has_comment();

	// Regression: other handlers still correct
	test_push_raw_data_snr_divided();
	test_push_control_data_snr_divided();
	test_push_advert_snr_divided();
	test_status_response_snr_divided();

	// V2 handlers
	test_v2_dm_snr_zero();
	test_v2_ch_snr_zero();

	// Negative: no raw reads
	test_no_raw_snr_in_dm_handler();
	test_no_raw_snr_in_ch_handler();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
