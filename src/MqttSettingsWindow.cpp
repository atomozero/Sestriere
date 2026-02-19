/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttSettingsWindow.cpp — MQTT and GPS settings dialog
 */

#include "MqttSettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdio>
#include <cstdlib>


static const uint32 MSG_APPLY = 'aply';
static const uint32 MSG_CANCEL = 'cncl';
static const uint32 MSG_ENABLE_CHANGED = 'ench';


MqttSettingsWindow::MqttSettingsWindow(BWindow* parent,
	const MqttSettings& settings)
	:
	BWindow(BRect(0, 0, 380, 350), "MQTT Settings",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
		| B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fSettings(settings)
{
	// GPS controls
	fLatitudeControl = new BTextControl("lat", "Latitude:", "", NULL);
	fLatitudeControl->SetModificationMessage(new BMessage('chng'));

	fLongitudeControl = new BTextControl("lon", "Longitude:", "", NULL);
	fLongitudeControl->SetModificationMessage(new BMessage('chng'));

	// MQTT controls
	fEnableCheck = new BCheckBox("enable", "Enable MQTT publishing",
		new BMessage(MSG_ENABLE_CHANGED));

	fIataControl = new BTextControl("iata", "IATA Code:", "",
		new BMessage('chng'));
	fIataControl->SetToolTip("Location code (e.g., VCE for Venice, FCO for Rome)");

	fBrokerControl = new BTextControl("broker", "Broker:", "", NULL);
	fPortControl = new BTextControl("port", "Port:", "", NULL);
	fUsernameControl = new BTextControl("user", "Username:", "", NULL);
	fPasswordControl = new BTextControl("pass", "Password:", "", NULL);

	// Status
	fStatusLabel = new BStringView("status", "Status: Not connected");
	fStatusLabel->SetHighColor(128, 128, 128);

	// Buttons
	fApplyButton = new BButton("apply", "Apply", new BMessage(MSG_APPLY));
	fCancelButton = new BButton("cancel", "Cancel", new BMessage(MSG_CANCEL));

	// Info labels
	BStringView* gpsInfo = new BStringView("gps_info",
		"Enter your GPS coordinates for map positioning");
	gpsInfo->SetHighColor(100, 100, 100);

	BStringView* mqttInfo = new BStringView("mqtt_info",
		"Data will be published to nodi.meshcoreitalia.it");
	mqttInfo->SetHighColor(100, 100, 100);

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)

		// GPS Section
		.AddGroup(B_VERTICAL, B_USE_HALF_ITEM_SPACING)
			.Add(new BStringView("gps_title", "GPS Position"))
			.Add(gpsInfo)
			.AddGrid(B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
				.Add(fLatitudeControl->CreateLabelLayoutItem(), 0, 0)
				.Add(fLatitudeControl->CreateTextViewLayoutItem(), 1, 0)
				.Add(fLongitudeControl->CreateLabelLayoutItem(), 0, 1)
				.Add(fLongitudeControl->CreateTextViewLayoutItem(), 1, 1)
			.End()
		.End()

		.Add(new BSeparatorView(B_HORIZONTAL))

		// MQTT Section
		.AddGroup(B_VERTICAL, B_USE_HALF_ITEM_SPACING)
			.Add(new BStringView("mqtt_title", "MQTT (meshcoreitalia.it)"))
			.Add(mqttInfo)
			.Add(fEnableCheck)
			.AddGrid(B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
				.Add(fIataControl->CreateLabelLayoutItem(), 0, 0)
				.Add(fIataControl->CreateTextViewLayoutItem(), 1, 0)
				.Add(fBrokerControl->CreateLabelLayoutItem(), 0, 1)
				.Add(fBrokerControl->CreateTextViewLayoutItem(), 1, 1)
				.Add(fPortControl->CreateLabelLayoutItem(), 0, 2)
				.Add(fPortControl->CreateTextViewLayoutItem(), 1, 2)
				.Add(fUsernameControl->CreateLabelLayoutItem(), 0, 3)
				.Add(fUsernameControl->CreateTextViewLayoutItem(), 1, 3)
				.Add(fPasswordControl->CreateLabelLayoutItem(), 0, 4)
				.Add(fPasswordControl->CreateTextViewLayoutItem(), 1, 4)
			.End()
			.Add(fStatusLabel)
		.End()

		.Add(new BSeparatorView(B_HORIZONTAL))

		// Buttons
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fApplyButton)
		.End()
	.End();

	// Make title labels bold
	BFont boldFont;
	boldFont.SetFace(B_BOLD_FACE);

	// Load current settings
	_LoadFromSettings();

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	fApplyButton->MakeDefault(true);
}


MqttSettingsWindow::~MqttSettingsWindow()
{
}


void
MqttSettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_APPLY:
			_Apply();
			PostMessage(B_QUIT_REQUESTED);
			break;

		case MSG_CANCEL:
			PostMessage(B_QUIT_REQUESTED);
			break;

		case MSG_ENABLE_CHANGED:
		{
			bool enabled = (fEnableCheck->Value() == B_CONTROL_ON);
			fIataControl->SetEnabled(enabled);
			fBrokerControl->SetEnabled(enabled);
			fPortControl->SetEnabled(enabled);
			fUsernameControl->SetEnabled(enabled);
			fPasswordControl->SetEnabled(enabled);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MqttSettingsWindow::QuitRequested()
{
	return true;
}


void
MqttSettingsWindow::_Apply()
{
	// Read values from controls
	fSettings.latitude = atof(fLatitudeControl->Text());
	fSettings.longitude = atof(fLongitudeControl->Text());
	fSettings.enabled = (fEnableCheck->Value() == B_CONTROL_ON);

	strlcpy(fSettings.iataCode, fIataControl->Text(), sizeof(fSettings.iataCode));
	strlcpy(fSettings.broker, fBrokerControl->Text(), sizeof(fSettings.broker));
	fSettings.port = atoi(fPortControl->Text());
	strlcpy(fSettings.username, fUsernameControl->Text(), sizeof(fSettings.username));
	strlcpy(fSettings.password, fPasswordControl->Text(), sizeof(fSettings.password));

	// Validate IATA code (uppercase, 3 chars)
	for (int i = 0; fSettings.iataCode[i]; i++) {
		if (fSettings.iataCode[i] >= 'a' && fSettings.iataCode[i] <= 'z')
			fSettings.iataCode[i] -= 32;  // To uppercase
	}

	// Send to parent
	if (fParent != NULL) {
		BMessage msg(MSG_MQTT_SETTINGS_CHANGED);
		msg.AddBool("enabled", fSettings.enabled);
		msg.AddDouble("latitude", fSettings.latitude);
		msg.AddDouble("longitude", fSettings.longitude);
		msg.AddString("iata", fSettings.iataCode);
		msg.AddString("broker", fSettings.broker);
		msg.AddInt32("port", fSettings.port);
		msg.AddString("username", fSettings.username);
		msg.AddString("password", fSettings.password);
		fParent->PostMessage(&msg);
	}
}


void
MqttSettingsWindow::_LoadFromSettings()
{
	char buf[32];

	// GPS
	snprintf(buf, sizeof(buf), "%.6f", fSettings.latitude);
	fLatitudeControl->SetText(buf);

	snprintf(buf, sizeof(buf), "%.6f", fSettings.longitude);
	fLongitudeControl->SetText(buf);

	// MQTT
	fEnableCheck->SetValue(fSettings.enabled ? B_CONTROL_ON : B_CONTROL_OFF);
	fIataControl->SetText(fSettings.iataCode);
	fBrokerControl->SetText(fSettings.broker);

	snprintf(buf, sizeof(buf), "%d", fSettings.port);
	fPortControl->SetText(buf);

	fUsernameControl->SetText(fSettings.username);
	fPasswordControl->SetText(fSettings.password);

	// Trigger enable/disable
	bool enabled = fSettings.enabled;
	fIataControl->SetEnabled(enabled);
	fBrokerControl->SetEnabled(enabled);
	fPortControl->SetEnabled(enabled);
	fUsernameControl->SetEnabled(enabled);
	fPasswordControl->SetEnabled(enabled);
}
