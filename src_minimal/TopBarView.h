/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TopBarView.h — Toolbar with map icons and status indicators
 */

#ifndef TOPBARVIEW_H
#define TOPBARVIEW_H

#include <View.h>
#include <String.h>

class TopBarView : public BView {
public:
						TopBarView(const char* name);
	virtual				~TopBarView();

	virtual void		Draw(BRect updateRect);
	virtual void		MouseDown(BPoint where);
	virtual void		MouseMoved(BPoint where, uint32 transit,
						const BMessage* dragMessage);
	virtual void		AttachedToWindow();
	virtual	BSize		MinSize();
	virtual	BSize		PreferredSize();

		// Status updates
		void			SetConnected(bool connected, const char* port = NULL);
		void			SetBattery(uint16 milliVolts);
		void			SetRadioStats(int8 rssi, int8 snr,
						uint32 txPkts, uint32 rxPkts);
		void			SetUptime(uint32 seconds);
		void			SetMqttStatus(bool connected);
		void			SetMqttEnabled(bool enabled);

private:
		void			_DrawNetworkMapIcon(BPoint center);
		void			_DrawGeoMapIcon(BPoint center);
		void			_DrawStatsIcon(BPoint center);
		void			_DrawTelemetryIcon(BPoint center);
		void			_DrawPacketAnalyzerIcon(BPoint center);
		void			_DrawDebugLogIcon(BPoint center);
		void			_DrawMqttToggle(BRect rect);
		void			_DrawMqttLogIcon(BPoint center);
		int32			_HitArea(BPoint where) const;
		const char*		_ToolTipForArea(int32 area) const;

		bool			fConnected;
		BString			fPortName;
		uint16			fBatteryMv;
		int8			fRssi;
		int8			fSnr;
		uint32			fTxPackets;
		uint32			fRxPackets;
		uint32			fUptime;
		bool			fMqttConnected;
		bool			fMqttEnabled;

		BRect			fNetworkMapRect;
		BRect			fGeoMapRect;
		BRect			fStatsRect;
		BRect			fTelemetryRect;
		BRect			fPacketAnalyzerRect;
		BRect			fDebugLogRect;
		BRect			fMqttToggleRect;
		BRect			fMqttLogRect;

		// Status indicator hit areas
		BRect			fConnectionDotRect;
		BRect			fBatteryRect;
		BRect			fRssiRect;
		BRect			fTxRxRect;
		BRect			fUptimeRect;

		int32			fHoverArea;
		mutable BString	fToolTipText;
};

#endif // TOPBARVIEW_H
