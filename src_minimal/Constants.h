/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Constants.h — Application constants and message codes
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <SupportDefs.h>

// Application info
#define APP_SIGNATURE "application/x-vnd.Sestriere"
#define APP_NAME "Sestriere"

// Serial frame constants
const uint8 kFrameMarkerInbound = '<';   // App -> Device
const uint8 kFrameMarkerOutbound = '>';  // Device -> App
const size_t kFrameHeaderSize = 3;       // marker + len_lo + len_hi
const size_t kMaxFramePayload = 512;
const size_t kMaxFrameSize = kFrameHeaderSize + kMaxFramePayload;

// =============================================================================
// MeshCore Companion Protocol - Inbound Commands (App -> Radio)
// =============================================================================
const uint8 CMD_APP_START = 1;
const uint8 CMD_SEND_TXT_MSG = 2;
const uint8 CMD_SEND_CHANNEL_TXT_MSG = 3;
const uint8 CMD_GET_CONTACTS = 4;
const uint8 CMD_GET_DEVICE_TIME = 5;
const uint8 CMD_SET_DEVICE_TIME = 6;
const uint8 CMD_SEND_SELF_ADVERT = 7;
const uint8 CMD_SET_ADVERT_NAME = 8;
const uint8 CMD_ADD_UPDATE_CONTACT = 9;
const uint8 CMD_SYNC_NEXT_MESSAGE = 10;
const uint8 CMD_SET_RADIO_PARAMS = 11;
const uint8 CMD_SET_RADIO_TX_POWER = 12;
const uint8 CMD_RESET_PATH = 13;
const uint8 CMD_SET_ADVERT_LATLON = 14;
const uint8 CMD_REMOVE_CONTACT = 15;
const uint8 CMD_SHARE_CONTACT = 16;
const uint8 CMD_EXPORT_CONTACT = 17;
const uint8 CMD_IMPORT_CONTACT = 18;
const uint8 CMD_REBOOT = 19;
const uint8 CMD_GET_BATT_AND_STORAGE = 20;
const uint8 CMD_SET_TUNING_PARAMS = 21;
const uint8 CMD_DEVICE_QUERY = 22;
const uint8 CMD_GET_CHANNEL = 31;
const uint8 CMD_SET_CHANNEL = 32;
const uint8 CMD_SEND_RAW_DATA = 25;
const uint8 CMD_SEND_LOGIN = 26;
const uint8 CMD_SEND_STATUS_REQ = 27;
const uint8 CMD_SEND_TRACE_PATH = 36;
const uint8 CMD_SET_DEVICE_PIN = 37;
const uint8 CMD_SET_OTHER_PARAMS = 38;
const uint8 CMD_SEND_TELEMETRY_REQ = 39;
const uint8 CMD_GET_CUSTOM_VARS = 40;
const uint8 CMD_SET_CUSTOM_VAR = 41;
const uint8 CMD_GET_ADVERT_PATH = 42;
const uint8 CMD_GET_TUNING_PARAMS = 43;
const uint8 CMD_SEND_BINARY_REQ = 50;
const uint8 CMD_FACTORY_RESET = 51;
const uint8 CMD_SEND_CONTROL_DATA = 55;
const uint8 CMD_GET_STATS = 56;

// =============================================================================
// MeshCore Companion Protocol - Outbound Responses (Radio -> App)
// =============================================================================
const uint8 RSP_OK = 0;
const uint8 RSP_ERR = 1;
const uint8 RSP_CONTACTS_START = 2;
const uint8 RSP_CONTACT = 3;
const uint8 RSP_END_OF_CONTACTS = 4;
const uint8 RSP_SELF_INFO = 5;
const uint8 RSP_SENT = 6;
const uint8 RSP_CONTACT_MSG_RECV = 7;
const uint8 RSP_CHANNEL_MSG_RECV = 8;
const uint8 RSP_CURR_TIME = 9;
const uint8 RSP_NO_MORE_MESSAGES = 10;
const uint8 RSP_EXPORT_CONTACT = 11;
const uint8 RSP_BATT_AND_STORAGE = 12;
const uint8 RSP_DEVICE_INFO = 13;
const uint8 RSP_CONTACT_MSG_RECV_V3 = 16;
const uint8 RSP_CHANNEL_INFO = 18;
const uint8 RSP_CHANNEL_MSG_RECV_V3 = 17;
const uint8 RSP_CUSTOM_VARS = 21;
const uint8 RSP_ADVERT_PATH = 22;
const uint8 RSP_STATS = 24;

// =============================================================================
// MeshCore Companion Protocol - Push Notifications (Radio -> App, unsolicited)
// =============================================================================
const uint8 PUSH_ADVERT = 0x80;
const uint8 PUSH_PATH_UPDATED = 0x81;
const uint8 PUSH_SEND_CONFIRMED = 0x82;
const uint8 PUSH_MSG_WAITING = 0x83;
const uint8 PUSH_RAW_DATA = 0x84;
const uint8 PUSH_LOGIN_SUCCESS = 0x85;
const uint8 PUSH_LOGIN_FAIL = 0x86;
const uint8 PUSH_STATUS_RESPONSE = 0x87;
const uint8 PUSH_RAW_RADIO_PACKET = 0x88;
const uint8 PUSH_TRACE_DATA = 0x89;
const uint8 PUSH_NEW_ADVERT = 0x8A;
const uint8 PUSH_TELEMETRY_RESPONSE = 0x8B;
const uint8 PUSH_BINARY_RESPONSE = 0x8C;
const uint8 PUSH_CONTROL_DATA = 0x8E;

// Text message types
const uint8 TXT_TYPE_PLAIN = 0;
const uint8 TXT_TYPE_CLI_DATA = 1;

// Message field names
const char* const kFieldPort = "port";
const char* const kFieldData = "data";
const char* const kFieldSize = "size";
const char* const kFieldError = "error";
const char* const kFieldErrorCode = "error_code";

// Application messages
enum {
	// Serial connection
	MSG_SERIAL_CONNECT = 'scon',
	MSG_SERIAL_DISCONNECT = 'sdis',
	MSG_SERIAL_CONNECTED = 'scok',
	MSG_SERIAL_DISCONNECTED = 'scdc',
	MSG_SERIAL_ERROR = 'serr',
	MSG_FRAME_RECEIVED = 'frec',
	MSG_FRAME_SENT = 'fsnt',

	// Settings & UI
	MSG_PORT_SELECTED = 'psel',
	MSG_PRESET_SELECTED = 'pres',
	MSG_APPLY_SETTINGS = 'aply',
	MSG_REFRESH_PORTS = 'rfpt',
	MSG_CLEAR_LOG = 'clog',
	MSG_SET_NAME = 'snam',
	MSG_INSTALL_DESKBAR = 'idkb',
	MSG_REMOVE_DESKBAR = 'rdkb',

	// Contacts & messages
	MSG_SYNC_CONTACTS = 'sync',
	MSG_CONTACT_SELECTED = 'csel',
	MSG_SEND_MESSAGE = 'smsg',
	MSG_CONTACT_ADDED = 'cadd',
	MSG_MESSAGE_RECEIVED = 'mrec',
	MSG_SEND_ADVERT = 'advt',
	MSG_DEVICE_QUERY = 'dqry',
	MSG_GET_BATTERY = 'batt',
	MSG_GET_STATS = 'stat',
	MSG_REQUEST_STATS_DATA = 'rsta',

	// Window navigation (shared across files)
	MSG_SHOW_NETWORK_MAP = 'nmap',
	MSG_SHOW_MAP = 'shmp',
	MSG_TRACE_PATH = 'trcp',
	MSG_SEND_LOGIN_CMD = 'slgn',
	MSG_EXPORT_CONTACT_CMD = 'exct',
	MSG_IMPORT_CONTACT_CMD = 'imct',

	// Tool windows
	MSG_SHOW_DEBUG_LOG = 'dbgl',
	MSG_SHOW_STATS = 'shss',
	MSG_SHOW_TELEMETRY = 'shtl',

	// Packet Analyzer
	MSG_SHOW_PACKET_ANALYZER = 'pkan',
	MSG_PACKET_CAPTURED = 'pkcp',
	MSG_PACKET_CAPTURE_START = 'pcst',
	MSG_PACKET_CAPTURE_STOP = 'pcsp',
	MSG_PACKET_CAPTURE_CLEAR = 'pccl',
	MSG_PACKET_EXPORT_CSV = 'pxcv',
	MSG_PACKET_FILTER_CHANGED = 'pfch',

	// MQTT toggle and log
	MSG_MQTT_TOGGLE = 'mqtg',
	MSG_SHOW_MQTT_LOG = 'mqlg',
	MSG_MQTT_LOG_ENTRY = 'mqle',

	// Input
	MSG_INPUT_MODIFIED = 'inmd',

	// Admin panel (inline in ContactInfoPanel)
	MSG_ADMIN_REFRESH_TICK = 'artk',
	MSG_ADMIN_REBOOT = 'arbt',
	MSG_ADMIN_FACTORY_RESET = 'arfr',
};

#endif // CONSTANTS_H
