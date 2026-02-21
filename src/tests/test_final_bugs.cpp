/*
 * Test: Final 3 bugs — strtok_r, timer NULL cleanup
 * Verifies non-reentrant strtok() replaced and destructor hygiene.
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
// Test 1: No bare strtok() — must use strtok_r()
// ============================================================================

static void
TestNoStrtok()
{
	const char* files[] = {
		"DatabaseManager.cpp",
		"MainWindow.cpp",
		NULL
	};

	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		assert(fp != NULL);

		char line[1024];
		int lineNum = 0;
		while (fgets(line, sizeof(line), fp)) {
			lineNum++;
			// Find strtok( but not strtok_r(
			char* pos = strstr(line, "strtok(");
			if (pos != NULL) {
				// Verify it's not strtok_r
				if (pos == line || *(pos - 1) != '_') {
					fprintf(stderr,
						"FAIL: Non-reentrant strtok() at %s:%d: %s",
						files[f], lineNum, line);
					fclose(fp);
					assert(0 && "Found non-reentrant strtok()");
				}
			}
		}
		fclose(fp);
	}

	printf("  PASS: No non-reentrant strtok() calls remain\n");
}


// ============================================================================
// Test 2: strtok_r() used with saveptr
// ============================================================================

static void
TestStrtokRUsed()
{
	const char* files[] = {
		"DatabaseManager.cpp",
		"MainWindow.cpp",
		NULL
	};

	for (int f = 0; files[f] != NULL; f++) {
		FILE* fp = OpenSource(files[f]);
		assert(fp != NULL);

		char line[1024];
		bool foundStrtokR = false;
		bool foundSaveptr = false;
		while (fgets(line, sizeof(line), fp)) {
			if (strstr(line, "strtok_r(") != NULL)
				foundStrtokR = true;
			if (strstr(line, "saveptr") != NULL)
				foundSaveptr = true;
		}
		fclose(fp);

		assert(foundStrtokR && "Expected strtok_r() usage");
		assert(foundSaveptr && "Expected saveptr variable for strtok_r");
	}

	printf("  PASS: strtok_r() with saveptr used in both files\n");
}


// ============================================================================
// Test 3: MissionControlWindow destructor NULLs timers
// ============================================================================

static void
TestMissionControlTimerNull()
{
	FILE* fp = OpenSource("MissionControlWindow.cpp");
	assert(fp != NULL);

	char line[1024];
	bool inDestructor = false;
	int deleteCount = 0;
	int nullCount = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "~MissionControlWindow()") != NULL)
			inDestructor = true;
		if (inDestructor) {
			if (strstr(line, "delete f") != NULL
				&& strstr(line, "Timer") != NULL)
				deleteCount++;
			if (strstr(line, "= NULL") != NULL)
				nullCount++;
			// End of destructor
			if (line[0] == '}' && inDestructor && deleteCount > 0)
				break;
		}
	}
	fclose(fp);

	printf("    MissionControlWindow dtor: %d deletes, %d NULLs\n",
		deleteCount, nullCount);
	assert(deleteCount == 3 && "Expected 3 timer deletes");
	assert(nullCount == 3 && "Expected 3 NULL assignments after delete");

	printf("  PASS: MissionControlWindow destructor NULLs all 3 timers\n");
}


// ============================================================================
// Test 4: NetworkMapWindow destructor NULLs timers
// ============================================================================

static void
TestNetworkMapTimerNull()
{
	FILE* fp = OpenSource("NetworkMapWindow.cpp");
	assert(fp != NULL);

	char line[1024];
	bool inDestructor = false;
	int deleteCount = 0;
	int nullCount = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "~NetworkMapWindow()") != NULL)
			inDestructor = true;
		if (inDestructor) {
			if (strstr(line, "delete f") != NULL
				&& strstr(line, "Timer") != NULL)
				deleteCount++;
			if (strstr(line, "= NULL") != NULL)
				nullCount++;
			if (line[0] == '}' && inDestructor && deleteCount > 0)
				break;
		}
	}
	fclose(fp);

	printf("    NetworkMapWindow dtor: %d deletes, %d NULLs\n",
		deleteCount, nullCount);
	assert(deleteCount == 3 && "Expected 3 timer deletes");
	assert(nullCount == 3 && "Expected 3 NULL assignments after delete");

	printf("  PASS: NetworkMapWindow destructor NULLs all 3 timers\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Final Bug Fixes Tests ===\n\n");

	TestNoStrtok();
	TestStrtokRUsed();
	TestMissionControlTimerNull();
	TestNetworkMapTimerNull();

	printf("\nAll 4 tests passed.\n");
	return 0;
}
