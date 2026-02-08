/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SettingsWindow.cpp — Application settings dialog implementation
 */

#include "SettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Slider.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "Protocol.h"
#include "Sestriere.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsWindow"


static const uint32 MSG_SETTING_CHANGED		= 'stch';
static const uint32 MSG_APPLY_SETTINGS		= 'apst';
static const uint32 MSG_REVERT_SETTINGS		= 'rvst';
static const uint32 MSG_PRESET_SELECTED		= 'prsl';


SettingsWindow::SettingsWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 400, 380), B_TRANSLATE(TR_TITLE_SETTINGS),
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fNodeNameControl(NULL),
	fLatitudeControl(NULL),
	fLongitudeControl(NULL),
	fPresetMenu(NULL),
	fTxPowerSlider(NULL),
	fFrequencyControl(NULL),
	fBandwidthControl(NULL),
	fSpreadingFactorControl(NULL),
	fCodingRateControl(NULL),
	fAutoSyncCheck(NULL),
	fNotificationsCheck(NULL),
	fApplyButton(NULL),
	fRevertButton(NULL),
	fSettingsChanged(false),
	fSelectedPreset(PRESET_CUSTOM)
{
	memset(&fCurrentSettings, 0, sizeof(fCurrentSettings));

	// Create tab view
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

	// About tab
	BView* aboutTab = new BView("about_tab", 0);
	aboutTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildAboutTab(aboutTab);
	tabView->AddTab(aboutTab, new BTab());
	tabView->TabAt(2)->SetLabel("About");

	// Buttons
	fApplyButton = new BButton("apply_button", B_TRANSLATE(TR_BUTTON_OK),
		new BMessage(MSG_APPLY_SETTINGS));
	fApplyButton->SetEnabled(false);

	fRevertButton = new BButton("revert_button", "Revert",
		new BMessage(MSG_REVERT_SETTINGS));
	fRevertButton->SetEnabled(false);

	BButton* cancelButton = new BButton("cancel_button",
		B_TRANSLATE(TR_BUTTON_CANCEL), new BMessage(B_QUIT_REQUESTED));

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

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	_LoadSettings();
}


SettingsWindow::~SettingsWindow()
{
}


void
SettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_SETTING_CHANGED:
			fSettingsChanged = true;
			fApplyButton->SetEnabled(true);
			fRevertButton->SetEnabled(true);
			break;

		case MSG_APPLY_SETTINGS:
			_OnApply();
			break;

		case MSG_REVERT_SETTINGS:
			_OnRevert();
			break;

		case MSG_PRESET_SELECTED:
		{
			int32 preset;
			if (message->FindInt32("preset", &preset) == B_OK)
				_OnPresetSelected(preset);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
SettingsWindow::SetCurrentSettings(const SelfInfo& selfInfo)
{
	memcpy(&fCurrentSettings, &selfInfo, sizeof(SelfInfo));

	if (fNodeNameControl != NULL)
		fNodeNameControl->SetText(selfInfo.name);

	if (fLatitudeControl != NULL) {
		char lat[32];
		snprintf(lat, sizeof(lat), "%.6f",
			Protocol::LatLonFromInt(selfInfo.advLat));
		fLatitudeControl->SetText(lat);
	}

	if (fLongitudeControl != NULL) {
		char lon[32];
		snprintf(lon, sizeof(lon), "%.6f",
			Protocol::LatLonFromInt(selfInfo.advLon));
		fLongitudeControl->SetText(lon);
	}

	if (fTxPowerSlider != NULL)
		fTxPowerSlider->SetValue(selfInfo.txPowerDbm);

	if (fFrequencyControl != NULL) {
		char freq[32];
		snprintf(freq, sizeof(freq), "%.3f", selfInfo.radioFreq / 1000000.0f);
		fFrequencyControl->SetText(freq);
	}

	if (fBandwidthControl != NULL) {
		char bw[32];
		snprintf(bw, sizeof(bw), "%.1f", selfInfo.radioBw / 1000.0f);
		fBandwidthControl->SetText(bw);
	}

	if (fSpreadingFactorControl != NULL) {
		char sf[8];
		snprintf(sf, sizeof(sf), "%d", selfInfo.radioSf);
		fSpreadingFactorControl->SetText(sf);
	}

	if (fCodingRateControl != NULL) {
		char cr[8];
		snprintf(cr, sizeof(cr), "%d", selfInfo.radioCr);
		fCodingRateControl->SetText(cr);
	}

	fSettingsChanged = false;
	fApplyButton->SetEnabled(false);
	fRevertButton->SetEnabled(false);
}


void
SettingsWindow::_BuildDeviceTab(BView* parent)
{
	fNodeNameControl = new BTextControl("node_name", "Node Name:", "",
		new BMessage(MSG_SETTING_CHANGED));
	fNodeNameControl->SetModificationMessage(new BMessage(MSG_SETTING_CHANGED));

	fLatitudeControl = new BTextControl("latitude", "Latitude:", "",
		new BMessage(MSG_SETTING_CHANGED));
	fLatitudeControl->SetModificationMessage(new BMessage(MSG_SETTING_CHANGED));

	fLongitudeControl = new BTextControl("longitude", "Longitude:", "",
		new BMessage(MSG_SETTING_CHANGED));
	fLongitudeControl->SetModificationMessage(new BMessage(MSG_SETTING_CHANGED));

	fAutoSyncCheck = new BCheckBox("auto_sync", "Auto-sync messages",
		new BMessage(MSG_SETTING_CHANGED));
	fAutoSyncCheck->SetValue(B_CONTROL_ON);

	fNotificationsCheck = new BCheckBox("notifications",
		"Desktop notifications", new BMessage(MSG_SETTING_CHANGED));
	fNotificationsCheck->SetValue(B_CONTROL_ON);

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
		.AddStrut(B_USE_DEFAULT_SPACING)
		.Add(fAutoSyncCheck)
		.Add(fNotificationsCheck)
		.AddGlue()
	.End();
}


void
SettingsWindow::_BuildRadioTab(BView* parent)
{
	// Preset menu
	BPopUpMenu* presetPopUp = new BPopUpMenu("preset_popup");
	for (int i = 0; i < PRESET_COUNT; i++) {
		BMessage* msg = new BMessage(MSG_PRESET_SELECTED);
		msg->AddInt32("preset", i);
		BMenuItem* item = new BMenuItem(kRadioPresets[i].name, msg);
		presetPopUp->AddItem(item);
	}
	fPresetMenu = new BMenuField("preset_menu", "Preset:", presetPopUp);

	fTxPowerSlider = new BSlider("tx_power", "TX Power (dBm)",
		new BMessage(MSG_SETTING_CHANGED), 0, 22, B_HORIZONTAL);
	fTxPowerSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fTxPowerSlider->SetHashMarkCount(12);
	fTxPowerSlider->SetLimitLabels("0", "22");
	fTxPowerSlider->SetModificationMessage(new BMessage(MSG_SETTING_CHANGED));

	fFrequencyControl = new BTextControl("frequency", "Frequency (MHz):", "",
		new BMessage(MSG_SETTING_CHANGED));
	fFrequencyControl->SetModificationMessage(new BMessage(MSG_SETTING_CHANGED));

	fBandwidthControl = new BTextControl("bandwidth", "Bandwidth (kHz):", "",
		new BMessage(MSG_SETTING_CHANGED));
	fBandwidthControl->SetModificationMessage(new BMessage(MSG_SETTING_CHANGED));

	fSpreadingFactorControl = new BTextControl("sf", "Spreading Factor:", "",
		new BMessage(MSG_SETTING_CHANGED));
	fSpreadingFactorControl->SetModificationMessage(
		new BMessage(MSG_SETTING_CHANGED));

	fCodingRateControl = new BTextControl("cr", "Coding Rate:", "",
		new BMessage(MSG_SETTING_CHANGED));
	fCodingRateControl->SetModificationMessage(
		new BMessage(MSG_SETTING_CHANGED));

	BStringView* warningLabel = new BStringView("warning",
		"Warning: Changing radio parameters may break\n"
		"communication with other nodes on different settings.");
	warningLabel->SetHighColor(200, 0, 0);

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
SettingsWindow::_BuildAboutTab(BView* parent)
{
	BStringView* nameLabel = new BStringView("app_name", kAppName);
	nameLabel->SetFont(be_bold_font);

	BStringView* versionLabel = new BStringView("version", "Version 0.1.0");

	BStringView* descLabel = new BStringView("description",
		"A native MeshCore LoRa mesh client for Haiku OS.\n\n"
		"The name recalls the Venetian 'sestieri' -\n"
		"interconnected districts like nodes in a mesh network.");

	BStringView* copyrightLabel = new BStringView("copyright",
		"Copyright 2025 Sestriere Authors");

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
	_SaveSettings();

	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app == NULL || app->GetSerialHandler() == NULL ||
		!app->GetSerialHandler()->IsConnected()) {
		// Not connected, just save local settings
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	// Apply settings to device
	uint8 buffer[64];

	// Update node name if changed
	const char* newName = fNodeNameControl->Text();
	if (strcmp(newName, fCurrentSettings.name) != 0) {
		size_t len = Protocol::BuildSetAdvertName(newName, buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);
	}

	// Update TX power if changed
	int32 newPower = fTxPowerSlider->Value();
	if ((uint8)newPower != fCurrentSettings.txPowerDbm) {
		size_t len = Protocol::BuildSetTxPower((uint8)newPower, buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);
	}

	// Update lat/lon if changed
	float newLat = atof(fLatitudeControl->Text());
	float newLon = atof(fLongitudeControl->Text());
	int32 newLatInt = Protocol::LatLonToInt(newLat);
	int32 newLonInt = Protocol::LatLonToInt(newLon);
	if (newLatInt != fCurrentSettings.advLat ||
		newLonInt != fCurrentSettings.advLon) {
		size_t len = Protocol::BuildSetAdvertLatLon(newLatInt, newLonInt, buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);
	}

	// Update radio parameters if changed
	float newFreqMHz = atof(fFrequencyControl->Text());
	float newBwKHz = atof(fBandwidthControl->Text());
	int newSf = atoi(fSpreadingFactorControl->Text());
	int newCr = atoi(fCodingRateControl->Text());

	uint32 newFreqHz = (uint32)(newFreqMHz * 1000000.0f);
	uint32 newBwHz = (uint32)(newBwKHz * 1000.0f);

	if (newFreqHz != fCurrentSettings.radioFreq ||
		newBwHz != fCurrentSettings.radioBw ||
		(uint8)newSf != fCurrentSettings.radioSf ||
		(uint8)newCr != fCurrentSettings.radioCr) {

		RadioParams params;
		params.freq = newFreqHz;
		params.bw = newBwHz;
		params.sf = (uint8)newSf;
		params.cr = (uint8)newCr;

		size_t len = Protocol::BuildSetRadioParams(params, buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);
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

	// If not custom, fill in the preset values
	if (preset != PRESET_CUSTOM) {
		const RadioPresetInfo& info = kRadioPresets[preset];

		char buf[32];
		snprintf(buf, sizeof(buf), "%.3f", info.freq / 1000000.0);
		fFrequencyControl->SetText(buf);

		snprintf(buf, sizeof(buf), "%.1f", info.bw / 1000.0);
		fBandwidthControl->SetText(buf);

		snprintf(buf, sizeof(buf), "%d", info.sf);
		fSpreadingFactorControl->SetText(buf);

		snprintf(buf, sizeof(buf), "%d", info.cr);
		fCodingRateControl->SetText(buf);

		fSettingsChanged = true;
		fApplyButton->SetEnabled(true);
		fRevertButton->SetEnabled(true);
	}
}


void
SettingsWindow::_OnRevert()
{
	SetCurrentSettings(fCurrentSettings);
}


void
SettingsWindow::_LoadSettings()
{
	// Load from current device if connected
	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app != NULL && app->GetSerialHandler() != NULL &&
		app->GetSerialHandler()->IsConnected()) {
		// TODO: Request current settings from device
	}
}


void
SettingsWindow::_SaveSettings()
{
	// Save to settings file
	// TODO: Implement persistent settings
}
