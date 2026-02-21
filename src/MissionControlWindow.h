/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MissionControlWindow.h — Unified dashboard for device/radio/network overview
 */

#ifndef MISSIONCONTROLWINDOW_H
#define MISSIONCONTROLWINDOW_H

#include <MessageRunner.h>
#include <Window.h>
#include <SupportDefs.h>

class BButton;
class BStringView;
class BTextView;
class BScrollView;
class BWindow;

class AlertBannerView;
class MetricCardView;
class HealthScoreView;
class ContactGridView;
class DashboardSNRView;
class PacketRateView;
class MiniTopoView;
class SessionTimelineView;

// Node data for mini topology
static const int32 kSparklinePoints = 8;

struct TopoNode {
	char		name[32];
	uint8		status;		// 0=offline, 1=recent, 2=online
	int8		snr;
	int8		snrHistory[kSparklinePoints];  // Recent SNR samples
	int32		snrHistoryCount;
};

class MissionControlWindow : public BWindow {
public:
							MissionControlWindow(BWindow* parent);
	virtual					~MissionControlWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

	// Called from MainWindow with LockLooper()
	void					SetConnectionState(bool connected,
								const char* deviceName,
								const char* firmware);
	void					SetBatteryInfo(uint16 battMv, uint32 usedKb,
								uint32 totalKb);
	void					SetDeviceStats(uint32 uptime, uint32 txPackets,
								uint32 rxPackets);
	void					SetRadioStats(int8 rssi, int8 snr,
								int8 noiseFloor);
	void					SetRadioConfig(uint32 freqHz, uint32 bwHz,
								uint8 sf, uint8 cr, uint8 txPower);
	void					SetPacketStats(uint32 txPackets,
								uint32 rxPackets);
	void					UpdateContacts(int32 total, int32 online,
								int32 recent);
	void					SetContactNodes(const TopoNode* nodes,
								int32 count);
	void					SetContactHeatmap(const int8* snrValues,
								const uint8* statuses,
								int32 count);
	void					AddSNRDataPoint(int8 snr);
	void					AddRSSIDataPoint(int8 rssi);
	void					AddActivityEvent(const char* category,
								const char* text);

private:
	void					_BuildLayout();
	void					_RecalcHealthScore();
	void					_CheckAlerts();
	void					_UpdateLastUpdate();
	void					_AddTimestampedEvent(const char* category,
								const char* text);

	BWindow*				fParent;

	// Alert banner
	AlertBannerView*		fAlertBanner;

	// Top row cards
	MetricCardView*			fDeviceCard;
	MetricCardView*			fRadioCard;

	// Network overview card
	HealthScoreView*		fHealthScore;
	ContactGridView*		fContactGrid;

	// Middle row charts
	DashboardSNRView*		fSNRChart;
	PacketRateView*			fPacketRateChart;

	// Mini topology
	MiniTopoView*			fMiniTopo;

	// Session timeline
	SessionTimelineView*	fTimeline;

	// Quick actions
	BButton*				fAdvertButton;
	BButton*				fSyncButton;
	BButton*				fStatsButton;

	// Bottom activity feed
	BTextView*				fActivityFeed;
	BScrollView*			fActivityScroll;
	int32					fActivityLineCount;

	// Last update footer
	BStringView*			fLastUpdateLabel;
	bigtime_t				fLastDataTime;

	// Cached state for health score
	bool					fConnected;
	uint16					fBatteryMv;
	int8					fRssi;
	int8					fSnr;
	int32					fContactsTotal;
	int32					fContactsOnline;

	// Storage
	uint32					fUsedKb;
	uint32					fTotalKb;

	// Cached for display
	uint32					fUptime;
	uint32					fTxPackets;
	uint32					fRxPackets;

	// Timers
	BMessageRunner*			fRefreshTimer;
	BMessageRunner*			fPulseTimer;
	BMessageRunner*			fAlertFlashTimer;
};

#endif // MISSIONCONTROLWINDOW_H
