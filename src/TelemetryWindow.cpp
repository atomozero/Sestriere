/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TelemetryWindow.cpp — Sensor telemetry dashboard (card-based redesign)
 */

#include "TelemetryWindow.h"

#include <Alert.h>
#include <File.h>
#include <FindDirectory.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <ScrollBar.h>
#include <SeparatorView.h>

#include <cmath>
#include <cstdio>
#include <cfloat>
#include <climits>
#include <ctime>

#include "Constants.h"
#include "DatabaseManager.h"


static const int32 kSparklineMax = 20;
static const float kCardWidth = 180;
static const float kCardHeight = 94;
static const float kChartHeight = 200;

static const bigtime_t kTimeRanges[] = {
	60LL * 1000000,				// 1 min
	5LL * 60 * 1000000,			// 5 min
	15LL * 60 * 1000000,		// 15 min
	60LL * 60 * 1000000,		// 1 hour
	6LL * 60 * 60 * 1000000,	// 6 hours
	24LL * 60 * 60 * 1000000,	// 24 hours
	7LL * 24 * 60 * 60 * 1000000	// 7 days
};

static const char* kTimeRangeLabels[] = {
	"1m", "5m", "15m", "1h", "6h", "24h", "7d"
};


// Mix a sensor color with panel background (80%/20%)
static rgb_color
ThemeAccent(uint8 r, uint8 g, uint8 b)
{
	rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color c;
	c.red = (uint8)(r * 0.8f + bg.red * 0.2f);
	c.green = (uint8)(g * 0.8f + bg.green * 0.2f);
	c.blue = (uint8)(b * 0.8f + bg.blue * 0.2f);
	c.alpha = 255;
	return c;
}


// ============================================================================
// TelemetrySectionHeader — node group header with underline
// ============================================================================

class TelemetrySectionHeader : public BView {
public:
	TelemetrySectionHeader(const char* title, int32 sensorCount)
		:
		BView("section_header", B_WILL_DRAW),
		fSensorCount(sensorCount)
	{
		strlcpy(fTitle, title ? title : "", sizeof(fTitle));
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 28));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 28));
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		// Title (bold)
		BFont font;
		GetFont(&font);
		font.SetSize(12);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);

		font_height fh;
		font.GetHeight(&fh);
		float baseline = bounds.top + fh.ascent + 4;

		rgb_color accent = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		SetHighColor(accent);
		DrawString(fTitle, BPoint(4, baseline));

		// Sensor count
		font.SetSize(10);
		font.SetFace(B_REGULAR_FACE);
		SetFont(&font);

		char countStr[32];
		snprintf(countStr, sizeof(countStr), " (%d sensor%s)",
			(int)fSensorCount, fSensorCount == 1 ? "" : "s");

		rgb_color dim = tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_LIGHTEN_1_TINT);
		SetHighColor(dim);

		BFont boldFont;
		GetFont(&boldFont);
		boldFont.SetSize(12);
		boldFont.SetFace(B_BOLD_FACE);
		float nameW = boldFont.StringWidth(fTitle);
		DrawString(countStr, BPoint(4 + nameW, baseline));

		// Underline
		float y = baseline + fh.descent + 2;
		SetHighColor(accent);
		SetPenSize(2.0);
		StrokeLine(BPoint(4, y), BPoint(bounds.right - 4, y));
	}

private:
	char		fTitle[64];
	int32		fSensorCount;
};


// ============================================================================
// SensorCardView — compact card with sparkline for a single sensor
// ============================================================================

class SensorCardView : public BView {
public:
	SensorCardView(SensorInfo* sensor, BWindow* target)
		:
		BView("sensor_card", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fSensor(sensor),
		fTarget(target),
		fSelected(false),
		fHovered(false)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(kCardWidth, kCardHeight));
		SetExplicitPreferredSize(BSize(kCardWidth, kCardHeight));
		SetExplicitMaxSize(BSize(kCardWidth, kCardHeight));
		SetEventMask(0, 0);
	}

	void SetSelected(bool selected)
	{
		if (fSelected != selected) {
			fSelected = selected;
			Invalidate();
		}
	}

	bool IsSelected() const { return fSelected; }

	void UpdateFromSensor()
	{
		Invalidate();
	}

	SensorInfo* Sensor() const { return fSensor; }

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color text = ui_color(B_PANEL_TEXT_COLOR);
		rgb_color dim = tint_color(text, B_LIGHTEN_1_TINT);
		rgb_color border = tint_color(bg, B_DARKEN_1_TINT);
		rgb_color cardBg = tint_color(bg, 1.03f);

		SetLowColor(cardBg);
		FillRect(bounds, B_SOLID_LOW);

		// Border or selection highlight
		if (fSelected) {
			rgb_color sel = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
			SetHighColor(sel);
			SetPenSize(2.0);
			StrokeRoundRect(bounds.InsetByCopy(1, 1), 4, 4);
		} else if (fHovered) {
			rgb_color hover = tint_color(bg, B_DARKEN_1_TINT);
			SetHighColor(hover);
			SetPenSize(1.0);
			StrokeRoundRect(bounds, 4, 4);
		} else {
			SetHighColor(border);
			SetPenSize(1.0);
			StrokeRoundRect(bounds, 4, 4);
		}

		// Color strip (4px left edge)
		BRect strip(bounds.left + 1, bounds.top + 4,
			bounds.left + 4, bounds.bottom - 4);
		SetHighColor(fSensor->color);
		FillRect(strip);

		float x = bounds.left + 10;

		// Sensor name (bold, small)
		BFont nameFont(be_bold_font);
		nameFont.SetSize(10);
		SetFont(&nameFont);
		font_height fh;
		nameFont.GetHeight(&fh);
		float y = bounds.top + fh.ascent + 6;
		SetHighColor(dim);
		DrawString(fSensor->name.String(), BPoint(x, y));

		// Current value (large)
		BFont valFont(be_bold_font);
		valFont.SetSize(16);
		SetFont(&valFont);
		font_height vfh;
		valFont.GetHeight(&vfh);
		y += vfh.ascent + 4;
		SetHighColor(text);

		char valStr[32];
		if (fabsf(fSensor->currentValue) >= 1000)
			snprintf(valStr, sizeof(valStr), "%.0f", fSensor->currentValue);
		else if (fabsf(fSensor->currentValue) >= 100)
			snprintf(valStr, sizeof(valStr), "%.1f", fSensor->currentValue);
		else
			snprintf(valStr, sizeof(valStr), "%.2f", fSensor->currentValue);
		DrawString(valStr, BPoint(x, y));

		// Unit (right of value, smaller)
		BFont unitFont(be_plain_font);
		unitFont.SetSize(10);
		SetFont(&unitFont);
		float valW = valFont.StringWidth(valStr);
		SetHighColor(dim);
		DrawString(fSensor->unit.String(), BPoint(x + valW + 4, y));

		// Min/Max/Avg line
		BFont statsFont(be_plain_font);
		statsFont.SetSize(9);
		SetFont(&statsFont);
		font_height sfh;
		statsFont.GetHeight(&sfh);
		y += sfh.ascent + 4;

		char statsStr[64];
		if (fSensor->minValue != FLT_MAX) {
			snprintf(statsStr, sizeof(statsStr), "%.1f / %.1f / %.1f",
				fSensor->minValue, fSensor->avgValue, fSensor->maxValue);
		} else {
			snprintf(statsStr, sizeof(statsStr), "-- / -- / --");
		}
		SetHighColor(dim);
		DrawString(statsStr, BPoint(x, y));

		// Sparkline (bottom area)
		if (fSensor->sparklineCount > 1) {
			float sparkLeft = x;
			float sparkRight = bounds.right - 8;
			float sparkTop = y + 6;
			float sparkBottom = bounds.bottom - 6;
			float sparkH = sparkBottom - sparkTop;
			float sparkW = sparkRight - sparkLeft;

			if (sparkH > 4 && sparkW > 10) {
				rgb_color lineColor = fSensor->color;
				SetHighColor(lineColor);
				SetPenSize(1.5);

				int32 count = fSensor->sparklineCount;
				float step = sparkW / (count - 1);

				BPoint prev(sparkLeft,
					sparkBottom - fSensor->sparkline[0] * sparkH);
				for (int32 i = 1; i < count; i++) {
					BPoint cur(sparkLeft + i * step,
						sparkBottom - fSensor->sparkline[i] * sparkH);
					StrokeLine(prev, cur);
					prev = cur;
				}
			}
		}

		SetPenSize(1.0);
	}

	void MouseDown(BPoint where)
	{
		BMessage msg(MSG_TELEMETRY_CARD_CLICKED);
		msg.AddPointer("sensor", fSensor);
		BMessenger(fTarget).SendMessage(&msg);
	}

	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* /*dragMessage*/)
	{
		if (transit == B_ENTERED_VIEW && !fHovered) {
			fHovered = true;
			Invalidate();
		} else if (transit == B_EXITED_VIEW && fHovered) {
			fHovered = false;
			Invalidate();
		}
	}

private:
	SensorInfo*	fSensor;
	BWindow*	fTarget;
	bool		fSelected;
	bool		fHovered;
};


// ============================================================================
// TelemetryChartView — theme-aware detail chart with interactive cursor
// ============================================================================

class TelemetryChartView : public BView {
public:
	TelemetryChartView()
		:
		BView("telemetry_chart", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fSensor(NULL),
		fTimeRange(60 * 1000000LL),
		fShowCursor(false)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR, B_DARKEN_1_TINT);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 0));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 0));
	}

	void SetSensor(SensorInfo* sensor)
	{
		fSensor = sensor;
		fShowCursor = false;
		Invalidate();
	}

	void SetTimeRange(bigtime_t range)
	{
		fTimeRange = range;
		Invalidate();
	}

	void SetVisible(bool visible)
	{
		float h = visible ? kChartHeight : 0;
		SetExplicitMinSize(BSize(B_SIZE_UNSET, h));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, h));
		InvalidateLayout();
		Invalidate();
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		if (bounds.Height() < 10)
			return;

		rgb_color bg = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_1_TINT);
		rgb_color grid = tint_color(bg, B_DARKEN_2_TINT);
		rgb_color text = ui_color(B_PANEL_TEXT_COLOR);
		rgb_color dim = tint_color(text, B_LIGHTEN_1_TINT);

		float marginL = 55, marginR = 15, marginT = 18, marginB = 32;
		BRect gr(bounds.left + marginL, bounds.top + marginT,
			bounds.right - marginR, bounds.bottom - marginB);
		fGraphRect = gr;

		// Background
		SetLowColor(bg);
		FillRect(bounds, B_SOLID_LOW);

		// Graph area slightly darker
		rgb_color graphBg = tint_color(bg, 1.04f);
		SetHighColor(graphBg);
		FillRect(gr);

		// Grid lines
		SetHighColor(grid);
		SetPenSize(1.0);
		int vLines = 6;
		float vStep = gr.Width() / vLines;
		for (int i = 0; i <= vLines; i++) {
			float x = gr.left + i * vStep;
			StrokeLine(BPoint(x, gr.top), BPoint(x, gr.bottom));
		}
		int hLines = 4;
		float hStep = gr.Height() / hLines;
		for (int i = 0; i <= hLines; i++) {
			float y = gr.top + i * hStep;
			StrokeLine(BPoint(gr.left, y), BPoint(gr.right, y));
		}

		// Border
		rgb_color borderColor = tint_color(bg, B_DARKEN_2_TINT);
		SetHighColor(borderColor);
		StrokeRect(gr);

		// Time labels
		BFont labelFont(be_plain_font);
		labelFont.SetSize(9);
		SetFont(&labelFont);
		SetHighColor(dim);
		SetLowColor(bg);

		for (int i = 0; i <= vLines; i++) {
			float x = gr.left + i * vStep;
			bigtime_t offset = (bigtime_t)((vLines - i)
				* fTimeRange / vLines);
			int seconds = (int)(offset / 1000000);
			char label[32];
			if (seconds >= 3600)
				snprintf(label, sizeof(label), "-%dh", seconds / 3600);
			else if (seconds >= 60)
				snprintf(label, sizeof(label), "-%dm", seconds / 60);
			else
				snprintf(label, sizeof(label), "-%ds", seconds);
			float lw = StringWidth(label);
			DrawString(label, BPoint(x - lw / 2, gr.bottom + 14));
		}

		if (fSensor == NULL)
			return;

		// Value labels
		float minVal = fSensor->minValue;
		float maxVal = fSensor->maxValue;
		if (minVal == FLT_MAX || maxVal == -FLT_MAX)
			return;
		if (minVal == maxVal) {
			minVal -= 1;
			maxVal += 1;
		}
		float range = maxVal - minVal;
		minVal -= range * 0.1f;
		maxVal += range * 0.1f;

		SetHighColor(dim);
		for (int i = 0; i <= hLines; i++) {
			float y = gr.top + i * hStep;
			float val = maxVal - (i * (maxVal - minVal) / hLines);
			char label[32];
			if (fabsf(val) >= 1000)
				snprintf(label, sizeof(label), "%.0f", val);
			else if (fabsf(val) >= 100)
				snprintf(label, sizeof(label), "%.1f", val);
			else
				snprintf(label, sizeof(label), "%.2f", val);
			float lw = StringWidth(label);
			DrawString(label, BPoint(gr.left - lw - 4, y + 4));
		}

		// Legend
		BFont legendFont(be_bold_font);
		legendFont.SetSize(10);
		SetFont(&legendFont);
		SetHighColor(fSensor->color);
		BString legend;
		legend << fSensor->name << " (" << fSensor->unit << ")";
		DrawString(legend.String(), BPoint(gr.left + 8, gr.top + 12));

		// Data line + alpha fill
		_DrawDataLine(gr, minVal, maxVal);

		// Cursor
		if (fShowCursor)
			_DrawCursor(gr, minVal, maxVal);
	}

	void MouseDown(BPoint where)
	{
		if (fGraphRect.Contains(where)) {
			fCursorPos = where;
			fShowCursor = true;
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
			Invalidate();
		}
	}

	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* /*dragMessage*/)
	{
		if (fShowCursor) {
			if (transit == B_EXITED_VIEW) {
				fShowCursor = false;
			} else {
				fCursorPos.x = where.x;
				if (fCursorPos.x < fGraphRect.left)
					fCursorPos.x = fGraphRect.left;
				if (fCursorPos.x > fGraphRect.right)
					fCursorPos.x = fGraphRect.right;
			}
			Invalidate();
		}
	}

private:
	void _DrawDataLine(BRect gr, float minVal, float maxVal)
	{
		if (fSensor == NULL || fSensor->history.CountItems() < 2)
			return;

		bigtime_t now = system_time();
		bigtime_t startTime = now - fTimeRange;
		float valRange = maxVal - minVal;

		// Collect visible points
		SetHighColor(fSensor->color);
		SetPenSize(2.0);

		BPoint prev;
		bool havePrev = false;

		for (int32 i = 0; i < fSensor->history.CountItems(); i++) {
			TelemetryDataPoint* pt = fSensor->history.ItemAt(i);
			if (pt->timestamp < startTime)
				continue;

			float x = gr.left + (float)(pt->timestamp - startTime)
				/ fTimeRange * gr.Width();
			float y = gr.bottom - (pt->value - minVal)
				/ valRange * gr.Height();
			if (y < gr.top) y = gr.top;
			if (y > gr.bottom) y = gr.bottom;

			BPoint cur(x, y);
			if (havePrev)
				StrokeLine(prev, cur);
			prev = cur;
			havePrev = true;
		}

		// Draw data dots
		SetPenSize(1.0);
		for (int32 i = 0; i < fSensor->history.CountItems(); i++) {
			TelemetryDataPoint* pt = fSensor->history.ItemAt(i);
			if (pt->timestamp < startTime)
				continue;

			float x = gr.left + (float)(pt->timestamp - startTime)
				/ fTimeRange * gr.Width();
			float y = gr.bottom - (pt->value - minVal)
				/ valRange * gr.Height();
			if (y < gr.top) y = gr.top;
			if (y > gr.bottom) y = gr.bottom;

			rgb_color dotBg = tint_color(
				ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
			SetHighColor(dotBg);
			FillEllipse(BPoint(x, y), 3, 3);
			SetHighColor(fSensor->color);
			StrokeEllipse(BPoint(x, y), 3, 3);
		}
	}

	void _DrawCursor(BRect gr, float minVal, float maxVal)
	{
		rgb_color cursorColor = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		SetHighColor(cursorColor);
		SetPenSize(1.0);
		StrokeLine(BPoint(fCursorPos.x, gr.top),
			BPoint(fCursorPos.x, gr.bottom));

		if (fSensor == NULL || fSensor->history.CountItems() == 0)
			return;

		bigtime_t now = system_time();
		bigtime_t startTime = now - fTimeRange;
		bigtime_t cursorTime = startTime + (bigtime_t)(
			(fCursorPos.x - gr.left) / gr.Width() * fTimeRange);

		TelemetryDataPoint* closest = NULL;
		bigtime_t closestDiff = LLONG_MAX;

		for (int32 i = 0; i < fSensor->history.CountItems(); i++) {
			TelemetryDataPoint* pt = fSensor->history.ItemAt(i);
			bigtime_t diff = llabs(pt->timestamp - cursorTime);
			if (diff < closestDiff) {
				closestDiff = diff;
				closest = pt;
			}
		}

		if (closest != NULL && closestDiff < fTimeRange / 10) {
			char tooltip[64];
			snprintf(tooltip, sizeof(tooltip), "%.2f %s",
				closest->value, fSensor->unit.String());

			BFont font(be_plain_font);
			font.SetSize(10);
			SetFont(&font);

			float tw = StringWidth(tooltip) + 10;
			float th = 18;

			BRect tr(fCursorPos.x - tw / 2, gr.top - th - 4,
				fCursorPos.x + tw / 2, gr.top - 4);
			if (tr.left < gr.left)
				tr.OffsetBy(gr.left - tr.left, 0);
			if (tr.right > gr.right)
				tr.OffsetBy(gr.right - tr.right, 0);

			rgb_color bg = tint_color(
				ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT);
			SetHighColor(bg);
			FillRoundRect(tr, 3, 3);

			rgb_color tipBorder = tint_color(bg, B_DARKEN_1_TINT);
			SetHighColor(tipBorder);
			StrokeRoundRect(tr, 3, 3);

			SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
			DrawString(tooltip, BPoint(tr.left + 5, tr.bottom - 5));
		}
	}

	SensorInfo*	fSensor;
	bigtime_t	fTimeRange;
	BPoint		fCursorPos;
	bool		fShowCursor;
	BRect		fGraphRect;
};


// ============================================================================
// TelemetryWindow
// ============================================================================

TelemetryWindow::TelemetryWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 699, 499), "Sensor Telemetry", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fParent(parent),
	fRefreshRunner(NULL),
	fActiveTimeRange(0),
	fSelectedSensor(NULL),
	fCurrentTimeRange(60 * 1000000LL),
	fLastDataTime(0),
	fSensors(20),
	fNeedsRebuild(false)
{
	_BuildLayout();

	if (fParent != NULL)
		CenterIn(fParent->Frame());
	else
		CenterOnScreen();

	BMessage timerMsg(MSG_TELEMETRY_TIMER);
	fRefreshRunner = new BMessageRunner(BMessenger(this), &timerMsg,
		1000000, -1);
}


TelemetryWindow::~TelemetryWindow()
{
	delete fRefreshRunner;
}


void
TelemetryWindow::_BuildLayout()
{
	// Toolbar row: time range buttons + action buttons
	BView* toolbar = new BView("toolbar", 0);
	toolbar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	for (int i = 0; i < 7; i++) {
		BMessage* msg = new BMessage(MSG_TELEMETRY_TIME_RANGE);
		msg->AddInt32("index", i);
		fTimeRangeButtons[i] = new BButton(kTimeRangeLabels[i],
			kTimeRangeLabels[i], msg);
	}

	fRequestAllButton = new BButton("reqall", "Request All",
		new BMessage(MSG_REQUEST_ALL_TELEMETRY));
	fLoadHistoryButton = new BButton("load", "Load",
		new BMessage('tldb'));
	fExportButton = new BButton("export", "Export",
		new BMessage(MSG_TELEMETRY_EXPORT));
	fClearButton = new BButton("clear", "Clear",
		new BMessage(MSG_TELEMETRY_CLEAR_HISTORY));

	BLayoutBuilder::Group<>(toolbar, B_HORIZONTAL, 3)
		.SetInsets(6, 4, 6, 4)
		.Add(fTimeRangeButtons[0])
		.Add(fTimeRangeButtons[1])
		.Add(fTimeRangeButtons[2])
		.Add(fTimeRangeButtons[3])
		.Add(fTimeRangeButtons[4])
		.Add(fTimeRangeButtons[5])
		.Add(fTimeRangeButtons[6])
		.AddGlue()
		.Add(fRequestAllButton)
		.Add(fLoadHistoryButton)
		.Add(fExportButton)
		.Add(fClearButton)
	.End();

	// Highlight active time range button
	fTimeRangeButtons[fActiveTimeRange]->SetEnabled(false);

	// Scrollable content area for sensor cards
	// NOTE: fContentView uses MANUAL positioning (no layout).
	// BScrollView + dynamic layout changes don't reliably update
	// scroll ranges on Haiku R1 beta5. Manual sizing works correctly.
	fContentView = new BView("content", B_WILL_DRAW);
	fContentView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fContentScroll = new BScrollView("content_scroll", fContentView,
		0, false, true, B_NO_BORDER);

	// Detail chart (hidden by default)
	fChartView = new TelemetryChartView();

	// Footer
	fSummaryLabel = new BStringView("summary", "No sensors");
	fFooterLabel = new BStringView("footer", "");

	BFont footerFont(be_plain_font);
	footerFont.SetSize(10);
	fSummaryLabel->SetFont(&footerFont);
	fFooterLabel->SetFont(&footerFont);

	BView* footer = new BView("footer_bar", 0);
	footer->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(footer, B_HORIZONTAL, 0)
		.SetInsets(8, 2, 8, 2)
		.Add(fSummaryLabel)
		.AddGlue()
		.Add(fFooterLabel)
	.End();

	// Separator
	BSeparatorView* sep = new BSeparatorView(B_HORIZONTAL);

	// Main layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(toolbar, 0)
		.Add(new BSeparatorView(B_HORIZONTAL), 0)
		.Add(fContentScroll, 1.0)
		.Add(sep, 0)
		.Add(fChartView, 0)
		.Add(footer, 0)
	.End();
}


void
TelemetryWindow::_RebuildContent()
{
	// Clear card back-pointers BEFORE deleting views to prevent
	// dangling pointer access from AddTelemetryData (other thread)
	for (int32 i = 0; i < fSensors.CountItems(); i++)
		fSensors.ItemAt(i)->cardView = NULL;

	// Remove all children from content view
	while (fContentView->CountChildren() > 0) {
		BView* child = fContentView->ChildAt(0);
		fContentView->RemoveChild(child);
		delete child;
	}

	// Collect unique nodeIds in order
	uint32 nodeIds[128];
	int32 nodeCount = 0;

	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* s = fSensors.ItemAt(i);
		bool found = false;
		for (int32 j = 0; j < nodeCount; j++) {
			if (nodeIds[j] == s->nodeId) {
				found = true;
				break;
			}
		}
		if (!found && nodeCount < 128)
			nodeIds[nodeCount++] = s->nodeId;
	}

	// Manual positioning constants
	static const float kInset = 10;
	static const float kVSpacing = 8;
	static const float kHeaderH = 28;
	static const float kCardSpacing = 8;

	float contentWidth = fContentScroll->Bounds().Width() - 14;
	if (contentWidth < 200)
		contentWidth = 680;

	float y = kInset;
	int32 totalSensors = 0;

	for (int32 n = 0; n < nodeCount; n++) {
		uint32 nodeId = nodeIds[n];

		// Collect sensors for this node
		BString nodeName;
		int32 sensorCount = 0;
		for (int32 i = 0; i < fSensors.CountItems(); i++) {
			SensorInfo* s = fSensors.ItemAt(i);
			if (s->nodeId == nodeId) {
				sensorCount++;
				if (nodeName.Length() == 0 && s->displayName.Length() > 0)
					nodeName = s->displayName;
			}
		}
		if (nodeName.Length() == 0) {
			char hex[16];
			snprintf(hex, sizeof(hex), "%08X", nodeId);
			nodeName = hex;
		}

		totalSensors += sensorCount;

		// Section header — manually positioned
		TelemetrySectionHeader* header = new TelemetrySectionHeader(
			nodeName.String(), sensorCount);
		fContentView->AddChild(header);
		header->MoveTo(kInset, y);
		header->ResizeTo(contentWidth - 2 * kInset, kHeaderH);
		y += kHeaderH + kVSpacing;

		// Cards — flow layout with automatic wrapping
		float cardX = kInset;
		float availW = contentWidth - 2 * kInset;

		for (int32 i = 0; i < fSensors.CountItems(); i++) {
			SensorInfo* s = fSensors.ItemAt(i);
			if (s->nodeId != nodeId)
				continue;

			// Wrap to next row if card doesn't fit
			if (cardX + kCardWidth > kInset + availW && cardX > kInset) {
				cardX = kInset;
				y += kCardHeight + kCardSpacing;
			}

			SensorCardView* card = new SensorCardView(s, this);
			s->cardView = card;
			fContentView->AddChild(card);
			card->MoveTo(cardX, y);
			card->ResizeTo(kCardWidth, kCardHeight);

			if (s == fSelectedSensor)
				card->SetSelected(true);

			cardX += kCardWidth + kCardSpacing;
		}

		y += kCardHeight + kVSpacing;
	}

	y += kInset;

	// Set explicit min size so BScrollView's DoLayout knows the
	// content needs this much height (even if visible area is smaller).
	// This causes BScrollView to enable vertical scrolling properly.
	fContentView->SetExplicitMinSize(BSize(B_SIZE_UNSET, y));

	// Also manually resize — handles case where layout pass is deferred
	float visibleHeight = fContentScroll->Bounds().Height();
	float totalHeight = (y > visibleHeight) ? y : visibleHeight;
	fContentView->ResizeTo(contentWidth, totalHeight);

	// Force scrollbar update as fallback
	BScrollBar* vbar = fContentScroll->ScrollBar(B_VERTICAL);
	if (vbar != NULL) {
		float maxScroll = (y > visibleHeight) ? y - visibleHeight : 0;
		vbar->SetRange(0, maxScroll);
		vbar->SetProportion(
			(y > 0) ? visibleHeight / y : 1.0);
	}

	fContentScroll->InvalidateLayout();

	// Update summary
	char summary[64];
	snprintf(summary, sizeof(summary), "%d sensor%s across %d node%s",
		(int)totalSensors, totalSensors == 1 ? "" : "s",
		(int)nodeCount, nodeCount == 1 ? "" : "s");
	fSummaryLabel->SetText(summary);

	fNeedsRebuild = false;

	fprintf(stderr, "[TelemetryWindow] Rebuild: %d sensors, %d nodes, "
		"%.0f content height\n",
		(int)totalSensors, (int)nodeCount, y);
}


void
TelemetryWindow::_SelectSensor(SensorInfo* sensor)
{
	if (fSelectedSensor == sensor) {
		// Toggle off
		_DeselectSensor();
		return;
	}

	// Deselect previous
	if (fSelectedSensor != NULL && fSelectedSensor->cardView != NULL)
		fSelectedSensor->cardView->SetSelected(false);

	fSelectedSensor = sensor;

	if (sensor != NULL) {
		if (sensor->cardView != NULL)
			sensor->cardView->SetSelected(true);

		fChartView->SetSensor(sensor);
		fChartView->SetTimeRange(fCurrentTimeRange);
		fChartView->SetVisible(true);
	}
}


void
TelemetryWindow::_DeselectSensor()
{
	if (fSelectedSensor != NULL && fSelectedSensor->cardView != NULL)
		fSelectedSensor->cardView->SetSelected(false);

	fSelectedSensor = NULL;
	fChartView->SetSensor(NULL);
	fChartView->SetVisible(false);
}


void
TelemetryWindow::_UpdateSparkline(SensorInfo* sensor)
{
	int32 count = sensor->history.CountItems();
	if (count < 2) {
		sensor->sparklineCount = 0;
		return;
	}

	// Take last N points (up to kSparklineMax)
	int32 start = (count > kSparklineMax) ? count - kSparklineMax : 0;
	int32 n = count - start;

	// Find local min/max for normalization
	float localMin = FLT_MAX, localMax = -FLT_MAX;
	for (int32 i = start; i < count; i++) {
		float v = sensor->history.ItemAt(i)->value;
		if (v < localMin) localMin = v;
		if (v > localMax) localMax = v;
	}

	float range = localMax - localMin;
	if (range < 0.001f)
		range = 1.0f;

	for (int32 i = 0; i < n; i++) {
		float v = sensor->history.ItemAt(start + i)->value;
		sensor->sparkline[i] = (v - localMin) / range;
	}
	sensor->sparklineCount = n;
}


void
TelemetryWindow::_UpdateFooter()
{
	if (fLastDataTime == 0) {
		fFooterLabel->SetText("");
		return;
	}

	bigtime_t elapsed = system_time() - fLastDataTime;
	int32 seconds = (int32)(elapsed / 1000000);

	char buf[64];
	if (seconds < 5)
		snprintf(buf, sizeof(buf), "Last update: just now");
	else if (seconds < 60)
		snprintf(buf, sizeof(buf), "Last update: %ds ago", (int)seconds);
	else
		snprintf(buf, sizeof(buf), "Last update: %dm ago",
			(int)(seconds / 60));

	fFooterLabel->SetText(buf);

	// Stale detection
	rgb_color color;
	if (seconds > 30)
		color = ThemeAccent(220, 60, 60);
	else
		color = tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT);
	fFooterLabel->SetHighColor(color);
}


void
TelemetryWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TELEMETRY_CARD_CLICKED:
		{
			SensorInfo* sensor = NULL;
			if (message->FindPointer("sensor",
				reinterpret_cast<void**>(&sensor)) == B_OK) {
				_SelectSensor(sensor);
			}
			break;
		}

		case MSG_TELEMETRY_TIME_RANGE:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK
				&& index >= 0 && index < 7) {
				// Re-enable previous button
				fTimeRangeButtons[fActiveTimeRange]->SetEnabled(true);
				fActiveTimeRange = index;
				fTimeRangeButtons[index]->SetEnabled(false);

				fCurrentTimeRange = kTimeRanges[index];
				fChartView->SetTimeRange(fCurrentTimeRange);
			}
			break;
		}

		case MSG_TELEMETRY_TIMER:
		{
			if (fNeedsRebuild)
				_RebuildContent();

			// Invalidate all visible cards
			for (int32 i = 0; i < fSensors.CountItems(); i++) {
				SensorInfo* s = fSensors.ItemAt(i);
				if (s->cardView != NULL)
					s->cardView->Invalidate();
			}

			fChartView->Invalidate();
			_UpdateFooter();
			break;
		}

		case MSG_TELEMETRY_EXPORT:
			_ExportData();
			break;

		case MSG_TELEMETRY_CLEAR_HISTORY:
			ClearAllData();
			break;

		case 'tldb':
			LoadHistoryFromDB();
			break;

		case MSG_REQUEST_ALL_TELEMETRY:
			if (fParent != NULL)
				BMessenger(fParent).SendMessage(message);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
TelemetryWindow::QuitRequested()
{
	Hide();
	return false;
}


void
TelemetryWindow::AddTelemetryData(uint32 nodeId, const BString& sensorName,
	SensorType type, float value, const BString& unit,
	const char* contactName)
{
	// Save to database first (thread-safe, no looper lock needed)
	DatabaseManager* db = DatabaseManager::Instance();
	if (db->IsOpen()) {
		db->InsertTelemetry(nodeId, sensorName.String(),
			(uint8)type, value, unit.String());
	}

	// Lock the window looper — this method is called from MainWindow's
	// thread, so we must synchronize with the TelemetryWindow thread
	// which runs Draw/_RebuildContent/timer handling.
	if (!LockLooper()) {
		fprintf(stderr, "[TelemetryWindow] AddTelemetryData: LockLooper FAILED"
			" for %s\n", sensorName.String());
		return;
	}

	bool isNew = false;

	// Check if sensor already exists
	SensorInfo* sensor = NULL;
	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* s = fSensors.ItemAt(i);
		if (s->nodeId == nodeId && s->name == sensorName) {
			sensor = s;
			break;
		}
	}

	if (sensor == NULL) {
		sensor = new SensorInfo();
		sensor->name = sensorName;
		sensor->unit = unit;
		sensor->type = type;
		sensor->nodeId = nodeId;
		sensor->color = _ColorForSensorType(type);
		fSensors.AddItem(sensor);
		isNew = true;
	}

	if (contactName != NULL && contactName[0] != '\0')
		sensor->displayName = contactName;

	TelemetryDataPoint* point = new TelemetryDataPoint();
	point->timestamp = system_time();
	point->value = value;
	sensor->history.AddItem(point);

	sensor->currentValue = value;

	if (value < sensor->minValue)
		sensor->minValue = value;
	if (value > sensor->maxValue)
		sensor->maxValue = value;

	// Calculate average
	float sum = 0;
	for (int32 i = 0; i < sensor->history.CountItems(); i++)
		sum += sensor->history.ItemAt(i)->value;
	sensor->avgValue = sum / sensor->history.CountItems();

	// Limit history
	while (sensor->history.CountItems() > 1000)
		sensor->history.RemoveItemAt(0);

	// Update sparkline
	_UpdateSparkline(sensor);

	fLastDataTime = system_time();

	if (isNew) {
		// Need full rebuild to add new card
		fNeedsRebuild = true;
		fprintf(stderr, "[TelemetryWindow] New sensor: %s (node %08X), "
			"total=%d, rebuild pending\n",
			sensorName.String(), nodeId,
			(int)fSensors.CountItems());
	} else if (sensor->cardView != NULL) {
		// O(1) update — just invalidate the card
		sensor->cardView->UpdateFromSensor();
	}

	// Update chart if this sensor is selected
	if (fSelectedSensor == sensor)
		fChartView->Invalidate();

	UnlockLooper();
}


void
TelemetryWindow::ClearAllData()
{
	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* sensor = fSensors.ItemAt(i);
		sensor->history.MakeEmpty();
		sensor->currentValue = 0;
		sensor->minValue = FLT_MAX;
		sensor->maxValue = -FLT_MAX;
		sensor->avgValue = 0;
		sensor->sparklineCount = 0;
	}

	_DeselectSensor();
	fLastDataTime = 0;

	// Invalidate all cards
	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* s = fSensors.ItemAt(i);
		if (s->cardView != NULL)
			s->cardView->Invalidate();
	}
	_UpdateFooter();
}


void
TelemetryWindow::_ExportData()
{
	BPath path;
	if (find_directory(B_DESKTOP_DIRECTORY, &path) != B_OK)
		return;

	BString filename("telemetry_");

	if (fSelectedSensor != NULL) {
		// Export single sensor
		filename << fSelectedSensor->name << "_";
	} else {
		filename << "all_";
	}
	filename << system_time() / 1000000 << ".csv";
	filename.ReplaceAll(" ", "_");
	path.Append(filename.String());

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	BString header("timestamp,node_id,sensor,value,unit\n");
	file.Write(header.String(), header.Length());

	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* sensor = fSensors.ItemAt(i);

		// If a sensor is selected, export only that one
		if (fSelectedSensor != NULL && sensor != fSelectedSensor)
			continue;

		for (int32 j = 0; j < sensor->history.CountItems(); j++) {
			TelemetryDataPoint* pt = sensor->history.ItemAt(j);
			BString line;
			line << pt->timestamp << "," << sensor->nodeId << ","
				 << sensor->name << "," << pt->value << ","
				 << sensor->unit << "\n";
			file.Write(line.String(), line.Length());
		}
	}

	BString msg;
	msg << "Telemetry data exported to:\n" << path.Path();
	BAlert* alert = new BAlert("Export", msg.String(), "OK");
	alert->Go();
}


void
TelemetryWindow::LoadHistoryFromDB()
{
	DatabaseManager* db = DatabaseManager::Instance();
	if (!db->IsOpen())
		return;

	OwningObjectList<BString> nodeNames(20);
	int32 count = db->GetTelemetryNodeIds(nodeNames);

	if (count == 0) {
		(new BAlert("No Data",
			"No historical telemetry data found in database.",
			"OK"))->Go();
		return;
	}

	uint32 since = (uint32)time(NULL) - (7 * 86400);
	int32 totalLoaded = 0;
	bool anyNew = false;

	for (int32 i = 0; i < nodeNames.CountItems(); i++) {
		BString* entry = nodeNames.ItemAt(i);
		if (entry == NULL)
			continue;

		int32 colonPos = entry->FindFirst(':');
		if (colonPos < 0)
			continue;

		BString nodeIdStr;
		entry->CopyInto(nodeIdStr, 0, colonPos);
		uint32 nodeId = (uint32)atoi(nodeIdStr.String());

		BString sensorName;
		entry->CopyInto(sensorName, colonPos + 1,
			entry->Length() - colonPos - 1);

		OwningObjectList<TelemetryRecord> records(100);
		int32 loaded = db->LoadTelemetryHistory(nodeId,
			sensorName.String(), since, records);

		if (loaded == 0)
			continue;

		SensorType type = SENSOR_CUSTOM;
		BString unit;
		if (records.CountItems() > 0) {
			type = (SensorType)records.ItemAt(0)->sensorType;
			unit = records.ItemAt(0)->unit;
		}

		// Check if this is a new sensor
		bool existed = false;
		for (int32 k = 0; k < fSensors.CountItems(); k++) {
			SensorInfo* s = fSensors.ItemAt(k);
			if (s->nodeId == nodeId && s->name == sensorName) {
				existed = true;
				break;
			}
		}

		SensorInfo* sensor = _FindOrCreateSensor(nodeId, sensorName,
			type, unit);
		if (!existed)
			anyNew = true;

		bigtime_t existingLatest = 0;
		if (sensor->history.CountItems() > 0)
			existingLatest = sensor->history.ItemAt(
				sensor->history.CountItems() - 1)->timestamp;

		for (int32 j = 0; j < records.CountItems(); j++) {
			TelemetryRecord* rec = records.ItemAt(j);
			bigtime_t recTime = (bigtime_t)rec->timestamp * 1000000LL;

			if (recTime <= existingLatest)
				continue;

			TelemetryDataPoint* point = new TelemetryDataPoint();
			point->timestamp = recTime;
			point->value = rec->value;
			sensor->history.AddItem(point);
			totalLoaded++;

			if (rec->value < sensor->minValue)
				sensor->minValue = rec->value;
			if (rec->value > sensor->maxValue)
				sensor->maxValue = rec->value;
		}

		if (sensor->history.CountItems() > 0) {
			float sum = 0;
			for (int32 k = 0; k < sensor->history.CountItems(); k++)
				sum += sensor->history.ItemAt(k)->value;
			sensor->avgValue = sum / sensor->history.CountItems();
			sensor->currentValue = sensor->history.ItemAt(
				sensor->history.CountItems() - 1)->value;
		}

		_UpdateSparkline(sensor);
	}

	if (anyNew)
		fNeedsRebuild = true;

	if (fNeedsRebuild)
		_RebuildContent();

	// Invalidate all cards
	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* s = fSensors.ItemAt(i);
		if (s->cardView != NULL)
			s->cardView->Invalidate();
	}

	if (fSelectedSensor != NULL)
		fChartView->Invalidate();

	fLastDataTime = system_time();

	fprintf(stderr, "[TelemetryWindow] Loaded %d historical data points\n",
		(int)totalLoaded);
}


SensorInfo*
TelemetryWindow::_FindOrCreateSensor(uint32 nodeId, const BString& name,
	SensorType type, const BString& unit)
{
	for (int32 i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* sensor = fSensors.ItemAt(i);
		if (sensor->nodeId == nodeId && sensor->name == name)
			return sensor;
	}

	SensorInfo* sensor = new SensorInfo();
	sensor->name = name;
	sensor->unit = unit;
	sensor->type = type;
	sensor->nodeId = nodeId;
	sensor->color = _ColorForSensorType(type);

	fSensors.AddItem(sensor);
	fNeedsRebuild = true;

	return sensor;
}


rgb_color
TelemetryWindow::_ColorForSensorType(SensorType type)
{
	switch (type) {
		case SENSOR_TEMPERATURE:
			return ThemeAccent(255, 100, 100);
		case SENSOR_HUMIDITY:
			return ThemeAccent(100, 180, 255);
		case SENSOR_PRESSURE:
			return ThemeAccent(180, 100, 255);
		case SENSOR_BATTERY:
			return ThemeAccent(100, 255, 100);
		case SENSOR_ALTITUDE:
			return ThemeAccent(255, 200, 100);
		case SENSOR_LIGHT:
			return ThemeAccent(255, 255, 100);
		case SENSOR_CO2:
			return ThemeAccent(100, 255, 200);
		default:
			return ThemeAccent(150, 150, 200);
	}
}
