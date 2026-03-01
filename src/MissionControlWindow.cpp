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
#include "Utils.h"


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
	if (snr > kSnrExcellent) return kColorGood;
	if (snr > kSnrGood) return (rgb_color){140, 200, 80, 255};
	if (snr > kSnrFair) return kColorFair;
	if (snr > kSnrPoor) return kColorPoor;
	return kColorBad;
}

static rgb_color
RssiColor(int8 rssi)
{
	if (rssi >= kRssiGood) return kColorGood;
	if (rssi >= kRssiFair) return kColorFair;
	if (rssi >= kRssiPoor) return kColorPoor;
	return kColorBad;
}

static rgb_color
BattColor(uint16 mv)
{
	if (mv >= kBattGoodMv) return kColorGood;
	if (mv >= kBattFairMv) return kColorFair;
	if (mv >= kBattLowMv) return kColorPoor;
	return kColorBad;
}

static rgb_color
ScoreColor(int32 score)
{
	if (score >= 75) return kColorGood;
	if (score >= 50) return kColorFair;
	if (score >= 25) return kColorPoor;
	return kColorBad;
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
			float labelWidth = StringWidth(fLabels[i]);
			float maxLabelWidth = bounds.Width() * 0.4f - 10;
			SetHighColor(dimColor);
			BString label(fLabels[i]);
			if (labelWidth > maxLabelWidth) {
				labelFont.TruncateString(&label, B_TRUNCATE_END,
					maxLabelWidth);
			}
			DrawString(label.String(), BPoint(10, textY));

			// Value (right-aligned, colored, truncated if needed)
			float maxValWidth = bounds.Width() * 0.58f - 10;
			BString value(fValues[i]);
			float valWidth = StringWidth(value.String());
			if (valWidth > maxValWidth) {
				labelFont.TruncateString(&value, B_TRUNCATE_MIDDLE,
					maxValWidth);
				valWidth = StringWidth(value.String());
			}
			SetHighColor(fValueColors[i]);
			DrawString(value.String(),
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
			if (fStoragePct < 60) fillColor = kColorGood;
			else if (fStoragePct < 80)
				fillColor = kColorFair;
			else fillColor = kColorBad;
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
		fRecent(0),
		fHeatmapCount(0),
		fShowHeatmap(false)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(80, 30));
		memset(fHeatmapSnr, 0, sizeof(fHeatmapSnr));
		memset(fHeatmapStatus, 0, sizeof(fHeatmapStatus));
	}

	void SetCounts(int32 total, int32 online, int32 recent)
	{
		fTotal = total;
		fOnline = online;
		fRecent = recent;
		Invalidate();
	}

	void SetHeatmapData(const int8* snrValues, const uint8* statuses,
		int32 count)
	{
		fHeatmapCount = count < 50 ? count : 50;
		memcpy(fHeatmapSnr, snrValues, fHeatmapCount * sizeof(int8));
		memcpy(fHeatmapStatus, statuses, fHeatmapCount * sizeof(uint8));
		fShowHeatmap = true;
		Invalidate();
	}

	void ClearHeatmap()
	{
		fHeatmapCount = 0;
		fShowHeatmap = false;
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

		// Dot grid below — use heatmap if available
		float dotSize = 7.0f;
		float gap = 3.0f;
		float dotY = fh.ascent + fh.descent + 6;
		int32 cols = (int32)((bounds.Width() - 4) / (dotSize + gap));
		if (cols < 1) cols = 1;

		float x = 4;
		float y = dotY;
		int32 drawn = 0;

		int32 drawCount = fTotal < 50 ? fTotal : 50;

		for (int32 i = 0; i < drawCount; i++) {
			rgb_color dotColor;

			if (fShowHeatmap && i < fHeatmapCount
				&& fHeatmapStatus[i] >= 1) {
				// Heatmap mode: color by SNR quality
				dotColor = SnrColor(fHeatmapSnr[i]);
			} else if (fShowHeatmap && i < fHeatmapCount) {
				// Offline with heatmap active
				dotColor = tint_color(bg, B_DARKEN_2_TINT);
			} else {
				// Fallback: status-only coloring
				if (i < fOnline)
					dotColor = kColorGood;
				else if (i < fOnline + fRecent)
					dotColor = kColorFair;
				else
					dotColor = tint_color(bg, B_DARKEN_2_TINT);
			}

			SetHighColor(dotColor);
			FillEllipse(BRect(x, y, x + dotSize, y + dotSize));

			// Status ring for online contacts in heatmap mode
			if (fShowHeatmap && i < fHeatmapCount
				&& fHeatmapStatus[i] == 2) {
				rgb_color ringColor = tint_color(dotColor,
					B_DARKEN_2_TINT);
				SetHighColor(ringColor);
				StrokeEllipse(BRect(x - 1, y - 1,
					x + dotSize + 1, y + dotSize + 1));
			}

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
	int8		fHeatmapSnr[50];
	uint8		fHeatmapStatus[50];
	int32		fHeatmapCount;
	bool		fShowHeatmap;
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
		SetHighColor(kColorGood);
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
// SessionTimelineView — Horizontal timeline of session events
// ============================================================================

static const int32 kMaxTimelineEvents = 200;

struct TimelineEvent {
	bigtime_t	time;
	uint8		type;	// 0=sys, 1=msg, 2=adv, 3=err
	char		label[32];
};

class SessionTimelineView : public BView {
public:
	SessionTimelineView()
		:
		BView("timeline", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fEventCount(0),
		fSessionStart(0)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 36));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 36));
	}

	void SetSessionStart(bigtime_t startTime)
	{
		fSessionStart = startTime;
	}

	void AddEvent(uint8 type, const char* label)
	{
		if (fSessionStart == 0)
			fSessionStart = system_time();

		if (fEventCount >= kMaxTimelineEvents) {
			// Shift events left
			memmove(&fEvents[0], &fEvents[1],
				(kMaxTimelineEvents - 1) * sizeof(TimelineEvent));
			fEventCount--;
		}

		TimelineEvent& ev = fEvents[fEventCount++];
		ev.time = system_time();
		ev.type = type;
		snprintf(ev.label, sizeof(ev.label), "%s", label);
		Invalidate();
	}

	void Clear()
	{
		fEventCount = 0;
		fSessionStart = 0;
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
		rgb_color dimColor = tint_color(textColor, B_LIGHTEN_1_TINT);
		rgb_color lineColor = tint_color(bg, B_DARKEN_2_TINT);

		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		float left = 4;
		float right = bounds.right - 4;
		float lineY = bounds.Height() / 2;
		float barW = right - left;

		// Title
		BFont titleFont;
		GetFont(&titleFont);
		titleFont.SetSize(8);
		SetFont(&titleFont);
		SetHighColor(dimColor);
		DrawString("Session", BPoint(left, 9));
		left += StringWidth("Session") + 6;
		barW = right - left;

		// Timeline base line
		SetHighColor(lineColor);
		StrokeLine(BPoint(left, lineY), BPoint(right, lineY));

		if (fEventCount == 0 || fSessionStart == 0)
			return;

		bigtime_t now = system_time();
		bigtime_t span = now - fSessionStart;
		if (span < 1000000) span = 1000000;  // At least 1s

		// Time labels
		SetHighColor(dimColor);
		DrawString("0", BPoint(left, bounds.bottom - 2));
		char endLabel[16];
		int32 totalSec = (int32)(span / 1000000);
		if (totalSec < 60)
			snprintf(endLabel, sizeof(endLabel), "%ds", (int)totalSec);
		else if (totalSec < 3600)
			snprintf(endLabel, sizeof(endLabel), "%dm",
				(int)(totalSec / 60));
		else
			snprintf(endLabel, sizeof(endLabel), "%dh",
				(int)(totalSec / 3600));
		float endW = StringWidth(endLabel);
		DrawString(endLabel, BPoint(right - endW, bounds.bottom - 2));

		// Draw event markers
		for (int32 i = 0; i < fEventCount; i++) {
			float t = (float)(fEvents[i].time - fSessionStart) / span;
			if (t < 0) t = 0;
			if (t > 1) t = 1;
			float x = left + t * barW;

			rgb_color evColor;
			float markerH;
			switch (fEvents[i].type) {
				case 1:  // Message
					evColor = (rgb_color){100, 180, 255, 255};
					markerH = 8;
					break;
				case 2:  // Advert
					evColor = kColorGood;
					markerH = 6;
					break;
				case 3:  // Error
					evColor = kColorBad;
					markerH = 10;
					break;
				default: // System
					evColor = kColorFair;
					markerH = 5;
					break;
			}

			SetHighColor(evColor);
			StrokeLine(BPoint(x, lineY - markerH),
				BPoint(x, lineY + markerH));
		}

		// "Now" marker
		SetHighColor(textColor);
		FillRect(BRect(right - 2, lineY - 4, right, lineY + 4));
	}

private:
	TimelineEvent	fEvents[kMaxTimelineEvents];
	int32			fEventCount;
	bigtime_t		fSessionStart;
};


// ============================================================================
// MiniTopoView — Compact network topology (self + contacts radial)
// ============================================================================

static const int32 kMaxTopoNodes = 32;

class MiniTopoView : public BView {
public:
	MiniTopoView()
		:
		BView("miniTopo", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fNodeCount(0)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(120, 80));
	}

	void SetNodes(const TopoNode* nodes, int32 count)
	{
		fNodeCount = count < kMaxTopoNodes ? count : kMaxTopoNodes;
		for (int32 i = 0; i < fNodeCount; i++)
			fNodes[i] = nodes[i];
		Invalidate();
	}

	void ClearNodes()
	{
		fNodeCount = 0;
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);

		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		float cx = bounds.Width() / 2;
		float cy = bounds.Height() / 2;

		// Draw title
		BFont titleFont;
		GetFont(&titleFont);
		titleFont.SetSize(9);
		titleFont.SetFace(B_BOLD_FACE);
		SetFont(&titleFont);
		SetHighColor(tint_color(textColor, B_LIGHTEN_1_TINT));
		float titleW = StringWidth("Topology");
		DrawString("Topology", BPoint(cx - titleW / 2, 12));

		// Self node (center)
		rgb_color selfColor = {100, 180, 255, 255};
		SetHighColor(selfColor);
		FillEllipse(BPoint(cx, cy), 8, 8);
		SetHighColor(tint_color(selfColor, B_DARKEN_2_TINT));
		StrokeEllipse(BPoint(cx, cy), 8, 8);

		// "Self" label
		BFont labelFont;
		GetFont(&labelFont);
		labelFont.SetSize(8);
		labelFont.SetFace(B_REGULAR_FACE);
		SetFont(&labelFont);
		SetHighColor(textColor);
		float selfW = StringWidth("Self");
		DrawString("Self", BPoint(cx - selfW / 2, cy + 18));

		if (fNodeCount == 0)
			return;

		// Compute radius for contact ring
		float maxR = (cx < cy ? cx : cy) - 28;
		if (maxR < 30) maxR = 30;

		float angleStep = (2.0f * M_PI) / fNodeCount;
		float startAngle = -M_PI / 2;  // Top

		for (int32 i = 0; i < fNodeCount; i++) {
			float angle = startAngle + i * angleStep;
			float nx = cx + maxR * cosf(angle);
			float ny = cy + maxR * sinf(angle);

			// Connection line to center
			rgb_color lineColor;
			float penW;
			switch (fNodes[i].status) {
				case 2:  // Online
					lineColor = kColorGood;
					penW = 2.0f;
					break;
				case 1:  // Recent
					lineColor = kColorFair;
					penW = 1.5f;
					break;
				default: // Offline
					lineColor = tint_color(bg, B_DARKEN_2_TINT);
					penW = 1.0f;
					break;
			}

			SetPenSize(penW);
			SetHighColor(lineColor);
			StrokeLine(BPoint(cx, cy), BPoint(nx, ny));
			SetPenSize(1.0f);

			// Node dot
			float dotR = 4.0f;
			rgb_color nodeColor;
			switch (fNodes[i].status) {
				case 2:
					nodeColor = kColorGood;
					break;
				case 1:
					nodeColor = kColorFair;
					break;
				default:
					nodeColor = tint_color(bg, B_DARKEN_3_TINT);
					break;
			}
			SetHighColor(nodeColor);
			FillEllipse(BPoint(nx, ny), dotR, dotR);

			// Name label (truncated)
			SetFont(&labelFont);
			SetHighColor(textColor);
			char shortName[12];
			strncpy(shortName, fNodes[i].name, 11);
			shortName[11] = '\0';

			float nameW = StringWidth(shortName);
			float labelX = nx - nameW / 2;
			float labelY;
			if (ny < cy)
				labelY = ny - dotR - 2;
			else
				labelY = ny + dotR + 9;

			// Clamp to bounds
			if (labelX < 2) labelX = 2;
			if (labelX + nameW > bounds.right - 2)
				labelX = bounds.right - 2 - nameW;

			DrawString(shortName, BPoint(labelX, labelY));

			// Sparkline (small SNR history line chart)
			if (fNodes[i].snrHistoryCount > 1) {
				float sparkW = 20.0f;
				float sparkH = 8.0f;
				float sparkX = nx + dotR + 3;
				float sparkY = ny - sparkH / 2;

				// Adjust position to avoid going off-screen
				if (sparkX + sparkW > bounds.right - 2)
					sparkX = nx - dotR - sparkW - 3;

				int32 cnt = fNodes[i].snrHistoryCount;
				float stepX = sparkW / (cnt - 1);

				// Find min/max for normalization
				int8 minSnr = fNodes[i].snrHistory[0];
				int8 maxSnr = fNodes[i].snrHistory[0];
				for (int32 j = 1; j < cnt; j++) {
					if (fNodes[i].snrHistory[j] < minSnr)
						minSnr = fNodes[i].snrHistory[j];
					if (fNodes[i].snrHistory[j] > maxSnr)
						maxSnr = fNodes[i].snrHistory[j];
				}
				if (maxSnr == minSnr) maxSnr = minSnr + 1;

				SetPenSize(1.0f);
				SetHighColor(SnrColor(fNodes[i].snr));
				BPoint prev(sparkX,
					sparkY + sparkH - (sparkH *
					(fNodes[i].snrHistory[0] - minSnr))
					/ (maxSnr - minSnr));
				for (int32 j = 1; j < cnt; j++) {
					float sx = sparkX + j * stepX;
					float sy = sparkY + sparkH - (sparkH *
						(fNodes[i].snrHistory[j] - minSnr))
						/ (maxSnr - minSnr);
					StrokeLine(prev, BPoint(sx, sy));
					prev.Set(sx, sy);
				}
			}
		}
	}

private:
	TopoNode	fNodes[kMaxTopoNodes];
	int32		fNodeCount;
};


// ============================================================================
// MissionControlWindow
// ============================================================================

MissionControlWindow::MissionControlWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 799, 519), "Mission Control",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fParent(parent),
	fAlertBanner(NULL),
	fDeviceCard(NULL),
	fRadioCard(NULL),
	fHealthScore(NULL),
	fContactGrid(NULL),
	fSNRChart(NULL),
	fPacketRateChart(NULL),
	fMiniTopo(NULL),
	fTimeline(NULL),
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
	fBatteryType(BATTERY_LIPO),
	fRssi(0),
	fSnr(0),
	fContactsTotal(0),
	fContactsOnline(0),
	fUsedKb(0),
	fTotalKb(0),
	fUptime(0),
	fTxPackets(0),
	fRxPackets(0),
	fRefreshTimer(NULL),
	fPulseTimer(NULL),
	fAlertFlashTimer(NULL)
{
	_BuildLayout();

	if (fParent != NULL)
		CenterIn(fParent->Frame());
	else
		CenterOnScreen();

	// Refresh timer (10 seconds) — requests stats
	BMessage tickMsg(MSG_REFRESH_TICK);
	fRefreshTimer = new BMessageRunner(BMessenger(this), &tickMsg,
		10000000, -1);

	// Pulse timer (200ms) — pulsing dot + last-update
	BMessage pulseMsg(MSG_PULSE_TICK);
	fPulseTimer = new BMessageRunner(BMessenger(this), &pulseMsg,
		200000, -1);

	// Alert flash timer (800ms)
	BMessage flashMsg(MSG_ALERT_FLASH);
	fAlertFlashTimer = new BMessageRunner(BMessenger(this), &flashMsg,
		800000, -1);
}


MissionControlWindow::~MissionControlWindow()
{
	delete fRefreshTimer;
	fRefreshTimer = NULL;
	delete fPulseTimer;
	fPulseTimer = NULL;
	delete fAlertFlashTimer;
	fAlertFlashTimer = NULL;
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
		kColorBad);
	fDeviceCard->SetRow(1, "Battery", "--");
	fDeviceCard->SetRow(2, "Uptime", "--");
	fDeviceCard->SetRow(3, "Firmware", "--");

	fRadioCard = new MetricCardView("Radio Health");
	fRadioCard->SetRow(0, "RSSI", "-- dBm");
	fRadioCard->SetRow(1, "SNR", "-- dB");
	fRadioCard->SetRow(2, "Noise Floor", "-- dBm");
	fRadioCard->SetRow(3, "TX Power", "-- dBm");
	fRadioCard->SetRow(4, "Frequency", "--");
	fRadioCard->SetRow(5, "Bandwidth", "--");

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

	// === Middle row charts + mini topology ===
	fSNRChart = new DashboardSNRView();
	fPacketRateChart = new PacketRateView();
	fMiniTopo = new MiniTopoView();

	// === Session timeline ===
	fTimeline = new SessionTimelineView();

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
		.Add(fMiniTopo, 1)
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
		.Add(fTimeline)
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
			kColorGood);
		fDeviceCard->SetPulse(true);
		fTimeline->SetSessionStart(system_time());
		fAdvertButton->SetEnabled(true);
		fSyncButton->SetEnabled(true);
		fStatsButton->SetEnabled(true);
	} else {
		fDeviceCard->SetRow(0, "Connection", "Disconnected",
			kColorBad);
		fDeviceCard->SetPulse(false);
		fDeviceCard->SetRow(1, "Battery", "--");
		fDeviceCard->SetRow(2, "Uptime", "--");
		fDeviceCard->SetRow(3, "Firmware", "--");
		fDeviceCard->SetStorageGauge(-1);
		fRadioCard->SetRow(0, "RSSI", "-- dBm");
		fRadioCard->SetRow(1, "SNR", "-- dB");
		fRadioCard->SetRow(2, "Noise Floor", "-- dBm");
		fRadioCard->SetRow(4, "Frequency", "--");
		fRadioCard->SetRow(5, "Bandwidth", "--");
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
		fMiniTopo->ClearNodes();
		fTimeline->Clear();
		fContactGrid->ClearHeatmap();
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

	int32 pct = BatteryPercent(battMv, (BatteryChemistry)fBatteryType);

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
MissionControlWindow::SetBatteryType(uint8 type)
{
	fBatteryType = type;
}


void
MissionControlWindow::SetDeviceStats(uint32 uptime, uint32 txPackets,
	uint32 rxPackets)
{
	fUptime = uptime;
	fTxPackets = txPackets;
	fRxPackets = rxPackets;

	char uptimeStr[32];
	FormatUptime(uptimeStr, sizeof(uptimeStr), uptime);
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

	char freqStr[24];
	snprintf(freqStr, sizeof(freqStr), "%.3f MHz",
		freqHz / 1000000.0);
	fRadioCard->SetRow(4, "Frequency", freqStr);

	char bwStr[24];
	if (bwHz >= 1000000)
		snprintf(bwStr, sizeof(bwStr), "%.1f MHz SF%d CR4/%d",
			bwHz / 1000000.0, (int)sf, (int)(cr + 4));
	else
		snprintf(bwStr, sizeof(bwStr), "%.0f kHz SF%d CR4/%d",
			bwHz / 1000.0, (int)sf, (int)(cr + 4));
	fRadioCard->SetRow(5, "Bandwidth", bwStr);

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
MissionControlWindow::SetContactNodes(const TopoNode* nodes, int32 count)
{
	fMiniTopo->SetNodes(nodes, count);
}


void
MissionControlWindow::SetContactHeatmap(const int8* snrValues,
	const uint8* statuses, int32 count)
{
	fContactGrid->SetHeatmapData(snrValues, statuses, count);
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

	// Feed the session timeline
	uint8 evType = 0;  // SYS
	if (strcmp(category, "MSG") == 0) evType = 1;
	else if (strcmp(category, "ADV") == 0) evType = 2;
	else if (strcmp(category, "ERR") == 0 || strcmp(category, "ALT") == 0)
		evType = 3;
	fTimeline->AddEvent(evType, text);
}


// ============================================================================
// Private helpers
// ============================================================================

void
MissionControlWindow::_RecalcHealthScore()
{
	int32 score = 0;

	if (fConnected) score += 25;

	score += BatteryPercent(fBatteryMv, (BatteryChemistry)fBatteryType)
		* 15 / 100;

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

	if (fBatteryMv > 0 && fBatteryMv < kBattLowMv) {
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
		fLastUpdateLabel->SetHighColor(kColorBad);
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
		catColor = kColorGood;
	else if (strcmp(category, "SYS") == 0)
		catColor = kColorFair;
	else if (strcmp(category, "ERR") == 0)
		catColor = kColorBad;
	else if (strcmp(category, "ALT") == 0)
		catColor = kColorBad;

	int32 bracketStart = insertPos + 9;
	int32 bracketLen = strlen(category) + 2;
	BFont monoFont(be_fixed_font);
	monoFont.SetSize(10);
	fActivityFeed->SetFontAndColor(bracketStart, bracketStart + bracketLen,
		&monoFont, B_FONT_ALL, &catColor);

	fActivityFeed->ScrollToOffset(fActivityFeed->TextLength());
}
