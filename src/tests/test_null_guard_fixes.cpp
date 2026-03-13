/*
 * test_null_guard_fixes.cpp — Tests for safety fixes:
 * 1. Window() null dereference guards in ChatView, TopBarView, GrowingTextView
 * 2. Timer cleanup in MainWindow destructor
 * 3. Consistent sqlite3_close_v2 usage in DatabaseManager
 * 4. TopBarView PostMessage leak fix (no 'new BMessage' in MouseDown)
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int gPassed = 0;
static int gFailed = 0;

static void Check(bool cond, const char* name) {
	if (cond) {
		printf("  PASS: %s\n", name);
		gPassed++;
	} else {
		printf("  FAIL: %s\n", name);
		gFailed++;
	}
}


// ---------- file search helpers ----------

static bool FileContains(const char* path, const char* needle) {
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);
	bool found = strstr(buf, needle) != NULL;
	free(buf);
	return found;
}

static bool FileContainsBetween(const char* path,
	const char* startMarker, const char* endMarker, const char* needle)
{
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	const char* start = strstr(buf, startMarker);
	if (!start) { free(buf); return false; }
	const char* end = strstr(start + strlen(startMarker), endMarker);
	if (!end) { free(buf); return false; }

	size_t regionLen = end - start;
	char* region = (char*)malloc(regionLen + 1);
	memcpy(region, start, regionLen);
	region[regionLen] = 0;
	bool found = strstr(region, needle) != NULL;
	free(region);
	free(buf);
	return found;
}


// ---------- Test: ChatView Window() null guard ----------

static void TestChatViewWindowGuard() {
	printf("\n--- ChatView Window() null guard ---\n");

	Check(FileContainsBetween(
		"../ChatView.cpp",
		"::MouseDown",
		"B_SECONDARY_MOUSE_BUTTON",
		"Window() == NULL"),
		"ChatView::MouseDown checks Window() != NULL before dereference");

	Check(FileContainsBetween(
		"../ChatView.cpp",
		"::MouseDown",
		"B_SECONDARY_MOUSE_BUTTON",
		"return"),
		"ChatView::MouseDown returns early if Window() is NULL");
}


// ---------- Test: TopBarView Window() null guard ----------

static void TestTopBarViewWindowGuard() {
	printf("\n--- TopBarView Window() null guard ---\n");

	Check(FileContainsBetween(
		"../TopBarView.cpp",
		"::MouseDown",
		"fNetworkMapRect",
		"Window()"),
		"TopBarView::MouseDown checks Window() before any PostMessage");

	Check(FileContainsBetween(
		"../TopBarView.cpp",
		"::MouseDown",
		"fNetworkMapRect",
		"return"),
		"TopBarView::MouseDown returns early if Window() is NULL");

	// Verify no leaked BMessage allocations (was: new BMessage(MSG_SHOW_STATS))
	Check(!FileContainsBetween(
		"../TopBarView.cpp",
		"::MouseDown",
		"BView::MouseDown",
		"new BMessage"),
		"TopBarView::MouseDown has no 'new BMessage' leak");

	// Verify uses local window variable
	Check(FileContainsBetween(
		"../TopBarView.cpp",
		"::MouseDown",
		"BView::MouseDown",
		"window->PostMessage"),
		"TopBarView::MouseDown uses cached window pointer");
}


// ---------- Test: GrowingTextView Window() null guard ----------

static void TestGrowingTextViewWindowGuard() {
	printf("\n--- GrowingTextView Window() null guard ---\n");

	Check(FileContainsBetween(
		"../GrowingTextView.cpp",
		"::KeyDown",
		"B_SHIFT_KEY",
		"Window()"),
		"GrowingTextView::KeyDown checks Window() before CurrentMessage");

	Check(FileContainsBetween(
		"../GrowingTextView.cpp",
		"::KeyDown",
		"B_SHIFT_KEY",
		"return"),
		"GrowingTextView::KeyDown returns early if Window() is NULL");

	Check(FileContainsBetween(
		"../GrowingTextView.cpp",
		"::KeyDown",
		"B_SHIFT_KEY",
		"window->CurrentMessage"),
		"GrowingTextView::KeyDown uses cached window pointer");

	// _NotifyModification already had guard
	Check(FileContainsBetween(
		"../GrowingTextView.cpp",
		"::_NotifyModification",
		"PostMessage",
		"Window() != NULL"),
		"GrowingTextView::_NotifyModification checks Window() != NULL");
}


// ---------- Test: Timer cleanup in MainWindow destructor ----------

static void TestTimerCleanup() {
	printf("\n--- MainWindow timer cleanup in destructor ---\n");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"::~MainWindow",
		"delete fProtocol",
		"delete fAdminRefreshTimer"),
		"Destructor deletes fAdminRefreshTimer");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"::~MainWindow",
		"delete fProtocol",
		"delete fTelemetryPollTimer"),
		"Destructor deletes fTelemetryPollTimer");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"::~MainWindow",
		"delete fProtocol",
		"delete fAutoConnectTimer"),
		"Destructor deletes fAutoConnectTimer");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"::~MainWindow",
		"delete fProtocol",
		"delete fStatsRefreshTimer"),
		"Destructor deletes fStatsRefreshTimer");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"::~MainWindow",
		"delete fProtocol",
		"delete fDeliveryCheckTimer"),
		"Destructor deletes fDeliveryCheckTimer");

	Check(FileContainsBetween(
		"../MainWindow.cpp",
		"::~MainWindow",
		"delete fProtocol",
		"delete fHandshakeTimer"),
		"Destructor deletes fHandshakeTimer");
}


// ---------- Test: sqlite3_close_v2 consistency ----------

static void TestSqliteCloseV2() {
	printf("\n--- DatabaseManager sqlite3_close_v2 consistency ---\n");

	Check(!FileContains(
		"../DatabaseManager.cpp",
		"sqlite3_close(fDB)"),
		"No bare sqlite3_close(fDB) — all use _v2 variant");

	Check(FileContains(
		"../DatabaseManager.cpp",
		"sqlite3_close_v2(fDB)"),
		"Uses sqlite3_close_v2(fDB) consistently");

	Check(FileContainsBetween(
		"../DatabaseManager.cpp",
		"Failed to open database",
		"return false",
		"sqlite3_close_v2"),
		"First open failure uses sqlite3_close_v2");

	Check(FileContainsBetween(
		"../DatabaseManager.cpp",
		"Failed to reopen database",
		"return false",
		"sqlite3_close_v2"),
		"Reopen failure uses sqlite3_close_v2");
}


int main() {
	printf("=== Safety Fixes Test Suite ===\n");

	TestChatViewWindowGuard();
	TestTopBarViewWindowGuard();
	TestGrowingTextViewWindowGuard();
	TestTimerCleanup();
	TestSqliteCloseV2();

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
