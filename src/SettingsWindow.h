/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SettingsWindow.h — Application settings dialog
 */

#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <Window.h>

#include "Types.h"

class BButton;
class BCheckBox;
class BSlider;
class BTextControl;

class SettingsWindow : public BWindow {
public:
							SettingsWindow(BWindow* parent);
	virtual					~SettingsWindow();

	virtual void			MessageReceived(BMessage* message);

			void			SetCurrentSettings(const SelfInfo& selfInfo);

private:
			void			_BuildDeviceTab(BView* parent);
			void			_BuildRadioTab(BView* parent);
			void			_BuildAboutTab(BView* parent);

			void			_OnApply();
			void			_OnRevert();
			void			_LoadSettings();
			void			_SaveSettings();

			BWindow*		fParent;

			// Device settings
			BTextControl*	fNodeNameControl;
			BTextControl*	fLatitudeControl;
			BTextControl*	fLongitudeControl;

			// Radio settings
			BSlider*		fTxPowerSlider;
			BTextControl*	fFrequencyControl;
			BTextControl*	fBandwidthControl;
			BTextControl*	fSpreadingFactorControl;
			BTextControl*	fCodingRateControl;

			// Checkboxes
			BCheckBox*		fAutoSyncCheck;
			BCheckBox*		fNotificationsCheck;

			// Buttons
			BButton*		fApplyButton;
			BButton*		fRevertButton;

			// Current settings
			SelfInfo		fCurrentSettings;
			bool			fSettingsChanged;
};

#endif // SETTINGSWINDOW_H
