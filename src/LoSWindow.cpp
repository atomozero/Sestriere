/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * LoSWindow.cpp — Line-of-Sight terrain profile analysis window
 */

#include "LoSWindow.h"

#include <Font.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Screen.h>
#include <StringView.h>
#include <View.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ElevationService.h"


// Internal messages
static const uint32 kMsgDataReady = 'ldrd';
static const uint32 kMsgFetchFailed = 'ldfl';

// Layout constants
static const float kLeftMargin = 50.0f;
static const float kRightMargin = 16.0f;
static const float kTopMargin = 12.0f;
static const float kBottomMargin = 24.0f;
static const int32 kNumSamples = 64;

// Antenna height above ground (meters)
static const double kAntennaHeight = 2.0;


// =============================================================================
// ProfileView — Custom BView for terrain profile rendering
// =============================================================================

class ProfileView : public BView {
public:
					ProfileView();
	virtual			~ProfileView();

	virtual void	Draw(BRect updateRect);
	virtual BSize	MinSize();
	virtual BSize	MaxSize();

			void	SetData(const TerrainPoint* points, int32 count,
						const LoSResult& result, double freqHz,
						double startElev, double endElev);
			void	ClearData();

private:
			void	_DrawTerrain(BRect chartRect);
			void	_DrawLoSLine(BRect chartRect);
			void	_DrawFresnelZone(BRect chartRect);
			void	_DrawGrid(BRect chartRect);
			void	_DrawAxisLabels(BRect chartRect);
			void	_DrawNoDataMessage(BRect chartRect);
			float	_MapX(BRect chartRect, double distance) const;
			float	_MapY(BRect chartRect, double elevation) const;

			const TerrainPoint*	fPoints;
			int32		fPointCount;
			LoSResult	fResult;
			double		fFreqHz;
			double		fMinElev;
			double		fMaxElev;
			double		fMaxDist;
			double		fTxElev;	// TX antenna absolute elevation
			double		fRxElev;	// RX antenna absolute elevation
};


ProfileView::ProfileView()
	:
	BView("profile_view", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fPoints(NULL),
	fPointCount(0),
	fFreqHz(868e6),
	fMinElev(0),
	fMaxElev(100),
	fMaxDist(1000),
	fTxElev(0),
	fRxElev(0)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


ProfileView::~ProfileView()
{
}


BSize
ProfileView::MinSize()
{
	return BSize(400, 200);
}


BSize
ProfileView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED);
}


void
ProfileView::SetData(const TerrainPoint* points, int32 count,
	const LoSResult& result, double freqHz,
	double startElev, double endElev)
{
	fPoints = points;
	fPointCount = count;
	fResult = result;
	fFreqHz = freqHz;
	fTxElev = startElev;
	fRxElev = endElev;

	if (count < 2) {
		fMinElev = 0;
		fMaxElev = 100;
		fMaxDist = 1000;
		Invalidate();
		return;
	}

	// Find elevation range
	fMinElev = 1e9;
	fMaxElev = -1e9;
	for (int32 i = 0; i < count; i++) {
		if (points[i].elevation < fMinElev)
			fMinElev = points[i].elevation;
		if (points[i].elevation > fMaxElev)
			fMaxElev = points[i].elevation;
	}

	// Include antenna heights in range
	if (fTxElev > fMaxElev) fMaxElev = fTxElev;
	if (fRxElev > fMaxElev) fMaxElev = fRxElev;

	// Add padding (10% top/bottom)
	double elevRange = fMaxElev - fMinElev;
	if (elevRange < 10) elevRange = 10;
	fMinElev -= elevRange * 0.1;
	fMaxElev += elevRange * 0.15;

	fMaxDist = points[count - 1].distance;

	Invalidate();
}


void
ProfileView::ClearData()
{
	fPoints = NULL;
	fPointCount = 0;
	Invalidate();
}


void
ProfileView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background
	SetLowColor(ViewColor());
	FillRect(bounds, B_SOLID_LOW);

	BRect chartRect(
		bounds.left + kLeftMargin,
		bounds.top + kTopMargin,
		bounds.right - kRightMargin,
		bounds.bottom - kBottomMargin);

	// Chart background
	rgb_color chartBg = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_1_TINT);
	SetHighColor(chartBg);
	FillRect(chartRect);

	// Border
	rgb_color gridColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_2_TINT);
	SetHighColor(gridColor);
	StrokeRect(chartRect);

	if (fPointCount < 2 || fPoints == NULL) {
		_DrawNoDataMessage(chartRect);
		return;
	}

	_DrawGrid(chartRect);
	_DrawTerrain(chartRect);
	_DrawFresnelZone(chartRect);
	_DrawLoSLine(chartRect);
	_DrawAxisLabels(chartRect);
}


void
ProfileView::_DrawTerrain(BRect chartRect)
{
	if (fPointCount < 2)
		return;

	// Terrain fill polygon (brown tint from panel bg)
	rgb_color panelBg = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color terrainColor;
	terrainColor.red = (uint8)(panelBg.red * 0.6 + 139 * 0.4);
	terrainColor.green = (uint8)(panelBg.green * 0.4 + 90 * 0.6);
	terrainColor.blue = (uint8)(panelBg.blue * 0.3 + 43 * 0.7);
	terrainColor.alpha = 255;

	// Build polygon: terrain line + bottom edge
	int32 polyCount = fPointCount + 2;
	BPoint* poly = new BPoint[polyCount];

	for (int32 i = 0; i < fPointCount; i++) {
		poly[i] = BPoint(
			_MapX(chartRect, fPoints[i].distance),
			_MapY(chartRect, fPoints[i].elevation));
	}
	// Close at bottom
	poly[fPointCount] = BPoint(
		_MapX(chartRect, fPoints[fPointCount - 1].distance),
		chartRect.bottom);
	poly[fPointCount + 1] = BPoint(
		_MapX(chartRect, fPoints[0].distance),
		chartRect.bottom);

	SetHighColor(terrainColor);
	FillPolygon(poly, polyCount);

	// Terrain outline
	rgb_color outlineColor = tint_color(terrainColor, B_DARKEN_2_TINT);
	SetHighColor(outlineColor);
	SetPenSize(1.5f);
	for (int32 i = 0; i < fPointCount - 1; i++) {
		StrokeLine(poly[i], poly[i + 1]);
	}
	SetPenSize(1.0f);

	delete[] poly;
}


void
ProfileView::_DrawLoSLine(BRect chartRect)
{
	if (fPointCount < 2)
		return;

	// LoS line between antenna positions
	BPoint txPt(_MapX(chartRect, 0), _MapY(chartRect, fTxElev));
	BPoint rxPt(_MapX(chartRect, fMaxDist), _MapY(chartRect, fRxElev));

	// Draw line
	rgb_color losColor;
	if (fResult.hasLineOfSight) {
		losColor = tint_color(ui_color(B_SUCCESS_COLOR), B_NO_TINT);
	} else {
		losColor = tint_color(ui_color(B_FAILURE_COLOR), B_NO_TINT);
	}
	SetHighColor(losColor);
	SetPenSize(2.0f);
	StrokeLine(txPt, rxPt);
	SetPenSize(1.0f);

	// Draw antenna markers
	rgb_color markerColor = ui_color(B_PANEL_TEXT_COLOR);
	SetHighColor(markerColor);

	// TX antenna
	float txGroundY = _MapY(chartRect, fPoints[0].elevation);
	StrokeLine(BPoint(txPt.x, txGroundY), txPt);
	FillEllipse(txPt, 4, 4);

	// RX antenna
	float rxGroundY = _MapY(chartRect,
		fPoints[fPointCount - 1].elevation);
	StrokeLine(BPoint(rxPt.x, rxGroundY), rxPt);
	FillEllipse(rxPt, 4, 4);
}


void
ProfileView::_DrawFresnelZone(BRect chartRect)
{
	if (fPointCount < 3 || fFreqHz <= 0)
		return;

	double totalDist = fMaxDist;
	if (totalDist <= 0)
		return;

	// Draw Fresnel zone as a filled band around the LoS line
	SetDrawingMode(B_OP_ALPHA);

	for (int32 i = 1; i < fPointCount - 1; i++) {
		double d1 = fPoints[i].distance;
		double d2 = totalDist - d1;
		double fraction = d1 / totalDist;

		// LoS elevation at this point
		double losElev = fTxElev + fraction * (fRxElev - fTxElev);

		// Fresnel radius
		double fresnel = FresnelRadius(d1, d2, fFreqHz);

		// Earth curvature
		double curvature = EarthCurvatureBulge(d1, d2);
		double effectiveTerrainElev = fPoints[i].elevation + curvature;

		// Clearance
		double clearance = losElev - effectiveTerrainElev;
		bool obstructed = (clearance < fresnel * 0.6);

		float x = _MapX(chartRect, d1);
		float topY = _MapY(chartRect, losElev + fresnel);
		float botY = _MapY(chartRect, losElev - fresnel);

		// Clamp to chart
		if (topY < chartRect.top) topY = chartRect.top;
		if (botY > chartRect.bottom) botY = chartRect.bottom;

		rgb_color zoneColor;
		if (obstructed) {
			// Red for obstructed
			rgb_color failColor = ui_color(B_FAILURE_COLOR);
			rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
			zoneColor.red = (uint8)(bg.red * 0.7 + failColor.red * 0.3);
			zoneColor.green = (uint8)(bg.green * 0.7 + failColor.green * 0.3);
			zoneColor.blue = (uint8)(bg.blue * 0.7 + failColor.blue * 0.3);
			zoneColor.alpha = 60;
		} else {
			// Green for clear
			rgb_color okColor = ui_color(B_SUCCESS_COLOR);
			rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
			zoneColor.red = (uint8)(bg.red * 0.7 + okColor.red * 0.3);
			zoneColor.green = (uint8)(bg.green * 0.7 + okColor.green * 0.3);
			zoneColor.blue = (uint8)(bg.blue * 0.7 + okColor.blue * 0.3);
			zoneColor.alpha = 40;
		}

		SetHighColor(zoneColor);

		// Draw as thin vertical strip
		float halfWidth = (chartRect.Width() / fPointCount) / 2;
		FillRect(BRect(x - halfWidth, topY, x + halfWidth, botY));
	}

	SetDrawingMode(B_OP_COPY);
}


void
ProfileView::_DrawGrid(BRect chartRect)
{
	rgb_color gridColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_2_TINT);
	SetHighColor(gridColor);

	// Horizontal grid lines (elevation)
	double elevRange = fMaxElev - fMinElev;
	double step = 100;
	if (elevRange < 200) step = 50;
	if (elevRange < 100) step = 20;
	if (elevRange < 50) step = 10;

	double firstLine = ceil(fMinElev / step) * step;
	for (double elev = firstLine; elev <= fMaxElev; elev += step) {
		float y = _MapY(chartRect, elev);
		if (y >= chartRect.top && y <= chartRect.bottom) {
			for (float x = chartRect.left; x < chartRect.right; x += 6)
				StrokeLine(BPoint(x, y),
					BPoint(std::min(x + 3.0f, chartRect.right), y));
		}
	}

	// Vertical grid lines (distance)
	double distStep = 1000;  // 1 km default
	if (fMaxDist > 50000) distStep = 10000;
	else if (fMaxDist > 20000) distStep = 5000;
	else if (fMaxDist > 5000) distStep = 2000;
	else if (fMaxDist < 2000) distStep = 500;

	for (double dist = distStep; dist < fMaxDist; dist += distStep) {
		float x = _MapX(chartRect, dist);
		if (x > chartRect.left && x < chartRect.right) {
			for (float y = chartRect.top; y < chartRect.bottom; y += 6)
				StrokeLine(BPoint(x, y),
					BPoint(x, std::min(y + 3.0f, chartRect.bottom)));
		}
	}
}


void
ProfileView::_DrawAxisLabels(BRect chartRect)
{
	BFont smallFont;
	GetFont(&smallFont);
	smallFont.SetSize(9);
	SetFont(&smallFont);

	font_height fh;
	smallFont.GetHeight(&fh);

	rgb_color labelColor = tint_color(ui_color(B_PANEL_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(labelColor);

	// Y-axis labels (elevation in m)
	double elevRange = fMaxElev - fMinElev;
	double step = 100;
	if (elevRange < 200) step = 50;
	if (elevRange < 100) step = 20;
	if (elevRange < 50) step = 10;

	double firstLine = ceil(fMinElev / step) * step;
	bool firstLabel = true;
	for (double elev = firstLine; elev <= fMaxElev; elev += step) {
		float y = _MapY(chartRect, elev);
		if (y >= chartRect.top + 8 && y <= chartRect.bottom - 4) {
			char label[16];
			if (firstLabel) {
				snprintf(label, sizeof(label), "%.0fm", elev);
				firstLabel = false;
			} else {
				snprintf(label, sizeof(label), "%.0f", elev);
			}
			float tw = StringWidth(label);
			SetHighColor(labelColor);
			DrawString(label,
				BPoint(chartRect.left - tw - 4, y + fh.ascent / 2));
		}
	}

	// X-axis labels (distance in km)
	double distStep = 1000;
	if (fMaxDist > 50000) distStep = 10000;
	else if (fMaxDist > 20000) distStep = 5000;
	else if (fMaxDist > 5000) distStep = 2000;
	else if (fMaxDist < 2000) distStep = 500;

	SetHighColor(labelColor);
	// Start label
	DrawString("0",
		BPoint(chartRect.left, chartRect.bottom + fh.ascent + 3));

	for (double dist = distStep; dist < fMaxDist; dist += distStep) {
		float x = _MapX(chartRect, dist);
		if (x > chartRect.left + 20 && x < chartRect.right - 20) {
			char label[16];
			if (dist >= 1000)
				snprintf(label, sizeof(label), "%.1fkm", dist / 1000.0);
			else
				snprintf(label, sizeof(label), "%.0fm", dist);
			float tw = StringWidth(label);
			SetHighColor(labelColor);
			DrawString(label,
				BPoint(x - tw / 2, chartRect.bottom + fh.ascent + 3));
		}
	}

	// End label
	{
		char label[16];
		snprintf(label, sizeof(label), "%.1fkm", fMaxDist / 1000.0);
		float tw = StringWidth(label);
		SetHighColor(labelColor);
		DrawString(label,
			BPoint(chartRect.right - tw, chartRect.bottom + fh.ascent + 3));
	}
}


void
ProfileView::_DrawNoDataMessage(BRect chartRect)
{
	BFont font;
	GetFont(&font);
	font.SetSize(11);
	SetFont(&font);

	font_height fh;
	font.GetHeight(&fh);

	rgb_color labelColor = tint_color(ui_color(B_PANEL_TEXT_COLOR),
		B_LIGHTEN_1_TINT);
	SetHighColor(labelColor);

	const char* msg = "Fetching elevation data...";
	float tw = StringWidth(msg);
	DrawString(msg,
		BPoint(chartRect.left + (chartRect.Width() - tw) / 2,
			chartRect.top + (chartRect.Height() + fh.ascent) / 2));
}


float
ProfileView::_MapX(BRect chartRect, double distance) const
{
	if (fMaxDist <= 0)
		return chartRect.left;
	double ratio = distance / fMaxDist;
	return chartRect.left + (float)(ratio * chartRect.Width());
}


float
ProfileView::_MapY(BRect chartRect, double elevation) const
{
	double range = fMaxElev - fMinElev;
	if (range <= 0)
		return chartRect.top + chartRect.Height() / 2;
	double ratio = (elevation - fMinElev) / range;
	return chartRect.bottom - (float)(ratio * chartRect.Height());
}


// =============================================================================
// LoSWindow
// =============================================================================

LoSWindow::LoSWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 600, 350), "Line of Sight",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fParent(parent),
	fProfileView(NULL),
	fTitleLabel(NULL),
	fStatusLabel(NULL),
	fStartLat(0),
	fStartLon(0),
	fEndLat(0),
	fEndLon(0),
	fFreqHz(868e6),
	fPoints(NULL),
	fPointCount(0),
	fHasData(false),
	fFetching(false),
	fFetchThreadId(-1)
{
	memset(fStartName, 0, sizeof(fStartName));
	memset(fEndName, 0, sizeof(fEndName));

	fTitleLabel = new BStringView("title_label", "Line of Sight Analysis");
	BFont titleFont;
	fTitleLabel->GetFont(&titleFont);
	titleFont.SetSize(titleFont.Size() + 1);
	titleFont.SetFace(B_BOLD_FACE);
	fTitleLabel->SetFont(&titleFont);

	fProfileView = new ProfileView();

	fStatusLabel = new BStringView("status_label", "Select a contact to analyze");

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_SMALL_INSETS)
		.Add(fTitleLabel)
		.Add(fProfileView, 1.0f)
		.Add(fStatusLabel)
		.End();

	// Center in parent
	if (parent != NULL) {
		BRect parentFrame = parent->Frame();
		BRect frame = Frame();
		MoveTo(
			parentFrame.left + (parentFrame.Width() - frame.Width()) / 2,
			parentFrame.top + (parentFrame.Height() - frame.Height()) / 2);
	} else {
		CenterOnScreen();
	}
}


LoSWindow::~LoSWindow()
{
	delete[] fPoints;
}


bool
LoSWindow::QuitRequested()
{
	Hide();
	return false;
}


void
LoSWindow::SetEndpoints(double startLat, double startLon,
	const char* startName, double endLat, double endLon,
	const char* endName)
{
	fStartLat = startLat;
	fStartLon = startLon;
	fEndLat = endLat;
	fEndLon = endLon;
	strlcpy(fStartName, startName, sizeof(fStartName));
	strlcpy(fEndName, endName, sizeof(fEndName));

	// Update title
	double distKm = HaversineDistance(startLat, startLon, endLat, endLon)
		/ 1000.0;
	char title[256];
	snprintf(title, sizeof(title), "LoS: %s \xe2\x86\x92 %s  |  %.1f km",
		fStartName, fEndName, distKm);
	fTitleLabel->SetText(title);
}


void
LoSWindow::SetFrequency(double freqHz)
{
	fFreqHz = freqHz;
}


void
LoSWindow::StartAnalysis()
{
	if (fFetching)
		return;

	fFetching = true;
	fHasData = false;

	fStatusLabel->SetText("Fetching elevation data...");
	((ProfileView*)fProfileView)->ClearData();

	fFetchThreadId = spawn_thread(_FetchThread, "los_fetch",
		B_NORMAL_PRIORITY, this);
	if (fFetchThreadId >= 0)
		resume_thread(fFetchThreadId);
	else {
		fFetching = false;
		fStatusLabel->SetText("Failed to start fetch thread");
	}
}


int32
LoSWindow::_FetchThread(void* data)
{
	LoSWindow* self = (LoSWindow*)data;

	// Allocate terrain points
	TerrainPoint* points = new(std::nothrow) TerrainPoint[kNumSamples];
	if (points == NULL) {
		BMessage msg(kMsgFetchFailed);
		self->PostMessage(&msg);
		return -1;
	}

	int32 count = 0;
	status_t status = ElevationService::BuildTerrainProfile(
		self->fStartLat, self->fStartLon,
		self->fEndLat, self->fEndLon,
		kNumSamples, points, &count);

	if (status != B_OK || count < 2) {
		delete[] points;
		BMessage msg(kMsgFetchFailed);
		self->PostMessage(&msg);
		return -1;
	}

	// Run LoS analysis
	LoSResult result = AnalyzeLineOfSight(points, count,
		kAntennaHeight, kAntennaHeight, self->fFreqHz);

	// Send results via BMessage (pass pointer via AddPointer)
	BMessage msg(kMsgDataReady);
	msg.AddPointer("points", points);
	msg.AddInt32("count", count);
	msg.AddBool("has_los", result.hasLineOfSight);
	msg.AddDouble("total_distance", result.totalDistance);
	msg.AddDouble("max_obstruction", result.maxObstruction);
	msg.AddDouble("worst_fresnel", result.worstFresnelRatio);
	msg.AddInt32("worst_index", result.worstPointIndex);
	self->PostMessage(&msg);

	return 0;
}


void
LoSWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgDataReady:
		{
			fFetching = false;

			// Take ownership of points from fetch thread
			void* ptr;
			if (message->FindPointer("points", &ptr) != B_OK)
				break;

			delete[] fPoints;
			fPoints = (TerrainPoint*)ptr;
			message->FindInt32("count", &fPointCount);

			// Rebuild LoSResult
			fResult.hasLineOfSight =
				message->GetBool("has_los", true);
			message->FindDouble("total_distance",
				&fResult.totalDistance);
			message->FindDouble("max_obstruction",
				&fResult.maxObstruction);
			message->FindDouble("worst_fresnel",
				&fResult.worstFresnelRatio);
			message->FindInt32("worst_index",
				&fResult.worstPointIndex);

			fHasData = true;

			// Update ProfileView
			double txElev = fPoints[0].elevation + kAntennaHeight;
			double rxElev = fPoints[fPointCount - 1].elevation
				+ kAntennaHeight;
			((ProfileView*)fProfileView)->SetData(
				fPoints, fPointCount, fResult, fFreqHz,
				txElev, rxElev);

			// Update status
			char status[256];
			if (fResult.hasLineOfSight) {
				snprintf(status, sizeof(status),
					"\xe2\x9c\x93 Clear line of sight  |  "
					"Fresnel clearance: %.0f%%  |  "
					"Distance: %.1f km",
					fResult.worstFresnelRatio * 100.0,
					fResult.totalDistance / 1000.0);
			} else {
				double obstructDist = 0;
				if (fResult.worstPointIndex >= 0
					&& fResult.worstPointIndex < fPointCount)
					obstructDist =
						fPoints[fResult.worstPointIndex].distance;

				snprintf(status, sizeof(status),
					"\xe2\x9c\x97 Obstructed at %.1f km  |  "
					"Clearance: %.0f%%  |  "
					"Obstruction: %.0f m",
					obstructDist / 1000.0,
					fResult.worstFresnelRatio * 100.0,
					fResult.maxObstruction);
			}
			fStatusLabel->SetText(status);
			break;
		}

		case kMsgFetchFailed:
			fFetching = false;
			fStatusLabel->SetText(
				"Failed to fetch elevation data (check network)");
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
