/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MapView.cpp — Geographic map visualization implementation
 */

#include "MapView.h"

#include <Cursor.h>
#include <Window.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "Protocol.h"


static const float kMinZoom = 100.0f;		// pixels per degree
static const float kMaxZoom = 100000.0f;
static const float kDefaultZoom = 5000.0f;
static const float kZoomStep = 1.5f;

static const float kNodeRadius = 8.0f;
static const float kSelfRadius = 10.0f;


MapView::MapView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_FRAME_EVENTS),
	fNodes(20, true),
	fHasSelfPosition(false),
	fCenterLat(45.0f),		// Default: northern Italy
	fCenterLon(7.0f),
	fZoom(kDefaultZoom),
	fDragging(false),
	fDragStartLat(0),
	fDragStartLon(0),
	fSelectedNode(NULL),
	fHoverNode(NULL)
{
	memset(&fSelfNode, 0, sizeof(fSelfNode));
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
MapView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	// Background - dark blue for "sea"
	SetHighColor(30, 40, 60);
	FillRect(bounds);

	// Draw grid first
	_DrawGrid();

	// Draw connections between nodes
	_DrawConnections();

	// Draw all nodes
	_DrawNodes();

	// Draw UI elements
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

	MapNode* node = _FindNodeAt(where);

	if (buttons & B_PRIMARY_MOUSE_BUTTON) {
		if (clicks == 2 && node != NULL) {
			// Double-click on node: center on it
			SetCenterPosition(node->latitude, node->longitude);
		} else if (node != NULL) {
			// Single click: select node
			if (fSelectedNode != NULL)
				fSelectedNode->isSelected = false;
			fSelectedNode = node;
			fSelectedNode->isSelected = true;

			// Notify parent
			BMessage msg(MSG_CONTACT_SELECTED);
			msg.AddData(kFieldPubKey, B_RAW_TYPE, node->publicKey,
				kPubKeyPrefixSize);
			msg.AddString(kFieldName, node->name);
			Window()->PostMessage(&msg);

			Invalidate();
		} else {
			// Click on empty space: start drag
			fDragging = true;
			fDragStart = where;
			fDragStartLat = fCenterLat;
			fDragStartLon = fCenterLon;
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		}
	} else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		// Right-click: context menu (future)
	}
}


void
MapView::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
{
	if (fDragging) {
		// Pan the map
		float dx = where.x - fDragStart.x;
		float dy = where.y - fDragStart.y;

		// Convert pixel delta to lat/lon delta
		float dLon = -dx / fZoom;
		float dLat = dy / fZoom;

		fCenterLat = fDragStartLat + dLat;
		fCenterLon = fDragStartLon + dLon;

		// Clamp latitude
		if (fCenterLat > 85.0f) fCenterLat = 85.0f;
		if (fCenterLat < -85.0f) fCenterLat = -85.0f;

		Invalidate();
	} else {
		// Hover detection
		MapNode* node = _FindNodeAt(where);
		if (node != fHoverNode) {
			fHoverNode = node;
			Invalidate();
		}
	}
}


void
MapView::MouseUp(BPoint where)
{
	fDragging = false;
}


void
MapView::ScrollTo(BPoint where)
{
	// Handle scroll wheel for zoom
	BView::ScrollTo(where);
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
	fSelfNode.type = ADV_TYPE_CHAT;
	fSelfNode.isSelf = true;
	fHasSelfPosition = true;

	// Center on self initially
	fCenterLat = lat;
	fCenterLon = lon;

	Invalidate();
}


void
MapView::AddNode(const Contact& contact)
{
	// Skip contacts without position
	if (contact.advLat == 0 && contact.advLon == 0)
		return;

	MapNode* node = new MapNode();
	memcpy(node->publicKey, contact.publicKey, kPublicKeySize);
	strlcpy(node->name, contact.advName, sizeof(node->name));
	node->latitude = Protocol::LatLonFromInt(contact.advLat);
	node->longitude = Protocol::LatLonFromInt(contact.advLon);
	node->type = contact.type;
	node->pathLen = contact.outPathLen;
	node->lastSnr = 0;
	node->isSelected = false;
	node->isSelf = false;

	fNodes.AddItem(node);
	Invalidate();
}


void
MapView::UpdateNode(const Contact& contact)
{
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (memcmp(node->publicKey, contact.publicKey, kPublicKeySize) == 0) {
			node->latitude = Protocol::LatLonFromInt(contact.advLat);
			node->longitude = Protocol::LatLonFromInt(contact.advLon);
			strlcpy(node->name, contact.advName, sizeof(node->name));
			node->type = contact.type;
			node->pathLen = contact.outPathLen;
			Invalidate();
			return;
		}
	}

	// Not found, add it
	AddNode(contact);
}


void
MapView::RemoveNode(const uint8* publicKey)
{
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (memcmp(node->publicKey, publicKey, kPublicKeySize) == 0) {
			if (fSelectedNode == node)
				fSelectedNode = NULL;
			if (fHoverNode == node)
				fHoverNode = NULL;
			fNodes.RemoveItemAt(i);
			Invalidate();
			return;
		}
	}
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
MapView::SetZoom(float zoom)
{
	if (zoom < kMinZoom) zoom = kMinZoom;
	if (zoom > kMaxZoom) zoom = kMaxZoom;
	fZoom = zoom;
	Invalidate();
}


void
MapView::ZoomIn()
{
	SetZoom(fZoom * kZoomStep);
}


void
MapView::ZoomOut()
{
	SetZoom(fZoom / kZoomStep);
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
		MapNode* node = fNodes.ItemAt(i);
		if (node->latitude < minLat) minLat = node->latitude;
		if (node->latitude > maxLat) maxLat = node->latitude;
		if (node->longitude < minLon) minLon = node->longitude;
		if (node->longitude > maxLon) maxLon = node->longitude;
	}

	// Center on the bounding box
	fCenterLat = (minLat + maxLat) / 2.0f;
	fCenterLon = (minLon + maxLon) / 2.0f;

	// Calculate zoom to fit
	BRect bounds = Bounds();
	float latSpan = maxLat - minLat + 0.001f;	// avoid division by zero
	float lonSpan = maxLon - minLon + 0.001f;

	float zoomLat = (bounds.Height() * 0.8f) / latSpan;
	float zoomLon = (bounds.Width() * 0.8f) / lonSpan;

	SetZoom(std::min(zoomLat, zoomLon));
}


void
MapView::SetCenterPosition(float lat, float lon)
{
	fCenterLat = lat;
	fCenterLon = lon;
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

	// Calculate visible area
	float minLat, maxLat, minLon, maxLon;
	_ScreenToLatLon(BPoint(0, bounds.Height()), minLat, minLon);
	_ScreenToLatLon(BPoint(bounds.Width(), 0), maxLat, maxLon);

	// Determine grid spacing based on zoom
	float gridSpacing;
	if (fZoom > 50000) gridSpacing = 0.001f;
	else if (fZoom > 10000) gridSpacing = 0.01f;
	else if (fZoom > 1000) gridSpacing = 0.1f;
	else if (fZoom > 500) gridSpacing = 0.5f;
	else gridSpacing = 1.0f;

	SetHighColor(50, 60, 80);
	SetPenSize(1.0f);

	// Draw latitude lines
	float startLat = floor(minLat / gridSpacing) * gridSpacing;
	for (float lat = startLat; lat <= maxLat; lat += gridSpacing) {
		BPoint p1 = _LatLonToScreen(lat, minLon);
		BPoint p2 = _LatLonToScreen(lat, maxLon);
		StrokeLine(p1, p2);
	}

	// Draw longitude lines
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
	// Draw self node
	if (fHasSelfPosition)
		_DrawNode(fSelfNode);

	// Draw other nodes
	for (int32 i = 0; i < fNodes.CountItems(); i++)
		_DrawNode(*fNodes.ItemAt(i));
}


void
MapView::_DrawNode(const MapNode& node)
{
	BPoint pos = _LatLonToScreen(node.latitude, node.longitude);
	BRect bounds = Bounds();

	// Skip if off-screen
	if (pos.x < -20 || pos.x > bounds.Width() + 20 ||
		pos.y < -20 || pos.y > bounds.Height() + 20)
		return;

	float radius = node.isSelf ? kSelfRadius : kNodeRadius;
	rgb_color fillColor = _ColorForType(node.type);
	rgb_color outlineColor = {255, 255, 255, 255};

	// Highlight if selected or hovered
	if (node.isSelected) {
		outlineColor = (rgb_color){255, 200, 0, 255};
		radius += 2;
	} else if (&node == fHoverNode) {
		outlineColor = (rgb_color){200, 200, 255, 255};
		radius += 1;
	}

	// Draw outer circle (outline)
	SetHighColor(outlineColor);
	FillEllipse(pos, radius + 2, radius + 2);

	// Draw inner circle (fill)
	SetHighColor(fillColor);
	FillEllipse(pos, radius, radius);

	// Draw self indicator (star pattern)
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

	// Draw name label
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
		MapNode* node = fNodes.ItemAt(i);
		BPoint nodePos = _LatLonToScreen(node->latitude, node->longitude);

		// Color based on path length
		rgb_color lineColor;
		if (node->pathLen == -1) {
			lineColor = (rgb_color){100, 100, 100, 128};  // Unknown
		} else if (node->pathLen == (int8)0xFF || node->pathLen == 0) {
			lineColor = (rgb_color){0, 200, 0, 180};      // Direct
		} else if (node->pathLen <= 2) {
			lineColor = (rgb_color){200, 200, 0, 180};    // 1-2 hops
		} else {
			lineColor = (rgb_color){200, 100, 0, 180};    // 3+ hops
		}

		SetHighColor(lineColor);

		// Draw dashed line for unknown paths
		if (node->pathLen == -1) {
			// Simple dashed line
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

	// Calculate scale bar length for nice round distance
	float metersPerPixel = 111000.0f / fZoom;  // approx meters per degree lat
	float targetPixels = 100.0f;
	float targetMeters = metersPerPixel * targetPixels;

	// Round to nice value
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

	// Draw scale bar
	float x = 20;
	float y = bounds.Height() - 30;

	SetHighColor(255, 255, 255);
	SetPenSize(2.0f);
	StrokeLine(BPoint(x, y), BPoint(x + barPixels, y));
	StrokeLine(BPoint(x, y - 5), BPoint(x, y + 5));
	StrokeLine(BPoint(x + barPixels, y - 5), BPoint(x + barPixels, y + 5));
	SetPenSize(1.0f);

	// Label
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

	// Draw compass rose
	SetHighColor(255, 255, 255);
	SetPenSize(2.0f);

	// N-S line
	StrokeLine(BPoint(cx, cy - size), BPoint(cx, cy + size));
	// E-W line
	StrokeLine(BPoint(cx - size, cy), BPoint(cx + size, cy));

	// North arrow
	SetHighColor(255, 0, 0);
	FillTriangle(BPoint(cx, cy - size),
		BPoint(cx - 5, cy - size + 10),
		BPoint(cx + 5, cy - size + 10));

	// N label
	SetHighColor(255, 255, 255);
	DrawString("N", BPoint(cx - 4, cy - size - 5));

	SetPenSize(1.0f);
}


MapNode*
MapView::_FindNodeAt(BPoint where)
{
	float radius = kNodeRadius + 5;

	// Check self node first
	if (fHasSelfPosition) {
		BPoint pos = _LatLonToScreen(fSelfNode.latitude, fSelfNode.longitude);
		if ((where - pos).Length() <= kSelfRadius + 5)
			return &fSelfNode;
	}

	// Check other nodes
	for (int32 i = fNodes.CountItems() - 1; i >= 0; i--) {
		MapNode* node = fNodes.ItemAt(i);
		BPoint pos = _LatLonToScreen(node->latitude, node->longitude);
		if ((where - pos).Length() <= radius)
			return node;
	}

	return NULL;
}


rgb_color
MapView::_ColorForSnr(int8 snr) const
{
	float snrDb = snr / 4.0f;

	if (snrDb >= 10)
		return (rgb_color){0, 255, 0, 255};		// Excellent
	else if (snrDb >= 5)
		return (rgb_color){150, 255, 0, 255};	// Good
	else if (snrDb >= 0)
		return (rgb_color){255, 255, 0, 255};	// Fair
	else if (snrDb >= -5)
		return (rgb_color){255, 150, 0, 255};	// Poor
	else
		return (rgb_color){255, 0, 0, 255};		// Bad
}


rgb_color
MapView::_ColorForType(uint8 type) const
{
	switch (type) {
		case ADV_TYPE_CHAT:
			return (rgb_color){100, 180, 255, 255};	// Blue
		case ADV_TYPE_REPEATER:
			return (rgb_color){100, 255, 100, 255};	// Green
		case ADV_TYPE_ROOM:
			return (rgb_color){255, 180, 100, 255};	// Orange
		default:
			return (rgb_color){180, 180, 180, 255};	// Gray
	}
}
