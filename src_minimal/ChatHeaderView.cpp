/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatHeaderView.cpp — Chat area header showing contact info implementation
 */

#include "ChatHeaderView.h"

#include <Font.h>
#include <LayoutUtils.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>


// Avatar colors (same as ContactItem)
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
static const float kHeaderHeight = 56.0f;
static const float kAvatarSize = 40.0f;
static const float kMargin = 12.0f;

// Theme-aware colors
static inline rgb_color HeaderBg()
{
	return ui_color(B_PANEL_BACKGROUND_COLOR);
}
static inline rgb_color NameColor()
{
	return ui_color(B_PANEL_TEXT_COLOR);
}
static inline rgb_color StatusColor()
{
	return tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT);
}
static inline rgb_color ChannelColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}
static inline rgb_color BorderColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT);
}
static const rgb_color kOnlineColor = {77, 182, 172, 255};


ChatHeaderView::ChatHeaderView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fContact(NULL),
	fDisplayName("Select a contact"),
	fStatus(""),
	fIsChannel(false),
	fPathLen(-1),
	fSnr(0)
{
	SetViewColor(HeaderBg());
	SetExplicitMinSize(BSize(B_SIZE_UNSET, kHeaderHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kHeaderHeight));
}


ChatHeaderView::~ChatHeaderView()
{
}


void
ChatHeaderView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background
	SetHighColor(HeaderBg());
	FillRect(bounds);

	// Bottom border
	SetHighColor(BorderColor());
	StrokeLine(BPoint(bounds.left, bounds.bottom),
		BPoint(bounds.right, bounds.bottom));

	if (fContact == NULL && !fIsChannel) {
		// No contact selected - show placeholder
		SetHighColor(StatusColor());
		BFont font;
		GetFont(&font);
		font.SetSize(14);
		SetFont(&font);

		font_height fh;
		font.GetHeight(&fh);

		DrawString(fDisplayName.String(),
			BPoint(kMargin, bounds.top + (bounds.Height() + fh.ascent - fh.descent) / 2));
		return;
	}

	// Avatar
	BRect avatarRect(
		kMargin,
		(bounds.Height() - kAvatarSize) / 2,
		kMargin + kAvatarSize,
		(bounds.Height() + kAvatarSize) / 2
	);
	_DrawAvatar(avatarRect);

	// Name
	float textLeft = avatarRect.right + kMargin;

	BFont nameFont;
	GetFont(&nameFont);
	nameFont.SetSize(15);
	nameFont.SetFace(B_BOLD_FACE);
	SetFont(&nameFont);

	font_height nameFh;
	nameFont.GetHeight(&nameFh);

	SetHighColor(fIsChannel ? ChannelColor() : NameColor());
	DrawString(fDisplayName.String(),
		BPoint(textLeft, bounds.top + kMargin + nameFh.ascent));

	// Status line
	BFont statusFont;
	GetFont(&statusFont);
	statusFont.SetSize(12);
	statusFont.SetFace(B_REGULAR_FACE);
	SetFont(&statusFont);

	font_height statusFh;
	statusFont.GetHeight(&statusFh);

	BString statusText;
	if (fIsChannel) {
		statusText = "Public channel";
	} else if (fContact != NULL) {
		// Type
		switch (fContact->type) {
			case 1: statusText = "Chat"; break;
			case 2: statusText = "Repeater"; break;
			case 3: statusText = "Room"; break;
			default: statusText = "Node"; break;
		}

		// Path
		if (fPathLen >= 0) {
			if (fPathLen == 0 || fPathLen == kPathLenDirect)
				statusText << " · Direct";
			else
				statusText << BString().SetToFormat(" · %d hop%s",
					fPathLen, fPathLen > 1 ? "s" : "");
		}

		// SNR
		if (fSnr != 0)
			statusText << BString().SetToFormat(" · SNR %d", fSnr);

		// Last seen
		if (fContact->lastSeen > 0) {
			uint32 now = (uint32)time(NULL);
			uint32 age = (now > fContact->lastSeen)
				? (now - fContact->lastSeen) : 0;
			if (age < 60)
				statusText << " · Just now";
			else if (age < 3600)
				statusText << BString().SetToFormat(" · %u min ago", age / 60);
			else if (age < 86400)
				statusText << BString().SetToFormat(" · %u hr ago", age / 3600);
		}
	} else if (fStatus.Length() > 0) {
		statusText = fStatus;
	}

	if (statusText.Length() > 0) {
		float statusY = bounds.bottom - kMargin - statusFh.descent;
		float textMidY = statusY - statusFh.ascent / 2;

		// Green dot for "online" contacts — centered on text
		if (!fIsChannel && fContact != NULL) {
			SetHighColor(kOnlineColor);
			FillEllipse(BRect(textLeft, textMidY - 4,
				textLeft + 8, textMidY + 4));
			textLeft += 12;
		}

		SetHighColor(StatusColor());
		DrawString(statusText.String(), BPoint(textLeft, statusY));
	}
}


void
ChatHeaderView::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (Parent() != NULL)
		SetViewColor(Parent()->ViewColor());
	SetViewColor(HeaderBg());
}


BSize
ChatHeaderView::MinSize()
{
	return BSize(200, kHeaderHeight);
}


BSize
ChatHeaderView::PreferredSize()
{
	return BSize(B_SIZE_UNLIMITED, kHeaderHeight);
}


void
ChatHeaderView::SetContact(const ContactInfo* contact)
{
	fContact = contact;
	fIsChannel = false;

	if (contact != NULL) {
		fDisplayName = contact->name[0] ? contact->name : "Unknown";
		fPathLen = contact->outPathLen;
	} else {
		fDisplayName = "Select a contact";
		fPathLen = -1;
	}

	Invalidate();
}


void
ChatHeaderView::SetChannel(bool isChannel)
{
	fIsChannel = isChannel;
	fContact = NULL;

	if (isChannel) {
		fDisplayName = "Public Channel";
	} else {
		fDisplayName = "Select a contact";
	}

	Invalidate();
}


void
ChatHeaderView::SetChannelName(const char* name)
{
	fIsChannel = true;
	fContact = NULL;
	fDisplayName = name;
	Invalidate();
}


void
ChatHeaderView::SetStatus(const char* status)
{
	fStatus = status;
	Invalidate();
}


void
ChatHeaderView::SetConnectionInfo(int8 pathLen, int8 snr)
{
	fPathLen = pathLen;
	fSnr = snr;
	Invalidate();
}


void
ChatHeaderView::_DrawAvatar(BRect rect)
{
	// Draw circular avatar with initials
	rgb_color avatarColor = fIsChannel ? ChannelColor() : _AvatarColor();
	SetHighColor(avatarColor);
	FillEllipse(rect);

	// Get initials
	BString initials;
	if (fIsChannel) {
		initials = "#";
	} else if (fContact != NULL) {
		const char* name = fContact->name;
		if (name[0] != '\0') {
			initials.Append(toupper(name[0]), 1);
			const char* space = strchr(name, ' ');
			if (space != NULL && space[1] != '\0') {
				initials.Append(toupper(space[1]), 1);
			}
		} else {
			initials = "?";
		}
	} else {
		initials = "?";
	}

	// Draw initials
	SetHighColor(255, 255, 255);
	BFont font;
	GetFont(&font);
	font.SetSize(kAvatarSize * 0.45f);
	font.SetFace(B_BOLD_FACE);
	SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);
	float textWidth = StringWidth(initials.String());

	DrawString(initials.String(),
		BPoint(rect.left + (rect.Width() - textWidth) / 2,
			rect.top + (rect.Height() + fh.ascent - fh.descent) / 2));
}


rgb_color
ChatHeaderView::_AvatarColor() const
{
	if (fContact == NULL)
		return kAvatarColors[0];

	// Generate consistent color based on name hash
	uint32 hash = 0;
	const char* name = fContact->name;
	while (*name) {
		hash = hash * 31 + (uint8)*name++;
	}
	return kAvatarColors[hash % kAvatarColorCount];
}
