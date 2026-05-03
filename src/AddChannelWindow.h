/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * AddChannelWindow.h — Dialog for adding/joining a channel
 */

#ifndef _ADDCHANNELWINDOW_H
#define _ADDCHANNELWINDOW_H

#include <Window.h>

class BButton;
class BRadioButton;
class BTextControl;


class AddChannelWindow : public BWindow {
public:
							AddChannelWindow(BWindow* parent);
	virtual					~AddChannelWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

private:
			void			_OnAdd();
			void			_UpdatePskField();

			BWindow*		fParent;
			BTextControl*	fNameControl;
			BTextControl*	fPskControl;
			BRadioButton*	fModeCreate;
			BRadioButton*	fModeJoin;
			BRadioButton*	fModeHashtag;
			BButton*		fAddButton;
			BButton*		fCancelButton;
};

#endif // _ADDCHANNELWINDOW_H
