/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatsWindow.cpp — Device statistics window implementation (modern design)
 */

#include "StatsWindow.h"

#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <StringView.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "Utils.h"


static const uint32 MSG_REFRESH_STATS = 'rfst';
static const uint32 MSG_STATS_WIN_TIMER = 'swtm';
static const bigtime_t kStatsWinRefreshInterval = 5000000;  // 5 seconds

// Sentinel: alpha=0 means "use default panel text color"
static const rgb_color kDefaultValueColor = {0, 0, 0, 0};


// Custom view for a single stat row with label and value
class StatRowView : public BView {
public:
	StatRowView(const char* label, const char* initialValue = "--")
		:
		BView(label, B_WILL_DRAW),
		fLabel(label),
		fValue(initialValue),
		fValueColor(kDefaultValueColor)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}

	void SetValue(const char* value, rgb_color color = kDefaultValueColor)
	{
		fValue = value;
		fValueColor = color;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();

		// Get font metrics
		BFont font;
		GetFont(&font);
		font_height fh;
		font.GetHeight(&fh);
		float baseline = fh.ascent + 2;

		// Draw label (left aligned, dimmed text)
		rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
		SetHighColor(tint_color(textColor, B_LIGHTEN_1_TINT));
		DrawString(fLabel.String(), BPoint(0, baseline));

		// Draw value (right aligned, colored)
		float valueWidth = StringWidth(fValue.String());
		if (fValueColor.alpha == 0)
			SetHighColor(textColor);
		else
			SetHighColor(fValueColor);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);
		DrawString(fValue.String(), BPoint(bounds.right - valueWidth, baseline));
		font.SetFace(B_REGULAR_FACE);
		SetFont(&font);
	}

	virtual BSize MinSize()
	{
		BFont font;
		GetFont(&font);
		font_height fh;
		font.GetHeight(&fh);
		return BSize(200, fh.ascent + fh.descent + 4);
	}

	virtual BSize PreferredSize() { return MinSize(); }
	virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, MinSize().height); }

private:
	BString fLabel;
	BString fValue;
	rgb_color fValueColor;
};


// Section header view
class SectionHeaderView : public BView {
public:
	SectionHeaderView(const char* title, const char* icon = NULL)
		:
		BView(title, B_WILL_DRAW),
		fTitle(title),
		fIcon(icon != NULL ? icon : "")
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();

		BFont font;
		GetFont(&font);
		font.SetSize(font.Size() * 1.1);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);

		font_height fh;
		font.GetHeight(&fh);
		float baseline = fh.ascent + 2;

		// Theme-aware header color
		rgb_color headerColor = ui_color(B_CONTROL_HIGHLIGHT_COLOR);

		// Draw icon if present
		float x = 0;
		if (fIcon.Length() > 0) {
			SetHighColor(headerColor);
			DrawString(fIcon.String(), BPoint(x, baseline));
			x += StringWidth(fIcon.String()) + 8;
		}

		// Draw title
		SetHighColor(headerColor);
		DrawString(fTitle.String(), BPoint(x, baseline));

		// Draw underline
		float lineY = bounds.bottom - 2;
		SetHighColor(tint_color(headerColor, B_LIGHTEN_2_TINT));
		StrokeLine(BPoint(0, lineY), BPoint(bounds.right, lineY));
	}

	virtual BSize MinSize()
	{
		BFont font;
		GetFont(&font);
		font_height fh;
		font.GetHeight(&fh);
		return BSize(150, fh.ascent + fh.descent + 8);
	}

	virtual BSize PreferredSize() { return MinSize(); }
	virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, MinSize().height); }

private:
	BString fTitle;
	BString fIcon;
};


StatsWindow::StatsWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 380, 520), "MeshCore Statistics",
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fScrollView(NULL),
	fUptimeView(NULL),
	fBatteryView(NULL),
	fNoiseFloorView(NULL),
	fRssiView(NULL),
	fSnrView(NULL),
	fTxAirTimeView(NULL),
	fRxAirTimeView(NULL),
	fRecvPacketsView(NULL),
	fSentPacketsView(NULL),
	fSentFloodView(NULL),
	fSentDirectView(NULL),
	fRecvFloodView(NULL),
	fRecvDirectView(NULL),
	fRefreshButton(NULL),
	fCloseButton(NULL),
	fRefreshTimer(NULL)
{
	// Create stat row views
	fUptimeView = new StatRowView("Uptime:");
	fBatteryView = new StatRowView("Battery:");

	fNoiseFloorView = new StatRowView("Noise Floor:");
	fRssiView = new StatRowView("RSSI:");
	fSnrView = new StatRowView("SNR:");
	fTxAirTimeView = new StatRowView("TX Air Time:");
	fRxAirTimeView = new StatRowView("RX Air Time:");

	fRecvPacketsView = new StatRowView("Received Packets:");
	fSentPacketsView = new StatRowView("Sent Packets:");
	fSentFloodView = new StatRowView("Sent (Flood):");
	fSentDirectView = new StatRowView("Sent (Direct):");
	fRecvFloodView = new StatRowView("Received (Flood):");
	fRecvDirectView = new StatRowView("Received (Direct):");

	// Buttons
	fRefreshButton = new BButton("refresh", "Refresh Now",
		new BMessage(MSG_REFRESH_STATS));
	fCloseButton = new BButton("close", "Close",
		new BMessage(B_QUIT_REQUESTED));

	// Build layout with sections
	BView* contentView = new BView("content", 0);
	contentView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BLayoutBuilder::Group<>(contentView, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING)

		// === Device Section ===
		.Add(new SectionHeaderView("Device Status"))
		.AddStrut(4)
		.Add(fUptimeView)
		.Add(fBatteryView)
		.AddStrut(12)

		// === Radio Section ===
		.Add(new SectionHeaderView("Radio Performance"))
		.AddStrut(4)
		.Add(fRssiView)
		.Add(fSnrView)
		.Add(fNoiseFloorView)
		.Add(fTxAirTimeView)
		.Add(fRxAirTimeView)
		.AddStrut(12)

		// === Packet Section ===
		.Add(new SectionHeaderView("Packet Statistics"))
		.AddStrut(4)
		.Add(fRecvPacketsView)
		.Add(fSentPacketsView)
		.Add(fSentFloodView)
		.Add(fSentDirectView)
		.Add(fRecvFloodView)
		.Add(fRecvDirectView)

		.AddGlue()
	.End();

	// Main layout with scroll view
	BScrollView* scrollView = new BScrollView("scroll", contentView,
		0, false, true, B_NO_BORDER);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(scrollView, 1.0)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING, B_USE_HALF_ITEM_SPACING,
				B_USE_WINDOW_SPACING, B_USE_WINDOW_SPACING)
			.Add(new BStringView("auto", "Auto-refresh: 5s"))
			.AddGlue()
			.Add(fRefreshButton)
			.Add(fCloseButton)
		.End()
	.End();

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	// Start auto-refresh timer
	BMessage timerMsg(MSG_STATS_WIN_TIMER);
	fRefreshTimer = new BMessageRunner(this, &timerMsg, kStatsWinRefreshInterval);
}


StatsWindow::~StatsWindow()
{
	delete fRefreshTimer;
}


bool
StatsWindow::QuitRequested()
{
	Hide();
	return false;
}


void
StatsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_REFRESH_STATS:
			_RequestStats();
			break;

		case MSG_STATS_WIN_TIMER:
			// Only request stats if window is visible
			if (!IsHidden())
				_RequestStats();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
StatsWindow::SetCoreStats(const CoreStats& stats)
{
	fCoreStats = stats;
	_UpdateDisplay();
}


void
StatsWindow::SetRadioStats(const RadioStats& stats)
{
	fRadioStats = stats;
	_UpdateDisplay();
}


void
StatsWindow::SetPacketStats(const PacketStats& stats)
{
	fPacketStats = stats;
	_UpdateDisplay();
}


void
StatsWindow::ParseStatsResponse(const uint8* data, size_t length)
{
	if (length < 2)
		return;

	uint8 subType = data[1];

	switch (subType) {
		case 0:  // Core stats
			// [2-3]=batt_mv(uint16 LE) [4-7]=uptime(uint32 LE)
			if (length >= 4)
				fCoreStats.batteryMv = ReadLE16(data + 2);
			if (length >= 8)
				fCoreStats.uptime = ReadLE32(data + 4);
			break;

		case 1:  // Radio stats
			// [2-3]=noise_floor(int16 LE) [4]=rssi(int8) [5]=snr(int8)
			// [6-9]=tx_air_time(uint32) [10-13]=rx_air_time(uint32)
			if (length >= 4)
				fRadioStats.noiseFloor = ReadLE16Signed(data + 2);
			if (length >= 6) {
				fRadioStats.lastRssi = (int8)data[4];
				fRadioStats.lastSnr = (int8)data[5];
			}
			if (length >= 14) {
				fRadioStats.txAirTimeMs = ReadLE32(data + 6);
				fRadioStats.rxAirTimeMs = ReadLE32(data + 10);
			}
			break;

		case 2:  // Packet stats (26 bytes)
			// [2-5]=recvPkts [6-9]=sentPkts [10-13]=sentFlood
			// [14-17]=sentDirect [18-21]=recvFlood [22-25]=recvDirect
			if (length >= 10) {
				fPacketStats.recvPackets = ReadLE32(data + 2);
				fPacketStats.sentPackets = ReadLE32(data + 6);
			}
			if (length >= 18) {
				fPacketStats.sentFlood = ReadLE32(data + 10);
				fPacketStats.sentDirect = ReadLE32(data + 14);
			}
			if (length >= 26) {
				fPacketStats.recvFlood = ReadLE32(data + 18);
				fPacketStats.recvDirect = ReadLE32(data + 22);
			}
			break;
	}

	_UpdateDisplay();
}


void
StatsWindow::_RequestStats()
{
	if (fParent != NULL) {
		// Use MSG_REQUEST_STATS_DATA to get data without reopening window
		BMessage msg(MSG_REQUEST_STATS_DATA);
		fParent->PostMessage(&msg);
	}
}


void
StatsWindow::_UpdateDisplay()
{
	char buf[64];

	// === Device Status ===
	// Uptime
	uint32 uptime = fCoreStats.uptime;
	uint32 days = uptime / 86400;
	uint32 hours = (uptime % 86400) / 3600;
	uint32 mins = (uptime % 3600) / 60;
	uint32 secs = uptime % 60;
	if (days > 0)
		snprintf(buf, sizeof(buf), "%ud %uh %um %us", days, hours, mins, secs);
	else if (hours > 0)
		snprintf(buf, sizeof(buf), "%uh %um %us", hours, mins, secs);
	else
		snprintf(buf, sizeof(buf), "%um %us", mins, secs);
	fUptimeView->SetValue(buf);

	// Battery
	if (fCoreStats.batteryMv > 0) {
		int32 pct = BatteryPercent(fCoreStats.batteryMv);
		snprintf(buf, sizeof(buf), "%u mV (%d%%)",
			(unsigned)fCoreStats.batteryMv, (int)pct);
		rgb_color battColor;
		if (fCoreStats.batteryMv >= kBattGoodMv)
			battColor = kColorGood;
		else if (fCoreStats.batteryMv >= kBattFairMv)
			battColor = kColorFair;
		else
			battColor = kColorBad;
		fBatteryView->SetValue(buf, battColor);
	}

	// === Radio Performance ===
	// RSSI
	snprintf(buf, sizeof(buf), "%d dBm", (int)fRadioStats.lastRssi);
	rgb_color rssiColor;
	if (fRadioStats.lastRssi >= kRssiGood)
		rssiColor = kColorGood;
	else if (fRadioStats.lastRssi >= kRssiPoor)
		rssiColor = kColorFair;
	else
		rssiColor = kColorBad;
	fRssiView->SetValue(buf, rssiColor);

	// SNR (V3: direct int8, NOT divided by 4)
	snprintf(buf, sizeof(buf), "%+d dB", (int)fRadioStats.lastSnr);
	rgb_color snrColor;
	if (fRadioStats.lastSnr >= kSnrExcellent)
		snrColor = kColorGood;
	else if (fRadioStats.lastSnr >= kSnrGood)
		snrColor = kColorFair;
	else
		snrColor = kColorBad;
	fSnrView->SetValue(buf, snrColor);

	// Noise Floor
	snprintf(buf, sizeof(buf), "%d dBm", (int)fRadioStats.noiseFloor);
	fNoiseFloorView->SetValue(buf);

	// TX Air Time
	if (fRadioStats.txAirTimeMs >= 60000)
		snprintf(buf, sizeof(buf), "%.1f min",
			fRadioStats.txAirTimeMs / 60000.0);
	else
		snprintf(buf, sizeof(buf), "%.1f s",
			fRadioStats.txAirTimeMs / 1000.0);
	fTxAirTimeView->SetValue(buf);

	// RX Air Time
	if (fRadioStats.rxAirTimeMs >= 60000)
		snprintf(buf, sizeof(buf), "%.1f min",
			fRadioStats.rxAirTimeMs / 60000.0);
	else
		snprintf(buf, sizeof(buf), "%.1f s",
			fRadioStats.rxAirTimeMs / 1000.0);
	fRxAirTimeView->SetValue(buf);

	// === Packet Statistics ===
	snprintf(buf, sizeof(buf), "%u", (unsigned)fPacketStats.recvPackets);
	fRecvPacketsView->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", (unsigned)fPacketStats.sentPackets);
	fSentPacketsView->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", (unsigned)fPacketStats.sentFlood);
	fSentFloodView->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", (unsigned)fPacketStats.sentDirect);
	fSentDirectView->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", (unsigned)fPacketStats.recvFlood);
	fRecvFloodView->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", (unsigned)fPacketStats.recvDirect);
	fRecvDirectView->SetValue(buf);
}
