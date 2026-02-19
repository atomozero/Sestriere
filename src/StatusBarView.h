/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatusBarView.h — Rich status bar with battery, radio stats, and connection info
 */

#ifndef STATUSBARVIEW_H
#define STATUSBARVIEW_H

#include <View.h>
#include <String.h>

class StatusBarView : public BView {
public:
							StatusBarView(const char* name);
	virtual					~StatusBarView();

	virtual void			Draw(BRect updateRect);
	virtual void			GetPreferredSize(float* width, float* height);

			// Connection status
			void			SetConnected(bool connected, const char* port = NULL);

			// Battery info
			void			SetBattery(uint16 milliVolts, int8 percent = -1);

			// Radio stats
			void			SetRadioStats(int8 rssi, int8 snr, uint32 txPackets,
								uint32 rxPackets);

			// Uptime
			void			SetUptime(uint32 seconds);

			// Raw packet count
			void			SetRawPackets(uint32 count);

			// MQTT status
			void			SetMqttConnected(bool connected);

private:
			void			_DrawSection(BRect& rect, const char* label,
								const char* value, rgb_color valueColor);
			rgb_color		_BatteryColor(uint16 milliVolts);
			rgb_color		_SignalColor(int8 snr);

			bool			fConnected;
			BString			fPortName;

			uint16			fBatteryMv;
			int8			fBatteryPercent;

			int8			fLastRssi;
			int8			fLastSnr;
			uint32			fTxPackets;
			uint32			fRxPackets;

			uint32			fUptime;
			uint32			fRawPackets;
			bool			fMqttConnected;
};

#endif // STATUSBARVIEW_H
