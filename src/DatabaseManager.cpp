/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DatabaseManager.cpp — SQLite message and SNR history persistence
 */

#include "DatabaseManager.h"

#include <Autolock.h>
#include <Entry.h>
#include <File.h>
#include <Path.h>

#include <cstdio>
#include <cstring>
#include <ctime>


DatabaseManager* DatabaseManager::sInstance = NULL;
BLocker DatabaseManager::sInstanceLock("DatabaseManager singleton");


DatabaseManager*
DatabaseManager::Instance()
{
	if (sInstance == NULL) {
		BAutolock lock(sInstanceLock);
		if (sInstance == NULL)
			sInstance = new DatabaseManager();
	}
	return sInstance;
}


void
DatabaseManager::Destroy()
{
	BAutolock lock(sInstanceLock);
	delete sInstance;
	sInstance = NULL;
}


DatabaseManager::DatabaseManager()
	:
	fDB(NULL),
	fLock("DatabaseManager"),
	fDirectory("")
{
}


DatabaseManager::~DatabaseManager()
{
	Close();
}


bool
DatabaseManager::Open(const char* directory)
{
	BAutolock lock(fLock);
	if (fDB != NULL)
		return true;

	fDirectory = directory;

	BString dbPath(directory);
	dbPath.Append("/sestriere.db");

	int rc = sqlite3_open(dbPath.String(), &fDB);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] Failed to open database: %s\n",
			sqlite3_errmsg(fDB));
		sqlite3_close(fDB);
		fDB = NULL;
		return false;
	}

	// Enable WAL mode for better concurrent access
	_Execute("PRAGMA journal_mode=WAL");
	_Execute("PRAGMA synchronous=NORMAL");

	if (!_CreateTables()) {
		fprintf(stderr, "[DatabaseManager] Failed to create tables\n");
		Close();
		return false;
	}

	// Migrate old messages.txt if database is empty
	if (_IsEmpty())
		_MigrateFromTextFile(directory);

	// Prune old SNR data (older than 30 days)
	PruneOldData(30);

	fprintf(stderr, "[DatabaseManager] Database opened: %s\n",
		dbPath.String());
	return true;
}


void
DatabaseManager::Close()
{
	BAutolock lock(fLock);
	if (fDB != NULL) {
		sqlite3_close(fDB);
		fDB = NULL;
	}
}


bool
DatabaseManager::InsertMessage(const char* contactKeyHex,
	const ChatMessage& message)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	// Format sender pubkey prefix as hex
	char senderHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(senderHex + i * 2, 3, "%02x", message.pubKeyPrefix[i]);

	const char* sql =
		"INSERT OR IGNORE INTO messages (contact_key, timestamp, outgoing, "
		"channel, sender_key, text, path_len, snr) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, message.timestamp);
	sqlite3_bind_int(stmt, 3, message.isOutgoing ? 1 : 0);
	sqlite3_bind_int(stmt, 4, message.isChannel ? 1 : 0);
	sqlite3_bind_text(stmt, 5, senderHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, message.text, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 7, message.pathLen);
	sqlite3_bind_int(stmt, 8, message.snr);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


int32
DatabaseManager::LoadMessages(const char* contactKeyHex,
	BObjectList<ChatMessage, true>& outMessages)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql =
		"SELECT timestamp, outgoing, sender_key, text, path_len, snr "
		"FROM messages WHERE contact_key = ? AND channel = 0 "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ChatMessage* msg = new ChatMessage();
		msg->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		msg->isOutgoing = sqlite3_column_int(stmt, 1) != 0;
		msg->isChannel = false;

		// Parse sender key hex
		const char* senderHex = (const char*)sqlite3_column_text(stmt, 2);
		if (senderHex != NULL) {
			for (int i = 0; i < 6 && senderHex[i * 2] != '\0'; i++) {
				unsigned int byte;
				sscanf(senderHex + i * 2, "%2x", &byte);
				msg->pubKeyPrefix[i] = (uint8)byte;
			}
		}

		const char* text = (const char*)sqlite3_column_text(stmt, 3);
		if (text != NULL)
			strlcpy(msg->text, text, sizeof(msg->text));

		msg->pathLen = (uint8)sqlite3_column_int(stmt, 4);
		msg->snr = (int8)sqlite3_column_int(stmt, 5);

		outMessages.AddItem(msg);
		count++;
	}

	sqlite3_finalize(stmt);
	return count;
}


int32
DatabaseManager::LoadChannelMessages(BObjectList<ChatMessage, true>& outMessages)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql =
		"SELECT timestamp, outgoing, sender_key, text, path_len, snr "
		"FROM messages WHERE channel = 1 "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ChatMessage* msg = new ChatMessage();
		msg->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		msg->isOutgoing = sqlite3_column_int(stmt, 1) != 0;
		msg->isChannel = true;

		const char* senderHex = (const char*)sqlite3_column_text(stmt, 2);
		if (senderHex != NULL) {
			for (int i = 0; i < 6 && senderHex[i * 2] != '\0'; i++) {
				unsigned int byte;
				sscanf(senderHex + i * 2, "%2x", &byte);
				msg->pubKeyPrefix[i] = (uint8)byte;
			}
		}

		const char* text = (const char*)sqlite3_column_text(stmt, 3);
		if (text != NULL)
			strlcpy(msg->text, text, sizeof(msg->text));

		msg->pathLen = (uint8)sqlite3_column_int(stmt, 4);
		msg->snr = (int8)sqlite3_column_int(stmt, 5);

		outMessages.AddItem(msg);
		count++;
	}

	sqlite3_finalize(stmt);
	return count;
}


bool
DatabaseManager::InsertSNRDataPoint(const char* contactKeyHex,
	uint32 timestamp, int8 snr, int8 rssi, uint8 pathLen)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	const char* sql =
		"INSERT INTO snr_history (contact_key, timestamp, snr, rssi, path_len) "
		"VALUES (?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, timestamp);
	sqlite3_bind_int(stmt, 3, snr);
	sqlite3_bind_int(stmt, 4, rssi);
	sqlite3_bind_int(stmt, 5, pathLen);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


int32
DatabaseManager::LoadSNRHistory(const char* contactKeyHex,
	uint32 sinceTimestamp, BObjectList<SNRDataPoint, true>& outPoints)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql =
		"SELECT timestamp, snr, rssi, path_len FROM snr_history "
		"WHERE contact_key = ? AND timestamp >= ? "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, sinceTimestamp);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		SNRDataPoint* point = new SNRDataPoint();
		point->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		point->snr = (int8)sqlite3_column_int(stmt, 1);
		point->rssi = (int8)sqlite3_column_int(stmt, 2);
		point->pathLen = (uint8)sqlite3_column_int(stmt, 3);

		outPoints.AddItem(point);
		count++;
	}

	sqlite3_finalize(stmt);
	return count;
}


int32
DatabaseManager::SearchMessages(const char* query,
	BObjectList<ChatMessage, true>& outMessages, int32 maxResults)
{
	BAutolock lock(fLock);
	if (fDB == NULL || query == NULL || query[0] == '\0')
		return 0;

	const char* sql =
		"SELECT timestamp, outgoing, channel, sender_key, text, path_len, snr "
		"FROM messages WHERE text LIKE ? "
		"ORDER BY timestamp DESC LIMIT ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	// Build LIKE pattern: %query%
	BString pattern;
	pattern.SetToFormat("%%%s%%", query);
	sqlite3_bind_text(stmt, 1, pattern.String(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, maxResults);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ChatMessage* msg = new ChatMessage();
		msg->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		msg->isOutgoing = sqlite3_column_int(stmt, 1) != 0;
		msg->isChannel = sqlite3_column_int(stmt, 2) != 0;

		const char* senderHex = (const char*)sqlite3_column_text(stmt, 3);
		if (senderHex != NULL) {
			for (int i = 0; i < 6 && senderHex[i * 2] != '\0'; i++) {
				unsigned int byte;
				sscanf(senderHex + i * 2, "%2x", &byte);
				msg->pubKeyPrefix[i] = (uint8)byte;
			}
		}

		const char* text = (const char*)sqlite3_column_text(stmt, 4);
		if (text != NULL)
			strlcpy(msg->text, text, sizeof(msg->text));

		msg->pathLen = (uint8)sqlite3_column_int(stmt, 5);
		msg->snr = (int8)sqlite3_column_int(stmt, 6);

		outMessages.AddItem(msg);
		count++;
	}

	sqlite3_finalize(stmt);
	return count;
}


int32
DatabaseManager::GetMessageCount(const char* contactKeyHex)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql = "SELECT COUNT(*) FROM messages WHERE contact_key = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);

	int32 count = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		count = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);
	return count;
}


int32
DatabaseManager::GetTotalMessageCount()
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql = "SELECT COUNT(*) FROM messages";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	int32 count = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		count = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);
	return count;
}


void
DatabaseManager::PruneOldData(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	uint32 cutoff = (uint32)time(NULL) - (maxAgeDays * 86400);

	char sql[128];
	snprintf(sql, sizeof(sql),
		"DELETE FROM snr_history WHERE timestamp < %u", cutoff);
	_Execute(sql);

	snprintf(sql, sizeof(sql),
		"DELETE FROM telemetry_history WHERE timestamp < %u", cutoff);
	_Execute(sql);
}


void
DatabaseManager::PruneOldMessages(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	uint32 cutoff = (uint32)time(NULL) - (maxAgeDays * 86400);

	char sql[128];
	snprintf(sql, sizeof(sql),
		"DELETE FROM messages WHERE timestamp < %u", cutoff);
	_Execute(sql);

	fprintf(stderr, "[DatabaseManager] Pruned messages older than %u days\n",
		maxAgeDays);
}


bool
DatabaseManager::InsertTelemetry(uint32 nodeId, const char* sensorName,
	uint8 sensorType, float value, const char* unit)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	const char* sql =
		"INSERT INTO telemetry_history "
		"(node_id, sensor_name, sensor_type, value, unit, timestamp) "
		"VALUES (?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return false;

	uint32 now = (uint32)time(NULL);
	sqlite3_bind_int(stmt, 1, nodeId);
	sqlite3_bind_text(stmt, 2, sensorName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, sensorType);
	sqlite3_bind_double(stmt, 4, (double)value);
	sqlite3_bind_text(stmt, 5, unit, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 6, now);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


int32
DatabaseManager::LoadTelemetryHistory(uint32 nodeId,
	const char* sensorName, uint32 sinceTimestamp,
	BObjectList<TelemetryRecord, true>& outRecords)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql =
		"SELECT timestamp, node_id, sensor_type, value, sensor_name, unit "
		"FROM telemetry_history "
		"WHERE node_id = ? AND sensor_name = ? AND timestamp >= ? "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	sqlite3_bind_int(stmt, 1, nodeId);
	sqlite3_bind_text(stmt, 2, sensorName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, sinceTimestamp);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		TelemetryRecord* rec = new TelemetryRecord();
		rec->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		rec->nodeId = (uint32)sqlite3_column_int(stmt, 1);
		rec->sensorType = (uint8)sqlite3_column_int(stmt, 2);
		rec->value = (float)sqlite3_column_double(stmt, 3);

		const char* name = (const char*)sqlite3_column_text(stmt, 4);
		if (name != NULL)
			strlcpy(rec->sensorName, name, sizeof(rec->sensorName));
		else
			rec->sensorName[0] = '\0';

		const char* u = (const char*)sqlite3_column_text(stmt, 5);
		if (u != NULL)
			strlcpy(rec->unit, u, sizeof(rec->unit));
		else
			rec->unit[0] = '\0';

		outRecords.AddItem(rec);
		count++;
	}

	sqlite3_finalize(stmt);
	return count;
}


int32
DatabaseManager::GetTelemetryNodeIds(BObjectList<BString, true>& outNodeNames)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql =
		"SELECT DISTINCT node_id || ':' || sensor_name "
		"FROM telemetry_history ORDER BY node_id, sensor_name";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* text = (const char*)sqlite3_column_text(stmt, 0);
		if (text != NULL) {
			outNodeNames.AddItem(new BString(text));
			count++;
		}
	}

	sqlite3_finalize(stmt);
	return count;
}


bool
DatabaseManager::_IsEmpty()
{
	if (fDB == NULL)
		return true;

	const char* sql = "SELECT COUNT(*) FROM messages";
	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return true;

	int32 count = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		count = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);
	return count == 0;
}


void
DatabaseManager::_MigrateFromTextFile(const char* directory)
{
	BString filePath(directory);
	filePath.Append("/messages.txt");

	BFile file(filePath.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	off_t size;
	file.GetSize(&size);
	if (size == 0 || size > 2 * 1024 * 1024)
		return;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return;
	}
	buffer[bytesRead] = '\0';

	fprintf(stderr, "[DatabaseManager] Migrating messages.txt (%d bytes)...\n",
		(int)bytesRead);

	// Begin transaction for bulk import
	_Execute("BEGIN TRANSACTION");

	int migrated = 0;
	char* line = strtok(buffer, "\n");
	while (line != NULL) {
		if (strlen(line) < 5) {
			line = strtok(NULL, "\n");
			continue;
		}

		if (line[0] == 'C' && line[1] == '|') {
			// Channel: C|timestamp|outgoing|senderHex|text
			uint32 timestamp;
			int outgoing;
			char senderHex[13];
			char text[256];

			if (sscanf(line + 2, "%u|%d|%12[^|]|%255[^\n]",
					&timestamp, &outgoing, senderHex, text) == 4) {
				ChatMessage msg;
				for (int i = 0; i < 6; i++) {
					unsigned int byte;
					sscanf(senderHex + i * 2, "%2x", &byte);
					msg.pubKeyPrefix[i] = (uint8)byte;
				}
				msg.timestamp = timestamp;
				msg.isOutgoing = (outgoing == 1);
				msg.isChannel = true;
				msg.pathLen = 0;
				msg.snr = 0;
				strlcpy(msg.text, text, sizeof(msg.text));

				InsertMessage("channel", msg);
				migrated++;
			}
		} else if (line[0] == 'D' && line[1] == '|') {
			// DM: D|contactHex|timestamp|outgoing|text
			char contactHex[13];
			uint32 timestamp;
			int outgoing;
			char text[256];

			if (sscanf(line + 2, "%12[^|]|%u|%d|%255[^\n]",
					contactHex, &timestamp, &outgoing, text) == 4) {
				ChatMessage msg;
				for (int i = 0; i < 6; i++) {
					unsigned int byte;
					sscanf(contactHex + i * 2, "%2x", &byte);
					msg.pubKeyPrefix[i] = (uint8)byte;
				}
				msg.timestamp = timestamp;
				msg.isOutgoing = (outgoing == 1);
				msg.isChannel = false;
				msg.pathLen = 0;
				msg.snr = 0;
				strlcpy(msg.text, text, sizeof(msg.text));

				InsertMessage(contactHex, msg);
				migrated++;
			}
		}

		line = strtok(NULL, "\n");
	}

	_Execute("COMMIT");
	delete[] buffer;

	if (migrated > 0) {
		fprintf(stderr, "[DatabaseManager] Migrated %d messages from text file\n",
			migrated);

		// Rename old file to .bak
		BString bakPath(directory);
		bakPath.Append("/messages.txt.bak");

		BEntry entry(filePath.String());
		if (entry.Exists())
			entry.Rename(bakPath.String());
	}
}


bool
DatabaseManager::_CreateTables()
{
	// Messages table - stores all chat messages with full metadata
	bool ok = _Execute(
		"CREATE TABLE IF NOT EXISTS messages ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  contact_key TEXT NOT NULL,"
		"  timestamp INTEGER NOT NULL,"
		"  outgoing INTEGER NOT NULL DEFAULT 0,"
		"  channel INTEGER NOT NULL DEFAULT 0,"
		"  sender_key TEXT,"
		"  text TEXT NOT NULL,"
		"  path_len INTEGER DEFAULT 255,"
		"  snr INTEGER DEFAULT 0"
		")");
	if (!ok)
		return false;

	// Index for fast contact message lookup
	ok = _Execute(
		"CREATE INDEX IF NOT EXISTS idx_messages_contact "
		"ON messages (contact_key, channel, timestamp)");
	if (!ok)
		return false;

	// Unique constraint to prevent duplicate messages
	_Execute(
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_unique "
		"ON messages (contact_key, timestamp, text)");
	// Ignore failure on existing databases where index might conflict

	// SNR history table - time-series signal quality data per contact
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS snr_history ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  contact_key TEXT NOT NULL,"
		"  timestamp INTEGER NOT NULL,"
		"  snr INTEGER NOT NULL,"
		"  rssi INTEGER DEFAULT 0,"
		"  path_len INTEGER DEFAULT 255"
		")");
	if (!ok)
		return false;

	// Index for fast SNR history lookup
	ok = _Execute(
		"CREATE INDEX IF NOT EXISTS idx_snr_contact_time "
		"ON snr_history (contact_key, timestamp)");
	if (!ok)
		return false;

	// Telemetry history table - time-series sensor data per node
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS telemetry_history ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  node_id INTEGER NOT NULL,"
		"  sensor_name TEXT NOT NULL,"
		"  sensor_type INTEGER NOT NULL,"
		"  value REAL NOT NULL,"
		"  unit TEXT,"
		"  timestamp INTEGER NOT NULL"
		")");
	if (!ok)
		return false;

	// Index for fast telemetry lookup
	ok = _Execute(
		"CREATE INDEX IF NOT EXISTS idx_telemetry_node_sensor "
		"ON telemetry_history (node_id, sensor_name, timestamp)");

	return ok;
}


bool
DatabaseManager::_Execute(const char* sql)
{
	if (fDB == NULL)
		return false;

	char* errMsg = NULL;
	int rc = sqlite3_exec(fDB, sql, NULL, NULL, &errMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] SQL error: %s\n",
			errMsg ? errMsg : "unknown");
		sqlite3_free(errMsg);
		return false;
	}
	return true;
}
