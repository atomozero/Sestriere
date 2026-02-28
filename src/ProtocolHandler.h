/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ProtocolHandler.h — MeshCore Companion Protocol V3 command builder
 */

#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include <SupportDefs.h>

#include "Constants.h"
#include "Types.h"

class SerialHandler;


class ProtocolHandler {
public:
						ProtocolHandler(SerialHandler* serial);

		void			SetSerialHandler(SerialHandler* serial);
		bool			IsConnected() const;

		// App lifecycle
		status_t		SendAppStart();
		status_t		SendDeviceQuery();
		status_t		SendExportSelf();
		status_t		SendGetContacts();
		status_t		SendSelfAdvert();
		status_t		SendSyncNextMessage();

		// Device info
		status_t		SendGetBattery();
		status_t		SendGetStats();
		status_t		SendGetDeviceTime();
		status_t		SendSetDeviceTime(uint32 epoch);
		status_t		SendReboot();
		status_t		SendFactoryReset();

		// Radio configuration
		status_t		SendRadioParams(uint32 freqHz, uint32 bwHz,
							uint8 sf, uint8 cr);
		status_t		SendSetTxPower(uint8 power);
		status_t		SendGetTuningParams();
		status_t		SendSetTuningParams(uint32 rxDelayBase,
							uint32 airtimeFactor);

		// Node identity
		status_t		SendSetName(const char* name);
		status_t		SendSetLatLon(double lat, double lon);
		status_t		SendOtherParams(uint8 manualAdd, uint8 telemetry,
							uint8 locPolicy, uint8 multiAcks);
		status_t		SendSetDevicePin(uint32 pin);

		// Contact management
		status_t		SendResetPath(const uint8* pubkey);
		status_t		SendRemoveContact(const uint8* pubkey);
		status_t		SendAddUpdateContact(const uint8* pubkey,
							const char* name, uint8 type);
		status_t		SendShareContact(const uint8* pubkey);

		// Messaging (frame-level only)
		status_t		SendDM(const uint8* pubkeyPrefix,
							uint8 txtType, uint32 timestamp,
							const char* text, size_t textLen);
		status_t		SendChannelMsg(uint8 channelIdx,
							uint32 timestamp, const char* text,
							size_t textLen);

		// Login and admin
		status_t		SendLogin(const uint8* pubkey,
							const char* password);
		status_t		SendStatusRequest(const uint8* pubkey);
		status_t		SendTelemetryRequest(const uint8* pubkey);
		status_t		SendTracePath(const ContactInfo* contact);

		// Channels
		status_t		SendGetChannel(uint8 index);
		status_t		SendSetChannel(uint8 index, const char* name,
							const uint8* secret);
		status_t		SendRemoveChannel(uint8 index);

		// Advanced
		status_t		SendRawData(const uint8* payload,
							size_t length);
		status_t		SendGetCustomVars();
		status_t		SendSetCustomVar(const char* nameValue);
		status_t		SendGetAdvertPath(const uint8* pubkey);
		status_t		SendBinaryRequest(const uint8* pubkey,
							const uint8* data, size_t length);
		status_t		SendControlData(uint8 subType,
							const uint8* payload, size_t length);

private:
		SerialHandler*	fSerial;
};


#endif // PROTOCOLHANDLER_H
