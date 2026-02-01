/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Constants.h — Application constants and message codes
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <SupportDefs.h>

// =============================================================================
// Application Signature
// =============================================================================

static const char* kAppSignature = "application/x-vnd.Sestriere";
static const char* kAppName = "Sestriere";

// =============================================================================
// Internal BMessage Codes
// =============================================================================

enum {
	// Serial communication
	MSG_SERIAL_CONNECT			= 'srco',
	MSG_SERIAL_DISCONNECT		= 'srdc',
	MSG_SERIAL_PORT_SELECTED	= 'srps',
	MSG_SERIAL_DATA_RECEIVED	= 'srdr',
	MSG_SERIAL_ERROR			= 'srer',
	MSG_SERIAL_CONNECTED		= 'srcd',
	MSG_SERIAL_DISCONNECTED		= 'srdd',

	// Protocol messages
	MSG_FRAME_RECEIVED			= 'frrc',
	MSG_DEVICE_INFO_RECEIVED	= 'dvir',
	MSG_SELF_INFO_RECEIVED		= 'sfir',
	MSG_CONTACTS_START			= 'ctst',
	MSG_CONTACT_RECEIVED		= 'ctrc',
	MSG_CONTACTS_END			= 'cten',
	MSG_MESSAGE_RECEIVED		= 'msrc',
	MSG_ACK_RECEIVED			= 'akrc',
	MSG_BATTERY_RECEIVED		= 'btrc',
	MSG_PUSH_NOTIFICATION		= 'push',
	MSG_SEND_CONFIRMED			= 'sdcf',
	MSG_NO_MORE_MESSAGES		= 'nomm',

	// UI actions
	MSG_CONTACT_SELECTED		= 'ctsl',
	MSG_SEND_MESSAGE			= 'sdms',
	MSG_REFRESH_CONTACTS		= 'rfct',
	MSG_SEND_ADVERT				= 'sdad',
	MSG_SHOW_SETTINGS			= 'shst',
	MSG_SHOW_ABOUT				= 'shab',
	MSG_SHOW_PORT_SELECTION		= 'shps',
	MSG_SYNC_MESSAGES			= 'symg',
	MSG_UPDATE_STATUS			= 'upst',
	MSG_REQUEST_BATTERY			= 'rqbt',

	// Settings
	MSG_SETTINGS_CHANGED		= 'stch',
	MSG_RADIO_PARAMS_CHANGED	= 'rdpc',
	MSG_NAME_CHANGED			= 'nmch',

	// Timer/periodic
	MSG_POLL_TIMER				= 'pltm',
	MSG_BATTERY_TIMER			= 'bttm'
};

// =============================================================================
// BMessage Field Names
// =============================================================================

static const char* kFieldPort		= "port";
static const char* kFieldData		= "data";
static const char* kFieldSize		= "size";
static const char* kFieldError		= "error";
static const char* kFieldErrorCode	= "error_code";
static const char* kFieldContact	= "contact";
static const char* kFieldMessage	= "message";
static const char* kFieldTimestamp	= "timestamp";
static const char* kFieldText		= "text";
static const char* kFieldPubKey		= "pubkey";
static const char* kFieldName		= "name";
static const char* kFieldType		= "type";
static const char* kFieldCode		= "code";
static const char* kFieldCount		= "count";
static const char* kFieldBattery	= "battery";
static const char* kFieldUsedKb		= "used_kb";
static const char* kFieldTotalKb	= "total_kb";
static const char* kFieldRoundTrip	= "round_trip";
static const char* kFieldSnr		= "snr";
static const char* kFieldPathLen	= "path_len";

// =============================================================================
// Default Settings
// =============================================================================

static const uint32 kDefaultBaudRate		= 115200;
static const uint8 kDefaultDataBits			= 8;
static const uint8 kDefaultStopBits			= 1;
static const int32 kDefaultWindowWidth		= 800;
static const int32 kDefaultWindowHeight		= 600;
static const int32 kMinWindowWidth			= 640;
static const int32 kMinWindowHeight			= 480;
static const int32 kContactListWidth		= 200;

// Timing constants (microseconds)
static const bigtime_t kSerialReadTimeout	= 100000;		// 100ms
static const bigtime_t kBatteryPollInterval	= 60000000;		// 60s
static const bigtime_t kMessagePollInterval	= 1000000;		// 1s

// Protocol constants
static const uint8 kAppProtocolVersion		= 1;
static const char* kAppIdentifier			= "Sestriere";

// =============================================================================
// Settings File
// =============================================================================

static const char* kSettingsFileName = "Sestriere_settings";
static const char* kSettingsFieldLastPort = "last_port";
static const char* kSettingsFieldWindowFrame = "window_frame";
static const char* kSettingsFieldLastContactSync = "last_contact_sync";

// =============================================================================
// UI Strings (for localization)
// =============================================================================

#define TR_APP_NAME					"Sestriere"
#define TR_MENU_FILE				"File"
#define TR_MENU_EDIT				"Edit"
#define TR_MENU_DEVICE				"Device"
#define TR_MENU_HELP				"Help"
#define TR_MENU_CONNECT				"Connect" B_UTF8_ELLIPSIS
#define TR_MENU_DISCONNECT			"Disconnect"
#define TR_MENU_SETTINGS			"Settings" B_UTF8_ELLIPSIS
#define TR_MENU_QUIT				"Quit"
#define TR_MENU_ABOUT				"About Sestriere" B_UTF8_ELLIPSIS
#define TR_MENU_REFRESH_CONTACTS	"Refresh Contacts"
#define TR_MENU_SEND_ADVERT			"Send Advertisement"
#define TR_MENU_REBOOT_DEVICE		"Reboot Device"

#define TR_STATUS_DISCONNECTED		"Disconnected"
#define TR_STATUS_CONNECTING		"Connecting..."
#define TR_STATUS_CONNECTED			"Connected"
#define TR_STATUS_ERROR				"Error"

#define TR_BUTTON_SEND				"Send"
#define TR_BUTTON_CONNECT			"Connect"
#define TR_BUTTON_CANCEL			"Cancel"
#define TR_BUTTON_OK				"OK"
#define TR_BUTTON_REFRESH			"Refresh"

#define TR_LABEL_PORT				"Port:"
#define TR_LABEL_NO_PORTS			"No serial ports found"
#define TR_LABEL_SELECT_PORT		"Select a serial port:"
#define TR_LABEL_MESSAGE			"Message:"
#define TR_LABEL_CONTACTS			"Contacts"
#define TR_LABEL_NO_CONTACTS		"No contacts"
#define TR_LABEL_NO_SELECTION		"Select a contact to start chatting"

#define TR_TITLE_PORT_SELECTION		"Select Serial Port"
#define TR_TITLE_SETTINGS			"Settings"
#define TR_TITLE_ABOUT				"About Sestriere"

#endif // CONSTANTS_H
