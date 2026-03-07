/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * VoiceSession.cpp — Voice message session management for LoRa voice sharing
 */

#include "VoiceSession.h"

#include <OS.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>


// VE2 envelope prefix
static const char* kVE2Prefix = "VE2:";
static const size_t kVE2PrefixLen = 4;


// =============================================================================
// Base-36 helpers (meshcore-sar compatible)
// =============================================================================

BString
VoiceSessionManager::ToBase36(uint32 value)
{
	if (value == 0)
		return BString("0");

	static const char kDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	char buf[16];
	int pos = sizeof(buf) - 1;
	buf[pos] = '\0';

	while (value > 0 && pos > 0) {
		buf[--pos] = kDigits[value % 36];
		value /= 36;
	}

	return BString(buf + pos);
}


uint32
VoiceSessionManager::FromBase36(const char* str)
{
	if (str == NULL || *str == '\0')
		return 0;

	uint32 result = 0;
	while (*str != '\0' && *str != ':') {
		result *= 36;
		char c = *str;
		if (c >= '0' && c <= '9')
			result += (c - '0');
		else if (c >= 'a' && c <= 'z')
			result += 10 + (c - 'a');
		else if (c >= 'A' && c <= 'Z')
			result += 10 + (c - 'A');
		else
			break;
		str++;
	}
	return result;
}


// =============================================================================
// VoiceSessionManager
// =============================================================================

VoiceSessionManager::VoiceSessionManager()
	:
	fSessions(8),
	fNextSessionId(1)
{
	// Seed session IDs from system time for uniqueness
	fNextSessionId = (uint32)(real_time_clock() & 0x7FFFFFFF);
	if (fNextSessionId == 0)
		fNextSessionId = 1;
}


VoiceSessionManager::~VoiceSessionManager()
{
	// OwningObjectList handles deletion
}


uint32
VoiceSessionManager::CreateOutgoing(const uint8* codec2Data, size_t size,
	VoicePacketMode mode, uint32 durationSec, const uint8* selfKeyPrefix)
{
	uint32 sid = fNextSessionId++;

	VoiceSession* session = new VoiceSession();
	session->sessionId = sid;
	session->mode = mode;
	session->durationSec = durationSec;
	session->totalBytes = (uint32)size;
	session->timestamp = (uint32)real_time_clock();
	session->state = VOICE_SENDING;
	session->createdTime = system_time();

	if (selfKeyPrefix != NULL)
		memcpy(session->senderKey, selfKeyPrefix, 6);

	// Copy Codec2 data for retransmission
	session->codec2Data = (uint8*)malloc(size);
	if (session->codec2Data != NULL) {
		memcpy(session->codec2Data, codec2Data, size);
		session->codec2Size = size;
	}

	// Fragment the data
	uint8 numFrags = (uint8)((size + kMaxVoiceFragmentPayload - 1)
		/ kMaxVoiceFragmentPayload);
	session->totalFragments = numFrags;
	session->receivedCount = numFrags;  // All fragments available locally

	size_t offset = 0;
	for (uint8 i = 0; i < numFrags; i++) {
		size_t chunkLen = size - offset;
		if (chunkLen > kMaxVoiceFragmentPayload)
			chunkLen = kMaxVoiceFragmentPayload;

		memcpy(session->fragments[i].data, codec2Data + offset, chunkLen);
		session->fragments[i].length = (uint16)chunkLen;
		session->fragments[i].received = true;
		offset += chunkLen;
	}

	fSessions.AddItem(session);
	return sid;
}


VoiceSession*
VoiceSessionManager::CreateFromEnvelope(const char* ve2Text)
{
	uint32 sid = 0;
	VoicePacketMode mode = VOICE_MODE_1300;
	uint8 total = 0;
	uint32 duration = 0, timestamp = 0;
	uint8 senderKey[6];
	memset(senderKey, 0, sizeof(senderKey));

	if (!ParseEnvelope(ve2Text, &sid, &mode, &total, &duration,
		senderKey, &timestamp))
		return NULL;

	// Check if session already exists
	VoiceSession* existing = FindSession(sid);
	if (existing != NULL)
		return existing;

	VoiceSession* session = new VoiceSession();
	session->sessionId = sid;
	session->mode = mode;
	session->totalFragments = total;
	session->receivedCount = 0;
	session->durationSec = duration;
	memcpy(session->senderKey, senderKey, 6);
	session->timestamp = timestamp;
	session->state = VOICE_PENDING;
	session->createdTime = system_time();

	fSessions.AddItem(session);
	return session;
}


bool
VoiceSessionManager::AddFragment(uint32 sessionId, uint8 index,
	const uint8* data, uint16 length)
{
	VoiceSession* session = FindSession(sessionId);
	if (session == NULL)
		return false;

	if (index >= session->totalFragments)
		return false;

	if (length > kMaxVoiceFragmentPayload)
		length = kMaxVoiceFragmentPayload;

	// Don't count duplicates
	if (!session->fragments[index].received) {
		session->fragments[index].received = true;
		session->receivedCount++;
	}

	memcpy(session->fragments[index].data, data, length);
	session->fragments[index].length = length;

	if (session->state == VOICE_PENDING)
		session->state = VOICE_LOADING;

	return session->IsComplete();
}


VoiceSession*
VoiceSessionManager::FindSession(uint32 sessionId)
{
	for (int32 i = 0; i < fSessions.CountItems(); i++) {
		VoiceSession* s = fSessions.ItemAt(i);
		if (s != NULL && s->sessionId == sessionId)
			return s;
	}
	return NULL;
}


void
VoiceSessionManager::PurgeExpired()
{
	for (int32 i = fSessions.CountItems() - 1; i >= 0; i--) {
		VoiceSession* s = fSessions.ItemAt(i);
		if (s != NULL && s->IsExpired() && s->state != VOICE_COMPLETE)
			fSessions.RemoveItemAt(i);
	}
}


// =============================================================================
// Static helpers
// =============================================================================


BString
VoiceSessionManager::FormatEnvelope(uint32 sessionId, VoicePacketMode mode,
	uint8 totalFragments, uint32 durationSec, const uint8* senderKey,
	uint32 timestamp)
{
	char keyHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(keyHex + i * 2, 3, "%02x", senderKey[i]);

	// VE2:{sid}:{mode}:{total}:{durS}:{senderKey6}:{ts}
	// All numeric fields in base-36 for meshcore-sar compatibility
	BString envelope;
	envelope.SetToFormat("VE2:%s:%s:%s:%s:%s:%s",
		ToBase36(sessionId).String(),
		ToBase36((uint32)mode).String(),
		ToBase36((uint32)totalFragments).String(),
		ToBase36(durationSec).String(),
		keyHex,
		ToBase36(timestamp).String());
	return envelope;
}


bool
VoiceSessionManager::ParseEnvelope(const char* text, uint32* outSid,
	VoicePacketMode* outMode, uint8* outTotal, uint32* outDuration,
	uint8* outSenderKey, uint32* outTimestamp)
{
	if (text == NULL || strncmp(text, kVE2Prefix, kVE2PrefixLen) != 0)
		return false;

	const char* p = text + kVE2PrefixLen;

	// Parse fields separated by ':'
	// VE2:{sid}:{mode}:{total}:{durS}:{senderKey6}:{ts}
	// Fields are base-36 encoded except senderKey which is hex

	// Field 1: session ID (base-36)
	const char* field = p;
	const char* sep = strchr(field, ':');
	if (sep == NULL) return false;
	if (outSid != NULL) *outSid = FromBase36(field);

	// Field 2: mode (base-36)
	field = sep + 1;
	sep = strchr(field, ':');
	if (sep == NULL) return false;
	if (outMode != NULL) *outMode = (VoicePacketMode)FromBase36(field);

	// Field 3: total fragments (base-36)
	field = sep + 1;
	sep = strchr(field, ':');
	if (sep == NULL) return false;
	if (outTotal != NULL) *outTotal = (uint8)FromBase36(field);

	// Field 4: duration seconds (base-36)
	field = sep + 1;
	sep = strchr(field, ':');
	if (sep == NULL) return false;
	if (outDuration != NULL) *outDuration = FromBase36(field);

	// Field 5: sender key (12-char hex)
	field = sep + 1;
	sep = strchr(field, ':');
	if (sep == NULL) return false;
	if (outSenderKey != NULL) {
		for (int i = 0; i < 6 && (field + i * 2) < sep; i++) {
			unsigned int byte;
			if (sscanf(field + i * 2, "%2x", &byte) == 1)
				outSenderKey[i] = (uint8)byte;
		}
	}

	// Field 6: timestamp (base-36)
	field = sep + 1;
	if (outTimestamp != NULL) {
		if (*field != '\0')
			*outTimestamp = FromBase36(field);
		else
			*outTimestamp = (uint32)real_time_clock();
	}

	return true;
}


bool
VoiceSessionManager::IsVoiceEnvelope(const char* text)
{
	return text != NULL && strncmp(text, kVE2Prefix, kVE2PrefixLen) == 0;
}


size_t
VoiceSessionManager::BuildFragmentPacket(uint8* outPacket, uint32 sessionId,
	VoicePacketMode mode, uint8 index, uint8 total, const uint8* fragData,
	uint16 fragLen)
{
	if (outPacket == NULL || fragData == NULL)
		return 0;

	// Header: [magic:1][sid_lo..sid_hi:4][mode:1][index:1][total:1]
	outPacket[0] = kVoiceMagic;
	outPacket[1] = (uint8)(sessionId & 0xFF);
	outPacket[2] = (uint8)((sessionId >> 8) & 0xFF);
	outPacket[3] = (uint8)((sessionId >> 16) & 0xFF);
	outPacket[4] = (uint8)((sessionId >> 24) & 0xFF);
	outPacket[5] = (uint8)mode;
	outPacket[6] = index;
	outPacket[7] = total;

	if (fragLen > kMaxVoiceFragmentPayload)
		fragLen = kMaxVoiceFragmentPayload;

	memcpy(outPacket + kVoiceHeaderSize, fragData, fragLen);

	return kVoiceHeaderSize + fragLen;
}


size_t
VoiceSessionManager::BuildFetchRequest(uint8* outPacket, uint32 sessionId,
	uint8 flags, const uint8* senderKey, uint32 timestamp,
	const uint8* missingIndices, uint8 count)
{
	if (outPacket == NULL || missingIndices == NULL || count == 0)
		return 0;

	// VR2 header: [magic:1][sid:4][flags:1][key6:6][ts:4][count:1][indices...]
	outPacket[0] = kVoiceFetchMagic;
	outPacket[1] = (uint8)(sessionId & 0xFF);
	outPacket[2] = (uint8)((sessionId >> 8) & 0xFF);
	outPacket[3] = (uint8)((sessionId >> 16) & 0xFF);
	outPacket[4] = (uint8)((sessionId >> 24) & 0xFF);
	outPacket[5] = flags;

	if (senderKey != NULL)
		memcpy(outPacket + 6, senderKey, 6);
	else
		memset(outPacket + 6, 0, 6);

	outPacket[12] = (uint8)(timestamp & 0xFF);
	outPacket[13] = (uint8)((timestamp >> 8) & 0xFF);
	outPacket[14] = (uint8)((timestamp >> 16) & 0xFF);
	outPacket[15] = (uint8)((timestamp >> 24) & 0xFF);
	outPacket[16] = count;

	memcpy(outPacket + 17, missingIndices, count);

	return 17 + count;
}
