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


static const uint32 MSG_REFRESH_STATS = 'rfst';
static const uint32 MSG_STATS_WIN_TIMER = 'swtm';
static const bigtime_t kStatsWinRefreshInterval = 5000000;  // 5 seconds

// Colors for status indicators
static const rgb_color kGoodColor = {60, 180, 60, 255};
static const rgb_color kWarningColor = {220, 160, 40, 255};
static const rgb_color kBadColor = {200, 60, 60, 255};
static const rgb_color kLabelColor = {80, 80, 80, 255};
static const rgb_color kValueColor = {0, 0, 0, 255};
static const rgb_color kHeaderColor = {40, 80, 120, 255};


// Helper to read uint32 little-endian
static uint32
ReadU32LE(const uint8* data)
{
	return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}


// Custom view for a single stat row with label and value
class StatRowView : public BView {
public:
	StatRowView(const char* label, const char* initialValue = "--")
		:
		BView(label, B_WILL_DRAW),
		fLabel(label),
		fValue(initialValue),
		fValueColor(kValueColor)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}

	void SetValue(const char* value, rgb_color color = kValueColor)
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

		// Draw label (left aligned, gray)
		SetHighColor(kLabelColor);
		DrawString(fLabel.String(), BPoint(0, baseline));

		// Draw value (right aligned, colored)
		float valueWidth = StringWidth(fValue.String());
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

		// Draw icon if present
		float x = 0;
		if (fIcon.Length() > 0) {
			SetHighColor(kHeaderColor);
			DrawString(fIcon.String(), BPoint(x, baseline));
			x += StringWidth(fIcon.String()) + 8;
		}

		// Draw title
		SetHighColor(kHeaderColor);
		DrawString(fTitle.String(), BPoint(x, baseline));

		// Draw underline
		float lineY = bounds.bottom - 2;
		SetHighColor(tint_color(kHeaderColor, B_LIGHTEN_2_TINT));
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
	fTxPacketsView(NULL),
	fRxPacketsView(NULL),
	fTxBytesView(NULL),
	fRxBytesView(NULL),
	fRoutedView(NULL),
	fDroppedView(NULL),
	fRssiView(NULL),
	fSnrView(NULL),
	fTxTimeView(NULL),
	fRxTimeView(NULL),
	fChannelBusyView(NULL),
	fCrcErrorsView(NULL),
	fAdvertsSentView(NULL),
	fAdvertsReceivedView(NULL),
	fMsgsSentView(NULL),
	fMsgsReceivedView(NULL),
	fAcksSentView(NULL),
	fAcksReceivedView(NULL),
	fRefreshButton(NULL),
	fCloseButton(NULL),
	fRefreshTimer(NULL)
{
	// Create stat row views
	fUptimeView = new StatRowView("Uptime:");
	fTxPacketsView = new StatRowView("TX Packets:");
	fRxPacketsView = new StatRowView("RX Packets:");
	fTxBytesView = new StatRowView("TX Bytes:");
	fRxBytesView = new StatRowView("RX Bytes:");
	fRoutedView = new StatRowView("Routed:");
	fDroppedView = new StatRowView("Dropped:");

	fRssiView = new StatRowView("Signal (RSSI):");
	fSnrView = new StatRowView("SNR:");
	fTxTimeView = new StatRowView("TX Time:");
	fRxTimeView = new StatRowView("RX Time:");
	fChannelBusyView = new StatRowView("Channel Busy:");
	fCrcErrorsView = new StatRowView("CRC Errors:");

	fAdvertsSentView = new StatRowView("Adverts Sent:");
	fAdvertsReceivedView = new StatRowView("Adverts Received:");
	fMsgsSentView = new StatRowView("Messages Sent:");
	fMsgsReceivedView = new StatRowView("Messages Received:");
	fAcksSentView = new StatRowView("ACKs Sent:");
	fAcksReceivedView = new StatRowView("ACKs Received:");

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
		.AddStrut(12)

		// === Traffic Section ===
		.Add(new SectionHeaderView("Network Traffic"))
		.AddStrut(4)
		.Add(fTxPacketsView)
		.Add(fRxPacketsView)
		.Add(fTxBytesView)
		.Add(fRxBytesView)
		.Add(fRoutedView)
		.Add(fDroppedView)
		.AddStrut(12)

		// === Radio Section ===
		.Add(new SectionHeaderView("Radio Performance"))
		.AddStrut(4)
		.Add(fRssiView)
		.Add(fSnrView)
		.Add(fTxTimeView)
		.Add(fRxTimeView)
		.Add(fChannelBusyView)
		.Add(fCrcErrorsView)
		.AddStrut(12)

		// === Messages Section ===
		.Add(new SectionHeaderView("Message Statistics"))
		.AddStrut(4)
		.Add(fAdvertsSentView)
		.Add(fAdvertsReceivedView)
		.Add(fMsgsSentView)
		.Add(fMsgsReceivedView)
		.Add(fAcksSentView)
		.Add(fAcksReceivedView)

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
		case 0:  // Core stats (11 bytes)
			// [2-3]=batt_mv(int16) [4-7]=uptime(uint32) [8-9]=err_flags [10]=queue_len
			if (length >= 8) {
				fCoreStats.uptime = ReadU32LE(data + 4);
			}
			break;

		case 1:  // Radio stats (14 bytes)
			// [2-3]=noise_floor(int16) [4]=rssi(int8) [5]=snr(int8)
			// [6-9]=tx_air_time(uint32) [10-13]=rx_air_time(uint32)
			if (length >= 6) {
				fRadioStats.lastRssi = (int8)data[4];
				fRadioStats.lastSnr = (int8)data[5];
			}
			if (length >= 14) {
				fRadioStats.txTimeMs = ReadU32LE(data + 6);
				fRadioStats.rxTimeMs = ReadU32LE(data + 10);
			}
			break;

		case 2:  // Packet stats (26 bytes)
			// [2-5]=recvPkts [6-9]=sentPkts [10-13]=sentFlood [14-17]=sentDirect
			// [18-21]=recvFlood [22-25]=recvDirect
			if (length >= 10) {
				fCoreStats.rxPackets = ReadU32LE(data + 2);
				fCoreStats.txPackets = ReadU32LE(data + 6);
			}
			if (length >= 18) {
				fCoreStats.txBytes = ReadU32LE(data + 10);
				fCoreStats.rxBytes = ReadU32LE(data + 14);
			}
			if (length >= 26) {
				fCoreStats.routedPackets = ReadU32LE(data + 18);
				fCoreStats.droppedPackets = ReadU32LE(data + 22);
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
	((StatRowView*)fUptimeView)->SetValue(buf);

	// Traffic stats
	snprintf(buf, sizeof(buf), "%u", fCoreStats.txPackets);
	((StatRowView*)fTxPacketsView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fCoreStats.rxPackets);
	((StatRowView*)fRxPacketsView)->SetValue(buf);

	// Format bytes nicely
	if (fCoreStats.txBytes >= 1048576)
		snprintf(buf, sizeof(buf), "%.1f MB", fCoreStats.txBytes / 1048576.0);
	else if (fCoreStats.txBytes >= 1024)
		snprintf(buf, sizeof(buf), "%.1f KB", fCoreStats.txBytes / 1024.0);
	else
		snprintf(buf, sizeof(buf), "%u B", fCoreStats.txBytes);
	((StatRowView*)fTxBytesView)->SetValue(buf);

	if (fCoreStats.rxBytes >= 1048576)
		snprintf(buf, sizeof(buf), "%.1f MB", fCoreStats.rxBytes / 1048576.0);
	else if (fCoreStats.rxBytes >= 1024)
		snprintf(buf, sizeof(buf), "%.1f KB", fCoreStats.rxBytes / 1024.0);
	else
		snprintf(buf, sizeof(buf), "%u B", fCoreStats.rxBytes);
	((StatRowView*)fRxBytesView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fCoreStats.routedPackets);
	((StatRowView*)fRoutedView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fCoreStats.droppedPackets);
	rgb_color dropColor = fCoreStats.droppedPackets > 0 ? kWarningColor : kValueColor;
	((StatRowView*)fDroppedView)->SetValue(buf, dropColor);

	// Radio stats with color coding
	snprintf(buf, sizeof(buf), "%d dBm", fRadioStats.lastRssi);
	rgb_color rssiColor;
	if (fRadioStats.lastRssi >= -70)
		rssiColor = kGoodColor;
	else if (fRadioStats.lastRssi >= -90)
		rssiColor = kWarningColor;
	else
		rssiColor = kBadColor;
	((StatRowView*)fRssiView)->SetValue(buf, rssiColor);

	float snrDb = fRadioStats.lastSnr / 4.0f;
	snprintf(buf, sizeof(buf), "%.1f dB", snrDb);
	rgb_color snrColor;
	if (snrDb >= 10.0f)
		snrColor = kGoodColor;
	else if (snrDb >= 0.0f)
		snrColor = kWarningColor;
	else
		snrColor = kBadColor;
	((StatRowView*)fSnrView)->SetValue(buf, snrColor);

	if (fRadioStats.txTimeMs >= 60000)
		snprintf(buf, sizeof(buf), "%.1f min", fRadioStats.txTimeMs / 60000.0);
	else
		snprintf(buf, sizeof(buf), "%.1f s", fRadioStats.txTimeMs / 1000.0);
	((StatRowView*)fTxTimeView)->SetValue(buf);

	if (fRadioStats.rxTimeMs >= 60000)
		snprintf(buf, sizeof(buf), "%.1f min", fRadioStats.rxTimeMs / 60000.0);
	else
		snprintf(buf, sizeof(buf), "%.1f s", fRadioStats.rxTimeMs / 1000.0);
	((StatRowView*)fRxTimeView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u%%", fRadioStats.channelBusy);
	rgb_color busyColor;
	if (fRadioStats.channelBusy <= 30)
		busyColor = kGoodColor;
	else if (fRadioStats.channelBusy <= 70)
		busyColor = kWarningColor;
	else
		busyColor = kBadColor;
	((StatRowView*)fChannelBusyView)->SetValue(buf, busyColor);

	snprintf(buf, sizeof(buf), "%u", fRadioStats.crcErrors);
	rgb_color crcColor = fRadioStats.crcErrors > 0 ? kBadColor : kValueColor;
	((StatRowView*)fCrcErrorsView)->SetValue(buf, crcColor);

	// Message stats
	snprintf(buf, sizeof(buf), "%u", fPacketStats.advertsSent);
	((StatRowView*)fAdvertsSentView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fPacketStats.advertsReceived);
	((StatRowView*)fAdvertsReceivedView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fPacketStats.messagesSent);
	((StatRowView*)fMsgsSentView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fPacketStats.messagesReceived);
	((StatRowView*)fMsgsReceivedView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fPacketStats.acksSent);
	((StatRowView*)fAcksSentView)->SetValue(buf);

	snprintf(buf, sizeof(buf), "%u", fPacketStats.acksReceived);
	((StatRowView*)fAcksReceivedView)->SetValue(buf);
}
