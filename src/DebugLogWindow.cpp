/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DebugLogWindow.cpp — Separate window for debug log implementation
 */

#include "DebugLogWindow.h"

#include <Button.h>
#include <InterfaceDefs.h>
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
		BRect frame(0, 0, 599, 349);
		sInstance = new DebugLogWindow(frame);
	}
	return sInstance;
}


void
DebugLogWindow::ShowWindow()
{
	DebugLogWindow* window = Instance();
	if (window->LockLooper()) {
		if (window->IsHidden())
			window->Show();
		window->Activate();
		window->UnlockLooper();
	}
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

	CenterOnScreen();
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


// Color mapping for MeshCore protocol categories
static rgb_color
_ColorForPrefix(const char* prefix)
{
	if (prefix == NULL)
		return ui_color(B_DOCUMENT_TEXT_COLOR);

	// Errors and warnings — red/orange
	if (strcmp(prefix, "ERROR") == 0 || strcmp(prefix, "ERR") == 0)
		return (rgb_color){200, 40, 40, 255};
	if (strcmp(prefix, "WARN") == 0 || strcmp(prefix, "WARNING") == 0)
		return (rgb_color){200, 140, 0, 255};

	// Protocol success — green
	if (strcmp(prefix, "OK") == 0)
		return (rgb_color){40, 160, 40, 255};

	// Media transfers — purple/magenta
	if (strcmp(prefix, "IMG") == 0 || strcmp(prefix, "VOICE") == 0
		|| strcmp(prefix, "KEY") == 0)
		return (rgb_color){140, 60, 180, 255};

	// Network/routing — blue
	if (strcmp(prefix, "TRACE") == 0 || strcmp(prefix, "PING") == 0
		|| strcmp(prefix, "RAW") == 0 || strcmp(prefix, "CTRL") == 0)
		return (rgb_color){40, 100, 200, 255};

	// Messaging — teal
	if (strcmp(prefix, "MSG") == 0 || strcmp(prefix, "CLI") == 0
		|| strcmp(prefix, "SAR") == 0)
		return (rgb_color){0, 140, 140, 255};

	// MQTT — dark cyan
	if (strcmp(prefix, "MQTT") == 0)
		return (rgb_color){60, 120, 160, 255};

	// Serial/hardware — gray
	if (strcmp(prefix, "SERIAL") == 0 || strcmp(prefix, "TX") == 0
		|| strcmp(prefix, "RX") == 0)
		return (rgb_color){120, 120, 140, 255};

	// Telemetry — orange
	if (strcmp(prefix, "TELEMETRY") == 0)
		return (rgb_color){200, 120, 40, 255};

	// Debug — dim gray
	if (strcmp(prefix, "DEBUG") == 0)
		return (rgb_color){140, 140, 140, 255};

	// INFO and default — standard text color
	return ui_color(B_DOCUMENT_TEXT_COLOR);
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

	// Append to log with color based on prefix category
	if (LockLooper()) {
		int32 insertPos = fLogView->TextLength();
		fLogView->Insert(insertPos, line.String(), line.Length());

		// Apply color to the newly inserted text
		rgb_color color = _ColorForPrefix(prefix);
		BFont font(be_fixed_font);
		font.SetSize(10);
		fLogView->SetFontAndColor(insertPos,
			insertPos + line.Length(), &font, B_FONT_ALL, &color);

		_PruneLog();
		fLogView->ScrollToOffset(fLogView->TextLength());
		UnlockLooper();
	}
}


// Color for protocol command/response codes based on their function
static rgb_color
_ColorForCommand(uint8 cmd, bool isTx)
{
	if (isTx) {
		// TX commands by category
		switch (cmd) {
			// Messaging — teal
			case CMD_SEND_TXT_MSG:
			case CMD_SEND_CHANNEL_TXT_MSG:
			case CMD_SYNC_NEXT_MESSAGE:
				return (rgb_color){0, 140, 140, 255};

			// Network/routing — blue
			case CMD_SEND_TRACE_PATH:
			case CMD_RESET_PATH:
			case CMD_SEND_RAW_DATA:
			case CMD_SEND_SELF_ADVERT:
			case CMD_SEND_ANON_REQ:
			case CMD_SEND_PATH_DISCOVERY_REQ:
				return (rgb_color){40, 100, 200, 255};

			// Device config — orange
			case CMD_SET_RADIO_PARAMS:
			case CMD_SET_RADIO_TX_POWER:
			case CMD_SET_ADVERT_NAME:
			case CMD_SET_ADVERT_LATLON:
			case CMD_SET_DEVICE_TIME:
			case CMD_SET_DEVICE_PIN:
			case CMD_SET_OTHER_PARAMS:
			case CMD_SET_TUNING_PARAMS:
			case CMD_SET_AUTO_ADD_CONFIG:
			case CMD_SET_PATH_HASH_MODE:
			case CMD_FACTORY_RESET:
			case CMD_REBOOT:
				return (rgb_color){200, 120, 40, 255};

			// Contacts — green
			case CMD_GET_CONTACTS:
			case CMD_ADD_UPDATE_CONTACT:
			case CMD_REMOVE_CONTACT:
			case CMD_SHARE_CONTACT:
			case CMD_EXPORT_CONTACT:
			case CMD_IMPORT_CONTACT:
				return (rgb_color){40, 160, 40, 255};

			// Login/auth — purple
			case CMD_SEND_LOGIN:
			case CMD_LOGOUT:
			case CMD_SEND_STATUS_REQ:
			case CMD_SEND_TELEMETRY_REQ:
			case CMD_SEND_CONTROL_DATA:
				return (rgb_color){140, 60, 180, 255};

			// Query/info — dim blue
			case CMD_GET_BATT_AND_STORAGE:
			case CMD_GET_STATS:
			case CMD_DEVICE_QUERY:
			case CMD_GET_DEVICE_TIME:
			case CMD_GET_CHANNEL:
			case CMD_SET_CHANNEL:
			case CMD_GET_CUSTOM_VARS:
			case CMD_SET_CUSTOM_VAR:
			case CMD_GET_TUNING_PARAMS:
			case CMD_GET_ADVERT_PATH:
			case CMD_GET_ALLOWED_REPEAT_FREQ:
			case CMD_GET_AUTO_ADD_CONFIG:
				return (rgb_color){80, 130, 180, 255};

			default:
				return (rgb_color){120, 120, 140, 255};
		}
	}

	// RX responses by category
	switch (cmd) {
		// Protocol status — green/red
		case RSP_OK:
			return (rgb_color){40, 160, 40, 255};
		case RSP_ERR:
		case RSP_DISABLED:
			return (rgb_color){200, 40, 40, 255};

		// Incoming messages — teal
		case RSP_CONTACT_MSG_RECV:
		case RSP_CONTACT_MSG_RECV_V3:
		case RSP_CHANNEL_MSG_RECV:
		case RSP_CHANNEL_MSG_RECV_V3:
		case RSP_NO_MORE_MESSAGES:
		case RSP_SENT:
			return (rgb_color){0, 140, 140, 255};

		// Contact data — green
		case RSP_CONTACTS_START:
		case RSP_CONTACT:
		case RSP_END_OF_CONTACTS:
		case RSP_EXPORT_CONTACT:
			return (rgb_color){40, 160, 40, 255};

		// Device info — orange
		case RSP_SELF_INFO:
		case RSP_DEVICE_INFO:
		case RSP_BATT_AND_STORAGE:
		case RSP_STATS:
		case RSP_CURR_TIME:
		case RSP_CHANNEL_INFO:
		case RSP_TUNING_PARAMS:
		case RSP_AUTO_ADD_CONFIG:
		case RSP_ALLOWED_REPEAT_FREQ:
		case RSP_CUSTOM_VARS:
			return (rgb_color){200, 120, 40, 255};

		// Push notifications — blue
		case PUSH_ADVERT:
		case PUSH_NEW_ADVERT:
		case PUSH_PATH_UPDATED:
		case PUSH_PATH_DISCOVERY:
		case PUSH_TRACE_DATA:
		case PUSH_RAW_DATA:
		case PUSH_LOG_RX_DATA:
			return (rgb_color){40, 100, 200, 255};

		// Delivery confirmations — cyan
		case PUSH_SEND_CONFIRMED:
		case PUSH_MSG_WAITING:
			return (rgb_color){0, 160, 160, 255};

		// Login/auth — purple
		case PUSH_LOGIN_SUCCESS:
		case PUSH_LOGIN_FAIL:
		case PUSH_STATUS_RESPONSE:
		case PUSH_TELEMETRY_RESPONSE:
		case PUSH_CONTROL_DATA:
		case RSP_PRIVATE_KEY:
		case RSP_SIGN_START:
		case RSP_SIGNATURE:
			return (rgb_color){140, 60, 180, 255};

		// Contact changes
		case PUSH_CONTACT_DELETED:
		case PUSH_CONTACTS_FULL:
			return (rgb_color){200, 100, 40, 255};

		default:
			return (rgb_color){120, 120, 140, 255};
	}
}


void
DebugLogWindow::LogHex(const char* prefix, const uint8* data, size_t length)
{
	BString hex = _FormatHex(data, length);
	bool isTx = (prefix != NULL && strstr(prefix, "TX") != NULL);
	const char* cmdName = (length > 0) ? _CommandName(data[0], isTx) : "";

	BString line;
	line.SetToFormat("%s [%zu]%s: %s", prefix, length, cmdName, hex.String());

	// Get timestamp
	time_t now = time(NULL);
	struct tm tmBuf;
	struct tm* tm = localtime_r(&now, &tmBuf);
	char timestamp[16];
	strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm);

	BString fullLine;
	fullLine.SetToFormat("[%s] %s: %s\n", timestamp, prefix, line.String());

	// Insert with command-specific color
	if (LockLooper()) {
		int32 insertPos = fLogView->TextLength();
		fLogView->Insert(insertPos, fullLine.String(), fullLine.Length());

		rgb_color color = (length > 0)
			? _ColorForCommand(data[0], isTx)
			: (rgb_color){120, 120, 140, 255};
		BFont font(be_fixed_font);
		font.SetSize(10);
		fLogView->SetFontAndColor(insertPos,
			insertPos + fullLine.Length(), &font, B_FONT_ALL, &color);

		_PruneLog();
		fLogView->ScrollToOffset(fLogView->TextLength());
		UnlockLooper();
	}
}


void
DebugLogWindow::Clear()
{
	if (LockLooper()) {
		fLogView->SetText("");
		UnlockLooper();
	}
}


void
DebugLogWindow::_PruneLog()
{
	int32 textLen = fLogView->TextLength();
	if (textLen > kMaxLogSize) {
		// Remove first quarter to avoid pruning on every message
		int32 removeLen = textLen / 4;
		const char* text = fLogView->Text();
		for (int32 i = removeLen; i < textLen && i < removeLen + 256; i++) {
			if (text[i] == '\n') {
				removeLen = i + 1;
				break;
			}
		}
		fLogView->Delete(0, removeLen);
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
DebugLogWindow::_CommandName(uint8 cmd, bool isTx)
{
	// CMD codes 0-22 overlap with RSP codes 0-22 numerically.
	// Use direction (TX = commands sent, RX = responses received) to disambiguate.
	if (isTx) {
		switch (cmd) {
			case CMD_APP_START:				return " (APP_START)";
			case CMD_SEND_TXT_MSG:			return " (SEND_TXT_MSG)";
			case CMD_SEND_CHANNEL_TXT_MSG:	return " (SEND_CHANNEL)";
			case CMD_GET_CONTACTS:			return " (GET_CONTACTS)";
			case CMD_GET_DEVICE_TIME:		return " (GET_TIME)";
			case CMD_SET_DEVICE_TIME:		return " (SET_TIME)";
			case CMD_SEND_SELF_ADVERT:		return " (SEND_ADVERT)";
			case CMD_SET_ADVERT_NAME:		return " (SET_NAME)";
			case CMD_ADD_UPDATE_CONTACT:	return " (ADD_CONTACT)";
			case CMD_SYNC_NEXT_MESSAGE:		return " (SYNC_MSG)";
			case CMD_SET_RADIO_PARAMS:		return " (SET_RADIO)";
			case CMD_SET_RADIO_TX_POWER:	return " (SET_TX_PWR)";
			case CMD_RESET_PATH:			return " (RESET_PATH)";
			case CMD_SET_ADVERT_LATLON:		return " (SET_LATLON)";
			case CMD_REMOVE_CONTACT:		return " (RM_CONTACT)";
			case CMD_SHARE_CONTACT:			return " (SHARE)";
			case CMD_EXPORT_CONTACT:		return " (EXPORT)";
			case CMD_IMPORT_CONTACT:		return " (IMPORT)";
			case CMD_REBOOT:				return " (REBOOT)";
			case CMD_GET_BATT_AND_STORAGE:	return " (GET_BATT)";
			case CMD_SET_TUNING_PARAMS:		return " (SET_TUNING)";
			case CMD_DEVICE_QUERY:			return " (DEV_QUERY)";
			case CMD_SEND_RAW_DATA:			return " (SEND_RAW)";
			case CMD_SEND_LOGIN:			return " (LOGIN)";
			case CMD_SEND_STATUS_REQ:		return " (STATUS_REQ)";
			case CMD_GET_CHANNEL:			return " (GET_CH)";
			case CMD_SET_CHANNEL:			return " (SET_CH)";
			case CMD_SEND_TRACE_PATH:		return " (TRACE)";
			case CMD_SET_DEVICE_PIN:		return " (SET_PIN)";
			case CMD_SET_OTHER_PARAMS:		return " (SET_OTHER)";
			case CMD_SEND_TELEMETRY_REQ:	return " (TEL_REQ)";
			case CMD_GET_CUSTOM_VARS:		return " (GET_VARS)";
			case CMD_SET_CUSTOM_VAR:		return " (SET_VAR)";
			case CMD_GET_ADVERT_PATH:		return " (GET_PATH)";
			case CMD_GET_TUNING_PARAMS:		return " (GET_TUNING)";
			case CMD_SEND_BINARY_REQ:		return " (BIN_REQ)";
			case CMD_FACTORY_RESET:			return " (FACTORY_RST)";
			case CMD_SEND_CONTROL_DATA:		return " (CTRL_DATA)";
			case CMD_GET_STATS:				return " (GET_STATS)";
			case CMD_SEND_ANON_REQ:			return " (ANON_REQ)";
			case CMD_SET_AUTO_ADD_CONFIG:	return " (SET_AUTOADD)";
			case CMD_GET_AUTO_ADD_CONFIG:	return " (GET_AUTOADD)";
			case CMD_GET_ALLOWED_REPEAT_FREQ: return " (GET_FREQ)";
			case CMD_SET_PATH_HASH_MODE:	return " (SET_HASH)";
			default: return "";
		}
	}

	// RX: responses and push notifications
	switch (cmd) {
		case RSP_OK:					return " (OK)";
		case RSP_ERR:					return " (ERR)";
		case RSP_CONTACTS_START:		return " (CONTACTS_START)";
		case RSP_CONTACT:				return " (CONTACT)";
		case RSP_END_OF_CONTACTS:		return " (END_CONTACTS)";
		case RSP_SELF_INFO:				return " (SELF_INFO)";
		case RSP_SENT:					return " (SENT)";
		case RSP_CONTACT_MSG_RECV:		return " (DM_V2)";
		case RSP_CHANNEL_MSG_RECV:		return " (CH_MSG_V2)";
		case RSP_CURR_TIME:				return " (TIME)";
		case RSP_NO_MORE_MESSAGES:		return " (NO_MORE_MSG)";
		case RSP_EXPORT_CONTACT:		return " (EXPORT)";
		case RSP_BATT_AND_STORAGE:		return " (BATTERY)";
		case RSP_DEVICE_INFO:			return " (DEV_INFO)";
		case RSP_PRIVATE_KEY:			return " (PRIV_KEY)";
		case RSP_DISABLED:				return " (DISABLED)";
		case RSP_CONTACT_MSG_RECV_V3:	return " (DM_V3)";
		case RSP_CHANNEL_MSG_RECV_V3:	return " (CH_MSG_V3)";
		case RSP_CHANNEL_INFO:			return " (CH_INFO)";
		case RSP_SIGN_START:			return " (SIGN_START)";
		case RSP_SIGNATURE:				return " (SIGNATURE)";
		case RSP_CUSTOM_VARS:			return " (VARS)";
		case RSP_ADVERT_PATH:			return " (ADV_PATH)";
		case RSP_TUNING_PARAMS:			return " (TUNING)";
		case RSP_STATS:					return " (STATS)";
		case RSP_AUTO_ADD_CONFIG:		return " (AUTOADD_CFG)";
		case RSP_ALLOWED_REPEAT_FREQ:	return " (FREQ)";

		// Push notifications (0x80+, no overlap with CMD)
		case PUSH_ADVERT:				return " (ADVERT)";
		case PUSH_PATH_UPDATED:			return " (PATH_UPD)";
		case PUSH_SEND_CONFIRMED:		return " (CONFIRMED)";
		case PUSH_MSG_WAITING:			return " (MSG_WAIT)";
		case PUSH_RAW_DATA:				return " (RAW_DATA)";
		case PUSH_LOGIN_SUCCESS:		return " (LOGIN_OK)";
		case PUSH_LOGIN_FAIL:			return " (LOGIN_FAIL)";
		case PUSH_STATUS_RESPONSE:		return " (STATUS)";
		case PUSH_LOG_RX_DATA:			return " (LOG_RX)";
		case PUSH_TRACE_DATA:			return " (TRACE)";
		case PUSH_NEW_ADVERT:			return " (NEW_ADV)";
		case PUSH_TELEMETRY_RESPONSE:	return " (TELEMETRY)";
		case PUSH_BINARY_RESPONSE:		return " (BIN_RSP)";
		case PUSH_PATH_DISCOVERY:		return " (PATH_DISC)";
		case PUSH_CONTROL_DATA:			return " (CTRL_DATA)";
		case PUSH_CONTACT_DELETED:		return " (CONTACT_DEL)";
		case PUSH_CONTACTS_FULL:		return " (FULL)";

		default: return "";
	}
}
