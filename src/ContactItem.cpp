/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactItem.cpp — Custom list item for contacts implementation
 */

#include "ContactItem.h"

#include <View.h>

#include <cstring>


ContactItem::ContactItem(const Contact& contact)
	:
	BListItem(),
	fBaselineOffset(0),
	fIconSize(16)
{
	memcpy(&fContact, &contact, sizeof(Contact));
}


ContactItem::~ContactItem()
{
}


void
ContactItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	rgb_color lowColor = owner->LowColor();
	rgb_color highColor = owner->HighColor();

	// Background
	if (IsSelected()) {
		owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
		owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
	} else {
		owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
		owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	}

	owner->FillRect(frame, B_SOLID_LOW);

	// Draw type icon/indicator
	const char* icon = _GetTypeIcon();
	BPoint iconPos(frame.left + 4, frame.top + fBaselineOffset);
	owner->DrawString(icon, iconPos);

	// Draw name
	BPoint textPos(frame.left + fIconSize + 8, frame.top + fBaselineOffset);
	owner->DrawString(fContact.advName, textPos);

	// Draw path indicator
	if (HasPath()) {
		rgb_color pathColor;
		_GetStatusColor(pathColor);
		owner->SetHighColor(pathColor);

		BPoint statusPos(frame.right - 12, frame.top + fBaselineOffset);
		owner->DrawString("\xE2\x97\x8F", statusPos);  // Filled circle
	}

	// Restore colors
	owner->SetLowColor(lowColor);
	owner->SetHighColor(highColor);
}


void
ContactItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	font_height fontHeight;
	font->GetHeight(&fontHeight);

	fBaselineOffset = fontHeight.ascent + fontHeight.leading + 2;

	// Set item height
	float height = fontHeight.ascent + fontHeight.descent + fontHeight.leading + 4;
	SetHeight(height);

	fIconSize = fontHeight.ascent;
}


void
ContactItem::SetContact(const Contact& contact)
{
	memcpy(&fContact, &contact, sizeof(Contact));
}


const char*
ContactItem::_GetTypeIcon() const
{
	switch (fContact.type) {
		case ADV_TYPE_CHAT:
			return "\xF0\x9F\x91\xA4";  // Person silhouette
		case ADV_TYPE_REPEATER:
			return "\xF0\x9F\x93\xA1";  // Antenna
		case ADV_TYPE_ROOM:
			return "\xF0\x9F\x8F\xA0";  // House
		default:
			return "\xE2\x9D\x93";      // Question mark
	}
}


void
ContactItem::_GetStatusColor(rgb_color& outColor) const
{
	if (fContact.outPathLen < 0) {
		// No path - gray
		outColor.red = 128;
		outColor.green = 128;
		outColor.blue = 128;
	} else if (fContact.outPathLen == 0) {
		// Direct path - green
		outColor.red = 0;
		outColor.green = 200;
		outColor.blue = 0;
	} else if (fContact.outPathLen <= 2) {
		// Short path - yellow
		outColor.red = 200;
		outColor.green = 200;
		outColor.blue = 0;
	} else {
		// Long path - orange
		outColor.red = 255;
		outColor.green = 128;
		outColor.blue = 0;
	}
	outColor.alpha = 255;
}
