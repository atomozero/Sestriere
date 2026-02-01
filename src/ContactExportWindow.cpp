/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactExportWindow.cpp — Contact import/export dialog implementation
 */

#include "ContactExportWindow.h"

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <Clipboard.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>

#include <cstring>
#include <cstdio>

#include "Constants.h"
#include "Protocol.h"
#include "Sestriere.h"
#include "SerialHandler.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ContactExportWindow"


static const uint32 MSG_COPY_CLIPBOARD	= 'cpcp';
static const uint32 MSG_DO_IMPORT		= 'dimp';


ContactExportWindow::ContactExportWindow(BWindow* parent, bool exportMode,
	const Contact* contact)
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
	if (contact != NULL)
		memcpy(&fContact, contact, sizeof(Contact));
	else
		memset(&fContact, 0, sizeof(Contact));

	// Create views
	if (fExportMode) {
		BString titleStr;
		titleStr.SetToFormat("Export: %s", fContact.advName);
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
			new BMessage(MSG_COPY_CLIPBOARD));
	} else {
		fActionButton = new BButton("action_button", "Import",
			new BMessage(MSG_DO_IMPORT));
	}

	fCloseButton = new BButton("close_button", "Close",
		new BMessage(B_QUIT_REQUESTED));

	fStatusLabel = new BStringView("status_label", "");

	// Layout
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

	// Center on parent
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();

	// If export mode, request export data from device
	if (fExportMode && contact != NULL) {
		Sestriere* app = dynamic_cast<Sestriere*>(be_app);
		if (app != NULL && app->GetSerialHandler() != NULL &&
			app->GetSerialHandler()->IsConnected()) {
			// CMD_EXPORT_CONTACT format:
			// [0] = CMD_EXPORT_CONTACT (17)
			// [1-6] = pub_key_prefix

			uint8 buffer[16];
			buffer[0] = CMD_EXPORT_CONTACT;
			memcpy(buffer + 1, fContact.publicKey, kPubKeyPrefixSize);
			app->GetSerialHandler()->SendFrame(buffer, 7);

			fStatusLabel->SetText("Requesting export data...");
		}
	}
}


ContactExportWindow::~ContactExportWindow()
{
}


void
ContactExportWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_COPY_CLIPBOARD:
			_OnCopyToClipboard();
			break;

		case MSG_DO_IMPORT:
			_OnImport();
			break;

		case RESP_CODE_EXPORT_CONTACT:
		{
			const void* data;
			ssize_t size;
			if (message->FindData(kFieldData, B_RAW_TYPE, &data, &size) == B_OK)
				SetExportData(static_cast<const uint8*>(data), size);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
ContactExportWindow::SetExportData(const uint8* data, size_t length)
{
	// Convert to hex string for display
	BString hexStr;
	for (size_t i = 0; i < length; i++) {
		char hex[4];
		snprintf(hex, sizeof(hex), "%02X", data[i]);
		hexStr.Append(hex);

		// Add space every 2 bytes for readability
		if ((i + 1) % 2 == 0 && i + 1 < length)
			hexStr.Append(" ");

		// Add newline every 32 bytes
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
		fStatusLabel->SetHighColor(200, 0, 0);
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

		fStatusLabel->SetHighColor(0, 150, 0);
		fStatusLabel->SetText("Copied to clipboard!");
	} else {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Failed to access clipboard.");
	}
}


void
ContactExportWindow::_OnImport()
{
	const char* hexText = fDataView->Text();
	if (hexText == NULL || hexText[0] == '\0') {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Please paste contact data first.");
		return;
	}

	// Parse hex string to bytes
	BString cleanHex;
	for (const char* p = hexText; *p != '\0'; p++) {
		if ((*p >= '0' && *p <= '9') ||
			(*p >= 'A' && *p <= 'F') ||
			(*p >= 'a' && *p <= 'f')) {
			cleanHex.Append(*p, 1);
		}
	}

	if (cleanHex.Length() < 2 || cleanHex.Length() % 2 != 0) {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Invalid hex data.");
		return;
	}

	size_t dataLen = cleanHex.Length() / 2;
	uint8* data = new uint8[dataLen];

	for (size_t i = 0; i < dataLen; i++) {
		char byteStr[3] = {cleanHex[i * 2], cleanHex[i * 2 + 1], '\0'};
		data[i] = (uint8)strtoul(byteStr, NULL, 16);
	}

	// Send import command to device
	Sestriere* app = dynamic_cast<Sestriere*>(be_app);
	if (app == NULL || app->GetSerialHandler() == NULL ||
		!app->GetSerialHandler()->IsConnected()) {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Not connected to device.");
		delete[] data;
		return;
	}

	// CMD_IMPORT_CONTACT format:
	// [0] = CMD_IMPORT_CONTACT (18)
	// [1+] = contact data

	uint8 buffer[256];
	buffer[0] = CMD_IMPORT_CONTACT;

	size_t copyLen = dataLen;
	if (copyLen > sizeof(buffer) - 1)
		copyLen = sizeof(buffer) - 1;

	memcpy(buffer + 1, data, copyLen);
	delete[] data;

	status_t status = app->GetSerialHandler()->SendFrame(buffer, 1 + copyLen);
	if (status == B_OK) {
		fStatusLabel->SetHighColor(0, 150, 0);
		fStatusLabel->SetText("Import command sent. Check contacts.");
	} else {
		fStatusLabel->SetHighColor(200, 0, 0);
		fStatusLabel->SetText("Failed to send import command.");
	}
}
