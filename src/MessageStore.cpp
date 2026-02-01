/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageStore.cpp — Persistent message storage implementation
 */

#include "MessageStore.h"

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

#include <cstring>
#include <cstdio>

// File format version
static const uint32 kMessageFileVersion = 1;
static const uint32 kMessageFileMagic = 'SMSF';  // Sestriere Message Store File

// In-memory unread tracking
#include <map>
static std::map<BString, int32> sUnreadCounts;


MessageStore* MessageStore::sInstance = NULL;


MessageStore*
MessageStore::Instance()
{
	if (sInstance == NULL)
		sInstance = new MessageStore();
	return sInstance;
}


void
MessageStore::Destroy()
{
	delete sInstance;
	sInstance = NULL;
}


MessageStore::MessageStore()
{
	// Ensure storage directory exists
	BPath path = _GetStoragePath();
	create_directory(path.Path(), 0755);
}


MessageStore::~MessageStore()
{
}


status_t
MessageStore::SaveMessage(const uint8* contactPubKey,
	const ReceivedMessage& message, bool outgoing, const char* senderName)
{
	BPath filePath = _GetContactFilePath(contactPubKey);

	BFile file(filePath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_OPEN_AT_END);
	if (file.InitCheck() != B_OK) {
		// Try creating the file fresh
		file.SetTo(filePath.Path(), B_WRITE_ONLY | B_CREATE_FILE);
		if (file.InitCheck() != B_OK)
			return file.InitCheck();

		// Write header for new file
		file.Write(&kMessageFileMagic, sizeof(kMessageFileMagic));
		file.Write(&kMessageFileVersion, sizeof(kMessageFileVersion));
	}

	// If file is empty, write header
	off_t size;
	file.GetSize(&size);
	if (size == 0) {
		file.Write(&kMessageFileMagic, sizeof(kMessageFileMagic));
		file.Write(&kMessageFileVersion, sizeof(kMessageFileVersion));
	}

	// Write message record
	uint8 outgoingFlag = outgoing ? 1 : 0;
	file.Write(&outgoingFlag, 1);

	// Write timestamp
	file.Write(&message.senderTimestamp, sizeof(message.senderTimestamp));

	// Write path info
	file.Write(&message.pathLen, 1);
	file.Write(&message.snr, 1);

	// Write pub key prefix
	file.Write(message.pubKeyPrefix, kPubKeyPrefixSize);

	// Write text length and text
	uint16 textLen = strlen(message.text);
	file.Write(&textLen, sizeof(textLen));
	file.Write(message.text, textLen);

	// Write sender name
	uint8 nameLen = senderName ? strlen(senderName) : 0;
	file.Write(&nameLen, 1);
	if (nameLen > 0)
		file.Write(senderName, nameLen);

	// Increment unread count for incoming messages
	if (!outgoing) {
		BString key = _PubKeyToHex(contactPubKey);
		sUnreadCounts[key]++;
	}

	return B_OK;
}


status_t
MessageStore::LoadMessages(const uint8* contactPubKey,
	BObjectList<StoredMessage>& outMessages)
{
	outMessages.MakeEmpty();

	BPath filePath = _GetContactFilePath(contactPubKey);

	BFile file(filePath.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return B_OK;  // No messages yet, not an error

	// Read and verify header
	uint32 magic, version;
	if (file.Read(&magic, sizeof(magic)) != sizeof(magic))
		return B_OK;
	if (magic != kMessageFileMagic)
		return B_BAD_DATA;

	if (file.Read(&version, sizeof(version)) != sizeof(version))
		return B_OK;
	if (version != kMessageFileVersion)
		return B_BAD_DATA;

	// Read messages
	while (true) {
		uint8 outgoingFlag;
		if (file.Read(&outgoingFlag, 1) != 1)
			break;

		StoredMessage* stored = new StoredMessage();
		stored->outgoing = (outgoingFlag != 0);

		// Read timestamp
		if (file.Read(&stored->message.senderTimestamp,
				sizeof(stored->message.senderTimestamp))
				!= sizeof(stored->message.senderTimestamp)) {
			delete stored;
			break;
		}

		// Read path info
		if (file.Read(&stored->message.pathLen, 1) != 1) {
			delete stored;
			break;
		}
		if (file.Read(&stored->message.snr, 1) != 1) {
			delete stored;
			break;
		}

		// Read pub key prefix
		if (file.Read(stored->message.pubKeyPrefix, kPubKeyPrefixSize)
				!= kPubKeyPrefixSize) {
			delete stored;
			break;
		}

		// Read text
		uint16 textLen;
		if (file.Read(&textLen, sizeof(textLen)) != sizeof(textLen)) {
			delete stored;
			break;
		}

		if (textLen >= sizeof(stored->message.text)) {
			delete stored;
			break;
		}

		if (file.Read(stored->message.text, textLen) != textLen) {
			delete stored;
			break;
		}
		stored->message.text[textLen] = '\0';

		// Read sender name
		uint8 nameLen;
		if (file.Read(&nameLen, 1) != 1) {
			delete stored;
			break;
		}

		if (nameLen > 0) {
			char nameBuf[256];
			if (file.Read(nameBuf, nameLen) != nameLen) {
				delete stored;
				break;
			}
			nameBuf[nameLen] = '\0';
			stored->senderName = nameBuf;
		}

		outMessages.AddItem(stored);
	}

	return B_OK;
}


status_t
MessageStore::ClearMessages(const uint8* contactPubKey)
{
	BPath filePath = _GetContactFilePath(contactPubKey);
	BEntry entry(filePath.Path());

	if (entry.Exists())
		return entry.Remove();

	return B_OK;
}


int32
MessageStore::GetUnreadCount(const uint8* contactPubKey)
{
	BString key = _PubKeyToHex(contactPubKey);
	auto it = sUnreadCounts.find(key);
	if (it != sUnreadCounts.end())
		return it->second;
	return 0;
}


void
MessageStore::MarkAsRead(const uint8* contactPubKey)
{
	BString key = _PubKeyToHex(contactPubKey);
	sUnreadCounts[key] = 0;
}


BString
MessageStore::GetLastMessagePreview(const uint8* contactPubKey)
{
	BObjectList<StoredMessage> messages(true);  // owns items
	if (LoadMessages(contactPubKey, messages) != B_OK)
		return "";

	if (messages.CountItems() == 0)
		return "";

	StoredMessage* last = messages.ItemAt(messages.CountItems() - 1);
	BString preview = last->message.text;

	// Truncate to reasonable length
	if (preview.Length() > 30) {
		preview.Truncate(27);
		preview.Append("...");
	}

	return preview;
}


uint32
MessageStore::GetLastMessageTime(const uint8* contactPubKey)
{
	BObjectList<StoredMessage> messages(true);  // owns items
	if (LoadMessages(contactPubKey, messages) != B_OK)
		return 0;

	if (messages.CountItems() == 0)
		return 0;

	StoredMessage* last = messages.ItemAt(messages.CountItems() - 1);
	return last->message.senderTimestamp;
}


BPath
MessageStore::_GetStoragePath() const
{
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("Sestriere");
	path.Append("messages");
	return path;
}


BPath
MessageStore::_GetContactFilePath(const uint8* contactPubKey) const
{
	BPath path = _GetStoragePath();

	BString filename = _PubKeyToHex(contactPubKey);
	filename.Append(".msg");

	path.Append(filename.String());
	return path;
}


BString
MessageStore::_PubKeyToHex(const uint8* pubKey) const
{
	BString hex;
	for (size_t i = 0; i < kPubKeyPrefixSize; i++) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%02x", pubKey[i]);
		hex.Append(buf);
	}
	return hex;
}
