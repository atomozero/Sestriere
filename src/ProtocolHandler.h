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
			void			SendAppStart();
			void			SendDeviceQuery();
			void			SendExportSelf();
			void			SendGetContacts();
			void			SendSelfAdvert();
			void			SendSyncNextMessage();

			// Device info
			void			SendGetBattery();
			void			SendGetStats();
			void			SendGetDeviceTime();
			void			SendSetDeviceTime(uint32 epoch);
			void			SendReboot();
			void			SendFactoryReset();

			// Radio configuration
			void			SendRadioParams(uint32 freqHz, uint32 bwHz,
								uint8 sf, uint8 cr);
			void			SendSetTxPower(uint8 power);
			void			SendGetTuningParams();
			void			SendSetTuningParams(uint32 rxDelayBase,
								uint32 airtimeFactor);

			// Node identity
			void			SendSetName(const char* name);
			void			SendSetLatLon(double lat, double lon);
			void			SendOtherParams(uint8 manualAdd, uint8 telemetry,
								uint8 locPolicy, uint8 multiAcks);
			void			SendSetDevicePin(uint32 pin);

			// Contact management
			void			SendResetPath(const uint8* pubkey);
			void			SendRemoveContact(const uint8* pubkey);
			void			SendAddUpdateContact(const uint8* pubkey,
								const char* name, uint8 type);
			void			SendShareContact(const uint8* pubkey);

			// Messaging (frame-level only)
			void			SendDM(const uint8* pubkeyPrefix,
								uint8 txtType, uint32 timestamp,
								const char* text, size_t textLen);
			void			SendChannelMsg(uint8 channelIdx,
								uint32 timestamp, const char* text,
								size_t textLen);

			// Login and admin
			void			SendLogin(const uint8* pubkey,
								const char* password);
			void			SendStatusRequest(const uint8* pubkey);
			void			SendTelemetryRequest(const uint8* pubkey);
			void			SendTracePath(const uint8* pubkey);

			// Channels
			void			SendGetChannel(uint8 index);
			void			SendSetChannel(uint8 index, const char* name,
								const uint8* secret);
			void			SendRemoveChannel(uint8 index);

			// Advanced
			void			SendRawData(const uint8* payload,
								size_t length);
			void			SendGetCustomVars();
			void			SendSetCustomVar(const char* nameValue);
			void			SendGetAdvertPath(const uint8* pubkey);
			void			SendBinaryRequest(const uint8* pubkey,
								const uint8* data, size_t length);
			void			SendControlData(uint8 subType,
								const uint8* payload, size_t length);

private:
			SerialHandler*	fSerial;
};


#endif // PROTOCOLHANDLER_H
