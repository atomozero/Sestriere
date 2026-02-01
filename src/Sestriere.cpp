/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Sestriere.cpp — Main application class implementation
 */

#include "Sestriere.h"

#include <AboutWindow.h>
#include <Alert.h>
#include <Catalog.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Screen.h>

#include "Constants.h"
#include "MainWindow.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Sestriere"


Sestriere::Sestriere()
	:
	BApplication(kAppSignature),
	fMainWindow(NULL),
	fSerialHandler(NULL),
	fLastPort(""),
	fWindowFrame(100, 100, 100 + kDefaultWindowWidth, 100 + kDefaultWindowHeight)
{
	_LoadSettings();
}


Sestriere::~Sestriere()
{
	_SaveSettings();
}


void
Sestriere::ReadyToRun()
{
	// Create the serial handler looper
	fSerialHandler = new SerialHandler(NULL);
	fSerialHandler->Run();

	// Create the main window
	fMainWindow = new MainWindow(fWindowFrame);
	fMainWindow->Show();

	// Set the serial handler's target to the main window
	fSerialHandler->SetTarget(fMainWindow);

	// If we have a last used port, try to connect automatically
	// (disabled for now - let user choose)
}


void
Sestriere::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_SHOW_ABOUT:
			AboutRequested();
			break;

		default:
			BApplication::MessageReceived(message);
			break;
	}
}


bool
Sestriere::QuitRequested()
{
	// Disconnect serial if connected
	if (fSerialHandler != NULL && fSerialHandler->IsConnected())
		fSerialHandler->Disconnect();

	// Save window position
	if (fMainWindow != NULL && fMainWindow->Lock()) {
		fWindowFrame = fMainWindow->Frame();
		fMainWindow->Unlock();
	}

	_SaveSettings();

	return true;
}


void
Sestriere::AboutRequested()
{
	const char* authors[] = {
		"Sestriere Authors",
		NULL
	};

	BAboutWindow* aboutWindow = new BAboutWindow(
		B_TRANSLATE_SYSTEM_NAME(kAppName),
		kAppSignature);

	aboutWindow->AddDescription(
		"A native MeshCore LoRa mesh client for Haiku OS.\n\n"
		"The name recalls the Venetian 'sestieri' - interconnected "
		"districts like nodes in a mesh network.");
	aboutWindow->AddCopyright(2025, "Sestriere Authors");
	aboutWindow->AddAuthors(authors);
	aboutWindow->Show();
}


void
Sestriere::_LoadSettings()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return;

	path.Append(kSettingsFileName);

	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	BMessage settings;
	if (settings.Unflatten(&file) != B_OK)
		return;

	// Load last used port
	const char* lastPort;
	if (settings.FindString(kSettingsFieldLastPort, &lastPort) == B_OK)
		fLastPort = lastPort;

	// Load window frame
	BRect frame;
	if (settings.FindRect(kSettingsFieldWindowFrame, &frame) == B_OK) {
		// Validate frame is on screen
		BScreen screen;
		BRect screenFrame = screen.Frame();
		if (screenFrame.Contains(frame.LeftTop()))
			fWindowFrame = frame;
	}
}


void
Sestriere::_SaveSettings()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return;

	path.Append(kSettingsFileName);

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	BMessage settings;
	settings.AddString(kSettingsFieldLastPort, fLastPort.String());
	settings.AddRect(kSettingsFieldWindowFrame, fWindowFrame);

	settings.Flatten(&file);
}


// =============================================================================
// main()
// =============================================================================

int
main()
{
	Sestriere app;
	app.Run();
	return 0;
}
