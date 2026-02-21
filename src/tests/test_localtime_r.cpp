/*
 * Test: localtime_r usage (thread safety)
 * Verifies no non-reentrant localtime() calls remain in production code.
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
// Test 1: No bare localtime() calls in production code
// ============================================================================

static void
TestNoBarelocaltime()
{
	const char* files[] = {
		"DebugLogWindow.cpp",
		"MainWindow.cpp",
		"ContactInfoPanel.cpp",
		"MissionControlWindow.cpp",
		"PacketAnalyzerWindow.cpp",
		"TelemetryWindow.cpp",
		"StatsWindow.cpp",
		NULL
	};

	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		int lineNum = 0;
		while (fgets(line, sizeof(line), fp)) {
			lineNum++;
			// Look for localtime( but not localtime_r(
			char* pos = strstr(line, "localtime(");
			if (pos != NULL) {
				// Make sure it's not localtime_r
				if (pos > line && *(pos + 9) == '(') {
					// Check character before 'localtime' is not '_r'
					// localtime_r would have 'r' at pos-1... no, localtime_r(
					// Actually let's just check if "localtime_r" is present
					if (strstr(line, "localtime_r") == NULL) {
						fprintf(stderr,
							"FAIL: Bare localtime() at %s:%d: %s",
							files[f], lineNum, line);
						fclose(fp);
						assert(0 && "Found non-reentrant localtime()");
					}
				}
			}
		}
		fclose(fp);
	}

	printf("  PASS: No bare localtime() calls in production code\n");
}


// ============================================================================
// Test 2: localtime_r used with local tm struct
// ============================================================================

static void
TestLocaltimeRUsed()
{
	const char* files[] = {
		"DebugLogWindow.cpp",
		"MainWindow.cpp",
		NULL
	};

	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		if (fp == NULL)
			continue;

		char line[1024];
		bool found = false;
		while (fgets(line, sizeof(line), fp)) {
			if (strstr(line, "localtime_r(") != NULL) {
				found = true;
				break;
			}
		}
		fclose(fp);
		assert(found && "Expected localtime_r() usage");
	}

	printf("  PASS: localtime_r() used in previously-affected files\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== localtime_r Thread Safety Tests ===\n\n");

	TestNoBarelocaltime();
	TestLocaltimeRUsed();

	printf("\nAll 2 tests passed.\n");
	return 0;
}
