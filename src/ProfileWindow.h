/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ProfileWindow.h — Profile export/import dialog (JSON format)
 */

#ifndef PROFILEWINDOW_H
#define PROFILEWINDOW_H

#include <Window.h>

#include "Compat.h"
#include "MqttClient.h"
#include "Types.h"

class BButton;
class BCheckBox;
class BFilePanel;
class BStringView;
class BTabView;
class BTextView;


// Imported profile data (parsed from JSON)
struct ImportedProfile {
	// Device metadata (informational only)
	char	deviceName[64];
	char	publicKey[65];
	char	firmware[32];

	// Contacts
	struct ImportedContact {
		uint8	publicKey[32];
		char	name[64];
		uint8	type;
		int32	latitude;
		int32	longitude;
	};
	ImportedContact	contacts[256];
	int32			contactCount;

	// Channels
	struct ImportedChannel {
		uint8	index;
		char	name[32];
		uint8	secret[16];
	};
	ImportedChannel	channels[16];
	int32			channelCount;

	// Radio
	uint32	radioFreq;
	uint32	radioBw;
	uint8	radioSf;
	uint8	radioCr;
	uint8	radioTxPower;
	bool	hasRadio;

	// MQTT
	MqttSettings	mqtt;
	bool			hasMqtt;

	ImportedProfile() : contactCount(0), channelCount(0),
		radioFreq(0), radioBw(0), radioSf(0), radioCr(0), radioTxPower(0),
		hasRadio(false), hasMqtt(false) {
		memset(deviceName, 0, sizeof(deviceName));
		memset(publicKey, 0, sizeof(publicKey));
		memset(firmware, 0, sizeof(firmware));
		memset(contacts, 0, sizeof(contacts));
		memset(channels, 0, sizeof(channels));
	}
};


class ProfileWindow : public BWindow {
public:
							ProfileWindow(BWindow* parent);
	virtual					~ProfileWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

	// Data setters (called by MainWindow before Show)
	void					SetDeviceInfo(const char* name,
								const char* publicKey,
								const char* firmware);
	void					SetContacts(
								const OwningObjectList<ContactInfo>& contacts);
	void					SetChannels(
								const OwningObjectList<ChannelInfo>& channels);
	void					SetRadioParams(uint32 freq, uint32 bw,
								uint8 sf, uint8 cr, uint8 txPower);
	void					SetMqttSettings(const MqttSettings& settings);

private:
	// UI construction
	BView*					_BuildExportTab();
	BView*					_BuildImportTab();

	// JSON export
	void					_DoExport(const char* path);

	// JSON import
	void					_DoImport(const char* path);
	void					_ShowPreview();
	void					_SendImportToMainWindow();

	// JSON parse helpers (strstr-based, schema-fixed)
	const char*				_FindJsonString(const char* json,
								const char* key, char* dest,
								size_t destSize);
	bool					_FindJsonInt(const char* json,
								const char* key, int64* value);
	bool					_FindJsonBool(const char* json,
								const char* key, bool* value);
	bool					_FindJsonDouble(const char* json,
								const char* key, double* value);
	const char*				_FindJsonArray(const char* json,
								const char* key);
	const char*				_NextJsonObject(const char* pos,
								const char** objEnd);

	BWindow*				fParent;

	// Tab views
	BTabView*				fTabView;

	// Export tab controls
	BCheckBox*				fExportContacts;
	BCheckBox*				fExportChannels;
	BCheckBox*				fExportRadio;
	BCheckBox*				fExportMqtt;
	BCheckBox*				fExportMqttPassword;
	BButton*				fExportButton;
	BStringView*			fExportStatus;

	// Import tab controls
	BButton*				fBrowseButton;
	BTextView*				fPreviewView;
	BCheckBox*				fImportContacts;
	BCheckBox*				fImportChannels;
	BCheckBox*				fImportRadio;
	BCheckBox*				fImportMqtt;
	BButton*				fImportButton;
	BStringView*			fImportStatus;

	// File panels
	BFilePanel*				fSavePanel;
	BFilePanel*				fOpenPanel;

	// Export data (copied from MainWindow)
	char					fDeviceName[64];
	char					fPublicKey[65];
	char					fFirmware[32];

	struct ExportContact {
		uint8	publicKey[32];
		char	name[64];
		uint8	type;
		int32	latitude;
		int32	longitude;
	};
	ExportContact			fExContacts[256];
	int32					fExContactCount;

	struct ExportChannel {
		uint8	index;
		char	name[32];
		uint8	secret[16];
	};
	ExportChannel			fExChannels[16];
	int32					fExChannelCount;

	uint32					fExRadioFreq;
	uint32					fExRadioBw;
	uint8					fExRadioSf;
	uint8					fExRadioCr;
	uint8					fExRadioTxPower;
	bool					fExHasRadio;

	MqttSettings			fExMqtt;

	// Imported data
	ImportedProfile			fImported;
	bool					fImportParsed;
};


#endif // PROFILEWINDOW_H
