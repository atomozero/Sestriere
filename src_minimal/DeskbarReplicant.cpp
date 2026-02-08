/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DeskbarReplicant.cpp — Deskbar tray icon replicant implementation
 */

#include "DeskbarReplicant.h"

#include <Bitmap.h>
#include <Deskbar.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <String.h>
#include <Window.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"


// Messages from the app to update replicant state
static const uint32 kMsgReplicantSetConnected	= 'rpco';
static const uint32 kMsgReplicantSetBattery		= 'rpba';
static const uint32 kMsgReplicantSetUnread		= 'rpun';


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

	status = archive->AddString("add_on", APP_SIGNATURE);
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
DeskbarReplicant::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();
	_DrawIcon(bounds);

	// Draw unread count badge
	if (fUnreadCount > 0) {
		SetHighColor(220, 40, 40);
		BRect badge(bounds.right - 7, bounds.top,
			bounds.right, bounds.top + 7);
		FillEllipse(badge);

		SetHighColor(255, 255, 255);
		char countStr[4];
		if (fUnreadCount > 9)
			strlcpy(countStr, "+", sizeof(countStr));
		else
			snprintf(countStr, sizeof(countStr), "%d",
				(int)(fUnreadCount & 0xF));

		BFont font;
		GetFont(&font);
		font.SetSize(7);
		SetFont(&font);
		DrawString(countStr,
			BPoint(bounds.right - 5, bounds.top + 6));
	}
}


void
DeskbarReplicant::MouseDown(BPoint where)
{
	BPoint screenWhere = ConvertToScreen(where);

	int32 buttons = B_PRIMARY_MOUSE_BUTTON;
	if (Window() != NULL && Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		_ShowPopupMenu(screenWhere);
	} else {
		// Launch or bring to front
		be_roster->Launch(APP_SIGNATURE);
	}
}


void
DeskbarReplicant::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgReplicantSetConnected:
		{
			bool connected;
			if (message->FindBool("connected", &connected) == B_OK)
				SetConnected(connected);
			break;
		}

		case kMsgReplicantSetBattery:
		{
			uint16 mV;
			if (message->FindUInt16("battery_mv", &mV) == B_OK) {
				// Convert mV to percentage (3.0V=0%, 4.2V=100%)
				int level = ((int)mV - 3000) * 100 / 1200;
				if (level < 0) level = 0;
				if (level > 100) level = 100;
				SetBatteryLevel((uint8)level);
			}
			break;
		}

		case kMsgReplicantSetUnread:
		{
			int32 count;
			if (message->FindInt32("count", &count) == B_OK)
				SetUnreadCount(count);
			break;
		}

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
		// Fallback: simple circle
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

	// Status line
	BString statusStr;
	if (fConnected) {
		statusStr = "Connected";
		if (fBatteryLevel > 0) {
			BString battStr;
			battStr.SetToFormat(" — Battery: %d%%", fBatteryLevel);
			statusStr.Append(battStr);
		}
	} else {
		statusStr = "Disconnected";
	}

	BMenuItem* statusItem = new BMenuItem(statusStr.String(), NULL);
	statusItem->SetEnabled(false);
	menu->AddItem(statusItem);

	menu->AddSeparatorItem();

	menu->AddItem(new BMenuItem("Open Sestriere",
		new BMessage('open')));

	menu->AddSeparatorItem();

	menu->AddItem(new BMenuItem("Remove from Deskbar",
		new BMessage('rmdb')));

	menu->SetTargetForItems(this);

	BMenuItem* selected = menu->Go(where, false, true,
		BRect(where.x - 2, where.y - 2,
			where.x + 2, where.y + 2));

	if (selected != NULL) {
		switch (selected->Message()->what) {
			case 'open':
				be_roster->Launch(APP_SIGNATURE);
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

	uint32* bits = (uint32*)bitmap->Bits();
	int32 bpr = bitmap->BytesPerRow() / 4;

	// Clear to transparent
	memset(bits, 0, bitmap->BitsLength());

	// Colors: teal for connected, gray for disconnected
	uint32 nodeColor = connected ? 0xFF00C8C8 : 0xFF999999;
	uint32 lineColor = connected ? 0xFF009090 : 0xFF777777;

	auto setPixel = [bits, bpr](int x, int y, uint32 color) {
		if (x >= 0 && x < 16 && y >= 0 && y < 16)
			bits[y * bpr + x] = color;
	};

	// Draw connecting lines (mesh edges)
	// Top-left to center
	for (int i = 4; i <= 7; i++) {
		setPixel(i, i, lineColor);
	}
	// Top-right to center
	for (int i = 0; i <= 3; i++) {
		setPixel(12 - i, 4 + i, lineColor);
	}
	// Center to bottom-left
	for (int i = 0; i <= 3; i++) {
		setPixel(7 - i, 8 + i, lineColor);
	}
	// Center to bottom-right
	for (int i = 0; i <= 3; i++) {
		setPixel(8 + i, 8 + i, lineColor);
	}
	// Horizontal line
	for (int i = 4; i <= 11; i++) {
		setPixel(i, 7, lineColor);
	}

	// Draw nodes (3x3 diamond shape)
	auto drawNode = [setPixel, nodeColor](int cx, int cy) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				if (dx == 0 || dy == 0)
					setPixel(cx + dx, cy + dy, nodeColor);
			}
		}
	};

	drawNode(3, 3);		// top-left
	drawNode(12, 3);	// top-right
	drawNode(7, 7);		// center
	drawNode(3, 12);	// bottom-left
	drawNode(12, 12);	// bottom-right

	return bitmap;
}


// Exported function for Deskbar shelf instantiation
extern "C" _EXPORT BView* instantiate_deskbar_item()
{
	return new DeskbarReplicant(BRect(0, 0, 15, 15),
		B_FOLLOW_LEFT | B_FOLLOW_TOP);
}
