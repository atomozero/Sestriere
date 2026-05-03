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


// Configurable advanced parameters (with defaults matching hardcoded values)
struct AdvancedSettings {
	int32		tileCacheMb;		// Default: 50
	int32		dbRetentionDays;	// Default: 30
	int32		voiceMaxSec;		// Default: 30
	int32		imageMaxDim;		// Default: 192
	int32		imageQuality;		// Default: 50
	int32		mediaMaxWidth;		// Default: 250
	int32		mediaMaxHeight;		// Default: 300

	AdvancedSettings()
		: tileCacheMb(50), dbRetentionDays(30), voiceMaxSec(30),
		  imageMaxDim(192), imageQuality(50),
		  mediaMaxWidth(250), mediaMaxHeight(300) {}
};


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

	// Advanced settings (configurable parameters)
	status_t				SaveAdvancedSettings(const AdvancedSettings& s);
	status_t				LoadAdvancedSettings(AdvancedSettings* out);

	// In-memory cache of advanced settings (loaded once at startup)
	const AdvancedSettings&	Advanced() const { return fAdvanced; }
	void					SetAdvanced(const AdvancedSettings& s)
								{ fAdvanced = s; }

private:
							PersistenceManager();

	status_t				_SaveMessage(const BMessage& msg,
								const char* filename);
	status_t				_LoadMessage(BMessage* msg,
								const char* filename);

	AdvancedSettings		fAdvanced;
	static PersistenceManager*	sInstance;
};


#endif // _PERSISTENCE_MANAGER_H
