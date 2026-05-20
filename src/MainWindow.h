/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MainWindow.h — Main application window with Telegram-style layout
 */

#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H

#include <Window.h>
#include "Compat.h"

class BFilePanel;

#include "ImageSession.h"
#include "MqttClient.h"
#include "TimeoutPredictor.h"
#include "SarMarker.h"
#include "Types.h"
#include "VoiceSession.h"

class AudioEngine;
class ContactManager;
class FrameParser;
class MediaHandler;

class BButton;
class BCheckBox;
class ProtocolHandler;
class BListView;
class BMenu;
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
class GifPickerWindow;
class ContactItem;
class MicIconView;
class MqttClient;
class MqttLogWindow;
class ContactExportWindow;
class LoginWindow;
class MapWindow;
class MissionControlWindow;
class NetworkMapWindow;
class ProfileWindow;
class PacketAnalyzerWindow;
class SerialHandler;
class SerialMonitorWindow;
class SettingsWindow;
class StatsWindow;
class TelemetryWindow;
class TopBarView;
class TracePathWindow;
class LoSWindow;

// Pending outgoing message tracked for delivery status
struct PendingMessage {
	char		contactKey[13] = {};	// 12 hex chars + null
	uint8		pubKey[32] = {};		// Full 32-byte pubkey for resend
	uint32		timestamp = 0;			// Message timestamp (for DB lookup)
	char		text[256] = {};			// Original plaintext (for display/DB)
	char		wireText[256] = {};		// Wire text (SMAZ compressed if applicable)
	size_t		wireLen = 0;			// Wire text length
	uint8		txtType = 0;			// TXT_TYPE_PLAIN or TXT_TYPE_CLI_DATA
	uint8		attemptCount = 1;		// Current attempt (1-based)
	bigtime_t	sentTime = 0;			// system_time() when sent
	bool		gotRspSent = false;		// True after RSP_SENT received
	uint32		expectedAck = 0;		// ackCode from RSP_SENT for matching CONFIRMED
	bool		inGracePeriod = false;	// True = waiting for late ACK after max retries
	bigtime_t	graceStartTime = 0;		// system_time() when grace period started
	int32		chatViewIndex = -1;		// Index in ChatView (-1 if not visible)

	PendingMessage() = default;
};

class MainWindow : public BWindow {
public:
							MainWindow();
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

private:
			// Forward-declared for use as return type below
			struct AdminSession {
				uint8	key[6];
				bool	isAdmin;
			};
			void			_BuildUI();
			void			_BuildMenuBar();
			void			_RefreshPorts();
			void			_RefreshPortMenu();
			void			_Connect();
			void			_Disconnect();
			void			_UpdateConnectionUI();

			// UI-level messaging (uses ProtocolHandler for frame building)
			void			_SendTextMessage(const char* text);
			void			_SendChannelMessage(const char* text);
			void			_SendCliCommand(const char* command);

			// Frame handling
			void			_OnFrameReceived(BMessage* message);
			void			_OnFrameSent(BMessage* message);
			void			_ParseFrame(const uint8* data, size_t length);
			void			_HandleFrameMessage(BMessage* message);

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
			void			_HandleCustomVars(const uint8* data, size_t length);
			void			_HandleAdvertPath(const uint8* data, size_t length);
			void			_HandleTuningParams(const uint8* data, size_t length);
			void			_HandleAllowedRepeatFreq(const uint8* data, size_t length);

			// Push notification handlers
			void			_HandleChannelInfo(const uint8* data, size_t length);

			void			_HandlePushMsgWaiting(const uint8* data, size_t length);
			void			_HandlePushAdvert(const uint8* data, size_t length);
			void			_HandlePushTraceData(const uint8* data, size_t length);
			void			_HandlePushTelemetry(const uint8* data, size_t length);
			void			_HandlePushLoginResult(const uint8* data,
								size_t length);
			void			_HandlePushStatusResponse(const uint8* data,
								size_t length);
			void			_HandleRawPacket(const uint8* data, size_t length);
			void			_HandlePushRawData(const uint8* data, size_t length);
			void			_HandlePushControlData(const uint8* data,
								size_t length);
			void			_HandlePushContactDeleted(const uint8* data,
								size_t length);
			void			_HandlePushPathDiscovery(const uint8* data,
								size_t length);
			void			_HandleSignResponse(const uint8* data,
								size_t length, bool isStart);

			// Connection events
			void			_OnConnected(BMessage* message);
			void			_OnDisconnected();
			void			_OnError(BMessage* message);

			// Logging (now uses DebugLogWindow)
			void			_LogTx(const uint8* data, size_t length);
			void			_LogRx(const uint8* data, size_t length);
			void			_ForwardFrameToSerialMonitor(const char* direction,
								const uint8* data, size_t length);
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

			// Message search
			void			_ToggleSearchBar();
			void			_PerformSearch(const char* query);
			void			_CloseSearch();

			// Media handler context
			void			_UpdateMediaContext();

			// Input char counter
			void			_UpdateCharCounter();

			// MQTT settings
			void			_SaveMqttSettings();
			void			_LoadMqttSettings();

			// Device settings (battery type, etc.)
			void			_SaveDeviceSettings();
			void			_LoadDeviceSettings();
			void			_SaveUISettings();
			void			_LoadUISettings();

			// People contacts integration
			void			_SaveContactAsPerson(ContactInfo* contact);
			void			_LoadPeopleContacts();
			BString			_GetPeoplePath();

			// GPX export
			void			_ExportGPX(const char* path);

			// Mute helpers
			bool			_IsMuted(const char* keyHex);
			void			_SetMuted(const char* keyHex, bool muted);

			// Contact group helpers
			void			_LoadContactGroups();
			void			_RefreshContactList();

			// Reactions
			void			_AddReactionToMessage(ChatMessage* msg,
								const char* emoji);

			// Nearest repeater calculation
			void			_UpdateNearestRepeater();

			// Admin session helpers
			AdminSession*	_FindAdminSession(const uint8* prefix);
			bool			_IsLoggedInto(const uint8* prefix);
			void			_ClearAdminSessions();

			// Delivery queue management
			void			_StartDeliveryTimer();
			void			_StopDeliveryTimer();
			void			_CheckDeliveryTimeouts();
			void			_DrainOutbox();
			void			_RetryMessage(PendingMessage* pending);
			void			_FailMessage(PendingMessage* pending);

			// Admin toolbar (in chat area, above input bar)
			void			_ShowAdminToolbar(bool show);

			// SAR marker forwarding
			void			_ForwardSarMarkerToMap(const SarMarker& marker,
								const char* senderName);
			void			_ShowSarMarkerDialog();

			// GIF sharing (download thread still in MainWindow for UI access)
			void			_DownloadAndDisplayGif(const char* gifId,
								int32 chatViewIndex);
	static	int32			_GifDownloadThread(void* data);

			// Image/Voice UI helpers (protocol logic in MediaHandler)
			void			_UpdateVoiceMessageView(uint32 sessionId);
			void			_StartImageFetch(uint32 sessionId);
			void			_UpdateImageMessageView(uint32 sessionId);

			SerialHandler*	fSerialHandler;
			ProtocolHandler* fProtocol;
			FrameParser*	fFrameParser;
			MediaHandler*	fMediaHandler;
			ContactManager*	fContactManager;

			// Menu bar and status bar
			BMenuBar*		fMenuBar;
			TopBarView*		fTopBar;
			BMenu*			fConnectMenu;
			BMenuItem*		fDisconnectItem;

			// UI elements - Sidebar
			BTextControl*	fSearchField;
			BCheckBox*		fShowChats;
			BCheckBox*		fShowRepeaters;
			BCheckBox*		fShowRooms;
			BListView*		fContactList;
			BScrollView*	fContactScroll;
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

			// UI elements - Admin toolbar (in chat area)
			BView*			fAdminToolbar;
			BButton*		fAdminVersionBtn;
			BButton*		fAdminNeighborsBtn;
			BButton*		fAdminClockBtn;
			BButton*		fAdminClearStatsBtn;
			BButton*		fAdminRebootBtn;
			BButton*		fAdminFactoryResetBtn;
			BButton*		fAdminSetNameBtn;
			BButton*		fAdminSetPwdBtn;
			BTextControl*	fAdminNameField;
			BTextControl*	fAdminPwdField;

			// UI elements - Info panel
			ContactInfoPanel*	fInfoPanel;

			// UI elements - Layout
			BSplitView*		fMainSplit;
			uint8			fDeviceAdvType;

			// State
			int32			fSelectedPreset;
			BString			fSelectedPort;
			int32			fSelectedContact;
			bool			fConnected;

			// Contacts (owned by fContactManager)
			bool			fSyncingContacts;
			uint32			fContactsSince;
			bool			fSyncingMessages;

			// Channels (all channels including Public at index 0)
			OwningObjectList<ChannelInfo>	fChannels;
			uint8			fMaxChannels;
			uint8			fChannelEnumIndex;   // Current index during enumeration
			bool			fEnumeratingChannels;
			int32			fSelectedChannelIdx; // -1 = none, >= 0 = channel slot

			// Delivery tracking — FIFO queue of pending outgoing messages
			OwningObjectList<PendingMessage>	fPendingMessages;
			BMessageRunner*	fDeliveryCheckTimer;
			TimeoutPredictor fTimeoutPredictor;

			// Mode flags
			bool			fSendingToChannel;
			bool			fLoginPending;
			uint8			fLoginTargetKey[6];

			// Admin sessions (multi-repeater login)
			OwningObjectList<AdminSession>	fAdminSessions;

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
			MissionControlWindow* fMissionControlWindow;
			ProfileWindow*	fProfileWindow;
			SerialMonitorWindow* fSerialMonitorWindow;
			GifPickerWindow* fGifPickerWindow;
			LoSWindow*		fLoSWindow;

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
			BMessageRunner*	fHandshakeTimer;

			// Cached stats for status bar
			uint8			fBatteryType;	// BatteryChemistry enum
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
			uint32			fDevicePin;
			uint8			fClientRepeat;   // v9+: client repeat/off-grid mode
			uint8			fPathHashMode;   // v10+: 0=1B, 1=2B, 2=3B hashes

			// Other params from RSP_SELF_INFO (for CMD_SET_OTHER_PARAMS)
			uint8			fMultiAcks;
			uint8			fAdvertLocPolicy;
			uint8			fTelemetryModes;
			uint8			fManualAddContacts;

			// Telemetry polling state
			int32			fTelemetryPollIndex;

			// GPX export
			BFilePanel*		fGpxSavePanel;

			// Ping
			bigtime_t		fPingStartTime;
			uint8			fPingTargetKey[kPubKeyPrefixSize];
			bool			fPingPending;
			BMessageRunner*	fPingTimeoutRunner;

			// Ping All
			bool			fPingAllActive;
			int32			fPingAllIndex;
			int32			fPingAllTotal;
			int32			fPingAllResponded;

			// Mute state
			BMessage		fMutedKeys;

			// Contact groups
			OwningObjectList<BString>	fGroupNames;

			// Voice/Image (sessions fully owned by MediaHandler)
			AudioEngine*	fAudioEngine;
			MicIconView*	fVoiceButton;

			// Image sharing UI
			BFilePanel*		fImageOpenPanel;
			BFilePanel*		fImageSavePanel;
			const BBitmap*	fSaveBitmap;
			BButton*		fAttachButton;
			BButton*		fGifButton;
			BMessageRunner*	fImageExpireTimer;
};


#endif // _MAINWINDOW_H
