/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChannelItem.cpp — Special list item for public channel implementation
 */

#include "ChannelItem.h"

#include <View.h>

#include <cstdio>


ChannelItem::ChannelItem(uint8 channelIndex, const char* name)
	:
	BListItem(),
	fChannelIndex(channelIndex),
	fName(name),
	fUnreadCount(0),
	fBaselineOffset(0)
{
}


ChannelItem::~ChannelItem()
{
}


void
ChannelItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	(void)complete;
	rgb_color lowColor = owner->LowColor();
	rgb_color highColor = owner->HighColor();

	// Background - slightly different for channels
	if (IsSelected()) {
		owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
		owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
	} else {
		// Channels have a subtle tinted background
		rgb_color channelBg = ui_color(B_LIST_BACKGROUND_COLOR);
		channelBg.red = (uint8)(channelBg.red * 0.95);
		channelBg.green = (uint8)(channelBg.green * 0.98);
		channelBg.blue = (uint8)(channelBg.blue * 1.0);
		owner->SetLowColor(channelBg);
		owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	}

	owner->FillRect(frame, B_SOLID_LOW);

	// Draw channel icon (broadcast symbol)
	BPoint iconPos(frame.left + 4, frame.top + fBaselineOffset);
	owner->DrawString("\xF0\x9F\x93\xA2", iconPos);  // Megaphone/loudspeaker emoji

	// Draw channel name
	BPoint textPos(frame.left + 24, frame.top + fBaselineOffset);
	owner->SetFont(be_bold_font);
	owner->DrawString(fName.String(), textPos);
	owner->SetFont(be_plain_font);

	// Draw unread count badge if any
	if (fUnreadCount > 0) {
		char badge[16];
		snprintf(badge, sizeof(badge), "(%d)", (int)fUnreadCount);

		float badgeWidth = owner->StringWidth(badge);
		BPoint badgePos(frame.right - badgeWidth - 8,
			frame.top + fBaselineOffset);

		// Draw badge background
		rgb_color badgeColor = {200, 50, 50, 255};
		owner->SetHighColor(badgeColor);

		BRect badgeRect(badgePos.x - 4, frame.top + 2,
			badgePos.x + badgeWidth + 4, frame.bottom - 2);
		owner->FillRoundRect(badgeRect, 3, 3);

		// Draw badge text
		owner->SetHighColor(255, 255, 255);
		owner->DrawString(badge, badgePos);
	}

	// Restore colors
	owner->SetLowColor(lowColor);
	owner->SetHighColor(highColor);
}


void
ChannelItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	font_height fontHeight;
	font->GetHeight(&fontHeight);

	fBaselineOffset = fontHeight.ascent + fontHeight.leading + 2;

	float height = fontHeight.ascent + fontHeight.descent +
		fontHeight.leading + 6;
	SetHeight(height);
}


void
ChannelItem::SetUnreadCount(int32 count)
{
	fUnreadCount = count;
}
