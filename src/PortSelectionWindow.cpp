/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * PortSelectionWindow.cpp — Serial port selection dialog implementation
 */

#include "PortSelectionWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>

#include "Constants.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PortSelectionWindow"


static const uint32 MSG_PORT_SELECTED	= 'ptsl';
static const uint32 MSG_REFRESH_PORTS	= 'rfpt';
static const uint32 MSG_DO_CONNECT		= 'doco';


PortSelectionWindow::PortSelectionWindow(BWindow* parent)
	:
	BWindow(BRect(0, 0, 300, 250), B_TRANSLATE(TR_TITLE_PORT_SELECTION),
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS |
		B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fPortList(NULL),
	fConnectButton(NULL),
	fRefreshButton(NULL),
	fStatusLabel(NULL)
{
	// Create views
	fStatusLabel = new BStringView("status_label",
		B_TRANSLATE(TR_LABEL_SELECT_PORT));

	fPortList = new BListView("port_list", B_SINGLE_SELECTION_LIST);
	fPortList->SetSelectionMessage(new BMessage(MSG_PORT_SELECTED));
	fPortList->SetInvocationMessage(new BMessage(MSG_DO_CONNECT));

	BScrollView* scrollView = new BScrollView("port_scroll", fPortList,
		0, false, true, B_FANCY_BORDER);

	fConnectButton = new BButton("connect_button",
		B_TRANSLATE(TR_BUTTON_CONNECT), new BMessage(MSG_DO_CONNECT));
	fConnectButton->SetEnabled(false);

	fRefreshButton = new BButton("refresh_button",
		B_TRANSLATE(TR_BUTTON_REFRESH), new BMessage(MSG_REFRESH_PORTS));

	BButton* cancelButton = new BButton("cancel_button",
		B_TRANSLATE(TR_BUTTON_CANCEL), new BMessage(B_QUIT_REQUESTED));

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fStatusLabel)
		.Add(scrollView, 1.0)
		.AddGroup(B_HORIZONTAL)
			.Add(fRefreshButton)
			.AddGlue()
			.Add(cancelButton)
			.Add(fConnectButton)
		.End()
	.End();

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	// Populate port list
	_PopulatePortList();
}


PortSelectionWindow::~PortSelectionWindow()
{
}


void
PortSelectionWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PORT_SELECTED:
		{
			int32 selection = fPortList->CurrentSelection();
			fConnectButton->SetEnabled(selection >= 0);
			break;
		}

		case MSG_DO_CONNECT:
			_OnConnect();
			break;

		case MSG_REFRESH_PORTS:
			_OnRefresh();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
PortSelectionWindow::_PopulatePortList()
{
	// Clear existing items
	for (int32 i = fPortList->CountItems() - 1; i >= 0; i--)
		delete fPortList->RemoveItem(i);

	// Get available ports
	BMessage ports;
	if (SerialHandler::ListPorts(&ports) == B_OK) {
		const char* port;
		int32 index = 0;
		while (ports.FindString(kFieldPort, index, &port) == B_OK) {
			fPortList->AddItem(new BStringItem(port));
			index++;
		}
	}

	// Update status
	if (fPortList->CountItems() == 0) {
		fStatusLabel->SetText(B_TRANSLATE(TR_LABEL_NO_PORTS));
		fConnectButton->SetEnabled(false);
	} else {
		fStatusLabel->SetText(B_TRANSLATE(TR_LABEL_SELECT_PORT));
	}
}


void
PortSelectionWindow::_OnConnect()
{
	int32 selection = fPortList->CurrentSelection();
	if (selection < 0)
		return;

	BStringItem* item = dynamic_cast<BStringItem*>(fPortList->ItemAt(selection));
	if (item == NULL)
		return;

	// Send port selection to parent
	if (fParent != NULL) {
		BMessage msg(MSG_SERIAL_PORT_SELECTED);
		msg.AddString(kFieldPort, item->Text());
		fParent->PostMessage(&msg);
	}

	// Close window
	PostMessage(B_QUIT_REQUESTED);
}


void
PortSelectionWindow::_OnRefresh()
{
	_PopulatePortList();
}
