/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageView.cpp — Individual message display item implementation
 */

#include "MessageView.h"

#include <View.h>

#include <cstdio>
#include <ctime>


MessageView::MessageView(const ReceivedMessage& message, bool outgoing,
	const char* senderName)
	:
	BListItem(),
	fText(message.text),
	fSenderName(senderName != NULL ? senderName : ""),
	fTimestamp(message.senderTimestamp),
	fOutgoing(outgoing),
	fPathLen(message.pathLen),
	fSnr(message.snr),
	fBaselineOffset(0),
	fTextHeight(0)
{
}


MessageView::~MessageView()
{
}


void
MessageView::DrawItem(BView* owner, BRect frame, bool complete)
{
	(void)complete;
	rgb_color lowColor = owner->LowColor();
	rgb_color highColor = owner->HighColor();

	// Background
	rgb_color bgColor;
	if (fOutgoing) {
		// Outgoing messages - light blue background
		bgColor.red = 220;
		bgColor.green = 235;
		bgColor.blue = 250;
		bgColor.alpha = 255;
	} else {
		// Incoming messages - light gray background
		bgColor.red = 240;
		bgColor.green = 240;
		bgColor.blue = 240;
		bgColor.alpha = 255;
	}

	owner->SetLowColor(bgColor);
	owner->SetHighColor(0, 0, 0);  // Black text

	// Draw message bubble
	BRect bubbleRect = frame;
	bubbleRect.InsetBy(4, 2);

	if (fOutgoing) {
		// Right-align outgoing messages
		bubbleRect.left = bubbleRect.right - (bubbleRect.Width() * 0.75);
	} else {
		// Left-align incoming messages
		bubbleRect.right = bubbleRect.left + (bubbleRect.Width() * 0.75);
	}

	owner->FillRoundRect(bubbleRect, 4, 4, B_SOLID_LOW);

	// Draw border
	owner->SetHighColor(tint_color(bgColor, B_DARKEN_2_TINT));
	owner->StrokeRoundRect(bubbleRect, 4, 4);

	// Draw timestamp and sender
	owner->SetHighColor(100, 100, 100);  // Gray for metadata

	char timeStr[16];
	_FormatTimestamp(timeStr, sizeof(timeStr));

	BPoint metaPos(bubbleRect.left + 6, bubbleRect.top + fBaselineOffset);

	if (!fOutgoing && fSenderName.Length() > 0) {
		BString meta;
		meta.SetToFormat("[%s] %s:", timeStr, fSenderName.String());
		owner->DrawString(meta.String(), metaPos);
	} else {
		BString meta;
		meta.SetToFormat("[%s]", timeStr);
		owner->DrawString(meta.String(), metaPos);
	}

	// Draw message text
	owner->SetHighColor(0, 0, 0);  // Black text

	font_height fh;
	owner->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	BPoint textPos(bubbleRect.left + 6, metaPos.y + lineHeight);
	owner->DrawString(fText.String(), textPos);

	// Draw path/SNR info for incoming messages
	if (!fOutgoing) {
		owner->SetHighColor(128, 128, 128);

		BString info;
		if (fPathLen == 0xFF) {
			info = "direct";
		} else {
			info.SetToFormat("%d hops", fPathLen);
		}

		if (fSnr > 0) {
			float snrDb = fSnr / 4.0f;
			BString snrStr;
			snrStr.SetToFormat(" SNR:%.1fdB", snrDb);
			info.Append(snrStr);
		}

		float infoWidth = owner->StringWidth(info.String());
		BPoint infoPos(bubbleRect.right - infoWidth - 6,
			bubbleRect.bottom - 4);
		owner->DrawString(info.String(), infoPos);
	}

	// Restore colors
	owner->SetLowColor(lowColor);
	owner->SetHighColor(highColor);
}


void
MessageView::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	font_height fontHeight;
	font->GetHeight(&fontHeight);

	fBaselineOffset = fontHeight.ascent + fontHeight.leading + 2;

	float lineHeight = fontHeight.ascent + fontHeight.descent +
		fontHeight.leading;

	// Calculate height based on text lines
	// For now, assume single line + metadata line + padding
	float height = lineHeight * 2 + 16;

	SetHeight(height);
}


void
MessageView::_FormatTimestamp(char* buffer, size_t size) const
{
	time_t timestamp = (time_t)fTimestamp;
	struct tm* tm = localtime(&timestamp);

	if (tm != NULL)
		strftime(buffer, size, "%H:%M", tm);
	else
		snprintf(buffer, size, "--:--");
}


float
MessageView::_CalcTextHeight(BView* owner, float maxWidth) const
{
	// TODO: Calculate wrapped text height
	font_height fh;
	owner->GetFontHeight(&fh);
	return fh.ascent + fh.descent + fh.leading;
}
