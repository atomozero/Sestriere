/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Protocol.h — MeshCore protocol encoder/decoder
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "Types.h"

#include <SupportDefs.h>

class Protocol {
public:
	// Parsing responses
	static bool			ParseDeviceInfo(const uint8* data, size_t len,
							DeviceInfo& outInfo);
	static bool			ParseSelfInfo(const uint8* data, size_t len,
							SelfInfo& outInfo);
	static bool			ParseContact(const uint8* data, size_t len,
							Contact& outContact);
	static bool			ParseReceivedMessage(const uint8* data, size_t len,
							ReceivedMessage& outMsg);
	static bool			ParseBatteryAndStorage(const uint8* data, size_t len,
							BatteryAndStorage& outBatt);
	static bool			ParseSendConfirmed(const uint8* data, size_t len,
							SendConfirmed& outConfirm);

	// Building commands (returns payload length, NOT including frame header)
	static size_t		BuildDeviceQuery(uint8 appVersion, uint8* outBuffer);
	static size_t		BuildAppStart(uint8 appVersion, const char* appName,
							uint8* outBuffer);
	static size_t		BuildGetContacts(uint32 since, uint8* outBuffer);
	static size_t		BuildSyncNextMessage(uint8* outBuffer);
	static size_t		BuildSendTextMessage(const uint8* pubKeyPrefix,
							const char* text, uint32 timestamp,
							uint8* outBuffer);
	static size_t		BuildSendChannelMessage(const char* text,
							uint32 timestamp, uint8* outBuffer);
	static size_t		BuildGetBatteryAndStorage(uint8* outBuffer);
	static size_t		BuildSendAdvert(bool flood, uint8* outBuffer);
	static size_t		BuildSetDeviceTime(uint32 epochSecs, uint8* outBuffer);
	static size_t		BuildSetAdvertName(const char* name, uint8* outBuffer);
	static size_t		BuildSetRadioParams(const RadioParams& params,
							uint8* outBuffer);
	static size_t		BuildSetTxPower(uint8 powerDbm, uint8* outBuffer);
	static size_t		BuildReboot(uint8* outBuffer);

	// Utility functions
	static const char*	GetAdvTypeName(uint8 type);
	static const char*	GetErrorName(uint8 errCode);
	static const char*	GetResponseName(uint8 code);
	static const char*	GetPushName(uint8 code);
	static void			FormatPublicKey(const uint8* key, char* outStr,
							size_t outSize);
	static void			FormatPubKeyPrefix(const uint8* prefix, char* outStr,
							size_t outSize);
	static float		LatLonFromInt(int32 value);
	static int32		LatLonToInt(float value);

	// Byte reading utilities (public for stats parsing)
	static uint16		ReadU16LE(const uint8* data);
	static uint32		ReadU32LE(const uint8* data);
	static int32		ReadI32LE(const uint8* data);

private:
	static uint16		_ReadU16LE(const uint8* data);
	static uint32		_ReadU32LE(const uint8* data);
	static int32		_ReadI32LE(const uint8* data);
	static void			_WriteU16LE(uint8* data, uint16 value);
	static void			_WriteU32LE(uint8* data, uint32 value);
	static void			_WriteI32LE(uint8* data, int32 value);
	static size_t		_SafeStrCopy(char* dest, const uint8* src,
							size_t srcLen, size_t destSize);
};

#endif // PROTOCOL_H
