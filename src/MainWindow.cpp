/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MainWindow.cpp — Main application window with Telegram-style layout
 */

#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <CardView.h>
#include <CheckBox.h>
#include <Clipboard.h>
#include <Deskbar.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <SeparatorItem.h>
#include <MessageRunner.h>
#include <MimeType.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "AddChannelWindow.h"
#include "ChatHeaderView.h"
#include "ChatView.h"
#include "Constants.h"
#include "ContactExportWindow.h"
#include "ContactInfoPanel.h"
#include "GifPickerWindow.h"
#include "GiphyClient.h"
#include "DatabaseManager.h"
#include "GrowingTextView.h"
#include "ImageCodec.h"
#include "ImageSession.h"
#include "MessageView.h"
#include "ContactItem.h"
#include "DebugLogWindow.h"
#include "LoginWindow.h"
#include "MapView.h"
#include "MissionControlWindow.h"
#include "MqttClient.h"
#include "MqttLogWindow.h"
#include "NetworkMapWindow.h"
#include "PacketAnalyzerWindow.h"
#include "ProfileWindow.h"
#include "RepeaterMonitorView.h"
#include "SerialMonitorWindow.h"
#include "NotificationManager.h"
#include "ProtocolHandler.h"
#include "SarMarker.h"
#include "SerialHandler.h"
#include "SettingsWindow.h"
#include "StatsWindow.h"
#include "TelemetryWindow.h"
#include "TopBarView.h"
#include "TracePathWindow.h"
#include "Utils.h"
#include "AudioEngine.h"
#include "VoiceCodec.h"
#include "VoiceSession.h"


// MainWindow-private message codes
enum {
	MSG_SHOW_TRACE_PATH = 'shtp',
	MSG_AUTO_CONNECT = 'autc',
	MSG_STATS_TIMER = 'sttm',
	MSG_UPDATE_CONTACTS = 'upct',
	MSG_SHOW_MQTT_SETTINGS = 'mqst',
	MSG_SHOW_DEVICE_SETTINGS = 'dvst',
	MSG_SHOW_LOGIN = 'lgn!',
	MSG_SHOW_EXPORT_CONTACT = 'expc',
	MSG_SHOW_IMPORT_CONTACT = 'impc',
	MSG_REQUEST_TELEMETRY = 'rqtl',
	MSG_POST_CONNECT_INIT = 'pcin',
	MSG_TOGGLE_SIDEBAR = 'tgsb',
	MSG_TOGGLE_INFO_PANEL = 'tgip',
	MSG_CONTACT_FILTER = 'cflt',
	MSG_TYPE_FILTER = 'tyfl',
	MSG_SEARCH_MESSAGES = 'srch',
	MSG_SEARCH_QUERY = 'srqy',
	MSG_SEARCH_CLOSE = 'srcl',
	MSG_AUTO_SYNC_CONTACTS = 'asnc',
	MSG_CONTACT_CONTEXT = 'cctx',
	MSG_CONTACT_REMOVE = 'crmv',
	MSG_CONTACT_RESET_PATH = 'crsp',
	MSG_ADD_CHANNEL = 'achn',
	MSG_REMOVE_CHANNEL = 'rmch',
	MSG_CREATE_CHANNEL = 'crcn',	// From AddChannelWindow
	MSG_HANDSHAKE_TIMEOUT = 'hstm',
	MSG_TRACE_ALL = 'tral',
	MSG_TRACE_ALL_NEXT = 'trn!',

	// Delivery check timer
	MSG_DELIVERY_CHECK_TICK = 'dlck',

	// Admin toolbar buttons (in chat area)
	kMsgAdminVersion = 'avrs',
	kMsgAdminNeighbors = 'anbr',
	kMsgAdminClock = 'aclk',
	kMsgAdminClearStats = 'acls',
	kMsgAdminReboot = 'arbc',
	kMsgAdminFactoryReset = 'afrs',
	kMsgAdminSetName = 'asnm',
	kMsgAdminSetPwd = 'aspw',
};

// Timer intervals
static const bigtime_t kAutoConnectDelay = 1000000;     // 1 second
static const bigtime_t kStatsRefreshInterval = 10000000; // 10 seconds
static const bigtime_t kAutoSyncDelay = 3000000;         // 3 seconds
static const bigtime_t kAdminRefreshInterval = 15000000; // 15 seconds
static const bigtime_t kHandshakeTimeout = 5000000;      // 5 seconds
static const bigtime_t kDeliveryCheckInterval = 10000000; // 10 seconds

// Delivery retry constants
static const bigtime_t kSendTimeout = 60000000;          // 60 seconds for RSP_SENT
static const bigtime_t kConfirmCleanup = 120000000;      // 2 minutes: remove SENT from queue
static const int kMaxRetryAttempts = 3;
static const int kMaxSimultaneousPending = 3;


// Thin BListView subclass to detect right-clicks
class ContactListView : public BListView {
public:
	ContactListView(const char* name)
		:
		BListView(name)
	{
	}

	virtual void MouseDown(BPoint where)
	{
		BMessage* current = Window()->CurrentMessage();
		int32 buttons = 0;
		if (current != NULL)
			current->FindInt32("buttons", &buttons);

		if (buttons & B_SECONDARY_MOUSE_BUTTON) {
			int32 index = IndexOf(where);
			if (index >= 0) {
				BPoint screenPt = where;
				ConvertToScreen(&screenPt);
				BMessage msg(MSG_CONTACT_CONTEXT);
				msg.AddInt32("index", index);
				msg.AddPoint("where", screenPt);
				Window()->PostMessage(&msg);
			}
			return;
		}

		BListView::MouseDown(where);
	}
};


static void
_ShowWindow(BWindow* window)
{
	if (window->LockLooper()) {
		if (window->IsHidden())
			window->Show();
		window->Activate();
		window->UnlockLooper();
	}
}


// Thread-safe: lock window looper, then check visibility.
// Returns true if window is visible and locked (caller must UnlockLooper).
static bool
_LockIfVisible(BWindow* window)
{
	if (window == NULL)
		return false;
	if (!window->LockLooper())
		return false;
	if (window->IsHidden()) {
		window->UnlockLooper();
		return false;
	}
	return true;
}

MainWindow::MainWindow()
	:
	BWindow(BRect(0, 0, 729, 489), APP_NAME,
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fSerialHandler(NULL),
	fMenuBar(NULL),
	fTopBar(NULL),
	fConnectItem(NULL),
	fDisconnectItem(NULL),
	fSearchField(NULL),
	fShowChats(NULL),
	fShowRepeaters(NULL),
	fShowRooms(NULL),
	fContactList(NULL),
	fContactScroll(NULL),
	fChannelItem(NULL),
	fSidebarDeviceLabel(NULL),
	fChatHeader(NULL),
	fChatView(NULL),
	fChatScroll(NULL),
	fMessageInput(NULL),
	fSendButton(NULL),
	fCharCounter(NULL),
	fSearchBar(NULL),
	fMsgSearchField(NULL),
	fSearchCloseButton(NULL),
	fSearchActive(false),
	fAdminToolbar(NULL),
	fAdminVersionBtn(NULL),
	fAdminNeighborsBtn(NULL),
	fAdminClockBtn(NULL),
	fAdminClearStatsBtn(NULL),
	fAdminRebootBtn(NULL),
	fAdminFactoryResetBtn(NULL),
	fAdminSetNameBtn(NULL),
	fAdminSetPwdBtn(NULL),
	fAdminNameField(NULL),
	fAdminPwdField(NULL),
	fInfoPanel(NULL),
	fMainSplit(NULL),
	fCardView(NULL),
	fRepeaterMonitorView(NULL),
	fDeviceAdvType(0),
	fSelectedPreset(PRESET_EU_UK_NARROW),
	fSelectedPort(""),
	fSelectedContact(-1),
	fConnected(false),
	fContacts(20),
	fOldContacts(20),
	fSyncingContacts(false),
	fSyncingMessages(false),
	fChannelMessages(50),
	fChannels(10),
	fMaxChannels(0),
	fChannelEnumIndex(0),
	fEnumeratingChannels(false),
	fSelectedChannelIdx(-1),
	fPendingMessages(10),
	fDeliveryCheckTimer(NULL),
	fSendingToChannel(false),
	fLoginPending(false),
	fLoggedIn(false),
	fSettingsWindow(NULL),
	fStatsWindow(NULL),
	fTracePathWindow(NULL),
	fNetworkMapWindow(NULL),
	fTelemetryWindow(NULL),
	fLoginWindow(NULL),
	fMapWindow(NULL),
	fContactExportWindow(NULL),
	fPacketAnalyzerWindow(NULL),
	fMqttLogWindow(NULL),
	fMissionControlWindow(NULL),
	fProfileWindow(NULL),
	fSerialMonitorWindow(NULL),
	fGifPickerWindow(NULL),
	fMqttClient(NULL),
	fRawPacketCount(0),
	fLastRawPacketTime(0),
	fAutoConnectTimer(NULL),
	fStatsRefreshTimer(NULL),
	fAutoSyncRunner(NULL),
	fAdminRefreshTimer(NULL),
	fTelemetryPollTimer(NULL),
	fHandshakeTimer(NULL),
	fRepeaterMapTimer(NULL),
	fBatteryType(BATTERY_LIPO),
	fBatteryMv(0),
	fLastRssi(0),
	fLastSnr(0),
	fNoiseFloor(0),
	fTxPackets(0),
	fRxPackets(0),
	fDeviceUptime(0),
	fVoiceSessions(NULL),
	fAudioEngine(NULL),
	fVoiceButton(NULL),
	fVoiceFragmentTimer(NULL),
	fCurrentVoiceSendSession(0),
	fCurrentVoiceSendIndex(0),
	fRecordingVoice(false),
	fVoicePlayPcm(NULL),
	fVoicePlayPcmSize(0),
	fImageSessions(NULL),
	fImageOpenPanel(NULL),
	fAttachButton(NULL),
	fGifButton(NULL),
	fImageFragmentTimer(NULL),
	fImageExpireTimer(NULL),
	fCurrentSendSession(0),
	fCurrentSendIndex(0)
{
	// Initialize device info
	memset(fDeviceName, 0, sizeof(fDeviceName));
	memset(fDeviceFirmware, 0, sizeof(fDeviceFirmware));
	memset(fDeviceBoard, 0, sizeof(fDeviceBoard));
	memset(fPublicKey, 0, sizeof(fPublicKey));
	fSelfNodeId = 0;
	memset(fLoginTargetKey, 0, sizeof(fLoginTargetKey));
	fTelemetryPollIndex = 0;
	fGpxSavePanel = NULL;
	fPingStartTime = 0;
	fPingPending = false;
	fPingTimeoutRunner = NULL;
	memset(fPingTargetKey, 0, sizeof(fPingTargetKey));
	fPingAllActive = false;
	fPingAllIndex = 0;
	fPingAllTotal = 0;
	fPingAllResponded = 0;
	memset(fLoggedInKey, 0, sizeof(fLoggedInKey));
	fHasDeviceInfo = false;
	fImageSessions = new ImageSessionManager();
	fVoiceSessions = new VoiceSessionManager();
	fAudioEngine = new AudioEngine();
	fRadioFreq = 0;
	fRadioBw = 0;
	fRadioSf = 0;
	fRadioCr = 0;
	fRadioTxPower = 0;
	fHasRadioParams = false;
	fMultiAcks = 0;
	fAdvertLocPolicy = 0;
	fTelemetryModes = 0;
	fManualAddContacts = 0;
	fAutoSyncRunner = NULL;
	_BuildUI();

	// Initialize SQLite database
	BString settingsDir = _GetSettingsPath();
	if (!settingsDir.IsEmpty()) {
		DatabaseManager::Instance()->Open(settingsDir.String());
	}

	// Create serial handler and protocol handler
	fSerialHandler = new SerialHandler(this);
	fSerialHandler->Run();
	fProtocol = new ProtocolHandler(fSerialHandler);

	// MQTT client is created lazily when needed
	fprintf(stderr, "[MainWindow] MQTT will be initialized on demand\n");
	fMqttClient = NULL;
	_LoadMqttSettings();  // Just load settings, don't create client yet
	fTopBar->SetMqttEnabled(fMqttSettings.enabled);

	// Load device settings (battery type, etc.)
	_LoadDeviceSettings();
	fTopBar->SetBatteryType(fBatteryType);

	// Load UI settings (contact filter checkboxes, etc.)
	_LoadUISettings();

	// Load muted keys and contact groups from database
	DatabaseManager::Instance()->LoadAllMuted(&fMutedKeys);
	_LoadContactGroups();

	fprintf(stderr, "[MainWindow] Refreshing ports...\n");

	// Refresh available ports
	_RefreshPorts();
	fprintf(stderr, "[MainWindow] Ports refreshed\n");

	// Load contacts from Haiku People files
	fprintf(stderr, "[MainWindow] Loading people contacts...\n");
	_LoadPeopleContacts();
	fprintf(stderr, "[MainWindow] People contacts loaded\n");

	SetSizeLimits(500, B_SIZE_UNLIMITED, 350, B_SIZE_UNLIMITED);
	CenterOnScreen();

	// Start auto-connect timer (will try to connect after 1 second)
	BMessage autoConnectMsg(MSG_AUTO_CONNECT);
	fAutoConnectTimer = new BMessageRunner(this, &autoConnectMsg, kAutoConnectDelay, 1);
}


MainWindow::~MainWindow()
{
	// Stop timers
	delete fAutoConnectTimer;
	delete fStatsRefreshTimer;
	delete fAutoSyncRunner;
	delete fHandshakeTimer;
	delete fRepeaterMapTimer;
	delete fPingTimeoutRunner;
	delete fDeliveryCheckTimer;

	// Protocol handler cleanup
	delete fProtocol;
	fProtocol = NULL;

	// Serial handler cleanup (if not already done in QuitRequested)
	if (fSerialHandler != NULL) {
		if (fSerialHandler->IsConnected())
			fSerialHandler->Disconnect();
		fSerialHandler->Lock();
		fSerialHandler->Quit();
		fSerialHandler = NULL;
	}

	// MQTT client cleanup
	if (fMqttClient != NULL) {
		fMqttClient->Disconnect();
		fMqttClient->Lock();
		fMqttClient->Quit();
		fMqttClient = NULL;
	}

	delete fGpxSavePanel;

	// Voice message cleanup
	delete fVoiceSessions;
	delete fAudioEngine;
	delete fVoiceFragmentTimer;
	delete[] fVoicePlayPcm;

	// Image sharing cleanup
	delete fImageSessions;
	delete fImageOpenPanel;
	delete fImageFragmentTimer;
	delete fImageExpireTimer;

	// Child windows and singletons are destroyed in QuitRequested()
}


void
MainWindow::_BuildMenuBar()
{
	// === Connection menu ===
	BMenu* connectionMenu = new BMenu("Connection");
	fConnectItem = new BMenuItem("Connect" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SERIAL_CONNECT), 'O');
	connectionMenu->AddItem(fConnectItem);
	fDisconnectItem = new BMenuItem("Disconnect",
		new BMessage(MSG_SERIAL_DISCONNECT), 'D');
	fDisconnectItem->SetEnabled(false);
	connectionMenu->AddItem(fDisconnectItem);
	connectionMenu->AddSeparatorItem();
	connectionMenu->AddItem(new BMenuItem("Quit",
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	connectionMenu->SetTargetForItems(this);
	fMenuBar->AddItem(connectionMenu);

	// === Device menu ===
	BMenu* deviceMenu = new BMenu("Device");
	deviceMenu->AddItem(new BMenuItem("Sync Contacts",
		new BMessage(MSG_SYNC_CONTACTS), 'R'));
	deviceMenu->AddItem(new BMenuItem("Send Advertisement",
		new BMessage(MSG_SEND_ADVERT), 'A'));
	deviceMenu->AddItem(new BMenuItem("Send SAR Marker" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SEND_SAR_MARKER)));
	deviceMenu->AddItem(new BMenuItem("Trace Path",
		new BMessage(MSG_SHOW_TRACE_PATH), 'T'));
	deviceMenu->AddItem(new BMenuItem("Ping All Contacts",
		new BMessage(MSG_PING_ALL)));
	deviceMenu->AddSeparatorItem();
	deviceMenu->AddItem(new BMenuItem("Login to Repeater/Room" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SHOW_LOGIN)));
	deviceMenu->AddItem(new BMenuItem("Request Telemetry",
		new BMessage(MSG_REQUEST_TELEMETRY)));
	deviceMenu->AddSeparatorItem();
	deviceMenu->AddItem(new BMenuItem("Device Info",
		new BMessage(MSG_DEVICE_QUERY)));
	deviceMenu->AddItem(new BMenuItem("Battery Status",
		new BMessage(MSG_GET_BATTERY)));
	deviceMenu->AddItem(new BMenuItem("Statistics",
		new BMessage(MSG_GET_STATS)));
	deviceMenu->SetTargetForItems(this);
	fMenuBar->AddItem(deviceMenu);

	// === Contacts menu ===
	BMenu* contactMenu = new BMenu("Contacts");
	contactMenu->AddItem(new BMenuItem("Export Contact" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SHOW_EXPORT_CONTACT)));
	contactMenu->AddItem(new BMenuItem("Import Contact" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SHOW_IMPORT_CONTACT)));
	contactMenu->AddSeparatorItem();
	contactMenu->AddItem(new BMenuItem("Export GPX" B_UTF8_ELLIPSIS,
		new BMessage(MSG_EXPORT_GPX)));
	contactMenu->AddSeparatorItem();
	contactMenu->AddItem(new BMenuItem("Export/Import Profile" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SHOW_PROFILE)));
	contactMenu->SetTargetForItems(this);
	fMenuBar->AddItem(contactMenu);

	// === View menu ===
	BMenu* viewMenu = new BMenu("View");
	viewMenu->AddItem(new BMenuItem("Search Messages",
		new BMessage(MSG_SEARCH_MESSAGES), 'F'));
	viewMenu->AddSeparatorItem();
	viewMenu->AddItem(new BMenuItem("Toggle Sidebar",
		new BMessage(MSG_TOGGLE_SIDEBAR), 'B'));
	viewMenu->AddItem(new BMenuItem("Toggle Info Panel",
		new BMessage(MSG_TOGGLE_INFO_PANEL), 'I'));
	viewMenu->AddSeparatorItem();
	viewMenu->AddItem(new BMenuItem("Debug Log",
		new BMessage(MSG_SHOW_DEBUG_LOG), 'L'));
	viewMenu->AddItem(new BMenuItem("Statistics",
		new BMessage(MSG_SHOW_STATS), 'S'));
	viewMenu->AddItem(new BMenuItem("Network Map",
		new BMessage(MSG_SHOW_NETWORK_MAP), 'M'));
	viewMenu->AddItem(new BMenuItem("Geographic Map",
		new BMessage(MSG_SHOW_MAP), 'G'));
	viewMenu->AddItem(new BMenuItem("Sensor Telemetry",
		new BMessage(MSG_SHOW_TELEMETRY), 'Y'));
	viewMenu->AddItem(new BMenuItem("Packet Analyzer",
		new BMessage(MSG_SHOW_PACKET_ANALYZER), 'P', B_SHIFT_KEY));
	viewMenu->AddItem(new BMenuItem("Mission Control",
		new BMessage(MSG_SHOW_MISSION_CONTROL), 'D', B_SHIFT_KEY));
	viewMenu->AddItem(new BMenuItem("Serial Monitor",
		new BMessage(MSG_SHOW_SERIAL_MONITOR), 'S', B_SHIFT_KEY));
	viewMenu->AddItem(new BMenuItem("Repeater Monitor",
		new BMessage(MSG_SHOW_REPEATER_MONITOR), 'R', B_SHIFT_KEY));
	viewMenu->AddSeparatorItem();
	viewMenu->AddItem(new BMenuItem("MQTT Log",
		new BMessage(MSG_SHOW_MQTT_LOG), 'M', B_SHIFT_KEY));
	viewMenu->SetTargetForItems(this);
	fMenuBar->AddItem(viewMenu);

	// === Settings menu ===
	BMenu* settingsMenu = new BMenu("Settings");
	settingsMenu->AddItem(new BMenuItem("Device & Radio" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SHOW_DEVICE_SETTINGS)));
	settingsMenu->AddItem(new BMenuItem("MQTT Settings" B_UTF8_ELLIPSIS,
		new BMessage(MSG_SHOW_MQTT_SETTINGS)));
	settingsMenu->AddSeparatorItem();
	settingsMenu->AddItem(new BMenuItem("Show in Deskbar",
		new BMessage(MSG_INSTALL_DESKBAR)));
	settingsMenu->AddItem(new BMenuItem("Remove from Deskbar",
		new BMessage(MSG_REMOVE_DESKBAR)));
	settingsMenu->SetTargetForItems(this);
	fMenuBar->AddItem(settingsMenu);
}


void
MainWindow::_BuildUI()
{
	// === LEFT SIDEBAR: Contact List ===

	// Search/filter field
	fSearchField = new BTextControl("search", NULL, "",
		new BMessage(MSG_CONTACT_FILTER));
	fSearchField->SetModificationMessage(new BMessage(MSG_CONTACT_FILTER));
	fSearchField->TextView()->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));

	// Type filter checkboxes
	fShowChats = new BCheckBox("show_chats", "Chat",
		new BMessage(MSG_TYPE_FILTER));
	fShowChats->SetValue(B_CONTROL_ON);
	fShowRepeaters = new BCheckBox("show_repeaters", "Repeater",
		new BMessage(MSG_TYPE_FILTER));
	fShowRepeaters->SetValue(B_CONTROL_ON);
	fShowRooms = new BCheckBox("show_rooms", "Room",
		new BMessage(MSG_TYPE_FILTER));
	fShowRooms->SetValue(B_CONTROL_ON);

	fContactList = new ContactListView("contacts");
	fContactList->SetSelectionMessage(new BMessage(MSG_CONTACT_SELECTED));

	// Add Public Channel as first item
	fChannelItem = new ContactItem("Public Channel", true);
	fContactList->AddItem(fChannelItem);

	fContactScroll = new BScrollView("contact_scroll", fContactList,
		B_WILL_DRAW | B_FRAME_EVENTS, false, true, B_PLAIN_BORDER);

	// Device info label at sidebar footer
	fSidebarDeviceLabel = new BStringView("device_label", "");
	BFont smallFont(be_plain_font);
	smallFont.SetSize(smallFont.Size() * 0.85f);
	fSidebarDeviceLabel->SetFont(&smallFont);
	fSidebarDeviceLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, B_LIGHTEN_1_TINT);
	fSidebarDeviceLabel->SetAlignment(B_ALIGN_CENTER);
	fSidebarDeviceLabel->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	// Sidebar with search + filters + contact list + device footer
	BView* sidebar = new BView("sidebar", 0);
	sidebar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	sidebar->SetExplicitMinSize(BSize(180, B_SIZE_UNSET));
	sidebar->SetExplicitPreferredSize(BSize(220, B_SIZE_UNSET));
	BLayoutBuilder::Group<>(sidebar, B_VERTICAL, 0)
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(4, 4, 4, 4)
			.Add(fSearchField, 1.0)
		.End()
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(6, 0, 6, 2)
			.Add(fShowChats)
			.Add(fShowRepeaters)
			.Add(fShowRooms)
		.End()
		.Add(fContactScroll, 1.0)
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(4, 2, 4, 4)
			.Add(fSidebarDeviceLabel)
		.End()
	.End();

	// === RIGHT SIDE: Chat Area ===

	// Chat header
	fChatHeader = new ChatHeaderView("chat_header");

	// Chat view
	fChatView = new ChatView("chat");
	fChatScroll = new BScrollView("chat_scroll", fChatView,
		B_WILL_DRAW | B_FRAME_EVENTS, false, true, B_PLAIN_BORDER);

	// Message input area (auto-growing, Enter to send)
	fMessageInput = new GrowingTextView("message",
		new BMessage(MSG_SEND_MESSAGE));
	fMessageInput->SetEnabled(false);
	BScrollView* inputScroll = new BScrollView("input_scroll",
		fMessageInput, B_WILL_DRAW, false, true, B_PLAIN_BORDER);

	fSendButton = new BButton("send", "Send",
		new BMessage(MSG_SEND_MESSAGE));
	fSendButton->SetEnabled(false);

	// Character counter (shows remaining chars)
	fCharCounter = new BStringView("char_counter", "0/160");
	fCharCounter->SetAlignment(B_ALIGN_CENTER);
	fCharCounter->SetExplicitMinSize(BSize(48, B_SIZE_UNSET));
	{
		BFont counterFont(be_plain_font);
		counterFont.SetSize(be_plain_font->Size() * 0.85);
		fCharCounter->SetFont(&counterFont);
	}
	fCharCounter->SetHighUIColor(B_PANEL_TEXT_COLOR);

	fMessageInput->SetModificationMessage(new BMessage(MSG_INPUT_MODIFIED));

	// Image attach button
	fAttachButton = new BButton("attach", "Img",
		new BMessage(MSG_SELECT_IMAGE));
	fAttachButton->SetEnabled(false);

	// GIF button
	fGifButton = new BButton("gif", "GIF",
		new BMessage(MSG_SELECT_GIF));
	fGifButton->SetEnabled(false);

	// Voice record button
	fVoiceButton = new BButton("voice", "Mic",
		new BMessage(MSG_VOICE_RECORD));
	fVoiceButton->SetEnabled(false);

	// Input bar: [attach] [gif] [mic] [input] [counter] [send]
	BView* inputBar = new BView("input_bar", 0);
	inputBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(inputBar, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(fAttachButton)
		.Add(fGifButton)
		.Add(fVoiceButton)
		.Add(inputScroll, 1.0)
		.Add(fCharCounter)
		.Add(fSendButton)
	.End();

	// Message search bar (hidden by default)
	fSearchBar = new BView("search_bar", 0);
	fSearchBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fMsgSearchField = new BTextControl("msg_search", "Find:", "",
		new BMessage(MSG_SEARCH_QUERY));
	fMsgSearchField->SetModificationMessage(new BMessage(MSG_SEARCH_QUERY));
	fMsgSearchField->TextView()->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));
	fMsgSearchField->SetToolTip("Search all messages (Cmd+F)");
	fSearchCloseButton = new BButton("close_search", "\xC3\x97",
		new BMessage(MSG_SEARCH_CLOSE));
	fSearchCloseButton->SetExplicitSize(BSize(24, 24));
	BLayoutBuilder::Group<>(fSearchBar, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.SetInsets(4, 2, 4, 2)
		.Add(fMsgSearchField, 1.0)
		.Add(fSearchCloseButton)
	.End();

	// Admin toolbar (hidden by default, shown during admin session)
	fAdminToolbar = new BView("admin_toolbar", 0);
	fAdminToolbar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Row 1: query/action buttons
	fAdminVersionBtn = new BButton("admin_ver", "Version",
		new BMessage(kMsgAdminVersion));
	fAdminNeighborsBtn = new BButton("admin_nbr", "Neighbors",
		new BMessage(kMsgAdminNeighbors));
	fAdminClockBtn = new BButton("admin_clk", "Clock",
		new BMessage(kMsgAdminClock));
	fAdminClearStatsBtn = new BButton("admin_cls", "Clear Stats",
		new BMessage(kMsgAdminClearStats));
	fAdminRebootBtn = new BButton("admin_reb", "Reboot",
		new BMessage(kMsgAdminReboot));
	fAdminFactoryResetBtn = new BButton("admin_frs", "Factory Reset",
		new BMessage(kMsgAdminFactoryReset));

	// Row 2: name/password fields
	fAdminNameField = new BTextControl("admin_name", "Name:", "", NULL);
	fAdminSetNameBtn = new BButton("admin_sn", "Set",
		new BMessage(kMsgAdminSetName));
	fAdminPwdField = new BTextControl("admin_pwd", "Pwd:", "", NULL);
	fAdminSetPwdBtn = new BButton("admin_sp", "Set",
		new BMessage(kMsgAdminSetPwd));

	BLayoutBuilder::Group<>(fAdminToolbar, B_VERTICAL, 1)
		.SetInsets(B_USE_SMALL_SPACING, 2, B_USE_SMALL_SPACING, 2)
		.AddGroup(B_HORIZONTAL, 2)
			.Add(fAdminVersionBtn)
			.Add(fAdminNeighborsBtn)
			.Add(fAdminClockBtn)
			.Add(fAdminClearStatsBtn)
		.End()
		.AddGroup(B_HORIZONTAL, 2)
			.Add(fAdminNameField, 1.0)
			.Add(fAdminSetNameBtn)
			.AddStrut(4)
			.Add(fAdminPwdField, 1.0)
			.Add(fAdminSetPwdBtn)
		.End()
		.AddGroup(B_HORIZONTAL, 2)
			.Add(fAdminRebootBtn)
			.Add(fAdminFactoryResetBtn)
			.AddGlue()
		.End()
	.End();

	// Chat panel (header + search bar + messages + admin toolbar + input)
	BView* chatPanel = new BView("chat_panel", 0);
	chatPanel->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(chatPanel, B_VERTICAL, 0)
		.Add(fChatHeader)
		.Add(fSearchBar)
		.Add(fChatScroll, 1.0)
		.Add(fAdminToolbar)
		.Add(inputBar)
	.End();

	// Hide search bar and admin toolbar initially
	fSearchBar->Hide();
	fAdminToolbar->Hide();

	// Hide chat header initially — info panel shows the contact name
	fChatHeader->Hide();

	// === RIGHT: Contact Info Panel ===
	fInfoPanel = new ContactInfoPanel("info_panel");

	// === MAIN SPLIT VIEW ===
	fMainSplit = new BSplitView(B_HORIZONTAL);
	fMainSplit->AddChild(sidebar, 0.28);
	fMainSplit->AddChild(chatPanel, 0.52);
	fMainSplit->AddChild(fInfoPanel, 0.20);
	fMainSplit->SetCollapsible(0, true);
	fMainSplit->SetCollapsible(1, false);
	fMainSplit->SetCollapsible(2, true);

	// === REPEATER MONITOR VIEW ===
	fRepeaterMonitorView = new RepeaterMonitorView(this);

	// === CARD VIEW (chat mode / repeater mode) ===
	fCardView = new BCardView("mode_card");
	fCardView->AddChild(fMainSplit);           // Card 0: chat
	fCardView->AddChild(fRepeaterMonitorView); // Card 1: repeater
	fCardView->CardLayout()->SetVisibleItem((int32)0);

	// === MENU BAR + STATUS BAR ===
	fMenuBar = new BMenuBar("menubar");
	_BuildMenuBar();
	fTopBar = new TopBarView("top_bar");

	// === MAIN LAYOUT ===
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fTopBar)
		.Add(fCardView, 1.0)
	.End();

	// Set admin toolbar button targets to this window
	fAdminVersionBtn->SetTarget(this);
	fAdminNeighborsBtn->SetTarget(this);
	fAdminClockBtn->SetTarget(this);
	fAdminClearStatsBtn->SetTarget(this);
	fAdminRebootBtn->SetTarget(this);
	fAdminFactoryResetBtn->SetTarget(this);
	fAdminSetNameBtn->SetTarget(this);
	fAdminSetPwdBtn->SetTarget(this);
}


void
MainWindow::_RefreshPorts()
{
	BMessage ports;
	if (SerialHandler::ListPorts(&ports) == B_OK) {
		const char* port;
		if (ports.FindString(kFieldPort, 0, &port) == B_OK) {
			fSelectedPort = port;
		}
	}
}


void
MainWindow::_UpdateConnectionUI()
{
	fConnectItem->SetEnabled(!fConnected);
	fDisconnectItem->SetEnabled(fConnected);

	if (fConnected) {
		fTopBar->SetConnected(true, fSelectedPort.String());
	} else {
		fTopBar->SetConnected(false);
		fMessageInput->SetEnabled(false);
		fSendButton->SetEnabled(false);
		fAttachButton->SetEnabled(false);
		fGifButton->SetEnabled(false);
		fVoiceButton->SetEnabled(false);
	}
}


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_KEY_DOWN:
		{
			const char* bytes;
			if (message->FindString("bytes", &bytes) == B_OK
				&& bytes[0] == B_ESCAPE && fSearchActive) {
				_CloseSearch();
				break;
			}
			BWindow::MessageReceived(message);
			break;
		}

		case MSG_SERIAL_CONNECT:
		{
			// Auto-select first port if none selected
			if (fSelectedPort.IsEmpty())
				_RefreshPorts();
			if (!fSelectedPort.IsEmpty())
				_Connect();
			else
				_LogMessage("ERROR", "No serial port available");
			break;
		}

		case MSG_SERIAL_DISCONNECT:
			_Disconnect();
			break;

		case MSG_SERIAL_CONNECTED:
			_OnConnected(message);
			break;

		case MSG_SERIAL_DISCONNECTED:
			_OnDisconnected();
			break;

		case MSG_SERIAL_ERROR:
			_OnError(message);
			break;

		case MSG_FRAME_RECEIVED:
			_OnFrameReceived(message);
			break;

		case MSG_FRAME_SENT:
			_OnFrameSent(message);
			break;

		case MSG_RAW_SERIAL_DATA:
		{
			const char* line;
			if (message->FindString("line", &line) == B_OK) {
				_LogMessage("SERIAL", line);
				// Forward to Serial Monitor window
				if (fSerialMonitorWindow != NULL
					&& fSerialMonitorWindow->LockLooper()) {
					fSerialMonitorWindow->AppendOutput(line);
					fSerialMonitorWindow->UnlockLooper();
				}
				// Forward to Repeater Monitor view (embedded, same looper)
				if (fRepeaterMonitorView != NULL)
					fRepeaterMonitorView->ProcessLine(line);
			}
			break;
		}

		case MSG_SYNC_CONTACTS:
			fSyncingContacts = true;
			fProtocol->SendGetContacts();
			break;

		case MSG_SEND_ADVERT:
			fProtocol->SendSelfAdvert();
			break;

		case MSG_DEVICE_QUERY:
		{
			if (!fHasDeviceInfo) {
				fProtocol->SendDeviceQuery();
				break;
			}

			BString info;
			info << "Name: " << (fDeviceName[0] ? fDeviceName : "Unknown") << "\n";
			info << "Board: " << (fDeviceBoard[0] ? fDeviceBoard : "Unknown") << "\n";
			info << "Firmware: " << (fDeviceFirmware[0] ? fDeviceFirmware : "Unknown") << "\n";

			if (fRadioFreq > 0) {
				float freqMHz = fRadioFreq / 1000.0f;
				info << BString().SetToFormat("\nFrequency: %.3f MHz\n", freqMHz);
				if (fRadioBw > 0) {
					if (fRadioBw >= 1000000)
						info << BString().SetToFormat("Bandwidth: %.1f MHz\n", fRadioBw / 1000000.0f);
					else
						info << BString().SetToFormat("Bandwidth: %.1f kHz\n", fRadioBw / 1000.0f);
				}
				if (fRadioSf > 0)
					info << BString().SetToFormat("Spreading Factor: SF%d\n", fRadioSf);
				if (fRadioCr > 0)
					info << BString().SetToFormat("Coding Rate: 4/%d\n", fRadioCr);
			}
			if (fRadioTxPower > 0)
				info << BString().SetToFormat("TX Power: %d dBm\n", fRadioTxPower);

			info << BString().SetToFormat("\nMax Channels: %d\n", fMaxChannels);

			if (fPublicKey[0] != '\0') {
				info << "\nPublic Key:\n";
				info.Append(fPublicKey, 32);
				info << "\n";
				info.Append(fPublicKey + 32, 32);
				info << "\n";
			}

			BAlert* alert = new BAlert("Device Info", info.String(),
				"Copy Key", "OK", NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
			alert->SetShortcut(1, B_ESCAPE);
			int32 result = alert->Go();
			if (result == 0 && fPublicKey[0] != '\0') {
				if (be_clipboard->Lock()) {
					be_clipboard->Clear();
					BMessage* clip = be_clipboard->Data();
					clip->AddData("text/plain", B_MIME_TYPE,
						fPublicKey, strlen(fPublicKey));
					be_clipboard->Commit();
					be_clipboard->Unlock();
				}
			}
			break;
		}

		case MSG_GET_BATTERY:
		{
			fProtocol->SendGetBattery();

			if (fBatteryMv > 0) {
				int32 pct = BatteryPercent(fBatteryMv,
					(BatteryChemistry)fBatteryType);

				const char* chemName = (fBatteryType < BATTERY_CHEMISTRY_COUNT)
					? kBatteryChemistryNames[fBatteryType] : "Unknown";
				BString info;
				info << BString().SetToFormat("Voltage: %u mV\n", fBatteryMv);
				info << BString().SetToFormat("Estimated: %d%%\n", (int)pct);

				const char* level;
				if (pct >= 75) level = "Good";
				else if (pct >= 40) level = "Fair";
				else if (pct >= 15) level = "Low";
				else level = "Critical";
				info << "Status: " << level << "\n";
				info << "Type: " << chemName << "\n";

				BAlert* alert = new BAlert("Battery Status",
					info.String(), "OK");
				alert->SetShortcut(0, B_ESCAPE);
				alert->Go(NULL);
			}
			break;
		}

		case MSG_GET_STATS:
			// Open stats window and request fresh data
			if (fStatsWindow == NULL) {
				fStatsWindow = new StatsWindow(this);
				fStatsWindow->Show();
			} else {
				_ShowWindow(fStatsWindow);
			}
			fProtocol->SendGetStats();
			break;

		case MSG_REQUEST_STATS_DATA:
			// Just request stats data without opening/showing window
			fProtocol->SendGetStats();
			break;

		case MSG_CONTACT_SELECTED:
		{
			int32 index = fContactList->CurrentSelection();
			if (index >= 0)
				_SelectContact(index);
			break;
		}

		case MSG_SEND_MESSAGE:
		{
			const char* text = fMessageInput->Text();
			if (text != NULL && text[0] != '\0') {
				_SendTextMessage(text);
				fMessageInput->SetText("");
				fMessageInput->MakeFocus(true);
			}
			break;
		}

		case MSG_INPUT_MODIFIED:
			_UpdateCharCounter();
			break;

		case MSG_CONTACT_FILTER:
		case MSG_TYPE_FILTER:
		{
			const char* filter = fSearchField->Text();
			_FilterContacts(filter);
			if (message->what == MSG_TYPE_FILTER)
				_SaveUISettings();
			break;
		}

		case MSG_SEARCH_MESSAGES:
			_ToggleSearchBar();
			break;

		case MSG_SEARCH_QUERY:
		{
			const char* query = fMsgSearchField->Text();
			if (query != NULL && query[0] != '\0')
				_PerformSearch(query);
			else
				_CloseSearch();
			break;
		}

		case MSG_SEARCH_CLOSE:
			_CloseSearch();
			break;

		case MSG_TOGGLE_SIDEBAR:
		{
			bool collapsed = fMainSplit->IsItemCollapsed(0);
			fMainSplit->SetItemCollapsed(0, !collapsed);
			break;
		}

		case MSG_TOGGLE_INFO_PANEL:
		{
			int32 infoIndex = fMainSplit->CountChildren() - 1;
			bool collapsed = fMainSplit->IsItemCollapsed(infoIndex);
			fMainSplit->SetItemCollapsed(infoIndex, !collapsed);
			// Hide chat header when info panel is visible (avoids duplicate name)
			if (collapsed) {
				// Info panel is being opened
				if (!fChatHeader->IsHidden())
					fChatHeader->Hide();
			} else {
				// Info panel is being closed
				if (fChatHeader->IsHidden())
					fChatHeader->Show();
			}
			break;
		}

		case MSG_SHOW_DEBUG_LOG:
			DebugLogWindow::ShowWindow();
			break;

		case MSG_SHOW_STATS:
		{
			if (fStatsWindow == NULL) {
				fStatsWindow = new StatsWindow(this);
				fStatsWindow->Show();
			} else {
				_ShowWindow(fStatsWindow);
			}
			// Request fresh stats
			fProtocol->SendGetStats();
			break;
		}

		case MSG_SHOW_NETWORK_MAP:
		{
			if (fNetworkMapWindow == NULL) {
				fNetworkMapWindow = new NetworkMapWindow(this);
				fNetworkMapWindow->Show();
			} else {
				_ShowWindow(fNetworkMapWindow);
			}
			// In repeater mode, use repeater topology instead of
			// companion contacts
			if (fCardView != NULL
				&& fCardView->CardLayout()->VisibleIndex() == 1) {
				_UpdateRepeaterMap();
			} else {
				if (fNetworkMapWindow->LockLooper()) {
					fNetworkMapWindow->UpdateFromContacts(&fContacts);
					fNetworkMapWindow->UnlockLooper();
				}
			}
			break;
		}

		case MSG_UPDATE_MAP_DATA:
		{
			// Update map data without showing/activating the window
			if (fNetworkMapWindow != NULL) {
				if (fCardView != NULL
					&& fCardView->CardLayout()->VisibleIndex() == 1) {
					_UpdateRepeaterMap();
				} else {
					if (fNetworkMapWindow->LockLooper()) {
						fNetworkMapWindow->UpdateFromContacts(&fContacts);
						fNetworkMapWindow->UnlockLooper();
					}
				}
			}
			break;
		}

		case MSG_SHOW_DEVICE_SETTINGS:
		{
			if (fSettingsWindow == NULL) {
				fSettingsWindow = new SettingsWindow(this);
				// Pre-fill with current device info
				if (fDeviceName[0] != '\0')
					fSettingsWindow->SetDeviceName(fDeviceName);
				if (fMqttSettings.latitude != 0.0 || fMqttSettings.longitude != 0.0) {
					fSettingsWindow->SetLatitude(fMqttSettings.latitude);
					fSettingsWindow->SetLongitude(fMqttSettings.longitude);
				}
				if (fHasRadioParams) {
					fSettingsWindow->SetRadioParams(fRadioFreq, fRadioBw,
						fRadioSf, fRadioCr, fRadioTxPower);
				}
				fSettingsWindow->SetMqttSettings(fMqttSettings);
				fSettingsWindow->SetBatteryType(fBatteryType);
				fSettingsWindow->Show();
			} else {
				if (fSettingsWindow->LockLooper()) {
					if (fHasRadioParams) {
						fSettingsWindow->SetRadioParams(fRadioFreq, fRadioBw,
							fRadioSf, fRadioCr, fRadioTxPower);
					}
					fSettingsWindow->SetBatteryType(fBatteryType);
					fSettingsWindow->UnlockLooper();
				}
				_ShowWindow(fSettingsWindow);
			}
			break;
		}

		case MSG_SHOW_MQTT_SETTINGS:
		{
			// Open the unified Settings window with MQTT tab
			if (fSettingsWindow == NULL) {
				fSettingsWindow = new SettingsWindow(this);
				fSettingsWindow->SetMqttSettings(fMqttSettings);
				fSettingsWindow->Show();
			} else {
				if (fSettingsWindow->LockLooper()) {
					fSettingsWindow->SetMqttSettings(fMqttSettings);
					fSettingsWindow->UnlockLooper();
				}
				_ShowWindow(fSettingsWindow);
			}
			break;
		}

		case MSG_INSTALL_DESKBAR:
		{
			BDeskbar deskbar;
			if (!deskbar.HasItem("Sestriere")) {
				entry_ref ref;
				if (be_roster->FindApp(APP_SIGNATURE, &ref) == B_OK) {
					deskbar.AddItem(&ref);
					_LogMessage("INFO", "Added to Deskbar");
				}
			} else {
				_LogMessage("INFO", "Already in Deskbar");
			}
			break;
		}

		case MSG_REMOVE_DESKBAR:
		{
			BDeskbar deskbar;
			if (deskbar.HasItem("Sestriere")) {
				deskbar.RemoveItem("Sestriere");
				_LogMessage("INFO", "Removed from Deskbar");
			}
			break;
		}

		case MSG_BATTERY_TYPE_CHANGED:
		{
			int32 type;
			if (message->FindInt32("type", &type) == B_OK
				&& type >= 0 && type < BATTERY_CHEMISTRY_COUNT) {
				fBatteryType = (uint8)type;
				fTopBar->SetBatteryType(fBatteryType);
				// Update Mission Control if visible
				if (_LockIfVisible(fMissionControlWindow)) {
					fMissionControlWindow->SetBatteryType(fBatteryType);
					fMissionControlWindow->UnlockLooper();
				}
				_SaveDeviceSettings();
				fprintf(stderr, "[MainWindow] Battery type set to %s\n",
					kBatteryChemistryNames[type]);
			}
			break;
		}

		case MSG_MQTT_SETTINGS_CHANGED:
		{
			// Update MQTT settings
			fMqttSettings.enabled = message->GetBool("enabled", false);
			fMqttSettings.latitude = message->GetDouble("latitude", 0.0);
			fMqttSettings.longitude = message->GetDouble("longitude", 0.0);
			strlcpy(fMqttSettings.iataCode, message->GetString("iata", "XXX"),
				sizeof(fMqttSettings.iataCode));
			strlcpy(fMqttSettings.broker, message->GetString("broker", ""),
				sizeof(fMqttSettings.broker));
			fMqttSettings.port = message->GetInt32("port", 1883);
			strlcpy(fMqttSettings.username, message->GetString("username", ""),
				sizeof(fMqttSettings.username));
			strlcpy(fMqttSettings.password, message->GetString("password", ""),
				sizeof(fMqttSettings.password));
			strlcpy(fMqttSettings.publicKey, fPublicKey, sizeof(fMqttSettings.publicKey));

			// Validate settings
			if (fMqttSettings.port < 1 || fMqttSettings.port > 65535)
				fMqttSettings.port = 1883;
			if (fMqttSettings.latitude < -90.0 || fMqttSettings.latitude > 90.0)
				fMqttSettings.latitude = 0.0;
			if (fMqttSettings.longitude < -180.0 || fMqttSettings.longitude > 180.0)
				fMqttSettings.longitude = 0.0;

			// Save settings
			_SaveMqttSettings();
			fTopBar->SetMqttEnabled(fMqttSettings.enabled);

			// Create MQTT client if needed
			if (fMqttClient == NULL && fMqttSettings.enabled) {
				fprintf(stderr, "[MainWindow] Creating MQTT client on demand...\n");
				fMqttClient = new MqttClient();
				fMqttClient->SetTarget(this);
				fMqttClient->SetLogTarget(this);
			}

			// Update MQTT client and reconnect if needed
			if (fMqttClient != NULL) {
				fMqttClient->SetSettings(fMqttSettings);
				if (fMqttSettings.enabled) {
					fMqttClient->Connect();
					_LogMessage("MQTT", "Connecting to broker...");
				} else {
					fMqttClient->Disconnect();
					_LogMessage("MQTT", "Disconnected");
				}
			}
			break;
		}

		case MSG_MQTT_CONNECTED:
			_LogMessage("MQTT", "Connected to broker");
			fTopBar->SetMqttStatus(true);
			fTopBar->SetMqttEnabled(fMqttSettings.enabled);
			if (fMqttLogWindow != NULL && fMqttLogWindow->LockLooper()) {
				fMqttLogWindow->AddLogEntry(MQTT_LOG_CONN,
					"Connected to broker");
				fMqttLogWindow->SetMqttStatus(true);
				fMqttLogWindow->UnlockLooper();
			}
			// Publish initial status only if we have device info
			if (fMqttClient != NULL && fHasDeviceInfo) {
				_LogMessage("MQTT", "Publishing initial status");
				fMqttClient->PublishStatus(fDeviceName, fDeviceFirmware, fDeviceBoard,
					fBatteryMv, fDeviceUptime, fNoiseFloor);
			} else if (!fHasDeviceInfo) {
				_LogMessage("MQTT", "Waiting for device info before publishing");
			}
			break;

		case MSG_MQTT_DISCONNECTED:
			_LogMessage("MQTT", "Disconnected from broker");
			fTopBar->SetMqttStatus(false);
			fTopBar->SetMqttEnabled(fMqttSettings.enabled);
			if (fMqttLogWindow != NULL && fMqttLogWindow->LockLooper()) {
				fMqttLogWindow->AddLogEntry(MQTT_LOG_CONN,
					"Disconnected from broker");
				fMqttLogWindow->SetMqttStatus(false);
				fMqttLogWindow->UnlockLooper();
			}
			break;

		case MSG_MQTT_ERROR:
		{
			const char* error = message->GetString("error", "Unknown error");
			_LogMessage("MQTT", BString().SetToFormat("Error: %s", error).String());
			fTopBar->SetMqttStatus(false);
			if (fMqttLogWindow != NULL && fMqttLogWindow->LockLooper()) {
				BString entry;
				entry.SetToFormat("Error: %s", error);
				fMqttLogWindow->AddLogEntry(MQTT_LOG_ERR, entry.String());
				fMqttLogWindow->UnlockLooper();
			}
			break;
		}

		case MSG_MQTT_PUBLISH_STATUS:
			// Publish status to MQTT (called by timer)
			// Only publish if we have device info (public key, name)
			if (fMqttClient != NULL && fMqttClient->IsConnected() && fHasDeviceInfo) {
				fMqttClient->PublishStatus(fDeviceName, fDeviceFirmware, fDeviceBoard,
					fBatteryMv, fDeviceUptime, fNoiseFloor);
			}
			break;

		case MSG_MQTT_TOGGLE:
		{
			if (!fMqttSettings.enabled) {
				// Not enabled: open MQTT settings
				PostMessage(MSG_SHOW_MQTT_SETTINGS);
			} else if (fMqttClient != NULL && fMqttClient->IsConnected()) {
				// Connected: disconnect
				fMqttClient->Disconnect();
				_LogMessage("MQTT", "Disconnecting...");
			} else {
				// Enabled but not connected: connect
				if (fMqttClient == NULL) {
					fMqttClient = new MqttClient();
					fMqttClient->SetTarget(this);
					fMqttClient->SetLogTarget(this);
				}
				strlcpy(fMqttSettings.publicKey, fPublicKey,
					sizeof(fMqttSettings.publicKey));
				fMqttClient->SetSettings(fMqttSettings);
				fMqttClient->Connect();
				_LogMessage("MQTT", "Connecting to broker...");
			}
			break;
		}

		case MSG_SHOW_MQTT_LOG:
			if (fMqttLogWindow == NULL)
				fMqttLogWindow = new MqttLogWindow();
			_ShowWindow(fMqttLogWindow);
			break;

		case MSG_MQTT_LOG_ENTRY:
		{
			int32 type = message->GetInt32("type", MQTT_LOG_INFO);
			const char* text = message->GetString("text", NULL);
			if (text != NULL && fMqttLogWindow != NULL) {
				if (fMqttLogWindow->LockLooper()) {
					fMqttLogWindow->AddLogEntry(type, text);
					fMqttLogWindow->UnlockLooper();
				}
			}
			break;
		}

		case MSG_SHOW_TRACE_PATH:
		{
			// Show trace path window for selected contact
			ContactInfo* contact = NULL;
			if (fSelectedContact > 0) {  // Not channel (index 0)
				ContactItem* item = dynamic_cast<ContactItem*>(
					fContactList->ItemAt(fSelectedContact));
				if (item != NULL && !item->IsChannel()) {
					contact = _FindContactByPrefix(item->GetContact().publicKey, kPubKeyPrefixSize);
				}
			}

			if (contact == NULL) {
				_LogMessage("WARN", "Select a contact first to trace path");
				break;
			}

			if (fTracePathWindow == NULL) {
				fTracePathWindow = new TracePathWindow(this, contact);
				fTracePathWindow->Show();
			} else {
				if (fTracePathWindow->LockLooper()) {
					fTracePathWindow->SetContact(contact);
					fTracePathWindow->UnlockLooper();
				}
				_ShowWindow(fTracePathWindow);
			}
			break;
		}

		case MSG_TRACE_PATH:
		{
			// Handle trace path request from TracePathWindow, ChatView, or NetworkMapWindow
			const void* pubkey;
			ssize_t size;
			if (message->FindData("pubkey", B_RAW_TYPE, &pubkey, &size) == B_OK) {
				const uint8* pk = (const uint8*)pubkey;

				// Look up full contact — SendTracePath needs full 32-byte key
				ContactInfo* contact = _FindContactByPrefix(pk,
					(size >= (ssize_t)kPubKeyPrefixSize) ? kPubKeyPrefixSize : size);

				if (contact == NULL) {
					if (size >= (ssize_t)kPubKeyPrefixSize) {
						_LogMessage("WARN", BString().SetToFormat(
							"Cannot trace: no contact for prefix %02X%02X%02X%02X%02X%02X (size=%d)",
							pk[0], pk[1], pk[2], pk[3], pk[4], pk[5], (int)size));
					} else {
						_LogMessage("WARN", "Cannot trace: pubkey data too short");
					}
					if (fTracePathWindow != NULL
						&& fTracePathWindow->LockLooper()) {
						fTracePathWindow->SetTraceComplete(false);
						fTracePathWindow->UnlockLooper();
					}
					break;
				}

				// Only show TracePathWindow if NOT from automated discovery
				bool silent = false;
				message->FindBool("silent", &silent);

				if (!silent) {
					if (!fConnected) {
						_LogMessage("WARN", "Cannot trace: radio not connected");
						if (fTracePathWindow != NULL
							&& fTracePathWindow->LockLooper()) {
							fTracePathWindow->SetTraceComplete(false);
							fTracePathWindow->UnlockLooper();
						}
						break;
					}

					if (fTracePathWindow == NULL) {
						fTracePathWindow = new TracePathWindow(this, contact);
						fTracePathWindow->Show();
					} else {
						_ShowWindow(fTracePathWindow);
					}

					if (fTracePathWindow->LockLooper()) {
						fTracePathWindow->StartExternalTrace(contact);
						fTracePathWindow->ResolveHopNames(&fContacts);
						fTracePathWindow->UnlockLooper();
					}
				}

				status_t result = fProtocol->SendTracePath(contact);
				if (result == B_BAD_VALUE) {
					_LogMessage("INFO", BString().SetToFormat(
						"Direct path to %s — cannot trace",
						contact->name[0] ? contact->name : "(unnamed)"));
				} else {
					_LogMessage("INFO", BString().SetToFormat(
						"Trace path sent for %s (%02X%02X%02X%02X%02X%02X)",
						contact->name,
						contact->publicKey[0], contact->publicKey[1],
						contact->publicKey[2], contact->publicKey[3],
						contact->publicKey[4], contact->publicKey[5]));
				}
			}
			break;
		}

		case MSG_TRACE_ALL:
		{
			if (!fConnected) {
				_LogMessage("WARN", "Cannot trace all: radio not connected");
				break;
			}
			int traceable = 0;
			int skipped = 0;
			for (int32 i = 0; i < fContacts.CountItems(); i++) {
				ContactInfo* c = fContacts.ItemAt(i);
				if (c == NULL || !c->isValid)
					continue;
				if (c->outPathLen < 0) {
					skipped++;  // Direct path confirmed — skip
					continue;
				}
				traceable++;
			}
			_LogMessage("INFO", BString().SetToFormat(
				"Trace ALL: %d traceable, %d skipped (direct)", traceable, skipped));
			if (traceable > 0) {
				// Start sequential trace: send first, schedule next with delay
				BMessage nextMsg(MSG_TRACE_ALL_NEXT);
				nextMsg.AddInt32("index", 0);
				PostMessage(&nextMsg);
			}
			break;
		}

		case MSG_TRACE_ALL_NEXT:
		{
			int32 startIdx = 0;
			message->FindInt32("index", &startIdx);
			for (int32 i = startIdx; i < fContacts.CountItems(); i++) {
				ContactInfo* c = fContacts.ItemAt(i);
				if (c == NULL || !c->isValid || c->outPathLen < 0)
					continue;
				// Send trace for this contact
				status_t result = fProtocol->SendTracePath(c);
				if (result == B_OK) {
					_LogMessage("INFO", BString().SetToFormat(
						"Trace sent: %s (path:%d)",
						c->name[0] ? c->name : "(unnamed)", c->outPathLen));
				}
				// Schedule next contact after 2 seconds (avoid flooding radio)
				BMessage nextMsg(MSG_TRACE_ALL_NEXT);
				nextMsg.AddInt32("index", i + 1);
				BMessageRunner::StartSending(this, &nextMsg, 2000000, 1);
				return;
			}
			_LogMessage("INFO", "Trace ALL complete");
			break;
		}

		case MSG_AUTO_CONNECT:
		{
			// Auto-connect on startup if port available
			if (!fConnected && !fSelectedPort.IsEmpty()) {
				_LogMessage("INFO", BString("Auto-connecting to ") << fSelectedPort << "...");
				_Connect();
			}
			break;
		}

		case MSG_POST_CONNECT_INIT:
		{
			// Deferred init commands after APP_START completes
			if (fConnected) {
				fProtocol->SendDeviceQuery();
				fProtocol->SendExportSelf();
				fProtocol->SendSelfAdvert();
				fSyncingContacts = true;
				fProtocol->SendGetContacts();
				fProtocol->SendGetBattery();
				fProtocol->SendGetStats();

				// Start periodic stats refresh timer
				delete fStatsRefreshTimer;
				fStatsRefreshTimer = NULL;
				BMessage timerMsg(MSG_STATS_TIMER);
				fStatsRefreshTimer = new BMessageRunner(this,
					&timerMsg, kStatsRefreshInterval);
			}
			break;
		}

		case MSG_STATS_TIMER:
		{
			// Periodic stats refresh for status bar
			if (fConnected) {
				fProtocol->SendGetBattery();
				fProtocol->SendGetStats();
			}
			break;
		}

		case MSG_DELIVERY_CHECK_TICK:
			_CheckDeliveryTimeouts();
			break;

		case MSG_HANDSHAKE_TIMEOUT:
		{
			delete fHandshakeTimer;
			fHandshakeTimer = NULL;

			if (fConnected && !fHasDeviceInfo) {
				_LogMessage("WARNING",
					"Device not responding — this may not be a MeshCore Companion Radio");

				// Stop the stats refresh timer to avoid flooding a non-companion device
				delete fStatsRefreshTimer;
				fStatsRefreshTimer = NULL;

				// Switch to raw mode so '>' in CLI prompts isn't parsed as frame marker
				fSerialHandler->SetRawMode(true);

				// Auto-switch to Repeater Monitor for non-companion devices
				_SwitchToRepeaterMode();
			}
			break;
		}

		case MSG_REPEATER_MAP_TICK:
		{
			_UpdateRepeaterMap();
			break;
		}

		case MSG_REPEATER_PACKET_FLOW:
		{
			const char* src = NULL;
			const char* dst = NULL;
			int8 snr = 0;
			int32 type = 0;
			if (message->FindString("src", &src) == B_OK
				&& message->FindString("dst", &dst) == B_OK) {
				message->FindInt8("snr", &snr);
				message->FindInt32("type", &type);
				bool isMessage = (type == 2);
				if (_LockIfVisible(fNetworkMapWindow)) {
					fNetworkMapWindow->TriggerPacketFlow(
						src, dst, snr, isMessage);
					fNetworkMapWindow->UnlockLooper();
				}
			}
			break;
		}

		case MSG_AUTO_SYNC_CONTACTS:
		{
			delete fAutoSyncRunner;
			fAutoSyncRunner = NULL;
			if (fConnected) {
				_LogMessage("INFO", "Auto-syncing contacts after new node discovery");
				fSyncingContacts = true;
				fProtocol->SendGetContacts();
			}
			break;
		}

		case MSG_UPDATE_CONTACTS:
			// Update contact list (called after loading from People files)
			_UpdateContactList();
			break;

		case MSG_CONTACT_CONTEXT:
		{
			int32 index;
			BPoint screenPt;
			if (message->FindInt32("index", &index) != B_OK
				|| message->FindPoint("where", &screenPt) != B_OK)
				break;

			ContactItem* ctxItem = dynamic_cast<ContactItem*>(
				fContactList->ItemAt(index));
			if (ctxItem == NULL)
				break;

			BPopUpMenu* menu = new BPopUpMenu("context", false, false);

			if (index == 0) {
				// Public Channel context menu — offer "Add Channel"
				menu->AddItem(new BMenuItem("Add Channel" B_UTF8_ELLIPSIS,
					new BMessage(MSG_ADD_CHANNEL)));
				// Mute/Unmute public channel
				menu->AddSeparatorItem();
				BMessage* muteMsg = new BMessage(MSG_CONTACT_MUTE_TOGGLE);
				muteMsg->AddString("key_hex", "ch_public");
				bool isMuted = _IsMuted("ch_public");
				menu->AddItem(new BMenuItem(
					isMuted ? "Unmute Channel" : "Mute Channel", muteMsg));
			} else if (ctxItem->IsChannel() && ctxItem->ChannelIndex() >= 0) {
				// Private channel context menu
				BMessage* removeMsg = new BMessage(MSG_REMOVE_CHANNEL);
				removeMsg->AddInt32("channel_idx", ctxItem->ChannelIndex());
				menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
				// Mute/Unmute private channel
				menu->AddSeparatorItem();
				char chKey[16];
				snprintf(chKey, sizeof(chKey), "ch_%d",
					ctxItem->ChannelIndex());
				BMessage* muteMsg = new BMessage(MSG_CONTACT_MUTE_TOGGLE);
				muteMsg->AddString("key_hex", chKey);
				bool isMuted = _IsMuted(chKey);
				menu->AddItem(new BMenuItem(
					isMuted ? "Unmute" : "Mute", muteMsg));
			} else {
				// Contact context menu
				char contactHex[kContactHexSize];
				FormatContactKey(contactHex, ctxItem->GetContact().publicKey);

				BMessage* pingMsg = new BMessage(MSG_CONTACT_PING);
				pingMsg->AddData("pubkey", B_RAW_TYPE,
					ctxItem->GetContact().publicKey, kPubKeySize);
				menu->AddItem(new BMenuItem("Ping", pingMsg));

				BMessage* traceMsg = new BMessage(MSG_TRACE_PATH);
				traceMsg->AddData("pubkey", B_RAW_TYPE,
					ctxItem->GetContact().publicKey, kPubKeySize);
				menu->AddItem(new BMenuItem("Trace Route", traceMsg));

				BMessage* resetMsg = new BMessage(MSG_CONTACT_RESET_PATH);
				resetMsg->AddData("pubkey", B_RAW_TYPE,
					ctxItem->GetContact().publicKey, kPubKeySize);
				menu->AddItem(new BMenuItem("Reset Path", resetMsg));
				menu->AddSeparatorItem();

				// Mute/Unmute contact
				BMessage* muteMsg = new BMessage(MSG_CONTACT_MUTE_TOGGLE);
				muteMsg->AddString("key_hex", contactHex);
				bool isMuted = _IsMuted(contactHex);
				menu->AddItem(new BMenuItem(
					isMuted ? "Unmute" : "Mute", muteMsg));

				// Group submenu
				BMenu* groupMenu = new BMenu("Group");
				// "New Group..." option
				BMessage* newGroupMsg = new BMessage(MSG_GROUP_CREATE);
				newGroupMsg->AddString("contact_key", contactHex);
				groupMenu->AddItem(new BMenuItem(
					"New Group" B_UTF8_ELLIPSIS, newGroupMsg));

				BString currentGroup =
					DatabaseManager::Instance()->GetContactGroup(contactHex);

				if (fGroupNames.CountItems() > 0) {
					groupMenu->AddSeparatorItem();
					for (int32 g = 0; g < fGroupNames.CountItems(); g++) {
						BString* gName = fGroupNames.ItemAt(g);
						if (gName == NULL)
							continue;
						BMessage* addMsg =
							new BMessage(MSG_GROUP_ADD_CONTACT);
						addMsg->AddString("group_name", gName->String());
						addMsg->AddString("contact_key", contactHex);
						BMenuItem* gItem = new BMenuItem(
							gName->String(), addMsg);
						if (currentGroup == *gName)
							gItem->SetMarked(true);
						groupMenu->AddItem(gItem);
					}
				}

				if (currentGroup.Length() > 0) {
					groupMenu->AddSeparatorItem();
					BMessage* rmMsg =
						new BMessage(MSG_GROUP_REMOVE_CONTACT);
					rmMsg->AddString("group_name", currentGroup.String());
					rmMsg->AddString("contact_key", contactHex);
					groupMenu->AddItem(new BMenuItem(
						"Remove from Group", rmMsg));

					BMessage* delMsg =
						new BMessage(MSG_GROUP_DELETE);
					delMsg->AddString("group_name", currentGroup.String());
					groupMenu->AddItem(new BMenuItem(
						"Delete Group", delMsg));
				}

				groupMenu->SetTargetForItems(this);
				menu->AddItem(groupMenu);

				menu->AddSeparatorItem();
				BMessage* removeMsg = new BMessage(MSG_CONTACT_REMOVE);
				removeMsg->AddData("pubkey", B_RAW_TYPE,
					ctxItem->GetContact().publicKey, kPubKeySize);
				menu->AddItem(new BMenuItem("Remove Contact", removeMsg));
			}

			menu->SetTargetForItems(this);
			menu->Go(screenPt, true, true);
			delete menu;
			break;
		}

		case MSG_CONTACT_PING:
		{
			if (!fConnected) {
				_LogMessage("WARN", "Not connected");
				BAlert* alert = new BAlert("Ping",
					"Cannot ping: radio not connected",
					"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
				alert->Go(NULL);
				break;
			}
			const void* keyData;
			ssize_t keySize;
			if (message->FindData("pubkey", B_RAW_TYPE, &keyData, &keySize) == B_OK
				&& keySize >= (ssize_t)kPubKeyPrefixSize) {
				// Look up full contact for SendTracePath
				ContactInfo* contact = _FindContactByPrefix(
					(const uint8*)keyData, kPubKeyPrefixSize);

				if (contact == NULL) {
					_LogMessage("WARN", "Cannot ping: contact not found");
					fPingPending = false;
					break;
				}

				const char* name = contact->name[0]
					? contact->name : "(unknown)";

				// Record ping start
				fPingStartTime = system_time();
				memcpy(fPingTargetKey, keyData, kPubKeyPrefixSize);
				fPingPending = true;

				status_t result = fProtocol->SendTracePath(contact);
				if (result != B_OK) {
					_LogMessage("PING", BString().SetToFormat(
						"Ping failed for %s", name));
					fPingPending = false;
				} else {
					_LogMessage("PING", BString().SetToFormat(
						"Pinging %s...", name));
					// Show feedback in chat
					ChatMessage pingMsg;
					pingMsg.timestamp = (uint32)time(NULL);
					pingMsg.isOutgoing = false;
					pingMsg.isChannel = true;
					snprintf(pingMsg.text, sizeof(pingMsg.text),
						"Pinging %s...", name);
					fChatView->AddMessage(pingMsg, "Ping");

					// Start 30s timeout
					delete fPingTimeoutRunner;
					BMessage timeoutMsg(MSG_PING_TIMEOUT);
					fPingTimeoutRunner = new BMessageRunner(this,
						&timeoutMsg, 30000000LL, 1);
				}
			}
			break;
		}

		case MSG_PING_TIMEOUT:
		{
			if (fPingPending) {
				fPingPending = false;
				ContactInfo* target = _FindContactByPrefix(
					fPingTargetKey, kPubKeyPrefixSize);
				const char* name = (target && target->name[0])
					? target->name : "(unknown)";

				BString result;
				result.SetToFormat("Ping %s: no response (timeout)", name);
				_LogMessage("PING", result.String());

				ChatMessage pingResult;
				pingResult.timestamp = (uint32)time(NULL);
				pingResult.isOutgoing = false;
				pingResult.isChannel = true;
				strlcpy(pingResult.text, result.String(),
					sizeof(pingResult.text));
				fChatView->AddMessage(pingResult, "Ping");

				// Advance to next contact in ping-all mode
				if (fPingAllActive) {
					fPingAllIndex++;
					BMessage nextMsg(MSG_PING_ALL_NEXT);
					nextMsg.AddInt32("index", fPingAllIndex);
					BMessageRunner::StartSending(this, &nextMsg, 2000000, 1);
				}
			}
			delete fPingTimeoutRunner;
			fPingTimeoutRunner = NULL;
			break;
		}

		case MSG_PING_ALL:
		{
			if (!fConnected) {
				BAlert* alert = new BAlert("Ping All",
					"Cannot ping: radio not connected",
					"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
				alert->Go(NULL);
				break;
			}
			if (fPingAllActive) {
				_LogMessage("WARN", "Ping All already in progress");
				break;
			}

			// Count valid contacts (exclude channel)
			int32 total = 0;
			for (int32 i = 0; i < fContacts.CountItems(); i++) {
				ContactInfo* c = fContacts.ItemAt(i);
				if (c != NULL && c->isValid && c->type != 0)
					total++;
			}

			if (total == 0) {
				ChatMessage noContacts;
				noContacts.timestamp = (uint32)time(NULL);
				noContacts.isOutgoing = false;
				noContacts.isChannel = true;
				strlcpy(noContacts.text, "Ping All: no contacts to ping",
					sizeof(noContacts.text));
				fChatView->AddMessage(noContacts, "Ping");
				break;
			}

			fPingAllActive = true;
			fPingAllIndex = 0;
			fPingAllTotal = total;
			fPingAllResponded = 0;

			ChatMessage startMsg;
			startMsg.timestamp = (uint32)time(NULL);
			startMsg.isOutgoing = false;
			startMsg.isChannel = true;
			snprintf(startMsg.text, sizeof(startMsg.text),
				"Pinging %d contacts...", (int)total);
			fChatView->AddMessage(startMsg, "Ping");
			_LogMessage("PING", BString().SetToFormat(
				"Ping All started: %d contacts", (int)total));

			// Start with first contact
			BMessage nextMsg(MSG_PING_ALL_NEXT);
			nextMsg.AddInt32("index", 0);
			PostMessage(&nextMsg);
			break;
		}

		case MSG_PING_ALL_NEXT:
		{
			if (!fPingAllActive || !fConnected) {
				fPingAllActive = false;
				break;
			}

			int32 startIdx = 0;
			message->FindInt32("index", &startIdx);

			// Find next valid contact
			for (int32 i = startIdx; i < fContacts.CountItems(); i++) {
				ContactInfo* c = fContacts.ItemAt(i);
				if (c == NULL || !c->isValid || c->type == 0)
					continue;

				fPingAllIndex = i;
				const char* name = c->name[0] ? c->name : "(unknown)";

				// Set up ping state
				fPingStartTime = system_time();
				memcpy(fPingTargetKey, c->publicKey, kPubKeyPrefixSize);
				fPingPending = true;

				status_t result = fProtocol->SendTracePath(c);
				if (result != B_OK) {
					_LogMessage("PING", BString().SetToFormat(
						"Ping failed for %s (send error)", name));
					fPingPending = false;
					// Try next after 2s
					fPingAllIndex = i + 1;
					BMessage nextMsg(MSG_PING_ALL_NEXT);
					nextMsg.AddInt32("index", i + 1);
					BMessageRunner::StartSending(this, &nextMsg, 2000000, 1);
					return;
				}

				_LogMessage("PING", BString().SetToFormat(
					"Pinging %s (%d/%d)...", name,
					(int)(fPingAllResponded + 1), (int)fPingAllTotal));

				// Start 10s timeout (shorter than single-ping 30s)
				delete fPingTimeoutRunner;
				BMessage timeoutMsg(MSG_PING_TIMEOUT);
				fPingTimeoutRunner = new BMessageRunner(this,
					&timeoutMsg, 10000000LL, 1);
				return;  // Wait for response or timeout
			}

			// All contacts pinged — show summary
			fPingAllActive = false;
			BString summary;
			summary.SetToFormat("Ping All complete: %d/%d responded",
				(int)fPingAllResponded, (int)fPingAllTotal);
			_LogMessage("PING", summary.String());

			ChatMessage doneMsg;
			doneMsg.timestamp = (uint32)time(NULL);
			doneMsg.isOutgoing = false;
			doneMsg.isChannel = true;
			strlcpy(doneMsg.text, summary.String(), sizeof(doneMsg.text));
			fChatView->AddMessage(doneMsg, "Ping");
			break;
		}

		case MSG_CONTACT_RESET_PATH:
		{
			const void* keyData;
			ssize_t keySize;
			if (message->FindData("pubkey", B_RAW_TYPE, &keyData, &keySize) == B_OK
				&& keySize >= (ssize_t)kPubKeySize) {
				fProtocol->SendResetPath((const uint8*)keyData);
			}
			break;
		}

		case MSG_CONTACT_REMOVE:
		{
			const void* keyData;
			ssize_t keySize;
			if (message->FindData("pubkey", B_RAW_TYPE, &keyData, &keySize) != B_OK
				|| keySize < (ssize_t)kPubKeySize)
				break;

			if (!fConnected) {
				BAlert* alert = new BAlert("Error",
					"Not connected to device.", "OK");
				alert->Go();
				break;
			}

			// Find contact name for confirmation
			ContactInfo* target = _FindContactByPrefix((const uint8*)keyData, 6);
			BString confirmText("Remove contact");
			if (target != NULL)
				confirmText.SetToFormat("Remove \"%s\" from device?", target->name);

			BAlert* confirm = new BAlert("Confirm", confirmText.String(),
				"Cancel", "Remove", NULL,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			if (confirm->Go() == 1) {
				fProtocol->SendRemoveContact((const uint8*)keyData);
				// Schedule auto-sync after 3s
				delete fAutoSyncRunner;
				fAutoSyncRunner = NULL;
				BMessage syncMsg(MSG_AUTO_SYNC_CONTACTS);
				fAutoSyncRunner = new BMessageRunner(this,
					&syncMsg, kAutoSyncDelay, 1);
			}
			break;
		}

		case MSG_REMOVE_CHANNEL:
		{
			int32 chIdx;
			if (message->FindInt32("channel_idx", &chIdx) != B_OK)
				break;

			if (!fConnected) {
				BAlert* alert = new BAlert("Error",
					"Not connected to device.", "OK");
				alert->Go();
				break;
			}

			// Find channel name
			BString chName;
			for (int32 i = 0; i < fChannels.CountItems(); i++) {
				if (fChannels.ItemAt(i)->index == (uint8)chIdx) {
					chName = fChannels.ItemAt(i)->name;
					break;
				}
			}

			BString confirmText;
			confirmText.SetToFormat("Remove channel #%s?", chName.String());
			BAlert* confirm = new BAlert("Confirm", confirmText.String(),
				"Cancel", "Remove", NULL,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			if (confirm->Go() == 1) {
				fProtocol->SendRemoveChannel((uint8)chIdx);
				// Remove from local list and update UI
				for (int32 i = 0; i < fChannels.CountItems(); i++) {
					if (fChannels.ItemAt(i)->index == (uint8)chIdx) {
						fChannels.RemoveItemAt(i);
						break;
					}
				}
				// Reset selection if we were viewing this channel
				if (fSelectedChannelIdx == chIdx) {
					fSelectedChannelIdx = -1;
					_SelectContact(0);  // Switch to Public Channel
				}
				_UpdateContactList();
			}
			break;
		}

		case MSG_ADD_CHANNEL:
		{
			if (!fConnected) {
				BAlert* alert = new BAlert("Error",
					"Not connected to device.", "OK");
				alert->Go();
				break;
			}
			if (fMaxChannels == 0) {
				BAlert* alert = new BAlert("Error",
					"Device does not support private channels.", "OK");
				alert->Go();
				break;
			}

			AddChannelWindow* win = new AddChannelWindow(this);
			win->Show();
			break;
		}

		case MSG_CREATE_CHANNEL:
		{
			const char* name;
			if (message->FindString("name", &name) != B_OK || name[0] == '\0')
				break;
			if (!fConnected)
				break;

			// Find first empty slot
			int32 emptySlot = -1;
			for (uint8 s = 0; s < fMaxChannels; s++) {
				bool used = false;
				for (int32 i = 0; i < fChannels.CountItems(); i++) {
					if (fChannels.ItemAt(i)->index == s) {
						used = true;
						break;
					}
				}
				if (!used) {
					emptySlot = s;
					break;
				}
			}
			if (emptySlot < 0) {
				BAlert* alert = new BAlert("Error",
					"No empty channel slots available.", "OK");
				alert->Go();
				break;
			}

			// Generate a simple PSK from the channel name
			uint8 secret[16];
			memset(secret, 0, sizeof(secret));
			size_t nameLen = strlen(name);
			for (size_t i = 0; i < 16 && i < nameLen; i++)
				secret[i] = (uint8)name[i];

			fProtocol->SendSetChannel((uint8)emptySlot, name, secret);

			// Add to local list immediately
			ChannelInfo* ch = new ChannelInfo();
			ch->index = (uint8)emptySlot;
			strlcpy(ch->name, name, sizeof(ch->name));
			memcpy(ch->secret, secret, 16);
			fChannels.AddItem(ch);
			_UpdateContactList();

			_LogMessage("OK", BString().SetToFormat(
				"Created channel #%s (slot %d)", name, (int)emptySlot));
			break;
		}

		case MSG_CONTACT_MUTE_TOGGLE:
		{
			const char* keyHex = message->GetString("key_hex", "");
			if (keyHex[0] != '\0') {
				bool currentlyMuted = _IsMuted(keyHex);
				_SetMuted(keyHex, !currentlyMuted);
			}
			break;
		}

		case MSG_GROUP_CREATE:
		{
			const char* contactKey = message->GetString("contact_key", "");
			// Generate incremental group name
			BString groupName;
			groupName.SetToFormat("Group %d",
				(int)fGroupNames.CountItems() + 1);

			DatabaseManager::Instance()->CreateGroup(groupName.String());
			if (contactKey[0] != '\0')
				DatabaseManager::Instance()->AddContactToGroup(
					groupName.String(), contactKey);
			_LoadContactGroups();
			_RefreshContactList();

			_LogMessage("INFO", BString().SetToFormat(
				"Created group \"%s\"", groupName.String()));
			break;
		}

		case MSG_GROUP_ADD_CONTACT:
		{
			const char* groupName = message->GetString("group_name", "");
			const char* contactKey = message->GetString("contact_key", "");
			if (groupName[0] != '\0' && contactKey[0] != '\0') {
				// If already in this group, remove instead (toggle)
				BString current =
					DatabaseManager::Instance()->GetContactGroup(contactKey);
				if (current == groupName)
					DatabaseManager::Instance()->RemoveContactFromGroup(
						groupName, contactKey);
				else
					DatabaseManager::Instance()->AddContactToGroup(
						groupName, contactKey);
				_RefreshContactList();
			}
			break;
		}

		case MSG_GROUP_REMOVE_CONTACT:
		{
			const char* groupName = message->GetString("group_name", "");
			const char* contactKey = message->GetString("contact_key", "");
			if (groupName[0] != '\0' && contactKey[0] != '\0') {
				DatabaseManager::Instance()->RemoveContactFromGroup(
					groupName, contactKey);
				_RefreshContactList();
			}
			break;
		}

		case MSG_GROUP_DELETE:
		{
			const char* groupName = message->GetString("group_name", "");
			if (groupName[0] != '\0') {
				DatabaseManager::Instance()->DeleteGroup(groupName);
				_LoadContactGroups();
				_RefreshContactList();
			}
			break;
		}

		case MSG_SET_NAME:
		{
			const char* name;
			if (message->FindString("name", &name) == B_OK)
				fProtocol->SendSetName(name);
			break;
		}

		case MSG_APPLY_SETTINGS:
		{
			if (!fSerialHandler->IsConnected()) {
				_LogMessage("ERROR", "Cannot apply settings: not connected");
				break;
			}

			// Handle radio parameters from SettingsWindow
			uint32 freq;
			if (message->FindUInt32("frequency", &freq) == B_OK) {
				uint32 bw;
				uint8 sf, cr;
				if (message->FindUInt32("bandwidth", &bw) == B_OK &&
					message->FindUInt8("sf", &sf) == B_OK &&
					message->FindUInt8("cr", &cr) == B_OK) {
					_LogMessage("INFO", BString().SetToFormat(
						"Setting radio: %.3f MHz, %.1f kHz BW, SF%u, CR%u",
						freq / 1000000.0, bw / 1000.0, sf, cr));
					fProtocol->SendRadioParams(freq, bw, sf, cr);
					fRadioFreq = freq;
					fRadioBw = bw;
					fRadioSf = sf;
					fRadioCr = cr;
					fHasRadioParams = true;
				}
			}

			// Handle lat/lon
			double lat, lon;
			if (message->FindDouble("latitude", &lat) == B_OK &&
				message->FindDouble("longitude", &lon) == B_OK) {
				fProtocol->SendSetLatLon(lat, lon);
				// Enable location sharing if setting non-zero coordinates
				if (fAdvertLocPolicy == 0 && (lat != 0.0 || lon != 0.0)) {
					fAdvertLocPolicy = 1;
					fProtocol->SendOtherParams(fManualAddContacts,
						fTelemetryModes, fAdvertLocPolicy, fMultiAcks);
				}
			}

			// Handle TX power
			uint8 txpower;
			if (message->FindUInt8("txpower", &txpower) == B_OK) {
				fProtocol->SendSetTxPower(txpower);
			}
			break;
		}

		case MSG_SHOW_TELEMETRY:
		{
			if (fTelemetryWindow == NULL) {
				fTelemetryWindow = new TelemetryWindow(this);
				fTelemetryWindow->Show();
			} else {
				_ShowWindow(fTelemetryWindow);
			}
			break;
		}

		case MSG_SHOW_PACKET_ANALYZER:
		{
			if (fPacketAnalyzerWindow == NULL) {
				fPacketAnalyzerWindow = new PacketAnalyzerWindow(this);
				fPacketAnalyzerWindow->Show();
			} else {
				_ShowWindow(fPacketAnalyzerWindow);
			}
			break;
		}

		case MSG_SHOW_MISSION_CONTROL:
		{
			if (fMissionControlWindow == NULL) {
				fMissionControlWindow = new MissionControlWindow(this);
				fMissionControlWindow->Show();
				// Populate with current state
				if (fMissionControlWindow->LockLooper()) {
					fMissionControlWindow->SetConnectionState(fConnected,
						fDeviceName, fDeviceFirmware);
					if (fBatteryMv > 0)
						fMissionControlWindow->SetBatteryInfo(fBatteryMv, 0, 0);
					if (fLastRssi != 0 || fLastSnr != 0)
						fMissionControlWindow->SetRadioStats(fLastRssi,
							fLastSnr, fNoiseFloor);
					if (fHasRadioParams)
						fMissionControlWindow->SetRadioConfig(fRadioFreq,
							fRadioBw, fRadioSf, fRadioCr, fRadioTxPower);
					if (fDeviceUptime > 0)
						fMissionControlWindow->SetDeviceStats(fDeviceUptime,
							fTxPackets, fRxPackets);
					// Seed the activity feed with current state
					fMissionControlWindow->AddActivityEvent("SYS",
						"Mission Control opened");
					if (fConnected) {
						BString msg;
						msg.SetToFormat("Device online: %s",
							fDeviceName[0] != '\0'
								? fDeviceName : fSelectedPort.String());
						fMissionControlWindow->AddActivityEvent("SYS",
							msg.String());
						if (fBatteryMv > 0) {
							int32 pct = BatteryPercent(fBatteryMv,
								(BatteryChemistry)fBatteryType);
							msg.SetToFormat("Battery: %d%% (%u mV)",
								(int)pct, (unsigned)fBatteryMv);
							fMissionControlWindow->AddActivityEvent("SYS",
								msg.String());
						}
						if (fLastRssi != 0) {
							msg.SetToFormat("Radio: RSSI %ddBm, SNR %+ddB",
								(int)fLastRssi, (int)fLastSnr);
							fMissionControlWindow->AddActivityEvent("SYS",
								msg.String());
						}
						msg.SetToFormat("Contacts: %d total",
							(int)fContacts.CountItems());
						fMissionControlWindow->AddActivityEvent("SYS",
							msg.String());
					} else {
						fMissionControlWindow->AddActivityEvent("SYS",
							"Device not connected");
					}
					fMissionControlWindow->UnlockLooper();
				}
			} else {
				_ShowWindow(fMissionControlWindow);
			}
			break;
		}

		case MSG_SHOW_SERIAL_MONITOR:
		{
			if (fSerialMonitorWindow == NULL) {
				fSerialMonitorWindow = new SerialMonitorWindow(this);
				fSerialMonitorWindow->Show();
			} else {
				_ShowWindow(fSerialMonitorWindow);
			}
			break;
		}

		case MSG_SHOW_REPEATER_MONITOR:
		{
			// Toggle between chat mode and repeater monitor
			int32 current = fCardView->CardLayout()->VisibleIndex();
			if (current == 0) {
				_SwitchToRepeaterMode();
			} else {
				_SwitchToChatMode();
			}
			break;
		}

		case MSG_SERIAL_SEND_RAW:
		{
			// Forward raw text command to SerialHandler
			if (fSerialHandler != NULL)
				fSerialHandler->PostMessage(message);
			break;
		}

		// Repeater admin commands
		case MSG_ADMIN_SEND_CLI:
		{
			const char* command;
			if (message->FindString("command", &command) == B_OK)
				_SendCliCommand(command);
			break;
		}

		case MSG_ADMIN_REBOOT:
			fProtocol->SendReboot();
			break;

		case MSG_ADMIN_FACTORY_RESET:
			fProtocol->SendFactoryReset();
			break;

		case MSG_ADMIN_REFRESH_TICK:
			if (fConnected && fLoggedIn && fInfoPanel->IsAdminSession()) {
				ContactInfo* target = _FindContactByPrefix(
					fLoggedInKey, kPubKeyPrefixSize);
				if (target != NULL)
					fProtocol->SendStatusRequest(target->publicKey);
			}
			break;

		// Admin toolbar buttons (in chat area)
		case kMsgAdminVersion:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "ver");
			PostMessage(&cmd);
			break;
		}

		case kMsgAdminNeighbors:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "neighbors");
			PostMessage(&cmd);
			break;
		}

		case kMsgAdminClock:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "clock");
			PostMessage(&cmd);
			break;
		}

		case kMsgAdminClearStats:
		{
			BMessage cmd(MSG_ADMIN_SEND_CLI);
			cmd.AddString("command", "clear stats");
			PostMessage(&cmd);
			break;
		}

		case kMsgAdminReboot:
		{
			BAlert* alert = new BAlert("Reboot",
				"Reboot this device?\n\n"
				"The device will restart and briefly go offline.",
				"Cancel", "Reboot", NULL,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			if (alert->Go() == 1)
				fProtocol->SendReboot();
			break;
		}

		case kMsgAdminFactoryReset:
		{
			BAlert* alert1 = new BAlert("Factory Reset",
				"Factory reset this device?\n\n"
				"ALL contacts, messages, and settings will be erased!",
				"Cancel", "Continue", NULL,
				B_WIDTH_AS_USUAL, B_STOP_ALERT);
			if (alert1->Go() == 1) {
				BAlert* alert2 = new BAlert("Confirm Reset",
					"Are you SURE?\n\n"
					"This cannot be undone!",
					"Cancel", "Reset", NULL,
					B_WIDTH_AS_USUAL, B_STOP_ALERT);
				if (alert2->Go() == 1)
					fProtocol->SendFactoryReset();
			}
			break;
		}

		case kMsgAdminSetName:
		{
			const char* text = fAdminNameField->Text();
			if (text != NULL && text[0] != '\0') {
				BString command("set name ");
				command << text;
				_SendCliCommand(command.String());
				fAdminNameField->SetText("");
			}
			break;
		}

		case kMsgAdminSetPwd:
		{
			const char* text = fAdminPwdField->Text();
			if (text != NULL && text[0] != '\0') {
				BAlert* alert = new BAlert("Change Password",
					"Change the admin password for this device?",
					"Cancel", "Change", NULL,
					B_WIDTH_AS_USUAL, B_WARNING_ALERT);
				if (alert->Go() == 1) {
					BString command("password ");
					command << text;
					_SendCliCommand(command.String());
					fAdminPwdField->SetText("");
				}
			}
			break;
		}

		case MSG_REQUEST_TELEMETRY:
		{
			// Send telemetry request for selected contact
			if (!fConnected) {
				_LogMessage("ERROR", "Not connected");
				break;
			}
			if (fSelectedContact <= 0) {
				_LogMessage("WARN", "Select a contact first");
				break;
			}
			ContactItem* item = dynamic_cast<ContactItem*>(
				fContactList->ItemAt(fSelectedContact));
			if (item != NULL && !item->IsChannel()) {
				fProtocol->SendTelemetryRequest(item->GetContact().publicKey);
				_LogMessage("INFO", "Requesting telemetry data...");

				// Open telemetry window
				PostMessage(MSG_SHOW_TELEMETRY);
			}
			break;
		}

		case MSG_REQUEST_ALL_TELEMETRY:
		{
			if (!fConnected) {
				_LogMessage("ERROR", "Not connected");
				break;
			}
			if (fContacts.CountItems() == 0) {
				_LogMessage("WARN", "No contacts to poll");
				break;
			}
			// Start polling all contacts at 2s intervals
			fTelemetryPollIndex = 0;
			delete fTelemetryPollTimer;
			fTelemetryPollTimer = NULL;
			BMessage pollMsg(MSG_TELEMETRY_POLL_TICK);
			fTelemetryPollTimer = new BMessageRunner(BMessenger(this),
				&pollMsg, 2000000, -1);
			// Send first request immediately
			PostMessage(MSG_TELEMETRY_POLL_TICK);
			// Open telemetry window
			PostMessage(MSG_SHOW_TELEMETRY);
			_LogMessage("INFO", "Requesting telemetry from all contacts...");
			break;
		}

		case MSG_TELEMETRY_POLL_TICK:
		{
			if (!fConnected || fTelemetryPollTimer == NULL)
				break;

			// Find next valid contact to poll
			while (fTelemetryPollIndex < fContacts.CountItems()) {
				ContactInfo* contact = fContacts.ItemAt(fTelemetryPollIndex);
				fTelemetryPollIndex++;
				if (contact != NULL && contact->type != 0) {
					// Skip channel-only contacts (type 0)
					fProtocol->SendTelemetryRequest(contact->publicKey);
					BString logMsg;
					logMsg.SetToFormat("Requesting telemetry from %s (%d/%d)",
						contact->name, (int)fTelemetryPollIndex,
						(int)fContacts.CountItems());
					_LogMessage("INFO", logMsg.String());
					break;
				}
			}

			// Check if we've finished polling all contacts
			if (fTelemetryPollIndex >= fContacts.CountItems()) {
				delete fTelemetryPollTimer;
				fTelemetryPollTimer = NULL;
				_LogMessage("INFO", "Telemetry poll complete");
			}
			break;
		}

		case MSG_SHOW_MAP:
		{
			if (fMapWindow == NULL) {
				fMapWindow = new MapWindow(this);
				// Set self position from MQTT settings
				if (fMqttSettings.latitude != 0.0 || fMqttSettings.longitude != 0.0) {
					fMapWindow->SetSelfPosition(
						(float)fMqttSettings.latitude,
						(float)fMqttSettings.longitude,
						fDeviceName[0] != '\0' ? fDeviceName : "Self");
				}
				fMapWindow->UpdateFromContacts(&fContacts,
					fMqttSettings.latitude, fMqttSettings.longitude);
				fMapWindow->Show();
			} else {
				_ShowWindow(fMapWindow);
			}
			break;
		}

		case MSG_SHOW_LOGIN:
		{
			ContactInfo* contact = NULL;
			if (fSelectedContact > 0) {
				ContactItem* item = dynamic_cast<ContactItem*>(
					fContactList->ItemAt(fSelectedContact));
				if (item != NULL && !item->IsChannel())
					contact = _FindContactByPrefix(
						item->GetContact().publicKey, kPubKeyPrefixSize);
			}

			if (contact == NULL) {
				_LogMessage("WARN", "Select a repeater or room contact first");
				break;
			}

			if (fLoginWindow == NULL) {
				fLoginWindow = new LoginWindow(this, contact);
				fLoginWindow->Show();
			} else {
				_ShowWindow(fLoginWindow);
			}
			break;
		}

		case MSG_SEND_LOGIN_CMD:
		{
			const void* pubkey;
			ssize_t size;
			const char* password;
			if (message->FindData("pubkey", B_RAW_TYPE, &pubkey, &size) == B_OK &&
				size >= (ssize_t)kPubKeySize &&
				message->FindString("password", &password) == B_OK) {
				fLoginPending = true;
				memcpy(fLoginTargetKey, pubkey,
					sizeof(fLoginTargetKey));
				fProtocol->SendLogin((const uint8*)pubkey, password);
			}
			break;
		}

		case MSG_SHOW_EXPORT_CONTACT:
		{
			ContactInfo* contact = NULL;
			if (fSelectedContact > 0) {
				ContactItem* item = dynamic_cast<ContactItem*>(
					fContactList->ItemAt(fSelectedContact));
				if (item != NULL && !item->IsChannel())
					contact = _FindContactByPrefix(
						item->GetContact().publicKey, kPubKeyPrefixSize);
			}

			if (contact == NULL) {
				_LogMessage("WARN", "Select a contact to export");
				break;
			}

			if (fContactExportWindow == NULL) {
				fContactExportWindow = new ContactExportWindow(this, true, contact);
				fContactExportWindow->Show();
			}
			break;
		}

		case MSG_SHOW_IMPORT_CONTACT:
		{
			if (fContactExportWindow == NULL) {
				fContactExportWindow = new ContactExportWindow(this, false);
				fContactExportWindow->Show();
			} else {
				_ShowWindow(fContactExportWindow);
			}
			break;
		}

		case MSG_EXPORT_CONTACT_CMD:
		{
			// Send export command to device
			if (!fConnected) break;
			const void* pubkey;
			ssize_t size;
			if (message->FindData("pubkey", B_RAW_TYPE, &pubkey, &size) == B_OK
				&& size >= (ssize_t)kPubKeyPrefixSize) {
				uint8 payload[7];
				payload[0] = CMD_EXPORT_CONTACT;
				memcpy(payload + 1, pubkey, 6);
				fSerialHandler->SendFrame(payload, 7);
				_LogMessage("INFO", "Requesting contact export data...");
			}
			break;
		}

		case MSG_SEND_SAR_MARKER:
		{
			_ShowSarMarkerDialog();
			break;
		}

		case MSG_EXPORT_GPX:
		{
			if (fContacts.CountItems() == 0) {
				_LogMessage("WARN", "No contacts to export");
				break;
			}
			if (fGpxSavePanel == NULL) {
				BMessage saveMsg(MSG_EXPORT_GPX_DONE);
				fGpxSavePanel = new BFilePanel(B_SAVE_PANEL,
					new BMessenger(this), NULL, 0, false, &saveMsg);
				fGpxSavePanel->SetSaveText("sestriere-contacts.gpx");
			}
			fGpxSavePanel->Show();
			break;
		}

		case MSG_EXPORT_GPX_DONE:
		{
			entry_ref dirRef;
			BString name;
			if (message->FindRef("directory", &dirRef) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BPath path(&dirRef);
				path.Append(name.String());
				_ExportGPX(path.Path());
			}
			break;
		}

		case MSG_IMPORT_CONTACT_CMD:
		{
			// Send import command to device
			if (!fConnected) break;
			const void* data;
			ssize_t size;
			if (message->FindData("data", B_RAW_TYPE, &data, &size) == B_OK) {
				uint8 payload[256];
				payload[0] = CMD_IMPORT_CONTACT;
				size_t copyLen = (size_t)size;
				if (copyLen > sizeof(payload) - 1)
					copyLen = sizeof(payload) - 1;
				memcpy(payload + 1, data, copyLen);
				fSerialHandler->SendFrame(payload, 1 + copyLen);
				_LogMessage("INFO", "Sending contact import command...");
			}
			break;
		}

		case MSG_SHOW_PROFILE:
		{
			if (fProfileWindow == NULL) {
				fProfileWindow = new ProfileWindow(this);
				// Window not yet shown — safe to call without lock
				fProfileWindow->SetDeviceInfo(fDeviceName, fPublicKey,
					fDeviceFirmware);
				fProfileWindow->SetContacts(fContacts);
				fProfileWindow->SetChannels(fChannels);
				if (fHasRadioParams)
					fProfileWindow->SetRadioParams(fRadioFreq, fRadioBw,
						fRadioSf, fRadioCr, fRadioTxPower);
				fProfileWindow->SetMqttSettings(fMqttSettings);
				fProfileWindow->Show();
			} else if (fProfileWindow->LockLooper()) {
				fProfileWindow->SetDeviceInfo(fDeviceName, fPublicKey,
					fDeviceFirmware);
				fProfileWindow->SetContacts(fContacts);
				fProfileWindow->SetChannels(fChannels);
				if (fHasRadioParams)
					fProfileWindow->SetRadioParams(fRadioFreq, fRadioBw,
						fRadioSf, fRadioCr, fRadioTxPower);
				fProfileWindow->SetMqttSettings(fMqttSettings);
				fProfileWindow->UnlockLooper();
				_ShowWindow(fProfileWindow);
			}
			break;
		}

		case MSG_PROFILE_IMPORT_CONTACTS:
		{
			if (!fConnected) {
				_LogMessage("ERROR", "Device not connected");
				break;
			}
			int32 count = 0;
			const void* pubkey;
			ssize_t size;
			for (int32 i = 0;
				message->FindData("pubkey", B_RAW_TYPE, i, &pubkey, &size)
					== B_OK;
				i++) {
				const char* name = message->GetString("name", i, "");
				int32 type = message->GetInt32("type", i);
				fProtocol->SendAddUpdateContact((const uint8*)pubkey,
					name, (uint8)type);
				count++;
			}
			if (count > 0) {
				BString logMsg;
				logMsg.SetToFormat("Profile: importing %d contacts...",
					(int)count);
				_LogMessage("INFO", logMsg.String());
				fProtocol->SendGetContacts();
			}
			break;
		}

		case MSG_PROFILE_IMPORT_CHANNELS:
		{
			if (!fConnected) {
				_LogMessage("ERROR", "Device not connected");
				break;
			}
			int32 count = 0;
			int32 index;
			for (int32 i = 0;
				message->FindInt32("index", i, &index) == B_OK;
				i++) {
				const char* name = message->GetString("name", i, "");
				const void* secret;
				ssize_t secretSize;
				if (message->FindData("secret", B_RAW_TYPE, i,
					&secret, &secretSize) == B_OK && secretSize == 16) {
					fProtocol->SendSetChannel((uint8)index, name,
						(const uint8*)secret);
					count++;
				}
			}
			if (count > 0) {
				BString logMsg;
				logMsg.SetToFormat("Profile: importing %d channels...",
					(int)count);
				_LogMessage("INFO", logMsg.String());
				// Re-enumerate channels
				fEnumeratingChannels = true;
				fChannelEnumIndex = 0;
				fProtocol->SendGetChannel(0);
			}
			break;
		}

		case MSG_PROFILE_IMPORT_RADIO:
		{
			if (!fConnected) {
				_LogMessage("ERROR", "Device not connected");
				break;
			}
			uint32 freq = message->GetUInt32("frequency", 0);
			uint32 bw = message->GetUInt32("bandwidth", 0);
			uint8 sf = message->GetUInt8("sf", 0);
			uint8 cr = message->GetUInt8("cr", 0);
			uint8 txPower = message->GetUInt8("txPower", 0);
			if (freq > 0) {
				fProtocol->SendRadioParams(freq, bw, sf, cr);
				if (txPower > 0)
					fProtocol->SendSetTxPower(txPower);
				_LogMessage("INFO", "Profile: radio parameters applied");
			}
			break;
		}

		case MSG_PROFILE_IMPORT_MQTT:
		{
			fMqttSettings.enabled = message->GetBool("enabled", false);
			strlcpy(fMqttSettings.broker, message->GetString("broker", ""),
				sizeof(fMqttSettings.broker));
			fMqttSettings.port = message->GetInt32("port", 1883);
			strlcpy(fMqttSettings.username,
				message->GetString("username", ""),
				sizeof(fMqttSettings.username));
			const char* pw = message->GetString("password", "");
			if (pw[0] != '\0')
				strlcpy(fMqttSettings.password, pw,
					sizeof(fMqttSettings.password));
			strlcpy(fMqttSettings.iataCode,
				message->GetString("iata", "XXX"),
				sizeof(fMqttSettings.iataCode));
			fMqttSettings.latitude = message->GetDouble("latitude", 0.0);
			fMqttSettings.longitude = message->GetDouble("longitude", 0.0);
			_SaveMqttSettings();
			_LogMessage("INFO", "Profile: MQTT settings imported");

			if (fMqttClient != NULL) {
				fMqttClient->SetSettings(fMqttSettings);
				if (fMqttSettings.enabled)
					fMqttClient->Connect();
			}
			break;
		}

		// --- Image sharing ---
		case MSG_SELECT_IMAGE:
		{
			if (fImageOpenPanel == NULL) {
				fImageOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this),
					NULL, B_FILE_NODE, false, new BMessage(MSG_IMAGE_SELECTED));
			}
			fImageOpenPanel->Show();
			break;
		}

		case MSG_IMAGE_SELECTED:
			_HandleImageSelected(message);
			break;

		case MSG_IMAGE_SEND_NEXT:
			_SendNextImageFragment();
			break;

		case MSG_IMAGE_FETCH_REQ:
		{
			uint32 sid;
			if (message->FindUInt32("session_id", &sid) == B_OK)
				_StartImageFetch(sid);
			break;
		}

		case MSG_IMAGE_EXPIRE:
			if (fImageSessions != NULL)
				fImageSessions->PurgeExpired();
			break;

		// --- Voice messages ---
		case MSG_VOICE_RECORD:
			if (fRecordingVoice)
				_StopVoiceRecord();
			else
				_StartVoiceRecord();
			break;

		case MSG_VOICE_SEND_NEXT:
			_SendNextVoiceFragment();
			break;

		case MSG_VOICE_PLAY_REQ:
		{
			uint32 sid;
			if (message->FindUInt32("session_id", &sid) == B_OK)
				_HandleVoicePlayRequest(sid);
			break;
		}

		case MSG_VOICE_PLAY_DONE:
		{
			// Reset playing state on all voice messages
			for (int32 i = 0; i < fChatView->CountItems(); i++) {
				MessageView* mv = dynamic_cast<MessageView*>(
					fChatView->ItemAt(i));
				if (mv != NULL && mv->IsVoiceMessage()
					&& mv->IsVoicePlaying()) {
					mv->SetVoicePlaying(false);
					fChatView->InvalidateItem(i);
				}
			}
			break;
		}

		case MSG_VOICE_EXPIRE:
			if (fVoiceSessions != NULL)
				fVoiceSessions->PurgeExpired();
			break;

		// --- GIF sharing ---
		case MSG_SELECT_GIF:
		{
			if (fGifPickerWindow == NULL)
				fGifPickerWindow = new GifPickerWindow(this);

			if (fGifPickerWindow->LockLooper()) {
				if (fGifPickerWindow->IsHidden())
					fGifPickerWindow->Show();
				else
					fGifPickerWindow->Activate();
				fGifPickerWindow->UnlockLooper();
			}
			break;
		}

		case MSG_GIF_SELECTED:
			_HandleGifSelected(message);
			break;

		case MSG_GIF_DOWNLOAD_REQ:
		{
			const char* gifId = NULL;
			int32 itemIndex = 0;
			if (message->FindString("gif_id", &gifId) == B_OK
				&& message->FindInt32("item_index", &itemIndex)
					== B_OK) {
				_DownloadAndDisplayGif(gifId, itemIndex);
			}
			break;
		}

		case MSG_GIF_LOADED:
		{
			int32 itemIndex;
			void* framesPtr = NULL;
			void* durationsPtr = NULL;
			int32 frameCount = 0;

			if (message->FindInt32("item_index", &itemIndex) != B_OK)
				break;
			if (message->FindPointer("frames", &framesPtr) != B_OK)
				break;
			if (message->FindPointer("durations", &durationsPtr) != B_OK)
				break;
			if (message->FindInt32("frame_count", &frameCount) != B_OK)
				break;

			BBitmap** frames = (BBitmap**)framesPtr;
			uint32* durations = (uint32*)durationsPtr;

			MessageView* mv = dynamic_cast<MessageView*>(
				fChatView->ItemAt(itemIndex));
			if (mv != NULL && mv->IsGifMessage()) {
				mv->SetGifFrames(frames, durations, frameCount);

				// Recalculate height and invalidate
				BFont font;
				fChatView->GetFont(&font);
				mv->Update(fChatView, &font);
				fChatView->InvalidateItem(itemIndex);

				if (frameCount > 1)
					fChatView->StartGifAnimation();
			} else {
				// Clean up if item no longer valid
				for (int32 i = 0; i < frameCount; i++)
					delete frames[i];
				delete[] frames;
				delete[] durations;
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MainWindow::QuitRequested()
{
	_SaveMessages();

	// Close database
	DatabaseManager::Destroy();

	// Disconnect serial before quitting
	if (fSerialHandler != NULL && fSerialHandler->IsConnected()) {
		fSerialHandler->Disconnect();
	}

	// Close child windows
	if (fSettingsWindow != NULL) {
		fSettingsWindow->Lock();
		fSettingsWindow->Quit();
		fSettingsWindow = NULL;
	}
	if (fStatsWindow != NULL) {
		fStatsWindow->Lock();
		fStatsWindow->Quit();
		fStatsWindow = NULL;
	}
	if (fTracePathWindow != NULL) {
		fTracePathWindow->Lock();
		fTracePathWindow->Quit();
		fTracePathWindow = NULL;
	}
	if (fNetworkMapWindow != NULL) {
		fNetworkMapWindow->Lock();
		fNetworkMapWindow->Quit();
		fNetworkMapWindow = NULL;
	}
	if (fTelemetryWindow != NULL) {
		fTelemetryWindow->Lock();
		fTelemetryWindow->Quit();
		fTelemetryWindow = NULL;
	}
	if (fLoginWindow != NULL) {
		fLoginWindow->Lock();
		fLoginWindow->Quit();
		fLoginWindow = NULL;
	}
	if (fMapWindow != NULL) {
		fMapWindow->Lock();
		fMapWindow->Quit();
		fMapWindow = NULL;
	}
	if (fContactExportWindow != NULL) {
		fContactExportWindow->Lock();
		fContactExportWindow->Quit();
		fContactExportWindow = NULL;
	}
	if (fPacketAnalyzerWindow != NULL) {
		fPacketAnalyzerWindow->Lock();
		fPacketAnalyzerWindow->Quit();
		fPacketAnalyzerWindow = NULL;
	}
	if (fMqttLogWindow != NULL) {
		fMqttLogWindow->Lock();
		fMqttLogWindow->Quit();
		fMqttLogWindow = NULL;
	}
	if (fMissionControlWindow != NULL) {
		fMissionControlWindow->Lock();
		fMissionControlWindow->Quit();
		fMissionControlWindow = NULL;
	}
	if (fProfileWindow != NULL) {
		fProfileWindow->Lock();
		fProfileWindow->Quit();
		fProfileWindow = NULL;
	}
	if (fSerialMonitorWindow != NULL) {
		fSerialMonitorWindow->Lock();
		fSerialMonitorWindow->Quit();
		fSerialMonitorWindow = NULL;
	}
	if (fGifPickerWindow != NULL) {
		fGifPickerWindow->Lock();
		fGifPickerWindow->Quit();
		fGifPickerWindow = NULL;
	}
	// fRepeaterMonitorView is a child of fCardView, destroyed automatically

	// Destroy singletons
	DebugLogWindow::Destroy();
	NotificationManager::Destroy();

	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
MainWindow::_SelectContact(int32 index)
{
	if (index < 0 || index >= fContactList->CountItems())
		return;

	fSelectedContact = index;
	fSelectedChannelIdx = -1;

	// Determine what was selected by checking the item
	ContactItem* selectedItem = dynamic_cast<ContactItem*>(
		fContactList->ItemAt(index));

	if (index == 0) {
		// Public Channel selected
		fSendingToChannel = true;
		fSelectedChannelIdx = -1;  // -1 = public channel (idx 0 on wire)
		fChatHeader->SetChannel(true);
		fChatHeader->SetConsoleMode(false);
		fChatView->SetCurrentContact(NULL);
		fInfoPanel->SetChannel(true);

		// Pass network stats to channel info panel
		{
			int32 totalContacts = fContacts.CountItems();
			int32 onlineCount = 0;
			uint32 now = (uint32)time(NULL);
			for (int32 i = 0; i < totalContacts; i++) {
				ContactInfo* c = fContacts.ItemAt(i);
				if (c != NULL && c->lastSeen > 0
					&& (now - c->lastSeen) < 300)
					onlineCount++;
			}
			fInfoPanel->SetChannelStats(totalContacts, onlineCount);
		}
		_ShowAdminToolbar(false);
		fMessageInput->SetEnabled(fConnected);
		fSendButton->SetEnabled(fConnected);
		fAttachButton->SetEnabled(fConnected);
		fGifButton->SetEnabled(fConnected);
		fVoiceButton->SetEnabled(fConnected);

		// Load channel message history
		fChatView->ClearMessages();
		for (int32 i = 0; i < fChannelMessages.CountItems(); i++) {
			ChatMessage* msg = fChannelMessages.ItemAt(i);
			if (msg != NULL) {
				const char* senderName = msg->isOutgoing ? "Me" : "Channel";
				// Try to find sender name
				if (!msg->isOutgoing) {
					ContactInfo* sender = _FindContactByPrefix(msg->pubKeyPrefix, 6);
					if (sender != NULL)
						senderName = sender->name;
				}
				fChatView->AddMessage(*msg, senderName);
			}
		}

		// Clear unread badge
		fChannelItem->ClearUnread();
		fContactList->InvalidateItem(0);

	} else if (selectedItem != NULL && selectedItem->IsChannel()
		&& selectedItem->ChannelIndex() >= 0) {
		// Private channel selected
		fSendingToChannel = true;
		fSelectedChannelIdx = selectedItem->ChannelIndex();

		// Find the ChannelInfo
		ChannelInfo* ch = NULL;
		for (int32 i = 0; i < fChannels.CountItems(); i++) {
			if (fChannels.ItemAt(i)->index == (uint8)fSelectedChannelIdx) {
				ch = fChannels.ItemAt(i);
				break;
			}
		}

		BString headerName;
		headerName.SetToFormat("#%s", ch ? ch->name : "Channel");
		fChatHeader->SetChannelName(headerName.String());
		fChatHeader->SetConsoleMode(false);
		fChatView->SetCurrentContact(NULL);
		fInfoPanel->SetChannel(true);
		_ShowAdminToolbar(false);
		fMessageInput->SetEnabled(fConnected);
		fSendButton->SetEnabled(fConnected);
		fAttachButton->SetEnabled(fConnected);
		fGifButton->SetEnabled(fConnected);
		fVoiceButton->SetEnabled(fConnected);

		// Load private channel message history
		fChatView->ClearMessages();
		if (ch != NULL) {
			for (int32 i = 0; i < ch->messages.CountItems(); i++) {
				ChatMessage* msg = ch->messages.ItemAt(i);
				if (msg != NULL) {
					const char* senderName = msg->isOutgoing ? "Me" : "Channel";
					if (!msg->isOutgoing) {
						ContactInfo* sender = _FindContactByPrefix(
							msg->pubKeyPrefix, 6);
						if (sender != NULL)
							senderName = sender->name;
					}
					fChatView->AddMessage(*msg, senderName);
				}
			}
		}

		// Clear unread badge
		selectedItem->ClearUnread();
		fContactList->InvalidateItem(index);

	} else if (index > 0 && selectedItem != NULL && !selectedItem->IsChannel()) {
		// Contact selected — find by pubkey (works correctly with filter)
		fSendingToChannel = false;

		const uint8* pubkey = selectedItem->GetContact().publicKey;
		ContactInfo* contact = _FindContactByPrefix(pubkey, kPubKeyPrefixSize);

		if (contact != NULL) {
			fChatHeader->SetContact(contact);
			// Enable console mode if logged into this contact
			bool isLoggedInContact = fLoggedIn &&
				memcmp(contact->publicKey, fLoggedInKey,
					kPubKeyPrefixSize) == 0;
			fChatHeader->SetConsoleMode(isLoggedInContact);
			fChatView->SetCurrentContact(contact);
			fInfoPanel->SetContact(contact);
			_ShowAdminToolbar(isLoggedInContact);
			fMessageInput->SetEnabled(fConnected);
			fSendButton->SetEnabled(fConnected);
			fAttachButton->SetEnabled(fConnected);
			fGifButton->SetEnabled(fConnected);
			fVoiceButton->SetEnabled(fConnected);

			// Clear unread badge for this contact
			selectedItem->ClearUnread();
			fContactList->InvalidateItem(index);

			// Auto-open login for repeater/room if not already logged in
			if (fConnected && !isLoggedInContact
				&& (contact->type == 2 || contact->type == 3)) {
				// Recreate login window for the new target contact
				if (fLoginWindow != NULL) {
					fLoginWindow->Lock();
					fLoginWindow->Quit();
				}
				fLoginWindow = new LoginWindow(this, contact);
				fLoginWindow->Show();
			}
		}
	} else {
		// Nothing selected
		fSendingToChannel = false;
		fChatHeader->SetContact(NULL);
		fChatHeader->SetConsoleMode(false);
		fChatView->SetCurrentContact(NULL);
		fInfoPanel->Clear();
		_ShowAdminToolbar(false);
		fMessageInput->SetEnabled(false);
		fSendButton->SetEnabled(false);
		fAttachButton->SetEnabled(false);
		fGifButton->SetEnabled(false);
		fVoiceButton->SetEnabled(false);
	}

	_UpdateCharCounter();
}


void
MainWindow::_Connect()
{
	if (fSelectedPort.IsEmpty()) {
		_LogMessage("ERROR", "No port selected");
		return;
	}

	_LogMessage("INFO", BString("Connecting to ") << fSelectedPort << "...");

	BMessage msg(MSG_SERIAL_CONNECT);
	msg.AddString(kFieldPort, fSelectedPort);
	fSerialHandler->PostMessage(&msg);
}


void
MainWindow::_Disconnect()
{
	_LogMessage("INFO", "Disconnecting...");
	fSerialHandler->PostMessage(MSG_SERIAL_DISCONNECT);
}
void
MainWindow::_SendCliCommand(const char* command)
{
	if (!fLoggedIn || !fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not logged into a repeater/room");
		return;
	}

	ContactInfo* target = _FindContactByPrefix(fLoggedInKey, kPubKeyPrefixSize);
	if (target == NULL) {
		_LogMessage("ERROR", "Login target contact not found");
		return;
	}

	size_t cmdLen = strlen(command);
	if (cmdLen == 0 || cmdLen > 160)
		return;

	// Rate-limit: refuse if too many pending messages
	if (fPendingMessages.CountItems() >= kMaxSimultaneousPending) {
		_LogMessage("ERROR", "Too many pending messages — wait for delivery");
		return;
	}

	uint32 timestamp = (uint32)time(NULL);
	fProtocol->SendDM(target->publicKey, TXT_TYPE_CLI_DATA, timestamp,
		command, cmdLen);
	fTopBar->FlashTx();

	// Add to chat history
	ChatMessage outMsg;
	memcpy(outMsg.pubKeyPrefix, target->publicKey, kPubKeyPrefixSize);
	strlcpy(outMsg.text, command, sizeof(outMsg.text));
	outMsg.timestamp = timestamp;
	outMsg.isOutgoing = true;
	outMsg.isChannel = false;
	outMsg.txtType = TXT_TYPE_CLI_DATA;
	outMsg.deliveryStatus = DELIVERY_PENDING;

	// Store in contact's message list
	target->messages.AddItem(new ChatMessage(outMsg));

	// Persist to DB
	char contactHex[kContactHexSize];
	FormatContactKey(contactHex, target->publicKey);
	DatabaseManager::Instance()->InsertMessage(contactHex, outMsg);

	// Update chat view if this contact is selected
	int32 chatViewIdx = -1;
	ContactItem* selItem = dynamic_cast<ContactItem*>(
		fContactList->ItemAt(fSelectedContact));
	if (selItem != NULL && !selItem->IsChannel() &&
		memcmp(selItem->GetContact().publicKey, fLoggedInKey,
			kPubKeyPrefixSize) == 0) {
		fChatView->AddMessage(outMsg, "Me");
		chatViewIdx = fChatView->CountItems() - 1;
		selItem->SetLastMessage(command, timestamp);
		fContactList->InvalidateItem(fSelectedContact);
	}

	// Add to pending message queue for delivery tracking
	PendingMessage* pending = new PendingMessage();
	strlcpy(pending->contactKey, contactHex, sizeof(pending->contactKey));
	memcpy(pending->pubKey, target->publicKey, kPubKeySize);
	pending->timestamp = timestamp;
	strlcpy(pending->text, command, sizeof(pending->text));
	pending->txtType = TXT_TYPE_CLI_DATA;
	pending->attemptCount = 1;
	pending->sentTime = system_time();
	pending->gotRspSent = false;
	pending->chatViewIndex = chatViewIdx;
	fPendingMessages.AddItem(pending);
	_StartDeliveryTimer();

	_LogMessage("CLI", BString().SetToFormat("> %s", command));
}
void
MainWindow::_SendTextMessage(const char* text)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	if (fSendingToChannel) {
		_SendChannelMessage(text);
		return;
	}

	if (fSelectedContact <= 0) {
		_LogMessage("ERROR", "No contact selected");
		return;
	}

	// Look up contact by pubkey from the selected list item
	// (can't use fSelectedContact-1 as index — private channels shift indices)
	ContactItem* selItem = dynamic_cast<ContactItem*>(
		fContactList->ItemAt(fSelectedContact));
	if (selItem == NULL || selItem->IsChannel()) {
		_LogMessage("ERROR", "No contact selected");
		return;
	}
	ContactInfo* contact = _FindContactByPrefix(
		selItem->GetContact().publicKey, kPubKeyPrefixSize);
	if (contact == NULL) {
		_LogMessage("ERROR", "Invalid contact");
		return;
	}

	size_t textLen = strlen(text);
	if (textLen > 160) {
		_LogMessage("ERROR", "Message too long (max 160 chars)");
		return;
	}

	// Rate-limit: refuse if too many pending messages
	if (fPendingMessages.CountItems() >= kMaxSimultaneousPending) {
		_LogMessage("ERROR", "Too many pending messages — wait for delivery");
		return;
	}

	{
		char hex[kContactHexSize];
		FormatPubKeyPrefix(hex, contact->publicKey);
		_LogMessage("INFO", BString().SetToFormat(
			"Sending DM to %s [%s] (idx %d, cli=%d): %s",
			contact->name, hex, (int)fSelectedContact,
			(int)(fLoggedIn && memcmp(contact->publicKey,
				fLoggedInKey, kPubKeyPrefixSize) == 0),
			text));
	}

	// Use CLI txt_type when logged into this contact
	bool isCli = (fLoggedIn &&
		memcmp(contact->publicKey, fLoggedInKey, kPubKeyPrefixSize) == 0);
	uint8 txtType = isCli ? TXT_TYPE_CLI_DATA : TXT_TYPE_PLAIN;

	uint32 timestamp = (uint32)time(NULL);
	fProtocol->SendDM(contact->publicKey, txtType, timestamp, text, textLen);
	fTopBar->FlashTx();

	// Add to chat view as outgoing message (pending delivery)
	ChatMessage outMsg;
	memcpy(outMsg.pubKeyPrefix, contact->publicKey, kPubKeyPrefixSize);
	strlcpy(outMsg.text, text, sizeof(outMsg.text));
	outMsg.timestamp = timestamp;
	outMsg.isOutgoing = true;
	outMsg.isChannel = false;
	outMsg.pathLen = 0;
	outMsg.snr = 0;
	outMsg.txtType = txtType;
	outMsg.deliveryStatus = DELIVERY_PENDING;
	fChatView->AddMessage(outMsg, "Me");

	int32 chatViewIdx = fChatView->CountItems() - 1;

	// Store message in contact's history
	ChatMessage* stored = new ChatMessage(outMsg);
	contact->messages.AddItem(stored);

	// Persist to database
	char contactHex[kContactHexSize];
	FormatContactKey(contactHex, contact->publicKey);
	DatabaseManager::Instance()->InsertMessage(contactHex, outMsg);

	// Add to pending message queue for delivery tracking
	PendingMessage* pending = new PendingMessage();
	strlcpy(pending->contactKey, contactHex, sizeof(pending->contactKey));
	memcpy(pending->pubKey, contact->publicKey, kPubKeySize);
	pending->timestamp = timestamp;
	strlcpy(pending->text, text, sizeof(pending->text));
	pending->txtType = txtType;
	pending->attemptCount = 1;
	pending->sentTime = system_time();
	pending->gotRspSent = false;
	pending->chatViewIndex = chatViewIdx;
	fPendingMessages.AddItem(pending);
	_StartDeliveryTimer();

	// Update contact item preview
	ContactItem* item = dynamic_cast<ContactItem*>(fContactList->ItemAt(fSelectedContact));
	if (item != NULL) {
		item->SetLastMessage(text, timestamp);
		fContactList->InvalidateItem(fSelectedContact);
	}
}


void
MainWindow::_SendChannelMessage(const char* text)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	size_t textLen = strlen(text);
	if (textLen > 200) {
		_LogMessage("ERROR", "Message too long (max 200 chars)");
		return;
	}

	_LogMessage("INFO", BString("Sending to channel: ") << text);

	uint8 wireChannelIdx = (fSelectedChannelIdx >= 0)
		? (uint8)fSelectedChannelIdx : 0;
	uint32 timestamp = (uint32)time(NULL);
	fProtocol->SendChannelMsg(wireChannelIdx, timestamp, text, textLen);
	fTopBar->FlashTx();

	// Add to chat view as outgoing message (sent immediately for channel)
	ChatMessage outMsg;
	memset(outMsg.pubKeyPrefix, 0, kPubKeyPrefixSize);
	strlcpy(outMsg.text, text, sizeof(outMsg.text));
	outMsg.timestamp = timestamp;
	outMsg.isOutgoing = true;
	outMsg.isChannel = true;
	outMsg.pathLen = 0;
	outMsg.snr = 0;
	outMsg.deliveryStatus = DELIVERY_SENT;

	// Store message in appropriate channel list
	ChatMessage* stored = new ChatMessage(outMsg);

	if (fSelectedChannelIdx >= 0) {
		// Private channel
		bool added = false;
		for (int32 i = 0; i < fChannels.CountItems(); i++) {
			ChannelInfo* ch = fChannels.ItemAt(i);
			if (ch->index == wireChannelIdx) {
				ch->messages.AddItem(stored);
				added = true;
				break;
			}
		}
		if (!added)
			delete stored;
		BString dbKey;
		dbKey.SetToFormat("channel_%d", wireChannelIdx);
		DatabaseManager::Instance()->InsertMessage(dbKey.String(), outMsg);
	} else {
		// Public channel
		fChannelMessages.AddItem(stored);
		DatabaseManager::Instance()->InsertMessage("channel", outMsg);
	}

	// Display in chat
	fChatView->AddMessage(outMsg, "Me");

	// Update sidebar item preview
	if (fSelectedChannelIdx < 0) {
		fChannelItem->SetLastMessage(text, timestamp);
		fContactList->InvalidateItem(0);
	} else {
		for (int32 i = 1; i < fContactList->CountItems(); i++) {
			ContactItem* ci = dynamic_cast<ContactItem*>(fContactList->ItemAt(i));
			if (ci != NULL && ci->IsChannel()
				&& ci->ChannelIndex() == fSelectedChannelIdx) {
				ci->SetLastMessage(text, timestamp);
				fContactList->InvalidateItem(i);
				break;
			}
		}
	}
}

void
MainWindow::_OnFrameReceived(BMessage* message)
{
	const void* data;
	ssize_t size;
	if (message->FindData(kFieldData, B_RAW_TYPE, &data, &size) == B_OK) {
		_LogRx((const uint8*)data, size);
		_ParseFrame((const uint8*)data, size);
	}
}


void
MainWindow::_OnFrameSent(BMessage* message)
{
	const void* data;
	ssize_t size;
	if (message->FindData(kFieldData, B_RAW_TYPE, &data, &size) == B_OK) {
		_LogTx((const uint8*)data, size);
	}
}


void
MainWindow::_ParseFrame(const uint8* data, size_t length)
{
	if (length < 1)
		return;

	// Forward every frame to the Packet Analyzer window
	if (fPacketAnalyzerWindow != NULL) {
		CapturedPacket pkt;
		pkt.index = ++fRawPacketCount;
		pkt.timestamp = (uint32)real_time_clock();
		pkt.captureTime = system_time();
		PacketAnalyzerWindow::_DecodePacket(pkt, data, length);

		// Resolve contact name from pubkey prefix
		if (pkt.sourceStr[0] != '\0') {
			// Determine pubkey offset in raw data based on packet code
			int keyOffset = -1;
			switch (pkt.code) {
				case RSP_CONTACT_MSG_RECV_V3:
					keyOffset = 4; break;
				case RSP_CONTACT_MSG_RECV:
					keyOffset = 1; break;
				case PUSH_ADVERT:
				case PUSH_NEW_ADVERT:
				case PUSH_TELEMETRY_RESPONSE:
				case RSP_CONTACT:
					keyOffset = 1; break;
				case RSP_SELF_INFO:
					keyOffset = 4; break;
				default:
					break;
			}
			if (keyOffset >= 0
				&& length >= (size_t)(keyOffset + kPubKeyPrefixSize)) {
				ContactInfo* contact = _FindContactByPrefix(
					data + keyOffset, kPubKeyPrefixSize);
				if (contact != NULL && contact->name[0] != '\0') {
					char hexPrefix[8];
					strlcpy(hexPrefix, pkt.sourceStr, sizeof(hexPrefix));
					snprintf(pkt.sourceStr, sizeof(pkt.sourceStr),
						"%s (%s)", contact->name, hexPrefix);
				}
			}
		}

		BMessage msg(MSG_PACKET_CAPTURED);
		msg.AddData("packet", B_RAW_TYPE, &pkt, sizeof(pkt));

		if (fPacketAnalyzerWindow->LockLooper()) {
			fPacketAnalyzerWindow->MessageReceived(&msg);
			fPacketAnalyzerWindow->UnlockLooper();
		}
	}

	uint8 cmd = data[0];

	switch (cmd) {
		case RSP_OK:
			_LogMessage("OK", "Command successful");
			break;
		case RSP_ERR:
			if (fEnumeratingChannels && length >= 2 && data[1] == 2) {
				// ERR_CODE_NOT_FOUND during channel enumeration = done
				fEnumeratingChannels = false;
				_LogMessage("OK", BString().SetToFormat(
					"Found %d configured channels",
					(int)fChannels.CountItems()));

				// Load private channel messages from DB
				DatabaseManager* db = DatabaseManager::Instance();
				if (db->IsOpen()) {
					for (int32 ci = 0; ci < fChannels.CountItems(); ci++) {
						ChannelInfo* ch = fChannels.ItemAt(ci);
						if (ch == NULL)
							continue;
						BString dbKey;
						dbKey.SetToFormat("channel_%d", ch->index);
						db->LoadMessages(dbKey.String(), ch->messages);
					}
				}

				_UpdateContactList();
			} else if (!fHasDeviceInfo && length == 2 && data[1] <= 3) {
				_LogMessage("OK", BString().SetToFormat(
					"APP_START acknowledged, protocol version: %d", data[1]));
				if (fLoginPending) {
					fLoginPending = false;
					if (fLoginWindow != NULL) {
						if (fLoginWindow->LockLooper()) {
							if (!fLoginWindow->IsHidden()) {
								fLoginWindow->SetLoginResult(false,
									"Login rejected (connection reset by device).");
							}
							fLoginWindow->UnlockLooper();
						}
					}
				}
			} else {
				_HandleCmdErr(data, length);
			}
			break;
		case RSP_CONTACTS_START:
			_HandleContactsStart(data, length);
			break;
		case RSP_CONTACT:
			_HandleContact(data, length);
			break;
		case RSP_END_OF_CONTACTS:
			_HandleContactsEnd(data, length);
			break;
		case RSP_SELF_INFO:
			_HandleSelfInfo(data, length);
			break;
		case RSP_SENT:
			_HandleMsgSent(data, length);
			break;
		case RSP_CONTACT_MSG_RECV:
			_HandleContactMsgRecv(data, length, false);
			break;
		case RSP_CONTACT_MSG_RECV_V3:
			_HandleContactMsgRecv(data, length, true);
			break;
		case RSP_CHANNEL_MSG_RECV:
			_HandleChannelMsgRecv(data, length, false);
			break;
		case RSP_CHANNEL_MSG_RECV_V3:
			_HandleChannelMsgRecv(data, length, true);
			break;
		case RSP_NO_MORE_MESSAGES:
			if (fSyncingMessages) {
				fSyncingMessages = false;
				_LogMessage("OK", "Offline message sync complete");
			}
			break;
		case RSP_DEVICE_INFO:
			_HandleDeviceInfo(data, length);
			break;
		case RSP_EXPORT_CONTACT:
			_HandleExportContact(data, length);
			break;
		case RSP_BATT_AND_STORAGE:
			_HandleBattAndStorage(data, length);
			break;
		case RSP_STATS:
			_HandleStats(data, length);
			break;
		case RSP_CHANNEL_INFO:
			_HandleChannelInfo(data, length);
			break;

		// Push notifications
		case PUSH_MSG_WAITING:
			_HandlePushMsgWaiting(data, length);
			break;
		case PUSH_ADVERT:
		case PUSH_NEW_ADVERT:
			_HandlePushAdvert(data, length);
			break;
		case PUSH_SEND_CONFIRMED:
		{
			uint32 roundTripMs = 0;
			// PUSH_SEND_CONFIRMED payload: [0]=code [1-4]=ackCode(u32) [5-8]=roundTripMs(u32)
			if (length >= 9) {
				roundTripMs = ReadLE32(data + 5);
			}

			BString logMsg("Message delivery confirmed");
			if (roundTripMs > 0)
				logMsg.SetToFormat("Message delivery confirmed (RTT: %lums)",
					(unsigned long)roundTripMs);
			_LogMessage("OK", logMsg.String());

			// Find first pending message that has RSP_SENT (FIFO)
			PendingMessage* pending = NULL;
			int32 pendingIdx = -1;
			for (int32 i = 0; i < fPendingMessages.CountItems(); i++) {
				PendingMessage* p = fPendingMessages.ItemAt(i);
				if (p != NULL && p->gotRspSent) {
					pending = p;
					pendingIdx = i;
					break;
				}
			}

			if (pending != NULL) {
				// Update ChatView if contact is currently displayed
				ContactInfo* currentContact = fChatView->CurrentContact();
				if (currentContact != NULL && pending->chatViewIndex >= 0) {
					char currentHex[kContactHexSize];
					FormatContactKey(currentHex,
						currentContact->publicKey);
					if (strcmp(currentHex, pending->contactKey) == 0) {
						fChatView->UpdateDeliveryStatus(
							pending->chatViewIndex,
							DELIVERY_CONFIRMED, roundTripMs);
					}
				}

				// Update stored ChatMessage in contact history
				ContactInfo* contact = _FindContactByPrefix(
					pending->pubKey, kPubKeyPrefixSize);
				if (contact != NULL) {
					for (int32 i = contact->messages.CountItems() - 1;
						i >= 0; i--) {
						ChatMessage* msg = contact->messages.ItemAt(i);
						if (msg != NULL && msg->isOutgoing
							&& msg->timestamp == pending->timestamp
							&& (msg->deliveryStatus == DELIVERY_SENT
								|| msg->deliveryStatus == DELIVERY_PENDING)) {
							msg->deliveryStatus = DELIVERY_CONFIRMED;
							msg->roundTripMs = roundTripMs;
							break;
						}
					}
				}

				// Persist to database
				DatabaseManager::Instance()->UpdateMessageDeliveryStatus(
					pending->contactKey, pending->timestamp,
					DELIVERY_CONFIRMED, roundTripMs);

				// Remove from queue
				fPendingMessages.RemoveItemAt(pendingIdx);

				// Stop timer if queue is empty
				if (fPendingMessages.CountItems() == 0)
					_StopDeliveryTimer();
			}
			break;
		}
		case PUSH_PATH_UPDATED:
			_LogMessage("INFO", "Path updated for a contact — re-syncing");
			// Path changed: re-sync contacts to get fresh outPath data
			if (fConnected && !fSyncingContacts) {
				fSyncingContacts = true;
				fProtocol->SendGetContacts();
			}
			break;
		case PUSH_TRACE_DATA:
			_HandlePushTraceData(data, length);
			break;
		case PUSH_TELEMETRY_RESPONSE:
			_HandlePushTelemetry(data, length);
			break;
		case PUSH_LOGIN_SUCCESS:
			_HandlePushLoginResult(PUSH_LOGIN_SUCCESS);
			break;
		case PUSH_LOGIN_FAIL:
			_HandlePushLoginResult(PUSH_LOGIN_FAIL);
			break;
		case PUSH_RAW_RADIO_PACKET:
			_HandleRawPacket(data, length);
			break;
		case PUSH_STATUS_RESPONSE:
			_HandlePushStatusResponse(data, length);
			break;

		case RSP_CURR_TIME:
			_HandleCurrTime(data, length);
			break;

		case PUSH_RAW_DATA:
			_HandlePushRawData(data, length);
			break;

		case PUSH_BINARY_RESPONSE:
			// Same payload format as PUSH_RAW_DATA for image fragments
			_HandlePushRawData(data, length);
			break;

		case RSP_CUSTOM_VARS:
			_HandleCustomVars(data, length);
			break;

		case RSP_ADVERT_PATH:
			_HandleAdvertPath(data, length);
			break;

		default:
			_LogMessage("WARN", BString().SetToFormat("Unknown response: 0x%02X", cmd));
			break;
	}
}


void
MainWindow::_HandleCurrTime(const uint8* data, size_t length)
{
	// RSP_CURR_TIME: [0]=code [1-4]=epoch_secs(uint32 LE)
	if (length < 5) {
		_LogMessage("WARN", "RSP_CURR_TIME: frame too short");
		return;
	}

	uint32 epoch = ReadLE32(data + 1);

	time_t t = (time_t)epoch;
	struct tm tm;
	localtime_r(&t, &tm);
	char timeBuf[64];
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);
	_LogMessage("INFO", BString().SetToFormat(
		"Device time: %s (epoch %u)", timeBuf, (unsigned)epoch));
}


void
MainWindow::_HandleCustomVars(const uint8* data, size_t length)
{
	// RSP_CUSTOM_VARS: [0]=code [1+]=name:value (comma-separated text)
	if (length < 2) {
		_LogMessage("INFO", "Custom variables: (empty)");
		return;
	}

	size_t textLen = strnlen((const char*)data + 1, length - 1);
	BString vars((const char*)data + 1, textLen);
	_LogMessage("INFO", BString("Custom variables: ") << vars);
}


void
MainWindow::_HandleAdvertPath(const uint8* data, size_t length)
{
	// RSP_ADVERT_PATH: [0]=code [1-4]=recv_timestamp(uint32 LE)
	// [5]=path_len [6+]=path(bytes)
	if (length < 6) {
		_LogMessage("WARN", "RSP_ADVERT_PATH: frame too short");
		return;
	}

	uint32 recvTs = ReadLE32(data + 1);
	uint8 pathLen = data[5];

	BString pathHex;
	for (size_t i = 6; i < length && i < (size_t)(6 + pathLen); i++) {
		if (i > 6)
			pathHex << " ";
		char hex[4];
		snprintf(hex, sizeof(hex), "%02X", data[i]);
		pathHex << hex;
	}

	time_t t = (time_t)recvTs;
	struct tm tm;
	localtime_r(&t, &tm);
	char timeBuf[64];
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);
	_LogMessage("INFO", BString().SetToFormat(
		"Advert path: recvTime=%s pathLen=%d path=[%s]",
		timeBuf, pathLen, pathHex.String()));
}


void
MainWindow::_HandlePushRawData(const uint8* data, size_t length)
{
	// PUSH_RAW_DATA: [0]=code [1]=SNR*4 [2]=RSSI [3]=reserved(0xFF)
	// [4+]=payload
	if (length < 4) {
		_LogMessage("WARN", "PUSH_RAW_DATA: frame too short");
		return;
	}

	int8 snr = (int8)data[1];
	int8 rssi = (int8)data[2];
	size_t payloadLen = length - 4;
	float snrDb = snr / 4.0f;

	// Check magic byte for image protocol
	if (payloadLen > 0) {
		const uint8* payload = data + 4;
		if (payload[0] == kImageMagic) {
			_HandleIncomingImageFragment(payload, payloadLen);
			return;
		} else if (payload[0] == kFetchMagic) {
			_HandleIncomingFetchRequest(payload, payloadLen);
			return;
		} else if (payload[0] == kVoiceMagic) {
			_HandleIncomingVoiceFragment(payload, payloadLen);
			return;
		} else if (payload[0] == kVoiceFetchMagic) {
			_HandleIncomingVoiceFetch(payload, payloadLen);
			return;
		}
	}

	_LogMessage("INFO", BString().SetToFormat(
		"Raw data received: SNR=%.1fdB RSSI=%ddBm payload=%zu bytes",
		snrDb, rssi, payloadLen));
}


void
MainWindow::_HandleDeviceInfo(const uint8* data, size_t length)
{
	if (length < 4) {
		_LogMessage("WARN", "RSP_DEVICE_INFO: frame too short");
		return;
	}

	// RSP_DEVICE_INFO format (per MeshCore Companion Protocol):
	// [0]     = code (0x0D)
	// [1]     = firmware_ver
	// [2]     = max_contacts_div_2 (v3+)
	// [3]     = max_channels (v3+)
	// [4-7]   = ble_pin (uint32 LE, v3+)
	// [8-19]  = firmware_build_date (null-terminated ASCII)
	// [20-59] = manufacturer_model (null-terminated ASCII)
	// [60-79] = semantic_version (null-terminated ASCII)

	BString info;
	info << "MeshCore Device Information\n\n";

	if (length >= 4) {
		uint8 fwVer = data[1];
		uint8 maxContacts = data[2] * 2;
		fMaxChannels = data[3];

		info << BString().SetToFormat("Protocol version: %d\n", fwVer);
		info << BString().SetToFormat("Max contacts: %d\nMax channels: %d\n",
			maxContacts, fMaxChannels);
	}

	if (length >= 20) {
		// Build date at offset 8
		char buildDate[16];
		memset(buildDate, 0, sizeof(buildDate));
		size_t dateLen = strnlen((const char*)data + 8, 12);
		memcpy(buildDate, data + 8, dateLen);
		info << "Build date: " << buildDate << "\n";
	}

	if (length >= 60) {
		// Board/manufacturer model at offset 20 (40 bytes)
		char board[42];
		memset(board, 0, sizeof(board));
		size_t boardLen = strnlen((const char*)data + 20, 40);
		if (boardLen > sizeof(board) - 1)
			boardLen = sizeof(board) - 1;
		memcpy(board, data + 20, boardLen);
		strlcpy(fDeviceBoard, board, sizeof(fDeviceBoard));
		info << "Board: " << fDeviceBoard << "\n";
	}

	if (length >= 61) {
		// Semantic version at offset 60
		char firmware[22];
		memset(firmware, 0, sizeof(firmware));
		size_t maxFwScan = length - 60;
		if (maxFwScan > 20)
			maxFwScan = 20;
		size_t fwLen = strnlen((const char*)data + 60, maxFwScan);
		memcpy(firmware, data + 60, fwLen);
		strlcpy(fDeviceFirmware, firmware, sizeof(fDeviceFirmware));
		info << "Firmware: " << fDeviceFirmware << "\n";
	}

	_LogMessage("INFO", info.String());

	_UpdateSidebarDeviceLabel();

	// Start enumerating channels if device supports them
	if (fMaxChannels > 0) {
		fChannels.MakeEmpty();
		fChannelEnumIndex = 0;
		fEnumeratingChannels = true;
		fProtocol->SendGetChannel(0);
	}
}


void
MainWindow::_HandleExportContact(const uint8* data, size_t length)
{
	if (length < 3) {
		_LogMessage("WARN", "RSP_EXPORT_CONTACT: frame too short");
		return;
	}

	// RSP_EXPORT_CONTACT (0x0B) contains "business card" data
	// Format: meshcore://{hex(card_data)}
	// Card data starts at offset 1, contains: public_key (32) + name + other info

	// RSP_EXPORT_CONTACT response - extract self public key and name
	// Card data format based on actual device output:
	// Offset 0: response code (0x0B)
	// Offset 1-2: type/flags (0x0011)
	// Offset 3-34: public key (32 bytes)
	// Offset 35+: other data (signature, etc)
	// Near end: device name as string

	// Extract public key from offset 3 (32 bytes = 64 hex chars)
	// Format: [0]=cmd, [1-2]=type, [3-34]=pubkey(32), [35+]=signature+name
	if (length >= 35) {
		FormatPubKeyFull(fPublicKey, data + 3);

		// Derive numeric node ID from first 4 pubkey bytes
		fSelfNodeId = ((uint32)data[3] << 24) | ((uint32)data[4] << 16)
			| ((uint32)data[5] << 8) | (uint32)data[6];

		_LogMessage("INFO", BString().SetToFormat("Self public key (64 chars): %.16s...", fPublicKey));

		// Update MQTT settings
		strlcpy(fMqttSettings.publicKey, fPublicKey, sizeof(fMqttSettings.publicKey));
		if (fMqttClient != NULL) {
			fMqttClient->SetSettings(fMqttSettings);
		}
	}

	// Extract name - scan from the end looking for printable string
	// The name "Haiku Node" was found at offset 104 in a 114-byte response
	if (length > 35) {
		// Search backwards from the end for a printable string
		size_t nameEnd = length;
		while (nameEnd > 35 && (data[nameEnd - 1] == 0 || data[nameEnd - 1] < 32))
			nameEnd--;

		if (nameEnd > 35) {
			size_t nameStart = nameEnd;
			while (nameStart > 35 && data[nameStart - 1] >= 32 && data[nameStart - 1] < 127)
				nameStart--;

			size_t nameLen = nameEnd - nameStart;
			if (nameLen >= 2 && nameLen < sizeof(fDeviceName)) {
				memcpy(fDeviceName, data + nameStart, nameLen);
				fDeviceName[nameLen] = '\0';
				_LogMessage("INFO", BString().SetToFormat("Self name: %s", fDeviceName));

				// Also update MQTT settings with device name
				strlcpy(fMqttSettings.deviceName, fDeviceName, sizeof(fMqttSettings.deviceName));
			}
		}
	}

	// Mark that we have device info
	if (fPublicKey[0] != '\0') {
		fHasDeviceInfo = true;

		// Handshake succeeded — cancel timeout
		delete fHandshakeTimer;
		fHandshakeTimer = NULL;

		// Update MQTT client with latest settings (including device name)
		if (fMqttClient != NULL) {
			fMqttClient->SetSettings(fMqttSettings);
		}

		// If MQTT is connected, publish status now
		if (fMqttClient != NULL && fMqttClient->IsConnected()) {
			_LogMessage("MQTT", "Self info received, publishing status now");
			fMqttClient->PublishStatus(fDeviceName, fDeviceFirmware, fDeviceBoard,
				fBatteryMv, fDeviceUptime, fNoiseFloor);
		}
	}
}


void
MainWindow::_HandleContactsStart(const uint8* data, size_t length)
{
	uint32 count = 0;
	if (length >= 5) {
		count = ReadLE32(data + 1);
	}
	_LogMessage("INFO", BString().SetToFormat("Receiving contacts (expected: %u)...", count));

	// Move existing contacts to temporary storage to preserve message history
	// They will be matched with incoming contacts in _HandleContact
	fOldContacts.MakeEmpty();
	while (fContacts.CountItems() > 0) {
		ContactInfo* contact = fContacts.RemoveItemAt(0);
		if (contact != NULL)
			fOldContacts.AddItem(contact);
	}
}


void
MainWindow::_HandleContact(const uint8* data, size_t length)
{
	if (length < kContactFrameSize) {
		_LogMessage("WARN", BString().SetToFormat(
			"Contact frame too short: %zu bytes", length));
		return;
	}

	ContactInfo* contact = new ContactInfo();
	memcpy(contact->publicKey, data + kContactPubKeyOffset, kPubKeySize);
	contact->type = data[kContactTypeOffset];
	contact->flags = data[kContactFlagsOffset];
	contact->outPathLen = (int8)data[kContactPathLenOffset];
	{
		int pathBytes = contact->outPathLen;  // 1 byte per hop hash
		if (pathBytes > 0 && pathBytes <= (int)kContactOutPathMaxSize)
			memcpy(contact->outPath, data + kContactOutPathOffset, pathBytes);
	}
	{
		char nameBuf[kContactNameSize + 1];
		memcpy(nameBuf, data + kContactNameOffset, kContactNameSize);
		nameBuf[kContactNameSize] = '\0';
		strlcpy(contact->name, nameBuf, sizeof(contact->name));
	}
	contact->lastSeen = ReadLE32(data + kContactLastSeenOffset);
	contact->latitude = ReadLE32Signed(data + kContactLatOffset);
	contact->longitude = ReadLE32Signed(data + kContactLonOffset);
	contact->isValid = true;

	// Look for existing contact to preserve message history
	for (int32 i = 0; i < fOldContacts.CountItems(); i++) {
		ContactInfo* old = fOldContacts.ItemAt(i);
		if (old != NULL && memcmp(old->publicKey, contact->publicKey, kPubKeyPrefixSize) == 0) {
			// Transfer message history from old contact
			while (old->messages.CountItems() > 0) {
				ChatMessage* msg = old->messages.RemoveItemAt(0);
				if (msg != NULL)
					contact->messages.AddItem(msg);
			}
			// Remove old contact from temp list (will be deleted at end)
			fOldContacts.RemoveItemAt(i);
			delete old;
			break;
		}
	}

	fContacts.AddItem(contact);

	// Save as Haiku People file
	_SaveContactAsPerson(contact);

	uint32 now = (uint32)time(NULL);
	uint32 age = (now > contact->lastSeen) ? (now - contact->lastSeen) : 0;

	// Log outPath hex for topology debugging
	BString pathHex;
	if (contact->outPathLen > 0) {
		pathHex << " outPath=[";
		for (int32 p = 0; p < contact->outPathLen && p < 16; p++) {
			if (p > 0) pathHex << ",";
			pathHex << BString().SetToFormat("%02X", contact->outPath[p]);
		}
		pathHex << "]";
	}
	_LogMessage("INFO", BString().SetToFormat("Contact: %s (type:%d flags:0x%02X path:%d lastSeen:%u age:%us)%s",
		contact->name[0] ? contact->name : "(unnamed)",
		contact->type, contact->flags, contact->outPathLen, contact->lastSeen, age,
		pathHex.String()));
}


void
MainWindow::_HandleContactsEnd(const uint8* data, size_t length)
{
	fSyncingContacts = false;

	// Clean up any remaining old contacts that weren't matched
	fOldContacts.MakeEmpty();

	_UpdateContactList();
	_LogMessage("OK", BString().SetToFormat("Received %d contacts",
		(int)fContacts.CountItems()));

	// Load saved messages after contacts are available
	_LoadMessages();

	// Re-select logged-in contact after list rebuild (login + resync race)
	if (fLoggedIn) {
		for (int32 i = 1; i < fContactList->CountItems(); i++) {
			ContactItem* item = dynamic_cast<ContactItem*>(
				fContactList->ItemAt(i));
			if (item != NULL && !item->IsChannel() &&
				memcmp(item->GetContact().publicKey,
					fLoggedInKey, kPubKeyPrefixSize) == 0) {
				fContactList->Select(i);
				_SelectContact(i);
				break;
			}
		}
	}

	// Start offline message sync to drain any queued messages
	fProtocol->SendSyncNextMessage();

	// Trigger immediate repeater map update now that contacts are available
	if (fCardView != NULL && fCardView->CardLayout()->VisibleIndex() == 1)
		_UpdateRepeaterMap();

	// Forward contact counts to Mission Control
	if (_LockIfVisible(fMissionControlWindow)) {
		uint32 now = (uint32)time(NULL);
		int32 total = fContacts.CountItems();
		int32 online = 0;
		int32 recent = 0;
		for (int32 i = 0; i < total; i++) {
			ContactInfo* c = fContacts.ItemAt(i);
			if (c != NULL && c->lastSeen > 0) {
				uint32 age = (now > c->lastSeen) ? (now - c->lastSeen) : 0;
				if (age < 300)
					online++;
				else if (age < 900)
					recent++;
			}
		}
		fMissionControlWindow->UpdateContacts(total, online, recent);

		// Build topology node list + heatmap data
		if (total > 0) {
			int32 nodeCount = total < 32 ? total : 32;
			TopoNode* nodes = new TopoNode[nodeCount];
			int32 heatmapCount = total < 50 ? total : 50;
			int8* snrValues = new int8[heatmapCount];
			uint8* statuses = new uint8[heatmapCount];

			for (int32 i = 0; i < (total < 50 ? total : 50); i++) {
				ContactInfo* c = fContacts.ItemAt(i);
				if (c == NULL) continue;

				uint32 age = (now > c->lastSeen)
					? (now - c->lastSeen) : 0;
				uint8 status;
				if (c->lastSeen == 0)
					status = 0;
				else if (age < 300)
					status = 2;
				else if (age < 900)
					status = 1;
				else
					status = 0;

				// Last incoming message SNR
				int8 lastSnr = 0;
				for (int32 j = c->messages.CountItems() - 1;
					j >= 0; j--) {
					ChatMessage* msg = c->messages.ItemAt(j);
					if (msg != NULL && !msg->isOutgoing
						&& msg->snr != 0) {
						lastSnr = msg->snr;
						break;
					}
				}

				// Topology node
				if (i < nodeCount) {
					snprintf(nodes[i].name,
						sizeof(nodes[i].name),
						"%.31s", c->name);
					nodes[i].snr = lastSnr;
					nodes[i].status = status;

					// Collect SNR sparkline from last N
					// incoming messages
					nodes[i].snrHistoryCount = 0;
					int32 msgCount = c->messages.CountItems();
					for (int32 j = msgCount - 1;
						j >= 0 && nodes[i].snrHistoryCount
							< kSparklinePoints; j--) {
						ChatMessage* msg =
							c->messages.ItemAt(j);
						if (msg != NULL && !msg->isOutgoing
							&& msg->snr != 0) {
							nodes[i].snrHistory[
								nodes[i].snrHistoryCount++]
								= msg->snr;
						}
					}
					// Reverse to chronological order
					for (int32 a = 0,
						b = nodes[i].snrHistoryCount - 1;
						a < b; a++, b--) {
						int8 tmp = nodes[i].snrHistory[a];
						nodes[i].snrHistory[a] =
							nodes[i].snrHistory[b];
						nodes[i].snrHistory[b] = tmp;
					}
				}

				// Heatmap data
				snrValues[i] = lastSnr;
				statuses[i] = status;
			}

			fMissionControlWindow->SetContactNodes(nodes,
				nodeCount);
			fMissionControlWindow->SetContactHeatmap(
				snrValues, statuses, heatmapCount);

			delete[] nodes;
			delete[] snrValues;
			delete[] statuses;
		}

		fMissionControlWindow->UnlockLooper();
	}

	// Refresh NetworkMapWindow edges even when hidden — keep topology alive
	if (fNetworkMapWindow != NULL && fNetworkMapWindow->LockLooper()) {
		fNetworkMapWindow->UpdateFromContacts(&fContacts);
		fNetworkMapWindow->UnlockLooper();
	}
}


void
MainWindow::_HandleSelfInfo(const uint8* data, size_t length)
{
	if (length < 2) {
		_LogMessage("WARN", "RSP_SELF_INFO: frame too short");
		return;
	}

	_LogMessage("INFO", BString().SetToFormat("Received self info, len=%zu", length));

	// RSP_SELF_INFO format (per MeshCore Companion Protocol):
	// [0]     = code (0x05)
	// [1]     = type (adv_type: 0=NONE, 1=CHAT, 2=REPEATER, 3=ROOM)
	// [2]     = tx_power_dbm
	// [3]     = max_tx_power
	// [4-35]  = public_key (32 bytes)
	// [36-39] = adv_lat (int32 LE, × 1E6)
	// [40-43] = adv_lon (int32 LE, × 1E6)
	// [44]    = multi_acks
	// [45]    = advert_loc_policy
	// [46]    = telemetry_modes
	// [47]    = manual_add_contacts
	// [48-51] = radio_freq (uint32 LE, Hz)
	// [52-55] = radio_bw (uint32 LE, Hz)
	// [56]    = radio_sf
	// [57]    = radio_cr
	// [58+]   = name (null-terminated string)

	if (length >= 36) {
		// Extract type and power info
		uint8 advType = data[1];
		fDeviceAdvType = advType;
		fRadioTxPower = data[2];
		uint8 maxTxPower = data[3];
		_LogMessage("INFO", BString().SetToFormat(
			"Self type:%d txPower:%d maxTxPower:%d", advType, fRadioTxPower, maxTxPower));

		// Auto-switch to repeater monitor for repeater/room devices
		if (advType == 2 || advType == 3)
			_SwitchToRepeaterMode();

		// Extract and store public key as hex string (offset 4-35)
		FormatPubKeyFull(fPublicKey, data + 4);

		// Derive numeric node ID from first 4 pubkey bytes
		fSelfNodeId = ((uint32)data[4] << 24) | ((uint32)data[5] << 16)
			| ((uint32)data[6] << 8) | (uint32)data[7];

		// Update MQTT settings with public key
		strlcpy(fMqttSettings.publicKey, fPublicKey, sizeof(fMqttSettings.publicKey));
		if (fMqttClient != NULL) {
			fMqttClient->SetSettings(fMqttSettings);
		}

		_LogMessage("INFO", BString().SetToFormat("Public key: %.12s...", fPublicKey));
	}

	if (length >= 44) {
		// Extract GPS coordinates
		int32 latRaw = ReadLE32Signed(data + 36);
		int32 lonRaw = ReadLE32Signed(data + 40);
		if (latRaw != 0 || lonRaw != 0) {
			double lat = latRaw / 1000000.0;
			double lon = lonRaw / 1000000.0;
			_LogMessage("INFO", BString().SetToFormat(
				"Self GPS: %.6f, %.6f", lat, lon));
		}
	}

	if (length >= 48) {
		// Extract other params (bytes 44-47)
		fMultiAcks = data[44];
		fAdvertLocPolicy = data[45];
		fTelemetryModes = data[46];
		fManualAddContacts = data[47];
		_LogMessage("INFO", BString().SetToFormat(
			"Other params: locPolicy=%d multiAcks=%d telemetry=%d manualAdd=%d",
			fAdvertLocPolicy, fMultiAcks, fTelemetryModes, fManualAddContacts));
	}

	if (length >= 58) {
		// Extract current radio parameters
		// RSP_SELF_INFO returns freq in kHz, BW in Hz (same as wire format)
		// We store everything in Hz internally
		uint32 freqKHz = ReadLE32(data + 48);
		fRadioFreq = freqKHz * 1000;  // kHz → Hz
		fRadioBw = ReadLE32(data + 52);
		fRadioSf = data[56];
		fRadioCr = data[57];
		fHasRadioParams = true;
		_LogMessage("INFO", BString().SetToFormat(
			"Radio: %.3f MHz, %.1f kHz BW, SF%u, CR%u",
			fRadioFreq / 1000000.0, fRadioBw / 1000.0, fRadioSf, fRadioCr));
	}

	if (length > 58) {
		// Extract device name (null-terminated string at offset 58)
		char tempName[64];
		size_t maxLen = length - 58;
		if (maxLen > sizeof(tempName) - 1)
			maxLen = sizeof(tempName) - 1;
		memcpy(tempName, data + 58, maxLen);
		tempName[maxLen] = '\0';
		strlcpy(fDeviceName, tempName, sizeof(fDeviceName));

		_LogMessage("INFO", BString().SetToFormat("Device name: %s", fDeviceName));

		// Update Network Map with self name
		if (_LockIfVisible(fNetworkMapWindow)) {
			fNetworkMapWindow->SetSelfInfo(fDeviceName);
			fNetworkMapWindow->UnlockLooper();
		}
	}

	// Forward radio config + connection to Mission Control
	if (_LockIfVisible(fMissionControlWindow)) {
		fMissionControlWindow->SetConnectionState(fConnected,
			fDeviceName, fDeviceFirmware);
		if (fHasRadioParams)
			fMissionControlWindow->SetRadioConfig(fRadioFreq,
				fRadioBw, fRadioSf, fRadioCr, fRadioTxPower);
		fMissionControlWindow->UnlockLooper();
	}

	// Mark that we have device info and publish MQTT status if connected
	fHasDeviceInfo = true;

	// Handshake succeeded — cancel timeout
	delete fHandshakeTimer;
	fHandshakeTimer = NULL;

	if (fMqttClient != NULL && fMqttClient->IsConnected()) {
		_LogMessage("MQTT", "Device info received, publishing status now");
		fMqttClient->PublishStatus(fDeviceName, fDeviceFirmware, fDeviceBoard,
			fBatteryMv, fDeviceUptime, fNoiseFloor);
	}

	_UpdateSidebarDeviceLabel();
}


void
MainWindow::_HandleContactMsgRecv(const uint8* data, size_t length, bool isV3)
{
	fTopBar->FlashRx();

	// V2 format: [0]=code [1-6]=pubkey [7]=pathLen [8]=txtType [9-12]=timestamp [13+]=text
	// V3 format: [0]=code [1]=snr [2-3]=reserved [4-9]=pubkey [10]=pathLen [11]=txtType
	//            [12-15]=timestamp [16+]=text

	const uint8* senderPrefix;
	uint8 pathLen;
	int8 snr;
	uint8 txtType;
	uint32 timestamp;
	size_t textOffset;

	if (isV3) {
		if (length < 16) {
			_LogMessage("WARN", "V3 contact message frame too short");
			return;
		}
		snr = (int8)data[kV3DmSnrOffset];
		senderPrefix = data + kV3DmSenderOffset;
		pathLen = data[kV3DmPathLenOffset];
		txtType = data[kV3DmTxtTypeOffset];
		timestamp = ReadLE32(data + kV3DmTimestampOffset);
		textOffset = kV3DmTextOffset;
	} else {
		if (length < kV2DmMinLength) {
			_LogMessage("WARN", "Contact message frame too short");
			return;
		}
		senderPrefix = data + kV2DmSenderOffset;
		pathLen = data[kV2DmPathLenOffset];
		snr = 0;  // V2 does not include SNR
		txtType = data[kV2DmTxtTypeOffset];
		timestamp = ReadLE32(data + kV2DmTimestampOffset);
		textOffset = kV2DmTextOffset;
	}

	size_t textLen = length - textOffset;
	char text[256];
	if (textLen > 255) textLen = 255;
	if (textLen > 0) memcpy(text, data + textOffset, textLen);
	text[textLen] = '\0';

	ContactInfo* sender = _FindContactByPrefix(senderPrefix, 6);
	BString senderName;
	if (sender != NULL) {
		senderName = sender->name;
	} else {
		senderName.SetToFormat("%02X%02X%02X...",
			senderPrefix[0], senderPrefix[1], senderPrefix[2]);
	}

	if (pathLen == kPathLenDirect || pathLen == 0) {
		_LogMessage("MSG", BString().SetToFormat("DM from %s [direct, SNR:%d]: %s",
			senderName.String(), snr, text));
	} else {
		_LogMessage("MSG", BString().SetToFormat("DM from %s [%d hops, SNR:%d]: %s",
			senderName.String(), pathLen, snr, text));
	}

	// Sanitize timestamp: if sender clock is off by more than 1 day,
	// use local time instead (avoids wrong ordering and pruning issues)
	uint32 now = (uint32)time(NULL);
	if (timestamp < now - 86400 || timestamp > now + 3600)
		timestamp = now;

	// Create ChatMessage
	ChatMessage chatMsg;
	memcpy(chatMsg.pubKeyPrefix, senderPrefix, kPubKeyPrefixSize);
	chatMsg.pathLen = pathLen;
	chatMsg.snr = snr;
	chatMsg.timestamp = timestamp;
	strlcpy(chatMsg.text, text, sizeof(chatMsg.text));
	chatMsg.isOutgoing = false;
	chatMsg.isChannel = false;
	chatMsg.txtType = txtType;

	// Add to chat if this contact is selected
	if (fChatView->CurrentContact() != NULL) {
		if (memcmp(fChatView->CurrentContact()->publicKey, senderPrefix,
				kPubKeyPrefixSize) == 0) {
			fChatView->AddMessage(chatMsg, senderName.String());
			// Update header with connection info
			fChatHeader->SetConnectionInfo(pathLen, snr);
		}
	}

	// Update contact item with last message
	ContactItem* item = _FindContactItemByPrefix(senderPrefix);
	if (item != NULL) {
		item->SetLastMessage(text, timestamp);
		// Increment unread if not currently viewing this contact (skip if muted)
		char muteCheckHex[kContactHexSize];
		FormatContactKey(muteCheckHex, senderPrefix);
		if (!_IsMuted(muteCheckHex)
			&& (fChatView->CurrentContact() == NULL
				|| memcmp(fChatView->CurrentContact()->publicKey, senderPrefix,
					kPubKeyPrefixSize) != 0)) {
			item->IncrementUnread();
		}
		fContactList->Invalidate();
	}

	// Always persist to database (even during contact sync when sender
	// may be temporarily NULL because fContacts is being rebuilt)
	char contactHex[kContactHexSize];
	FormatContactKey(contactHex, senderPrefix);
	DatabaseManager* db = DatabaseManager::Instance();
	db->InsertMessage(contactHex, chatMsg);

	// Check for VE2 voice envelope — create incoming session
	if (VoiceSessionManager::IsVoiceEnvelope(text)) {
		VoiceSession* voiceSession =
			fVoiceSessions->CreateFromEnvelope(text);
		if (voiceSession != NULL) {
			_LogMessage("VOICE", BString().SetToFormat(
				"Received voice envelope: session %08x, %lds, %d fragments",
				voiceSession->sessionId,
				(long)voiceSession->durationSec,
				(int)voiceSession->totalFragments));
		}
	}
	// Check for IE2 image envelope — auto-fetch fragments
	else if (ImageSessionManager::IsImageEnvelope(text)) {
		ImageSession* imgSession = fImageSessions->CreateFromEnvelope(text);
		if (imgSession != NULL) {
			_LogMessage("IMG", BString().SetToFormat(
				"Received image envelope: session %08x, %dx%d, %d fragments",
				imgSession->sessionId, (int)imgSession->width,
				(int)imgSession->height, (int)imgSession->totalFragments));
			_StartImageFetch(imgSession->sessionId);
		}
	}

	// Check for GIF message — auto-download
	if (GiphyClient::IsGifMessage(text)) {
		// Find the MessageView that was just added
		int32 lastIndex = fChatView->CountItems() - 1;
		if (lastIndex >= 0) {
			MessageView* mv = dynamic_cast<MessageView*>(
				fChatView->ItemAt(lastIndex));
			if (mv != NULL && mv->IsGifMessage()) {
				char gifId[64];
				if (GiphyClient::ExtractGifId(text, gifId, sizeof(gifId))) {
					mv->SetGifLoadState(1);
					fChatView->InvalidateItem(lastIndex);
					_DownloadAndDisplayGif(gifId, lastIndex);
				}
			}
		}
	}

	// Store message in contact's in-memory history and update lastSeen
	if (sender != NULL) {
		ChatMessage* stored = new ChatMessage(chatMsg);
		sender->messages.AddItem(stored);
		sender->lastSeen = (uint32)time(NULL);

		// Record SNR data point for historical charting
		if (snr != 0 || pathLen != kPathLenDirect) {
			db->InsertSNRDataPoint(contactHex,
				(uint32)time(NULL), snr, fLastRssi, pathLen);

			// Refresh SNR chart if this contact is displayed
			const ContactInfo* displayed = fInfoPanel->GetContact();
			if (displayed != NULL
				&& memcmp(displayed->publicKey, senderPrefix,
					kPubKeyPrefixSize) == 0) {
				fInfoPanel->RefreshSNRChart();
			}
		}
	}

	// Desktop notification if window not active and not muted
	{
		char notifyHex[kContactHexSize];
		FormatContactKey(notifyHex, senderPrefix);
		if (!IsActive() && !_IsMuted(notifyHex)) {
			NotificationManager::Instance()->NotifyNewMessage(
				senderName.String(), text, false);
		}
	}

	// Trigger pulse and update link quality on network map
	if (fNetworkMapWindow != NULL && fNetworkMapWindow->LockLooper()) {
		fNetworkMapWindow->TriggerNodePulse(senderPrefix);
		if (snr != 0)
			fNetworkMapWindow->UpdateLinkQuality(senderPrefix, snr, fLastRssi);
		fNetworkMapWindow->UnlockLooper();
	}

	// Check for SAR marker and forward to GeoMap
	{
		SarMarker sarMarker;
		if (ParseSarMarker(text, sarMarker))
			_ForwardSarMarkerToMap(sarMarker, senderName.String());
	}

	// Auto-trace when message hop count disagrees with stored outPath
	if (sender != NULL && pathLen != kPathLenDirect && pathLen > 0
		&& sender->outPathLen == 0 && fConnected) {
		_LogMessage("INFO", BString().SetToFormat(
			"Path mismatch: %s shows %d hops but outPathLen=%d — tracing",
			senderName.String(), (int)pathLen, (int)sender->outPathLen));
		fProtocol->SendTracePath(sender);
	}

	// Forward to Mission Control activity feed
	if (_LockIfVisible(fMissionControlWindow)) {
		BString eventText;
		eventText.SetToFormat("DM from %s — SNR: %ddB, %s",
			senderName.String(), (int)snr,
			(pathLen == kPathLenDirect || pathLen == 0)
				? "direct" : BString().SetToFormat("%d hops",
					(int)pathLen).String());
		fMissionControlWindow->AddActivityEvent("MSG",
			eventText.String());
		if (snr != 0)
			fMissionControlWindow->AddSNRDataPoint(snr);
		fMissionControlWindow->UnlockLooper();
	}

	// Publish to MQTT /packets topic
	if (fMqttClient != NULL && fMqttClient->IsConnected()) {
		fMqttClient->PublishPacket(timestamp, snr, fLastRssi,
			"DM", senderPrefix, 6,
			(const uint8*)text, textLen);
	}

	// Continue offline message sync loop
	if (fSyncingMessages)
		fProtocol->SendSyncNextMessage();
}


void
MainWindow::_HandleChannelMsgRecv(const uint8* data, size_t length, bool isV3)
{
	fTopBar->FlashRx();

	// V2 format: [0]=code [1]=channelIdx [2]=pathLen [3]=txtType
	//            [4-7]=timestamp [8+]=text (includes "SenderName: message")
	// V3 format: [0]=code [1]=snr [2-3]=reserved [4]=channelIdx [5]=pathLen
	//            [6]=txtType [7-10]=timestamp [11+]=text

	uint8 channelIdx;
	uint8 pathLen;
	int8 snr;
	uint32 timestamp;
	size_t textOffset;

	if (isV3) {
		if (length < 11) {
			_LogMessage("WARN", "V3 channel message frame too short");
			return;
		}
		snr = (int8)data[kV3ChSnrOffset];
		channelIdx = data[kV3ChChannelOffset];
		pathLen = data[kV3ChPathLenOffset];
		// data[kV3ChTxtTypeOffset] = txt_type
		timestamp = ReadLE32(data + kV3ChTimestampOffset);
		textOffset = kV3ChTextOffset;
	} else {
		if (length < kV2ChMinLength) {
			_LogMessage("WARN", "Channel message frame too short");
			return;
		}
		channelIdx = data[kV2ChChannelOffset];
		pathLen = data[kV2ChPathLenOffset];
		snr = 0;  // V2 does not include SNR
		// data[kV2ChTxtTypeOffset] = txt_type
		timestamp = ReadLE32(data + kV2ChTimestampOffset);
		textOffset = kV2ChTextOffset;
	}
	size_t textLen = length - textOffset;
	char fullText[256];
	if (textLen > 255) textLen = 255;
	if (textLen > 0) memcpy(fullText, data + textOffset, textLen);
	fullText[textLen] = '\0';

	// Parse sender name from text (format: "SenderName: message")
	BString senderName = "Channel";
	char messageText[256];
	strlcpy(messageText, fullText, sizeof(messageText));

	const char* colonPos = strstr(fullText, ": ");
	if (colonPos != NULL && colonPos - fullText < 32) {
		// Extract sender name (before ": ")
		size_t nameLen = colonPos - fullText;
		char nameBuf[33];
		memcpy(nameBuf, fullText, nameLen);
		nameBuf[nameLen] = '\0';
		senderName = nameBuf;

		// Extract message (after ": ")
		strlcpy(messageText, colonPos + 2, sizeof(messageText));
	}

	// Sanitize timestamp from sender's clock
	{
		uint32 now = (uint32)time(NULL);
		if (timestamp < now - 86400 || timestamp > now + 3600)
			timestamp = now;
	}

	_LogMessage("MSG", BString().SetToFormat("CHANNEL from %s: %s",
		senderName.String(), messageText));

	// Try to find contact by name to get pubkey prefix
	ContactInfo* sender = NULL;
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* c = fContacts.ItemAt(i);
		if (c != NULL && strcmp(c->name, senderName.String()) == 0) {
			sender = c;
			sender->lastSeen = (uint32)time(NULL);
			break;
		}
	}

	// Create and store ChatMessage
	ChatMessage chatMsg;
	if (sender != NULL) {
		memcpy(chatMsg.pubKeyPrefix, sender->publicKey, kPubKeyPrefixSize);
	} else {
		memset(chatMsg.pubKeyPrefix, 0, kPubKeyPrefixSize);
	}
	chatMsg.pathLen = pathLen;
	chatMsg.snr = snr;
	chatMsg.timestamp = timestamp;
	strlcpy(chatMsg.text, messageText, sizeof(chatMsg.text));
	chatMsg.isOutgoing = false;
	chatMsg.isChannel = true;

	ChatMessage* stored = new ChatMessage(chatMsg);
	DatabaseManager* db = DatabaseManager::Instance();

	// Store in appropriate channel list
	if (channelIdx > 0) {
		// Private channel
		bool added = false;
		for (int32 i = 0; i < fChannels.CountItems(); i++) {
			ChannelInfo* ch = fChannels.ItemAt(i);
			if (ch->index == channelIdx) {
				ch->messages.AddItem(stored);
				added = true;
				break;
			}
		}
		if (!added)
			delete stored;
		BString dbKey;
		dbKey.SetToFormat("channel_%d", channelIdx);
		db->InsertMessage(dbKey.String(), chatMsg);
	} else {
		// Public channel
		fChannelMessages.AddItem(stored);
		db->InsertMessage("channel", chatMsg);
	}

	// Check for VE2 voice envelope
	if (VoiceSessionManager::IsVoiceEnvelope(messageText)) {
		VoiceSession* voiceSession =
			fVoiceSessions->CreateFromEnvelope(messageText);
		if (voiceSession != NULL) {
			_LogMessage("VOICE", BString().SetToFormat(
				"Received channel voice envelope: session %08x",
				voiceSession->sessionId));
		}
	}
	// Check for IE2 image envelope — auto-fetch fragments
	else if (ImageSessionManager::IsImageEnvelope(messageText)) {
		ImageSession* imgSession =
			fImageSessions->CreateFromEnvelope(messageText);
		if (imgSession != NULL) {
			_LogMessage("IMG", BString().SetToFormat(
				"Received channel image envelope: session %08x",
				imgSession->sessionId));
			_StartImageFetch(imgSession->sessionId);
		}
	}

	// Check for GIF message — auto-download
	if (GiphyClient::IsGifMessage(messageText)) {
		int32 lastIndex = fChatView->CountItems() - 1;
		if (lastIndex >= 0) {
			MessageView* mv = dynamic_cast<MessageView*>(
				fChatView->ItemAt(lastIndex));
			if (mv != NULL && mv->IsGifMessage()) {
				char gifId[64];
				if (GiphyClient::ExtractGifId(messageText,
					gifId, sizeof(gifId))) {
					mv->SetGifLoadState(1);
					fChatView->InvalidateItem(lastIndex);
					_DownloadAndDisplayGif(gifId, lastIndex);
				}
			}
		}
	}

	// Record SNR data point for sender contact
	if (sender != NULL && snr != 0) {
		char senderHex[kContactHexSize];
		FormatContactKey(senderHex, sender->publicKey);
		db->InsertSNRDataPoint(senderHex,
			(uint32)time(NULL), snr, fLastRssi, pathLen);

		// Refresh SNR chart if this sender is displayed
		const ContactInfo* displayed = fInfoPanel->GetContact();
		if (displayed != NULL
			&& memcmp(displayed->publicKey, sender->publicKey,
				kPubKeyPrefixSize) == 0) {
			fInfoPanel->RefreshSNRChart();
		}
	}

	// Add to chat if this channel is currently selected
	bool isCurrentChannel = false;
	if (fSendingToChannel) {
		if (channelIdx == 0 && fSelectedChannelIdx < 0)
			isCurrentChannel = true;  // Viewing public, message is public
		else if (channelIdx > 0 && fSelectedChannelIdx == (int32)channelIdx)
			isCurrentChannel = true;  // Viewing this private channel
	}
	if (isCurrentChannel)
		fChatView->AddMessage(chatMsg, senderName.String());

	// Build channel mute key
	char channelMuteKey[16];
	if (channelIdx == 0)
		strlcpy(channelMuteKey, "ch_public", sizeof(channelMuteKey));
	else
		snprintf(channelMuteKey, sizeof(channelMuteKey), "ch_%d",
			(int)channelIdx);
	bool channelMuted = _IsMuted(channelMuteKey);

	// Update sidebar item
	if (channelIdx == 0) {
		fChannelItem->SetLastMessage(messageText, timestamp);
		if (!isCurrentChannel && !channelMuted)
			fChannelItem->IncrementUnread();
		fContactList->InvalidateItem(0);
	} else {
		// Update private channel sidebar item
		for (int32 i = 1; i < fContactList->CountItems(); i++) {
			ContactItem* ci = dynamic_cast<ContactItem*>(fContactList->ItemAt(i));
			if (ci != NULL && ci->IsChannel()
				&& ci->ChannelIndex() == (int32)channelIdx) {
				ci->SetLastMessage(messageText, timestamp);
				if (!isCurrentChannel && !channelMuted)
					ci->IncrementUnread();
				fContactList->InvalidateItem(i);
				break;
			}
		}
	}

	// Check for SAR marker and forward to GeoMap
	{
		SarMarker sarMarker;
		if (ParseSarMarker(messageText, sarMarker))
			_ForwardSarMarkerToMap(sarMarker, senderName.String());
	}

	// Desktop notification if window not active and channel not muted
	if (!IsActive() && !channelMuted) {
		NotificationManager::Instance()->NotifyNewMessage(
			senderName.String(), messageText, true);
	}

	// Forward to Mission Control activity feed
	if (_LockIfVisible(fMissionControlWindow)) {
		BString eventText;
		eventText.SetToFormat("CH from %s: %s",
			senderName.String(), messageText);
		fMissionControlWindow->AddActivityEvent("MSG",
			eventText.String());
		if (snr != 0)
			fMissionControlWindow->AddSNRDataPoint(snr);
		fMissionControlWindow->UnlockLooper();
	}

	// Publish to MQTT /packets topic
	if (fMqttClient != NULL && fMqttClient->IsConnected()) {
		const uint8* fromKey = (sender != NULL) ? sender->publicKey : (const uint8*)"\0\0\0\0\0\0";
		fMqttClient->PublishPacket(timestamp, 0, fLastRssi,
			"CH", fromKey, 6,
			(const uint8*)messageText, strlen(messageText));
	}

	// Continue offline message sync loop
	if (fSyncingMessages)
		fProtocol->SendSyncNextMessage();
}


void
MainWindow::_HandleBattAndStorage(const uint8* data, size_t length)
{
	// RSP_BATT_AND_STORAGE format:
	// [0]   = code (0x0C)
	// [1-2] = milli_volts (uint16 LE)
	// [3-6] = used_kb (uint32 LE, optional)
	// [7-10]= total_kb (uint32 LE, optional)

	if (length >= 3) {
		uint16 battMv = ReadLE16(data + kBattMvOffset);
		fBatteryMv = battMv;

		uint32 usedKb = 0;
		uint32 totalKb = 0;
		if (length >= 7)
			usedKb = ReadLE32(data + kStorageUsedOffset);
		if (length >= 11)
			totalKb = ReadLE32(data + kStorageTotalOffset);

		// Calculate storage percentage for status bar
		int8 storagePct = 0;
		if (totalKb > 0)
			storagePct = (int8)(((uint64)usedKb * 100) / totalKb);

		_LogMessage("INFO", BString().SetToFormat(
			"Battery: %u mV, Storage: %u/%u KB (%d%%)",
			battMv, usedKb, totalKb, storagePct));

		fTopBar->SetBattery(battMv);

		// Forward battery voltage to TelemetryWindow as sensor data
		if (fTelemetryWindow != NULL) {
			if (fTelemetryWindow->LockLooper()) {
				const char* name = fDeviceName[0] != '\0' ? fDeviceName : NULL;
				float batteryV = battMv / 1000.0f;
				fTelemetryWindow->AddTelemetryData(fSelfNodeId, "Battery",
					SENSOR_BATTERY, batteryV, "V", name);
				if (totalKb > 0) {
					float storagePctF = (usedKb * 100.0f) / totalKb;
					fTelemetryWindow->AddTelemetryData(fSelfNodeId, "Storage",
						SENSOR_CUSTOM, storagePctF, "%", name);
				}
				fTelemetryWindow->UnlockLooper();
			}
		}

		// Forward to Mission Control
		if (_LockIfVisible(fMissionControlWindow)) {
			fMissionControlWindow->SetBatteryInfo(battMv, usedKb,
				totalKb);
			int32 pct = BatteryPercent(battMv,
				(BatteryChemistry)fBatteryType);
			BString battEvent;
			battEvent.SetToFormat("Battery: %d%% (%u mV)",
				(int)pct, (unsigned)battMv);
			fMissionControlWindow->AddActivityEvent("SYS",
				battEvent.String());
			fMissionControlWindow->UnlockLooper();
		}

	}
}


void
MainWindow::_HandleStats(const uint8* data, size_t length)
{
	if (length < 2)
		return;

	uint8 subType = data[kStatsCoreSubtypeOffset];

	// Parse stats for status bar (per MeshCore Companion Protocol)
	switch (subType) {
		case kStatsSubtypeCore:
			// [2-3]=batt_mv(int16) [4-7]=uptime(uint32) [8-9]=err_flags [10]=queue_len
			if (length >= 8) {
				fDeviceUptime = ReadLE32(data + kStatsCoreUptimeOffset);
				fTopBar->SetUptime(fDeviceUptime);
			}
			if (length >= 4) {
				uint16 battMv = ReadLE16(data + kStatsCoreBattOffset);
				if (battMv > 0) {
					fBatteryMv = battMv;
				}
			}
			break;

		case kStatsSubtypeRadio:
			// [2-3]=noise_floor(int16) [4]=rssi(int8) [5]=snr(int8)
			// [6-9]=tx_air_time [10-13]=rx_air_time
			if (length >= 6) {
				fNoiseFloor = (int8)ReadLE16(data + kStatsRadioNoiseOffset);
				fLastRssi = (int8)data[kStatsRadioRssiOffset];
				fLastSnr = (int8)data[kStatsRadioSnrOffset];
				fTopBar->SetRadioStats(fLastRssi, fLastSnr, fTxPackets, fRxPackets);

				// Forward radio stats to TelemetryWindow as sensor data
				if (fTelemetryWindow != NULL) {
					if (fTelemetryWindow->LockLooper()) {
						const char* name = fDeviceName[0] != '\0' ? fDeviceName : NULL;
						fTelemetryWindow->AddTelemetryData(fSelfNodeId, "Noise Floor",
							SENSOR_CUSTOM, (float)fNoiseFloor, "dBm", name);
						fTelemetryWindow->AddTelemetryData(fSelfNodeId, "RSSI",
							SENSOR_CUSTOM, (float)fLastRssi, "dBm", name);
						fTelemetryWindow->AddTelemetryData(fSelfNodeId, "SNR",
							SENSOR_CUSTOM, (float)fLastSnr, "dB", name);
						fTelemetryWindow->UnlockLooper();
					}
				}
			}
			break;

		case kStatsSubtypePackets:
			// [2-5]=recvPkts [6-9]=sentPkts [10-13]=sentFlood [14-17]=sentDirect
			// [18-21]=recvFlood [22-25]=recvDirect
			if (length >= 10) {
				fRxPackets = ReadLE32(data + kStatsPacketsRxOffset);
				fTxPackets = ReadLE32(data + kStatsPacketsTxOffset);
				fTopBar->SetRadioStats(fLastRssi, fLastSnr, fTxPackets, fRxPackets);
			}
			break;
	}

	// Forward to StatsWindow if open (must lock the window first!)
	if (_LockIfVisible(fStatsWindow)) {
		fStatsWindow->ParseStatsResponse(data, length);
		fStatsWindow->UnlockLooper();
	}

	// Forward to Mission Control dashboard
	if (_LockIfVisible(fMissionControlWindow)) {
		switch (subType) {
			case 0:  // Core stats
				fMissionControlWindow->SetDeviceStats(fDeviceUptime,
					fTxPackets, fRxPackets);
				break;
			case 1:  // Radio stats
			{
				fMissionControlWindow->SetRadioStats(fLastRssi,
					fLastSnr, fNoiseFloor);
				fMissionControlWindow->AddRSSIDataPoint(fLastRssi);
				BString radioEvent;
				radioEvent.SetToFormat(
					"Radio update: RSSI %ddBm, SNR %+ddB, NF %ddBm",
					(int)fLastRssi, (int)fLastSnr, (int)fNoiseFloor);
				fMissionControlWindow->AddActivityEvent("SYS",
					radioEvent.String());
				break;
			}
			case 2:  // Packet stats
			{
				fMissionControlWindow->SetPacketStats(fTxPackets,
					fRxPackets);
				BString pktEvent;
				pktEvent.SetToFormat(
					"Packets: TX %u, RX %u",
					(unsigned)fTxPackets, (unsigned)fRxPackets);
				fMissionControlWindow->AddActivityEvent("SYS",
					pktEvent.String());
				break;
			}
		}
		fMissionControlWindow->UnlockLooper();
	}

}


void
MainWindow::_HandlePushMsgWaiting(const uint8* data, size_t length)
{
	_LogMessage("INFO", "Messages waiting - fetching...");
	fProtocol->SendSyncNextMessage();
}
void
MainWindow::_HandleChannelInfo(const uint8* data, size_t length)
{
	// RSP_CHANNEL_INFO format:
	// [0] = code (0x12)
	// [1] = channel_idx
	// [2-33] = name (32 bytes, null-terminated)
	// [34-49] = secret (16 bytes PSK)

	if (length < 50)
		return;

	uint8 idx = data[1];
	char name[33];
	memset(name, 0, sizeof(name));
	memcpy(name, data + 2, 32);

	if (name[0] != '\0') {
		// Non-empty channel — add to list
		ChannelInfo* channel = new ChannelInfo();
		channel->index = idx;
		strlcpy(channel->name, name, sizeof(channel->name));
		memcpy(channel->secret, data + 34, 16);
		fChannels.AddItem(channel);

		_LogMessage("INFO", BString().SetToFormat(
			"Channel %d: %s", idx, name));
	}

	// Continue enumeration
	if (fEnumeratingChannels) {
		fChannelEnumIndex = idx + 1;
		if (fChannelEnumIndex < fMaxChannels) {
			fProtocol->SendGetChannel(fChannelEnumIndex);
		} else {
			fEnumeratingChannels = false;
			_LogMessage("OK", BString().SetToFormat(
				"Found %d configured channels",
				(int)fChannels.CountItems()));

			// Load private channel messages from DB
			// (channel enumeration happens after contact sync)
			DatabaseManager* db = DatabaseManager::Instance();
			if (db->IsOpen()) {
				int32 loaded = 0;
				for (int32 i = 0; i < fChannels.CountItems(); i++) {
					ChannelInfo* ch = fChannels.ItemAt(i);
					if (ch == NULL)
						continue;
					BString dbKey;
					dbKey.SetToFormat("channel_%d", ch->index);
					loaded += db->LoadMessages(dbKey.String(), ch->messages);
				}
				if (loaded > 0) {
					_LogMessage("INFO", BString().SetToFormat(
						"Loaded %d private channel messages from database",
						loaded));
				}
			}

			_UpdateContactList();
		}
	}
}


void
MainWindow::_HandlePushAdvert(const uint8* data, size_t length)
{
	fTopBar->FlashRx();
	_LogMessage("INFO", "New advertisement received");

	if (length < 7)
		return;

	// PUSH_ADVERT format:
	// [0] = command (0x80)
	// [1-6] = pub key prefix (6 bytes)
	// [7] = SNR * 4 (if present)
	// [8] = RSSI (if present)

	const uint8* pubKeyPrefix = data + 1;
	uint32 now = (uint32)time(NULL);

	int8 snr = (length >= 8) ? (int8)data[7] : 0;
	int8 rssi = (length >= 9) ? (int8)data[8] : fLastRssi;

	// Update lastSeen for this contact
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* contact = fContacts.ItemAt(i);
		if (contact != NULL && memcmp(contact->publicKey, pubKeyPrefix, kPubKeyPrefixSize) == 0) {
			contact->lastSeen = now;
			_LogMessage("DEBUG", BString().SetToFormat("Updated lastSeen for %s (SNR:%d RSSI:%d)",
				contact->name, snr / 4, rssi));
			break;
		}
	}

	// Record SNR data point from advert for historical charting
	if (snr != 0) {
		int8 actualSnr = snr / 4;  // Advert SNR is stored ×4
		char contactHex[kContactHexSize];
		FormatContactKey(contactHex, pubKeyPrefix);
		DatabaseManager::Instance()->InsertSNRDataPoint(contactHex,
			now, actualSnr, rssi, 0);

		// Refresh SNR chart if this contact is currently displayed
		const ContactInfo* displayed = fInfoPanel->GetContact();
		if (displayed != NULL
			&& memcmp(displayed->publicKey, pubKeyPrefix,
				kPubKeyPrefixSize) == 0) {
			fInfoPanel->RefreshSNRChart();
		}
	}

	// Trigger pulse and update link quality on network map
	if (_LockIfVisible(fNetworkMapWindow)) {
		fNetworkMapWindow->TriggerNodePulse(pubKeyPrefix);
		if (snr != 0)
			fNetworkMapWindow->UpdateLinkQuality(pubKeyPrefix, snr / 4, rssi);
		fNetworkMapWindow->UnlockLooper();
	}

	// Forward to Mission Control activity feed
	if (_LockIfVisible(fMissionControlWindow)) {
		ContactInfo* advertContact = _FindContactByPrefix(
			pubKeyPrefix, kPubKeyPrefixSize);
		BString advName;
		if (advertContact != NULL && advertContact->name[0] != '\0')
			advName = advertContact->name;
		else
			advName.SetToFormat("%02X%02X%02X",
				pubKeyPrefix[0], pubKeyPrefix[1], pubKeyPrefix[2]);
		BString eventText;
		eventText.SetToFormat("New advert: %s", advName.String());
		fMissionControlWindow->AddActivityEvent("ADV",
			eventText.String());
		fMissionControlWindow->UnlockLooper();
	}

	// Publish to MQTT if connected (snr is raw ×4, divide for actual dB)
	if (fMqttClient != NULL && fMqttClient->IsConnected()) {
		fMqttClient->PublishPacket(now, snr / 4, rssi, "advert",
			pubKeyPrefix, kPubKeyPrefixSize, data, length);
	}
}


void
MainWindow::_HandlePushTraceData(const uint8* data, size_t length)
{
	if (length < 3) {
		_LogMessage("WARN", "PUSH_TRACE_DATA: frame too short");
		return;
	}

	uint8 pl = data[2];
	uint8 nh = (pl <= 1) ? pl : (pl / 4);
	_LogMessage("INFO", BString().SetToFormat(
		"Trace data received: pathLen=%d, numHops=%d, frameLen=%d",
		(int)pl, (int)nh, (int)length));

	// Forward to TracePathWindow if open (must lock the window first!)
	if (fTracePathWindow == NULL) {
		_LogMessage("WARN", "TracePathWindow is NULL, cannot forward");
	} else if (_LockIfVisible(fTracePathWindow)) {
		_LogMessage("INFO", "Forwarding trace data to TracePathWindow");
		fTracePathWindow->ParseTraceData(data, length);
		fTracePathWindow->ResolveHopNames(&fContacts);
		fTracePathWindow->UnlockLooper();
	} else {
		_LogMessage("WARN", "TracePathWindow not visible or could not lock");
	}

	// Forward to NetworkMapWindow for trace route visualization
	// (even when hidden — keep topology data fresh)
	if (fNetworkMapWindow != NULL && fNetworkMapWindow->LockLooper()) {
		fNetworkMapWindow->HandleTraceData(data, length);
		fNetworkMapWindow->UnlockLooper();
	}

	// Handle ping response
	if (fPingPending) {
		bigtime_t rtt = system_time() - fPingStartTime;
		fPingPending = false;
		delete fPingTimeoutRunner;
		fPingTimeoutRunner = NULL;

		uint8 pathLen = (length >= 3) ? data[2] : 0;
		uint8 hops = (pathLen <= 1) ? pathLen : (pathLen / 4);
		ContactInfo* target = _FindContactByPrefix(
			fPingTargetKey, kPubKeyPrefixSize);
		const char* name = (target && target->name[0])
			? target->name : "(unknown)";

		BString result;
		if (rtt < 1000)
			result.SetToFormat("Ping %s: %lld us, %d hop(s)",
				name, (long long)rtt, hops);
		else if (rtt < 1000000)
			result.SetToFormat("Ping %s: %lld ms, %d hop(s)",
				name, (long long)(rtt / 1000), hops);
		else
			result.SetToFormat("Ping %s: %.1f s, %d hop(s)",
				name, rtt / 1000000.0, hops);

		_LogMessage("PING", result.String());

		// Show result in chat
		ChatMessage pingResult;
		pingResult.timestamp = (uint32)time(NULL);
		pingResult.isOutgoing = false;
		pingResult.isChannel = true;
		strlcpy(pingResult.text, result.String(), sizeof(pingResult.text));
		fChatView->AddMessage(pingResult, "Ping");

		// Advance to next contact in ping-all mode
		if (fPingAllActive) {
			fPingAllResponded++;
			fPingAllIndex++;
			BMessage nextMsg(MSG_PING_ALL_NEXT);
			nextMsg.AddInt32("index", fPingAllIndex);
			BMessageRunner::StartSending(this, &nextMsg, 2000000, 1);
		}
	}

	// Log trace path summary
	if (length >= 12) {
		uint8 pl = data[2];
		uint8 nh = (pl <= 1) ? pl : (pl / 4);
		_LogMessage("TRACE", BString().SetToFormat(
			"Trace path: %d hops, flags=0x%02X", nh, data[3]));
	}
}


void
MainWindow::_HandlePushTelemetry(const uint8* data, size_t length)
{
	// PUSH_TELEMETRY_RESPONSE format:
	// [0] = command (0x8B)
	// [1-6] = sender pub key prefix
	// [7] = sensor type
	// [8-11] = value (float, LE)
	// [12+] = unit string (optional)

	if (length < 12) {
		_LogMessage("WARN", "Telemetry response too short");
		return;
	}

	const uint8* senderPrefix = data + 1;
	uint8 sensorType = data[7];

	float value;
	memcpy(&value, data + 8, sizeof(float));

	// Determine sensor name and unit
	BString sensorName;
	BString unit;
	SensorType type = (SensorType)sensorType;

	switch (sensorType) {
		case 0: sensorName = "Temperature"; unit = "\xC2\xB0" "C"; break;
		case 1: sensorName = "Humidity"; unit = "%"; break;
		case 2: sensorName = "Pressure"; unit = "hPa"; break;
		case 3: sensorName = "Battery"; unit = "V"; break;
		case 4: sensorName = "Altitude"; unit = "m"; break;
		case 5: sensorName = "Light"; unit = "lux"; break;
		case 6: sensorName = "CO2"; unit = "ppm"; break;
		default: sensorName = "Sensor"; unit = ""; type = SENSOR_CUSTOM; break;
	}

	// Extract unit from payload if present
	if (length > 12) {
		char unitBuf[32];
		size_t unitLen = length - 12;
		if (unitLen > 31) unitLen = 31;
		memcpy(unitBuf, data + 12, unitLen);
		unitBuf[unitLen] = '\0';
		unit = unitBuf;
	}

	// Build node ID from pubkey prefix
	uint32 nodeId = (senderPrefix[0] << 24) | (senderPrefix[1] << 16) |
		(senderPrefix[2] << 8) | senderPrefix[3];

	// Find contact name
	ContactInfo* sender = _FindContactByPrefix(senderPrefix, 6);
	BString senderName;
	if (sender != NULL)
		senderName = sender->name;
	else
		senderName.SetToFormat("%02X%02X%02X", senderPrefix[0],
			senderPrefix[1], senderPrefix[2]);

	_LogMessage("TELEMETRY", BString().SetToFormat("%s from %s: %.2f %s",
		sensorName.String(), senderName.String(), value, unit.String()));

	// Open telemetry window and add data
	if (fTelemetryWindow == NULL) {
		fTelemetryWindow = new TelemetryWindow(this);
		fTelemetryWindow->Show();
	}

	if (fTelemetryWindow->LockLooper()) {
		fTelemetryWindow->AddTelemetryData(nodeId, sensorName, type, value, unit,
			senderName.String());
		fTelemetryWindow->UnlockLooper();
	}
}


void
MainWindow::_HandlePushLoginResult(uint8 code)
{
	fLoginPending = false;
	bool success = (code == PUSH_LOGIN_SUCCESS);

	if (success) {
		char hex[kContactHexSize];
		FormatPubKeyPrefix(hex, fLoginTargetKey);
		_LogMessage("OK", BString().SetToFormat(
			"Login successful! (0x%02X) target=%s", code, hex));
	} else {
		_LogMessage("ERROR", BString().SetToFormat(
			"Login failed (0x%02X)", code));
	}

	// Forward result to login window if open
	if (fLoginWindow != NULL) {
		if (fLoginWindow->LockLooper()) {
			if (!fLoginWindow->IsHidden()) {
				fLoginWindow->SetLoginResult(success,
					success ? "Login successful!" : "Login failed. Check password.");
			}
			fLoginWindow->UnlockLooper();
		}
	}

	// After successful login, select the target contact so the user
	// can see the room/repeater menu messages, and refresh contacts
	if (success) {
		// Track active login session for CLI console mode
		fLoggedIn = true;
		memcpy(fLoggedInKey, fLoginTargetKey, kPubKeyPrefixSize);

		// Auto-select the contact we just logged into
		for (int32 i = 1; i < fContactList->CountItems(); i++) {
			ContactItem* item = dynamic_cast<ContactItem*>(
				fContactList->ItemAt(i));
			if (item != NULL && !item->IsChannel() &&
				memcmp(item->GetContact().publicKey,
					fLoginTargetKey, kPubKeyPrefixSize) == 0) {
				fContactList->Select(i);
				_SelectContact(i);
				fChatHeader->SetConsoleMode(true);
				break;
			}
		}

		fSyncingContacts = true;
		fProtocol->SendGetContacts();
		fProtocol->SendGetBattery();
		fProtocol->SendGetStats();

		// Show admin sections in ContactInfoPanel for repeater/room
		ContactInfo* targetContact = _FindContactByPrefix(
			fLoginTargetKey, kPubKeyPrefixSize);
		if (targetContact != NULL
			&& (targetContact->type == 2 || targetContact->type == 3)) {
			fInfoPanel->SetAdminSession(true);
			_ShowAdminToolbar(true);

			// Request remote repeater status
			fProtocol->SendStatusRequest(targetContact->publicKey);

			// Start admin auto-refresh timer (15s)
			delete fAdminRefreshTimer;
			fAdminRefreshTimer = NULL;
			fAdminRefreshTimer = new BMessageRunner(this,
				new BMessage(MSG_ADMIN_REFRESH_TICK),
				kAdminRefreshInterval);
		}
	}
}


void
MainWindow::_HandlePushStatusResponse(const uint8* data, size_t length)
{
	// PUSH_STATUS_RESPONSE format (from meshcore_py parse_status):
	// [0]     = 0x87 (code)
	// [1]     = reserved
	// [2-7]   = pubkey prefix (6 bytes)
	// [8-9]   = bat_mV (uint16 LE)
	// [10-11] = tx_queue_len (uint16 LE)
	// [12-13] = noise_floor (int16 LE, signed)
	// [14-15] = last_rssi (int16 LE, signed)
	// [16-19] = nb_recv (uint32 LE)
	// [20-23] = nb_sent (uint32 LE)
	// [24-27] = airtime (uint32 LE)
	// [28-31] = uptime (uint32 LE)
	// ...
	// [50-51] = last_snr (int16 LE, signed, value/4)

	if (length < 32)
		return;

	const uint8* prefix = &data[2];

	// Verify this is from the repeater we're logged into
	if (!fLoggedIn || memcmp(prefix, fLoggedInKey, kPubKeyPrefixSize) != 0)
		return;

	uint16 battMv = ReadLE16(data + 8);
	int16 noiseFloor = ReadLE16Signed(data + 12);
	int16 rssi = ReadLE16Signed(data + 14);
	uint32 rxPkts = ReadLE32(data + 16);
	uint32 txPkts = ReadLE32(data + 20);
	uint32 uptime = ReadLE32(data + 28);

	int16 snrRaw = 0;
	if (length >= 52)
		snrRaw = ReadLE16Signed(data + 50);
	int8 snr = (int8)(snrRaw / 4);

	// Forward to ContactInfoPanel
	fInfoPanel->SetBatteryInfo(battMv, 0, 0);
	fInfoPanel->SetRadioStats(uptime, txPkts, rxPkts,
		(int8)rssi, snr, (int8)noiseFloor);

	char hex[kContactHexSize];
	FormatPubKeyPrefix(hex, prefix);
	_LogMessage("INFO", BString().SetToFormat(
		"Remote status [%s]: %umV, up %us, rssi %d, snr %d",
		hex, battMv, uptime, (int)rssi, snr));
}


void
MainWindow::_HandleRawPacket(const uint8* data, size_t length)
{
	// Raw radio packets are captured by _ParseFrame and forwarded
	// to the Packet Analyzer window. Only log summary here.
	uint32 now = (uint32)real_time_clock();
	if ((now - fLastRawPacketTime) > 10) {
		_LogMessage("RAW", BString().SetToFormat(
			"Raw radio packets received (total: %u)", fRawPacketCount));
		fLastRawPacketTime = now;
	}
}


void
MainWindow::_HandleMsgSent(const uint8* data, size_t length)
{
	_LogMessage("OK", "Message sent successfully");

	// Find first pending message that hasn't received RSP_SENT yet (FIFO)
	PendingMessage* pending = NULL;
	for (int32 i = 0; i < fPendingMessages.CountItems(); i++) {
		PendingMessage* p = fPendingMessages.ItemAt(i);
		if (p != NULL && !p->gotRspSent) {
			pending = p;
			break;
		}
	}

	if (pending == NULL)
		return;

	pending->gotRspSent = true;

	// Update ChatView if the pending message's contact is currently displayed
	ContactInfo* currentContact = fChatView->CurrentContact();
	if (currentContact != NULL && pending->chatViewIndex >= 0) {
		char currentHex[kContactHexSize];
		FormatContactKey(currentHex, currentContact->publicKey);
		if (strcmp(currentHex, pending->contactKey) == 0) {
			fChatView->UpdateDeliveryStatus(pending->chatViewIndex,
				DELIVERY_SENT);
		}
	}

	// Update the stored ChatMessage in contact history
	ContactInfo* contact = _FindContactByPrefix(pending->pubKey,
		kPubKeyPrefixSize);
	if (contact != NULL) {
		for (int32 i = contact->messages.CountItems() - 1; i >= 0; i--) {
			ChatMessage* msg = contact->messages.ItemAt(i);
			if (msg != NULL && msg->isOutgoing
				&& msg->timestamp == pending->timestamp
				&& msg->deliveryStatus == DELIVERY_PENDING) {
				msg->deliveryStatus = DELIVERY_SENT;
				break;
			}
		}
	}

	// Persist status to database
	DatabaseManager::Instance()->UpdateMessageDeliveryStatus(
		pending->contactKey, pending->timestamp, DELIVERY_SENT, 0);
}


void
MainWindow::_HandleCmdErr(const uint8* data, size_t length)
{
	BString msg("Command error");
	if (length > 1) {
		msg.SetToFormat("Command error: code %d", data[1]);
	}
	_LogMessage("ERROR", msg);

	// If there's a pending message waiting for RSP_SENT, this error is for it
	for (int32 i = 0; i < fPendingMessages.CountItems(); i++) {
		PendingMessage* pending = fPendingMessages.ItemAt(i);
		if (pending != NULL && !pending->gotRspSent) {
			if (pending->attemptCount < kMaxRetryAttempts) {
				_RetryMessage(pending);
			} else {
				_FailMessage(pending);
				fPendingMessages.RemoveItemAt(i);
				if (fPendingMessages.CountItems() == 0)
					_StopDeliveryTimer();
			}
			break;
		}
	}
}


void
MainWindow::_StartDeliveryTimer()
{
	if (fDeliveryCheckTimer != NULL)
		return;  // Already running

	BMessage msg(MSG_DELIVERY_CHECK_TICK);
	fDeliveryCheckTimer = new BMessageRunner(this, &msg,
		kDeliveryCheckInterval);
}


void
MainWindow::_StopDeliveryTimer()
{
	delete fDeliveryCheckTimer;
	fDeliveryCheckTimer = NULL;
}


void
MainWindow::_CheckDeliveryTimeouts()
{
	bigtime_t now = system_time();

	for (int32 i = fPendingMessages.CountItems() - 1; i >= 0; i--) {
		PendingMessage* pending = fPendingMessages.ItemAt(i);
		if (pending == NULL)
			continue;

		bigtime_t elapsed = now - pending->sentTime;

		if (!pending->gotRspSent && elapsed > kSendTimeout) {
			// No RSP_SENT after timeout — retry or fail
			if (pending->attemptCount < kMaxRetryAttempts) {
				_RetryMessage(pending);
			} else {
				_FailMessage(pending);
				fPendingMessages.RemoveItemAt(i);
			}
		} else if (pending->gotRspSent && elapsed > kConfirmCleanup) {
			// Got RSP_SENT but no CONFIRMED after 2 minutes
			// This is normal for offline recipients — just clean up the queue
			// Status stays as SENT (✓) in the UI
			fPendingMessages.RemoveItemAt(i);
		}
	}

	// Stop timer if queue is empty
	if (fPendingMessages.CountItems() == 0)
		_StopDeliveryTimer();
}


void
MainWindow::_RetryMessage(PendingMessage* pending)
{
	pending->attemptCount++;
	pending->sentTime = system_time();
	pending->gotRspSent = false;

	_LogMessage("INFO", BString().SetToFormat(
		"Retrying message (attempt %d/%d)",
		(int)pending->attemptCount, kMaxRetryAttempts));

	// Update ChatView to show RETRYING status
	ContactInfo* currentContact = fChatView->CurrentContact();
	if (currentContact != NULL && pending->chatViewIndex >= 0) {
		char currentHex[kContactHexSize];
		FormatContactKey(currentHex, currentContact->publicKey);
		if (strcmp(currentHex, pending->contactKey) == 0) {
			fChatView->UpdateDeliveryStatus(pending->chatViewIndex,
				DELIVERY_RETRYING, 0, pending->attemptCount);
		}
	}

	// Update in-memory ChatMessage
	ContactInfo* contact = _FindContactByPrefix(pending->pubKey,
		kPubKeyPrefixSize);
	if (contact != NULL) {
		for (int32 i = contact->messages.CountItems() - 1; i >= 0; i--) {
			ChatMessage* msg = contact->messages.ItemAt(i);
			if (msg != NULL && msg->isOutgoing
				&& msg->timestamp == pending->timestamp
				&& (msg->deliveryStatus == DELIVERY_PENDING
					|| msg->deliveryStatus == DELIVERY_RETRYING)) {
				msg->deliveryStatus = DELIVERY_RETRYING;
				msg->retryCount = pending->attemptCount;
				break;
			}
		}
	}

	// Re-send via protocol
	size_t textLen = strlen(pending->text);
	fProtocol->SendDM(pending->pubKey, pending->txtType,
		pending->timestamp, pending->text, textLen);
	fTopBar->FlashTx();
}


void
MainWindow::_FailMessage(PendingMessage* pending)
{
	_LogMessage("ERROR", BString().SetToFormat(
		"Message delivery failed after %d attempt(s)",
		(int)pending->attemptCount));

	// Update ChatView to show FAILED status
	ContactInfo* currentContact = fChatView->CurrentContact();
	if (currentContact != NULL && pending->chatViewIndex >= 0) {
		char currentHex[kContactHexSize];
		FormatContactKey(currentHex, currentContact->publicKey);
		if (strcmp(currentHex, pending->contactKey) == 0) {
			fChatView->UpdateDeliveryStatus(pending->chatViewIndex,
				DELIVERY_FAILED);
		}
	}

	// Update in-memory ChatMessage
	ContactInfo* contact = _FindContactByPrefix(pending->pubKey,
		kPubKeyPrefixSize);
	if (contact != NULL) {
		for (int32 i = contact->messages.CountItems() - 1; i >= 0; i--) {
			ChatMessage* msg = contact->messages.ItemAt(i);
			if (msg != NULL && msg->isOutgoing
				&& msg->timestamp == pending->timestamp
				&& msg->deliveryStatus != DELIVERY_CONFIRMED
				&& msg->deliveryStatus != DELIVERY_SENT) {
				msg->deliveryStatus = DELIVERY_FAILED;
				break;
			}
		}
	}

	// Persist FAILED status to database
	DatabaseManager::Instance()->UpdateMessageDeliveryStatus(
		pending->contactKey, pending->timestamp, DELIVERY_FAILED, 0);
}


void
MainWindow::_OnConnected(BMessage* message)
{
	fConnected = true;
	_UpdateConnectionUI();

	const char* port;
	if (message->FindString(kFieldPort, &port) == B_OK) {
		_LogMessage("OK", BString("Connected: ") << port);
		fTopBar->SetConnected(true, port);
	}

	// Enable message input if something is selected
	if (fSelectedContact >= 0) {
		fMessageInput->SetEnabled(true);
		fSendButton->SetEnabled(true);
		fAttachButton->SetEnabled(true);
		fGifButton->SetEnabled(true);
		fVoiceButton->SetEnabled(true);
	}

	// Forward to Mission Control
	if (_LockIfVisible(fMissionControlWindow)) {
		fMissionControlWindow->SetConnectionState(true,
			fDeviceName, fDeviceFirmware);
		BString connMsg;
		connMsg.SetToFormat("Connected to %s",
			(port != NULL) ? port : "device");
		fMissionControlWindow->AddActivityEvent("SYS",
			connMsg.String());
		fMissionControlWindow->UnlockLooper();
	}

	// Send APP_START first, then schedule init commands after a short delay
	fProtocol->SendAppStart();

	// Use a one-shot delayed message instead of blocking snooze()
	BMessage initMsg(MSG_POST_CONNECT_INIT);
	BMessageRunner::StartSending(this, &initMsg, 100000, 1);  // 100ms, once

	// Start handshake timeout — if device doesn't respond within 5s, warn user
	delete fHandshakeTimer;
	fHandshakeTimer = NULL;
	BMessage hsMsg(MSG_HANDSHAKE_TIMEOUT);
	fHandshakeTimer = new BMessageRunner(this, &hsMsg, kHandshakeTimeout, 1);
}


void
MainWindow::_OnDisconnected()
{
	fConnected = false;
	fHasDeviceInfo = false;  // Reset for next connection
	fDeviceAdvType = 0;
	_SwitchToChatMode();
	fSyncingMessages = false;
	fEnumeratingChannels = false;

	// Mark all pending messages as FAILED on disconnect
	while (fPendingMessages.CountItems() > 0) {
		PendingMessage* pending = fPendingMessages.ItemAt(0);
		if (pending != NULL)
			_FailMessage(pending);
		fPendingMessages.RemoveItemAt(0);
	}
	_StopDeliveryTimer();

	fLoggedIn = false;
	memset(fLoggedInKey, 0, kPubKeyPrefixSize);
	fChatHeader->SetConsoleMode(false);

	// Cancel ping-all if in progress
	fPingAllActive = false;
	fPingPending = false;
	delete fPingTimeoutRunner;
	fPingTimeoutRunner = NULL;

	// Stop periodic stats timer
	delete fStatsRefreshTimer;
	fStatsRefreshTimer = NULL;

	// Stop admin refresh timer and clear admin session
	delete fAdminRefreshTimer;
	fAdminRefreshTimer = NULL;
	fInfoPanel->SetAdminSession(false);
	_ShowAdminToolbar(false);

	// Stop telemetry poll timer
	delete fTelemetryPollTimer;
	fTelemetryPollTimer = NULL;

	// Stop handshake timer
	delete fHandshakeTimer;
	fHandshakeTimer = NULL;

	// Forward to Mission Control
	if (_LockIfVisible(fMissionControlWindow)) {
		fMissionControlWindow->SetConnectionState(false, NULL, NULL);
		fMissionControlWindow->AddActivityEvent("SYS", "Disconnected");
		fMissionControlWindow->UnlockLooper();
	}

	_UpdateConnectionUI();
	_LogMessage("INFO", "Disconnected");
}


void
MainWindow::_OnError(BMessage* message)
{
	const char* error;
	if (message->FindString(kFieldError, &error) == B_OK) {
		_LogMessage("ERROR", error);
	}
}


void
MainWindow::_UpdateSidebarDeviceLabel()
{
	BString label;

	if (fDeviceBoard[0] != '\0')
		label << fDeviceBoard;

	if (fDeviceFirmware[0] != '\0') {
		if (label.Length() > 0)
			label << " \xC2\xB7 ";
		label << "fw " << fDeviceFirmware;
	}

	fSidebarDeviceLabel->SetText(label.String());
}


void
MainWindow::_UpdateContactList()
{
	// Save current selection's pubkey so we can restore it after the
	// list rebuild.  _FilterContacts() destroys all ContactItem objects
	// and recreates them, which invalidates fSelectedContact's index.
	uint8 savedKey[kPubKeyPrefixSize] = {};
	bool hadContactSelected = false;

	if (fSelectedContact > 0 && !fSendingToChannel) {
		ContactItem* sel = dynamic_cast<ContactItem*>(
			fContactList->ItemAt(fSelectedContact));
		if (sel != NULL && !sel->IsChannel()) {
			memcpy(savedKey, sel->GetContact().publicKey,
				kPubKeyPrefixSize);
			hadContactSelected = true;
		}
	}

	// Get current filter text
	const char* filter = (fSearchField != NULL) ? fSearchField->Text() : "";
	_FilterContacts(filter);

	// Restore contact selection by pubkey (index may have shifted)
	if (hadContactSelected) {
		for (int32 i = 1; i < fContactList->CountItems(); i++) {
			ContactItem* item = dynamic_cast<ContactItem*>(
				fContactList->ItemAt(i));
			if (item != NULL && !item->IsChannel()
				&& memcmp(item->GetContact().publicKey,
					savedKey, kPubKeyPrefixSize) == 0) {
				fContactList->Select(i);
				_SelectContact(i);
				break;
			}
		}
	}

	// Update network map if open
	if (_LockIfVisible(fNetworkMapWindow)) {
		fNetworkMapWindow->UpdateFromContacts(&fContacts);
		fNetworkMapWindow->UnlockLooper();
	}
}


void
MainWindow::_FilterContacts(const char* filter)
{
	// Type filter state
	bool showChats = (fShowChats->Value() == B_CONTROL_ON);
	bool showRepeaters = (fShowRepeaters->Value() == B_CONTROL_ON);
	bool showRooms = (fShowRooms->Value() == B_CONTROL_ON);

	// Suppress selection messages while rebuilding the list to avoid
	// queued MSG_CONTACT_SELECTED overriding programmatic re-selection
	fContactList->SetSelectionMessage(NULL);

	// Remove all items except the Channel item (index 0)
	while (fContactList->CountItems() > 1) {
		BListItem* item = fContactList->RemoveItem(1);
		delete item;
	}

	// Set muted state on Public Channel item
	if (fChannelItem != NULL)
		fChannelItem->SetMuted(_IsMuted("ch_public"));

	bool hasFilter = (filter != NULL && filter[0] != '\0');

	// Add private channels after Public Channel
	for (int32 i = 0; i < fChannels.CountItems(); i++) {
		ChannelInfo* ch = fChannels.ItemAt(i);
		if (ch == NULL || ch->IsEmpty())
			continue;

		if (hasFilter) {
			BString name(ch->name);
			BString query(filter);
			name.ToLower();
			query.ToLower();
			if (name.FindFirst(query) < 0)
				continue;
		}

		BString label;
		label.SetToFormat("#%s", ch->name);
		ContactItem* item = new ContactItem(label.String(), true);
		item->SetChannelIndex(ch->index);

		// Set muted state on private channel
		char chKey[16];
		snprintf(chKey, sizeof(chKey), "ch_%d", (int)ch->index);
		item->SetMuted(_IsMuted(chKey));

		fContactList->AddItem(item);
	}

	// Build grouped contact list
	DatabaseManager* db = DatabaseManager::Instance();
	bool hasGroups = (fGroupNames.CountItems() > 0);

	if (hasGroups) {
		// Track which contacts have been added (by index in fContacts)
		int32 contactCount = fContacts.CountItems();
		bool* added = new bool[contactCount];
		memset(added, 0, contactCount * sizeof(bool));

		// For each group, add a separator and its members
		for (int32 g = 0; g < fGroupNames.CountItems(); g++) {
			BString* groupName = fGroupNames.ItemAt(g);
			if (groupName == NULL)
				continue;

			// Check if group name matches filter
			if (hasFilter) {
				BString gLower(*groupName);
				gLower.ToLower();
				BString qLower(filter);
				qLower.ToLower();
				// Show group if its name matches, or if any member matches
				// We'll check member match below
			}

			// Collect members of this group
			OwningObjectList<BString> memberKeys;
			db->LoadGroupMembers(groupName->String(), memberKeys);

			// Find matching contacts for this group
			bool groupHasVisibleMembers = false;
			for (int32 m = 0; m < memberKeys.CountItems(); m++) {
				BString* mk = memberKeys.ItemAt(m);
				if (mk == NULL)
					continue;
				for (int32 c = 0; c < contactCount; c++) {
					if (added[c])
						continue;
					ContactInfo* contact = fContacts.ItemAt(c);
					if (contact == NULL || !contact->isValid)
						continue;
					// Type filter
					if ((contact->type == 2 && !showRepeaters)
						|| (contact->type == 3 && !showRooms)
						|| (contact->type <= 1 && !showChats))
						continue;
					char hex[kContactHexSize];
					FormatContactKey(hex, contact->publicKey);
					if (*mk != hex)
						continue;
					// Apply filter
					if (hasFilter) {
						BString name(contact->name);
						BString query(filter);
						name.ToLower();
						query.ToLower();
						BString gLower(*groupName);
						gLower.ToLower();
						if (name.FindFirst(query) < 0
							&& gLower.FindFirst(query) < 0)
							continue;
					}
					groupHasVisibleMembers = true;
					break;
				}
				if (groupHasVisibleMembers)
					break;
			}

			if (!groupHasVisibleMembers)
				continue;

			// Add group separator
			BStringItem* separator = new BStringItem(
				groupName->String());
			separator->SetEnabled(false);
			fContactList->AddItem(separator);

			// Add member contacts
			for (int32 m = 0; m < memberKeys.CountItems(); m++) {
				BString* mk = memberKeys.ItemAt(m);
				if (mk == NULL)
					continue;
				for (int32 c = 0; c < contactCount; c++) {
					if (added[c])
						continue;
					ContactInfo* contact = fContacts.ItemAt(c);
					if (contact == NULL || !contact->isValid)
						continue;
					if ((contact->type == 2 && !showRepeaters)
						|| (contact->type == 3 && !showRooms)
						|| (contact->type <= 1 && !showChats))
						continue;
					char hex[kContactHexSize];
					FormatContactKey(hex, contact->publicKey);
					if (*mk != hex)
						continue;
					if (hasFilter) {
						BString name(contact->name);
						BString query(filter);
						name.ToLower();
						query.ToLower();
						BString gLower(*groupName);
						gLower.ToLower();
						if (name.FindFirst(query) < 0
							&& gLower.FindFirst(query) < 0)
							continue;
					}
					ContactItem* item = new ContactItem(*contact);
					item->SetMuted(_IsMuted(hex));
					fContactList->AddItem(item);
					added[c] = true;
					break;
				}
			}
		}

		// Add ungrouped contacts
		bool addedUngroupedHeader = false;
		for (int32 c = 0; c < contactCount; c++) {
			if (added[c])
				continue;
			ContactInfo* contact = fContacts.ItemAt(c);
			if (contact == NULL || !contact->isValid)
				continue;
			if ((contact->type == 2 && !showRepeaters)
				|| (contact->type == 3 && !showRooms)
				|| (contact->type <= 1 && !showChats))
				continue;
			if (hasFilter) {
				BString name(contact->name);
				BString query(filter);
				name.ToLower();
				query.ToLower();
				if (name.FindFirst(query) < 0)
					continue;
			}
			if (!addedUngroupedHeader) {
				BStringItem* separator =
					new BStringItem("Ungrouped");
				separator->SetEnabled(false);
				fContactList->AddItem(separator);
				addedUngroupedHeader = true;
			}
			char hex[kContactHexSize];
			FormatContactKey(hex, contact->publicKey);
			ContactItem* item = new ContactItem(*contact);
			item->SetMuted(_IsMuted(hex));
			fContactList->AddItem(item);
		}

		delete[] added;
	} else {
		// No groups — flat list (original behavior)
		for (int32 i = 0; i < fContacts.CountItems(); i++) {
			ContactInfo* contact = fContacts.ItemAt(i);
			if (contact == NULL || !contact->isValid)
				continue;

			// Type filter
			if ((contact->type == 2 && !showRepeaters)
				|| (contact->type == 3 && !showRooms)
				|| (contact->type <= 1 && !showChats))
				continue;

			// Apply case-insensitive filter on name
			if (hasFilter) {
				BString name(contact->name);
				BString query(filter);
				name.ToLower();
				query.ToLower();
				if (name.FindFirst(query) < 0)
					continue;
			}

			char hex[kContactHexSize];
			FormatContactKey(hex, contact->publicKey);
			ContactItem* item = new ContactItem(*contact);
			item->SetMuted(_IsMuted(hex));
			fContactList->AddItem(item);
		}
	}

	// Re-enable selection messages
	fContactList->SetSelectionMessage(new BMessage(MSG_CONTACT_SELECTED));
}


ContactItem*
MainWindow::_FindContactItemByPrefix(const uint8* prefix)
{
	// Skip channel item (index 0)
	for (int32 i = 1; i < fContactList->CountItems(); i++) {
		ContactItem* item = dynamic_cast<ContactItem*>(fContactList->ItemAt(i));
		if (item != NULL && !item->IsChannel()) {
			if (memcmp(item->GetContact().publicKey, prefix, kPubKeyPrefixSize) == 0)
				return item;
		}
	}
	return NULL;
}


ContactInfo*
MainWindow::_FindContactByPrefix(const uint8* prefix, size_t prefixLen)
{
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* contact = fContacts.ItemAt(i);
		if (contact != NULL && contact->isValid) {
			if (memcmp(contact->publicKey, prefix, prefixLen) == 0)
				return contact;
		}
	}
	return NULL;
}


void
MainWindow::_LogMessage(const char* prefix, const char* text)
{
	DebugLogWindow::Instance()->LogMessage(prefix, text);
}


void
MainWindow::_LogTx(const uint8* data, size_t length)
{
	DebugLogWindow::Instance()->LogHex("TX", data, length);
}


void
MainWindow::_LogRx(const uint8* data, size_t length)
{
	DebugLogWindow::Instance()->LogHex("RX", data, length);
}


BString
MainWindow::_GetSettingsPath()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return "";

	path.Append("Sestriere");

	// Create directory if it doesn't exist
	BDirectory dir(path.Path());
	if (dir.InitCheck() != B_OK) {
		create_directory(path.Path(), 0755);
	}

	return BString(path.Path());
}


void
MainWindow::_SaveMqttSettings()
{
	BString settingsPath = _GetSettingsPath();
	if (settingsPath.IsEmpty())
		return;

	BString filePath = settingsPath;
	filePath.Append("/mqtt.settings");

	BFile file(filePath.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK) {
		_LogMessage("WARN", "Failed to save MQTT settings");
		return;
	}

	// Write as simple key=value format
	BString content;
	content << "enabled=" << (fMqttSettings.enabled ? "1" : "0") << "\n";
	content << "latitude=" << fMqttSettings.latitude << "\n";
	content << "longitude=" << fMqttSettings.longitude << "\n";
	content << "iata=" << fMqttSettings.iataCode << "\n";
	content << "broker=" << fMqttSettings.broker << "\n";
	content << "port=" << fMqttSettings.port << "\n";
	content << "username=" << fMqttSettings.username << "\n";
	content << "password=" << fMqttSettings.password << "\n";

	file.Write(content.String(), content.Length());
	_LogMessage("INFO", "MQTT settings saved");
}


void
MainWindow::_LoadMqttSettings()
{
	BString settingsPath = _GetSettingsPath();
	if (settingsPath.IsEmpty())
		return;

	BString filePath = settingsPath;
	filePath.Append("/mqtt.settings");

	BFile file(filePath.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK) {
		// No settings file, use defaults
		return;
	}

	// Read entire file
	off_t size;
	file.GetSize(&size);
	if (size <= 0 || size > 4096)
		return;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return;
	}
	buffer[bytesRead] = '\0';

	// Parse key=value pairs
	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		char* eq = strchr(line, '=');
		if (eq != NULL) {
			*eq = '\0';
			char* key = line;
			char* value = eq + 1;

			if (strcmp(key, "enabled") == 0)
				fMqttSettings.enabled = (atoi(value) != 0);
			else if (strcmp(key, "latitude") == 0)
				fMqttSettings.latitude = atof(value);
			else if (strcmp(key, "longitude") == 0)
				fMqttSettings.longitude = atof(value);
			else if (strcmp(key, "iata") == 0)
				strlcpy(fMqttSettings.iataCode, value, sizeof(fMqttSettings.iataCode));
			else if (strcmp(key, "broker") == 0)
				strlcpy(fMqttSettings.broker, value, sizeof(fMqttSettings.broker));
			else if (strcmp(key, "port") == 0)
				fMqttSettings.port = atoi(value);
			else if (strcmp(key, "username") == 0)
				strlcpy(fMqttSettings.username, value, sizeof(fMqttSettings.username));
			else if (strcmp(key, "password") == 0)
				strlcpy(fMqttSettings.password, value, sizeof(fMqttSettings.password));
		}
		line = strtok_r(NULL, "\n", &saveptr);
	}

	delete[] buffer;

	// Validate loaded settings
	if (fMqttSettings.port < 1 || fMqttSettings.port > 65535)
		fMqttSettings.port = 1883;
	if (fMqttSettings.latitude < -90.0 || fMqttSettings.latitude > 90.0)
		fMqttSettings.latitude = 0.0;
	if (fMqttSettings.longitude < -180.0 || fMqttSettings.longitude > 180.0)
		fMqttSettings.longitude = 0.0;

	// Apply to MQTT client if it exists
	if (fMqttClient != NULL) {
		fMqttClient->SetSettings(fMqttSettings);
	}

	fprintf(stderr, "[MainWindow] MQTT settings loaded (enabled=%s, lat=%.4f, lon=%.4f)\n",
		fMqttSettings.enabled ? "yes" : "no", fMqttSettings.latitude, fMqttSettings.longitude);
}


void
MainWindow::_SaveDeviceSettings()
{
	BString settingsPath = _GetSettingsPath();
	if (settingsPath.IsEmpty())
		return;

	BString filePath = settingsPath;
	filePath.Append("/device.settings");

	BFile file(filePath.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	BString content;
	content << "battery_type=" << (int)fBatteryType << "\n";
	file.Write(content.String(), content.Length());
}


void
MainWindow::_LoadDeviceSettings()
{
	BString settingsPath = _GetSettingsPath();
	if (settingsPath.IsEmpty())
		return;

	BString filePath = settingsPath;
	filePath.Append("/device.settings");

	BFile file(filePath.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	off_t size;
	file.GetSize(&size);
	if (size <= 0 || size > 1024)
		return;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return;
	}
	buffer[bytesRead] = '\0';

	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		char* eq = strchr(line, '=');
		if (eq != NULL) {
			*eq = '\0';
			char* key = line;
			char* value = eq + 1;

			if (strcmp(key, "battery_type") == 0) {
				int type = atoi(value);
				if (type >= 0 && type < BATTERY_CHEMISTRY_COUNT)
					fBatteryType = (uint8)type;
			}
		}
		line = strtok_r(NULL, "\n", &saveptr);
	}

	delete[] buffer;
	fprintf(stderr, "[MainWindow] Device settings loaded (battery=%s)\n",
		kBatteryChemistryNames[fBatteryType]);
}


void
MainWindow::_SaveUISettings()
{
	BString settingsPath = _GetSettingsPath();
	if (settingsPath.IsEmpty())
		return;

	BString filePath = settingsPath;
	filePath.Append("/ui.settings");

	BFile file(filePath.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	BString content;
	content << "show_chats=" << (fShowChats->Value() == B_CONTROL_ON ? "1" : "0") << "\n";
	content << "show_repeaters=" << (fShowRepeaters->Value() == B_CONTROL_ON ? "1" : "0") << "\n";
	content << "show_rooms=" << (fShowRooms->Value() == B_CONTROL_ON ? "1" : "0") << "\n";
	file.Write(content.String(), content.Length());
}


void
MainWindow::_LoadUISettings()
{
	BString settingsPath = _GetSettingsPath();
	if (settingsPath.IsEmpty())
		return;

	BString filePath = settingsPath;
	filePath.Append("/ui.settings");

	BFile file(filePath.String(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	off_t size;
	file.GetSize(&size);
	if (size <= 0 || size > 1024)
		return;

	char* buffer = new char[size + 1];
	ssize_t bytesRead = file.Read(buffer, size);
	if (bytesRead <= 0) {
		delete[] buffer;
		return;
	}
	buffer[bytesRead] = '\0';

	char* saveptr = NULL;
	char* line = strtok_r(buffer, "\n", &saveptr);
	while (line != NULL) {
		char* eq = strchr(line, '=');
		if (eq != NULL) {
			*eq = '\0';
			char* key = line;
			char* value = eq + 1;

			if (strcmp(key, "show_chats") == 0)
				fShowChats->SetValue(atoi(value) ? B_CONTROL_ON : B_CONTROL_OFF);
			else if (strcmp(key, "show_repeaters") == 0)
				fShowRepeaters->SetValue(atoi(value) ? B_CONTROL_ON : B_CONTROL_OFF);
			else if (strcmp(key, "show_rooms") == 0)
				fShowRooms->SetValue(atoi(value) ? B_CONTROL_ON : B_CONTROL_OFF);
		}
		line = strtok_r(NULL, "\n", &saveptr);
	}

	delete[] buffer;
	fprintf(stderr, "[MainWindow] UI settings loaded (chats=%d, repeaters=%d, rooms=%d)\n",
		fShowChats->Value() == B_CONTROL_ON,
		fShowRepeaters->Value() == B_CONTROL_ON,
		fShowRooms->Value() == B_CONTROL_ON);
}


void
MainWindow::_SaveMessages()
{
	// Messages are now saved to SQLite on arrival (see _HandleContactMsgRecv,
	// _HandleChannelMsgRecv, _SendTextMessage, _SendChannelMessage).
	// This method is kept for any in-memory messages not yet persisted.
	DatabaseManager* db = DatabaseManager::Instance();
	if (!db->IsOpen())
		return;

	_LogMessage("INFO", "Messages persisted to database");
}


void
MainWindow::_LoadMessages()
{
	DatabaseManager* db = DatabaseManager::Instance();
	if (!db->IsOpen())
		return;

	// Load public channel messages from database
	int32 loadedChannel = db->LoadChannelMessages(fChannelMessages);

	// Note: private channel messages are loaded when channel enumeration
	// completes (in _HandleChannelInfo / RSP_ERR handler), because channel
	// enumeration runs after the contact sync that triggers _LoadMessages.

	// Load DM messages for each contact
	int32 loadedDM = 0;
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* contact = fContacts.ItemAt(i);
		if (contact == NULL)
			continue;

		char contactHex[kContactHexSize];
		FormatContactKey(contactHex, contact->publicKey);

		loadedDM += db->LoadMessages(contactHex, contact->messages);
	}

	if (loadedChannel > 0 || loadedDM > 0) {
		_LogMessage("INFO", BString().SetToFormat(
			"Loaded %d channel, %d DM messages from database",
			loadedChannel, loadedDM).String());
	}
}


void
MainWindow::_ToggleSearchBar()
{
	if (fSearchBar == NULL)
		return;

	if (fSearchBar->IsHidden()) {
		fSearchBar->Show();
		fMsgSearchField->MakeFocus(true);
		fSearchActive = true;
	} else {
		_CloseSearch();
	}
}


void
MainWindow::_PerformSearch(const char* query)
{
	if (query == NULL || query[0] == '\0')
		return;

	DatabaseManager* db = DatabaseManager::Instance();
	if (!db->IsOpen())
		return;

	OwningObjectList<ChatMessage> results(50);
	int32 found = db->SearchMessages(query, results, 50);

	// Display results in chat view
	fChatView->ClearMessages();

	if (found == 0) {
		// Show "no results" as a system message
		ChatMessage noResult;
		noResult.timestamp = (uint32)time(NULL);
		noResult.isOutgoing = false;
		noResult.isChannel = true;
		strlcpy(noResult.text, "No messages found", sizeof(noResult.text));
		fChatView->AddMessage(noResult, "Search");
	} else {
		// Show results (newest first from DB, we reverse for display)
		for (int32 i = results.CountItems() - 1; i >= 0; i--) {
			ChatMessage* msg = results.ItemAt(i);
			if (msg != NULL) {
				const char* senderName = msg->isOutgoing ? "Me" : "...";

				// Try to find sender name by pubkey prefix
				if (!msg->isOutgoing) {
					ContactInfo* sender = _FindContactByPrefix(
						msg->pubKeyPrefix, kPubKeyPrefixSize);
					if (sender != NULL)
						senderName = sender->name;
				}

				fChatView->AddMessage(*msg, senderName);
			}
		}
	}

	_LogMessage("INFO", BString().SetToFormat(
		"Search '%s': %d results", query, found));
}


void
MainWindow::_CloseSearch()
{
	if (fSearchBar != NULL && !fSearchBar->IsHidden())
		fSearchBar->Hide();

	fSearchActive = false;

	if (fMsgSearchField != NULL)
		fMsgSearchField->SetText("");

	// Restore current contact's messages
	if (fSelectedContact == 0) {
		// Restore channel messages
		fChatView->ClearMessages();
		for (int32 i = 0; i < fChannelMessages.CountItems(); i++) {
			ChatMessage* msg = fChannelMessages.ItemAt(i);
			if (msg != NULL) {
				const char* senderName = msg->isOutgoing ? "Me" : "Channel";
				if (!msg->isOutgoing) {
					ContactInfo* sender = _FindContactByPrefix(
						msg->pubKeyPrefix, kPubKeyPrefixSize);
					if (sender != NULL)
						senderName = sender->name;
				}
				fChatView->AddMessage(*msg, senderName);
			}
		}
	} else if (fSelectedContact > 0) {
		// Look up contact by pubkey from list item (not by index arithmetic)
		ContactItem* selItem = dynamic_cast<ContactItem*>(
			fContactList->ItemAt(fSelectedContact));
		if (selItem != NULL && !selItem->IsChannel()) {
			ContactInfo* contact = _FindContactByPrefix(
				selItem->GetContact().publicKey, kPubKeyPrefixSize);
			if (contact != NULL)
				fChatView->SetCurrentContact(contact);
		}
	}
}


void
MainWindow::_UpdateCharCounter()
{
	int32 maxChars = fSendingToChannel ? 200 : 160;
	int32 current = fMessageInput->TextLength();
	int32 remaining = maxChars - current;

	BString label;
	label.SetToFormat("%ld/%ld", (long)current, (long)maxChars);
	fCharCounter->SetText(label.String());

	// Color: red when over limit, warning when close, normal otherwise
	if (remaining < 0)
		fCharCounter->SetHighColor(ui_color(B_FAILURE_COLOR));
	else if (remaining <= 20)
		fCharCounter->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_MAX_TINT));
	else
		fCharCounter->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
}


BString
MainWindow::_GetPeoplePath()
{
	BPath path;
	if (find_directory(B_USER_DIRECTORY, &path) != B_OK)
		return "";

	path.Append("people");

	// Create directory if it doesn't exist
	BDirectory dir(path.Path());
	if (dir.InitCheck() != B_OK) {
		create_directory(path.Path(), 0755);
	}

	// Create MeshCore subfolder
	path.Append("MeshCore");
	BDirectory meshDir(path.Path());
	if (meshDir.InitCheck() != B_OK) {
		create_directory(path.Path(), 0755);
	}

	return BString(path.Path());
}


void
MainWindow::_SaveContactAsPerson(ContactInfo* contact)
{
	if (contact == NULL || !contact->isValid)
		return;

	BString peoplePath = _GetPeoplePath();
	if (peoplePath.IsEmpty())
		return;

	// Create filename from public key prefix (first 6 bytes as hex)
	char hexPrefix[kContactHexSize];
	FormatContactKey(hexPrefix, contact->publicKey);

	BString filePath = peoplePath;
	filePath << "/" << contact->name;

	// Create the Person file
	BFile file(filePath.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK) {
		_LogMessage("ERROR", BString("Could not create Person file: ") << filePath);
		return;
	}

	// Set MIME type to application/x-person
	BNodeInfo nodeInfo(&file);
	nodeInfo.SetType("application/x-person");

	// === Standard People attributes ===

	// Full name
	file.WriteAttr("META:name", B_STRING_TYPE, 0, contact->name, strlen(contact->name) + 1);

	// Nickname from pubkey prefix
	char nickname[32];
	snprintf(nickname, sizeof(nickname), "MC-%s", hexPrefix);
	file.WriteAttr("META:nickname", B_STRING_TYPE, 0, nickname, strlen(nickname) + 1);

	// Group
	const char* group = "MeshCore";
	file.WriteAttr("META:group", B_STRING_TYPE, 0, group, strlen(group) + 1);

	// Company/Organization
	const char* company = "MeshCore Network";
	file.WriteAttr("META:company", B_STRING_TYPE, 0, company, strlen(company) + 1);

	// URL (MeshCore address format)
	char url[80];
	snprintf(url, sizeof(url), "meshcore://%s", hexPrefix);
	file.WriteAttr("META:url", B_STRING_TYPE, 0, url, strlen(url) + 1);

	// Contact type as human-readable text in "work address" field
	const char* typeStr;
	switch (contact->type) {
		case 1: typeStr = "Companion"; break;
		case 2: typeStr = "Repeater"; break;
		case 3: typeStr = "Room"; break;
		default: typeStr = "Unknown"; break;
	}
	file.WriteAttr("META:waddress", B_STRING_TYPE, 0, typeStr, strlen(typeStr) + 1);

	// Last seen as formatted date in notes
	char note[128];
	if (contact->lastSeen > 0) {
		time_t lastTime = (time_t)contact->lastSeen;
		struct tm tmBuf;
		struct tm* tm = localtime_r(&lastTime, &tmBuf);
		char timeStr[32];
		if (tm != NULL)
			strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm);
		else
			snprintf(timeStr, sizeof(timeStr), "Unknown");
		snprintf(note, sizeof(note), "Last seen: %s\nPath: %d hops", timeStr, contact->outPathLen);
	} else {
		snprintf(note, sizeof(note), "Path: %d hops", contact->outPathLen);
	}
	file.WriteAttr("META:note", B_STRING_TYPE, 0, note, strlen(note) + 1);

	// === MeshCore-specific attributes ===

	// Public key (full 32 bytes as hex string)
	char pubkeyHex[65];
	for (int i = 0; i < 32; i++) {
		snprintf(pubkeyHex + i * 2, 3, "%02x", contact->publicKey[i]);
	}
	file.WriteAttr("MESHCORE:pubkey", B_STRING_TYPE, 0, pubkeyHex, strlen(pubkeyHex) + 1);

	// Contact type (numeric)
	file.WriteAttr("MESHCORE:type", B_UINT8_TYPE, 0, &contact->type, sizeof(uint8));

	// Flags
	file.WriteAttr("MESHCORE:flags", B_UINT8_TYPE, 0, &contact->flags, sizeof(uint8));

	// Outbound path length
	file.WriteAttr("MESHCORE:lastrssi", B_INT8_TYPE, 0, &contact->outPathLen, sizeof(int8));

	// Last seen timestamp
	file.WriteAttr("MESHCORE:lastseen", B_UINT32_TYPE, 0, &contact->lastSeen, sizeof(uint32));
}


void
MainWindow::_LoadPeopleContacts()
{
	BString peoplePath = _GetPeoplePath();
	if (peoplePath.IsEmpty())
		return;

	BDirectory dir(peoplePath.String());
	if (dir.InitCheck() != B_OK)
		return;

	int loaded = 0;
	BEntry entry;
	while (dir.GetNextEntry(&entry) == B_OK) {
		BFile file(&entry, B_READ_ONLY);
		if (file.InitCheck() != B_OK)
			continue;

		// Check if it's a Person file
		BNodeInfo nodeInfo(&file);
		char mimeType[256];
		if (nodeInfo.GetType(mimeType) != B_OK ||
			strcmp(mimeType, "application/x-person") != 0) {
			continue;
		}

		// Check if it has MeshCore pubkey
		char pubkeyHex[65];
		if (file.ReadAttr("MESHCORE:pubkey", B_STRING_TYPE, 0, pubkeyHex, 65) <= 0)
			continue;

		// Create ContactInfo
		ContactInfo* contact = new ContactInfo();

		// Parse public key from hex
		ParseHexPubKey(contact->publicKey, pubkeyHex);

		// Read name
		char name[64];
		if (file.ReadAttr("META:name", B_STRING_TYPE, 0, name, sizeof(name)) > 0) {
			strlcpy(contact->name, name, sizeof(contact->name));
		}

		// Read MeshCore attributes
		file.ReadAttr("MESHCORE:type", B_UINT8_TYPE, 0, &contact->type, sizeof(uint8));
		file.ReadAttr("MESHCORE:flags", B_UINT8_TYPE, 0, &contact->flags, sizeof(uint8));
		file.ReadAttr("MESHCORE:lastrssi", B_INT8_TYPE, 0, &contact->outPathLen, sizeof(int8));
		file.ReadAttr("MESHCORE:lastseen", B_UINT32_TYPE, 0, &contact->lastSeen, sizeof(uint32));

		contact->isValid = true;

		// Check if contact already exists (by pubkey)
		bool exists = false;
		for (int32 i = 0; i < fContacts.CountItems(); i++) {
			ContactInfo* existing = fContacts.ItemAt(i);
			if (existing != NULL &&
				memcmp(existing->publicKey, contact->publicKey, 32) == 0) {
				exists = true;
				break;
			}
		}

		if (!exists) {
			fContacts.AddItem(contact);
			loaded++;
		} else {
			delete contact;
		}
	}

	if (loaded > 0) {
		// Post message to update UI after constructor is done
		PostMessage(MSG_UPDATE_CONTACTS);
	}
}


void
MainWindow::_ExportGPX(const char* path)
{
	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK) {
		_LogMessage("ERR", BString().SetToFormat(
			"Failed to create GPX file: %s", path));
		return;
	}

	BString gpx;
	gpx << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<gpx version=\"1.1\" creator=\"Sestriere\"\n"
		<< "  xmlns=\"http://www.topografix.com/GPX/1/1\">\n";

	int32 exported = 0;
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* contact = fContacts.ItemAt(i);
		if (contact == NULL || !contact->HasGPS())
			continue;

		double lat = contact->latitude / 1e6;
		double lon = contact->longitude / 1e6;

		// Node type label
		const char* typeStr = "Node";
		switch (contact->type) {
			case 1: typeStr = "Chat"; break;
			case 2: typeStr = "Repeater"; break;
			case 3: typeStr = "Room"; break;
		}

		BString name(contact->name[0] ? contact->name : "(unnamed)");
		// Escape XML special chars
		name.ReplaceAll("&", "&amp;");
		name.ReplaceAll("<", "&lt;");
		name.ReplaceAll(">", "&gt;");
		name.ReplaceAll("\"", "&quot;");

		gpx.SetToFormat("%s  <wpt lat=\"%.7f\" lon=\"%.7f\">\n",
			gpx.String(), lat, lon);
		gpx << "    <name>" << name << "</name>\n";
		gpx << "    <type>" << typeStr << "</type>\n";

		if (contact->lastSeen > 0) {
			time_t t = (time_t)contact->lastSeen;
			struct tm tm;
			if (localtime_r(&t, &tm) != NULL) {
				char timeBuf[32];
				strftime(timeBuf, sizeof(timeBuf),
					"%Y-%m-%dT%H:%M:%S", &tm);
				gpx << "    <time>" << timeBuf << "</time>\n";
			}
		}

		// Public key as description
		char hexKey[kContactHexSize];
		for (size_t j = 0; j < kPubKeyPrefixSize; j++)
			snprintf(hexKey + j * 2, 3, "%02X",
				contact->publicKey[j]);
		gpx << "    <desc>" << typeStr << " [" << hexKey << "]</desc>\n";

		gpx << "  </wpt>\n";
		exported++;
	}

	gpx << "</gpx>\n";
	file.Write(gpx.String(), gpx.Length());

	_LogMessage("OK", BString().SetToFormat(
		"Exported %d contacts with GPS to %s", (int)exported, path));
}


bool
MainWindow::_IsMuted(const char* keyHex)
{
	const char* key;
	for (int32 i = 0;
		fMutedKeys.FindString("muted_key", i, &key) == B_OK; i++) {
		if (strcmp(key, keyHex) == 0)
			return true;
	}
	return false;
}


void
MainWindow::_SetMuted(const char* keyHex, bool muted)
{
	DatabaseManager::Instance()->SetMuted(keyHex, muted);

	// Update in-memory cache
	if (muted) {
		if (!_IsMuted(keyHex))
			fMutedKeys.AddString("muted_key", keyHex);
	} else {
		// Remove from fMutedKeys
		const char* key;
		for (int32 i = 0;
			fMutedKeys.FindString("muted_key", i, &key) == B_OK; i++) {
			if (strcmp(key, keyHex) == 0) {
				fMutedKeys.RemoveData("muted_key", i);
				break;
			}
		}
	}

	// Update visual state on matching sidebar items
	for (int32 i = 0; i < fContactList->CountItems(); i++) {
		ContactItem* ci = dynamic_cast<ContactItem*>(fContactList->ItemAt(i));
		if (ci == NULL)
			continue;

		if (ci->IsChannel()) {
			char channelKey[16];
			if (i == 0)
				strlcpy(channelKey, "ch_public", sizeof(channelKey));
			else
				snprintf(channelKey, sizeof(channelKey), "ch_%d",
					ci->ChannelIndex());
			if (strcmp(channelKey, keyHex) == 0) {
				ci->SetMuted(muted);
				fContactList->InvalidateItem(i);
			}
		} else {
			char contactHex[kContactHexSize];
			FormatContactKey(contactHex, ci->GetContact().publicKey);
			if (strcmp(contactHex, keyHex) == 0) {
				ci->SetMuted(muted);
				fContactList->InvalidateItem(i);
			}
		}
	}
}


void
MainWindow::_LoadContactGroups()
{
	fGroupNames.MakeEmpty();
	DatabaseManager::Instance()->LoadGroups(fGroupNames);
}


void
MainWindow::_RefreshContactList()
{
	const char* filter = fSearchField ? fSearchField->Text() : "";
	_FilterContacts(filter);
}


void
MainWindow::_ShowAdminToolbar(bool show)
{
	if (fAdminToolbar == NULL)
		return;

	if (show) {
		if (fAdminToolbar->IsHidden())
			fAdminToolbar->Show();
	} else {
		if (!fAdminToolbar->IsHidden())
			fAdminToolbar->Hide();
	}
}


void
MainWindow::_SwitchToRepeaterMode()
{
	if (fCardView == NULL)
		return;

	// If already in repeater mode, just update the map and return
	if (fCardView->CardLayout()->VisibleIndex() == 1) {
		_UpdateRepeaterMap();
		return;
	}

	fCardView->CardLayout()->SetVisibleItem(1);

	// Update window title
	SetTitle("Sestriere \xe2\x80\x94 Repeater Monitor");

	// Auto-request firmware version and log
	if (fSerialHandler != NULL) {
		BMessage verMsg(MSG_SERIAL_SEND_RAW);
		verMsg.AddString("text", "ver");
		fSerialHandler->PostMessage(&verMsg);

		BMessage logMsg(MSG_SERIAL_SEND_RAW);
		logMsg.AddString("text", "log");
		fSerialHandler->PostMessage(&logMsg);
	}

	// Start repeater map update timer (5 seconds)
	delete fRepeaterMapTimer;
	fRepeaterMapTimer = NULL;
	BMessage mapTick(MSG_REPEATER_MAP_TICK);
	fRepeaterMapTimer = new BMessageRunner(this, &mapTick, 5000000);

	// Auto-open the network map (only on first switch, not re-activation)
	if (fNetworkMapWindow == NULL)
		PostMessage(MSG_SHOW_NETWORK_MAP);
	else
		_UpdateRepeaterMap();
}


void
MainWindow::_SwitchToChatMode()
{
	if (fCardView == NULL)
		return;

	// Only switch if we're not already in chat mode
	if (fCardView->CardLayout()->VisibleIndex() == 0)
		return;

	fCardView->CardLayout()->SetVisibleItem((int32)0);

	// Stop repeater map timer
	delete fRepeaterMapTimer;
	fRepeaterMapTimer = NULL;

	// Clear repeater monitor data
	if (fRepeaterMonitorView != NULL)
		fRepeaterMonitorView->Clear();

	// Reset window title
	SetTitle("Sestriere");
}


void
MainWindow::_UpdateRepeaterMap()
{
	if (fNetworkMapWindow == NULL || fRepeaterMonitorView == NULL)
		return;
	if (!_LockIfVisible(fNetworkMapWindow))
		return;

	RepeaterNodeInfo nodes[64];
	int32 nodeCount = fRepeaterMonitorView->GetNodeInfos(nodes, 64);

	// Try to resolve full names by matching hex ID (first pubkey byte)
	// against known contacts
	for (int32 i = 0; i < nodeCount; i++) {
		if (nodes[i].fullName[0] != '\0')
			continue;  // Already resolved

		// Parse hex ID to byte value
		unsigned int hexVal = 0;
		sscanf(nodes[i].name, "%x", &hexVal);
		uint8 idByte = (uint8)(hexVal & 0xFF);

		// Search contacts for matching first pubkey byte
		for (int32 c = 0; c < fContacts.CountItems(); c++) {
			ContactInfo* contact = fContacts.ItemAt(c);
			if (contact != NULL && contact->isValid
				&& contact->publicKey[0] == idByte
				&& contact->name[0] != '\0') {
				strlcpy(nodes[i].fullName, contact->name,
					sizeof(nodes[i].fullName));
				break;
			}
		}
	}

	// Merge companion contacts that aren't already in the repeater log
	int32 mergedCount = 0;
	int32 contactTotal = fContacts.CountItems();
	_LogMessage("MAP", BString().SetToFormat(
		"RepeaterMap: %d log nodes, %d contacts to merge",
		(int)nodeCount, (int)contactTotal));
	for (int32 c = 0; c < contactTotal && nodeCount < 64; c++) {
		ContactInfo* contact = fContacts.ItemAt(c);
		if (contact == NULL || !contact->isValid || contact->name[0] == '\0')
			continue;

		uint8 idByte = contact->publicKey[0];

		// Skip if this contact is already in the repeater node list
		bool alreadyPresent = false;
		for (int32 n = 0; n < nodeCount; n++) {
			unsigned int hexVal = 0;
			sscanf(nodes[n].name, "%x", &hexVal);
			if ((uint8)(hexVal & 0xFF) == idByte) {
				alreadyPresent = true;
				break;
			}
		}
		if (alreadyPresent)
			continue;

		// Add as a new node from companion contacts
		RepeaterNodeInfo& info = nodes[nodeCount];
		memset(&info, 0, sizeof(RepeaterNodeInfo));
		snprintf(info.name, sizeof(info.name), "%02X", idByte);
		strlcpy(info.fullName, contact->name, sizeof(info.fullName));
		info.packetCount = 0;
		info.isSelf = false;
		info.isDirect = (contact->outPathLen < 0);
		info.isForwarded = (contact->outPathLen > 0);
		nodeCount++;
		mergedCount++;
	}

	if (mergedCount > 0) {
		_LogMessage("MAP", BString().SetToFormat(
			"RepeaterMap: merged %d companion contacts (total nodes: %d)",
			(int)mergedCount, (int)nodeCount));
	}

	RepeaterLink links[128];
	int32 linkCount = fRepeaterMonitorView->GetLinks(links, 128);

	fNetworkMapWindow->SetRepeaterTopology(
		fDeviceName[0] ? fDeviceName : "Repeater",
		nodes, nodeCount, links, linkCount);
	fNetworkMapWindow->UnlockLooper();
}


void
MainWindow::_ForwardSarMarkerToMap(const SarMarker& marker,
	const char* senderName)
{
	_LogMessage("SAR", BString().SetToFormat(
		"SAR marker from %s: %s %s at %.4f,%.4f — %s",
		senderName, marker.emoji, SarMarkerTypeName(marker.type),
		marker.lat, marker.lon,
		marker.notes[0] ? marker.notes : "(no note)"));

	if (fMapWindow == NULL)
		return;

	if (fMapWindow->LockLooper()) {
		fMapWindow->AddSarPin(
			(float)marker.lat, (float)marker.lon,
			marker.emoji, senderName, marker.colorIndex);
		fMapWindow->UnlockLooper();
	}
}


void
MainWindow::_ShowSarMarkerDialog()
{
	// Step 1: Choose SAR type via BAlert (max 3 buttons per alert)
	BAlert* typeAlert = new BAlert("SAR Marker",
		"Select SAR marker type to send:",
		"Person", "Fire", "Staging");
	int32 choice = typeAlert->Go();
	if (choice < 0)
		return;

	SarMarkerType sarType;
	if (choice == 0) sarType = SAR_PERSON;
	else if (choice == 1) sarType = SAR_FIRE;
	else sarType = SAR_STAGING;

	// Offer more types if none of the first 3 were selected...
	// Actually user already picked one. For Object/Unknown, show second alert.
	// Simplify: first alert covers the 3 most common types.

	// Step 2: Get coordinates from selected contact GPS
	double lat = 0, lon = 0;
	if (fSelectedContact >= 0) {
		ContactItem* item = dynamic_cast<ContactItem*>(
			fContactList->ItemAt(fSelectedContact));
		if (item != NULL) {
			const ContactInfo& ci = item->GetContact();
			if (ci.HasGPS()) {
				lat = ci.latitude / 1e6;
				lon = ci.longitude / 1e6;
			}
		}
	}

	if (lat == 0.0 && lon == 0.0) {
		BAlert* noGps = new BAlert("SAR Marker",
			"No GPS coordinates available.\n\n"
			"Select a contact with GPS position, or\n"
			"type a SAR marker directly in chat:\n"
			"S:\xF0\x9F\xA7\x91:0:45.44,7.12:Note",
			"OK");
		noGps->Go();
		return;
	}

	// Build and send the marker
	SarMarker marker;
	marker.type = sarType;
	strlcpy(marker.emoji, SarMarkerEmoji(sarType), sizeof(marker.emoji));
	marker.colorIndex = 0;
	marker.lat = lat;
	marker.lon = lon;
	marker.isValid = true;
	strlcpy(marker.notes, SarMarkerTypeName(sarType),
		sizeof(marker.notes));

	char sarText[256];
	FormatSarMarker(sarText, sizeof(sarText), marker);

	_SendTextMessage(sarText);

	_LogMessage("SAR", BString().SetToFormat(
		"Sent SAR marker: %s", sarText));
}


// =============================================================================
// Image Sharing
// =============================================================================


// --- GIF sharing implementation ---

struct GifDownloadContext {
	char		gifId[64];
	int32		chatViewIndex;
	BWindow*	targetWindow;
};


void
MainWindow::_HandleGifSelected(BMessage* msg)
{
	const char* gifId = NULL;
	if (msg->FindString("gif_id", &gifId) != B_OK || gifId == NULL)
		return;

	// Compose g:ID text and send as normal message
	BString gifText;
	gifText.SetToFormat("g:%s", gifId);

	if (fSendingToChannel || fSelectedChannelIdx >= 0)
		_SendChannelMessage(gifText.String());
	else
		_SendTextMessage(gifText.String());

	// The message was just added to chat — find it and start download
	int32 lastIndex = fChatView->CountItems() - 1;
	if (lastIndex >= 0) {
		MessageView* mv = dynamic_cast<MessageView*>(
			fChatView->ItemAt(lastIndex));
		if (mv != NULL && mv->IsGifMessage()) {
			mv->SetGifLoadState(1);  // loading
			fChatView->InvalidateItem(lastIndex);
			_DownloadAndDisplayGif(gifId, lastIndex);
		}
	}
}


void
MainWindow::_DownloadAndDisplayGif(const char* gifId, int32 chatViewIndex)
{
	GifDownloadContext* ctx = new GifDownloadContext;
	strlcpy(ctx->gifId, gifId, sizeof(ctx->gifId));
	ctx->chatViewIndex = chatViewIndex;
	ctx->targetWindow = this;

	thread_id tid = spawn_thread(_GifDownloadThread, "gif_download",
		B_LOW_PRIORITY, ctx);
	if (tid >= 0)
		resume_thread(tid);
	else
		delete ctx;
}


int32
MainWindow::_GifDownloadThread(void* data)
{
	GifDownloadContext* ctx = (GifDownloadContext*)data;

	// Check cache first
	BString cachePath;
	cachePath.SetToFormat(
		"/boot/home/config/settings/Sestriere/gif_cache/%s.gif",
		ctx->gifId);

	uint8* gifData = NULL;
	size_t gifSize = 0;

	// Try loading from cache
	BEntry entry(cachePath.String());
	if (entry.Exists()) {
		off_t fileSize;
		entry.GetSize(&fileSize);
		if (fileSize > 0) {
			gifData = (uint8*)malloc(fileSize);
			if (gifData != NULL) {
				BFile file(cachePath.String(), B_READ_ONLY);
				if (file.Read(gifData, fileSize) == fileSize) {
					gifSize = (size_t)fileSize;
				} else {
					free(gifData);
					gifData = NULL;
				}
			}
		}
	}

	// Download if not cached
	if (gifData == NULL) {
		char gifUrl[512];
		GiphyClient::BuildGifUrl(ctx->gifId, gifUrl, sizeof(gifUrl));
		if (GiphyClient::DownloadData(gifUrl, &gifData, &gifSize) != B_OK) {
			// Send error state
			BMessage msg(MSG_GIF_LOADED);
			msg.AddInt32("item_index", ctx->chatViewIndex);
			msg.AddPointer("frames", NULL);
			msg.AddPointer("durations", NULL);
			msg.AddInt32("frame_count", 0);
			BMessenger(ctx->targetWindow).SendMessage(&msg);
			delete ctx;
			return 0;
		}

		// Save to cache
		create_directory(
			"/boot/home/config/settings/Sestriere/gif_cache",
			0755);
		BFile cacheFile(cachePath.String(),
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (cacheFile.InitCheck() == B_OK)
			cacheFile.Write(gifData, gifSize);
	}

	// Decode GIF frames
	BBitmap** frames = NULL;
	uint32* durations = NULL;
	int32 frameCount = 0;

	status_t status = ImageCodec::DecompressGifFrames(gifData, gifSize,
		&frames, &durations, &frameCount);
	free(gifData);

	if (status != B_OK) {
		delete ctx;
		return 0;
	}

	// Post result to main window
	BMessage msg(MSG_GIF_LOADED);
	msg.AddInt32("item_index", ctx->chatViewIndex);
	msg.AddPointer("frames", frames);
	msg.AddPointer("durations", durations);
	msg.AddInt32("frame_count", frameCount);
	BMessenger(ctx->targetWindow).SendMessage(&msg);

	delete ctx;
	return 0;
}


void
MainWindow::_HandleImageSelected(BMessage* message)
{
	entry_ref ref;
	if (message->FindRef("refs", &ref) != B_OK)
		return;

	BPath path;
	BEntry entry(&ref);
	if (entry.GetPath(&path) != B_OK)
		return;

	if (!fConnected) {
		_LogMessage("WARN", "Cannot send image: not connected");
		return;
	}

	// Compress image
	uint8* jpegData = NULL;
	size_t jpegSize = 0;
	int32 w = 0, h = 0;
	status_t status = ImageCodec::CompressImageFile(path.Path(),
		&jpegData, &jpegSize, &w, &h, 128, 65);
	if (status != B_OK) {
		_LogMessage("ERROR", BString().SetToFormat(
			"Failed to compress image: %s", strerror(status)));
		return;
	}

	_LogMessage("IMG", BString().SetToFormat(
		"Compressed %s → %zd bytes (%ldx%ld grayscale JPEG)",
		path.Leaf(), jpegSize, (long)w, (long)h));

	// Get self key prefix for envelope
	uint8 selfKey[6];
	for (int i = 0; i < 6 && fPublicKey[i * 2] != '\0'; i++) {
		unsigned int byte;
		if (sscanf(fPublicKey + i * 2, "%2x", &byte) == 1)
			selfKey[i] = (uint8)byte;
	}

	// Create outgoing session
	uint32 sid = fImageSessions->CreateOutgoing(jpegData, jpegSize,
		w, h, selfKey);
	free(jpegData);

	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL) {
		_LogMessage("ERROR", "Failed to create image session");
		return;
	}

	// Format and send IE2 envelope as a text message
	BString envelope = ImageSessionManager::FormatEnvelope(sid,
		session->format, session->totalFragments, w, h,
		(uint32)session->jpegSize, selfKey,
		session->timestamp);

	// Send envelope as regular text (this adds it to chat view too)
	if (fSendingToChannel)
		_SendChannelMessage(envelope.String());
	else
		_SendTextMessage(envelope.String());

	// Set decoded bitmap on the outgoing MessageView so it's visible immediately
	BBitmap* previewBitmap = ImageCodec::DecompressImageData(
		session->jpegData, session->jpegSize);
	if (previewBitmap != NULL) {
		for (int32 i = fChatView->CountItems() - 1; i >= 0; i--) {
			MessageView* mv = dynamic_cast<MessageView*>(
				fChatView->ItemAt(i));
			if (mv != NULL && mv->IsImageMessage()
				&& mv->ImageSessionId() == sid) {
				mv->SetImageBitmap(previewBitmap);
				BFont font;
				fChatView->GetFont(&font);
				mv->Update(fChatView, &font);
				fChatView->InvalidateItem(i);
				break;
			}
		}
	}

	// Save image to DB
	if (fChatView->CurrentContact() != NULL) {
		char contactKey[13];
		for (int i = 0; i < 6; i++)
			snprintf(contactKey + i * 2, 3, "%02x",
				fChatView->CurrentContact()->publicKey[i]);
		DatabaseManager::Instance()->InsertImage(contactKey, sid,
			w, h, session->jpegData, session->jpegSize);
	}

	// Start fragment transmission timer
	fCurrentSendSession = sid;
	fCurrentSendIndex = 0;

	delete fImageFragmentTimer;
	BMessage timerMsg(MSG_IMAGE_SEND_NEXT);
	fImageFragmentTimer = new BMessageRunner(this, &timerMsg, 200000, -1);

	_LogMessage("IMG", BString().SetToFormat(
		"Sending %d fragments for session %08x",
		(int)session->totalFragments, sid));
}


void
MainWindow::_SendNextImageFragment()
{
	ImageSession* session = fImageSessions->FindSession(fCurrentSendSession);
	if (session == NULL || fCurrentSendIndex >= session->totalFragments) {
		// Done sending
		delete fImageFragmentTimer;
		fImageFragmentTimer = NULL;
		if (session != NULL) {
			session->state = IMAGE_COMPLETE;
			_LogMessage("IMG", BString().SetToFormat(
				"All %d fragments sent for session %08x",
				(int)session->totalFragments, fCurrentSendSession));
		}
		return;
	}

	// Build fragment packet
	uint8 packet[kImageHeaderSize + kMaxFragmentPayload];
	ImageFragment& frag = session->fragments[fCurrentSendIndex];
	size_t pktLen = ImageSessionManager::BuildFragmentPacket(packet,
		session->sessionId, session->format, fCurrentSendIndex,
		session->totalFragments, frag.data, frag.length);

	// Send via CMD_SEND_RAW_DATA
	if (fProtocol != NULL)
		fProtocol->SendRawData(packet, pktLen);

	fCurrentSendIndex++;
}


void
MainWindow::_HandleIncomingImageFragment(const uint8* payload, size_t length)
{
	if (length < kImageHeaderSize)
		return;

	// Parse header: [magic:1][sid:4][fmt:1][idx:1][total:1][data...]
	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 idx = payload[6];
	uint8 total = payload[7];

	const uint8* fragData = payload + kImageHeaderSize;
	uint16 fragLen = (uint16)(length - kImageHeaderSize);

	// If session doesn't exist yet, create a stub (envelope might arrive later)
	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL) {
		// Create a minimal session stub
		ImageSession* stub = new ImageSession();
		stub->sessionId = sid;
		stub->format = payload[5];
		stub->totalFragments = total;
		stub->state = IMAGE_LOADING;
		stub->createdTime = system_time();
		// Can't add to manager directly — just wait for envelope
		delete stub;
		_LogMessage("IMG", BString().SetToFormat(
			"Fragment %d/%d for unknown session %08x (waiting for envelope)",
			idx + 1, total, sid));
		return;
	}

	bool complete = fImageSessions->AddFragment(sid, idx, fragData, fragLen);

	_LogMessage("IMG", BString().SetToFormat(
		"Fragment %d/%d for session %08x (%d/%d received)",
		idx + 1, total, sid,
		(int)session->receivedCount, (int)session->totalFragments));

	// Update the message view
	_UpdateImageMessageView(sid);

	if (complete) {
		session->state = IMAGE_COMPLETE;
		if (session->Reassemble()) {
			_LogMessage("IMG", BString().SetToFormat(
				"Session %08x complete, %zu bytes JPEG",
				sid, session->jpegSize));

			// Decompress and assign bitmap to message view
			BBitmap* bitmap = ImageCodec::DecompressImageData(
				session->jpegData, session->jpegSize);
			if (bitmap != NULL) {
				// Find the MessageView for this session and set bitmap
				for (int32 i = 0; i < fChatView->CountItems(); i++) {
					MessageView* mv = dynamic_cast<MessageView*>(
						fChatView->ItemAt(i));
					if (mv != NULL && mv->IsImageMessage()
						&& mv->ImageSessionId() == sid) {
						mv->SetImageBitmap(bitmap);
						BFont font;
						fChatView->GetFont(&font);
						mv->Update(fChatView, &font);
						fChatView->InvalidateItem(i);
						break;
					}
				}
			}

			// Save to DB
			if (fChatView->CurrentContact() != NULL) {
				char contactKey[13];
				for (int i = 0; i < 6; i++)
					snprintf(contactKey + i * 2, 3, "%02x",
						fChatView->CurrentContact()->publicKey[i]);
				DatabaseManager::Instance()->InsertImage(contactKey, sid,
					session->width, session->height,
					session->jpegData, session->jpegSize);
			}
		}
	}
}


void
MainWindow::_HandleIncomingFetchRequest(const uint8* payload, size_t length)
{
	if (length < 6)
		return;

	// Parse: [magic:1][sid:4][count:1][indices...]
	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 count = payload[5];

	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL || session->state != IMAGE_SENDING) {
		_LogMessage("IMG", BString().SetToFormat(
			"Fetch request for unknown/non-outgoing session %08x", sid));
		return;
	}

	_LogMessage("IMG", BString().SetToFormat(
		"Fetch request: %d fragments for session %08x", (int)count, sid));

	// Resend requested fragments
	for (uint8 i = 0; i < count && (size_t)(6 + i) < length; i++) {
		uint8 idx = payload[6 + i];
		if (idx >= session->totalFragments)
			continue;

		uint8 packet[kImageHeaderSize + kMaxFragmentPayload];
		ImageFragment& frag = session->fragments[idx];
		size_t pktLen = ImageSessionManager::BuildFragmentPacket(packet,
			sid, session->format, idx, session->totalFragments,
			frag.data, frag.length);

		if (fProtocol != NULL)
			fProtocol->SendRawData(packet, pktLen);
	}
}


void
MainWindow::_StartImageFetch(uint32 sessionId)
{
	ImageSession* session = fImageSessions->FindSession(sessionId);
	if (session == NULL)
		return;

	// Build list of missing fragments
	uint8 missing[255];
	uint8 count = 0;
	for (uint8 i = 0; i < session->totalFragments && count < 255; i++) {
		if (!session->fragments[i].received)
			missing[count++] = i;
	}

	if (count == 0) {
		_LogMessage("IMG", "No missing fragments to fetch");
		return;
	}

	// Build fetch request
	uint8 packet[6 + 255];
	size_t pktLen = ImageSessionManager::BuildFetchRequest(packet,
		sessionId, missing, count);

	// Look up sender's full 32-byte pubkey from contact list
	// (session->senderKey is only 6 bytes — passing it to SendBinaryRequest
	// would cause a buffer overread)
	ContactInfo* sender = _FindContactByPrefix(session->senderKey,
		kPubKeyPrefixSize);
	if (sender == NULL || fProtocol == NULL) {
		_LogMessage("IMG", "Cannot fetch: sender not in contact list");
		return;
	}
	fProtocol->SendBinaryRequest(sender->publicKey, packet, pktLen);

	session->state = IMAGE_LOADING;
	_UpdateImageMessageView(sessionId);

	_LogMessage("IMG", BString().SetToFormat(
		"Sent fetch request for %d fragments of session %08x",
		(int)count, sessionId));
}


void
MainWindow::_UpdateImageMessageView(uint32 sid)
{
	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL)
		return;

	for (int32 i = 0; i < fChatView->CountItems(); i++) {
		MessageView* mv = dynamic_cast<MessageView*>(fChatView->ItemAt(i));
		if (mv != NULL && mv->IsImageMessage()
			&& mv->ImageSessionId() == sid) {
			mv->SetImageState(session->state, session->receivedCount);
			fChatView->InvalidateItem(i);
			break;
		}
	}
}


// =============================================================================
// Voice messages
// =============================================================================


void
MainWindow::_StartVoiceRecord()
{
	if (fRecordingVoice || fAudioEngine == NULL)
		return;

	status_t err = fAudioEngine->StartRecording();
	if (err != B_OK) {
		_LogMessage("VOICE", BString().SetToFormat(
			"Failed to start recording: %s", strerror(err)));
		BAlert* alert = new BAlert("Voice",
			"Could not start recording.\n"
			"Check that a microphone is connected.",
			"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go(NULL);
		return;
	}

	fRecordingVoice = true;
	fVoiceButton->SetLabel("Stop");
	_LogMessage("VOICE", "Recording started");
}


void
MainWindow::_StopVoiceRecord()
{
	if (!fRecordingVoice || fAudioEngine == NULL)
		return;

	int16* pcm = NULL;
	size_t sampleCount = 0;
	fAudioEngine->StopRecording(&pcm, &sampleCount);
	fRecordingVoice = false;
	fVoiceButton->SetLabel("Mic");

	if (pcm == NULL || sampleCount == 0) {
		_LogMessage("VOICE", "No audio recorded");
		delete[] pcm;
		return;
	}

	_LogMessage("VOICE", BString().SetToFormat(
		"Recorded %zu samples (%.1f seconds)",
		sampleCount, sampleCount / 8000.0f));

	// Encode to Codec2
	uint8* codec2Data = NULL;
	size_t codec2Size = 0;
	status_t err = VoiceCodec::Encode(pcm, sampleCount, VOICE_MODE_1300,
		&codec2Data, &codec2Size);
	delete[] pcm;

	if (err != B_OK || codec2Data == NULL) {
		_LogMessage("VOICE", "Failed to encode audio");
		return;
	}

	uint32 durationSec = VoiceCodec::DurationSec(codec2Size,
		VOICE_MODE_1300);
	if (durationSec == 0)
		durationSec = 1;

	// Get self key prefix
	uint8 selfKey[6];
	for (int i = 0; i < 6 && fPublicKey[i * 2] != '\0'; i++) {
		unsigned int byte;
		if (sscanf(fPublicKey + i * 2, "%2x", &byte) == 1)
			selfKey[i] = (uint8)byte;
	}

	// Create outgoing session
	uint32 sid = fVoiceSessions->CreateOutgoing(codec2Data, codec2Size,
		VOICE_MODE_1300, durationSec, selfKey);
	free(codec2Data);

	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL) {
		_LogMessage("ERROR", "Failed to create voice session");
		return;
	}

	// Format and send VE2 envelope as a text message
	BString envelope = VoiceSessionManager::FormatEnvelope(sid,
		VOICE_MODE_1300, session->totalFragments, durationSec,
		selfKey, session->timestamp);

	// Send envelope as regular text
	if (fSendingToChannel)
		_SendChannelMessage(envelope.String());
	else
		_SendTextMessage(envelope.String());

	// Save to DB
	if (fChatView->CurrentContact() != NULL) {
		char contactKey[13];
		for (int i = 0; i < 6; i++)
			snprintf(contactKey + i * 2, 3, "%02x",
				fChatView->CurrentContact()->publicKey[i]);
		DatabaseManager::Instance()->InsertVoiceClip(contactKey, sid,
			durationSec, (uint8)VOICE_MODE_1300,
			session->codec2Data, session->codec2Size);
	}

	// Start fragment transmission timer
	fCurrentVoiceSendSession = sid;
	fCurrentVoiceSendIndex = 0;

	delete fVoiceFragmentTimer;
	BMessage timerMsg(MSG_VOICE_SEND_NEXT);
	fVoiceFragmentTimer = new BMessageRunner(this, &timerMsg, 200000, -1);

	_LogMessage("VOICE", BString().SetToFormat(
		"Sending %d fragments for voice session %08x",
		(int)session->totalFragments, sid));
}


void
MainWindow::_SendNextVoiceFragment()
{
	VoiceSession* session = fVoiceSessions->FindSession(
		fCurrentVoiceSendSession);
	if (session == NULL
		|| fCurrentVoiceSendIndex >= session->totalFragments) {
		// Done sending
		delete fVoiceFragmentTimer;
		fVoiceFragmentTimer = NULL;
		if (session != NULL) {
			session->state = VOICE_COMPLETE;
			_LogMessage("VOICE", BString().SetToFormat(
				"All %d fragments sent for voice session %08x",
				(int)session->totalFragments, fCurrentVoiceSendSession));
		}
		return;
	}

	// Build fragment packet
	uint8 packet[kVoiceHeaderSize + kMaxVoiceFragmentPayload];
	VoiceFragment& frag = session->fragments[fCurrentVoiceSendIndex];
	size_t pktLen = VoiceSessionManager::BuildFragmentPacket(packet,
		session->sessionId, session->mode, fCurrentVoiceSendIndex,
		session->totalFragments, frag.data, frag.length);

	// Send via CMD_SEND_RAW_DATA
	if (fProtocol != NULL)
		fProtocol->SendRawData(packet, pktLen);

	fCurrentVoiceSendIndex++;
}


void
MainWindow::_HandleIncomingVoiceFragment(const uint8* payload, size_t length)
{
	if (length < kVoiceHeaderSize)
		return;

	// Parse header: [magic:1][sid:4][mode:1][idx:1][total:1][data...]
	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 idx = payload[6];
	uint8 total = payload[7];

	const uint8* fragData = payload + kVoiceHeaderSize;
	uint16 fragLen = (uint16)(length - kVoiceHeaderSize);

	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL) {
		_LogMessage("VOICE", BString().SetToFormat(
			"Fragment %d/%d for unknown voice session %08x",
			idx + 1, total, sid));
		return;
	}

	bool complete = fVoiceSessions->AddFragment(sid, idx, fragData, fragLen);

	_LogMessage("VOICE", BString().SetToFormat(
		"Voice fragment %d/%d for session %08x (%d/%d received)",
		idx + 1, total, sid,
		(int)session->receivedCount, (int)session->totalFragments));

	_UpdateVoiceMessageView(sid);

	if (complete) {
		session->state = VOICE_COMPLETE;
		if (session->Reassemble()) {
			_LogMessage("VOICE", BString().SetToFormat(
				"Voice session %08x complete, %zu bytes Codec2",
				sid, session->codec2Size));

			// Save to DB
			if (fChatView->CurrentContact() != NULL) {
				char contactKey[13];
				for (int i = 0; i < 6; i++)
					snprintf(contactKey + i * 2, 3, "%02x",
						fChatView->CurrentContact()->publicKey[i]);
				DatabaseManager::Instance()->InsertVoiceClip(contactKey,
					sid, session->durationSec, (uint8)session->mode,
					session->codec2Data, session->codec2Size);
			}
		}
		_UpdateVoiceMessageView(sid);
	}
}


void
MainWindow::_HandleIncomingVoiceFetch(const uint8* payload, size_t length)
{
	// VR2: [magic:1][sid:4][flags:1][key6:6][ts:4][count:1][indices...]
	if (length < 17)
		return;

	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 count = payload[16];

	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL || session->state != VOICE_SENDING) {
		_LogMessage("VOICE", BString().SetToFormat(
			"Voice fetch for unknown/non-outgoing session %08x", sid));
		return;
	}

	_LogMessage("VOICE", BString().SetToFormat(
		"Voice fetch request: %d fragments for session %08x",
		(int)count, sid));

	// Resend requested fragments
	for (uint8 i = 0; i < count && (size_t)(17 + i) < length; i++) {
		uint8 idx = payload[17 + i];
		if (idx >= session->totalFragments)
			continue;

		uint8 packet[kVoiceHeaderSize + kMaxVoiceFragmentPayload];
		VoiceFragment& frag = session->fragments[idx];
		size_t pktLen = VoiceSessionManager::BuildFragmentPacket(packet,
			sid, session->mode, idx, session->totalFragments,
			frag.data, frag.length);

		if (fProtocol != NULL)
			fProtocol->SendRawData(packet, pktLen);
	}
}


void
MainWindow::_StartVoiceFetch(uint32 sessionId)
{
	VoiceSession* session = fVoiceSessions->FindSession(sessionId);
	if (session == NULL)
		return;

	// Build list of missing fragments
	uint8 missing[255];
	uint8 count = 0;
	for (uint8 i = 0; i < session->totalFragments && count < 255; i++) {
		if (!session->fragments[i].received)
			missing[count++] = i;
	}

	if (count == 0) {
		_LogMessage("VOICE", "No missing voice fragments to fetch");
		return;
	}

	// Build VR2 fetch request
	uint8 packet[17 + 255];
	size_t pktLen = VoiceSessionManager::BuildFetchRequest(packet,
		sessionId, 1, session->senderKey, session->timestamp,
		missing, count);

	// Look up sender's full 32-byte pubkey from contact list
	ContactInfo* sender = _FindContactByPrefix(session->senderKey,
		kPubKeyPrefixSize);
	if (sender == NULL || fProtocol == NULL) {
		_LogMessage("VOICE", "Cannot fetch: sender not in contact list");
		return;
	}
	fProtocol->SendBinaryRequest(sender->publicKey, packet, pktLen);

	session->state = VOICE_LOADING;
	_UpdateVoiceMessageView(sessionId);

	_LogMessage("VOICE", BString().SetToFormat(
		"Sent voice fetch request for %d fragments of session %08x",
		(int)count, sessionId));
}


void
MainWindow::_UpdateVoiceMessageView(uint32 sid)
{
	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL)
		return;

	for (int32 i = 0; i < fChatView->CountItems(); i++) {
		MessageView* mv = dynamic_cast<MessageView*>(
			fChatView->ItemAt(i));
		if (mv != NULL && mv->IsVoiceMessage()
			&& mv->VoiceSessionId() == sid) {
			mv->SetVoiceState(session->state, session->receivedCount);
			fChatView->InvalidateItem(i);
			break;
		}
	}
}


void
MainWindow::_HandleVoicePlayRequest(uint32 sessionId)
{
	VoiceSession* session = fVoiceSessions->FindSession(sessionId);

	// If session not found or not complete, try loading from DB
	if (session == NULL || session->state != VOICE_COMPLETE) {
		if (session != NULL && session->state == VOICE_PENDING) {
			// Need to fetch fragments first
			_StartVoiceFetch(sessionId);
			return;
		}

		// Try loading from database
		uint8* c2Data = NULL;
		size_t c2Size = 0;
		uint32 dur = 0;
		uint8 mode = 0;
		if (DatabaseManager::Instance()->LoadVoiceClip(sessionId,
			&c2Data, &c2Size, &dur, &mode)) {
			// Decode and play directly
			int16* pcm = NULL;
			size_t pcmSamples = 0;
			if (VoiceCodec::Decode(c2Data, c2Size,
				(VoicePacketMode)mode, &pcm, &pcmSamples) == B_OK) {
				delete[] fVoicePlayPcm;
				fVoicePlayPcm = pcm;
				fVoicePlayPcmSize = pcmSamples;

				// Mark playing in message view
				for (int32 i = 0; i < fChatView->CountItems(); i++) {
					MessageView* mv = dynamic_cast<MessageView*>(
						fChatView->ItemAt(i));
					if (mv != NULL && mv->IsVoiceMessage()
						&& mv->VoiceSessionId() == sessionId) {
						mv->SetVoicePlaying(true);
						fChatView->InvalidateItem(i);
						break;
					}
				}

				fAudioEngine->Play(fVoicePlayPcm, fVoicePlayPcmSize,
					this);
			}
			free(c2Data);
			return;
		}

		_LogMessage("VOICE", "Voice clip not available");
		return;
	}

	// Session is complete — decode and play
	if (session->codec2Data == NULL || session->codec2Size == 0) {
		if (!session->Reassemble()) {
			_LogMessage("VOICE", "Failed to reassemble voice data");
			return;
		}
	}

	int16* pcm = NULL;
	size_t pcmSamples = 0;
	if (VoiceCodec::Decode(session->codec2Data, session->codec2Size,
		session->mode, &pcm, &pcmSamples) != B_OK) {
		_LogMessage("VOICE", "Failed to decode Codec2 data");
		return;
	}

	// Store PCM so it stays valid during playback
	delete[] fVoicePlayPcm;
	fVoicePlayPcm = pcm;
	fVoicePlayPcmSize = pcmSamples;

	// Mark playing in message view
	for (int32 i = 0; i < fChatView->CountItems(); i++) {
		MessageView* mv = dynamic_cast<MessageView*>(
			fChatView->ItemAt(i));
		if (mv != NULL && mv->IsVoiceMessage()
			&& mv->VoiceSessionId() == sessionId) {
			mv->SetVoicePlaying(true);
			fChatView->InvalidateItem(i);
			break;
		}
	}

	fAudioEngine->Play(fVoicePlayPcm, fVoicePlayPcmSize, this);
	_LogMessage("VOICE", BString().SetToFormat(
		"Playing voice session %08x (%zu samples)",
		sessionId, pcmSamples));
}
