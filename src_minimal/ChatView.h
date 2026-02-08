/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatView.h — Chat message display view
 */

#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <ListView.h>
#include <ObjectList.h>
#include <String.h>

#include "Types.h"

class MessageView;

class ChatView : public BListView {
public:
							ChatView(const char* name);
	virtual					~ChatView();

	virtual void			AttachedToWindow();
	virtual void			FrameResized(float newWidth, float newHeight);
	virtual void			MessageReceived(BMessage* message);

			void			AddMessage(const ChatMessage& message,
								const char* senderName = NULL);
			void			ClearMessages();

			void			SetCurrentContact(ContactInfo* contact);
			ContactInfo*	CurrentContact() const { return fCurrentContact; }

			// Access to message history for the current contact
			BObjectList<ChatMessage, true>* GetMessageHistory();

private:
			void			_ScrollToBottom();

			ContactInfo*	fCurrentContact;
			BString			fCurrentContactName;

			// Message history per contact (pubkey prefix -> messages)
			// For simplicity, we store messages in memory only
			// Key is hex string of first 6 bytes of pubkey
			BObjectList<ChatMessage, true>	fCurrentMessages;
};

#endif // CHATVIEW_H
