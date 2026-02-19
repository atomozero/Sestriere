/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SNRChartView.cpp — Historical SNR line chart widget implementation
 */

#include "SNRChartView.h"

#include <Font.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>


// Chart layout constants
static const float kLeftMargin = 30.0f;
static const float kRightMargin = 8.0f;
static const float kTopMargin = 4.0f;
static const float kBottomMargin = 16.0f;
static const float kChartMinHeight = 80.0f;

// Theme-aware chart colors
static inline rgb_color ChartBg()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
}
static inline rgb_color GridColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT);
}
static inline rgb_color AxisLabelColor()
{
	return tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT);
}
static inline rgb_color LineColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}
static inline rgb_color DotColor()
{
	return tint_color(ui_color(B_CONTROL_HIGHLIGHT_COLOR), B_DARKEN_1_TINT);
}
static inline rgb_color FillColor()
{
	rgb_color c = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
	c.alpha = 40;
	return c;
}


SNRChartView::SNRChartView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fPoints(NULL),
	fPointCount(0),
	fMinSNR(-20),
	fMaxSNR(10),
	fMinTime(0),
	fMaxTime(0)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


SNRChartView::~SNRChartView()
{
	delete[] fPoints;
}


void
SNRChartView::AttachedToWindow()
{
	BView::AttachedToWindow();
}


BSize
SNRChartView::MinSize()
{
	return BSize(160, kChartMinHeight);
}


BSize
SNRChartView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, kChartMinHeight + 40);
}


void
SNRChartView::SetData(const BObjectList<SNRDataPoint, true>& points)
{
	delete[] fPoints;
	fPointCount = points.CountItems();

	if (fPointCount == 0) {
		fPoints = NULL;
		Invalidate();
		return;
	}

	fPoints = new DataPoint[fPointCount];
	fMinSNR = 127;
	fMaxSNR = -128;
	fMinTime = UINT32_MAX;
	fMaxTime = 0;

	for (int32 i = 0; i < fPointCount; i++) {
		SNRDataPoint* src = points.ItemAt(i);
		fPoints[i].timestamp = src->timestamp;
		fPoints[i].snr = src->snr;

		if (src->snr < fMinSNR)
			fMinSNR = src->snr;
		if (src->snr > fMaxSNR)
			fMaxSNR = src->snr;
		if (src->timestamp < fMinTime)
			fMinTime = src->timestamp;
		if (src->timestamp > fMaxTime)
			fMaxTime = src->timestamp;
	}

	// Ensure reasonable Y range (at least 10 dB span)
	if (fMaxSNR - fMinSNR < 10) {
		int8 mid = (fMaxSNR + fMinSNR) / 2;
		fMinSNR = mid - 5;
		fMaxSNR = mid + 5;
	}

	// Round to nice values
	fMinSNR = (fMinSNR / 5) * 5 - 5;
	fMaxSNR = ((fMaxSNR + 4) / 5) * 5 + 5;

	// Clamp to sensible range
	if (fMinSNR < -30)
		fMinSNR = -30;
	if (fMaxSNR > 20)
		fMaxSNR = 20;

	Invalidate();
}


void
SNRChartView::ClearData()
{
	delete[] fPoints;
	fPoints = NULL;
	fPointCount = 0;
	Invalidate();
}


void
SNRChartView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background
	SetLowColor(ViewColor());
	FillRect(bounds, B_SOLID_LOW);

	// Chart area
	BRect chartRect(
		bounds.left + kLeftMargin,
		bounds.top + kTopMargin,
		bounds.right - kRightMargin,
		bounds.bottom - kBottomMargin);

	// Chart background
	SetHighColor(ChartBg());
	FillRect(chartRect);

	// Border
	SetHighColor(GridColor());
	StrokeRect(chartRect);

	if (fPointCount == 0 || fPoints == NULL) {
		_DrawNoDataMessage(chartRect);
		return;
	}

	_DrawGrid(chartRect);
	_DrawLine(chartRect);
	_DrawAxisLabels(chartRect);
}


void
SNRChartView::_DrawGrid(BRect chartRect)
{
	SetHighColor(GridColor());

	// Horizontal grid lines (SNR values)
	int8 step = 5;
	for (int8 snr = fMinSNR; snr <= fMaxSNR; snr += step) {
		float y = _MapY(chartRect, snr);
		if (y >= chartRect.top && y <= chartRect.bottom) {
			// Dashed line
			for (float x = chartRect.left; x < chartRect.right; x += 6)
				StrokeLine(BPoint(x, y), BPoint(std::min(x + 3, chartRect.right), y));
		}
	}

	// Zero line (thicker)
	if (fMinSNR <= 0 && fMaxSNR >= 0) {
		float zeroY = _MapY(chartRect, 0);
		SetHighColor(tint_color(GridColor(), B_DARKEN_1_TINT));
		StrokeLine(BPoint(chartRect.left, zeroY),
			BPoint(chartRect.right, zeroY));
	}
}


void
SNRChartView::_DrawLine(BRect chartRect)
{
	if (fPointCount < 1)
		return;

	// Draw filled area under the line (semi-transparent)
	if (fPointCount >= 2) {
		SetHighColor(FillColor());
		SetDrawingMode(B_OP_ALPHA);

		BPoint points[fPointCount + 2];
		for (int32 i = 0; i < fPointCount; i++) {
			float x = _MapX(chartRect, fPoints[i].timestamp);
			float y = _MapY(chartRect, fPoints[i].snr);
			if (y < chartRect.top) y = chartRect.top;
			if (y > chartRect.bottom) y = chartRect.bottom;
			points[i] = BPoint(x, y);
		}
		// Close the polygon at the bottom
		points[fPointCount] = BPoint(
			_MapX(chartRect, fPoints[fPointCount - 1].timestamp),
			chartRect.bottom);
		points[fPointCount + 1] = BPoint(
			_MapX(chartRect, fPoints[0].timestamp),
			chartRect.bottom);

		FillPolygon(points, fPointCount + 2);
		SetDrawingMode(B_OP_COPY);
	}

	SetPenSize(1.5f);
	SetHighColor(LineColor());

	// Draw connected line
	BPoint prevPoint;
	for (int32 i = 0; i < fPointCount; i++) {
		float x = _MapX(chartRect, fPoints[i].timestamp);
		float y = _MapY(chartRect, fPoints[i].snr);

		// Clamp to chart area
		if (y < chartRect.top) y = chartRect.top;
		if (y > chartRect.bottom) y = chartRect.bottom;

		BPoint pt(x, y);

		if (i > 0) {
			SetHighColor(LineColor());
			StrokeLine(prevPoint, pt);
		}

		prevPoint = pt;
	}

	// Draw dots at each data point
	SetHighColor(DotColor());
	for (int32 i = 0; i < fPointCount; i++) {
		float x = _MapX(chartRect, fPoints[i].timestamp);
		float y = _MapY(chartRect, fPoints[i].snr);

		if (y < chartRect.top) y = chartRect.top;
		if (y > chartRect.bottom) y = chartRect.bottom;

		FillEllipse(BPoint(x, y), 2.5f, 2.5f);
	}

	SetPenSize(1.0f);
}


void
SNRChartView::_DrawAxisLabels(BRect chartRect)
{
	BFont smallFont;
	GetFont(&smallFont);
	smallFont.SetSize(9);
	SetFont(&smallFont);

	font_height fh;
	smallFont.GetHeight(&fh);

	SetHighColor(AxisLabelColor());

	// Y-axis labels (SNR values in dB)
	int8 step = 5;
	bool firstLabel = true;
	for (int8 snr = fMinSNR; snr <= fMaxSNR; snr += step) {
		float y = _MapY(chartRect, snr);
		if (y >= chartRect.top && y <= chartRect.bottom) {
			char label[12];
			if (firstLabel) {
				snprintf(label, sizeof(label), "%ddB", snr);
				firstLabel = false;
			} else {
				snprintf(label, sizeof(label), "%d", snr);
			}
			float tw = StringWidth(label);
			DrawString(label,
				BPoint(chartRect.left - tw - 3, y + fh.ascent / 2));
		}
	}

	// X-axis labels (time)
	if (fMinTime < fMaxTime) {
		// Start label
		struct tm tm;
		time_t t = (time_t)fMinTime;
		if (localtime_r(&t, &tm) != NULL) {
			char label[8];
			strftime(label, sizeof(label), "%H:%M", &tm);
			DrawString(label,
				BPoint(chartRect.left, chartRect.bottom + fh.ascent + 2));
		}

		// End label
		t = (time_t)fMaxTime;
		if (localtime_r(&t, &tm) != NULL) {
			char label[8];
			strftime(label, sizeof(label), "%H:%M", &tm);
			float tw = StringWidth(label);
			DrawString(label,
				BPoint(chartRect.right - tw, chartRect.bottom + fh.ascent + 2));
		}
	}
}


void
SNRChartView::_DrawNoDataMessage(BRect chartRect)
{
	BFont font;
	GetFont(&font);
	font.SetSize(10);
	SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);

	SetHighColor(AxisLabelColor());
	const char* msg = "No SNR data";
	float tw = StringWidth(msg);
	DrawString(msg,
		BPoint(chartRect.left + (chartRect.Width() - tw) / 2,
			chartRect.top + (chartRect.Height() + fh.ascent) / 2));
}


float
SNRChartView::_MapY(BRect chartRect, int8 snr) const
{
	if (fMaxSNR == fMinSNR)
		return chartRect.top + chartRect.Height() / 2;

	float ratio = (float)(snr - fMinSNR) / (float)(fMaxSNR - fMinSNR);
	// Invert Y (higher SNR = higher on chart = lower Y coordinate)
	return chartRect.bottom - ratio * chartRect.Height();
}


float
SNRChartView::_MapX(BRect chartRect, uint32 timestamp) const
{
	if (fMaxTime == fMinTime)
		return chartRect.left + chartRect.Width() / 2;

	float ratio = (float)(timestamp - fMinTime) / (float)(fMaxTime - fMinTime);
	return chartRect.left + ratio * chartRect.Width();
}
