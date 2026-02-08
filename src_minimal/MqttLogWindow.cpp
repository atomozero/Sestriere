/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttLogWindow.cpp — Rich MQTT debug log window
 */

#include "MqttLogWindow.h"

#include <Button.h>
#include <CheckBox.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "Constants.h"
#include "MqttClient.h"


static const uint32 kMsgClear = 'mclr';
static const uint32 kMsgFilterChanged = 'mflt';
static const uint32 kMsgSearchChanged = 'msrc';
static const uint32 kMsgAutoScrollToggle = 'masc';
static const uint32 kMsgRefreshOnShow = 'mrfs';

// Filter values: -1 = All, 0-4 = specific MqttLogType
static const int32 kFilterAll = -1;


MqttLogWindow::MqttLogWindow()
	:
	BWindow(BRect(200, 200, 750, 550), "MQTT Log",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fLogView(NULL),
	fScrollView(NULL),
	fFilterMenu(NULL),
	fFilterField(NULL),
	fSearchField(NULL),
	fAutoScroll(NULL),
	fStatusView(NULL),
	fMsgCountView(NULL),
	fErrCountView(NULL),
	fUptimeView(NULL),
	fClearButton(NULL),
	fEntries(20),
	fCurrentFilter(kFilterAll),
	fMsgCount(0),
	fErrCount(0),
	fConnectTime(0),
	fIsConnected(false)
{
	// Filter popup menu
	fFilterMenu = new BPopUpMenu("filter");
	BMessage* allMsg = new BMessage(kMsgFilterChanged);
	allMsg->AddInt32("filter", kFilterAll);
	fFilterMenu->AddItem(new BMenuItem("All", allMsg));

	BMessage* connMsg = new BMessage(kMsgFilterChanged);
	connMsg->AddInt32("filter", MQTT_LOG_CONN);
	fFilterMenu->AddItem(new BMenuItem("Connect", connMsg));

	BMessage* pubMsg = new BMessage(kMsgFilterChanged);
	pubMsg->AddInt32("filter", MQTT_LOG_PUB);
	fFilterMenu->AddItem(new BMenuItem("Publish", pubMsg));

	BMessage* errMsg = new BMessage(kMsgFilterChanged);
	errMsg->AddInt32("filter", MQTT_LOG_ERR);
	fFilterMenu->AddItem(new BMenuItem("Error", errMsg));

	BMessage* reconnMsg = new BMessage(kMsgFilterChanged);
	reconnMsg->AddInt32("filter", MQTT_LOG_RECONN);
	fFilterMenu->AddItem(new BMenuItem("Reconnect", reconnMsg));

	fFilterMenu->ItemAt(0)->SetMarked(true);
	fFilterField = new BMenuField("filter_field", NULL, fFilterMenu);
	fFilterField->SetExplicitMaxSize(BSize(110, B_SIZE_UNSET));

	// Search field
	fSearchField = new BTextControl("search", NULL, NULL,
		new BMessage(kMsgSearchChanged));
	fSearchField->SetModificationMessage(new BMessage(kMsgSearchChanged));
	fSearchField->TextView()->SetExplicitMinSize(BSize(120, B_SIZE_UNSET));

	// Auto-scroll checkbox
	fAutoScroll = new BCheckBox("autoscroll", "Auto-scroll",
		new BMessage(kMsgAutoScrollToggle));
	fAutoScroll->SetValue(B_CONTROL_ON);

	// Log text view — stylable, monospace
	fLogView = new BTextView("mqtt_log");
	fLogView->MakeEditable(false);
	fLogView->MakeSelectable(true);
	fLogView->SetStylable(true);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);
	fLogView->SetFontAndColor(&monoFont);

	fScrollView = new BScrollView("mqtt_log_scroll",
		fLogView, B_WILL_DRAW | B_FRAME_EVENTS, false, true);

	// Status bar views
	fStatusView = new BStringView("status", "Disconnected");
	fMsgCountView = new BStringView("msg_count", "Msgs: 0");
	fErrCountView = new BStringView("err_count", "Errs: 0");
	fUptimeView = new BStringView("uptime", "");

	fClearButton = new BButton("clear", "Clear",
		new BMessage(kMsgClear));

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		// Toolbar row
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING,
				B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
			.Add(fFilterField)
			.Add(fSearchField, 1.0)
			.Add(fAutoScroll)
		.End()
		// Log area
		.Add(fScrollView, 1.0)
		// Status bar
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_SMALL_SPACING, 2, B_USE_SMALL_SPACING, 2)
			.Add(fStatusView)
			.AddStrut(8)
			.Add(fMsgCountView)
			.AddStrut(8)
			.Add(fErrCountView)
			.AddStrut(8)
			.Add(fUptimeView)
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
			fEntries.MakeEmpty();
			fMsgCount = 0;
			fErrCount = 0;
			fLogView->SetText("");
			_UpdateStatusBar();
			break;

		case kMsgFilterChanged:
		{
			int32 filter;
			if (message->FindInt32("filter", &filter) == B_OK) {
				fCurrentFilter = filter;
				_RebuildLog();
			}
			break;
		}

		case kMsgSearchChanged:
			_RebuildLog();
			break;

		case kMsgAutoScrollToggle:
			break;

		case kMsgRefreshOnShow:
			// Sync views with internal state after window is fully visible
			if (fIsConnected) {
				fStatusView->SetText("Connected");
				fStatusView->SetHighUIColor(B_SUCCESS_COLOR);
			} else {
				fStatusView->SetText("Disconnected");
				fStatusView->SetHighUIColor(B_FAILURE_COLOR);
			}
			_RebuildLog();
			_UpdateStatusBar();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
MqttLogWindow::Show()
{
	BWindow::Show();

	// Defer UI rebuild — BWindow::Show() starts the looper; doing
	// heavy view operations while an external thread holds our lock
	// can deadlock.  PostMessage queues work for after unlock.
	PostMessage(kMsgRefreshOnShow);
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
	// Always update internal state
	fIsConnected = connected;
	SetTitle(connected ? "MQTT Log \xe2\x80\x94 Connected"
		: "MQTT Log \xe2\x80\x94 Disconnected");

	if (connected)
		fConnectTime = time(NULL);
	else
		fConnectTime = 0;

	// Skip view updates if window was never shown
	if (IsHidden())
		return;

	if (connected) {
		fStatusView->SetText("Connected");
		fStatusView->SetHighUIColor(B_SUCCESS_COLOR);
	} else {
		fStatusView->SetText("Disconnected");
		fStatusView->SetHighUIColor(B_FAILURE_COLOR);
	}

	_UpdateStatusBar();
}


void
MqttLogWindow::AddLogEntry(int32 type, const char* text)
{
	if (text == NULL)
		return;

	time_t now = time(NULL);
	MqttLogEntry* entry = new MqttLogEntry(type, now, text);
	fEntries.AddItem(entry);

	// Update counters
	fMsgCount++;
	if (type == MQTT_LOG_ERR)
		fErrCount++;

	// Prune if over limit
	_PruneEntries();

	// Skip UI operations if window was never shown — views aren't laid out yet
	if (IsHidden())
		return;

	// Only append to view if it matches the current filter
	if (_MatchesFilter(entry))
		_AppendStyledEntry(entry);

	_UpdateStatusBar();
}


void
MqttLogWindow::_AppendStyledEntry(MqttLogEntry* entry)
{
	struct tm tmBuf;
	localtime_r(&entry->timestamp, &tmBuf);

	char timestamp[16];
	snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] ",
		tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

	const char* tag = _TagForType(entry->type);

	// Build the full line: "[HH:MM:SS] TAG    text\n"
	BString line;
	line << timestamp << tag << "  " << entry->text << "\n";

	// Build text_run_array on the stack (safe for BFont copy-construction)
	// 3 runs: timestamp (default), tag (colored), text (default)
	struct {
		uint32		count;
		text_run	runs[3];
	} runBuffer;

	int32 tagStart = strlen(timestamp);
	int32 tagLen = strlen(tag);
	int32 textStart = tagStart + tagLen + 2;

	rgb_color tagColor = _ColorForType(entry->type);
	rgb_color defaultColor = ui_color(B_DOCUMENT_TEXT_COLOR);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);

	runBuffer.count = 3;

	// Timestamp run
	runBuffer.runs[0].offset = 0;
	runBuffer.runs[0].font = monoFont;
	runBuffer.runs[0].color = defaultColor;

	// Tag run (colored)
	runBuffer.runs[1].offset = tagStart;
	runBuffer.runs[1].font = monoFont;
	runBuffer.runs[1].color = tagColor;

	// Text run (back to default)
	runBuffer.runs[2].offset = textStart;
	runBuffer.runs[2].font = monoFont;
	runBuffer.runs[2].color = defaultColor;

	int32 insertOffset = fLogView->TextLength();
	fLogView->Insert(insertOffset, line.String(), line.Length(),
		(text_run_array*)&runBuffer);

	// Auto-scroll if enabled
	if (fAutoScroll->Value() == B_CONTROL_ON)
		fLogView->ScrollToOffset(fLogView->TextLength());
}


void
MqttLogWindow::_RebuildLog()
{
	fLogView->SetText("");

	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		MqttLogEntry* entry = fEntries.ItemAt(i);
		if (_MatchesFilter(entry))
			_AppendStyledEntry(entry);
	}
}


bool
MqttLogWindow::_MatchesFilter(MqttLogEntry* entry)
{
	// Check type filter
	if (fCurrentFilter != kFilterAll && entry->type != fCurrentFilter)
		return false;

	// Check search text
	const char* searchText = fSearchField->Text();
	if (searchText != NULL && searchText[0] != '\0') {
		BString entryText(entry->text);
		BString search(searchText);
		entryText.ToLower();
		search.ToLower();
		if (entryText.FindFirst(search) < 0)
			return false;
	}

	return true;
}


void
MqttLogWindow::_UpdateStatusBar()
{
	BString msgStr;
	msgStr.SetToFormat("Msgs: %d", (int)fMsgCount);
	fMsgCountView->SetText(msgStr.String());

	BString errStr;
	errStr.SetToFormat("Errs: %d", (int)fErrCount);
	fErrCountView->SetText(errStr.String());

	if (fIsConnected && fConnectTime > 0) {
		time_t now = time(NULL);
		int32 elapsed = (int32)(now - fConnectTime);
		int32 hours = elapsed / 3600;
		int32 mins = (elapsed % 3600) / 60;
		int32 secs = elapsed % 60;

		BString uptimeStr;
		if (hours > 0)
			uptimeStr.SetToFormat("Uptime: %dh %dm", (int)hours, (int)mins);
		else if (mins > 0)
			uptimeStr.SetToFormat("Uptime: %dm %ds", (int)mins, (int)secs);
		else
			uptimeStr.SetToFormat("Uptime: %ds", (int)secs);
		fUptimeView->SetText(uptimeStr.String());
	} else {
		fUptimeView->SetText("");
	}
}


rgb_color
MqttLogWindow::_ColorForType(int32 type)
{
	switch (type) {
		case MQTT_LOG_CONN:
			return ui_color(B_SUCCESS_COLOR);
		case MQTT_LOG_PUB:
			return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		case MQTT_LOG_ERR:
			return ui_color(B_FAILURE_COLOR);
		case MQTT_LOG_RECONN:
		{
			rgb_color base = {180, 140, 0, 255};
			return base;
		}
		case MQTT_LOG_INFO:
		default:
			return ui_color(B_DOCUMENT_TEXT_COLOR);
	}
}


const char*
MqttLogWindow::_TagForType(int32 type)
{
	switch (type) {
		case MQTT_LOG_CONN:		return "CONN  ";
		case MQTT_LOG_PUB:		return "PUB   ";
		case MQTT_LOG_ERR:		return "ERR   ";
		case MQTT_LOG_RECONN:	return "RECONN";
		case MQTT_LOG_INFO:		return "INFO  ";
		default:				return "      ";
	}
}


void
MqttLogWindow::_PruneEntries()
{
	while (fEntries.CountItems() > kMaxEntries) {
		delete fEntries.RemoveItemAt(0);
	}
}
