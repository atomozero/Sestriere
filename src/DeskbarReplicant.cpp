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
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <String.h>
#include <Window.h>

#include <cstdio>
#include <cstring>

// Inline constants to avoid pulling in the full app's headers and globals
// when loaded as a lightweight Deskbar add-on.
static const char* kAppSignature = "application/x-vnd.Sestriere";
static const uint32 kMsgSendAdvert = 'advt';

static inline int
_BatteryPercent(uint16 millivolts)
{
	// Simple LiPo range: 3000-4200mV
	if (millivolts <= 3000) return 0;
	if (millivolts >= 4200) return 100;
	return ((int)millivolts - 3000) * 100 / 1200;
}


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

	// Recreate icons at actual size (Deskbar may have resized us)
	float newHeight = Bounds().Height();
	float iconHeight = fIconConnected != NULL
		? fIconConnected->Bounds().Height() : 0;
	if (newHeight != iconHeight) {
		delete fIconConnected;
		delete fIconDisconnected;
		fIconConnected = _CreateIcon(true);
		fIconDisconnected = _CreateIcon(false);
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
		be_roster->Launch(kAppSignature);
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
				int level = _BatteryPercent(mV);
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

	BMenuItem* advertItem = new BMenuItem("Send Advert",
		new BMessage('advt'));
	advertItem->SetEnabled(fConnected);
	menu->AddItem(advertItem);

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
				be_roster->Launch(kAppSignature);
				break;

			case 'advt':
			{
				BMessenger appMessenger(kAppSignature);
				if (appMessenger.IsValid())
					appMessenger.SendMessage(kMsgSendAdvert);
				break;
			}

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


// HVIF antenna icon data
static const uint8 kIconAntenna[] = {
	0x6e, 0x63, 0x69, 0x66, 0x06, 0x05, 0x01, 0x02, 0x01, 0x06, 0x02, 0x3a,
	0xc1, 0x3b, 0x37, 0x6e, 0x9e, 0xba, 0x3a, 0xf5, 0x3d, 0x94, 0x92, 0x48,
	0xbc, 0x57, 0x49, 0x23, 0xf2, 0x00, 0xf0, 0xf7, 0xfd, 0xff, 0xae, 0xbf,
	0xd0, 0x02, 0x01, 0x06, 0x02, 0x3a, 0x77, 0xec, 0xb5, 0x2a, 0xd8, 0xb7,
	0xcd, 0x04, 0xbc, 0xf6, 0x4e, 0x4a, 0x4f, 0x6a, 0x4a, 0x7a, 0x3f, 0x00,
	0x8f, 0xa0, 0xb1, 0xff, 0x67, 0x77, 0x88, 0x02, 0x01, 0x06, 0x04, 0x3d,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 0x49,
	0x40, 0x00, 0x49, 0x40, 0x00, 0x00, 0xff, 0xff, 0xff, 0x9c, 0x8e, 0xa5,
	0xbc, 0xd0, 0x67, 0x77, 0x88, 0xff, 0x7e, 0x92, 0xa6, 0x02, 0x03, 0x02,
	0x03, 0x00, 0x00, 0x00, 0xba, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x4a, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x5e, 0xff, 0x05, 0x05, 0x00,
	0x7c, 0xff, 0x06, 0x06, 0xff, 0xd5, 0xff, 0x06, 0x06, 0x00, 0x04, 0x00,
	0x72, 0x08, 0x08, 0x03, 0x25, 0x4f, 0x3a, 0x30, 0x3e, 0x5b, 0x08, 0x06,
	0x2b, 0x47, 0x3d, 0x51, 0x31, 0x40, 0x3c, 0x45, 0x35, 0x39, 0x3b, 0x3a,
	0x08, 0x06, 0x3c, 0x3a, 0x3e, 0x39, 0x3e, 0x45, 0x43, 0x42, 0x3f, 0x50,
	0x46, 0x4a, 0x08, 0x02, 0x3a, 0x30, 0x4b, 0x4f, 0x08, 0x03, 0x25, 0x51,
	0x56, 0x58, 0x3e, 0x5d, 0x08, 0x03, 0x3d, 0x4c, 0x56, 0x58, 0x4c, 0x51,
	0x02, 0x02, 0x5b, 0x57, 0x5f, 0x57, 0x57, 0x57, 0x5b, 0x5b, 0x57, 0x5b,
	0x5f, 0x5b, 0x02, 0x04, 0x40, 0x2e, 0x4a, 0x2e, 0x36, 0x2e, 0x2e, 0x40,
	0x2e, 0x36, 0x2e, 0x4a, 0x40, 0x52, 0x36, 0x52, 0x4a, 0x52, 0x52, 0x40,
	0x52, 0x4a, 0x52, 0x36, 0x0b, 0x0a, 0x05, 0x03, 0x04, 0x05, 0x06, 0x10,
	0x01, 0x17, 0x84, 0x22, 0x04, 0x0a, 0x00, 0x04, 0x00, 0x01, 0x02, 0x03,
	0x18, 0x00, 0x15, 0x01, 0x17, 0x88, 0x22, 0x04, 0x0a, 0x00, 0x04, 0x00,
	0x01, 0x02, 0x03, 0x18, 0x15, 0xff, 0x01, 0x17, 0x86, 0x22, 0x04, 0x0a,
	0x02, 0x01, 0x03, 0x10, 0x01, 0x17, 0x82, 0x22, 0x04, 0x0a, 0x02, 0x01,
	0x02, 0x10, 0x01, 0x17, 0x82, 0x22, 0x04, 0x0a, 0x01, 0x01, 0x01, 0x10,
	0x01, 0x17, 0x82, 0x22, 0x04, 0x0a, 0x01, 0x01, 0x00, 0x10, 0x01, 0x17,
	0x82, 0x22, 0x04, 0x0a, 0x00, 0x01, 0x07, 0x0a, 0x3e, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x46, 0x80, 0x00, 0xc5,
	0x00, 0x00, 0x00, 0x15, 0x0a, 0x00, 0x01, 0x07, 0x0a, 0x3d, 0x8e, 0x38,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x8e, 0x38, 0x46, 0xf1, 0xc7,
	0xc4, 0x1c, 0x71, 0x15, 0xff, 0x0a, 0x03, 0x01, 0x07, 0x02, 0x3c, 0xaa,
	0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0xaa, 0xaa, 0x47, 0xd5,
	0x55, 0xbe, 0xaa, 0xaa, 0x0a, 0x04, 0x01, 0x07, 0x30, 0x1a, 0x0a, 0x04,
	0x15, 0x8c, 0x00, 0x04, 0x17, 0x88, 0x00, 0x04, 0x17, 0x88, 0x00, 0x04,
	0x17, 0x84, 0x00, 0x04
};


BBitmap*
DeskbarReplicant::_CreateIcon(bool connected)
{
	float size = Bounds().Height();
	if (size < 1)
		size = 15;
	BRect rect(0, 0, size, size);
	BBitmap* bitmap = new BBitmap(rect, B_RGBA32);

	status_t status = BIconUtils::GetVectorIcon(kIconAntenna,
		sizeof(kIconAntenna), bitmap);

	if (status != B_OK) {
		// Fallback: simple circle
		uint32* bits = (uint32*)bitmap->Bits();
		memset(bits, 0, bitmap->BitsLength());
		return bitmap;
	}

	// For disconnected state, desaturate the icon
	if (!connected) {
		uint8* bits = (uint8*)bitmap->Bits();
		int32 length = bitmap->BitsLength();
		for (int32 i = 0; i < length; i += 4) {
			// B_RGBA32: [B, G, R, A]
			uint8 b = bits[i];
			uint8 g = bits[i + 1];
			uint8 r = bits[i + 2];
			uint8 gray = (uint8)((r * 77 + g * 150 + b * 29) >> 8);
			bits[i] = gray;
			bits[i + 1] = gray;
			bits[i + 2] = gray;
			// Reduce alpha slightly for dimmed look
			bits[i + 3] = (uint8)(bits[i + 3] * 3 / 4);
		}
	}

	return bitmap;
}


// Exported function for Deskbar shelf instantiation
extern "C" _EXPORT BView* instantiate_deskbar_item(float maxWidth,
	float maxHeight)
{
	// Square icon sized to tray height
	return new DeskbarReplicant(
		BRect(0, 0, maxHeight - 1, maxHeight - 1),
		B_FOLLOW_LEFT | B_FOLLOW_TOP);
}
