/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MicIconView.cpp — Clickable microphone icon for voice recording
 */

#include "MicIconView.h"

#include <Font.h>
#include <Window.h>


MicIconView::MicIconView(BMessage* clickMsg)
	:
	BView("mic_icon", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fClickMsg(clickMsg),
	fEnabled(false),
	fRecording(false)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetToolTip("Record voice message");
	float em = be_plain_font->Size();
	SetExplicitSize(BSize(em * 3.0, em * 3.0));
}


MicIconView::~MicIconView()
{
	delete fClickMsg;
}


void
MicIconView::Draw(BRect updateRect)
{
	BRect r = Bounds();
	const char* icon = fRecording
		? "\xE2\x97\xBC"     // stop square
		: "\xF0\x9F\x8E\x99"; // microphone

	BFont font(be_plain_font);
	font.SetSize(be_plain_font->Size() * 2.0);
	SetFont(&font);

	if (fEnabled) {
		if (fRecording)
			SetHighColor(200, 40, 40);
		else
			SetHighUIColor(B_PANEL_TEXT_COLOR);
	} else {
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_1_TINT));
	}

	font_height fh;
	font.GetHeight(&fh);
	float tw = font.StringWidth(icon);
	float x = (r.Width() - tw) / 2.0;
	float y = (r.Height() + fh.ascent - fh.descent) / 2.0;
	DrawString(icon, BPoint(x, y));
}


void
MicIconView::MouseDown(BPoint where)
{
	if (fEnabled && fClickMsg != NULL && Window() != NULL)
		Window()->PostMessage(fClickMsg);
}


void
MicIconView::SetEnabled(bool enabled)
{
	if (fEnabled != enabled) {
		fEnabled = enabled;
		Invalidate();
	}
}


void
MicIconView::SetRecording(bool recording)
{
	if (fRecording != recording) {
		fRecording = recording;
		Invalidate();
	}
}
