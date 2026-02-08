/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * NetworkMapWindow.cpp — Dynamic network topology visualization
 */

#include "NetworkMapWindow.h"

#include "Constants.h"

#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <Slider.h>
#include <StringView.h>
#include <TextView.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "Constants.h"


static const uint32 MSG_REFRESH_MAP = 'rfmp';
static const uint32 MSG_MAP_TIMER = 'mptm';
static const uint32 MSG_ANIMATION_TICK = 'antk';
static const uint32 MSG_TOGGLE_LABELS = 'tglb';
static const uint32 MSG_TOGGLE_SIGNAL = 'tgsg';
static const uint32 MSG_TOGGLE_ANIMATION = 'tgan';
static const uint32 MSG_ZOOM_CHANGED = 'zoom';
static const uint32 MSG_NODE_CHAT = 'ndch';
static const uint32 MSG_NODE_TRACE = 'ndtr';
static const uint32 MSG_NODE_INFO = 'ndin';
static const uint32 MSG_AUTO_TRACE = 'autr';
static const uint32 MSG_TOGGLE_AUTO_TRACE = 'tgat';

static const bigtime_t kMapRefreshInterval = 3000000;   // 3 seconds
static const bigtime_t kAutoTraceInterval = 30000000;   // 30 seconds
static const bigtime_t kAnimationInterval = 50000;      // 50ms = 20fps

// Colors
static const rgb_color kBackgroundColor = {24, 28, 32, 255};
static const rgb_color kGridColor = {40, 44, 48, 255};
static const rgb_color kGridColorBright = {50, 55, 60, 255};
static const rgb_color kSelfNodeColor = {80, 160, 255, 255};
static const rgb_color kSelfNodeGlow = {80, 160, 255, 80};
static const rgb_color kNodeOutlineColor = {180, 180, 180, 255};
static const rgb_color kLabelColor = {220, 220, 220, 255};
static const rgb_color kLabelBgColor = {0, 0, 0, 180};
static const rgb_color kConnectionColor = {60, 80, 100, 150};
static const rgb_color kSelectedColor = {255, 200, 100, 255};
static const rgb_color kOfflineColor = {100, 100, 100, 255};
static const rgb_color kPulseColor = {255, 255, 255, 200};

// Signal quality colors (RSSI-based, legacy)
static const rgb_color kSignalExcellent = {50, 205, 50, 255};   // Lime green
static const rgb_color kSignalGood = {154, 205, 50, 255};       // Yellow-green
static const rgb_color kSignalFair = {255, 193, 37, 255};       // Goldenrod
static const rgb_color kSignalPoor = {255, 140, 0, 255};        // Dark orange
static const rgb_color kSignalBad = {220, 20, 60, 255};         // Crimson

// SNR-based link quality colors
static const rgb_color kLinkExcellent = {50, 205, 50, 255};     // Green (SNR > 5)
static const rgb_color kLinkGood = {100, 200, 100, 255};        // Light green (0..5)
static const rgb_color kLinkFair = {255, 193, 37, 255};         // Gold (-5..0)
static const rgb_color kLinkPoor = {255, 140, 0, 255};          // Orange (-10..-5)
static const rgb_color kLinkBad = {220, 20, 60, 255};           // Red (< -10)
static const rgb_color kLinkUnknown = {80, 80, 80, 255};        // Gray (no data)
static const rgb_color kTraceRouteColor = {180, 120, 255, 255}; // Purple for trace paths

// Node sizes
static const float kSelfNodeRadius = 24.0f;
static const float kNodeRadiusMin = 10.0f;
static const float kNodeRadiusMax = 18.0f;
static const float kMinDistance = 80.0f;
static const float kMaxDistance = 280.0f;

// Time thresholds (seconds)
static const uint32 kOnlineThreshold = 300;      // 5 minutes
static const uint32 kRecentThreshold = 3600;     // 1 hour
static const uint32 kStaleThreshold = 86400;     // 24 hours


// ============================================================================
// NetworkMapView
// ============================================================================

NetworkMapView::NetworkMapView()
	:
	BView("network_map", B_WILL_DRAW | B_FRAME_EVENTS | B_PULSE_NEEDED),
	fNodes(20),
	fCenter(0, 0),
	fZoom(1.0f),
	fShowLabels(true),
	fShowSignalStrength(true),
	fSelectedNode(NULL),
	fDragStart(0, 0),
	fDragging(false)
{
	SetViewColor(kBackgroundColor);
	memset(fSelfNode.name, 0, sizeof(fSelfNode.name));
	strlcpy(fSelfNode.name, "Me", sizeof(fSelfNode.name));
	fSelfNode.pulsePhase = 0;
	fSelfNode.activityLevel = 1.0f;
}


NetworkMapView::~NetworkMapView()
{
}


void
NetworkMapView::AttachedToWindow()
{
	BView::AttachedToWindow();
	// Start animation timer
	Window()->SetPulseRate(50000);  // 50ms
}


void
NetworkMapView::Pulse()
{
	// Update animations
	bool needsRedraw = false;

	// Self node gentle pulse
	fSelfNode.pulsePhase += 0.05f;
	if (fSelfNode.pulsePhase > 2 * M_PI)
		fSelfNode.pulsePhase -= 2 * M_PI;

	// Update all node animations
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		// Decay pulse effect
		if (node->pulsePhase > 0) {
			node->pulsePhase -= 0.1f;
			if (node->pulsePhase < 0)
				node->pulsePhase = 0;
			needsRedraw = true;
		}

		// Animate data flow dots for online/recent nodes
		if (node->status == STATUS_ONLINE || node->status == STATUS_RECENT) {
			node->flowPhase += 0.02f;
			if (node->flowPhase > 1.0f)
				node->flowPhase -= 1.0f;
			needsRedraw = true;
		}

		// Smooth position interpolation
		float dx = node->targetPosition.x - node->position.x;
		float dy = node->targetPosition.y - node->position.y;
		if (fabsf(dx) > 1 || fabsf(dy) > 1) {
			node->position.x += dx * 0.15f;
			node->position.y += dy * 0.15f;
			needsRedraw = true;
		}
	}

	if (needsRedraw)
		Invalidate();
}


void
NetworkMapView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	fCenter.Set(bounds.Width() / 2, bounds.Height() / 2);

	// Draw background gradient effect
	SetHighColor(kBackgroundColor);
	FillRect(bounds);

	// Draw concentric grid circles with distance labels
	SetPenSize(1.0f);
	BFont font;
	GetFont(&font);
	font.SetSize(9);
	SetFont(&font);

	for (int ring = 1; ring <= 5; ring++) {
		float radius = ring * 60 * fZoom;

		// Alternate grid colors
		SetHighColor(ring % 2 == 0 ? kGridColorBright : kGridColor);
		StrokeEllipse(fCenter, radius, radius);

		// Draw distance label
		if (fShowSignalStrength) {
			char distLabel[16];
			int rssiEst = -30 - (ring * 14);  // Rough RSSI estimate
			snprintf(distLabel, sizeof(distLabel), "%d dBm", rssiEst);

			SetHighColor(kGridColor);
			float labelWidth = StringWidth(distLabel);
			DrawString(distLabel, BPoint(fCenter.x - labelWidth / 2,
				fCenter.y - radius + 12));
		}
	}

	// Draw cross at center
	SetHighColor(kGridColorBright);
	float crossSize = 15.0f;
	StrokeLine(BPoint(fCenter.x - crossSize, fCenter.y),
			   BPoint(fCenter.x + crossSize, fCenter.y));
	StrokeLine(BPoint(fCenter.x, fCenter.y - crossSize),
			   BPoint(fCenter.x, fCenter.y + crossSize));

	// Calculate target positions for all nodes
	_CalculatePositions();

	// Draw connections first (behind nodes)
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node != NULL) {
			_DrawConnection(fCenter, node->position, node);
		}
	}

	// Draw trace route overlays (between connections and nodes)
	_DrawTraceRoutes();

	// Draw self node at center with glow
	_DrawSelfNode();

	// Draw all other nodes
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node != NULL) {
			_DrawNode(*node);
		}
	}

	// Draw info panel
	_DrawInfoPanel();

	// Draw link quality legend
	_DrawLinkQualityLegend();

	// Draw stats
	_DrawStats();
}


void
NetworkMapView::MouseDown(BPoint where)
{
	uint32 buttons;
	GetMouse(&where, &buttons);

	// Check if clicking on a node
	MapNode* clickedNode = NULL;

	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node != NULL) {
			float dist = sqrtf(powf(where.x - node->position.x, 2) +
							   powf(where.y - node->position.y, 2));
			float nodeRadius = _RadiusForNode(*node) * fZoom;
			if (dist <= nodeRadius) {
				clickedNode = node;
				break;
			}
		}
	}

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		// Right-click context menu
		if (clickedNode != NULL) {
			_ShowNodeContextMenu(where, clickedNode);
		}
		return;
	}

	// Left click - select
	fSelectedNode = clickedNode;

	// Deselect others
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node != NULL) {
			node->isSelected = (node == fSelectedNode);
		}
	}

	// Double-click to chat
	static bigtime_t lastClickTime = 0;
	static MapNode* lastClickedNode = NULL;
	bigtime_t now = system_time();

	if (clickedNode != NULL && clickedNode == lastClickedNode &&
		(now - lastClickTime) < 500000) {  // 500ms
		// Double-click - open chat
		BMessage msg(MSG_NODE_CHAT);
		msg.AddData("pubkey", B_RAW_TYPE, clickedNode->pubKeyPrefix, kPubKeyPrefixSize);
		Window()->PostMessage(&msg);
	}

	lastClickTime = now;
	lastClickedNode = clickedNode;

	Invalidate();
}


void
NetworkMapView::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
{
	// Update hover state for tooltips (could be enhanced)
}


void
NetworkMapView::MouseUp(BPoint where)
{
	fDragging = false;
}


void
NetworkMapView::FrameResized(float newWidth, float newHeight)
{
	Invalidate();
}


void
NetworkMapView::SetNodes(const BObjectList<ContactInfo, true>* contacts)
{
	// Clear selection first (it might point to a node we're about to delete)
	fSelectedNode = NULL;

	// Keep existing nodes to preserve animation state
	// RemoveItemAt returns the pointer without deleting it
	BObjectList<MapNode, false> oldNodes(20);
	while (fNodes.CountItems() > 0) {
		MapNode* node = fNodes.RemoveItemAt(0);
		if (node != NULL)
			oldNodes.AddItem(node);
	}

	if (contacts == NULL) {
		// Delete all old nodes
		for (int32 i = 0; i < oldNodes.CountItems(); i++)
			delete oldNodes.ItemAt(i);
		Invalidate();
		return;
	}

	uint32 now = (uint32)time(NULL);

	for (int32 i = 0; i < contacts->CountItems(); i++) {
		ContactInfo* contact = contacts->ItemAt(i);
		if (contact == NULL || !contact->isValid)
			continue;

		// Try to find existing node to preserve state
		MapNode* existingNode = NULL;
		for (int32 j = 0; j < oldNodes.CountItems(); j++) {
			MapNode* old = oldNodes.ItemAt(j);
			if (old && memcmp(old->pubKeyPrefix, contact->publicKey, kPubKeyPrefixSize) == 0) {
				existingNode = oldNodes.RemoveItemAt(j);
				break;
			}
		}

		MapNode* node;
		if (existingNode) {
			node = existingNode;
		} else {
			node = new MapNode();
			node->position = fCenter;  // Start at center
		}

		memcpy(node->pubKeyPrefix, contact->publicKey, kPubKeyPrefixSize);
		strlcpy(node->name, contact->name, sizeof(node->name));
		node->nodeType = contact->type;  // 1=CHAT, 2=REPEATER, 3=ROOM
		node->hops = (contact->outPathLen >= 0) ? contact->outPathLen : 1;
		node->lastSeen = contact->lastSeen;

		// Extract last known SNR from most recent incoming message
		int32 msgCount = contact->messages.CountItems();
		bool foundSNR = false;
		for (int32 m = msgCount - 1; m >= 0; m--) {
			ChatMessage* msg = contact->messages.ItemAt(m);
			if (msg != NULL && !msg->isOutgoing && msg->snr != 0) {
				node->snr = msg->snr;
				node->hasSNRData = true;
				foundSNR = true;
				break;
			}
		}
		if (!foundSNR && !existingNode) {
			node->rssi = 0;
			node->snr = 0;
			node->hasSNRData = false;
		}

		// Calculate activity level based on message count
		node->messageCount = contact->messages.CountItems();
		node->activityLevel = fminf(1.0f, node->messageCount / 20.0f);

		// Calculate online status
		uint32 age = (now > node->lastSeen) ? (now - node->lastSeen) : 0;
		if (age < kOnlineThreshold)
			node->status = STATUS_ONLINE;
		else if (age < kRecentThreshold)
			node->status = STATUS_RECENT;
		else if (age < kStaleThreshold)
			node->status = STATUS_AWAY;
		else
			node->status = STATUS_OFFLINE;

		fNodes.AddItem(node);
	}

	// Delete remaining old nodes (ones not reused)
	for (int32 i = 0; i < oldNodes.CountItems(); i++)
		delete oldNodes.ItemAt(i);

	Invalidate();
}


void
NetworkMapView::SetSelfInfo(const char* name, int8 rssi, int8 snr)
{
	if (name != NULL && name[0] != '\0') {
		strlcpy(fSelfNode.name, name, sizeof(fSelfNode.name));
	}
	fSelfNode.rssi = rssi;
	fSelfNode.snr = snr;
	Invalidate();
}


void
NetworkMapView::SetShowLabels(bool show)
{
	fShowLabels = show;
	Invalidate();
}


void
NetworkMapView::SetShowSignalStrength(bool show)
{
	fShowSignalStrength = show;
	Invalidate();
}


void
NetworkMapView::SetZoom(float zoom)
{
	fZoom = zoom;
	Invalidate();
}


void
NetworkMapView::TriggerNodePulse(const uint8* pubKeyPrefix)
{
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node && memcmp(node->pubKeyPrefix, pubKeyPrefix, kPubKeyPrefixSize) == 0) {
			node->pulsePhase = 1.0f;
			Invalidate();
			break;
		}
	}
}


void
NetworkMapView::UpdateNodeSNR(const uint8* pubKeyPrefix, int8 snr, int8 rssi)
{
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node && memcmp(node->pubKeyPrefix, pubKeyPrefix, kPubKeyPrefixSize) == 0) {
			node->snr = snr;
			node->rssi = rssi;
			node->hasSNRData = true;
			Invalidate();
			break;
		}
	}
}


void
NetworkMapView::SetTraceRoute(const TraceRoute& route)
{
	// Replace existing route for same destination, or add new
	for (int32 i = 0; i < fTraceRoutes.CountItems(); i++) {
		TraceRoute* existing = fTraceRoutes.ItemAt(i);
		if (existing && memcmp(existing->destKeyPrefix, route.destKeyPrefix,
				kPubKeyPrefixSize) == 0) {
			*existing = route;
			Invalidate();
			return;
		}
	}

	TraceRoute* newRoute = new TraceRoute(route);
	fTraceRoutes.AddItem(newRoute);
	Invalidate();
}


void
NetworkMapView::ClearTraceRoutes()
{
	fTraceRoutes.MakeEmpty();
	Invalidate();
}


void
NetworkMapView::_CalculatePositions()
{
	int32 count = fNodes.CountItems();
	if (count == 0)
		return;

	// Position nodes in a spiral/circle around center
	// Distance from center based on RSSI
	// Angle distributed based on activity and hash

	for (int32 i = 0; i < count; i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		// Use pubkey to create consistent angle
		uint32 hash = node->pubKeyPrefix[0] | (node->pubKeyPrefix[1] << 8);
		float baseAngle = (hash % 360) * M_PI / 180.0f;

		// Add offset based on index to prevent overlap
		float angle = baseAngle + i * 0.3f;

		float distance = _DistanceForRssi(node->rssi) * fZoom;

		node->targetPosition.x = fCenter.x + cosf(angle) * distance;
		node->targetPosition.y = fCenter.y + sinf(angle) * distance;

		// Initialize position if not set
		if (node->position.x == 0 && node->position.y == 0) {
			node->position = fCenter;
		}
	}
}


void
NetworkMapView::_DrawSelfNode()
{
	float radius = kSelfNodeRadius * fZoom;

	// Animated glow
	float glowSize = radius * (1.2f + 0.1f * sinf(fSelfNode.pulsePhase));
	SetHighColor(kSelfNodeGlow);
	FillEllipse(fCenter, glowSize, glowSize);

	// Main circle
	SetHighColor(kSelfNodeColor);
	FillEllipse(fCenter, radius, radius);

	// Outline
	SetHighColor(kNodeOutlineColor);
	SetPenSize(2.0f);
	StrokeEllipse(fCenter, radius, radius);

	// Draw self icon (star pattern)
	SetHighColor(255, 255, 255);
	SetPenSize(2.0f);
	float starSize = radius * 0.5f;
	for (int i = 0; i < 4; i++) {
		float a = i * M_PI / 4;
		StrokeLine(
			BPoint(fCenter.x + cosf(a) * starSize * 0.3f,
				   fCenter.y + sinf(a) * starSize * 0.3f),
			BPoint(fCenter.x + cosf(a) * starSize,
				   fCenter.y + sinf(a) * starSize));
	}

	// Label
	if (fShowLabels) {
		BFont font;
		GetFont(&font);
		font.SetSize(12);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);

		float labelWidth = StringWidth(fSelfNode.name);
		float labelX = fCenter.x - labelWidth / 2;
		float labelY = fCenter.y + radius + 18;

		// Background
		SetHighColor(kLabelBgColor);
		FillRoundRect(BRect(labelX - 6, labelY - 12, labelX + labelWidth + 6, labelY + 4), 4, 4);

		// Text
		SetHighColor(kSelfNodeColor);
		DrawString(fSelfNode.name, BPoint(labelX, labelY));
	}
}


void
NetworkMapView::_DrawNode(const MapNode& node)
{
	float radius = _RadiusForNode(node) * fZoom;
	rgb_color fillColor = _ColorForNode(node);
	bool isRepeater = (node.nodeType == NODE_REPEATER);

	// Apply opacity based on last seen
	float opacity = _OpacityForNode(node);

	// Pulse effect for recent activity
	if (node.pulsePhase > 0) {
		float pulseRadius = radius * (1.0f + node.pulsePhase * 0.5f);
		rgb_color pulseColor = kPulseColor;
		pulseColor.alpha = (uint8)(node.pulsePhase * 200);
		SetHighColor(pulseColor);
		if (isRepeater) {
			// Hexagon pulse for repeaters
			BPoint hexPulse[6];
			for (int i = 0; i < 6; i++) {
				float angle = i * M_PI / 3.0f - M_PI / 6.0f;
				hexPulse[i].Set(node.position.x + cosf(angle) * pulseRadius,
								node.position.y + sinf(angle) * pulseRadius);
			}
			StrokePolygon(hexPulse, 6, true);
		} else {
			StrokeEllipse(node.position, pulseRadius, pulseRadius);
		}
	}

	if (isRepeater) {
		// Draw hexagon for repeaters
		BPoint hexPoints[6];
		BPoint hexShadow[6];
		for (int i = 0; i < 6; i++) {
			float angle = i * M_PI / 3.0f - M_PI / 6.0f;
			hexPoints[i].Set(node.position.x + cosf(angle) * radius,
							 node.position.y + sinf(angle) * radius);
			hexShadow[i].Set(node.position.x + 2 + cosf(angle) * radius,
							 node.position.y + 2 + sinf(angle) * radius);
		}

		// Shadow
		SetHighColor(0, 0, 0, (uint8)(60 * opacity));
		FillPolygon(hexShadow, 6);

		// Main hexagon
		fillColor.alpha = (uint8)(255 * opacity);
		SetHighColor(fillColor);
		FillPolygon(hexPoints, 6);

		// Status indicator ring
		rgb_color statusColor = _StatusColor(node.status);
		statusColor.alpha = (uint8)(255 * opacity);
		SetHighColor(statusColor);
		SetPenSize(3.0f);
		BPoint hexOuter[6];
		for (int i = 0; i < 6; i++) {
			float angle = i * M_PI / 3.0f - M_PI / 6.0f;
			hexOuter[i].Set(node.position.x + cosf(angle) * (radius + 2),
							node.position.y + sinf(angle) * (radius + 2));
		}
		StrokePolygon(hexOuter, 6, true);

		// Selection outline
		if (node.isSelected) {
			SetHighColor(kSelectedColor);
			SetPenSize(3.0f);
			BPoint hexSel[6];
			for (int i = 0; i < 6; i++) {
				float angle = i * M_PI / 3.0f - M_PI / 6.0f;
				hexSel[i].Set(node.position.x + cosf(angle) * (radius + 5),
							  node.position.y + sinf(angle) * (radius + 5));
			}
			StrokePolygon(hexSel, 6, true);
		}

		// Inner outline
		rgb_color outline = kNodeOutlineColor;
		outline.alpha = (uint8)(200 * opacity);
		SetHighColor(outline);
		SetPenSize(1.5f);
		StrokePolygon(hexPoints, 6, true);

		// Draw "R" icon inside for repeater
		SetHighColor(255, 255, 255, (uint8)(220 * opacity));
		BFont font;
		GetFont(&font);
		font.SetSize(radius * 0.9f);
		font.SetFace(B_BOLD_FACE);
		SetFont(&font);
		float rWidth = StringWidth("R");
		DrawString("R", BPoint(node.position.x - rWidth / 2, node.position.y + radius * 0.3f));
	} else {
		// Draw circle for regular nodes
		// Shadow
		SetHighColor(0, 0, 0, (uint8)(60 * opacity));
		FillEllipse(BPoint(node.position.x + 2, node.position.y + 2), radius, radius);

		// Main circle
		fillColor.alpha = (uint8)(255 * opacity);
		SetHighColor(fillColor);
		FillEllipse(node.position, radius, radius);

		// Status indicator ring
		rgb_color statusColor = _StatusColor(node.status);
		statusColor.alpha = (uint8)(255 * opacity);
		SetHighColor(statusColor);
		SetPenSize(3.0f);
		StrokeEllipse(node.position, radius + 2, radius + 2);

		// Selection outline
		if (node.isSelected) {
			SetHighColor(kSelectedColor);
			SetPenSize(3.0f);
			StrokeEllipse(node.position, radius + 5, radius + 5);
		}

		// Inner outline
		rgb_color outline = kNodeOutlineColor;
		outline.alpha = (uint8)(200 * opacity);
		SetHighColor(outline);
		SetPenSize(1.5f);
		StrokeEllipse(node.position, radius, radius);
	}

	// Hop count badge (for multi-hop contacts)
	if (node.hops > 1) {
		float badgeRadius = 8 * fZoom;
		float badgeX = node.position.x + radius * 0.7f;
		float badgeY = node.position.y - radius * 0.7f;

		SetHighColor(60, 60, 60, (uint8)(220 * opacity));
		FillEllipse(BPoint(badgeX, badgeY), badgeRadius, badgeRadius);

		SetHighColor(255, 255, 255, (uint8)(255 * opacity));
		BFont font;
		GetFont(&font);
		font.SetSize(9 * fZoom);
		SetFont(&font);

		char hopStr[4];
		snprintf(hopStr, sizeof(hopStr), "%d", node.hops);
		float hopWidth = StringWidth(hopStr);
		DrawString(hopStr, BPoint(badgeX - hopWidth / 2, badgeY + 3));
	}

	// Message count badge
	if (node.messageCount > 0) {
		float badgeRadius = 8 * fZoom;
		float badgeX = node.position.x - radius * 0.7f;
		float badgeY = node.position.y - radius * 0.7f;

		SetHighColor(80, 140, 200, (uint8)(220 * opacity));
		FillEllipse(BPoint(badgeX, badgeY), badgeRadius, badgeRadius);

		SetHighColor(255, 255, 255, (uint8)(255 * opacity));
		BFont font;
		GetFont(&font);
		font.SetSize(8 * fZoom);
		SetFont(&font);

		char msgStr[8];
		if (node.messageCount > 99)
			strlcpy(msgStr, "99+", sizeof(msgStr));
		else
			snprintf(msgStr, sizeof(msgStr), "%d", (int)(node.messageCount & 0xFF));
		float msgWidth = StringWidth(msgStr);
		DrawString(msgStr, BPoint(badgeX - msgWidth / 2, badgeY + 3));
	}

	// Labels
	if (fShowLabels) {
		BFont font;
		GetFont(&font);
		font.SetSize(11);
		SetFont(&font);

		// Name
		const char* displayName = node.name[0] ? node.name : "Unknown";
		float labelWidth = StringWidth(displayName);
		float labelX = node.position.x - labelWidth / 2;
		float labelY = node.position.y + radius + 16;

		// Background
		SetHighColor(0, 0, 0, (uint8)(180 * opacity));
		FillRoundRect(BRect(labelX - 4, labelY - 11, labelX + labelWidth + 4, labelY + 3), 3, 3);

		// Text
		rgb_color textColor = kLabelColor;
		textColor.alpha = (uint8)(255 * opacity);
		SetHighColor(textColor);
		DrawString(displayName, BPoint(labelX, labelY));

		// Signal info (SNR-based if available)
		if (fShowSignalStrength) {
			font.SetSize(9);
			SetFont(&font);

			char signalStr[32];
			rgb_color sigColor;
			if (node.hasSNRData) {
				snprintf(signalStr, sizeof(signalStr), "SNR %d dB", node.snr);
				sigColor = _ColorForSNR(node.snr);
			} else {
				snprintf(signalStr, sizeof(signalStr), "no data");
				sigColor = kLinkUnknown;
			}

			float sigWidth = StringWidth(signalStr);
			float sigX = node.position.x - sigWidth / 2;
			float sigY = labelY + 13;

			sigColor.alpha = (uint8)(255 * opacity);
			SetHighColor(sigColor);
			DrawString(signalStr, BPoint(sigX, sigY));
		}
	}
}


void
NetworkMapView::_DrawConnection(BPoint from, BPoint to, const MapNode* node)
{
	if (node == NULL)
		return;

	float opacity = _OpacityForNode(*node);

	// Choose color based on SNR if available, else RSSI
	rgb_color lineColor;
	if (node->hasSNRData)
		lineColor = _ColorForSNR(node->snr);
	else
		lineColor = kLinkUnknown;

	lineColor.alpha = (uint8)(150 * opacity);

	// Line thickness based on SNR quality
	float thickness = node->hasSNRData
		? _ThicknessForSNR(node->snr) * fZoom
		: 1.5f * fZoom;

	SetHighColor(lineColor);
	SetPenSize(thickness);

	// Multi-hop: dashed line
	if (node->hops > 1) {
		float dx = to.x - from.x;
		float dy = to.y - from.y;
		float len = sqrtf(dx * dx + dy * dy);
		float dashLen = 12.0f;
		int dashes = (int)(len / dashLen);
		if (dashes < 1) dashes = 1;

		for (int i = 0; i < dashes; i += 2) {
			float t1 = (float)i / dashes;
			float t2 = (float)(i + 1) / dashes;
			StrokeLine(
				BPoint(from.x + dx * t1, from.y + dy * t1),
				BPoint(from.x + dx * t2, from.y + dy * t2));
		}
	} else {
		StrokeLine(from, to);
	}

	// Draw glow effect for selected node connections
	if (node->isSelected) {
		rgb_color glowColor = lineColor;
		glowColor.alpha = (uint8)(40 * opacity);
		SetHighColor(glowColor);
		SetPenSize(thickness + 4.0f * fZoom);
		StrokeLine(from, to);
	}

	SetPenSize(1.0f);

	// Draw animated data flow dots for online nodes
	if (node->status == STATUS_ONLINE || node->status == STATUS_RECENT) {
		rgb_color dotColor = lineColor;
		dotColor.alpha = (uint8)(200 * opacity);
		_DrawFlowDots(from, to, node->flowPhase, dotColor);
	}

	// Draw SNR label at midpoint if signal strength is shown
	if (fShowSignalStrength && node->hasSNRData) {
		BPoint mid((from.x + to.x) / 2, (from.y + to.y) / 2);
		_DrawLinkLabel(mid, node);
	}
}


void
NetworkMapView::_DrawLinkLabel(BPoint midPoint, const MapNode* node)
{
	if (node == NULL || !node->hasSNRData)
		return;

	float opacity = _OpacityForNode(*node);

	BFont font;
	GetFont(&font);
	font.SetSize(9);
	SetFont(&font);

	char label[32];
	snprintf(label, sizeof(label), "%d dB", node->snr);

	float labelWidth = StringWidth(label);
	float labelX = midPoint.x - labelWidth / 2;
	float labelY = midPoint.y + 3;

	// Background pill
	SetHighColor(0, 0, 0, (uint8)(200 * opacity));
	FillRoundRect(BRect(labelX - 4, labelY - 10, labelX + labelWidth + 4, labelY + 3),
		4, 4);

	// Border matching link color
	rgb_color borderColor = _ColorForSNR(node->snr);
	borderColor.alpha = (uint8)(150 * opacity);
	SetHighColor(borderColor);
	SetPenSize(1.0f);
	StrokeRoundRect(BRect(labelX - 4, labelY - 10, labelX + labelWidth + 4, labelY + 3),
		4, 4);

	// Text
	rgb_color textColor = _ColorForSNR(node->snr);
	textColor.alpha = (uint8)(255 * opacity);
	SetHighColor(textColor);
	DrawString(label, BPoint(labelX, labelY));
}


void
NetworkMapView::_DrawFlowDots(BPoint from, BPoint to, float phase, rgb_color color)
{
	float dx = to.x - from.x;
	float dy = to.y - from.y;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 30.0f)
		return;

	// Draw 3 small dots flowing along the connection
	int dotCount = 3;
	float dotSpacing = 1.0f / dotCount;

	for (int i = 0; i < dotCount; i++) {
		float t = fmodf(phase + i * dotSpacing, 1.0f);
		float dotX = from.x + dx * t;
		float dotY = from.y + dy * t;

		// Fade dots at endpoints
		float edgeFade = 1.0f;
		if (t < 0.1f)
			edgeFade = t / 0.1f;
		else if (t > 0.9f)
			edgeFade = (1.0f - t) / 0.1f;

		color.alpha = (uint8)(180 * edgeFade);
		SetHighColor(color);
		float dotRadius = 2.5f * fZoom;
		FillEllipse(BPoint(dotX, dotY), dotRadius, dotRadius);
	}
}


void
NetworkMapView::_DrawTraceRoutes()
{
	uint32 now = (uint32)time(NULL);

	for (int32 r = 0; r < fTraceRoutes.CountItems(); r++) {
		TraceRoute* route = fTraceRoutes.ItemAt(r);
		if (route == NULL || route->pathLen == 0)
			continue;

		// Find destination node
		MapNode* destNode = NULL;
		for (int32 i = 0; i < fNodes.CountItems(); i++) {
			MapNode* node = fNodes.ItemAt(i);
			if (node && memcmp(node->pubKeyPrefix, route->destKeyPrefix,
					kPubKeyPrefixSize) == 0) {
				destNode = node;
				break;
			}
		}
		if (destNode == NULL)
			continue;

		// Fade out old traces (older than 2 minutes)
		uint32 age = (now > route->timestamp) ? (now - route->timestamp) : 0;
		float traceFade = 1.0f;
		if (age > 90)
			traceFade = fmaxf(0.0f, 1.0f - (age - 90) / 30.0f);
		if (traceFade <= 0.0f)
			continue;

		// Draw the trace route as a highlighted path
		// For multi-hop, interpolate between self and dest with waypoints
		BPoint pathStart = fCenter;
		BPoint pathEnd = destNode->position;

		if (route->pathLen <= 1) {
			// Direct path — single highlighted line
			rgb_color traceColor = kTraceRouteColor;
			traceColor.alpha = (uint8)(180 * traceFade);
			SetHighColor(traceColor);
			SetPenSize(3.0f * fZoom);
			StrokeLine(pathStart, pathEnd);
		} else {
			// Multi-hop — draw waypoints along the path
			float dx = pathEnd.x - pathStart.x;
			float dy = pathEnd.y - pathStart.y;

			BPoint prev = pathStart;
			for (uint8 h = 0; h < route->pathLen; h++) {
				float t = (float)(h + 1) / (route->pathLen + 1);
				// Offset each hop slightly perpendicular for visibility
				float perpX = -dy * 0.08f;
				float perpY = dx * 0.08f;
				float hopOffset = (h % 2 == 0) ? 1.0f : -1.0f;

				BPoint hopPoint(
					pathStart.x + dx * t + perpX * hopOffset,
					pathStart.y + dy * t + perpY * hopOffset);

				// SNR color for this hop segment
				int8 hopSnr = route->hops[h].snr / 4;
				rgb_color segColor = _ColorForSNR(hopSnr);
				segColor.alpha = (uint8)(160 * traceFade);

				// Draw segment
				SetHighColor(segColor);
				SetPenSize(2.5f * fZoom);
				StrokeLine(prev, hopPoint);

				// Draw hop waypoint dot
				rgb_color dotColor = kTraceRouteColor;
				dotColor.alpha = (uint8)(220 * traceFade);
				SetHighColor(dotColor);
				float dotRadius = 4.0f * fZoom;
				FillEllipse(hopPoint, dotRadius, dotRadius);

				// Draw hop number
				BFont font;
				GetFont(&font);
				font.SetSize(8);
				SetFont(&font);
				SetHighColor(255, 255, 255, (uint8)(255 * traceFade));
				char hopNum[4];
				snprintf(hopNum, sizeof(hopNum), "%d", h + 1);
				float numWidth = StringWidth(hopNum);
				DrawString(hopNum, BPoint(hopPoint.x - numWidth / 2,
					hopPoint.y + 3));

				prev = hopPoint;
			}

			// Final segment to destination
			int8 destSnr = route->destSnr / 4;
			rgb_color destSegColor = _ColorForSNR(destSnr);
			destSegColor.alpha = (uint8)(160 * traceFade);
			SetHighColor(destSegColor);
			SetPenSize(2.5f * fZoom);
			StrokeLine(prev, pathEnd);
		}

		// Animate trace pulse
		route->animPhase += 0.01f;
		if (route->animPhase > 1.0f)
			route->animPhase -= 1.0f;
	}

	SetPenSize(1.0f);
}


void
NetworkMapView::_DrawInfoPanel()
{
	if (fSelectedNode == NULL)
		return;

	BRect bounds = Bounds();
	float panelWidth = 180;
	float panelHeight = 180;
	float panelX = bounds.right - panelWidth - 10;
	float panelY = 10;

	// Panel background
	SetHighColor(30, 34, 38, 230);
	FillRoundRect(BRect(panelX, panelY, panelX + panelWidth, panelY + panelHeight), 6, 6);

	// Border
	SetHighColor(60, 65, 70);
	SetPenSize(1.0f);
	StrokeRoundRect(BRect(panelX, panelY, panelX + panelWidth, panelY + panelHeight), 6, 6);

	BFont font;
	GetFont(&font);
	float y = panelY + 20;
	float x = panelX + 10;

	// Title
	font.SetSize(12);
	font.SetFace(B_BOLD_FACE);
	SetFont(&font);
	SetHighColor(kLabelColor);
	const char* name = fSelectedNode->name[0] ? fSelectedNode->name : "Unknown";
	DrawString(name, BPoint(x, y));
	y += 18;

	// Details
	font.SetSize(10);
	font.SetFace(B_REGULAR_FACE);
	SetFont(&font);

	// Node type
	const char* typeStr;
	rgb_color typeColor;
	switch (fSelectedNode->nodeType) {
		case NODE_COMPANION:
			typeStr = "Companion";
			typeColor = (rgb_color){100, 180, 255, 255};  // Light blue
			break;
		case NODE_REPEATER:
			typeStr = "Repeater";
			typeColor = (rgb_color){255, 180, 50, 255};  // Orange
			break;
		case NODE_ROOM:
			typeStr = "Room Server";
			typeColor = (rgb_color){100, 255, 100, 255};  // Green
			break;
		default:
			typeStr = "Unknown";
			typeColor = kLabelColor;
			break;
	}
	SetHighColor(typeColor);
	DrawString(typeStr, BPoint(x, y));
	y += 14;

	// Status
	SetHighColor(_StatusColor(fSelectedNode->status));
	const char* statusStr[] = {"Online", "Recent", "Away", "Offline"};
	DrawString(statusStr[fSelectedNode->status], BPoint(x, y));
	y += 14;

	// SNR / Signal
	char buf[64];
	if (fSelectedNode->hasSNRData) {
		SetHighColor(_ColorForSNR(fSelectedNode->snr));
		snprintf(buf, sizeof(buf), "SNR: %d dB", fSelectedNode->snr);
		DrawString(buf, BPoint(x, y));
		y += 14;

		if (fSelectedNode->rssi != 0) {
			SetHighColor(_ColorForSignal(fSelectedNode->rssi));
			snprintf(buf, sizeof(buf), "RSSI: %d dBm", fSelectedNode->rssi);
			DrawString(buf, BPoint(x, y));
			y += 14;
		}
	} else {
		SetHighColor(kLabelColor);
		DrawString("No signal data", BPoint(x, y));
		y += 14;
	}

	// Messages
	SetHighColor(kLabelColor);
	snprintf(buf, sizeof(buf), "Messages: %d", fSelectedNode->messageCount);
	DrawString(buf, BPoint(x, y));
	y += 14;

	// Last seen
	uint32 now = (uint32)time(NULL);
	uint32 age = (now > fSelectedNode->lastSeen) ? (now - fSelectedNode->lastSeen) : 0;
	if (age < 60)
		snprintf(buf, sizeof(buf), "Last seen: %ds ago", age);
	else if (age < 3600)
		snprintf(buf, sizeof(buf), "Last seen: %dm ago", age / 60);
	else if (age < 86400)
		snprintf(buf, sizeof(buf), "Last seen: %dh ago", age / 3600);
	else
		snprintf(buf, sizeof(buf), "Last seen: %dd ago", age / 86400);
	DrawString(buf, BPoint(x, y));
	y += 14;

	// Public key prefix
	SetHighColor(100, 100, 100);
	font.SetSize(9);
	SetFont(&font);
	char keyStr[20];
	snprintf(keyStr, sizeof(keyStr), "%02X%02X%02X%02X%02X%02X",
		fSelectedNode->pubKeyPrefix[0], fSelectedNode->pubKeyPrefix[1],
		fSelectedNode->pubKeyPrefix[2], fSelectedNode->pubKeyPrefix[3],
		fSelectedNode->pubKeyPrefix[4], fSelectedNode->pubKeyPrefix[5]);
	DrawString(keyStr, BPoint(x, y));
}


void
NetworkMapView::_DrawLinkQualityLegend()
{
	if (!fShowSignalStrength)
		return;

	BRect bounds = Bounds();
	float legendX = 10;
	float legendY = bounds.bottom - 100;

	// Background
	SetHighColor(30, 34, 38, 200);
	FillRoundRect(BRect(legendX - 5, legendY - 15, legendX + 110, bounds.bottom - 5), 4, 4);

	BFont font;
	GetFont(&font);
	font.SetSize(9);
	SetFont(&font);
	SetHighColor(kLabelColor);
	DrawString("Link Quality (SNR)", BPoint(legendX, legendY));

	legendY += 12;
	struct { rgb_color color; const char* label; float thickness; } legend[] = {
		{kLinkExcellent, "> 5 dB",     3.5f},
		{kLinkGood,      "0 to 5 dB",  3.0f},
		{kLinkFair,      "-5 to 0 dB", 2.5f},
		{kLinkPoor,      "-10 to -5",  2.0f},
		{kLinkBad,       "< -10 dB",   1.5f},
		{kLinkUnknown,   "No data",    1.0f}
	};

	for (int i = 0; i < 6; i++) {
		// Draw colored line sample
		SetHighColor(legend[i].color);
		SetPenSize(legend[i].thickness);
		float lineY = legendY + i * 12 + 4;
		StrokeLine(BPoint(legendX, lineY), BPoint(legendX + 16, lineY));

		// Label
		SetHighColor(kLabelColor);
		font.SetSize(9);
		SetFont(&font);
		DrawString(legend[i].label, BPoint(legendX + 22, legendY + i * 12 + 8));
	}

	SetPenSize(1.0f);
}


void
NetworkMapView::_DrawStats()
{
	BRect bounds = Bounds();
	(void)bounds;  // Unused for now

	// Draw node count and stats
	BFont font;
	GetFont(&font);
	font.SetSize(10);
	SetFont(&font);

	int32 total = fNodes.CountItems();
	int32 online = 0;
	int32 repeaters = 0;
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;
		if (node->status == STATUS_ONLINE)
			online++;
		if (node->nodeType == NODE_REPEATER)
			repeaters++;
	}

	char stats[128];
	if (repeaters > 0)
		snprintf(stats, sizeof(stats), "Nodes: %d total, %d online, %d repeaters",
			(int)total, (int)online, (int)repeaters);
	else
		snprintf(stats, sizeof(stats), "Nodes: %d total, %d online", (int)total, (int)online);

	SetHighColor(kLabelColor);
	DrawString(stats, BPoint(10, 20));
}


void
NetworkMapView::_ShowNodeContextMenu(BPoint where, MapNode* node)
{
	BPopUpMenu* menu = new BPopUpMenu("NodeMenu", false, false);

	BMessage* chatMsg = new BMessage(MSG_NODE_CHAT);
	chatMsg->AddData("pubkey", B_RAW_TYPE, node->pubKeyPrefix, kPubKeyPrefixSize);
	menu->AddItem(new BMenuItem("Open Chat", chatMsg));

	BMessage* traceMsg = new BMessage(MSG_NODE_TRACE);
	traceMsg->AddData("pubkey", B_RAW_TYPE, node->pubKeyPrefix, kPubKeyPrefixSize);
	menu->AddItem(new BMenuItem("Trace Path", traceMsg));

	menu->AddSeparatorItem();

	BMessage* infoMsg = new BMessage(MSG_NODE_INFO);
	infoMsg->AddData("pubkey", B_RAW_TYPE, node->pubKeyPrefix, kPubKeyPrefixSize);
	menu->AddItem(new BMenuItem("Node Info", infoMsg));

	menu->SetTargetForItems(Window());

	ConvertToScreen(&where);
	menu->Go(where, true, true, true);
}


float
NetworkMapView::_RadiusForNode(const MapNode& node) const
{
	// Size based on activity level
	return kNodeRadiusMin + (kNodeRadiusMax - kNodeRadiusMin) * node.activityLevel;
}


float
NetworkMapView::_OpacityForNode(const MapNode& node) const
{
	switch (node.status) {
		case STATUS_ONLINE: return 1.0f;
		case STATUS_RECENT: return 0.85f;
		case STATUS_AWAY: return 0.6f;
		case STATUS_OFFLINE: return 0.4f;
		default: return 1.0f;
	}
}


rgb_color
NetworkMapView::_ColorForNode(const MapNode& node) const
{
	if (node.status == STATUS_OFFLINE)
		return kOfflineColor;
	return _ColorForSignal(node.rssi);
}


rgb_color
NetworkMapView::_ColorForSignal(int8 rssi) const
{
	if (rssi >= -60)
		return kSignalExcellent;
	else if (rssi >= -70)
		return kSignalGood;
	else if (rssi >= -80)
		return kSignalFair;
	else if (rssi >= -90)
		return kSignalPoor;
	else
		return kSignalBad;
}


rgb_color
NetworkMapView::_ColorForSNR(int8 snr) const
{
	if (snr > 5)
		return kLinkExcellent;
	else if (snr >= 0)
		return kLinkGood;
	else if (snr >= -5)
		return kLinkFair;
	else if (snr >= -10)
		return kLinkPoor;
	else
		return kLinkBad;
}


LinkQuality
NetworkMapView::_QualityForSNR(int8 snr) const
{
	if (snr > 5)
		return LINK_EXCELLENT;
	else if (snr >= 0)
		return LINK_GOOD;
	else if (snr >= -5)
		return LINK_FAIR;
	else if (snr >= -10)
		return LINK_POOR;
	else
		return LINK_BAD;
}


float
NetworkMapView::_ThicknessForSNR(int8 snr) const
{
	// Map SNR to line thickness: excellent=3.5, bad=1.5
	if (snr > 5)
		return 3.5f;
	else if (snr >= 0)
		return 3.0f;
	else if (snr >= -5)
		return 2.5f;
	else if (snr >= -10)
		return 2.0f;
	else
		return 1.5f;
}


rgb_color
NetworkMapView::_StatusColor(NodeStatus status) const
{
	switch (status) {
		case STATUS_ONLINE: return (rgb_color){50, 205, 50, 255};   // Green
		case STATUS_RECENT: return (rgb_color){255, 193, 37, 255};  // Gold
		case STATUS_AWAY: return (rgb_color){255, 140, 0, 255};     // Orange
		case STATUS_OFFLINE: return (rgb_color){128, 128, 128, 255}; // Gray
		default: return (rgb_color){128, 128, 128, 255};
	}
}


float
NetworkMapView::_DistanceForRssi(int8 rssi) const
{
	// Map RSSI to distance
	if (rssi > -30) rssi = -30;
	if (rssi < -100) rssi = -100;

	float normalized = (float)(-rssi - 30) / 70.0f;
	return kMinDistance + normalized * (kMaxDistance - kMinDistance);
}


// ============================================================================
// NetworkMapWindow
// ============================================================================

NetworkMapWindow::NetworkMapWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 700, 550), "Network Map",
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fMapView(NULL),
	fInfoLabel(NULL),
	fShowLabelsCheck(NULL),
	fShowSignalCheck(NULL),
	fAutoTraceCheck(NULL),
	fZoomSlider(NULL),
	fRefreshButton(NULL),
	fCloseButton(NULL),
	fRefreshTimer(NULL),
	fAutoTraceTimer(NULL),
	fAutoTraceIndex(0)
{
	memset(fSelfName, 0, sizeof(fSelfName));

	// Create map view
	fMapView = new NetworkMapView();

	// Create controls
	fShowLabelsCheck = new BCheckBox("show_labels", "Names",
		new BMessage(MSG_TOGGLE_LABELS));
	fShowLabelsCheck->SetValue(B_CONTROL_ON);

	fShowSignalCheck = new BCheckBox("show_signal", "Signal",
		new BMessage(MSG_TOGGLE_SIGNAL));
	fShowSignalCheck->SetValue(B_CONTROL_ON);

	fAutoTraceCheck = new BCheckBox("auto_trace", "Auto-trace",
		new BMessage(MSG_TOGGLE_AUTO_TRACE));
	fAutoTraceCheck->SetValue(B_CONTROL_OFF);

	fZoomSlider = new BSlider("zoom", NULL,
		new BMessage(MSG_ZOOM_CHANGED), 50, 150, B_HORIZONTAL);
	fZoomSlider->SetValue(100);
	fZoomSlider->SetHashMarks(B_HASH_MARKS_NONE);
	fZoomSlider->SetBarThickness(8);

	fRefreshButton = new BButton("refresh", "Refresh",
		new BMessage(MSG_REFRESH_MAP));
	fCloseButton = new BButton("close", "Close",
		new BMessage(B_QUIT_REQUESTED));

	fInfoLabel = new BStringView("info", "Double-click node to chat • Right-click for menu");
	fInfoLabel->SetHighColor(150, 150, 150);

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMapView, 1.0)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_HALF_ITEM_SPACING)
			.Add(fShowLabelsCheck)
			.Add(fShowSignalCheck)
			.Add(fAutoTraceCheck)
			.AddStrut(10)
			.Add(new BStringView("zoom_label", "Zoom:"))
			.Add(fZoomSlider, 0.2)
			.AddGlue()
			.Add(fRefreshButton)
			.Add(fCloseButton)
		.End()
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_HALF_ITEM_SPACING, 0, B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
			.Add(fInfoLabel)
		.End()
	.End();

	// Set minimum size
	SetSizeLimits(500, B_SIZE_UNLIMITED, 400, B_SIZE_UNLIMITED);

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	// Start refresh timer
	BMessage timerMsg(MSG_MAP_TIMER);
	fRefreshTimer = new BMessageRunner(this, &timerMsg, kMapRefreshInterval);
}


NetworkMapWindow::~NetworkMapWindow()
{
	delete fRefreshTimer;
	delete fAutoTraceTimer;
}


bool
NetworkMapWindow::QuitRequested()
{
	Hide();
	return false;
}


void
NetworkMapWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_REFRESH_MAP:
			_RequestUpdate();
			break;

		case MSG_MAP_TIMER:
			if (!IsHidden())
				_RequestUpdate();
			break;

		case MSG_TOGGLE_LABELS:
			fMapView->SetShowLabels(fShowLabelsCheck->Value() == B_CONTROL_ON);
			break;

		case MSG_TOGGLE_SIGNAL:
			fMapView->SetShowSignalStrength(fShowSignalCheck->Value() == B_CONTROL_ON);
			break;

		case MSG_ZOOM_CHANGED:
			fMapView->SetZoom(fZoomSlider->Value() / 100.0f);
			break;

		case MSG_NODE_CHAT:
		case MSG_NODE_TRACE:
		case MSG_NODE_INFO:
			// Forward to parent window
			if (fParent != NULL) {
				BMessage forward(*message);
				if (message->what == MSG_NODE_CHAT)
					forward.what = MSG_CONTACT_SELECTED;
				else if (message->what == MSG_NODE_TRACE)
					forward.what = MSG_TRACE_PATH;
				fParent->PostMessage(&forward);
			}
			break;

		case MSG_TOGGLE_AUTO_TRACE:
			if (fAutoTraceCheck->Value() == B_CONTROL_ON) {
				// Start auto-trace timer
				fAutoTraceIndex = 0;
				delete fAutoTraceTimer;
				BMessage traceMsg(MSG_AUTO_TRACE);
				fAutoTraceTimer = new BMessageRunner(this, &traceMsg,
					kAutoTraceInterval);
				// Trigger first trace immediately
				_RequestAutoTrace();
			} else {
				// Stop auto-trace
				delete fAutoTraceTimer;
				fAutoTraceTimer = NULL;
				fMapView->ClearTraceRoutes();
			}
			break;

		case MSG_AUTO_TRACE:
			if (!IsHidden() && fAutoTraceCheck->Value() == B_CONTROL_ON)
				_RequestAutoTrace();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
NetworkMapWindow::UpdateFromContacts(const BObjectList<ContactInfo, true>* contacts)
{
	fMapView->SetNodes(contacts);
}


void
NetworkMapWindow::SetSelfInfo(const char* name)
{
	if (name != NULL) {
		strlcpy(fSelfName, name, sizeof(fSelfName));
		fMapView->SetSelfInfo(fSelfName, 0, 0);
	}
}


void
NetworkMapWindow::TriggerNodePulse(const uint8* pubKeyPrefix)
{
	fMapView->TriggerNodePulse(pubKeyPrefix);
}


void
NetworkMapWindow::UpdateLinkQuality(const uint8* pubKeyPrefix, int8 snr, int8 rssi)
{
	fMapView->UpdateNodeSNR(pubKeyPrefix, snr, rssi);
}


void
NetworkMapWindow::HandleTraceData(const uint8* data, size_t length)
{
	// PUSH_TRACE_DATA format:
	// [0]     = code (0x89)
	// [1]     = reserved
	// [2]     = path_len
	// [3]     = flags
	// [4-7]   = tag (int32) — contains dest pubkey prefix bytes
	// [8-11]  = auth_code (int32)
	// [12..12+pathLen-1]      = path_hashes
	// [12+pathLen..12+2*pathLen] = path_snrs (pathLen+1 values)

	if (length < 12)
		return;

	uint8 pathLen = data[2];
	if (pathLen > 16) pathLen = 16;

	TraceRoute route;
	route.pathLen = pathLen;
	route.timestamp = (uint32)time(NULL);

	// Extract tag as partial dest identifier
	if (length >= 8)
		memcpy(route.destKeyPrefix, data + 4, 4 < kPubKeyPrefixSize ? 4 : kPubKeyPrefixSize);

	size_t hashOffset = 12;
	size_t snrOffset = 12 + pathLen;

	for (uint8 i = 0; i < pathLen && i < 16; i++) {
		if (hashOffset + i < length)
			route.hops[i].hash = data[hashOffset + i];
		if (snrOffset + i < length)
			route.hops[i].snr = (int8)data[snrOffset + i];
	}

	// Destination SNR
	if (snrOffset + pathLen < length)
		route.destSnr = (int8)data[snrOffset + pathLen];

	fMapView->SetTraceRoute(route);
}


void
NetworkMapWindow::_RequestAutoTrace()
{
	// Request trace path for each online contact, one at a time (round-robin)
	MapNode* selectedNode = fMapView->GetSelectedNode();
	if (selectedNode != NULL && selectedNode->status <= STATUS_RECENT) {
		// If a node is selected, trace that one
		BMessage msg(MSG_TRACE_PATH);
		msg.AddData("pubkey", B_RAW_TYPE, selectedNode->pubKeyPrefix, kPubKeyPrefixSize);
		if (fParent != NULL)
			fParent->PostMessage(&msg);
		return;
	}

	// Otherwise, no automatic trace without selection (to avoid radio spam)
}


void
NetworkMapWindow::_RequestUpdate()
{
	// Just request the parent to update the map with current data
	// Don't trigger a full contact sync which would clear message history
	if (fParent != NULL) {
		BMessage msg(MSG_SHOW_NETWORK_MAP);
		fParent->PostMessage(&msg);
	}
}
