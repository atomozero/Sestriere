/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatView.h — Chat message display view
 */

#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <ListView.h>
#include "Compat.h"
#include <String.h>

#include "Types.h"

class BMessageRunner;
class MessageView;

class ChatView : public BListView {
public:
							ChatView(const char* name);
	virtual					~ChatView();

	virtual void			AttachedToWindow();
	virtual void			FrameResized(float newWidth, float newHeight);
	virtual void			MouseDown(BPoint where);
	virtual bool			InitiateDrag(BPoint point, int32 index,
								bool wasSelected);
	virtual void			MessageReceived(BMessage* message);

			void			AddMessage(const ChatMessage& message,
								const char* senderName = NULL);
			void			UpdateDeliveryStatus(int32 index, uint8 status,
								uint32 rtt = 0, uint8 retryCount = 0);
			void			ClearMessages();

			void			SetCurrentContact(ContactInfo* contact);
			ContactInfo*	CurrentContact() const { return fCurrentContact; }

			// Access to message history for the current contact
			OwningObjectList<ChatMessage>* GetMessageHistory();

			void			SetSelfName(const char* name);

			// GIF animation
			void			StartGifAnimation();

private:
			void			_ScrollToBottom();

			ContactInfo*	fCurrentContact;
			BString			fCurrentContactName;
			BString			fSelfName;
			BMessageRunner*	fGifAnimateRunner;

			// Message history per contact (pubkey prefix -> messages)
			// For simplicity, we store messages in memory only
			// Key is hex string of first 6 bytes of pubkey
			OwningObjectList<ChatMessage>	fCurrentMessages;
};

#endif // CHATVIEW_H
