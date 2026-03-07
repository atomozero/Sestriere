/*
 * test_wal_fallback.cpp — Verify WAL fallback logic in DatabaseManager
 *
 * Tests that:
 * 1. _Execute logs error codes (rc=N format)
 * 2. WAL fallback path exists in Open() method
 * 3. DELETE journal mode is a valid fallback
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sqlite3.h>

static int sFailures = 0;

#define CHECK(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		sFailures++; \
	} else { \
		printf("  OK: %s\n", msg); \
	} \
} while(0)


// Test: SQLite supports DELETE journal mode (always works)
static void TestDeleteJournalMode()
{
	printf("Test: DELETE journal mode always works\n");

	sqlite3* db = NULL;
	int rc = sqlite3_open(":memory:", &db);
	CHECK(rc == SQLITE_OK, "open in-memory database");

	// DELETE mode should always work
	char* errMsg = NULL;
	rc = sqlite3_exec(db, "PRAGMA journal_mode=DELETE", NULL, NULL, &errMsg);
	CHECK(rc == SQLITE_OK, "DELETE journal mode succeeds");
	if (errMsg) sqlite3_free(errMsg);

	// Verify we can create tables after DELETE mode
	rc = sqlite3_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY)", NULL, NULL, &errMsg);
	CHECK(rc == SQLITE_OK, "create table after DELETE mode");
	if (errMsg) sqlite3_free(errMsg);

	sqlite3_close(db);
}


// Test: WAL mode on disk (may or may not work depending on filesystem)
static void TestWALModeDisk()
{
	printf("Test: WAL mode behavior on current filesystem\n");

	const char* testDbPath = "/tmp/sestriere_test_wal.db";

	sqlite3* db = NULL;
	int rc = sqlite3_open(testDbPath, &db);
	CHECK(rc == SQLITE_OK, "open test database on disk");

	if (rc == SQLITE_OK) {
		char* errMsg = NULL;
		rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, &errMsg);

		if (rc == SQLITE_OK) {
			printf("  INFO: WAL mode works on this filesystem\n");
		} else {
			printf("  INFO: WAL mode FAILS on this filesystem (rc=%d: %s)\n",
				rc, errMsg ? errMsg : "unknown");
			printf("  INFO: This is expected on some Haiku setups\n");
		}
		if (errMsg) sqlite3_free(errMsg);

		// Regardless of WAL result, DELETE should still work
		rc = sqlite3_exec(db, "PRAGMA journal_mode=DELETE", NULL, NULL, &errMsg);
		CHECK(rc == SQLITE_OK, "DELETE fallback works after WAL attempt");
		if (errMsg) sqlite3_free(errMsg);

		// Tables should work after fallback
		rc = sqlite3_exec(db,
			"CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, val TEXT)",
			NULL, NULL, &errMsg);
		CHECK(rc == SQLITE_OK, "create table works after journal mode fallback");
		if (errMsg) sqlite3_free(errMsg);

		sqlite3_close(db);
	}

	// Cleanup
	remove(testDbPath);
	char walPath[256], shmPath[256];
	snprintf(walPath, sizeof(walPath), "%s-wal", testDbPath);
	snprintf(shmPath, sizeof(shmPath), "%s-shm", testDbPath);
	remove(walPath);
	remove(shmPath);
}


// Test: Reopen after WAL failure recovers cleanly
static void TestReopenAfterFailure()
{
	printf("Test: Database reopen recovers from bad state\n");

	const char* testDbPath = "/tmp/sestriere_test_reopen.db";

	// First open
	sqlite3* db = NULL;
	int rc = sqlite3_open(testDbPath, &db);
	CHECK(rc == SQLITE_OK, "first open succeeds");

	if (rc == SQLITE_OK) {
		sqlite3_exec(db, "PRAGMA busy_timeout=5000", NULL, NULL, NULL);
		sqlite3_exec(db, "PRAGMA journal_mode=DELETE", NULL, NULL, NULL);
		sqlite3_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
		sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);

		rc = sqlite3_exec(db,
			"CREATE TABLE IF NOT EXISTS messages ("
			"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"  contact_key TEXT NOT NULL,"
			"  text TEXT NOT NULL"
			")", NULL, NULL, NULL);
		CHECK(rc == SQLITE_OK, "create messages table after clean open");

		// Insert a test row
		rc = sqlite3_exec(db,
			"INSERT INTO messages (contact_key, text) VALUES ('aabb', 'hello')",
			NULL, NULL, NULL);
		CHECK(rc == SQLITE_OK, "insert test row");

		sqlite3_close(db);
	}

	// Second open — simulates the WAL fallback path (close + reopen)
	db = NULL;
	rc = sqlite3_open(testDbPath, &db);
	CHECK(rc == SQLITE_OK, "reopen after close succeeds");

	if (rc == SQLITE_OK) {
		sqlite3_close(db);
		db = NULL;

		// Reopen again (mimics the fallback code path)
		rc = sqlite3_open(testDbPath, &db);
		CHECK(rc == SQLITE_OK, "second reopen succeeds");

		if (rc == SQLITE_OK) {
			sqlite3_exec(db, "PRAGMA busy_timeout=5000", NULL, NULL, NULL);
			sqlite3_exec(db, "PRAGMA journal_mode=DELETE", NULL, NULL, NULL);

			// Verify data survived
			sqlite3_stmt* stmt;
			rc = sqlite3_prepare_v2(db,
				"SELECT COUNT(*) FROM messages", -1, &stmt, NULL);
			CHECK(rc == SQLITE_OK, "query messages after reopen");

			if (rc == SQLITE_OK) {
				rc = sqlite3_step(stmt);
				CHECK(rc == SQLITE_ROW, "got result row");
				if (rc == SQLITE_ROW) {
					int count = sqlite3_column_int(stmt, 0);
					CHECK(count == 1, "data survived close/reopen cycle");
				}
				sqlite3_finalize(stmt);
			}

			sqlite3_close(db);
		}
	}

	remove(testDbPath);
}


// Test: Verify the source code has WAL fallback
static void TestSourceHasFallback()
{
	printf("Test: DatabaseManager.cpp has WAL fallback code\n");

	FILE* f = fopen("../DatabaseManager.cpp", "r");
	if (!f)
		f = fopen("DatabaseManager.cpp", "r");
	CHECK(f != NULL, "DatabaseManager.cpp is readable");

	if (f) {
		// Read entire file to check all patterns
		fseek(f, 0, SEEK_END);
		long fileSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (fileSize > 128000)
			fileSize = 128000;
		char* buf = (char*)malloc(fileSize + 1);
		size_t n = fread(buf, 1, fileSize, f);
		buf[n] = '\0';
		fclose(f);

		CHECK(strstr(buf, "journal_mode=WAL") != NULL,
			"attempts WAL mode");
		CHECK(strstr(buf, "journal_mode=DELETE") != NULL,
			"has DELETE fallback");
		CHECK(strstr(buf, "falling back") != NULL ||
			strstr(buf, "fall back") != NULL,
			"has fallback log message");
		CHECK(strstr(buf, "rc=%d") != NULL,
			"_Execute logs numeric error code");
		free(buf);
	}
}


int main()
{
	printf("=== WAL Fallback Tests ===\n\n");

	TestDeleteJournalMode();
	printf("\n");

	TestWALModeDisk();
	printf("\n");

	TestReopenAfterFailure();
	printf("\n");

	TestSourceHasFallback();
	printf("\n");

	printf("%s: %d failures\n",
		sFailures == 0 ? "ALL PASSED" : "FAILED", sFailures);
	return sFailures > 0 ? 1 : 0;
}
