/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TracePathWindow.cpp — Trace path visualization window implementation
 */

#include "TracePathWindow.h"

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>

#include <cstring>
#include <cstdio>

#include "Constants.h"
#include "Protocol.h"
#include "Sestriere.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TracePathWindow"


static const uint32 MSG_START_TRACE = 'sttr';


TracePathWindow::TracePathWindow(BWindow* parent, const Contact* contact)
	:
	BWindow(BRect(0, 0, 350, 300), "Trace Path",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fTargetLabel(NULL),
	fHopList(NULL),
	fTraceButton(NULL),
	fCloseButton(NULL),
	fStatusLabel(NULL),
	fHops(10, true),
	fTracing(false)
{
	if (contact != NULL)
		memcpy(&fContact, contact, sizeof(Contact));
	else
		memset(&fContact, 0, sizeof(Contact));

	// Create views
	BString titleStr;
	titleStr.SetToFormat("Trace path to: %s", fContact.advName);
	fTargetLabel = new BStringView("target_label", titleStr.String());
	fTargetLabel->SetFont(be_bold_font);

	fHopList = new BListView("hop_list", B_SINGLE_SELECTION_LIST);

	BScrollView* scrollView = new BScrollView("hop_scroll", fHopList,
		0, false, true, B_FANCY_BORDER);

	fTraceButton = new BButton("trace_button", "Start Trace",
		new BMessage(MSG_START_TRACE));

	fCloseButton = new BButton("close_button", "Close",
		new BMessage(B_QUIT_REQUESTED));

	fStatusLabel = new BStringView("status_label", "Press Start to begin trace");

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fTargetLabel)
		.Add(scrollView, 1.0)
		.Add(fStatusLabel)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCloseButton)
			.Add(fTraceButton)
		.End()
	.End();

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();
}


TracePathWindow::~TracePathWindow()
{
}


void
TracePathWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_START_TRACE:
			_OnStartTrace();
			break;

		case PUSH_CODE_TRACE_DATA:
		{
			const void* data;
			ssize_t size;
			if (message->FindData(kFieldData, B_RAW_TYPE, &data, &size) == B_OK
				&& size >= 7) {
				const uint8* payload = static_cast<const uint8*>(data);

				TraceHop* hop = new TraceHop();
				memset(hop, 0, sizeof(TraceHop));

				// Parse trace data
				// [0] = PUSH_CODE_TRACE_DATA (0x89)
				// [1-6] = pub_key_prefix
				// [7] = snr (signed, *4)
				// [8+] = name (optional)

				memcpy(hop->pubKeyPrefix, payload + 1, kPubKeyPrefixSize);

				if (size >= 8)
					hop->snr = (int8)payload[7];

				if (size > 8) {
					size_t nameLen = size - 8;
					if (nameLen >= sizeof(hop->name))
						nameLen = sizeof(hop->name) - 1;
					memcpy(hop->name, payload + 8, nameLen);
					hop->name[nameLen] = '\0';
				}

				hop->timestamp = (uint32)real_time_clock();
				AddTraceHop(*hop);
				delete hop;
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
TracePathWindow::AddTraceHop(const TraceHop& hop)
{
	TraceHop* newHop = new TraceHop(hop);
	fHops.AddItem(newHop);
	_UpdateHopList();
}


void
TracePathWindow::SetTraceComplete(bool success)
{
	fTracing = false;
	fTraceButton->SetEnabled(true);
	fTraceButton->SetLabel("Start Trace");

	if (success) {
		BString status;
		status.SetToFormat("Trace complete: %d hops", (int)fHops.CountItems());
		fStatusLabel->SetText(status.String());
	} else {
		fStatusLabel->SetText("Trace failed or timed out");
	}
}


void
TracePathWindow::_OnStartTrace()
{
	if (fTracing)
		return;

	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app == NULL || app->GetSerialHandler() == NULL ||
		!app->GetSerialHandler()->IsConnected()) {
		fStatusLabel->SetText("Not connected to device");
		return;
	}

	// Clear previous results
	fHops.MakeEmpty();
	_UpdateHopList();

	fTracing = true;
	fTraceButton->SetEnabled(false);
	fTraceButton->SetLabel("Tracing...");
	fStatusLabel->SetText("Sending trace request...");

	// CMD_SEND_TRACE_PATH format:
	// [0] = CMD_SEND_TRACE_PATH (36)
	// [1-6] = pub_key_prefix

	uint8 buffer[16];
	buffer[0] = CMD_SEND_TRACE_PATH;
	memcpy(buffer + 1, fContact.publicKey, kPubKeyPrefixSize);

	app->GetSerialHandler()->SendFrame(buffer, 7);
}


void
TracePathWindow::_UpdateHopList()
{
	// Clear list
	for (int32 i = fHopList->CountItems() - 1; i >= 0; i--)
		delete fHopList->RemoveItem(i);

	// Add hops
	for (int32 i = 0; i < fHops.CountItems(); i++) {
		TraceHop* hop = fHops.ItemAt(i);

		BString hopStr;
		float snrDb = hop->snr / 4.0f;

		char keyPrefix[16];
		Protocol::FormatPubKeyPrefix(hop->pubKeyPrefix, keyPrefix,
			sizeof(keyPrefix));

		if (hop->name[0] != '\0') {
			hopStr.SetToFormat("%d. %s [%s] SNR: %.1f dB",
				(int)(i + 1), hop->name, keyPrefix, snrDb);
		} else {
			hopStr.SetToFormat("%d. [%s] SNR: %.1f dB",
				(int)(i + 1), keyPrefix, snrDb);
		}

		fHopList->AddItem(new BStringItem(hopStr.String()));
	}

	fHopList->Invalidate();
}
