/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MeshGraphView.cpp — Network topology graph visualization implementation
 */

#include "MeshGraphView.h"

#include <GroupLayout.h>
#include <Window.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Constants.h"
#include "Protocol.h"


static const float kMinNodeRadius = 12.0f;
static const float kMaxNodeRadius = 25.0f;
static const float kSelfNodeRadius = 30.0f;

// Physics constants
static const float kDefaultRepulsion = 5000.0f;
static const float kDefaultAttraction = 0.01f;
static const float kDefaultDamping = 0.85f;
static const float kDefaultCenterPull = 0.001f;
static const float kMinVelocity = 0.1f;


MeshGraphView::MeshGraphView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_PULSE_NEEDED),
	fNodes(20),
	fEdges(50),
	fSelfIndex(-1),
	fAnimationEnabled(true),
	fDraggedNode(NULL),
	fRepulsion(kDefaultRepulsion),
	fAttraction(kDefaultAttraction),
	fDamping(kDefaultDamping),
	fCenterPull(kDefaultCenterPull)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


MeshGraphView::~MeshGraphView()
{
}


void
MeshGraphView::AttachedToWindow()
{
	BView::AttachedToWindow();

	if (fAnimationEnabled)
		Window()->SetPulseRate(50000);  // 50ms = 20fps
}


void
MeshGraphView::DetachedFromWindow()
{
	BView::DetachedFromWindow();
}


void
MeshGraphView::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();

	// Dark background with subtle gradient
	SetHighColor(25, 30, 40);
	FillRect(bounds);

	// Draw grid pattern
	SetHighColor(35, 40, 50);
	float gridSize = 30.0f;
	for (float x = fmod(bounds.left, gridSize); x < bounds.right; x += gridSize)
		StrokeLine(BPoint(x, bounds.top), BPoint(x, bounds.bottom));
	for (float y = fmod(bounds.top, gridSize); y < bounds.bottom; y += gridSize)
		StrokeLine(BPoint(bounds.left, y), BPoint(bounds.right, y));

	// Draw edges first (behind nodes)
	_DrawEdges();

	// Draw nodes
	_DrawNodes();

	// Draw legend
	_DrawLegend();
}


void
MeshGraphView::MouseDown(BPoint where)
{
	GraphNode* node = _FindNodeAt(where);

	if (node != NULL) {
		// Start dragging
		fDraggedNode = node;
		node->isDragging = true;
		fDragOffset = BPoint(node->x - where.x, node->y - where.y);
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);

		// Select node
		for (int32 i = 0; i < fNodes.CountItems(); i++) {
			GraphNode* n = fNodes.ItemAt(i);
			if (n != NULL)
				n->isSelected = false;
		}
		node->isSelected = true;

		Invalidate();
	}
}


void
MeshGraphView::MouseMoved(BPoint where, uint32 /*transit*/, const BMessage* /*dragMessage*/)
{
	if (fDraggedNode != NULL) {
		fDraggedNode->x = where.x + fDragOffset.x;
		fDraggedNode->y = where.y + fDragOffset.y;
		fDraggedNode->vx = 0;
		fDraggedNode->vy = 0;
		Invalidate();
	}
}


void
MeshGraphView::MouseUp(BPoint /*where*/)
{
	if (fDraggedNode != NULL) {
		fDraggedNode->isDragging = false;
		fDraggedNode = NULL;
	}
}


void
MeshGraphView::FrameResized(float newWidth, float newHeight)
{
	BView::FrameResized(newWidth, newHeight);

	// Re-center nodes
	if (fSelfIndex >= 0 && fSelfIndex < fNodes.CountItems()) {
		GraphNode* self = fNodes.ItemAt(fSelfIndex);
		if (self == NULL)
			return;
		self->x = newWidth / 2;
		self->y = newHeight / 2;
	}

	Invalidate();
}


void
MeshGraphView::MessageReceived(BMessage* message)
{
	BView::MessageReceived(message);
}


void
MeshGraphView::Pulse()
{
	if (fAnimationEnabled) {
		_UpdatePhysics();
		Invalidate();
	}
}


void
MeshGraphView::SetSelfNode(const char* name, uint8 type, uint8 txPower)
{
	GraphNode* node = new GraphNode();
	memset(node, 0, sizeof(GraphNode));
	strlcpy(node->name, name, sizeof(node->name));
	node->type = type;
	node->txPower = txPower;
	node->isSelf = true;

	// Center position
	BRect bounds = Bounds();
	node->x = bounds.Width() / 2;
	node->y = bounds.Height() / 2;

	fSelfIndex = fNodes.CountItems();
	fNodes.AddItem(node);

	Invalidate();
}


void
MeshGraphView::AddNode(const Contact& contact)
{
	GraphNode* node = new GraphNode();
	memcpy(node->publicKey, contact.publicKey, kPublicKeySize);
	strlcpy(node->name, contact.advName, sizeof(node->name));
	node->type = contact.type;
	node->pathLen = contact.outPathLen;
	node->txPower = 20;  // Default
	node->isSelf = false;
	node->isSelected = false;
	node->isDragging = false;

	// Random initial position around center
	BRect bounds = Bounds();
	float angle = (float)(rand() % 360) * M_PI / 180.0f;
	float dist = 100 + rand() % 100;
	node->x = bounds.Width() / 2 + cos(angle) * dist;
	node->y = bounds.Height() / 2 + sin(angle) * dist;
	node->vx = 0;
	node->vy = 0;

	int32 newIndex = fNodes.CountItems();
	fNodes.AddItem(node);

	// Add edge to self if we have path info
	if (fSelfIndex >= 0 && contact.outPathLen >= 0) {
		AddEdge(fSelfIndex, newIndex, 0, (uint8)contact.outPathLen);
	}

	Invalidate();
}


void
MeshGraphView::UpdateNode(const Contact& contact)
{
	int32 index = _FindNodeIndex(contact.publicKey);
	if (index < 0) {
		AddNode(contact);
		return;
	}

	GraphNode* node = fNodes.ItemAt(index);
	strlcpy(node->name, contact.advName, sizeof(node->name));
	node->type = contact.type;
	node->pathLen = contact.outPathLen;

	Invalidate();
}


void
MeshGraphView::RemoveNode(const uint8* publicKey)
{
	int32 index = _FindNodeIndex(publicKey);
	if (index < 0)
		return;

	// Remove edges involving this node
	for (int32 i = fEdges.CountItems() - 1; i >= 0; i--) {
		GraphEdge* edge = fEdges.ItemAt(i);
		if (edge->fromIndex == index || edge->toIndex == index)
			fEdges.RemoveItemAt(i);
	}

	// Update edge indices
	for (int32 i = 0; i < fEdges.CountItems(); i++) {
		GraphEdge* edge = fEdges.ItemAt(i);
		if (edge->fromIndex > index) edge->fromIndex--;
		if (edge->toIndex > index) edge->toIndex--;
	}

	// Update self index
	if (fSelfIndex > index)
		fSelfIndex--;
	else if (fSelfIndex == index)
		fSelfIndex = -1;

	fNodes.RemoveItemAt(index);
	Invalidate();
}


void
MeshGraphView::ClearNodes()
{
	fNodes.MakeEmpty();
	fEdges.MakeEmpty();
	fSelfIndex = -1;
	fDraggedNode = NULL;
	Invalidate();
}


void
MeshGraphView::AddEdge(int32 fromIndex, int32 toIndex, int8 snr, uint8 hops)
{
	// Check if edge already exists
	for (int32 i = 0; i < fEdges.CountItems(); i++) {
		GraphEdge* edge = fEdges.ItemAt(i);
		if ((edge->fromIndex == fromIndex && edge->toIndex == toIndex) ||
			(edge->fromIndex == toIndex && edge->toIndex == fromIndex)) {
			edge->snr = snr;
			edge->hops = hops;
			edge->isActive = true;
			return;
		}
	}

	GraphEdge* edge = new GraphEdge();
	edge->fromIndex = fromIndex;
	edge->toIndex = toIndex;
	edge->snr = snr;
	edge->hops = hops;
	edge->isActive = true;

	fEdges.AddItem(edge);
}


void
MeshGraphView::SetEdgeActive(int32 fromIndex, int32 toIndex, bool active)
{
	for (int32 i = 0; i < fEdges.CountItems(); i++) {
		GraphEdge* edge = fEdges.ItemAt(i);
		if ((edge->fromIndex == fromIndex && edge->toIndex == toIndex) ||
			(edge->fromIndex == toIndex && edge->toIndex == fromIndex)) {
			edge->isActive = active;
			Invalidate();
			return;
		}
	}
}


void
MeshGraphView::SetAnimationEnabled(bool enabled)
{
	fAnimationEnabled = enabled;
	if (Window() != NULL) {
		if (enabled)
			Window()->SetPulseRate(50000);
		else
			Window()->SetPulseRate(0);
	}
}


void
MeshGraphView::ResetLayout()
{
	BRect bounds = Bounds();
	float cx = bounds.Width() / 2;
	float cy = bounds.Height() / 2;

	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GraphNode* node = fNodes.ItemAt(i);

		if (node->isSelf) {
			node->x = cx;
			node->y = cy;
		} else {
			float angle = (float)(rand() % 360) * M_PI / 180.0f;
			float dist = 100 + rand() % 100;
			node->x = cx + cos(angle) * dist;
			node->y = cy + sin(angle) * dist;
		}

		node->vx = 0;
		node->vy = 0;
	}

	Invalidate();
}


void
MeshGraphView::_DrawNodes()
{
	// Draw self node last (on top)
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GraphNode* node = fNodes.ItemAt(i);
		if (node != NULL && i != fSelfIndex)
			_DrawNode(*node);
	}

	if (fSelfIndex >= 0 && fSelfIndex < fNodes.CountItems()) {
		GraphNode* self = fNodes.ItemAt(fSelfIndex);
		if (self != NULL)
			_DrawNode(*self);
	}
}


void
MeshGraphView::_DrawNode(const GraphNode& node)
{
	float radius = _NodeRadius(node);
	rgb_color color = _NodeColor(node);

	// Glow effect for self
	if (node.isSelf) {
		SetDrawingMode(B_OP_ALPHA);
		for (float r = radius + 15; r > radius; r -= 3) {
			uint8 alpha = (uint8)(50 * (1.0f - (r - radius) / 15.0f));
			SetHighColor(color.red, color.green, color.blue, alpha);
			FillEllipse(BPoint(node.x, node.y), r, r);
		}
		SetDrawingMode(B_OP_COPY);
	}

	// Selection ring
	if (node.isSelected) {
		SetHighColor(255, 200, 0);
		SetPenSize(3.0f);
		StrokeEllipse(BPoint(node.x, node.y), radius + 4, radius + 4);
		SetPenSize(1.0f);
	}

	// Node body
	SetHighColor(color);
	FillEllipse(BPoint(node.x, node.y), radius, radius);

	// Node border
	SetHighColor(255, 255, 255);
	SetPenSize(node.isSelf ? 3.0f : 2.0f);
	StrokeEllipse(BPoint(node.x, node.y), radius, radius);
	SetPenSize(1.0f);

	// Icon based on type
	SetHighColor(255, 255, 255);
	BFont font;
	GetFont(&font);
	font.SetSize(radius * 0.8f);
	SetFont(&font);

	const char* icon = "";
	switch (node.type) {
		case ADV_TYPE_CHAT:		icon = "C"; break;
		case ADV_TYPE_REPEATER:	icon = "R"; break;
		case ADV_TYPE_ROOM:		icon = "H"; break;
		default:				icon = "?"; break;
	}

	float iconWidth = font.StringWidth(icon);
	DrawString(icon, BPoint(node.x - iconWidth / 2, node.y + radius * 0.3f));

	// Name label
	font.SetSize(11);
	SetFont(&font);
	float labelWidth = font.StringWidth(node.name);

	// Label background
	SetHighColor(0, 0, 0, 200);
	FillRoundRect(BRect(node.x - labelWidth / 2 - 4, node.y + radius + 4,
		node.x + labelWidth / 2 + 4, node.y + radius + 20), 4, 4);

	// Label text
	SetHighColor(255, 255, 255);
	DrawString(node.name, BPoint(node.x - labelWidth / 2, node.y + radius + 16));
}


void
MeshGraphView::_DrawEdges()
{
	for (int32 i = 0; i < fEdges.CountItems(); i++) {
		GraphEdge* edge = fEdges.ItemAt(i);
		if (edge != NULL)
			_DrawEdge(*edge);
	}
}


void
MeshGraphView::_DrawEdge(const GraphEdge& edge)
{
	if (edge.fromIndex < 0 || edge.fromIndex >= fNodes.CountItems() ||
		edge.toIndex < 0 || edge.toIndex >= fNodes.CountItems())
		return;

	GraphNode* from = fNodes.ItemAt(edge.fromIndex);
	GraphNode* to = fNodes.ItemAt(edge.toIndex);
	if (from == NULL || to == NULL)
		return;

	rgb_color color = _EdgeColor(edge);
	float thickness = edge.isActive ? 2.5f : 1.0f;

	// Calculate edge endpoints (don't go inside nodes)
	float dx = to->x - from->x;
	float dy = to->y - from->y;
	float dist = sqrt(dx * dx + dy * dy);

	if (dist < 1) return;

	float fromRadius = _NodeRadius(*from);
	float toRadius = _NodeRadius(*to);

	float startX = from->x + dx / dist * fromRadius;
	float startY = from->y + dy / dist * fromRadius;
	float endX = to->x - dx / dist * toRadius;
	float endY = to->y - dy / dist * toRadius;

	SetHighColor(color);
	SetPenSize(thickness);
	StrokeLine(BPoint(startX, startY), BPoint(endX, endY));

	// Draw hop count indicator
	if (edge.hops > 0 && edge.hops < 10) {
		float midX = (startX + endX) / 2;
		float midY = (startY + endY) / 2;

		SetHighColor(40, 50, 70);
		FillEllipse(BPoint(midX, midY), 10, 10);
		SetHighColor(255, 255, 255);
		StrokeEllipse(BPoint(midX, midY), 10, 10);

		char hopStr[4];
		snprintf(hopStr, sizeof(hopStr), "%d", edge.hops);
		BFont font;
		GetFont(&font);
		font.SetSize(10);
		SetFont(&font);
		float w = font.StringWidth(hopStr);
		DrawString(hopStr, BPoint(midX - w / 2, midY + 4));
	}

	SetPenSize(1.0f);
}


void
MeshGraphView::_DrawLegend()
{
	BRect bounds = Bounds();
	float x = 15;
	float y = bounds.Height() - 100;

	// Background
	SetHighColor(0, 0, 0, 180);
	FillRoundRect(BRect(x - 5, y - 5, x + 120, y + 85), 5, 5);

	SetHighColor(255, 255, 255);
	BFont font;
	GetFont(&font);
	font.SetSize(10);
	SetFont(&font);

	DrawString("Legend:", BPoint(x, y + 10));

	// Node types
	y += 25;
	SetHighColor(100, 180, 255);
	FillEllipse(BPoint(x + 8, y), 6, 6);
	SetHighColor(255, 255, 255);
	DrawString("Chat", BPoint(x + 20, y + 4));

	y += 18;
	SetHighColor(100, 255, 100);
	FillEllipse(BPoint(x + 8, y), 6, 6);
	SetHighColor(255, 255, 255);
	DrawString("Repeater", BPoint(x + 20, y + 4));

	y += 18;
	SetHighColor(255, 180, 100);
	FillEllipse(BPoint(x + 8, y), 6, 6);
	SetHighColor(255, 255, 255);
	DrawString("Room", BPoint(x + 20, y + 4));

	y += 18;
	SetHighColor(255, 100, 100);
	SetPenSize(2);
	StrokeLine(BPoint(x, y), BPoint(x + 16, y));
	SetHighColor(255, 255, 255);
	DrawString("Direct", BPoint(x + 20, y + 4));
	SetPenSize(1);
}


void
MeshGraphView::_UpdatePhysics()
{
	if (fNodes.CountItems() < 2)
		return;

	_ApplyForces();

	// Update positions
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GraphNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;

		if (node->isDragging)
			continue;

		// Apply velocity
		node->x += node->vx;
		node->y += node->vy;

		// Apply damping
		node->vx *= fDamping;
		node->vy *= fDamping;

		// Stop if velocity is very small
		if (fabs(node->vx) < kMinVelocity) node->vx = 0;
		if (fabs(node->vy) < kMinVelocity) node->vy = 0;
	}

	_ConstrainToView();
}


void
MeshGraphView::_ApplyForces()
{
	BRect bounds = Bounds();
	float cx = bounds.Width() / 2;
	float cy = bounds.Height() / 2;

	// Repulsion between all nodes
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GraphNode* nodeA = fNodes.ItemAt(i);
		if (nodeA == NULL)
			continue;

		if (nodeA->isDragging)
			continue;

		for (int32 j = i + 1; j < fNodes.CountItems(); j++) {
			GraphNode* nodeB = fNodes.ItemAt(j);
			if (nodeB == NULL)
				continue;

			float dx = nodeB->x - nodeA->x;
			float dy = nodeB->y - nodeA->y;
			float dist = sqrt(dx * dx + dy * dy);

			if (dist < 1) dist = 1;

			float force = fRepulsion / (dist * dist);

			float fx = (dx / dist) * force;
			float fy = (dy / dist) * force;

			if (!nodeA->isDragging) {
				nodeA->vx -= fx;
				nodeA->vy -= fy;
			}
			if (!nodeB->isDragging) {
				nodeB->vx += fx;
				nodeB->vy += fy;
			}
		}

		// Center pull (keeps graph centered)
		if (!nodeA->isSelf) {
			nodeA->vx += (cx - nodeA->x) * fCenterPull;
			nodeA->vy += (cy - nodeA->y) * fCenterPull;
		}
	}

	// Attraction along edges
	for (int32 i = 0; i < fEdges.CountItems(); i++) {
		GraphEdge* edge = fEdges.ItemAt(i);

		if (edge == NULL ||
			edge->fromIndex < 0 || edge->fromIndex >= fNodes.CountItems() ||
			edge->toIndex < 0 || edge->toIndex >= fNodes.CountItems())
			continue;

		GraphNode* from = fNodes.ItemAt(edge->fromIndex);
		GraphNode* to = fNodes.ItemAt(edge->toIndex);
		if (from == NULL || to == NULL)
			continue;

		float dx = to->x - from->x;
		float dy = to->y - from->y;
		float dist = sqrt(dx * dx + dy * dy);

		if (dist < 1) continue;

		// Ideal distance based on hops
		float idealDist = 150 + edge->hops * 50;
		float force = (dist - idealDist) * fAttraction;

		float fx = (dx / dist) * force;
		float fy = (dy / dist) * force;

		if (!from->isDragging) {
			from->vx += fx;
			from->vy += fy;
		}
		if (!to->isDragging) {
			to->vx -= fx;
			to->vy -= fy;
		}
	}
}


void
MeshGraphView::_ConstrainToView()
{
	BRect bounds = Bounds();
	float margin = 50;

	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GraphNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;
		float radius = _NodeRadius(*node);

		if (node->x - radius < margin) {
			node->x = margin + radius;
			node->vx = 0;
		}
		if (node->x + radius > bounds.Width() - margin) {
			node->x = bounds.Width() - margin - radius;
			node->vx = 0;
		}
		if (node->y - radius < margin) {
			node->y = margin + radius;
			node->vy = 0;
		}
		if (node->y + radius > bounds.Height() - margin) {
			node->y = bounds.Height() - margin - radius;
			node->vy = 0;
		}
	}
}


int32
MeshGraphView::_FindNodeIndex(const uint8* publicKey) const
{
	for (int32 i = 0; i < fNodes.CountItems(); i++) {
		GraphNode* node = fNodes.ItemAt(i);
		if (node != NULL &&
			memcmp(node->publicKey, publicKey, kPublicKeySize) == 0)
			return i;
	}
	return -1;
}


GraphNode*
MeshGraphView::_FindNodeAt(BPoint where)
{
	// Check in reverse order (top nodes first)
	for (int32 i = fNodes.CountItems() - 1; i >= 0; i--) {
		GraphNode* node = fNodes.ItemAt(i);
		if (node == NULL)
			continue;
		float radius = _NodeRadius(*node);
		float dx = where.x - node->x;
		float dy = where.y - node->y;

		if (dx * dx + dy * dy <= radius * radius)
			return node;
	}
	return NULL;
}


float
MeshGraphView::_NodeRadius(const GraphNode& node) const
{
	if (node.isSelf)
		return kSelfNodeRadius;

	// Scale based on TX power
	float t = (node.txPower - 10) / 12.0f;  // 10-22 dBm range
	if (t < 0) t = 0;
	if (t > 1) t = 1;

	return kMinNodeRadius + t * (kMaxNodeRadius - kMinNodeRadius);
}


rgb_color
MeshGraphView::_NodeColor(const GraphNode& node) const
{
	switch (node.type) {
		case ADV_TYPE_CHAT:
			return (rgb_color){100, 180, 255, 255};
		case ADV_TYPE_REPEATER:
			return (rgb_color){100, 255, 100, 255};
		case ADV_TYPE_ROOM:
			return (rgb_color){255, 180, 100, 255};
		default:
			return (rgb_color){180, 180, 180, 255};
	}
}


rgb_color
MeshGraphView::_EdgeColor(const GraphEdge& edge) const
{
	if (!edge.isActive)
		return (rgb_color){80, 80, 80, 128};

	if (edge.hops == 0 || edge.hops == 0xFF)
		return (rgb_color){100, 255, 100, 200};  // Direct
	else if (edge.hops <= 2)
		return (rgb_color){255, 200, 50, 200};   // 1-2 hops
	else
		return (rgb_color){255, 100, 50, 200};   // 3+ hops
}


// ============================================================================
// MeshGraphWindow Implementation
// ============================================================================

MeshGraphWindow::MeshGraphWindow(BRect frame, BMessenger target)
	: BWindow(frame, "Network Graph", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	  fTarget(target)
{
	fGraphView = new MeshGraphView("graph");
	fGraphView->SetExplicitMinSize(BSize(400, 300));

	SetLayout(new BGroupLayout(B_VERTICAL));
	AddChild(fGraphView);

	// Enable animation
	fGraphView->SetAnimationEnabled(true);
}


MeshGraphWindow::~MeshGraphWindow()
{
}


void
MeshGraphWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MeshGraphWindow::QuitRequested()
{
	Hide();
	return false;
}


void
MeshGraphWindow::AddNode(uint32 nodeId, const char* name, int pathLen, bool isSelf)
{
	Contact contact;
	memset(&contact, 0, sizeof(contact));

	// Store nodeId in first 4 bytes of publicKey
	contact.publicKey[0] = (nodeId >> 24) & 0xFF;
	contact.publicKey[1] = (nodeId >> 16) & 0xFF;
	contact.publicKey[2] = (nodeId >> 8) & 0xFF;
	contact.publicKey[3] = nodeId & 0xFF;

	strlcpy(contact.advName, name, sizeof(contact.advName));
	contact.outPathLen = pathLen;
	contact.type = ADV_TYPE_CHAT;

	if (isSelf) {
		fGraphView->SetSelfNode(name, ADV_TYPE_CHAT, 20);
	} else {
		fGraphView->AddNode(contact);
	}
}


void
MeshGraphWindow::AddEdge(uint32 fromId, uint32 toId, int hops)
{
	(void)toId;  // Reserved for future use

	// Find node indices (fromId=0 means self node, which is at index 0)
	int32 fromIndex = (fromId == 0) ? 0 : -1;
	int32 toIndex = -1;

	// We'll let the view handle the edge creation
	fGraphView->AddEdge(fromIndex, toIndex, 0, hops);
}
