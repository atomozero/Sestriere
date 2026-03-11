/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepMonWindow.cpp — Main window for standalone Repeater Monitor
 */

#include "RepMonWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <Path.h>
#include <String.h>

#include <cstdio>
#include <cstring>

#include "RepMonConstants.h"
#include "RepeaterMonitorView.h"
#include "SerialHandler.h"
#include "SerialMonitorWindow.h"


static const uint32 kMsgPortSelected = 'psel';
static const uint32 kMsgLoadLog = 'llog';
static const uint32 kMsgLoadLogRef = 'llrf';
static const uint32 kMsgRefreshPorts = 'rfpt';


RepMonWindow::RepMonWindow()
	:
	BWindow(BRect(50, 50, 950, 650),
		"Repeater Monitor",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fMenuBar(NULL),
	fPortMenu(NULL),
	fDisconnectItem(NULL),
	fSerialHandler(NULL),
	fMonitorView(NULL),
	fSerialMonitorWindow(NULL),
	fOpenPanel(NULL),
	fConnected(false)
{
	// Create serial handler
	fSerialHandler = new SerialHandler(this);
	fSerialHandler->Run();
	fSerialHandler->SetRawMode(true);

	// Create monitor view
	fMonitorView = new RepeaterMonitorView(this);

	// Build menu bar
	fMenuBar = new BMenuBar("menubar");
	_BuildMenuBar();

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fMonitorView, 1.0)
	.End();

	CenterOnScreen();
}


RepMonWindow::~RepMonWindow()
{
	delete fOpenPanel;

	if (fSerialHandler != NULL) {
		fSerialHandler->Lock();
		fSerialHandler->Quit();
	}
}


void
RepMonWindow::_BuildMenuBar()
{
	// Connection menu
	BMenu* connMenu = new BMenu("Connection");

	fPortMenu = new BMenu("Connect");
	_RefreshPortMenu();
	connMenu->AddItem(fPortMenu);

	fDisconnectItem = new BMenuItem("Disconnect",
		new BMessage(MSG_SERIAL_DISCONNECT));
	fDisconnectItem->SetEnabled(false);
	connMenu->AddItem(fDisconnectItem);

	connMenu->AddSeparatorItem();

	connMenu->AddItem(new BMenuItem("Refresh Ports",
		new BMessage(kMsgRefreshPorts)));

	connMenu->AddSeparatorItem();

	connMenu->AddItem(new BMenuItem("Load Log File" B_UTF8_ELLIPSIS,
		new BMessage(kMsgLoadLog), 'O'));

	connMenu->AddSeparatorItem();

	connMenu->AddItem(new BMenuItem("Quit",
		new BMessage(B_QUIT_REQUESTED), 'Q'));

	connMenu->SetTargetForItems(this);
	fPortMenu->SetTargetForItems(this);
	fMenuBar->AddItem(connMenu);

	// View menu
	BMenu* viewMenu = new BMenu("View");

	viewMenu->AddItem(new BMenuItem("Serial Monitor",
		new BMessage(MSG_SHOW_SERIAL_MONITOR), 'S', B_SHIFT_KEY));

	viewMenu->SetTargetForItems(this);
	fMenuBar->AddItem(viewMenu);
}


void
RepMonWindow::_RefreshPortMenu()
{
	// Clear existing items
	while (fPortMenu->CountItems() > 0)
		delete fPortMenu->RemoveItem((int32)0);

	// List available ports
	BMessage ports;
	SerialHandler::ListPorts(&ports);

	const char* port;
	int32 count = 0;
	for (int32 i = 0;
		ports.FindString(kFieldPort, i, &port) == B_OK; i++) {
		BMessage* msg = new BMessage(kMsgPortSelected);
		msg->AddString(kFieldPort, port);
		fPortMenu->AddItem(new BMenuItem(port, msg));
		count++;
	}

	if (count == 0) {
		BMenuItem* noPort = new BMenuItem("(no ports found)", NULL);
		noPort->SetEnabled(false);
		fPortMenu->AddItem(noPort);
	}

	fPortMenu->SetTargetForItems(this);
}


void
RepMonWindow::_Connect(const char* port)
{
	if (fConnected)
		_Disconnect();

	BMessage msg(MSG_SERIAL_CONNECT);
	msg.AddString(kFieldPort, port);
	fSerialHandler->PostMessage(&msg);
}


void
RepMonWindow::_Disconnect()
{
	fSerialHandler->PostMessage(MSG_SERIAL_DISCONNECT);
}


void
RepMonWindow::_LoadLogFile(entry_ref* ref)
{
	BPath path(ref);
	if (path.InitCheck() != B_OK)
		return;

	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK) {
		BAlert* alert = new BAlert("Error",
			"Could not open log file.", "OK",
			NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go();
		return;
	}

	// Clear existing data
	fMonitorView->Clear();

	// Read and process line by line
	off_t fileSize;
	file.GetSize(&fileSize);

	char* buffer = new char[fileSize + 1];
	file.Read(buffer, fileSize);
	buffer[fileSize] = '\0';

	char* line = strtok(buffer, "\n");
	while (line != NULL) {
		// Strip \r
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\r')
			line[len - 1] = '\0';

		if (line[0] != '\0')
			fMonitorView->ProcessLine(line);

		line = strtok(NULL, "\n");
	}

	delete[] buffer;

	// Update window title
	BString title("Repeater Monitor \xe2\x80\x94 ");
	title << path.Leaf();
	SetTitle(title.String());
}


void
RepMonWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgPortSelected:
		{
			const char* port;
			if (message->FindString(kFieldPort, &port) == B_OK)
				_Connect(port);
			break;
		}

		case MSG_SERIAL_DISCONNECT:
			_Disconnect();
			break;

		case MSG_SERIAL_CONNECTED:
		{
			fConnected = true;
			fDisconnectItem->SetEnabled(true);

			const char* port;
			if (message->FindString(kFieldPort, &port) == B_OK) {
				BString title("Repeater Monitor \xe2\x80\x94 ");
				title << port;
				SetTitle(title.String());
			}

			// Auto-request firmware version and log
			BMessage verMsg(MSG_SERIAL_SEND_RAW);
			verMsg.AddString("text", "ver");
			fSerialHandler->PostMessage(&verMsg);

			BMessage logMsg(MSG_SERIAL_SEND_RAW);
			logMsg.AddString("text", "log");
			fSerialHandler->PostMessage(&logMsg);
			break;
		}

		case MSG_SERIAL_DISCONNECTED:
		{
			fConnected = false;
			fDisconnectItem->SetEnabled(false);
			SetTitle("Repeater Monitor");
			break;
		}

		case MSG_SERIAL_ERROR:
		{
			const char* error;
			if (message->FindString(kFieldError, &error) == B_OK) {
				BAlert* alert = new BAlert("Serial Error",
					error, "OK",
					NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
				alert->Go();
			}
			break;
		}

		case MSG_RAW_SERIAL_DATA:
		{
			const char* line;
			if (message->FindString("line", &line) == B_OK) {
				// Forward to monitor view
				fMonitorView->ProcessLine(line);

				// Forward to serial monitor window
				if (fSerialMonitorWindow != NULL
					&& fSerialMonitorWindow->LockLooper()) {
					fSerialMonitorWindow->AppendOutput(line);
					fSerialMonitorWindow->UnlockLooper();
				}
			}
			break;
		}

		case MSG_SERIAL_SEND_RAW:
		{
			// Forward from SerialMonitorWindow or RepeaterMonitorView
			// to serial handler
			fSerialHandler->PostMessage(message);
			break;
		}

		case MSG_SHOW_SERIAL_MONITOR:
		{
			if (fSerialMonitorWindow == NULL) {
				fSerialMonitorWindow = new SerialMonitorWindow(this);
				fSerialMonitorWindow->Show();
			} else {
				if (fSerialMonitorWindow->LockLooper()) {
					if (fSerialMonitorWindow->IsHidden())
						fSerialMonitorWindow->Show();
					fSerialMonitorWindow->Activate();
					fSerialMonitorWindow->UnlockLooper();
				}
			}
			break;
		}

		case MSG_REPEATER_PACKET_FLOW:
			// Standalone app does not have NetworkMapWindow — ignore
			break;

		case MSG_FRAME_RECEIVED:
		case MSG_FRAME_SENT:
			// Standalone app uses raw mode only — ignore frame messages
			break;

		case kMsgLoadLog:
		{
			if (fOpenPanel == NULL) {
				BMessage* openMsg = new BMessage(kMsgLoadLogRef);
				fOpenPanel = new BFilePanel(B_OPEN_PANEL,
					new BMessenger(this), NULL,
					B_FILE_NODE, false, openMsg);
			}
			fOpenPanel->Show();
			break;
		}

		case kMsgLoadLogRef:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				_LoadLogFile(&ref);
			break;
		}

		case kMsgRefreshPorts:
			_RefreshPortMenu();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
RepMonWindow::QuitRequested()
{
	if (fSerialMonitorWindow != NULL) {
		fSerialMonitorWindow->Lock();
		fSerialMonitorWindow->Quit();
		fSerialMonitorWindow = NULL;
	}

	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
