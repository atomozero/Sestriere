/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DatabaseManager.h — SQLite message and SNR history persistence
 */

#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <Locker.h>
#include <Message.h>
#include "Compat.h"
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
								OwningObjectList<ChatMessage>& outMessages);
			int32			LoadChannelMessages(
								OwningObjectList<ChatMessage>& outMessages);

			// SNR history
			bool			InsertSNRDataPoint(const char* contactKeyHex,
								uint32 timestamp, int8 snr, int8 rssi,
								uint8 pathLen);
			int32			LoadSNRHistory(const char* contactKeyHex,
								uint32 sinceTimestamp,
								OwningObjectList<SNRDataPoint>& outPoints);

			// Search
			int32			SearchMessages(const char* query,
								OwningObjectList<ChatMessage>& outMessages,
								int32 maxResults = 50);

			// Delivery status update
			bool			UpdateMessageDeliveryStatus(
								const char* contactKeyHex,
								uint32 timestamp, uint8 status,
								uint32 roundTripMs);

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
								OwningObjectList<TelemetryRecord>& outRecords);
			int32			GetTelemetryNodeIds(
								OwningObjectList<BString>& outNodeNames);

			// Mute settings
			bool			SetMuted(const char* keyHex, bool muted);
			bool			IsMuted(const char* keyHex);
			void			LoadAllMuted(BMessage* outMsg);

			// Contact groups
			bool			CreateGroup(const char* name);
			bool			DeleteGroup(const char* name);
			bool			AddContactToGroup(const char* groupName,
								const char* contactKeyHex);
			bool			RemoveContactFromGroup(const char* groupName,
								const char* contactKeyHex);
			int32			LoadGroups(OwningObjectList<BString>& outNames);
			int32			LoadGroupMembers(const char* groupName,
								OwningObjectList<BString>& outKeys);
			BString			GetContactGroup(const char* contactKeyHex);

			// Topology edges (discovered inter-node connections)
			bool			InsertTopologyEdge(const char* fromHex,
								const char* toHex, int8 snr);
			int32			LoadTopologyEdges(
								BMessage* outEdges);
			void			PruneOldEdges(uint32 maxAgeDays);

			// Voice clip persistence (BLOB storage for voice messages)
			bool			InsertVoiceClip(const char* contactKey,
								uint32 sessionId, uint32 durationSec,
								uint8 mode, const uint8* codec2Data,
								size_t dataSize);
			bool			LoadVoiceClip(uint32 sessionId,
								uint8** outData, size_t* outSize,
								uint32* outDuration, uint8* outMode);
			void			PruneOldVoiceClips(uint32 maxAgeDays);

			// Image persistence (BLOB storage for LoRa image sharing)
			bool			InsertImage(const char* contactKey,
								uint32 sessionId, int32 width, int32 height,
								const uint8* jpegData, size_t jpegSize);
			bool			LoadImage(uint32 sessionId,
								uint8** outJpegData, size_t* outSize,
								int32* outWidth, int32* outHeight);
			void			PruneOldImages(uint32 maxAgeDays);

			// Multi-companion partitioning
			void			SetCompanionKey(const char* key);
			const char*		CompanionKey() const { return fCompanionKey; }

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
			char			fCompanionKey[13];  // 12 hex + null
};

#endif // DATABASEMANAGER_H
