/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ProtocolHandler.cpp — MeshCore Companion Protocol V3 command builder
 */

#include "ProtocolHandler.h"

#include "SerialHandler.h"

#include <OS.h>

#include <cstdio>
#include <cstring>
#include <ctime>


ProtocolHandler::ProtocolHandler(SerialHandler* serial)
	:
	fSerial(serial)
{
}


void
ProtocolHandler::SetSerialHandler(SerialHandler* serial)
{
	fSerial = serial;
}


bool
ProtocolHandler::IsConnected() const
{
	return fSerial != NULL && fSerial->IsConnected();
}


// =============================================================================
// App lifecycle
// =============================================================================


status_t
ProtocolHandler::SendAppStart()
{
	// CMD_APP_START: [0]=code [1]=app_ver [2-7]=reserved [8+]=app_name
	uint8 payload[32];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_APP_START;
	payload[1] = 3;  // V3 protocol
	const char* appName = APP_NAME;
	size_t nameLen = strlen(appName);
	if (nameLen > sizeof(payload) - 9)
		nameLen = sizeof(payload) - 9;
	memcpy(payload + 8, appName, nameLen);
	return fSerial->SendFrame(payload, 8 + nameLen);
}


status_t
ProtocolHandler::SendDeviceQuery()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[2];
	payload[0] = CMD_DEVICE_QUERY;
	payload[1] = 3;  // Request V3
	return fSerial->SendFrame(payload, 2);
}


status_t
ProtocolHandler::SendExportSelf()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_EXPORT_CONTACT;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendGetContacts()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_GET_CONTACTS;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendSelfAdvert()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_SEND_SELF_ADVERT;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendSyncNextMessage()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_SYNC_NEXT_MESSAGE;
	return fSerial->SendFrame(payload, 1);
}


// =============================================================================
// Device info
// =============================================================================


status_t
ProtocolHandler::SendGetBattery()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_GET_BATT_AND_STORAGE;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendGetStats()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[2];
	payload[0] = CMD_GET_STATS;

	// Request all three stat subtypes
	payload[1] = 0;  // Core
	fSerial->SendFrame(payload, 2);
	payload[1] = 1;  // Radio
	fSerial->SendFrame(payload, 2);
	payload[1] = 2;  // Packets
	return fSerial->SendFrame(payload, 2);
}


status_t
ProtocolHandler::SendGetDeviceTime()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_GET_DEVICE_TIME;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendSetDeviceTime(uint32 epoch)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[5];
	payload[0] = CMD_SET_DEVICE_TIME;
	payload[1] = epoch & 0xFF;
	payload[2] = (epoch >> 8) & 0xFF;
	payload[3] = (epoch >> 16) & 0xFF;
	payload[4] = (epoch >> 24) & 0xFF;
	return fSerial->SendFrame(payload, 5);
}


status_t
ProtocolHandler::SendReboot()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_REBOOT;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendFactoryReset()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_FACTORY_RESET;
	return fSerial->SendFrame(payload, 1);
}


// =============================================================================
// Radio configuration
// =============================================================================


status_t
ProtocolHandler::SendRadioParams(uint32 freqHz, uint32 bwHz, uint8 sf, uint8 cr)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	// Protocol wire format: frequency in kHz, bandwidth in Hz
	uint32 freqKHz = freqHz / 1000;

	uint8 payload[11];
	payload[0] = CMD_SET_RADIO_PARAMS;
	payload[1] = freqKHz & 0xFF;
	payload[2] = (freqKHz >> 8) & 0xFF;
	payload[3] = (freqKHz >> 16) & 0xFF;
	payload[4] = (freqKHz >> 24) & 0xFF;
	payload[5] = bwHz & 0xFF;
	payload[6] = (bwHz >> 8) & 0xFF;
	payload[7] = (bwHz >> 16) & 0xFF;
	payload[8] = (bwHz >> 24) & 0xFF;
	payload[9] = sf;
	payload[10] = cr;
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendSetTxPower(uint8 power)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[2];
	payload[0] = CMD_SET_RADIO_TX_POWER;
	payload[1] = power;
	return fSerial->SendFrame(payload, 2);
}


status_t
ProtocolHandler::SendGetTuningParams()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_GET_TUNING_PARAMS;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendSetTuningParams(uint32 rxDelayBase, uint32 airtimeFactor)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[17];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_SET_TUNING_PARAMS;
	payload[1] = rxDelayBase & 0xFF;
	payload[2] = (rxDelayBase >> 8) & 0xFF;
	payload[3] = (rxDelayBase >> 16) & 0xFF;
	payload[4] = (rxDelayBase >> 24) & 0xFF;
	payload[5] = airtimeFactor & 0xFF;
	payload[6] = (airtimeFactor >> 8) & 0xFF;
	payload[7] = (airtimeFactor >> 16) & 0xFF;
	payload[8] = (airtimeFactor >> 24) & 0xFF;
	return fSerial->SendFrame(payload, sizeof(payload));
}


// =============================================================================
// Node identity
// =============================================================================


status_t
ProtocolHandler::SendSetName(const char* name)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	size_t nameLen = strlen(name);
	if (nameLen > 31)
		nameLen = 31;

	uint8 payload[33];
	payload[0] = CMD_SET_ADVERT_NAME;
	memcpy(payload + 1, name, nameLen);
	payload[1 + nameLen] = '\0';
	return fSerial->SendFrame(payload, 2 + nameLen);
}


status_t
ProtocolHandler::SendSetLatLon(double lat, double lon)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	int32 latInt = (int32)(lat * 1000000.0);
	int32 lonInt = (int32)(lon * 1000000.0);

	uint8 payload[9];
	payload[0] = CMD_SET_ADVERT_LATLON;
	payload[1] = latInt & 0xFF;
	payload[2] = (latInt >> 8) & 0xFF;
	payload[3] = (latInt >> 16) & 0xFF;
	payload[4] = (latInt >> 24) & 0xFF;
	payload[5] = lonInt & 0xFF;
	payload[6] = (lonInt >> 8) & 0xFF;
	payload[7] = (lonInt >> 16) & 0xFF;
	payload[8] = (lonInt >> 24) & 0xFF;
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendOtherParams(uint8 manualAdd, uint8 telemetry,
	uint8 locPolicy, uint8 multiAcks)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[5];
	payload[0] = CMD_SET_OTHER_PARAMS;
	payload[1] = manualAdd;
	payload[2] = telemetry;
	payload[3] = locPolicy;
	payload[4] = multiAcks;
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendSetDevicePin(uint32 pin)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[5];
	payload[0] = CMD_SET_DEVICE_PIN;
	payload[1] = pin & 0xFF;
	payload[2] = (pin >> 8) & 0xFF;
	payload[3] = (pin >> 16) & 0xFF;
	payload[4] = (pin >> 24) & 0xFF;
	return fSerial->SendFrame(payload, 5);
}


// =============================================================================
// Contact management
// =============================================================================


status_t
ProtocolHandler::SendResetPath(const uint8* pubkey)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1 + kPubKeySize];
	payload[0] = CMD_RESET_PATH;
	memcpy(payload + 1, pubkey, kPubKeySize);
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendRemoveContact(const uint8* pubkey)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1 + kPubKeySize];
	payload[0] = CMD_REMOVE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendAddUpdateContact(const uint8* pubkey, const char* name,
	uint8 type)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	// Frame: [CMD][pubkey*32][type][flags][outPathLen][outPath*64][name*32]
	// [lastAdvert*4][lat*4][lon*4] = 144 bytes
	uint8 payload[144];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_ADD_UPDATE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	payload[33] = type;
	payload[34] = 0;    // flags
	payload[35] = 0xFF; // outPathLen (unknown)
	size_t nameLen = strlen(name);
	if (nameLen > 31)
		nameLen = 31;
	memcpy(payload + 100, name, nameLen);
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendShareContact(const uint8* pubkey)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[33];
	payload[0] = CMD_SHARE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	return fSerial->SendFrame(payload, 33);
}


// =============================================================================
// Messaging (frame-level only)
// =============================================================================


status_t
ProtocolHandler::SendDM(const uint8* pubkeyPrefix, uint8 txtType,
	uint32 timestamp, const char* text, size_t textLen)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[256];
	size_t pos = 0;

	payload[pos++] = CMD_SEND_TXT_MSG;
	payload[pos++] = txtType;
	payload[pos++] = 0;  // attempt

	payload[pos++] = timestamp & 0xFF;
	payload[pos++] = (timestamp >> 8) & 0xFF;
	payload[pos++] = (timestamp >> 16) & 0xFF;
	payload[pos++] = (timestamp >> 24) & 0xFF;

	memcpy(payload + pos, pubkeyPrefix, kPubKeyPrefixSize);
	pos += kPubKeyPrefixSize;

	memcpy(payload + pos, text, textLen);
	pos += textLen;

	return fSerial->SendFrame(payload, pos);
}


status_t
ProtocolHandler::SendChannelMsg(uint8 channelIdx, uint32 timestamp,
	const char* text, size_t textLen)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[256];
	size_t pos = 0;

	payload[pos++] = CMD_SEND_CHANNEL_TXT_MSG;
	payload[pos++] = 0;  // txt_type: plain
	payload[pos++] = channelIdx;

	payload[pos++] = timestamp & 0xFF;
	payload[pos++] = (timestamp >> 8) & 0xFF;
	payload[pos++] = (timestamp >> 16) & 0xFF;
	payload[pos++] = (timestamp >> 24) & 0xFF;

	memcpy(payload + pos, text, textLen);
	pos += textLen;

	return fSerial->SendFrame(payload, pos);
}


// =============================================================================
// Login and admin
// =============================================================================


status_t
ProtocolHandler::SendLogin(const uint8* pubkey, const char* password)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	size_t passLen = strlen(password);
	if (passLen > 15)
		passLen = 15;  // Protocol max

	uint8 payload[128];
	payload[0] = CMD_SEND_LOGIN;
	memcpy(payload + 1, pubkey, kPubKeySize);  // Full 32-byte key
	memcpy(payload + 33, password, passLen);
	payload[33 + passLen] = '\0';
	return fSerial->SendFrame(payload, 34 + passLen);
}


status_t
ProtocolHandler::SendStatusRequest(const uint8* pubkey)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[33];
	payload[0] = CMD_SEND_STATUS_REQ;
	memcpy(&payload[1], pubkey, 32);
	return fSerial->SendFrame(payload, 33);
}


status_t
ProtocolHandler::SendTelemetryRequest(const uint8* pubkey)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	// V3: [CMD][reserved*3][pubkey*32] = 36 bytes
	uint8 payload[36];
	payload[0] = CMD_SEND_TELEMETRY_REQ;
	payload[1] = 0;
	payload[2] = 0;
	payload[3] = 0;
	memcpy(&payload[4], pubkey, 32);
	return fSerial->SendFrame(payload, 36);
}


status_t
ProtocolHandler::SendTracePath(const ContactInfo* contact)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	if (contact == NULL)
		return B_BAD_VALUE;

	// Cannot trace a direct-path contact (no intermediate hops)
	if (contact->outPathLen <= 0)
		return B_BAD_VALUE;

	// Frame: [CMD][tag:4 LE][auth_code:4 LE][flags:1][symmetric_path]
	// tag: random 32-bit to match PUSH_TRACE_DATA response
	// auth_code: 0
	// flags & 0x03: path_hash_mode (2 = 4-byte hash, V3 standard)
	// symmetric_path: built from outPath reversed + mirrored

	uint8 payload[256];
	size_t pos = 0;

	payload[pos++] = CMD_SEND_TRACE_PATH;

	// tag = lower 32 bits of system_time (unique enough to match responses)
	uint32 tag = (uint32)system_time();
	payload[pos++] = tag & 0xFF;
	payload[pos++] = (tag >> 8) & 0xFF;
	payload[pos++] = (tag >> 16) & 0xFF;
	payload[pos++] = (tag >> 24) & 0xFF;

	// auth_code = 0
	payload[pos++] = 0;
	payload[pos++] = 0;
	payload[pos++] = 0;
	payload[pos++] = 0;

	// flags: path_hash_mode = 0 (1-byte hashes, as stored in contact frame)
	payload[pos++] = 0;

	// outPath stores 1-byte hop hashes in dest→source order: [h0, h1, h2]
	// Reverse to source→dest: [h2, h1, h0]
	// Symmetric path: [h2, h1, h0, h1, h2] (forward + mirror)
	int numHops = contact->outPathLen;
	if (numHops > 16)
		numHops = 16;

	// Reverse the path (dest→source → source→dest)
	uint8 reversed[16];
	for (int i = 0; i < numHops; i++)
		reversed[i] = contact->outPath[numHops - 1 - i];

	// For repeater/room (type 2/3): prepend destination hash
	if (contact->type == 2 || contact->type == 3) {
		if (pos + 1 > sizeof(payload))
			return B_NO_MEMORY;
		// 1-byte hash of destination pubkey
		payload[pos++] = contact->publicKey[0];
	}

	// Build symmetric path: [r0, r1, ..., r(n-1), r(n-2), ..., r0]
	for (int i = 0; i < numHops; i++) {
		if (pos + 1 > sizeof(payload))
			return B_NO_MEMORY;
		payload[pos++] = reversed[i];
	}
	for (int i = numHops - 2; i >= 0; i--) {
		if (pos + 1 > sizeof(payload))
			return B_NO_MEMORY;
		payload[pos++] = reversed[i];
	}

	return fSerial->SendFrame(payload, pos);
}


// =============================================================================
// Channels
// =============================================================================


status_t
ProtocolHandler::SendGetChannel(uint8 index)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[2];
	payload[0] = CMD_GET_CHANNEL;
	payload[1] = index;
	return fSerial->SendFrame(payload, 2);
}


status_t
ProtocolHandler::SendSetChannel(uint8 index, const char* name,
	const uint8* secret)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[50];
	payload[0] = CMD_SET_CHANNEL;
	payload[1] = index;
	memset(payload + 2, 0, 32);
	strlcpy((char*)(payload + 2), name, 32);
	memcpy(payload + 34, secret, 16);
	return fSerial->SendFrame(payload, sizeof(payload));
}


status_t
ProtocolHandler::SendRemoveChannel(uint8 index)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[50];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_SET_CHANNEL;
	payload[1] = index;
	return fSerial->SendFrame(payload, sizeof(payload));
}


// =============================================================================
// Advanced
// =============================================================================


status_t
ProtocolHandler::SendRawData(const uint8* rawPayload, size_t rawLength)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	if (rawLength > kMaxFramePayload - 1)
		return B_BAD_VALUE;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SEND_RAW_DATA;
	memcpy(payload + 1, rawPayload, rawLength);
	return fSerial->SendFrame(payload, 1 + rawLength);
}


status_t
ProtocolHandler::SendGetCustomVars()
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	uint8 payload[1];
	payload[0] = CMD_GET_CUSTOM_VARS;
	return fSerial->SendFrame(payload, 1);
}


status_t
ProtocolHandler::SendSetCustomVar(const char* nameValue)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	size_t len = strlen(nameValue);
	if (len > kMaxFramePayload - 2)
		return B_BAD_VALUE;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SET_CUSTOM_VAR;
	memcpy(payload + 1, nameValue, len);
	payload[1 + len] = '\0';
	return fSerial->SendFrame(payload, 2 + len);
}


status_t
ProtocolHandler::SendGetAdvertPath(const uint8* pubkey)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	// Frame: [CMD][reserved=0][pubkey*32] = 34 bytes
	uint8 payload[34];
	payload[0] = CMD_GET_ADVERT_PATH;
	payload[1] = 0;
	memcpy(payload + 2, pubkey, kPubKeySize);
	return fSerial->SendFrame(payload, 34);
}


status_t
ProtocolHandler::SendBinaryRequest(const uint8* pubkey, const uint8* reqData,
	size_t reqLength)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	if (reqLength > kMaxFramePayload - 33)
		return B_BAD_VALUE;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SEND_BINARY_REQ;
	memcpy(payload + 1, pubkey, kPubKeySize);
	if (reqLength > 0)
		memcpy(payload + 33, reqData, reqLength);
	return fSerial->SendFrame(payload, 33 + reqLength);
}


status_t
ProtocolHandler::SendControlData(uint8 subType, const uint8* ctrlPayload,
	size_t ctrlLength)
{
	if (!IsConnected())
		return B_NOT_INITIALIZED;

	if (ctrlLength > kMaxFramePayload - 3)
		return B_BAD_VALUE;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SEND_CONTROL_DATA;
	payload[1] = 0; // flags
	payload[2] = subType;
	if (ctrlLength > 0)
		memcpy(payload + 3, ctrlPayload, ctrlLength);
	return fSerial->SendFrame(payload, 3 + ctrlLength);
}
