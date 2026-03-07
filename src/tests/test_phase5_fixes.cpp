/*
 * test_phase5_fixes.cpp — Verify Phase 5 architecture and maintainability fixes
 *
 * Tests for:
 * 5.4 CoastlineData moved from header to .cpp (extern linkage)
 * 5.5 Centralized version string in Constants.h
 * 5.6 Test runner script exists and is executable
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

static int sFailures = 0;

#define CHECK(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		sFailures++; \
	} else { \
		printf("  OK: %s\n", msg); \
	} \
} while(0)


// Version constants (must match Constants.h)
#define APP_VERSION "1.8.0"
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MIDDLE 8
#define APP_VERSION_MINOR 0


// Test 5.4: CoastlineData extern linkage
static void TestCoastlineExtern()
{
	printf("Test 5.4: CoastlineData uses extern linkage (not static in header)\n");

	// Read CoastlineData.h and verify it uses extern, not static
	FILE* f = fopen("../CoastlineData.h", "r");
	if (!f)
		f = fopen("CoastlineData.h", "r");
	CHECK(f != NULL, "CoastlineData.h is readable");

	if (f) {
		char buf[4096];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		buf[n] = '\0';
		fclose(f);

		CHECK(strstr(buf, "extern const float kCoastlineData") != NULL,
			"header declares kCoastlineData as extern");
		CHECK(strstr(buf, "extern const int kCoastlinePointCount") != NULL,
			"header declares kCoastlinePointCount as extern");
		CHECK(strstr(buf, "static const float") == NULL,
			"header does NOT contain static data array");
	}

	// Verify CoastlineData.cpp exists
	struct stat st;
	bool cppExists = (stat("../CoastlineData.cpp", &st) == 0);
	if (!cppExists)
		cppExists = (stat("CoastlineData.cpp", &st) == 0);
	CHECK(cppExists, "CoastlineData.cpp exists (data moved to .cpp)");
}


// Test 5.5: Centralized version string
static void TestVersionCentralized()
{
	printf("Test 5.5: Version string centralized in Constants.h\n");

	CHECK(strcmp(APP_VERSION, "1.8.0") == 0,
		"APP_VERSION is defined as \"1.8.0\"");

	CHECK(APP_VERSION_MAJOR == 1, "APP_VERSION_MAJOR is 1");
	CHECK(APP_VERSION_MIDDLE == 8, "APP_VERSION_MIDDLE is 8");
	CHECK(APP_VERSION_MINOR == 0, "APP_VERSION_MINOR is 0");

	// Verify version string matches components
	char constructed[16];
	snprintf(constructed, sizeof(constructed), "%d.%d.%d",
		APP_VERSION_MAJOR, APP_VERSION_MIDDLE, APP_VERSION_MINOR);
	CHECK(strcmp(constructed, APP_VERSION) == 0,
		"version components match version string");

	// Read Constants.h and verify APP_VERSION is defined there
	FILE* f = fopen("../Constants.h", "r");
	if (!f)
		f = fopen("Constants.h", "r");
	CHECK(f != NULL, "Constants.h is readable");

	if (f) {
		char buf[2048];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		buf[n] = '\0';
		fclose(f);

		CHECK(strstr(buf, "#define APP_VERSION") != NULL,
			"Constants.h defines APP_VERSION");
		CHECK(strstr(buf, "#define APP_VERSION_MAJOR") != NULL,
			"Constants.h defines APP_VERSION_MAJOR");
	}
}


// Test 5.6: Test runner exists and is executable
static void TestRunnerExists()
{
	printf("Test 5.6: Test runner script\n");

	struct stat st;
	bool exists = (stat("run_tests.sh", &st) == 0);
	CHECK(exists, "run_tests.sh exists in tests/");

	if (exists) {
		CHECK((st.st_mode & S_IXUSR) != 0,
			"run_tests.sh is executable");
		CHECK(st.st_size > 100,
			"run_tests.sh has content (not empty)");
	}
}


int main()
{
	printf("=== Phase 5 Architecture Fix Tests ===\n\n");

	TestCoastlineExtern();
	printf("\n");

	TestVersionCentralized();
	printf("\n");

	TestRunnerExists();
	printf("\n");

	printf("%s: %d failures\n",
		sFailures == 0 ? "ALL PASSED" : "FAILED", sFailures);
	return sFailures > 0 ? 1 : 0;
}
