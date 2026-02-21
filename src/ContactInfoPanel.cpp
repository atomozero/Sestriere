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
#include <TabView.h>
#include <TextControl.h>
#include <Window.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <Alert.h>

#include "Constants.h"
#include "DatabaseManager.h"
#include "Utils.h"
#include "SNRChartView.h"


// Internal message codes for admin buttons
static const uint32 kMsgRebootClicked = 'arbc';
static const uint32 kMsgResetClicked = 'arfc';
static const uint32 kMsgVersionClicked = 'cver';
static const uint32 kMsgNeighborsClicked = 'cnbr';
static const uint32 kMsgClockClicked = 'cclk';
static const uint32 kMsgClearStatsClicked = 'ccls';
static const uint32 kMsgSetNameClicked = 'csnm';
static const uint32 kMsgPasswordClicked = 'cspw';


// Avatar colors — use kAvatarPalette from Constants.h

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
		case 1: return kTypeBadgeChat;
		case 2: return kTypeBadgeRepeater;
		case 3: return kTypeBadgeRoom;
		default: return LabelColor();
	}
}

// Status colors — use named constants from Constants.h
static const rgb_color& kOnlineColor = kStatusOnline;
static const rgb_color& kRecentColor = kStatusRecent;
static const rgb_color& kOfflineColor = kStatusOffline;


ContactInfoPanel::ContactInfoPanel(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fContact(NULL),
	fIsChannel(false),
	fChannelContactCount(0),
	fChannelOnlineCount(0),
	fSNRChart(NULL),
	fAdminActive(false),
	fBattMv(0),
	fUsedKb(0),
	fTotalKb(0),
	fAdminUptime(0),
	fAdminTxPkts(0),
	fAdminRxPkts(0),
	fAdminRssi(0),
	fAdminSnr(0),
	fAdminNoise(0),
	fAdminTabView(NULL),
	fVersionButton(NULL),
	fNeighborsButton(NULL),
	fClockButton(NULL),
	fClearStatsButton(NULL),
	fSetNameField(NULL),
	fSetNameButton(NULL),
	fPasswordField(NULL),
	fPasswordButton(NULL),
	fRebootButton(NULL),
	fFactoryResetButton(NULL)
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

	// Build admin BTabView (hidden until admin session)
	_BuildAdminTabs();
}


ContactInfoPanel::~ContactInfoPanel()
{
}


void
ContactInfoPanel::AttachedToWindow()
{
	BView::AttachedToWindow();
	fRebootButton->SetTarget(this);
	fFactoryResetButton->SetTarget(this);
	fVersionButton->SetTarget(this);
	fNeighborsButton->SetTarget(this);
	fClockButton->SetTarget(this);
	fClearStatsButton->SetTarget(this);
	fSetNameButton->SetTarget(this);
	fPasswordButton->SetTarget(this);
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
		case kMsgRebootClicked:
		{
			BAlert* alert = new BAlert("Reboot",
				"Reboot this device?\n\n"
				"The device will restart and briefly go offline.",
				"Cancel", "Reboot", NULL,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			if (alert->Go() == 1) {
				BMessage cmd(MSG_ADMIN_REBOOT);
				Window()->PostMessage(&cmd);
			}
			break;
		}

		case kMsgResetClicked:
		{
			BAlert* alert1 = new BAlert("Factory Reset",
				"Factory reset this device?\n\n"
				"ALL contacts, messages, and settings will be erased!",
				"Cancel", "Continue", NULL,
				B_WIDTH_AS_USUAL, B_STOP_ALERT);
			if (alert1->Go() == 1) {
				BAlert* alert2 = new BAlert("Confirm Reset",
					"Are you SURE?\n\n"
					"This cannot be undone!",
					"Cancel", "Reset", NULL,
					B_WIDTH_AS_USUAL, B_STOP_ALERT);
				if (alert2->Go() == 1) {
					BMessage cmd(MSG_ADMIN_FACTORY_RESET);
					Window()->PostMessage(&cmd);
				}
			}
			break;
		}

		case kMsgVersionClicked:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "ver");
			Window()->PostMessage(&cmd);
			break;
		}

		case kMsgNeighborsClicked:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "neighbors");
			Window()->PostMessage(&cmd);
			break;
		}

		case kMsgClockClicked:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "clock");
			Window()->PostMessage(&cmd);
			break;
		}

		case kMsgClearStatsClicked:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "clear stats");
			Window()->PostMessage(&cmd);
			break;
		}

		case kMsgSetNameClicked:
		{
			const char* text = fSetNameField->Text();
			if (text != NULL && text[0] != '\0') {
				BString command("set name ");
				command << text;
				BMessage cmd(MSG_ADMIN_SEND_CLI);
				cmd.AddString("command", command.String());
				Window()->PostMessage(&cmd);
				fSetNameField->SetText("");
			}
			break;
		}

		case kMsgPasswordClicked:
		{
			const char* text = fPasswordField->Text();
			if (text != NULL && text[0] != '\0') {
				BAlert* alert = new BAlert("Change Password",
					"Change the admin password for this device?",
					"Cancel", "Change", NULL,
					B_WIDTH_AS_USUAL, B_WARNING_ALERT);
				if (alert->Go() == 1) {
					BString command("password ");
					command << text;
					BMessage cmd(MSG_ADMIN_SEND_CLI);
					cmd.AddString("command", command.String());
					Window()->PostMessage(&cmd);
					fPasswordField->SetText("");
				}
			}
			break;
		}

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
			_DrawInfoRow(y, "Online", onlineStr, kStatusOnline);
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

	// === Admin sections (repeater/room) ===
	if (fAdminActive && fContact->type >= 2)
		_DrawAdminSections(y);

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

	// Show admin tabs only for repeater/room contacts
	bool showAdmin = (fAdminActive && contact != NULL && contact->type >= 2);
	_ShowAdminTabs(showAdmin);

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
	_ShowAdminTabs(false);
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
	fAdminActive = false;
	_ShowAdminTabs(false);
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
		return kAvatarPalette[0];

	uint32 hash = 0;
	const char* name = fContact->name;
	while (*name)
		hash = hash * 31 + (uint8)*name++;
	return kAvatarPalette[hash % kAvatarPaletteCount];
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

	if (age < 86400) {
		FormatTimeAgo(buffer, size, age);
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


// === Admin session methods ===

void
ContactInfoPanel::SetAdminSession(bool active)
{
	fAdminActive = active;

	// Only show tabs if current contact is a repeater/room
	bool showTabs = (active && fContact != NULL && fContact->type >= 2);
	_ShowAdminTabs(showTabs);
	Invalidate();
}


void
ContactInfoPanel::SetBatteryInfo(uint16 battMv, uint32 usedKb, uint32 totalKb)
{
	fBattMv = battMv;
	fUsedKb = usedKb;
	fTotalKb = totalKb;
	if (fAdminActive)
		Invalidate();
}


void
ContactInfoPanel::SetRadioStats(uint32 uptime, uint32 txPkts, uint32 rxPkts,
	int8 rssi, int8 snr, int8 noise)
{
	fAdminUptime = uptime;
	fAdminTxPkts = txPkts;
	fAdminRxPkts = rxPkts;
	fAdminRssi = rssi;
	fAdminSnr = snr;
	fAdminNoise = noise;
	if (fAdminActive)
		Invalidate();
}


void
ContactInfoPanel::_DrawSectionHeader(float& y, const char* title)
{
	BRect bounds = Bounds();
	float x = bounds.left + kMargin;

	// Separator line
	SetHighColor(BorderColor());
	StrokeLine(BPoint(x, y), BPoint(bounds.right - kMargin, y));
	y += 6;

	// Section title
	BFont headerFont;
	GetFont(&headerFont);
	headerFont.SetSize(10);
	headerFont.SetFace(B_BOLD_FACE);
	SetFont(&headerFont);

	font_height hfh;
	headerFont.GetHeight(&hfh);
	SetHighColor(AccentColor());
	DrawString(title, BPoint(x, y + hfh.ascent));
	y += hfh.ascent + hfh.descent + 6;

	// Restore info font
	BFont infoFont;
	GetFont(&infoFont);
	infoFont.SetSize(11);
	infoFont.SetFace(B_REGULAR_FACE);
	SetFont(&infoFont);
}


void
ContactInfoPanel::_DrawAdminSections(float& y)
{
	y += 4;

	// === Device section ===
	_DrawSectionHeader(y, "Device");

	// Battery — percentage with color
	int battPct = 0;
	if (fBattMv > 0)
		battPct = ((int)fBattMv - kBattMinMv) * 100 / kBattRangeMv;
	if (battPct < 0) battPct = 0;
	if (battPct > 100) battPct = 100;

	rgb_color battColor;
	if (battPct > 50)
		battColor = kColorGood;
	else if (battPct > 20)
		battColor = kColorFair;
	else
		battColor = kColorBad;

	char battStr[32];
	snprintf(battStr, sizeof(battStr), "%d%% (%u mV)", battPct, fBattMv);
	_DrawInfoRow(y, "Batt", battStr, battColor);

	// Battery bar (compact)
	BRect bounds = Bounds();
	float x = bounds.left + kMargin;
	float barW = bounds.Width() - kMargin * 2;
	float barH = 4;
	float barY = y - 2;

	SetHighColor(tint_color(PanelBg(), B_DARKEN_2_TINT));
	BRect barBg(x, barY, x + barW, barY + barH);
	FillRoundRect(barBg, 2, 2);

	SetHighColor(battColor);
	BRect barFill(x, barY, x + barW * battPct / 100.0f, barY + barH);
	FillRoundRect(barFill, 2, 2);
	y += barH + 4;

	// Storage
	if (fTotalKb > 0) {
		char storStr[32];
		float usedMb = fUsedKb / 1024.0f;
		float totalMb = fTotalKb / 1024.0f;
		snprintf(storStr, sizeof(storStr), "%.1f / %.1f MB",
			usedMb, totalMb);
		_DrawInfoRow(y, "Store", storStr, TextColor());
	}

	// Uptime
	if (fAdminUptime > 0) {
		char uptStr[32];
		FormatUptime(uptStr, sizeof(uptStr), fAdminUptime);
		_DrawInfoRow(y, "Up", uptStr, TextColor());
	}

	// === Radio section ===
	_DrawSectionHeader(y, "Radio");

	// TX / RX
	char pktStr[32];
	snprintf(pktStr, sizeof(pktStr), "%u / %u",
		fAdminTxPkts, fAdminRxPkts);
	_DrawInfoRow(y, "TX/RX", pktStr, TextColor());

	// RSSI
	char rssiStr[16];
	snprintf(rssiStr, sizeof(rssiStr), "%d dBm", fAdminRssi);
	_DrawInfoRow(y, "RSSI", rssiStr, TextColor());

	// SNR (colored)
	char snrStr[16];
	snprintf(snrStr, sizeof(snrStr), "%d dB", fAdminSnr);
	rgb_color snrColor;
	if (fAdminSnr > 0)
		snrColor = kColorGood;
	else if (fAdminSnr > -10)
		snrColor = kColorFair;
	else
		snrColor = kColorBad;
	_DrawInfoRow(y, "SNR", snrStr, snrColor);

	// Noise floor
	char noiseStr[16];
	snprintf(noiseStr, sizeof(noiseStr), "%d dBm", fAdminNoise);
	_DrawInfoRow(y, "Noise", noiseStr, TextColor());

	// Position admin tabs below stats
	_PositionAdminTabs(y);
}


void
ContactInfoPanel::_BuildAdminTabs()
{
	fAdminTabView = new BTabView("admin_tabs", B_WIDTH_FROM_WIDEST);
	fAdminTabView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// === Query tab ===
	BView* queryTab = new BView("query_tab", B_SUPPORTS_LAYOUT);
	queryTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fVersionButton = new BButton("version", "Version",
		new BMessage(kMsgVersionClicked));
	fNeighborsButton = new BButton("neighbors", "Neighbors",
		new BMessage(kMsgNeighborsClicked));
	fClockButton = new BButton("clock", "Clock",
		new BMessage(kMsgClockClicked));
	fClearStatsButton = new BButton("clear_stats", "Clear Stats",
		new BMessage(kMsgClearStatsClicked));

	BLayoutBuilder::Group<>(queryTab, B_VERTICAL, 4)
		.SetInsets(4, 4, 4, 4)
		.AddGroup(B_HORIZONTAL, 4)
			.Add(fVersionButton)
			.Add(fNeighborsButton)
		.End()
		.AddGroup(B_HORIZONTAL, 4)
			.Add(fClockButton)
			.Add(fClearStatsButton)
		.End()
		.AddGlue();

	fAdminTabView->AddTab(queryTab, new BTab());
	fAdminTabView->TabAt(0)->SetLabel("Query");

	// === Config tab ===
	BView* configTab = new BView("config_tab", B_SUPPORTS_LAYOUT);
	configTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fSetNameField = new BTextControl("set_name_field", "Name:", "",
		NULL);
	fSetNameButton = new BButton("set_name", "Set",
		new BMessage(kMsgSetNameClicked));
	fPasswordField = new BTextControl("password_field", "Pwd:", "",
		NULL);
	fPasswordButton = new BButton("set_password", "Set",
		new BMessage(kMsgPasswordClicked));

	BLayoutBuilder::Group<>(configTab, B_VERTICAL, 4)
		.SetInsets(4, 4, 4, 4)
		.AddGroup(B_HORIZONTAL, 4)
			.Add(fSetNameField)
			.Add(fSetNameButton)
		.End()
		.AddGroup(B_HORIZONTAL, 4)
			.Add(fPasswordField)
			.Add(fPasswordButton)
		.End()
		.AddGlue();

	fAdminTabView->AddTab(configTab, new BTab());
	fAdminTabView->TabAt(1)->SetLabel("Config");

	// === Actions tab ===
	BView* actionsTab = new BView("actions_tab", B_SUPPORTS_LAYOUT);
	actionsTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fRebootButton = new BButton("reboot", "Reboot",
		new BMessage(kMsgRebootClicked));
	fFactoryResetButton = new BButton("factory_reset", "Factory Reset",
		new BMessage(kMsgResetClicked));

	BLayoutBuilder::Group<>(actionsTab, B_VERTICAL, 4)
		.SetInsets(4, 4, 4, 4)
		.AddGroup(B_HORIZONTAL, 4)
			.Add(fRebootButton)
			.Add(fFactoryResetButton)
		.End()
		.AddGlue();

	fAdminTabView->AddTab(actionsTab, new BTab());
	fAdminTabView->TabAt(2)->SetLabel("Actions");

	// Initially hidden, positioned off-screen
	fAdminTabView->MoveTo(kMargin, 500);
	fAdminTabView->ResizeTo(kPanelMinWidth - kMargin * 2, 100);
	fAdminTabView->Hide();
	AddChild(fAdminTabView);
}


void
ContactInfoPanel::_PositionAdminTabs(float& y)
{
	if (fAdminTabView == NULL)
		return;

	BRect bounds = Bounds();
	float x = bounds.left + kMargin;
	float w = bounds.Width() - kMargin * 2;

	// Ensure minimum width
	if (w < 60)
		w = 60;

	// Tab height: tab bar (~22px) + content (2 rows of buttons ~60px)
	float h = 100;

	y += 4;
	fAdminTabView->MoveTo(x, y);
	fAdminTabView->ResizeTo(w, h);
	y += h + 4;
}


void
ContactInfoPanel::_ShowAdminTabs(bool show)
{
	if (fAdminTabView == NULL)
		return;

	if (show) {
		if (fAdminTabView->IsHidden())
			fAdminTabView->Show();
	} else {
		if (!fAdminTabView->IsHidden())
			fAdminTabView->Hide();
	}
}
