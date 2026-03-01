/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactExportWindow.cpp — Contact import/export dialog implementation
 */

#include "ContactExportWindow.h"

#include <Button.h>
#include <Clipboard.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Constants.h"


static const uint32 kMsgCopyClipboard	= 'cpcp';
static const uint32 kMsgDoImport		= 'dimp';

// Messages sent to parent window
static const uint32 kMsgExportContact	= 'exct';
static const uint32 kMsgImportContact	= 'imct';


ContactExportWindow::ContactExportWindow(BWindow* parent, bool exportMode,
	const ContactInfo* contact)
	:
	BWindow(BRect(0, 0, 400, 300),
		exportMode ? "Export Contact" : "Import Contact",
		B_FLOATING_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fExportMode(exportMode),
	fTitleLabel(NULL),
	fDataView(NULL),
	fActionButton(NULL),
	fCloseButton(NULL),
	fStatusLabel(NULL)
{
	memset(fPublicKey, 0, sizeof(fPublicKey));
	memset(fContactName, 0, sizeof(fContactName));

	if (contact != NULL) {
		memcpy(fPublicKey, contact->publicKey, 32);
		strlcpy(fContactName, contact->name, sizeof(fContactName));
	}

	if (fExportMode) {
		BString titleStr;
		titleStr.SetToFormat("Export: %s", fContactName);
		fTitleLabel = new BStringView("title_label", titleStr.String());
	} else {
		fTitleLabel = new BStringView("title_label",
			"Paste contact data below:");
	}
	fTitleLabel->SetFont(be_bold_font);

	fDataView = new BTextView("data_view");
	fDataView->SetStylable(false);
	fDataView->MakeEditable(!fExportMode);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);
	fDataView->SetFontAndColor(&monoFont);

	BScrollView* scrollView = new BScrollView("data_scroll", fDataView,
		0, false, true, B_FANCY_BORDER);

	if (fExportMode) {
		fActionButton = new BButton("action_button", "Copy to Clipboard",
			new BMessage(kMsgCopyClipboard));
	} else {
		fActionButton = new BButton("action_button", "Import",
			new BMessage(kMsgDoImport));
	}

	fCloseButton = new BButton("close_button", "Close",
		new BMessage(B_QUIT_REQUESTED));

	fStatusLabel = new BStringView("status_label", "");

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fTitleLabel)
		.Add(scrollView, 1.0)
		.Add(fStatusLabel)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCloseButton)
			.Add(fActionButton)
		.End()
	.End();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	// If export mode, request data from parent
	if (fExportMode && contact != NULL && fParent != NULL) {
		BMessage exportReq(kMsgExportContact);
		exportReq.AddData("pubkey", B_RAW_TYPE, fPublicKey, 6);
		fParent->PostMessage(&exportReq);
		fStatusLabel->SetText("Requesting export data...");
	}
}


ContactExportWindow::~ContactExportWindow()
{
}


bool
ContactExportWindow::QuitRequested()
{
	Hide();
	return false;
}


void
ContactExportWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgCopyClipboard:
			_OnCopyToClipboard();
			break;

		case kMsgDoImport:
			_OnImport();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
ContactExportWindow::SetExportData(const uint8* data, size_t length)
{
	BString hexStr;
	for (size_t i = 0; i < length; i++) {
		char hex[4];
		snprintf(hex, sizeof(hex), "%02X", data[i]);
		hexStr.Append(hex);

		if ((i + 1) % 2 == 0 && i + 1 < length)
			hexStr.Append(" ");

		if ((i + 1) % 32 == 0 && i + 1 < length)
			hexStr.Append("\n");
	}

	fDataView->SetText(hexStr.String());
	fStatusLabel->SetText("Export data ready. Copy to share.");
}


void
ContactExportWindow::_OnCopyToClipboard()
{
	const char* text = fDataView->Text();
	if (text == NULL || text[0] == '\0') {
		fStatusLabel->SetHighColor(kColorBad);
		fStatusLabel->SetText("No data to copy.");
		return;
	}

	if (be_clipboard->Lock()) {
		be_clipboard->Clear();

		BMessage* clip = be_clipboard->Data();
		if (clip != NULL) {
			clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
			be_clipboard->Commit();
		}

		be_clipboard->Unlock();

		fStatusLabel->SetHighColor(kColorGood);
		fStatusLabel->SetText("Copied to clipboard!");
	} else {
		fStatusLabel->SetHighColor(kColorBad);
		fStatusLabel->SetText("Failed to access clipboard.");
	}
}


void
ContactExportWindow::_OnImport()
{
	const char* hexText = fDataView->Text();
	if (hexText == NULL || hexText[0] == '\0') {
		fStatusLabel->SetHighColor(kColorBad);
		fStatusLabel->SetText("Please paste contact data first.");
		return;
	}

	// Parse hex string
	BString cleanHex;
	for (const char* p = hexText; *p != '\0'; p++) {
		if ((*p >= '0' && *p <= '9') ||
			(*p >= 'A' && *p <= 'F') ||
			(*p >= 'a' && *p <= 'f')) {
			cleanHex.Append(*p, 1);
		}
	}

	if (cleanHex.Length() < 2 || cleanHex.Length() % 2 != 0) {
		fStatusLabel->SetHighColor(kColorBad);
		fStatusLabel->SetText("Invalid hex data.");
		return;
	}

	size_t dataLen = cleanHex.Length() / 2;
	uint8* data = new uint8[dataLen];

	for (size_t i = 0; i < dataLen; i++) {
		char byteStr[3] = {cleanHex[i * 2], cleanHex[i * 2 + 1], '\0'};
		data[i] = (uint8)strtoul(byteStr, NULL, 16);
	}

	// Send import command to parent
	if (fParent != NULL) {
		BMessage importMsg(kMsgImportContact);
		importMsg.AddData("data", B_RAW_TYPE, data, dataLen);
		fParent->PostMessage(&importMsg);

		fStatusLabel->SetHighColor(kColorGood);
		fStatusLabel->SetText("Import command sent. Check contacts.");
	} else {
		fStatusLabel->SetHighColor(kColorBad);
		fStatusLabel->SetText("Not connected.");
	}

	delete[] data;
}
