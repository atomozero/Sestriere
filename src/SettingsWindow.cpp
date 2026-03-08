/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SettingsWindow.cpp — Device and radio settings dialog implementation
 */

#include "SettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <SeparatorView.h>
#include <Slider.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "MqttClient.h"
#include "Utils.h"


static const uint32 kMsgSettingChanged	= 'stch';
static const uint32 kMsgApplySettings	= 'apst';
static const uint32 kMsgRevertSettings	= 'rvst';
static const uint32 kMsgPresetSelected	= 'prsl';
static const uint32 kMsgMqttEnableChanged = 'mqen';
static const uint32 kMsgTogglePasswordVis = 'tpvs';
static const uint32 kMsgBatteryTypeSelected = 'btsl';


SettingsWindow::SettingsWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 420, 400), "Settings",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fNodeNameControl(NULL),
	fLatitudeControl(NULL),
	fLongitudeControl(NULL),
	fBatteryTypeMenu(NULL),
	fPresetMenu(NULL),
	fTxPowerSlider(NULL),
	fFrequencyControl(NULL),
	fBandwidthControl(NULL),
	fSpreadingFactorControl(NULL),
	fCodingRateControl(NULL),
	fMqttEnableCheck(NULL),
	fMqttIataControl(NULL),
	fMqttBrokerControl(NULL),
	fMqttPortControl(NULL),
	fMqttUsernameControl(NULL),
	fMqttPasswordControl(NULL),
	fMqttStatusLabel(NULL),
	fApplyButton(NULL),
	fRevertButton(NULL),
	fSettingsChanged(false),
	fSelectedPreset(PRESET_CUSTOM)
{
	BTabView* tabView = new BTabView("settings_tabs", B_WIDTH_FROM_WIDEST);

	// Device tab
	BView* deviceTab = new BView("device_tab", 0);
	deviceTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildDeviceTab(deviceTab);
	tabView->AddTab(deviceTab, new BTab());
	tabView->TabAt(0)->SetLabel("Device");

	// Radio tab
	BView* radioTab = new BView("radio_tab", 0);
	radioTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildRadioTab(radioTab);
	tabView->AddTab(radioTab, new BTab());
	tabView->TabAt(1)->SetLabel("Radio");

	// MQTT tab
	BView* mqttTab = new BView("mqtt_tab", 0);
	mqttTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildMqttTab(mqttTab);
	tabView->AddTab(mqttTab, new BTab());
	tabView->TabAt(2)->SetLabel("MQTT");

	// About tab
	BView* aboutTab = new BView("about_tab", 0);
	aboutTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildAboutTab(aboutTab);
	tabView->AddTab(aboutTab, new BTab());
	tabView->TabAt(3)->SetLabel("About");

	// Buttons
	fApplyButton = new BButton("apply_button", "Apply",
		new BMessage(kMsgApplySettings));
	fApplyButton->SetEnabled(false);

	fRevertButton = new BButton("revert_button", "Revert",
		new BMessage(kMsgRevertSettings));
	fRevertButton->SetEnabled(false);

	BButton* cancelButton = new BButton("cancel_button", "Cancel",
		new BMessage(B_QUIT_REQUESTED));

	// Main layout
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(tabView, 1.0)
		.AddGroup(B_HORIZONTAL)
			.Add(fRevertButton)
			.AddGlue()
			.Add(cancelButton)
			.Add(fApplyButton)
		.End()
	.End();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();
}


SettingsWindow::~SettingsWindow()
{
}


bool
SettingsWindow::QuitRequested()
{
	Hide();
	return false;
}


void
SettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSettingChanged:
			fSettingsChanged = true;
			fApplyButton->SetEnabled(true);
			fRevertButton->SetEnabled(true);
			break;

		case kMsgApplySettings:
			_OnApply();
			break;

		case kMsgRevertSettings:
			// Reset to empty / defaults
			if (fNodeNameControl != NULL)
				fNodeNameControl->SetText("");
			if (fLatitudeControl != NULL)
				fLatitudeControl->SetText("0.000000");
			if (fLongitudeControl != NULL)
				fLongitudeControl->SetText("0.000000");
			fSettingsChanged = false;
			fApplyButton->SetEnabled(false);
			fRevertButton->SetEnabled(false);
			break;

		case kMsgPresetSelected:
		{
			int32 preset;
			if (message->FindInt32("preset", &preset) == B_OK)
				_OnPresetSelected(preset);
			break;
		}

		case kMsgMqttEnableChanged:
			_OnMqttEnableChanged();
			fSettingsChanged = true;
			fApplyButton->SetEnabled(true);
			fRevertButton->SetEnabled(true);
			break;

		case kMsgBatteryTypeSelected:
		{
			fSettingsChanged = true;
			fApplyButton->SetEnabled(true);
			fRevertButton->SetEnabled(true);
			break;
		}

		case kMsgTogglePasswordVis:
		{
			bool hidden = fMqttPasswordControl->TextView()->IsTypingHidden();
			fMqttPasswordControl->TextView()->HideTyping(!hidden);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
SettingsWindow::SetDeviceName(const char* name)
{
	if (fNodeNameControl != NULL)
		fNodeNameControl->SetText(name);
}


void
SettingsWindow::SetLatitude(double lat)
{
	if (fLatitudeControl != NULL) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%.6f", lat);
		fLatitudeControl->SetText(buf);
	}
}


void
SettingsWindow::SetLongitude(double lon)
{
	if (fLongitudeControl != NULL) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%.6f", lon);
		fLongitudeControl->SetText(buf);
	}
}


void
SettingsWindow::SetBatteryType(uint8 type)
{
	if (fBatteryTypeMenu != NULL && type < BATTERY_CHEMISTRY_COUNT) {
		BMenu* menu = fBatteryTypeMenu->Menu();
		if (menu != NULL) {
			BMenuItem* item = menu->ItemAt(type);
			if (item != NULL)
				item->SetMarked(true);
		}
	}
}


void
SettingsWindow::SetRadioPreset(int32 preset)
{
	if (preset >= 0 && preset < PRESET_COUNT)
		_OnPresetSelected(preset);
}


void
SettingsWindow::SetRadioParams(uint32 freqHz, uint32 bwHz, uint8 sf, uint8 cr,
	uint8 txPower)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%.3f", freqHz / 1000000.0);
	fFrequencyControl->SetText(buf);

	snprintf(buf, sizeof(buf), "%.1f", bwHz / 1000.0);
	fBandwidthControl->SetText(buf);

	snprintf(buf, sizeof(buf), "%u", sf);
	fSpreadingFactorControl->SetText(buf);

	snprintf(buf, sizeof(buf), "%u", cr);
	fCodingRateControl->SetText(buf);

	fTxPowerSlider->SetValue(txPower);
}


void
SettingsWindow::_BuildDeviceTab(BView* parent)
{
	fNodeNameControl = new BTextControl("node_name", "Node Name:", "",
		new BMessage(kMsgSettingChanged));
	fNodeNameControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fLatitudeControl = new BTextControl("latitude", "Latitude:", "",
		new BMessage(kMsgSettingChanged));
	fLatitudeControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fLongitudeControl = new BTextControl("longitude", "Longitude:", "",
		new BMessage(kMsgSettingChanged));
	fLongitudeControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	// Battery chemistry dropdown
	BPopUpMenu* battPopUp = new BPopUpMenu("battery_popup");
	for (int i = 0; i < BATTERY_CHEMISTRY_COUNT; i++) {
		BMessage* msg = new BMessage(kMsgBatteryTypeSelected);
		msg->AddInt32("type", i);
		BMenuItem* item = new BMenuItem(kBatteryChemistryNames[i], msg);
		battPopUp->AddItem(item);
	}
	fBatteryTypeMenu = new BMenuField("battery_type", "Battery:", battPopUp);
	// Default to LiPo
	battPopUp->ItemAt(BATTERY_LIPO)->SetMarked(true);

	BStringView* infoLabel = new BStringView("info",
		"Changes will be sent to the connected device.");
	infoLabel->SetHighColor(kStatusOffline);

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fNodeNameControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fNodeNameControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fLatitudeControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fLatitudeControl->CreateTextViewLayoutItem(), 1, 1)
			.Add(fLongitudeControl->CreateLabelLayoutItem(), 0, 2)
			.Add(fLongitudeControl->CreateTextViewLayoutItem(), 1, 2)
		.End()
		.Add(fBatteryTypeMenu)
		.AddStrut(B_USE_DEFAULT_SPACING)
		.Add(infoLabel)
		.AddGlue()
	.End();
}


void
SettingsWindow::_BuildRadioTab(BView* parent)
{
	// Preset menu
	BPopUpMenu* presetPopUp = new BPopUpMenu("preset_popup");
	for (int i = 0; i < PRESET_COUNT; i++) {
		BMessage* msg = new BMessage(kMsgPresetSelected);
		msg->AddInt32("preset", i);
		BMenuItem* item = new BMenuItem(kRadioPresets[i].name, msg);
		presetPopUp->AddItem(item);
	}
	fPresetMenu = new BMenuField("preset_menu", "Preset:", presetPopUp);

	fTxPowerSlider = new BSlider("tx_power", "TX Power (dBm)",
		new BMessage(kMsgSettingChanged), 0, 22, B_HORIZONTAL);
	fTxPowerSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fTxPowerSlider->SetHashMarkCount(12);
	fTxPowerSlider->SetLimitLabels("0", "22");
	fTxPowerSlider->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fFrequencyControl = new BTextControl("frequency", "Frequency (MHz):", "",
		new BMessage(kMsgSettingChanged));
	fFrequencyControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fBandwidthControl = new BTextControl("bandwidth", "Bandwidth (kHz):", "",
		new BMessage(kMsgSettingChanged));
	fBandwidthControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fSpreadingFactorControl = new BTextControl("sf", "Spreading Factor:", "",
		new BMessage(kMsgSettingChanged));
	fSpreadingFactorControl->SetModificationMessage(
		new BMessage(kMsgSettingChanged));

	fCodingRateControl = new BTextControl("cr", "Coding Rate:", "",
		new BMessage(kMsgSettingChanged));
	fCodingRateControl->SetModificationMessage(
		new BMessage(kMsgSettingChanged));

	BStringView* warningLabel = new BStringView("warning",
		"Warning: Changing radio parameters may break\n"
		"communication with other nodes.");
	warningLabel->SetHighColor(kColorBad);

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fPresetMenu)
		.Add(fTxPowerSlider)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fFrequencyControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fFrequencyControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fBandwidthControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fBandwidthControl->CreateTextViewLayoutItem(), 1, 1)
			.Add(fSpreadingFactorControl->CreateLabelLayoutItem(), 0, 2)
			.Add(fSpreadingFactorControl->CreateTextViewLayoutItem(), 1, 2)
			.Add(fCodingRateControl->CreateLabelLayoutItem(), 0, 3)
			.Add(fCodingRateControl->CreateTextViewLayoutItem(), 1, 3)
		.End()
		.AddStrut(B_USE_DEFAULT_SPACING)
		.Add(warningLabel)
		.AddGlue()
	.End();
}


void
SettingsWindow::_BuildMqttTab(BView* parent)
{
	fMqttEnableCheck = new BCheckBox("mqtt_enable", "Enable MQTT publishing",
		new BMessage(kMsgMqttEnableChanged));

	fMqttIataControl = new BTextControl("mqtt_iata", "IATA Code:", "",
		new BMessage(kMsgSettingChanged));
	fMqttIataControl->SetToolTip("Location code (e.g., VCE for Venice)");
	fMqttIataControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fMqttBrokerControl = new BTextControl("mqtt_broker", "Broker:", "",
		new BMessage(kMsgSettingChanged));
	fMqttBrokerControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fMqttPortControl = new BTextControl("mqtt_port", "Port:", "",
		new BMessage(kMsgSettingChanged));
	fMqttPortControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fMqttUsernameControl = new BTextControl("mqtt_user", "Username:", "",
		new BMessage(kMsgSettingChanged));
	fMqttUsernameControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	fMqttPasswordControl = new BTextControl("mqtt_pass", "Password:", "",
		new BMessage(kMsgSettingChanged));
	fMqttPasswordControl->TextView()->HideTyping(true);
	fMqttPasswordControl->SetModificationMessage(new BMessage(kMsgSettingChanged));

	BCheckBox* showPassCheck = new BCheckBox("show_pass", "Show",
		new BMessage(kMsgTogglePasswordVis));

	fMqttStatusLabel = new BStringView("mqtt_status", "Status: Not connected");
	fMqttStatusLabel->SetHighColor(kStatusOffline);

	BStringView* mqttInfo = new BStringView("mqtt_info",
		"Data published to nodi.meshcoreitalia.it");
	mqttInfo->SetHighColor(kStatusOffline);

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fMqttEnableCheck)
		.Add(mqttInfo)
		.AddStrut(B_USE_HALF_ITEM_SPACING)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fMqttIataControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fMqttIataControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fMqttBrokerControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fMqttBrokerControl->CreateTextViewLayoutItem(), 1, 1)
			.Add(fMqttPortControl->CreateLabelLayoutItem(), 0, 2)
			.Add(fMqttPortControl->CreateTextViewLayoutItem(), 1, 2)
			.Add(fMqttUsernameControl->CreateLabelLayoutItem(), 0, 3)
			.Add(fMqttUsernameControl->CreateTextViewLayoutItem(), 1, 3)
			.Add(fMqttPasswordControl->CreateLabelLayoutItem(), 0, 4)
			.Add(fMqttPasswordControl->CreateTextViewLayoutItem(), 1, 4)
		.End()
		.AddGroup(B_HORIZONTAL, 0)
			.AddGlue()
			.Add(showPassCheck)
		.End()
		.AddStrut(B_USE_HALF_ITEM_SPACING)
		.Add(fMqttStatusLabel)
		.AddGlue()
	.End();

	// Initial state: disabled
	_OnMqttEnableChanged();
}


void
SettingsWindow::_BuildAboutTab(BView* parent)
{
	BStringView* nameLabel = new BStringView("app_name", APP_NAME);
	nameLabel->SetFont(be_bold_font);

	BString versionStr("Version ");
	versionStr << APP_VERSION;
	BStringView* versionLabel = new BStringView("version", versionStr.String());

	BStringView* descLabel = new BStringView("description",
		"A native MeshCore LoRa mesh client for Haiku OS.\n\n"
		"The name recalls the Venetian 'sestieri' -\n"
		"interconnected districts like nodes in a mesh network.");

	BStringView* copyrightLabel = new BStringView("copyright",
		"Copyright 2025-2026 Sestriere Authors");

	BStringView* licenseLabel = new BStringView("license",
		"Distributed under the MIT license.");

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGroup(B_VERTICAL, 0)
			.Add(nameLabel)
			.Add(versionLabel)
		.End()
		.AddStrut(B_USE_BIG_SPACING)
		.Add(descLabel)
		.AddStrut(B_USE_DEFAULT_SPACING)
		.Add(copyrightLabel)
		.Add(licenseLabel)
		.AddGlue()
	.End();
}


void
SettingsWindow::_OnApply()
{
	if (fParent == NULL) {
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	// Send device name change if non-empty
	const char* newName = fNodeNameControl->Text();
	if (newName != NULL && newName[0] != '\0') {
		BMessage nameMsg(MSG_SET_NAME);
		nameMsg.AddString("name", newName);
		fParent->PostMessage(&nameMsg);
	}

	// Send battery type
	if (fBatteryTypeMenu != NULL) {
		BMenu* battMenu = fBatteryTypeMenu->Menu();
		if (battMenu != NULL) {
			int32 battType = battMenu->IndexOf(battMenu->FindMarked());
			if (battType >= 0) {
				BMessage battMsg(MSG_BATTERY_TYPE_CHANGED);
				battMsg.AddInt32("type", battType);
				fParent->PostMessage(&battMsg);
			}
		}
	}

	// Send lat/lon update
	double lat = atof(fLatitudeControl->Text());
	double lon = atof(fLongitudeControl->Text());
	if (lat != 0.0 || lon != 0.0) {
		BMessage latlonMsg(MSG_APPLY_SETTINGS);
		latlonMsg.AddDouble("latitude", lat);
		latlonMsg.AddDouble("longitude", lon);
		fParent->PostMessage(&latlonMsg);
	}

	// Send radio parameters
	float freqMHz = atof(fFrequencyControl->Text());
	float bwKHz = atof(fBandwidthControl->Text());
	int sf = atoi(fSpreadingFactorControl->Text());
	int cr = atoi(fCodingRateControl->Text());

	if (freqMHz > 0 && bwKHz > 0 && sf > 0 && cr > 0) {
		BMessage radioMsg(MSG_APPLY_SETTINGS);
		radioMsg.AddUInt32("frequency", (uint32)(freqMHz * 1000000.0f));
		radioMsg.AddUInt32("bandwidth", (uint32)(bwKHz * 1000.0f));
		radioMsg.AddUInt8("sf", (uint8)sf);
		radioMsg.AddUInt8("cr", (uint8)cr);
		fParent->PostMessage(&radioMsg);
	}

	// Send TX power
	int32 txPower = fTxPowerSlider->Value();
	if (txPower > 0) {
		BMessage powerMsg(MSG_APPLY_SETTINGS);
		powerMsg.AddUInt8("txpower", (uint8)txPower);
		fParent->PostMessage(&powerMsg);
	}

	// Send MQTT settings
	if (fMqttEnableCheck != NULL) {
		fMqttSettings.enabled = (fMqttEnableCheck->Value() == B_CONTROL_ON);
		strlcpy(fMqttSettings.iataCode, fMqttIataControl->Text(),
			sizeof(fMqttSettings.iataCode));
		strlcpy(fMqttSettings.broker, fMqttBrokerControl->Text(),
			sizeof(fMqttSettings.broker));
		fMqttSettings.port = atoi(fMqttPortControl->Text());
		strlcpy(fMqttSettings.username, fMqttUsernameControl->Text(),
			sizeof(fMqttSettings.username));
		strlcpy(fMqttSettings.password, fMqttPasswordControl->Text(),
			sizeof(fMqttSettings.password));

		// Uppercase IATA code
		for (int i = 0; fMqttSettings.iataCode[i]; i++) {
			if (fMqttSettings.iataCode[i] >= 'a'
				&& fMqttSettings.iataCode[i] <= 'z')
				fMqttSettings.iataCode[i] -= 32;
		}

		BMessage mqttMsg(MSG_MQTT_SETTINGS_CHANGED);
		mqttMsg.AddBool("enabled", fMqttSettings.enabled);
		mqttMsg.AddDouble("latitude", fMqttSettings.latitude);
		mqttMsg.AddDouble("longitude", fMqttSettings.longitude);
		mqttMsg.AddString("iata", fMqttSettings.iataCode);
		mqttMsg.AddString("broker", fMqttSettings.broker);
		mqttMsg.AddInt32("port", fMqttSettings.port);
		mqttMsg.AddString("username", fMqttSettings.username);
		mqttMsg.AddString("password", fMqttSettings.password);
		fParent->PostMessage(&mqttMsg);
	}

	fSettingsChanged = false;
	fApplyButton->SetEnabled(false);
	fRevertButton->SetEnabled(false);

	PostMessage(B_QUIT_REQUESTED);
}


void
SettingsWindow::_OnPresetSelected(int32 preset)
{
	if (preset < 0 || preset >= PRESET_COUNT)
		return;

	fSelectedPreset = preset;

	// Update menu selection
	BMenu* menu = fPresetMenu->Menu();
	if (menu != NULL) {
		BMenuItem* item = menu->ItemAt(preset);
		if (item != NULL)
			item->SetMarked(true);
	}

	// Fill in preset values (skip Custom)
	if (preset != PRESET_CUSTOM) {
		const RadioPresetInfo& info = kRadioPresets[preset];

		char buf[32];
		snprintf(buf, sizeof(buf), "%.3f", info.frequency / 1000000.0);
		fFrequencyControl->SetText(buf);

		snprintf(buf, sizeof(buf), "%.1f", info.bandwidth / 1000.0);
		fBandwidthControl->SetText(buf);

		snprintf(buf, sizeof(buf), "%d", info.spreadingFactor);
		fSpreadingFactorControl->SetText(buf);

		snprintf(buf, sizeof(buf), "%d", info.codingRate);
		fCodingRateControl->SetText(buf);

		fSettingsChanged = true;
		fApplyButton->SetEnabled(true);
		fRevertButton->SetEnabled(true);
	}
}


void
SettingsWindow::SetMqttSettings(const MqttSettings& settings)
{
	fMqttSettings = settings;

	if (fMqttEnableCheck == NULL)
		return;

	fMqttEnableCheck->SetValue(settings.enabled ? B_CONTROL_ON : B_CONTROL_OFF);
	fMqttIataControl->SetText(settings.iataCode);
	fMqttBrokerControl->SetText(settings.broker);

	char buf[16];
	snprintf(buf, sizeof(buf), "%d", settings.port);
	fMqttPortControl->SetText(buf);

	fMqttUsernameControl->SetText(settings.username);
	fMqttPasswordControl->SetText(settings.password);

	_OnMqttEnableChanged();
}


void
SettingsWindow::_OnMqttEnableChanged()
{
	if (fMqttEnableCheck == NULL)
		return;

	bool enabled = (fMqttEnableCheck->Value() == B_CONTROL_ON);
	fMqttIataControl->SetEnabled(enabled);
	fMqttBrokerControl->SetEnabled(enabled);
	fMqttPortControl->SetEnabled(enabled);
	fMqttUsernameControl->SetEnabled(enabled);
	fMqttPasswordControl->SetEnabled(enabled);
}
