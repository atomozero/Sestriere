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

inline const char* kAppSignature = "application/x-vnd.Sestriere";
inline const char* kAppName = "Sestriere";

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
	MSG_BATTERY_TIMER			= 'bttm',

	// Contact actions
	MSG_SHOW_LOGIN				= 'shlg',
	MSG_SHOW_TRACE_PATH			= 'shtp',
	MSG_EXPORT_CONTACT			= 'exct',
	MSG_IMPORT_CONTACT			= 'imct',
	MSG_SEND_CHANNEL_MESSAGE	= 'scms',

	// Channel
	MSG_CHANNEL_SELECTED		= 'chsl',
	MSG_PUBLIC_CHANNEL			= 'pbch',

	// Statistics
	MSG_SHOW_STATS				= 'shss',

	// Deskbar
	MSG_INSTALL_DESKBAR			= 'idkb',
	MSG_REMOVE_DESKBAR			= 'rdkb',

	// View windows
	MSG_SHOW_MAP				= 'shmp',
	MSG_SHOW_MESH_GRAPH			= 'shmg',
	MSG_SHOW_TELEMETRY			= 'shtl'
};

// =============================================================================
// BMessage Field Names
// =============================================================================

inline const char* kFieldPort		= "port";
inline const char* kFieldData		= "data";
inline const char* kFieldSize		= "size";
inline const char* kFieldError		= "error";
inline const char* kFieldErrorCode	= "error_code";
inline const char* kFieldContact	= "contact";
inline const char* kFieldMessage	= "message";
inline const char* kFieldTimestamp	= "timestamp";
inline const char* kFieldText		= "text";
inline const char* kFieldPubKey		= "pubkey";
inline const char* kFieldName		= "name";
inline const char* kFieldType		= "type";
inline const char* kFieldCode		= "code";
inline const char* kFieldCount		= "count";
inline const char* kFieldBattery	= "battery";
inline const char* kFieldUsedKb		= "used_kb";
inline const char* kFieldTotalKb	= "total_kb";
inline const char* kFieldRoundTrip	= "round_trip";
inline const char* kFieldSnr		= "snr";
inline const char* kFieldPathLen	= "path_len";

// =============================================================================
// Default Settings
// =============================================================================

inline constexpr uint32 kDefaultBaudRate		= 115200;
inline constexpr uint8 kDefaultDataBits			= 8;
inline constexpr uint8 kDefaultStopBits			= 1;
inline constexpr int32 kDefaultWindowWidth		= 800;
inline constexpr int32 kDefaultWindowHeight		= 600;
inline constexpr int32 kMinWindowWidth			= 640;
inline constexpr int32 kMinWindowHeight			= 480;
inline constexpr int32 kContactListWidth		= 200;

// Timing constants (microseconds)
inline constexpr bigtime_t kSerialReadTimeout	= 100000;		// 100ms
inline constexpr bigtime_t kBatteryPollInterval	= 60000000;		// 60s
inline constexpr bigtime_t kMessagePollInterval	= 1000000;		// 1s

// Protocol constants
inline constexpr uint8 kAppProtocolVersion		= 1;
inline const char* kAppIdentifier				= "Sestriere";

// =============================================================================
// Settings File
// =============================================================================

inline const char* kSettingsFileName = "Sestriere_settings";
inline const char* kSettingsFieldLastPort = "last_port";
inline const char* kSettingsFieldWindowFrame = "window_frame";
inline const char* kSettingsFieldLastContactSync = "last_contact_sync";

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
#define TR_MENU_CONTACT				"Contact"
#define TR_MENU_LOGIN				"Login" B_UTF8_ELLIPSIS
#define TR_MENU_TRACE_PATH			"Trace Path" B_UTF8_ELLIPSIS
#define TR_MENU_EXPORT_CONTACT		"Export Contact" B_UTF8_ELLIPSIS
#define TR_MENU_IMPORT_CONTACT		"Import Contact" B_UTF8_ELLIPSIS
#define TR_MENU_PUBLIC_CHANNEL		"Public Channel"
#define TR_MENU_STATISTICS			"Statistics" B_UTF8_ELLIPSIS
#define TR_MENU_INSTALL_DESKBAR		"Show in Deskbar"
#define TR_MENU_REMOVE_DESKBAR		"Remove from Deskbar"

#define TR_MENU_VIEW				"View"
#define TR_MENU_MAP_VIEW			"Map View"
#define TR_MENU_MESH_GRAPH			"Network Graph"
#define TR_MENU_TELEMETRY			"Sensor Telemetry"

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
