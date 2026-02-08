/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MainWindow.cpp — Main application window implementation
 */

#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Roster.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstring>
#include <cstdio>
#include <ctime>

#include <Deskbar.h>

#include "ChannelItem.h"
#include "ChatView.h"
#include "Constants.h"
#include "ContactExportWindow.h"
#include "ContactListView.h"
#include "DeskbarReplicant.h"
#include "LoginWindow.h"
#include "NotificationManager.h"
#include "PortSelectionWindow.h"
#include "Protocol.h"
#include "Sestriere.h"
#include "SerialHandler.h"
#include "SettingsWindow.h"
#include "StatsWindow.h"
#include "StatusBarView.h"
#include "TelemetryWindow.h"
#include "TracePathWindow.h"
#include "MapView.h"
#include "MeshGraphView.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"


MainWindow::MainWindow(BRect frame)
	:
	BWindow(frame, B_TRANSLATE_SYSTEM_NAME(kAppName),
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fMenuBar(NULL),
	fConnectItem(NULL),
	fDisconnectItem(NULL),
	fRefreshContactsItem(NULL),
	fSendAdvertItem(NULL),
	fLoginItem(NULL),
	fTracePathItem(NULL),
	fExportContactItem(NULL),
	fSplitView(NULL),
	fStatusBar(NULL),
	fContactList(NULL),
	fChatView(NULL),
	fMessageInput(NULL),
	fSendButton(NULL),
	fPlaceholderLabel(NULL),
	fConnected(false),
	fHandshakeComplete(false),
	fSendingToChannel(false),
	fExpectedContactCount(0),
	fReceivedContactCount(0),
	fBatteryTimer(NULL)
{
	memset(&fDeviceInfo, 0, sizeof(fDeviceInfo));
	memset(&fSelfInfo, 0, sizeof(fSelfInfo));

	_BuildMenu();
	_BuildLayout();
	_UpdateConnectionUI();

	SetSizeLimits(kMinWindowWidth, B_SIZE_UNLIMITED,
		kMinWindowHeight, B_SIZE_UNLIMITED);
}


MainWindow::~MainWindow()
{
	_StopPolling();
}


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		// Serial events
		case MSG_SERIAL_CONNECTED:
			SetConnected(true);
			break;

		case MSG_SERIAL_DISCONNECTED:
			SetConnected(false);
			break;

		case MSG_SERIAL_ERROR:
		{
			const char* error;
			if (message->FindString(kFieldError, &error) == B_OK) {
				BString message("Serial communication error:\n\n");
				message.Append(error);
				BAlert* alert = new BAlert("Serial Error", message.String(),
					"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
				alert->Go(NULL);  // Async, won't block
			}
			SetConnected(false);
			break;
		}

		// Protocol events
		case MSG_FRAME_RECEIVED:
			_HandleFrameReceived(message);
			break;

		case MSG_DEVICE_INFO_RECEIVED:
			_HandleDeviceInfo(message);
			break;

		case MSG_SELF_INFO_RECEIVED:
			_HandleSelfInfo(message);
			break;

		case MSG_CONTACTS_START:
			_HandleContactsStart(message);
			break;

		case MSG_CONTACT_RECEIVED:
			_HandleContactReceived(message);
			break;

		case MSG_CONTACTS_END:
			_HandleContactsEnd(message);
			break;

		case MSG_MESSAGE_RECEIVED:
			_HandleMessageReceived(message);
			break;

		case MSG_SEND_CONFIRMED:
			_HandleSendConfirmed(message);
			break;

		case MSG_BATTERY_RECEIVED:
			_HandleBatteryReceived(message);
			break;

		case MSG_PUSH_NOTIFICATION:
			_HandlePushNotification(message);
			break;

		// UI actions
		case MSG_SERIAL_CONNECT:
		case MSG_SHOW_PORT_SELECTION:
		{
			PortSelectionWindow* portWindow = new PortSelectionWindow(this);
			portWindow->Show();
			break;
		}

		case MSG_SERIAL_PORT_SELECTED:
		{
			const char* port;
			if (message->FindString(kFieldPort, &port) == B_OK) {
				Sestriere* app = dynamic_cast<Sestriere*>(be_app);
				if (app != NULL && app->GetSerialHandler() != NULL) {
					status_t status = app->GetSerialHandler()->Connect(port);
					if (status != B_OK) {
						BString errMsg("Failed to connect to ");
						errMsg.Append(port);
						errMsg.Append(":\n\n");
						errMsg.Append(strerror(status));
						BAlert* alert = new BAlert("Connection Error",
							errMsg.String(), "OK", NULL, NULL,
							B_WIDTH_AS_USUAL, B_STOP_ALERT);
						alert->Go(NULL);
					}
				}
			}
			break;
		}

		case MSG_SERIAL_DISCONNECT:
		{
			Sestriere* app = dynamic_cast<Sestriere*>(be_app);
			if (app != NULL && app->GetSerialHandler() != NULL)
				app->GetSerialHandler()->Disconnect();
			break;
		}

		case MSG_CONTACT_SELECTED:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK) {
				// Enable message input when a contact is selected
				if (index >= 0) {
					fSendingToChannel = false;
					fMessageInput->SetEnabled(true);
					fSendButton->SetEnabled(true);
					if (fPlaceholderLabel != NULL)
						fPlaceholderLabel->Hide();
					if (fChatView != NULL)
						fChatView->Show();

					// Update contact menu items
					Contact* contact = fContactList->ContactAt(index);
					bool isRepeaterOrRoom = (contact != NULL &&
						(contact->type == ADV_TYPE_REPEATER ||
						 contact->type == ADV_TYPE_ROOM));
					fLoginItem->SetEnabled(isRepeaterOrRoom);
					fTracePathItem->SetEnabled(contact != NULL);
					fExportContactItem->SetEnabled(contact != NULL);
				}
			}
			break;
		}

		case MSG_SEND_MESSAGE:
			_SendCurrentMessage();
			break;

		case MSG_REFRESH_CONTACTS:
		{
			Sestriere* app = dynamic_cast<Sestriere*>(be_app);
			if (app != NULL && app->GetSerialHandler() != NULL
				&& app->GetSerialHandler()->IsConnected()) {
				uint8 buffer[64];
				size_t len = Protocol::BuildGetContacts(0, buffer);
				app->GetSerialHandler()->SendFrame(buffer, len);
			}
			break;
		}

		case MSG_SEND_ADVERT:
		{
			Sestriere* app = dynamic_cast<Sestriere*>(be_app);
			if (app != NULL && app->GetSerialHandler() != NULL
				&& app->GetSerialHandler()->IsConnected()) {
				uint8 buffer[64];
				size_t len = Protocol::BuildSendAdvert(false, buffer);
				app->GetSerialHandler()->SendFrame(buffer, len);
			}
			break;
		}

		case MSG_SHOW_SETTINGS:
		{
			SettingsWindow* settingsWindow = new SettingsWindow(this);
			settingsWindow->SetCurrentSettings(fSelfInfo);
			settingsWindow->Show();
			break;
		}

		case MSG_SHOW_ABOUT:
			be_app->PostMessage(B_ABOUT_REQUESTED);
			break;

		case MSG_SHOW_LOGIN:
		{
			int32 selectedIndex = fContactList->CurrentSelection();
			if (selectedIndex >= 0) {
				Contact* contact = fContactList->ContactAt(selectedIndex);
				if (contact != NULL && (contact->type == ADV_TYPE_REPEATER ||
					contact->type == ADV_TYPE_ROOM)) {
					LoginWindow* loginWindow = new LoginWindow(this, contact);
					loginWindow->Show();
				}
			}
			break;
		}

		case MSG_SHOW_TRACE_PATH:
		{
			int32 selectedIndex = fContactList->CurrentSelection();
			if (selectedIndex >= 0) {
				Contact* contact = fContactList->ContactAt(selectedIndex);
				if (contact != NULL) {
					TracePathWindow* traceWindow = new TracePathWindow(this, contact);
					traceWindow->Show();
				}
			}
			break;
		}

		case MSG_EXPORT_CONTACT:
		{
			int32 selectedIndex = fContactList->CurrentSelection();
			if (selectedIndex >= 0) {
				Contact* contact = fContactList->ContactAt(selectedIndex);
				if (contact != NULL) {
					ContactExportWindow* exportWindow =
						new ContactExportWindow(this, true, contact);
					exportWindow->Show();
				}
			}
			break;
		}

		case MSG_IMPORT_CONTACT:
		{
			ContactExportWindow* importWindow =
				new ContactExportWindow(this, false, NULL);
			importWindow->Show();
			break;
		}

		case MSG_PUBLIC_CHANNEL:
		{
			// Switch to public channel mode
			fContactList->DeselectAll();
			fMessageInput->SetEnabled(fConnected);
			fSendButton->SetEnabled(fConnected);
			fChatView->SetCurrentContact(NULL);
			fStatusBar->SetTemporaryStatus("Public Channel selected");
			fSendingToChannel = true;
			break;
		}

		case MSG_SHOW_STATS:
		{
			StatsWindow* statsWindow = new StatsWindow(this);
			statsWindow->Show();
			break;
		}

		case MSG_INSTALL_DESKBAR:
		{
			BDeskbar deskbar;
			if (!deskbar.HasItem("Sestriere")) {
				entry_ref ref;
				if (be_roster->FindApp(kAppSignature, &ref) == B_OK) {
					deskbar.AddItem(&ref);
					fStatusBar->SetTemporaryStatus("Added to Deskbar");
				}
			}
			break;
		}

		case MSG_REMOVE_DESKBAR:
		{
			BDeskbar deskbar;
			if (deskbar.HasItem("Sestriere")) {
				deskbar.RemoveItem("Sestriere");
				fStatusBar->SetTemporaryStatus("Removed from Deskbar");
			}
			break;
		}

		case MSG_BATTERY_TIMER:
		{
			Sestriere* app = dynamic_cast<Sestriere*>(be_app);
			if (app != NULL && app->GetSerialHandler() != NULL
				&& app->GetSerialHandler()->IsConnected()) {
				uint8 buffer[64];
				size_t len = Protocol::BuildGetBatteryAndStorage(buffer);
				app->GetSerialHandler()->SendFrame(buffer, len);
			}
			break;
		}

		case MSG_SHOW_MAP:
		{
			// Create map window with contacts
			BRect mapFrame(100, 100, 700, 550);
			MapWindow* mapWindow = new MapWindow(mapFrame, BMessenger(this));

			// Add contacts with coordinates to the map
			for (int32 i = 0; i < fContactList->CountItems(); i++) {
				Contact* contact = fContactList->ContactAt(i);
				if (contact != NULL && (contact->advLat != 0 || contact->advLon != 0)) {
					// Use first 4 bytes of public key as ID
					uint32 nodeId = (contact->publicKey[0] << 24) |
						(contact->publicKey[1] << 16) |
						(contact->publicKey[2] << 8) |
						contact->publicKey[3];
					mapWindow->AddNode(nodeId, contact->advName,
						contact->advLat / 1000000.0, contact->advLon / 1000000.0,
						contact->type, contact->lastAdvert,
						contact->outPathLen >= 0 ? contact->outPathLen : 0);
				}
			}
			// Add self node if we have location
			if (fSelfInfo.advLat != 0 || fSelfInfo.advLon != 0) {
				mapWindow->AddNode(0, fSelfInfo.name,
					fSelfInfo.advLat / 1000000.0, fSelfInfo.advLon / 1000000.0,
					ADV_TYPE_CHAT, (uint32)time(NULL), 0);
				mapWindow->SetSelfNode(0);
			}

			mapWindow->Show();
			break;
		}

		case MSG_SHOW_MESH_GRAPH:
		{
			// Create mesh graph window
			BRect graphFrame(120, 120, 720, 570);
			MeshGraphWindow* graphWindow = new MeshGraphWindow(graphFrame, BMessenger(this));

			// Add self as center node
			graphWindow->AddNode(0, fSelfInfo.name, 0, true);

			// Add contacts as nodes
			for (int32 i = 0; i < fContactList->CountItems(); i++) {
				Contact* contact = fContactList->ContactAt(i);
				if (contact != NULL) {
					// Use first 4 bytes of public key as ID
					uint32 nodeId = (contact->publicKey[0] << 24) |
						(contact->publicKey[1] << 16) |
						(contact->publicKey[2] << 8) |
						contact->publicKey[3];
					int pathLen = contact->outPathLen >= 0 ? contact->outPathLen : 0;
					graphWindow->AddNode(nodeId, contact->advName, pathLen, false);
					// Add edge from self to contact
					graphWindow->AddEdge(0, nodeId, pathLen);
				}
			}

			graphWindow->Show();
			break;
		}

		case MSG_SHOW_TELEMETRY:
		{
			// Create telemetry window
			BRect telemetryFrame(140, 140, 840, 590);
			TelemetryWindow* telemetryWindow = new TelemetryWindow(telemetryFrame,
				BMessenger(this));

			// Add some sample sensor data (in real use, this would come from devices)
			// The window will update as telemetry data arrives from the mesh network

			telemetryWindow->Show();
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
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
MainWindow::SetConnected(bool connected)
{
	fConnected = connected;
	_UpdateConnectionUI();

	if (connected) {
		fHandshakeComplete = false;
		// Send DEVICE_QUERY to start handshake
		Sestriere* app = dynamic_cast<Sestriere*>(be_app);
		if (app != NULL && app->GetSerialHandler() != NULL) {
			uint8 buffer[64];
			size_t len = Protocol::BuildDeviceQuery(kAppProtocolVersion, buffer);
			app->GetSerialHandler()->SendFrame(buffer, len);
		}
	} else {
		fHandshakeComplete = false;
		_StopPolling();
		fStatusBar->SetConnectionStatus(TR_STATUS_DISCONNECTED);
	}
}


void
MainWindow::SetDeviceInfo(const DeviceInfo& info)
{
	fDeviceInfo = info;
	// After receiving device info, send APP_START
	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app != NULL && app->GetSerialHandler() != NULL) {
		uint8 buffer[64];
		size_t len = Protocol::BuildAppStart(kAppProtocolVersion,
			kAppIdentifier, buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);
	}
}


void
MainWindow::SetSelfInfo(const SelfInfo& info)
{
	fSelfInfo = info;
	fHandshakeComplete = true;
	fStatusBar->SetNodeName(info.name);
	fStatusBar->SetConnectionStatus(TR_STATUS_CONNECTED);

	char radioInfo[64];
	snprintf(radioInfo, sizeof(radioInfo), "SF%d %.3fMHz",
		info.radioSf, info.radioFreq / 1000000.0f);
	fStatusBar->SetRadioInfo(radioInfo);

	// Request contacts
	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app != NULL && app->GetSerialHandler() != NULL) {
		uint8 buffer[64];
		size_t len = Protocol::BuildGetContacts(0, buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);

		// Also request battery status
		len = Protocol::BuildGetBatteryAndStorage(buffer);
		app->GetSerialHandler()->SendFrame(buffer, len);
	}

	// Start battery polling timer
	_StartPolling();
}


void
MainWindow::SetBatteryInfo(const BatteryAndStorage& info)
{
	fStatusBar->SetBatteryInfo(info.milliVolts);
}


void
MainWindow::AddContact(const Contact& contact)
{
	fContactList->AddContact(contact);
	fReceivedContactCount++;
}


void
MainWindow::ClearContacts()
{
	fContactList->ClearContacts();
	fReceivedContactCount = 0;
}


void
MainWindow::UpdateContactCount(int32 count)
{
	fExpectedContactCount = count;
}


void
MainWindow::AddMessage(const ReceivedMessage& message, bool outgoing)
{
	fChatView->AddMessage(message, outgoing);
}


void
MainWindow::MessageSent(const char* text, uint32 timestamp)
{
	ReceivedMessage msg;
	memset(&msg, 0, sizeof(msg));
	strlcpy(msg.text, text, sizeof(msg.text));
	msg.senderTimestamp = timestamp;
	msg.txtType = TXT_TYPE_PLAIN;
	fChatView->AddMessage(msg, true);
}


void
MainWindow::MessageConfirmed(uint32 ackCode, uint32 roundTripMs)
{
	(void)ackCode;
	char status[64];
	snprintf(status, sizeof(status), "Delivered (%ums)", (unsigned)roundTripMs);
	fStatusBar->SetTemporaryStatus(status);
}


void
MainWindow::_BuildMenu()
{
	fMenuBar = new BMenuBar("menubar");

	// File menu
	BMenu* fileMenu = new BMenu(B_TRANSLATE(TR_MENU_FILE));
	fConnectItem = new BMenuItem(B_TRANSLATE(TR_MENU_CONNECT),
		new BMessage(MSG_SHOW_PORT_SELECTION), 'O');
	fileMenu->AddItem(fConnectItem);
	fDisconnectItem = new BMenuItem(B_TRANSLATE(TR_MENU_DISCONNECT),
		new BMessage(MSG_SERIAL_DISCONNECT), 'D');
	fileMenu->AddItem(fDisconnectItem);
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_SETTINGS),
		new BMessage(MSG_SHOW_SETTINGS), ','));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_QUIT),
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(fileMenu);

	// Contact menu
	BMenu* contactMenu = new BMenu(B_TRANSLATE(TR_MENU_CONTACT));
	fLoginItem = new BMenuItem(B_TRANSLATE(TR_MENU_LOGIN),
		new BMessage(MSG_SHOW_LOGIN), 'L');
	contactMenu->AddItem(fLoginItem);
	fTracePathItem = new BMenuItem(B_TRANSLATE(TR_MENU_TRACE_PATH),
		new BMessage(MSG_SHOW_TRACE_PATH), 'T');
	contactMenu->AddItem(fTracePathItem);
	contactMenu->AddSeparatorItem();
	fExportContactItem = new BMenuItem(B_TRANSLATE(TR_MENU_EXPORT_CONTACT),
		new BMessage(MSG_EXPORT_CONTACT), 'E');
	contactMenu->AddItem(fExportContactItem);
	contactMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_IMPORT_CONTACT),
		new BMessage(MSG_IMPORT_CONTACT), 'I', B_SHIFT_KEY));
	fMenuBar->AddItem(contactMenu);

	// Device menu
	BMenu* deviceMenu = new BMenu(B_TRANSLATE(TR_MENU_DEVICE));
	fRefreshContactsItem = new BMenuItem(B_TRANSLATE(TR_MENU_REFRESH_CONTACTS),
		new BMessage(MSG_REFRESH_CONTACTS), 'R');
	deviceMenu->AddItem(fRefreshContactsItem);
	fSendAdvertItem = new BMenuItem(B_TRANSLATE(TR_MENU_SEND_ADVERT),
		new BMessage(MSG_SEND_ADVERT), 'A');
	deviceMenu->AddItem(fSendAdvertItem);
	deviceMenu->AddSeparatorItem();
	deviceMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_PUBLIC_CHANNEL),
		new BMessage(MSG_PUBLIC_CHANNEL), 'P'));
	deviceMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_STATISTICS),
		new BMessage(MSG_SHOW_STATS), 'I'));
	deviceMenu->AddSeparatorItem();
	deviceMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_REBOOT_DEVICE),
		new BMessage(CMD_REBOOT)));
	fMenuBar->AddItem(deviceMenu);

	// View menu
	BMenu* viewMenu = new BMenu(B_TRANSLATE(TR_MENU_VIEW));
	viewMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_MAP_VIEW),
		new BMessage(MSG_SHOW_MAP), 'M'));
	viewMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_MESH_GRAPH),
		new BMessage(MSG_SHOW_MESH_GRAPH), 'G'));
	viewMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_TELEMETRY),
		new BMessage(MSG_SHOW_TELEMETRY), 'S'));
	fMenuBar->AddItem(viewMenu);

	// Help menu
	BMenu* helpMenu = new BMenu(B_TRANSLATE(TR_MENU_HELP));
	helpMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_ABOUT),
		new BMessage(MSG_SHOW_ABOUT)));
	helpMenu->AddSeparatorItem();
	helpMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_INSTALL_DESKBAR),
		new BMessage(MSG_INSTALL_DESKBAR)));
	helpMenu->AddItem(new BMenuItem(B_TRANSLATE(TR_MENU_REMOVE_DESKBAR),
		new BMessage(MSG_REMOVE_DESKBAR)));
	fMenuBar->AddItem(helpMenu);
}


void
MainWindow::_BuildLayout()
{
	// Status bar at top
	fStatusBar = new StatusBarView("statusbar");

	// Contact list on left
	fContactList = new ContactListView("contacts");
	BScrollView* contactScroll = new BScrollView("contact_scroll",
		fContactList, 0, false, true, B_PLAIN_BORDER);

	// Chat view on right
	fChatView = new ChatView("chat");
	fChatView->SetContactList(fContactList);
	BScrollView* chatScroll = new BScrollView("chat_scroll",
		fChatView, 0, false, true, B_PLAIN_BORDER);

	// Placeholder for when no contact selected
	fPlaceholderLabel = new BStringView("placeholder",
		B_TRANSLATE(TR_LABEL_NO_SELECTION));
	fPlaceholderLabel->SetAlignment(B_ALIGN_CENTER);

	// Message input area
	fMessageInput = new BTextControl("message_input", NULL, "",
		new BMessage(MSG_SEND_MESSAGE));
	fMessageInput->SetEnabled(false);

	fSendButton = new BButton("send_button", B_TRANSLATE(TR_BUTTON_SEND),
		new BMessage(MSG_SEND_MESSAGE));
	fSendButton->SetEnabled(false);
	fSendButton->MakeDefault(true);

	// Right side panel with chat and input
	BView* chatPanel = new BView("chat_panel", 0);
	BLayoutBuilder::Group<>(chatPanel, B_VERTICAL, 0)
		.Add(chatScroll, 1.0)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_SPACING)
			.Add(fMessageInput, 1.0)
			.Add(fSendButton)
		.End()
	.End();

	// Split view for contacts and chat
	fSplitView = new BSplitView(B_HORIZONTAL);
	fSplitView->AddChild(contactScroll, 0.25);
	fSplitView->AddChild(chatPanel, 0.75);
	fSplitView->SetCollapsible(0, false);
	fSplitView->SetCollapsible(1, false);

	// Main layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fStatusBar)
		.Add(fSplitView, 1.0)
	.End();
}


void
MainWindow::_UpdateConnectionUI()
{
	fConnectItem->SetEnabled(!fConnected);
	fDisconnectItem->SetEnabled(fConnected);
	fRefreshContactsItem->SetEnabled(fConnected);
	fSendAdvertItem->SetEnabled(fConnected);

	if (!fConnected) {
		fMessageInput->SetEnabled(false);
		fSendButton->SetEnabled(false);
	}
}


void
MainWindow::_SendCurrentMessage()
{
	const char* text = fMessageInput->Text();
	if (text == NULL || text[0] == '\0')
		return;

	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app == NULL || app->GetSerialHandler() == NULL)
		return;

	uint32 timestamp = (uint32)real_time_clock();
	uint8 buffer[256];
	size_t len;

	if (fSendingToChannel) {
		// Send to public channel
		len = Protocol::BuildSendChannelMessage(text, timestamp, buffer);
	} else {
		// Send to selected contact
		int32 selectedIndex = fContactList->CurrentSelection();
		if (selectedIndex < 0)
			return;

		Contact* contact = fContactList->ContactAt(selectedIndex);
		if (contact == NULL)
			return;

		len = Protocol::BuildSendTextMessage(
			contact->publicKey, text, timestamp, buffer);
	}

	if (app->GetSerialHandler()->SendFrame(buffer, len) == B_OK) {
		MessageSent(text, timestamp);
		fMessageInput->SetText("");
	}
}


void
MainWindow::_StartPolling()
{
	if (fBatteryTimer == NULL) {
		BMessage timerMsg(MSG_BATTERY_TIMER);
		fBatteryTimer = new BMessageRunner(this, &timerMsg,
			kBatteryPollInterval);
	}
}


void
MainWindow::_StopPolling()
{
	delete fBatteryTimer;
	fBatteryTimer = NULL;
}


void
MainWindow::_HandleFrameReceived(BMessage* message)
{
	const void* data;
	ssize_t size;
	if (message->FindData(kFieldData, B_RAW_TYPE, &data, &size) != B_OK)
		return;

	if (size < 1)
		return;

	const uint8* payload = static_cast<const uint8*>(data);
	uint8 code = payload[0];

	// Handle response codes
	if (code < 0x80) {
		switch (code) {
			case RESP_CODE_DEVICE_INFO:
			{
				DeviceInfo info;
				if (Protocol::ParseDeviceInfo(payload, size, info))
					SetDeviceInfo(info);
				break;
			}

			case RESP_CODE_SELF_INFO:
			{
				SelfInfo info;
				if (Protocol::ParseSelfInfo(payload, size, info))
					SetSelfInfo(info);
				break;
			}

			case RESP_CODE_CONTACTS_START:
			{
				if (size >= 2) {
					ClearContacts();
					UpdateContactCount(payload[1]);
				}
				break;
			}

			case RESP_CODE_CONTACT:
			{
				Contact contact;
				if (Protocol::ParseContact(payload, size, contact))
					AddContact(contact);
				break;
			}

			case RESP_CODE_END_OF_CONTACTS:
				// Contacts sync complete, now sync messages
				{
					Sestriere* app = dynamic_cast<Sestriere*>(be_app);
					if (app != NULL && app->GetSerialHandler() != NULL) {
						uint8 buffer[64];
						size_t len = Protocol::BuildSyncNextMessage(buffer);
						app->GetSerialHandler()->SendFrame(buffer, len);
					}
				}
				break;

			case RESP_CODE_CONTACT_MSG_RECV:
			case RESP_CODE_CONTACT_MSG_RECV_V3:
			{
				ReceivedMessage msg;
				if (Protocol::ParseReceivedMessage(payload, size, msg)) {
					msg.isChannel = false;
					AddMessage(msg, false);
					// Request next message
					Sestriere* app = dynamic_cast<Sestriere*>(be_app);
					if (app != NULL && app->GetSerialHandler() != NULL) {
						uint8 buffer[64];
						size_t len = Protocol::BuildSyncNextMessage(buffer);
						app->GetSerialHandler()->SendFrame(buffer, len);
					}
				}
				break;
			}

			case RESP_CODE_CHANNEL_MSG_RECV:
			case RESP_CODE_CHANNEL_MSG_RECV_V3:
			{
				ReceivedMessage msg;
				if (Protocol::ParseReceivedMessage(payload, size, msg)) {
					msg.isChannel = true;
					AddMessage(msg, false);
					// Request next message
					Sestriere* app = dynamic_cast<Sestriere*>(be_app);
					if (app != NULL && app->GetSerialHandler() != NULL) {
						uint8 buffer[64];
						size_t len = Protocol::BuildSyncNextMessage(buffer);
						app->GetSerialHandler()->SendFrame(buffer, len);
					}
				}
				break;
			}

			case RESP_CODE_NO_MORE_MESSAGES:
				// Message sync complete
				break;

			case RESP_CODE_BATT_AND_STORAGE:
			{
				BatteryAndStorage info;
				if (Protocol::ParseBatteryAndStorage(payload, size, info))
					SetBatteryInfo(info);
				break;
			}

			case RESP_CODE_SENT:
				// Message sent, waiting for ACK
				break;

			case RESP_CODE_OK:
				// Generic OK
				break;

			case RESP_CODE_ERR:
				if (size >= 2) {
					BString errMsg("Device error:\n\n");
					errMsg.Append(Protocol::GetErrorName(payload[1]));
					BAlert* alert = new BAlert("Device Error",
						errMsg.String(), "OK", NULL, NULL,
						B_WIDTH_AS_USUAL, B_WARNING_ALERT);
					alert->Go(NULL);
				}
				break;
		}
	} else {
		// Handle push notifications
		switch (code) {
			case PUSH_CODE_MSG_WAITING:
			{
				// New message available, fetch it
				Sestriere* app = dynamic_cast<Sestriere*>(be_app);
				if (app != NULL && app->GetSerialHandler() != NULL) {
					uint8 buffer[64];
					size_t len = Protocol::BuildSyncNextMessage(buffer);
					app->GetSerialHandler()->SendFrame(buffer, len);
				}
				break;
			}

			case PUSH_CODE_SEND_CONFIRMED:
			{
				SendConfirmed confirm;
				if (Protocol::ParseSendConfirmed(payload, size, confirm))
					MessageConfirmed(confirm.ackCode, confirm.roundTripMs);
				break;
			}

			case PUSH_CODE_ADVERT:
			case PUSH_CODE_NEW_ADVERT:
				// New advertisement received, refresh contacts
				PostMessage(MSG_REFRESH_CONTACTS);
				break;

			case PUSH_CODE_PATH_UPDATED:
				// Path updated, could refresh contacts
				break;

			case PUSH_CODE_TELEMETRY_RESPONSE:
			{
				// Telemetry data received
				// Format: [0]=code, [1-6]=pubkey_prefix, [7]=sensor_type,
				//         [8-11]=value(float LE), [12+]=sensor_name
				if (size >= 12) {
					uint8 sensorType = payload[7];
					float value;
					memcpy(&value, payload + 8, sizeof(float));

					BString sensorName;
					if (size > 12) {
						sensorName.SetTo((const char*)(payload + 12), size - 12);
					} else {
						sensorName = "Unknown";
					}

					// Get node ID from pub key prefix
					uint32 nodeId = Protocol::ReadU32LE(payload + 1);

					// Determine sensor type and unit
					SensorType type = (SensorType)sensorType;
					BString unit;
					switch (type) {
						case SENSOR_TEMPERATURE: unit = "°C"; break;
						case SENSOR_HUMIDITY: unit = "%"; break;
						case SENSOR_PRESSURE: unit = "hPa"; break;
						case SENSOR_BATTERY: unit = "V"; break;
						case SENSOR_ALTITUDE: unit = "m"; break;
						default: unit = ""; break;
					}

					// Forward to telemetry windows
					BMessage telemetryMsg(MSG_TELEMETRY_DATA);
					telemetryMsg.AddInt32("nodeId", nodeId);
					telemetryMsg.AddString("sensorName", sensorName);
					telemetryMsg.AddInt32("sensorType", type);
					telemetryMsg.AddFloat("value", value);
					telemetryMsg.AddString("unit", unit);
					be_app->PostMessage(&telemetryMsg);
				}
				break;
			}

			default:
				fprintf(stderr, "Unhandled push code: 0x%02X\n", code);
				break;
		}
	}
}


void
MainWindow::_HandleDeviceInfo(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleSelfInfo(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleContactsStart(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleContactReceived(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleContactsEnd(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleMessageReceived(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleSendConfirmed(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandleBatteryReceived(BMessage* message)
{
	(void)message;
}


void
MainWindow::_HandlePushNotification(BMessage* message)
{
	(void)message;
}
