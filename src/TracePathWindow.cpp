/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TracePathWindow.cpp — Trace path visualization window implementation
 */

#include "TracePathWindow.h"

#include <Application.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>

#include <cstring>
#include <cstdio>

#include "Constants.h"


static const uint32 MSG_START_TRACE = 'sttr';


TracePathWindow::TracePathWindow(BWindow* parent, const ContactInfo* contact)
	:
	BWindow(BRect(0, 0, 380, 320), "Trace Path",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fTargetLabel(NULL),
	fHopList(NULL),
	fTraceButton(NULL),
	fCloseButton(NULL),
	fStatusLabel(NULL),
	fHops(10),
	fTracing(false)
{
	if (contact != NULL)
		fContact = *contact;
	else
		fContact = ContactInfo();

	// Create views
	BString titleStr;
	titleStr.SetToFormat("Trace path to: %s",
		fContact.name[0] ? fContact.name : "Unknown");
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


bool
TracePathWindow::QuitRequested()
{
	// Just hide instead of closing - MainWindow manages our lifetime
	Hide();
	return false;
}


void
TracePathWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_START_TRACE:
			_OnStartTrace();
			break;

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
TracePathWindow::ParseTraceData(const uint8* data, size_t length)
{
	// PUSH_TRACE_DATA format:
	// [0]     = code (0x89)
	// [1]     = reserved
	// [2]     = pathLen (byte count of hop data, NOT hop count)
	// [3]     = flags
	// [4-7]   = tag (uint32)
	// [8-11]  = authCode (uint32)
	// [12..12+pathLen-1]  = hop identifiers
	// [12+pathLen..]      = SNR bytes (numHops + 1 values)
	//
	// pathLen=1: single 1-byte hash (legacy)
	// pathLen>=4: N hops × 4-byte pubkey prefix (numHops = pathLen / 4)

	if (length < 12)
		return;

	uint8 pathLen = data[2];

	uint8 numHops;
	uint8 hopSize;
	if (pathLen <= 1) {
		numHops = pathLen;
		hopSize = 1;
	} else {
		hopSize = 4;
		numHops = pathLen / hopSize;
	}

	size_t hashOffset = 12;
	size_t snrOffset = hashOffset + pathLen;

	// Clear previous hops for a fresh trace result
	fHops.MakeEmpty();

	for (uint8 i = 0; i < numHops; i++) {
		TraceHop hop;
		memset(hop.pubKeyPrefix, 0, sizeof(hop.pubKeyPrefix));

		size_t hopStart = hashOffset + (size_t)i * hopSize;
		if (hopStart + hopSize <= length) {
			if (hopSize == 4) {
				memcpy(hop.pubKeyPrefix, data + hopStart, 4);
				snprintf(hop.name, sizeof(hop.name),
					"Hop #%d (%02X%02X%02X%02X)", i + 1,
					hop.pubKeyPrefix[0], hop.pubKeyPrefix[1],
					hop.pubKeyPrefix[2], hop.pubKeyPrefix[3]);
			} else {
				hop.pubKeyPrefix[0] = data[hopStart];
				snprintf(hop.name, sizeof(hop.name),
					"Hop #%d (hash: 0x%02X)", i + 1, hop.pubKeyPrefix[0]);
			}
		}

		if (snrOffset + i < length)
			hop.snr = (int8)data[snrOffset + i];

		hop.timestamp = (uint32)real_time_clock();
		AddTraceHop(hop);
	}

	// Last SNR entry is the destination's SNR
	if (numHops > 0 && snrOffset + numHops < length) {
		TraceHop destHop;
		memset(destHop.pubKeyPrefix, 0, sizeof(destHop.pubKeyPrefix));
		destHop.snr = (int8)data[snrOffset + numHops];
		snprintf(destHop.name, sizeof(destHop.name), "Destination");
		destHop.timestamp = (uint32)real_time_clock();
		AddTraceHop(destHop);
	}

	// Mark trace as complete
	SetTraceComplete(numHops > 0);
}


void
TracePathWindow::_OnStartTrace()
{
	if (fTracing)
		return;

	// Clear previous results
	fHops.MakeEmpty();
	_UpdateHopList();

	fTracing = true;
	fTraceButton->SetEnabled(false);
	fTraceButton->SetLabel("Tracing...");
	fStatusLabel->SetText("Sending trace request...");

	// Send trace path request to parent window
	if (fParent != NULL) {
		BMessage msg(MSG_TRACE_PATH);
		msg.AddData("pubkey", B_RAW_TYPE, fContact.publicKey, kPubKeyPrefixSize);
		fParent->PostMessage(&msg);
	}
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
		if (hop == NULL)
			continue;

		BString hopStr;
		float snrDb = hop->snr / 4.0f;

		char keyPrefix[16];
		_FormatPubKeyPrefix(hop->pubKeyPrefix, keyPrefix, sizeof(keyPrefix));

		// Color indicator based on SNR
		const char* quality;
		if (snrDb >= 10.0f)
			quality = "[++]";  // Excellent
		else if (snrDb >= 5.0f)
			quality = "[+ ]";  // Good
		else if (snrDb >= 0.0f)
			quality = "[  ]";  // Fair
		else if (snrDb >= -5.0f)
			quality = "[ -]";  // Poor
		else
			quality = "[--]";  // Bad

		if (hop->name[0] != '\0') {
			hopStr.SetToFormat("%d. %s %s [%s] SNR: %.1f dB",
				(int)(i + 1), quality, hop->name, keyPrefix, snrDb);
		} else {
			hopStr.SetToFormat("%d. %s [%s] SNR: %.1f dB",
				(int)(i + 1), quality, keyPrefix, snrDb);
		}

		fHopList->AddItem(new BStringItem(hopStr.String()));
	}

	fHopList->Invalidate();
}


void
TracePathWindow::_FormatPubKeyPrefix(const uint8* prefix, char* out, size_t outSize)
{
	if (outSize < 13) {
		if (outSize > 0)
			out[0] = '\0';
		return;
	}

	for (size_t i = 0; i < kPubKeyPrefixSize; i++)
		snprintf(out + i * 2, 3, "%02X", prefix[i]);
}
