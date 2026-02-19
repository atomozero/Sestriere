/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * AddChannelWindow.h — Dialog for adding a private channel
 */

#ifndef ADDCHANNELWINDOW_H
#define ADDCHANNELWINDOW_H

#include <Window.h>

class BButton;
class BTextControl;


class AddChannelWindow : public BWindow {
public:
							AddChannelWindow(BWindow* parent);
	virtual					~AddChannelWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

private:
			void			_OnAdd();

			BWindow*		fParent;
			BTextControl*	fNameControl;
			BButton*		fAddButton;
			BButton*		fCancelButton;
};

#endif // ADDCHANNELWINDOW_H
