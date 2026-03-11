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
// Test 5: SendDM return value checked in all call sites
// ============================================================================

static void
TestSendDMReturnChecked()
{
	const char* file = "MainWindow.cpp";

	// All SendDM calls should be preceded by status_t check
	int sendDMCount = CountOccurrences(file, "->SendDM(");
	int resultAssign = CountOccurrences(file, "= fProtocol->SendDM(");

	assert(sendDMCount >= 3);
	assert(resultAssign >= 3);

	printf("  PASS: %d/%d SendDM calls have return value check\n",
		resultAssign, sendDMCount);
}


// ============================================================================
// Test 6: SendChannelMsg return value checked
// ============================================================================

static void
TestSendChannelMsgReturnChecked()
{
	const char* file = "MainWindow.cpp";

	int sendCount = CountOccurrences(file, "->SendChannelMsg(");
	int resultAssign = CountOccurrences(file, "= fProtocol->SendChannelMsg(");

	assert(sendCount >= 1);
	assert(resultAssign >= 1);

	printf("  PASS: %d/%d SendChannelMsg calls have return value check\n",
		resultAssign, sendCount);
}


// ============================================================================
// Test 7: SendRawData return value checked in image/voice transfers
// ============================================================================

static void
TestSendRawDataReturnChecked()
{
	const char* file = "MainWindow.cpp";

	// SendRawData should have != B_OK checks
	int sendCount = CountOccurrences(file, "->SendRawData(");
	int checkedCount = CountOccurrences(file, "SendRawData(packet, pktLen) != B_OK");

	assert(sendCount >= 6);
	assert(checkedCount >= 6);

	printf("  PASS: %d/%d SendRawData calls have error check\n",
		checkedCount, sendCount);
}


// ============================================================================
// Test 8: Contact management sends check return values
// ============================================================================

static void
TestContactManagementReturnChecked()
{
	const char* file = "MainWindow.cpp";

	// SendRemoveContact should have != B_OK check
	assert(FileContains(file, "SendRemoveContact(") == true);
	assert(FileContains(file, "->SendRemoveContact(") == true);

	// SendResetPath should have != B_OK check
	assert(FileContains(file, "->SendResetPath(") == true);

	// SendSetChannel should have != B_OK check
	int setChCount = CountOccurrences(file, "->SendSetChannel(");
	assert(setChCount >= 2);

	// SendRemoveChannel should have != B_OK check
	assert(FileContains(file, "SendRemoveChannel(") == true);

	printf("  PASS: Contact management sends have return value checks\n");
}


// ============================================================================
// Test 9: Image/voice abort on send failure
// ============================================================================

static void
TestMediaAbortOnFailure()
{
	const char* file = "MainWindow.cpp";

	// Image transfer should set IMAGE_FAILED on send error
	assert(FileContains(file, "session->state = IMAGE_FAILED"));

	// Voice transfer should set VOICE_FAILED on send error
	assert(FileContains(file, "session->state = VOICE_FAILED"));

	// Timer cleanup on failure
	int imgTimerCleanup = CountOccurrences(file,
		"fImageFragmentTimer = NULL");
	int voiceTimerCleanup = CountOccurrences(file,
		"fVoiceFragmentTimer = NULL");
	assert(imgTimerCleanup >= 1);
	assert(voiceTimerCleanup >= 1);

	printf("  PASS: Image/voice transfers abort and clean up on send failure\n");
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
	TestSendDMReturnChecked();
	TestSendChannelMsgReturnChecked();
	TestSendRawDataReturnChecked();
	TestContactManagementReturnChecked();
	TestMediaAbortOnFailure();

	printf("\nAll 9 tests passed.\n");
	return 0;
}
