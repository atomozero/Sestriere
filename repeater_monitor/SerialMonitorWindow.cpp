/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SerialMonitorWindow.cpp — Terminal-style serial monitor for repeater CLI
 */

#include "SerialMonitorWindow.h"

#include <Button.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <Looper.h>
#include <Messenger.h>
#include <Path.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TextView.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "RepMonConstants.h"


static const uint32 kMsgSend = 'send';
static const uint32 kMsgSave = 'save';
static const uint32 kMsgSaveRef = 'svrf';
static const uint32 kMsgClear = 'sclr';


SerialMonitorWindow::SerialMonitorWindow(BHandler* target)
	:
	BWindow(BRect(0, 0, 599, 399),
		"Serial Monitor \xe2\x80\x94 Repeater Monitor",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fOutputView(NULL),
	fScrollView(NULL),
	fInputField(NULL),
	fSendButton(NULL),
	fSaveButton(NULL),
	fClearButton(NULL),
	fSavePanel(NULL),
	fTarget(target)
{
	// Output text view — monospace, read-only
	fOutputView = new BTextView("serial_output");
	fOutputView->MakeEditable(false);
	fOutputView->MakeSelectable(true);
	fOutputView->SetStylable(false);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(12);
	fOutputView->SetFontAndColor(&monoFont);

	fScrollView = new BScrollView("serial_scroll",
		fOutputView, B_WILL_DRAW | B_FRAME_EVENTS, false, true);

	// Input field with ">" prompt
	fInputField = new BTextControl("input", ">", "",
		new BMessage(kMsgSend));

	BFont inputFont(be_fixed_font);
	inputFont.SetSize(12);
	fInputField->TextView()->SetFontAndColor(&inputFont);

	// Buttons
	fSendButton = new BButton("send", "Send",
		new BMessage(kMsgSend));
	fSaveButton = new BButton("save", "Save Log",
		new BMessage(kMsgSave));
	fClearButton = new BButton("clear", "Clear",
		new BMessage(kMsgClear));

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		// Output area
		.Add(fScrollView, 1.0)
		// Input row
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING,
				B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
			.Add(fInputField, 1.0)
			.Add(fSendButton)
			.Add(fSaveButton)
			.Add(fClearButton)
		.End()
	.End();

	// Make Send the default button (Enter to send)
	SetDefaultButton(fSendButton);

	CenterOnScreen();
}


SerialMonitorWindow::~SerialMonitorWindow()
{
	delete fSavePanel;
}


void
SerialMonitorWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSend:
			_SendCommand();
			break;

		case kMsgSave:
			_SaveLog();
			break;

		case kMsgSaveRef:
		{
			entry_ref dirRef;
			BString name;
			if (message->FindRef("directory", &dirRef) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BPath dirPath(&dirRef);
				BPath filePath(dirPath.Path(), name.String());

				BFile file(filePath.Path(),
					B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
				if (file.InitCheck() == B_OK) {
					const char* text = fOutputView->Text();
					int32 len = fOutputView->TextLength();
					file.Write(text, len);

					BString msg;
					msg.SetToFormat("--- Log saved to %s (%d bytes) ---",
						filePath.Path(), (int)len);
					AppendOutput(msg.String());
				}
			}
			break;
		}

		case kMsgClear:
			fOutputView->SetText("");
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
SerialMonitorWindow::QuitRequested()
{
	Hide();
	return false;
}


void
SerialMonitorWindow::AppendOutput(const char* text)
{
	if (text == NULL)
		return;

	// Build timestamped line
	time_t now = time(NULL);
	struct tm tmBuf;
	localtime_r(&now, &tmBuf);

	char timestamp[16];
	snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] ",
		tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

	BString line;
	line << timestamp << text << "\n";

	int32 textLen = fOutputView->TextLength();
	fOutputView->Insert(textLen, line.String(), line.Length());

	// Auto-scroll to bottom
	fOutputView->ScrollToOffset(fOutputView->TextLength());

	// Prune if too large
	_PruneOutput();
}


void
SerialMonitorWindow::_SendCommand()
{
	const char* text = fInputField->Text();
	if (text == NULL || text[0] == '\0')
		return;

	// Local echo
	BString echo;
	echo << "> " << text;
	AppendOutput(echo.String());

	// Forward to MainWindow which routes to SerialHandler
	if (fTarget != NULL) {
		BMessage msg(MSG_SERIAL_SEND_RAW);
		msg.AddString("text", text);

		BLooper* looper = fTarget->Looper();
		if (looper != NULL)
			looper->PostMessage(&msg, fTarget);
	}

	// Clear input
	fInputField->SetText("");
	fInputField->MakeFocus(true);
}


void
SerialMonitorWindow::_SaveLog()
{
	if (fSavePanel == NULL) {
		BMessage* msg = new BMessage(kMsgSaveRef);
		fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this),
			NULL, 0, false, msg);
		fSavePanel->SetButtonLabel(B_DEFAULT_BUTTON, "Save");
	}

	// Generate default filename with timestamp
	time_t now = time(NULL);
	struct tm tmBuf;
	localtime_r(&now, &tmBuf);

	char filename[64];
	snprintf(filename, sizeof(filename),
		"serial_log_%04d%02d%02d_%02d%02d%02d.txt",
		tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
		tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

	fSavePanel->SetSaveText(filename);
	fSavePanel->Show();
}


void
SerialMonitorWindow::_PruneOutput()
{
	int32 textLen = fOutputView->TextLength();
	if (textLen > kMaxOutputSize) {
		// Remove first quarter of text
		int32 removeLen = textLen / 4;

		// Find a newline near the cut point to avoid splitting a line
		const char* text = fOutputView->Text();
		for (int32 i = removeLen; i < textLen && i < removeLen + 256; i++) {
			if (text[i] == '\n') {
				removeLen = i + 1;
				break;
			}
		}

		fOutputView->Delete(0, removeLen);
	}
}
