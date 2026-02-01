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
class BTextControl;
class BStringView;

class LoginWindow : public BWindow {
public:
							LoginWindow(BWindow* parent, const Contact* contact);
	virtual					~LoginWindow();

	virtual void			MessageReceived(BMessage* message);

			void			SetLoginResult(bool success, const char* message = NULL);

private:
			void			_OnLogin();

			BWindow*		fParent;
			Contact			fContact;

			BStringView*	fTargetLabel;
			BTextControl*	fPasswordControl;
			BButton*		fLoginButton;
			BButton*		fCancelButton;
			BStringView*	fStatusLabel;

			bool			fLoggingIn;
};

#endif // LOGINWINDOW_H
