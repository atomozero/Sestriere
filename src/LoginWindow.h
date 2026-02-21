/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * LoginWindow.h — Repeater/Room login dialog
 */

#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <Window.h>

#include "Types.h"

class BButton;
class BMessageRunner;
class BStringView;
class BTextControl;


class LoginWindow : public BWindow {
public:
							LoginWindow(BWindow* parent,
								const ContactInfo* contact);
	virtual					~LoginWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			SetLoginResult(bool success,
								const char* message = NULL);

private:
			void			_OnLogin();

			BWindow*		fParent;
			uint8			fPublicKey[32];
			char			fContactName[64];

			BStringView*	fTargetLabel;
			BTextControl*	fPasswordControl;
			BButton*		fLoginButton;
			BButton*		fCancelButton;
			BStringView*	fStatusLabel;

			bool			fLoggingIn;
			BMessageRunner*	fTimeoutRunner;
			BMessageRunner*	fCloseRunner;
};

#endif // LOGINWINDOW_H
