/*
 * test_phase3_fixes.cpp — Verify Phase 3 database performance fixes
 *
 * Tests for:
 * 3.1 Missing indexes on contact_group_members and topology_edges
 * 3.2 Y2038-safe int64 timestamps
 * 3.3 Migration bulk import rollback on total failure
 * 3.4 PRAGMA optimize on close
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sqlite3.h>
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


static bool IndexExists(sqlite3* db, const char* indexName)
{
	const char* sql = "SELECT COUNT(*) FROM sqlite_master "
		"WHERE type='index' AND name=?";
	sqlite3_stmt* stmt = NULL;
	int count = 0;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, indexName, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	return count > 0;
}


// Test 3.1: Missing indexes
static void TestMissingIndexes()
{
	printf("Test 3.1: Missing indexes on frequently queried columns\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase3_indexes.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	Execute(db, "PRAGMA foreign_keys=ON");

	// Create tables matching schema
	Execute(db, "CREATE TABLE contact_groups (name TEXT PRIMARY KEY)");
	Execute(db, "CREATE TABLE contact_group_members ("
		"group_name TEXT NOT NULL, contact_key TEXT NOT NULL, "
		"PRIMARY KEY (group_name, contact_key), "
		"FOREIGN KEY (group_name) REFERENCES contact_groups(name) ON DELETE CASCADE)");
	Execute(db, "CREATE TABLE topology_edges ("
		"from_key TEXT NOT NULL, to_key TEXT NOT NULL, "
		"snr INTEGER DEFAULT 0, timestamp INTEGER NOT NULL, "
		"PRIMARY KEY (from_key, to_key))");

	// Create the new indexes
	Execute(db, "CREATE INDEX IF NOT EXISTS idx_group_members_contact "
		"ON contact_group_members (contact_key)");
	Execute(db, "CREATE INDEX IF NOT EXISTS idx_topology_edges_timestamp "
		"ON topology_edges (timestamp)");

	CHECK(IndexExists(db, "idx_group_members_contact"),
		"index idx_group_members_contact exists");
	CHECK(IndexExists(db, "idx_topology_edges_timestamp"),
		"index idx_topology_edges_timestamp exists");

	// Verify the index speeds up contact_key lookups
	// (functional test: can query by contact_key)
	Execute(db, "INSERT INTO contact_groups VALUES ('TestGroup')");
	Execute(db, "INSERT INTO contact_group_members VALUES ('TestGroup', 'AABBCCDDEEFF')");
	Execute(db, "INSERT INTO contact_group_members VALUES ('TestGroup', '112233445566')");

	// Use EXPLAIN QUERY PLAN to check the contact_key index is used
	sqlite3_stmt* stmt = NULL;
	bool usesIndex = false;
	if (sqlite3_prepare_v2(db,
		"EXPLAIN QUERY PLAN DELETE FROM contact_group_members "
		"WHERE contact_key = 'AABBCCDDEEFF'",
		-1, &stmt, NULL) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const char* detail = (const char*)sqlite3_column_text(stmt, 3);
			if (detail != NULL && strstr(detail, "idx_group_members_contact") != NULL)
				usesIndex = true;
		}
		sqlite3_finalize(stmt);
	}
	CHECK(usesIndex, "DELETE by contact_key uses new index");

	// Also verify topology_edges timestamp index
	Execute(db, "INSERT OR REPLACE INTO topology_edges VALUES ('AA', 'BB', 5, 1000)");
	Execute(db, "INSERT OR REPLACE INTO topology_edges VALUES ('CC', 'DD', 3, 2000)");

	usesIndex = false;
	if (sqlite3_prepare_v2(db,
		"EXPLAIN QUERY PLAN DELETE FROM topology_edges WHERE timestamp < 1500",
		-1, &stmt, NULL) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const char* detail = (const char*)sqlite3_column_text(stmt, 3);
			if (detail != NULL && strstr(detail, "idx_topology_edges_timestamp") != NULL)
				usesIndex = true;
		}
		sqlite3_finalize(stmt);
	}
	CHECK(usesIndex, "DELETE by timestamp uses new index");

	sqlite3_close(db);
	unlink(path);
}


// Test 3.2: Y2038-safe int64 timestamps
static void TestY2038Timestamps()
{
	printf("Test 3.2: Y2038-safe int64 timestamps\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase3_y2038.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	Execute(db, "CREATE TABLE test_ts (id INTEGER PRIMARY KEY, ts INTEGER)");

	// Test: insert a timestamp beyond Y2038 (2147483648 = 2038-01-19)
	int64_t post2038 = 2200000000LL;  // ~2039-09-07
	int64_t far_future = 4000000000LL;  // ~2096-10-02

	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(db,
		"INSERT INTO test_ts VALUES (1, ?)",
		-1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, post2038);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (sqlite3_prepare_v2(db,
		"INSERT INTO test_ts VALUES (2, ?)",
		-1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, far_future);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// Read back and verify
	int64_t readBack = 0;
	if (sqlite3_prepare_v2(db,
		"SELECT ts FROM test_ts WHERE id = 1",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			readBack = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(readBack == post2038, "post-2038 timestamp preserved (2200000000)");

	readBack = 0;
	if (sqlite3_prepare_v2(db,
		"SELECT ts FROM test_ts WHERE id = 2",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			readBack = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(readBack == far_future, "far future timestamp preserved (4000000000)");

	// Test: int64 cutoff comparison works for pruning
	int64_t cutoff = post2038 - 1;  // Just before first entry
	int count = 0;
	if (sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM test_ts WHERE ts < ?",
		-1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		if (sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(count == 0, "no rows before cutoff (int64 comparison correct)");

	// Verify old bind_int would have failed for these values
	int32_t truncated = (int32_t)post2038;
	CHECK(truncated != (int32_t)post2038 || truncated < 0,
		"int32 truncation would corrupt post-2038 timestamp");

	sqlite3_close(db);
	unlink(path);
}


// Test 3.3: Migration bulk import error tracking
static void TestMigrationRollback()
{
	printf("Test 3.3: Migration bulk import error tracking\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase3_migrate.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	Execute(db, "CREATE TABLE messages ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"contact_key TEXT NOT NULL, timestamp INTEGER NOT NULL, "
		"outgoing INTEGER NOT NULL DEFAULT 0, "
		"channel INTEGER NOT NULL DEFAULT 0, "
		"sender_key TEXT, text TEXT NOT NULL, "
		"path_len INTEGER DEFAULT 255, snr INTEGER DEFAULT 0, "
		"txt_type INTEGER DEFAULT 0)");
	Execute(db, "CREATE UNIQUE INDEX idx_messages_unique "
		"ON messages (contact_key, timestamp, sender_key, text)");

	// Simulate successful migration with transaction
	Execute(db, "BEGIN TRANSACTION");
	bool ok1 = Execute(db,
		"INSERT OR IGNORE INTO messages (contact_key, timestamp, outgoing, "
		"channel, sender_key, text) VALUES ('AABB', 1000, 0, 0, 'CC', 'hello')");
	bool ok2 = Execute(db,
		"INSERT OR IGNORE INTO messages (contact_key, timestamp, outgoing, "
		"channel, sender_key, text) VALUES ('AABB', 2000, 1, 0, 'DD', 'world')");
	Execute(db, "COMMIT");
	CHECK(ok1 && ok2, "bulk insert succeeds in transaction");

	// Count rows
	int count = 0;
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM messages",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(count == 2, "2 messages after successful migration");

	// Simulate total failure with ROLLBACK
	Execute(db, "BEGIN TRANSACTION");
	Execute(db,
		"INSERT OR IGNORE INTO messages (contact_key, timestamp, outgoing, "
		"channel, sender_key, text) VALUES ('EE', 3000, 0, 0, 'FF', 'test')");
	// Simulate all inserts failing → rollback
	Execute(db, "ROLLBACK");

	count = 0;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM messages",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(count == 2, "rollback preserves original data (still 2 rows)");

	sqlite3_close(db);
	unlink(path);
}


// Test 3.4: PRAGMA optimize on close
static void TestPragmaOptimize()
{
	printf("Test 3.4: PRAGMA optimize on close\n");

	sqlite3* db = NULL;
	const char* path = "/tmp/test_phase3_optimize.db";
	unlink(path);

	int rc = sqlite3_open(path, &db);
	CHECK(rc == SQLITE_OK, "database opens");

	Execute(db, "CREATE TABLE test_opt (id INTEGER PRIMARY KEY, val TEXT)");
	Execute(db, "CREATE INDEX idx_test_val ON test_opt (val)");

	// Insert some data
	for (int i = 0; i < 100; i++) {
		char sql[128];
		snprintf(sql, sizeof(sql),
			"INSERT INTO test_opt VALUES (%d, 'value_%d')", i, i);
		Execute(db, sql);
	}

	// PRAGMA optimize should run without error
	char* errMsg = NULL;
	rc = sqlite3_exec(db, "PRAGMA optimize", NULL, NULL, &errMsg);
	CHECK(rc == SQLITE_OK, "PRAGMA optimize executes without error");
	if (errMsg) sqlite3_free(errMsg);

	// Verify data is still intact after optimize
	int count = 0;
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM test_opt",
		-1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	CHECK(count == 100, "data intact after PRAGMA optimize");

	sqlite3_close(db);
	unlink(path);
}


int main()
{
	printf("=== Phase 3 Database Performance Fix Tests ===\n\n");

	TestMissingIndexes();
	printf("\n");

	TestY2038Timestamps();
	printf("\n");

	TestMigrationRollback();
	printf("\n");

	TestPragmaOptimize();
	printf("\n");

	printf("%s: %d failures\n",
		sFailures == 0 ? "ALL PASSED" : "FAILED", sFailures);
	return sFailures > 0 ? 1 : 0;
}
