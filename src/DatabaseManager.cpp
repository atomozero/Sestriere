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
	fCompanionKey[0] = '\0';
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
		sqlite3_close_v2(fDB);
		fDB = NULL;
		return false;
	}

	// Set busy timeout first — needed before any locking operations
	_Execute("PRAGMA busy_timeout=5000");

	// Try WAL mode for better concurrent access, fall back to DELETE
	// if filesystem doesn't support shared-memory locking (common on Haiku)
	if (!_Execute("PRAGMA journal_mode=WAL")) {
		fprintf(stderr, "[DatabaseManager] WAL mode not supported, "
			"falling back to DELETE journal\n");
		// Close and reopen to reset locking state after WAL failure
		sqlite3_close_v2(fDB);
		fDB = NULL;
		int rc2 = sqlite3_open(dbPath.String(), &fDB);
		if (rc2 != SQLITE_OK) {
			fprintf(stderr, "[DatabaseManager] Failed to reopen database: %s\n",
				sqlite3_errmsg(fDB));
			sqlite3_close_v2(fDB);
			fDB = NULL;
			return false;
		}
		_Execute("PRAGMA busy_timeout=5000");
		_Execute("PRAGMA journal_mode=DELETE");
	}

	_Execute("PRAGMA synchronous=NORMAL");
	_Execute("PRAGMA foreign_keys=ON");

	if (!_CreateTables()) {
		fprintf(stderr, "[DatabaseManager] Failed to create tables\n");
		Close();
		return false;
	}

	// Schema migrations: add columns if missing (silently ignore if already exist)
	sqlite3_exec(fDB, "ALTER TABLE messages ADD COLUMN txt_type INTEGER DEFAULT 0",
		NULL, NULL, NULL);
	sqlite3_exec(fDB, "ALTER TABLE messages ADD COLUMN delivery_status INTEGER DEFAULT 1",
		NULL, NULL, NULL);
	sqlite3_exec(fDB, "ALTER TABLE messages ADD COLUMN round_trip_ms INTEGER DEFAULT 0",
		NULL, NULL, NULL);

	// Multi-companion partitioning: add companion_key column
	sqlite3_exec(fDB, "ALTER TABLE messages ADD COLUMN companion_key TEXT DEFAULT ''",
		NULL, NULL, NULL);
	sqlite3_exec(fDB, "ALTER TABLE snr_history ADD COLUMN companion_key TEXT DEFAULT ''",
		NULL, NULL, NULL);

	// Migrate old messages.txt if database is empty
	if (_IsEmpty())
		_MigrateFromTextFile(directory);

	// Prune old SNR data (older than 30 days)
	PruneOldData(30);
	// Prune old topology edges (older than 30 days)
	PruneOldEdges(30);

	fprintf(stderr, "[DatabaseManager] Database opened: %s\n",
		dbPath.String());
	return true;
}


void
DatabaseManager::Close()
{
	BAutolock lock(fLock);
	if (fDB != NULL) {
		// Optimize query planner before closing
		_Execute("PRAGMA optimize");
		sqlite3_close_v2(fDB);
		fDB = NULL;
	}
}


void
DatabaseManager::SetCompanionKey(const char* key)
{
	BAutolock lock(fLock);
	if (key != NULL)
		strlcpy(fCompanionKey, key, sizeof(fCompanionKey));
	else
		fCompanionKey[0] = '\0';
	fprintf(stderr, "[DatabaseManager] Companion key set to: %s\n",
		fCompanionKey[0] ? fCompanionKey : "(none)");
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
		"channel, sender_key, text, path_len, snr, txt_type, "
		"delivery_status, round_trip_ms, companion_key) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (int64_t)message.timestamp);
	sqlite3_bind_int(stmt, 3, message.isOutgoing ? 1 : 0);
	sqlite3_bind_int(stmt, 4, message.isChannel ? 1 : 0);
	sqlite3_bind_text(stmt, 5, senderHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, message.text, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 7, message.pathLen);
	sqlite3_bind_int(stmt, 8, message.snr);
	sqlite3_bind_int(stmt, 9, message.txtType);
	sqlite3_bind_int(stmt, 10, message.deliveryStatus);
	sqlite3_bind_int(stmt, 11, message.roundTripMs);
	sqlite3_bind_text(stmt, 12, fCompanionKey, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::UpdateMessageDeliveryStatus(const char* contactKeyHex,
	uint32 timestamp, uint8 status, uint32 roundTripMs)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	const char* sql =
		"UPDATE messages SET delivery_status = ?, round_trip_ms = ? "
		"WHERE contact_key = ? AND timestamp = ? AND outgoing = 1 "
		"AND companion_key = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

	sqlite3_bind_int(stmt, 1, status);
	sqlite3_bind_int(stmt, 2, roundTripMs);
	sqlite3_bind_text(stmt, 3, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 4, (int64_t)timestamp);
	sqlite3_bind_text(stmt, 5, fCompanionKey, -1, SQLITE_TRANSIENT);

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
		"SELECT timestamp, outgoing, channel, sender_key, text, path_len, "
		"snr, txt_type, delivery_status, round_trip_ms "
		"FROM messages WHERE contact_key = ? AND companion_key = ? "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, fCompanionKey, -1, SQLITE_TRANSIENT);

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ChatMessage* msg = new ChatMessage();
		msg->timestamp = (uint32)sqlite3_column_int(stmt, 0);
		msg->isOutgoing = sqlite3_column_int(stmt, 1) != 0;
		msg->isChannel = sqlite3_column_int(stmt, 2) != 0;

		// Parse sender key hex
		const char* senderHex = (const char*)sqlite3_column_text(stmt, 3);
		if (senderHex != NULL)
			ParseHexPrefix(msg->pubKeyPrefix, senderHex);

		const char* text = (const char*)sqlite3_column_text(stmt, 4);
		if (text != NULL)
			strlcpy(msg->text, text, sizeof(msg->text));

		msg->pathLen = (uint8)sqlite3_column_int(stmt, 5);
		msg->snr = (int8)sqlite3_column_int(stmt, 6);
		msg->txtType = (uint8)sqlite3_column_int(stmt, 7);
		msg->deliveryStatus = (uint8)sqlite3_column_int(stmt, 8);
		msg->roundTripMs = (uint32)sqlite3_column_int(stmt, 9);

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
		"SELECT timestamp, outgoing, sender_key, text, path_len, snr, "
		"txt_type, delivery_status, round_trip_ms "
		"FROM messages WHERE contact_key = 'channel' AND companion_key = ? "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, fCompanionKey, -1, SQLITE_TRANSIENT);

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
		msg->txtType = (uint8)sqlite3_column_int(stmt, 6);
		msg->deliveryStatus = (uint8)sqlite3_column_int(stmt, 7);
		msg->roundTripMs = (uint32)sqlite3_column_int(stmt, 8);

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
		"INSERT INTO snr_history (contact_key, timestamp, snr, rssi, path_len, "
		"companion_key) VALUES (?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return false;
	}

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (int64_t)timestamp);
	sqlite3_bind_int(stmt, 3, snr);
	sqlite3_bind_int(stmt, 4, rssi);
	sqlite3_bind_int(stmt, 5, pathLen);
	sqlite3_bind_text(stmt, 6, fCompanionKey, -1, SQLITE_TRANSIENT);

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
		"WHERE contact_key = ? AND timestamp >= ? AND companion_key = ? "
		"ORDER BY timestamp ASC";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (int64_t)sinceTimestamp);
	sqlite3_bind_text(stmt, 3, fCompanionKey, -1, SQLITE_TRANSIENT);

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
		"FROM messages WHERE text LIKE ? AND companion_key = ? "
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
	sqlite3_bind_text(stmt, 2, fCompanionKey, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, maxResults);

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

	const char* sql =
		"SELECT COUNT(*) FROM messages "
		"WHERE contact_key = ? AND companion_key = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, fCompanionKey, -1, SQLITE_TRANSIENT);

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

	const char* sql =
		"SELECT COUNT(*) FROM messages WHERE companion_key = ?";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, fCompanionKey, -1, SQLITE_TRANSIENT);

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

	int64_t cutoff = (int64_t)time(NULL) - (int64_t)(maxAgeDays * 86400);

	const char* sqlSnr = "DELETE FROM snr_history WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sqlSnr, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			fprintf(stderr, "[DatabaseManager] prune snr_history: %s\n",
				sqlite3_errmsg(fDB));
		sqlite3_finalize(stmt);
	}

	const char* sqlTelem = "DELETE FROM telemetry_history WHERE timestamp < ?";
	stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sqlTelem, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			fprintf(stderr, "[DatabaseManager] prune telemetry_history: %s\n",
				sqlite3_errmsg(fDB));
		sqlite3_finalize(stmt);
	}

	// Prune old images
	PruneOldImages(maxAgeDays);

	// Prune old voice clips
	PruneOldVoiceClips(maxAgeDays);
}


void
DatabaseManager::PruneOldMessages(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	int64_t cutoff = (int64_t)time(NULL) - (int64_t)(maxAgeDays * 86400);

	const char* sql = "DELETE FROM messages WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			fprintf(stderr, "[DatabaseManager] prune messages: %s\n",
				sqlite3_errmsg(fDB));
		sqlite3_finalize(stmt);
	}

	fprintf(stderr, "[DatabaseManager] Pruned messages older than %" B_PRIu32
		" days\n", maxAgeDays);
}


// =============================================================================
// Image persistence
// =============================================================================


bool
DatabaseManager::InsertImage(const char* contactKey, uint32 sessionId,
	int32 width, int32 height, const uint8* jpegData, size_t jpegSize)
{
	BAutolock lock(fLock);
	if (fDB == NULL || jpegData == NULL || jpegSize == 0)
		return false;

	const char* sql =
		"INSERT OR REPLACE INTO images "
		"(session_id, contact_key, timestamp, width, height, jpeg_data, jpeg_size) "
		"VALUES (?, ?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) != SQLITE_OK)
		return false;

	sqlite3_bind_int(stmt, 1, (int)sessionId);
	sqlite3_bind_text(stmt, 2, contactKey, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, (int64_t)time(NULL));
	sqlite3_bind_int(stmt, 4, width);
	sqlite3_bind_int(stmt, 5, height);
	sqlite3_bind_blob(stmt, 6, jpegData, (int)jpegSize, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 7, (int)jpegSize);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::LoadImage(uint32 sessionId, uint8** outJpegData,
	size_t* outSize, int32* outWidth, int32* outHeight)
{
	BAutolock lock(fLock);
	if (fDB == NULL || outJpegData == NULL || outSize == NULL)
		return false;

	const char* sql =
		"SELECT jpeg_data, jpeg_size, width, height FROM images "
		"WHERE session_id = ?";

	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) != SQLITE_OK)
		return false;

	sqlite3_bind_int(stmt, 1, (int)sessionId);

	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const void* blob = sqlite3_column_blob(stmt, 0);
		int blobSize = sqlite3_column_bytes(stmt, 0);
		int jpegSize = sqlite3_column_int(stmt, 1);

		// Use the smaller of blob size and recorded jpeg_size
		int dataSize = (blobSize < jpegSize) ? blobSize : jpegSize;
		if (blob != NULL && dataSize > 0) {
			uint8* data = (uint8*)malloc(dataSize);
			if (data != NULL) {
				memcpy(data, blob, dataSize);
				*outJpegData = data;
				*outSize = (size_t)dataSize;
				if (outWidth != NULL)
					*outWidth = sqlite3_column_int(stmt, 2);
				if (outHeight != NULL)
					*outHeight = sqlite3_column_int(stmt, 3);
				found = true;
			}
		}
	}

	sqlite3_finalize(stmt);
	return found;
}


void
DatabaseManager::PruneOldImages(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	int64_t cutoff = (int64_t)time(NULL) - (int64_t)(maxAgeDays * 86400);

	const char* sql = "DELETE FROM images WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			fprintf(stderr, "[DatabaseManager] prune images: %s\n",
				sqlite3_errmsg(fDB));
		sqlite3_finalize(stmt);
	}
}


// =============================================================================
// Voice clips
// =============================================================================


bool
DatabaseManager::InsertVoiceClip(const char* contactKey, uint32 sessionId,
	uint32 durationSec, uint8 mode, const uint8* codec2Data, size_t dataSize)
{
	BAutolock lock(fLock);
	if (fDB == NULL || codec2Data == NULL || dataSize == 0)
		return false;

	const char* sql =
		"INSERT OR REPLACE INTO voice_clips "
		"(session_id, contact_key, timestamp, duration_sec, codec2_mode, "
		"codec2_data, codec2_size) VALUES (?, ?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) != SQLITE_OK)
		return false;

	sqlite3_bind_int(stmt, 1, (int)sessionId);
	sqlite3_bind_text(stmt, 2, contactKey, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, (int64_t)time(NULL));
	sqlite3_bind_int(stmt, 4, (int)durationSec);
	sqlite3_bind_int(stmt, 5, (int)mode);
	sqlite3_bind_blob(stmt, 6, codec2Data, (int)dataSize, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 7, (int)dataSize);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}


bool
DatabaseManager::LoadVoiceClip(uint32 sessionId, uint8** outData,
	size_t* outSize, uint32* outDuration, uint8* outMode)
{
	BAutolock lock(fLock);
	if (fDB == NULL || outData == NULL || outSize == NULL)
		return false;

	const char* sql =
		"SELECT codec2_data, codec2_size, duration_sec, codec2_mode "
		"FROM voice_clips WHERE session_id = ?";

	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) != SQLITE_OK)
		return false;

	sqlite3_bind_int(stmt, 1, (int)sessionId);

	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const void* blob = sqlite3_column_blob(stmt, 0);
		int blobSize = sqlite3_column_bytes(stmt, 0);
		int c2Size = sqlite3_column_int(stmt, 1);

		int dataSize = (blobSize < c2Size) ? blobSize : c2Size;
		if (blob != NULL && dataSize > 0) {
			uint8* data = (uint8*)malloc(dataSize);
			if (data != NULL) {
				memcpy(data, blob, dataSize);
				*outData = data;
				*outSize = (size_t)dataSize;
				if (outDuration != NULL)
					*outDuration = (uint32)sqlite3_column_int(stmt, 2);
				if (outMode != NULL)
					*outMode = (uint8)sqlite3_column_int(stmt, 3);
				found = true;
			}
		}
	}

	sqlite3_finalize(stmt);
	return found;
}


void
DatabaseManager::PruneOldVoiceClips(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	int64_t cutoff = (int64_t)time(NULL) - (int64_t)(maxAgeDays * 86400);

	const char* sql = "DELETE FROM voice_clips WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			fprintf(stderr, "[DatabaseManager] prune voice_clips: %s\n",
				sqlite3_errmsg(fDB));
		sqlite3_finalize(stmt);
	}
}


// =============================================================================
// Topology edges
// =============================================================================

bool
DatabaseManager::InsertTopologyEdge(const char* fromHex, const char* toHex,
	int8 snr)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return false;

	const char* sql =
		"INSERT OR REPLACE INTO topology_edges "
		"(from_key, to_key, snr, timestamp) VALUES (?, ?, ?, ?)";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, fromHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, toHex, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, snr);
	sqlite3_bind_int64(stmt, 4, (int64_t)time(NULL));

	bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return ok;
}


int32
DatabaseManager::LoadTopologyEdges(BMessage* outEdges)
{
	BAutolock lock(fLock);
	if (fDB == NULL || outEdges == NULL)
		return 0;

	const char* sql =
		"SELECT from_key, to_key, snr, timestamp FROM topology_edges "
		"ORDER BY timestamp DESC";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) != SQLITE_OK)
		return 0;

	int32 count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* fromKey = (const char*)sqlite3_column_text(stmt, 0);
		const char* toKey = (const char*)sqlite3_column_text(stmt, 1);
		int8 snr = (int8)sqlite3_column_int(stmt, 2);
		uint32 ts = (uint32)sqlite3_column_int(stmt, 3);

		outEdges->AddString("from_key", fromKey);
		outEdges->AddString("to_key", toKey);
		outEdges->AddInt8("snr", snr);
		outEdges->AddInt32("timestamp", (int32)ts);
		count++;
	}
	sqlite3_finalize(stmt);
	return count;
}


void
DatabaseManager::PruneOldEdges(uint32 maxAgeDays)
{
	BAutolock lock(fLock);
	if (fDB == NULL)
		return;

	int64_t cutoff = (int64_t)time(NULL) - (int64_t)(maxAgeDays * 86400);

	const char* sql = "DELETE FROM topology_edges WHERE timestamp < ?";
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, cutoff);
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			fprintf(stderr, "[DatabaseManager] prune topology_edges: %s\n",
				sqlite3_errmsg(fDB));
		sqlite3_finalize(stmt);
	}
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

	// Atomic: remove from old group + insert into new group
	_Execute("BEGIN TRANSACTION");

	// Remove from any existing group first (1 group per contact)
	const char* removeSql =
		"DELETE FROM contact_group_members WHERE contact_key = ?";
	sqlite3_stmt* removeStmt;
	if (sqlite3_prepare_v2(fDB, removeSql, -1, &removeStmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(removeStmt, 1, contactKeyHex, -1, SQLITE_TRANSIENT);
		int rc = sqlite3_step(removeStmt);
		sqlite3_finalize(removeStmt);
		if (rc != SQLITE_DONE) {
			fprintf(stderr, "[DatabaseManager] remove from group failed: %s\n",
				sqlite3_errmsg(fDB));
			_Execute("ROLLBACK");
			return false;
		}
	}

	const char* sql =
		"INSERT OR REPLACE INTO contact_group_members "
		"(group_name, contact_key) VALUES (?, ?)";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(fDB, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] prepare failed: %s\n",
			sqlite3_errmsg(fDB));
		_Execute("ROLLBACK");
		return false;
	}

	sqlite3_bind_text(stmt, 1, groupName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, contactKeyHex, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		fprintf(stderr, "[DatabaseManager] insert into group failed: %s\n",
			sqlite3_errmsg(fDB));
		_Execute("ROLLBACK");
		return false;
	}

	_Execute("COMMIT");
	return true;
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

	int64_t now = (int64_t)time(NULL);
	sqlite3_bind_int(stmt, 1, nodeId);
	sqlite3_bind_text(stmt, 2, sensorName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, sensorType);
	sqlite3_bind_double(stmt, 4, (double)value);
	sqlite3_bind_text(stmt, 5, unit, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 6, now);

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
	sqlite3_bind_int64(stmt, 3, (int64_t)sinceTimestamp);

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
	int errors = 0;
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

				if (InsertMessage("channel", msg))
					migrated++;
				else
					errors++;
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

				if (InsertMessage(contactHex, msg))
					migrated++;
				else
					errors++;
			}
		}

		line = strtok_r(NULL, "\n", &saveptr);
	}

	if (errors > 0 && migrated == 0) {
		fprintf(stderr, "[DatabaseManager] Migration failed (%d errors), "
			"rolling back\n", errors);
		_Execute("ROLLBACK");
	} else {
		_Execute("COMMIT");
		if (errors > 0)
			fprintf(stderr, "[DatabaseManager] Migration partial: %d errors\n",
				errors);
	}
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
		"  snr INTEGER DEFAULT 0,"
		"  txt_type INTEGER DEFAULT 0"
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
	// Include sender_key so different senders with same text+timestamp
	// on channel don't collide; companion_key partitions per companion
	_Execute("DROP INDEX IF EXISTS idx_messages_unique");
	_Execute(
		"CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_unique "
		"ON messages (companion_key, contact_key, timestamp, sender_key, text)");

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
	if (!ok)
		return false;

	// Index for fast group membership lookup by contact
	_Execute(
		"CREATE INDEX IF NOT EXISTS idx_group_members_contact "
		"ON contact_group_members (contact_key)");

	// Topology edges table — discovered inter-node connections
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS topology_edges ("
		"  from_key TEXT NOT NULL,"
		"  to_key TEXT NOT NULL,"
		"  snr INTEGER DEFAULT 0,"
		"  timestamp INTEGER NOT NULL,"
		"  PRIMARY KEY (from_key, to_key)"
		")");
	if (!ok)
		return false;

	// Index for fast pruning by timestamp
	_Execute(
		"CREATE INDEX IF NOT EXISTS idx_topology_edges_timestamp "
		"ON topology_edges (timestamp)");

	// Images table — BLOB storage for LoRa image sharing
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS images ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  session_id INTEGER NOT NULL UNIQUE,"
		"  contact_key TEXT NOT NULL,"
		"  timestamp INTEGER NOT NULL,"
		"  width INTEGER,"
		"  height INTEGER,"
		"  jpeg_data BLOB NOT NULL,"
		"  jpeg_size INTEGER NOT NULL"
		")");
	if (!ok)
		return false;

	_Execute(
		"CREATE INDEX IF NOT EXISTS idx_images_contact "
		"ON images (contact_key, timestamp)");

	// Voice clips table — BLOB storage for voice messages
	ok = _Execute(
		"CREATE TABLE IF NOT EXISTS voice_clips ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  session_id INTEGER NOT NULL UNIQUE,"
		"  contact_key TEXT NOT NULL,"
		"  timestamp INTEGER NOT NULL,"
		"  duration_sec INTEGER DEFAULT 0,"
		"  codec2_mode INTEGER DEFAULT 3,"
		"  codec2_data BLOB NOT NULL,"
		"  codec2_size INTEGER NOT NULL"
		")");
	if (!ok)
		return false;

	_Execute(
		"CREATE INDEX IF NOT EXISTS idx_voice_clips_contact "
		"ON voice_clips (contact_key, timestamp)");

	return true;
}


bool
DatabaseManager::_Execute(const char* sql)
{
	if (fDB == NULL)
		return false;

	char* errMsg = NULL;
	int rc = sqlite3_exec(fDB, sql, NULL, NULL, &errMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[DatabaseManager] SQL error (rc=%d): %s\n",
			rc, errMsg ? errMsg : "unknown");
		sqlite3_free(errMsg);
		return false;
	}
	return true;
}
