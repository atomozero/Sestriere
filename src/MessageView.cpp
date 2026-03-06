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
#include <cmath>
#include <cstdio>
#include <ctime>

#include "Constants.h"
#include "EmojiRenderer.h"
#include "GiphyClient.h"
#include "ImageSession.h"
#include "SarMarker.h"


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
static const float kSarBorderWidth = 3.0f;


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
	fRetryCount(message.retryCount),
	fHopsClickRect(),
	fIsSarMarker(false),
	fSarMarker(),
	fIsImageMsg(false),
	fImageSessionId(0),
	fImageWidth(0),
	fImageHeight(0),
	fImageTotalFragments(0),
	fImageReceivedFragments(0),
	fImageState(IMAGE_PENDING),
	fImageBitmap(NULL),
	fIsGifMsg(false),
	fGifFrames(NULL),
	fGifDurations(NULL),
	fGifFrameCount(0),
	fGifCurrentFrame(0),
	fGifLastAdvance(0),
	fGifLoadState(0),
	fBaselineOffset(0)
{
	memcpy(fPubKeyPrefix, message.pubKeyPrefix, sizeof(fPubKeyPrefix));

	// Check for GIF message (g:ID format)
	if (GiphyClient::IsGifMessage(fText.String())) {
		fIsGifMsg = true;
		GiphyClient::ExtractGifId(fText.String(), fGifId, sizeof(fGifId));
	}

	// Check for IE2 image envelope before SAR marker
	if (!fIsGifMsg && ImageSessionManager::IsImageEnvelope(fText.String())) {
		fIsImageMsg = true;
		uint32 sid = 0;
		uint8 fmt = 0, total = 0;
		int32 w = 0, h = 0;
		uint32 bytes = 0, ts = 0;
		uint8 senderKey[6];
		if (ImageSessionManager::ParseEnvelope(fText.String(), &sid, &fmt,
			&total, &w, &h, &bytes, senderKey, &ts)) {
			fImageSessionId = sid;
			fImageWidth = w;
			fImageHeight = h;
			fImageTotalFragments = total;
		}
	} else {
		fIsSarMarker = ParseSarMarker(fText.String(), fSarMarker);
	}
}


MessageView::~MessageView()
{
	delete fImageBitmap;

	if (fGifFrames != NULL) {
		for (int32 i = 0; i < fGifFrameCount; i++)
			delete fGifFrames[i];
		delete[] fGifFrames;
		delete[] fGifDurations;
	}
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

	// GIF message — separate rendering path
	if (fIsGifMsg) {
		_DrawGifBubble(owner, frame);
		owner->PopState();
		return;
	}

	// Image message — separate rendering path
	if (fIsImageMsg) {
		_DrawImageBubble(owner, frame);
		owner->PopState();
		return;
	}

	bool isCli = (fTxtType == 1);
	bool isSar = fIsSarMarker;

	// Set font explicitly to ensure consistent measurement and drawing.
	// CLI uses monospace; regular messages use the system plain font.
	if (isCli)
		owner->SetFont(be_fixed_font);
	else
		owner->SetFont(be_plain_font);

	font_height fh;
	owner->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	// Calculate bubble dimensions
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
	// Use ceilf() to avoid subpixel rounding causing text overflow
	float maxLineWidth = 0;
	for (const auto& line : lines) {
		float w = ceilf(owner->StringWidth(line.String()));
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
		headerWidth = ceilf(boldFont.StringWidth(fSenderName.String()));
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
			case DELIVERY_FAILED:
				displayMeta.SetToFormat("%s \xE2\x9C\x97", timeStr);  // ✗
				break;
			case DELIVERY_RETRYING:
				displayMeta.SetToFormat("%s \xE2\x86\xBB %d/3",       // ↻ N/3
					timeStr, (int)fRetryCount);
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
	rgb_color sarColor = {0, 0, 0, 255};
	if (isSar) {
		sarColor = SarMarkerColor(fSarMarker.colorIndex);
		// Mix 15% SAR color + 85% panel background
		rgb_color panelBg = ui_color(B_PANEL_BACKGROUND_COLOR);
		bubbleColor.red = (uint8)(panelBg.red * 0.85f + sarColor.red * 0.15f);
		bubbleColor.green = (uint8)(panelBg.green * 0.85f + sarColor.green * 0.15f);
		bubbleColor.blue = (uint8)(panelBg.blue * 0.85f + sarColor.blue * 0.15f);
		bubbleColor.alpha = 255;
		borderColor = tint_color(sarColor, B_DARKEN_2_TINT);
	} else if (isCli) {
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

	// SAR marker: colored left border stripe
	if (isSar) {
		owner->SetHighColor(sarColor);
		BRect stripe(bubbleRect.left, bubbleRect.top + kBubbleRadius,
			bubbleRect.left + kSarBorderWidth, bubbleRect.bottom - kBubbleRadius);
		owner->FillRect(stripe);
	}

	// Draw content
	float yPos = bubbleRect.top + kBubblePadding + fh.ascent;

	// Draw sender name for incoming messages
	if (!fOutgoing && fSenderName.Length() > 0) {
		BFont savedFont;
		owner->GetFont(&savedFont);

		BFont boldFont(savedFont);
		boldFont.SetFace(B_BOLD_FACE);
		owner->SetFont(&boldFont);
		owner->SetHighColor(SenderNameColor());

		owner->DrawString(fSenderName.String(),
			BPoint(bubbleRect.left + kBubblePadding, yPos));

		owner->SetFont(&savedFont);
		yPos += lineHeight;
	}

	// Draw message text
	float contentLeft = bubbleRect.left + kBubblePadding
		+ (isSar ? kSarBorderWidth : 0);

	if (isSar) {
		// SAR marker: emoji + type name in bold
		BFont savedFont;
		owner->GetFont(&savedFont);
		BFont boldFont(savedFont);
		boldFont.SetFace(B_BOLD_FACE);
		owner->SetFont(&boldFont);
		owner->SetHighColor(BubbleTextColor());

		BString titleLine;
		titleLine.SetToFormat("%s %s", fSarMarker.emoji,
			SarMarkerTypeName(fSarMarker.type));
		owner->DrawString(titleLine.String(), BPoint(contentLeft, yPos));
		yPos += lineHeight;

		// Coordinates in monospace
		owner->SetFont(be_fixed_font);
		owner->SetHighColor(MetaTextColor());
		char coordBuf[64];
		snprintf(coordBuf, sizeof(coordBuf), "%.4f%c, %.4f%c",
			fSarMarker.lat < 0 ? -fSarMarker.lat : fSarMarker.lat,
			fSarMarker.lat >= 0 ? 'N' : 'S',
			fSarMarker.lon < 0 ? -fSarMarker.lon : fSarMarker.lon,
			fSarMarker.lon >= 0 ? 'E' : 'W');
		owner->DrawString(coordBuf, BPoint(contentLeft, yPos));
		yPos += lineHeight;

		// Note (if present)
		if (fSarMarker.notes[0] != '\0') {
			owner->SetFont(&savedFont);
			owner->SetHighColor(BubbleTextColor());
			owner->DrawString(fSarMarker.notes, BPoint(contentLeft, yPos));
			yPos += lineHeight;
		}

		owner->SetFont(&savedFont);
	} else if (isCli && fOutgoing) {
		// Bright amber prompt for outgoing CLI commands
		owner->SetHighColor((rgb_color){255, 210, 80, 255});
		for (const auto& line : lines) {
			owner->DrawString(line.String(),
				BPoint(contentLeft, yPos));
			yPos += lineHeight;
		}
	} else if (isCli) {
		// Bright green text for incoming CLI responses (terminal-style)
		owner->SetHighColor((rgb_color){130, 255, 200, 255});
		for (const auto& line : lines) {
			owner->DrawString(line.String(),
				BPoint(contentLeft, yPos));
			yPos += lineHeight;
		}
	} else {
		owner->SetHighColor(BubbleTextColor());
		float emojiSize = fh.ascent + fh.descent;
		for (const auto& line : lines) {
			EmojiRenderer::DrawLine(owner, line.String(),
				BPoint(contentLeft, yPos), emojiSize);
			yPos += lineHeight;
		}
	}

	// Draw timestamp and delivery info
	owner->SetFont(&metaFont);

	// Draw base meta text — color-code by delivery status
	if (fOutgoing && fDeliveryStatus == DELIVERY_FAILED)
		owner->SetHighColor(kColorBad);
	else if (fOutgoing && fDeliveryStatus == DELIVERY_RETRYING)
		owner->SetHighColor(kColorFair);
	else
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

	// GIF messages have fixed height
	if (fIsGifMsg) {
		font_height fontHeight;
		font->GetHeight(&fontHeight);
		float lineHeight = fontHeight.ascent + fontHeight.descent
			+ fontHeight.leading;
		float headerHeight = (!fOutgoing && fSenderName.Length() > 0)
			? lineHeight : 0;
		float metaHeight = lineHeight * 0.8f;
		float imgH = 100.0f;
		if (fGifFrames != NULL && fGifFrames[0] != NULL) {
			float imgW = fGifFrames[0]->Bounds().Width() + 1;
			imgH = fGifFrames[0]->Bounds().Height() + 1;
			// Scale to max 200px width
			if (imgW > 200.0f) {
				imgH = imgH * 200.0f / imgW;
			}
		}
		float height = headerHeight + imgH + metaHeight
			+ kBubblePadding * 2 + kBubbleMargin;
		SetHeight(height);
		return;
	}

	// Image messages have fixed height based on content
	if (fIsImageMsg) {
		font_height fontHeight;
		font->GetHeight(&fontHeight);
		float lineHeight = fontHeight.ascent + fontHeight.descent
			+ fontHeight.leading;
		float headerHeight = (!fOutgoing && fSenderName.Length() > 0)
			? lineHeight : 0;
		float metaHeight = lineHeight * 0.8f;
		float imgH = 100.0f;  // placeholder height
		if (fImageBitmap != NULL)
			imgH = fImageBitmap->Bounds().Height() + 1;
		float height = headerHeight + imgH + metaHeight
			+ kBubblePadding * 2 + kBubbleMargin;
		SetHeight(height);
		return;
	}

	bool isCli = (fTxtType == 1);
	bool isSar = fIsSarMarker;

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

	int32 lineCount;

	if (isSar) {
		// SAR marker: title + coordinates + optional note = 2-3 lines
		lineCount = 2;
		if (fSarMarker.notes[0] != '\0')
			lineCount = 3;
	} else {
		// For CLI messages, prepend "> " to outgoing commands
		BString displayText = fText;
		if (isCli && fOutgoing)
			displayText.Prepend("> ");

		// Estimate line count for height calculation.
		// NOTE: Do NOT call owner->SetFont() here — it triggers
		// BListView::_UpdateItems() → Update() infinite recursion.
		// Use measureFont->StringWidth() directly for consistent results.
		float availWidth = maxBubbleWidth - kBubblePadding * 2;
		float textWidth = measureFont->StringWidth(displayText.String());
		lineCount = (textWidth > availWidth)
			? (int32)(textWidth / availWidth) + 1 : 1;
		if (lineCount < 1)
			lineCount = 1;
	}

	std::vector<BString> lines;
	for (int32 i = 0; i < lineCount; i++)
		lines.push_back("");

	// Calculate total height
	float textHeight = lines.size() * lineHeight;
	float headerHeight = (!fOutgoing && fSenderName.Length() > 0) ? lineHeight : 0;
	float metaHeight = lineHeight * 0.8f;

	float height = headerHeight + textHeight + metaHeight +
		kBubblePadding * 2 + kBubbleMargin;

	SetHeight(height);
}


void
MessageView::SetDeliveryStatus(uint8 status, uint32 rtt, uint8 retryCount)
{
	fDeliveryStatus = status;
	fRoundTripMs = rtt;
	fRetryCount = retryCount;
}


void
MessageView::SetImageState(ImageSessionState state, uint8 receivedCount)
{
	fImageState = state;
	fImageReceivedFragments = receivedCount;
}


void
MessageView::SetImageBitmap(BBitmap* bitmap)
{
	delete fImageBitmap;
	fImageBitmap = bitmap;
	if (bitmap != NULL)
		fImageState = IMAGE_COMPLETE;
}


void
MessageView::_DrawImageBubble(BView* owner, BRect frame)
{
	owner->SetFont(be_plain_font);
	font_height fh;
	owner->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	// Meta text (timestamp + delivery)
	char timeStr[16];
	_FormatTimestamp(timeStr, sizeof(timeStr));
	BString displayMeta;
	if (fOutgoing) {
		switch (fDeliveryStatus) {
			case DELIVERY_PENDING:
				displayMeta.SetToFormat("%s \xE2\x8F\xB3", timeStr);
				break;
			case DELIVERY_CONFIRMED:
				displayMeta.SetToFormat("%s \xE2\x9C\x93\xE2\x9C\x93", timeStr);
				break;
			default:
				displayMeta.SetToFormat("%s \xE2\x9C\x93", timeStr);
				break;
		}
	} else {
		displayMeta.SetToFormat("%s", timeStr);
	}

	BFont metaFont;
	owner->GetFont(&metaFont);
	metaFont.SetSize(metaFont.Size() * 0.85f);
	float metaWidth = metaFont.StringWidth(displayMeta.String());
	float metaHeight = lineHeight * 0.8f;

	// Image area dimensions
	float imgW, imgH;
	if (fImageBitmap != NULL) {
		imgW = fImageBitmap->Bounds().Width() + 1;
		imgH = fImageBitmap->Bounds().Height() + 1;
	} else {
		imgW = 128;
		imgH = 100;
	}

	float headerHeight = (!fOutgoing && fSenderName.Length() > 0)
		? lineHeight : 0;
	float contentWidth = std::max(imgW, metaWidth);
	float bubbleWidth = contentWidth + kBubblePadding * 2;
	float bubbleHeight = headerHeight + imgH + metaHeight + kBubblePadding * 2;

	// Position bubble
	BRect bubbleRect;
	if (fOutgoing) {
		bubbleRect.left = frame.right - bubbleWidth - kBubbleMargin;
		bubbleRect.right = frame.right - kBubbleMargin;
	} else {
		bubbleRect.left = frame.left + kBubbleMargin;
		bubbleRect.right = frame.left + kBubbleMargin + bubbleWidth;
	}
	bubbleRect.top = frame.top + kBubbleMargin / 2;
	bubbleRect.bottom = bubbleRect.top + bubbleHeight;

	// Draw bubble
	rgb_color bubbleColor = fOutgoing
		? OutgoingBubbleColor() : IncomingBubbleColor();
	rgb_color borderColor = fOutgoing
		? OutgoingBorderColor() : IncomingBorderColor();

	// Shadow
	owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_2_TINT));
	BRect shadowRect = bubbleRect;
	shadowRect.OffsetBy(1, 1);
	owner->FillRoundRect(shadowRect, kBubbleRadius, kBubbleRadius);

	owner->SetHighColor(bubbleColor);
	owner->FillRoundRect(bubbleRect, kBubbleRadius, kBubbleRadius);
	owner->SetHighColor(borderColor);
	owner->StrokeRoundRect(bubbleRect, kBubbleRadius, kBubbleRadius);

	float yPos = bubbleRect.top + kBubblePadding + fh.ascent;

	// Sender name
	if (!fOutgoing && fSenderName.Length() > 0) {
		BFont savedFont;
		owner->GetFont(&savedFont);
		BFont boldFont(savedFont);
		boldFont.SetFace(B_BOLD_FACE);
		owner->SetFont(&boldFont);
		owner->SetHighColor(SenderNameColor());
		owner->DrawString(fSenderName.String(),
			BPoint(bubbleRect.left + kBubblePadding, yPos));
		owner->SetFont(&savedFont);
		yPos += lineHeight;
	}

	float contentLeft = bubbleRect.left + kBubblePadding;
	float imageTop = yPos - fh.ascent;

	if (fImageState == IMAGE_COMPLETE && fImageBitmap != NULL) {
		// Draw the decoded image
		BRect srcRect = fImageBitmap->Bounds();
		BRect destRect(contentLeft, imageTop,
			contentLeft + imgW - 1, imageTop + imgH - 1);
		owner->DrawBitmap(fImageBitmap, srcRect, destRect);
	} else if (fImageState == IMAGE_LOADING) {
		// Gray placeholder with progress bar
		rgb_color placeholderBg = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT);
		BRect imgRect(contentLeft, imageTop,
			contentLeft + imgW - 1, imageTop + imgH - 1);
		owner->SetHighColor(placeholderBg);
		owner->FillRect(imgRect);

		// Progress bar
		float progress = 0;
		if (fImageTotalFragments > 0)
			progress = (float)fImageReceivedFragments / fImageTotalFragments;
		float barY = imageTop + imgH * 0.5f - 4;
		float barW = imgW - 20;
		BRect barBg(contentLeft + 10, barY,
			contentLeft + 10 + barW, barY + 8);
		owner->SetHighColor(tint_color(placeholderBg, B_DARKEN_1_TINT));
		owner->FillRoundRect(barBg, 3, 3);
		if (progress > 0) {
			BRect barFill(barBg.left, barBg.top,
				barBg.left + barW * progress, barBg.bottom);
			owner->SetHighColor(kColorGood);
			owner->FillRoundRect(barFill, 3, 3);
		}

		// Fragment count text
		BString fragText;
		fragText.SetToFormat("%d/%d fragments",
			(int)fImageReceivedFragments, (int)fImageTotalFragments);
		float tw = owner->StringWidth(fragText.String());
		owner->SetHighColor(BubbleTextColor());
		owner->DrawString(fragText.String(),
			BPoint(contentLeft + (imgW - tw) / 2, barY + 24));
	} else if (fImageState == IMAGE_FAILED) {
		// Failed state
		BRect imgRect(contentLeft, imageTop,
			contentLeft + imgW - 1, imageTop + imgH - 1);
		rgb_color placeholderBg = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT);
		owner->SetHighColor(placeholderBg);
		owner->FillRect(imgRect);
		owner->SetHighColor(kColorBad);
		const char* failText = "Image transfer failed";
		float tw = owner->StringWidth(failText);
		owner->DrawString(failText,
			BPoint(contentLeft + (imgW - tw) / 2,
				imageTop + imgH / 2 + fh.ascent / 2));
	} else {
		// PENDING — placeholder with "tap to download"
		BRect imgRect(contentLeft, imageTop,
			contentLeft + imgW - 1, imageTop + imgH - 1);
		rgb_color placeholderBg = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT);
		owner->SetHighColor(placeholderBg);
		owner->FillRect(imgRect);

		owner->SetHighColor(BubbleTextColor());
		BString sizeText;
		sizeText.SetToFormat("Image %ldx%ld", (long)fImageWidth,
			(long)fImageHeight);
		float tw = owner->StringWidth(sizeText.String());
		owner->DrawString(sizeText.String(),
			BPoint(contentLeft + (imgW - tw) / 2,
				imageTop + imgH / 2 - 4));

		owner->SetHighColor(ui_color(B_CONTROL_HIGHLIGHT_COLOR));
		const char* dlText = "Waiting for fragments...";
		tw = owner->StringWidth(dlText);
		owner->DrawString(dlText,
			BPoint(contentLeft + (imgW - tw) / 2,
				imageTop + imgH / 2 + fh.ascent + 4));
	}

	// Draw meta text
	owner->SetFont(&metaFont);
	owner->SetHighColor(MetaTextColor());
	float metaX = bubbleRect.right - kBubblePadding - metaWidth;
	float metaY = bubbleRect.bottom - kBubblePadding / 2;
	owner->DrawString(displayMeta.String(), BPoint(metaX, metaY));
}


void
MessageView::SetGifLoadState(uint8 state)
{
	fGifLoadState = state;
}


void
MessageView::SetGifFrames(BBitmap** frames, uint32* durations, int32 count)
{
	// Clean up old frames
	if (fGifFrames != NULL) {
		for (int32 i = 0; i < fGifFrameCount; i++)
			delete fGifFrames[i];
		delete[] fGifFrames;
		delete[] fGifDurations;
	}

	fGifFrames = frames;
	fGifDurations = durations;
	fGifFrameCount = count;
	fGifCurrentFrame = 0;
	fGifLastAdvance = system_time();
	fGifLoadState = 2;  // loaded
}


void
MessageView::AdvanceGifFrame()
{
	if (fGifFrameCount <= 1)
		return;

	bigtime_t now = system_time();
	bigtime_t elapsed = now - fGifLastAdvance;
	if (elapsed >= (bigtime_t)fGifDurations[fGifCurrentFrame] * 1000) {
		fGifCurrentFrame = (fGifCurrentFrame + 1) % fGifFrameCount;
		fGifLastAdvance = now;
	}
}


uint32
MessageView::CurrentFrameDuration() const
{
	if (fGifFrameCount == 0 || fGifDurations == NULL)
		return 100;
	return fGifDurations[fGifCurrentFrame];
}


void
MessageView::_DrawGifBubble(BView* owner, BRect frame)
{
	owner->SetFont(be_plain_font);
	font_height fh;
	owner->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	// Meta text
	char timeStr[16];
	_FormatTimestamp(timeStr, sizeof(timeStr));
	BString displayMeta;
	if (fOutgoing) {
		switch (fDeliveryStatus) {
			case DELIVERY_PENDING:
				displayMeta.SetToFormat("%s \xE2\x8F\xB3", timeStr);
				break;
			case DELIVERY_CONFIRMED:
				displayMeta.SetToFormat("%s \xE2\x9C\x93\xE2\x9C\x93",
					timeStr);
				break;
			default:
				displayMeta.SetToFormat("%s \xE2\x9C\x93", timeStr);
				break;
		}
	} else {
		if (fPathLen == 0 || fPathLen == kPathLenDirect)
			displayMeta.SetToFormat("%s", timeStr);
		else
			displayMeta.SetToFormat("%s \xC2\xB7 %d hops", timeStr, fPathLen);
	}

	BFont metaFont;
	owner->GetFont(&metaFont);
	metaFont.SetSize(metaFont.Size() * 0.85f);
	float metaWidth = metaFont.StringWidth(displayMeta.String());
	float metaHeight = lineHeight * 0.8f;

	// Image dimensions
	float imgW = 128, imgH = 100;
	float displayW = imgW, displayH = imgH;

	if (fGifFrames != NULL && fGifFrames[0] != NULL) {
		imgW = fGifFrames[0]->Bounds().Width() + 1;
		imgH = fGifFrames[0]->Bounds().Height() + 1;
		displayW = imgW;
		displayH = imgH;
		if (displayW > 200.0f) {
			displayH = displayH * 200.0f / displayW;
			displayW = 200.0f;
		}
	}

	float headerHeight = (!fOutgoing && fSenderName.Length() > 0)
		? lineHeight : 0;
	float contentWidth = std::max(displayW, metaWidth);
	float bubbleWidth = contentWidth + kBubblePadding * 2;
	float bubbleHeight = headerHeight + displayH + metaHeight
		+ kBubblePadding * 2;

	// Position bubble
	BRect bubbleRect;
	if (fOutgoing) {
		bubbleRect.left = frame.right - bubbleWidth - kBubbleMargin;
		bubbleRect.right = frame.right - kBubbleMargin;
	} else {
		bubbleRect.left = frame.left + kBubbleMargin;
		bubbleRect.right = frame.left + kBubbleMargin + bubbleWidth;
	}
	bubbleRect.top = frame.top + kBubbleMargin / 2;
	bubbleRect.bottom = bubbleRect.top + bubbleHeight;

	// Draw bubble
	rgb_color bubbleColor = fOutgoing
		? OutgoingBubbleColor() : IncomingBubbleColor();
	rgb_color borderColor = fOutgoing
		? OutgoingBorderColor() : IncomingBorderColor();

	// Shadow
	owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_2_TINT));
	BRect shadowRect = bubbleRect;
	shadowRect.OffsetBy(1, 1);
	owner->FillRoundRect(shadowRect, kBubbleRadius, kBubbleRadius);

	owner->SetHighColor(bubbleColor);
	owner->FillRoundRect(bubbleRect, kBubbleRadius, kBubbleRadius);
	owner->SetHighColor(borderColor);
	owner->StrokeRoundRect(bubbleRect, kBubbleRadius, kBubbleRadius);

	float yPos = bubbleRect.top + kBubblePadding + fh.ascent;

	// Sender name
	if (!fOutgoing && fSenderName.Length() > 0) {
		BFont savedFont;
		owner->GetFont(&savedFont);
		BFont boldFont(savedFont);
		boldFont.SetFace(B_BOLD_FACE);
		owner->SetFont(&boldFont);
		owner->SetHighColor(SenderNameColor());
		owner->DrawString(fSenderName.String(),
			BPoint(bubbleRect.left + kBubblePadding, yPos));
		owner->SetFont(&savedFont);
		yPos += lineHeight;
	}

	float contentLeft = bubbleRect.left + kBubblePadding;
	float imageTop = yPos - fh.ascent;

	if (fGifLoadState == 2 && fGifFrames != NULL
		&& fGifCurrentFrame < fGifFrameCount) {
		// Draw current GIF frame
		BBitmap* currentFrame = fGifFrames[fGifCurrentFrame];
		BRect srcRect = currentFrame->Bounds();
		BRect destRect(contentLeft, imageTop,
			contentLeft + displayW - 1, imageTop + displayH - 1);
		owner->DrawBitmap(currentFrame, srcRect, destRect);

		// "GIF" badge in top-right corner
		BFont badgeFont(be_plain_font);
		badgeFont.SetSize(9);
		owner->SetFont(&badgeFont);
		float badgeW = badgeFont.StringWidth("GIF") + 8;
		float badgeH = 14;
		BRect badgeRect(destRect.right - badgeW - 2, destRect.top + 2,
			destRect.right - 2, destRect.top + 2 + badgeH);
		rgb_color badgeBg = {0, 0, 0, 180};
		owner->SetHighColor(badgeBg);
		owner->FillRoundRect(badgeRect, 3, 3);
		owner->SetHighColor((rgb_color){255, 255, 255, 255});
		font_height bfh;
		badgeFont.GetHeight(&bfh);
		owner->DrawString("GIF",
			BPoint(badgeRect.left + 4,
				badgeRect.top + bfh.ascent + 1));
		owner->SetFont(be_plain_font);
	} else if (fGifLoadState == 1) {
		// Loading placeholder
		BRect imgRect(contentLeft, imageTop,
			contentLeft + displayW - 1, imageTop + displayH - 1);
		owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		owner->FillRect(imgRect);
		owner->SetHighColor(BubbleTextColor());
		const char* loadText = "Loading GIF...";
		float tw = owner->StringWidth(loadText);
		owner->DrawString(loadText,
			BPoint(contentLeft + (displayW - tw) / 2,
				imageTop + displayH / 2 + fh.ascent / 2));
	} else if (fGifLoadState == 3) {
		// Error
		BRect imgRect(contentLeft, imageTop,
			contentLeft + displayW - 1, imageTop + displayH - 1);
		owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		owner->FillRect(imgRect);
		owner->SetHighColor(kColorBad);
		const char* errText = "GIF not available";
		float tw = owner->StringWidth(errText);
		owner->DrawString(errText,
			BPoint(contentLeft + (displayW - tw) / 2,
				imageTop + displayH / 2 + fh.ascent / 2));
	} else {
		// Not loaded yet
		BRect imgRect(contentLeft, imageTop,
			contentLeft + displayW - 1, imageTop + displayH - 1);
		owner->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		owner->FillRect(imgRect);
		owner->SetHighColor(MetaTextColor());
		const char* gifText = "GIF";
		float tw = owner->StringWidth(gifText);
		owner->DrawString(gifText,
			BPoint(contentLeft + (displayW - tw) / 2,
				imageTop + displayH / 2 + fh.ascent / 2));
	}

	// Meta text
	owner->SetFont(&metaFont);
	owner->SetHighColor(MetaTextColor());
	float metaX = bubbleRect.right - kBubblePadding - metaWidth;
	float metaY = bubbleRect.bottom - kBubblePadding / 2;
	owner->DrawString(displayMeta.String(), BPoint(metaX, metaY));
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
