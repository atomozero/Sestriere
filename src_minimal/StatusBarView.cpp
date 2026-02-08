/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatusBarView.cpp — Rich status bar implementation
 */

#include "StatusBarView.h"

#include <Font.h>
#include <Window.h>

#include <cstdio>


// Layout constants
static const float kStatusBarHeight = 24.0f;
static const float kSectionPadding = 12.0f;
static const float kSeparatorWidth = 1.0f;

// Colors
static const rgb_color kBackgroundColor = {48, 48, 48, 255};
static const rgb_color kLabelColor = {160, 160, 160, 255};
static const rgb_color kValueColor = {220, 220, 220, 255};
static const rgb_color kConnectedColor = {100, 200, 100, 255};
static const rgb_color kDisconnectedColor = {200, 100, 100, 255};
static const rgb_color kSeparatorColor = {80, 80, 80, 255};

// Battery colors
static const rgb_color kBatteryGood = {100, 200, 100, 255};
static const rgb_color kBatteryMedium = {220, 180, 60, 255};
static const rgb_color kBatteryLow = {220, 100, 60, 255};
static const rgb_color kBatteryCritical = {200, 60, 60, 255};

// Signal colors
static const rgb_color kSignalExcellent = {100, 200, 100, 255};
static const rgb_color kSignalGood = {180, 200, 100, 255};
static const rgb_color kSignalFair = {220, 180, 60, 255};
static const rgb_color kSignalPoor = {220, 120, 60, 255};
static const rgb_color kSignalBad = {200, 60, 60, 255};


StatusBarView::StatusBarView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fConnected(false),
	fPortName(""),
	fBatteryMv(0),
	fBatteryPercent(-1),
	fLastRssi(0),
	fLastSnr(0),
	fTxPackets(0),
	fRxPackets(0),
	fUptime(0),
	fRawPackets(0),
	fMqttConnected(false)
{
	SetViewColor(kBackgroundColor);
	SetLowColor(kBackgroundColor);
	SetExplicitMinSize(BSize(B_SIZE_UNSET, kStatusBarHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kStatusBarHeight));
}


StatusBarView::~StatusBarView()
{
}


void
StatusBarView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background
	SetHighColor(kBackgroundColor);
	FillRect(bounds);

	// Get font metrics
	BFont font;
	GetFont(&font);
	font.SetSize(11);
	SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);
	float textY = (bounds.Height() + fh.ascent - fh.descent) / 2;

	float x = kSectionPadding;
	char buf[64];

	// === Connection Status ===
	if (fConnected) {
		SetHighColor(kConnectedColor);
		snprintf(buf, sizeof(buf), "Connected: %s", fPortName.String());
	} else {
		SetHighColor(kDisconnectedColor);
		snprintf(buf, sizeof(buf), "Disconnected");
	}
	DrawString(buf, BPoint(x, textY));
	x += StringWidth(buf) + kSectionPadding;

	// Separator
	SetHighColor(kSeparatorColor);
	StrokeLine(BPoint(x, 4), BPoint(x, bounds.Height() - 4));
	x += kSeparatorWidth + kSectionPadding;

	// === Battery ===
	if (fBatteryMv > 0) {
		SetHighColor(kLabelColor);
		DrawString("Batt:", BPoint(x, textY));
		x += StringWidth("Batt:") + 4;

		SetHighColor(_BatteryColor(fBatteryMv));
		snprintf(buf, sizeof(buf), "%umV", fBatteryMv);
		DrawString(buf, BPoint(x, textY));
		x += StringWidth(buf) + kSectionPadding;

		// Storage (separate from battery)
		if (fBatteryPercent >= 0) {
			SetHighColor(kLabelColor);
			DrawString("Mem:", BPoint(x, textY));
			x += StringWidth("Mem:") + 4;

			SetHighColor(kValueColor);
			snprintf(buf, sizeof(buf), "%d%%", fBatteryPercent);
			DrawString(buf, BPoint(x, textY));
			x += StringWidth(buf) + kSectionPadding;
		}

		// Separator
		SetHighColor(kSeparatorColor);
		StrokeLine(BPoint(x, 4), BPoint(x, bounds.Height() - 4));
		x += kSeparatorWidth + kSectionPadding;
	}

	// === Radio Stats ===
	if (fConnected) {
		// RSSI
		SetHighColor(kLabelColor);
		DrawString("RSSI:", BPoint(x, textY));
		x += StringWidth("RSSI:") + 4;

		SetHighColor(kValueColor);
		snprintf(buf, sizeof(buf), "%ddBm", fLastRssi);
		DrawString(buf, BPoint(x, textY));
		x += StringWidth(buf) + kSectionPadding;

		// SNR
		SetHighColor(kLabelColor);
		DrawString("SNR:", BPoint(x, textY));
		x += StringWidth("SNR:") + 4;

		float snrDb = fLastSnr / 4.0f;
		SetHighColor(_SignalColor(fLastSnr));
		snprintf(buf, sizeof(buf), "%.1fdB", snrDb);
		DrawString(buf, BPoint(x, textY));
		x += StringWidth(buf) + kSectionPadding;

		// Separator
		SetHighColor(kSeparatorColor);
		StrokeLine(BPoint(x, 4), BPoint(x, bounds.Height() - 4));
		x += kSeparatorWidth + kSectionPadding;

		// TX/RX Packets
		SetHighColor(kLabelColor);
		DrawString("TX:", BPoint(x, textY));
		x += StringWidth("TX:") + 4;

		SetHighColor(kValueColor);
		snprintf(buf, sizeof(buf), "%u", fTxPackets);
		DrawString(buf, BPoint(x, textY));
		x += StringWidth(buf) + kSectionPadding;

		SetHighColor(kLabelColor);
		DrawString("RX:", BPoint(x, textY));
		x += StringWidth("RX:") + 4;

		SetHighColor(kValueColor);
		snprintf(buf, sizeof(buf), "%u", fRxPackets);
		DrawString(buf, BPoint(x, textY));
		x += StringWidth(buf) + kSectionPadding;

		// Raw packets if any
		if (fRawPackets > 0) {
			SetHighColor(kLabelColor);
			DrawString("Raw:", BPoint(x, textY));
			x += StringWidth("Raw:") + 4;

			SetHighColor(kValueColor);
			snprintf(buf, sizeof(buf), "%u", fRawPackets);
			DrawString(buf, BPoint(x, textY));
			x += StringWidth(buf) + kSectionPadding;
		}
	}

	// === Right-aligned section: MQTT status and Uptime ===
	float rightX = bounds.right - kSectionPadding;

	// MQTT status (rightmost)
	const char* mqttStatus = fMqttConnected ? "MQTT: ON" : "MQTT: OFF";
	float mqttWidth = StringWidth(mqttStatus);
	rightX -= mqttWidth;
	SetHighColor(fMqttConnected ? kConnectedColor : kDisconnectedColor);
	DrawString(mqttStatus, BPoint(rightX, textY));
	rightX -= kSectionPadding;

	// Uptime (before MQTT)
	if (fUptime > 0) {
		uint32 hours = fUptime / 3600;
		uint32 mins = (fUptime % 3600) / 60;
		uint32 secs = fUptime % 60;
		snprintf(buf, sizeof(buf), "Up: %u:%02u:%02u", hours, mins, secs);

		float uptimeWidth = StringWidth(buf);
		rightX -= uptimeWidth;
		SetHighColor(kLabelColor);
		DrawString(buf, BPoint(rightX, textY));
		rightX -= kSectionPadding;
	}
}


void
StatusBarView::GetPreferredSize(float* width, float* height)
{
	if (width != NULL)
		*width = 600;
	if (height != NULL)
		*height = kStatusBarHeight;
}


void
StatusBarView::SetConnected(bool connected, const char* port)
{
	fConnected = connected;
	if (port != NULL)
		fPortName = port;
	else
		fPortName = "";
	Invalidate();
}


void
StatusBarView::SetBattery(uint16 milliVolts, int8 percent)
{
	fBatteryMv = milliVolts;
	fBatteryPercent = percent;
	Invalidate();
}


void
StatusBarView::SetRadioStats(int8 rssi, int8 snr, uint32 txPackets, uint32 rxPackets)
{
	fLastRssi = rssi;
	fLastSnr = snr;
	fTxPackets = txPackets;
	fRxPackets = rxPackets;
	Invalidate();
}


void
StatusBarView::SetUptime(uint32 seconds)
{
	fUptime = seconds;
	Invalidate();
}


void
StatusBarView::SetRawPackets(uint32 count)
{
	fRawPackets = count;
	Invalidate();
}


void
StatusBarView::SetMqttConnected(bool connected)
{
	fMqttConnected = connected;
	Invalidate();
}


rgb_color
StatusBarView::_BatteryColor(uint16 milliVolts)
{
	// Typical LiPo battery ranges:
	// 4200mV = 100%, 3700mV = ~50%, 3400mV = ~20%, 3200mV = critical
	if (milliVolts >= 3900)
		return kBatteryGood;
	else if (milliVolts >= 3600)
		return kBatteryMedium;
	else if (milliVolts >= 3400)
		return kBatteryLow;
	else
		return kBatteryCritical;
}


rgb_color
StatusBarView::_SignalColor(int8 snr)
{
	// SNR is stored as value * 4
	float snrDb = snr / 4.0f;

	if (snrDb >= 10.0f)
		return kSignalExcellent;
	else if (snrDb >= 5.0f)
		return kSignalGood;
	else if (snrDb >= 0.0f)
		return kSignalFair;
	else if (snrDb >= -5.0f)
		return kSignalPoor;
	else
		return kSignalBad;
}
