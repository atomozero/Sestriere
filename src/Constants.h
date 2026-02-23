/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Constants.h — Application constants and message codes
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <GraphicsDefs.h>
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

// =============================================================================
// Protocol Frame Field Offsets
// =============================================================================

// Contact hex key buffer sizes
const size_t kContactHexSize = 13;    // 6-byte prefix as hex (12 chars + null)
const size_t kPubKeyHexSize = 65;     // 32-byte key as hex (64 chars + null)

// RSP_CONTACT frame (148 bytes total)
const size_t kContactFrameSize = 148;
const size_t kContactPubKeyOffset = 1;     // [1-32] = public key (32 bytes)
const size_t kContactTypeOffset = 33;      // [33] = ADV_TYPE
const size_t kContactFlagsOffset = 34;     // [34] = flags
const size_t kContactPathLenOffset = 35;   // [35] = outbound path length
const size_t kContactNameOffset = 100;     // [100-131] = name (32 bytes)
const size_t kContactNameSize = 32;
const size_t kContactLastSeenOffset = 132; // [132-135] = last seen (uint32 LE)
const size_t kContactLatOffset = 136;      // [136-139] = latitude (int32 LE)
const size_t kContactLonOffset = 140;      // [140-143] = longitude (int32 LE)

// RSP_CONTACT_MSG_RECV_V3 (V3 direct message)
const size_t kV3DmSnrOffset = 1;           // [1] = SNR (int8)
const size_t kV3DmRssiOffset = 2;          // [2] = RSSI (int8)
const size_t kV3DmPathLenOffsetB = 3;      // [3] = path_len
const size_t kV3DmSenderOffset = 4;        // [4-9] = sender pubkey prefix
const size_t kV3DmPathLenOffset = 10;      // [10] = path_len (duplicate)
const size_t kV3DmTxtTypeOffset = 11;      // [11] = txt_type
const size_t kV3DmTimestampOffset = 12;    // [12-15] = timestamp (uint32 LE)
const size_t kV3DmTextOffset = 16;         // [16+] = text content
const size_t kV3DmMinLength = 16;

// RSP_CONTACT_MSG_RECV (V2 direct message)
const size_t kV2DmSenderOffset = 1;        // [1-6] = sender pubkey prefix
const size_t kV2DmPathLenOffset = 7;       // [7] = path_len
const size_t kV2DmTxtTypeOffset = 8;       // [8] = txt_type
const size_t kV2DmTimestampOffset = 9;     // [9-12] = timestamp (uint32 LE)
const size_t kV2DmTextOffset = 13;         // [13+] = text content
const size_t kV2DmMinLength = 13;

// RSP_CHANNEL_MSG_RECV_V3 (V3 channel message)
const size_t kV3ChSnrOffset = 1;           // [1] = SNR (int8)
const size_t kV3ChChannelOffset = 4;       // [4] = channel index
const size_t kV3ChPathLenOffset = 5;       // [5] = path_len
const size_t kV3ChTxtTypeOffset = 6;       // [6] = txt_type
const size_t kV3ChTimestampOffset = 7;     // [7-10] = timestamp (uint32 LE)
const size_t kV3ChTextOffset = 11;         // [11+] = text content
const size_t kV3ChMinLength = 11;

// RSP_CHANNEL_MSG_RECV (V2 channel message)
const size_t kV2ChChannelOffset = 1;       // [1] = channel index
const size_t kV2ChPathLenOffset = 2;       // [2] = path_len
const size_t kV2ChTxtTypeOffset = 3;       // [3] = txt_type
const size_t kV2ChTimestampOffset = 4;     // [4-7] = timestamp (uint32 LE)
const size_t kV2ChTextOffset = 8;          // [8+] = text content
const size_t kV2ChMinLength = 9;

// RSP_BATT_AND_STORAGE
const size_t kBattMvOffset = 1;            // [1-2] = battery mV (uint16 LE)
const size_t kStorageUsedOffset = 3;       // [3-6] = used KB (uint32 LE)
const size_t kStorageTotalOffset = 7;      // [7-10] = total KB (uint32 LE)

// RSP_STATS subtypes
const uint8 kStatsSubtypeCore = 0;
const uint8 kStatsSubtypeRadio = 1;
const uint8 kStatsSubtypePackets = 2;
// Core stats offsets (subtype 0)
const size_t kStatsCoreSubtypeOffset = 1;  // [1] = subtype
const size_t kStatsCoreBattOffset = 2;     // [2-3] = battery mV (uint16 LE)
const size_t kStatsCoreUptimeOffset = 4;   // [4-7] = uptime (uint32 LE)
// Radio stats offsets (subtype 1)
const size_t kStatsRadioNoiseOffset = 2;   // [2-3] = noise floor (int16 LE)
const size_t kStatsRadioRssiOffset = 4;    // [4] = RSSI (int8)
const size_t kStatsRadioSnrOffset = 5;     // [5] = SNR (int8)
// Packet stats offsets (subtype 2)
const size_t kStatsPacketsRxOffset = 2;    // [2-5] = recv packets (uint32 LE)
const size_t kStatsPacketsTxOffset = 6;    // [6-9] = sent packets (uint32 LE)

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

	// MQTT
	MSG_MQTT_TOGGLE = 'mqtg',
	MSG_SHOW_MQTT_LOG = 'mqlg',
	MSG_MQTT_LOG_ENTRY = 'mqle',
	MSG_MQTT_SETTINGS_CHANGED = 'mqch',

	// Input
	MSG_INPUT_MODIFIED = 'inmd',

	// Admin panel (inline in ContactInfoPanel)
	MSG_ADMIN_REFRESH_TICK = 'artk',
	MSG_ADMIN_REBOOT = 'arbt',
	MSG_ADMIN_FACTORY_RESET = 'arfr',

	// Remote telemetry polling
	MSG_REQUEST_ALL_TELEMETRY = 'rqat',
	MSG_TELEMETRY_POLL_TICK   = 'tptk',

	// Admin CLI commands (sent as TXT_TYPE_CLI_DATA to logged-in repeater/room)
	MSG_ADMIN_SEND_CLI = 'ascl',

	// Mission Control dashboard
	MSG_SHOW_MISSION_CONTROL = 'mctr',

	// GPX export
	MSG_EXPORT_GPX = 'xgpx',
	MSG_EXPORT_GPX_DONE = 'xgpd',

	// Ping
	MSG_CONTACT_PING = 'cpng',

	// Profile export/import
	MSG_SHOW_PROFILE = 'prof',
	MSG_PROFILE_IMPORT_CONTACTS = 'pimc',
	MSG_PROFILE_IMPORT_CHANNELS = 'pich',
	MSG_PROFILE_IMPORT_RADIO = 'pird',
	MSG_PROFILE_IMPORT_MQTT = 'pimq',

	// Mute toggle
	MSG_CONTACT_MUTE_TOGGLE = 'cmut',

	// Raw serial data (non-protocol text from device)
	MSG_RAW_SERIAL_DATA = 'raws',

	// Serial Monitor
	MSG_SERIAL_SEND_RAW = 'srnd',
	MSG_SHOW_SERIAL_MONITOR = 'srmn',

	// Contact groups
	MSG_GROUP_ADD_CONTACT = 'gadd',
	MSG_GROUP_REMOVE_CONTACT = 'grmv',
	MSG_GROUP_CREATE = 'gcrt',
	MSG_GROUP_DELETE = 'gdel',
};

// =============================================================================
// UI Color Constants
// =============================================================================

// Status indicator colors (contact online/recent/offline)
const rgb_color kStatusOnline = {77, 182, 172, 255};    // Teal
const rgb_color kStatusRecent = {220, 180, 60, 255};    // Gold
const rgb_color kStatusOffline = {140, 140, 140, 255};  // Gray

// Signal quality level colors (SNR, RSSI, battery, health)
const rgb_color kColorGood = {80, 180, 80, 255};        // Green
const rgb_color kColorFair = {200, 170, 50, 255};       // Yellow
const rgb_color kColorPoor = {210, 120, 50, 255};       // Orange
const rgb_color kColorBad = {200, 60, 60, 255};         // Red

// Node type badge colors
const rgb_color kTypeBadgeChat = {79, 195, 247, 255};       // Light Blue
const rgb_color kTypeBadgeRepeater = {100, 160, 100, 255};  // Green
const rgb_color kTypeBadgeRoom = {120, 120, 180, 255};      // Purple

// Avatar color palette (Telegram-style, shared across ContactItem/ChatHeader/InfoPanel)
const rgb_color kAvatarPalette[] = {
	{229, 115, 115, 255},  // Red
	{186, 104, 200, 255},  // Purple
	{121, 134, 203, 255},  // Indigo
	{79, 195, 247, 255},   // Light Blue
	{77, 182, 172, 255},   // Teal
	{129, 199, 132, 255},  // Green
	{255, 183, 77, 255},   // Orange
	{240, 98, 146, 255},   // Pink
};
const int kAvatarPaletteCount = 8;

// =============================================================================
// Signal & Battery Thresholds
// =============================================================================

// Battery voltage thresholds (millivolts)
const uint16 kBattGoodMv = 3900;
const uint16 kBattFairMv = 3600;
const uint16 kBattLowMv = 3400;

// Battery percentage calculation (3.0V = 0%, 4.2V = 100%)
const uint16 kBattMinMv = 3000;
const uint16 kBattRangeMv = 1200;

// RSSI thresholds (dBm)
const int8 kRssiGood = -60;
const int8 kRssiFair = -80;
const int8 kRssiPoor = -90;

// SNR thresholds (dB)
const int8 kSnrExcellent = 5;
const int8 kSnrGood = 0;
const int8 kSnrFair = -5;
const int8 kSnrPoor = -10;

#endif // CONSTANTS_H
