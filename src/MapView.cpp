/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MapView.cpp — Geographic map visualization implementation
 */

#include "MapView.h"

#include <Bitmap.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <Path.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "CoastlineData.h"
#include "Constants.h"
#include "TileCache.h"


// Zoom levels match Google Maps / OSM standard: fZoom = 256 * 2^z / 360
// Each step doubles/halves the scale (one tile zoom level)
static const int kMinZoomLevel = 2;		// world view
static const int kMaxZoomLevel = 18;	// street level
static const int kDefaultZoomLevel = 13;
static const float kMinFitSpan = 0.01f;	// ~1.1 km minimum visible area


static float
_ZoomForLevel(int level)
{
	// pixels per degree of longitude at this zoom level
	return 256.0f * powf(2.0f, level) / 360.0f;
}


static int
_LevelForZoom(float zoom)
{
	// inverse: z = log2(zoom * 360 / 256)
	int z = (int)roundf(log2f(zoom * 360.0f / 256.0f));
	if (z < kMinZoomLevel) z = kMinZoomLevel;
	if (z > kMaxZoomLevel) z = kMaxZoomLevel;
	return z;
}
static const float kNodeRadius = 8.0f;
static const float kSelfRadius = 10.0f;

static const uint32 kMsgZoomIn		= 'zmin';
static const uint32 kMsgZoomOut		= 'zmot';
static const uint32 kMsgZoomFit		= 'zmft';
static const uint32 kMsgCenterSelf	= 'cnsl';
static const uint32 kMsgToggleTiles	= 'tltg';

// Land fill color for coastlines
static const rgb_color kLandColor = {60, 75, 55, 255};
static const rgb_color kCoastlineStroke = {90, 110, 80, 255};


// ============================================================================
// MapView
// ============================================================================

MapView::MapView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_FRAME_EVENTS),
	fNodes(20),
	fHasSelfPosition(false),
	fCenterLat(45.0f),
	fCenterLon(7.0f),
	fZoom(_ZoomForLevel(kDefaultZoomLevel)),
	fDragging(false),
	fDragStartLat(0),
	fDragStartLon(0),
	fSelectedNode(NULL),
	fHoverNode(NULL),
	fTileCache(NULL),
	fShowTiles(false),
	fShowCoastlines(true),
	fLastTileZ(-1),
	fLastTileMinX(-1),
	fLastTileMinY(-1),
	fLastTileMaxX(-1),
	fLastTileMaxY(-1)
{
	fSelfNode = GeoMapNode();
	fSelfNode.isSelf = true;
	SetViewColor(B_TRANSPARENT_COLOR);

	// Create tile cache in settings directory
	BPath settingsPath;
	find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath);
	settingsPath.Append("Sestriere/tiles");
	fTileCache = new TileCache(settingsPath.Path());
	fTileCache->Run();

	LoadMapState();
}


MapView::~MapView()
{
	if (fTileCache != NULL) {
		fTileCache->Lock();
		fTileCache->Quit();
	}
}


void
MapView::AttachedToWindow()
{
	BView::AttachedToWindow();
	SetEventMask(B_POINTER_EVENTS);
}


void
MapView::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();

	// 1. Background - dark blue for "sea"
	SetHighColor(30, 40, 60);
	FillRect(bounds);

	// 2. OSM tiles (if enabled and loaded)
	if (fShowTiles)
		_DrawTiles();

	// 3. Coastlines (always drawn as offline fallback)
	if (fShowCoastlines)
		_DrawCoastlines();

	// 4. Grid (dimmed if tiles active)
	_DrawGrid();

	// 5. Connections
	_DrawConnections();

	// 6. Nodes
	_DrawNodes();

	// 7. SAR pins (on top of nodes)
	_DrawSarPins();

	// 8. Scale bar + compass + cache info
	_DrawScaleBar();
	_DrawCompass();

	// 9. Tile cache stats + zoom level (bottom-right, small)
	{
		int zLevel = _LevelForZoom(fZoom);
		char info[64];
		if (fShowTiles && fTileCache != NULL) {
			float mb = (float)fTileCache->DiskCacheSize() / (1024 * 1024);
			int32 count = fTileCache->DiskTileCount();
			snprintf(info, sizeof(info), "Z%d  %ld tiles  %.1f/50 MB",
				zLevel, (long)count, mb);
		} else {
			snprintf(info, sizeof(info), "Z%d", zLevel);
		}

		BFont small;
		GetFont(&small);
		small.SetSize(10);
		SetFont(&small);

		float sw = small.StringWidth(info);
		float ix = bounds.Width() - sw - 10;
		float iy = bounds.Height() - 10;

		SetHighColor(0, 0, 0, 140);
		FillRoundRect(BRect(ix - 4, iy - 11, ix + sw + 4, iy + 3), 3, 3);
		SetHighColor(200, 200, 200);
		DrawString(info, BPoint(ix, iy));
	}
}


void
MapView::MouseDown(BPoint where)
{
	int32 buttons;
	if (Window()->CurrentMessage()->FindInt32("buttons", &buttons) != B_OK)
		buttons = B_PRIMARY_MOUSE_BUTTON;

	int32 clicks;
	if (Window()->CurrentMessage()->FindInt32("clicks", &clicks) != B_OK)
		clicks = 1;

	GeoMapNode* node = _FindNodeAt(where);

	if (buttons & B_PRIMARY_MOUSE_BUTTON) {
		if (clicks == 2 && node != NULL) {
			fCenterLat = node->latitude;
			fCenterLon = node->longitude;
			Invalidate();
		} else if (node != NULL) {
			if (fSelectedNode != NULL)
				fSelectedNode->isSelected = false;
			fSelectedNode = node;
			fSelectedNode->isSelected = true;
			Invalidate();
		} else {
			fDragging = true;
			fDragStart = where;
			fDragStartLat = fCenterLat;
			fDragStartLon = fCenterLon;
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		}
	}
}


void
MapView::MouseMoved(BPoint where, uint32 /*transit*/,
	const BMessage* /*dragMessage*/)
{
	if (fDragging) {
		float dx = where.x - fDragStart.x;
		float dy = where.y - fDragStart.y;

		// In Mercator, longitude is linear but latitude is non-linear.
		// For panning, we compute the Mercator Y difference in screen space.
		float dLon = -dx / fZoom;

		// Convert screen dy back to latitude change via Mercator inverse
		float centerMercY = _MercatorY(fDragStartLat);
		float newMercY = centerMercY + dy / fZoom;

		// Inverse Mercator Y to latitude
		float newLat = atan(sinh(newMercY * M_PI / 180.0f)) * 180.0f / M_PI;

		fCenterLat = newLat;
		fCenterLon = fDragStartLon + dLon;

		if (fCenterLat > 85.0f) fCenterLat = 85.0f;
		if (fCenterLat < -85.0f) fCenterLat = -85.0f;

		Invalidate();
	} else {
		GeoMapNode* node = _FindNodeAt(where);
		if (node != fHoverNode) {
			fHoverNode = node;
			Invalidate();
		}
	}
}


void
MapView::MouseUp(BPoint /*where*/)
{
	fDragging = false;
}


void
MapView::FrameResized(float newWidth, float newHeight)
{
	BView::FrameResized(newWidth, newHeight);
	Invalidate();
}


void
MapView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_MOUSE_WHEEL_CHANGED:
		{
			float deltaY;
			if (message->FindFloat("be:wheel_delta_y", &deltaY) == B_OK) {
				if (deltaY < 0)
					ZoomIn();
				else
					ZoomOut();
			}
			break;
		}

		case MSG_TILES_READY:
			Invalidate();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
MapView::SetSelfPosition(float lat, float lon, const char* name)
{
	fSelfNode.latitude = lat;
	fSelfNode.longitude = lon;
	strlcpy(fSelfNode.name, name, sizeof(fSelfNode.name));
	fSelfNode.isSelf = true;
	fHasSelfPosition = true;

	fCenterLat = lat;
	fCenterLon = lon;

	Invalidate();
}


void
MapView::AddOrUpdateNode(const ContactInfo& contact, float lat, float lon)
{
	if (lat == 0.0f && lon == 0.0f)
		return;

	// Check if node already exists
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GeoMapNode* node = fNodes.ItemAt(i);
		if (memcmp(node->publicKey, contact.publicKey, 6) == 0) {
			node->latitude = lat;
			node->longitude = lon;
			strlcpy(node->name, contact.name, sizeof(node->name));
			node->type = contact.type;
			node->pathLen = contact.outPathLen;
			Invalidate();
			return;
		}
	}

	GeoMapNode* node = new GeoMapNode();
	memcpy(node->publicKey, contact.publicKey, 32);
	strlcpy(node->name, contact.name, sizeof(node->name));
	node->latitude = lat;
	node->longitude = lon;
	node->type = contact.type;
	node->pathLen = contact.outPathLen;

	fNodes.AddItem(node);
	Invalidate();
}


void
MapView::ClearNodes()
{
	fNodes.MakeEmpty();
	fSelectedNode = NULL;
	fHoverNode = NULL;
	Invalidate();
}


void
MapView::AddSarPin(float lat, float lon, const char* emoji,
	const char* name, int colorIndex)
{
	SarMapPin* pin = new SarMapPin();
	pin->lat = lat;
	pin->lon = lon;
	strlcpy(pin->emoji, emoji, sizeof(pin->emoji));
	strlcpy(pin->name, name, sizeof(pin->name));
	pin->colorIndex = colorIndex;
	pin->timestamp = (uint32)time(NULL);
	fSarPins.AddItem(pin);
	Invalidate();
}


void
MapView::ClearSarPins()
{
	fSarPins.MakeEmpty();
	Invalidate();
}


void
MapView::_DrawSarPins()
{
	for (int32 i = 0; i < fSarPins.CountItems(); i++) {
		SarMapPin* pin = fSarPins.ItemAt(i);
		BPoint pos = _LatLonToScreen((float)pin->lat, (float)pin->lon);

		// Skip if off-screen
		BRect bounds = Bounds();
		if (pos.x < bounds.left - 20 || pos.x > bounds.right + 20
			|| pos.y < bounds.top - 20 || pos.y > bounds.bottom + 20)
			continue;

		rgb_color pinColor = SarMarkerColor(pin->colorIndex);
		float radius = 8.0f;

		// Filled circle with pin color
		SetHighColor(pinColor);
		FillEllipse(pos, radius, radius);

		// Dark border
		rgb_color dark = tint_color(pinColor, B_DARKEN_3_TINT);
		SetHighColor(dark);
		StrokeEllipse(pos, radius, radius);

		// Draw emoji centered in circle
		SetHighColor(255, 255, 255);
		font_height fh;
		GetFontHeight(&fh);
		float emojiWidth = StringWidth(pin->emoji);
		DrawString(pin->emoji,
			BPoint(pos.x - emojiWidth / 2,
				pos.y + fh.ascent / 2 - 1));

		// Label below pin
		if (pin->name[0] != '\0') {
			BFont labelFont(be_plain_font);
			labelFont.SetSize(9.0f);
			SetFont(&labelFont);

			float nameWidth = labelFont.StringWidth(pin->name);
			float labelX = pos.x - nameWidth / 2;
			float labelY = pos.y + radius + fh.ascent + 2;

			// Background for readability
			rgb_color labelBg = ui_color(B_PANEL_BACKGROUND_COLOR);
			labelBg.alpha = 200;
			BRect labelRect(labelX - 2, labelY - fh.ascent,
				labelX + nameWidth + 2, labelY + fh.descent);
			SetHighColor(labelBg);
			FillRoundRect(labelRect, 2, 2);

			SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
			DrawString(pin->name, BPoint(labelX, labelY));

			SetFont(be_plain_font);
		}
	}
}


void
MapView::ZoomIn()
{
	int level = _LevelForZoom(fZoom);
	if (level < kMaxZoomLevel) {
		fZoom = _ZoomForLevel(level + 1);
		Invalidate();
	}
}


void
MapView::ZoomOut()
{
	int level = _LevelForZoom(fZoom);
	if (level > kMinZoomLevel) {
		fZoom = _ZoomForLevel(level - 1);
		Invalidate();
	}
}


void
MapView::ZoomToFit()
{
	if (fNodes.CountItems() == 0 && !fHasSelfPosition)
		return;

	float minLat = 90, maxLat = -90;
	float minLon = 180, maxLon = -180;

	if (fHasSelfPosition) {
		minLat = maxLat = fSelfNode.latitude;
		minLon = maxLon = fSelfNode.longitude;
	}

	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GeoMapNode* node = fNodes.ItemAt(i);
		if (node->latitude < minLat) minLat = node->latitude;
		if (node->latitude > maxLat) maxLat = node->latitude;
		if (node->longitude < minLon) minLon = node->longitude;
		if (node->longitude > maxLon) maxLon = node->longitude;
	}

	fCenterLat = (minLat + maxLat) / 2.0f;
	fCenterLon = (minLon + maxLon) / 2.0f;

	BRect bounds = Bounds();

	// Ensure minimum visible area
	float lonSpan = std::max(maxLon - minLon, kMinFitSpan);

	// For Mercator, compute Y span in mercator degrees
	float mercMin = _MercatorY(minLat);
	float mercMax = _MercatorY(maxLat);
	float mercSpan = std::max(mercMax - mercMin, kMinFitSpan);

	// fZoom is pixels per degree of longitude
	float zoomLon = (bounds.Width() * 0.7f) / lonSpan;
	float zoomLat = (bounds.Height() * 0.7f) / mercSpan;

	float rawZoom = std::min(zoomLat, zoomLon);
	int level = _LevelForZoom(rawZoom);
	fZoom = _ZoomForLevel(level);

	Invalidate();
}


void
MapView::CenterOnSelf()
{
	if (fHasSelfPosition) {
		fCenterLat = fSelfNode.latitude;
		fCenterLon = fSelfNode.longitude;
		Invalidate();
	}
}


void
MapView::SetTilesEnabled(bool enabled)
{
	fShowTiles = enabled;
	if (fTileCache != NULL)
		fTileCache->SetEnabled(enabled);
	Invalidate();
}


void
MapView::SaveMapState()
{
	BPath settingsPath;
	find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath);
	settingsPath.Append("Sestriere");
	create_directory(settingsPath.Path(), 0755);
	settingsPath.Append("map.settings");

	FILE* fp = fopen(settingsPath.Path(), "w");
	if (fp == NULL)
		return;

	fprintf(fp, "center_lat=%.6f\n", fCenterLat);
	fprintf(fp, "center_lon=%.6f\n", fCenterLon);
	fprintf(fp, "zoom=%.2f\n", fZoom);
	fprintf(fp, "tiles=%d\n", fShowTiles ? 1 : 0);
	fclose(fp);
}


void
MapView::LoadMapState()
{
	BPath settingsPath;
	find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath);
	settingsPath.Append("Sestriere/map.settings");

	FILE* fp = fopen(settingsPath.Path(), "r");
	if (fp == NULL)
		return;

	char line[128];
	while (fgets(line, sizeof(line), fp) != NULL) {
		double val;
		int ival;
		if (sscanf(line, "center_lat=%lf", &val) == 1) {
			if (val >= -85.0 && val <= 85.0)
				fCenterLat = (float)val;
		} else if (sscanf(line, "center_lon=%lf", &val) == 1) {
			if (val >= -180.0 && val <= 180.0)
				fCenterLon = (float)val;
		} else if (sscanf(line, "zoom=%lf", &val) == 1) {
			float minZoom = _ZoomForLevel(kMinZoomLevel);
			float maxZoom = _ZoomForLevel(kMaxZoomLevel);
			if (val >= minZoom && val <= maxZoom)
				fZoom = (float)val;
		} else if (sscanf(line, "tiles=%d", &ival) == 1) {
			fShowTiles = (ival != 0);
			if (fTileCache != NULL)
				fTileCache->SetEnabled(fShowTiles);
		}
	}
	fclose(fp);
}


// ============================================================================
// Mercator projection
// ============================================================================

/*static*/ float
MapView::_MercatorY(float latDeg)
{
	// Clamp to avoid infinity at poles
	if (latDeg > 85.051f) latDeg = 85.051f;
	if (latDeg < -85.051f) latDeg = -85.051f;

	float latRad = latDeg * M_PI / 180.0f;
	return log(tan(M_PI / 4.0f + latRad / 2.0f)) * (180.0f / M_PI);
}


BPoint
MapView::_LatLonToScreen(float lat, float lon) const
{
	BRect bounds = Bounds();
	float centerX = bounds.Width() / 2.0f;
	float centerY = bounds.Height() / 2.0f;

	// fZoom = pixels per degree of longitude
	float x = centerX + (lon - fCenterLon) * fZoom;
	float y = centerY - (_MercatorY(lat) - _MercatorY(fCenterLat)) * fZoom;

	return BPoint(x, y);
}


void
MapView::_ScreenToLatLon(BPoint screen, float& lat, float& lon) const
{
	BRect bounds = Bounds();
	float centerX = bounds.Width() / 2.0f;
	float centerY = bounds.Height() / 2.0f;

	lon = fCenterLon + (screen.x - centerX) / fZoom;

	float mercY = _MercatorY(fCenterLat) - (screen.y - centerY) / fZoom;
	// Inverse Mercator: lat = atan(sinh(mercY * pi / 180)) * 180 / pi
	lat = atan(sinh(mercY * M_PI / 180.0f)) * 180.0f / M_PI;
}


// ============================================================================
// Tile drawing
// ============================================================================

int
MapView::_ZoomToTileZoom() const
{
	// fZoom is snapped to discrete levels, so this is a direct conversion.
	// Clamp to OSM tile range (tiles available up to z=17).
	int tileZ = _LevelForZoom(fZoom);
	if (tileZ > 17) tileZ = 17;
	return tileZ;
}


void
MapView::_DrawTiles()
{
	if (fTileCache == NULL || !fShowTiles)
		return;

	BRect bounds = Bounds();
	int tileZ = _ZoomToTileZoom();
	int numTiles = 1 << tileZ;

	// Get visible area in lat/lon
	float topLat, leftLon, botLat, rightLon;
	_ScreenToLatLon(BPoint(0, 0), topLat, leftLon);
	_ScreenToLatLon(BPoint(bounds.Width(), bounds.Height()), botLat, rightLon);

	// Convert to tile coords
	// tile x = floor((lon + 180) / 360 * 2^z)
	int minTileX = (int)floor((leftLon + 180.0f) / 360.0f * numTiles);
	int maxTileX = (int)floor((rightLon + 180.0f) / 360.0f * numTiles);

	// tile y = floor((1 - ln(tan(lat) + sec(lat)) / pi) / 2 * 2^z)
	float latRadTop = topLat * M_PI / 180.0f;
	float latRadBot = botLat * M_PI / 180.0f;

	int minTileY = (int)floor((1.0f - log(tan(latRadTop) +
		1.0f / cos(latRadTop)) / M_PI) / 2.0f * numTiles);
	int maxTileY = (int)floor((1.0f - log(tan(latRadBot) +
		1.0f / cos(latRadBot)) / M_PI) / 2.0f * numTiles);

	// Clamp
	if (minTileX < 0) minTileX = 0;
	if (maxTileX >= numTiles) maxTileX = numTiles - 1;
	if (minTileY < 0) minTileY = 0;
	if (maxTileY >= numTiles) maxTileY = numTiles - 1;

	// Draw cached tiles
	for (int tx = minTileX; tx <= maxTileX; tx++) {
		for (int ty = minTileY; ty <= maxTileY; ty++) {
			BBitmap* bitmap = fTileCache->GetCachedTile(tileZ, tx, ty);
			if (bitmap == NULL)
				continue;

			// Tile top-left corner in lat/lon
			float tileLon = (float)tx * 360.0f / numTiles - 180.0f;
			float n = M_PI - 2.0f * M_PI * ty / numTiles;
			float tileLat = 180.0f / M_PI * atan(0.5f *
				(exp(n) - exp(-n)));

			// Tile bottom-right
			float nextLon = (float)(tx + 1) * 360.0f / numTiles - 180.0f;
			float n2 = M_PI - 2.0f * M_PI * (ty + 1) / numTiles;
			float nextLat = 180.0f / M_PI * atan(0.5f *
				(exp(n2) - exp(-n2)));

			BPoint topLeft = _LatLonToScreen(tileLat, tileLon);
			BPoint botRight = _LatLonToScreen(nextLat, nextLon);

			BRect destRect(topLeft.x, topLeft.y, botRight.x, botRight.y);
			DrawBitmap(bitmap, destRect);
		}
	}

	// Request tiles async (if visible range changed)
	if (tileZ != fLastTileZ || minTileX != fLastTileMinX
		|| minTileY != fLastTileMinY || maxTileX != fLastTileMaxX
		|| maxTileY != fLastTileMaxY) {
		fLastTileZ = tileZ;
		fLastTileMinX = minTileX;
		fLastTileMinY = minTileY;
		fLastTileMaxX = maxTileX;
		fLastTileMaxY = maxTileY;

		fTileCache->RequestTiles(tileZ, minTileX, minTileY,
			maxTileX, maxTileY, this);
	}
}


// ============================================================================
// Coastline drawing
// ============================================================================

void
MapView::_DrawCoastlines()
{
	if (fShowTiles) {
		// When tiles are active, draw thin coastline outlines only
		SetHighColor(kCoastlineStroke);
		SetPenSize(1.0f);
	} else {
		// No tiles: fill land polygons
		SetHighColor(kLandColor);
	}

	// Walk through coastline data drawing polylines
	int i = 0;
	while (i < kCoastlinePointCount) {
		float lat = kCoastlineData[i * 2];
		float lon = kCoastlineData[i * 2 + 1];

		if (lat >= 998.0f && lon >= 998.0f) {
			// Sentinel — skip
			i++;
			continue;
		}

		// Collect polyline points until next sentinel
		BPoint polyPoints[256];
		int pointCount = 0;

		while (i < kCoastlinePointCount && pointCount < 256) {
			lat = kCoastlineData[i * 2];
			lon = kCoastlineData[i * 2 + 1];

			if (lat >= 998.0f && lon >= 998.0f) {
				i++;
				break;
			}

			polyPoints[pointCount] = _LatLonToScreen(lat, lon);
			pointCount++;
			i++;
		}

		if (pointCount < 2)
			continue;

		if (fShowTiles) {
			// Just stroke the outline
			for (int p = 0; p < pointCount - 1; p++)
				StrokeLine(polyPoints[p], polyPoints[p + 1]);
		} else {
			// Fill the polygon for land
			if (pointCount >= 3) {
				SetHighColor(kLandColor);
				FillPolygon(polyPoints, pointCount);
			}
			// Stroke the outline
			SetHighColor(kCoastlineStroke);
			SetPenSize(1.0f);
			for (int p = 0; p < pointCount - 1; p++)
				StrokeLine(polyPoints[p], polyPoints[p + 1]);
		}
	}

	SetPenSize(1.0f);
}


void
MapView::_DrawGrid()
{
	BRect bounds = Bounds();

	float minLat, maxLat, minLon, maxLon;
	_ScreenToLatLon(BPoint(0, bounds.Height()), minLat, minLon);
	_ScreenToLatLon(BPoint(bounds.Width(), 0), maxLat, maxLon);

	// Adaptive spacing: maintain ~50-120px between grid lines at all zoom levels
	float gridSpacing;
	if (fZoom > 50000) gridSpacing = 0.001f;
	else if (fZoom > 10000) gridSpacing = 0.01f;
	else if (fZoom > 1000) gridSpacing = 0.1f;
	else if (fZoom > 500) gridSpacing = 0.5f;
	else if (fZoom > 100) gridSpacing = 1.0f;
	else if (fZoom > 40) gridSpacing = 2.0f;
	else if (fZoom > 20) gridSpacing = 5.0f;
	else if (fZoom > 8) gridSpacing = 10.0f;
	else if (fZoom > 4) gridSpacing = 15.0f;
	else gridSpacing = 30.0f;

	// Theme-aware grid color
	rgb_color panelBg = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color gridColor = tint_color(panelBg, B_DARKEN_2_TINT);
	gridColor.alpha = fShowTiles ? 60 : 120;
	SetHighColor(gridColor);
	SetDrawingMode(B_OP_ALPHA);
	SetPenSize(1.0f);

	float startLat = floor(minLat / gridSpacing) * gridSpacing;
	for (float lat = startLat; lat <= maxLat; lat += gridSpacing) {
		BPoint p1 = _LatLonToScreen(lat, minLon);
		BPoint p2 = _LatLonToScreen(lat, maxLon);
		StrokeLine(p1, p2);
	}

	float startLon = floor(minLon / gridSpacing) * gridSpacing;
	for (float lon = startLon; lon <= maxLon; lon += gridSpacing) {
		BPoint p1 = _LatLonToScreen(minLat, lon);
		BPoint p2 = _LatLonToScreen(maxLat, lon);
		StrokeLine(p1, p2);
	}

	// Coordinate labels on grid edges
	BFont labelFont(be_plain_font);
	labelFont.SetSize(9.0f);
	SetFont(&labelFont);
	font_height fh;
	labelFont.GetHeight(&fh);
	float fontAscent = fh.ascent;
	float fontHeight = fh.ascent + fh.descent;

	rgb_color labelBg = panelBg;
	labelBg.alpha = 180;
	rgb_color textColor = tint_color(panelBg, B_DARKEN_MAX_TINT);

	const float kLabelPadH = 2.0f;
	const float kLabelPadV = 1.0f;
	const float kLabelMargin = 3.0f;

	// Latitude labels on left edge
	for (float lat = startLat; lat <= maxLat; lat += gridSpacing) {
		BPoint screenPt = _LatLonToScreen(lat, minLon);
		float y = screenPt.y;
		if (y < fontHeight || y > bounds.Height() - kLabelMargin)
			continue;

		char label[16];
		float absLat = fabsf(lat);
		if (gridSpacing >= 1.0f)
			snprintf(label, sizeof(label), "%d°%s", (int)absLat,
				lat > 0 ? "N" : (lat < 0 ? "S" : ""));
		else
			snprintf(label, sizeof(label), "%.1f°%s", absLat,
				lat > 0 ? "N" : (lat < 0 ? "S" : ""));

		float labelWidth = labelFont.StringWidth(label);
		BRect bg(kLabelMargin, y - fontAscent - kLabelPadV,
			kLabelMargin + labelWidth + 2 * kLabelPadH,
			y + fh.descent + kLabelPadV);
		SetHighColor(labelBg);
		FillRect(bg);
		SetHighColor(textColor);
		DrawString(label, BPoint(kLabelMargin + kLabelPadH, y));
	}

	// Longitude labels on bottom edge
	for (float lon = startLon; lon <= maxLon; lon += gridSpacing) {
		BPoint screenPt = _LatLonToScreen(maxLat, lon);
		float x = screenPt.x;

		char label[16];
		float absLon = fabsf(lon);
		if (gridSpacing >= 1.0f)
			snprintf(label, sizeof(label), "%d°%s", (int)absLon,
				lon > 0 ? "E" : (lon < 0 ? "W" : ""));
		else
			snprintf(label, sizeof(label), "%.1f°%s", absLon,
				lon > 0 ? "E" : (lon < 0 ? "W" : ""));

		float labelWidth = labelFont.StringWidth(label);
		if (x < kLabelMargin || x + labelWidth > bounds.Width() - kLabelMargin)
			continue;

		float labelY = bounds.Height() - kLabelMargin - fh.descent;
		BRect bg(x - kLabelPadH, labelY - fontAscent - kLabelPadV,
			x + labelWidth + kLabelPadH,
			labelY + fh.descent + kLabelPadV);
		SetHighColor(labelBg);
		FillRect(bg);
		SetHighColor(textColor);
		DrawString(label, BPoint(x, labelY));
	}

	SetDrawingMode(B_OP_COPY);
}


void
MapView::_DrawNodes()
{
	if (fHasSelfPosition)
		_DrawNode(fSelfNode);

	for (int32 i = 0; i < fNodes.CountItems(); i++)
		_DrawNode(*fNodes.ItemAt(i));
}


void
MapView::_DrawNode(const GeoMapNode& node)
{
	BPoint pos = _LatLonToScreen(node.latitude, node.longitude);
	BRect bounds = Bounds();

	if (pos.x < -20 || pos.x > bounds.Width() + 20 ||
		pos.y < -20 || pos.y > bounds.Height() + 20)
		return;

	float radius = node.isSelf ? kSelfRadius : kNodeRadius;
	rgb_color fillColor = _ColorForType(node.type);
	rgb_color outlineColor = {255, 255, 255, 255};

	if (node.isSelected) {
		outlineColor = (rgb_color){255, 200, 0, 255};
		radius += 2;
	} else if (&node == fHoverNode) {
		outlineColor = (rgb_color){200, 200, 255, 255};
		radius += 1;
	}

	// Outer circle
	SetHighColor(outlineColor);
	FillEllipse(pos, radius + 2, radius + 2);

	// Inner circle
	SetHighColor(fillColor);
	FillEllipse(pos, radius, radius);

	// Self indicator
	if (node.isSelf) {
		SetHighColor(255, 255, 255);
		SetPenSize(2.0f);
		for (int j = 0; j < 4; j++) {
			float angle = j * M_PI / 4.0f;
			BPoint p1(pos.x + cos(angle) * 4, pos.y + sin(angle) * 4);
			BPoint p2(pos.x + cos(angle) * radius,
				pos.y + sin(angle) * radius);
			StrokeLine(p1, p2);
		}
		SetPenSize(1.0f);
	}

	// Name label
	SetHighColor(255, 255, 255);
	BFont font;
	GetFont(&font);
	font.SetSize(10);
	SetFont(&font);

	float labelWidth = font.StringWidth(node.name);
	BPoint labelPos(pos.x - labelWidth / 2, pos.y + radius + 12);

	// Label background
	SetHighColor(0, 0, 0, 180);
	FillRoundRect(BRect(labelPos.x - 2, labelPos.y - 10,
		labelPos.x + labelWidth + 2, labelPos.y + 2), 3, 3);

	// Label text
	SetHighColor(255, 255, 255);
	DrawString(node.name, labelPos);
}


void
MapView::_DrawConnections()
{
	if (!fHasSelfPosition)
		return;

	BPoint selfPos = _LatLonToScreen(fSelfNode.latitude, fSelfNode.longitude);
	SetPenSize(1.5f);

	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GeoMapNode* node = fNodes.ItemAt(i);
		BPoint nodePos = _LatLonToScreen(node->latitude, node->longitude);

		rgb_color lineColor;
		if (node->pathLen == -1) {
			lineColor = (rgb_color){100, 100, 100, 128};
		} else if (node->pathLen == (int8)kPathLenDirect || node->pathLen == 0) {
			lineColor = (rgb_color){0, 200, 0, 180};
		} else if (node->pathLen <= 2) {
			lineColor = (rgb_color){200, 200, 0, 180};
		} else {
			lineColor = (rgb_color){200, 100, 0, 180};
		}

		SetHighColor(lineColor);

		if (node->pathLen == -1) {
			// Dashed line
			float length = sqrt(pow(nodePos.x - selfPos.x, 2) +
				pow(nodePos.y - selfPos.y, 2));
			float dashLen = 5.0f;
			int dashes = (int)(length / dashLen);

			for (int d = 0; d < dashes; d += 2) {
				float t1 = (float)d / dashes;
				float t2 = (float)(d + 1) / dashes;
				BPoint p1(selfPos.x + t1 * (nodePos.x - selfPos.x),
					selfPos.y + t1 * (nodePos.y - selfPos.y));
				BPoint p2(selfPos.x + t2 * (nodePos.x - selfPos.x),
					selfPos.y + t2 * (nodePos.y - selfPos.y));
				StrokeLine(p1, p2);
			}
		} else {
			StrokeLine(selfPos, nodePos);
		}
	}

	SetPenSize(1.0f);
}


void
MapView::_DrawScaleBar()
{
	BRect bounds = Bounds();

	// At the equator, 1 degree lon ≈ 111km.
	// fZoom = pixels per degree lon, so metersPerPixel ≈ 111000 / fZoom
	float metersPerPixel = 111000.0f / fZoom;
	float targetPixels = 100.0f;
	float targetMeters = metersPerPixel * targetPixels;

	float scale;
	const char* unit;
	if (targetMeters >= 1000) {
		scale = floor(targetMeters / 1000.0f);
		if (scale < 1) scale = 1;
		unit = "km";
		targetMeters = scale * 1000;
	} else {
		scale = floor(targetMeters / 100.0f) * 100;
		if (scale < 100) scale = 100;
		unit = "m";
		targetMeters = scale;
	}

	float barPixels = targetMeters / metersPerPixel;

	float x = 20;
	float y = bounds.Height() - 30;

	SetHighColor(255, 255, 255);
	SetPenSize(2.0f);
	StrokeLine(BPoint(x, y), BPoint(x + barPixels, y));
	StrokeLine(BPoint(x, y - 5), BPoint(x, y + 5));
	StrokeLine(BPoint(x + barPixels, y - 5), BPoint(x + barPixels, y + 5));
	SetPenSize(1.0f);

	char label[32];
	snprintf(label, sizeof(label), "%.0f %s", scale, unit);
	DrawString(label, BPoint(x + barPixels / 2 - 15, y - 8));
}


void
MapView::_DrawCompass()
{
	BRect bounds = Bounds();
	float cx = bounds.Width() - 40;
	float cy = 40;
	float size = 20;

	SetHighColor(255, 255, 255);
	SetPenSize(2.0f);

	StrokeLine(BPoint(cx, cy - size), BPoint(cx, cy + size));
	StrokeLine(BPoint(cx - size, cy), BPoint(cx + size, cy));

	// North arrow
	SetHighColor(255, 0, 0);
	FillTriangle(BPoint(cx, cy - size),
		BPoint(cx - 5, cy - size + 10),
		BPoint(cx + 5, cy - size + 10));

	SetHighColor(255, 255, 255);
	DrawString("N", BPoint(cx - 4, cy - size - 5));

	SetPenSize(1.0f);
}


GeoMapNode*
MapView::_FindNodeAt(BPoint where)
{
	float radius = kNodeRadius + 5;

	if (fHasSelfPosition) {
		BPoint pos = _LatLonToScreen(fSelfNode.latitude, fSelfNode.longitude);
		float dx = where.x - pos.x;
		float dy = where.y - pos.y;
		if (sqrt(dx * dx + dy * dy) <= kSelfRadius + 5)
			return &fSelfNode;
	}

	for (int32 i = fNodes.CountItems() - 1; i >= 0; i--) {
		GeoMapNode* node = fNodes.ItemAt(i);
		BPoint pos = _LatLonToScreen(node->latitude, node->longitude);
		float dx = where.x - pos.x;
		float dy = where.y - pos.y;
		if (sqrt(dx * dx + dy * dy) <= radius)
			return node;
	}

	return NULL;
}


rgb_color
MapView::_ColorForType(uint8 type) const
{
	switch (type) {
		case 1:		// Companion (chat device)
			return (rgb_color){100, 180, 255, 255};
		case 2:		// Repeater
			return (rgb_color){100, 255, 100, 255};
		case 3:		// Room
			return (rgb_color){255, 180, 100, 255};
		default:
			return (rgb_color){180, 180, 180, 255};
	}
}


// ============================================================================
// MapWindow
// ============================================================================

MapWindow::MapWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 599, 449), "Geographic Map", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fParent(parent)
{
	fMapView = new MapView("map");
	fMapView->SetExplicitMinSize(BSize(400, 300));

	fZoomInButton = new BButton("zoom_in", "+", new BMessage(kMsgZoomIn));
	fZoomOutButton = new BButton("zoom_out", "-", new BMessage(kMsgZoomOut));
	fFitButton = new BButton("fit", "Fit", new BMessage(kMsgZoomFit));
	fCenterButton = new BButton("center", "Center",
		new BMessage(kMsgCenterSelf));
	fTilesCheckBox = new BCheckBox("tiles", "Online Map",
		new BMessage(kMsgToggleTiles));

	BView* controlBar = new BView("controls", 0);
	controlBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BLayoutBuilder::Group<>(controlBar, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(fZoomInButton)
		.Add(fZoomOutButton)
		.Add(fFitButton)
		.Add(fCenterButton)
		.AddGlue()
		.Add(fTilesCheckBox)
	.End();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMapView, 1.0)
		.Add(controlBar)
	.End();

	if (fParent != NULL)
		CenterIn(fParent->Frame());
	else
		CenterOnScreen();

	// Sync checkbox with loaded tile preference
	fTilesCheckBox->SetValue(
		fMapView->TilesEnabled() ? B_CONTROL_ON : B_CONTROL_OFF);
}


MapWindow::~MapWindow()
{
}


void
MapWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgZoomIn:
			fMapView->ZoomIn();
			break;
		case kMsgZoomOut:
			fMapView->ZoomOut();
			break;
		case kMsgZoomFit:
			fMapView->ZoomToFit();
			break;
		case kMsgCenterSelf:
			fMapView->CenterOnSelf();
			break;
		case kMsgToggleTiles:
			fMapView->SetTilesEnabled(
				fTilesCheckBox->Value() == B_CONTROL_ON);
			break;
		case MSG_SAR_MARKER:
		{
			float lat = 0, lon = 0;
			int32 colorIndex = 0;
			const char* emoji = "";
			const char* name = "";
			message->FindFloat("lat", &lat);
			message->FindFloat("lon", &lon);
			message->FindInt32("color", &colorIndex);
			message->FindString("emoji", &emoji);
			message->FindString("name", &name);
			fMapView->AddSarPin(lat, lon, emoji, name, (int)colorIndex);
			break;
		}
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MapWindow::QuitRequested()
{
	fMapView->SaveMapState();
	Hide();
	return false;
}


void
MapWindow::SetSelfPosition(float lat, float lon, const char* name)
{
	fMapView->SetSelfPosition(lat, lon, name);
}


void
MapWindow::AddSarPin(float lat, float lon, const char* emoji,
	const char* name, int colorIndex)
{
	fMapView->AddSarPin(lat, lon, emoji, name, colorIndex);
}


void
MapWindow::UpdateFromContacts(OwningObjectList<ContactInfo>* contacts,
	double /*defaultLat*/, double /*defaultLon*/)
{
	fMapView->ClearNodes();

	for (int32 i = 0; i < contacts->CountItems(); i++) {
		ContactInfo* contact = contacts->ItemAt(i);
		if (contact == NULL || !contact->isValid)
			continue;

		// Only add contacts that have valid GPS coordinates
		if (!contact->HasGPS())
			continue;

		// Convert int32 (1e-6 degrees) to float degrees
		float lat = (float)(contact->latitude / 1e6);
		float lon = (float)(contact->longitude / 1e6);

		fMapView->AddOrUpdateNode(*contact, lat, lon);
	}
}
