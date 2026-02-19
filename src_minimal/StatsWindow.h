/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatsWindow.h — Device statistics window
 */

#ifndef STATSWINDOW_H
#define STATSWINDOW_H

#include <Window.h>

class BButton;
class BMessageRunner;
class BScrollView;
class StatRowView;

// Statistics data structures — MeshCore V3 protocol fields
struct CoreStats {
	uint32	uptime;
	uint16	batteryMv;

	CoreStats() : uptime(0), batteryMv(0) {}
};

struct RadioStats {
	int16	noiseFloor;
	int8	lastRssi;
	int8	lastSnr;
	uint32	txAirTimeMs;
	uint32	rxAirTimeMs;

	RadioStats() : noiseFloor(0), lastRssi(0), lastSnr(0),
				   txAirTimeMs(0), rxAirTimeMs(0) {}
};

struct PacketStats {
	uint32	recvPackets;
	uint32	sentPackets;
	uint32	sentFlood;
	uint32	sentDirect;
	uint32	recvFlood;
	uint32	recvDirect;

	PacketStats() : recvPackets(0), sentPackets(0), sentFlood(0),
					sentDirect(0), recvFlood(0), recvDirect(0) {}
};

class StatsWindow : public BWindow {
public:
							StatsWindow(BWindow* parent);
	virtual					~StatsWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			SetCoreStats(const CoreStats& stats);
			void			SetRadioStats(const RadioStats& stats);
			void			SetPacketStats(const PacketStats& stats);

			// Parse stats response from device
			void			ParseStatsResponse(const uint8* data, size_t length);

private:
			void			_RequestStats();
			void			_UpdateDisplay();

			BWindow*		fParent;
			BScrollView*	fScrollView;

			// Core stats views
			StatRowView*	fUptimeView;
			StatRowView*	fBatteryView;

			// Radio stats views
			StatRowView*	fNoiseFloorView;
			StatRowView*	fRssiView;
			StatRowView*	fSnrView;
			StatRowView*	fTxAirTimeView;
			StatRowView*	fRxAirTimeView;

			// Packet stats views
			StatRowView*	fRecvPacketsView;
			StatRowView*	fSentPacketsView;
			StatRowView*	fSentFloodView;
			StatRowView*	fSentDirectView;
			StatRowView*	fRecvFloodView;
			StatRowView*	fRecvDirectView;

			BButton*		fRefreshButton;
			BButton*		fCloseButton;

			CoreStats		fCoreStats;
			RadioStats		fRadioStats;
			PacketStats		fPacketStats;

			BMessageRunner*	fRefreshTimer;
};

#endif // STATSWINDOW_H
