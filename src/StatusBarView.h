/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatusBarView.h — Status bar showing connection and device info
 */

#ifndef STATUSBARVIEW_H
#define STATUSBARVIEW_H

#include <View.h>
#include <String.h>

class BMessageRunner;
class BStringView;

class StatusBarView : public BView {
public:
							StatusBarView(const char* name);
	virtual					~StatusBarView();

	virtual void			AttachedToWindow();
	virtual void			MessageReceived(BMessage* message);

			void			SetNodeName(const char* name);
			void			SetConnectionStatus(const char* status);
			void			SetBatteryInfo(uint16 milliVolts);
			void			SetRadioInfo(const char* info);
			void			SetTemporaryStatus(const char* status,
								bigtime_t duration = 3000000);

private:
			void			_UpdateDisplay();
			void			_ClearTemporaryStatus();

			BStringView*	fNodeNameView;
			BStringView*	fStatusView;
			BStringView*	fBatteryView;
			BStringView*	fRadioView;

			BString			fNodeName;
			BString			fConnectionStatus;
			BString			fBatteryInfo;
			BString			fRadioInfo;
			BString			fTempStatus;

			BMessageRunner*	fTempStatusTimer;
};

#endif // STATUSBARVIEW_H
