/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DebugLogWindow.h — Separate window for debug log
 */

#ifndef DEBUGLOGWINDOW_H
#define DEBUGLOGWINDOW_H

#include <Window.h>

class BButton;
class BTextView;

class DebugLogWindow : public BWindow {
public:
							DebugLogWindow(BRect frame);
	virtual					~DebugLogWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			LogMessage(const char* prefix, const char* text);
			void			LogHex(const char* prefix, const uint8* data,
								size_t length);
			void			Clear();

	static	DebugLogWindow*	Instance();
	static	void			ShowWindow();
	static	void			Destroy();

private:
			BString			_FormatHex(const uint8* data, size_t length);
			const char*		_CommandName(uint8 cmd);

			BTextView*		fLogView;
			BButton*		fClearButton;

	static	DebugLogWindow*	sInstance;
};

#endif // DEBUGLOGWINDOW_H
