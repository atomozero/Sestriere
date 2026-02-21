/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DebugLogWindow.cpp — Separate window for debug log implementation
 */

#include "DebugLogWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <TextView.h>

#include <cstdio>
#include <ctime>

#include "Constants.h"


DebugLogWindow* DebugLogWindow::sInstance = NULL;


DebugLogWindow*
DebugLogWindow::Instance()
{
	if (sInstance == NULL) {
		BRect frame(150, 150, 750, 500);
		sInstance = new DebugLogWindow(frame);
	}
	return sInstance;
}


void
DebugLogWindow::ShowWindow()
{
	DebugLogWindow* window = Instance();
	if (window->IsHidden())
		window->Show();
	window->Activate();
}


void
DebugLogWindow::Destroy()
{
	if (sInstance != NULL) {
		sInstance->Lock();
		sInstance->Quit();
		sInstance = NULL;
	}
}


DebugLogWindow::DebugLogWindow(BRect frame)
	:
	BWindow(frame, "Debug Log - Sestriere",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fLogView(NULL),
	fClearButton(NULL)
{
	// Log view
	fLogView = new BTextView("log");
	fLogView->MakeEditable(false);
	fLogView->SetStylable(true);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(10);
	fLogView->SetFontAndColor(&monoFont);

	BScrollView* scrollView = new BScrollView("log_scroll", fLogView,
		B_WILL_DRAW | B_FRAME_EVENTS, false, true);

	// Clear button
	fClearButton = new BButton("clear", "Clear Log",
		new BMessage(MSG_CLEAR_LOG));

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_INSETS)
		.Add(scrollView, 1.0)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fClearButton)
		.End()
	.End();

	// Don't quit app when this window closes
	SetFlags(Flags() | B_CLOSE_ON_ESCAPE);
}


DebugLogWindow::~DebugLogWindow()
{
	sInstance = NULL;
}


void
DebugLogWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_CLEAR_LOG:
			Clear();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
DebugLogWindow::QuitRequested()
{
	// Just hide, don't actually quit
	Hide();
	return false;
}


void
DebugLogWindow::LogMessage(const char* prefix, const char* text)
{
	// Get timestamp
	time_t now = time(NULL);
	struct tm tmBuf;
	struct tm* tm = localtime_r(&now, &tmBuf);
	char timestamp[16];
	strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm);

	BString line;
	line.SetToFormat("[%s] %s: %s\n", timestamp, prefix, text);

	// Append to log (must lock window first)
	if (LockLooper()) {
		fLogView->Insert(fLogView->TextLength(), line.String(), line.Length());
		fLogView->ScrollToOffset(fLogView->TextLength());
		UnlockLooper();
	}
}


void
DebugLogWindow::LogHex(const char* prefix, const uint8* data, size_t length)
{
	BString hex = _FormatHex(data, length);
	const char* cmdName = (length > 0) ? _CommandName(data[0]) : "";

	BString line;
	line.SetToFormat("%s [%zu]%s: %s", prefix, length, cmdName, hex.String());
	LogMessage(prefix, line.String());
}


void
DebugLogWindow::Clear()
{
	if (LockLooper()) {
		fLogView->SetText("");
		UnlockLooper();
	}
}


BString
DebugLogWindow::_FormatHex(const uint8* data, size_t length)
{
	BString result;
	for (size_t i = 0; i < length && i < 48; i++) {
		char buf[4];
		snprintf(buf, sizeof(buf), "%02X ", data[i]);
		result << buf;
	}
	if (length > 48) {
		result << "...";
	}
	return result;
}


const char*
DebugLogWindow::_CommandName(uint8 cmd)
{
	// Inbound commands (App -> Radio)
	switch (cmd) {
		case 1: return " (APP_START)";
		case 2: return " (SEND_TXT_MSG)";
		case 3: return " (SEND_CHANNEL_TXT_MSG)";
		case 4: return " (GET_CONTACTS)";
		case 5: return " (GET_DEVICE_TIME)";
		case 6: return " (SET_DEVICE_TIME)";
		case 7: return " (SEND_SELF_ADVERT)";
		case 8: return " (SET_ADVERT_NAME)";
		case 9: return " (ADD_UPDATE_CONTACT)";
		case 10: return " (SYNC_NEXT_MESSAGE)";
		case 11: return " (SET_RADIO_PARAMS)";
		case 12: return " (SET_RADIO_TX_POWER)";
		case 13: return " (RESET_PATH)";
		case 14: return " (SET_ADVERT_LATLON)";
		case 20: return " (GET_BATT_AND_STORAGE)";
		case 22: return " (DEVICE_QUERY)";
		case 56: return " (GET_STATS)";
		default: break;
	}

	// Push notifications
	switch (cmd) {
		case 0x80: return " (PUSH_ADVERT)";
		case 0x81: return " (PUSH_PATH_UPDATED)";
		case 0x82: return " (PUSH_SEND_CONFIRMED)";
		case 0x83: return " (PUSH_MSG_WAITING)";
		case 0x8A: return " (PUSH_NEW_ADVERT)";
		case 0x8B: return " (PUSH_TELEMETRY)";
		default: return "";
	}
}
