/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Sestriere.cpp — Main application entry point
 */

#include <Application.h>
#include <Window.h>

#include <cstdio>

#include "Constants.h"
#include "MainWindow.h"


class SestriereApp : public BApplication {
public:
							SestriereApp();
	virtual					~SestriereApp();

	virtual void			ReadyToRun();
	virtual bool			QuitRequested();

private:
			MainWindow*		fMainWindow;
};


SestriereApp::SestriereApp()
	:
	BApplication(APP_SIGNATURE),
	fMainWindow(NULL)
{
}


SestriereApp::~SestriereApp()
{
}


void
SestriereApp::ReadyToRun()
{
	fprintf(stderr, "[App] ReadyToRun: creating MainWindow...\n");
	fMainWindow = new MainWindow();
	fprintf(stderr, "[App] ReadyToRun: MainWindow created, calling Show()...\n");
	fMainWindow->Show();
	fprintf(stderr, "[App] ReadyToRun: Show() called, returning\n");
}


bool
SestriereApp::QuitRequested()
{
	// MainWindow handles cleanup in its QuitRequested
	// Just let the default behavior close everything
	return BApplication::QuitRequested();
}


int
main()
{
	fprintf(stderr, "[main] Starting SestriereApp...\n");
	SestriereApp app;
	fprintf(stderr, "[main] App created, calling Run()...\n");
	app.Run();
	fprintf(stderr, "[main] App.Run() returned, exiting\n");
	return 0;
}
