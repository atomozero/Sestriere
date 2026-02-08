/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttLogWindow.cpp — MQTT message log window
 */

#include "MqttLogWindow.h"

#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <TextView.h>

#include <cstdio>
#include <ctime>

#include "Constants.h"


static const uint32 kMsgClear = 'mclr';


MqttLogWindow::MqttLogWindow()
	:
	BWindow(BRect(200, 200, 600, 500), "MQTT Log",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fLogView(NULL),
	fClearButton(NULL)
{
	fLogView = new BTextView("mqtt_log");
	fLogView->MakeEditable(false);
	fLogView->MakeSelectable(true);
	fLogView->SetStylable(false);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);
	fLogView->SetFontAndColor(&monoFont);

	BScrollView* scrollView = new BScrollView("mqtt_log_scroll",
		fLogView, B_WILL_DRAW | B_FRAME_EVENTS, false, true);

	fClearButton = new BButton("clear", "Clear",
		new BMessage(kMsgClear));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(scrollView, 1.0)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING,
				B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(fClearButton)
		.End()
	.End();
}


MqttLogWindow::~MqttLogWindow()
{
}


void
MqttLogWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgClear:
			fLogView->SetText("");
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MqttLogWindow::QuitRequested()
{
	Hide();
	return false;
}


void
MqttLogWindow::SetMqttStatus(bool connected)
{
	SetTitle(connected ? "MQTT Log — Connected" : "MQTT Log — Disconnected");
}


void
MqttLogWindow::AddLogEntry(const char* entry)
{
	if (entry == NULL)
		return;

	// Build timestamped line
	time_t now = time(NULL);
	struct tm tmBuf;
	localtime_r(&now, &tmBuf);

	char timestamp[16];
	snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] ",
		tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

	BString line;
	line << timestamp << entry << "\n";

	// Append to text view
	fLogView->Insert(fLogView->TextLength(), line.String(), line.Length());

	// Auto-scroll to bottom
	fLogView->ScrollToOffset(fLogView->TextLength());

	// Prune if too many lines
	_PruneLines();
}


void
MqttLogWindow::_PruneLines()
{
	const char* text = fLogView->Text();
	int32 lineCount = 0;

	// Count lines
	for (const char* p = text; *p != '\0'; p++) {
		if (*p == '\n')
			lineCount++;
	}

	if (lineCount <= kMaxLines)
		return;

	// Find offset of line (lineCount - kMaxLines)
	int32 linesToRemove = lineCount - kMaxLines;
	int32 offset = 0;
	for (const char* p = text; *p != '\0'; p++) {
		if (*p == '\n') {
			linesToRemove--;
			if (linesToRemove <= 0) {
				offset = (p - text) + 1;
				break;
			}
		}
	}

	if (offset > 0) {
		fLogView->Delete(0, offset);
	}
}
