/*
 * test_elevation_service.cpp — Pattern tests for ElevationService
 *
 * Build: g++ -o test_elevation_service test_elevation_service.cpp -I../ -lm
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


// File content checker
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
// Header tests
// ============================================================================

static void
test_header_guard()
{
	TEST("ElevationService.h: header guard");
	if (FileContains("../ElevationService.h", "#ifndef ELEVATIONSERVICE_H"))
		PASS();
	else
		FAIL("missing header guard");
}

static void
test_header_fetch_elevations()
{
	TEST("ElevationService.h: FetchElevations method");
	if (FileContains("../ElevationService.h", "FetchElevations"))
		PASS();
	else
		FAIL("missing FetchElevations");
}

static void
test_header_build_terrain()
{
	TEST("ElevationService.h: BuildTerrainProfile method");
	if (FileContains("../ElevationService.h", "BuildTerrainProfile"))
		PASS();
	else
		FAIL("missing BuildTerrainProfile");
}

static void
test_header_includes_los()
{
	TEST("ElevationService.h: includes LoSAnalysis");
	if (FileContains("../ElevationService.h", "LoSAnalysis.h"))
		PASS();
	else
		FAIL("missing LoSAnalysis.h include");
}


// ============================================================================
// Implementation tests
// ============================================================================

static void
test_impl_curl_include()
{
	TEST("ElevationService.cpp: includes curl");
	if (FileContains("../ElevationService.cpp", "#include <curl/curl.h>"))
		PASS();
	else
		FAIL("missing curl include");
}

static void
test_impl_bjson_include()
{
	TEST("ElevationService.cpp: includes BJson");
	if (FileContains("../ElevationService.cpp", "Json.h"))
		PASS();
	else
		FAIL("missing BJson include");
}

static void
test_impl_api_url()
{
	TEST("ElevationService.cpp: Open-Meteo API URL");
	if (FileContains("../ElevationService.cpp",
		"api.open-meteo.com/v1/elevation"))
		PASS();
	else
		FAIL("missing API URL");
}

static void
test_impl_curl_buffer()
{
	TEST("ElevationService.cpp: CurlBuffer struct");
	if (FileContains("../ElevationService.cpp", "CurlBuffer"))
		PASS();
	else
		FAIL("missing CurlBuffer");
}

static void
test_impl_batch_limit()
{
	TEST("ElevationService.cpp: batch limit 100");
	if (FileContains("../ElevationService.cpp", "kMaxCoordsPerRequest = 100"))
		PASS();
	else
		FAIL("missing batch limit");
}

static void
test_impl_curl_timeout()
{
	TEST("ElevationService.cpp: curl timeout set");
	if (FileContains("../ElevationService.cpp", "CURLOPT_TIMEOUT"))
		PASS();
	else
		FAIL("missing curl timeout");
}

static void
test_impl_json_elevation_key()
{
	TEST("ElevationService.cpp: parses elevation array");
	if (FileContains("../ElevationService.cpp", "\"elevation\""))
		PASS();
	else
		FAIL("missing elevation key parsing");
}

static void
test_impl_interpolate_call()
{
	TEST("ElevationService.cpp: calls InterpolatePoints");
	if (FileContains("../ElevationService.cpp", "InterpolatePoints"))
		PASS();
	else
		FAIL("missing InterpolatePoints call");
}

static void
test_impl_haversine_call()
{
	TEST("ElevationService.cpp: calls HaversineDistance");
	if (FileContains("../ElevationService.cpp", "HaversineDistance"))
		PASS();
	else
		FAIL("missing HaversineDistance call");
}

static void
test_impl_bjson_array_pattern()
{
	TEST("ElevationService.cpp: BJson array indexing");
	// BJson arrays use string keys "0","1","2"...
	if (FileContains("../ElevationService.cpp", "FindDouble"))
		PASS();
	else
		FAIL("missing BJson array double parsing");
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== ElevationService Pattern Tests ===\n\n");

	// Header
	test_header_guard();
	test_header_fetch_elevations();
	test_header_build_terrain();
	test_header_includes_los();

	// Implementation
	test_impl_curl_include();
	test_impl_bjson_include();
	test_impl_api_url();
	test_impl_curl_buffer();
	test_impl_batch_limit();
	test_impl_curl_timeout();
	test_impl_json_elevation_key();
	test_impl_interpolate_call();
	test_impl_haversine_call();
	test_impl_bjson_array_pattern();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
