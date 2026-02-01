/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactItem.cpp — Custom list item for contacts implementation
 */

#include "ContactItem.h"

#include <View.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "MessageStore.h"


// Badge colors
static const rgb_color kUnreadBadgeColor = {66, 133, 244, 255};    // Blue
static const rgb_color kUnreadBadgeText = {255, 255, 255, 255};    // White
static const rgb_color kPreviewTextColor = {128, 128, 128, 255};   // Gray


ContactItem::ContactItem(const Contact& contact)
	:
	BListItem(),
	fBaselineOffset(0),
	fIconSize(16),
	fUnreadCount(0),
	fLastMessage(""),
	fLastMessageTime(0)
{
	memcpy(&fContact, &contact, sizeof(Contact));
	RefreshMessageInfo();
}


ContactItem::~ContactItem()
{
}


void
ContactItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	(void)complete;
	rgb_color lowColor = owner->LowColor();
	rgb_color highColor = owner->HighColor();

	// Background
	if (IsSelected()) {
		owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
		owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
	} else {
		owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
		owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	}

	owner->FillRect(frame, B_SOLID_LOW);

	font_height fh;
	owner->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	// Draw type icon/indicator
	const char* icon = _GetTypeIcon();
	BPoint iconPos(frame.left + 4, frame.top + fBaselineOffset);
	owner->DrawString(icon, iconPos);

	// Calculate positions
	float nameX = frame.left + fIconSize + 8;
	float rightMargin = frame.right - 8;

	// Draw unread badge if present
	float badgeWidth = 0;
	if (fUnreadCount > 0) {
		BString countStr;
		countStr.SetToFormat("%d", fUnreadCount > 99 ? 99 : fUnreadCount);
		if (fUnreadCount > 99)
			countStr.Append("+");

		float textWidth = owner->StringWidth(countStr.String());
		badgeWidth = textWidth + 12;
		float badgeHeight = lineHeight - 2;
		float badgeX = rightMargin - badgeWidth;
		float badgeY = frame.top + (frame.Height() - badgeHeight) / 2;

		// Draw badge background
		BRect badgeRect(badgeX, badgeY, badgeX + badgeWidth, badgeY + badgeHeight);
		owner->SetHighColor(kUnreadBadgeColor);
		owner->FillRoundRect(badgeRect, badgeHeight / 2, badgeHeight / 2);

		// Draw badge text
		owner->SetHighColor(kUnreadBadgeText);
		owner->DrawString(countStr.String(),
			BPoint(badgeX + 6, badgeY + fh.ascent));

		rightMargin = badgeX - 4;
	}

	// Draw status indicator (path quality)
	if (HasPath() && fUnreadCount == 0) {
		rgb_color pathColor;
		_GetStatusColor(pathColor);
		owner->SetHighColor(pathColor);

		BPoint statusPos(rightMargin - 8, frame.top + fBaselineOffset);
		owner->DrawString("\xE2\x97\x8F", statusPos);  // Filled circle
		rightMargin -= 12;
	}

	// Draw name (first line)
	if (IsSelected())
		owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
	else
		owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));

	BPoint textPos(nameX, frame.top + fBaselineOffset);
	owner->DrawString(fContact.advName, textPos);

	// Draw last message preview and time (second line)
	if (fLastMessage.Length() > 0 || fLastMessageTime > 0) {
		// Time on the right
		if (fLastMessageTime > 0) {
			char timeStr[32];
			_FormatRelativeTime(fLastMessageTime, timeStr, sizeof(timeStr));

			owner->SetHighColor(kPreviewTextColor);
			float timeWidth = owner->StringWidth(timeStr);
			owner->DrawString(timeStr,
				BPoint(frame.right - 8 - timeWidth, frame.top + fBaselineOffset + lineHeight));
		}

		// Message preview on the left
		if (fLastMessage.Length() > 0) {
			owner->SetHighColor(kPreviewTextColor);

			// Truncate if needed
			BString preview = fLastMessage;
			float maxWidth = frame.right - nameX - 60;  // Leave room for time
			while (preview.Length() > 0 &&
					owner->StringWidth(preview.String()) > maxWidth) {
				preview.Truncate(preview.Length() - 1);
			}
			if (preview.Length() < fLastMessage.Length())
				preview.Append("...");

			owner->DrawString(preview.String(),
				BPoint(nameX, frame.top + fBaselineOffset + lineHeight));
		}
	}

	// Restore colors
	owner->SetLowColor(lowColor);
	owner->SetHighColor(highColor);
}


void
ContactItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	font_height fontHeight;
	font->GetHeight(&fontHeight);

	fBaselineOffset = fontHeight.ascent + fontHeight.leading + 4;

	float lineHeight = fontHeight.ascent + fontHeight.descent + fontHeight.leading;

	// Two lines: name + preview
	float height = lineHeight * 2 + 8;
	SetHeight(height);

	fIconSize = fontHeight.ascent;
}


void
ContactItem::SetContact(const Contact& contact)
{
	memcpy(&fContact, &contact, sizeof(Contact));
}


const char*
ContactItem::_GetTypeIcon() const
{
	switch (fContact.type) {
		case ADV_TYPE_CHAT:
			return "\xF0\x9F\x91\xA4";  // Person silhouette
		case ADV_TYPE_REPEATER:
			return "\xF0\x9F\x93\xA1";  // Antenna
		case ADV_TYPE_ROOM:
			return "\xF0\x9F\x8F\xA0";  // House
		default:
			return "\xE2\x9D\x93";      // Question mark
	}
}


void
ContactItem::_GetStatusColor(rgb_color& outColor) const
{
	if (fContact.outPathLen < 0) {
		// No path - gray
		outColor.red = 128;
		outColor.green = 128;
		outColor.blue = 128;
	} else if (fContact.outPathLen == 0) {
		// Direct path - green
		outColor.red = 0;
		outColor.green = 200;
		outColor.blue = 0;
	} else if (fContact.outPathLen <= 2) {
		// Short path - yellow
		outColor.red = 200;
		outColor.green = 200;
		outColor.blue = 0;
	} else {
		// Long path - orange
		outColor.red = 255;
		outColor.green = 128;
		outColor.blue = 0;
	}
	outColor.alpha = 255;
}


void
ContactItem::SetUnreadCount(int32 count)
{
	fUnreadCount = count;
}


void
ContactItem::SetLastMessage(const char* text, uint32 timestamp)
{
	fLastMessage = text ? text : "";
	fLastMessageTime = timestamp;
}


void
ContactItem::RefreshMessageInfo()
{
	MessageStore* store = MessageStore::Instance();

	fUnreadCount = store->GetUnreadCount(fContact.publicKey);
	fLastMessage = store->GetLastMessagePreview(fContact.publicKey);
	fLastMessageTime = store->GetLastMessageTime(fContact.publicKey);
}


void
ContactItem::_FormatRelativeTime(uint32 timestamp, char* buffer, size_t size) const
{
	time_t now = time(NULL);
	time_t msgTime = (time_t)timestamp;
	int64 diff = now - msgTime;

	if (diff < 0) {
		// Future time, just show time
		struct tm* tm = localtime(&msgTime);
		if (tm)
			strftime(buffer, size, "%H:%M", tm);
		else
			snprintf(buffer, size, "--:--");
		return;
	}

	if (diff < 60) {
		snprintf(buffer, size, "now");
	} else if (diff < 3600) {
		snprintf(buffer, size, "%dm", (int)(diff / 60));
	} else if (diff < 86400) {
		snprintf(buffer, size, "%dh", (int)(diff / 3600));
	} else if (diff < 604800) {
		snprintf(buffer, size, "%dd", (int)(diff / 86400));
	} else {
		struct tm* tm = localtime(&msgTime);
		if (tm)
			strftime(buffer, size, "%d/%m", tm);
		else
			snprintf(buffer, size, "---");
	}
}
