/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageStore.h — Persistent message storage
 */

#ifndef MESSAGESTORE_H
#define MESSAGESTORE_H

#include <File.h>
#include <Path.h>
#include <String.h>
#include <ObjectList.h>

#include "Types.h"

// Stored message structure (includes direction flag)
struct StoredMessage {
	ReceivedMessage	message;
	bool			outgoing;
	BString			senderName;
};

class MessageStore {
public:
	static MessageStore*	Instance();
	static void				Destroy();

			status_t		SaveMessage(const uint8* contactPubKey,
								const ReceivedMessage& message,
								bool outgoing,
								const char* senderName = NULL);

			status_t		LoadMessages(const uint8* contactPubKey,
								BObjectList<StoredMessage>& outMessages);

			status_t		ClearMessages(const uint8* contactPubKey);

			int32			GetUnreadCount(const uint8* contactPubKey);
			void			MarkAsRead(const uint8* contactPubKey);

			BString			GetLastMessagePreview(const uint8* contactPubKey);
			uint32			GetLastMessageTime(const uint8* contactPubKey);

private:
							MessageStore();
							~MessageStore();

			BPath			_GetStoragePath() const;
			BPath			_GetContactFilePath(const uint8* contactPubKey) const;
			BString			_PubKeyToHex(const uint8* pubKey) const;

	static MessageStore*	sInstance;
};

#endif // MESSAGESTORE_H
