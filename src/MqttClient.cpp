/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttClient.cpp — MQTT client implementation
 */

#include "MqttClient.h"

#include <Autolock.h>
#include <Messenger.h>
#include <MessageRunner.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <mosquitto.h>

#include "Constants.h"


static const bigtime_t kStatusInterval = 60000000;      // 60 seconds
static const bigtime_t kReconnectInitDelay = 5000000;    // 5 seconds
static const bigtime_t kReconnectMaxDelay = 60000000;    // 60 seconds


MqttClient::MqttClient()
	:
	BLooper("MqttClient"),
	fMosquitto(NULL),
	fTarget(NULL),
	fLogTarget(NULL),
	fStateLock("MqttState"),
	fConnected(false),
	fManualDisconnect(false),
	fStatusTimer(NULL),
	fReconnectTimer(NULL),
	fReconnectDelay(kReconnectInitDelay),
	fInitialized(false)
{
	fprintf(stderr, "[MQTT] Constructor (lazy init)\n");
	// Don't initialize mosquitto here - do it lazily when connecting
	Run();
}


MqttClient::~MqttClient()
{
	delete fStatusTimer;
	delete fReconnectTimer;

	if (fMosquitto != NULL) {
		mosquitto_loop_stop(fMosquitto, true);
		mosquitto_disconnect(fMosquitto);
		mosquitto_destroy(fMosquitto);
	}
	if (fInitialized) {
		mosquitto_lib_cleanup();
	}
}


void
MqttClient::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_MQTT_CONNECT:
			_DoConnect();
			break;

		case MSG_MQTT_DISCONNECT:
			_DoDisconnect();
			break;

		case MSG_MQTT_STATUS_TIMER:
			// Request status publish from main window
			if (fTarget != NULL && fConnected) {
				BMessage request(MSG_MQTT_PUBLISH_STATUS);
				BMessenger(fTarget).SendMessage(&request);
			}
			break;

		case MSG_MQTT_RECONNECT_TIMER:
			if (!fConnected && fSettings.enabled && !fManualDisconnect) {
				fprintf(stderr, "[MQTT] Reconnect timer fired, attempting...\n");
				_SendLogEntry(MQTT_LOG_RECONN, "Auto-reconnecting...");
				_DoConnect();
			}
			break;

		case MSG_MQTT_CONN_STATE_CHANGED:
			_HandleConnStateChanged(message);
			break;

		default:
			BLooper::MessageReceived(message);
			break;
	}
}


void
MqttClient::SetSettings(const MqttSettings& settings)
{
	fSettings = settings;
}


void
MqttClient::SetTarget(BHandler* target)
{
	fTarget = target;
}


void
MqttClient::SetLogTarget(BHandler* target)
{
	fLogTarget = target;
}


bool
MqttClient::IsConnected() const
{
	BAutolock lock(fStateLock);
	return fConnected;
}


void
MqttClient::Connect()
{
	fManualDisconnect = false;
	_StopReconnectTimer();
	fReconnectDelay = kReconnectInitDelay;
	PostMessage(MSG_MQTT_CONNECT);
}


void
MqttClient::Disconnect()
{
	fManualDisconnect = true;
	_StopReconnectTimer();
	PostMessage(MSG_MQTT_DISCONNECT);
}


void
MqttClient::_DoConnect()
{
	fprintf(stderr, "[MQTT] _DoConnect called (enabled=%d, initialized=%d)\n",
		fSettings.enabled, fInitialized);

	if (!fSettings.enabled) {
		fprintf(stderr, "[MQTT] Not connecting: not enabled\n");
		return;
	}

	// Lazy initialization of mosquitto
	if (!fInitialized) {
		fprintf(stderr, "[MQTT] Initializing mosquitto library...\n");
		mosquitto_lib_init();

		fMosquitto = mosquitto_new("sestriere", true, this);
		if (fMosquitto == NULL) {
			fprintf(stderr, "[MQTT] Failed to create mosquitto client\n");
			return;
		}

		mosquitto_connect_callback_set(fMosquitto, _OnConnectCallback);
		mosquitto_disconnect_callback_set(fMosquitto, _OnDisconnectCallback);
		mosquitto_publish_callback_set(fMosquitto, _OnPublishCallback);
		fInitialized = true;
		fprintf(stderr, "[MQTT] Mosquitto initialized\n");
	}

	if (fConnected) {
		fprintf(stderr, "[MQTT] Already connected, disconnecting first\n");
		_DoDisconnect();
	}

	// Set credentials
	fprintf(stderr, "[MQTT] Connecting to %s:%d\n",
		fSettings.broker, fSettings.port);
	mosquitto_username_pw_set(fMosquitto, fSettings.username, fSettings.password);

	{
		BString logEntry;
		logEntry.SetToFormat("Connecting to %s:%d...", fSettings.broker, fSettings.port);
		_SendLogEntry(MQTT_LOG_CONN, logEntry.String());
	}

	// Connect (non-blocking)
	int rc = mosquitto_connect_async(fMosquitto, fSettings.broker, fSettings.port, 60);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "[MQTT] Connect failed: %s\n", mosquitto_strerror(rc));
		BString logEntry;
		logEntry.SetToFormat("Connect failed: %s", mosquitto_strerror(rc));
		_SendLogEntry(MQTT_LOG_ERR, logEntry.String());
		if (fTarget != NULL) {
			BMessage error(MSG_MQTT_ERROR);
			error.AddString("error", mosquitto_strerror(rc));
			BMessenger(fTarget).SendMessage(&error);
		}
		return;
	}

	fprintf(stderr, "[MQTT] Connection initiated, starting loop\n");

	// Start network loop
	mosquitto_loop_start(fMosquitto);

	// Start status timer
	delete fStatusTimer;
	BMessage timerMsg(MSG_MQTT_STATUS_TIMER);
	fStatusTimer = new BMessageRunner(this, &timerMsg, kStatusInterval);
}


void
MqttClient::_DoDisconnect()
{
	if (fMosquitto == NULL)
		return;

	delete fStatusTimer;
	fStatusTimer = NULL;

	mosquitto_loop_stop(fMosquitto, true);
	mosquitto_disconnect(fMosquitto);
	{
		BAutolock lock(fStateLock);
		fConnected = false;
	}
}


void
MqttClient::PublishStatus(const char* deviceName, const char* firmware,
	const char* board, uint16 batteryMv, uint32 uptimeS, int8 noiseFloor)
{
	if (!fConnected || fMosquitto == NULL)
		return;

	// Get current timestamp in ISO8601 format
	time_t now = time(NULL);
	struct tm* tm = gmtime(&now);
	char timestamp[32];
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm);

	// Format matching meshcore-to-maps expected format
	char json[768];
	snprintf(json, sizeof(json),
		"{"
		"\"status\":\"online\","
		"\"timestamp\":\"%s\","
		"\"origin\":\"%s\","
		"\"origin_id\":\"%s\","
		"\"model\":\"%s\","
		"\"firmware_version\":\"%s\","
		"\"client_version\":\"sestriere/haiku-1.0\","
		"\"lat\":%.6f,"
		"\"lon\":%.6f,"
		"\"stats\":{"
			"\"battery_mv\":%u,"
			"\"uptime_secs\":%u,"
			"\"noise_floor\":%d"
		"}"
		"}",
		timestamp,
		deviceName ? deviceName : "Unknown",
		fSettings.publicKey,
		board ? board : "unknown",
		firmware ? firmware : "unknown",
		fSettings.latitude,
		fSettings.longitude,
		batteryMv,
		uptimeS,
		noiseFloor);

	_Publish(_BuildStatusTopic().String(), json);
}


void
MqttClient::PublishPacket(uint32 timestamp, int8 snr, int8 rssi,
	const char* packetType, const uint8* fromKey, size_t keyLen,
	const uint8* payload, size_t payloadLen)
{
	if (!fConnected || fMosquitto == NULL)
		return;

	// Get current timestamp in ISO8601 format
	time_t now = time(NULL);
	struct tm* tm = gmtime(&now);
	char isoTimestamp[32];
	strftime(isoTimestamp, sizeof(isoTimestamp), "%Y-%m-%dT%H:%M:%SZ", tm);

	char timeStr[16], dateStr[16];
	strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm);
	strftime(dateStr, sizeof(dateStr), "%m/%d/%Y", tm);

	// Build from_key hex string
	char fromKeyHex[32];
	size_t hexLen = (keyLen > 6) ? 6 : keyLen;
	for (size_t i = 0; i < hexLen; i++) {
		snprintf(fromKeyHex + i * 2, 3, "%02X", fromKey[i]);
	}
	fromKeyHex[hexLen * 2] = '\0';

	// Build payload hex (limited to first 64 bytes)
	char payloadHex[129];
	size_t payloadHexLen = (payloadLen > 64) ? 64 : payloadLen;
	for (size_t i = 0; i < payloadHexLen; i++) {
		snprintf(payloadHex + i * 2, 3, "%02X", payload[i]);
	}
	payloadHex[payloadHexLen * 2] = '\0';

	// Format matching meshcore-to-maps expected format
	char json[768];
	snprintf(json, sizeof(json),
		"{"
		"\"origin\":\"%s\","
		"\"origin_id\":\"%s\","
		"\"timestamp\":\"%s\","
		"\"type\":\"PACKET\","
		"\"direction\":\"rx\","
		"\"time\":\"%s\","
		"\"date\":\"%s\","
		"\"packet_type\":\"%s\","
		"\"from_key\":\"%s\","
		"\"raw\":\"%s\","
		"\"len\":%zu,"
		"\"SNR\":%.1f,"
		"\"RSSI\":%d,"
		"\"lat\":%.6f,"
		"\"lon\":%.6f"
		"}",
		fSettings.deviceName[0] ? fSettings.deviceName : "Observer",  // origin
		fSettings.publicKey,  // origin_id
		isoTimestamp,
		timeStr,
		dateStr,
		packetType ? packetType : "unknown",
		fromKeyHex,
		payloadHex,
		payloadLen,
		snr / 4.0f,  // SNR is stored as SNR*4
		rssi,
		fSettings.latitude,
		fSettings.longitude);

	_Publish(_BuildPacketsTopic().String(), json);
}


void
MqttClient::_Publish(const char* topic, const char* json)
{
	if (fMosquitto == NULL || topic == NULL || json == NULL) {
		fprintf(stderr, "[MQTT] _Publish: invalid params (mosq=%p)\n", fMosquitto);
		return;
	}

	fprintf(stderr, "[MQTT] Publishing to: %s\n", topic);
	fprintf(stderr, "[MQTT] Payload: %.100s%s\n", json, strlen(json) > 100 ? "..." : "");

	int rc = mosquitto_publish(fMosquitto, NULL, topic, strlen(json), json, 0, false);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "[MQTT] Publish error: %s\n", mosquitto_strerror(rc));
		BString logEntry;
		logEntry.SetToFormat("Publish error: %s (topic: %s)", mosquitto_strerror(rc), topic);
		_SendLogEntry(MQTT_LOG_ERR, logEntry.String());
	} else {
		fprintf(stderr, "[MQTT] Publish queued OK\n");
		BString logEntry;
		logEntry.SetToFormat("→ %s (%zu B)", topic, strlen(json));
		_SendLogEntry(MQTT_LOG_PUB, logEntry.String());
	}
}


void
MqttClient::_SendLogEntry(int32 type, const char* text)
{
	if (fLogTarget != NULL && text != NULL) {
		BMessage logMsg(MSG_MQTT_LOG_ENTRY);
		logMsg.AddInt32("type", type);
		logMsg.AddString("text", text);
		BMessenger(fLogTarget).SendMessage(&logMsg);
	}
}


void
MqttClient::_StartReconnectTimer()
{
	_StopReconnectTimer();

	int32 delaySecs = (int32)(fReconnectDelay / 1000000);
	fprintf(stderr, "[MQTT] Scheduling reconnect in %d seconds\n", delaySecs);

	BString logEntry;
	logEntry.SetToFormat("Reconnecting in %d seconds...", delaySecs);
	_SendLogEntry(MQTT_LOG_RECONN, logEntry.String());

	BMessage timerMsg(MSG_MQTT_RECONNECT_TIMER);
	fReconnectTimer = new BMessageRunner(this, &timerMsg,
		fReconnectDelay, 1);

	// Exponential backoff: double the delay, cap at max
	fReconnectDelay *= 2;
	if (fReconnectDelay > kReconnectMaxDelay)
		fReconnectDelay = kReconnectMaxDelay;
}


void
MqttClient::_StopReconnectTimer()
{
	delete fReconnectTimer;
	fReconnectTimer = NULL;
}


BString
MqttClient::_BuildStatusTopic()
{
	BString topic;
	topic.SetToFormat("meshcore/%s/%s/status",
		fSettings.iataCode, fSettings.publicKey);
	return topic;
}


BString
MqttClient::_BuildPacketsTopic()
{
	BString topic;
	topic.SetToFormat("meshcore/%s/%s/packets",
		fSettings.iataCode, fSettings.publicKey);
	return topic;
}


void
MqttClient::_OnConnectCallback(struct mosquitto* mosq, void* obj, int rc)
{
	// This runs in mosquitto's network thread — post to looper for safe state change
	MqttClient* client = (MqttClient*)obj;
	fprintf(stderr, "[MQTT] Connect callback: rc=%d (%s)\n", rc,
		rc == 0 ? "success" : mosquitto_connack_string(rc));

	BMessage stateMsg(MSG_MQTT_CONN_STATE_CHANGED);
	stateMsg.AddBool("connected", rc == 0);
	stateMsg.AddInt32("rc", rc);
	client->PostMessage(&stateMsg);
}


void
MqttClient::_OnDisconnectCallback(struct mosquitto* mosq, void* obj, int rc)
{
	// This runs in mosquitto's network thread — post to looper for safe state change
	MqttClient* client = (MqttClient*)obj;
	fprintf(stderr, "[MQTT] Disconnected (rc=%d)\n", rc);

	BMessage stateMsg(MSG_MQTT_CONN_STATE_CHANGED);
	stateMsg.AddBool("connected", false);
	stateMsg.AddInt32("rc", rc);
	stateMsg.AddBool("was_disconnect", true);
	client->PostMessage(&stateMsg);
}


void
MqttClient::_HandleConnStateChanged(BMessage* message)
{
	// Runs in BLooper thread — safe to modify all state
	bool connected = false;
	int32 rc = 0;
	bool wasDisconnect = false;
	message->FindBool("connected", &connected);
	message->FindInt32("rc", &rc);
	message->FindBool("was_disconnect", &wasDisconnect);

	{
		BAutolock lock(fStateLock);
		fConnected = connected;
	}

	if (connected) {
		fReconnectDelay = kReconnectInitDelay;
		fprintf(stderr, "[MQTT] Connected successfully!\n");
		if (fTarget != NULL) {
			BMessage msg(MSG_MQTT_CONNECTED);
			BMessenger(fTarget).SendMessage(&msg);
		}
	} else if (wasDisconnect) {
		// Disconnect event
		if (fTarget != NULL) {
			BMessage msg(MSG_MQTT_DISCONNECTED);
			BMessenger(fTarget).SendMessage(&msg);
		}
		// Auto-reconnect on unexpected disconnect (rc != 0)
		if (rc != 0 && !fManualDisconnect && fSettings.enabled) {
			BString logEntry;
			logEntry.SetToFormat("Unexpected disconnect (rc=%d)", (int)rc);
			_SendLogEntry(MQTT_LOG_ERR, logEntry.String());
			_StartReconnectTimer();
		}
	} else {
		// Connection failure
		fprintf(stderr, "[MQTT] Connection failed: rc=%d\n", (int)rc);
		if (fTarget != NULL) {
			BMessage error(MSG_MQTT_ERROR);
			error.AddString("error", "Connection failed");
			BMessenger(fTarget).SendMessage(&error);
		}
		if (!fManualDisconnect && fSettings.enabled)
			_StartReconnectTimer();
	}
}


void
MqttClient::_OnPublishCallback(struct mosquitto* mosq, void* obj, int mid)
{
	fprintf(stderr, "[MQTT] Message %d published successfully\n", mid);
}
