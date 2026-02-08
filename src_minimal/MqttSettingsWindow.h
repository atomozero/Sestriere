/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttSettingsWindow.h — MQTT and GPS settings dialog
 */

#ifndef MQTTSETTINGSWINDOW_H
#define MQTTSETTINGSWINDOW_H

#include <Window.h>

#include "MqttClient.h"

class BButton;
class BCheckBox;
class BStringView;
class BTextControl;

class MqttSettingsWindow : public BWindow {
public:
							MqttSettingsWindow(BWindow* parent,
								const MqttSettings& settings);
	virtual					~MqttSettingsWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

private:
			void			_Apply();
			void			_LoadFromSettings();

			BWindow*		fParent;
			MqttSettings	fSettings;

			// GPS
			BTextControl*	fLatitudeControl;
			BTextControl*	fLongitudeControl;

			// MQTT
			BCheckBox*		fEnableCheck;
			BTextControl*	fIataControl;
			BTextControl*	fBrokerControl;
			BTextControl*	fPortControl;
			BTextControl*	fUsernameControl;
			BTextControl*	fPasswordControl;

			// Status
			BStringView*	fStatusLabel;

			// Buttons
			BButton*		fApplyButton;
			BButton*		fCancelButton;
};

// Message sent to parent when settings are applied
static const uint32 MSG_MQTT_SETTINGS_CHANGED = 'mqsc';

#endif // MQTTSETTINGSWINDOW_H
