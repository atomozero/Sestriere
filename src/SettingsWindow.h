/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SettingsWindow.h — Device and radio settings dialog
 */

#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <Window.h>

#include "MqttClient.h"
#include "Types.h"

class BButton;
class BCheckBox;
class BMenuField;
class BSlider;
class BStringView;
class BTextControl;

class SettingsWindow : public BWindow {
public:
							SettingsWindow(BWindow* parent);
	virtual					~SettingsWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			SetDeviceName(const char* name);
			void			SetLatitude(double lat);
			void			SetLongitude(double lon);
			void			SetRadioPreset(int32 preset);
			void			SetBatteryType(uint8 type);
			void			SetRadioParams(uint32 freqHz, uint32 bwHz,
								uint8 sf, uint8 cr, uint8 txPower);
			void			SetMqttSettings(const MqttSettings& settings);
			void			SetTuningParams(uint32 rxDelayBase,
								uint32 airtimeFactor);
			void			SetDevicePin(uint32 pin);

private:
			void			_BuildDeviceTab(BView* parent);
			void			_BuildRadioTab(BView* parent);
			void			_BuildMqttTab(BView* parent);

			void			_OnApply();
			void			_OnPresetSelected(int32 preset);
			void			_OnMqttEnableChanged();

			BWindow*		fParent;

			// Device settings
			BTextControl*	fNodeNameControl;
			BTextControl*	fLatitudeControl;
			BTextControl*	fLongitudeControl;
			BMenuField*		fBatteryTypeMenu;
			BTextControl*	fRxDelayBaseControl;
			BTextControl*	fAirtimeFactorControl;
			BTextControl*	fDevicePinControl;

			// Radio settings
			BMenuField*		fPresetMenu;
			BSlider*		fTxPowerSlider;
			BTextControl*	fFrequencyControl;
			BTextControl*	fBandwidthControl;
			BTextControl*	fSpreadingFactorControl;
			BTextControl*	fCodingRateControl;

			// MQTT settings
			BCheckBox*		fMqttEnableCheck;
			BTextControl*	fMqttIataControl;
			BTextControl*	fMqttBrokerControl;
			BTextControl*	fMqttPortControl;
			BTextControl*	fMqttUsernameControl;
			BTextControl*	fMqttPasswordControl;
			BStringView*	fMqttStatusLabel;
			MqttSettings	fMqttSettings;

			// Buttons
			BButton*		fApplyButton;
			BButton*		fRevertButton;

			// State
			bool			fSettingsChanged;
			int32			fSelectedPreset;
};

#endif // SETTINGSWINDOW_H
