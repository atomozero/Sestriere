/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageView.h — Individual message display item
 */

#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <ListItem.h>
#include <String.h>

#include "Types.h"

class MessageView : public BListItem {
public:
							MessageView(const ReceivedMessage& message,
								bool outgoing, const char* senderName);
	virtual					~MessageView();

	virtual void			DrawItem(BView* owner, BRect frame,
								bool complete = false);
	virtual void			Update(BView* owner, const BFont* font);

			bool			IsOutgoing() const { return fOutgoing; }
			uint32			Timestamp() const { return fTimestamp; }
			const char*		Text() const { return fText.String(); }
			const char*		SenderName() const { return fSenderName.String(); }

private:
			void			_FormatTimestamp(char* buffer, size_t size) const;
			float			_CalcTextHeight(BView* owner, float maxWidth) const;

			BString			fText;
			BString			fSenderName;
			uint32			fTimestamp;
			bool			fOutgoing;
			uint8			fPathLen;
			uint8			fSnr;

			float			fBaselineOffset;
			float			fTextHeight;
};

#endif // MESSAGEVIEW_H
