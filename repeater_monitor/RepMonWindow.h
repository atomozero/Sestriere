/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepMonWindow.h — Main window for standalone Repeater Monitor
 */

#ifndef REPMONWINDOW_H
#define REPMONWINDOW_H

#include <Window.h>

class BFilePanel;
class BMenu;
class BMenuBar;
class BMenuItem;
class RepeaterMonitorView;
class SerialHandler;
class SerialMonitorWindow;


class RepMonWindow : public BWindow {
public:
						RepMonWindow();
	virtual				~RepMonWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

private:
		void			_BuildMenuBar();
		void			_RefreshPortMenu();
		void			_Connect(const char* port);
		void			_Disconnect();
		void			_LoadLogFile(entry_ref* ref);

		BMenuBar*				fMenuBar;
		BMenu*					fPortMenu;
		BMenuItem*				fDisconnectItem;

		SerialHandler*			fSerialHandler;
		RepeaterMonitorView*	fMonitorView;
		SerialMonitorWindow*	fSerialMonitorWindow;

		BFilePanel*				fOpenPanel;
		bool					fConnected;
};

#endif // REPMONWINDOW_H
