/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChannelItem.h — Special list item for public channel
 */

#ifndef CHANNELITEM_H
#define CHANNELITEM_H

#include <ListItem.h>
#include <String.h>

class ChannelItem : public BListItem {
public:
							ChannelItem(uint8 channelIndex = 0,
								const char* name = "Public Channel");
	virtual					~ChannelItem();

	virtual void			DrawItem(BView* owner, BRect frame,
								bool complete = false);
	virtual void			Update(BView* owner, const BFont* font);

			uint8			ChannelIndex() const { return fChannelIndex; }
			const char*		Name() const { return fName.String(); }
			void			SetName(const char* name) { fName = name; }
			void			SetUnreadCount(int32 count);
			int32			UnreadCount() const { return fUnreadCount; }

			bool			IsChannel() const { return true; }

private:
			uint8			fChannelIndex;
			BString			fName;
			int32			fUnreadCount;
			float			fBaselineOffset;
};

#endif // CHANNELITEM_H
