/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MapView.cpp — Geographic map visualization implementation
 */

#include "MapView.h"

#include <Button.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Constants.h"


static const float kMinZoom = 100.0f;
static const float kMaxZoom = 100000.0f;
static const float kDefaultZoom = 5000.0f;
static const float kZoomStep = 1.5f;
static const float kNodeRadius = 8.0f;
static const float kSelfRadius = 10.0f;

static const uint32 kMsgZoomIn		= 'zmin';
static const uint32 kMsgZoomOut		= 'zmot';
static const uint32 kMsgZoomFit		= 'zmft';
static const uint32 kMsgCenterSelf	= 'cnsl';


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
	fZoom(kDefaultZoom),
	fDragging(false),
	fDragStartLat(0),
	fDragStartLon(0),
	fSelectedNode(NULL),
	fHoverNode(NULL)
{
	fSelfNode = GeoMapNode();
	fSelfNode.isSelf = true;
	SetViewColor(B_TRANSPARENT_COLOR);
}


MapView::~MapView()
{
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

	// Background - dark blue for "sea"
	SetHighColor(30, 40, 60);
	FillRect(bounds);

	_DrawGrid();
	_DrawConnections();
	_DrawNodes();
	_DrawScaleBar();
	_DrawCompass();
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

		float dLon = -dx / fZoom;
		float dLat = dy / fZoom;

		fCenterLat = fDragStartLat + dLat;
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
MapView::ZoomIn()
{
	fZoom *= kZoomStep;
	if (fZoom > kMaxZoom) fZoom = kMaxZoom;
	Invalidate();
}


void
MapView::ZoomOut()
{
	fZoom /= kZoomStep;
	if (fZoom < kMinZoom) fZoom = kMinZoom;
	Invalidate();
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
	float latSpan = maxLat - minLat + 0.001f;
	float lonSpan = maxLon - minLon + 0.001f;

	float zoomLat = (bounds.Height() * 0.8f) / latSpan;
	float zoomLon = (bounds.Width() * 0.8f) / lonSpan;

	fZoom = std::min(zoomLat, zoomLon);
	if (fZoom < kMinZoom) fZoom = kMinZoom;
	if (fZoom > kMaxZoom) fZoom = kMaxZoom;

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


BPoint
MapView::_LatLonToScreen(float lat, float lon) const
{
	BRect bounds = Bounds();
	float centerX = bounds.Width() / 2.0f;
	float centerY = bounds.Height() / 2.0f;

	float x = centerX + (lon - fCenterLon) * fZoom;
	float y = centerY - (lat - fCenterLat) * fZoom;

	return BPoint(x, y);
}


void
MapView::_ScreenToLatLon(BPoint screen, float& lat, float& lon) const
{
	BRect bounds = Bounds();
	float centerX = bounds.Width() / 2.0f;
	float centerY = bounds.Height() / 2.0f;

	lon = fCenterLon + (screen.x - centerX) / fZoom;
	lat = fCenterLat - (screen.y - centerY) / fZoom;
}


void
MapView::_DrawGrid()
{
	BRect bounds = Bounds();

	float minLat, maxLat, minLon, maxLon;
	_ScreenToLatLon(BPoint(0, bounds.Height()), minLat, minLon);
	_ScreenToLatLon(BPoint(bounds.Width(), 0), maxLat, maxLon);

	float gridSpacing;
	if (fZoom > 50000) gridSpacing = 0.001f;
	else if (fZoom > 10000) gridSpacing = 0.01f;
	else if (fZoom > 1000) gridSpacing = 0.1f;
	else if (fZoom > 500) gridSpacing = 0.5f;
	else gridSpacing = 1.0f;

	SetHighColor(50, 60, 80);
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
		for (int i = 0; i < 4; i++) {
			float angle = i * M_PI / 4.0f;
			BPoint p1(pos.x + cos(angle) * 4, pos.y + sin(angle) * 4);
			BPoint p2(pos.x + cos(angle) * radius, pos.y + sin(angle) * radius);
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
	fCenterButton = new BButton("center", "Center", new BMessage(kMsgCenterSelf));

	BView* controlBar = new BView("controls", 0);
	controlBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BLayoutBuilder::Group<>(controlBar, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(fZoomInButton)
		.Add(fZoomOutButton)
		.Add(fFitButton)
		.Add(fCenterButton)
		.AddGlue()
	.End();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMapView, 1.0)
		.Add(controlBar)
	.End();

	if (fParent != NULL)
		CenterIn(fParent->Frame());
	else
		CenterOnScreen();
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
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MapWindow::QuitRequested()
{
	Hide();
	return false;
}


void
MapWindow::SetSelfPosition(float lat, float lon, const char* name)
{
	fMapView->SetSelfPosition(lat, lon, name);
}


void
MapWindow::UpdateFromContacts(BObjectList<ContactInfo, true>* contacts,
	double defaultLat, double defaultLon)
{
	fMapView->ClearNodes();

	for (int32 i = 0; i < contacts->CountItems(); i++) {
		ContactInfo* contact = contacts->ItemAt(i);
		if (contact == NULL || !contact->isValid)
			continue;

		// Use default lat/lon as placeholder for now
		// Real positions come from contact advert data
		float lat = (float)defaultLat;
		float lon = (float)defaultLon;

		// Offset nodes slightly so they don't overlap
		lat += (float)(i * 0.001);
		lon += (float)(i * 0.001);

		fMapView->AddOrUpdateNode(*contact, lat, lon);
	}
}
