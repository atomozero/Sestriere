/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactItem.h — Custom list item for contacts
 */

#ifndef CONTACTITEM_H
#define CONTACTITEM_H

#include <ListItem.h>
#include <String.h>

#include "Types.h"

class ContactItem : public BListItem {
public:
							ContactItem(const Contact& contact);
	virtual					~ContactItem();

	virtual void			DrawItem(BView* owner, BRect frame,
								bool complete = false);
	virtual void			Update(BView* owner, const BFont* font);

			const Contact&	GetContact() const { return fContact; }
			Contact&		GetContact() { return fContact; }
			void			SetContact(const Contact& contact);

			const char*		Name() const { return fContact.advName; }
			uint8			Type() const { return fContact.type; }
			bool			HasPath() const { return fContact.outPathLen >= 0; }

			void			SetUnreadCount(int32 count);
			int32			UnreadCount() const { return fUnreadCount; }

			void			SetLastMessage(const char* text, uint32 timestamp);
			const char*		LastMessagePreview() const { return fLastMessage.String(); }
			uint32			LastMessageTime() const { return fLastMessageTime; }

			void			RefreshMessageInfo();

private:
			const char*		_GetTypeIcon() const;
			void			_GetStatusColor(rgb_color& outColor) const;
			void			_FormatRelativeTime(uint32 timestamp, char* buffer,
								size_t size) const;

			Contact			fContact;
			float			fBaselineOffset;
			float			fIconSize;

			int32			fUnreadCount;
			BString			fLastMessage;
			uint32			fLastMessageTime;
};

#endif // CONTACTITEM_H
