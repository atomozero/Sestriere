/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactItem.cpp — Telegram-style contact list item implementation
 */

#include "ContactItem.h"

#include <View.h>
#include <Font.h>
#include <Region.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>


// Avatar colors (Telegram-style palette)
static const rgb_color kAvatarColors[] = {
	{229, 115, 115, 255},  // Red
	{186, 104, 200, 255},  // Purple
	{121, 134, 203, 255},  // Indigo
	{79, 195, 247, 255},   // Light Blue
	{77, 182, 172, 255},   // Teal
	{129, 199, 132, 255},  // Green
	{255, 183, 77, 255},   // Orange
	{240, 98, 146, 255},   // Pink
};
static const int kAvatarColorCount = sizeof(kAvatarColors) / sizeof(kAvatarColors[0]);

// Layout constants
static const float kItemHeight = 52.0f;
static const float kAvatarSize = 36.0f;
static const float kAvatarMargin = 8.0f;
static const float kTextMargin = 7.0f;
static const float kBadgeSize = 16.0f;
static const float kStatusDotSize = 8.0f;
static const float kTypeBadgeWidth = 16.0f;

// Status colors (fixed — semantic, not theme-derived)
static const rgb_color kOnlineColor = {77, 182, 172, 255};   // Teal
static const rgb_color kRecentColor = {220, 180, 60, 255};   // Gold
static const rgb_color kOfflineColor = {140, 140, 140, 255}; // Gray

// Theme-aware colors
static inline rgb_color NameColor()
{
	return ui_color(B_LIST_ITEM_TEXT_COLOR);
}
static inline rgb_color MessageColor()
{
	return tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR), B_LIGHTEN_1_TINT);
}
static inline rgb_color TimeColor()
{
	return tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR), B_LIGHTEN_2_TINT);
}
static inline rgb_color SelectedBg()
{
	return ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
}
static inline rgb_color ChannelColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}
static const rgb_color kBadgeColor = {77, 182, 172, 255};  // Accent teal
static const rgb_color kBadgeTextColor = {255, 255, 255, 255};


ContactItem::ContactItem(const ContactInfo& contact)
	:
	BListItem(),
	fContact(contact),
	fLastMessage(""),
	fLastMessageTime(0),
	fUnreadCount(0),
	fIsChannel(false),
	fChannelIndex(-1),
	fBaselineOffset(0)
{
}


ContactItem::ContactItem(const char* name, bool isChannel)
	:
	BListItem(),
	fLastMessage(""),
	fLastMessageTime(0),
	fUnreadCount(0),
	fIsChannel(isChannel),
	fChannelIndex(-1),
	fBaselineOffset(0)
{
	fContact = ContactInfo();  // value-initialize
	strlcpy(fContact.name, name, sizeof(fContact.name));
	fContact.isValid = true;
}


ContactItem::~ContactItem()
{
}


void
ContactItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	owner->PushState();

	// Background
	if (IsSelected()) {
		owner->SetLowColor(SelectedBg());
	} else {
		owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
	}
	owner->FillRect(frame, B_SOLID_LOW);

	// Separator line at bottom
	owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
	owner->StrokeLine(
		BPoint(frame.left + kAvatarMargin + kAvatarSize + kTextMargin, frame.bottom),
		BPoint(frame.right, frame.bottom));

	// Avatar
	BRect avatarRect(
		frame.left + kAvatarMargin,
		frame.top + (frame.Height() - kAvatarSize) / 2,
		frame.left + kAvatarMargin + kAvatarSize,
		frame.top + (frame.Height() + kAvatarSize) / 2
	);
	_DrawAvatar(owner, avatarRect);

	// Status dot on avatar (bottom-right of avatar circle)
	if (!fIsChannel) {
		rgb_color dotColor = _StatusColor();
		owner->SetHighColor(dotColor);
		BRect dotRect(
			avatarRect.right - kStatusDotSize,
			avatarRect.bottom - kStatusDotSize,
			avatarRect.right,
			avatarRect.bottom
		);
		// White border around dot
		owner->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
		BRect dotBorder = dotRect;
		dotBorder.InsetBy(-1, -1);
		owner->FillEllipse(dotBorder);
		owner->SetHighColor(dotColor);
		owner->FillEllipse(dotRect);
	}

	// Fonts
	BFont nameFont;
	owner->GetFont(&nameFont);
	nameFont.SetFace(B_BOLD_FACE);

	BFont smallFont;
	owner->GetFont(&smallFont);
	smallFont.SetSize(smallFont.Size() * 0.88f);

	font_height nameFh, smallFh;
	nameFont.GetHeight(&nameFh);
	smallFont.GetHeight(&smallFh);

	float textLeft = avatarRect.right + kTextMargin;
	float textRight = frame.right - kTextMargin;
	float topY = frame.top + 8;
	float bottomY = frame.bottom - 7;

	// Time string (top-right)
	char timeStr[16] = "";
	if (fLastMessageTime > 0)
		_FormatTime(timeStr, sizeof(timeStr), fLastMessageTime);

	float timeWidth = 0;
	if (timeStr[0] != '\0') {
		owner->SetFont(&smallFont);
		timeWidth = owner->StringWidth(timeStr) + kTextMargin;
		owner->SetHighColor(TimeColor());
		owner->DrawString(timeStr,
			BPoint(textRight - owner->StringWidth(timeStr),
				topY + nameFh.ascent));
	}

	// Contact name (top-left, bold)
	owner->SetFont(&nameFont);
	owner->SetHighColor(fIsChannel ? ChannelColor() : NameColor());

	BString displayName = fContact.name[0] ? fContact.name : "Unknown";

	// Type badge space reservation
	float typeBadgeSpace = 0;
	if (!fIsChannel && (fContact.type == 2 || fContact.type == 3))
		typeBadgeSpace = kTypeBadgeWidth + 4;

	float maxNameWidth = textRight - textLeft - timeWidth - typeBadgeSpace;

	// Truncate name
	BString truncatedName = displayName;
	if (owner->StringWidth(truncatedName.String()) > maxNameWidth) {
		while (truncatedName.Length() > 1) {
			truncatedName.Truncate(truncatedName.Length() - 1);
			BString test = truncatedName;
			test.Append("...");
			if (owner->StringWidth(test.String()) <= maxNameWidth)
				break;
		}
		truncatedName.Append("...");
	}

	float nameEndX = textLeft + owner->StringWidth(truncatedName.String());
	owner->DrawString(truncatedName.String(), BPoint(textLeft, topY + nameFh.ascent));

	// Type badge [R] or [S] after the name
	if (!fIsChannel && (fContact.type == 2 || fContact.type == 3)) {
		owner->SetFont(&smallFont);
		const char* badge = (fContact.type == 2) ? "R" : "S";
		float badgeW = owner->StringWidth(badge) + 6;
		float badgeH = smallFh.ascent + smallFh.descent + 2;
		float badgeX = nameEndX + 4;
		float badgeY2 = topY + nameFh.ascent - smallFh.ascent;

		// Badge background
		rgb_color badgeBg = (fContact.type == 2)
			? (rgb_color){100, 160, 100, 255}   // Green for Repeater
			: (rgb_color){120, 120, 180, 255};   // Purple for Room
		owner->SetHighColor(badgeBg);
		BRect badgeRect(badgeX, badgeY2 - 1, badgeX + badgeW, badgeY2 + badgeH - 1);
		owner->FillRoundRect(badgeRect, 3, 3);

		// Badge text
		owner->SetHighColor(255, 255, 255);
		owner->DrawString(badge,
			BPoint(badgeX + 3, badgeY2 + smallFh.ascent));
	}

	// Message preview (bottom-left)
	owner->SetFont(&smallFont);
	owner->SetHighColor(MessageColor());

	float unreadSpace = (fUnreadCount > 0) ? kBadgeSize + kTextMargin : 0;
	float maxMsgWidth = textRight - textLeft - unreadSpace;

	BString msgPreview = fLastMessage;
	if (msgPreview.Length() == 0)
		msgPreview = fIsChannel ? "Public channel" : "No messages";

	if (owner->StringWidth(msgPreview.String()) > maxMsgWidth) {
		while (msgPreview.Length() > 1) {
			msgPreview.Truncate(msgPreview.Length() - 1);
			BString test = msgPreview;
			test.Append("...");
			if (owner->StringWidth(test.String()) <= maxMsgWidth)
				break;
		}
		msgPreview.Append("...");
	}

	owner->DrawString(msgPreview.String(),
		BPoint(textLeft, bottomY - smallFh.descent));

	// Unread badge (bottom-right)
	if (fUnreadCount > 0) {
		float badgeCenterY = bottomY - smallFh.descent - kBadgeSize / 2 + 2;
		BRect badgeRect(
			textRight - kBadgeSize,
			badgeCenterY - kBadgeSize / 2,
			textRight,
			badgeCenterY + kBadgeSize / 2
		);

		owner->SetHighColor(kBadgeColor);
		owner->FillEllipse(badgeRect);

		owner->SetHighColor(kBadgeTextColor);
		BFont badgeFont;
		owner->GetFont(&badgeFont);
		badgeFont.SetSize(9);
		badgeFont.SetFace(B_BOLD_FACE);
		owner->SetFont(&badgeFont);

		char badgeStr[8];
		if (fUnreadCount > 99)
			snprintf(badgeStr, sizeof(badgeStr), "99+");
		else
			snprintf(badgeStr, sizeof(badgeStr), "%u", (unsigned)fUnreadCount);

		float badgeTextW = owner->StringWidth(badgeStr);
		font_height badgeFh;
		badgeFont.GetHeight(&badgeFh);
		owner->DrawString(badgeStr,
			BPoint(badgeRect.left + (badgeRect.Width() - badgeTextW) / 2,
				badgeRect.top + (badgeRect.Height() + badgeFh.ascent) / 2 - 1));
	}

	owner->PopState();
}


void
ContactItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	font_height fh;
	font->GetHeight(&fh);
	fBaselineOffset = fh.ascent + fh.leading;

	SetHeight(kItemHeight);
}


void
ContactItem::SetContact(const ContactInfo& contact)
{
	fContact = contact;
}


void
ContactItem::SetLastMessage(const char* text, uint32 timestamp)
{
	fLastMessage = text;
	fLastMessageTime = timestamp;
}


void
ContactItem::SetUnreadCount(int32 count)
{
	fUnreadCount = count;
}


void
ContactItem::IncrementUnread()
{
	fUnreadCount++;
}


void
ContactItem::ClearUnread()
{
	fUnreadCount = 0;
}


void
ContactItem::_DrawAvatar(BView* owner, BRect rect)
{
	// Draw circular avatar with initials
	rgb_color avatarColor = fIsChannel ? ChannelColor() : _AvatarColor();
	owner->SetHighColor(avatarColor);
	owner->FillEllipse(rect);

	// Get initials (first letter, or first two for two-word names)
	BString initials;
	if (fIsChannel) {
		initials = "#";  // Channel symbol
	} else {
		const char* name = fContact.name;
		if (name[0] != '\0') {
			initials.Append(toupper(name[0]), 1);
			// Find second word
			const char* space = strchr(name, ' ');
			if (space != NULL && space[1] != '\0') {
				initials.Append(toupper(space[1]), 1);
			}
		} else {
			initials = "?";
		}
	}

	// Draw initials
	owner->SetHighColor(255, 255, 255);
	BFont font;
	owner->GetFont(&font);
	font.SetSize(kAvatarSize * 0.45f);
	font.SetFace(B_BOLD_FACE);
	owner->SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);
	float textWidth = owner->StringWidth(initials.String());

	owner->DrawString(initials.String(),
		BPoint(rect.left + (rect.Width() - textWidth) / 2,
			rect.top + (rect.Height() + fh.ascent - fh.descent) / 2));
}


rgb_color
ContactItem::_AvatarColor() const
{
	// Generate consistent color based on name hash
	uint32 hash = 0;
	const char* name = fContact.name;
	while (*name) {
		hash = hash * 31 + (uint8)*name++;
	}
	return kAvatarColors[hash % kAvatarColorCount];
}


rgb_color
ContactItem::_StatusColor() const
{
	if (!fContact.isValid || fContact.lastSeen == 0)
		return kOfflineColor;

	uint32 now = (uint32)time(NULL);
	uint32 age = (now > fContact.lastSeen) ? (now - fContact.lastSeen) : 0;

	if (age < 300)        // < 5 minutes
		return kOnlineColor;
	else if (age < 3600)  // < 1 hour
		return kRecentColor;
	else
		return kOfflineColor;
}


void
ContactItem::_FormatTime(char* buffer, size_t size, uint32 timestamp) const
{
	time_t now = time(NULL);
	time_t msgTime = (time_t)timestamp;

	struct tm nowTm;
	struct tm msgTm;
	if (localtime_r(&now, &nowTm) == NULL
		|| localtime_r(&msgTime, &msgTm) == NULL) {
		strlcpy(buffer, "?", size);
		return;
	}

	if (nowTm.tm_year == msgTm.tm_year && nowTm.tm_yday == msgTm.tm_yday) {
		// Today: show time
		strftime(buffer, size, "%H:%M", &msgTm);
	} else if (nowTm.tm_year == msgTm.tm_year
		&& nowTm.tm_yday - msgTm.tm_yday == 1) {
		// Yesterday
		snprintf(buffer, size, "Ieri");
	} else if (nowTm.tm_year == msgTm.tm_year
		&& nowTm.tm_yday - msgTm.tm_yday < 7) {
		// This week: show day name
		strftime(buffer, size, "%a", &msgTm);
	} else {
		// Older: show date
		strftime(buffer, size, "%d/%m", &msgTm);
	}
}
