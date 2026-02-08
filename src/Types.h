/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Types.h — Protocol structures and constants for MeshCore
 */

#ifndef TYPES_H
#define TYPES_H

#include <SupportDefs.h>

// =============================================================================
// Frame Constants
// =============================================================================

static const uint8 kFrameMarkerInbound = 0x3C;		// '<' App -> Radio
static const uint8 kFrameMarkerOutbound = 0x3E;		// '>' Radio -> App
static const size_t kFrameHeaderSize = 3;			// marker + len_lo + len_hi
static const size_t kMaxFramePayload = 512;
static const size_t kMaxFrameSize = kFrameHeaderSize + kMaxFramePayload;

// =============================================================================
// Command Codes (App -> Radio)
// =============================================================================

enum CommandCode : uint8 {
	CMD_APP_START				= 1,
	CMD_SEND_TXT_MSG			= 2,
	CMD_SEND_CHANNEL_TXT_MSG	= 3,
	CMD_GET_CONTACTS			= 4,
	CMD_GET_DEVICE_TIME			= 5,
	CMD_SET_DEVICE_TIME			= 6,
	CMD_SEND_SELF_ADVERT		= 7,
	CMD_SET_ADVERT_NAME			= 8,
	CMD_ADD_UPDATE_CONTACT		= 9,
	CMD_SYNC_NEXT_MESSAGE		= 10,
	CMD_SET_RADIO_PARAMS		= 11,
	CMD_SET_RADIO_TX_POWER		= 12,
	CMD_RESET_PATH				= 13,
	CMD_SET_ADVERT_LATLON		= 14,
	CMD_REMOVE_CONTACT			= 15,
	CMD_SHARE_CONTACT			= 16,
	CMD_EXPORT_CONTACT			= 17,
	CMD_IMPORT_CONTACT			= 18,
	CMD_REBOOT					= 19,
	CMD_GET_BATT_AND_STORAGE	= 20,
	CMD_SET_TUNING_PARAMS		= 21,
	CMD_DEVICE_QUERY			= 22,
	CMD_SEND_RAW_DATA			= 25,
	CMD_SEND_LOGIN				= 26,
	CMD_SEND_STATUS_REQ			= 27,
	CMD_SEND_TRACE_PATH			= 36,
	CMD_SET_DEVICE_PIN			= 37,
	CMD_SET_OTHER_PARAMS		= 38,
	CMD_SEND_TELEMETRY_REQ		= 39,
	CMD_GET_CUSTOM_VARS			= 40,
	CMD_SET_CUSTOM_VAR			= 41,
	CMD_GET_ADVERT_PATH			= 42,
	CMD_GET_TUNING_PARAMS		= 43,
	CMD_SEND_BINARY_REQ			= 50,
	CMD_FACTORY_RESET			= 51,
	CMD_SEND_CONTROL_DATA		= 55,
	CMD_GET_STATS				= 56
};

// =============================================================================
// Response Codes (Radio -> App)
// =============================================================================

enum ResponseCode : uint8 {
	RESP_CODE_OK					= 0,
	RESP_CODE_ERR					= 1,
	RESP_CODE_CONTACTS_START		= 2,
	RESP_CODE_CONTACT				= 3,
	RESP_CODE_END_OF_CONTACTS		= 4,
	RESP_CODE_SELF_INFO				= 5,
	RESP_CODE_SENT					= 6,
	RESP_CODE_CONTACT_MSG_RECV		= 7,
	RESP_CODE_CHANNEL_MSG_RECV		= 8,
	RESP_CODE_CURR_TIME				= 9,
	RESP_CODE_NO_MORE_MESSAGES		= 10,
	RESP_CODE_EXPORT_CONTACT		= 11,
	RESP_CODE_BATT_AND_STORAGE		= 12,
	RESP_CODE_DEVICE_INFO			= 13,
	RESP_CODE_CONTACT_MSG_RECV_V3	= 16,
	RESP_CODE_CHANNEL_MSG_RECV_V3	= 17,
	RESP_CODE_TUNING_PARAMS			= 21,
	RESP_CODE_ADVERT_PATH			= 22,
	RESP_CODE_STATS					= 24
};

// =============================================================================
// Push Notification Codes (Radio -> App, async)
// =============================================================================

enum PushCode : uint8 {
	PUSH_CODE_ADVERT				= 0x80,
	PUSH_CODE_PATH_UPDATED			= 0x81,
	PUSH_CODE_SEND_CONFIRMED		= 0x82,
	PUSH_CODE_MSG_WAITING			= 0x83,
	PUSH_CODE_RAW_DATA				= 0x84,
	PUSH_CODE_LOGIN_SUCCESS			= 0x85,
	PUSH_CODE_LOGIN_FAIL			= 0x86,
	PUSH_CODE_STATUS_RESPONSE		= 0x87,
	PUSH_CODE_TRACE_DATA			= 0x89,
	PUSH_CODE_NEW_ADVERT			= 0x8A,
	PUSH_CODE_TELEMETRY_RESPONSE	= 0x8B,
	PUSH_CODE_BINARY_RESPONSE		= 0x8C,
	PUSH_CODE_CONTROL_DATA			= 0x8E
};

// =============================================================================
// Advertisement Types
// =============================================================================

enum AdvType : uint8 {
	ADV_TYPE_NONE		= 0,
	ADV_TYPE_CHAT		= 1,
	ADV_TYPE_REPEATER	= 2,
	ADV_TYPE_ROOM		= 3
};

// =============================================================================
// Text Types
// =============================================================================

enum TxtType : uint8 {
	TXT_TYPE_PLAIN			= 0,
	TXT_TYPE_CLI_DATA		= 1,
	TXT_TYPE_SIGNED_PLAIN	= 2
};

// =============================================================================
// Error Codes
// =============================================================================

enum ErrCode : uint8 {
	ERR_CODE_UNSUPPORTED_CMD	= 1,
	ERR_CODE_NOT_FOUND			= 2,
	ERR_CODE_TABLE_FULL			= 3,
	ERR_CODE_BAD_STATE			= 4,
	ERR_CODE_FILE_IO_ERROR		= 5,
	ERR_CODE_ILLEGAL_ARG		= 6
};

// =============================================================================
// Statistics Sub-types
// =============================================================================

enum StatsType : uint8 {
	STATS_TYPE_CORE		= 0,
	STATS_TYPE_RADIO	= 1,
	STATS_TYPE_PACKETS	= 2
};

// =============================================================================
// Size Constants
// =============================================================================

static const size_t kPublicKeySize		= 32;
static const size_t kPubKeyPrefixSize	= 6;
static const size_t kMaxPathLen			= 64;
static const size_t kMaxNameLen			= 32;
static const size_t kMaxMessageLen		= 160;

// =============================================================================
// Data Structures
// =============================================================================

// Device information (response to CMD_DEVICE_QUERY)
struct DeviceInfo {
	uint8	firmwareVersion;
	uint8	maxContactsDiv2;		// max_contacts = value * 2
	uint8	maxChannels;
	uint32	blePin;
	char	firmwareBuildDate[12];	// e.g. "19 Feb 2025"
	char	manufacturerModel[40];
	char	semanticVersion[20];
};

// Local node info (response to CMD_APP_START)
struct SelfInfo {
	uint8	type;					// AdvType
	uint8	txPowerDbm;
	uint8	maxTxPower;
	uint8	publicKey[kPublicKeySize];
	int32	advLat;					// latitude * 1E6
	int32	advLon;					// longitude * 1E6
	uint8	multiAcks;
	uint8	advertLocPolicy;
	uint8	telemetryModes;
	uint8	manualAddContacts;
	uint32	radioFreq;				// freq * 1000 (Hz)
	uint32	radioBw;				// bandwidth * 1000 (Hz)
	uint8	radioSf;				// spreading factor
	uint8	radioCr;				// coding rate
	char	name[kMaxNameLen];
};

// Contact entry
struct Contact {
	uint8	publicKey[kPublicKeySize];
	uint8	type;					// AdvType
	uint8	flags;
	int8	outPathLen;				// -1 if path unknown
	uint8	outPath[kMaxPathLen];
	char	advName[kMaxNameLen];
	uint32	lastAdvert;				// timestamp of last advert
	int32	advLat;
	int32	advLon;
	uint32	lastMod;				// timestamp of last modification
};

// Received message
struct ReceivedMessage {
	uint8	pubKeyPrefix[kPubKeyPrefixSize];
	uint8	pathLen;				// 0xFF = direct, otherwise hop count
	uint8	txtType;				// TxtType
	uint8	snr;					// SNR * 4 (v3 only)
	uint32	senderTimestamp;
	char	text[kMaxMessageLen + 1];
	bool	isChannel;
	uint8	channelIdx;
};

// Battery and storage status
struct BatteryAndStorage {
	uint16	milliVolts;
	uint32	usedKb;
	uint32	totalKb;
};

// Send confirmation
struct SendConfirmed {
	uint32	ackCode;
	uint32	roundTripMs;
};

// Radio parameters
struct RadioParams {
	uint32	freq;		// Hz
	uint32	bw;			// Hz
	uint8	sf;			// spreading factor (7-12)
	uint8	cr;			// coding rate (5-8)
};

// MeshCore Radio Presets
enum RadioPreset {
	PRESET_CUSTOM = 0,
	PRESET_MESHCORE_DEFAULT,	// 906.875 MHz, 250kHz, SF10, CR5
	PRESET_EU_868,				// 869.525 MHz, 125kHz, SF11, CR5
	PRESET_EU_868_NARROW,		// 869.525 MHz, 62.5kHz, SF9, CR8 (EU/UK Narrow)
	PRESET_EU_433,				// 433.875 MHz, 125kHz, SF9, CR5
	PRESET_US_915,				// 906.875 MHz, 250kHz, SF10, CR5
	PRESET_US_915_FAST,			// 906.875 MHz, 500kHz, SF7, CR5
	PRESET_AU_915,				// 915.0 MHz, 250kHz, SF10, CR5
	PRESET_NZ_915,				// 915.0 MHz, 125kHz, SF9, CR5
	PRESET_LONG_RANGE,			// 906.875 MHz, 62.5kHz, SF12, CR8
	PRESET_MEDIUM_RANGE,		// 906.875 MHz, 125kHz, SF10, CR5
	PRESET_FAST,				// 906.875 MHz, 500kHz, SF7, CR5
	PRESET_COUNT
};

struct RadioPresetInfo {
	const char*	name;
	uint32		freq;		// Hz
	uint32		bw;			// Hz
	uint8		sf;
	uint8		cr;
};

// Preset definitions (from MeshCore defaults)
static const RadioPresetInfo kRadioPresets[] = {
	{ "Custom",              0,         0,      0,  0 },
	{ "MeshCore Default",    906875000, 250000, 10, 5 },
	{ "EU 868 MHz",          869525000, 125000, 11, 5 },
	{ "EU/UK 868 Narrow",    869525000,  62500,  9, 8 },
	{ "EU 433 MHz",          433875000, 125000,  9, 5 },
	{ "US 915 MHz",          906875000, 250000, 10, 5 },
	{ "US 915 Fast",         906875000, 500000,  7, 5 },
	{ "AU 915 MHz",          915000000, 250000, 10, 5 },
	{ "NZ 915 MHz",          915000000, 125000,  9, 5 },
	{ "Long Range (Slow)",   906875000,  62500, 12, 8 },
	{ "Medium Range",        906875000, 125000, 10, 5 },
	{ "Fast (Short Range)",  906875000, 500000,  7, 5 },
};

// Tuning parameters
struct TuningParams {
	uint8	rxDelay;
	uint8	txWindow;
	uint8	airtime;
};

#endif // TYPES_H
