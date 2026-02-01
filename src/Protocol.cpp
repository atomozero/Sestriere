/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Protocol.cpp — MeshCore protocol encoder/decoder implementation
 */

#include "Protocol.h"

#include <cstdio>
#include <cstring>
#include <algorithm>


// =============================================================================
// Parsing Responses
// =============================================================================

bool
Protocol::ParseDeviceInfo(const uint8* data, size_t len, DeviceInfo& outInfo)
{
	// Response format:
	// [0] = RESP_CODE_DEVICE_INFO (13)
	// [1] = firmware_version
	// [2] = max_contacts_div2
	// [3] = max_channels
	// [4-7] = ble_pin (uint32 LE)
	// [8-19] = firmware_build_date (12 bytes)
	// [20+] = manufacturer_model (null-terminated)
	// [after] = semantic_version (null-terminated)

	if (len < 20)
		return false;

	if (data[0] != RESP_CODE_DEVICE_INFO)
		return false;

	memset(&outInfo, 0, sizeof(outInfo));

	outInfo.firmwareVersion = data[1];
	outInfo.maxContactsDiv2 = data[2];
	outInfo.maxChannels = data[3];
	outInfo.blePin = _ReadU32LE(data + 4);

	// Firmware build date (12 bytes, not necessarily null-terminated)
	_SafeStrCopy(outInfo.firmwareBuildDate, data + 8, 12,
		sizeof(outInfo.firmwareBuildDate));

	// Manufacturer/model string
	size_t pos = 20;
	size_t modelLen = strnlen((const char*)(data + pos), len - pos);
	_SafeStrCopy(outInfo.manufacturerModel, data + pos,
		std::min(modelLen, sizeof(outInfo.manufacturerModel) - 1),
		sizeof(outInfo.manufacturerModel));

	// Semantic version (after manufacturer string)
	pos += modelLen + 1;
	if (pos < len) {
		size_t verLen = strnlen((const char*)(data + pos), len - pos);
		_SafeStrCopy(outInfo.semanticVersion, data + pos,
			std::min(verLen, sizeof(outInfo.semanticVersion) - 1),
			sizeof(outInfo.semanticVersion));
	}

	return true;
}


bool
Protocol::ParseSelfInfo(const uint8* data, size_t len, SelfInfo& outInfo)
{
	// Response format:
	// [0] = RESP_CODE_SELF_INFO (5)
	// [1] = type (AdvType)
	// [2] = tx_power_dbm
	// [3] = max_tx_power
	// [4-35] = public_key (32 bytes)
	// [36-39] = adv_lat (int32 LE)
	// [40-43] = adv_lon (int32 LE)
	// [44] = multi_acks
	// [45] = advert_loc_policy
	// [46] = telemetry_modes
	// [47] = manual_add_contacts
	// [48-51] = radio_freq (uint32 LE)
	// [52-55] = radio_bw (uint32 LE)
	// [56] = radio_sf
	// [57] = radio_cr
	// [58+] = name (null-terminated)

	if (len < 58)
		return false;

	if (data[0] != RESP_CODE_SELF_INFO)
		return false;

	memset(&outInfo, 0, sizeof(outInfo));

	outInfo.type = data[1];
	outInfo.txPowerDbm = data[2];
	outInfo.maxTxPower = data[3];
	memcpy(outInfo.publicKey, data + 4, kPublicKeySize);
	outInfo.advLat = _ReadI32LE(data + 36);
	outInfo.advLon = _ReadI32LE(data + 40);
	outInfo.multiAcks = data[44];
	outInfo.advertLocPolicy = data[45];
	outInfo.telemetryModes = data[46];
	outInfo.manualAddContacts = data[47];
	outInfo.radioFreq = _ReadU32LE(data + 48);
	outInfo.radioBw = _ReadU32LE(data + 52);
	outInfo.radioSf = data[56];
	outInfo.radioCr = data[57];

	// Name
	if (len > 58) {
		size_t nameLen = strnlen((const char*)(data + 58), len - 58);
		_SafeStrCopy(outInfo.name, data + 58,
			std::min(nameLen, sizeof(outInfo.name) - 1),
			sizeof(outInfo.name));
	}

	return true;
}


bool
Protocol::ParseContact(const uint8* data, size_t len, Contact& outContact)
{
	// Response format:
	// [0] = RESP_CODE_CONTACT (3)
	// [1-32] = public_key (32 bytes)
	// [33] = type
	// [34] = flags
	// [35] = out_path_len (-1 if unknown)
	// [36-99] = out_path (64 bytes)
	// [100-131] = adv_name (32 bytes)
	// [132-135] = last_advert (uint32 LE)
	// [136-139] = adv_lat (int32 LE)
	// [140-143] = adv_lon (int32 LE)
	// [144-147] = last_mod (uint32 LE)

	if (len < 148)
		return false;

	if (data[0] != RESP_CODE_CONTACT)
		return false;

	memset(&outContact, 0, sizeof(outContact));

	memcpy(outContact.publicKey, data + 1, kPublicKeySize);
	outContact.type = data[33];
	outContact.flags = data[34];
	outContact.outPathLen = (int8)data[35];
	memcpy(outContact.outPath, data + 36, kMaxPathLen);

	_SafeStrCopy(outContact.advName, data + 100, kMaxNameLen,
		sizeof(outContact.advName));

	outContact.lastAdvert = _ReadU32LE(data + 132);
	outContact.advLat = _ReadI32LE(data + 136);
	outContact.advLon = _ReadI32LE(data + 140);
	outContact.lastMod = _ReadU32LE(data + 144);

	return true;
}


bool
Protocol::ParseReceivedMessage(const uint8* data, size_t len,
	ReceivedMessage& outMsg)
{
	// RESP_CODE_CONTACT_MSG_RECV (7) or RESP_CODE_CHANNEL_MSG_RECV (8):
	// [0] = code
	// [1-6] = pub_key_prefix (6 bytes)
	// [7] = path_len (0xFF = direct)
	// [8] = txt_type
	// [9-12] = sender_timestamp (uint32 LE)
	// [13+] = text (null-terminated)
	//
	// V3 versions (16, 17) add SNR:
	// [13] = snr
	// [14+] = text

	if (len < 13)
		return false;

	uint8 code = data[0];
	bool isV3 = (code == RESP_CODE_CONTACT_MSG_RECV_V3 ||
		code == RESP_CODE_CHANNEL_MSG_RECV_V3);

	memset(&outMsg, 0, sizeof(outMsg));

	memcpy(outMsg.pubKeyPrefix, data + 1, kPubKeyPrefixSize);
	outMsg.pathLen = data[7];
	outMsg.txtType = data[8];
	outMsg.senderTimestamp = _ReadU32LE(data + 9);

	size_t textOffset = 13;
	if (isV3) {
		if (len < 14)
			return false;
		outMsg.snr = data[13];
		textOffset = 14;
	}

	if (len > textOffset) {
		size_t textLen = strnlen((const char*)(data + textOffset),
			len - textOffset);
		_SafeStrCopy(outMsg.text, data + textOffset,
			std::min(textLen, (size_t)kMaxMessageLen),
			sizeof(outMsg.text));
	}

	outMsg.isChannel = (code == RESP_CODE_CHANNEL_MSG_RECV ||
		code == RESP_CODE_CHANNEL_MSG_RECV_V3);

	return true;
}


bool
Protocol::ParseBatteryAndStorage(const uint8* data, size_t len,
	BatteryAndStorage& outBatt)
{
	// Response format:
	// [0] = RESP_CODE_BATT_AND_STORAGE (12)
	// [1-2] = millivolts (uint16 LE)
	// [3-6] = used_kb (uint32 LE)
	// [7-10] = total_kb (uint32 LE)

	if (len < 11)
		return false;

	if (data[0] != RESP_CODE_BATT_AND_STORAGE)
		return false;

	outBatt.milliVolts = _ReadU16LE(data + 1);
	outBatt.usedKb = _ReadU32LE(data + 3);
	outBatt.totalKb = _ReadU32LE(data + 7);

	return true;
}


bool
Protocol::ParseSendConfirmed(const uint8* data, size_t len,
	SendConfirmed& outConfirm)
{
	// Push format:
	// [0] = PUSH_CODE_SEND_CONFIRMED (0x82)
	// [1-4] = ack_code (uint32 LE)
	// [5-8] = round_trip_ms (uint32 LE)

	if (len < 9)
		return false;

	if (data[0] != PUSH_CODE_SEND_CONFIRMED)
		return false;

	outConfirm.ackCode = _ReadU32LE(data + 1);
	outConfirm.roundTripMs = _ReadU32LE(data + 5);

	return true;
}


// =============================================================================
// Building Commands
// =============================================================================

size_t
Protocol::BuildDeviceQuery(uint8 appVersion, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_DEVICE_QUERY (22)
	// [1] = app_version

	outBuffer[0] = CMD_DEVICE_QUERY;
	outBuffer[1] = appVersion;
	return 2;
}


size_t
Protocol::BuildAppStart(uint8 appVersion, const char* appName, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_APP_START (1)
	// [1] = app_version
	// [2+] = app_name (null-terminated)

	outBuffer[0] = CMD_APP_START;
	outBuffer[1] = appVersion;

	size_t nameLen = strlen(appName);
	if (nameLen > 31)
		nameLen = 31;

	memcpy(outBuffer + 2, appName, nameLen);
	outBuffer[2 + nameLen] = '\0';

	return 3 + nameLen;
}


size_t
Protocol::BuildGetContacts(uint32 since, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_GET_CONTACTS (4)
	// [1-4] = since (uint32 LE)

	outBuffer[0] = CMD_GET_CONTACTS;
	_WriteU32LE(outBuffer + 1, since);
	return 5;
}


size_t
Protocol::BuildSyncNextMessage(uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SYNC_NEXT_MESSAGE (10)

	outBuffer[0] = CMD_SYNC_NEXT_MESSAGE;
	return 1;
}


size_t
Protocol::BuildSendTextMessage(const uint8* pubKeyPrefix, const char* text,
	uint32 timestamp, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SEND_TXT_MSG (2)
	// [1-6] = pub_key_prefix (6 bytes)
	// [7] = txt_type (0 = plain)
	// [8-11] = timestamp (uint32 LE)
	// [12+] = text (null-terminated)

	outBuffer[0] = CMD_SEND_TXT_MSG;
	memcpy(outBuffer + 1, pubKeyPrefix, kPubKeyPrefixSize);
	outBuffer[7] = TXT_TYPE_PLAIN;
	_WriteU32LE(outBuffer + 8, timestamp);

	size_t textLen = strlen(text);
	if (textLen > kMaxMessageLen)
		textLen = kMaxMessageLen;

	memcpy(outBuffer + 12, text, textLen);
	outBuffer[12 + textLen] = '\0';

	return 13 + textLen;
}


size_t
Protocol::BuildSendChannelMessage(const char* text, uint32 timestamp,
	uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SEND_CHANNEL_TXT_MSG (3)
	// [1] = channel_idx (0 = public)
	// [2] = txt_type (0 = plain)
	// [3-6] = timestamp (uint32 LE)
	// [7+] = text (null-terminated)

	outBuffer[0] = CMD_SEND_CHANNEL_TXT_MSG;
	outBuffer[1] = 0;  // public channel
	outBuffer[2] = TXT_TYPE_PLAIN;
	_WriteU32LE(outBuffer + 3, timestamp);

	size_t textLen = strlen(text);
	if (textLen > kMaxMessageLen)
		textLen = kMaxMessageLen;

	memcpy(outBuffer + 7, text, textLen);
	outBuffer[7 + textLen] = '\0';

	return 8 + textLen;
}


size_t
Protocol::BuildGetBatteryAndStorage(uint8* outBuffer)
{
	outBuffer[0] = CMD_GET_BATT_AND_STORAGE;
	return 1;
}


size_t
Protocol::BuildSendAdvert(bool flood, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SEND_SELF_ADVERT (7)
	// [1] = flood (0 = zero-hop, 1 = flood)

	outBuffer[0] = CMD_SEND_SELF_ADVERT;
	outBuffer[1] = flood ? 1 : 0;
	return 2;
}


size_t
Protocol::BuildSetDeviceTime(uint32 epochSecs, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SET_DEVICE_TIME (6)
	// [1-4] = epoch_secs (uint32 LE)

	outBuffer[0] = CMD_SET_DEVICE_TIME;
	_WriteU32LE(outBuffer + 1, epochSecs);
	return 5;
}


size_t
Protocol::BuildSetAdvertName(const char* name, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SET_ADVERT_NAME (8)
	// [1+] = name (null-terminated)

	outBuffer[0] = CMD_SET_ADVERT_NAME;

	size_t nameLen = strlen(name);
	if (nameLen > kMaxNameLen - 1)
		nameLen = kMaxNameLen - 1;

	memcpy(outBuffer + 1, name, nameLen);
	outBuffer[1 + nameLen] = '\0';

	return 2 + nameLen;
}


size_t
Protocol::BuildSetRadioParams(const RadioParams& params, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SET_RADIO_PARAMS (11)
	// [1-4] = freq (uint32 LE)
	// [5-8] = bw (uint32 LE)
	// [9] = sf
	// [10] = cr

	outBuffer[0] = CMD_SET_RADIO_PARAMS;
	_WriteU32LE(outBuffer + 1, params.freq);
	_WriteU32LE(outBuffer + 5, params.bw);
	outBuffer[9] = params.sf;
	outBuffer[10] = params.cr;
	return 11;
}


size_t
Protocol::BuildSetTxPower(uint8 powerDbm, uint8* outBuffer)
{
	// Command format:
	// [0] = CMD_SET_RADIO_TX_POWER (12)
	// [1] = power_dbm

	outBuffer[0] = CMD_SET_RADIO_TX_POWER;
	outBuffer[1] = powerDbm;
	return 2;
}


size_t
Protocol::BuildReboot(uint8* outBuffer)
{
	outBuffer[0] = CMD_REBOOT;
	return 1;
}


// =============================================================================
// Utility Functions
// =============================================================================

const char*
Protocol::GetAdvTypeName(uint8 type)
{
	switch (type) {
		case ADV_TYPE_NONE:		return "None";
		case ADV_TYPE_CHAT:		return "Chat";
		case ADV_TYPE_REPEATER:	return "Repeater";
		case ADV_TYPE_ROOM:		return "Room";
		default:				return "Unknown";
	}
}


const char*
Protocol::GetErrorName(uint8 errCode)
{
	switch (errCode) {
		case ERR_CODE_UNSUPPORTED_CMD:	return "Unsupported command";
		case ERR_CODE_NOT_FOUND:		return "Not found";
		case ERR_CODE_TABLE_FULL:		return "Table full";
		case ERR_CODE_BAD_STATE:		return "Bad state";
		case ERR_CODE_FILE_IO_ERROR:	return "File I/O error";
		case ERR_CODE_ILLEGAL_ARG:		return "Illegal argument";
		default:						return "Unknown error";
	}
}


const char*
Protocol::GetResponseName(uint8 code)
{
	switch (code) {
		case RESP_CODE_OK:					return "OK";
		case RESP_CODE_ERR:					return "Error";
		case RESP_CODE_CONTACTS_START:		return "Contacts Start";
		case RESP_CODE_CONTACT:				return "Contact";
		case RESP_CODE_END_OF_CONTACTS:		return "End of Contacts";
		case RESP_CODE_SELF_INFO:			return "Self Info";
		case RESP_CODE_SENT:				return "Sent";
		case RESP_CODE_CONTACT_MSG_RECV:	return "Contact Message";
		case RESP_CODE_CHANNEL_MSG_RECV:	return "Channel Message";
		case RESP_CODE_CURR_TIME:			return "Current Time";
		case RESP_CODE_NO_MORE_MESSAGES:	return "No More Messages";
		case RESP_CODE_EXPORT_CONTACT:		return "Export Contact";
		case RESP_CODE_BATT_AND_STORAGE:	return "Battery & Storage";
		case RESP_CODE_DEVICE_INFO:			return "Device Info";
		case RESP_CODE_CONTACT_MSG_RECV_V3:	return "Contact Message V3";
		case RESP_CODE_CHANNEL_MSG_RECV_V3:	return "Channel Message V3";
		case RESP_CODE_TUNING_PARAMS:		return "Tuning Params";
		case RESP_CODE_ADVERT_PATH:			return "Advert Path";
		case RESP_CODE_STATS:				return "Stats";
		default:							return "Unknown";
	}
}


const char*
Protocol::GetPushName(uint8 code)
{
	switch (code) {
		case PUSH_CODE_ADVERT:				return "Advertisement";
		case PUSH_CODE_PATH_UPDATED:		return "Path Updated";
		case PUSH_CODE_SEND_CONFIRMED:		return "Send Confirmed";
		case PUSH_CODE_MSG_WAITING:			return "Message Waiting";
		case PUSH_CODE_RAW_DATA:			return "Raw Data";
		case PUSH_CODE_LOGIN_SUCCESS:		return "Login Success";
		case PUSH_CODE_LOGIN_FAIL:			return "Login Fail";
		case PUSH_CODE_STATUS_RESPONSE:		return "Status Response";
		case PUSH_CODE_TRACE_DATA:			return "Trace Data";
		case PUSH_CODE_NEW_ADVERT:			return "New Advertisement";
		case PUSH_CODE_TELEMETRY_RESPONSE:	return "Telemetry Response";
		case PUSH_CODE_BINARY_RESPONSE:		return "Binary Response";
		case PUSH_CODE_CONTROL_DATA:		return "Control Data";
		default:							return "Unknown";
	}
}


void
Protocol::FormatPublicKey(const uint8* key, char* outStr, size_t outSize)
{
	if (outSize < 65) {
		if (outSize > 0)
			outStr[0] = '\0';
		return;
	}

	for (size_t i = 0; i < kPublicKeySize; i++)
		snprintf(outStr + i * 2, 3, "%02X", key[i]);
}


void
Protocol::FormatPubKeyPrefix(const uint8* prefix, char* outStr, size_t outSize)
{
	if (outSize < 13) {
		if (outSize > 0)
			outStr[0] = '\0';
		return;
	}

	for (size_t i = 0; i < kPubKeyPrefixSize; i++)
		snprintf(outStr + i * 2, 3, "%02X", prefix[i]);
}


float
Protocol::LatLonFromInt(int32 value)
{
	return value / 1000000.0f;
}


int32
Protocol::LatLonToInt(float value)
{
	return (int32)(value * 1000000.0f);
}


// =============================================================================
// Public Byte Reading Utilities
// =============================================================================

uint16
Protocol::ReadU16LE(const uint8* data)
{
	return data[0] | (data[1] << 8);
}


uint32
Protocol::ReadU32LE(const uint8* data)
{
	return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}


int32
Protocol::ReadI32LE(const uint8* data)
{
	return (int32)ReadU32LE(data);
}


// =============================================================================
// Private Helper Functions
// =============================================================================

uint16
Protocol::_ReadU16LE(const uint8* data)
{
	return data[0] | (data[1] << 8);
}


uint32
Protocol::_ReadU32LE(const uint8* data)
{
	return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}


int32
Protocol::_ReadI32LE(const uint8* data)
{
	return (int32)_ReadU32LE(data);
}


void
Protocol::_WriteU16LE(uint8* data, uint16 value)
{
	data[0] = value & 0xFF;
	data[1] = (value >> 8) & 0xFF;
}


void
Protocol::_WriteU32LE(uint8* data, uint32 value)
{
	data[0] = value & 0xFF;
	data[1] = (value >> 8) & 0xFF;
	data[2] = (value >> 16) & 0xFF;
	data[3] = (value >> 24) & 0xFF;
}


void
Protocol::_WriteI32LE(uint8* data, int32 value)
{
	_WriteU32LE(data, (uint32)value);
}


size_t
Protocol::_SafeStrCopy(char* dest, const uint8* src, size_t srcLen,
	size_t destSize)
{
	if (destSize == 0)
		return 0;

	size_t copyLen = std::min(srcLen, destSize - 1);
	memcpy(dest, src, copyLen);
	dest[copyLen] = '\0';

	return copyLen;
}
