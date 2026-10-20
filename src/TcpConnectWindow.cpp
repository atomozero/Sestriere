/*
 * Copyright 2026, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TcpConnectWindow.cpp — Dialog asking for the TCP address of a device
 */

#include "TcpConnectWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdlib>

#include "Constants.h"


static const uint32 kMsgDoConnect = 'dtcp';


TcpConnectWindow::TcpConnectWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 300, 120), "TCP Connection",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS |
		B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fAddressControl(NULL),
	fConnectButton(NULL),
	fCancelButton(NULL),
	fStatusLabel(NULL)
{
	BStringView* infoLabel = new BStringView("info_label",
		"Enter the TCP address of the MeshCore device:");

	fAddressControl = new BTextControl("address", "Address:",
		"192.168.1.100:4403", new BMessage(kMsgDoConnect));

	fConnectButton = new BButton("connect_button", "Connect",
		new BMessage(kMsgDoConnect));
	fConnectButton->MakeDefault(true);

	fCancelButton = new BButton("cancel_button", "Cancel",
		new BMessage(B_QUIT_REQUESTED));

	fStatusLabel = new BStringView("status_label", "");
	fStatusLabel->SetHighColor(kColorBad);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(infoLabel)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fAddressControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fAddressControl->CreateTextViewLayoutItem(), 1, 0)
		.End()
		.Add(fStatusLabel)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fConnectButton)
		.End()
	.End();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	fAddressControl->MakeFocus(true);
	fAddressControl->TextView()->SelectAll();
}


void
TcpConnectWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgDoConnect:
			_OnConnect();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
TcpConnectWindow::_OnConnect()
{
	BString address(fAddressControl->Text());
	address.Trim();

	// host:port, with a port between 1 and 65535
	int32 colon = address.FindLast(':');
	if (colon <= 0 || colon == address.Length() - 1) {
		fStatusLabel->SetText("Use the form host:port.");
		return;
	}
	BString portString;
	address.CopyInto(portString, colon + 1, address.Length() - colon - 1);
	char* end = NULL;
	long port = strtol(portString.String(), &end, 10);
	if (*end != '\0' || port < 1 || port > 65535) {
		fStatusLabel->SetText("The port must be a number from 1 to 65535.");
		return;
	}

	if (fParent != NULL) {
		BMessage connectMsg(MSG_TCP_ADDRESS_ENTERED);
		connectMsg.AddString("address", address);
		fParent->PostMessage(&connectMsg);
	}

	PostMessage(B_QUIT_REQUESTED);
}
