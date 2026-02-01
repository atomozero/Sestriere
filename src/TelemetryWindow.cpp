/*
 * Sestriere - MeshCore Client for Haiku OS
 * TelemetryWindow.cpp - Sensor telemetry dashboard implementation
 */

#include "TelemetryWindow.h"
#include "Constants.h"

#include <Application.h>
#include <Box.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <SeparatorView.h>

#include <stdio.h>
#include <math.h>
#include <float.h>


// ============================================================================
// TelemetryGraphView Implementation
// ============================================================================

TelemetryGraphView::TelemetryGraphView(BRect frame)
	: BView(frame, "telemetry_graph", B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW | B_FRAME_EVENTS),
	  fSensor(NULL),
	  fTimeRange(60 * 1000000LL),	// 1 minute default
	  fShowCursor(false),
	  fMarginLeft(60),
	  fMarginRight(20),
	  fMarginTop(20),
	  fMarginBottom(40)
{
	SetViewColor(30, 30, 35);
}


TelemetryGraphView::~TelemetryGraphView()
{
}


void
TelemetryGraphView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Calculate graph area
	fGraphRect.left = bounds.left + fMarginLeft;
	fGraphRect.top = bounds.top + fMarginTop;
	fGraphRect.right = bounds.right - fMarginRight;
	fGraphRect.bottom = bounds.bottom - fMarginBottom;

	// Draw background
	SetHighColor(30, 30, 35);
	FillRect(bounds);

	// Draw graph area background
	SetHighColor(25, 25, 30);
	FillRect(fGraphRect);

	// Draw grid
	_DrawGrid();

	// Draw graph line
	if (fSensor != NULL)
		_DrawGraph();

	// Draw cursor
	if (fShowCursor)
		_DrawCursor();

	// Draw legend
	_DrawLegend();

	// Draw border
	SetHighColor(60, 60, 70);
	StrokeRect(fGraphRect);
}


void
TelemetryGraphView::MouseDown(BPoint where)
{
	if (fGraphRect.Contains(where)) {
		fCursorPos = where;
		fShowCursor = true;
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		Invalidate();
	}
}


void
TelemetryGraphView::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
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


void
TelemetryGraphView::SetSensor(SensorInfo* sensor)
{
	fSensor = sensor;
	fShowCursor = false;
	Invalidate();
}


void
TelemetryGraphView::SetTimeRange(bigtime_t range)
{
	fTimeRange = range;
	Invalidate();
}


void
TelemetryGraphView::_DrawGrid()
{
	SetHighColor(45, 45, 55);
	SetPenSize(1.0);

	// Vertical grid lines (time)
	int numVLines = 6;
	float vStep = fGraphRect.Width() / numVLines;
	for (int i = 0; i <= numVLines; i++) {
		float x = fGraphRect.left + i * vStep;
		StrokeLine(BPoint(x, fGraphRect.top), BPoint(x, fGraphRect.bottom));
	}

	// Horizontal grid lines (values)
	int numHLines = 5;
	float hStep = fGraphRect.Height() / numHLines;
	for (int i = 0; i <= numHLines; i++) {
		float y = fGraphRect.top + i * hStep;
		StrokeLine(BPoint(fGraphRect.left, y), BPoint(fGraphRect.right, y));
	}

	// Draw time labels
	SetHighColor(150, 150, 160);
	SetLowColor(30, 30, 35);
	BFont font(be_plain_font);
	font.SetSize(9);
	SetFont(&font);

	bigtime_t now = system_time();
	for (int i = 0; i <= numVLines; i++) {
		float x = fGraphRect.left + i * vStep;
		bigtime_t timeOffset = (bigtime_t)((numVLines - i) * fTimeRange / numVLines);
		int seconds = (int)(timeOffset / 1000000);

		char label[32];
		if (seconds >= 3600)
			snprintf(label, sizeof(label), "-%dh", seconds / 3600);
		else if (seconds >= 60)
			snprintf(label, sizeof(label), "-%dm", seconds / 60);
		else
			snprintf(label, sizeof(label), "-%ds", seconds);

		float labelWidth = StringWidth(label);
		DrawString(label, BPoint(x - labelWidth / 2, fGraphRect.bottom + 15));
	}

	// Draw value labels if sensor is set
	if (fSensor != NULL && fSensor->history.CountItems() > 0) {
		float minVal = fSensor->minValue;
		float maxVal = fSensor->maxValue;

		if (minVal == maxVal) {
			minVal -= 1;
			maxVal += 1;
		}

		// Add 10% padding
		float range = maxVal - minVal;
		minVal -= range * 0.1;
		maxVal += range * 0.1;

		for (int i = 0; i <= numHLines; i++) {
			float y = fGraphRect.top + i * hStep;
			float value = maxVal - (i * (maxVal - minVal) / numHLines);

			char label[32];
			if (fabs(value) >= 1000)
				snprintf(label, sizeof(label), "%.0f", value);
			else if (fabs(value) >= 100)
				snprintf(label, sizeof(label), "%.1f", value);
			else
				snprintf(label, sizeof(label), "%.2f", value);

			float labelWidth = StringWidth(label);
			DrawString(label, BPoint(fGraphRect.left - labelWidth - 5, y + 4));
		}
	}
}


void
TelemetryGraphView::_DrawGraph()
{
	if (fSensor == NULL || fSensor->history.CountItems() < 2)
		return;

	float minVal = fSensor->minValue;
	float maxVal = fSensor->maxValue;

	if (minVal == maxVal) {
		minVal -= 1;
		maxVal += 1;
	}

	// Add 10% padding
	float range = maxVal - minVal;
	minVal -= range * 0.1;
	maxVal += range * 0.1;

	bigtime_t now = system_time();
	bigtime_t startTime = now - fTimeRange;

	// Draw the line
	SetHighColor(fSensor->color);
	SetPenSize(2.0);

	BPoint prevPoint;
	bool havePrev = false;

	for (int i = 0; i < fSensor->history.CountItems(); i++) {
		TelemetryDataPoint* point = fSensor->history.ItemAt(i);

		if (point->timestamp < startTime)
			continue;

		float x = fGraphRect.left +
			(float)(point->timestamp - startTime) / fTimeRange * fGraphRect.Width();
		float y = fGraphRect.bottom -
			(point->value - minVal) / (maxVal - minVal) * fGraphRect.Height();

		// Clamp to graph area
		if (y < fGraphRect.top) y = fGraphRect.top;
		if (y > fGraphRect.bottom) y = fGraphRect.bottom;

		BPoint currentPoint(x, y);

		if (havePrev) {
			StrokeLine(prevPoint, currentPoint);
		}

		prevPoint = currentPoint;
		havePrev = true;
	}

	// Draw data points
	SetPenSize(1.0);
	for (int i = 0; i < fSensor->history.CountItems(); i++) {
		TelemetryDataPoint* point = fSensor->history.ItemAt(i);

		if (point->timestamp < startTime)
			continue;

		float x = fGraphRect.left +
			(float)(point->timestamp - startTime) / fTimeRange * fGraphRect.Width();
		float y = fGraphRect.bottom -
			(point->value - minVal) / (maxVal - minVal) * fGraphRect.Height();

		if (y < fGraphRect.top) y = fGraphRect.top;
		if (y > fGraphRect.bottom) y = fGraphRect.bottom;

		SetHighColor(255, 255, 255);
		FillEllipse(BPoint(x, y), 3, 3);
		SetHighColor(fSensor->color);
		StrokeEllipse(BPoint(x, y), 3, 3);
	}
}


void
TelemetryGraphView::_DrawCursor()
{
	// Draw vertical cursor line
	SetHighColor(255, 255, 0, 180);
	SetPenSize(1.0);
	StrokeLine(BPoint(fCursorPos.x, fGraphRect.top),
		BPoint(fCursorPos.x, fGraphRect.bottom));

	// Find value at cursor position
	if (fSensor == NULL || fSensor->history.CountItems() == 0)
		return;

	bigtime_t now = system_time();
	bigtime_t startTime = now - fTimeRange;
	bigtime_t cursorTime = startTime +
		(bigtime_t)((fCursorPos.x - fGraphRect.left) / fGraphRect.Width() * fTimeRange);

	// Find closest data point
	TelemetryDataPoint* closest = NULL;
	bigtime_t closestDiff = LLONG_MAX;

	for (int i = 0; i < fSensor->history.CountItems(); i++) {
		TelemetryDataPoint* point = fSensor->history.ItemAt(i);
		bigtime_t diff = abs(point->timestamp - cursorTime);
		if (diff < closestDiff) {
			closestDiff = diff;
			closest = point;
		}
	}

	if (closest != NULL && closestDiff < fTimeRange / 10) {
		// Draw tooltip
		char tooltip[64];
		snprintf(tooltip, sizeof(tooltip), "%.2f %s", closest->value,
			fSensor->unit.String());

		BFont font(be_plain_font);
		font.SetSize(10);
		SetFont(&font);

		float tooltipWidth = StringWidth(tooltip) + 10;
		float tooltipHeight = 18;

		BRect tooltipRect(fCursorPos.x - tooltipWidth / 2,
			fGraphRect.top - tooltipHeight - 5,
			fCursorPos.x + tooltipWidth / 2,
			fGraphRect.top - 5);

		// Adjust if off-screen
		if (tooltipRect.left < fGraphRect.left)
			tooltipRect.OffsetBy(fGraphRect.left - tooltipRect.left, 0);
		if (tooltipRect.right > fGraphRect.right)
			tooltipRect.OffsetBy(fGraphRect.right - tooltipRect.right, 0);

		SetHighColor(60, 60, 70);
		FillRoundRect(tooltipRect, 3, 3);
		SetHighColor(100, 100, 110);
		StrokeRoundRect(tooltipRect, 3, 3);

		SetHighColor(255, 255, 255);
		DrawString(tooltip, BPoint(tooltipRect.left + 5, tooltipRect.bottom - 5));
	}
}


void
TelemetryGraphView::_DrawLegend()
{
	if (fSensor == NULL)
		return;

	BFont font(be_bold_font);
	font.SetSize(11);
	SetFont(&font);

	BString legend;
	legend << fSensor->name << " (" << fSensor->unit << ")";

	SetHighColor(fSensor->color);
	DrawString(legend.String(), BPoint(fGraphRect.left + 10, fGraphRect.top + 15));
}


float
TelemetryGraphView::_ValueToY(float value)
{
	if (fSensor == NULL)
		return fGraphRect.bottom;

	float minVal = fSensor->minValue;
	float maxVal = fSensor->maxValue;

	if (minVal == maxVal) {
		minVal -= 1;
		maxVal += 1;
	}

	float range = maxVal - minVal;
	minVal -= range * 0.1;
	maxVal += range * 0.1;

	return fGraphRect.bottom -
		(value - minVal) / (maxVal - minVal) * fGraphRect.Height();
}


bigtime_t
TelemetryGraphView::_XToTime(float x)
{
	bigtime_t now = system_time();
	bigtime_t startTime = now - fTimeRange;
	return startTime +
		(bigtime_t)((x - fGraphRect.left) / fGraphRect.Width() * fTimeRange);
}


float
TelemetryGraphView::_TimeToX(bigtime_t time)
{
	bigtime_t now = system_time();
	bigtime_t startTime = now - fTimeRange;
	return fGraphRect.left +
		(float)(time - startTime) / fTimeRange * fGraphRect.Width();
}


// ============================================================================
// TelemetrySensorView Implementation
// ============================================================================

TelemetrySensorView::TelemetrySensorView(BRect frame, SensorInfo* sensor)
	: BView(frame, "sensor_view", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
		B_WILL_DRAW | B_FRAME_EVENTS),
	  fSensor(sensor),
	  fSelected(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


TelemetrySensorView::~TelemetrySensorView()
{
}


void
TelemetrySensorView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background
	if (fSelected) {
		SetHighColor(60, 100, 140);
	} else {
		SetHighColor(45, 45, 50);
	}
	FillRect(bounds);

	// Border
	SetHighColor(60, 60, 70);
	StrokeLine(BPoint(bounds.left, bounds.bottom),
		BPoint(bounds.right, bounds.bottom));

	// Sensor color indicator
	SetHighColor(fSensor->color);
	FillRect(BRect(bounds.left + 5, bounds.top + 8,
		bounds.left + 15, bounds.bottom - 8));

	// Sensor name
	BFont font(be_bold_font);
	font.SetSize(11);
	SetFont(&font);
	SetHighColor(220, 220, 230);
	SetLowColor(ViewColor());
	DrawString(fSensor->name.String(), BPoint(bounds.left + 25, bounds.top + 18));

	// Current value
	font = be_plain_font;
	font.SetSize(10);
	SetFont(&font);

	char valueStr[64];
	snprintf(valueStr, sizeof(valueStr), "%.2f %s",
		fSensor->currentValue, fSensor->unit.String());

	SetHighColor(180, 180, 190);
	DrawString(valueStr, BPoint(bounds.left + 25, bounds.top + 32));

	// Node ID
	char nodeStr[32];
	snprintf(nodeStr, sizeof(nodeStr), "Node: %08X", fSensor->nodeId);
	float nodeWidth = StringWidth(nodeStr);
	SetHighColor(120, 120, 130);
	DrawString(nodeStr, BPoint(bounds.right - nodeWidth - 10, bounds.top + 25));
}


void
TelemetrySensorView::MouseDown(BPoint where)
{
	// Notify parent
	BMessage msg(MSG_TELEMETRY_SELECT_SENSOR);
	msg.AddPointer("sensor", fSensor);
	Window()->PostMessage(&msg);
}


void
TelemetrySensorView::SetSelected(bool selected)
{
	if (fSelected != selected) {
		fSelected = selected;
		Invalidate();
	}
}


void
TelemetrySensorView::UpdateValue()
{
	Invalidate();
}


// ============================================================================
// TelemetryWindow Implementation
// ============================================================================

TelemetryWindow::TelemetryWindow(BRect frame, BMessenger target)
	: BWindow(frame, "Sensor Telemetry", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	  fTarget(target),
	  fRefreshRunner(NULL),
	  fSelectedSensor(-1),
	  fCurrentTimeRange(60 * 1000000LL),	// 1 minute
	  fSensors(20)
{
	_BuildLayout();

	// Start refresh timer (update every second)
	BMessage timerMsg(MSG_TELEMETRY_TIMER);
	fRefreshRunner = new BMessageRunner(BMessenger(this), &timerMsg,
		1000000, -1);	// 1 second interval, infinite count
}


TelemetryWindow::~TelemetryWindow()
{
	delete fRefreshRunner;
}


void
TelemetryWindow::_BuildLayout()
{
	fRootView = new BView("root", B_WILL_DRAW);
	fRootView->SetViewColor(40, 40, 45);

	// Left panel - sensor list
	fSensorListView = new BView("sensor_list", B_WILL_DRAW);
	fSensorListView->SetViewColor(40, 40, 45);
	fSensorListView->SetExplicitMinSize(BSize(220, B_SIZE_UNSET));
	fSensorListView->SetExplicitMaxSize(BSize(220, B_SIZE_UNLIMITED));

	fSensorScrollView = new BScrollView("sensor_scroll", fSensorListView,
		B_FOLLOW_ALL_SIDES, 0, false, true);

	// Right panel - graph and stats
	fGraphView = new TelemetryGraphView(BRect(0, 0, 400, 250));
	fGraphView->SetExplicitMinSize(BSize(400, 250));

	// Stats box
	BBox* statsBox = new BBox("stats_box");
	statsBox->SetLabel("Statistics");

	fCurrentValueView = new BStringView("current", "Current: --");
	fMinValueView = new BStringView("min", "Min: --");
	fMaxValueView = new BStringView("max", "Max: --");
	fAvgValueView = new BStringView("avg", "Average: --");
	fNodeIdView = new BStringView("node", "Node: --");

	BView* statsContent = new BView("stats_content", 0);
	statsContent->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	BLayoutBuilder::Group<>(statsContent, B_HORIZONTAL, 20)
		.SetInsets(10)
		.Add(fCurrentValueView)
		.Add(fMinValueView)
		.Add(fMaxValueView)
		.Add(fAvgValueView)
		.AddGlue()
		.Add(fNodeIdView)
	.End();

	statsBox->AddChild(statsContent);

	// Time range buttons
	BView* timeRangeView = new BView("time_range", 0);
	timeRangeView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	BStringView* timeLabel = new BStringView("time_label", "Time Range:");
	fRange1MinButton = new BButton("1m", "1 min", new BMessage('tr1m'));
	fRange5MinButton = new BButton("5m", "5 min", new BMessage('tr5m'));
	fRange15MinButton = new BButton("15m", "15 min", new BMessage('tr15'));
	fRange1HourButton = new BButton("1h", "1 hour", new BMessage('tr1h'));

	BLayoutBuilder::Group<>(timeRangeView, B_HORIZONTAL, 5)
		.SetInsets(5)
		.Add(timeLabel)
		.Add(fRange1MinButton)
		.Add(fRange5MinButton)
		.Add(fRange15MinButton)
		.Add(fRange1HourButton)
		.AddGlue()
	.End();

	// Control buttons
	fExportButton = new BButton("export", "Export CSV", new BMessage(MSG_TELEMETRY_EXPORT));
	fClearButton = new BButton("clear", "Clear History", new BMessage(MSG_TELEMETRY_CLEAR_HISTORY));

	BView* buttonView = new BView("buttons", 0);
	buttonView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	BLayoutBuilder::Group<>(buttonView, B_HORIZONTAL, 10)
		.SetInsets(5)
		.AddGlue()
		.Add(fExportButton)
		.Add(fClearButton)
	.End();

	// Main layout
	BLayoutBuilder::Group<>(fRootView, B_HORIZONTAL, 0)
		.Add(fSensorScrollView, 0.3)
		.AddGroup(B_VERTICAL, 5, 0.7)
			.SetInsets(10)
			.Add(fGraphView, 1.0)
			.Add(statsBox, 0)
			.Add(timeRangeView, 0)
			.Add(buttonView, 0)
		.End()
	.End();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fRootView)
	.End();
}


void
TelemetryWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TELEMETRY_SELECT_SENSOR:
		{
			SensorInfo* sensor;
			if (message->FindPointer("sensor", (void**)&sensor) == B_OK) {
				for (int i = 0; i < fSensors.CountItems(); i++) {
					if (fSensors.ItemAt(i) == sensor) {
						_SelectSensor(i);
						break;
					}
				}
			}
			break;
		}

		case MSG_TELEMETRY_TIMER:
			// Update all sensor views
			for (int i = 0; i < fSensorListView->CountChildren(); i++) {
				TelemetrySensorView* view =
					dynamic_cast<TelemetrySensorView*>(fSensorListView->ChildAt(i));
				if (view != NULL)
					view->UpdateValue();
			}
			fGraphView->Invalidate();
			break;

		case MSG_TELEMETRY_EXPORT:
			_ExportData();
			break;

		case MSG_TELEMETRY_CLEAR_HISTORY:
			ClearAllData();
			break;

		case 'tr1m':
			fCurrentTimeRange = 60 * 1000000LL;
			fGraphView->SetTimeRange(fCurrentTimeRange);
			break;

		case 'tr5m':
			fCurrentTimeRange = 5 * 60 * 1000000LL;
			fGraphView->SetTimeRange(fCurrentTimeRange);
			break;

		case 'tr15':
			fCurrentTimeRange = 15 * 60 * 1000000LL;
			fGraphView->SetTimeRange(fCurrentTimeRange);
			break;

		case 'tr1h':
			fCurrentTimeRange = 60 * 60 * 1000000LL;
			fGraphView->SetTimeRange(fCurrentTimeRange);
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
	SensorType type, float value, const BString& unit)
{
	SensorInfo* sensor = _FindOrCreateSensor(nodeId, sensorName, type, unit);

	// Add data point
	TelemetryDataPoint* point = new TelemetryDataPoint();
	point->timestamp = system_time();
	point->value = value;
	sensor->history.AddItem(point);

	// Update current value
	sensor->currentValue = value;

	// Update min/max
	if (value < sensor->minValue)
		sensor->minValue = value;
	if (value > sensor->maxValue)
		sensor->maxValue = value;

	// Calculate average
	float sum = 0;
	for (int i = 0; i < sensor->history.CountItems(); i++) {
		sum += sensor->history.ItemAt(i)->value;
	}
	sensor->avgValue = sum / sensor->history.CountItems();

	// Limit history size (keep last 1000 points)
	while (sensor->history.CountItems() > 1000) {
		sensor->history.RemoveItemAt(0);
	}

	// Update UI
	_UpdateSensorList();
	if (fSelectedSensor >= 0 && fSensors.ItemAt(fSelectedSensor) == sensor) {
		_UpdateStats();
		fGraphView->Invalidate();
	}
}


void
TelemetryWindow::ClearAllData()
{
	for (int i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* sensor = fSensors.ItemAt(i);
		sensor->history.MakeEmpty();
		sensor->currentValue = 0;
		sensor->minValue = FLT_MAX;
		sensor->maxValue = -FLT_MAX;
		sensor->avgValue = 0;
	}

	_UpdateStats();
	fGraphView->Invalidate();
}


SensorInfo*
TelemetryWindow::_FindOrCreateSensor(uint32 nodeId, const BString& name,
	SensorType type, const BString& unit)
{
	// Look for existing sensor
	for (int i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* sensor = fSensors.ItemAt(i);
		if (sensor->nodeId == nodeId && sensor->name == name)
			return sensor;
	}

	// Create new sensor
	SensorInfo* sensor = new SensorInfo();
	sensor->name = name;
	sensor->unit = unit;
	sensor->type = type;
	sensor->nodeId = nodeId;
	sensor->color = _ColorForSensorType(type);

	fSensors.AddItem(sensor);
	_UpdateSensorList();

	// Select first sensor if none selected
	if (fSelectedSensor < 0)
		_SelectSensor(0);

	return sensor;
}


void
TelemetryWindow::_UpdateSensorList()
{
	// Remove old views
	while (fSensorListView->CountChildren() > 0) {
		BView* child = fSensorListView->ChildAt(0);
		fSensorListView->RemoveChild(child);
		delete child;
	}

	// Add sensor views
	float y = 0;
	float viewHeight = 45;
	BRect listBounds = fSensorListView->Bounds();

	for (int i = 0; i < fSensors.CountItems(); i++) {
		SensorInfo* sensor = fSensors.ItemAt(i);
		BRect frame(0, y, listBounds.Width() - B_V_SCROLL_BAR_WIDTH, y + viewHeight);

		TelemetrySensorView* view = new TelemetrySensorView(frame, sensor);
		view->SetSelected(i == fSelectedSensor);
		fSensorListView->AddChild(view);

		y += viewHeight;
	}

	// Update scroll view data rect
	fSensorListView->ResizeTo(listBounds.Width(), y);
}


void
TelemetryWindow::_SelectSensor(int32 index)
{
	if (index < 0 || index >= fSensors.CountItems())
		return;

	// Update selection state
	int32 oldSelection = fSelectedSensor;
	fSelectedSensor = index;

	// Update sensor views
	for (int i = 0; i < fSensorListView->CountChildren(); i++) {
		TelemetrySensorView* view =
			dynamic_cast<TelemetrySensorView*>(fSensorListView->ChildAt(i));
		if (view != NULL)
			view->SetSelected(i == fSelectedSensor);
	}

	// Update graph
	SensorInfo* sensor = fSensors.ItemAt(fSelectedSensor);
	fGraphView->SetSensor(sensor);

	// Update stats
	_UpdateStats();
}


void
TelemetryWindow::_UpdateStats()
{
	if (fSelectedSensor < 0 || fSelectedSensor >= fSensors.CountItems()) {
		fCurrentValueView->SetText("Current: --");
		fMinValueView->SetText("Min: --");
		fMaxValueView->SetText("Max: --");
		fAvgValueView->SetText("Average: --");
		fNodeIdView->SetText("Node: --");
		return;
	}

	SensorInfo* sensor = fSensors.ItemAt(fSelectedSensor);
	char buffer[64];

	snprintf(buffer, sizeof(buffer), "Current: %.2f %s",
		sensor->currentValue, sensor->unit.String());
	fCurrentValueView->SetText(buffer);

	if (sensor->minValue != FLT_MAX) {
		snprintf(buffer, sizeof(buffer), "Min: %.2f %s",
			sensor->minValue, sensor->unit.String());
	} else {
		snprintf(buffer, sizeof(buffer), "Min: --");
	}
	fMinValueView->SetText(buffer);

	if (sensor->maxValue != -FLT_MAX) {
		snprintf(buffer, sizeof(buffer), "Max: %.2f %s",
			sensor->maxValue, sensor->unit.String());
	} else {
		snprintf(buffer, sizeof(buffer), "Max: --");
	}
	fMaxValueView->SetText(buffer);

	if (sensor->history.CountItems() > 0) {
		snprintf(buffer, sizeof(buffer), "Average: %.2f %s",
			sensor->avgValue, sensor->unit.String());
	} else {
		snprintf(buffer, sizeof(buffer), "Average: --");
	}
	fAvgValueView->SetText(buffer);

	snprintf(buffer, sizeof(buffer), "Node: %08X", sensor->nodeId);
	fNodeIdView->SetText(buffer);
}


void
TelemetryWindow::_ExportData()
{
	if (fSelectedSensor < 0 || fSelectedSensor >= fSensors.CountItems())
		return;

	SensorInfo* sensor = fSensors.ItemAt(fSelectedSensor);
	if (sensor->history.CountItems() == 0)
		return;

	// Get desktop path
	BPath path;
	if (find_directory(B_DESKTOP_DIRECTORY, &path) != B_OK)
		return;

	// Create filename
	BString filename;
	filename << "telemetry_" << sensor->name << "_";
	filename << system_time() / 1000000 << ".csv";
	filename.ReplaceAll(" ", "_");

	path.Append(filename.String());

	// Write CSV
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	// Header
	BString header;
	header << "timestamp,value,unit\n";
	file.Write(header.String(), header.Length());

	// Data
	for (int i = 0; i < sensor->history.CountItems(); i++) {
		TelemetryDataPoint* point = sensor->history.ItemAt(i);

		BString line;
		line << point->timestamp << "," << point->value << ","
			 << sensor->unit << "\n";
		file.Write(line.String(), line.Length());
	}

	// Show notification
	BString msg;
	msg << "Telemetry data exported to:\n" << path.Path();
	BAlert* alert = new BAlert("Export", msg.String(), "OK");
	alert->Go();
}


rgb_color
TelemetryWindow::_ColorForSensorType(SensorType type)
{
	switch (type) {
		case SENSOR_TEMPERATURE:
			return (rgb_color){255, 100, 100, 255};	// Red
		case SENSOR_HUMIDITY:
			return (rgb_color){100, 180, 255, 255};	// Light blue
		case SENSOR_PRESSURE:
			return (rgb_color){180, 100, 255, 255};	// Purple
		case SENSOR_BATTERY:
			return (rgb_color){100, 255, 100, 255};	// Green
		case SENSOR_ALTITUDE:
			return (rgb_color){255, 200, 100, 255};	// Orange
		case SENSOR_LIGHT:
			return (rgb_color){255, 255, 100, 255};	// Yellow
		case SENSOR_CO2:
			return (rgb_color){100, 255, 200, 255};	// Cyan
		default:
			return (rgb_color){150, 150, 200, 255};	// Gray-blue
	}
}


const char*
TelemetryWindow::_IconForSensorType(SensorType type)
{
	switch (type) {
		case SENSOR_TEMPERATURE:
			return "temp";
		case SENSOR_HUMIDITY:
			return "humidity";
		case SENSOR_PRESSURE:
			return "pressure";
		case SENSOR_BATTERY:
			return "battery";
		case SENSOR_ALTITUDE:
			return "altitude";
		case SENSOR_LIGHT:
			return "light";
		case SENSOR_CO2:
			return "co2";
		default:
			return "sensor";
	}
}
