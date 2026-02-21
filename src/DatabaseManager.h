/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DatabaseManager.h — SQLite message and SNR history persistence
 */

#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <Locker.h>
#include <ObjectList.h>
#include <String.h>

#include <sqlite3.h>

#include "Types.h"

// SNR data point for charting
struct SNRDataPoint {
	uint32	timestamp;
	int8	snr;
	int8	rssi;
	uint8	pathLen;
};

// Telemetry data point for historical charts
struct TelemetryRecord {
	uint32	timestamp;
	uint32	nodeId;
	uint8	sensorType;
	float	value;
	char	sensorName[32];
	char	unit[16];
};


class DatabaseManager {
public:
	static	DatabaseManager*	Instance();
	static	void				Destroy();

			bool			Open(const char* directory);
			void			Close();
			bool			IsOpen() const { return fDB != NULL; }

			// Message persistence
			bool			InsertMessage(const char* contactKeyHex,
								const ChatMessage& message);
			int32			LoadMessages(const char* contactKeyHex,
								BObjectList<ChatMessage, true>& outMessages);
			int32			LoadChannelMessages(
								BObjectList<ChatMessage, true>& outMessages);

			// SNR history
			bool			InsertSNRDataPoint(const char* contactKeyHex,
								uint32 timestamp, int8 snr, int8 rssi,
								uint8 pathLen);
			int32			LoadSNRHistory(const char* contactKeyHex,
								uint32 sinceTimestamp,
								BObjectList<SNRDataPoint, true>& outPoints);

			// Search
			int32			SearchMessages(const char* query,
								BObjectList<ChatMessage, true>& outMessages,
								int32 maxResults = 50);

			// Statistics
			int32			GetMessageCount(const char* contactKeyHex);
			int32			GetTotalMessageCount();

			// Telemetry history
			bool			InsertTelemetry(uint32 nodeId,
								const char* sensorName,
								uint8 sensorType, float value,
								const char* unit);
			int32			LoadTelemetryHistory(uint32 nodeId,
								const char* sensorName,
								uint32 sinceTimestamp,
								BObjectList<TelemetryRecord, true>& outRecords);
			int32			GetTelemetryNodeIds(
								BObjectList<BString, true>& outNodeNames);

			// Maintenance
			void			PruneOldData(uint32 maxAgeDays);
			void			PruneOldMessages(uint32 maxAgeDays);

private:
							DatabaseManager();
							~DatabaseManager();

			bool			_CreateTables();
			bool			_Execute(const char* sql);
			void			_MigrateFromTextFile(const char* directory);
			bool			_IsEmpty();

	static	DatabaseManager*	sInstance;
	static	BLocker				sInstanceLock;
			sqlite3*		fDB;
			BLocker			fLock;
			BString			fDirectory;
};

#endif // DATABASEMANAGER_H
