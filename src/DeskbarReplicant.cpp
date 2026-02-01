/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DeskbarReplicant.cpp — Deskbar tray icon replicant implementation
 */

#include "DeskbarReplicant.h"

#include <Bitmap.h>
#include <Deskbar.h>
#include <IconUtils.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Resources.h>
#include <Roster.h>
#include <Window.h>

#include <cstdio>

#include "Constants.h"


// Replicant signature
static const char* kReplicantSignature = "application/x-vnd.Sestriere-DeskbarReplicant";

// Simple mesh icon (16x16 RGBA)
static const uint8 kIconConnectedData[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0xC5, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0xC5, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x82, 0xC5, 0xFF, 0x00, 0x82, 0xC5, 0xFF,
	0x00, 0x82, 0xC5, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8 kIconDisconnectedData[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x99, 0x99, 0x99, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x99, 0x99, 0x99, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x99, 0x99, 0x99, 0xFF, 0x99, 0x99, 0x99, 0xFF,
	0x99, 0x99, 0x99, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


DeskbarReplicant::DeskbarReplicant(BRect frame, int32 resizingMode)
	:
	BView(frame, "Sestriere", resizingMode,
		B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fConnected(false),
	fBatteryLevel(0),
	fUnreadCount(0),
	fIconConnected(NULL),
	fIconDisconnected(NULL)
{
	_Init();
}


DeskbarReplicant::DeskbarReplicant(BMessage* archive)
	:
	BView(archive),
	fConnected(false),
	fBatteryLevel(0),
	fUnreadCount(0),
	fIconConnected(NULL),
	fIconDisconnected(NULL)
{
	_Init();
}


DeskbarReplicant::~DeskbarReplicant()
{
	delete fIconConnected;
	delete fIconDisconnected;
}


DeskbarReplicant*
DeskbarReplicant::Instantiate(BMessage* archive)
{
	if (!validate_instantiation(archive, "DeskbarReplicant"))
		return NULL;

	return new DeskbarReplicant(archive);
}


status_t
DeskbarReplicant::Archive(BMessage* archive, bool deep) const
{
	status_t status = BView::Archive(archive, deep);
	if (status != B_OK)
		return status;

	status = archive->AddString("class", "DeskbarReplicant");
	if (status != B_OK)
		return status;

	status = archive->AddString("add_on", kAppSignature);
	if (status != B_OK)
		return status;

	return B_OK;
}


void
DeskbarReplicant::AttachedToWindow()
{
	BView::AttachedToWindow();

	if (Parent() != NULL) {
		SetViewColor(Parent()->ViewColor());
		SetLowColor(Parent()->ViewColor());
	}
}


void
DeskbarReplicant::DetachedFromWindow()
{
	BView::DetachedFromWindow();
}


void
DeskbarReplicant::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	_DrawIcon(bounds);

	// Draw unread count badge if any
	if (fUnreadCount > 0) {
		SetHighColor(255, 0, 0);
		BRect badge(bounds.right - 6, bounds.top, bounds.right, bounds.top + 6);
		FillEllipse(badge);

		SetHighColor(255, 255, 255);
		char countStr[4];
		if (fUnreadCount > 9)
			snprintf(countStr, sizeof(countStr), "+");
		else
			snprintf(countStr, sizeof(countStr), "%d", (int)fUnreadCount);

		BFont font;
		GetFont(&font);
		font.SetSize(8);
		SetFont(&font);
		DrawString(countStr, BPoint(bounds.right - 5, bounds.top + 5));
	}
}


void
DeskbarReplicant::MouseDown(BPoint where)
{
	BPoint screenWhere = ConvertToScreen(where);

	int32 buttons;
	if (Window()->CurrentMessage()->FindInt32("buttons", &buttons) != B_OK)
		buttons = B_PRIMARY_MOUSE_BUTTON;

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		_ShowPopupMenu(screenWhere);
	} else {
		// Launch or bring to front
		be_roster->Launch(kAppSignature);
	}
}


void
DeskbarReplicant::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_SERIAL_CONNECTED:
			SetConnected(true);
			break;

		case MSG_SERIAL_DISCONNECTED:
			SetConnected(false);
			break;

		case MSG_BATTERY_RECEIVED:
		{
			uint16 mV;
			if (message->FindUInt16(kFieldBattery, &mV) == B_OK) {
				// Convert mV to percentage (rough estimate)
				// 3.0V = 0%, 4.2V = 100%
				int level = (mV - 3000) * 100 / 1200;
				if (level < 0) level = 0;
				if (level > 100) level = 100;
				SetBatteryLevel(level);
			}
			break;
		}

		case MSG_MESSAGE_RECEIVED:
			SetUnreadCount(fUnreadCount + 1);
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
DeskbarReplicant::SetConnected(bool connected)
{
	if (fConnected == connected)
		return;

	fConnected = connected;
	Invalidate();
}


void
DeskbarReplicant::SetBatteryLevel(uint8 level)
{
	if (fBatteryLevel == level)
		return;

	fBatteryLevel = level;
	Invalidate();
}


void
DeskbarReplicant::SetUnreadCount(int32 count)
{
	if (fUnreadCount == count)
		return;

	fUnreadCount = count;
	Invalidate();
}


void
DeskbarReplicant::_Init()
{
	fIconConnected = _CreateIcon(true);
	fIconDisconnected = _CreateIcon(false);
}


void
DeskbarReplicant::_DrawIcon(BRect bounds)
{
	BBitmap* icon = fConnected ? fIconConnected : fIconDisconnected;
	if (icon != NULL) {
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(icon, bounds);
		SetDrawingMode(B_OP_COPY);
	} else {
		// Fallback: draw a simple circle
		if (fConnected)
			SetHighColor(0, 150, 0);
		else
			SetHighColor(150, 150, 150);

		BRect circle = bounds.InsetByCopy(2, 2);
		FillEllipse(circle);
	}
}


void
DeskbarReplicant::_ShowPopupMenu(BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("DeskbarMenu", false, false);

	BString statusStr;
	if (fConnected) {
		statusStr = "Connected";
		if (fBatteryLevel > 0)
			statusStr.Append(" - Battery: ").Append(
				BString().SetToFormat("%d%%", fBatteryLevel));
	} else {
		statusStr = "Disconnected";
	}

	menu->AddItem(new BMenuItem(statusStr.String(), NULL));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Open Sestriere",
		new BMessage('open')));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Remove from Deskbar",
		new BMessage('rmdb')));

	menu->SetTargetForItems(this);

	BMenuItem* selected = menu->Go(where, false, true,
		BRect(where.x - 2, where.y - 2, where.x + 2, where.y + 2));

	if (selected != NULL) {
		switch (selected->Message()->what) {
			case 'open':
				be_roster->Launch(kAppSignature);
				break;

			case 'rmdb':
			{
				BDeskbar deskbar;
				deskbar.RemoveItem("Sestriere");
				break;
			}
		}
	}

	delete menu;
}


BBitmap*
DeskbarReplicant::_CreateIcon(bool connected)
{
	BRect rect(0, 0, 15, 15);
	BBitmap* bitmap = new BBitmap(rect, B_RGBA32);

	// Create a simple mesh network icon
	uint32* bits = (uint32*)bitmap->Bits();
	int32 bpr = bitmap->BytesPerRow() / 4;

	// Clear to transparent
	memset(bits, 0, bitmap->BitsLength());

	// Draw node circles
	uint32 nodeColor = connected ? 0xFF82C5FF : 0xFF999999;
	uint32 lineColor = connected ? 0xFF659EC4 : 0xFF777777;

	// Helper to set a pixel
	auto setPixel = [bits, bpr](int x, int y, uint32 color) {
		if (x >= 0 && x < 16 && y >= 0 && y < 16)
			bits[y * bpr + x] = color;
	};

	// Draw connecting lines
	for (int i = 4; i <= 11; i++) {
		setPixel(i, 7, lineColor);  // horizontal
		setPixel(7, i, lineColor);  // vertical
	}

	// Draw nodes (3x3 circles)
	auto drawNode = [setPixel, nodeColor](int cx, int cy) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				if (dx == 0 || dy == 0)
					setPixel(cx + dx, cy + dy, nodeColor);
			}
		}
	};

	drawNode(3, 3);    // top-left
	drawNode(12, 3);   // top-right
	drawNode(7, 7);    // center
	drawNode(3, 12);   // bottom-left
	drawNode(12, 12);  // bottom-right

	return bitmap;
}


// Function to install replicant in Deskbar
extern "C" _EXPORT BView* instantiate_deskbar_item()
{
	return new DeskbarReplicant(BRect(0, 0, 15, 15),
		B_FOLLOW_LEFT | B_FOLLOW_TOP);
}
