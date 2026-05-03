/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * PersistenceManager.h — Settings persistence (MQTT, device, UI)
 */

#ifndef _PERSISTENCE_MANAGER_H
#define _PERSISTENCE_MANAGER_H

#include <Message.h>
#include <String.h>

#include "MqttClient.h"


// Consolidates all settings file I/O into one place.
// MainWindow retains the in-memory state but delegates read/write to this class.

class PersistenceManager {
public:
	static PersistenceManager*	Instance();

	// Paths
	BString					SettingsDir() const;
	BString					SettingsPath(const char* filename) const;

	// MQTT settings
	status_t				SaveMqttSettings(const MqttSettings& settings);
	status_t				LoadMqttSettings(MqttSettings* outSettings);

	// Device settings (battery type, etc.)
	status_t				SaveDeviceSettings(const BMessage& archive);
	status_t				LoadDeviceSettings(BMessage* outArchive);

	// UI settings (sidebar width, window rect, etc.)
	status_t				SaveUISettings(const BMessage& archive);
	status_t				LoadUISettings(BMessage* outArchive);

	// Mute list
	status_t				SaveMuteList(const BMessage& keys);
	status_t				LoadMuteList(BMessage* outKeys);

	// Contact groups
	status_t				SaveContactGroups(const BMessage& groups);
	status_t				LoadContactGroups(BMessage* outGroups);

private:
							PersistenceManager();

	status_t				_SaveMessage(const BMessage& msg,
								const char* filename);
	status_t				_LoadMessage(BMessage* msg,
								const char* filename);

	static PersistenceManager*	sInstance;
};


#endif // _PERSISTENCE_MANAGER_H
