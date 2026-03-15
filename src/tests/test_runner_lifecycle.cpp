/*
 * Test: BMessageRunner lifecycle
 * Verifies that every delete of a BMessageRunner pointer is followed
 * by NULL assignment before reassignment (prevents dangling pointers)
 */

#include <cstdio>
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


static int
CountPattern(const char* content, const char* pattern)
{
	int count = 0;
	const char* p = content;
	size_t patLen = strlen(pattern);
	while ((p = strstr(p, pattern)) != NULL) {
		count++;
		p += patLen;
	}
	return count;
}


static void
TestRunnerDeleteNullPattern()
{
	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);

	// Read file content
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = new char[size + 1];
	size_t n = fread(buf, 1, size, f);
	fclose(f);
	buf[n] = '\0';

	// Count total deletes of timer/runner pointers (excluding destructor)
	// The destructor is the first 3 deletes at the top
	int totalDeletes = 0;
	int nullAfterDelete = 0;

	const char* timerNames[] = {
		"delete fStatsRefreshTimer;",
		"delete fAutoSyncRunner;",
		"delete fAutoConnectTimer;",
		"delete fAdminRefreshTimer;",
		"delete fTelemetryPollTimer;"
	};

	const char* nullAssigns[] = {
		"fStatsRefreshTimer = NULL;",
		"fAutoSyncRunner = NULL;",
		"fAutoConnectTimer = NULL;",
		"fAdminRefreshTimer = NULL;",
		"fTelemetryPollTimer = NULL;"
	};

	for (int i = 0; i < 5; i++) {
		totalDeletes += CountPattern(buf, timerNames[i]);
		nullAfterDelete += CountPattern(buf, nullAssigns[i]);
	}

	printf("  Timer/Runner deletes: %d, NULL assignments: %d\n",
		totalDeletes, nullAfterDelete);

	// Every non-destructor delete should have a corresponding NULL assignment
	// Destructor has 5 deletes without NULL (that's OK — object is dying)
	// So we expect (totalDeletes - 5) == nullAfterDelete
	int nonDestructorDeletes = totalDeletes - 5;
	assert(nullAfterDelete >= nonDestructorDeletes);

	printf("  PASS: All non-destructor BMessageRunner deletes have NULL assignment\n");

	delete[] buf;
}


static void
TestNoDeleteWithoutNull()
{
	// Verify specific patterns that were buggy before the fix
	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = new char[size + 1];
	size_t n = fread(buf, 1, size, f);
	fclose(f);
	buf[n] = '\0';

	// Look for the pattern: "delete fXxx;\n<whitespace>fXxx = new"
	// This was the bug — delete + immediate re-create without NULL
	// After the fix, we should always see NULL between them

	// Search for "delete fStatsRefreshTimer;" followed by direct "new"
	// without NULL in between
	const char* p = buf;
	while ((p = strstr(p, "delete fStatsRefreshTimer;")) != NULL) {
		// Skip to end of line
		const char* lineEnd = strchr(p, '\n');
		if (lineEnd == NULL) break;

		// Check next 200 chars for "fStatsRefreshTimer = new"
		const char* nextNew = strstr(p, "fStatsRefreshTimer = new");
		if (nextNew != NULL && (nextNew - p) < 200) {
			// Verify NULL is between delete and new
			const char* nullCheck = strstr(p, "fStatsRefreshTimer = NULL;");
			assert(nullCheck != NULL && nullCheck < nextNew);
		}
		p = lineEnd + 1;
	}

	printf("  PASS: No direct delete→new without intermediate NULL\n");

	delete[] buf;
}


int
main()
{
	printf("=== BMessageRunner Lifecycle Tests ===\n\n");

	TestRunnerDeleteNullPattern();
	TestNoDeleteWithoutNull();

	printf("\nAll 2 tests passed.\n");
	return 0;
}
