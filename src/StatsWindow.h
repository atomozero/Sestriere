/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatsWindow.h — Device statistics window
 */

#ifndef STATSWINDOW_H
#define STATSWINDOW_H

#include <Window.h>

#include "Types.h"

class BButton;
class BListView;
class BMessageRunner;
class BStringView;
class BTabView;

// Statistics data structures
struct CoreStats {
	uint32	uptime;
	uint32	txPackets;
	uint32	rxPackets;
	uint32	txBytes;
	uint32	rxBytes;
	uint32	routedPackets;
	uint32	droppedPackets;
};

struct RadioStats {
	int8	lastRssi;
	int8	lastSnr;
	uint32	txTimeMs;
	uint32	rxTimeMs;
	uint8	channelBusy;
	uint32	crcErrors;
};

struct PacketStats {
	uint32	advertsSent;
	uint32	advertsReceived;
	uint32	messagesSent;
	uint32	messagesReceived;
	uint32	acksSent;
	uint32	acksReceived;
};

class StatsWindow : public BWindow {
public:
							StatsWindow(BWindow* parent);
	virtual					~StatsWindow();

	virtual void			MessageReceived(BMessage* message);

			void			SetCoreStats(const CoreStats& stats);
			void			SetRadioStats(const RadioStats& stats);
			void			SetPacketStats(const PacketStats& stats);

private:
			void			_BuildCoreTab(BView* parent);
			void			_BuildRadioTab(BView* parent);
			void			_BuildPacketTab(BView* parent);
			void			_RequestStats();
			void			_UpdateDisplay();

			BWindow*		fParent;
			BTabView*		fTabView;

			// Core stats views
			BStringView*	fUptimeView;
			BStringView*	fTxPacketsView;
			BStringView*	fRxPacketsView;
			BStringView*	fTxBytesView;
			BStringView*	fRxBytesView;
			BStringView*	fRoutedView;
			BStringView*	fDroppedView;

			// Radio stats views
			BStringView*	fRssiView;
			BStringView*	fSnrView;
			BStringView*	fTxTimeView;
			BStringView*	fRxTimeView;
			BStringView*	fChannelBusyView;
			BStringView*	fCrcErrorsView;

			// Packet stats views
			BStringView*	fAdvertsSentView;
			BStringView*	fAdvertsReceivedView;
			BStringView*	fMsgsSentView;
			BStringView*	fMsgsReceivedView;
			BStringView*	fAcksSentView;
			BStringView*	fAcksReceivedView;

			BButton*		fRefreshButton;
			BButton*		fCloseButton;

			CoreStats		fCoreStats;
			RadioStats		fRadioStats;
			PacketStats		fPacketStats;

			BMessageRunner*	fRefreshTimer;
};

#endif // STATSWINDOW_H
