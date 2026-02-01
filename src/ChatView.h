/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatView.h — Chat message display view
 */

#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <ListView.h>
#include <String.h>

#include "Types.h"

class MessageView;

class ChatView : public BListView {
public:
							ChatView(const char* name);
	virtual					~ChatView();

	virtual void			AttachedToWindow();
	virtual void			MessageReceived(BMessage* message);

			void			AddMessage(const ReceivedMessage& message,
								bool outgoing);
			void			ClearMessages();

			void			SetCurrentContact(const Contact* contact);
			const Contact*	CurrentContact() const { return fCurrentContact; }

private:
			void			_ScrollToBottom();
			const char*		_FindContactName(const uint8* pubKeyPrefix) const;

			const Contact*	fCurrentContact;
			BString			fCurrentContactName;
};

#endif // CHATVIEW_H
