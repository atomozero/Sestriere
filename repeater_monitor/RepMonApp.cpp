/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepMonApp.cpp — Standalone Repeater Monitor application entry point
 */

#include <Application.h>

#include "RepMonWindow.h"


class RepMonApp : public BApplication {
public:
	RepMonApp()
		:
		BApplication("application/x-vnd.Sestriere-RepMon")
	{
		RepMonWindow* window = new RepMonWindow();
		window->Show();
	}
};


int
main()
{
	RepMonApp app;
	app.Run();
	return 0;
}
