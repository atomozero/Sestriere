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

// Statistics data structures
struct CoreStats {
	uint32	uptime;
	uint32	txPackets;
	uint32	rxPackets;
	uint32	txBytes;
	uint32	rxBytes;
	uint32	routedPackets;
	uint32	droppedPackets;

	CoreStats() : uptime(0), txPackets(0), rxPackets(0), txBytes(0),
				  rxBytes(0), routedPackets(0), droppedPackets(0) {}
};

struct RadioStats {
	int8	lastRssi;
	int8	lastSnr;		// SNR * 4
	uint32	txTimeMs;
	uint32	rxTimeMs;
	uint8	channelBusy;	// percentage
	uint32	crcErrors;

	RadioStats() : lastRssi(0), lastSnr(0), txTimeMs(0), rxTimeMs(0),
				   channelBusy(0), crcErrors(0) {}
};

struct PacketStats {
	uint32	advertsSent;
	uint32	advertsReceived;
	uint32	messagesSent;
	uint32	messagesReceived;
	uint32	acksSent;
	uint32	acksReceived;

	PacketStats() : advertsSent(0), advertsReceived(0), messagesSent(0),
					messagesReceived(0), acksSent(0), acksReceived(0) {}
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
			StatRowView*	fTxPacketsView;
			StatRowView*	fRxPacketsView;
			StatRowView*	fTxBytesView;
			StatRowView*	fRxBytesView;
			StatRowView*	fRoutedView;
			StatRowView*	fDroppedView;

			// Radio stats views
			StatRowView*	fRssiView;
			StatRowView*	fSnrView;
			StatRowView*	fTxTimeView;
			StatRowView*	fRxTimeView;
			StatRowView*	fChannelBusyView;
			StatRowView*	fCrcErrorsView;

			// Packet stats views
			StatRowView*	fAdvertsSentView;
			StatRowView*	fAdvertsReceivedView;
			StatRowView*	fMsgsSentView;
			StatRowView*	fMsgsReceivedView;
			StatRowView*	fAcksSentView;
			StatRowView*	fAcksReceivedView;

			BButton*		fRefreshButton;
			BButton*		fCloseButton;

			CoreStats		fCoreStats;
			RadioStats		fRadioStats;
			PacketStats		fPacketStats;

			BMessageRunner*	fRefreshTimer;
};

#endif // STATSWINDOW_H
