/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MissionControlWindow.cpp — Unified dashboard for device/radio/network overview
 */

#include "MissionControlWindow.h"

#include <Box.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <OS.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "Constants.h"


// ============================================================================
// Constants
// ============================================================================

static const uint32 MSG_REFRESH_TICK = 'rftk';
static const uint32 MSG_PULSE_TICK = 'puls';
static const uint32 MSG_ALERT_FLASH = 'alfl';
static const uint32 MSG_ACTION_ADVERT = 'aadv';
static const uint32 MSG_ACTION_SYNC = 'asyn';
static const uint32 MSG_ACTION_STATS = 'asta';

static const int32 kMaxActivityLines = 50;
static const int32 kSNRBufferSize = 200;
static const int32 kPacketRateBars = 60;
static const float kCardMinWidth = 200.0f;
static const float kCardMinHeight = 100.0f;
static const bigtime_t kStaleThreshold = 30000000;  // 30 seconds


// ============================================================================
// Color helpers (theme-aware)
// ============================================================================

static rgb_color
SnrColor(int8 snr)
{
	if (snr > 5) return (rgb_color){80, 180, 80, 255};
	if (snr > 0) return (rgb_color){140, 200, 80, 255};
	if (snr > -5) return (rgb_color){200, 170, 50, 255};
	if (snr > -10) return (rgb_color){210, 120, 50, 255};
	return (rgb_color){200, 60, 60, 255};
}

static rgb_color
RssiColor(int8 rssi)
{
	if (rssi >= -60) return (rgb_color){80, 180, 80, 255};
	if (rssi >= -80) return (rgb_color){200, 170, 50, 255};
	if (rssi >= -90) return (rgb_color){210, 120, 50, 255};
	return (rgb_color){200, 60, 60, 255};
}

static rgb_color
BattColor(uint16 mv)
{
	if (mv >= 3900) return (rgb_color){80, 180, 80, 255};
	if (mv >= 3600) return (rgb_color){200, 170, 50, 255};
	if (mv >= 3400) return (rgb_color){210, 120, 50, 255};
	return (rgb_color){200, 60, 60, 255};
}

static rgb_color
ScoreColor(int32 score)
{
	if (score >= 75) return (rgb_color){80, 180, 80, 255};
	if (score >= 50) return (rgb_color){200, 170, 50, 255};
	if (score >= 25) return (rgb_color){210, 120, 50, 255};
	return (rgb_color){200, 60, 60, 255};
}


// ============================================================================
// AlertBannerView — Flashing red/amber banner for critical conditions
// ============================================================================

class AlertBannerView : public BView {
public:
	AlertBannerView()
		:
		BView("alertBanner", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fVisible(false),
		fFlashOn(true)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 0));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 0));
		memset(fText, 0, sizeof(fText));
	}

	void SetAlert(const char* text, bool critical = true)
	{
		strlcpy(fText, text, sizeof(fText));
		fCritical = critical;
		fVisible = (text != NULL && text[0] != '\0');
		fFlashOn = true;
		float h = fVisible ? 22.0f : 0.0f;
		SetExplicitMinSize(BSize(B_SIZE_UNSET, h));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, h));
		InvalidateLayout();
		Invalidate();
	}

	void ClearAlert()
	{
		fVisible = false;
		fText[0] = '\0';
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 0));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 0));
		InvalidateLayout();
		Invalidate();
	}

	void ToggleFlash()
	{
		if (!fVisible) return;
		fFlashOn = !fFlashOn;
		Invalidate();
	}

	bool IsAlertVisible() const { return fVisible; }

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		if (!fVisible || bounds.Height() < 2) {
			SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
			FillRect(bounds);
			return;
		}

		rgb_color alertColor;
		if (fCritical)
			alertColor = fFlashOn
				? (rgb_color){200, 50, 50, 255}
				: (rgb_color){160, 40, 40, 255};
		else
			alertColor = fFlashOn
				? (rgb_color){210, 160, 40, 255}
				: (rgb_color){180, 140, 30, 255};

		SetHighColor(alertColor);
		FillRoundRect(bounds, 3, 3);

		BFont font;
		GetFont(&font);
		font.SetSize(11);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);
		font_height fh;
		font.GetHeight(&fh);

		float textY = (bounds.Height() + fh.ascent - fh.descent) / 2;
		SetHighColor(255, 255, 255, 255);

		// Warning icon
		BString displayText;
		displayText.SetToFormat("\xE2\x9A\xA0 %s", fText);
		DrawString(displayText.String(), BPoint(8, textY));
	}

private:
	bool		fVisible;
	bool		fFlashOn;
	bool		fCritical;
	char		fText[128];
};


// ============================================================================
// MetricCardView — Card with title, metric rows, and optional pulsing dot
// ============================================================================

class MetricCardView : public BView {
public:
	MetricCardView(const char* title)
		:
		BView(title, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fTitle(title),
		fPulsePhase(0),
		fShowPulse(false),
		fStoragePct(-1)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(kCardMinWidth, kCardMinHeight));
		memset(fLabels, 0, sizeof(fLabels));
		memset(fValues, 0, sizeof(fValues));
		for (int i = 0; i < 6; i++)
			fValueColors[i] = ui_color(B_PANEL_TEXT_COLOR);
		fRowCount = 0;
	}

	void SetRow(int32 row, const char* label, const char* value,
		rgb_color color = (rgb_color){0, 0, 0, 0})
	{
		if (row < 0 || row >= 6) return;
		strlcpy(fLabels[row], label, sizeof(fLabels[row]));
		strlcpy(fValues[row], value, sizeof(fValues[row]));
		if (color.alpha != 0)
			fValueColors[row] = color;
		else
			fValueColors[row] = ui_color(B_PANEL_TEXT_COLOR);
		if (row >= fRowCount)
			fRowCount = row + 1;
		Invalidate();
	}

	void SetPulse(bool show) { fShowPulse = show; Invalidate(); }

	void SetStorageGauge(int8 pct) { fStoragePct = pct; Invalidate(); }

	void Pulse()
	{
		fPulsePhase = (fPulsePhase + 1) % 20;
		if (fShowPulse)
			Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
		rgb_color dimColor = tint_color(textColor, B_LIGHTEN_1_TINT);
		rgb_color borderColor = tint_color(bg, B_DARKEN_2_TINT);

		// Background
		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		// Border
		SetHighColor(borderColor);
		StrokeRoundRect(bounds, 4, 4);

		// Title + pulsing dot
		BFont titleFont;
		GetFont(&titleFont);
		titleFont.SetSize(10);
		titleFont.SetFace(B_BOLD_FACE);
		SetFont(&titleFont);

		font_height fh;
		titleFont.GetHeight(&fh);
		float y = 8 + fh.ascent;
		SetHighColor(dimColor);
		float titleX = 10;

		// Pulsing connection dot
		if (fShowPulse) {
			float dotR = 4.0f;
			float alpha = 0.5f + 0.5f * sinf(fPulsePhase * M_PI / 10.0f);
			uint8 g = (uint8)(120 + 60 * alpha);
			SetHighColor((rgb_color){60, g, 60, 255});
			FillEllipse(BPoint(titleX + dotR, y - fh.ascent / 2 + 1),
				dotR, dotR);
			titleX += dotR * 2 + 4;
		}

		SetHighColor(dimColor);
		DrawString(fTitle.String(), BPoint(titleX, y));
		y += fh.descent + fh.leading + 2;

		// Separator line
		SetHighColor(borderColor);
		StrokeLine(BPoint(8, y), BPoint(bounds.right - 8, y));
		y += 6;

		// Metric rows
		BFont labelFont;
		GetFont(&labelFont);
		labelFont.SetSize(11);
		labelFont.SetFace(B_REGULAR_FACE);
		SetFont(&labelFont);
		labelFont.GetHeight(&fh);

		float rowHeight = fh.ascent + fh.descent + fh.leading + 4;

		for (int32 i = 0; i < fRowCount; i++) {
			float textY = y + fh.ascent;

			// Label (left-aligned, dim)
			SetHighColor(dimColor);
			DrawString(fLabels[i], BPoint(10, textY));

			// Value (right-aligned, colored)
			float valWidth = StringWidth(fValues[i]);
			SetHighColor(fValueColors[i]);
			DrawString(fValues[i],
				BPoint(bounds.right - 10 - valWidth, textY));

			y += rowHeight;
		}

		// Storage gauge bar (at bottom of card)
		if (fStoragePct >= 0) {
			float gaugeY = y + 2;
			float gaugeH = 6;
			float gaugeLeft = 10;
			float gaugeRight = bounds.right - 10;
			float gaugeWidth = gaugeRight - gaugeLeft;

			// Track
			SetHighColor(tint_color(bg, B_DARKEN_1_TINT));
			FillRoundRect(BRect(gaugeLeft, gaugeY,
				gaugeRight, gaugeY + gaugeH), 2, 2);

			// Fill
			float fillWidth = (fStoragePct / 100.0f) * gaugeWidth;
			rgb_color fillColor;
			if (fStoragePct < 60) fillColor = (rgb_color){80, 180, 80, 255};
			else if (fStoragePct < 80)
				fillColor = (rgb_color){200, 170, 50, 255};
			else fillColor = (rgb_color){200, 60, 60, 255};
			SetHighColor(fillColor);
			if (fillWidth > 1)
				FillRoundRect(BRect(gaugeLeft, gaugeY,
					gaugeLeft + fillWidth, gaugeY + gaugeH), 2, 2);

			// Label
			labelFont.SetSize(8);
			SetFont(&labelFont);
			SetHighColor(dimColor);
			char storLabel[24];
			snprintf(storLabel, sizeof(storLabel), "Storage: %d%%",
				(int)fStoragePct);
			DrawString(storLabel, BPoint(gaugeLeft, gaugeY + gaugeH + 9));
		}
	}

private:
	BString		fTitle;
	char		fLabels[6][32];
	char		fValues[6][48];
	rgb_color	fValueColors[6];
	int32		fRowCount;
	int32		fPulsePhase;
	bool		fShowPulse;
	int8		fStoragePct;
};


// ============================================================================
// HealthScoreView — Colored arc with score 0-100
// ============================================================================

class HealthScoreView : public BView {
public:
	HealthScoreView()
		:
		BView("healthScore", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fScore(0)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(80, 60));
	}

	void SetScore(int32 score)
	{
		if (score < 0) score = 0;
		if (score > 100) score = 100;
		fScore = score;
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);

		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		float cx = floorf(bounds.Width() / 2);
		float cy = floorf(bounds.Height() * 0.6f);
		float penW = 4.0f;
		float maxRadiusX = cx - penW - 2;
		float maxRadiusY = cy - penW - 2;
		float radius = fmin(maxRadiusX, maxRadiusY);
		if (radius < 8) radius = 8;

		// Background arc (gray)
		rgb_color trackColor = tint_color(bg, B_DARKEN_2_TINT);
		SetHighColor(trackColor);
		SetPenSize(penW);
		StrokeArc(BPoint(cx, cy), radius, radius, 0, 180);

		// Score arc (colored, sweeps from left)
		float sweepAngle = (fScore / 100.0f) * 180.0f;
		SetHighColor(ScoreColor(fScore));
		SetPenSize(penW);
		StrokeArc(BPoint(cx, cy), radius, radius, 180, sweepAngle);
		SetPenSize(1.0f);

		// Score text centered in arc
		char scoreStr[8];
		snprintf(scoreStr, sizeof(scoreStr), "%d", (int)fScore);

		BFont scoreFont;
		GetFont(&scoreFont);
		scoreFont.SetSize(16);
		scoreFont.SetFace(B_BOLD_FACE);
		SetFont(&scoreFont);

		font_height fh;
		scoreFont.GetHeight(&fh);
		float textW = StringWidth(scoreStr);
		SetHighColor(ScoreColor(fScore));
		DrawString(scoreStr, BPoint(cx - textW / 2, cy - 4));

		// "/ 100" label below score
		scoreFont.SetSize(9);
		scoreFont.SetFace(B_REGULAR_FACE);
		SetFont(&scoreFont);
		rgb_color dimText = tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_LIGHTEN_1_TINT);
		SetHighColor(dimText);
		textW = StringWidth("/ 100");
		DrawString("/ 100", BPoint(cx - textW / 2, cy + 8));
	}

private:
	int32		fScore;
};


// ============================================================================
// ContactGridView — Compact grid of colored dots for contacts
// ============================================================================

class ContactGridView : public BView {
public:
	ContactGridView()
		:
		BView("contactGrid", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fTotal(0),
		fOnline(0),
		fRecent(0)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(80, 30));
	}

	void SetCounts(int32 total, int32 online, int32 recent)
	{
		fTotal = total;
		fOnline = online;
		fRecent = recent;
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);

		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		// Summary text
		BFont font;
		GetFont(&font);
		font.SetSize(10);
		SetFont(&font);
		font_height fh;
		font.GetHeight(&fh);

		rgb_color dimColor = tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_LIGHTEN_1_TINT);

		if (fTotal <= 0) {
			SetHighColor(dimColor);
			DrawString("No contacts", BPoint(4, fh.ascent + 2));
			return;
		}

		// Draw count summary first
		char summary[48];
		snprintf(summary, sizeof(summary), "%d online / %d total",
			(int)fOnline, (int)fTotal);
		SetHighColor(dimColor);
		DrawString(summary, BPoint(4, fh.ascent + 2));

		// Dot grid below
		float dotSize = 7.0f;
		float gap = 3.0f;
		float dotY = fh.ascent + fh.descent + 6;
		int32 cols = (int32)((bounds.Width() - 4) / (dotSize + gap));
		if (cols < 1) cols = 1;

		float x = 4;
		float y = dotY;
		int32 drawn = 0;

		rgb_color onlineColor = {80, 180, 80, 255};
		rgb_color recentColor = {200, 170, 50, 255};
		rgb_color offlineColor = tint_color(bg, B_DARKEN_2_TINT);

		for (int32 i = 0; i < fTotal && i < 50; i++) {
			if (i < fOnline)
				SetHighColor(onlineColor);
			else if (i < fOnline + fRecent)
				SetHighColor(recentColor);
			else
				SetHighColor(offlineColor);

			FillEllipse(BRect(x, y, x + dotSize, y + dotSize));
			drawn++;

			x += dotSize + gap;
			if (drawn % cols == 0) {
				x = 4;
				y += dotSize + gap;
			}
		}
	}

private:
	int32		fTotal;
	int32		fOnline;
	int32		fRecent;
};


// ============================================================================
// DashboardSNRView — Rolling SNR + RSSI dual line chart (200 points)
// ============================================================================

class DashboardSNRView : public BView {
public:
	DashboardSNRView()
		:
		BView("snrChart", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fSnrHead(0),
		fSnrCount(0),
		fRssiHead(0),
		fRssiCount(0)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(200, 80));
		memset(fSnrBuffer, 0, sizeof(fSnrBuffer));
		memset(fRssiBuffer, 0, sizeof(fRssiBuffer));
	}

	void AddSNRPoint(int8 snr)
	{
		fSnrBuffer[fSnrHead] = snr;
		fSnrHead = (fSnrHead + 1) % kSNRBufferSize;
		if (fSnrCount < kSNRBufferSize)
			fSnrCount++;
		Invalidate();
	}

	void AddRSSIPoint(int8 rssi)
	{
		fRssiBuffer[fRssiHead] = rssi;
		fRssiHead = (fRssiHead + 1) % kSNRBufferSize;
		if (fRssiCount < kSNRBufferSize)
			fRssiCount++;
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
		rgb_color dimColor = tint_color(textColor, B_LIGHTEN_1_TINT);
		rgb_color borderColor = tint_color(bg, B_DARKEN_2_TINT);

		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		// Border
		SetHighColor(borderColor);
		StrokeRoundRect(bounds, 4, 4);

		// Title and legend
		BFont titleFont;
		GetFont(&titleFont);
		titleFont.SetSize(10);
		titleFont.SetFace(B_BOLD_FACE);
		SetFont(&titleFont);
		font_height fh;
		titleFont.GetHeight(&fh);
		SetHighColor(dimColor);
		DrawString("SNR / RSSI Trend", BPoint(8, 6 + fh.ascent));

		// Legend (right side)
		titleFont.SetSize(8);
		SetFont(&titleFont);
		float lx = bounds.right - 100;
		SetHighColor((rgb_color){80, 180, 80, 255});
		StrokeLine(BPoint(lx, 9), BPoint(lx + 12, 9));
		SetHighColor(dimColor);
		DrawString("SNR", BPoint(lx + 14, 12));
		lx += 40;
		SetHighColor((rgb_color){100, 140, 200, 255});
		StrokeLine(BPoint(lx, 9), BPoint(lx + 12, 9));
		SetHighColor(dimColor);
		DrawString("RSSI", BPoint(lx + 14, 12));

		float chartTop = 20;
		float chartBottom = bounds.bottom - 4;
		float chartLeft = 30;
		float chartRight = bounds.right - 40;
		float chartHeight = chartBottom - chartTop;
		float chartWidth = chartRight - chartLeft;

		if (chartWidth < 10 || chartHeight < 10) return;

		// SNR background zones
		float snrMax = 15;
		float snrRange = 35.0f;  // 15 to -20

		rgb_color greenZone = {
			(uint8)(bg.red * 0.85f + 80 * 0.15f),
			(uint8)(bg.green * 0.85f + 180 * 0.15f),
			(uint8)(bg.blue * 0.85f + 80 * 0.15f), 255};
		rgb_color yellowZone = {
			(uint8)(bg.red * 0.85f + 200 * 0.15f),
			(uint8)(bg.green * 0.85f + 170 * 0.15f),
			(uint8)(bg.blue * 0.85f + 50 * 0.15f), 255};
		rgb_color redZone = {
			(uint8)(bg.red * 0.85f + 200 * 0.15f),
			(uint8)(bg.green * 0.85f + 60 * 0.15f),
			(uint8)(bg.blue * 0.85f + 60 * 0.15f), 255};

		float greenBottom = chartTop + ((snrMax - 5) / snrRange) * chartHeight;
		SetHighColor(greenZone);
		FillRect(BRect(chartLeft, chartTop, chartRight, greenBottom));

		float yellowBottom = chartTop + ((snrMax + 5) / snrRange) * chartHeight;
		SetHighColor(yellowZone);
		FillRect(BRect(chartLeft, greenBottom, chartRight, yellowBottom));

		float redTop = chartTop + ((snrMax + 10) / snrRange) * chartHeight;
		SetHighColor(redZone);
		FillRect(BRect(chartLeft, redTop, chartRight, chartBottom));

		// Zero line
		float zeroY = chartTop + (snrMax / snrRange) * chartHeight;
		SetHighColor(tint_color(bg, B_DARKEN_3_TINT));
		StrokeLine(BPoint(chartLeft, zeroY), BPoint(chartRight, zeroY));

		// SNR Y-axis labels (left)
		titleFont.SetSize(8);
		titleFont.SetFace(B_REGULAR_FACE);
		SetFont(&titleFont);
		SetHighColor(dimColor);
		for (int snr = -15; snr <= 10; snr += 5) {
			float y = chartTop + ((snrMax - snr) / snrRange) * chartHeight;
			char label[8];
			snprintf(label, sizeof(label), "%d", snr);
			float lw = StringWidth(label);
			DrawString(label, BPoint(chartLeft - lw - 2, y + 3));
		}

		// RSSI Y-axis labels (right)
		float rssiMax = -40.0f;
		float rssiMin = -120.0f;
		float rssiRange = rssiMax - rssiMin;
		SetHighColor((rgb_color){100, 140, 200, 255});
		for (int rssi = -120; rssi <= -40; rssi += 20) {
			float y = chartTop + ((rssiMax - rssi) / rssiRange) * chartHeight;
			char label[8];
			snprintf(label, sizeof(label), "%d", rssi);
			DrawString(label, BPoint(chartRight + 3, y + 3));
		}

		// RSSI data line (behind SNR)
		if (fRssiCount >= 2) {
			SetPenSize(1.0f);
			int32 start = (fRssiHead - fRssiCount + kSNRBufferSize)
				% kSNRBufferSize;
			float stepX = chartWidth / (float)(kSNRBufferSize - 1);
			rgb_color rssiLineColor = {100, 140, 200, 255};
			SetHighColor(rssiLineColor);

			BPoint prev;
			for (int32 i = 0; i < fRssiCount; i++) {
				int32 idx = (start + i) % kSNRBufferSize;
				float x = chartLeft + i * stepX;
				float y = chartTop + ((rssiMax - fRssiBuffer[idx]) / rssiRange)
					* chartHeight;
				if (y < chartTop) y = chartTop;
				if (y > chartBottom) y = chartBottom;

				BPoint pt(x, y);
				if (i > 0)
					StrokeLine(prev, pt);
				prev = pt;
			}
		}

		// SNR data line (foreground)
		if (fSnrCount >= 2) {
			SetPenSize(1.5f);
			int32 start = (fSnrHead - fSnrCount + kSNRBufferSize)
				% kSNRBufferSize;
			float stepX = chartWidth / (float)(kSNRBufferSize - 1);

			BPoint prev;
			for (int32 i = 0; i < fSnrCount; i++) {
				int32 idx = (start + i) % kSNRBufferSize;
				float x = chartLeft + i * stepX;
				float y = chartTop +
					((snrMax - fSnrBuffer[idx]) / snrRange) * chartHeight;
				if (y < chartTop) y = chartTop;
				if (y > chartBottom) y = chartBottom;

				BPoint pt(x, y);
				if (i > 0) {
					SetHighColor(SnrColor(fSnrBuffer[idx]));
					StrokeLine(prev, pt);
				}
				prev = pt;
			}
			SetPenSize(1.0f);
		}
	}

private:
	int8		fSnrBuffer[kSNRBufferSize];
	int32		fSnrHead;
	int32		fSnrCount;
	int8		fRssiBuffer[kSNRBufferSize];
	int32		fRssiHead;
	int32		fRssiCount;
};


// ============================================================================
// PacketRateView — TX/RX bar chart (60 bars, 10 minutes at 10s intervals)
// ============================================================================

class PacketRateView : public BView {
public:
	PacketRateView()
		:
		BView("packetRate", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fHead(0),
		fCount(0),
		fLastTx(0),
		fLastRx(0)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(200, 80));
		memset(fTxBars, 0, sizeof(fTxBars));
		memset(fRxBars, 0, sizeof(fRxBars));
	}

	void RecordSample(uint32 txTotal, uint32 rxTotal)
	{
		if (fLastTx > 0 || fLastRx > 0) {
			uint32 txDelta = (txTotal >= fLastTx) ? (txTotal - fLastTx) : 0;
			uint32 rxDelta = (rxTotal >= fLastRx) ? (rxTotal - fLastRx) : 0;
			fTxBars[fHead] = txDelta;
			fRxBars[fHead] = rxDelta;
			fHead = (fHead + 1) % kPacketRateBars;
			if (fCount < kPacketRateBars)
				fCount++;
		}
		fLastTx = txTotal;
		fLastRx = rxTotal;
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
		rgb_color dimColor = tint_color(textColor, B_LIGHTEN_1_TINT);
		rgb_color borderColor = tint_color(bg, B_DARKEN_2_TINT);

		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		SetHighColor(borderColor);
		StrokeRoundRect(bounds, 4, 4);

		BFont titleFont;
		GetFont(&titleFont);
		titleFont.SetSize(10);
		titleFont.SetFace(B_BOLD_FACE);
		SetFont(&titleFont);
		font_height fh;
		titleFont.GetHeight(&fh);
		SetHighColor(dimColor);
		DrawString("Packet Rate (10 min)", BPoint(8, 6 + fh.ascent));

		// Legend
		float legendX = bounds.right - 80;
		rgb_color txColor = {100, 180, 255, 255};
		rgb_color rxColor = {255, 160, 40, 255};
		titleFont.SetSize(8);
		SetFont(&titleFont);
		SetHighColor(txColor);
		FillRect(BRect(legendX, 6, legendX + 8, 12));
		SetHighColor(dimColor);
		DrawString("TX", BPoint(legendX + 10, 12));
		SetHighColor(rxColor);
		FillRect(BRect(legendX + 30, 6, legendX + 38, 12));
		SetHighColor(dimColor);
		DrawString("RX", BPoint(legendX + 40, 12));

		float chartTop = 20;
		float chartBottom = bounds.bottom - 4;
		float chartLeft = 8;
		float chartRight = bounds.right - 4;
		float chartHeight = chartBottom - chartTop;
		float chartWidth = chartRight - chartLeft;

		if (chartWidth < 10 || chartHeight < 10) return;

		uint32 maxVal = 1;
		int32 start = (fHead - fCount + kPacketRateBars) % kPacketRateBars;
		for (int32 i = 0; i < fCount; i++) {
			int32 idx = (start + i) % kPacketRateBars;
			uint32 total = fTxBars[idx] + fRxBars[idx];
			if (total > maxVal) maxVal = total;
		}

		SetHighColor(borderColor);
		StrokeLine(BPoint(chartLeft, chartBottom),
			BPoint(chartRight, chartBottom));

		if (fCount == 0) return;

		float barWidth = chartWidth / kPacketRateBars;
		if (barWidth < 1) barWidth = 1;
		float barGap = barWidth * 0.2f;
		float effectiveBarW = barWidth - barGap;
		if (effectiveBarW < 1) effectiveBarW = 1;

		for (int32 i = 0; i < fCount; i++) {
			int32 idx = (start + i) % kPacketRateBars;
			float x = chartLeft + i * barWidth;

			float txHeight = (fTxBars[idx] / (float)maxVal) * chartHeight;
			float rxHeight = (fRxBars[idx] / (float)maxVal) * chartHeight;

			if (txHeight > 0.5f) {
				SetHighColor(txColor);
				FillRect(BRect(x, chartBottom - txHeight - rxHeight,
					x + effectiveBarW, chartBottom - rxHeight));
			}
			if (rxHeight > 0.5f) {
				SetHighColor(rxColor);
				FillRect(BRect(x, chartBottom - rxHeight,
					x + effectiveBarW, chartBottom));
			}
		}
	}

private:
	uint32		fTxBars[kPacketRateBars];
	uint32		fRxBars[kPacketRateBars];
	int32		fHead;
	int32		fCount;
	uint32		fLastTx;
	uint32		fLastRx;
};


// ============================================================================
// MissionControlWindow
// ============================================================================

MissionControlWindow::MissionControlWindow(BWindow* parent)
	:
	BWindow(BRect(120, 80, 920, 680), "Mission Control",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fParent(parent),
	fAlertBanner(NULL),
	fDeviceCard(NULL),
	fRadioCard(NULL),
	fHealthScore(NULL),
	fContactGrid(NULL),
	fSNRChart(NULL),
	fPacketRateChart(NULL),
	fAdvertButton(NULL),
	fSyncButton(NULL),
	fStatsButton(NULL),
	fActivityFeed(NULL),
	fActivityScroll(NULL),
	fActivityLineCount(0),
	fLastUpdateLabel(NULL),
	fLastDataTime(0),
	fConnected(false),
	fBatteryMv(0),
	fRssi(0),
	fSnr(0),
	fContactsTotal(0),
	fContactsOnline(0),
	fUsedKb(0),
	fTotalKb(0),
	fUptime(0),
	fTxPackets(0),
	fRxPackets(0)
{
	_BuildLayout();

	// Refresh timer (10 seconds) — requests stats
	BMessage tickMsg(MSG_REFRESH_TICK);
	new BMessageRunner(BMessenger(this), &tickMsg, 10000000, -1);

	// Pulse timer (200ms) — pulsing dot + last-update
	BMessage pulseMsg(MSG_PULSE_TICK);
	new BMessageRunner(BMessenger(this), &pulseMsg, 200000, -1);

	// Alert flash timer (800ms)
	BMessage flashMsg(MSG_ALERT_FLASH);
	new BMessageRunner(BMessenger(this), &flashMsg, 800000, -1);
}


MissionControlWindow::~MissionControlWindow()
{
}


bool
MissionControlWindow::QuitRequested()
{
	Hide();
	return false;
}


void
MissionControlWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_REFRESH_TICK:
			if (fParent != NULL)
				fParent->PostMessage(MSG_REQUEST_STATS_DATA);
			break;

		case MSG_PULSE_TICK:
			fDeviceCard->Pulse();
			_UpdateLastUpdate();
			break;

		case MSG_ALERT_FLASH:
			if (fAlertBanner != NULL)
				fAlertBanner->ToggleFlash();
			break;

		case MSG_ACTION_ADVERT:
			if (fParent != NULL)
				fParent->PostMessage(MSG_SEND_ADVERT);
			break;

		case MSG_ACTION_SYNC:
			if (fParent != NULL)
				fParent->PostMessage(MSG_SYNC_CONTACTS);
			break;

		case MSG_ACTION_STATS:
			if (fParent != NULL) {
				fParent->PostMessage(MSG_GET_BATTERY);
				fParent->PostMessage(MSG_GET_STATS);
			}
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
MissionControlWindow::_BuildLayout()
{
	// === Alert banner (top, initially hidden) ===
	fAlertBanner = new AlertBannerView();

	// === Top row cards ===
	fDeviceCard = new MetricCardView("Device Status");
	fDeviceCard->SetRow(0, "Connection", "Disconnected",
		(rgb_color){200, 60, 60, 255});
	fDeviceCard->SetRow(1, "Battery", "--");
	fDeviceCard->SetRow(2, "Uptime", "--");
	fDeviceCard->SetRow(3, "Firmware", "--");

	fRadioCard = new MetricCardView("Radio Health");
	fRadioCard->SetRow(0, "RSSI", "-- dBm");
	fRadioCard->SetRow(1, "SNR", "-- dB");
	fRadioCard->SetRow(2, "Noise Floor", "-- dBm");
	fRadioCard->SetRow(3, "TX Power", "-- dBm");

	// Network overview card
	BBox* networkCard = new BBox("networkCard");
	networkCard->SetLabel("Network Overview");
	networkCard->SetExplicitMinSize(BSize(kCardMinWidth, kCardMinHeight));

	fHealthScore = new HealthScoreView();
	fContactGrid = new ContactGridView();

	BView* networkInner = new BView("networkInner", 0);
	networkInner->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(networkInner, B_VERTICAL, 2)
		.SetInsets(2, 2, 2, 2)
		.Add(fHealthScore, 2)
		.Add(fContactGrid, 1)
	.End();
	networkCard->AddChild(networkInner);

	// === Middle row charts ===
	fSNRChart = new DashboardSNRView();
	fPacketRateChart = new PacketRateView();

	// === Quick Actions bar ===
	fAdvertButton = new BButton("advert", "Send Advert",
		new BMessage(MSG_ACTION_ADVERT));
	fSyncButton = new BButton("sync", "Sync Contacts",
		new BMessage(MSG_ACTION_SYNC));
	fStatsButton = new BButton("stats", "Refresh Stats",
		new BMessage(MSG_ACTION_STATS));
	fAdvertButton->SetEnabled(false);
	fSyncButton->SetEnabled(false);
	fStatsButton->SetEnabled(false);

	// === Bottom: Activity feed ===
	BRect textRect(0, 0, 600, 200);
	fActivityFeed = new BTextView(textRect, "activityFeed",
		textRect, B_FOLLOW_ALL, B_WILL_DRAW);
	fActivityFeed->MakeEditable(false);
	fActivityFeed->MakeSelectable(true);
	fActivityFeed->SetStylable(true);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(10);
	rgb_color docTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);
	fActivityFeed->SetFontAndColor(&monoFont, B_FONT_ALL, &docTextColor);
	fActivityFeed->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);

	fActivityScroll = new BScrollView("activityScroll", fActivityFeed,
		B_FOLLOW_ALL, 0, false, true);
	fActivityScroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 80));

	// Last update footer
	fLastUpdateLabel = new BStringView("lastUpdate", "Last update: --");
	BFont footerFont;
	fLastUpdateLabel->GetFont(&footerFont);
	footerFont.SetSize(9);
	fLastUpdateLabel->SetFont(&footerFont);
	fLastUpdateLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, B_LIGHTEN_1_TINT);

	// === Main layout ===
	BSplitView* topSplit = new BSplitView(B_HORIZONTAL, 4);
	BSplitView* midSplit = new BSplitView(B_HORIZONTAL, 4);

	BLayoutBuilder::Split<>(topSplit)
		.Add(fDeviceCard, 1)
		.Add(fRadioCard, 1)
		.Add(networkCard, 1)
	.End();

	BLayoutBuilder::Split<>(midSplit)
		.Add(fSNRChart, 3)
		.Add(fPacketRateChart, 2)
	.End();

	// Activity feed title
	BStringView* feedTitle = new BStringView("feedTitle", "Activity Feed");
	BFont feedFont;
	feedTitle->GetFont(&feedFont);
	feedFont.SetSize(10);
	feedFont.SetFace(B_BOLD_FACE);
	feedTitle->SetFont(&feedFont);
	feedTitle->SetHighUIColor(B_PANEL_TEXT_COLOR, B_LIGHTEN_1_TINT);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 2)
		.SetInsets(6, 6, 6, 6)
		.Add(fAlertBanner)
		.Add(topSplit, 2)
		.Add(midSplit, 2)
		.AddGroup(B_HORIZONTAL, 4, 0)
			.Add(fAdvertButton)
			.Add(fSyncButton)
			.Add(fStatsButton)
			.AddGlue()
		.End()
		.Add(feedTitle)
		.Add(fActivityScroll, 1)
		.Add(fLastUpdateLabel)
	.End();
}


// ============================================================================
// Public setters (called from MainWindow with LockLooper())
// ============================================================================

void
MissionControlWindow::SetConnectionState(bool connected,
	const char* deviceName, const char* firmware)
{
	fConnected = connected;
	if (connected) {
		BString connStr;
		connStr.SetToFormat("Connected: %s",
			deviceName != NULL ? deviceName : "");
		fDeviceCard->SetRow(0, "Connection", connStr.String(),
			(rgb_color){80, 180, 80, 255});
		fDeviceCard->SetPulse(true);
		fAdvertButton->SetEnabled(true);
		fSyncButton->SetEnabled(true);
		fStatsButton->SetEnabled(true);
	} else {
		fDeviceCard->SetRow(0, "Connection", "Disconnected",
			(rgb_color){200, 60, 60, 255});
		fDeviceCard->SetPulse(false);
		fDeviceCard->SetRow(1, "Battery", "--");
		fDeviceCard->SetRow(2, "Uptime", "--");
		fDeviceCard->SetRow(3, "Firmware", "--");
		fDeviceCard->SetStorageGauge(-1);
		fRadioCard->SetRow(0, "RSSI", "-- dBm");
		fRadioCard->SetRow(1, "SNR", "-- dB");
		fRadioCard->SetRow(2, "Noise Floor", "-- dBm");
		fRadioCard->SetRow(3, "TX Power", "-- dBm");
		fBatteryMv = 0;
		fRssi = 0;
		fSnr = 0;
		fUptime = 0;
		fTxPackets = 0;
		fRxPackets = 0;
		fAdvertButton->SetEnabled(false);
		fSyncButton->SetEnabled(false);
		fStatsButton->SetEnabled(false);
	}

	if (firmware != NULL && firmware[0] != '\0')
		fDeviceCard->SetRow(3, "Firmware", firmware);

	_RecalcHealthScore();
	_CheckAlerts();
	fLastDataTime = system_time();
}


void
MissionControlWindow::SetBatteryInfo(uint16 battMv, uint32 usedKb,
	uint32 totalKb)
{
	fBatteryMv = battMv;
	fUsedKb = usedKb;
	fTotalKb = totalKb;

	int32 pct = 0;
	if (battMv >= 4200) pct = 100;
	else if (battMv >= 3300) pct = (int32)((battMv - 3300) / 9.0f);

	char battStr[32];
	snprintf(battStr, sizeof(battStr), "%d%% (%u mV)", (int)pct,
		(unsigned)battMv);
	fDeviceCard->SetRow(1, "Battery", battStr, BattColor(battMv));

	// Storage gauge
	if (totalKb > 0) {
		int8 storagePct = (int8)((usedKb * 100) / totalKb);
		fDeviceCard->SetStorageGauge(storagePct);
	}

	_RecalcHealthScore();
	_CheckAlerts();
	fLastDataTime = system_time();
}


void
MissionControlWindow::SetDeviceStats(uint32 uptime, uint32 txPackets,
	uint32 rxPackets)
{
	fUptime = uptime;
	fTxPackets = txPackets;
	fRxPackets = rxPackets;

	char uptimeStr[32];
	uint32 d = uptime / 86400;
	uint32 h = (uptime % 86400) / 3600;
	uint32 m = (uptime % 3600) / 60;
	if (d > 0)
		snprintf(uptimeStr, sizeof(uptimeStr), "%ud %uh %um",
			(unsigned)d, (unsigned)h, (unsigned)m);
	else
		snprintf(uptimeStr, sizeof(uptimeStr), "%uh %um",
			(unsigned)h, (unsigned)m);
	fDeviceCard->SetRow(2, "Uptime", uptimeStr);

	fPacketRateChart->RecordSample(txPackets, rxPackets);
	fLastDataTime = system_time();
}


void
MissionControlWindow::SetRadioStats(int8 rssi, int8 snr, int8 noiseFloor)
{
	fRssi = rssi;
	fSnr = snr;

	char rssiStr[16], snrStr[16], noiseStr[16];
	snprintf(rssiStr, sizeof(rssiStr), "%d dBm", (int)rssi);
	snprintf(snrStr, sizeof(snrStr), "%+d dB", (int)snr);
	snprintf(noiseStr, sizeof(noiseStr), "%d dBm", (int)noiseFloor);

	fRadioCard->SetRow(0, "RSSI", rssiStr, RssiColor(rssi));
	fRadioCard->SetRow(1, "SNR", snrStr, SnrColor(snr));
	fRadioCard->SetRow(2, "Noise Floor", noiseStr);

	AddSNRDataPoint(snr);
	AddRSSIDataPoint(rssi);

	_RecalcHealthScore();
	_CheckAlerts();
	fLastDataTime = system_time();
}


void
MissionControlWindow::SetRadioConfig(uint32 freqHz, uint32 bwHz,
	uint8 sf, uint8 cr, uint8 txPower)
{
	char txPStr[16];
	snprintf(txPStr, sizeof(txPStr), "%d dBm", (int)txPower);
	fRadioCard->SetRow(3, "TX Power", txPStr);
	fLastDataTime = system_time();
}


void
MissionControlWindow::SetPacketStats(uint32 txPackets, uint32 rxPackets)
{
	fTxPackets = txPackets;
	fRxPackets = rxPackets;
	fPacketRateChart->RecordSample(txPackets, rxPackets);
	fLastDataTime = system_time();
}


void
MissionControlWindow::UpdateContacts(int32 total, int32 online, int32 recent)
{
	fContactsTotal = total;
	fContactsOnline = online;
	fContactGrid->SetCounts(total, online, recent);
	_RecalcHealthScore();
	fLastDataTime = system_time();
}


void
MissionControlWindow::AddSNRDataPoint(int8 snr)
{
	fSNRChart->AddSNRPoint(snr);
}


void
MissionControlWindow::AddRSSIDataPoint(int8 rssi)
{
	fSNRChart->AddRSSIPoint(rssi);
}


void
MissionControlWindow::AddActivityEvent(const char* category, const char* text)
{
	_AddTimestampedEvent(category, text);
}


// ============================================================================
// Private helpers
// ============================================================================

void
MissionControlWindow::_RecalcHealthScore()
{
	int32 score = 0;

	if (fConnected) score += 25;

	if (fBatteryMv >= 4200) score += 15;
	else if (fBatteryMv >= 3300)
		score += (int32)(((fBatteryMv - 3300) / 900.0f) * 15);

	if (fRssi != 0) {
		int32 rssiScore = (int32)((fRssi + 120) * 20.0f / 80.0f);
		if (rssiScore < 0) rssiScore = 0;
		if (rssiScore > 20) rssiScore = 20;
		score += rssiScore;
	}

	if (fSnr != 0 || fRssi != 0) {
		int32 snrScore = (int32)((fSnr + 20) * 20.0f / 35.0f);
		if (snrScore < 0) snrScore = 0;
		if (snrScore > 20) snrScore = 20;
		score += snrScore;
	}

	if (fContactsTotal > 0)
		score += (int32)((fContactsOnline * 20.0f) / fContactsTotal);

	fHealthScore->SetScore(score);
}


void
MissionControlWindow::_CheckAlerts()
{
	// Priority: disconnect > low battery > bad SNR
	if (!fConnected && fAlertBanner->IsAlertVisible()) {
		// Keep disconnect alert if already showing
		return;
	}

	if (!fConnected) {
		fAlertBanner->SetAlert("Device disconnected!", true);
		return;
	}

	if (fBatteryMv > 0 && fBatteryMv < 3400) {
		char msg[64];
		snprintf(msg, sizeof(msg), "Low battery: %u mV — charge soon!",
			(unsigned)fBatteryMv);
		fAlertBanner->SetAlert(msg, true);
		return;
	}

	if (fSnr != 0 && fSnr < -10) {
		char msg[64];
		snprintf(msg, sizeof(msg), "Poor signal quality: SNR %+d dB",
			(int)fSnr);
		fAlertBanner->SetAlert(msg, false);
		return;
	}

	// All clear
	fAlertBanner->ClearAlert();
}


void
MissionControlWindow::_UpdateLastUpdate()
{
	if (fLastDataTime == 0) {
		fLastUpdateLabel->SetText("Last update: --");
		fLastUpdateLabel->SetHighUIColor(B_PANEL_TEXT_COLOR,
			B_LIGHTEN_1_TINT);
		return;
	}

	bigtime_t elapsed = system_time() - fLastDataTime;
	int32 seconds = (int32)(elapsed / 1000000);

	char str[48];
	if (seconds < 2)
		snprintf(str, sizeof(str), "Last update: just now");
	else if (seconds < 60)
		snprintf(str, sizeof(str), "Last update: %ds ago", (int)seconds);
	else
		snprintf(str, sizeof(str), "Last update: %dm %ds ago",
			(int)(seconds / 60), (int)(seconds % 60));

	fLastUpdateLabel->SetText(str);

	// Go red when stale
	if (elapsed > kStaleThreshold && fConnected)
		fLastUpdateLabel->SetHighColor((rgb_color){200, 60, 60, 255});
	else
		fLastUpdateLabel->SetHighUIColor(B_PANEL_TEXT_COLOR,
			B_LIGHTEN_1_TINT);
}


void
MissionControlWindow::_AddTimestampedEvent(const char* category,
	const char* text)
{
	time_t now = time(NULL);
	struct tm tmBuf;
	localtime_r(&now, &tmBuf);

	char timeStr[16];
	strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);

	BString line;
	line.SetToFormat("%s [%s] %s\n", timeStr, category, text);

	// Prune old lines
	if (fActivityLineCount >= kMaxActivityLines) {
		const char* textPtr = fActivityFeed->Text();
		const char* newline = strchr(textPtr, '\n');
		if (newline != NULL) {
			int32 removeLen = (int32)(newline - textPtr + 1);
			fActivityFeed->Delete(0, removeLen);
			fActivityLineCount--;
		}
	}

	// Append new line
	int32 insertPos = fActivityFeed->TextLength();
	fActivityFeed->Insert(insertPos, line.String(), line.Length());
	fActivityLineCount++;

	// Color the category tag
	rgb_color catColor = ui_color(B_DOCUMENT_TEXT_COLOR);
	if (strcmp(category, "MSG") == 0)
		catColor = (rgb_color){100, 180, 255, 255};
	else if (strcmp(category, "ADV") == 0)
		catColor = (rgb_color){80, 180, 80, 255};
	else if (strcmp(category, "SYS") == 0)
		catColor = (rgb_color){200, 170, 50, 255};
	else if (strcmp(category, "ERR") == 0)
		catColor = (rgb_color){200, 60, 60, 255};
	else if (strcmp(category, "ALT") == 0)
		catColor = (rgb_color){220, 80, 80, 255};

	int32 bracketStart = insertPos + 9;
	int32 bracketLen = strlen(category) + 2;
	BFont monoFont(be_fixed_font);
	monoFont.SetSize(10);
	fActivityFeed->SetFontAndColor(bracketStart, bracketStart + bracketLen,
		&monoFont, B_FONT_ALL, &catColor);

	fActivityFeed->ScrollToOffset(fActivityFeed->TextLength());
}
