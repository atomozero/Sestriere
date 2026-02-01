/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * StatsWindow.cpp — Device statistics window implementation
 */

#include "StatsWindow.h"

#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <StringView.h>
#include <TabView.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"
#include "Protocol.h"
#include "Sestriere.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "StatsWindow"


static const uint32 MSG_REFRESH_STATS	= 'rfst';
static const uint32 MSG_STATS_TIMER		= 'sttm';
static const bigtime_t kStatsRefreshInterval = 5000000;  // 5 seconds


StatsWindow::StatsWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 400, 350), "Device Statistics",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fTabView(NULL),
	fUptimeView(NULL),
	fTxPacketsView(NULL),
	fRxPacketsView(NULL),
	fTxBytesView(NULL),
	fRxBytesView(NULL),
	fRoutedView(NULL),
	fDroppedView(NULL),
	fRssiView(NULL),
	fSnrView(NULL),
	fTxTimeView(NULL),
	fRxTimeView(NULL),
	fChannelBusyView(NULL),
	fCrcErrorsView(NULL),
	fAdvertsSentView(NULL),
	fAdvertsReceivedView(NULL),
	fMsgsSentView(NULL),
	fMsgsReceivedView(NULL),
	fAcksSentView(NULL),
	fAcksReceivedView(NULL),
	fRefreshButton(NULL),
	fCloseButton(NULL),
	fRefreshTimer(NULL)
{
	memset(&fCoreStats, 0, sizeof(fCoreStats));
	memset(&fRadioStats, 0, sizeof(fRadioStats));
	memset(&fPacketStats, 0, sizeof(fPacketStats));

	// Create tab view
	fTabView = new BTabView("stats_tabs", B_WIDTH_FROM_WIDEST);

	// Core stats tab
	BView* coreTab = new BView("core_tab", 0);
	coreTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildCoreTab(coreTab);
	fTabView->AddTab(coreTab, new BTab());
	fTabView->TabAt(0)->SetLabel("Core");

	// Radio stats tab
	BView* radioTab = new BView("radio_tab", 0);
	radioTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildRadioTab(radioTab);
	fTabView->AddTab(radioTab, new BTab());
	fTabView->TabAt(1)->SetLabel("Radio");

	// Packet stats tab
	BView* packetTab = new BView("packet_tab", 0);
	packetTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildPacketTab(packetTab);
	fTabView->AddTab(packetTab, new BTab());
	fTabView->TabAt(2)->SetLabel("Packets");

	// Buttons
	fRefreshButton = new BButton("refresh_button", B_TRANSLATE(TR_BUTTON_REFRESH),
		new BMessage(MSG_REFRESH_STATS));

	fCloseButton = new BButton("close_button", "Close",
		new BMessage(B_QUIT_REQUESTED));

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fTabView, 1.0)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fRefreshButton)
			.Add(fCloseButton)
		.End()
	.End();

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	// Initial stats request
	_RequestStats();

	// Start auto-refresh timer
	BMessage timerMsg(MSG_STATS_TIMER);
	fRefreshTimer = new BMessageRunner(this, &timerMsg, kStatsRefreshInterval);
}


StatsWindow::~StatsWindow()
{
	delete fRefreshTimer;
}


void
StatsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_REFRESH_STATS:
		case MSG_STATS_TIMER:
			_RequestStats();
			break;

		case RESP_CODE_STATS:
		{
			const void* data;
			ssize_t size;
			if (message->FindData(kFieldData, B_RAW_TYPE, &data, &size) != B_OK)
				break;

			if (size < 2)
				break;

			const uint8* payload = static_cast<const uint8*>(data);
			uint8 subType = payload[1];

			switch (subType) {
				case 0:  // Core stats
					if (size >= 30) {
						fCoreStats.uptime = Protocol::ReadU32LE(payload + 2);
						fCoreStats.txPackets = Protocol::ReadU32LE(payload + 6);
						fCoreStats.rxPackets = Protocol::ReadU32LE(payload + 10);
						fCoreStats.txBytes = Protocol::ReadU32LE(payload + 14);
						fCoreStats.rxBytes = Protocol::ReadU32LE(payload + 18);
						fCoreStats.routedPackets = Protocol::ReadU32LE(payload + 22);
						fCoreStats.droppedPackets = Protocol::ReadU32LE(payload + 26);
					}
					break;

				case 1:  // Radio stats
					if (size >= 16) {
						fRadioStats.lastRssi = (int8)payload[2];
						fRadioStats.lastSnr = (int8)payload[3];
						fRadioStats.txTimeMs = Protocol::ReadU32LE(payload + 4);
						fRadioStats.rxTimeMs = Protocol::ReadU32LE(payload + 8);
						fRadioStats.channelBusy = payload[12];
						fRadioStats.crcErrors = Protocol::ReadU32LE(payload + 13);
					}
					break;

				case 2:  // Packet stats
					if (size >= 26) {
						fPacketStats.advertsSent = Protocol::ReadU32LE(payload + 2);
						fPacketStats.advertsReceived = Protocol::ReadU32LE(payload + 6);
						fPacketStats.messagesSent = Protocol::ReadU32LE(payload + 10);
						fPacketStats.messagesReceived = Protocol::ReadU32LE(payload + 14);
						fPacketStats.acksSent = Protocol::ReadU32LE(payload + 18);
						fPacketStats.acksReceived = Protocol::ReadU32LE(payload + 22);
					}
					break;
			}

			_UpdateDisplay();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
StatsWindow::SetCoreStats(const CoreStats& stats)
{
	fCoreStats = stats;
	_UpdateDisplay();
}


void
StatsWindow::SetRadioStats(const RadioStats& stats)
{
	fRadioStats = stats;
	_UpdateDisplay();
}


void
StatsWindow::SetPacketStats(const PacketStats& stats)
{
	fPacketStats = stats;
	_UpdateDisplay();
}


void
StatsWindow::_BuildCoreTab(BView* parent)
{
	fUptimeView = new BStringView("uptime", "Uptime: --");
	fTxPacketsView = new BStringView("tx_packets", "TX Packets: --");
	fRxPacketsView = new BStringView("rx_packets", "RX Packets: --");
	fTxBytesView = new BStringView("tx_bytes", "TX Bytes: --");
	fRxBytesView = new BStringView("rx_bytes", "RX Bytes: --");
	fRoutedView = new BStringView("routed", "Routed: --");
	fDroppedView = new BStringView("dropped", "Dropped: --");

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fUptimeView)
		.Add(fTxPacketsView)
		.Add(fRxPacketsView)
		.Add(fTxBytesView)
		.Add(fRxBytesView)
		.Add(fRoutedView)
		.Add(fDroppedView)
		.AddGlue()
	.End();
}


void
StatsWindow::_BuildRadioTab(BView* parent)
{
	fRssiView = new BStringView("rssi", "Last RSSI: --");
	fSnrView = new BStringView("snr", "Last SNR: --");
	fTxTimeView = new BStringView("tx_time", "TX Time: --");
	fRxTimeView = new BStringView("rx_time", "RX Time: --");
	fChannelBusyView = new BStringView("channel_busy", "Channel Busy: --");
	fCrcErrorsView = new BStringView("crc_errors", "CRC Errors: --");

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fRssiView)
		.Add(fSnrView)
		.Add(fTxTimeView)
		.Add(fRxTimeView)
		.Add(fChannelBusyView)
		.Add(fCrcErrorsView)
		.AddGlue()
	.End();
}


void
StatsWindow::_BuildPacketTab(BView* parent)
{
	fAdvertsSentView = new BStringView("adverts_sent", "Adverts Sent: --");
	fAdvertsReceivedView = new BStringView("adverts_received", "Adverts Received: --");
	fMsgsSentView = new BStringView("msgs_sent", "Messages Sent: --");
	fMsgsReceivedView = new BStringView("msgs_received", "Messages Received: --");
	fAcksSentView = new BStringView("acks_sent", "ACKs Sent: --");
	fAcksReceivedView = new BStringView("acks_received", "ACKs Received: --");

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fAdvertsSentView)
		.Add(fAdvertsReceivedView)
		.Add(fMsgsSentView)
		.Add(fMsgsReceivedView)
		.Add(fAcksSentView)
		.Add(fAcksReceivedView)
		.AddGlue()
	.End();
}


void
StatsWindow::_RequestStats()
{
	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app == NULL || app->GetSerialHandler() == NULL ||
		!app->GetSerialHandler()->IsConnected())
		return;

	// Request all three stat types
	uint8 buffer[8];

	// Core stats (sub=0)
	buffer[0] = CMD_GET_STATS;
	buffer[1] = 0;
	app->GetSerialHandler()->SendFrame(buffer, 2);

	// Radio stats (sub=1)
	buffer[1] = 1;
	app->GetSerialHandler()->SendFrame(buffer, 2);

	// Packet stats (sub=2)
	buffer[1] = 2;
	app->GetSerialHandler()->SendFrame(buffer, 2);
}


void
StatsWindow::_UpdateDisplay()
{
	char buf[64];

	// Core stats
	uint32 uptime = fCoreStats.uptime;
	uint32 hours = uptime / 3600;
	uint32 mins = (uptime % 3600) / 60;
	uint32 secs = uptime % 60;
	snprintf(buf, sizeof(buf), "Uptime: %u:%02u:%02u", hours, mins, secs);
	fUptimeView->SetText(buf);

	snprintf(buf, sizeof(buf), "TX Packets: %u", fCoreStats.txPackets);
	fTxPacketsView->SetText(buf);

	snprintf(buf, sizeof(buf), "RX Packets: %u", fCoreStats.rxPackets);
	fRxPacketsView->SetText(buf);

	snprintf(buf, sizeof(buf), "TX Bytes: %u", fCoreStats.txBytes);
	fTxBytesView->SetText(buf);

	snprintf(buf, sizeof(buf), "RX Bytes: %u", fCoreStats.rxBytes);
	fRxBytesView->SetText(buf);

	snprintf(buf, sizeof(buf), "Routed: %u", fCoreStats.routedPackets);
	fRoutedView->SetText(buf);

	snprintf(buf, sizeof(buf), "Dropped: %u", fCoreStats.droppedPackets);
	fDroppedView->SetText(buf);

	// Radio stats
	snprintf(buf, sizeof(buf), "Last RSSI: %d dBm", fRadioStats.lastRssi);
	fRssiView->SetText(buf);

	snprintf(buf, sizeof(buf), "Last SNR: %.1f dB", fRadioStats.lastSnr / 4.0f);
	fSnrView->SetText(buf);

	snprintf(buf, sizeof(buf), "TX Time: %.1f s", fRadioStats.txTimeMs / 1000.0f);
	fTxTimeView->SetText(buf);

	snprintf(buf, sizeof(buf), "RX Time: %.1f s", fRadioStats.rxTimeMs / 1000.0f);
	fRxTimeView->SetText(buf);

	snprintf(buf, sizeof(buf), "Channel Busy: %u%%", fRadioStats.channelBusy);
	fChannelBusyView->SetText(buf);

	snprintf(buf, sizeof(buf), "CRC Errors: %u", fRadioStats.crcErrors);
	fCrcErrorsView->SetText(buf);

	// Packet stats
	snprintf(buf, sizeof(buf), "Adverts Sent: %u", fPacketStats.advertsSent);
	fAdvertsSentView->SetText(buf);

	snprintf(buf, sizeof(buf), "Adverts Received: %u", fPacketStats.advertsReceived);
	fAdvertsReceivedView->SetText(buf);

	snprintf(buf, sizeof(buf), "Messages Sent: %u", fPacketStats.messagesSent);
	fMsgsSentView->SetText(buf);

	snprintf(buf, sizeof(buf), "Messages Received: %u", fPacketStats.messagesReceived);
	fMsgsReceivedView->SetText(buf);

	snprintf(buf, sizeof(buf), "ACKs Sent: %u", fPacketStats.acksSent);
	fAcksSentView->SetText(buf);

	snprintf(buf, sizeof(buf), "ACKs Received: %u", fPacketStats.acksReceived);
	fAcksReceivedView->SetText(buf);
}
