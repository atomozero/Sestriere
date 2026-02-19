/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * AddChannelWindow.cpp — Dialog for adding a private channel
 */

#include "AddChannelWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>
#include <TextView.h>

#include "Constants.h"


static const uint32 kMsgDoAdd = 'dadd';

// Message sent to parent window with "name" field
static const uint32 kMsgCreateChannel = 'crcn';


AddChannelWindow::AddChannelWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 280, 100), "Add Channel",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS |
		B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fNameControl(NULL),
	fAddButton(NULL),
	fCancelButton(NULL)
{
	fNameControl = new BTextControl("name", "Name:", "",
		new BMessage(kMsgDoAdd));
	fNameControl->SetModificationMessage(NULL);
	fNameControl->TextView()->SetMaxBytes(31);  // ChannelInfo.name is 32 bytes

	fAddButton = new BButton("add_button", "Add",
		new BMessage(kMsgDoAdd));
	fAddButton->MakeDefault(true);

	fCancelButton = new BButton("cancel_button", "Cancel",
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fNameControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fNameControl->CreateTextViewLayoutItem(), 1, 0)
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

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
AddChannelWindow::_OnAdd()
{
	const char* name = fNameControl->Text();
	if (name == NULL || name[0] == '\0')
		return;

	if (fParent != NULL) {
		BMessage msg(kMsgCreateChannel);
		msg.AddString("name", name);
		fParent->PostMessage(&msg);
	}

	// Clear and hide
	fNameControl->SetText("");
	PostMessage(B_QUIT_REQUESTED);
}
