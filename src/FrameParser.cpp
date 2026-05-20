/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * FrameParser.cpp — MeshCore protocol frame decoder (BHandler)
 */

#include "FrameParser.h"

#include <Message.h>
#include <Messenger.h>
#include <String.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "Types.h"


// Inline LE readers (same as MainWindow uses)
static inline uint16
ReadLE16(const uint8* p)
{
	return (uint16)p[0] | ((uint16)p[1] << 8);
}


static inline uint32
ReadLE32(const uint8* p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8)
		| ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}


FrameParser::FrameParser(BHandler* target)
	:
	BHandler("FrameParser"),
	fTarget(target)
{
}


FrameParser::~FrameParser()
{
}


void
FrameParser::MessageReceived(BMessage* message)
{
	BHandler::MessageReceived(message);
}


void
FrameParser::_PostToTarget(BMessage* message, const uint8* rawData,
	size_t rawLength)
{
	// Always include raw frame data for handlers that still need byte access
	if (rawData != NULL && rawLength > 0 && !message->HasData("raw", B_RAW_TYPE))
		message->AddData("raw", B_RAW_TYPE, rawData, rawLength);

	if (fTarget != NULL && fTarget->Looper() != NULL)
		BMessenger(fTarget, fTarget->Looper()).SendMessage(message);
}


void
FrameParser::ParseFrame(const uint8* data, size_t length)
{
	if (length < 1)
		return;

	// Always include raw frame data for packet analyzer forwarding
	uint8 cmd = data[0];

	switch (cmd) {
		case RSP_OK:
		{
			BMessage msg(MSG_FRAME_OK);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_ERR:
			_ParseCmdErr(data, length);
			break;
		case RSP_CONTACTS_START:
		{
			BMessage msg(MSG_FRAME_CONTACTS_START);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_CONTACT:
			_ParseContact(data, length);
			break;
		case RSP_END_OF_CONTACTS:
		{
			BMessage msg(MSG_FRAME_CONTACTS_END);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_SELF_INFO:
			_ParseSelfInfo(data, length);
			break;
		case RSP_SENT:
			_ParseMsgSent(data, length);
			break;
		case RSP_CONTACT_MSG_RECV:
			_ParseContactMsg(data, length, false);
			break;
		case RSP_CONTACT_MSG_RECV_V3:
			_ParseContactMsg(data, length, true);
			break;
		case RSP_CHANNEL_MSG_RECV:
			_ParseChannelMsg(data, length, false);
			break;
		case RSP_CHANNEL_MSG_RECV_V3:
			_ParseChannelMsg(data, length, true);
			break;
		case RSP_NO_MORE_MESSAGES:
		{
			BMessage msg(MSG_FRAME_NO_MORE_MSGS);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_DEVICE_INFO:
			_ParseDeviceInfo(data, length);
			break;
		case RSP_EXPORT_CONTACT:
			_ParseExportContact(data, length);
			break;
		case RSP_BATT_AND_STORAGE:
			_ParseBattAndStorage(data, length);
			break;
		case RSP_STATS:
			_ParseStats(data, length);
			break;
		case RSP_AUTO_ADD_CONFIG:
		{
			BMessage msg(MSG_FRAME_AUTO_ADD_CONFIG);
			if (length >= 2)
				msg.AddUInt8("flags", data[1]);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_CHANNEL_INFO:
			_ParseChannelInfo(data, length);
			break;
		case PUSH_MSG_WAITING:
		{
			BMessage msg(MSG_FRAME_MSG_WAITING);
			_PostToTarget(&msg, data, length);
			break;
		}
		case PUSH_ADVERT:
		case PUSH_NEW_ADVERT:
			_ParseAdvert(data, length);
			break;
		case PUSH_SEND_CONFIRMED:
			_ParseSendConfirmed(data, length);
			break;
		case PUSH_PATH_UPDATED:
		{
			BMessage msg(MSG_FRAME_PATH_UPDATED);
			_PostToTarget(&msg, data, length);
			break;
		}
		case PUSH_TRACE_DATA:
			_ParseTraceData(data, length);
			break;
		case PUSH_TELEMETRY_RESPONSE:
			_ParseTelemetry(data, length);
			break;
		case PUSH_LOGIN_SUCCESS:
			_ParseLoginResult(data, length, true);
			break;
		case PUSH_LOGIN_FAIL:
			_ParseLoginResult(data, length, false);
			break;
		case PUSH_LOG_RX_DATA:
			_ParseRawPacket(data, length);
			break;
		case PUSH_STATUS_RESPONSE:
			_ParseStatusResponse(data, length);
			break;
		case RSP_CURR_TIME:
			_ParseCurrTime(data, length);
			break;
		case PUSH_RAW_DATA:
		case PUSH_BINARY_RESPONSE:
			_ParseRawData(data, length);
			break;
		case PUSH_CONTROL_DATA:
			_ParseControlData(data, length);
			break;
		case PUSH_PATH_DISCOVERY:
			_ParsePathDiscovery(data, length);
			break;
		case PUSH_CONTACT_DELETED:
			_ParseContactDeleted(data, length);
			break;
		case PUSH_CONTACTS_FULL:
		{
			BMessage msg(MSG_FRAME_CONTACTS_FULL);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_DISABLED:
		{
			BMessage msg(MSG_FRAME_DISABLED);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_PRIVATE_KEY:
		{
			BMessage msg(MSG_FRAME_PRIVATE_KEY);
			if (length > 1)
				msg.AddData("key", B_RAW_TYPE, data + 1, length - 1);
			_PostToTarget(&msg, data, length);
			break;
		}
		case RSP_SIGN_START:
			_ParseSignResponse(data, length, true);
			break;
		case RSP_SIGNATURE:
			_ParseSignResponse(data, length, false);
			break;
		case RSP_CUSTOM_VARS:
			_ParseCustomVars(data, length);
			break;
		case RSP_ADVERT_PATH:
			_ParseAdvertPath(data, length);
			break;
		case RSP_TUNING_PARAMS:
			_ParseTuningParams(data, length);
			break;
		case RSP_ALLOWED_REPEAT_FREQ:
			_ParseAllowedRepeatFreq(data, length);
			break;
		default:
		{
			BMessage msg(MSG_FRAME_UNKNOWN);
			msg.AddUInt8("code", cmd);
			_PostToTarget(&msg, data, length);
			break;
		}
	}
}


// =============================================================================
// Individual frame parsers - extract binary fields into BMessage
// =============================================================================


void
FrameParser::_ParseCmdErr(const uint8* data, size_t length)
{
	BMessage msg(MSG_FRAME_ERR);
	if (length >= 2)
		msg.AddUInt8("error_code", data[1]);
	if (length > 2)
		msg.AddData("raw", B_RAW_TYPE, data, length);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseSelfInfo(const uint8* data, size_t length)
{
	// RSP_SELF_INFO: [0]=code [1]=advType [2]=txPower [3]=reserved
	// [4-35]=pubkey(32) [36-67]=name(32) [68+]=radio params
	if (length < 36)
		return;

	BMessage msg(MSG_FRAME_SELF_INFO);
	msg.AddUInt8("adv_type", data[1]);
	msg.AddUInt8("tx_power", data[2]);
	msg.AddData("pubkey", B_RAW_TYPE, data + 4, 32);

	// Device name (null-terminated within 32 bytes at offset 36)
	if (length >= 68) {
		char name[33] = {};
		memcpy(name, data + 36, 32);
		msg.AddString("name", name);
	}

	// Radio params at offset 68+
	if (length >= 80) {
		msg.AddUInt32("frequency", ReadLE32(data + 68));
		msg.AddUInt32("bandwidth", ReadLE32(data + 72));
		msg.AddUInt8("sf", data[76]);
		msg.AddUInt8("cr", data[77]);
	}

	// Extended fields
	if (length >= 81)
		msg.AddUInt8("multi_acks", data[78]);
	if (length >= 82)
		msg.AddUInt8("advert_loc_policy", data[79]);
	if (length >= 83)
		msg.AddUInt8("telemetry_modes", data[80]);
	if (length >= 88) {
		msg.AddInt32("latitude", static_cast<int32>(ReadLE32(data + 81)));
		msg.AddInt32("longitude", static_cast<int32>(ReadLE32(data + 85)));
	}
	if (length >= 89)
		msg.AddBool("manual_add_contacts", data[88] != 0);

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseContact(const uint8* data, size_t length)
{
	// RSP_CONTACT: 148-byte contact frame
	// [0]=code [1-32]=pubkey [33]=type [34]=flags [35]=outPathLen
	// [36-99]=outPath(64) [100-131]=name(32) [132-135]=lastSeen(u32)
	// [136-139]=lat(i32) [140-143]=lon(i32) [144-147]=reserved
	if (length < 136)
		return;

	BMessage msg(MSG_FRAME_CONTACT);
	msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
	msg.AddUInt8("type", data[33]);
	msg.AddUInt8("flags", data[34]);
	msg.AddUInt8("out_path_len", data[35]);

	// Out path (up to 16 hops)
	uint8 pathLen = data[35];
	if (pathLen > 16) pathLen = 16;
	if (pathLen > 0 && length >= (size_t)(36 + pathLen))
		msg.AddData("out_path", B_RAW_TYPE, data + 36, pathLen);

	// Name (32 bytes at offset 100)
	char name[33] = {};
	size_t nameEnd = 131;
	while (nameEnd > 100 && data[nameEnd] == 0)
		nameEnd--;
	size_t nameLen = nameEnd - 100 + 1;
	if (nameLen > 32) nameLen = 32;
	memcpy(name, data + 100, nameLen);
	msg.AddString("name", name);

	// Last seen
	if (length >= 136)
		msg.AddUInt32("last_seen", ReadLE32(data + 132));

	// GPS
	if (length >= 144) {
		msg.AddInt32("latitude", static_cast<int32>(ReadLE32(data + 136)));
		msg.AddInt32("longitude", static_cast<int32>(ReadLE32(data + 140)));
	}

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseContactMsg(const uint8* data, size_t length, bool isV3)
{
	// V3: [0]=code [1]=snr(Q6.2) [2-3]=reserved [4-9]=pubkey6 [10]=pathLen
	//     [11]=txtType [12-15]=timestamp(LE) [16+]=text
	// V2: [0]=code [1-6]=pubkey6 [7]=pathLen [8]=txtType
	//     [9-12]=timestamp(LE) [13+]=text
	BMessage msg(MSG_FRAME_CONTACT_MSG);
	msg.AddBool("is_v3", isV3);

	if (isV3) {
		if (length < kV3DmMinLength) return;
		msg.AddInt8("snr", static_cast<int8>(data[kV3DmSnrOffset]));
		msg.AddData("pubkey", B_RAW_TYPE,
			data + kV3DmSenderOffset, kPubKeyPrefixSize);
		msg.AddUInt8("path_len", data[kV3DmPathLenOffset]);
		msg.AddUInt8("txt_type", data[kV3DmTxtTypeOffset]);
		msg.AddUInt32("timestamp", ReadLE32(data + kV3DmTimestampOffset));

		if (length > kV3DmTextOffset)
			msg.AddData("text", B_RAW_TYPE,
				data + kV3DmTextOffset, length - kV3DmTextOffset);
	} else {
		if (length < kV2DmMinLength) return;
		msg.AddInt8("snr", 0);
		msg.AddData("pubkey", B_RAW_TYPE,
			data + kV2DmSenderOffset, kPubKeyPrefixSize);
		msg.AddUInt8("path_len", data[kV2DmPathLenOffset]);
		msg.AddUInt8("txt_type", data[kV2DmTxtTypeOffset]);
		msg.AddUInt32("timestamp", ReadLE32(data + kV2DmTimestampOffset));

		if (length > kV2DmTextOffset)
			msg.AddData("text", B_RAW_TYPE,
				data + kV2DmTextOffset, length - kV2DmTextOffset);
	}

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseChannelMsg(const uint8* data, size_t length, bool isV3)
{
	// V3: [0]=code [1]=snr(Q6.2) [2-3]=reserved [4]=channelIdx
	//     [5]=pathLen [6]=txtType [7-10]=timestamp(LE) [11+]=text
	// V2: [0]=code [1]=channelIdx [2]=pathLen [3]=txtType
	//     [4-7]=timestamp(LE) [8+]=text
	BMessage msg(MSG_FRAME_CHANNEL_MSG);
	msg.AddBool("is_v3", isV3);

	if (isV3) {
		if (length < kV3ChMinLength) return;
		msg.AddInt8("snr", static_cast<int8>(data[kV3ChSnrOffset]));
		msg.AddUInt8("channel_idx", data[kV3ChChannelOffset]);
		msg.AddUInt8("path_len", data[kV3ChPathLenOffset]);
		msg.AddUInt8("txt_type", data[kV3ChTxtTypeOffset]);
		msg.AddUInt32("timestamp", ReadLE32(data + kV3ChTimestampOffset));

		if (length > kV3ChTextOffset)
			msg.AddData("text", B_RAW_TYPE,
				data + kV3ChTextOffset, length - kV3ChTextOffset);
	} else {
		if (length < kV2ChMinLength) return;
		msg.AddInt8("snr", 0);
		msg.AddUInt8("channel_idx", data[kV2ChChannelOffset]);
		msg.AddUInt8("path_len", data[kV2ChPathLenOffset]);
		msg.AddUInt8("txt_type", data[kV2ChTxtTypeOffset]);
		msg.AddUInt32("timestamp", ReadLE32(data + kV2ChTimestampOffset));

		if (length > kV2ChTextOffset)
			msg.AddData("text", B_RAW_TYPE,
				data + kV2ChTextOffset, length - kV2ChTextOffset);
	}

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseDeviceInfo(const uint8* data, size_t length)
{
	// RSP_DEVICE_INFO: [0]=code [1]=boardType [2]=maxContacts/2
	// [3]=maxChannels [4-7]=firmware(4 chars) [8+]=features
	if (length < 4)
		return;

	BMessage msg(MSG_FRAME_DEVICE_INFO);
	msg.AddUInt8("board_type", data[1]);
	msg.AddUInt8("max_contacts_raw", data[2]);
	msg.AddUInt8("max_channels", data[3]);

	if (length >= 8) {
		char fw[5] = {};
		memcpy(fw, data + 4, 4);
		msg.AddString("firmware", fw);
	}

	// Extended fields (PIN, client_repeat, path_hash_mode, etc.)
	if (length >= 12)
		msg.AddUInt32("device_pin", ReadLE32(data + 8));
	if (length >= 13)
		msg.AddUInt8("client_repeat", data[12]);
	if (length >= 14)
		msg.AddUInt8("path_hash_mode", data[13]);

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseExportContact(const uint8* data, size_t length)
{
	// RSP_EXPORT_CONTACT: same format as RSP_SELF_INFO
	// Reuse self-info parser, but with different message code
	if (length < 36)
		return;

	BMessage msg(MSG_FRAME_EXPORT_CONTACT);
	msg.AddData("pubkey", B_RAW_TYPE, data + 4, 32);
	if (length >= 68) {
		char name[33] = {};
		memcpy(name, data + 36, 32);
		msg.AddString("name", name);
	}
	msg.AddData("raw", B_RAW_TYPE, data, length);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseBattAndStorage(const uint8* data, size_t length)
{
	// RSP_BATT_AND_STORAGE: [0]=code [1-2]=battMv(u16 LE)
	// [3-6]=usedKb(u32 LE) [7-10]=totalKb(u32 LE)
	if (length < 3)
		return;

	BMessage msg(MSG_FRAME_BATT_STORAGE);
	msg.AddUInt16("batt_mv", ReadLE16(data + 1));
	if (length >= 7)
		msg.AddUInt32("used_kb", ReadLE32(data + 3));
	if (length >= 11)
		msg.AddUInt32("total_kb", ReadLE32(data + 7));
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseStats(const uint8* data, size_t length)
{
	// RSP_STATS: [0]=code [1]=subType
	// Core: [2-3]=battMv [4-7]=uptime
	// Radio: [2-3]=noiseFloor [4]=rssi [5]=snr
	// Packets: [2-5]=recvPkts [6-9]=sentPkts
	if (length < 2)
		return;

	BMessage msg(MSG_FRAME_STATS);
	msg.AddUInt8("sub_type", data[1]);
	msg.AddData("raw", B_RAW_TYPE, data, length);

	if (data[1] == 0 && length >= 8) {
		// Core stats
		msg.AddUInt16("batt_mv", ReadLE16(data + 2));
		msg.AddUInt32("uptime", ReadLE32(data + 4));
	} else if (data[1] == 1 && length >= 6) {
		// Radio stats
		msg.AddInt16("noise_floor",
			static_cast<int16>(ReadLE16(data + 2)));
		msg.AddInt8("rssi", static_cast<int8>(data[4]));
		msg.AddInt8("snr", static_cast<int8>(data[5]));
	} else if (data[1] == 2 && length >= 10) {
		// Packet stats
		msg.AddUInt32("recv_pkts", ReadLE32(data + 2));
		msg.AddUInt32("sent_pkts", ReadLE32(data + 6));
	}

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseChannelInfo(const uint8* data, size_t length)
{
	// RSP_CHANNEL_INFO: [0]=code [1]=index [2-17]=psk(16) [18+]=name
	if (length < 18)
		return;

	BMessage msg(MSG_FRAME_CHANNEL_INFO);
	msg.AddUInt8("index", data[1]);
	msg.AddData("psk", B_RAW_TYPE, data + 2, 16);

	if (length > 18) {
		char name[65] = {};
		size_t nameLen = length - 18;
		if (nameLen > 64) nameLen = 64;
		memcpy(name, data + 18, nameLen);
		msg.AddString("name", name);
	}

	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseAdvert(const uint8* data, size_t length)
{
	// PUSH_ADVERT/PUSH_NEW_ADVERT: [0]=code [1-32]=pubkey [33]=type
	// [34]=flags [35-66]=name(32) [67-70]=lat [71-74]=lon
	if (length < 34)
		return;

	BMessage msg(MSG_FRAME_ADVERT);
	msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
	msg.AddUInt8("type", data[33]);
	if (length >= 35)
		msg.AddUInt8("flags", data[34]);

	if (length >= 67) {
		char name[33] = {};
		memcpy(name, data + 35, 32);
		msg.AddString("name", name);
	}

	if (length >= 75) {
		msg.AddInt32("latitude", static_cast<int32>(ReadLE32(data + 67)));
		msg.AddInt32("longitude", static_cast<int32>(ReadLE32(data + 71)));
	}

	msg.AddBool("is_new", data[0] == PUSH_NEW_ADVERT);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseSendConfirmed(const uint8* data, size_t length)
{
	// PUSH_SEND_CONFIRMED: [0]=code [1-4]=ackCode(u32) [5-8]=roundTripMs(u32)
	BMessage msg(MSG_FRAME_SEND_CONFIRMED);
	if (length >= 5)
		msg.AddUInt32("ack_code", ReadLE32(data + 1));
	if (length >= 9)
		msg.AddUInt32("round_trip_ms", ReadLE32(data + 5));
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseMsgSent(const uint8* data, size_t length)
{
	// RSP_SENT: [0]=code [1-4]=ackCode(u32 LE)
	BMessage msg(MSG_FRAME_MSG_SENT);
	if (length >= 5)
		msg.AddUInt32("ack_code", ReadLE32(data + 1));
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseTraceData(const uint8* data, size_t length)
{
	// PUSH_TRACE_DATA: [0]=code [1]=status [2]=pathLen
	// [3]=reserved [4-7]=tag(u32) [8-11]=authCode(u32)
	// [12+]=hashes+snrs
	BMessage msg(MSG_FRAME_TRACE_DATA);
	msg.AddData("raw", B_RAW_TYPE, data, length);
	if (length >= 3) {
		msg.AddUInt8("status", data[1]);
		msg.AddUInt8("path_len", data[2]);
	}
	if (length >= 12) {
		msg.AddUInt32("tag", ReadLE32(data + 4));
		msg.AddUInt32("auth_code", ReadLE32(data + 8));
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseTelemetry(const uint8* data, size_t length)
{
	// PUSH_TELEMETRY_RESPONSE: [0]=code [1-32]=pubkey [33+]=telemetry data
	BMessage msg(MSG_FRAME_TELEMETRY);
	if (length > 33)
		msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
	msg.AddData("raw", B_RAW_TYPE, data, length);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseRawData(const uint8* data, size_t length)
{
	// PUSH_RAW_DATA: [0]=code [1-32]=senderPubkey [33+]=payload
	BMessage msg(MSG_FRAME_RAW_DATA);
	if (length > 33) {
		msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
		msg.AddData("payload", B_RAW_TYPE, data + 33, length - 33);
	} else {
		msg.AddData("raw", B_RAW_TYPE, data, length);
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseControlData(const uint8* data, size_t length)
{
	// PUSH_CONTROL_DATA: [0]=code [1-32]=senderPubkey [33+]=payload
	BMessage msg(MSG_FRAME_CONTROL_DATA);
	if (length > 33) {
		msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
		msg.AddData("payload", B_RAW_TYPE, data + 33, length - 33);
	} else {
		msg.AddData("raw", B_RAW_TYPE, data, length);
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseLoginResult(const uint8* data, size_t length, bool success)
{
	BMessage msg(MSG_FRAME_LOGIN_RESULT);
	msg.AddBool("success", success);
	if (length > 1)
		msg.AddData("raw", B_RAW_TYPE, data + 1, length - 1);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseStatusResponse(const uint8* data, size_t length)
{
	// PUSH_STATUS_RESPONSE: [0]=code [1-32]=pubkey [33+]=status text
	BMessage msg(MSG_FRAME_STATUS_RESPONSE);
	if (length > 33) {
		msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
		size_t textLen = strnlen(
			reinterpret_cast<const char*>(data + 33), length - 33);
		msg.AddString("status",
			BString(reinterpret_cast<const char*>(data + 33), textLen));
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseRawPacket(const uint8* data, size_t length)
{
	// PUSH_LOG_RX_DATA: raw radio packet for monitor
	BMessage msg(MSG_FRAME_RAW_PACKET);
	msg.AddData("raw", B_RAW_TYPE, data, length);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParsePathDiscovery(const uint8* data, size_t length)
{
	// PUSH_PATH_DISCOVERY: [0]=code [1-32]=pubkey [33]=pathLen [34+]=path
	BMessage msg(MSG_FRAME_PATH_DISCOVERY);
	if (length > 33) {
		msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
		if (length > 34)
			msg.AddUInt8("path_len", data[33]);
		if (length > 34)
			msg.AddData("path", B_RAW_TYPE, data + 34, length - 34);
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseContactDeleted(const uint8* data, size_t length)
{
	// PUSH_CONTACT_DELETED: [0]=code [1-32]=pubkey
	BMessage msg(MSG_FRAME_CONTACT_DELETED);
	if (length >= 33)
		msg.AddData("pubkey", B_RAW_TYPE, data + 1, 32);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseSignResponse(const uint8* data, size_t length, bool isStart)
{
	BMessage msg(MSG_FRAME_SIGN_RESPONSE);
	msg.AddBool("is_start", isStart);
	if (length > 1)
		msg.AddData("raw", B_RAW_TYPE, data + 1, length - 1);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseCustomVars(const uint8* data, size_t length)
{
	// RSP_CUSTOM_VARS: [0]=code [1+]=name:value text
	BMessage msg(MSG_FRAME_CUSTOM_VARS);
	if (length > 1) {
		size_t textLen = strnlen(
			reinterpret_cast<const char*>(data + 1), length - 1);
		msg.AddString("vars",
			BString(reinterpret_cast<const char*>(data + 1), textLen));
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseAdvertPath(const uint8* data, size_t length)
{
	// RSP_ADVERT_PATH: [0]=code [1-4]=recv_timestamp [5]=path_len [6+]=path
	BMessage msg(MSG_FRAME_ADVERT_PATH);
	if (length >= 6) {
		msg.AddUInt32("recv_timestamp", ReadLE32(data + 1));
		msg.AddUInt8("path_len", data[5]);
		if (length > 6)
			msg.AddData("path", B_RAW_TYPE, data + 6, length - 6);
	}
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseTuningParams(const uint8* data, size_t length)
{
	// RSP_TUNING_PARAMS: [0]=code [1-4]=rxDelayBase(u32)
	// [5-8]=airtimeFactor(u32) [9-16]=reserved
	BMessage msg(MSG_FRAME_TUNING_PARAMS);
	if (length >= 5)
		msg.AddUInt32("rx_delay_base", ReadLE32(data + 1));
	if (length >= 9)
		msg.AddUInt32("airtime_factor", ReadLE32(data + 5));
	msg.AddData("raw", B_RAW_TYPE, data, length);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseAllowedRepeatFreq(const uint8* data, size_t length)
{
	BMessage msg(MSG_FRAME_ALLOWED_REPEAT);
	msg.AddData("raw", B_RAW_TYPE, data, length);
	_PostToTarget(&msg, data, length);
}


void
FrameParser::_ParseCurrTime(const uint8* data, size_t length)
{
	// RSP_CURR_TIME: [0]=code [1-4]=epoch_secs(u32 LE)
	BMessage msg(MSG_FRAME_CURR_TIME);
	if (length >= 5)
		msg.AddUInt32("epoch", ReadLE32(data + 1));
	_PostToTarget(&msg, data, length);
}
