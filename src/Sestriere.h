/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Sestriere.h — Main application class
 */

#ifndef SESTRIERE_H
#define SESTRIERE_H

#include <Application.h>
#include <Message.h>

class MainWindow;
class SerialHandler;

class Sestriere : public BApplication {
public:
							Sestriere();
	virtual					~Sestriere();

	virtual void			ReadyToRun();
	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();
	virtual void			AboutRequested();

			MainWindow*		GetMainWindow() const { return fMainWindow; }
			SerialHandler*	GetSerialHandler() const { return fSerialHandler; }

private:
			void			_LoadSettings();
			void			_SaveSettings();

			MainWindow*		fMainWindow;
			SerialHandler*	fSerialHandler;
			BString			fLastPort;
			BRect			fWindowFrame;
};

#endif // SESTRIERE_H
