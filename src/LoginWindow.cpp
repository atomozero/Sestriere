/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * LoginWindow.cpp — Repeater/Room login dialog implementation
 */

#include "LoginWindow.h"

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstring>

#include "Constants.h"
#include "Protocol.h"
#include "Sestriere.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "LoginWindow"


static const uint32 MSG_DO_LOGIN = 'dlog';


LoginWindow::LoginWindow(BWindow* parent, const Contact* contact)
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
	fLoggingIn(false)
{
	if (contact != NULL)
		memcpy(&fContact, contact, sizeof(Contact));
	else
		memset(&fContact, 0, sizeof(Contact));

	// Title based on contact type
	const char* typeStr = Protocol::GetAdvTypeName(fContact.type);
	BString title;
	title.SetToFormat("Login to %s", typeStr);
	SetTitle(title.String());

	// Create views
	BString targetStr;
	targetStr.SetToFormat("Target: %s (%s)", fContact.advName, typeStr);
	fTargetLabel = new BStringView("target_label", targetStr.String());

	fPasswordControl = new BTextControl("password", "Password:", "",
		new BMessage(MSG_DO_LOGIN));
	fPasswordControl->TextView()->HideTyping(true);

	fLoginButton = new BButton("login_button", "Login",
		new BMessage(MSG_DO_LOGIN));
	fLoginButton->MakeDefault(true);

	fCancelButton = new BButton("cancel_button", B_TRANSLATE(TR_BUTTON_CANCEL),
		new BMessage(B_QUIT_REQUESTED));

	fStatusLabel = new BStringView("status_label", "");
	fStatusLabel->SetHighColor(100, 100, 100);

	// Layout
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

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	fPasswordControl->MakeFocus(true);
}


LoginWindow::~LoginWindow()
{
}


void
LoginWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_DO_LOGIN:
			_OnLogin();
			break;

		case MSG_PUSH_NOTIFICATION:
		{
			uint8 code;
			if (message->FindUInt8(kFieldCode, &code) == B_OK) {
				if (code == PUSH_CODE_LOGIN_SUCCESS) {
					SetLoginResult(true, "Login successful!");
				} else if (code == PUSH_CODE_LOGIN_FAIL) {
					SetLoginResult(false, "Login failed. Check password.");
				}
			}
			break;
		}

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

	if (success) {
		fStatusLabel->SetHighColor(0, 150, 0);
		fStatusLabel->SetText(message != NULL ? message : "Success!");

		// Close after short delay
		snooze(500000);  // 500ms
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

	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app == NULL || app->GetSerialHandler() == NULL ||
		!app->GetSerialHandler()->IsConnected()) {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Not connected to device.");
		return;
	}

	fLoggingIn = true;
	fLoginButton->SetEnabled(false);
	fPasswordControl->SetEnabled(false);
	fStatusLabel->SetHighColor(100, 100, 100);
	fStatusLabel->SetText("Logging in...");

	// Build and send login command
	// CMD_SEND_LOGIN format:
	// [0] = CMD_SEND_LOGIN (26)
	// [1-6] = pub_key_prefix (6 bytes)
	// [7+] = password (null-terminated)

	uint8 buffer[128];
	buffer[0] = CMD_SEND_LOGIN;
	memcpy(buffer + 1, fContact.publicKey, kPubKeyPrefixSize);

	size_t passLen = strlen(password);
	if (passLen > 64)
		passLen = 64;
	memcpy(buffer + 7, password, passLen);
	buffer[7 + passLen] = '\0';

	size_t totalLen = 8 + passLen;
	app->GetSerialHandler()->SendFrame(buffer, totalLen);
}
