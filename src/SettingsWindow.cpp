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
#include <Clipboard.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <Slider.h>
#include <StringItem.h>
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
static const uint32 kMsgApplyTuning = 'atun';
static const uint32 kMsgSetDevicePin = 'sdpn';
static const uint32 kMsgChannelSelected = 'chsl';
static const uint32 kMsgCopyChannelPsk = 'cpsk';
static const uint32 kMsgChannelAdd = 'chad';
static const uint32 kMsgChannelRemove = 'chrm';
static const uint32 kMsgAutoAddChanged = 'aach';
static const uint32 kMsgPathHashSelected = 'phsl';
static const uint32 kMsgVarSelected = 'vrsl';
static const uint32 kMsgVarSet = 'vrst';
static const uint32 kMsgVarRefresh = 'vrrf';


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
	fRxDelayBaseControl(NULL),
	fAirtimeFactorControl(NULL),
	fDevicePinControl(NULL),
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
	fChannelListView(NULL),
	fChannelSlotLabel(NULL),
	fChannelPskField(NULL),
	fChannelAddButton(NULL),
	fChannelRemoveButton(NULL),
	fChannelCopyPskButton(NULL),
	fChannelEntryCount(0),
	fMaxChannels(0),
	fApplyButton(NULL),
	fRevertButton(NULL),
	fSettingsChanged(false),
	fSelectedPreset(PRESET_CUSTOM)
{
	memset(fChannelEntries, 0, sizeof(fChannelEntries));
	fPathHashMenu = NULL;
	fAutoAddChat = NULL;
	fAutoAddRepeater = NULL;
	fAutoAddRoom = NULL;
	fAutoAddSensor = NULL;
	fAutoAddOverwrite = NULL;
	fVarListView = NULL;
	fVarNameControl = NULL;
	fVarValueControl = NULL;
	fVarSetButton = NULL;
	fVarRefreshButton = NULL;

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

	// Channels tab
	BView* channelsTab = new BView("channels_tab", 0);
	channelsTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildChannelsTab(channelsTab);
	tabView->AddTab(channelsTab, new BTab());
	tabView->TabAt(3)->SetLabel("Channels");

	// Variables tab
	BView* varsTab = new BView("vars_tab", 0);
	varsTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildVariablesTab(varsTab);
	tabView->AddTab(varsTab, new BTab());
	tabView->TabAt(4)->SetLabel("Variables");

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

		case kMsgApplyTuning:
		{
			if (fParent == NULL)
				break;
			uint32 rxDelay = (uint32)atoi(fRxDelayBaseControl->Text());
			uint32 airtimeFactor = (uint32)atoi(
				fAirtimeFactorControl->Text());
			BMessage tuningMsg(MSG_SET_TUNING_PARAMS);
			tuningMsg.AddUInt32("rx_delay_base", rxDelay);
			tuningMsg.AddUInt32("airtime_factor", airtimeFactor);
			fParent->PostMessage(&tuningMsg);
			break;
		}

		case kMsgSetDevicePin:
		{
			if (fParent == NULL)
				break;
			uint32 pin = (uint32)atoi(fDevicePinControl->Text());
			BMessage pinMsg(MSG_SET_DEVICE_PIN);
			pinMsg.AddUInt32("pin", pin);
			fParent->PostMessage(&pinMsg);
			break;
		}

		case kMsgChannelSelected:
			_OnChannelSelected();
			break;

		case kMsgCopyChannelPsk:
			_OnCopyChannelPsk();
			break;

		case kMsgChannelAdd:
		{
			if (fParent == NULL)
				break;
			BMessage addMsg(MSG_ADD_CHANNEL);
			fParent->PostMessage(&addMsg);
			break;
		}

		case kMsgPathHashSelected:
		{
			if (fParent == NULL)
				break;
			uint8 mode;
			if (message->FindUInt8("mode", &mode) == B_OK) {
				BMessage setMsg(MSG_SET_PATH_HASH_MODE);
				setMsg.AddUInt8("mode", mode);
				fParent->PostMessage(&setMsg);
			}
			break;
		}

		case kMsgAutoAddChanged:
		{
			if (fParent == NULL)
				break;
			uint8 flags = 0;
			if (fAutoAddChat != NULL
				&& fAutoAddChat->Value() == B_CONTROL_ON)
				flags |= AUTO_ADD_CHAT;
			if (fAutoAddRepeater != NULL
				&& fAutoAddRepeater->Value() == B_CONTROL_ON)
				flags |= AUTO_ADD_REPEATER;
			if (fAutoAddRoom != NULL
				&& fAutoAddRoom->Value() == B_CONTROL_ON)
				flags |= AUTO_ADD_ROOM;
			if (fAutoAddSensor != NULL
				&& fAutoAddSensor->Value() == B_CONTROL_ON)
				flags |= AUTO_ADD_SENSOR;
			if (fAutoAddOverwrite != NULL
				&& fAutoAddOverwrite->Value() == B_CONTROL_ON)
				flags |= AUTO_ADD_OVERWRITE_OLDEST;
			BMessage setMsg(MSG_SET_AUTO_ADD_CONFIG);
			setMsg.AddUInt8("flags", flags);
			fParent->PostMessage(&setMsg);
			break;
		}

		case kMsgVarSelected:
		{
			if (fVarListView == NULL)
				break;
			int32 sel = fVarListView->CurrentSelection();
			if (sel < 0)
				break;
			BStringItem* item = dynamic_cast<BStringItem*>(
				fVarListView->ItemAt(sel));
			if (item == NULL)
				break;
			// Parse "name:value" from the list item text
			const char* text = item->Text();
			const char* colon = strchr(text, ':');
			if (colon != NULL) {
				BString name(text, colon - text);
				BString value(colon + 1);
				if (fVarNameControl != NULL)
					fVarNameControl->SetText(name.String());
				if (fVarValueControl != NULL)
					fVarValueControl->SetText(value.String());
			}
			break;
		}

		case kMsgVarSet:
		{
			if (fParent == NULL || fVarNameControl == NULL
				|| fVarValueControl == NULL)
				break;
			const char* name = fVarNameControl->Text();
			const char* value = fVarValueControl->Text();
			if (name == NULL || name[0] == '\0')
				break;
			BString nameValue;
			nameValue.SetToFormat("%s:%s", name, value);
			BMessage setMsg(MSG_SET_CUSTOM_VAR);
			setMsg.AddString("name_value", nameValue.String());
			fParent->PostMessage(&setMsg);
			break;
		}

		case kMsgVarRefresh:
		{
			if (fParent != NULL)
				fParent->PostMessage(new BMessage(MSG_GET_CUSTOM_VARS));
			break;
		}

		case kMsgChannelRemove:
		{
			if (fParent == NULL || fChannelListView == NULL)
				break;
			int32 sel = fChannelListView->CurrentSelection();
			if (sel < 0 || sel >= fChannelEntryCount)
				break;
			BMessage removeMsg(MSG_REMOVE_CHANNEL);
			removeMsg.AddInt32("channel_idx",
				fChannelEntries[sel].index);
			fParent->PostMessage(&removeMsg);
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

	// Tuning parameters
	fRxDelayBaseControl = new BTextControl("rx_delay", "RX Delay Base:", "",
		new BMessage(kMsgSettingChanged));
	fRxDelayBaseControl->SetModificationMessage(
		new BMessage(kMsgSettingChanged));
	fRxDelayBaseControl->SetToolTip("Base delay for receive window (ms)");

	fAirtimeFactorControl = new BTextControl("airtime_factor",
		"Airtime Factor:", "", new BMessage(kMsgSettingChanged));
	fAirtimeFactorControl->SetModificationMessage(
		new BMessage(kMsgSettingChanged));
	fAirtimeFactorControl->SetToolTip(
		"Multiplier for airtime calculation");

	BButton* applyTuningButton = new BButton("apply_tuning",
		"Apply Tuning", new BMessage(kMsgApplyTuning));

	// Device PIN
	fDevicePinControl = new BTextControl("device_pin", "BLE PIN:", "",
		new BMessage(kMsgSettingChanged));
	fDevicePinControl->SetModificationMessage(
		new BMessage(kMsgSettingChanged));
	fDevicePinControl->SetToolTip("BLE pairing PIN (0 = disabled)");

	BButton* setPinButton = new BButton("set_pin",
		"Set PIN", new BMessage(kMsgSetDevicePin));

	// Path hash mode dropdown
	BPopUpMenu* pathHashPopUp = new BPopUpMenu("path_hash_popup");
	static const char* kPathHashModes[] = {
		"1 byte (default)", "2 bytes", "3 bytes"
	};
	for (int i = 0; i < 3; i++) {
		BMessage* phMsg = new BMessage(kMsgPathHashSelected);
		phMsg->AddUInt8("mode", (uint8)i);
		pathHashPopUp->AddItem(new BMenuItem(kPathHashModes[i], phMsg));
	}
	fPathHashMenu = new BMenuField("path_hash", "Path Hash:",
		pathHashPopUp);
	pathHashPopUp->ItemAt(0)->SetMarked(true);

	// Auto-add contact configuration
	fAutoAddChat = new BCheckBox("aa_chat", "Chat",
		new BMessage(kMsgAutoAddChanged));
	fAutoAddRepeater = new BCheckBox("aa_rep", "Repeater",
		new BMessage(kMsgAutoAddChanged));
	fAutoAddRoom = new BCheckBox("aa_room", "Room",
		new BMessage(kMsgAutoAddChanged));
	fAutoAddSensor = new BCheckBox("aa_sensor", "Sensor",
		new BMessage(kMsgAutoAddChanged));
	fAutoAddOverwrite = new BCheckBox("aa_overwrite",
		"Overwrite oldest", new BMessage(kMsgAutoAddChanged));

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
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fRxDelayBaseControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fRxDelayBaseControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fAirtimeFactorControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fAirtimeFactorControl->CreateTextViewLayoutItem(), 1, 1)
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyTuningButton)
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fDevicePinControl)
			.Add(setPinButton)
		.End()
		.Add(fPathHashMenu)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
			.Add(new BStringView("aa_label", "Auto-Add Contacts:"))
			.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
				.Add(fAutoAddChat)
				.Add(fAutoAddRepeater)
				.Add(fAutoAddRoom)
				.Add(fAutoAddSensor)
			.End()
			.Add(fAutoAddOverwrite)
		.End()
		.AddStrut(B_USE_HALF_ITEM_SPACING)
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


void
SettingsWindow::SetTuningParams(uint32 rxDelayBase, uint32 airtimeFactor)
{
	if (fRxDelayBaseControl != NULL) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%u", (unsigned)rxDelayBase);
		fRxDelayBaseControl->SetText(buf);
	}
	if (fAirtimeFactorControl != NULL) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%u", (unsigned)airtimeFactor);
		fAirtimeFactorControl->SetText(buf);
	}
}


void
SettingsWindow::SetDevicePin(uint32 pin)
{
	if (fDevicePinControl != NULL) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
		fDevicePinControl->SetText(buf);
	}
}


void
SettingsWindow::_BuildChannelsTab(BView* parent)
{
	fChannelSlotLabel = new BStringView("slot_label", "0 / 0 slots used");

	fChannelListView = new BListView("channel_list", B_SINGLE_SELECTION_LIST);
	fChannelListView->SetSelectionMessage(new BMessage(kMsgChannelSelected));

	BScrollView* scrollView = new BScrollView("channel_scroll",
		fChannelListView, 0, false, true);

	fChannelPskField = new BTextControl("channel_psk", "PSK:", "", NULL);
	fChannelPskField->TextView()->MakeEditable(false);
	fChannelPskField->SetEnabled(false);

	fChannelCopyPskButton = new BButton("copy_psk", "Copy PSK",
		new BMessage(kMsgCopyChannelPsk));
	fChannelCopyPskButton->SetEnabled(false);

	fChannelAddButton = new BButton("add_channel", "Add Channel",
		new BMessage(kMsgChannelAdd));

	fChannelRemoveButton = new BButton("remove_channel", "Remove Channel",
		new BMessage(kMsgChannelRemove));
	fChannelRemoveButton->SetEnabled(false);

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fChannelSlotLabel)
		.Add(scrollView, 1.0)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fChannelPskField->CreateLabelLayoutItem(), 0, 0)
			.Add(fChannelPskField->CreateTextViewLayoutItem(), 1, 0)
			.Add(fChannelCopyPskButton, 2, 0)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fChannelAddButton)
			.Add(fChannelRemoveButton)
			.AddGlue()
		.End()
	.End();
}


void
SettingsWindow::SetChannels(const OwningObjectList<ChannelInfo>& channels,
	uint8 maxChannels)
{
	fMaxChannels = maxChannels;
	fChannelEntryCount = 0;

	for (int32 i = 0; i < channels.CountItems() && fChannelEntryCount < 16;
			i++) {
		ChannelInfo* ch = channels.ItemAt(i);
		if (ch == NULL || ch->IsEmpty())
			continue;
		SettingsChannelEntry& entry = fChannelEntries[fChannelEntryCount];
		entry.index = ch->index;
		strlcpy(entry.name, ch->name, sizeof(entry.name));
		memcpy(entry.secret, ch->secret, 16);
		fChannelEntryCount++;
	}

	// Update list view
	if (fChannelListView != NULL) {
		fChannelListView->MakeEmpty();
		for (int32 i = 0; i < fChannelEntryCount; i++) {
			BString label;
			label.SetToFormat("Slot %d: %s",
				fChannelEntries[i].index, fChannelEntries[i].name);
			fChannelListView->AddItem(new BStringItem(label.String()));
		}
	}

	// Update slot counter
	if (fChannelSlotLabel != NULL) {
		BString slotText;
		slotText.SetToFormat("%d / %d slots used",
			(int)fChannelEntryCount, (int)fMaxChannels);
		fChannelSlotLabel->SetText(slotText.String());
	}

	// Clear PSK display and disable buttons
	if (fChannelPskField != NULL)
		fChannelPskField->SetText("");
	if (fChannelCopyPskButton != NULL)
		fChannelCopyPskButton->SetEnabled(false);
	if (fChannelRemoveButton != NULL)
		fChannelRemoveButton->SetEnabled(false);
}


void
SettingsWindow::_OnChannelSelected()
{
	if (fChannelListView == NULL)
		return;

	int32 sel = fChannelListView->CurrentSelection();
	if (sel < 0 || sel >= fChannelEntryCount) {
		fChannelPskField->SetText("");
		fChannelCopyPskButton->SetEnabled(false);
		fChannelRemoveButton->SetEnabled(false);
		return;
	}

	// Format PSK as space-separated hex
	const uint8* secret = fChannelEntries[sel].secret;
	char hex[64];
	int pos = 0;
	for (int i = 0; i < 16; i++) {
		if (i > 0)
			hex[pos++] = ' ';
		snprintf(hex + pos, sizeof(hex) - pos, "%02X", secret[i]);
		pos += 2;
	}
	hex[pos] = '\0';

	fChannelPskField->SetText(hex);
	fChannelCopyPskButton->SetEnabled(true);
	fChannelRemoveButton->SetEnabled(true);
}


void
SettingsWindow::_OnCopyChannelPsk()
{
	if (fChannelPskField == NULL)
		return;

	const char* text = fChannelPskField->Text();
	if (text == NULL || text[0] == '\0')
		return;

	if (be_clipboard->Lock()) {
		be_clipboard->Clear();
		BMessage* clip = be_clipboard->Data();
		if (clip != NULL) {
			clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
			be_clipboard->Commit();
		}
		be_clipboard->Unlock();
	}
}


void
SettingsWindow::_BuildVariablesTab(BView* parent)
{
	fVarListView = new BListView("var_list", B_SINGLE_SELECTION_LIST);
	fVarListView->SetSelectionMessage(new BMessage(kMsgVarSelected));

	BScrollView* scroll = new BScrollView("var_scroll", fVarListView,
		B_WILL_DRAW | B_FRAME_EVENTS, false, true, B_PLAIN_BORDER);

	fVarNameControl = new BTextControl("var_name", "Name:", "", NULL);
	fVarValueControl = new BTextControl("var_value", "Value:", "", NULL);

	fVarSetButton = new BButton("var_set", "Set",
		new BMessage(kMsgVarSet));
	fVarRefreshButton = new BButton("var_refresh", "Refresh",
		new BMessage(kMsgVarRefresh));

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(scroll, 3.0)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fVarNameControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fVarNameControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fVarValueControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fVarValueControl->CreateTextViewLayoutItem(), 1, 1)
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fVarRefreshButton)
			.Add(fVarSetButton)
		.End()
	.End();
}


void
SettingsWindow::SetCustomVars(const char* varsText)
{
	if (fVarListView == NULL)
		return;

	// Clear existing items
	while (fVarListView->CountItems() > 0) {
		BListItem* item = fVarListView->RemoveItem(
			fVarListView->CountItems() - 1);
		delete item;
	}

	if (varsText == NULL || varsText[0] == '\0')
		return;

	// Parse comma-separated "name:value" pairs
	BString text(varsText);
	int32 start = 0;
	int32 comma;
	while (start < text.Length()) {
		comma = text.FindFirst(',', start);
		if (comma < 0)
			comma = text.Length();
		BString pair;
		text.CopyInto(pair, start, comma - start);
		pair.Trim();
		if (pair.Length() > 0)
			fVarListView->AddItem(new BStringItem(pair.String()));
		start = comma + 1;
	}
}


void
SettingsWindow::SetAutoAddConfig(uint8 flags)
{
	if (fAutoAddChat != NULL)
		fAutoAddChat->SetValue(
			(flags & AUTO_ADD_CHAT) != 0 ? B_CONTROL_ON : B_CONTROL_OFF);
	if (fAutoAddRepeater != NULL)
		fAutoAddRepeater->SetValue(
			(flags & AUTO_ADD_REPEATER) != 0
				? B_CONTROL_ON : B_CONTROL_OFF);
	if (fAutoAddRoom != NULL)
		fAutoAddRoom->SetValue(
			(flags & AUTO_ADD_ROOM) != 0 ? B_CONTROL_ON : B_CONTROL_OFF);
	if (fAutoAddSensor != NULL)
		fAutoAddSensor->SetValue(
			(flags & AUTO_ADD_SENSOR) != 0
				? B_CONTROL_ON : B_CONTROL_OFF);
	if (fAutoAddOverwrite != NULL)
		fAutoAddOverwrite->SetValue(
			(flags & AUTO_ADD_OVERWRITE_OLDEST) != 0
				? B_CONTROL_ON : B_CONTROL_OFF);
}
