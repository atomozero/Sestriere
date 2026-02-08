/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactInfoPanel.cpp — Right-side panel showing selected contact details
 */

#include "ContactInfoPanel.h"

#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <Window.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "Constants.h"
#include "DatabaseManager.h"
#include "SNRChartView.h"


// Avatar colors (same palette as ContactItem/ChatHeaderView)
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
static const float kPanelMinWidth = 200.0f;
static const float kAvatarSize = 64.0f;
static const float kMargin = 12.0f;
static const float kRowHeight = 20.0f;

// Theme-aware colors
static inline rgb_color PanelBg()
{
	return ui_color(B_PANEL_BACKGROUND_COLOR);
}
static inline rgb_color TextColor()
{
	return ui_color(B_PANEL_TEXT_COLOR);
}
static inline rgb_color LabelColor()
{
	return tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT);
}
static inline rgb_color AccentColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}
static inline rgb_color BorderColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT);
}

// Type badge colors
static inline rgb_color TypeBadgeColor(uint8 type)
{
	switch (type) {
		case 1: return (rgb_color){79, 195, 247, 255};   // Blue for Chat
		case 2: return (rgb_color){100, 160, 100, 255};   // Green for Repeater
		case 3: return (rgb_color){120, 120, 180, 255};   // Purple for Room
		default: return LabelColor();
	}
}

// Status colors
static const rgb_color kOnlineColor = {77, 182, 172, 255};
static const rgb_color kRecentColor = {220, 180, 60, 255};
static const rgb_color kOfflineColor = {140, 140, 140, 255};


ContactInfoPanel::ContactInfoPanel(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fContact(NULL),
	fIsChannel(false),
	fChannelContactCount(0),
	fChannelOnlineCount(0),
	fSNRChart(NULL)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetExplicitMinSize(BSize(kPanelMinWidth, B_SIZE_UNSET));
	SetExplicitPreferredSize(BSize(220, B_SIZE_UNSET));

	// Create SNR chart child view — hidden until a contact is selected
	fSNRChart = new SNRChartView("snr_chart");
	fSNRChart->MoveTo(kMargin, 280);
	fSNRChart->ResizeTo(kPanelMinWidth - kMargin * 2, 100);
	fSNRChart->Hide();
	AddChild(fSNRChart);
}


ContactInfoPanel::~ContactInfoPanel()
{
}


void
ContactInfoPanel::AttachedToWindow()
{
	BView::AttachedToWindow();
}


void
ContactInfoPanel::FrameResized(float newWidth, float newHeight)
{
	BView::FrameResized(newWidth, newHeight);
	Invalidate();
}


void
ContactInfoPanel::MessageReceived(BMessage* message)
{
	switch (message->what) {
		default:
			BView::MessageReceived(message);
			break;
	}
}


BSize
ContactInfoPanel::MinSize()
{
	return BSize(kPanelMinWidth, 300);
}


BSize
ContactInfoPanel::PreferredSize()
{
	return BSize(220, B_SIZE_UNLIMITED);
}


void
ContactInfoPanel::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background
	SetLowColor(PanelBg());
	FillRect(bounds, B_SOLID_LOW);

	// Left border
	SetHighColor(BorderColor());
	StrokeLine(BPoint(bounds.left, bounds.top),
		BPoint(bounds.left, bounds.bottom));

	if (fContact == NULL && !fIsChannel) {
		// No contact selected
		BFont font;
		GetFont(&font);
		font.SetSize(12);
		SetFont(&font);

		font_height fh;
		font.GetHeight(&fh);

		SetHighColor(LabelColor());
		const char* hint = "Select a contact";
		float tw = StringWidth(hint);
		DrawString(hint,
			BPoint((bounds.Width() - tw) / 2,
				bounds.top + 40 + fh.ascent));
		return;
	}

	float x = bounds.left + kMargin;
	float y = bounds.top + kMargin;
	float contentWidth = bounds.Width() - kMargin * 2;

	// === Avatar (centered) ===
	float avatarX = x + (contentWidth - kAvatarSize) / 2;
	BRect avatarRect(avatarX, y, avatarX + kAvatarSize, y + kAvatarSize);
	_DrawAvatar(avatarRect);
	y += kAvatarSize + kMargin;

	// === Name (centered, bold) ===
	BFont nameFont;
	GetFont(&nameFont);
	nameFont.SetSize(14);
	nameFont.SetFace(B_BOLD_FACE);
	SetFont(&nameFont);

	font_height nameFh;
	nameFont.GetHeight(&nameFh);

	const char* displayName;
	if (fIsChannel) {
		displayName = "Public Channel";
		SetHighColor(AccentColor());
	} else {
		displayName = fContact->name[0] ? fContact->name : "Unknown";
		SetHighColor(TextColor());
	}

	float nameWidth = StringWidth(displayName);
	float nameX = x + (contentWidth - nameWidth) / 2;
	if (nameX < x)
		nameX = x;
	DrawString(displayName, BPoint(nameX, y + nameFh.ascent));
	y += nameFh.ascent + nameFh.descent + 4;

	// === Type badge (centered) ===
	if (!fIsChannel) {
		BFont smallFont;
		GetFont(&smallFont);
		smallFont.SetSize(11);
		smallFont.SetFace(B_REGULAR_FACE);
		SetFont(&smallFont);

		font_height smallFh;
		smallFont.GetHeight(&smallFh);

		const char* typeName = _TypeName();
		float badgeTextW = StringWidth(typeName);
		float badgeW = badgeTextW + 12;
		float badgeH = smallFh.ascent + smallFh.descent + 4;
		float badgeX = x + (contentWidth - badgeW) / 2;

		// Badge background
		SetHighColor(TypeBadgeColor(fContact->type));
		BRect badgeRect(badgeX, y, badgeX + badgeW, y + badgeH);
		FillRoundRect(badgeRect, 4, 4);

		// Badge text
		SetHighColor(255, 255, 255);
		DrawString(typeName,
			BPoint(badgeX + 6, y + 2 + smallFh.ascent));
		y += badgeH + kMargin;
	} else {
		y += 4;
	}

	// === Separator ===
	SetHighColor(BorderColor());
	StrokeLine(BPoint(x, y), BPoint(bounds.right - kMargin, y));
	y += kMargin;

	// === Info rows ===
	BFont infoFont;
	GetFont(&infoFont);
	infoFont.SetSize(11);
	infoFont.SetFace(B_REGULAR_FACE);
	SetFont(&infoFont);

	if (fIsChannel) {
		_DrawInfoRow(y, "Type", "Broadcast Channel", AccentColor());

		// Message count from DB
		int32 msgCount = DatabaseManager::Instance()->GetMessageCount("channel");
		if (msgCount > 0) {
			char countStr[16];
			snprintf(countStr, sizeof(countStr), "%d", (int)msgCount);
			_DrawInfoRow(y, "Msgs", countStr, TextColor());
		}

		// Network stats
		if (fChannelContactCount > 0) {
			char contactStr[32];
			snprintf(contactStr, sizeof(contactStr), "%d",
				(int)fChannelContactCount);
			_DrawInfoRow(y, "Nodes", contactStr, TextColor());
		}
		if (fChannelOnlineCount > 0) {
			char onlineStr[32];
			snprintf(onlineStr, sizeof(onlineStr), "%d",
				(int)fChannelOnlineCount);
			_DrawInfoRow(y, "Online", onlineStr,
				(rgb_color){77, 182, 172, 255});
		}

		// Separator
		y += 4;
		SetHighColor(BorderColor());
		StrokeLine(BPoint(x, y), BPoint(bounds.right - kMargin, y));
		y += kMargin;

		// Description
		BFont descFont;
		GetFont(&descFont);
		descFont.SetSize(10);
		descFont.SetFace(B_REGULAR_FACE);
		SetFont(&descFont);
		SetHighColor(LabelColor());

		font_height descFh;
		descFont.GetHeight(&descFh);
		float lineH = descFh.ascent + descFh.descent + 2;
		float maxW = contentWidth;

		const char* lines[] = {
			"Messages sent here are",
			"visible to all nodes",
			"in radio range.",
			"",
			"Received messages show",
			"sender name and SNR."
		};
		for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
			if (lines[i][0] == '\0') {
				y += lineH / 2;
				continue;
			}
			// Truncate if wider than panel
			BString line(lines[i]);
			float tw = StringWidth(line.String());
			if (tw > maxW)
				descFont.TruncateString(&line, B_TRUNCATE_END, maxW);
			DrawString(line.String(), BPoint(x, y + descFh.ascent));
			y += lineH;
		}

		return;
	}

	// Public key
	char pubKeyStr[20];
	_FormatPubKey(pubKeyStr, sizeof(pubKeyStr));
	_DrawInfoRow(y, "Key", pubKeyStr, TextColor());

	// Type
	_DrawInfoRow(y, "Type", _TypeName(), TypeBadgeColor(fContact->type));

	// Path length
	char pathStr[24];
	if (fContact->outPathLen == (int8)0xFF || fContact->outPathLen == kPathLenDirect) {
		snprintf(pathStr, sizeof(pathStr), "Direct");
	} else if (fContact->outPathLen < 0) {
		snprintf(pathStr, sizeof(pathStr), "Unknown");
	} else if (fContact->outPathLen == 0) {
		snprintf(pathStr, sizeof(pathStr), "Direct");
	} else {
		snprintf(pathStr, sizeof(pathStr), "%d hop%s",
			fContact->outPathLen,
			fContact->outPathLen > 1 ? "s" : "");
	}
	_DrawInfoRow(y, "Path", pathStr, TextColor());

	// Last seen
	char lastSeenStr[32];
	_FormatLastSeen(lastSeenStr, sizeof(lastSeenStr));

	rgb_color lastSeenColor = kOfflineColor;
	if (fContact->lastSeen > 0) {
		uint32 now = (uint32)time(NULL);
		uint32 age = (now > fContact->lastSeen) ? (now - fContact->lastSeen) : 0;
		if (age < 300)
			lastSeenColor = kOnlineColor;
		else if (age < 3600)
			lastSeenColor = kRecentColor;
	}
	_DrawInfoRow(y, "Seen", lastSeenStr, lastSeenColor);

	// Message count
	char contactHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(contactHex + i * 2, 3, "%02x", fContact->publicKey[i]);
	int32 msgCount = DatabaseManager::Instance()->GetMessageCount(contactHex);
	if (msgCount > 0) {
		char countStr[16];
		snprintf(countStr, sizeof(countStr), "%d", (int)msgCount);
		_DrawInfoRow(y, "Msgs", countStr, TextColor());
	}

	// Flags
	if (fContact->flags != 0) {
		char flagsStr[16];
		snprintf(flagsStr, sizeof(flagsStr), "0x%02X", fContact->flags);
		_DrawInfoRow(y, "Flags", flagsStr, LabelColor());
	}

	// === SNR Chart section ===
	y += 4;

	// Separator before chart
	SetHighColor(BorderColor());
	StrokeLine(BPoint(x, y), BPoint(bounds.right - kMargin, y));
	y += 4;

	// "Signal History" label
	BFont chartLabelFont;
	GetFont(&chartLabelFont);
	chartLabelFont.SetSize(10);
	chartLabelFont.SetFace(B_BOLD_FACE);
	SetFont(&chartLabelFont);

	font_height chartLabelFh;
	chartLabelFont.GetHeight(&chartLabelFh);
	SetHighColor(LabelColor());
	DrawString("Signal History (SNR)",
		BPoint(x, y + chartLabelFh.ascent));
	y += chartLabelFh.ascent + chartLabelFh.descent + 4;

	// Reposition chart to match current layout
	_PositionChart(y);
}


void
ContactInfoPanel::SetContact(const ContactInfo* contact)
{
	fContact = contact;
	fIsChannel = false;
	_UpdateSNRChart();
	if (fSNRChart != NULL && fSNRChart->IsHidden())
		fSNRChart->Show();
	Invalidate();
}


void
ContactInfoPanel::SetChannel(bool isChannel)
{
	fIsChannel = isChannel;
	if (isChannel)
		fContact = NULL;
	if (fSNRChart != NULL) {
		fSNRChart->ClearData();
		if (!fSNRChart->IsHidden())
			fSNRChart->Hide();
	}
	Invalidate();
}


void
ContactInfoPanel::SetChannelStats(int32 contactCount, int32 onlineCount)
{
	fChannelContactCount = contactCount;
	fChannelOnlineCount = onlineCount;
	if (fIsChannel)
		Invalidate();
}


void
ContactInfoPanel::Clear()
{
	fContact = NULL;
	fIsChannel = false;
	if (fSNRChart != NULL) {
		fSNRChart->ClearData();
		if (!fSNRChart->IsHidden())
			fSNRChart->Hide();
	}
	Invalidate();
}


void
ContactInfoPanel::RefreshSNRChart()
{
	_UpdateSNRChart();
	Invalidate();
}


void
ContactInfoPanel::_DrawAvatar(BRect rect)
{
	rgb_color avatarColor = fIsChannel ? AccentColor() : _AvatarColor();
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
			if (space != NULL && space[1] != '\0')
				initials.Append(toupper(space[1]), 1);
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
	font.SetSize(kAvatarSize * 0.4f);
	font.SetFace(B_BOLD_FACE);
	SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);
	float textWidth = StringWidth(initials.String());

	DrawString(initials.String(),
		BPoint(rect.left + (rect.Width() - textWidth) / 2,
			rect.top + (rect.Height() + fh.ascent - fh.descent) / 2));
}


void
ContactInfoPanel::_DrawInfoRow(float& y, const char* label, const char* value,
	rgb_color valueColor)
{
	font_height fh;
	BFont font;
	GetFont(&font);
	font.GetHeight(&fh);

	float x = Bounds().left + kMargin;

	// Label
	SetHighColor(LabelColor());
	DrawString(label, BPoint(x, y + fh.ascent));

	// Value (right-aligned or after label)
	float labelW = StringWidth(label);
	float valueX = x + labelW + 8;

	SetHighColor(valueColor);
	DrawString(value, BPoint(valueX, y + fh.ascent));

	y += fh.ascent + fh.descent + 6;
}


rgb_color
ContactInfoPanel::_AvatarColor() const
{
	if (fContact == NULL)
		return kAvatarColors[0];

	uint32 hash = 0;
	const char* name = fContact->name;
	while (*name)
		hash = hash * 31 + (uint8)*name++;
	return kAvatarColors[hash % kAvatarColorCount];
}


const char*
ContactInfoPanel::_TypeName() const
{
	if (fContact == NULL)
		return "Unknown";

	switch (fContact->type) {
		case 0: return "Unknown";
		case 1: return "Chat";
		case 2: return "Repeater";
		case 3: return "Room";
		default: return "Other";
	}
}


void
ContactInfoPanel::_FormatLastSeen(char* buffer, size_t size) const
{
	if (fContact == NULL || fContact->lastSeen == 0) {
		strlcpy(buffer, "Never", size);
		return;
	}

	time_t now = time(NULL);
	time_t seen = (time_t)fContact->lastSeen;
	uint32 age = (now > seen) ? (uint32)(now - seen) : 0;

	if (age < 60) {
		snprintf(buffer, size, "Just now");
	} else if (age < 3600) {
		snprintf(buffer, size, "%u min ago", age / 60);
	} else if (age < 86400) {
		snprintf(buffer, size, "%u hr ago", age / 3600);
	} else {
		struct tm tm;
		if (localtime_r(&seen, &tm) != NULL)
			strftime(buffer, size, "%d/%m %H:%M", &tm);
		else
			strlcpy(buffer, "?", size);
	}
}


void
ContactInfoPanel::_FormatPubKey(char* buffer, size_t size) const
{
	if (fContact == NULL) {
		strlcpy(buffer, "N/A", size);
		return;
	}

	// Show first 4 bytes as hex with ellipsis
	snprintf(buffer, size, "%02X%02X%02X%02X...",
		fContact->publicKey[0], fContact->publicKey[1],
		fContact->publicKey[2], fContact->publicKey[3]);
}


void
ContactInfoPanel::_UpdateSNRChart()
{
	if (fSNRChart == NULL)
		return;

	if (fContact == NULL) {
		fSNRChart->ClearData();
		return;
	}

	// Format contact key as hex
	char contactHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(contactHex + i * 2, 3, "%02x", fContact->publicKey[i]);

	// Load last 24 hours of SNR data
	uint32 since = (uint32)time(NULL) - 86400;
	BObjectList<SNRDataPoint, true> points(100);
	DatabaseManager::Instance()->LoadSNRHistory(contactHex, since, points);

	if (points.CountItems() > 0) {
		fSNRChart->SetData(points);
	} else {
		fSNRChart->ClearData();
	}
}


void
ContactInfoPanel::_PositionChart(float y)
{
	if (fSNRChart == NULL)
		return;

	BRect bounds = Bounds();
	float chartLeft = bounds.left + kMargin;
	float chartWidth = bounds.Width() - kMargin * 2;
	float chartHeight = 100;

	// Ensure minimum width
	if (chartWidth < 60)
		chartWidth = 60;

	fSNRChart->MoveTo(chartLeft, y);
	fSNRChart->ResizeTo(chartWidth, chartHeight);
	fSNRChart->Invalidate();
}
