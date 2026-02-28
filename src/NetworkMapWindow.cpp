/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * NetworkMapWindow.cpp — Dynamic network topology visualization
 */

#include "NetworkMapWindow.h"

#include "Constants.h"
#include "DatabaseManager.h"
#include "RepeaterMonitorView.h"

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


static void
_PrefixToHex(const uint8* prefix, char* outHex, size_t prefixLen)
{
	for (size_t i = 0; i < prefixLen; i++)
		snprintf(outHex + i * 2, 3, "%02x", prefix[i]);
}


static void
_HexToPrefix(const char* hex, uint8* outPrefix, size_t prefixLen)
{
	for (size_t i = 0; i < prefixLen; i++) {
		unsigned int val = 0;
		sscanf(hex + i * 2, "%02x", &val);
		outPrefix[i] = (uint8)val;
	}
}


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
static const uint32 MSG_DISCOVER_TOPOLOGY = 'dtop';
static const uint32 MSG_DISCOVERY_TICK = 'dctk';

static const bigtime_t kMapRefreshInterval = 3000000;   // 3 seconds
static const bigtime_t kAutoTraceInterval = 30000000;   // 30 seconds
static const bigtime_t kAnimationInterval = 50000;      // 50ms = 20fps
static const bigtime_t kDiscoveryTickInterval = 10000000; // 10 seconds between trace requests (radio has ~3 slot limit)
static const uint32 kEdgeExpireSeconds = 600;            // 10 minutes edge lifetime
static const uint32 kAutoTraceSkipSeconds = 300;         // 5 min: skip recently traced nodes

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
static const rgb_color kEdgeDiscoveredColor = {100, 180, 255, 200}; // Blue for discovered edges
static const rgb_color kHubGlowColor = {255, 180, 50, 40};         // Amber glow for repeater hubs

// Node sizes
static const float kSelfNodeRadius = 24.0f;
static const float kNodeRadiusMin = 10.0f;
static const float kNodeRadiusMax = 18.0f;
static const float kMinDistance = 80.0f;
static const float kMaxDistance = 280.0f;

// Topology layout rings
static const float kRing1Distance = 120.0f;  // Direct contacts
static const float kRing2Distance = 220.0f;  // 2-hop contacts
static const float kRing3Distance = 300.0f;  // 3+ hop contacts
static const float kClusterSpread = 40.0f;   // Spread of nodes around relay

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
	fHideInactive(false),
	fSelectedNode(NULL),
	fDragStart(0, 0),
	fDragging(false),
	fEdges(20)
{
	SetViewColor(kBackgroundColor);
	memset(fSelfNode.name, 0, sizeof(fSelfNode.name));
	strlcpy(fSelfNode.name, "Me", sizeof(fSelfNode.name));
	fSelfNode.pulsePhase = 0;
	fSelfNode.activityLevel = 1.0f;
	fPacketFlowCount = 0;
	memset(fSelfHexId, 0, sizeof(fSelfHexId));
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

	// Animate packet flows
	for (int32 i = 0; i < fPacketFlowCount; i++) {
		PacketFlowAnim& flow = fPacketFlows[i];
		if (!flow.active)
			continue;
		if (flow.phase < 1.0f) {
			flow.phase += 0.04f;  // ~1.25s traversal at 50ms/tick
		} else {
			// Arrived — fade out
			flow.alpha -= 0.08f;
			if (flow.alpha <= 0.0f) {
				flow.active = false;
				continue;
			}
		}
		needsRedraw = true;
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

	// Expire old edges
	uint32 now = (uint32)time(NULL);
	for (int32 i = fEdges.CountItems() - 1; i >= 0; i--) {
		TopologyEdge* edge = fEdges.ItemAt(i);
		if (edge && now - edge->timestamp > kEdgeExpireSeconds)
			fEdges.RemoveItemAt(i);
	}

	// Draw topology edges (discovered inter-node connections)
	_DrawTopologyEdges();

	// Draw self→node connections for direct or undiscovered nodes
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		// Skip inactive nodes when filter is active
		if (_IsNodeHidden(*node))
			continue;

		// Check if this node has discovered edges (to or from it)
		bool hasEdge = false;
		for (int32 e = 0; e < fEdges.CountItems(); e++) {
			TopologyEdge* edge = fEdges.ItemAt(e);
			if (edge &&
				(memcmp(edge->toPrefix, node->pubKeyPrefix, kPubKeyPrefixSize) == 0
				|| memcmp(edge->fromPrefix, node->pubKeyPrefix, kPubKeyPrefixSize) == 0)) {
				hasEdge = true;
				break;
			}
		}

		if (!hasEdge) {
			if (node->hops <= 1) {
				// Direct contact — draw solid connection to center
				_DrawConnection(fCenter, node->position, node);
			} else {
				// Multi-hop but path unknown — draw faint "?" indicator
				_DrawUnknownPath(fCenter, node->position, node);
			}
		}
	}

	// Draw trace route overlays (between connections and nodes)
	_DrawTraceRoutes();

	// Draw packet flow animations
	_DrawPacketFlows();

	// Draw self node at center with glow
	_DrawSelfNode();

	// Draw all other nodes
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node != NULL && !_IsNodeHidden(*node)) {
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
NetworkMapView::SetNodes(const OwningObjectList<ContactInfo>* contacts)
{
	// Clear selection first (it might point to a node we're about to delete)
	fSelectedNode = NULL;

	// Keep existing nodes to preserve animation state
	// RemoveItemAt returns the pointer without deleting it
	BObjectList<MapNode> oldNodes(20);
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

	// Build topology edges from outPath data in contact frames
	BuildEdgesFromOutPaths(contacts);

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
NetworkMapView::SetRepeaterTopology(const char* selfName,
	const RepeaterNodeInfo* nodes, int32 nodeCount,
	const RepeaterLink* links, int32 linkCount)
{
	// Save selection to restore after rebuild
	uint8 savedSelPrefix[kPubKeyPrefixSize];
	bool hadSelection = (fSelectedNode != NULL);
	if (hadSelection)
		memcpy(savedSelPrefix, fSelectedNode->pubKeyPrefix,
			kPubKeyPrefixSize);
	fSelectedNode = NULL;

	// Keep existing nodes to preserve animation state
	BObjectList<MapNode> oldNodes(20);
	while (fNodes.CountItems() > 0) {
		MapNode* node = fNodes.RemoveItemAt(0);
		if (node != NULL)
			oldNodes.AddItem(node);
	}

	// Clear existing edges
	fEdges.MakeEmpty();

	// Update self node name (fallback)
	if (selfName != NULL && selfName[0] != '\0')
		strlcpy(fSelfNode.name, selfName, sizeof(fSelfNode.name));

	// Track which hex ID is the self/repeater node
	char selfHexId[8] = {};

	// Create MapNodes from RepeaterNodeInfo
	for (int32 i = 0; i < nodeCount; i++) {
		const RepeaterNodeInfo& info = nodes[i];
		if (info.name[0] == '\0')
			continue;

		// If this node is detected as self (the repeater), merge into
		// fSelfNode at center instead of creating a duplicate
		if (info.isSelf) {
			strlcpy(selfHexId, info.name, sizeof(selfHexId));
			strlcpy(fSelfHexId, info.name, sizeof(fSelfHexId));
			// Set synthetic prefix for self node (for packet flow matching)
			unsigned int selfHexVal = 0;
			sscanf(info.name, "%x", &selfHexVal);
			memset(fSelfNode.pubKeyPrefix, 0,
				sizeof(fSelfNode.pubKeyPrefix));
			fSelfNode.pubKeyPrefix[0] = (uint8)(selfHexVal & 0xFF);
			// Use resolved full name if available
			if (info.fullName[0] != '\0')
				strlcpy(fSelfNode.name, info.fullName,
					sizeof(fSelfNode.name));
			fSelfNode.nodeType = NODE_REPEATER;
			fSelfNode.messageCount = info.packetCount;
			fSelfNode.activityLevel = fminf(1.0f,
				info.packetCount / 20.0f);
			fSelfNode.status = STATUS_ONLINE;
			fSelfNode.lastSeen = (uint32)time(NULL);
			continue;
		}

		// Build synthetic pubkey prefix from hex ID
		uint8 syntheticPrefix[kPubKeyPrefixSize];
		memset(syntheticPrefix, 0, sizeof(syntheticPrefix));
		// Parse hex ID (e.g. "FD") into first byte
		unsigned int hexVal = 0;
		sscanf(info.name, "%x", &hexVal);
		syntheticPrefix[0] = (uint8)(hexVal & 0xFF);
		// Use index as second byte to avoid collisions between
		// single-byte hex IDs
		syntheticPrefix[1] = (uint8)(i + 1);

		// Try to reuse existing node for animation continuity
		MapNode* existingNode = NULL;
		for (int32 j = 0; j < oldNodes.CountItems(); j++) {
			MapNode* old = oldNodes.ItemAt(j);
			if (old && memcmp(old->pubKeyPrefix, syntheticPrefix,
					kPubKeyPrefixSize) == 0) {
				existingNode = oldNodes.RemoveItemAt(j);
				break;
			}
		}

		MapNode* node;
		if (existingNode != NULL) {
			node = existingNode;
		} else {
			node = new MapNode();
			node->position = fCenter;
		}

		memcpy(node->pubKeyPrefix, syntheticPrefix, kPubKeyPrefixSize);
		// Use full resolved name if available, otherwise hex ID
		if (info.fullName[0] != '\0')
			strlcpy(node->name, info.fullName, sizeof(node->name));
		else
			strlcpy(node->name, info.name, sizeof(node->name));
		node->nodeType = NODE_UNKNOWN;
		node->hops = info.isDirect ? 1 : (info.isForwarded ? 2 : 1);
		node->messageCount = info.packetCount;
		node->activityLevel = fminf(1.0f, info.packetCount / 20.0f);

		// Nodes from repeater log (packetCount > 0) are online;
		// companion-only contacts (packetCount == 0) are shown dimmer
		if (info.packetCount > 0)
			node->status = STATUS_ONLINE;
		else
			node->status = STATUS_RECENT;

		if (info.avgSnr != 0) {
			node->snr = info.avgSnr;
			node->hasSNRData = true;
		}
		if (info.avgRssi != 0)
			node->rssi = (int8)(info.avgRssi > 127 ? 127
				: (info.avgRssi < -128 ? -128 : info.avgRssi));

		node->lastSeen = (uint32)time(NULL);

		fNodes.AddItem(node);
	}

	// Delete remaining old nodes
	for (int32 i = 0; i < oldNodes.CountItems(); i++)
		delete oldNodes.ItemAt(i);

	// Build TopologyEdges from RepeaterLink data
	for (int32 i = 0; i < linkCount; i++) {
		const RepeaterLink& link = links[i];
		if (link.src[0] == '\0' || link.dst[0] == '\0')
			continue;

		// Find source and dest nodes by hex ID
		// Self node is at center (fSelfNode), others in fNodes
		MapNode* srcNode = NULL;
		MapNode* dstNode = NULL;

		// Check if src or dst is the self/repeater node
		if (selfHexId[0] != '\0') {
			if (strcmp(link.src, selfHexId) == 0)
				srcNode = &fSelfNode;
			if (strcmp(link.dst, selfHexId) == 0)
				dstNode = &fSelfNode;
		}

		// Search remaining nodes by hex ID via input array
		for (int32 n = 0; n < nodeCount; n++) {
			if (nodes[n].isSelf)
				continue;  // Already handled above
			if (srcNode == NULL
				&& strcmp(nodes[n].name, link.src) == 0) {
				// Find corresponding MapNode in fNodes
				for (int32 m = 0; m < fNodes.CountItems(); m++) {
					MapNode* mn = fNodes.ItemAt(m);
					if (mn == NULL)
						continue;
					uint8 prefix[kPubKeyPrefixSize] = {};
					unsigned int hv = 0;
					sscanf(nodes[n].name, "%x", &hv);
					prefix[0] = (uint8)(hv & 0xFF);
					prefix[1] = (uint8)(n + 1);
					if (memcmp(mn->pubKeyPrefix, prefix,
							kPubKeyPrefixSize) == 0) {
						srcNode = mn;
						break;
					}
				}
			}
			if (dstNode == NULL
				&& strcmp(nodes[n].name, link.dst) == 0) {
				for (int32 m = 0; m < fNodes.CountItems(); m++) {
					MapNode* mn = fNodes.ItemAt(m);
					if (mn == NULL)
						continue;
					uint8 prefix[kPubKeyPrefixSize] = {};
					unsigned int hv = 0;
					sscanf(nodes[n].name, "%x", &hv);
					prefix[0] = (uint8)(hv & 0xFF);
					prefix[1] = (uint8)(n + 1);
					if (memcmp(mn->pubKeyPrefix, prefix,
							kPubKeyPrefixSize) == 0) {
						dstNode = mn;
						break;
					}
				}
			}
		}

		if (srcNode == NULL || dstNode == NULL)
			continue;

		TopologyEdge* edge = new TopologyEdge();
		memcpy(edge->fromPrefix, srcNode->pubKeyPrefix, kPubKeyPrefixSize);
		memcpy(edge->toPrefix, dstNode->pubKeyPrefix, kPubKeyPrefixSize);
		edge->snr = link.snr;
		edge->timestamp = (uint32)time(NULL);
		edge->ambiguous = false;
		fEdges.AddItem(edge);
	}

	// Restore selection if the node still exists
	if (hadSelection) {
		MapNode* restored = _FindNodeByPrefix(savedSelPrefix);
		if (restored != NULL) {
			fSelectedNode = restored;
			restored->isSelected = true;
		}
	}

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
NetworkMapView::SetHideInactive(bool hide)
{
	fHideInactive = hide;
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
NetworkMapView::TriggerPacketFlow(const char* srcHex, const char* dstHex,
	int8 snr, bool isMessage)
{
	// Find a free slot or reuse the oldest
	int32 slot = -1;
	for (int32 i = 0; i < fPacketFlowCount; i++) {
		if (!fPacketFlows[i].active) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		if (fPacketFlowCount < kMaxPacketFlows) {
			slot = fPacketFlowCount++;
		} else {
			// Reuse oldest (lowest alpha or highest phase)
			slot = 0;
			float lowestAlpha = fPacketFlows[0].alpha;
			for (int32 i = 1; i < kMaxPacketFlows; i++) {
				if (fPacketFlows[i].alpha < lowestAlpha) {
					lowestAlpha = fPacketFlows[i].alpha;
					slot = i;
				}
			}
		}
	}

	PacketFlowAnim& flow = fPacketFlows[slot];
	flow.active = true;
	flow.phase = 0.0f;
	flow.alpha = 1.0f;

	// Color by packet type
	if (isMessage)
		flow.color = (rgb_color){60, 130, 240, 255};	// Blue for messages
	else
		flow.color = (rgb_color){80, 200, 80, 255};	// Green for advert/other

	// Map hex IDs to pubKeyPrefix
	// Check self node first
	memset(flow.fromPrefix, 0, sizeof(flow.fromPrefix));
	memset(flow.toPrefix, 0, sizeof(flow.toPrefix));

	bool foundSrc = false;
	bool foundDst = false;

	// Self node match
	if (fSelfHexId[0] != '\0') {
		if (strcmp(srcHex, fSelfHexId) == 0) {
			memcpy(flow.fromPrefix, fSelfNode.pubKeyPrefix,
				kPubKeyPrefixSize);
			foundSrc = true;
		}
		if (strcmp(dstHex, fSelfHexId) == 0) {
			memcpy(flow.toPrefix, fSelfNode.pubKeyPrefix,
				kPubKeyPrefixSize);
			foundDst = true;
		}
	}

	// Parse hex IDs to byte values for prefix matching
	unsigned int srcHexVal = 0;
	unsigned int dstHexVal = 0;
	sscanf(srcHex, "%x", &srcHexVal);
	sscanf(dstHex, "%x", &dstHexVal);

	// Search other nodes by name or synthetic prefix[0]
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		// Match by: direct name, or synthetic prefix[0] (hex byte value)
		bool srcMatch = (strcmp(srcHex, node->name) == 0)
			|| (node->pubKeyPrefix[0] == (uint8)(srcHexVal & 0xFF)
				&& srcHexVal != 0);
		bool dstMatch = (strcmp(dstHex, node->name) == 0)
			|| (node->pubKeyPrefix[0] == (uint8)(dstHexVal & 0xFF)
				&& dstHexVal != 0);

		if (!foundSrc && srcMatch) {
			memcpy(flow.fromPrefix, node->pubKeyPrefix,
				kPubKeyPrefixSize);
			foundSrc = true;
		}
		if (!foundDst && dstMatch) {
			memcpy(flow.toPrefix, node->pubKeyPrefix,
				kPubKeyPrefixSize);
			foundDst = true;
		}
		if (foundSrc && foundDst)
			break;
	}

	if (!foundSrc || !foundDst) {
		flow.active = false;
		return;
	}

	Invalidate();
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
NetworkMapView::BuildEdgesFromTrace(const TraceRoute& route)
{
	// Build topology edges from trace data by matching 4-byte hop prefixes
	// to known contacts. Creates edges: Self→Hop1, Hop1→Hop2, ..., HopN→Dest
	if (route.numHops == 0)
		return;

	uint32 now = (uint32)time(NULL);

	// Determine match length: 4 bytes for multi-hop, 1 for legacy single-byte
	size_t matchLen = (route.numHops == 1 &&
		route.hops[0].hopPrefix[1] == 0 &&
		route.hops[0].hopPrefix[2] == 0 &&
		route.hops[0].hopPrefix[3] == 0) ? 1 : 4;

	// Build chain: self → hop1 → hop2 → ... → dest
	uint8 selfPrefix[kPubKeyPrefixSize];
	memset(selfPrefix, 0, sizeof(selfPrefix));

	const uint8* prevPrefix = selfPrefix;
	bool prevIsSelf = true;

	for (uint8 h = 0; h < route.numHops; h++) {
		MapNode* hopNode = _MatchHopToContact(
			route.hops[h].hopPrefix, matchLen);
		if (hopNode == NULL)
			continue;  // Unknown hop — skip

		// Create edge from previous node to this hop
		TopologyEdge* edge = new TopologyEdge();
		if (prevIsSelf)
			memset(edge->fromPrefix, 0, sizeof(edge->fromPrefix));
		else
			memcpy(edge->fromPrefix, prevPrefix, kPubKeyPrefixSize);
		memcpy(edge->toPrefix, hopNode->pubKeyPrefix, kPubKeyPrefixSize);
		edge->snr = route.hops[h].snr;
		edge->timestamp = now;
		edge->ambiguous = (matchLen == 1);  // 1-byte match is inherently ambiguous

		// Replace existing edge for same from→to pair, or add new
		bool replaced = false;
		for (int32 e = 0; e < fEdges.CountItems(); e++) {
			TopologyEdge* existing = fEdges.ItemAt(e);
			if (existing &&
				memcmp(existing->fromPrefix, edge->fromPrefix, kPubKeyPrefixSize) == 0 &&
				memcmp(existing->toPrefix, edge->toPrefix, kPubKeyPrefixSize) == 0) {
				*existing = *edge;
				delete edge;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			fEdges.AddItem(edge);

		prevPrefix = hopNode->pubKeyPrefix;
		prevIsSelf = false;
	}

	// Final edge: last hop → destination
	MapNode* destNode = _FindNodeByPrefix(route.destKeyPrefix);
	if (destNode != NULL && !prevIsSelf) {
		TopologyEdge* finalEdge = new TopologyEdge();
		memcpy(finalEdge->fromPrefix, prevPrefix, kPubKeyPrefixSize);
		memcpy(finalEdge->toPrefix, destNode->pubKeyPrefix, kPubKeyPrefixSize);
		finalEdge->snr = route.destSnr;
		finalEdge->timestamp = now;

		bool replaced = false;
		for (int32 e = 0; e < fEdges.CountItems(); e++) {
			TopologyEdge* existing = fEdges.ItemAt(e);
			if (existing &&
				memcmp(existing->fromPrefix, finalEdge->fromPrefix, kPubKeyPrefixSize) == 0 &&
				memcmp(existing->toPrefix, finalEdge->toPrefix, kPubKeyPrefixSize) == 0) {
				*existing = *finalEdge;
				delete finalEdge;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			fEdges.AddItem(finalEdge);
	}

	// Persist all non-ambiguous edges to database
	DatabaseManager* db = DatabaseManager::Instance();
	if (db != NULL && db->IsOpen()) {
		for (int32 e = 0; e < fEdges.CountItems(); e++) {
			TopologyEdge* edge = fEdges.ItemAt(e);
			if (edge == NULL || edge->ambiguous)
				continue;
			if (edge->timestamp != now)
				continue;  // Only save newly created/updated edges

			char fromHex[kPubKeyPrefixSize * 2 + 1];
			char toHex[kPubKeyPrefixSize * 2 + 1];
			memset(fromHex, 0, sizeof(fromHex));
			memset(toHex, 0, sizeof(toHex));
			_PrefixToHex(edge->fromPrefix, fromHex, kPubKeyPrefixSize);
			_PrefixToHex(edge->toPrefix, toHex, kPubKeyPrefixSize);
			db->InsertTopologyEdge(fromHex, toHex, edge->snr);
		}
	}

	Invalidate();
}


void
NetworkMapView::BuildEdgesFromOutPaths(
	const OwningObjectList<ContactInfo>* contacts)
{
	// Build topology edges from outPath data in contact frames.
	// outPath contains 1-byte hashes (publicKey[0]) in source→dest order.
	// Build edge chains: Self → outPath[0] → outPath[1] → ... → Contact
	if (contacts == NULL)
		return;

	uint32 now = (uint32)time(NULL);
	uint8 selfPrefix[kPubKeyPrefixSize];
	memset(selfPrefix, 0, sizeof(selfPrefix));

	for (int32 i = 0; i < contacts->CountItems(); i++) {
		ContactInfo* contact = contacts->ItemAt(i);
		if (contact == NULL || !contact->isValid)
			continue;
		if (contact->outPathLen <= 0)
			continue;

		int32 pathLen = contact->outPathLen;
		if (pathLen > 16)
			pathLen = 16;

		// Build edge chain: self → hop1 → hop2 → ... → contact
		// outPath is already in source→dest order
		const uint8* prevPrefix = selfPrefix;
		bool prevIsSelf = true;

		for (int32 h = 0; h < pathLen; h++) {
			// Match 1-byte hash to a known node
			MapNode* hopNode = _MatchHopToContact(&contact->outPath[h], 1);
			if (hopNode == NULL)
				continue;  // Unknown hop — skip

			// Don't create self→self edge if hop resolves to the contact itself
			if (memcmp(hopNode->pubKeyPrefix, contact->publicKey,
					kPubKeyPrefixSize) == 0)
				continue;

			TopologyEdge* edge = new TopologyEdge();
			if (prevIsSelf)
				memset(edge->fromPrefix, 0, sizeof(edge->fromPrefix));
			else
				memcpy(edge->fromPrefix, prevPrefix, kPubKeyPrefixSize);
			memcpy(edge->toPrefix, hopNode->pubKeyPrefix, kPubKeyPrefixSize);
			edge->snr = 0;  // outPath doesn't carry SNR data
			edge->timestamp = now;
			edge->ambiguous = true;  // 1-byte match is inherently ambiguous

			// Replace existing edge for same from→to pair, or add new
			bool replaced = false;
			for (int32 e = 0; e < fEdges.CountItems(); e++) {
				TopologyEdge* existing = fEdges.ItemAt(e);
				if (existing &&
					memcmp(existing->fromPrefix, edge->fromPrefix,
						kPubKeyPrefixSize) == 0 &&
					memcmp(existing->toPrefix, edge->toPrefix,
						kPubKeyPrefixSize) == 0) {
					// Only replace if existing is also ambiguous (don't
					// overwrite higher-quality trace data edges)
					if (existing->ambiguous) {
						*existing = *edge;
					}
					delete edge;
					replaced = true;
					break;
				}
			}
			if (!replaced)
				fEdges.AddItem(edge);

			prevPrefix = hopNode->pubKeyPrefix;
			prevIsSelf = false;
		}

		// Final edge: last hop → contact
		if (!prevIsSelf) {
			TopologyEdge* finalEdge = new TopologyEdge();
			memcpy(finalEdge->fromPrefix, prevPrefix, kPubKeyPrefixSize);
			memcpy(finalEdge->toPrefix, contact->publicKey, kPubKeyPrefixSize);
			finalEdge->snr = 0;
			finalEdge->timestamp = now;
			finalEdge->ambiguous = true;

			bool replaced = false;
			for (int32 e = 0; e < fEdges.CountItems(); e++) {
				TopologyEdge* existing = fEdges.ItemAt(e);
				if (existing &&
					memcmp(existing->fromPrefix, finalEdge->fromPrefix,
						kPubKeyPrefixSize) == 0 &&
					memcmp(existing->toPrefix, finalEdge->toPrefix,
						kPubKeyPrefixSize) == 0) {
					if (existing->ambiguous) {
						*existing = *finalEdge;
					}
					delete finalEdge;
					replaced = true;
					break;
				}
			}
			if (!replaced)
				fEdges.AddItem(finalEdge);
		} else if (pathLen > 0) {
			// All hops were unresolvable, but contact has a path — create
			// direct self→contact edge as fallback
			TopologyEdge* directEdge = new TopologyEdge();
			memset(directEdge->fromPrefix, 0, sizeof(directEdge->fromPrefix));
			memcpy(directEdge->toPrefix, contact->publicKey, kPubKeyPrefixSize);
			directEdge->snr = 0;
			directEdge->timestamp = now;
			directEdge->ambiguous = true;

			bool replaced = false;
			for (int32 e = 0; e < fEdges.CountItems(); e++) {
				TopologyEdge* existing = fEdges.ItemAt(e);
				if (existing &&
					memcmp(existing->fromPrefix, directEdge->fromPrefix,
						kPubKeyPrefixSize) == 0 &&
					memcmp(existing->toPrefix, directEdge->toPrefix,
						kPubKeyPrefixSize) == 0) {
					if (existing->ambiguous) {
						*existing = *directEdge;
					}
					delete directEdge;
					replaced = true;
					break;
				}
			}
			if (!replaced)
				fEdges.AddItem(directEdge);
		}
	}

}


void
NetworkMapView::LoadSavedEdges()
{
	DatabaseManager* db = DatabaseManager::Instance();
	if (db == NULL || !db->IsOpen())
		return;

	BMessage edgeData;
	int32 count = db->LoadTopologyEdges(&edgeData);
	if (count == 0)
		return;

	for (int32 i = 0; i < count; i++) {
		const char* fromHex = NULL;
		const char* toHex = NULL;
		int8 snr = 0;
		int32 ts = 0;

		if (edgeData.FindString("from_key", i, &fromHex) != B_OK)
			continue;
		if (edgeData.FindString("to_key", i, &toHex) != B_OK)
			continue;
		edgeData.FindInt8("snr", i, &snr);
		edgeData.FindInt32("timestamp", i, &ts);

		TopologyEdge* edge = new TopologyEdge();
		_HexToPrefix(fromHex, edge->fromPrefix, kPubKeyPrefixSize);
		_HexToPrefix(toHex, edge->toPrefix, kPubKeyPrefixSize);
		edge->snr = snr;
		edge->timestamp = (uint32)ts;

		// Skip if already exists
		bool exists = false;
		for (int32 e = 0; e < fEdges.CountItems(); e++) {
			TopologyEdge* existing = fEdges.ItemAt(e);
			if (existing &&
				memcmp(existing->fromPrefix, edge->fromPrefix, kPubKeyPrefixSize) == 0 &&
				memcmp(existing->toPrefix, edge->toPrefix, kPubKeyPrefixSize) == 0) {
				exists = true;
				break;
			}
		}
		if (exists) {
			delete edge;
		} else {
			fEdges.AddItem(edge);
		}
	}

}


void
NetworkMapView::ExpireStaleEdges()
{
	uint32 now = (uint32)time(NULL);
	for (int32 i = fEdges.CountItems() - 1; i >= 0; i--) {
		TopologyEdge* edge = fEdges.ItemAt(i);
		if (edge && now - edge->timestamp > kEdgeExpireSeconds)
			fEdges.RemoveItemAt(i);
	}
}


MapNode*
NetworkMapView::FindNodeByHopPrefix(const uint8* hopPrefix,
	size_t prefixLen) const
{
	return _MatchHopToContact(hopPrefix, prefixLen);
}


int32
NetworkMapView::GetMultiHopNodes(BObjectList<MapNode>* outList) const
{
	if (outList == NULL)
		return 0;

	int32 count = 0;
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;
		if (node->hops >= 2 &&
			(node->status == STATUS_ONLINE || node->status == STATUS_RECENT)) {
			outList->AddItem(node);
			count++;
		}
	}
	return count;
}


int32
NetworkMapView::GetOnlineNodes(BObjectList<MapNode>* outList) const
{
	if (outList == NULL)
		return 0;

	int32 count = 0;
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;
		// Include all known nodes for topology discovery
		outList->AddItem(node);
		count++;
	}
	return count;
}


void
NetworkMapView::_CalculatePositions()
{
	int32 count = fNodes.CountItems();
	if (count == 0)
		return;

	// Layered concentric layout:
	// Ring 0 = self (center)
	// Ring 1 = direct contacts (hops <= 1)
	// Ring 2 = 2-hop contacts, clustered near discovered relay
	// Ring 3 = 3+ hop contacts

	// Pre-pass: update hops based on discovered topology edges
	// If a trace revealed a node goes through a relay, promote it to Ring 2+
	for (int32 i = 0; i < count; i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		MapNode* relay = _FindRelayForNode(node);
		if (relay != NULL && node->hops <= 1) {
			// Trace discovered this node goes through a relay, not direct
			node->hops = 2;
		}
	}

	// First pass: separate nodes into rings
	BObjectList<MapNode> ring1(10);  // Direct
	BObjectList<MapNode> ring2(10);  // 2-hop
	BObjectList<MapNode> ring3(10);  // 3+ hop

	for (int32 i = 0; i < count; i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		// Skip hidden nodes from layout so visible nodes redistribute evenly
		if (_IsNodeHidden(*node))
			continue;

		if (node->hops <= 1)
			ring1.AddItem(node);
		else if (node->hops == 2)
			ring2.AddItem(node);
		else
			ring3.AddItem(node);
	}

	// Position Ring 1: direct contacts evenly around center
	int32 r1Count = ring1.CountItems();
	for (int32 i = 0; i < r1Count; i++) {
		MapNode* node = ring1.ItemAt(i);
		if (node == NULL)
			continue;

		// Use pubkey for consistent angle
		uint32 hash = node->pubKeyPrefix[0] | (node->pubKeyPrefix[1] << 8);
		float baseAngle = (hash % 360) * M_PI / 180.0f;
		// Spread evenly if many direct nodes
		float angleSpread = (r1Count > 1) ? (2.0f * M_PI / r1Count) : 0;
		float angle = baseAngle + i * angleSpread * 0.3f;

		float distance = kRing1Distance * fZoom;

		// Repeaters get drawn slightly closer (hub position)
		if (node->nodeType == NODE_REPEATER || node->nodeType == NODE_ROOM)
			distance *= 0.85f;

		node->targetPosition.x = fCenter.x + cosf(angle) * distance;
		node->targetPosition.y = fCenter.y + sinf(angle) * distance;

		if (node->position.x == 0 && node->position.y == 0)
			node->position = fCenter;
	}

	// Position Ring 2: 2-hop contacts clustered near their relay
	for (int32 i = 0; i < ring2.CountItems(); i++) {
		MapNode* node = ring2.ItemAt(i);
		if (node == NULL)
			continue;

		// Try to find the relay this node connects through
		MapNode* relay = _FindRelayForNode(node);

		uint32 hash = node->pubKeyPrefix[0] | (node->pubKeyPrefix[1] << 8);
		float baseAngle = (hash % 360) * M_PI / 180.0f;

		if (relay != NULL) {
			// Position near the relay, further from center
			float relayAngle = atan2f(relay->targetPosition.y - fCenter.y,
									  relay->targetPosition.x - fCenter.x);
			// Spread around relay angle
			float spread = baseAngle * 0.3f - 0.15f * M_PI;
			float angle = relayAngle + spread;
			float distance = kRing2Distance * fZoom;

			node->targetPosition.x = fCenter.x + cosf(angle) * distance;
			node->targetPosition.y = fCenter.y + sinf(angle) * distance;
		} else {
			// No relay found — use standard positioning
			float angle = baseAngle + i * 0.4f;
			float distance = kRing2Distance * fZoom;

			node->targetPosition.x = fCenter.x + cosf(angle) * distance;
			node->targetPosition.y = fCenter.y + sinf(angle) * distance;
		}

		if (node->position.x == 0 && node->position.y == 0)
			node->position = fCenter;
	}

	// Position Ring 3: 3+ hop contacts
	for (int32 i = 0; i < ring3.CountItems(); i++) {
		MapNode* node = ring3.ItemAt(i);
		if (node == NULL)
			continue;

		MapNode* relay = _FindRelayForNode(node);

		uint32 hash = node->pubKeyPrefix[0] | (node->pubKeyPrefix[1] << 8);
		float baseAngle = (hash % 360) * M_PI / 180.0f;

		if (relay != NULL) {
			float relayAngle = atan2f(relay->targetPosition.y - fCenter.y,
									  relay->targetPosition.x - fCenter.x);
			float spread = baseAngle * 0.3f - 0.15f * M_PI;
			float angle = relayAngle + spread;
			float distance = kRing3Distance * fZoom;

			node->targetPosition.x = fCenter.x + cosf(angle) * distance;
			node->targetPosition.y = fCenter.y + sinf(angle) * distance;
		} else {
			float angle = baseAngle + i * 0.5f;
			float distance = kRing3Distance * fZoom;

			node->targetPosition.x = fCenter.x + cosf(angle) * distance;
			node->targetPosition.y = fCenter.y + sinf(angle) * distance;
		}

		if (node->position.x == 0 && node->position.y == 0)
			node->position = fCenter;
	}

	// Overlap resolution: push apart nodes that are too close
	float minSep = (kNodeRadiusMax * 2.0f + 20.0f) * fZoom;
	for (int32 pass = 0; pass < 5; pass++) {
		bool moved = false;
		for (int32 i = 0; i < count; i++) {
			MapNode* a = fNodes.ItemAt(i);
			if (a == NULL) continue;
			for (int32 j = i + 1; j < count; j++) {
				MapNode* b = fNodes.ItemAt(j);
				if (b == NULL) continue;

				float dx = b->targetPosition.x - a->targetPosition.x;
				float dy = b->targetPosition.y - a->targetPosition.y;
				float dist = sqrtf(dx * dx + dy * dy);

				if (dist < minSep && dist > 0.01f) {
					float overlap = (minSep - dist) * 0.5f;
					float nx = dx / dist;
					float ny = dy / dist;
					a->targetPosition.x -= nx * overlap;
					a->targetPosition.y -= ny * overlap;
					b->targetPosition.x += nx * overlap;
					b->targetPosition.y += ny * overlap;
					moved = true;
				} else if (dist <= 0.01f) {
					// Coincident — nudge using index to break tie
					float angle = (i * 2.3f + j * 1.7f);
					a->targetPosition.x -= cosf(angle) * minSep * 0.5f;
					a->targetPosition.y -= sinf(angle) * minSep * 0.5f;
					b->targetPosition.x += cosf(angle) * minSep * 0.5f;
					b->targetPosition.y += sinf(angle) * minSep * 0.5f;
					moved = true;
				}
			}
		}
		if (!moved)
			break;
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

	// Draw hub glow for repeaters that have discovered edges
	if (isRepeater) {
		bool isHub = false;
		for (int32 e = 0; e < fEdges.CountItems(); e++) {
			TopologyEdge* edge = fEdges.ItemAt(e);
			if (edge && !edge->ambiguous &&
				memcmp(edge->fromPrefix, node.pubKeyPrefix, kPubKeyPrefixSize) == 0) {
				isHub = true;
				break;
			}
		}
		if (isHub) {
			float glowRadius = radius * 2.5f;
			rgb_color glow = kHubGlowColor;
			glow.alpha = (uint8)(40 * opacity);
			SetHighColor(glow);
			FillEllipse(node.position, glowRadius, glowRadius);
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
NetworkMapView::_DrawUnknownPath(BPoint from, BPoint to, const MapNode* node)
{
	if (node == NULL)
		return;

	float opacity = _OpacityForNode(*node) * 0.4f;

	// Very faint thin dashed line
	rgb_color lineColor = kLinkUnknown;
	lineColor.alpha = (uint8)(80 * opacity);
	SetHighColor(lineColor);
	SetPenSize(1.0f * fZoom);

	float dx = to.x - from.x;
	float dy = to.y - from.y;
	float len = sqrtf(dx * dx + dy * dy);
	float dashLen = 8.0f;
	int dashes = (int)(len / dashLen);
	if (dashes < 1) dashes = 1;

	for (int i = 0; i < dashes; i += 2) {
		float t1 = (float)i / dashes;
		float t2 = (float)(i + 1) / dashes;
		StrokeLine(
			BPoint(from.x + dx * t1, from.y + dy * t1),
			BPoint(from.x + dx * t2, from.y + dy * t2));
	}

	SetPenSize(1.0f);

	// Draw "?" label at midpoint to indicate unknown path
	BPoint mid((from.x + to.x) / 2, (from.y + to.y) / 2);

	BFont font;
	GetFont(&font);
	font.SetSize(10);
	font.SetFace(B_BOLD_FACE);
	SetFont(&font);

	char label[16];
	snprintf(label, sizeof(label), "? %d hops", node->hops);
	float labelWidth = StringWidth(label);
	float labelX = mid.x - labelWidth / 2;
	float labelY = mid.y + 4;

	// Background pill
	rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
	bg.alpha = (uint8)(180 * opacity);
	SetHighColor(bg);
	FillRoundRect(BRect(labelX - 4, labelY - 11, labelX + labelWidth + 4, labelY + 3),
		4, 4);

	// Border
	rgb_color borderColor = kLinkUnknown;
	borderColor.alpha = (uint8)(120 * opacity);
	SetHighColor(borderColor);
	StrokeRoundRect(BRect(labelX - 4, labelY - 11, labelX + labelWidth + 4, labelY + 3),
		4, 4);

	// Text
	rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
	textColor.alpha = (uint8)(180 * opacity);
	SetHighColor(textColor);
	DrawString(label, BPoint(labelX, labelY));

	font.SetFace(0);
	SetFont(&font);
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
NetworkMapView::_DrawPacketFlows()
{
	for (int32 i = 0; i < fPacketFlowCount; i++) {
		PacketFlowAnim& flow = fPacketFlows[i];
		if (!flow.active)
			continue;

		// Find source and destination positions
		BPoint fromPos = fCenter;
		BPoint toPos = fCenter;

		// Self node uses fCenter
		bool srcIsSelf = (memcmp(flow.fromPrefix, fSelfNode.pubKeyPrefix,
			kPubKeyPrefixSize) == 0);
		bool dstIsSelf = (memcmp(flow.toPrefix, fSelfNode.pubKeyPrefix,
			kPubKeyPrefixSize) == 0);

		if (srcIsSelf) {
			fromPos = fCenter;
		} else {
			MapNode* srcNode = _FindNodeByPrefix(flow.fromPrefix);
			if (srcNode != NULL)
				fromPos = srcNode->position;
			else
				continue;  // Source node not found
		}

		if (dstIsSelf) {
			toPos = fCenter;
		} else {
			MapNode* dstNode = _FindNodeByPrefix(flow.toPrefix);
			if (dstNode != NULL)
				toPos = dstNode->position;
			else
				continue;  // Dest node not found
		}

		float dx = toPos.x - fromPos.x;
		float dy = toPos.y - fromPos.y;
		float len = sqrtf(dx * dx + dy * dy);
		if (len < 20.0f)
			continue;

		float phase = flow.phase < 1.0f ? flow.phase : 1.0f;
		float baseAlpha = flow.alpha;

		// Main dot position
		float dotX = fromPos.x + dx * phase;
		float dotY = fromPos.y + dy * phase;

		// Draw trail (3 smaller dots behind the main one)
		float trailOffsets[] = {0.03f, 0.06f, 0.09f};
		float trailRadii[] = {4.0f, 3.0f, 2.0f};
		float trailAlphas[] = {0.6f, 0.3f, 0.1f};

		SetPenSize(1.0f);
		for (int t = 2; t >= 0; t--) {
			float trailPhase = phase - trailOffsets[t];
			if (trailPhase < 0.0f)
				continue;
			float tx = fromPos.x + dx * trailPhase;
			float ty = fromPos.y + dy * trailPhase;

			rgb_color trailColor = flow.color;
			trailColor.alpha = (uint8)(255 * baseAlpha * trailAlphas[t]);
			SetHighColor(trailColor);
			float r = trailRadii[t] * fZoom;
			FillEllipse(BPoint(tx, ty), r, r);
		}

		// Draw main dot
		rgb_color mainColor = flow.color;
		mainColor.alpha = (uint8)(255 * baseAlpha);
		SetHighColor(mainColor);
		float mainRadius = 5.0f * fZoom;
		FillEllipse(BPoint(dotX, dotY), mainRadius, mainRadius);

		// Trigger destination node pulse when arriving
		if (flow.phase >= 1.0f && flow.alpha > 0.9f) {
			if (dstIsSelf) {
				// Pulse self node
				fSelfNode.pulsePhase = (float)M_PI;
			} else {
				MapNode* dstNode = _FindNodeByPrefix(flow.toPrefix);
				if (dstNode != NULL && dstNode->pulsePhase < 0.5f)
					dstNode->pulsePhase = 1.0f;
			}
		}
	}
}


void
NetworkMapView::_DrawTraceRoutes()
{
	uint32 now = (uint32)time(NULL);

	for (int32 r = 0; r < fTraceRoutes.CountItems(); r++) {
		TraceRoute* route = fTraceRoutes.ItemAt(r);
		if (route == NULL || route->numHops == 0)
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

		if (route->numHops <= 1) {
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
			for (uint8 h = 0; h < route->numHops; h++) {
				float t = (float)(h + 1) / (route->numHops + 1);
				// Offset each hop slightly perpendicular for visibility
				float perpX = -dy * 0.08f;
				float perpY = dx * 0.08f;
				float hopOffset = (h % 2 == 0) ? 1.0f : -1.0f;

				BPoint hopPoint(
					pathStart.x + dx * t + perpX * hopOffset,
					pathStart.y + dy * t + perpY * hopOffset);

				// SNR color for this hop segment
				int8 hopSnr = route->hops[h].snr;
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
			int8 destSnr = route->destSnr;
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
NetworkMapView::_DrawTopologyEdges()
{
	uint32 now = (uint32)time(NULL);

	for (int32 e = 0; e < fEdges.CountItems(); e++) {
		TopologyEdge* edge = fEdges.ItemAt(e);
		if (edge == NULL)
			continue;

		// Find from and to nodes
		BPoint fromPos;
		BPoint toPos;
		float opacity = edge->ambiguous ? 0.5f : 0.8f;

		// Check if "from" is self node (all zeros or matching self prefix)
		uint8 zeroPrefix[kPubKeyPrefixSize];
		memset(zeroPrefix, 0, sizeof(zeroPrefix));
		bool fromIsSelf = (memcmp(edge->fromPrefix, zeroPrefix, kPubKeyPrefixSize) == 0)
			|| (memcmp(edge->fromPrefix, fSelfNode.pubKeyPrefix, kPubKeyPrefixSize) == 0);

		if (fromIsSelf) {
			fromPos = fCenter;
		} else {
			MapNode* fromNode = _FindNodeByPrefix(edge->fromPrefix);
			if (fromNode == NULL)
				continue;
			if (_IsNodeHidden(*fromNode))
				continue;
			fromPos = fromNode->position;
			opacity *= _OpacityForNode(*fromNode);
		}

		bool toIsSelf = (memcmp(edge->toPrefix, zeroPrefix, kPubKeyPrefixSize) == 0)
			|| (memcmp(edge->toPrefix, fSelfNode.pubKeyPrefix, kPubKeyPrefixSize) == 0);

		MapNode* toNode = NULL;
		if (toIsSelf) {
			toPos = fCenter;
			toNode = &fSelfNode;
		} else {
			toNode = _FindNodeByPrefix(edge->toPrefix);
			if (toNode == NULL)
				continue;
			if (_IsNodeHidden(*toNode))
				continue;
			toPos = toNode->position;
			opacity *= _OpacityForNode(*toNode);
		}

		// Fade out edges approaching expiry
		uint32 age = (now > edge->timestamp) ? (now - edge->timestamp) : 0;
		if (age > kEdgeExpireSeconds - 120) {
			float fade = (float)(kEdgeExpireSeconds - age) / 120.0f;
			opacity *= fmaxf(0.0f, fade);
		}

		// Color based on hop SNR
		rgb_color lineColor = _ColorForSNR(edge->snr);
		lineColor.alpha = (uint8)(200 * opacity);

		float thickness = _ThicknessForSNR(edge->snr) * fZoom;
		if (edge->ambiguous)
			thickness = fmaxf(1.0f, thickness * 0.6f);

		SetHighColor(lineColor);
		SetPenSize(thickness);

		if (edge->ambiguous) {
			// Draw dashed line for ambiguous (outPath-derived) edges
			BPoint delta = toPos - fromPos;
			float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
			if (length > 0) {
				float dashLen = 6.0f * fZoom;
				float gapLen = 4.0f * fZoom;
				float step = dashLen + gapLen;
				float dx = delta.x / length;
				float dy = delta.y / length;
				for (float d = 0; d < length; d += step) {
					float end = fminf(d + dashLen, length);
					StrokeLine(
						BPoint(fromPos.x + dx * d, fromPos.y + dy * d),
						BPoint(fromPos.x + dx * end, fromPos.y + dy * end));
				}
			}
		} else {
			StrokeLine(fromPos, toPos);
		}

		// Draw glow for repeater hub connections
		if (!fromIsSelf) {
			MapNode* fromNode = _FindNodeByPrefix(edge->fromPrefix);
			if (fromNode && (fromNode->nodeType == NODE_REPEATER ||
							 fromNode->nodeType == NODE_ROOM)) {
				rgb_color glow = kHubGlowColor;
				glow.alpha = (uint8)(30 * opacity);
				SetHighColor(glow);
				SetPenSize(thickness + 6.0f * fZoom);
				StrokeLine(fromPos, toPos);
			}
		}

		SetPenSize(1.0f);

		// Draw animated flow dots on discovered edges for active nodes
		if (toNode->status == STATUS_ONLINE || toNode->status == STATUS_RECENT) {
			rgb_color dotColor = lineColor;
			dotColor.alpha = (uint8)(180 * opacity);
			_DrawFlowDots(fromPos, toPos, toNode->flowPhase, dotColor);
		}

		// Draw SNR label at midpoint
		if (fShowSignalStrength) {
			BFont font;
			GetFont(&font);
			font.SetSize(8);
			SetFont(&font);

			char label[16];
			snprintf(label, sizeof(label), "%d dB", edge->snr);

			BPoint mid((fromPos.x + toPos.x) / 2, (fromPos.y + toPos.y) / 2);
			float labelWidth = StringWidth(label);

			// Background pill
			SetHighColor(0, 0, 0, (uint8)(180 * opacity));
			FillRoundRect(BRect(mid.x - labelWidth / 2 - 3, mid.y - 8,
				mid.x + labelWidth / 2 + 3, mid.y + 4), 3, 3);

			rgb_color textColor = _ColorForSNR(edge->snr);
			textColor.alpha = (uint8)(255 * opacity);
			SetHighColor(textColor);
			DrawString(label, BPoint(mid.x - labelWidth / 2, mid.y + 2));
		}
	}
}


void
NetworkMapView::_DrawInfoPanel()
{
	if (fSelectedNode == NULL)
		return;

	BRect bounds = Bounds();
	float panelWidth = 180;
	float panelHeight = 210;
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

	// Hops
	if (fSelectedNode->hops > 1) {
		SetHighColor(kLabelColor);
		snprintf(buf, sizeof(buf), "Hops: %d", fSelectedNode->hops);
		DrawString(buf, BPoint(x, y));
		y += 14;

		// Show relay if discovered
		MapNode* relay = _FindRelayForNode(fSelectedNode);
		if (relay != NULL) {
			SetHighColor(kEdgeDiscoveredColor);
			char viaStr[80];
			snprintf(viaStr, sizeof(viaStr), "Via: %.70s",
				relay->name[0] ? relay->name : "Unknown");
			DrawString(viaStr, BPoint(x, y));
			y += 14;
		}
	}

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
	int32 hidden = 0;
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;
		if (node->status == STATUS_ONLINE)
			online++;
		if (node->nodeType == NODE_REPEATER)
			repeaters++;
		if (_IsNodeHidden(*node))
			hidden++;
	}

	int32 edges = fEdges.CountItems();

	char stats[160];
	if (hidden > 0) {
		snprintf(stats, sizeof(stats),
			"Nodes: %d (%d online, %d hidden)",
			(int)total, (int)online, (int)hidden);
	} else if (edges > 0) {
		snprintf(stats, sizeof(stats),
			"Nodes: %d (%d online, %d repeaters) \xe2\x80\x94 %d edges",
			(int)total, (int)online, (int)repeaters, (int)edges);
	} else if (repeaters > 0) {
		snprintf(stats, sizeof(stats),
			"Nodes: %d total, %d online, %d repeaters",
			(int)total, (int)online, (int)repeaters);
	} else {
		snprintf(stats, sizeof(stats),
			"Nodes: %d total, %d online", (int)total, (int)online);
	}

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
	if (rssi >= kRssiGood)
		return kSignalExcellent;
	else if (rssi >= -70)
		return kSignalGood;
	else if (rssi >= kRssiFair)
		return kSignalFair;
	else if (rssi >= kRssiPoor)
		return kSignalPoor;
	else
		return kSignalBad;
}


rgb_color
NetworkMapView::_ColorForSNR(int8 snr) const
{
	if (snr > kSnrExcellent)
		return kLinkExcellent;
	else if (snr >= kSnrGood)
		return kLinkGood;
	else if (snr >= kSnrFair)
		return kLinkFair;
	else if (snr >= kSnrPoor)
		return kLinkPoor;
	else
		return kLinkBad;
}


LinkQuality
NetworkMapView::_QualityForSNR(int8 snr) const
{
	if (snr > kSnrExcellent)
		return LINK_EXCELLENT;
	else if (snr >= kSnrGood)
		return LINK_GOOD;
	else if (snr >= kSnrFair)
		return LINK_FAIR;
	else if (snr >= kSnrPoor)
		return LINK_POOR;
	else
		return LINK_BAD;
}


float
NetworkMapView::_ThicknessForSNR(int8 snr) const
{
	// Map SNR to line thickness: excellent=3.5, bad=1.5
	if (snr > kSnrExcellent)
		return 3.5f;
	else if (snr >= kSnrGood)
		return 3.0f;
	else if (snr >= kSnrFair)
		return 2.5f;
	else if (snr >= kSnrPoor)
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


MapNode*
NetworkMapView::_MatchHopToContact(const uint8* hopPrefix,
	size_t prefixLen) const
{
	// Match a hop's 4-byte (or 1-byte) prefix against known contacts
	if (prefixLen == 0)
		return NULL;

	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		if (memcmp(node->pubKeyPrefix, hopPrefix, prefixLen) == 0)
			return node;
	}

	return NULL;
}


MapNode*
NetworkMapView::_FindNodeByPrefix(const uint8* prefix) const
{
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		MapNode* node = fNodes.ItemAt(i);
		if (node && memcmp(node->pubKeyPrefix, prefix, kPubKeyPrefixSize) == 0)
			return node;
	}
	return NULL;
}


MapNode*
NetworkMapView::_FindRelayForNode(const MapNode* node) const
{
	// Find which relay node this one connects through, using discovered edges
	// Walk backward through edges: find an edge where toPrefix == node's prefix
	// Prefer non-ambiguous (trace) edges, fall back to ambiguous (outPath) edges
	MapNode* ambiguousRelay = NULL;

	for (int32 e = 0; e < fEdges.CountItems(); e++) {
		TopologyEdge* edge = fEdges.ItemAt(e);
		if (edge == NULL)
			continue;

		if (memcmp(edge->toPrefix, node->pubKeyPrefix, kPubKeyPrefixSize) == 0) {
			// "from" is the relay for this node
			// Check if "from" is self (all zeros)
			uint8 zeroPrefix[kPubKeyPrefixSize];
			memset(zeroPrefix, 0, sizeof(zeroPrefix));
			if (memcmp(edge->fromPrefix, zeroPrefix, kPubKeyPrefixSize) == 0)
				return NULL;  // Connected to self, not relayed

			MapNode* relay = _FindNodeByPrefix(edge->fromPrefix);
			if (relay == NULL)
				continue;

			if (!edge->ambiguous)
				return relay;  // High-quality match, return immediately

			if (ambiguousRelay == NULL)
				ambiguousRelay = relay;  // Save as fallback
		}
	}
	return ambiguousRelay;
}


// ============================================================================
// NetworkMapWindow
// ============================================================================

NetworkMapWindow::NetworkMapWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 700, 550), "Network Map",
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fMapView(NULL),
	fInfoLabel(NULL),
	fShowLabelsCheck(NULL),
	fShowSignalCheck(NULL),
	fAutoTraceCheck(NULL),
	fHideInactiveCheck(NULL),
	fZoomSlider(NULL),
	fRefreshButton(NULL),
	fMapNetworkButton(NULL),
	fCloseButton(NULL),
	fRefreshTimer(NULL),
	fAutoTraceTimer(NULL),
	fDiscoveryTimer(NULL),
	fAutoTraceIndex(0),
	fDiscoveryTotal(0),
	fDiscoveryActive(false),
	fDiscoveryWaitTicks(0),
	fHasPendingTrace(false)
{
	memset(fSelfName, 0, sizeof(fSelfName));
	memset(fPendingTracePrefix, 0, sizeof(fPendingTracePrefix));

	// Create map view
	fMapView = new NetworkMapView();

	// Load previously discovered topology edges from database
	fMapView->LoadSavedEdges();

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

	fHideInactiveCheck = new BCheckBox("hide_inactive", "Hide inactive",
		new BMessage(MSG_TOGGLE_HIDE_INACTIVE));
	fHideInactiveCheck->SetValue(B_CONTROL_OFF);

	fZoomSlider = new BSlider("zoom", NULL,
		new BMessage(MSG_ZOOM_CHANGED), 50, 150, B_HORIZONTAL);
	fZoomSlider->SetValue(100);
	fZoomSlider->SetHashMarks(B_HASH_MARKS_NONE);
	fZoomSlider->SetBarThickness(8);

	fRefreshButton = new BButton("refresh", "Refresh",
		new BMessage(MSG_REFRESH_MAP));
	fMapNetworkButton = new BButton("map_network", "Map Network",
		new BMessage(MSG_DISCOVER_TOPOLOGY));
	fCloseButton = new BButton("close", "Close",
		new BMessage(B_QUIT_REQUESTED));

	fInfoLabel = new BStringView("info", "Double-click node to chat \xE2\x80\xA2 Right-click for menu");
	fInfoLabel->SetTruncation(B_TRUNCATE_END);
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
			.Add(fHideInactiveCheck)
			.AddStrut(10)
			.Add(new BStringView("zoom_label", "Zoom:"))
			.Add(fZoomSlider, 0.2)
			.AddGlue()
			.Add(fMapNetworkButton)
			.Add(fRefreshButton)
			.Add(fCloseButton)
		.End()
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_HALF_ITEM_SPACING, 0, B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
			.Add(fInfoLabel)
		.End()
	.End();

	// Allow unlimited resize (layout children may constrain otherwise)
	fMapView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
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
	fRefreshTimer = NULL;
	delete fAutoTraceTimer;
	fAutoTraceTimer = NULL;
	delete fDiscoveryTimer;
	fDiscoveryTimer = NULL;
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

		case MSG_TOGGLE_HIDE_INACTIVE:
			fMapView->SetHideInactive(fHideInactiveCheck->Value() == B_CONTROL_ON);
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

		case MSG_DISCOVER_TOPOLOGY:
			_RequestFullDiscovery();
			break;

		case MSG_DISCOVERY_TICK:
			_DiscoveryTick();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
NetworkMapWindow::UpdateFromContacts(const OwningObjectList<ContactInfo>* contacts)
{
	fMapView->SetNodes(contacts);
}


void
NetworkMapWindow::BuildEdgesFromOutPaths(
	const OwningObjectList<ContactInfo>* contacts)
{
	fMapView->BuildEdgesFromOutPaths(contacts);
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
NetworkMapWindow::SetRepeaterTopology(const char* selfName,
	const RepeaterNodeInfo* nodes, int32 nodeCount,
	const RepeaterLink* links, int32 linkCount)
{
	fMapView->SetRepeaterTopology(selfName, nodes, nodeCount,
		links, linkCount);

	// Update info label
	char info[64];
	snprintf(info, sizeof(info), "Repeater topology — %d nodes, %d links",
		(int)nodeCount, (int)linkCount);
	fInfoLabel->SetText(info);
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
NetworkMapWindow::TriggerPacketFlow(const char* srcHex, const char* dstHex,
	int8 snr, bool isMessage)
{
	fMapView->TriggerPacketFlow(srcHex, dstHex, snr, isMessage);
}


void
NetworkMapWindow::HandleTraceData(const uint8* data, size_t length)
{
	// PUSH_TRACE_DATA format:
	// [0]     = code (0x89)
	// [1]     = reserved
	// [2]     = pathLen (byte count of hop data, NOT hop count)
	// [3]     = flags
	// [4-7]   = tag (uint32)
	// [8-11]  = authCode (uint32)
	// [12..12+pathLen-1]  = hop identifiers
	// [12+pathLen..]      = SNR bytes (numHops + 1 values)
	//
	// Hop format depends on pathLen:
	// - pathLen=1: single 1-byte hash (legacy single-hop)
	// - pathLen>=4: N hops × 4-byte pubkey prefix (numHops = pathLen / 4)

	if (length < 12)
		return;

	uint8 pathLen = data[2];

	// Determine number of hops and hop identifier size
	uint8 numHops;
	uint8 hopSize;
	if (pathLen <= 1) {
		numHops = pathLen;	// 0 or 1
		hopSize = 1;		// single-byte hash
	} else {
		hopSize = 4;		// 4-byte pubkey prefix per hop
		numHops = pathLen / hopSize;
	}
	if (numHops > 16) numHops = 16;

	TraceRoute route;
	route.numHops = numHops;
	route.timestamp = (uint32)time(NULL);

	// Identify destination: try tag bytes [4-7] as 4-byte prefix match
	// against known contacts (most reliable), fall back to pending trace
	bool destFound = false;
	if (length >= 8) {
		MapNode* destByTag = fMapView->FindNodeByHopPrefix(data + 4, 4);
		if (destByTag != NULL) {
			memcpy(route.destKeyPrefix, destByTag->pubKeyPrefix, kPubKeyPrefixSize);
			destFound = true;
		}
	}

	if (!destFound && fHasPendingTrace) {
		memcpy(route.destKeyPrefix, fPendingTracePrefix, kPubKeyPrefixSize);
		destFound = true;
	}

	if (!destFound) {
		MapNode* selectedNode = fMapView->GetSelectedNode();
		if (selectedNode != NULL)
			memcpy(route.destKeyPrefix, selectedNode->pubKeyPrefix, kPubKeyPrefixSize);
	}

	fHasPendingTrace = false;

	// Parse hop identifiers
	size_t hashOffset = 12;
	for (uint8 i = 0; i < numHops; i++) {
		size_t hopStart = hashOffset + (size_t)i * hopSize;
		if (hopStart + hopSize > length)
			break;

		if (hopSize == 4) {
			memcpy(route.hops[i].hopPrefix, data + hopStart, 4);
		} else {
			// Legacy 1-byte hash — store in first byte
			route.hops[i].hopPrefix[0] = data[hopStart];
		}
	}

	// Parse SNR bytes (after hop data)
	size_t snrOffset = hashOffset + pathLen;
	for (uint8 i = 0; i < numHops; i++) {
		if (snrOffset + i < length)
			route.hops[i].snr = (int8)data[snrOffset + i];
	}

	// Destination SNR (last SNR byte)
	if (snrOffset + numHops < length)
		route.destSnr = (int8)data[snrOffset + numHops];

	fMapView->SetTraceRoute(route);

	// Build topology edges from the trace data
	fMapView->BuildEdgesFromTrace(route);

	// If discovery is active/waiting, reset wait timer and update status
	if (fDiscoveryActive) {
		fDiscoveryWaitTicks = 0;  // Reset wait — more responses may come
		char status[80];
		snprintf(status, sizeof(status),
			"Mapping... %d edges discovered (waiting for responses)",
			(int)fMapView->CountEdges());
		fInfoLabel->SetText(status);
	}
}


void
NetworkMapWindow::_RequestAutoTrace()
{
	// If a node is selected, trace that one first
	MapNode* selectedNode = fMapView->GetSelectedNode();
	if (selectedNode != NULL && selectedNode->status <= STATUS_RECENT) {
		memcpy(fPendingTracePrefix, selectedNode->pubKeyPrefix, kPubKeyPrefixSize);
		fHasPendingTrace = true;

		BMessage msg(MSG_TRACE_PATH);
		msg.AddData("pubkey", B_RAW_TYPE, selectedNode->pubKeyPrefix, kPubKeyPrefixSize);
		msg.AddBool("silent", true);
		if (fParent != NULL)
			fParent->PostMessage(&msg);
		return;
	}

	// Round-robin through all online/recent multi-hop nodes
	BObjectList<MapNode> multiHop(10);
	fMapView->GetMultiHopNodes(&multiHop);

	if (multiHop.CountItems() == 0)
		return;

	int32 startIndex = fAutoTraceIndex % multiHop.CountItems();
	int32 idx = startIndex;
	MapNode* node = multiHop.ItemAt(idx);

	if (node != NULL && fParent != NULL) {
		memcpy(fPendingTracePrefix, node->pubKeyPrefix, kPubKeyPrefixSize);
		fHasPendingTrace = true;

		BMessage msg(MSG_TRACE_PATH);
		msg.AddData("pubkey", B_RAW_TYPE, node->pubKeyPrefix, kPubKeyPrefixSize);
		msg.AddBool("silent", true);
		fParent->PostMessage(&msg);
	}

	fAutoTraceIndex = (idx + 1) % multiHop.CountItems();
}


void
NetworkMapWindow::_RequestFullDiscovery()
{
	if (fDiscoveryActive)
		return;  // Already running

	// Queue all online/recent nodes for trace (not just multi-hop)
	fDiscoveryQueue.MakeEmpty();
	fMapView->GetOnlineNodes(&fDiscoveryQueue);

	fDiscoveryTotal = fDiscoveryQueue.CountItems();
	if (fDiscoveryTotal == 0) {
		fInfoLabel->SetText("No online nodes to trace");
		return;
	}

	fDiscoveryActive = true;
	fDiscoveryWaitTicks = 0;
	fMapNetworkButton->SetEnabled(false);

	char status[64];
	snprintf(status, sizeof(status), "Mapping... 0/%d nodes traced",
		(int)fDiscoveryTotal);
	fInfoLabel->SetText(status);

	// Start discovery tick timer (2s between traces)
	delete fDiscoveryTimer;
	BMessage tickMsg(MSG_DISCOVERY_TICK);
	fDiscoveryTimer = new BMessageRunner(this, &tickMsg, kDiscoveryTickInterval);

	// Send first trace immediately
	_DiscoveryTick();
}


void
NetworkMapWindow::_DiscoveryTick()
{
	if (!fDiscoveryActive) {
		return;
	}

	if (fDiscoveryQueue.CountItems() == 0) {
		// All requests sent — wait for radio responses (up to 3 ticks = 30s)
		fDiscoveryWaitTicks++;
		if (fDiscoveryWaitTicks <= 3) {
			char status[64];
			snprintf(status, sizeof(status),
				"Waiting for responses... (%d edges so far)",
				(int)fMapView->CountEdges());
			fInfoLabel->SetText(status);
			return;
		}

		// Done waiting
		fDiscoveryActive = false;
		fMapNetworkButton->SetEnabled(true);
		delete fDiscoveryTimer;
		fDiscoveryTimer = NULL;

		char status[64];
		snprintf(status, sizeof(status),
			"Topology mapped — %d edges discovered",
			(int)fMapView->CountEdges());
		fInfoLabel->SetText(status);
		return;
	}

	// Pop next node from queue
	MapNode* node = fDiscoveryQueue.RemoveItemAt(0);
	if (node != NULL && fParent != NULL) {
		memcpy(fPendingTracePrefix, node->pubKeyPrefix, kPubKeyPrefixSize);
		fHasPendingTrace = true;

		BMessage msg(MSG_TRACE_PATH);
		msg.AddData("pubkey", B_RAW_TYPE, node->pubKeyPrefix, kPubKeyPrefixSize);
		msg.AddBool("silent", true);
		fParent->PostMessage(&msg);
	}

	// Update progress
	int32 done = fDiscoveryTotal - fDiscoveryQueue.CountItems();
	char status[64];
	snprintf(status, sizeof(status), "Mapping... %d/%d nodes traced",
		(int)done, (int)fDiscoveryTotal);
	fInfoLabel->SetText(status);
}


void
NetworkMapWindow::_RequestUpdate()
{
	// Update map data without bringing window to front
	if (fParent != NULL) {
		BMessage msg(MSG_UPDATE_MAP_DATA);
		fParent->PostMessage(&msg);
	}
}
