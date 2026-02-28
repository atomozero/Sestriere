/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageView.cpp — Individual chat message bubble display implementation
 */

#include "MessageView.h"

#include <View.h>
#include <Font.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "Constants.h"


// Theme-aware colors derived from Haiku system colors
static inline rgb_color OutgoingBubbleColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
}
static inline rgb_color OutgoingBorderColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT);
}
static inline rgb_color IncomingBubbleColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_LIGHTEN_1_TINT);
}
static inline rgb_color IncomingBorderColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
}
static inline rgb_color BubbleTextColor()
{
	return ui_color(B_DOCUMENT_TEXT_COLOR);
}
static inline rgb_color MetaTextColor()
{
	return tint_color(ui_color(B_DOCUMENT_TEXT_COLOR), B_LIGHTEN_1_TINT);
}
static inline rgb_color SenderNameColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}

// Layout constants - Haiku style (smaller radius, subtle)
static const float kBubblePadding = 8.0f;
static const float kBubbleRadius = 4.0f;       // Haiku uses smaller radii
static const float kBubbleMaxWidthRatio = 0.75f;
static const float kBubbleMargin = 6.0f;
static const float kBorderWidth = 1.0f;


MessageView::MessageView(const ChatMessage& message, const char* senderName)
	:
	BListItem(),
	fText(message.text),
	fSenderName(senderName != NULL ? senderName : ""),
	fTimestamp(message.timestamp),
	fOutgoing(message.isOutgoing),
	fIsChannel(message.isChannel),
	fPathLen(message.pathLen),
	fSnr(message.snr),
	fDeliveryStatus(message.deliveryStatus),
	fRoundTripMs(message.roundTripMs),
	fTxtType(message.txtType),
	fHopsClickRect(),
	fBaselineOffset(0)
{
	memcpy(fPubKeyPrefix, message.pubKeyPrefix, sizeof(fPubKeyPrefix));
}


MessageView::~MessageView()
{
}


void
MessageView::DrawItem(BView* owner, BRect frame, bool complete)
{
	(void)complete;
	owner->PushState();

	// Clear background (light gray to match chat view)
	rgb_color chatBg = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
	owner->SetLowColor(chatBg);
	owner->FillRect(frame, B_SOLID_LOW);

	bool isCli = (fTxtType == 1);

	// Use monospace font for CLI messages
	if (isCli)
		owner->SetFont(be_fixed_font);

	font_height fh;
	owner->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	// Calculate bubble dimensions — use owner bounds (same as Update)
	float maxBubbleWidth = owner->Bounds().Width() * kBubbleMaxWidthRatio;

	// For CLI messages, prepend "> " to outgoing commands
	BString displayText = fText;
	if (isCli && fOutgoing)
		displayText.Prepend("> ");

	// Wrap text if needed
	std::vector<BString> lines;
	_WrapText(owner, isCli ? displayText : fText,
		maxBubbleWidth - kBubblePadding * 2, lines);

	// Calculate actual bubble width based on longest line
	float maxLineWidth = 0;
	for (const auto& line : lines) {
		float w = owner->StringWidth(line.String());
		if (w > maxLineWidth)
			maxLineWidth = w;
	}

	// Include sender name in width calculation for incoming messages
	// Must use bold font since the name is drawn bold
	float headerWidth = 0;
	if (!fOutgoing && fSenderName.Length() > 0) {
		BFont boldFont;
		owner->GetFont(&boldFont);
		boldFont.SetFace(B_BOLD_FACE);
		headerWidth = boldFont.StringWidth(fSenderName.String());
	}

	// Calculate meta text — build the actual displayed string for sizing
	char timeStr[16];
	_FormatTimestamp(timeStr, sizeof(timeStr));

	BString displayMeta;
	if (!fOutgoing) {
		// Incoming: time + hops (no checkmarks)
		if (fPathLen == 0 || fPathLen == kPathLenDirect)
			displayMeta.SetToFormat("%s", timeStr);
		else
			displayMeta.SetToFormat("%s \xC2\xB7 %d hops", timeStr, fPathLen);
	} else {
		// Outgoing: time + delivery status indicator
		switch (fDeliveryStatus) {
			case DELIVERY_PENDING:
				displayMeta.SetToFormat("%s \xE2\x8F\xB3", timeStr);  // ⏳
				break;
			case DELIVERY_CONFIRMED:
				if (fRoundTripMs > 0)
					displayMeta.SetToFormat("%s \xE2\x9C\x93\xE2\x9C\x93 %lums",
						timeStr, (unsigned long)fRoundTripMs);
				else
					displayMeta.SetToFormat("%s \xE2\x9C\x93\xE2\x9C\x93", timeStr);
				break;
			default:  // DELIVERY_SENT
				displayMeta.SetToFormat("%s \xE2\x9C\x93", timeStr);  // ✓
				break;
		}
	}

	BString snrStr;
	float snrWidth = 0;
	bool showSnr = (!fOutgoing && fSnr != 0);
	if (showSnr) {
		snrStr.SetToFormat(" SNR %d", fSnr);
	}

	// Measure total meta width with actual smaller font
	BFont metaFont;
	owner->GetFont(&metaFont);
	metaFont.SetSize(metaFont.Size() * 0.85f);
	float metaWidth = metaFont.StringWidth(displayMeta.String());
	if (showSnr)
		metaWidth += metaFont.StringWidth(snrStr.String()) + 4;

	// Bubble must fit text, header, AND meta
	float bubbleContentWidth = std::max({maxLineWidth, headerWidth, metaWidth});
	float bubbleWidth = bubbleContentWidth + kBubblePadding * 2;

	// For wrapped messages, use full max width to avoid word-boundary gaps
	if (lines.size() > 1)
		bubbleWidth = maxBubbleWidth;

	// Calculate bubble height
	float textHeight = lines.size() * lineHeight;
	float headerHeight = (!fOutgoing && fSenderName.Length() > 0) ? lineHeight : 0;
	float metaHeight = lineHeight * 0.8f;  // Smaller font for time/info
	float bubbleHeight = headerHeight + textHeight + metaHeight + kBubblePadding * 2;

	// Position bubble
	BRect bubbleRect;
	if (fOutgoing) {
		// Right-align outgoing messages
		bubbleRect.left = frame.right - bubbleWidth - kBubbleMargin;
		bubbleRect.right = frame.right - kBubbleMargin;
	} else {
		// Left-align incoming messages
		bubbleRect.left = frame.left + kBubbleMargin;
		bubbleRect.right = frame.left + kBubbleMargin + bubbleWidth;
	}
	bubbleRect.top = frame.top + kBubbleMargin / 2;
	bubbleRect.bottom = bubbleRect.top + bubbleHeight;

	// Draw bubble background with Haiku-style subtle shadow
	rgb_color bubbleColor;
	rgb_color borderColor;
	if (isCli) {
		// Terminal-style: very dark background for CLI messages
		bubbleColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_MAX_TINT);
		borderColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_4_TINT);
	} else {
		bubbleColor = fOutgoing ? OutgoingBubbleColor() : IncomingBubbleColor();
		borderColor = fOutgoing ? OutgoingBorderColor() : IncomingBorderColor();
	}

	// Subtle shadow (offset by 1 pixel)
	owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT));
	BRect shadowRect = bubbleRect;
	shadowRect.OffsetBy(1, 1);
	owner->FillRoundRect(shadowRect, kBubbleRadius, kBubbleRadius);

	// Fill bubble
	owner->SetHighColor(bubbleColor);
	owner->FillRoundRect(bubbleRect, kBubbleRadius, kBubbleRadius);

	// Draw border (Haiku-style)
	owner->SetHighColor(borderColor);
	owner->StrokeRoundRect(bubbleRect, kBubbleRadius, kBubbleRadius);

	// Draw content
	float yPos = bubbleRect.top + kBubblePadding + fh.ascent;

	// Draw sender name for incoming messages
	if (!fOutgoing && fSenderName.Length() > 0) {
		owner->SetHighColor(SenderNameColor());
		BFont boldFont;
		owner->GetFont(&boldFont);
		boldFont.SetFace(B_BOLD_FACE);
		owner->SetFont(&boldFont);

		owner->DrawString(fSenderName.String(),
			BPoint(bubbleRect.left + kBubblePadding, yPos));

		boldFont.SetFace(B_REGULAR_FACE);
		owner->SetFont(&boldFont);
		yPos += lineHeight;
	}

	// Draw message text
	if (isCli && fOutgoing) {
		// Bright amber prompt for outgoing CLI commands
		owner->SetHighColor((rgb_color){255, 210, 80, 255});
	} else if (isCli) {
		// Bright green text for incoming CLI responses (terminal-style)
		owner->SetHighColor((rgb_color){130, 255, 200, 255});
	} else {
		owner->SetHighColor(BubbleTextColor());
	}

	for (const auto& line : lines) {
		owner->DrawString(line.String(),
			BPoint(bubbleRect.left + kBubblePadding, yPos));
		yPos += lineHeight;
	}

	// Draw timestamp and delivery info
	owner->SetFont(&metaFont);

	// Draw base meta text
	owner->SetHighColor(MetaTextColor());

	float metaWidthActual = metaFont.StringWidth(displayMeta.String());
	snrWidth = showSnr ? (metaFont.StringWidth(snrStr.String()) + 4) : 0;

	float totalMetaWidth = metaWidthActual + snrWidth;
	float metaX = bubbleRect.right - kBubblePadding - totalMetaWidth;
	float metaY = bubbleRect.bottom - kBubblePadding / 2;

	// For incoming multi-hop: split rendering to make "X hops" clickable
	bool clickableHops = HasClickableHops();
	if (clickableHops) {
		// Draw time + separator in normal meta color
		BString timePart;
		timePart.SetToFormat("%s \xC2\xB7 ", timeStr);
		owner->SetHighColor(MetaTextColor());
		owner->DrawString(timePart.String(), BPoint(metaX, metaY));

		float timePartWidth = metaFont.StringWidth(timePart.String());

		// Draw "X hops" in link color with underline
		BString hopsPart;
		hopsPart.SetToFormat("%d hops", fPathLen);
		float hopsWidth = metaFont.StringWidth(hopsPart.String());

		float hopsX = metaX + timePartWidth;
		owner->SetHighColor(ui_color(B_CONTROL_HIGHLIGHT_COLOR));
		owner->DrawString(hopsPart.String(), BPoint(hopsX, metaY));

		// Draw underline
		font_height mfh;
		metaFont.GetHeight(&mfh);
		float underlineY = metaY + mfh.descent * 0.5f;
		owner->StrokeLine(BPoint(hopsX, underlineY),
			BPoint(hopsX + hopsWidth, underlineY));

		// Save click rect in list view coordinates (frame-relative)
		fHopsClickRect.Set(hopsX, metaY - mfh.ascent,
			hopsX + hopsWidth, metaY + mfh.descent);
	} else {
		fHopsClickRect = BRect();
		owner->DrawString(displayMeta.String(), BPoint(metaX, metaY));
	}

	// Draw SNR value with color coding
	if (showSnr) {
		rgb_color snrColor;
		if (fSnr > 0)
			snrColor = kColorGood;
		else if (fSnr >= -10)
			snrColor = kColorFair;
		else
			snrColor = kColorBad;

		owner->SetHighColor(snrColor);
		owner->DrawString(snrStr.String(),
			BPoint(metaX + metaWidthActual, metaY));
	}

	owner->PopState();
}


void
MessageView::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	bool isCli = (fTxtType == 1);

	// Use monospace font metrics for CLI messages
	const BFont* measureFont = isCli ? be_fixed_font : font;

	font_height fontHeight;
	measureFont->GetHeight(&fontHeight);

	fBaselineOffset = fontHeight.ascent + fontHeight.leading + 2;

	float lineHeight = fontHeight.ascent + fontHeight.descent +
		fontHeight.leading;

	// Calculate max bubble width
	BRect ownerBounds = owner->Bounds();
	float maxBubbleWidth = ownerBounds.Width() * kBubbleMaxWidthRatio;

	// For CLI messages, prepend "> " to outgoing commands
	BString displayText = fText;
	if (isCli && fOutgoing)
		displayText.Prepend("> ");

	// Wrap text to calculate actual height
	// NOTE: Do NOT call owner->SetFont() here — it triggers
	// BListView::_UpdateItems() → Update() infinite recursion.
	// For CLI, estimate wrapping using monospace font directly.
	std::vector<BString> lines;
	if (isCli) {
		// Estimate line count using monospace font width
		float availWidth = maxBubbleWidth - kBubblePadding * 2;
		float textWidth = be_fixed_font->StringWidth(
			displayText.String());
		int32 lineCount = (textWidth > availWidth)
			? (int32)(textWidth / availWidth) + 1 : 1;
		for (int32 i = 0; i < lineCount; i++)
			lines.push_back("");
		if (lines.empty())
			lines.push_back("");
	} else {
		_WrapText(owner, fText,
			maxBubbleWidth - kBubblePadding * 2, lines);
	}

	// Calculate total height
	float textHeight = lines.size() * lineHeight;
	float headerHeight = (!fOutgoing && fSenderName.Length() > 0) ? lineHeight : 0;
	float metaHeight = lineHeight * 0.8f;

	float height = headerHeight + textHeight + metaHeight +
		kBubblePadding * 2 + kBubbleMargin;

	SetHeight(height);
}


void
MessageView::SetDeliveryStatus(uint8 status, uint32 rtt)
{
	fDeliveryStatus = status;
	fRoundTripMs = rtt;
}


void
MessageView::_FormatTimestamp(char* buffer, size_t size) const
{
	time_t timestamp = (time_t)fTimestamp;
	struct tm tm;

	if (localtime_r(&timestamp, &tm) != NULL)
		strftime(buffer, size, "%H:%M", &tm);
	else
		snprintf(buffer, size, "--:--");
}


void
MessageView::_WrapText(BView* owner, const BString& text, float maxWidth,
	std::vector<BString>& outLines) const
{
	outLines.clear();

	if (text.Length() == 0) {
		outLines.push_back("");
		return;
	}

	// Check if text fits on one line
	if (owner->StringWidth(text.String()) <= maxWidth) {
		outLines.push_back(text);
		return;
	}

	// Need to wrap - split by words
	BString remaining = text;
	BString currentLine;

	while (remaining.Length() > 0) {
		// Find next word
		int32 spacePos = remaining.FindFirst(' ');
		BString word;

		if (spacePos < 0) {
			word = remaining;
			remaining = "";
		} else {
			remaining.CopyInto(word, 0, spacePos);
			remaining.Remove(0, spacePos + 1);
		}

		// Try adding word to current line
		BString testLine = currentLine;
		if (testLine.Length() > 0)
			testLine.Append(" ");
		testLine.Append(word);

		if (owner->StringWidth(testLine.String()) <= maxWidth) {
			currentLine = testLine;
		} else {
			// Word doesn't fit - start new line
			if (currentLine.Length() > 0) {
				outLines.push_back(currentLine);
				currentLine = word;
			} else {
				// Word alone is too long - need to break it
				BString partial;
				for (int32 i = 0; i < word.Length(); i++) {
					char c = word.ByteAt(i);
					BString test = partial;
					test.Append(c, 1);
					if (owner->StringWidth(test.String()) > maxWidth) {
						if (partial.Length() > 0) {
							outLines.push_back(partial);
							partial = "";
							partial.Append(c, 1);
						}
					} else {
						partial.Append(c, 1);
					}
				}
				currentLine = partial;
			}
		}
	}

	// Don't forget the last line
	if (currentLine.Length() > 0)
		outLines.push_back(currentLine);

	// Ensure at least one line
	if (outLines.empty())
		outLines.push_back("");
}
