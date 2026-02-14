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
#include <Deskbar.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
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
#include <StringView.h>
#include <TextControl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "AddChannelWindow.h"
#include "ChatHeaderView.h"
#include "ChatView.h"
#include "Constants.h"
#include "ContactExportWindow.h"
#include "ContactInfoPanel.h"
#include "DatabaseManager.h"
#include "GrowingTextView.h"
#include "ContactItem.h"
#include "DebugLogWindow.h"
#include "LoginWindow.h"
#include "MapView.h"
#include "MqttClient.h"
#include "MqttLogWindow.h"
#include "MqttSettingsWindow.h"
#include "NetworkMapWindow.h"
#include "PacketAnalyzerWindow.h"
#include "RepeaterAdminWindow.h"
#include "NotificationManager.h"
#include "SerialHandler.h"
#include "SettingsWindow.h"
#include "StatsWindow.h"
#include "TelemetryWindow.h"
#include "TopBarView.h"
#include "TracePathWindow.h"


// MainWindow-private message codes
enum {
	MSG_SHOW_SETTINGS = 'shst',
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
};

// Timer intervals
static const bigtime_t kAutoConnectDelay = 1000000;     // 1 second
static const bigtime_t kStatsRefreshInterval = 10000000; // 10 seconds
static const bigtime_t kAutoSyncDelay = 3000000;         // 3 seconds


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


MainWindow::MainWindow()
	:
	BWindow(BRect(100, 100, 900, 650), APP_NAME,
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fSerialHandler(NULL),
	fMenuBar(NULL),
	fTopBar(NULL),
	fConnectItem(NULL),
	fDisconnectItem(NULL),
	fSearchField(NULL),
	fContactList(NULL),
	fContactScroll(NULL),
	fChannelItem(NULL),
	fSidebarDeviceLabel(NULL),
	fChatHeader(NULL),
	fChatView(NULL),
	fChatScroll(NULL),
	fMessageInput(NULL),
	fSendButton(NULL),
	fSearchBar(NULL),
	fMsgSearchField(NULL),
	fSearchCloseButton(NULL),
	fSearchActive(false),
	fInfoPanel(NULL),
	fMainSplit(NULL),
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
	fPendingMsgIndex(-1),
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
	fRepeaterAdminWindow(NULL),
	fMqttLogWindow(NULL),
	fMqttClient(NULL),
	fRawPacketCount(0),
	fLastRawPacketTime(0),
	fAutoConnectTimer(NULL),
	fStatsRefreshTimer(NULL),
	fAutoSyncRunner(NULL),
	fBatteryMv(0),
	fLastRssi(0),
	fLastSnr(0),
	fNoiseFloor(0),
	fTxPackets(0),
	fRxPackets(0),
	fDeviceUptime(0)
{
	// Initialize device info
	memset(fDeviceName, 0, sizeof(fDeviceName));
	memset(fDeviceFirmware, 0, sizeof(fDeviceFirmware));
	memset(fDeviceBoard, 0, sizeof(fDeviceBoard));
	memset(fPublicKey, 0, sizeof(fPublicKey));
	fSelfNodeId = 0;
	memset(fLoginTargetKey, 0, sizeof(fLoginTargetKey));
	memset(fLoggedInKey, 0, sizeof(fLoggedInKey));
	fHasDeviceInfo = false;
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

	// Create serial handler
	fSerialHandler = new SerialHandler(this);
	fSerialHandler->Run();

	// MQTT client is created lazily when needed
	fprintf(stderr, "[MainWindow] MQTT will be initialized on demand\n");
	fMqttClient = NULL;
	_LoadMqttSettings();  // Just load settings, don't create client yet
	fTopBar->SetMqttEnabled(fMqttSettings.enabled);

	fprintf(stderr, "[MainWindow] Refreshing ports...\n");

	// Refresh available ports
	_RefreshPorts();
	fprintf(stderr, "[MainWindow] Ports refreshed\n");

	// Load contacts from Haiku People files
	fprintf(stderr, "[MainWindow] Loading people contacts...\n");
	_LoadPeopleContacts();
	fprintf(stderr, "[MainWindow] People contacts loaded\n");

	SetSizeLimits(600, B_SIZE_UNLIMITED, 400, B_SIZE_UNLIMITED);

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
	deviceMenu->AddItem(new BMenuItem("Trace Path",
		new BMessage(MSG_SHOW_TRACE_PATH), 'T'));
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

	// Sidebar with search + contact list + device footer
	BView* sidebar = new BView("sidebar", 0);
	sidebar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	sidebar->SetExplicitMinSize(BSize(220, B_SIZE_UNSET));
	sidebar->SetExplicitPreferredSize(BSize(260, B_SIZE_UNSET));
	BLayoutBuilder::Group<>(sidebar, B_VERTICAL, 0)
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(4, 4, 4, 4)
			.Add(fSearchField, 1.0)
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

	// Input bar
	BView* inputBar = new BView("input_bar", 0);
	inputBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(inputBar, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(inputScroll, 1.0)
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

	// Chat panel (header + search bar + messages + input)
	BView* chatPanel = new BView("chat_panel", 0);
	chatPanel->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(chatPanel, B_VERTICAL, 0)
		.Add(fChatHeader)
		.Add(fSearchBar)
		.Add(fChatScroll, 1.0)
		.Add(inputBar)
	.End();

	// Hide search bar initially
	fSearchBar->Hide();

	// Hide chat header initially — info panel shows the contact name
	fChatHeader->Hide();

	// === RIGHT: Contact Info Panel ===
	fInfoPanel = new ContactInfoPanel("info_panel");

	// === MAIN SPLIT VIEW ===
	fMainSplit = new BSplitView(B_HORIZONTAL);
	fMainSplit->AddChild(sidebar, 0.22);
	fMainSplit->AddChild(chatPanel, 0.56);
	fMainSplit->AddChild(fInfoPanel, 0.22);
	fMainSplit->SetCollapsible(0, true);
	fMainSplit->SetCollapsible(1, false);
	fMainSplit->SetCollapsible(2, true);

	// === MENU BAR + STATUS BAR ===
	fMenuBar = new BMenuBar("menubar");
	_BuildMenuBar();
	fTopBar = new TopBarView("top_bar");

	// === MAIN LAYOUT ===
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fTopBar)
		.Add(fMainSplit, 1.0)
	.End();
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

		case MSG_SYNC_CONTACTS:
			_SendGetContacts();
			break;

		case MSG_SEND_ADVERT:
			_SendSelfAdvert();
			break;

		case MSG_DEVICE_QUERY:
			_SendDeviceQuery();
			break;

		case MSG_GET_BATTERY:
			_SendGetBattery();
			break;

		case MSG_GET_STATS:
			// Open stats window and request fresh data
			if (fStatsWindow == NULL) {
				fStatsWindow = new StatsWindow(this);
				fStatsWindow->Show();
			} else {
				_ShowWindow(fStatsWindow);
			}
			_SendGetStats();
			break;

		case MSG_REQUEST_STATS_DATA:
			// Just request stats data without opening/showing window
			_SendGetStats();
			break;

		case MSG_CONTACT_SELECTED:
		{
			int32 index = fContactList->CurrentSelection();
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

		case MSG_CONTACT_FILTER:
		{
			const char* filter = fSearchField->Text();
			_FilterContacts(filter);
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
			_SendGetStats();
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
			// Update with current contacts
			if (fNetworkMapWindow->LockLooper()) {
				fNetworkMapWindow->UpdateFromContacts(&fContacts);
				fNetworkMapWindow->UnlockLooper();
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
				fSettingsWindow->Show();
			} else {
				if (fSettingsWindow->LockLooper()) {
					if (fHasRadioParams) {
						fSettingsWindow->SetRadioParams(fRadioFreq, fRadioBw,
							fRadioSf, fRadioCr, fRadioTxPower);
					}
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

		case MSG_MQTT_SETTINGS_CHANGED:
		{
			// Update MQTT settings
			fMqttSettings.enabled = message->GetBool("enabled", false);
			fMqttSettings.latitude = message->GetDouble("latitude", 0.0);
			fMqttSettings.longitude = message->GetDouble("longitude", 0.0);
			strlcpy(fMqttSettings.iataCode, message->GetString("iata", "XXX"),
				sizeof(fMqttSettings.iataCode));
			strlcpy(fMqttSettings.broker, message->GetString("broker", "nodi.meshcoreitalia.it"),
				sizeof(fMqttSettings.broker));
			fMqttSettings.port = message->GetInt32("port", 1883);
			strlcpy(fMqttSettings.username, message->GetString("username", "meshcore"),
				sizeof(fMqttSettings.username));
			strlcpy(fMqttSettings.password, message->GetString("password", "meshcore25"),
				sizeof(fMqttSettings.password));
			strlcpy(fMqttSettings.publicKey, fPublicKey, sizeof(fMqttSettings.publicKey));

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
				_ShowWindow(fTracePathWindow);
			}
			break;
		}

		case MSG_TRACE_PATH:
		{
			// Handle trace path request from TracePathWindow or NetworkMapWindow
			const void* pubkey;
			ssize_t size;
			if (message->FindData("pubkey", B_RAW_TYPE, &pubkey, &size) == B_OK) {
				// Find contact for this pubkey
				ContactInfo* contact = _FindContactByPrefix((const uint8*)pubkey, kPubKeyPrefixSize);

				// Create/show trace window if not open
				if (fTracePathWindow == NULL) {
					fTracePathWindow = new TracePathWindow(this, contact);
					fTracePathWindow->Show();
				} else {
					_ShowWindow(fTracePathWindow);
				}

				// Send trace request to device
				if (fSerialHandler->IsConnected()) {
					uint8 payload[7];
					payload[0] = CMD_SEND_TRACE_PATH;
					memcpy(payload + 1, pubkey, 6);
					fSerialHandler->SendFrame(payload, 7);
					_LogMessage("INFO", "Sending trace path request...");
				}
			}
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
				_SendDeviceQuery();
				_SendExportSelf();
				_SendSelfAdvert();
				_SendGetContacts();
				_SendGetBattery();
				_SendGetStats();

				// Start periodic stats refresh timer
				delete fStatsRefreshTimer;
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
				_SendGetBattery();
				_SendGetStats();
			}
			break;
		}

		case MSG_AUTO_SYNC_CONTACTS:
		{
			delete fAutoSyncRunner;
			fAutoSyncRunner = NULL;
			if (fConnected) {
				_LogMessage("INFO", "Auto-syncing contacts after new node discovery");
				_SendGetContacts();
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
			} else if (ctxItem->IsChannel() && ctxItem->ChannelIndex() >= 0) {
				// Private channel context menu
				BMessage* removeMsg = new BMessage(MSG_REMOVE_CHANNEL);
				removeMsg->AddInt32("channel_idx", ctxItem->ChannelIndex());
				menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
			} else {
				// Contact context menu
				BMessage* resetMsg = new BMessage(MSG_CONTACT_RESET_PATH);
				resetMsg->AddData("pubkey", B_RAW_TYPE,
					ctxItem->GetContact().publicKey, kPubKeySize);
				menu->AddItem(new BMenuItem("Reset Path", resetMsg));
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

		case MSG_CONTACT_RESET_PATH:
		{
			const void* keyData;
			ssize_t keySize;
			if (message->FindData("pubkey", B_RAW_TYPE, &keyData, &keySize) == B_OK
				&& keySize >= (ssize_t)kPubKeySize) {
				_SendResetPath((const uint8*)keyData);
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
				_SendRemoveContact((const uint8*)keyData);
				// Schedule auto-sync after 3s
				delete fAutoSyncRunner;
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
				_SendRemoveChannel((uint8)chIdx);
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

			_SendSetChannel((uint8)emptySlot, name, secret);

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

		case MSG_SET_NAME:
		{
			const char* name;
			if (message->FindString("name", &name) == B_OK)
				_SendSetName(name);
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
					// freq and bw arrive in Hz from SettingsWindow
					// Protocol wire format: freq in kHz, bw in Hz
					uint32 freqKHz = freq / 1000;
					_LogMessage("INFO", BString().SetToFormat(
						"Setting radio: %.3f MHz, %.1f kHz BW, SF%u, CR%u",
						freq / 1000000.0, bw / 1000.0, sf, cr));

					uint8 payload[11];
					payload[0] = CMD_SET_RADIO_PARAMS;
					payload[1] = freqKHz & 0xFF;
					payload[2] = (freqKHz >> 8) & 0xFF;
					payload[3] = (freqKHz >> 16) & 0xFF;
					payload[4] = (freqKHz >> 24) & 0xFF;
					payload[5] = bw & 0xFF;
					payload[6] = (bw >> 8) & 0xFF;
					payload[7] = (bw >> 16) & 0xFF;
					payload[8] = (bw >> 24) & 0xFF;
					payload[9] = sf;
					payload[10] = cr;
					status_t err = fSerialHandler->SendFrame(payload,
						sizeof(payload));
					if (err == B_OK) {
						// Update local state so settings window shows new values
						fRadioFreq = freq;
						fRadioBw = bw;
						fRadioSf = sf;
						fRadioCr = cr;
						fHasRadioParams = true;
						_LogMessage("OK", BString().SetToFormat(
							"Radio params applied: %.3f MHz, %.1f kHz BW, SF%u, CR%u",
							freq / 1000000.0, bw / 1000.0, sf, cr));
					} else {
						_LogMessage("ERROR", BString().SetToFormat(
							"Failed to send radio params: %s",
							strerror(err)));
					}
				}
			}

			// Handle lat/lon
			double lat, lon;
			if (message->FindDouble("latitude", &lat) == B_OK &&
				message->FindDouble("longitude", &lon) == B_OK) {
				_SendSetLatLon(lat, lon);
			}

			// Handle TX power
			uint8 txpower;
			if (message->FindUInt8("txpower", &txpower) == B_OK) {
				_SendSetTxPower(txpower);
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

		// Repeater admin commands
		case MSG_ADMIN_REBOOT:
			_SendReboot();
			break;

		case MSG_ADMIN_FACTORY_RESET:
			_SendFactoryReset();
			break;

		case MSG_ADMIN_REMOVE_CONTACT:
		{
			const char* contactKey;
			if (message->FindString("contact_key", &contactKey) == B_OK) {
				// Parse hex key back to bytes
				uint8 keyBytes[6];
				for (int i = 0; i < 6 && contactKey[i * 2] != '\0'; i++) {
					unsigned int byte;
					sscanf(contactKey + i * 2, "%02X", &byte);
					keyBytes[i] = (uint8)byte;
				}
				_SendRemoveContact(keyBytes);
			}
			break;
		}

		case MSG_ADMIN_RESET_PATH:
		{
			const char* contactKey;
			if (message->FindString("contact_key", &contactKey) == B_OK) {
				uint8 keyBytes[6];
				for (int i = 0; i < 6 && contactKey[i * 2] != '\0'; i++) {
					unsigned int byte;
					sscanf(contactKey + i * 2, "%02X", &byte);
					keyBytes[i] = (uint8)byte;
				}
				_SendResetPath(keyBytes);
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
				uint8 payload[7];
				payload[0] = CMD_SEND_TELEMETRY_REQ;
				memcpy(payload + 1, item->GetContact().publicKey, 6);
				fSerialHandler->SendFrame(payload, 7);
				_LogMessage("INFO", "Requesting telemetry data...");

				// Open telemetry window
				PostMessage(MSG_SHOW_TELEMETRY);
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
				_SendLogin((const uint8*)pubkey, password);
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
			if (message->FindData("pubkey", B_RAW_TYPE, &pubkey, &size) == B_OK) {
				uint8 payload[7];
				payload[0] = CMD_EXPORT_CONTACT;
				memcpy(payload + 1, pubkey, 6);
				fSerialHandler->SendFrame(payload, 7);
				_LogMessage("INFO", "Requesting contact export data...");
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
	if (fMqttLogWindow != NULL) {
		fMqttLogWindow->Lock();
		fMqttLogWindow->Quit();
		fMqttLogWindow = NULL;
	}

	// Destroy singletons
	DebugLogWindow::Destroy();
	NotificationManager::Destroy();

	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
MainWindow::_SelectContact(int32 index)
{
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
		fMessageInput->SetEnabled(fConnected);
		fSendButton->SetEnabled(fConnected);

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
		fMessageInput->SetEnabled(fConnected);
		fSendButton->SetEnabled(fConnected);

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
			fChatHeader->SetConsoleMode(fLoggedIn &&
				memcmp(contact->publicKey, fLoggedInKey,
					kPubKeyPrefixSize) == 0);
			fChatView->SetCurrentContact(contact);
			fInfoPanel->SetContact(contact);
			fMessageInput->SetEnabled(fConnected);
			fSendButton->SetEnabled(fConnected);

			// Clear unread badge for this contact
			selectedItem->ClearUnread();
			fContactList->InvalidateItem(index);
		}
	} else {
		// Nothing selected
		fSendingToChannel = false;
		fChatHeader->SetContact(NULL);
		fChatHeader->SetConsoleMode(false);
		fChatView->SetCurrentContact(NULL);
		fInfoPanel->Clear();
		fMessageInput->SetEnabled(false);
		fSendButton->SetEnabled(false);
	}
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
MainWindow::_SendAppStart()
{
	_LogMessage("INFO", "Sending APP_START command...");
	// CMD_APP_START format: [0]=code [1]=app_ver [2-7]=reserved(zeros) [8+]=app_name
	uint8 payload[32];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_APP_START;
	payload[1] = 3;  // app_target_ver (request V3 protocol)
	// [2-7] reserved zeros already set by memset
	const char* appName = APP_NAME;
	size_t nameLen = strlen(appName);
	if (nameLen > sizeof(payload) - 9)
		nameLen = sizeof(payload) - 9;
	memcpy(payload + 8, appName, nameLen);
	fSerialHandler->SendFrame(payload, 8 + nameLen);
}


void
MainWindow::_SendDeviceQuery()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}
	_LogMessage("INFO", "Querying device info...");
	uint8 payload[2];
	payload[0] = CMD_DEVICE_QUERY;
	payload[1] = 3;  // Request V3 protocol (SNR, RSSI, pathLen in messages)
	fSerialHandler->SendFrame(payload, 2);
}


void
MainWindow::_SendExportSelf()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}
	_LogMessage("INFO", "Requesting self export (for public key)...");
	// CMD_EXPORT_CONTACT without public key = export SELF
	uint8 payload[1];
	payload[0] = CMD_EXPORT_CONTACT;
	fSerialHandler->SendFrame(payload, 1);
}


void
MainWindow::_SendRadioParams()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	const RadioPresetInfo& preset = kRadioPresets[fSelectedPreset];
	uint32 freqHz = preset.frequency;
	uint32 bwHz = preset.bandwidth;

	_LogMessage("INFO", BString().SetToFormat(
		"Setting radio: %.3f MHz, %u kHz BW, SF%u, CR%u",
		freqHz / 1000000.0, bwHz / 1000, preset.spreadingFactor, preset.codingRate));

	uint8 payload[11];
	payload[0] = CMD_SET_RADIO_PARAMS;
	payload[1] = freqHz & 0xFF;
	payload[2] = (freqHz >> 8) & 0xFF;
	payload[3] = (freqHz >> 16) & 0xFF;
	payload[4] = (freqHz >> 24) & 0xFF;
	payload[5] = bwHz & 0xFF;
	payload[6] = (bwHz >> 8) & 0xFF;
	payload[7] = (bwHz >> 16) & 0xFF;
	payload[8] = (bwHz >> 24) & 0xFF;
	payload[9] = preset.spreadingFactor;
	payload[10] = preset.codingRate;

	fSerialHandler->SendFrame(payload, sizeof(payload));
}


void
MainWindow::_SendGetContacts()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}
	_LogMessage("INFO", "Requesting contact list...");
	fSyncingContacts = true;
	// Note: Don't clear contacts here - wait for RSP_CONTACTS_START
	// which will preserve message history

	uint8 payload[1];
	payload[0] = CMD_GET_CONTACTS;
	fSerialHandler->SendFrame(payload, 1);
}


void
MainWindow::_SendSelfAdvert()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}
	_LogMessage("INFO", "Sending self advertisement...");
	uint8 payload[1];
	payload[0] = CMD_SEND_SELF_ADVERT;
	fSerialHandler->SendFrame(payload, 1);
}


void
MainWindow::_SendGetBattery()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}
	_LogMessage("INFO", "Requesting battery status...");
	uint8 payload[1];
	payload[0] = CMD_GET_BATT_AND_STORAGE;
	fSerialHandler->SendFrame(payload, 1);
}


void
MainWindow::_SendGetStats()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}
	_LogMessage("INFO", "Requesting statistics...");

	uint8 payload[2];
	payload[0] = CMD_GET_STATS;

	// Request all three stat subtypes
	payload[1] = 0;  // Core stats
	fSerialHandler->SendFrame(payload, 2);

	payload[1] = 1;  // Radio stats
	fSerialHandler->SendFrame(payload, 2);

	payload[1] = 2;  // Packet stats
	fSerialHandler->SendFrame(payload, 2);
}


void
MainWindow::_SendSetName(const char* name)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	size_t nameLen = strlen(name);
	if (nameLen > 31) {
		_LogMessage("ERROR", "Name too long (max 31 chars)");
		return;
	}

	_LogMessage("INFO", BString("Setting node name to: ") << name);

	uint8 payload[33];
	payload[0] = CMD_SET_ADVERT_NAME;
	memcpy(payload + 1, name, nameLen);
	payload[1 + nameLen] = '\0';

	fSerialHandler->SendFrame(payload, 2 + nameLen);
}


void
MainWindow::_SendSetLatLon(double lat, double lon)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	int32 latInt = (int32)(lat * 1000000.0);
	int32 lonInt = (int32)(lon * 1000000.0);

	_LogMessage("INFO", BString().SetToFormat(
		"Setting lat/lon: %.6f, %.6f", lat, lon));

	uint8 payload[9];
	payload[0] = CMD_SET_ADVERT_LATLON;
	payload[1] = latInt & 0xFF;
	payload[2] = (latInt >> 8) & 0xFF;
	payload[3] = (latInt >> 16) & 0xFF;
	payload[4] = (latInt >> 24) & 0xFF;
	payload[5] = lonInt & 0xFF;
	payload[6] = (lonInt >> 8) & 0xFF;
	payload[7] = (lonInt >> 16) & 0xFF;
	payload[8] = (lonInt >> 24) & 0xFF;

	fSerialHandler->SendFrame(payload, sizeof(payload));

	// Also enable location sharing in adverts if not already on
	if (fAdvertLocPolicy == 0 && (lat != 0.0 || lon != 0.0)) {
		_LogMessage("INFO", "Enabling advert_loc_policy to share GPS in adverts");
		fAdvertLocPolicy = 1;
		_SendOtherParams();
	}
}


void
MainWindow::_SendOtherParams()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	_LogMessage("INFO", BString().SetToFormat(
		"Sending other params: manualAdd=%d telemetry=%d locPolicy=%d multiAcks=%d",
		fManualAddContacts, fTelemetryModes, fAdvertLocPolicy, fMultiAcks));

	uint8 payload[5];
	payload[0] = CMD_SET_OTHER_PARAMS;
	payload[1] = fManualAddContacts;
	payload[2] = fTelemetryModes;
	payload[3] = fAdvertLocPolicy;
	payload[4] = fMultiAcks;
	fSerialHandler->SendFrame(payload, sizeof(payload));
}


void
MainWindow::_SendSetTxPower(uint8 power)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	_LogMessage("INFO", BString().SetToFormat("Setting TX power to %d dBm", power));

	uint8 payload[2];
	payload[0] = CMD_SET_RADIO_TX_POWER;
	payload[1] = power;
	fSerialHandler->SendFrame(payload, 2);
}


void
MainWindow::_SendLogin(const uint8* pubkey, const char* password)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	size_t passLen = strlen(password);
	if (passLen > 15)
		passLen = 15;  // Protocol max password length

	uint8 payload[128];
	payload[0] = CMD_SEND_LOGIN;
	memcpy(payload + 1, pubkey, kPubKeySize);  // Full 32-byte public key
	memcpy(payload + 33, password, passLen);
	payload[33 + passLen] = '\0';

	fSerialHandler->SendFrame(payload, 34 + passLen);

	char hexKey[13];
	for (int i = 0; i < 6; i++)
		snprintf(hexKey + i * 2, 3, "%02X", pubkey[i]);
	_LogMessage("INFO", BString().SetToFormat(
		"Login sent to %s, frame size=%zu, waiting for 0x85/0x86...",
		hexKey, 34 + passLen));
}


void
MainWindow::_SendReboot()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	uint8 payload[1];
	payload[0] = CMD_REBOOT;
	fSerialHandler->SendFrame(payload, 1);
	_LogMessage("INFO", "Sending reboot command...");
}


void
MainWindow::_SendFactoryReset()
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	uint8 payload[1];
	payload[0] = CMD_FACTORY_RESET;
	fSerialHandler->SendFrame(payload, 1);
	_LogMessage("INFO", "Sending factory reset command...");
}


void
MainWindow::_SendResetPath(const uint8* pubkey)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	uint8 payload[7];
	payload[0] = CMD_RESET_PATH;
	memcpy(payload + 1, pubkey, 6);
	fSerialHandler->SendFrame(payload, 7);
	_LogMessage("INFO", "Sending reset path command...");
}


void
MainWindow::_SendRemoveContact(const uint8* pubkey)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	uint8 payload[7];
	payload[0] = CMD_REMOVE_CONTACT;
	memcpy(payload + 1, pubkey, 6);
	fSerialHandler->SendFrame(payload, 7);
	_LogMessage("INFO", "Sending remove contact command...");
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

	{
		char hex[13];
		for (int i = 0; i < 6; i++)
			snprintf(hex + i * 2, 3, "%02X", contact->publicKey[i]);
		_LogMessage("INFO", BString().SetToFormat(
			"Sending DM to %s [%s] (idx %d, cli=%d): %s",
			contact->name, hex, (int)fSelectedContact,
			(int)(fLoggedIn && memcmp(contact->publicKey,
				fLoggedInKey, kPubKeyPrefixSize) == 0),
			text));
	}

	uint8 payload[256];
	size_t pos = 0;

	// Use CLI txt_type when logged into this contact
	bool isCli = (fLoggedIn &&
		memcmp(contact->publicKey, fLoggedInKey, kPubKeyPrefixSize) == 0);

	payload[pos++] = CMD_SEND_TXT_MSG;
	payload[pos++] = isCli ? TXT_TYPE_CLI_DATA : TXT_TYPE_PLAIN;
	payload[pos++] = 0;  // attempt

	uint32 timestamp = (uint32)time(NULL);
	payload[pos++] = timestamp & 0xFF;
	payload[pos++] = (timestamp >> 8) & 0xFF;
	payload[pos++] = (timestamp >> 16) & 0xFF;
	payload[pos++] = (timestamp >> 24) & 0xFF;

	memcpy(payload + pos, contact->publicKey, 6);
	pos += 6;

	memcpy(payload + pos, text, textLen);
	pos += textLen;

	fSerialHandler->SendFrame(payload, pos);

	// Add to chat view as outgoing message (pending delivery)
	ChatMessage outMsg;
	memcpy(outMsg.pubKeyPrefix, contact->publicKey, kPubKeyPrefixSize);
	strlcpy(outMsg.text, text, sizeof(outMsg.text));
	outMsg.timestamp = timestamp;
	outMsg.isOutgoing = true;
	outMsg.isChannel = false;
	outMsg.pathLen = 0;
	outMsg.snr = 0;
	outMsg.txtType = isCli ? TXT_TYPE_CLI_DATA : TXT_TYPE_PLAIN;
	outMsg.deliveryStatus = DELIVERY_PENDING;
	fChatView->AddMessage(outMsg, "Me");

	// Track pending message index for delivery status updates
	fPendingMsgIndex = fChatView->CountItems() - 1;

	// Store message in contact's history
	ChatMessage* stored = new ChatMessage(outMsg);
	contact->messages.AddItem(stored);

	// Persist to database
	char contactHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(contactHex + i * 2, 3, "%02x", contact->publicKey[i]);
	DatabaseManager::Instance()->InsertMessage(contactHex, outMsg);

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

	uint8 payload[256];
	size_t pos = 0;

	payload[pos++] = CMD_SEND_CHANNEL_TXT_MSG;
	// Use selected channel index (0 = public, N = private channel slot)
	uint8 wireChannelIdx = (fSelectedChannelIdx >= 0)
		? (uint8)fSelectedChannelIdx : 0;

	payload[pos++] = 0;  // txt_type: plain
	payload[pos++] = wireChannelIdx;

	// Timestamp (4 bytes, little-endian)
	uint32 timestamp = (uint32)time(NULL);
	payload[pos++] = timestamp & 0xFF;
	payload[pos++] = (timestamp >> 8) & 0xFF;
	payload[pos++] = (timestamp >> 16) & 0xFF;
	payload[pos++] = (timestamp >> 24) & 0xFF;

	// Text content
	memcpy(payload + pos, text, textLen);
	pos += textLen;

	fSerialHandler->SendFrame(payload, pos);

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

	// Debug: log every incoming frame command byte
	if (length >= 1) {
		BString hexDump;
		size_t dumpLen = std::min(length, (size_t)16);
		for (size_t i = 0; i < dumpLen; i++)
			hexDump << BString().SetToFormat("%02X ", data[i]);
		_LogMessage("FRAME", BString().SetToFormat(
			"cmd=0x%02X len=%zu [%s]", data[0], length,
			hexDump.String()));
	}

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
				roundTripMs = (uint32)data[5] | ((uint32)data[6] << 8)
					| ((uint32)data[7] << 16) | ((uint32)data[8] << 24);
			}

			BString logMsg("Message delivery confirmed");
			if (roundTripMs > 0)
				logMsg.SetToFormat("Message delivery confirmed (RTT: %lums)",
					(unsigned long)roundTripMs);
			_LogMessage("OK", logMsg.String());

			// Update pending message to CONFIRMED status
			if (fPendingMsgIndex >= 0) {
				fChatView->UpdateDeliveryStatus(fPendingMsgIndex,
					DELIVERY_CONFIRMED, roundTripMs);

				// Also update stored ChatMessage
				ContactInfo* contact = fChatView->CurrentContact();
				if (contact != NULL) {
					int32 lastIdx = contact->messages.CountItems() - 1;
					if (lastIdx >= 0) {
						ChatMessage* msg = contact->messages.ItemAt(lastIdx);
						if (msg != NULL && msg->isOutgoing) {
							msg->deliveryStatus = DELIVERY_CONFIRMED;
							msg->roundTripMs = roundTripMs;
						}
					}
				}

				fPendingMsgIndex = -1;
			}
			break;
		}
		case PUSH_PATH_UPDATED:
			_LogMessage("INFO", "Path updated for a contact");
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

		default:
			_LogMessage("WARN", BString().SetToFormat("Unknown response: 0x%02X", cmd));
			break;
	}
}


void
MainWindow::_HandleDeviceInfo(const uint8* data, size_t length)
{
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
		size_t fwLen = strnlen((const char*)data + 60, 20);
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
		_SendGetChannel(0);
	}
}


void
MainWindow::_HandleExportContact(const uint8* data, size_t length)
{
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
		for (int i = 0; i < 32; i++) {
			snprintf(fPublicKey + i * 2, 3, "%02X", data[3 + i]);
		}
		fPublicKey[64] = '\0';  // Ensure exactly 64 chars

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
		count = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
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
	if (length < 148) {
		_LogMessage("WARN", BString().SetToFormat(
			"Contact frame too short: %zu bytes", length));
		return;
	}

	ContactInfo* contact = new ContactInfo();
	memcpy(contact->publicKey, data + 1, 32);
	contact->type = data[33];
	contact->flags = data[34];
	contact->outPathLen = (int8)data[35];
	memcpy(contact->name, data + 100, 32);
	contact->name[31] = '\0';
	contact->lastSeen = data[132] | (data[133] << 8) |
		(data[134] << 16) | (data[135] << 24);
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
	_LogMessage("INFO", BString().SetToFormat("Contact: %s (type:%d flags:0x%02X rssi:%d lastSeen:%u age:%us)",
		contact->name[0] ? contact->name : "(unnamed)",
		contact->type, contact->flags, contact->outPathLen, contact->lastSeen, age));
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
	_SendSyncNextMessage();

	// Forward contact list to RepeaterAdminWindow if open
	if (fRepeaterAdminWindow != NULL) {
		if (fRepeaterAdminWindow->LockLooper()) {
			fRepeaterAdminWindow->UpdateContactList(&fContacts);
			fRepeaterAdminWindow->UnlockLooper();
		}
	}
}


void
MainWindow::_HandleSelfInfo(const uint8* data, size_t length)
{
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
		fRadioTxPower = data[2];
		uint8 maxTxPower = data[3];
		_LogMessage("INFO", BString().SetToFormat(
			"Self type:%d txPower:%d maxTxPower:%d", advType, fRadioTxPower, maxTxPower));

		// Extract and store public key as hex string (offset 4-35)
		for (int i = 0; i < 32; i++) {
			snprintf(fPublicKey + i * 2, 3, "%02X", data[4 + i]);
		}
		fPublicKey[64] = '\0';

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
		int32 latRaw = data[36] | (data[37] << 8) | (data[38] << 16) | (data[39] << 24);
		int32 lonRaw = data[40] | (data[41] << 8) | (data[42] << 16) | (data[43] << 24);
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
		uint32 freqKHz = data[48] | (data[49] << 8) | (data[50] << 16)
			| (data[51] << 24);
		fRadioFreq = freqKHz * 1000;  // kHz → Hz
		fRadioBw = data[52] | (data[53] << 8) | (data[54] << 16) | (data[55] << 24);
		fRadioSf = data[56];
		fRadioCr = data[57];
		fHasRadioParams = true;
		_LogMessage("INFO", BString().SetToFormat(
			"Radio: %.3f MHz, %.1f kHz BW, SF%u, CR%u",
			fRadioFreq / 1000000.0, fRadioBw / 1000.0, fRadioSf, fRadioCr));
	}

	if (length > 58) {
		// Extract device name (null-terminated string at offset 58)
		size_t nameLen = strnlen((const char*)(data + 58), length - 58);
		if (nameLen > sizeof(fDeviceName) - 1)
			nameLen = sizeof(fDeviceName) - 1;
		memcpy(fDeviceName, data + 58, nameLen);
		fDeviceName[nameLen] = '\0';

		_LogMessage("INFO", BString().SetToFormat("Device name: %s", fDeviceName));

		// Update Network Map with self name
		if (fNetworkMapWindow != NULL && !fNetworkMapWindow->IsHidden()) {
			if (fNetworkMapWindow->LockLooper()) {
				fNetworkMapWindow->SetSelfInfo(fDeviceName);
				fNetworkMapWindow->UnlockLooper();
			}
		}
	}

	// Mark that we have device info and publish MQTT status if connected
	fHasDeviceInfo = true;
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
		snr = (int8)data[1];
		senderPrefix = data + 4;
		pathLen = data[10];
		txtType = data[11];  // 0=plain, 1=cli_data, 2=signed_plain
		timestamp = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
		textOffset = 16;
	} else {
		if (length < 13) {
			_LogMessage("WARN", "Contact message frame too short");
			return;
		}
		senderPrefix = data + 1;
		pathLen = data[7];
		snr = 0;  // V2 does not include SNR
		txtType = data[8];  // 0=plain, 1=cli_data
		timestamp = data[9] | (data[10] << 8) | (data[11] << 16) | (data[12] << 24);
		textOffset = 13;
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
		// Increment unread if not currently viewing this contact
		if (fChatView->CurrentContact() == NULL ||
			memcmp(fChatView->CurrentContact()->publicKey, senderPrefix,
				kPubKeyPrefixSize) != 0) {
			item->IncrementUnread();
		}
		fContactList->Invalidate();
	}

	// Store message in contact's history and update lastSeen
	if (sender != NULL) {
		ChatMessage* stored = new ChatMessage(chatMsg);
		sender->messages.AddItem(stored);
		sender->lastSeen = (uint32)time(NULL);

		// Persist to database and record SNR history
		char contactHex[13];
		for (int i = 0; i < 6; i++)
			snprintf(contactHex + i * 2, 3, "%02x", senderPrefix[i]);
		DatabaseManager* db = DatabaseManager::Instance();
		db->InsertMessage(contactHex, chatMsg);

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

	// Desktop notification if window not active
	if (!IsActive()) {
		NotificationManager::Instance()->NotifyNewMessage(
			senderName.String(), text, false);
	}

	// Trigger pulse and update link quality on network map
	if (fNetworkMapWindow != NULL && !fNetworkMapWindow->IsHidden()) {
		if (fNetworkMapWindow->LockLooper()) {
			fNetworkMapWindow->TriggerNodePulse(senderPrefix);
			if (snr != 0)
				fNetworkMapWindow->UpdateLinkQuality(senderPrefix, snr, fLastRssi);
			fNetworkMapWindow->UnlockLooper();
		}
	}

	// Publish to MQTT /packets topic
	if (fMqttClient != NULL && fMqttClient->IsConnected()) {
		fMqttClient->PublishPacket(timestamp, snr, fLastRssi,
			"DM", senderPrefix, 6,
			(const uint8*)text, textLen);
	}

	// Continue offline message sync loop
	if (fSyncingMessages)
		_SendSyncNextMessage();
}


void
MainWindow::_HandleChannelMsgRecv(const uint8* data, size_t length, bool isV3)
{
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
		snr = (int8)data[1];
		channelIdx = data[4];
		pathLen = data[5];
		// data[6] = txt_type
		timestamp = data[7] | (data[8] << 8) | (data[9] << 16) | (data[10] << 24);
		textOffset = 11;
	} else {
		if (length < 9) {
			_LogMessage("WARN", "Channel message frame too short");
			return;
		}
		channelIdx = data[1];
		pathLen = data[2];
		snr = 0;  // V2 does not include SNR
		// data[3] = txt_type
		timestamp = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
		textOffset = 8;
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

	// Record SNR data point for sender contact
	if (sender != NULL && snr != 0) {
		char senderHex[13];
		for (int i = 0; i < 6; i++)
			snprintf(senderHex + i * 2, 3, "%02x", sender->publicKey[i]);
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

	// Update sidebar item
	if (channelIdx == 0) {
		fChannelItem->SetLastMessage(messageText, timestamp);
		if (!isCurrentChannel)
			fChannelItem->IncrementUnread();
		fContactList->InvalidateItem(0);
	} else {
		// Update private channel sidebar item
		for (int32 i = 1; i < fContactList->CountItems(); i++) {
			ContactItem* ci = dynamic_cast<ContactItem*>(fContactList->ItemAt(i));
			if (ci != NULL && ci->IsChannel()
				&& ci->ChannelIndex() == (int32)channelIdx) {
				ci->SetLastMessage(messageText, timestamp);
				if (!isCurrentChannel)
					ci->IncrementUnread();
				fContactList->InvalidateItem(i);
				break;
			}
		}
	}

	// Desktop notification if window not active
	if (!IsActive()) {
		NotificationManager::Instance()->NotifyNewMessage(
			senderName.String(), messageText, true);
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
		_SendSyncNextMessage();
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
		uint16 battMv = data[1] | (data[2] << 8);
		fBatteryMv = battMv;

		uint32 usedKb = 0;
		uint32 totalKb = 0;
		if (length >= 7)
			usedKb = data[3] | (data[4] << 8) | (data[5] << 16) | (data[6] << 24);
		if (length >= 11)
			totalKb = data[7] | (data[8] << 8) | (data[9] << 16) | (data[10] << 24);

		// Calculate storage percentage for status bar
		int8 storagePct = 0;
		if (totalKb > 0)
			storagePct = (int8)((usedKb * 100) / totalKb);

		_LogMessage("INFO", BString().SetToFormat(
			"Battery: %u mV, Storage: %u/%u KB (%d%%)",
			battMv, usedKb, totalKb, storagePct));

		fTopBar->SetBattery(battMv);

		// Forward battery voltage to TelemetryWindow as sensor data
		if (fTelemetryWindow != NULL) {
			if (fTelemetryWindow->LockLooper()) {
				float batteryV = battMv / 1000.0f;
				fTelemetryWindow->AddTelemetryData(fSelfNodeId, "Battery",
					SENSOR_BATTERY, batteryV, "V");
				if (totalKb > 0) {
					float storagePctF = (usedKb * 100.0f) / totalKb;
					fTelemetryWindow->AddTelemetryData(fSelfNodeId, "Storage",
						SENSOR_CUSTOM, storagePctF, "%");
				}
				fTelemetryWindow->UnlockLooper();
			}
		}

		// Forward to RepeaterAdminWindow if open
		if (fRepeaterAdminWindow != NULL) {
			if (fRepeaterAdminWindow->LockLooper()) {
				fRepeaterAdminWindow->SetBatteryInfo(battMv, usedKb, totalKb);
				fRepeaterAdminWindow->UnlockLooper();
			}
		}
	}
}


void
MainWindow::_HandleStats(const uint8* data, size_t length)
{
	if (length < 2)
		return;

	uint8 subType = data[1];

	// Parse stats for status bar (per MeshCore Companion Protocol)
	switch (subType) {
		case 0:  // Core stats (11 bytes)
			// [2-3]=batt_mv(int16) [4-7]=uptime(uint32) [8-9]=err_flags [10]=queue_len
			if (length >= 8) {
				fDeviceUptime = data[4] | (data[5] << 8) |
					(data[6] << 16) | (data[7] << 24);
				fTopBar->SetUptime(fDeviceUptime);
			}
			if (length >= 4) {
				uint16 battMv = data[2] | (data[3] << 8);
				if (battMv > 0) {
					fBatteryMv = battMv;
				}
			}
			break;

		case 1:  // Radio stats (14 bytes)
			// [2-3]=noise_floor(int16) [4]=rssi(int8) [5]=snr(int8)
			// [6-9]=tx_air_time [10-13]=rx_air_time
			if (length >= 6) {
				fNoiseFloor = (int8)(data[2] | (data[3] << 8));
				fLastRssi = (int8)data[4];
				fLastSnr = (int8)data[5];
				fTopBar->SetRadioStats(fLastRssi, fLastSnr, fTxPackets, fRxPackets);

				// Forward radio stats to TelemetryWindow as sensor data
				if (fTelemetryWindow != NULL) {
					if (fTelemetryWindow->LockLooper()) {
						fTelemetryWindow->AddTelemetryData(fSelfNodeId, "Noise Floor",
							SENSOR_CUSTOM, (float)fNoiseFloor, "dBm");
						fTelemetryWindow->AddTelemetryData(fSelfNodeId, "RSSI",
							SENSOR_CUSTOM, (float)fLastRssi, "dBm");
						fTelemetryWindow->AddTelemetryData(fSelfNodeId, "SNR",
							SENSOR_CUSTOM, (float)fLastSnr, "dB");
						fTelemetryWindow->UnlockLooper();
					}
				}
			}
			break;

		case 2:  // Packet stats (26 bytes)
			// [2-5]=recvPkts [6-9]=sentPkts [10-13]=sentFlood [14-17]=sentDirect
			// [18-21]=recvFlood [22-25]=recvDirect
			if (length >= 10) {
				fRxPackets = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
				fTxPackets = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
				fTopBar->SetRadioStats(fLastRssi, fLastSnr, fTxPackets, fRxPackets);
			}
			break;
	}

	// Forward to StatsWindow if open (must lock the window first!)
	if (fStatsWindow != NULL && !fStatsWindow->IsHidden()) {
		if (fStatsWindow->LockLooper()) {
			fStatsWindow->ParseStatsResponse(data, length);
			fStatsWindow->UnlockLooper();
		}
	}

	// Forward to RepeaterAdminWindow if open
	if (fRepeaterAdminWindow != NULL) {
		if (fRepeaterAdminWindow->LockLooper()) {
			fRepeaterAdminWindow->SetStats(fDeviceUptime, fTxPackets,
				fRxPackets, fLastRssi, fLastSnr, fNoiseFloor);
			fRepeaterAdminWindow->UnlockLooper();
		}
	}
}


void
MainWindow::_HandlePushMsgWaiting(const uint8* data, size_t length)
{
	_LogMessage("INFO", "Messages waiting - fetching...");
	_SendSyncNextMessage();
}


void
MainWindow::_SendSyncNextMessage()
{
	if (!fSerialHandler->IsConnected())
		return;

	fSyncingMessages = true;
	uint8 payload[1];
	payload[0] = CMD_SYNC_NEXT_MESSAGE;
	fSerialHandler->SendFrame(payload, 1);
}


void
MainWindow::_SendGetChannel(uint8 index)
{
	if (!fSerialHandler->IsConnected())
		return;

	uint8 payload[2];
	payload[0] = CMD_GET_CHANNEL;
	payload[1] = index;
	fSerialHandler->SendFrame(payload, 2);
}


void
MainWindow::_SendSetChannel(uint8 index, const char* name, const uint8* secret)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	uint8 payload[50];
	payload[0] = CMD_SET_CHANNEL;
	payload[1] = index;
	memset(payload + 2, 0, 32);
	strlcpy((char*)(payload + 2), name, 32);
	memcpy(payload + 34, secret, 16);
	fSerialHandler->SendFrame(payload, sizeof(payload));

	_LogMessage("INFO", BString().SetToFormat(
		"Setting channel %d: %s", index, name));
}


void
MainWindow::_SendRemoveChannel(uint8 index)
{
	if (!fSerialHandler->IsConnected()) {
		_LogMessage("ERROR", "Not connected");
		return;
	}

	// Remove = set with empty name and zero secret
	uint8 payload[50];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_SET_CHANNEL;
	payload[1] = index;
	fSerialHandler->SendFrame(payload, sizeof(payload));

	_LogMessage("INFO", BString().SetToFormat("Removing channel %d", index));
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
			_SendGetChannel(fChannelEnumIndex);
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
		char contactHex[13];
		for (int i = 0; i < 6; i++)
			snprintf(contactHex + i * 2, 3, "%02x", pubKeyPrefix[i]);
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
	if (fNetworkMapWindow != NULL && !fNetworkMapWindow->IsHidden()) {
		if (fNetworkMapWindow->LockLooper()) {
			fNetworkMapWindow->TriggerNodePulse(pubKeyPrefix);
			if (snr != 0)
				fNetworkMapWindow->UpdateLinkQuality(pubKeyPrefix, snr / 4, rssi);
			fNetworkMapWindow->UnlockLooper();
		}
	}

	// Publish to MQTT if connected
	if (fMqttClient != NULL && fMqttClient->IsConnected()) {
		fMqttClient->PublishPacket(now, snr, rssi, "advert",
			pubKeyPrefix, kPubKeyPrefixSize, data, length);
	}
}


void
MainWindow::_HandlePushTraceData(const uint8* data, size_t length)
{
	_LogMessage("INFO", "Trace path data received");

	// Forward to TracePathWindow if open (must lock the window first!)
	if (fTracePathWindow != NULL && !fTracePathWindow->IsHidden()) {
		if (fTracePathWindow->LockLooper()) {
			fTracePathWindow->ParseTraceData(data, length);
			fTracePathWindow->UnlockLooper();
		}
	}

	// Forward to NetworkMapWindow for trace route visualization
	if (fNetworkMapWindow != NULL && !fNetworkMapWindow->IsHidden()) {
		if (fNetworkMapWindow->LockLooper()) {
			fNetworkMapWindow->HandleTraceData(data, length);
			fNetworkMapWindow->UnlockLooper();
		}
	}

	// Log trace path summary
	if (length >= 12) {
		uint8 pathLen = data[2];
		_LogMessage("TRACE", BString().SetToFormat(
			"Trace path: %d hops, flags=0x%02X", pathLen, data[3]));
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
		fTelemetryWindow->AddTelemetryData(nodeId, sensorName, type, value, unit);
		fTelemetryWindow->UnlockLooper();
	}
}


void
MainWindow::_HandlePushLoginResult(uint8 code)
{
	fLoginPending = false;
	bool success = (code == PUSH_LOGIN_SUCCESS);

	if (success) {
		char hex[13];
		for (int i = 0; i < 6; i++)
			snprintf(hex + i * 2, 3, "%02X", fLoginTargetKey[i]);
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

		_SendGetContacts();
		_SendGetBattery();
		_SendGetStats();

		// Open Repeater Admin window for repeater/room
		ContactInfo* targetContact = _FindContactByPrefix(
			fLoginTargetKey, kPubKeyPrefixSize);
		if (targetContact != NULL
			&& (targetContact->type == 2 || targetContact->type == 3)) {
			if (fRepeaterAdminWindow == NULL) {
				fRepeaterAdminWindow = new RepeaterAdminWindow(this,
					targetContact->publicKey, targetContact->name,
					targetContact->type);
				fRepeaterAdminWindow->Show();
			} else {
				if (fRepeaterAdminWindow->LockLooper()) {
					fRepeaterAdminWindow->SetSessionActive(true);
					fRepeaterAdminWindow->UnlockLooper();
				}
				_ShowWindow(fRepeaterAdminWindow);
			}
		}
	}
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

	// Update pending message to SENT status
	if (fPendingMsgIndex >= 0) {
		fChatView->UpdateDeliveryStatus(fPendingMsgIndex, DELIVERY_SENT);

		// Also update the stored ChatMessage in contact history
		ContactInfo* contact = fChatView->CurrentContact();
		if (contact != NULL) {
			int32 lastIdx = contact->messages.CountItems() - 1;
			if (lastIdx >= 0) {
				ChatMessage* msg = contact->messages.ItemAt(lastIdx);
				if (msg != NULL && msg->isOutgoing)
					msg->deliveryStatus = DELIVERY_SENT;
			}
		}
	}
}


void
MainWindow::_HandleCmdErr(const uint8* data, size_t length)
{
	BString msg("Command error");
	if (length > 1) {
		msg.SetToFormat("Command error: code %d", data[1]);
	}
	_LogMessage("ERROR", msg);
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
	}

	// Send APP_START first, then schedule init commands after a short delay
	_SendAppStart();

	// Use a one-shot delayed message instead of blocking snooze()
	BMessage initMsg(MSG_POST_CONNECT_INIT);
	BMessageRunner::StartSending(this, &initMsg, 100000, 1);  // 100ms, once
}


void
MainWindow::_OnDisconnected()
{
	fConnected = false;
	fHasDeviceInfo = false;  // Reset for next connection
	fSyncingMessages = false;
	fEnumeratingChannels = false;
	fPendingMsgIndex = -1;
	fLoggedIn = false;
	memset(fLoggedInKey, 0, kPubKeyPrefixSize);
	fChatHeader->SetConsoleMode(false);

	// Stop periodic stats timer
	delete fStatsRefreshTimer;
	fStatsRefreshTimer = NULL;

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
	if (fNetworkMapWindow != NULL && !fNetworkMapWindow->IsHidden()) {
		if (fNetworkMapWindow->LockLooper()) {
			fNetworkMapWindow->UpdateFromContacts(&fContacts);
			fNetworkMapWindow->UnlockLooper();
		}
	}
}


void
MainWindow::_FilterContacts(const char* filter)
{
	// Suppress selection messages while rebuilding the list to avoid
	// queued MSG_CONTACT_SELECTED overriding programmatic re-selection
	fContactList->SetSelectionMessage(NULL);

	// Remove all items except the Channel item (index 0)
	while (fContactList->CountItems() > 1) {
		BListItem* item = fContactList->RemoveItem(1);
		delete item;
	}

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
		fContactList->AddItem(item);
	}

	// Add contacts that match the filter
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* contact = fContacts.ItemAt(i);
		if (contact == NULL || !contact->isValid)
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

		ContactItem* item = new ContactItem(*contact);
		fContactList->AddItem(item);
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
	char* line = strtok(buffer, "\n");
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
		line = strtok(NULL, "\n");
	}

	delete[] buffer;

	// Apply to MQTT client if it exists
	if (fMqttClient != NULL) {
		fMqttClient->SetSettings(fMqttSettings);
	}

	fprintf(stderr, "[MainWindow] MQTT settings loaded (enabled=%s, lat=%.4f, lon=%.4f)\n",
		fMqttSettings.enabled ? "yes" : "no", fMqttSettings.latitude, fMqttSettings.longitude);
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

		char contactHex[13];
		for (int j = 0; j < 6; j++)
			snprintf(contactHex + j * 2, 3, "%02x", contact->publicKey[j]);

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

	BObjectList<ChatMessage, true> results(50);
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
	char hexPrefix[13];
	for (int i = 0; i < 6; i++) {
		snprintf(hexPrefix + i * 2, 3, "%02x", contact->publicKey[i]);
	}

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
		struct tm* tm = localtime(&lastTime);
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
		for (int i = 0; i < 32; i++) {
			unsigned int byte;
			if (sscanf(pubkeyHex + i * 2, "%2x", &byte) == 1)
				contact->publicKey[i] = (uint8)byte;
		}

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
