/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MainWindow.h — Main application window with Telegram-style layout
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Window.h>
#include <ObjectList.h>

#include "MqttClient.h"
#include "Types.h"

class BButton;
class BListView;
class BMenuBar;
class BMenuItem;
class BMessageRunner;
class BScrollView;
class BSplitView;
class BStringView;
class BTextControl;
class GrowingTextView;
class ChatHeaderView;
class ChatView;
class ContactInfoPanel;
class ContactItem;
class MqttClient;
class MqttLogWindow;
class ContactExportWindow;
class LoginWindow;
class MapWindow;
class NetworkMapWindow;
class PacketAnalyzerWindow;
class SerialHandler;
class SettingsWindow;
class StatsWindow;
class TelemetryWindow;
class TopBarView;
class TracePathWindow;

class MainWindow : public BWindow {
public:
							MainWindow();
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

private:
			void			_BuildUI();
			void			_BuildMenuBar();
			void			_RefreshPorts();
			void			_Connect();
			void			_Disconnect();
			void			_UpdateConnectionUI();

			// Protocol commands
			void			_SendAppStart();
			void			_SendDeviceQuery();
			void			_SendExportSelf();
			void			_SendRadioParams();
			void			_SendGetContacts();
			void			_SendSelfAdvert();
			void			_SendGetBattery();
			void			_SendGetStats();
			void			_SendSetName(const char* name);
			void			_SendTextMessage(const char* text);
			void			_SendChannelMessage(const char* text);
			void			_SendSetLatLon(double lat, double lon);
			void			_SendSetTxPower(uint8 power);
			void			_SendLogin(const uint8* pubkey, const char* password);
			void			_SendReboot();
			void			_SendFactoryReset();
			void			_SendResetPath(const uint8* pubkey);
			void			_SendRemoveContact(const uint8* pubkey);
			void			_SendOtherParams();
			void			_SendSyncNextMessage();
			void			_SendGetChannel(uint8 index);
			void			_SendSetChannel(uint8 index, const char* name,
								const uint8* secret);
			void			_SendRemoveChannel(uint8 index);
			void			_SendStatusRequest(const uint8* pubkey);
			void			_SendTelemetryRequest(const uint8* pubkey);
			void			_SendCliCommand(const char* command);
			void			_SendGetDeviceTime();
			void			_SendSetDeviceTime(uint32 epoch);
			void			_SendAddUpdateContact(const uint8* pubkey,
								const char* name, uint8 type);
			void			_SendShareContact(const uint8* pubkey);
			void			_SendGetTuningParams();
			void			_SendSetTuningParams(uint32 rxDelayBase,
								uint32 airtimeFactor);
			void			_SendRawData(const uint8* payload, size_t length);

			// Frame handling
			void			_OnFrameReceived(BMessage* message);
			void			_OnFrameSent(BMessage* message);
			void			_ParseFrame(const uint8* data, size_t length);

			// Protocol responses
			void			_HandleDeviceInfo(const uint8* data, size_t length);
			void			_HandleExportContact(const uint8* data, size_t length);
			void			_HandleContactsStart(const uint8* data, size_t length);
			void			_HandleContact(const uint8* data, size_t length);
			void			_HandleContactsEnd(const uint8* data, size_t length);
			void			_HandleSelfInfo(const uint8* data, size_t length);
			void			_HandleMsgSent(const uint8* data, size_t length);
			void			_HandleContactMsgRecv(const uint8* data, size_t length,
								bool isV3);
			void			_HandleChannelMsgRecv(const uint8* data, size_t length,
								bool isV3);
			void			_HandleBattAndStorage(const uint8* data, size_t length);
			void			_HandleStats(const uint8* data, size_t length);
			void			_HandleCmdErr(const uint8* data, size_t length);
			void			_HandleCurrTime(const uint8* data, size_t length);

			// Push notification handlers
			void			_HandleChannelInfo(const uint8* data, size_t length);

			void			_HandlePushMsgWaiting(const uint8* data, size_t length);
			void			_HandlePushAdvert(const uint8* data, size_t length);
			void			_HandlePushTraceData(const uint8* data, size_t length);
			void			_HandlePushTelemetry(const uint8* data, size_t length);
			void			_HandlePushLoginResult(uint8 code);
			void			_HandlePushStatusResponse(const uint8* data,
								size_t length);
			void			_HandleRawPacket(const uint8* data, size_t length);
			void			_HandlePushRawData(const uint8* data, size_t length);

			// Connection events
			void			_OnConnected(BMessage* message);
			void			_OnDisconnected();
			void			_OnError(BMessage* message);

			// Logging (now uses DebugLogWindow)
			void			_LogTx(const uint8* data, size_t length);
			void			_LogRx(const uint8* data, size_t length);
			void			_LogMessage(const char* prefix, const char* text);

			// Sidebar
			void			_UpdateSidebarDeviceLabel();

			// Contact helpers
			void			_UpdateContactList();
			void			_FilterContacts(const char* filter);
			void			_SelectContact(int32 index);
			ContactItem*	_FindContactItemByPrefix(const uint8* prefix);
			ContactInfo*	_FindContactByPrefix(const uint8* prefix, size_t prefixLen);

			// Message persistence
			void			_SaveMessages();
			void			_LoadMessages();
			BString			_GetSettingsPath();

			// Message search
			void			_ToggleSearchBar();
			void			_PerformSearch(const char* query);
			void			_CloseSearch();

			// Input char counter
			void			_UpdateCharCounter();

			// MQTT settings
			void			_SaveMqttSettings();
			void			_LoadMqttSettings();

			// People contacts integration
			void			_SaveContactAsPerson(ContactInfo* contact);
			void			_LoadPeopleContacts();
			BString			_GetPeoplePath();

			SerialHandler*	fSerialHandler;

			// Menu bar and status bar
			BMenuBar*		fMenuBar;
			TopBarView*		fTopBar;
			BMenuItem*		fConnectItem;
			BMenuItem*		fDisconnectItem;

			// UI elements - Sidebar
			BTextControl*	fSearchField;
			BListView*		fContactList;
			BScrollView*	fContactScroll;
			ContactItem*	fChannelItem;
			BStringView*	fSidebarDeviceLabel;

			// UI elements - Chat area
			ChatHeaderView*	fChatHeader;
			ChatView*		fChatView;
			BScrollView*	fChatScroll;
			GrowingTextView*	fMessageInput;
			BButton*		fSendButton;
			BStringView*	fCharCounter;

			// UI elements - Message search
			BView*			fSearchBar;
			BTextControl*	fMsgSearchField;
			BButton*		fSearchCloseButton;
			bool			fSearchActive;

			// UI elements - Info panel
			ContactInfoPanel*	fInfoPanel;

			// UI elements - Layout
			BSplitView*		fMainSplit;

			// State
			int32			fSelectedPreset;
			BString			fSelectedPort;
			int32			fSelectedContact;
			bool			fConnected;

			// Contacts (owning = true)
			BObjectList<ContactInfo, true>	fContacts;
			BObjectList<ContactInfo, true>	fOldContacts;  // Temp storage during sync
			bool			fSyncingContacts;
			bool			fSyncingMessages;

			// Channel messages (public channel history)
			BObjectList<ChatMessage, true>	fChannelMessages;

			// Private channels
			BObjectList<ChannelInfo, true>	fChannels;
			uint8			fMaxChannels;
			uint8			fChannelEnumIndex;   // Current index during enumeration
			bool			fEnumeratingChannels;
			int32			fSelectedChannelIdx; // -1 = none, >= 0 = channel slot

			// Delivery tracking
			int32			fPendingMsgIndex;

			// Mode flags
			bool			fSendingToChannel;
			bool			fLoginPending;
			uint8			fLoginTargetKey[6];
			bool			fLoggedIn;
			uint8			fLoggedInKey[6];

			// Child windows
			SettingsWindow*	fSettingsWindow;
			StatsWindow*	fStatsWindow;
			TracePathWindow* fTracePathWindow;
			NetworkMapWindow* fNetworkMapWindow;
			TelemetryWindow* fTelemetryWindow;
			LoginWindow*	fLoginWindow;
			MapWindow*		fMapWindow;
			ContactExportWindow* fContactExportWindow;
			PacketAnalyzerWindow* fPacketAnalyzerWindow;
			MqttLogWindow*	fMqttLogWindow;

			// MQTT client
			MqttClient*		fMqttClient;
			MqttSettings	fMqttSettings;

			// Raw packet analysis
			uint32			fRawPacketCount;
			uint32			fLastRawPacketTime;

			// Timers
			BMessageRunner*	fAutoConnectTimer;
			BMessageRunner*	fStatsRefreshTimer;
			BMessageRunner*	fAutoSyncRunner;
			BMessageRunner*	fAdminRefreshTimer;
			BMessageRunner*	fTelemetryPollTimer;

			// Cached stats for status bar
			uint16			fBatteryMv;
			int8			fLastRssi;
			int8			fLastSnr;
			int8			fNoiseFloor;
			uint32			fTxPackets;
			uint32			fRxPackets;
			uint32			fDeviceUptime;

			// Device info for MQTT
			char			fDeviceName[64];
			char			fDeviceFirmware[32];
			char			fDeviceBoard[32];
			char			fPublicKey[65];  // 64 hex + null
			uint32			fSelfNodeId;     // First 4 bytes of pubkey as uint32
			bool			fHasDeviceInfo;  // True after RSP_SELF_INFO received

			// Radio parameters from RSP_SELF_INFO
			uint32			fRadioFreq;      // Hz
			uint32			fRadioBw;        // Hz
			uint8			fRadioSf;
			uint8			fRadioCr;
			uint8			fRadioTxPower;
			bool			fHasRadioParams;

			// Other params from RSP_SELF_INFO (for CMD_SET_OTHER_PARAMS)
			uint8			fMultiAcks;
			uint8			fAdvertLocPolicy;
			uint8			fTelemetryModes;
			uint8			fManualAddContacts;

			// Telemetry polling state
			int32			fTelemetryPollIndex;
};


#endif // MAINWINDOW_H
