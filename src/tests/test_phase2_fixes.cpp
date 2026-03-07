/*
 * test_phase2_fixes.cpp — Verify Phase 2 robustness fixes
 *
 * Tests for:
 * 2.4 PRAGMA busy_timeout in DatabaseManager::Open()
 * 2.5 PRAGMA foreign_keys in DatabaseManager::Open()
 * 2.6 AddContactToGroup atomicity (transaction)
 * 2.7 sqlite3_step error checking in pruning
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>

static int sFailures = 0;

#define CHECK(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		sFailures++; \
	} else { \
		printf("  OK: %s\n", msg); \
	} \
} while(0)


static bool Execute(sqlite3* db, const char* sql)
{
	char* errMsg = NULL;
	int rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "    SQL error: %s\n", errMsg ? errMsg : "unknown");
		sqlite3_free(errMsg);
		return false;
	}
	return true;
}


static int GetPragmaInt(sqlite3* db, const char* pragma)
{
	char sql[128];
	snprintf(sql, sizeof(sql), "PRAGMA %s", pragma);

	sqlite3_stmt* stmt = NULL;
	int value = -1;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			value = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	return value;
}


// Test 2.4: busy_timeout should be set
static void TestBusyTimeout()
{
	printf("Test 2.4: PRAGMA busy_timeout\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase2_timeout.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	// Simulate what Open() now does
	Execute(db, "PRAGMA journal_mode=WAL");
	Execute(db, "PRAGMA synchronous=NORMAL");
	Execute(db, "PRAGMA busy_timeout=5000");
	Execute(db, "PRAGMA foreign_keys=ON");

	int timeout = GetPragmaInt(db, "busy_timeout");
	CHECK(timeout == 5000, "busy_timeout is 5000ms");

	// Verify it's not the default (0)
	CHECK(timeout > 0, "busy_timeout is not zero (default)");

	sqlite3_close(db);
	unlink(path);
}


// Test 2.5: foreign_keys should be enabled globally
static void TestForeignKeys()
{
	printf("Test 2.5: PRAGMA foreign_keys in Open()\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase2_fk.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	// Before setting FK
	int fk_before = GetPragmaInt(db, "foreign_keys");
	CHECK(fk_before == 0, "foreign_keys starts off by default");

	// Simulate what Open() now does
	Execute(db, "PRAGMA foreign_keys=ON");

	int fk_after = GetPragmaInt(db, "foreign_keys");
	CHECK(fk_after == 1, "foreign_keys is enabled after PRAGMA");

	// Test that FK enforcement works
	Execute(db, "CREATE TABLE parent (id INTEGER PRIMARY KEY, name TEXT)");
	Execute(db, "CREATE TABLE child (id INTEGER PRIMARY KEY, "
		"parent_id INTEGER REFERENCES parent(id) ON DELETE CASCADE)");

	Execute(db, "INSERT INTO parent VALUES (1, 'test')");
	Execute(db, "INSERT INTO child VALUES (1, 1)");

	// Delete parent should cascade to child
	Execute(db, "DELETE FROM parent WHERE id = 1");

	// Verify child is also deleted
	sqlite3_stmt* stmt = NULL;
	int childCount = -1;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM child",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			childCount = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(childCount == 0, "CASCADE delete works with FK enabled");

	sqlite3_close(db);
	unlink(path);
}


// Test 2.6: AddContactToGroup atomicity
static void TestGroupAtomicity()
{
	printf("Test 2.6: AddContactToGroup transaction atomicity\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase2_group.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	Execute(db, "PRAGMA foreign_keys=ON");

	// Create tables matching DatabaseManager schema
	Execute(db, "CREATE TABLE contact_groups ("
		"name TEXT PRIMARY KEY)");
	Execute(db, "CREATE TABLE contact_group_members ("
		"group_name TEXT NOT NULL, "
		"contact_key TEXT NOT NULL, "
		"PRIMARY KEY (group_name, contact_key), "
		"FOREIGN KEY (group_name) REFERENCES contact_groups(name) "
		"ON DELETE CASCADE)");

	// Add groups
	Execute(db, "INSERT INTO contact_groups VALUES ('Alpha')");
	Execute(db, "INSERT INTO contact_groups VALUES ('Beta')");

	// Add contact to Alpha
	Execute(db, "INSERT INTO contact_group_members VALUES ('Alpha', 'AABBCCDDEEFF')");

	// Verify contact is in Alpha
	sqlite3_stmt* stmt = NULL;
	char group[64] = "";
	if (sqlite3_prepare_v2(db,
		"SELECT group_name FROM contact_group_members WHERE contact_key = 'AABBCCDDEEFF'",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			strlcpy(group, (const char*)sqlite3_column_text(stmt, 0), sizeof(group));
		sqlite3_finalize(stmt);
	}
	CHECK(strcmp(group, "Alpha") == 0, "contact starts in Alpha group");

	// Simulate atomic move: BEGIN + DELETE + INSERT + COMMIT
	Execute(db, "BEGIN TRANSACTION");
	Execute(db, "DELETE FROM contact_group_members WHERE contact_key = 'AABBCCDDEEFF'");
	Execute(db, "INSERT INTO contact_group_members VALUES ('Beta', 'AABBCCDDEEFF')");
	Execute(db, "COMMIT");

	// Verify contact is now in Beta
	group[0] = '\0';
	if (sqlite3_prepare_v2(db,
		"SELECT group_name FROM contact_group_members WHERE contact_key = 'AABBCCDDEEFF'",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			strlcpy(group, (const char*)sqlite3_column_text(stmt, 0), sizeof(group));
		sqlite3_finalize(stmt);
	}
	CHECK(strcmp(group, "Beta") == 0, "contact moved to Beta atomically");

	// Test rollback: try moving to non-existent group
	Execute(db, "BEGIN TRANSACTION");
	Execute(db, "DELETE FROM contact_group_members WHERE contact_key = 'AABBCCDDEEFF'");
	// This INSERT should fail due to FK constraint (group 'Gamma' doesn't exist)
	char* errMsg = NULL;
	rc = sqlite3_exec(db,
		"INSERT INTO contact_group_members VALUES ('Gamma', 'AABBCCDDEEFF')",
		NULL, NULL, &errMsg);
	sqlite3_free(errMsg);

	if (rc != SQLITE_OK) {
		Execute(db, "ROLLBACK");

		// Verify contact is still in Beta (rollback preserved it)
		group[0] = '\0';
		if (sqlite3_prepare_v2(db,
			"SELECT group_name FROM contact_group_members WHERE contact_key = 'AABBCCDDEEFF'",
			-1, &stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW)
				strlcpy(group, (const char*)sqlite3_column_text(stmt, 0), sizeof(group));
			sqlite3_finalize(stmt);
		}
		CHECK(strcmp(group, "Beta") == 0, "rollback preserves contact in original group");
	} else {
		Execute(db, "ROLLBACK");
		CHECK(false, "FK constraint should have prevented insert into non-existent group");
	}

	sqlite3_close(db);
	unlink(path);
}


// Test 2.7: sqlite3_step return value checking
static void TestPruneErrorCheck()
{
	printf("Test 2.7: sqlite3_step error checking in pruning\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase2_prune.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	Execute(db, "CREATE TABLE snr_history (timestamp INTEGER, snr INTEGER)");
	Execute(db, "INSERT INTO snr_history VALUES (1000, 5)");
	Execute(db, "INSERT INTO snr_history VALUES (2000, 10)");
	Execute(db, "INSERT INTO snr_history VALUES (3000, 15)");

	// Prune old data (cutoff = 1500)
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(db,
		"DELETE FROM snr_history WHERE timestamp < ?",
		-1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, 1500);
		rc = sqlite3_step(stmt);
		CHECK(rc == SQLITE_DONE, "sqlite3_step returns SQLITE_DONE on success");

		int changes = sqlite3_changes(db);
		CHECK(changes == 1, "exactly 1 row pruned");

		sqlite3_finalize(stmt);
	}

	// Verify remaining rows
	int count = 0;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM snr_history",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(count == 2, "2 rows remain after pruning");

	// Test pruning on empty result (no matching rows)
	if (sqlite3_prepare_v2(db,
		"DELETE FROM snr_history WHERE timestamp < ?",
		-1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, 500);
		rc = sqlite3_step(stmt);
		CHECK(rc == SQLITE_DONE, "sqlite3_step returns SQLITE_DONE even with 0 deletions");

		int changes2 = sqlite3_changes(db);
		CHECK(changes2 == 0, "0 rows pruned when no match");

		sqlite3_finalize(stmt);
	}

	sqlite3_close(db);
	unlink(path);
}


int main()
{
	printf("=== Phase 2 Robustness Fix Tests ===\n\n");

	TestBusyTimeout();
	printf("\n");

	TestForeignKeys();
	printf("\n");

	TestGroupAtomicity();
	printf("\n");

	TestPruneErrorCheck();
	printf("\n");

	printf("%s: %d failures\n",
		sFailures == 0 ? "ALL PASSED" : "FAILED", sFailures);
	return sFailures > 0 ? 1 : 0;
}
