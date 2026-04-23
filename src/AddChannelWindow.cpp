/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * AddChannelWindow.cpp — Dialog for adding/joining a channel
 */

#include "AddChannelWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <RadioButton.h>
#include <TextControl.h>
#include <TextView.h>

#include "Constants.h"


static const uint32 kMsgDoAdd = 'dadd';
static const uint32 kMsgModeChanged = 'mdch';

// Message sent to parent window with "name" and optional "psk" fields
static const uint32 kMsgCreateChannel = 'crcn';


AddChannelWindow::AddChannelWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 380, 180), "Add Channel",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fNameControl(NULL),
	fPskControl(NULL),
	fModeCreate(NULL),
	fModeJoin(NULL),
	fModeHashtag(NULL),
	fAddButton(NULL),
	fCancelButton(NULL)
{
	fModeCreate = new BRadioButton("mode_create", "Create private",
		new BMessage(kMsgModeChanged));
	fModeJoin = new BRadioButton("mode_join", "Join private",
		new BMessage(kMsgModeChanged));
	fModeHashtag = new BRadioButton("mode_hashtag", "Join hashtag",
		new BMessage(kMsgModeChanged));
	fModeCreate->SetValue(B_CONTROL_ON);

	fNameControl = new BTextControl("name", "Name:", "",
		new BMessage(kMsgDoAdd));
	fNameControl->SetModificationMessage(NULL);
	fNameControl->TextView()->SetMaxBytes(31);

	fPskControl = new BTextControl("psk", "PSK (hex):", "", NULL);
	fPskControl->SetModificationMessage(NULL);
	fPskControl->TextView()->SetMaxBytes(32);  // 16 bytes = 32 hex chars
	fPskControl->SetEnabled(false);

	fAddButton = new BButton("add_button", "Create",
		new BMessage(kMsgDoAdd));
	fAddButton->MakeDefault(true);

	fCancelButton = new BButton("cancel_button", "Cancel",
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fModeCreate)
			.Add(fModeJoin)
			.Add(fModeHashtag)
		.End()
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fNameControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fNameControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fPskControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fPskControl->CreateTextViewLayoutItem(), 1, 1)
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fAddButton)
		.End()
	.End();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	fNameControl->MakeFocus(true);
}


AddChannelWindow::~AddChannelWindow()
{
}


bool
AddChannelWindow::QuitRequested()
{
	return true;
}


void
AddChannelWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgDoAdd:
			_OnAdd();
			break;

		case kMsgModeChanged:
			_UpdatePskField();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
AddChannelWindow::_UpdatePskField()
{
	bool joinMode = (fModeJoin->Value() == B_CONTROL_ON);
	bool hashtagMode = (fModeHashtag->Value() == B_CONTROL_ON);
	fPskControl->SetEnabled(joinMode);
	if (joinMode || hashtagMode)
		fAddButton->SetLabel("Join");
	else
		fAddButton->SetLabel("Create");
}


void
AddChannelWindow::_OnAdd()
{
	const char* name = fNameControl->Text();
	if (name == NULL || name[0] == '\0')
		return;

	if (fParent == NULL)
		return;

	BMessage msg(kMsgCreateChannel);
	msg.AddString("name", name);

	if (fModeHashtag->Value() == B_CONTROL_ON) {
		// Hashtag mode: PSK derived from channel name via SHA-256
		msg.AddBool("hashtag", true);
	} else if (fModeJoin->Value() == B_CONTROL_ON) {
		// Join mode: user-provided PSK in hex
		const char* pskHex = fPskControl->Text();
		if (pskHex != NULL && pskHex[0] != '\0')
			msg.AddString("psk_hex", pskHex);
	} else {
		// Create mode: generate random PSK
		msg.AddBool("random_psk", true);
	}

	fParent->PostMessage(&msg);

	fNameControl->SetText("");
	fPskControl->SetText("");
	PostMessage(B_QUIT_REQUESTED);
}
