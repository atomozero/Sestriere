/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatusBarView.cpp — Status bar implementation
 */

#include "StatusBarView.h"

#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <StringView.h>

#include <cstdio>

#include "Constants.h"


static const uint32 MSG_TEMP_STATUS_CLEAR = 'tscc';


StatusBarView::StatusBarView(const char* name)
	:
	BView(name, B_WILL_DRAW),
	fNodeNameView(NULL),
	fStatusView(NULL),
	fBatteryView(NULL),
	fRadioView(NULL),
	fNodeName(""),
	fConnectionStatus(TR_STATUS_DISCONNECTED),
	fBatteryInfo(""),
	fRadioInfo(""),
	fTempStatus(""),
	fTempStatusTimer(NULL)
{
	// Create child views
	fNodeNameView = new BStringView("node_name", "");
	fNodeNameView->SetFont(be_bold_font);

	fStatusView = new BStringView("status", TR_STATUS_DISCONNECTED);

	fBatteryView = new BStringView("battery", "");

	fRadioView = new BStringView("radio", "");

	// Set colors
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Layout
	BLayoutBuilder::Group<>(this, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(fNodeNameView)
		.AddGlue()
		.Add(fStatusView)
		.Add(fBatteryView)
		.Add(fRadioView)
	.End();
}


StatusBarView::~StatusBarView()
{
	delete fTempStatusTimer;
}


void
StatusBarView::AttachedToWindow()
{
	BView::AttachedToWindow();
}


void
StatusBarView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TEMP_STATUS_CLEAR:
			_ClearTemporaryStatus();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
StatusBarView::SetNodeName(const char* name)
{
	fNodeName = name;
	_UpdateDisplay();
}


void
StatusBarView::SetConnectionStatus(const char* status)
{
	fConnectionStatus = status;
	_UpdateDisplay();
}


void
StatusBarView::SetBatteryInfo(uint16 milliVolts)
{
	if (milliVolts > 0) {
		float volts = milliVolts / 1000.0f;
		fBatteryInfo.SetToFormat("\xF0\x9F\x94\x8B %.2fV", volts);  // Battery emoji
	} else {
		fBatteryInfo = "";
	}
	_UpdateDisplay();
}


void
StatusBarView::SetRadioInfo(const char* info)
{
	if (info != NULL && info[0] != '\0') {
		fRadioInfo.SetToFormat("\xF0\x9F\x93\xA1 %s", info);  // Antenna emoji
	} else {
		fRadioInfo = "";
	}
	_UpdateDisplay();
}


void
StatusBarView::SetTemporaryStatus(const char* status, bigtime_t duration)
{
	fTempStatus = status;
	_UpdateDisplay();

	// Clear any existing timer
	delete fTempStatusTimer;
	fTempStatusTimer = NULL;

	// Set new timer to clear temp status
	if (duration > 0) {
		BMessage clearMsg(MSG_TEMP_STATUS_CLEAR);
		fTempStatusTimer = new BMessageRunner(this, &clearMsg, duration, 1);
	}
}


void
StatusBarView::_UpdateDisplay()
{
	if (fNodeNameView != NULL)
		fNodeNameView->SetText(fNodeName.String());

	if (fStatusView != NULL) {
		if (fTempStatus.Length() > 0)
			fStatusView->SetText(fTempStatus.String());
		else
			fStatusView->SetText(fConnectionStatus.String());
	}

	if (fBatteryView != NULL)
		fBatteryView->SetText(fBatteryInfo.String());

	if (fRadioView != NULL)
		fRadioView->SetText(fRadioInfo.String());

	Invalidate();
}


void
StatusBarView::_ClearTemporaryStatus()
{
	fTempStatus = "";
	delete fTempStatusTimer;
	fTempStatusTimer = NULL;
	_UpdateDisplay();
}
