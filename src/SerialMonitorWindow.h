/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SerialMonitorWindow.h — Terminal-style serial monitor for repeater CLI
 */

#ifndef SERIALMONITORWINDOW_H
#define SERIALMONITORWINDOW_H

#include <Window.h>

class BButton;
class BFilePanel;
class BHandler;
class BScrollView;
class BTextControl;
class BTextView;


class SerialMonitorWindow : public BWindow {
public:
						SerialMonitorWindow(BHandler* target);
	virtual				~SerialMonitorWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

			void		AppendOutput(const char* text);

private:
			void		_SendCommand();
			void		_SaveLog();
			void		_PruneOutput();

			BTextView*		fOutputView;
			BScrollView*	fScrollView;
			BTextControl*	fInputField;
			BButton*		fSendButton;
			BButton*		fSaveButton;
			BButton*		fClearButton;
			BFilePanel*		fSavePanel;
			BHandler*		fTarget;

	static const int32	kMaxOutputSize = 512 * 1024;  // 512 KB
};

#endif // SERIALMONITORWINDOW_H
