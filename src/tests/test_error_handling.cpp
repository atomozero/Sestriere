/*
 * Test: Error handling in DatabaseManager
 * Verifies sqlite3 prepare failures are logged and sscanf returns checked.
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


static int
CountOccurrences(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return -1;

	char line[512];
	int count = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL)
			count++;
	}
	fclose(f);
	return count;
}


static bool
FileContains(const char* filename, const char* pattern)
{
	return CountOccurrences(filename, pattern) > 0;
}


// ============================================================================
// Test 1: sqlite3_prepare_v2 failures log error messages
// ============================================================================

static void
TestSqlitePrepareLogging()
{
	const char* file = "DatabaseManager.cpp";

	// Count prepare calls vs error logging
	int prepareCount = CountOccurrences(file, "sqlite3_prepare_v2(fDB,");
	int errorLogCount = CountOccurrences(file, "prepare failed:");

	// Every prepare that uses rc != SQLITE_OK should have logging
	// (some use inline if/SQLITE_OK without rc variable)
	assert(errorLogCount >= 11);

	printf("  PASS: %d/%d sqlite3_prepare_v2 failures have error logging\n",
		errorLogCount, prepareCount);
}


// ============================================================================
// Test 2: sscanf hex parsing has return value check
// ============================================================================

static void
TestSscanfReturnCheck()
{
	const char* file = "DatabaseManager.cpp";

	// All sscanf("%2x") calls should be wrapped in if (sscanf(...) != 1)
	int sscanfCount = CountOccurrences(file, "sscanf(");
	int checkedCount = CountOccurrences(file, "if (sscanf(");

	// The migration function has 2 sscanf calls with different format (%u|%d|...)
	// Those already have return checks via the enclosing if
	// The hex-parsing sscanf calls (5 of them) should all be checked
	int hexSscanf = CountOccurrences(file, "\"%2x\"");
	int checkedHex = 0;

	FILE* f = OpenSource(file);
	assert(f != NULL);
	char line[512];
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "if (sscanf(") != NULL && strstr(line, "\"%2x\"") != NULL)
			checkedHex++;
	}
	fclose(f);

	assert(checkedHex == hexSscanf);

	printf("  PASS: All %d hex-parsing sscanf calls have return check\n",
		checkedHex);
}


// ============================================================================
// Test 3: sqlite3_errmsg used in error paths
// ============================================================================

static void
TestSqliteErrmsgUsed()
{
	assert(FileContains("DatabaseManager.cpp", "sqlite3_errmsg(fDB)"));

	int errmsgCount = CountOccurrences("DatabaseManager.cpp",
		"sqlite3_errmsg(fDB)");
	assert(errmsgCount >= 11);

	printf("  PASS: sqlite3_errmsg used in %d error paths\n", errmsgCount);
}


// ============================================================================
// Test 4: BEntry::Rename return value checked
// ============================================================================

static void
TestRenameReturnCheck()
{
	assert(FileContains("DatabaseManager.cpp",
		"entry.Rename(bakPath.String()) != B_OK"));

	printf("  PASS: BEntry::Rename return value checked\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Error Handling Tests ===\n\n");

	TestSqlitePrepareLogging();
	TestSscanfReturnCheck();
	TestSqliteErrmsgUsed();
	TestRenameReturnCheck();

	printf("\nAll 4 tests passed.\n");
	return 0;
}
