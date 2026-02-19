/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactItem.h — Telegram-style contact list item
 */

#ifndef CONTACTITEM_H
#define CONTACTITEM_H

#include <ListItem.h>
#include <String.h>

#include "Types.h"

class ContactItem : public BListItem {
public:
							ContactItem(const ContactInfo& contact);
							ContactItem(const char* name, bool isChannel = false);
	virtual					~ContactItem();

	virtual void			DrawItem(BView* owner, BRect frame,
								bool complete = false);
	virtual void			Update(BView* owner, const BFont* font);

			void			SetContact(const ContactInfo& contact);
			ContactInfo&	GetContact() { return fContact; }
			const ContactInfo&	GetContact() const { return fContact; }

			void			SetLastMessage(const char* text, uint32 timestamp);
			void			SetUnreadCount(int32 count);
			void			IncrementUnread();
			void			ClearUnread();

			bool			IsChannel() const { return fIsChannel; }
			int32			UnreadCount() const { return fUnreadCount; }
			void			SetChannelIndex(int32 index) { fChannelIndex = index; }
			int32			ChannelIndex() const { return fChannelIndex; }

private:
			void			_DrawAvatar(BView* owner, BRect rect);
			rgb_color		_AvatarColor() const;
			rgb_color		_StatusColor() const;
			void			_FormatTime(char* buffer, size_t size, uint32 timestamp) const;

			ContactInfo		fContact;
			BString			fLastMessage;
			uint32			fLastMessageTime;
			int32			fUnreadCount;
			bool			fIsChannel;
			int32			fChannelIndex;	// -1 = public ch, >= 0 = private ch slot

			float			fBaselineOffset;
};

#endif // CONTACTITEM_H
