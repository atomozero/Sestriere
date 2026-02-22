/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ProfileWindow.cpp — Profile export/import dialog (JSON format)
 */

#include "ProfileWindow.h"

#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <Path.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TabView.h>
#include <TextView.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "Constants.h"
#include "Utils.h"


// Window-private message codes
enum {
	kMsgExportClicked	= 'pxcl',
	kMsgBrowseClicked	= 'pbcl',
	kMsgImportClicked	= 'picl',
	kMsgSaveRef			= 'psrf',
	kMsgOpenRef			= 'porf',
};

// Maximum profile file size (256 KB)
static const off_t kMaxProfileSize = 256 * 1024;

// JSON format identifier
static const char* kProfileFormat = "sestriere-profile";
static const int kProfileVersion = 1;


ProfileWindow::ProfileWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 480, 420),
		"Export/Import Profile",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fTabView(NULL),
	fExportContacts(NULL),
	fExportChannels(NULL),
	fExportRadio(NULL),
	fExportMqtt(NULL),
	fExportMqttPassword(NULL),
	fExportButton(NULL),
	fExportStatus(NULL),
	fBrowseButton(NULL),
	fPreviewView(NULL),
	fImportContacts(NULL),
	fImportChannels(NULL),
	fImportRadio(NULL),
	fImportMqtt(NULL),
	fImportButton(NULL),
	fImportStatus(NULL),
	fSavePanel(NULL),
	fOpenPanel(NULL),
	fExContactCount(0),
	fExChannelCount(0),
	fExRadioFreq(0),
	fExRadioBw(0),
	fExRadioSf(0),
	fExRadioCr(0),
	fExRadioTxPower(0),
	fExHasRadio(false),
	fImportParsed(false)
{
	memset(fDeviceName, 0, sizeof(fDeviceName));
	memset(fPublicKey, 0, sizeof(fPublicKey));
	memset(fFirmware, 0, sizeof(fFirmware));
	memset(fExContacts, 0, sizeof(fExContacts));
	memset(fExChannels, 0, sizeof(fExChannels));

	fTabView = new BTabView("tab_view", B_WIDTH_FROM_WIDEST);
	fTabView->AddTab(_BuildExportTab());
	fTabView->AddTab(_BuildImportTab());

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fTabView)
	.End();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();
}


ProfileWindow::~ProfileWindow()
{
	delete fSavePanel;
	delete fOpenPanel;
}


bool
ProfileWindow::QuitRequested()
{
	Hide();
	return false;
}


void
ProfileWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgExportClicked:
		{
			if (fSavePanel == NULL) {
				BMessage saveMsg(kMsgSaveRef);
				fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this),
					NULL, B_FILE_NODE, false, &saveMsg, NULL, true, true);
				fSavePanel->SetSaveText("profile.json");
			}
			fSavePanel->Show();
			break;
		}

		case kMsgBrowseClicked:
		{
			if (fOpenPanel == NULL) {
				BMessage openMsg(kMsgOpenRef);
				fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this),
					NULL, B_FILE_NODE, false, &openMsg, NULL, true, true);
			}
			fOpenPanel->Show();
			break;
		}

		case kMsgSaveRef:
		{
			entry_ref ref;
			BString name;
			if (message->FindRef("directory", &ref) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BPath dirPath(&ref);
				BString fullPath(dirPath.Path());
				fullPath.Append("/");
				fullPath.Append(name);
				_DoExport(fullPath.String());
			}
			break;
		}

		case kMsgOpenRef:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK) {
				BPath filePath(&ref);
				_DoImport(filePath.Path());
			}
			break;
		}

		case kMsgImportClicked:
			_SendImportToMainWindow();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


// =============================================================================
// Data setters
// =============================================================================

void
ProfileWindow::SetDeviceInfo(const char* name, const char* publicKey,
	const char* firmware)
{
	strlcpy(fDeviceName, name, sizeof(fDeviceName));
	strlcpy(fPublicKey, publicKey, sizeof(fPublicKey));
	strlcpy(fFirmware, firmware, sizeof(fFirmware));
}


void
ProfileWindow::SetContacts(const OwningObjectList<ContactInfo>& contacts)
{
	fExContactCount = 0;
	for (int32 i = 0; i < contacts.CountItems() && fExContactCount < 256; i++) {
		ContactInfo* ci = contacts.ItemAt(i);
		if (ci == NULL || !ci->isValid)
			continue;
		ExportContact& ec = fExContacts[fExContactCount];
		memcpy(ec.publicKey, ci->publicKey, 32);
		strlcpy(ec.name, ci->name, sizeof(ec.name));
		ec.type = ci->type;
		ec.latitude = ci->latitude;
		ec.longitude = ci->longitude;
		fExContactCount++;
	}
}


void
ProfileWindow::SetChannels(const OwningObjectList<ChannelInfo>& channels)
{
	fExChannelCount = 0;
	for (int32 i = 0; i < channels.CountItems() && fExChannelCount < 16; i++) {
		ChannelInfo* ch = channels.ItemAt(i);
		if (ch == NULL || ch->IsEmpty())
			continue;
		ExportChannel& ec = fExChannels[fExChannelCount];
		ec.index = ch->index;
		strlcpy(ec.name, ch->name, sizeof(ec.name));
		memcpy(ec.secret, ch->secret, 16);
		fExChannelCount++;
	}
}


void
ProfileWindow::SetRadioParams(uint32 freq, uint32 bw, uint8 sf, uint8 cr,
	uint8 txPower)
{
	fExRadioFreq = freq;
	fExRadioBw = bw;
	fExRadioSf = sf;
	fExRadioCr = cr;
	fExRadioTxPower = txPower;
	fExHasRadio = true;
}


void
ProfileWindow::SetMqttSettings(const MqttSettings& settings)
{
	fExMqtt = settings;
}


// =============================================================================
// UI construction
// =============================================================================

BView*
ProfileWindow::_BuildExportTab()
{
	fExportContacts = new BCheckBox("ex_contacts", "Contacts", NULL);
	fExportContacts->SetValue(B_CONTROL_ON);
	fExportChannels = new BCheckBox("ex_channels", "Channels", NULL);
	fExportChannels->SetValue(B_CONTROL_ON);
	fExportRadio = new BCheckBox("ex_radio", "Radio parameters", NULL);
	fExportRadio->SetValue(B_CONTROL_ON);
	fExportMqtt = new BCheckBox("ex_mqtt", "MQTT settings", NULL);
	fExportMqtt->SetValue(B_CONTROL_ON);
	fExportMqttPassword = new BCheckBox("ex_mqttpw",
		"Include MQTT password", NULL);
	fExportMqttPassword->SetValue(B_CONTROL_OFF);

	fExportButton = new BButton("export_btn", "Export" B_UTF8_ELLIPSIS,
		new BMessage(kMsgExportClicked));
	fExportStatus = new BStringView("export_status", "");

	BView* view = new BView("Export", B_WILL_DRAW);
	BLayoutBuilder::Group<>(view, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fExportContacts)
		.Add(fExportChannels)
		.Add(fExportRadio)
		.Add(fExportMqtt)
		.Add(fExportMqttPassword)
		.AddStrut(B_USE_SMALL_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fExportStatus, 1.0)
			.Add(fExportButton)
		.End()
	.End();

	return view;
}


BView*
ProfileWindow::_BuildImportTab()
{
	fBrowseButton = new BButton("browse_btn", "Browse" B_UTF8_ELLIPSIS,
		new BMessage(kMsgBrowseClicked));

	fPreviewView = new BTextView("preview");
	fPreviewView->SetStylable(false);
	fPreviewView->MakeEditable(false);
	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);
	fPreviewView->SetFontAndColor(&monoFont);

	BScrollView* scrollView = new BScrollView("preview_scroll", fPreviewView,
		0, false, true, B_FANCY_BORDER);

	fImportContacts = new BCheckBox("im_contacts", "Contacts", NULL);
	fImportContacts->SetEnabled(false);
	fImportChannels = new BCheckBox("im_channels", "Channels", NULL);
	fImportChannels->SetEnabled(false);
	fImportRadio = new BCheckBox("im_radio", "Radio parameters", NULL);
	fImportRadio->SetEnabled(false);
	fImportMqtt = new BCheckBox("im_mqtt", "MQTT settings", NULL);
	fImportMqtt->SetEnabled(false);

	fImportButton = new BButton("import_btn", "Import",
		new BMessage(kMsgImportClicked));
	fImportButton->SetEnabled(false);

	fImportStatus = new BStringView("import_status", "");

	BView* view = new BView("Import", B_WILL_DRAW);
	BLayoutBuilder::Group<>(view, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fBrowseButton)
			.AddGlue()
		.End()
		.Add(scrollView, 1.0)
		.AddGroup(B_HORIZONTAL)
			.Add(fImportContacts)
			.Add(fImportChannels)
			.Add(fImportRadio)
			.Add(fImportMqtt)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fImportStatus, 1.0)
			.Add(fImportButton)
		.End()
	.End();

	return view;
}


// =============================================================================
// JSON Export
// =============================================================================

void
ProfileWindow::_DoExport(const char* path)
{
	FILE* f = fopen(path, "w");
	if (f == NULL) {
		fExportStatus->SetText("Error: cannot create file");
		return;
	}

	// Get current timestamp
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char timeStr[32];
	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);

	bool includeContacts = fExportContacts->Value() == B_CONTROL_ON;
	bool includeChannels = fExportChannels->Value() == B_CONTROL_ON;
	bool includeRadio = fExportRadio->Value() == B_CONTROL_ON;
	bool includeMqtt = fExportMqtt->Value() == B_CONTROL_ON;
	bool includeMqttPw = fExportMqttPassword->Value() == B_CONTROL_ON;

	fprintf(f, "{\n");
	fprintf(f, "  \"format\": \"%s\",\n", kProfileFormat);
	fprintf(f, "  \"version\": %d,\n", kProfileVersion);
	fprintf(f, "  \"exported\": \"%s\",\n", timeStr);

	// Device info (metadata only)
	fprintf(f, "  \"device\": {\n");
	fprintf(f, "    \"name\": \"%s\",\n", fDeviceName);
	fprintf(f, "    \"publicKey\": \"%s\",\n", fPublicKey);
	fprintf(f, "    \"firmware\": \"%s\"\n", fFirmware);
	fprintf(f, "  }");

	int exportedContacts = 0;
	int exportedChannels = 0;

	// Contacts
	if (includeContacts && fExContactCount > 0) {
		fprintf(f, ",\n  \"contacts\": [\n");
		for (int32 i = 0; i < fExContactCount; i++) {
			ExportContact& ec = fExContacts[i];
			char hexKey[65];
			FormatPubKeyFull(hexKey, ec.publicKey);

			fprintf(f, "    {\n");
			fprintf(f, "      \"publicKey\": \"%s\",\n", hexKey);
			fprintf(f, "      \"name\": \"%s\",\n", ec.name);
			fprintf(f, "      \"type\": %d", ec.type);
			if (ec.latitude != 0 || ec.longitude != 0) {
				fprintf(f, ",\n      \"latitude\": %d,\n", (int)ec.latitude);
				fprintf(f, "      \"longitude\": %d", (int)ec.longitude);
			}
			fprintf(f, "\n    }");
			if (i + 1 < fExContactCount)
				fprintf(f, ",");
			fprintf(f, "\n");
			exportedContacts++;
		}
		fprintf(f, "  ]");
	}

	// Channels
	if (includeChannels && fExChannelCount > 0) {
		fprintf(f, ",\n  \"channels\": [\n");
		for (int32 i = 0; i < fExChannelCount; i++) {
			ExportChannel& ec = fExChannels[i];
			char hexSecret[33];
			FormatHexBytes(hexSecret, ec.secret, 16);
			hexSecret[32] = '\0';

			fprintf(f, "    {\n");
			fprintf(f, "      \"index\": %d,\n", ec.index);
			fprintf(f, "      \"name\": \"%s\",\n", ec.name);
			fprintf(f, "      \"secret\": \"%s\"\n", hexSecret);
			fprintf(f, "    }");
			if (i + 1 < fExChannelCount)
				fprintf(f, ",");
			fprintf(f, "\n");
			exportedChannels++;
		}
		fprintf(f, "  ]");
	}

	// Radio
	if (includeRadio && fExHasRadio) {
		fprintf(f, ",\n  \"radio\": {\n");
		fprintf(f, "    \"frequency\": %u,\n", (unsigned)fExRadioFreq);
		fprintf(f, "    \"bandwidth\": %u,\n", (unsigned)fExRadioBw);
		fprintf(f, "    \"spreadingFactor\": %d,\n", fExRadioSf);
		fprintf(f, "    \"codingRate\": %d,\n", fExRadioCr);
		fprintf(f, "    \"txPower\": %d\n", fExRadioTxPower);
		fprintf(f, "  }");
	}

	// MQTT
	if (includeMqtt) {
		fprintf(f, ",\n  \"mqtt\": {\n");
		fprintf(f, "    \"enabled\": %s,\n", fExMqtt.enabled ? "true" : "false");
		fprintf(f, "    \"broker\": \"%s\",\n", fExMqtt.broker);
		fprintf(f, "    \"port\": %d,\n", fExMqtt.port);
		fprintf(f, "    \"username\": \"%s\",\n", fExMqtt.username);
		fprintf(f, "    \"password\": \"%s\",\n",
			includeMqttPw ? fExMqtt.password : "");
		fprintf(f, "    \"iataCode\": \"%s\",\n", fExMqtt.iataCode);
		fprintf(f, "    \"latitude\": %.7f,\n", fExMqtt.latitude);
		fprintf(f, "    \"longitude\": %.7f\n", fExMqtt.longitude);
		fprintf(f, "  }");
	}

	fprintf(f, "\n}\n");
	fclose(f);

	// Status message
	BString status;
	status.SetToFormat("Exported %d contacts, %d channels to %s",
		exportedContacts, exportedChannels,
		strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
	fExportStatus->SetText(status.String());
}


// =============================================================================
// JSON Import
// =============================================================================

void
ProfileWindow::_DoImport(const char* path)
{
	// Reset state
	fImportParsed = false;
	fImported = ImportedProfile();
	fPreviewView->SetText("");
	fImportContacts->SetEnabled(false);
	fImportContacts->SetValue(B_CONTROL_OFF);
	fImportChannels->SetEnabled(false);
	fImportChannels->SetValue(B_CONTROL_OFF);
	fImportRadio->SetEnabled(false);
	fImportRadio->SetValue(B_CONTROL_OFF);
	fImportMqtt->SetEnabled(false);
	fImportMqtt->SetValue(B_CONTROL_OFF);
	fImportButton->SetEnabled(false);

	// Read file
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK) {
		fImportStatus->SetText("Error: cannot open file");
		return;
	}

	off_t size;
	file.GetSize(&size);
	if (size <= 0) {
		fImportStatus->SetText("Error: empty file");
		return;
	}
	if (size > kMaxProfileSize) {
		fImportStatus->SetText("Error: file too large (max 256 KB)");
		return;
	}

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		fImportStatus->SetText("Error: cannot read file");
		return;
	}
	buffer[bytesRead] = '\0';

	// Validate format
	char format[64];
	if (_FindJsonString(buffer, "format", format, sizeof(format)) == NULL
		|| strcmp(format, kProfileFormat) != 0) {
		delete[] buffer;
		fImportStatus->SetText("Error: not a Sestriere profile");
		return;
	}

	int64 version = 0;
	if (!_FindJsonInt(buffer, "version", &version) || version != kProfileVersion) {
		delete[] buffer;
		fImportStatus->SetText("Error: unsupported format version");
		return;
	}

	// Parse device metadata
	const char* deviceSection = strstr(buffer, "\"device\"");
	if (deviceSection != NULL) {
		_FindJsonString(deviceSection, "name",
			fImported.deviceName, sizeof(fImported.deviceName));
		_FindJsonString(deviceSection, "publicKey",
			fImported.publicKey, sizeof(fImported.publicKey));
		_FindJsonString(deviceSection, "firmware",
			fImported.firmware, sizeof(fImported.firmware));
	}

	// Parse contacts
	const char* contactsArray = _FindJsonArray(buffer, "contacts");
	if (contactsArray != NULL) {
		const char* objEnd = NULL;
		const char* obj = _NextJsonObject(contactsArray, &objEnd);
		while (obj != NULL && fImported.contactCount < 256) {
			ImportedProfile::ImportedContact& ic =
				fImported.contacts[fImported.contactCount];

			char hexKey[65];
			if (_FindJsonString(obj, "publicKey", hexKey, sizeof(hexKey)) != NULL
				&& strlen(hexKey) == 64) {
				if (ParseHexPubKey(ic.publicKey, hexKey)) {
					_FindJsonString(obj, "name", ic.name, sizeof(ic.name));
					int64 type = 0;
					_FindJsonInt(obj, "type", &type);
					ic.type = (uint8)type;
					int64 lat = 0, lon = 0;
					_FindJsonInt(obj, "latitude", &lat);
					_FindJsonInt(obj, "longitude", &lon);
					ic.latitude = (int32)lat;
					ic.longitude = (int32)lon;
					fImported.contactCount++;
				}
			}
			obj = _NextJsonObject(objEnd, &objEnd);
		}
	}

	// Parse channels
	const char* channelsArray = _FindJsonArray(buffer, "channels");
	if (channelsArray != NULL) {
		const char* objEnd = NULL;
		const char* obj = _NextJsonObject(channelsArray, &objEnd);
		while (obj != NULL && fImported.channelCount < 16) {
			ImportedProfile::ImportedChannel& ic =
				fImported.channels[fImported.channelCount];

			int64 index = 0;
			_FindJsonInt(obj, "index", &index);
			ic.index = (uint8)index;

			_FindJsonString(obj, "name", ic.name, sizeof(ic.name));

			char hexSecret[33];
			if (_FindJsonString(obj, "secret", hexSecret, sizeof(hexSecret)) != NULL
				&& strlen(hexSecret) == 32) {
				for (int i = 0; i < 16; i++) {
					unsigned int byte;
					sscanf(hexSecret + i * 2, "%2x", &byte);
					ic.secret[i] = (uint8)byte;
				}
				fImported.channelCount++;
			}
			obj = _NextJsonObject(objEnd, &objEnd);
		}
	}

	// Parse radio
	const char* radioSection = strstr(buffer, "\"radio\"");
	if (radioSection != NULL) {
		int64 freq = 0, bw = 0, sf = 0, cr = 0, txp = 0;
		if (_FindJsonInt(radioSection, "frequency", &freq) && freq > 0) {
			fImported.radioFreq = (uint32)freq;
			_FindJsonInt(radioSection, "bandwidth", &bw);
			fImported.radioBw = (uint32)bw;
			_FindJsonInt(radioSection, "spreadingFactor", &sf);
			fImported.radioSf = (uint8)sf;
			_FindJsonInt(radioSection, "codingRate", &cr);
			fImported.radioCr = (uint8)cr;
			_FindJsonInt(radioSection, "txPower", &txp);
			fImported.radioTxPower = (uint8)txp;
			fImported.hasRadio = true;
		}
	}

	// Parse MQTT
	const char* mqttSection = strstr(buffer, "\"mqtt\"");
	if (mqttSection != NULL) {
		fImported.hasMqtt = true;
		_FindJsonBool(mqttSection, "enabled", &fImported.mqtt.enabled);
		_FindJsonString(mqttSection, "broker",
			fImported.mqtt.broker, sizeof(fImported.mqtt.broker));
		int64 port = 1883;
		_FindJsonInt(mqttSection, "port", &port);
		fImported.mqtt.port = (int)port;
		_FindJsonString(mqttSection, "username",
			fImported.mqtt.username, sizeof(fImported.mqtt.username));
		_FindJsonString(mqttSection, "password",
			fImported.mqtt.password, sizeof(fImported.mqtt.password));
		_FindJsonString(mqttSection, "iataCode",
			fImported.mqtt.iataCode, sizeof(fImported.mqtt.iataCode));
		_FindJsonDouble(mqttSection, "latitude", &fImported.mqtt.latitude);
		_FindJsonDouble(mqttSection, "longitude", &fImported.mqtt.longitude);
	}

	delete[] buffer;

	fImportParsed = true;
	_ShowPreview();
}


void
ProfileWindow::_ShowPreview()
{
	if (!fImportParsed)
		return;

	BString preview;

	// Device info
	if (fImported.deviceName[0] != '\0')
		preview << "Device: " << fImported.deviceName << "\n";
	if (fImported.firmware[0] != '\0')
		preview << "Firmware: " << fImported.firmware << "\n";
	if (fImported.publicKey[0] != '\0') {
		preview << "Key: ";
		preview.Append(fImported.publicKey, 12);
		preview << "...\n";
	}
	preview << "\n";

	// Contacts summary
	if (fImported.contactCount > 0) {
		preview << "Contacts: " << fImported.contactCount << "\n";
		for (int32 i = 0; i < fImported.contactCount && i < 10; i++) {
			preview << "  - " << fImported.contacts[i].name;
			uint8 type = fImported.contacts[i].type;
			if (type == 1) preview << " (chat)";
			else if (type == 2) preview << " (repeater)";
			else if (type == 3) preview << " (room)";
			preview << "\n";
		}
		if (fImported.contactCount > 10)
			preview << "  ... and " << (fImported.contactCount - 10) << " more\n";

		fImportContacts->SetEnabled(true);
		fImportContacts->SetValue(B_CONTROL_ON);
	} else {
		preview << "Contacts: (none)\n";
	}
	preview << "\n";

	// Channels summary
	if (fImported.channelCount > 0) {
		preview << "Channels: " << fImported.channelCount << "\n";
		for (int32 i = 0; i < fImported.channelCount; i++) {
			preview << "  - [" << (int)fImported.channels[i].index << "] "
				<< fImported.channels[i].name << "\n";
		}
		fImportChannels->SetEnabled(true);
		fImportChannels->SetValue(B_CONTROL_ON);
	} else {
		preview << "Channels: (none)\n";
	}
	preview << "\n";

	// Radio summary
	if (fImported.hasRadio) {
		preview << "Radio: ";
		preview << (fImported.radioFreq / 1000000.0) << " MHz, ";
		preview << (fImported.radioBw / 1000) << " kHz, ";
		preview << "SF" << (int)fImported.radioSf << ", ";
		preview << "CR" << (int)fImported.radioCr << ", ";
		preview << (int)fImported.radioTxPower << " dBm\n";

		fImportRadio->SetEnabled(true);
		fImportRadio->SetValue(B_CONTROL_ON);
	} else {
		preview << "Radio: (none)\n";
	}
	preview << "\n";

	// MQTT summary
	if (fImported.hasMqtt) {
		preview << "MQTT: " << fImported.mqtt.broker;
		if (fImported.mqtt.port != 1883)
			preview << ":" << fImported.mqtt.port;
		preview << " (" << (fImported.mqtt.enabled ? "enabled" : "disabled") << ")\n";

		fImportMqtt->SetEnabled(true);
		fImportMqtt->SetValue(B_CONTROL_ON);
	} else {
		preview << "MQTT: (none)\n";
	}

	preview << "\n--- This file contains cryptographic keys. Store it securely. ---\n";

	fPreviewView->SetText(preview.String());
	fImportButton->SetEnabled(true);
	fImportStatus->SetText("Ready to import. Select sections above.");
}


void
ProfileWindow::_SendImportToMainWindow()
{
	if (!fImportParsed || fParent == NULL)
		return;

	int importedSections = 0;

	// Import contacts
	if (fImportContacts->Value() == B_CONTROL_ON && fImported.contactCount > 0) {
		BMessage msg(MSG_PROFILE_IMPORT_CONTACTS);
		for (int32 i = 0; i < fImported.contactCount; i++) {
			ImportedProfile::ImportedContact& ic = fImported.contacts[i];
			msg.AddData("pubkey", B_RAW_TYPE, ic.publicKey, 32);
			msg.AddString("name", ic.name);
			msg.AddInt32("type", ic.type);
		}
		fParent->PostMessage(&msg);
		importedSections++;
	}

	// Import channels
	if (fImportChannels->Value() == B_CONTROL_ON && fImported.channelCount > 0) {
		BMessage msg(MSG_PROFILE_IMPORT_CHANNELS);
		for (int32 i = 0; i < fImported.channelCount; i++) {
			ImportedProfile::ImportedChannel& ic = fImported.channels[i];
			msg.AddInt32("index", ic.index);
			msg.AddString("name", ic.name);
			msg.AddData("secret", B_RAW_TYPE, ic.secret, 16);
		}
		fParent->PostMessage(&msg);
		importedSections++;
	}

	// Import radio
	if (fImportRadio->Value() == B_CONTROL_ON && fImported.hasRadio) {
		BMessage msg(MSG_PROFILE_IMPORT_RADIO);
		msg.AddUInt32("frequency", fImported.radioFreq);
		msg.AddUInt32("bandwidth", fImported.radioBw);
		msg.AddUInt8("sf", fImported.radioSf);
		msg.AddUInt8("cr", fImported.radioCr);
		msg.AddUInt8("txPower", fImported.radioTxPower);
		fParent->PostMessage(&msg);
		importedSections++;
	}

	// Import MQTT
	if (fImportMqtt->Value() == B_CONTROL_ON && fImported.hasMqtt) {
		BMessage msg(MSG_PROFILE_IMPORT_MQTT);
		msg.AddBool("enabled", fImported.mqtt.enabled);
		msg.AddString("broker", fImported.mqtt.broker);
		msg.AddInt32("port", fImported.mqtt.port);
		msg.AddString("username", fImported.mqtt.username);
		msg.AddString("password", fImported.mqtt.password);
		msg.AddString("iata", fImported.mqtt.iataCode);
		msg.AddDouble("latitude", fImported.mqtt.latitude);
		msg.AddDouble("longitude", fImported.mqtt.longitude);
		fParent->PostMessage(&msg);
		importedSections++;
	}

	if (importedSections > 0) {
		BString status;
		status.SetToFormat("Import sent (%d sections). Check device.",
			importedSections);
		fImportStatus->SetText(status.String());
	} else {
		fImportStatus->SetText("No sections selected.");
	}
}


// =============================================================================
// JSON parse helpers (strstr-based, schema-fixed)
// =============================================================================

const char*
ProfileWindow::_FindJsonString(const char* json, const char* key,
	char* dest, size_t destSize)
{
	// Search for "key" pattern
	char searchKey[128];
	snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

	const char* pos = strstr(json, searchKey);
	if (pos == NULL)
		return NULL;

	// Skip past key and find colon
	pos += strlen(searchKey);
	while (*pos != '\0' && *pos != ':')
		pos++;
	if (*pos == '\0')
		return NULL;
	pos++; // skip ':'

	// Skip whitespace
	while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
		pos++;

	if (*pos != '"')
		return NULL;
	pos++; // skip opening quote

	// Copy string content, handle escapes
	size_t di = 0;
	while (*pos != '\0' && *pos != '"' && di < destSize - 1) {
		if (*pos == '\\' && *(pos + 1) != '\0') {
			pos++;
			if (*pos == '"') dest[di++] = '"';
			else if (*pos == '\\') dest[di++] = '\\';
			else if (*pos == 'n') dest[di++] = '\n';
			else dest[di++] = *pos;
		} else {
			dest[di++] = *pos;
		}
		pos++;
	}
	dest[di] = '\0';

	return dest;
}


bool
ProfileWindow::_FindJsonInt(const char* json, const char* key, int64* value)
{
	char searchKey[128];
	snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

	const char* pos = strstr(json, searchKey);
	if (pos == NULL)
		return false;

	pos += strlen(searchKey);
	while (*pos != '\0' && *pos != ':')
		pos++;
	if (*pos == '\0')
		return false;
	pos++;

	while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
		pos++;

	char* end = NULL;
	*value = strtoll(pos, &end, 10);
	return (end != pos);
}


bool
ProfileWindow::_FindJsonBool(const char* json, const char* key, bool* value)
{
	char searchKey[128];
	snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

	const char* pos = strstr(json, searchKey);
	if (pos == NULL)
		return false;

	pos += strlen(searchKey);
	while (*pos != '\0' && *pos != ':')
		pos++;
	if (*pos == '\0')
		return false;
	pos++;

	while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
		pos++;

	if (strncmp(pos, "true", 4) == 0) {
		*value = true;
		return true;
	}
	if (strncmp(pos, "false", 5) == 0) {
		*value = false;
		return true;
	}
	return false;
}


bool
ProfileWindow::_FindJsonDouble(const char* json, const char* key, double* value)
{
	char searchKey[128];
	snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

	const char* pos = strstr(json, searchKey);
	if (pos == NULL)
		return false;

	pos += strlen(searchKey);
	while (*pos != '\0' && *pos != ':')
		pos++;
	if (*pos == '\0')
		return false;
	pos++;

	while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
		pos++;

	char* end = NULL;
	*value = strtod(pos, &end);
	return (end != pos);
}


const char*
ProfileWindow::_FindJsonArray(const char* json, const char* key)
{
	char searchKey[128];
	snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

	const char* pos = strstr(json, searchKey);
	if (pos == NULL)
		return NULL;

	pos += strlen(searchKey);
	while (*pos != '\0' && *pos != '[')
		pos++;
	if (*pos == '\0')
		return NULL;

	return pos; // points to '['
}


const char*
ProfileWindow::_NextJsonObject(const char* pos, const char** objEnd)
{
	if (pos == NULL)
		return NULL;

	// Find next '{'
	while (*pos != '\0' && *pos != '{' && *pos != ']')
		pos++;
	if (*pos != '{')
		return NULL;

	const char* start = pos;

	// Find matching '}' (handle nested braces)
	int depth = 0;
	bool inString = false;
	while (*pos != '\0') {
		if (*pos == '"' && (pos == start || *(pos - 1) != '\\'))
			inString = !inString;
		else if (!inString) {
			if (*pos == '{') depth++;
			else if (*pos == '}') {
				depth--;
				if (depth == 0) {
					if (objEnd != NULL)
						*objEnd = pos + 1;
					return start;
				}
			}
		}
		pos++;
	}
	return NULL;
}
