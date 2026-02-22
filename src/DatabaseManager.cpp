/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DatabaseManager.cpp — SQLite message and SNR history persistence
 */

#include "DatabaseManager.h"
#include "Constants.h"
#include "Utils.h"

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
	char senderHex[kContactHexSize];
	FormatContactKey(senderHex, message.pubKeyPrefix);

	const char* sql =
		"INSERT OR IGNORE INTO messages (contact_key, timestamp, outgoing, "
		"channel, sender_key, text, path_len, snr) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

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
	OwningObjectList<ChatMessage>& outMessages)
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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ChatMessage* msg = new ChatMessage();
		msg->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		msg->isOutgoing = sqlite3_column_int(stmt, 1) != 0;
		msg->isChannel = false;

		// Parse sender key hex
		const char* senderHex = (const char*)sqlite3_column_text(stmt, 2);
		if (senderHex != NULL)
			ParseHexPrefix(msg->pubKeyPrefix, senderHex);

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
DatabaseManager::LoadChannelMessages(OwningObjectList<ChatMessage>& outMessages)
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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ChatMessage* msg = new ChatMessage();
		msg->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		msg->isOutgoing = sqlite3_column_int(stmt, 1) != 0;
		msg->isChannel = true;

		const char* senderHex = (const char*)sqlite3_column_text(stmt, 2);
		if (senderHex != NULL)
			ParseHexPrefix(msg->pubKeyPrefix, senderHex);

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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

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
	uint32 sinceTimestamp, OwningObjectList<SNRDataPoint>& outPoints)
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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

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
	OwningObjectList<ChatMessage>& outMessages, int32 maxResults)
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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

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
		if (senderHex != NULL)
			ParseHexPrefix(msg->pubKeyPrefix, senderHex);

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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

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

	int32 cutoff = (int32)((uint32)time(NULL) - (maxAgeDays * 86400));

	const char* sqlSnr = "DELETE FROM snr_history WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sqlSnr, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, cutoff);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	const char* sqlTelem = "DELETE FROM telemetry_history WHERE timestamp < ?";
	stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sqlTelem, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, cutoff);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}


void
DatabaseManager::PruneOldMessages(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	int32 cutoff = (int32)((uint32)time(NULL) - (maxAgeDays * 86400));

	const char* sql = "DELETE FROM messages WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, cutoff);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	fprintf(stderr, "[DatabaseManager] Pruned messages older than %" B_PRIu32
		" days\n", maxAgeDays);
}


bool
DatabaseManager::SetMuted(const char* keyHex, bool muted)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	const char* sql =
		"INSERT OR REPLACE INTO mute_settings (key_hex, muted) VALUES (?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

	sqlite3_bind_text(stmt, 1, keyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, muted ? 1 : 0);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::IsMuted(const char* keyHex)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	const char* sql =
		"SELECT muted FROM mute_settings WHERE key_hex = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, keyHex, -1, SQLITE_TRANSIENT);

	bool muted = false;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		muted = (sqlite3_column_int(stmt, 0) != 0);

	sqlite3_finalize(stmt);
	return muted;
}


void
DatabaseManager::LoadAllMuted(BMessage* outMsg)
{
	BAutolock lock(fLock);
	if (fDB == NULL || outMsg == NULL)
		return;

	const char* sql =
		"SELECT key_hex FROM mute_settings WHERE muted = 1";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* key = (const char*)sqlite3_column_text(stmt, 0);
		if (key != NULL)
			outMsg->AddString("muted_key", key);
	}

	sqlite3_finalize(stmt);
}


bool
DatabaseManager::CreateGroup(const char* name)
{
	BAutolock lock(fLock);
	if (fDB == NULL || name == NULL || name[0] == '\0')
		return false;

	const char* sql =
		"INSERT OR IGNORE INTO contact_groups (name) VALUES (?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::DeleteGroup(const char* name)
{
	BAutolock lock(fLock);
	if (fDB == NULL || name == NULL)
		return false;

	// Enable foreign keys for CASCADE to work
	_Execute("PRAGMA foreign_keys = ON");

	const char* sql = "DELETE FROM contact_groups WHERE name = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::AddContactToGroup(const char* groupName,
	const char* contactKeyHex)
{
	BAutolock lock(fLock);
	if (fDB == NULL || groupName == NULL || contactKeyHex == NULL)
		return false;

	// Remove from any existing group first (1 group per contact)
	const char* removeSql =
		"DELETE FROM contact_group_members WHERE contact_key = ?";
	sqlite3_stmt* removeStmt;
	if (sqlite3_prepare_v2(fDB, removeSql, -1, &removeStmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(removeStmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
		sqlite3_step(removeStmt);
		sqlite3_finalize(removeStmt);
	}

	const char* sql =
		"INSERT OR REPLACE INTO contact_group_members "
		"(group_name, contact_key) VALUES (?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

	sqlite3_bind_text(stmt, 1, groupName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, contactKeyHex, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::RemoveContactFromGroup(const char* groupName,
	const char* contactKeyHex)
{
	BAutolock lock(fLock);
	if (fDB == NULL || groupName == NULL || contactKeyHex == NULL)
		return false;

	const char* sql =
		"DELETE FROM contact_group_members "
		"WHERE group_name = ? AND contact_key = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, groupName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, contactKeyHex, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


int32
DatabaseManager::LoadGroups(OwningObjectList<BString>& outNames)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql = "SELECT name FROM contact_groups ORDER BY name ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* name = (const char*)sqlite3_column_text(stmt, 0);
		if (name != NULL) {
			outNames.AddItem(new BString(name));
			count++;
		}
	}

	sqlite3_finalize(stmt);
	return count;
}


int32
DatabaseManager::LoadGroupMembers(const char* groupName,
	OwningObjectList<BString>& outKeys)
{
	BAutolock lock(fLock);
	if (fDB == NULL || groupName == NULL)
		return 0;

	const char* sql =
		"SELECT contact_key FROM contact_group_members "
		"WHERE group_name = ? ORDER BY contact_key ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return 0;

	sqlite3_bind_text(stmt, 1, groupName, -1, SQLITE_TRANSIENT);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* key = (const char*)sqlite3_column_text(stmt, 0);
		if (key != NULL) {
			outKeys.AddItem(new BString(key));
			count++;
		}
	}

	sqlite3_finalize(stmt);
	return count;
}


BString
DatabaseManager::GetContactGroup(const char* contactKeyHex)
{
	BAutolock lock(fLock);
	BString result;
	if (fDB == NULL || contactKeyHex == NULL)
		return result;

	const char* sql =
		"SELECT group_name FROM contact_group_members WHERE contact_key = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		return result;

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* name = (const char*)sqlite3_column_text(stmt, 0);
		if (name != NULL)
			result = name;
	}

	sqlite3_finalize(stmt);
	return result;
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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

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
	OwningObjectList<TelemetryRecord>& outRecords)
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
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

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
DatabaseManager::GetTelemetryNodeIds(OwningObjectList<BString>& outNodeNames)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return 0;

	const char* sql =
		"SELECT DISTINCT node_id || ':' || sensor_name "
		"FROM telemetry_history ORDER BY node_id, sensor_name";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

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
	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		if (strlen(line) < 5) {
			line = strtok_r(NULL, "\n", &saveptr);
			continue;
		}

		if (line[0] == 'C' && line[1] == '|') {
			// Channel: C|timestamp|outgoing|senderHex|text
			uint32 timestamp;
			int outgoing;
			char senderHex[kContactHexSize];
			char text[256];

			if (sscanf(line + 2, "%u|%d|%12[^|]|%255[^\n]",
					&timestamp, &outgoing, senderHex, text) == 4) {
				ChatMessage msg;
				ParseHexPrefix(msg.pubKeyPrefix, senderHex);
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
			char contactHex[kContactHexSize];
			uint32 timestamp;
			int outgoing;
			char text[256];

			if (sscanf(line + 2, "%12[^|]|%u|%d|%255[^\n]",
					contactHex, &timestamp, &outgoing, text) == 4) {
				ChatMessage msg;
				ParseHexPrefix(msg.pubKeyPrefix, contactHex);
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

		line = strtok_r(NULL, "\n", &saveptr);
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
		if (entry.Exists()) {
			if (entry.Rename(bakPath.String()) != B_OK)
				fprintf(stderr, "[DatabaseManager] Could not rename %s to .bak\n",
					filePath.String());
		}
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
	if (!ok)
		return false;

	// Mute settings table — stores muted contacts and channels
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS mute_settings ("
		"  key_hex TEXT PRIMARY KEY,"
		"  muted INTEGER NOT NULL DEFAULT 0"
		")");
	if (!ok)
		return false;

	// Contact groups table
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS contact_groups ("
		"  name TEXT PRIMARY KEY"
		")");
	if (!ok)
		return false;

	// Contact group membership table
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS contact_group_members ("
		"  group_name TEXT NOT NULL,"
		"  contact_key TEXT NOT NULL,"
		"  PRIMARY KEY (group_name, contact_key),"
		"  FOREIGN KEY (group_name) REFERENCES contact_groups(name) ON DELETE CASCADE"
		")");

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
