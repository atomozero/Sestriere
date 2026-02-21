/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TopBarView.cpp — Toolbar with map icons and status indicators
 */

#include "TopBarView.h"

#include <Application.h>
#include <ControlLook.h>
#include <Font.h>
#include <IconUtils.h>
#include <MessageRunner.h>
#include <OS.h>
#include <Resources.h>
#include <Window.h>

#include <cmath>
#include <cstdio>

#include "Constants.h"


static const uint32 MSG_LED_OFF = 'loff';
static const bigtime_t kLedFlashDuration = 300000;  // 300ms

static const float kPadding = 8.0f;
static const float kIconSize = 16.0f;
static const float kIconSpacing = 8.0f;
static const float kBarHeight = 24.0f;

// Hit-test area indices
enum {
	kAreaNone = -1,
	kAreaNetworkMap = 0,
	kAreaGeoMap,
	kAreaStats,
	kAreaTelemetry,
	kAreaPacketAnalyzer,
	kAreaDebugLog,
	kAreaMissionControl,
	kAreaMqttToggle,
	kAreaMqttLog,
	kAreaConnectionDot,
	kAreaBattery,
	kAreaRssi,
	kAreaTxRx,
	kAreaUptime,
};

// Battery color by voltage
static inline rgb_color
BatteryColor(uint16 mv)
{
	if (mv >= kBattGoodMv) return kColorGood;
	if (mv >= kBattFairMv) return kColorFair;
	if (mv >= kBattLowMv) return kColorPoor;
	return kColorBad;
}

// Signal color by RSSI
static inline rgb_color
SignalColor(int8 rssi)
{
	if (rssi >= kRssiGood) return kColorGood;
	if (rssi >= kRssiFair) return kColorFair;
	if (rssi >= kRssiPoor) return kColorPoor;
	return kColorBad;
}


TopBarView::TopBarView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fConnected(false),
	fPortName(""),
	fBatteryMv(0),
	fRssi(0),
	fSnr(0),
	fTxPackets(0),
	fRxPackets(0),
	fUptime(0),
	fMqttConnected(false),
	fMqttEnabled(false),
	fHoverArea(-1),
	fTxFlashTime(0),
	fRxFlashTime(0),
	fMapsIcon(NULL),
	fEarthIcon(NULL)
{
	SetViewUIColor(B_MENU_BACKGROUND_COLOR);
	SetExplicitMinSize(BSize(350, kBarHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kBarHeight));
	_LoadIcons();
}


TopBarView::~TopBarView()
{
	delete fMapsIcon;
	delete fEarthIcon;
}


void
TopBarView::AttachedToWindow()
{
	BView::AttachedToWindow();
}


BSize
TopBarView::MinSize()
{
	return BSize(350, kBarHeight);
}


BSize
TopBarView::PreferredSize()
{
	return BSize(B_SIZE_UNLIMITED, kBarHeight);
}


void
TopBarView::_LoadIcons()
{
	BResources* res = BApplication::AppResources();
	if (res == NULL)
		return;

	int32 iconSize = (int32)kIconSize;

	// Load "Maps" HVIF icon (resource ID 1)
	size_t dataSize;
	const void* data = res->LoadResource('VICN', 1, &dataSize);
	if (data != NULL) {
		fMapsIcon = new BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1),
			B_RGBA32);
		if (BIconUtils::GetVectorIcon((const uint8*)data, dataSize,
				fMapsIcon) != B_OK) {
			delete fMapsIcon;
			fMapsIcon = NULL;
		}
	}

	// Load "Earth" HVIF icon (resource ID 2)
	data = res->LoadResource('VICN', 2, &dataSize);
	if (data != NULL) {
		fEarthIcon = new BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1),
			B_RGBA32);
		if (BIconUtils::GetVectorIcon((const uint8*)data, dataSize,
				fEarthIcon) != B_OK) {
			delete fEarthIcon;
			fEarthIcon = NULL;
		}
	}
}


void
TopBarView::_DrawNetworkMapIcon(BPoint center)
{
	// 4 nodes in diamond pattern connected by lines
	float r = 6.0f;
	BPoint top(center.x, center.y - r);
	BPoint right(center.x + r, center.y);
	BPoint bottom(center.x, center.y + r);
	BPoint left(center.x - r, center.y);

	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	StrokeLine(top, right);
	StrokeLine(right, bottom);
	StrokeLine(bottom, left);
	StrokeLine(left, top);
	StrokeLine(top, bottom);
	StrokeLine(left, right);

	float dotR = 2.0f;
	FillEllipse(top, dotR, dotR);
	FillEllipse(right, dotR, dotR);
	FillEllipse(bottom, dotR, dotR);
	FillEllipse(left, dotR, dotR);
}


void
TopBarView::_DrawGeoMapIcon(BPoint center)
{
	// Globe: circle + equator line + meridian ellipse
	float r = 7.0f;
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	StrokeEllipse(center, r, r);
	StrokeLine(BPoint(center.x - r, center.y),
		BPoint(center.x + r, center.y));
	StrokeEllipse(center, r * 0.4f, r);
}


void
TopBarView::_DrawStatsIcon(BPoint center)
{
	// Bar chart: 3 vertical bars of different heights
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	float barW = 3.0f;
	float gap = 1.5f;
	float baseY = center.y + 6.0f;
	float startX = center.x - (barW * 3 + gap * 2) / 2;

	// Bar 1 (short)
	FillRect(BRect(startX, baseY - 5, startX + barW, baseY));
	startX += barW + gap;
	// Bar 2 (tall)
	FillRect(BRect(startX, baseY - 10, startX + barW, baseY));
	startX += barW + gap;
	// Bar 3 (medium)
	FillRect(BRect(startX, baseY - 7, startX + barW, baseY));
}


void
TopBarView::_DrawTelemetryIcon(BPoint center)
{
	// Oscilloscope wave (sine-like squiggle)
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	SetPenSize(1.5f);
	float startX = center.x - 7.0f;
	float amplitude = 4.0f;
	BPoint prev(startX, center.y);
	for (int i = 1; i <= 14; i++) {
		float px = startX + i;
		float py = center.y - amplitude * sinf(i * 0.9f);
		StrokeLine(prev, BPoint(px, py));
		prev.Set(px, py);
	}
	SetPenSize(1.0f);
}


void
TopBarView::_DrawPacketAnalyzerIcon(BPoint center)
{
	// Horizontal lines like a list/packet view
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	float w = 10.0f;
	float left = center.x - w / 2;
	float right = center.x + w / 2;
	float y = center.y - 5.0f;
	for (int i = 0; i < 4; i++) {
		StrokeLine(BPoint(left, y), BPoint(right, y));
		y += 3.0f;
	}
}


void
TopBarView::_DrawDebugLogIcon(BPoint center)
{
	// Terminal prompt: >_
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	BFont font;
	GetFont(&font);
	font.SetSize(11);
	SetFont(&font);
	font_height fh;
	font.GetHeight(&fh);
	float textY = center.y + (fh.ascent - fh.descent) / 2;
	float textW = StringWidth(">_");
	DrawString(">_", BPoint(center.x - textW / 2, textY));
}


void
TopBarView::_DrawMissionControlIcon(BPoint center)
{
	// Dashboard: 4-quadrant grid
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	float s = 6.0f;  // half-size
	float gap = 1.0f;

	// Top-left quadrant
	StrokeRect(BRect(center.x - s, center.y - s,
		center.x - gap, center.y - gap));
	// Top-right quadrant
	StrokeRect(BRect(center.x + gap, center.y - s,
		center.x + s, center.y - gap));
	// Bottom-left quadrant
	StrokeRect(BRect(center.x - s, center.y + gap,
		center.x - gap, center.y + s));
	// Bottom-right quadrant
	StrokeRect(BRect(center.x + gap, center.y + gap,
		center.x + s, center.y + s));
}


void
TopBarView::_DrawMqttToggle(BRect rect)
{
	rgb_color bg = ui_color(B_MENU_BACKGROUND_COLOR);

	if (fMqttConnected) {
		// Green filled pill
		SetHighColor(kColorGood);
		FillRoundRect(rect, 4, 4);
		SetHighColor(255, 255, 255, 255);
	} else if (fMqttEnabled) {
		// Outlined pill (enabled but not connected)
		rgb_color outline = tint_color(bg, B_DARKEN_3_TINT);
		SetHighColor(outline);
		StrokeRoundRect(rect, 4, 4);
		SetHighColor(ui_color(B_MENU_ITEM_TEXT_COLOR));
	} else {
		// Gray filled pill (disabled)
		rgb_color gray = tint_color(bg, B_DARKEN_1_TINT);
		SetHighColor(gray);
		FillRoundRect(rect, 4, 4);
		SetHighColor(tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
			B_LIGHTEN_2_TINT));
	}

	BFont font;
	GetFont(&font);
	font.SetSize(9);
	SetFont(&font);
	font_height fh;
	font.GetHeight(&fh);
	float textY = rect.top + (rect.Height() + fh.ascent - fh.descent) / 2;
	float textW = StringWidth("MQTT");
	DrawString("MQTT", BPoint(rect.left + (rect.Width() - textW) / 2, textY));

	// Restore font size
	font.SetSize(11);
	SetFont(&font);
}


void
TopBarView::_DrawMqttLogIcon(BPoint center)
{
	// Speech bubble icon
	rgb_color iconColor = tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(iconColor);

	// Bubble body
	BRect bubble(center.x - 6, center.y - 5, center.x + 6, center.y + 2);
	StrokeRoundRect(bubble, 3, 3);

	// Tail (small triangle below)
	StrokeLine(BPoint(center.x - 2, center.y + 2),
		BPoint(center.x - 4, center.y + 5));
	StrokeLine(BPoint(center.x - 4, center.y + 5),
		BPoint(center.x + 1, center.y + 2));
}


void
TopBarView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	rgb_color bg = ui_color(B_MENU_BACKGROUND_COLOR);
	rgb_color textColor = ui_color(B_MENU_ITEM_TEXT_COLOR);
	rgb_color dimColor = tint_color(textColor, B_LIGHTEN_1_TINT);

	// Background
	SetLowColor(bg);
	FillRect(bounds, B_SOLID_LOW);

	// Bottom border
	SetHighColor(tint_color(bg, B_DARKEN_2_TINT));
	StrokeLine(BPoint(bounds.left, bounds.bottom),
		BPoint(bounds.right, bounds.bottom));

	BFont font;
	GetFont(&font);
	font.SetSize(11);
	SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);
	float textY = (bounds.Height() + fh.ascent - fh.descent) / 2;
	float iconY = bounds.Height() / 2;

	// Hover highlight color (theme-aware)
	rgb_color hoverColor = tint_color(bg, B_DARKEN_1_TINT);

	// === LEFT: Icon buttons ===
	float x = kPadding;

	// Network Map icon (HVIF or fallback)
	float iconCenterX = x + kIconSize / 2;
	fNetworkMapRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaNetworkMap) {
		SetHighColor(hoverColor);
		FillRoundRect(fNetworkMapRect, 3, 3);
	}
	if (fMapsIcon != NULL) {
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fMapsIcon,
			BPoint(x, iconY - kIconSize / 2));
		SetDrawingMode(B_OP_COPY);
	} else {
		_DrawNetworkMapIcon(BPoint(iconCenterX, iconY));
	}
	x += kIconSize + kIconSpacing;

	// Geographic Map icon (HVIF or fallback)
	iconCenterX = x + kIconSize / 2;
	fGeoMapRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaGeoMap) {
		SetHighColor(hoverColor);
		FillRoundRect(fGeoMapRect, 3, 3);
	}
	if (fEarthIcon != NULL) {
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fEarthIcon,
			BPoint(x, iconY - kIconSize / 2));
		SetDrawingMode(B_OP_COPY);
	} else {
		_DrawGeoMapIcon(BPoint(iconCenterX, iconY));
	}
	x += kIconSize + kIconSpacing;

	// Stats icon
	iconCenterX = x + kIconSize / 2;
	fStatsRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaStats) {
		SetHighColor(hoverColor);
		FillRoundRect(fStatsRect, 3, 3);
	}
	_DrawStatsIcon(BPoint(iconCenterX, iconY));
	x += kIconSize + kIconSpacing;

	// Telemetry icon
	iconCenterX = x + kIconSize / 2;
	fTelemetryRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaTelemetry) {
		SetHighColor(hoverColor);
		FillRoundRect(fTelemetryRect, 3, 3);
	}
	_DrawTelemetryIcon(BPoint(iconCenterX, iconY));
	x += kIconSize + kIconSpacing;

	// Packet Analyzer icon
	iconCenterX = x + kIconSize / 2;
	fPacketAnalyzerRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaPacketAnalyzer) {
		SetHighColor(hoverColor);
		FillRoundRect(fPacketAnalyzerRect, 3, 3);
	}
	_DrawPacketAnalyzerIcon(BPoint(iconCenterX, iconY));
	x += kIconSize + kIconSpacing;

	// Debug Log icon
	iconCenterX = x + kIconSize / 2;
	fDebugLogRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaDebugLog) {
		SetHighColor(hoverColor);
		FillRoundRect(fDebugLogRect, 3, 3);
	}
	_DrawDebugLogIcon(BPoint(iconCenterX, iconY));
	x += kIconSize + kIconSpacing;

	// Mission Control icon
	iconCenterX = x + kIconSize / 2;
	fMissionControlRect.Set(x, 0, x + kIconSize, bounds.bottom);
	if (fHoverArea == kAreaMissionControl) {
		SetHighColor(hoverColor);
		FillRoundRect(fMissionControlRect, 3, 3);
	}
	_DrawMissionControlIcon(BPoint(iconCenterX, iconY));
	x += kIconSize + kIconSpacing;

	// Vertical separator after tool icons
	rgb_color sepColor = tint_color(bg, B_DARKEN_2_TINT);
	SetHighColor(sepColor);
	StrokeLine(BPoint(x, 4), BPoint(x, bounds.bottom - 4));
	x += kIconSpacing;

	// Reset status indicator rects (may not be drawn if window too narrow)
	fConnectionDotRect = BRect();
	fBatteryRect = BRect();
	fRssiRect = BRect();
	fTxRxRect = BRect();
	fUptimeRect = BRect();

	// Connection status dot
	if (fConnected) {
		SetHighColor(80, 180, 80, 255);
	} else {
		SetHighColor(200, 60, 60, 255);
	}
	BRect dotRect(x, iconY - 3, x + 6, iconY + 3);
	FillEllipse(dotRect);
	fConnectionDotRect.Set(x - 2, 0, x + 8, bounds.bottom);
	x += 10;

	// TX/RX activity LEDs (modem-style)
	if (fConnected) {
		bigtime_t now = system_time();
		bool txOn = (now - fTxFlashTime) < kLedFlashDuration;
		bool rxOn = (now - fRxFlashTime) < kLedFlashDuration;

		rgb_color ledOff = tint_color(bg, B_DARKEN_2_TINT);

		// TX label
		BFont ledFont;
		GetFont(&ledFont);
		ledFont.SetSize(8);
		SetFont(&ledFont);
		SetHighColor(dimColor);
		DrawString("T", BPoint(x, textY));
		x += ledFont.StringWidth("T") + 2;

		// TX LED (blue)
		if (txOn)
			SetHighColor(100, 180, 255, 255);
		else
			SetHighColor(ledOff);
		BRect txDot(x, iconY - 3.5f, x + 7, iconY + 3.5f);
		FillEllipse(txDot);
		x += 10;

		// RX label
		SetHighColor(dimColor);
		DrawString("R", BPoint(x, textY));
		x += ledFont.StringWidth("R") + 2;

		// RX LED (orange)
		if (rxOn)
			SetHighColor(255, 160, 40, 255);
		else
			SetHighColor(ledOff);
		BRect rxDot(x, iconY - 3.5f, x + 7, iconY + 3.5f);
		FillEllipse(rxDot);
		x += 10;

		// Restore font
		SetFont(&font);
	}

	// === RIGHT: Status indicators (drawn right-to-left) ===
	float rx = bounds.right - kPadding;
	float leftEdge = x + kPadding;  // Minimum boundary for right-side content

	// Uptime (rightmost) — only if space permits
	if (fUptime > 0) {
		char uptimeStr[16];
		uint32 h = fUptime / 3600;
		uint32 m = (fUptime % 3600) / 60;
		snprintf(uptimeStr, sizeof(uptimeStr), "%u:%02u", h, m);

		float w = StringWidth(uptimeStr) + 4 + StringWidth("Up ") + kPadding;
		if (rx - w > leftEdge) {
			float rxStart = rx;
			float uw = StringWidth(uptimeStr);
			rx -= uw;
			SetHighColor(dimColor);
			DrawString(uptimeStr, BPoint(rx, textY));
			rx -= 4;

			float lw = StringWidth("Up ");
			rx -= lw;
			SetHighColor(dimColor);
			DrawString("Up ", BPoint(rx, textY));
			fUptimeRect.Set(rx - 2, 0, rxStart, bounds.bottom);
			rx -= kPadding;
		}
	}

	// MQTT Log icon (to the right of MQTT toggle) — always drawn
	float mqttLogSize = kIconSize;
	rx -= mqttLogSize;
	fMqttLogRect.Set(rx, 0, rx + mqttLogSize, bounds.bottom);
	if (fHoverArea == kAreaMqttLog) {
		SetHighColor(hoverColor);
		FillRoundRect(fMqttLogRect, 3, 3);
	}
	_DrawMqttLogIcon(BPoint(rx + mqttLogSize / 2, iconY));
	rx -= 4;

	// MQTT Toggle pill — always drawn
	float mqttPillW = 38.0f;
	float mqttPillH = 14.0f;
	rx -= mqttPillW;
	fMqttToggleRect.Set(rx, iconY - mqttPillH / 2,
		rx + mqttPillW, iconY + mqttPillH / 2);
	_DrawMqttToggle(fMqttToggleRect);
	rx -= kPadding;

	// Separator before MQTT area
	SetHighColor(sepColor);
	StrokeLine(BPoint(rx, 4), BPoint(rx, bounds.bottom - 4));
	rx -= kPadding;

	// TX/RX counts — only if space permits
	if (fConnected && rx > leftEdge + 80) {
		char pktStr[32];
		snprintf(pktStr, sizeof(pktStr), "\xE2\x96\xB2%u \xE2\x96\xBC%u",
			(unsigned)fTxPackets, (unsigned)fRxPackets);
		float w = StringWidth(pktStr);
		if (rx - w - kPadding > leftEdge) {
			float rxStart = rx;
			rx -= w;
			SetHighColor(dimColor);
			DrawString(pktStr, BPoint(rx, textY));
			fTxRxRect.Set(rx - 2, 0, rxStart, bounds.bottom);
			rx -= kPadding;
		}
	}

	// RSSI/SNR — only if space permits
	if (fConnected && fRssi != 0 && rx > leftEdge + 50) {
		char rssiStr[24];
		snprintf(rssiStr, sizeof(rssiStr), "%ddBm", fRssi);
		float w = StringWidth(rssiStr);
		if (rx - w - kPadding > leftEdge) {
			float rxStart = rx;
			rx -= w;
			SetHighColor(SignalColor(fRssi));
			DrawString(rssiStr, BPoint(rx, textY));
			fRssiRect.Set(rx - 2, 0, rxStart, bounds.bottom);
			rx -= kPadding;
		}
	}

	// Battery — only if space permits
	if (fBatteryMv > 0 && rx > leftEdge + 40) {
		float rxStart = rx;
		char battStr[16];
		snprintf(battStr, sizeof(battStr), "%umV", (unsigned)fBatteryMv);
		float w = StringWidth(battStr);
		rx -= w;
		SetHighColor(BatteryColor(fBatteryMv));
		DrawString(battStr, BPoint(rx, textY));
		rx -= 4;

		// Battery icon (simple rectangle)
		float iconW = 14;
		float iconH = 8;
		rx -= iconW + 2;
		BRect battRect(rx, iconY - iconH / 2, rx + iconW, iconY + iconH / 2);
		SetHighColor(BatteryColor(fBatteryMv));
		StrokeRect(battRect);
		float level = 0;
		if (fBatteryMv >= 4200) level = 1.0f;
		else if (fBatteryMv >= 3300) level = (fBatteryMv - 3300) / 900.0f;
		BRect fillRect = battRect;
		fillRect.InsetBy(1, 1);
		fillRect.right = fillRect.left + fillRect.Width() * level;
		FillRect(fillRect);
		BRect tipRect(battRect.right, iconY - 2, battRect.right + 2, iconY + 2);
		FillRect(tipRect);
		fBatteryRect.Set(rx - 2, 0, rxStart, bounds.bottom);
		rx -= kPadding;
	}
}


void
TopBarView::MouseDown(BPoint where)
{
	if (fNetworkMapRect.Contains(where)) {
		Window()->PostMessage(MSG_SHOW_NETWORK_MAP);
		return;
	}
	if (fGeoMapRect.Contains(where)) {
		Window()->PostMessage(MSG_SHOW_MAP);
		return;
	}
	if (fStatsRect.Contains(where)) {
		Window()->PostMessage(new BMessage(MSG_SHOW_STATS));
		return;
	}
	if (fTelemetryRect.Contains(where)) {
		Window()->PostMessage(new BMessage(MSG_SHOW_TELEMETRY));
		return;
	}
	if (fPacketAnalyzerRect.Contains(where)) {
		Window()->PostMessage(MSG_SHOW_PACKET_ANALYZER);
		return;
	}
	if (fDebugLogRect.Contains(where)) {
		Window()->PostMessage(new BMessage(MSG_SHOW_DEBUG_LOG));
		return;
	}
	if (fMissionControlRect.Contains(where)) {
		Window()->PostMessage(MSG_SHOW_MISSION_CONTROL);
		return;
	}
	if (fMqttToggleRect.Contains(where)) {
		Window()->PostMessage(MSG_MQTT_TOGGLE);
		return;
	}
	if (fMqttLogRect.Contains(where)) {
		Window()->PostMessage(MSG_SHOW_MQTT_LOG);
		return;
	}
	BView::MouseDown(where);
}


void
TopBarView::SetConnected(bool connected, const char* port)
{
	fConnected = connected;
	fPortName = (port != NULL) ? port : "";
	if (!connected) {
		fBatteryMv = 0;
		fRssi = 0;
		fSnr = 0;
		fTxPackets = 0;
		fRxPackets = 0;
		fUptime = 0;
	}
	Invalidate();
}


void
TopBarView::SetBattery(uint16 milliVolts)
{
	fBatteryMv = milliVolts;
	Invalidate();
}


void
TopBarView::SetRadioStats(int8 rssi, int8 snr, uint32 txPkts, uint32 rxPkts)
{
	fRssi = rssi;
	fSnr = snr;
	fTxPackets = txPkts;
	fRxPackets = rxPkts;
	Invalidate();
}


void
TopBarView::SetUptime(uint32 seconds)
{
	fUptime = seconds;
	Invalidate();
}


void
TopBarView::SetMqttStatus(bool connected)
{
	fMqttConnected = connected;
	Invalidate();
}


void
TopBarView::SetMqttEnabled(bool enabled)
{
	fMqttEnabled = enabled;
	Invalidate();
}


void
TopBarView::FlashTx()
{
	fTxFlashTime = system_time();
	Invalidate();
	// Schedule redraw to turn LED off
	BMessage msg(MSG_LED_OFF);
	BMessageRunner::StartSending(BMessenger(this),
		&msg, kLedFlashDuration, 1);
}


void
TopBarView::FlashRx()
{
	fRxFlashTime = system_time();
	Invalidate();
	BMessage msg(MSG_LED_OFF);
	BMessageRunner::StartSending(BMessenger(this),
		&msg, kLedFlashDuration, 1);
}


void
TopBarView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_LED_OFF:
			Invalidate();
			break;
		default:
			BView::MessageReceived(message);
			break;
	}
}


void
TopBarView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	int32 area = kAreaNone;
	if (transit != B_EXITED_VIEW)
		area = _HitArea(where);

	if (area != fHoverArea) {
		fHoverArea = area;
		const char* tip = _ToolTipForArea(area);
		SetToolTip(tip);
		Invalidate();
	}

	BView::MouseMoved(where, transit, dragMessage);
}


int32
TopBarView::_HitArea(BPoint where) const
{
	if (fNetworkMapRect.Contains(where))
		return kAreaNetworkMap;
	if (fGeoMapRect.Contains(where))
		return kAreaGeoMap;
	if (fStatsRect.Contains(where))
		return kAreaStats;
	if (fTelemetryRect.Contains(where))
		return kAreaTelemetry;
	if (fPacketAnalyzerRect.Contains(where))
		return kAreaPacketAnalyzer;
	if (fDebugLogRect.Contains(where))
		return kAreaDebugLog;
	if (fMissionControlRect.Contains(where))
		return kAreaMissionControl;
	if (fMqttToggleRect.Contains(where))
		return kAreaMqttToggle;
	if (fMqttLogRect.Contains(where))
		return kAreaMqttLog;
	if (fConnectionDotRect.IsValid() && fConnectionDotRect.Contains(where))
		return kAreaConnectionDot;
	if (fBatteryRect.IsValid() && fBatteryRect.Contains(where))
		return kAreaBattery;
	if (fRssiRect.IsValid() && fRssiRect.Contains(where))
		return kAreaRssi;
	if (fTxRxRect.IsValid() && fTxRxRect.Contains(where))
		return kAreaTxRx;
	if (fUptimeRect.IsValid() && fUptimeRect.Contains(where))
		return kAreaUptime;
	return kAreaNone;
}


const char*
TopBarView::_ToolTipForArea(int32 area) const
{
	switch (area) {
		case kAreaNetworkMap:
			return "Network Map";
		case kAreaGeoMap:
			return "Geographic Map";
		case kAreaStats:
			return "Statistics";
		case kAreaTelemetry:
			return "Sensor Telemetry";
		case kAreaPacketAnalyzer:
			return "Packet Analyzer";
		case kAreaDebugLog:
			return "Debug Log";
		case kAreaMissionControl:
			return "Mission Control";
		case kAreaMqttToggle:
			if (fMqttConnected)
				return "MQTT: Connected (click to disconnect)";
			else if (fMqttEnabled)
				return "MQTT: Disconnected (click to connect)";
			else
				return "MQTT: Disabled (click to configure)";
		case kAreaMqttLog:
			return "MQTT Log";
		case kAreaConnectionDot:
		{
			if (fConnected) {
				fToolTipText.SetToFormat("Serial: Connected\nPort: %s",
					fPortName.Length() > 0 ? fPortName.String() : "unknown");
			} else {
				fToolTipText = "Serial: Disconnected";
			}
			return fToolTipText.String();
		}
		case kAreaBattery:
		{
			int32 pct = 0;
			if (fBatteryMv >= 4200)
				pct = 100;
			else if (fBatteryMv >= 3300)
				pct = (int32)((fBatteryMv - 3300) / 9.0f);
			const char* state = "Critical";
			if (fBatteryMv >= kBattGoodMv) state = "Good";
			else if (fBatteryMv >= kBattFairMv) state = "OK";
			else if (fBatteryMv >= kBattLowMv) state = "Low";
			fToolTipText.SetToFormat("Battery: %u mV (~%d%%)\n"
				"State: %s\n"
				"Range: 3300 mV (empty) \xe2\x80\x93 4200 mV (full)",
				(unsigned)fBatteryMv, (int)pct, state);
			return fToolTipText.String();
		}
		case kAreaRssi:
		{
			const char* quality = "Bad";
			if (fRssi >= -60) quality = "Excellent";
			else if (fRssi >= -70) quality = "Good";
			else if (fRssi >= -80) quality = "Fair";
			else if (fRssi >= -90) quality = "Poor";
			fToolTipText.SetToFormat("RSSI: %d dBm\n"
				"SNR: %d dB\n"
				"Signal quality: %s",
				(int)fRssi, (int)fSnr, quality);
			return fToolTipText.String();
		}
		case kAreaTxRx:
		{
			fToolTipText.SetToFormat("Transmitted: %u packets\n"
				"Received: %u packets",
				(unsigned)fTxPackets, (unsigned)fRxPackets);
			return fToolTipText.String();
		}
		case kAreaUptime:
		{
			uint32 h = fUptime / 3600;
			uint32 m = (fUptime % 3600) / 60;
			uint32 s = fUptime % 60;
			fToolTipText.SetToFormat("Device uptime: %uh %02um %02us\n"
				"(%u seconds total)",
				(unsigned)h, (unsigned)m, (unsigned)s, (unsigned)fUptime);
			return fToolTipText.String();
		}
		default:
			return NULL;
	}
}
