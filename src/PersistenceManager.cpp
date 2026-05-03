/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * PersistenceManager.cpp — Settings persistence (key=value text files)
 */

#include "PersistenceManager.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


PersistenceManager* PersistenceManager::sInstance = NULL;


PersistenceManager*
PersistenceManager::Instance()
{
	if (sInstance == NULL)
		sInstance = new PersistenceManager();
	return sInstance;
}


PersistenceManager::PersistenceManager()
{
}


BString
PersistenceManager::SettingsDir() const
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return "";

	path.Append("Sestriere");
	create_directory(path.Path(), 0755);
	return BString(path.Path());
}


BString
PersistenceManager::SettingsPath(const char* filename) const
{
	BString dir = SettingsDir();
	if (dir.IsEmpty())
		return "";
	dir << "/" << filename;
	return dir;
}


// =============================================================================
// MQTT settings — text key=value format
// =============================================================================

status_t
PersistenceManager::SaveMqttSettings(const MqttSettings& s)
{
	BString path = SettingsPath("mqtt.settings");
	if (path.IsEmpty())
		return B_ERROR;

	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	BString content;
	content << "enabled=" << (s.enabled ? "1" : "0") << "\n";
	content << "latitude=" << s.latitude << "\n";
	content << "longitude=" << s.longitude << "\n";
	content << "iata=" << s.iataCode << "\n";
	content << "broker=" << s.broker << "\n";
	content << "port=" << s.port << "\n";
	content << "username=" << s.username << "\n";
	content << "password=" << s.password << "\n";

	file.Write(content.String(), content.Length());
	return B_OK;
}


status_t
PersistenceManager::LoadMqttSettings(MqttSettings* out)
{
	BString path = SettingsPath("mqtt.settings");
	if (path.IsEmpty())
		return B_ERROR;

	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	off_t size;
	file.GetSize(&size);
	if (size <= 0 || size > 4096)
		return B_ERROR;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return B_ERROR;
	}
	buffer[bytesRead] = '\0';

	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		char* eq = strchr(line, '=');
		if (eq != NULL) {
			*eq = '\0';
			const char* key = line;
			const char* value = eq + 1;

			if (strcmp(key, "enabled") == 0)
				out->enabled = (atoi(value) != 0);
			else if (strcmp(key, "latitude") == 0)
				out->latitude = atof(value);
			else if (strcmp(key, "longitude") == 0)
				out->longitude = atof(value);
			else if (strcmp(key, "iata") == 0)
				strlcpy(out->iataCode, value, sizeof(out->iataCode));
			else if (strcmp(key, "broker") == 0)
				strlcpy(out->broker, value, sizeof(out->broker));
			else if (strcmp(key, "port") == 0)
				out->port = atoi(value);
			else if (strcmp(key, "username") == 0)
				strlcpy(out->username, value, sizeof(out->username));
			else if (strcmp(key, "password") == 0)
				strlcpy(out->password, value, sizeof(out->password));
		}
		line = strtok_r(NULL, "\n", &saveptr);
	}
	delete[] buffer;

	// Validate
	if (out->port < 1 || out->port > 65535)
		out->port = 1883;
	if (out->latitude < -90.0 || out->latitude > 90.0)
		out->latitude = 0.0;
	if (out->longitude < -180.0 || out->longitude > 180.0)
		out->longitude = 0.0;

	return B_OK;
}


// =============================================================================
// Device settings
// =============================================================================

status_t
PersistenceManager::SaveDeviceSettings(const BMessage& archive)
{
	BString path = SettingsPath("device.settings");
	if (path.IsEmpty())
		return B_ERROR;

	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	int32 battType = 0;
	archive.FindInt32("battery_type", &battType);

	BString content;
	content << "battery_type=" << battType << "\n";
	file.Write(content.String(), content.Length());
	return B_OK;
}


status_t
PersistenceManager::LoadDeviceSettings(BMessage* outArchive)
{
	BString path = SettingsPath("device.settings");
	if (path.IsEmpty())
		return B_ERROR;

	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	off_t size;
	file.GetSize(&size);
	if (size <= 0 || size > 1024)
		return B_ERROR;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return B_ERROR;
	}
	buffer[bytesRead] = '\0';

	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		char* eq = strchr(line, '=');
		if (eq != NULL) {
			*eq = '\0';
			if (strcmp(line, "battery_type") == 0)
				outArchive->AddInt32("battery_type", atoi(eq + 1));
		}
		line = strtok_r(NULL, "\n", &saveptr);
	}
	delete[] buffer;
	return B_OK;
}


// =============================================================================
// UI settings
// =============================================================================

status_t
PersistenceManager::SaveUISettings(const BMessage& archive)
{
	BString path = SettingsPath("ui.settings");
	if (path.IsEmpty())
		return B_ERROR;

	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	BString content;
	bool val;
	if (archive.FindBool("show_chats", &val) == B_OK)
		content << "show_chats=" << (val ? "1" : "0") << "\n";
	if (archive.FindBool("show_repeaters", &val) == B_OK)
		content << "show_repeaters=" << (val ? "1" : "0") << "\n";
	if (archive.FindBool("show_rooms", &val) == B_OK)
		content << "show_rooms=" << (val ? "1" : "0") << "\n";

	float w0, w1, w2;
	if (archive.FindFloat("weight0", &w0) == B_OK
		&& archive.FindFloat("weight1", &w1) == B_OK
		&& archive.FindFloat("weight2", &w2) == B_OK) {
		BString weights;
		weights.SetToFormat("split_weights=%.4f,%.4f,%.4f\n", w0, w1, w2);
		content << weights;
	}

	file.Write(content.String(), content.Length());
	return B_OK;
}


status_t
PersistenceManager::LoadUISettings(BMessage* outArchive)
{
	BString path = SettingsPath("ui.settings");
	if (path.IsEmpty())
		return B_ERROR;

	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	off_t size;
	file.GetSize(&size);
	if (size <= 0 || size > 1024)
		return B_ERROR;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return B_ERROR;
	}
	buffer[bytesRead] = '\0';

	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		char* eq = strchr(line, '=');
		if (eq != NULL) {
			*eq = '\0';
			const char* key = line;
			const char* value = eq + 1;

			if (strcmp(key, "show_chats") == 0)
				outArchive->AddBool("show_chats", atoi(value) != 0);
			else if (strcmp(key, "show_repeaters") == 0)
				outArchive->AddBool("show_repeaters", atoi(value) != 0);
			else if (strcmp(key, "show_rooms") == 0)
				outArchive->AddBool("show_rooms", atoi(value) != 0);
			else if (strcmp(key, "split_weights") == 0) {
				float w0, w1, w2;
				if (sscanf(value, "%f,%f,%f", &w0, &w1, &w2) == 3) {
					outArchive->AddFloat("weight0", w0);
					outArchive->AddFloat("weight1", w1);
					outArchive->AddFloat("weight2", w2);
				}
			}
		}
		line = strtok_r(NULL, "\n", &saveptr);
	}
	delete[] buffer;
	return B_OK;
}


// =============================================================================
// Mute list / Contact groups — BMessage flattened format
// =============================================================================

status_t
PersistenceManager::SaveMuteList(const BMessage& keys)
{
	BString path = SettingsPath("mute_list");
	if (path.IsEmpty())
		return B_ERROR;
	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();
	return keys.Flatten(&file);
}


status_t
PersistenceManager::LoadMuteList(BMessage* outKeys)
{
	BString path = SettingsPath("mute_list");
	if (path.IsEmpty())
		return B_ERROR;
	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();
	return outKeys->Unflatten(&file);
}


status_t
PersistenceManager::SaveContactGroups(const BMessage& groups)
{
	BString path = SettingsPath("contact_groups");
	if (path.IsEmpty())
		return B_ERROR;
	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();
	return groups.Flatten(&file);
}


status_t
PersistenceManager::LoadContactGroups(BMessage* outGroups)
{
	BString path = SettingsPath("contact_groups");
	if (path.IsEmpty())
		return B_ERROR;
	BFile file(path.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();
	return outGroups->Unflatten(&file);
}
