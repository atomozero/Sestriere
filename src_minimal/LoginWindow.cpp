/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * LoginWindow.cpp — Repeater/Room login dialog implementation
 */

#include "LoginWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstring>

#include "Constants.h"


static const uint32 kMsgDoLogin		= 'dlog';
static const uint32 kMsgLoginTimeout	= 'ltmo';

// Message sent to parent to execute the login command
static const uint32 kMsgSendLogin = 'slgn';


LoginWindow::LoginWindow(BWindow* parent, const ContactInfo* contact)
	:
	BWindow(BRect(0, 0, 300, 150), "Login",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS |
		B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fTargetLabel(NULL),
	fPasswordControl(NULL),
	fLoginButton(NULL),
	fCancelButton(NULL),
	fStatusLabel(NULL),
	fLoggingIn(false),
	fTimeoutRunner(NULL)
{
	memset(fPublicKey, 0, sizeof(fPublicKey));
	memset(fContactName, 0, sizeof(fContactName));

	if (contact != NULL) {
		memcpy(fPublicKey, contact->publicKey, 32);
		strlcpy(fContactName, contact->name, sizeof(fContactName));
	}

	// Contact type description
	const char* typeStr = "Node";
	if (contact != NULL) {
		switch (contact->type) {
			case 1: typeStr = "Companion"; break;
			case 2: typeStr = "Repeater"; break;
			case 3: typeStr = "Room"; break;
			default: typeStr = "Node"; break;
		}
	}

	BString title;
	title.SetToFormat("Login to %s", typeStr);
	SetTitle(title.String());

	BString targetStr;
	targetStr.SetToFormat("Target: %s (%s)", fContactName, typeStr);
	fTargetLabel = new BStringView("target_label", targetStr.String());

	fPasswordControl = new BTextControl("password", "Password:", "",
		new BMessage(kMsgDoLogin));
	fPasswordControl->TextView()->HideTyping(true);

	fLoginButton = new BButton("login_button", "Login",
		new BMessage(kMsgDoLogin));
	fLoginButton->MakeDefault(true);

	fCancelButton = new BButton("cancel_button", "Cancel",
		new BMessage(B_QUIT_REQUESTED));

	fStatusLabel = new BStringView("status_label", "");
	fStatusLabel->SetHighColor(100, 100, 100);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fTargetLabel)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fPasswordControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fPasswordControl->CreateTextViewLayoutItem(), 1, 0)
		.End()
		.Add(fStatusLabel)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fLoginButton)
		.End()
	.End();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	fPasswordControl->MakeFocus(true);
}


LoginWindow::~LoginWindow()
{
	delete fTimeoutRunner;
}


bool
LoginWindow::QuitRequested()
{
	Hide();
	return false;
}


void
LoginWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgDoLogin:
			_OnLogin();
			break;

		case kMsgLoginTimeout:
			if (fLoggingIn) {
				SetLoginResult(false, "No response from device (timeout).");
			}
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
LoginWindow::SetLoginResult(bool success, const char* message)
{
	fLoggingIn = false;
	fLoginButton->SetEnabled(true);
	fPasswordControl->SetEnabled(true);

	// Cancel timeout timer
	delete fTimeoutRunner;
	fTimeoutRunner = NULL;

	if (success) {
		fStatusLabel->SetHighColor(0, 150, 0);
		fStatusLabel->SetText(message != NULL ? message : "Success!");

		snooze(500000);
		PostMessage(B_QUIT_REQUESTED);
	} else {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText(message != NULL ? message : "Login failed.");
	}
}


void
LoginWindow::_OnLogin()
{
	if (fLoggingIn)
		return;

	const char* password = fPasswordControl->Text();
	if (password == NULL || password[0] == '\0') {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Please enter a password.");
		return;
	}

	fLoggingIn = true;
	fLoginButton->SetEnabled(false);
	fPasswordControl->SetEnabled(false);
	fStatusLabel->SetHighColor(100, 100, 100);
	fStatusLabel->SetText("Logging in...");

	// Send login request to parent window
	if (fParent != NULL) {
		BMessage loginMsg(kMsgSendLogin);
		loginMsg.AddData("pubkey", B_RAW_TYPE, fPublicKey, 32);
		loginMsg.AddString("password", password);
		fParent->PostMessage(&loginMsg);
	}

	// Start 10-second timeout
	delete fTimeoutRunner;
	BMessage timeoutMsg(kMsgLoginTimeout);
	fTimeoutRunner = new BMessageRunner(BMessenger(this), &timeoutMsg,
		10000000, 1);
}
