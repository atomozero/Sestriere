/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ProtocolHandler.cpp — MeshCore Companion Protocol V3 command builder
 */

#include "ProtocolHandler.h"

#include "SerialHandler.h"

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


void
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
	fSerial->SendFrame(payload, 8 + nameLen);
}


void
ProtocolHandler::SendDeviceQuery()
{
	if (!IsConnected())
		return;

	uint8 payload[2];
	payload[0] = CMD_DEVICE_QUERY;
	payload[1] = 3;  // Request V3
	fSerial->SendFrame(payload, 2);
}


void
ProtocolHandler::SendExportSelf()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_EXPORT_CONTACT;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendGetContacts()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_GET_CONTACTS;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendSelfAdvert()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_SEND_SELF_ADVERT;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendSyncNextMessage()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_SYNC_NEXT_MESSAGE;
	fSerial->SendFrame(payload, 1);
}


// =============================================================================
// Device info
// =============================================================================


void
ProtocolHandler::SendGetBattery()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_GET_BATT_AND_STORAGE;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendGetStats()
{
	if (!IsConnected())
		return;

	uint8 payload[2];
	payload[0] = CMD_GET_STATS;

	// Request all three stat subtypes
	payload[1] = 0;  // Core
	fSerial->SendFrame(payload, 2);
	payload[1] = 1;  // Radio
	fSerial->SendFrame(payload, 2);
	payload[1] = 2;  // Packets
	fSerial->SendFrame(payload, 2);
}


void
ProtocolHandler::SendGetDeviceTime()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_GET_DEVICE_TIME;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendSetDeviceTime(uint32 epoch)
{
	if (!IsConnected())
		return;

	uint8 payload[5];
	payload[0] = CMD_SET_DEVICE_TIME;
	payload[1] = epoch & 0xFF;
	payload[2] = (epoch >> 8) & 0xFF;
	payload[3] = (epoch >> 16) & 0xFF;
	payload[4] = (epoch >> 24) & 0xFF;
	fSerial->SendFrame(payload, 5);
}


void
ProtocolHandler::SendReboot()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_REBOOT;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendFactoryReset()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_FACTORY_RESET;
	fSerial->SendFrame(payload, 1);
}


// =============================================================================
// Radio configuration
// =============================================================================


void
ProtocolHandler::SendRadioParams(uint32 freqHz, uint32 bwHz, uint8 sf, uint8 cr)
{
	if (!IsConnected())
		return;

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
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendSetTxPower(uint8 power)
{
	if (!IsConnected())
		return;

	uint8 payload[2];
	payload[0] = CMD_SET_RADIO_TX_POWER;
	payload[1] = power;
	fSerial->SendFrame(payload, 2);
}


void
ProtocolHandler::SendGetTuningParams()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_GET_TUNING_PARAMS;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendSetTuningParams(uint32 rxDelayBase, uint32 airtimeFactor)
{
	if (!IsConnected())
		return;

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
	fSerial->SendFrame(payload, sizeof(payload));
}


// =============================================================================
// Node identity
// =============================================================================


void
ProtocolHandler::SendSetName(const char* name)
{
	if (!IsConnected())
		return;

	size_t nameLen = strlen(name);
	if (nameLen > 31)
		nameLen = 31;

	uint8 payload[33];
	payload[0] = CMD_SET_ADVERT_NAME;
	memcpy(payload + 1, name, nameLen);
	payload[1 + nameLen] = '\0';
	fSerial->SendFrame(payload, 2 + nameLen);
}


void
ProtocolHandler::SendSetLatLon(double lat, double lon)
{
	if (!IsConnected())
		return;

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
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendOtherParams(uint8 manualAdd, uint8 telemetry,
	uint8 locPolicy, uint8 multiAcks)
{
	if (!IsConnected())
		return;

	uint8 payload[5];
	payload[0] = CMD_SET_OTHER_PARAMS;
	payload[1] = manualAdd;
	payload[2] = telemetry;
	payload[3] = locPolicy;
	payload[4] = multiAcks;
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendSetDevicePin(uint32 pin)
{
	if (!IsConnected())
		return;

	uint8 payload[5];
	payload[0] = CMD_SET_DEVICE_PIN;
	payload[1] = pin & 0xFF;
	payload[2] = (pin >> 8) & 0xFF;
	payload[3] = (pin >> 16) & 0xFF;
	payload[4] = (pin >> 24) & 0xFF;
	fSerial->SendFrame(payload, 5);
}


// =============================================================================
// Contact management
// =============================================================================


void
ProtocolHandler::SendResetPath(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	uint8 payload[1 + kPubKeySize];
	payload[0] = CMD_RESET_PATH;
	memcpy(payload + 1, pubkey, kPubKeySize);
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendRemoveContact(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	uint8 payload[1 + kPubKeySize];
	payload[0] = CMD_REMOVE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendAddUpdateContact(const uint8* pubkey, const char* name,
	uint8 type)
{
	if (!IsConnected())
		return;

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
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendShareContact(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	uint8 payload[33];
	payload[0] = CMD_SHARE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	fSerial->SendFrame(payload, 33);
}


// =============================================================================
// Messaging (frame-level only)
// =============================================================================


void
ProtocolHandler::SendDM(const uint8* pubkeyPrefix, uint8 txtType,
	uint32 timestamp, const char* text, size_t textLen)
{
	if (!IsConnected())
		return;

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

	fSerial->SendFrame(payload, pos);
}


void
ProtocolHandler::SendChannelMsg(uint8 channelIdx, uint32 timestamp,
	const char* text, size_t textLen)
{
	if (!IsConnected())
		return;

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

	fSerial->SendFrame(payload, pos);
}


// =============================================================================
// Login and admin
// =============================================================================


void
ProtocolHandler::SendLogin(const uint8* pubkey, const char* password)
{
	if (!IsConnected())
		return;

	size_t passLen = strlen(password);
	if (passLen > 15)
		passLen = 15;  // Protocol max

	uint8 payload[128];
	payload[0] = CMD_SEND_LOGIN;
	memcpy(payload + 1, pubkey, kPubKeySize);  // Full 32-byte key
	memcpy(payload + 33, password, passLen);
	payload[33 + passLen] = '\0';
	fSerial->SendFrame(payload, 34 + passLen);
}


void
ProtocolHandler::SendStatusRequest(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	uint8 payload[33];
	payload[0] = CMD_SEND_STATUS_REQ;
	memcpy(&payload[1], pubkey, 32);
	fSerial->SendFrame(payload, 33);
}


void
ProtocolHandler::SendTelemetryRequest(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	// V3: [CMD][reserved*3][pubkey*32] = 36 bytes
	uint8 payload[36];
	payload[0] = CMD_SEND_TELEMETRY_REQ;
	payload[1] = 0;
	payload[2] = 0;
	payload[3] = 0;
	memcpy(&payload[4], pubkey, 32);
	fSerial->SendFrame(payload, 36);
}


void
ProtocolHandler::SendTracePath(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	// Frame: [CMD][tag*4][authCode*4][flags][path...]
	uint8 payload[16];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_SEND_TRACE_PATH;
	memcpy(payload + 10, pubkey, kPubKeyPrefixSize);
	fSerial->SendFrame(payload, 10 + kPubKeyPrefixSize);
}


// =============================================================================
// Channels
// =============================================================================


void
ProtocolHandler::SendGetChannel(uint8 index)
{
	if (!IsConnected())
		return;

	uint8 payload[2];
	payload[0] = CMD_GET_CHANNEL;
	payload[1] = index;
	fSerial->SendFrame(payload, 2);
}


void
ProtocolHandler::SendSetChannel(uint8 index, const char* name,
	const uint8* secret)
{
	if (!IsConnected())
		return;

	uint8 payload[50];
	payload[0] = CMD_SET_CHANNEL;
	payload[1] = index;
	memset(payload + 2, 0, 32);
	strlcpy((char*)(payload + 2), name, 32);
	memcpy(payload + 34, secret, 16);
	fSerial->SendFrame(payload, sizeof(payload));
}


void
ProtocolHandler::SendRemoveChannel(uint8 index)
{
	if (!IsConnected())
		return;

	uint8 payload[50];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_SET_CHANNEL;
	payload[1] = index;
	fSerial->SendFrame(payload, sizeof(payload));
}


// =============================================================================
// Advanced
// =============================================================================


void
ProtocolHandler::SendRawData(const uint8* rawPayload, size_t rawLength)
{
	if (!IsConnected())
		return;

	if (rawLength > kMaxFramePayload - 1)
		return;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SEND_RAW_DATA;
	memcpy(payload + 1, rawPayload, rawLength);
	fSerial->SendFrame(payload, 1 + rawLength);
}


void
ProtocolHandler::SendGetCustomVars()
{
	if (!IsConnected())
		return;

	uint8 payload[1];
	payload[0] = CMD_GET_CUSTOM_VARS;
	fSerial->SendFrame(payload, 1);
}


void
ProtocolHandler::SendSetCustomVar(const char* nameValue)
{
	if (!IsConnected())
		return;

	size_t len = strlen(nameValue);
	if (len > kMaxFramePayload - 2)
		return;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SET_CUSTOM_VAR;
	memcpy(payload + 1, nameValue, len);
	payload[1 + len] = '\0';
	fSerial->SendFrame(payload, 2 + len);
}


void
ProtocolHandler::SendGetAdvertPath(const uint8* pubkey)
{
	if (!IsConnected())
		return;

	// Frame: [CMD][reserved=0][pubkey*32] = 34 bytes
	uint8 payload[34];
	payload[0] = CMD_GET_ADVERT_PATH;
	payload[1] = 0;
	memcpy(payload + 2, pubkey, kPubKeySize);
	fSerial->SendFrame(payload, 34);
}


void
ProtocolHandler::SendBinaryRequest(const uint8* pubkey, const uint8* reqData,
	size_t reqLength)
{
	if (!IsConnected())
		return;

	if (reqLength > kMaxFramePayload - 33)
		return;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SEND_BINARY_REQ;
	memcpy(payload + 1, pubkey, kPubKeySize);
	if (reqLength > 0)
		memcpy(payload + 33, reqData, reqLength);
	fSerial->SendFrame(payload, 33 + reqLength);
}


void
ProtocolHandler::SendControlData(uint8 subType, const uint8* ctrlPayload,
	size_t ctrlLength)
{
	if (!IsConnected())
		return;

	if (ctrlLength > kMaxFramePayload - 3)
		return;

	uint8 payload[kMaxFramePayload];
	payload[0] = CMD_SEND_CONTROL_DATA;
	payload[1] = 0; // flags
	payload[2] = subType;
	if (ctrlLength > 0)
		memcpy(payload + 3, ctrlPayload, ctrlLength);
	fSerial->SendFrame(payload, 3 + ctrlLength);
}
