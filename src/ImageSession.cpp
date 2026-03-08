/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ImageSession.cpp — Image fragment session management for LoRa image sharing
 */

#include "ImageSession.h"

#include <OS.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>


// IE2 envelope prefix
static const char* kIE2Prefix = "IE2:";
static const size_t kIE2PrefixLen = 4;


ImageSessionManager::ImageSessionManager()
	:
	fSessions(8),
	fNextSessionId(1)
{
	// Seed session IDs from system time for uniqueness
	fNextSessionId = (uint32)(real_time_clock() & 0x7FFFFFFF);
	if (fNextSessionId == 0)
		fNextSessionId = 1;
}


ImageSessionManager::~ImageSessionManager()
{
	// OwningObjectList handles deletion
}


uint32
ImageSessionManager::CreateOutgoing(const uint8* jpegData, size_t size,
	int32 width, int32 height, const uint8* selfKeyPrefix)
{
	uint32 sid = fNextSessionId++;

	ImageSession* session = new ImageSession();
	session->sessionId = sid;
	session->format = kImageFormatWebP;
	session->width = width;
	session->height = height;
	session->totalBytes = (uint32)size;
	session->timestamp = (uint32)real_time_clock();
	session->state = IMAGE_SENDING;
	session->createdTime = system_time();

	if (selfKeyPrefix != NULL)
		memcpy(session->senderKey, selfKeyPrefix, 6);

	// Copy JPEG data for retransmission
	session->jpegData = (uint8*)malloc(size);
	if (session->jpegData != NULL) {
		memcpy(session->jpegData, jpegData, size);
		session->jpegSize = size;
	}

	// Fragment the data
	uint8 numFrags = (uint8)((size + kMaxFragmentPayload - 1)
		/ kMaxFragmentPayload);
	session->totalFragments = numFrags;
	session->receivedCount = numFrags;  // All fragments available locally

	size_t offset = 0;
	for (uint8 i = 0; i < numFrags; i++) {
		size_t chunkLen = size - offset;
		if (chunkLen > kMaxFragmentPayload)
			chunkLen = kMaxFragmentPayload;

		memcpy(session->fragments[i].data, jpegData + offset, chunkLen);
		session->fragments[i].length = (uint16)chunkLen;
		session->fragments[i].received = true;
		offset += chunkLen;
	}

	fSessions.AddItem(session);
	return sid;
}


ImageSession*
ImageSessionManager::CreateFromEnvelope(const char* ie2Text)
{
	uint32 sid = 0;
	uint8 format = 0, total = 0;
	int32 width = 0, height = 0;
	uint32 totalBytes = 0, timestamp = 0;
	uint8 senderKey[6];
	memset(senderKey, 0, sizeof(senderKey));

	if (!ParseEnvelope(ie2Text, &sid, &format, &total, &width, &height,
		&totalBytes, senderKey, &timestamp))
		return NULL;

	// Check if session already exists
	ImageSession* existing = FindSession(sid);
	if (existing != NULL)
		return existing;

	ImageSession* session = new ImageSession();
	session->sessionId = sid;
	session->format = format;
	session->totalFragments = total;
	session->receivedCount = 0;
	session->width = width;
	session->height = height;
	session->totalBytes = totalBytes;
	memcpy(session->senderKey, senderKey, 6);
	session->timestamp = timestamp;
	session->state = IMAGE_PENDING;
	session->createdTime = system_time();

	fSessions.AddItem(session);
	return session;
}


bool
ImageSessionManager::AddFragment(uint32 sessionId, uint8 index,
	const uint8* data, uint16 length)
{
	ImageSession* session = FindSession(sessionId);
	if (session == NULL)
		return false;

	if (index >= session->totalFragments)
		return false;

	if (length > kMaxFragmentPayload)
		length = kMaxFragmentPayload;

	// Don't count duplicates
	if (!session->fragments[index].received) {
		session->fragments[index].received = true;
		session->receivedCount++;
	}

	memcpy(session->fragments[index].data, data, length);
	session->fragments[index].length = length;

	if (session->state == IMAGE_PENDING)
		session->state = IMAGE_LOADING;

	return session->IsComplete();
}


ImageSession*
ImageSessionManager::FindSession(uint32 sessionId)
{
	for (int32 i = 0; i < fSessions.CountItems(); i++) {
		ImageSession* s = fSessions.ItemAt(i);
		if (s != NULL && s->sessionId == sessionId)
			return s;
	}
	return NULL;
}


void
ImageSessionManager::PurgeExpired()
{
	for (int32 i = fSessions.CountItems() - 1; i >= 0; i--) {
		ImageSession* s = fSessions.ItemAt(i);
		if (s != NULL && s->IsExpired() && s->state != IMAGE_COMPLETE)
			fSessions.RemoveItemAt(i);
	}
}


// --- Static helpers ---


BString
ImageSessionManager::FormatEnvelope(uint32 sessionId, uint8 format,
	uint8 totalFragments, int32 width, int32 height, uint32 totalBytes,
	const uint8* senderKey, uint32 timestamp)
{
	char keyHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(keyHex + i * 2, 3, "%02x", senderKey[i]);

	BString envelope;
	envelope.SetToFormat("IE2:%08x:%d:%d:%ld:%ld:%lu:%s:%lu",
		sessionId, (int)format, (int)totalFragments,
		(long)width, (long)height,
		(unsigned long)totalBytes, keyHex,
		(unsigned long)timestamp);
	return envelope;
}


bool
ImageSessionManager::ParseEnvelope(const char* text, uint32* outSid,
	uint8* outFormat, uint8* outTotal, int32* outWidth, int32* outHeight,
	uint32* outBytes, uint8* outSenderKey, uint32* outTimestamp)
{
	if (text == NULL || strncmp(text, kIE2Prefix, kIE2PrefixLen) != 0)
		return false;

	const char* p = text + kIE2PrefixLen;

	// Parse session ID (8 hex chars)
	unsigned int sid;
	int format, total;
	long width, height;
	unsigned long totalBytes, timestamp;
	char keyHex[13];
	memset(keyHex, 0, sizeof(keyHex));

	int parsed = sscanf(p, "%8x:%d:%d:%ld:%ld:%lu:%12[0-9a-f]:%lu",
		&sid, &format, &total, &width, &height, &totalBytes,
		keyHex, &timestamp);

	if (parsed < 7)
		return false;

	if (outSid != NULL) *outSid = (uint32)sid;
	if (outFormat != NULL) *outFormat = (uint8)format;
	if (outTotal != NULL) *outTotal = (uint8)total;
	if (outWidth != NULL) *outWidth = (int32)width;
	if (outHeight != NULL) *outHeight = (int32)height;
	if (outBytes != NULL) *outBytes = (uint32)totalBytes;
	if (outTimestamp != NULL) *outTimestamp = (parsed >= 8)
		? (uint32)timestamp : (uint32)real_time_clock();

	if (outSenderKey != NULL) {
		for (int i = 0; i < 6 && keyHex[i * 2] != '\0'; i++) {
			unsigned int byte;
			if (sscanf(keyHex + i * 2, "%2x", &byte) == 1)
				outSenderKey[i] = (uint8)byte;
		}
	}

	return true;
}


bool
ImageSessionManager::IsImageEnvelope(const char* text)
{
	return text != NULL && strncmp(text, kIE2Prefix, kIE2PrefixLen) == 0;
}


size_t
ImageSessionManager::BuildFragmentPacket(uint8* outPacket, uint32 sessionId,
	uint8 format, uint8 index, uint8 total, const uint8* fragData,
	uint16 fragLen)
{
	if (outPacket == NULL || fragData == NULL)
		return 0;

	// Header: [magic:1][sid_lo..sid_hi:4][format:1][index:1][total:1]
	outPacket[0] = kImageMagic;
	outPacket[1] = (uint8)(sessionId & 0xFF);
	outPacket[2] = (uint8)((sessionId >> 8) & 0xFF);
	outPacket[3] = (uint8)((sessionId >> 16) & 0xFF);
	outPacket[4] = (uint8)((sessionId >> 24) & 0xFF);
	outPacket[5] = format;
	outPacket[6] = index;
	outPacket[7] = total;

	if (fragLen > kMaxFragmentPayload)
		fragLen = kMaxFragmentPayload;

	memcpy(outPacket + kImageHeaderSize, fragData, fragLen);

	return kImageHeaderSize + fragLen;
}


size_t
ImageSessionManager::BuildFetchRequest(uint8* outPacket, uint32 sessionId,
	const uint8* missingIndices, uint8 count)
{
	if (outPacket == NULL || missingIndices == NULL || count == 0)
		return 0;

	// Header: [magic:1][sid:4][count:1][indices...]
	outPacket[0] = kFetchMagic;
	outPacket[1] = (uint8)(sessionId & 0xFF);
	outPacket[2] = (uint8)((sessionId >> 8) & 0xFF);
	outPacket[3] = (uint8)((sessionId >> 16) & 0xFF);
	outPacket[4] = (uint8)((sessionId >> 24) & 0xFF);
	outPacket[5] = count;

	memcpy(outPacket + 6, missingIndices, count);

	return 6 + count;
}
