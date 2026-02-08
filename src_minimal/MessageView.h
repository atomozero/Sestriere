/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageView.h — Individual chat message bubble display
 */

#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <ListItem.h>
#include <String.h>

#include <vector>

#include "Types.h"

class MessageView : public BListItem {
public:
							MessageView(const ChatMessage& message,
								const char* senderName = NULL);
	virtual					~MessageView();

	virtual void			DrawItem(BView* owner, BRect frame,
								bool complete = false);
	virtual void			Update(BView* owner, const BFont* font);

			bool			IsOutgoing() const { return fOutgoing; }
			bool			IsChannel() const { return fIsChannel; }
			uint32			Timestamp() const { return fTimestamp; }
			const char*		Text() const { return fText.String(); }
			const char*		SenderName() const { return fSenderName.String(); }

private:
			void			_FormatTimestamp(char* buffer, size_t size) const;
			void			_WrapText(BView* owner, const BString& text,
								float maxWidth,
								std::vector<BString>& outLines) const;

			BString			fText;
			BString			fSenderName;
			uint32			fTimestamp;
			bool			fOutgoing;
			bool			fIsChannel;
			uint8			fPathLen;
			int8			fSnr;

			float			fBaselineOffset;
};

#endif // MESSAGEVIEW_H
