/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttClient.h — MQTT client for meshcoreitalia.it integration
 */

#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <Looper.h>
#include <MessageRunner.h>
#include <String.h>

struct mosquitto;

// MQTT Settings structure
struct MqttSettings {
	bool		enabled;
	char		broker[128];
	int			port;
	char		username[64];
	char		password[64];
	char		iataCode[8];		// Location code (e.g., VCE, FCO)
	double		latitude;
	double		longitude;
	char		publicKey[65];		// 64 hex chars + null
	char		deviceName[64];		// Device name for origin field

	MqttSettings() : enabled(false), port(1883), latitude(0), longitude(0) {
		strlcpy(broker, "nodi.meshcoreitalia.it", sizeof(broker));
		strlcpy(username, "meshcore", sizeof(username));
		strlcpy(password, "meshcore25", sizeof(password));
		strlcpy(iataCode, "XXX", sizeof(iataCode));
		memset(publicKey, 0, sizeof(publicKey));
		memset(deviceName, 0, sizeof(deviceName));
	}
};

// Message codes for MqttClient
enum {
	MSG_MQTT_CONNECT = 'mqcn',
	MSG_MQTT_DISCONNECT = 'mqdc',
	MSG_MQTT_PUBLISH_STATUS = 'mqps',
	MSG_MQTT_PUBLISH_PACKET = 'mqpp',
	MSG_MQTT_CONNECTED = 'mqok',
	MSG_MQTT_DISCONNECTED = 'mqds',
	MSG_MQTT_ERROR = 'mqer',
	MSG_MQTT_STATUS_TIMER = 'mqtm',
	MSG_MQTT_RECONNECT_TIMER = 'mqrt'
};


class MqttClient : public BLooper {
public:
							MqttClient();
	virtual					~MqttClient();

	virtual void			MessageReceived(BMessage* message);

			void			SetSettings(const MqttSettings& settings);
			void			SetTarget(BHandler* target);
			void			SetLogTarget(BHandler* target);

			void			Connect();
			void			Disconnect();
			bool			IsConnected() const { return fConnected; }

			// Publish device status
			void			PublishStatus(const char* deviceName,
								const char* firmware, const char* board,
								uint16 batteryMv, uint32 uptimeS,
								int8 noiseFloor);

			// Publish received packet
			void			PublishPacket(uint32 timestamp,
								int8 snr, int8 rssi,
								const char* packetType,
								const uint8* fromKey, size_t keyLen,
								const uint8* payload, size_t payloadLen);

private:
			void			_DoConnect();
			void			_DoDisconnect();
			void			_StartReconnectTimer();
			void			_StopReconnectTimer();
			void			_Publish(const char* topic, const char* json);
			BString			_BuildStatusTopic();
			BString			_BuildPacketsTopic();

	static void				_OnConnectCallback(struct mosquitto* mosq,
								void* obj, int rc);
	static void				_OnDisconnectCallback(struct mosquitto* mosq,
								void* obj, int rc);
	static void				_OnPublishCallback(struct mosquitto* mosq,
								void* obj, int mid);

			void			_SendLogEntry(const char* text);

			struct mosquitto*	fMosquitto;
			MqttSettings	fSettings;
			BHandler*		fTarget;
			BHandler*		fLogTarget;
			bool			fConnected;
			bool			fManualDisconnect;
			BMessageRunner*	fStatusTimer;
			BMessageRunner*	fReconnectTimer;
			bigtime_t		fReconnectDelay;
			bool			fInitialized;
};

#endif // MQTTCLIENT_H
