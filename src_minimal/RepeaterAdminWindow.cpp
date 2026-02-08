/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepeaterAdminWindow.cpp — Remote repeater administration window
 */

#include "RepeaterAdminWindow.h"

#include <Alert.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <SeparatorView.h>
#include <StringView.h>
#include <TabView.h>

#include <cstdio>
#include <cstring>

#include "Constants.h"


// Column indices for the contact list
enum {
	kContactNameCol = 0,
	kContactTypeCol,
	kContactKeyCol,
	kContactPathCol,
	kContactLastSeenCol,
	kContactColumnCount
};

// Theme-aware label colors
static inline rgb_color LabelColor()
{
	return tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT);
}

static inline rgb_color ValueColor()
{
	return ui_color(B_PANEL_TEXT_COLOR);
}

static inline rgb_color ActiveColor()
{
	return (rgb_color){77, 182, 172, 255};
}

static inline rgb_color InactiveColor()
{
	return (rgb_color){229, 115, 115, 255};
}


// Helper to create a styled label + value pair
static BView*
_CreateInfoRow(const char* label, BStringView** valueView,
	const char* initialValue = "-")
{
	BView* row = new BView("row", 0);
	row->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BStringView* labelView = new BStringView("label", label);
	labelView->SetHighColor(LabelColor());
	BFont boldFont(be_plain_font);
	boldFont.SetFace(B_BOLD_FACE);
	labelView->SetFont(&boldFont);
	labelView->SetExplicitMinSize(BSize(110, B_SIZE_UNSET));

	*valueView = new BStringView("value", initialValue);
	(*valueView)->SetHighColor(ValueColor());

	BLayoutBuilder::Group<>(row, B_HORIZONTAL, 4)
		.SetInsets(8, 2, 8, 2)
		.Add(labelView)
		.Add(*valueView)
		.AddGlue()
	.End();

	return row;
}


RepeaterAdminWindow::RepeaterAdminWindow(BWindow* parent,
	const uint8* pubkey, const char* name, uint8 type)
	:
	BWindow(BRect(100, 100, 560, 500), "Repeater Administration",
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fNodeName(name),
	fNodeType(type),
	fTabView(NULL),
	fNameView(NULL),
	fTypeView(NULL),
	fSessionView(NULL),
	fBatteryView(NULL),
	fStorageView(NULL),
	fUptimeView(NULL),
	fTxPacketsView(NULL),
	fRxPacketsView(NULL),
	fRssiView(NULL),
	fSnrView(NULL),
	fNoiseFloorView(NULL),
	fContactListView(NULL),
	fRemoveContactButton(NULL),
	fShareContactButton(NULL),
	fResetPathButton(NULL),
	fRebootButton(NULL),
	fFactoryResetButton(NULL),
	fRefreshButton(NULL),
	fSessionActive(true),
	fBattMv(0),
	fUsedKb(0),
	fTotalKb(0),
	fUptime(0),
	fTxPkts(0),
	fRxPkts(0),
	fRssi(0),
	fSnr(0),
	fNoise(0),
	fRefreshTimer(NULL)
{
	memcpy(fPublicKey, pubkey, 32);

	// Set window title with node name
	BString title;
	title.SetToFormat("Admin: %s", name);
	SetTitle(title.String());

	_BuildUI();
	_UpdateOverview();

	// Auto-refresh every 15 seconds
	BMessage timerMsg(MSG_ADMIN_REFRESH_TIMER);
	fRefreshTimer = new BMessageRunner(BMessenger(this), &timerMsg,
		15000000);
}


RepeaterAdminWindow::~RepeaterAdminWindow()
{
	delete fRefreshTimer;
}


bool
RepeaterAdminWindow::QuitRequested()
{
	Hide();
	return false;
}


void
RepeaterAdminWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_ADMIN_REFRESH:
		case MSG_ADMIN_REFRESH_TIMER:
		{
			// Request fresh data from parent (battery + stats)
			if (fParent != NULL && fSessionActive) {
				fParent->PostMessage(MSG_GET_BATTERY);
				fParent->PostMessage(MSG_REQUEST_STATS_DATA);
			}
			break;
		}

		case MSG_ADMIN_REBOOT:
		{
			BAlert* alert = new BAlert("Confirm Reboot",
				"Are you sure you want to reboot this device?\n"
				"The connection will be lost.",
				"Cancel", "Reboot", NULL,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			if (alert->Go() == 1) {
				// Send reboot command via parent
				BMessage cmd(MSG_ADMIN_REBOOT);
				cmd.AddData("pubkey", B_RAW_TYPE, fPublicKey, 32);
				if (fParent != NULL)
					fParent->PostMessage(&cmd);
			}
			break;
		}

		case MSG_ADMIN_FACTORY_RESET:
		{
			BAlert* alert = new BAlert("Confirm Factory Reset",
				"WARNING: This will erase all settings and contacts "
				"on the device!\n\n"
				"Are you absolutely sure?",
				"Cancel", "Factory Reset", NULL,
				B_WIDTH_AS_USUAL, B_STOP_ALERT);
			if (alert->Go() == 1) {
				// Double confirm
				BAlert* confirm = new BAlert("Final Confirmation",
					"This action CANNOT be undone.\nProceed with factory reset?",
					"Cancel", "Yes, Reset", NULL,
					B_WIDTH_AS_USUAL, B_STOP_ALERT);
				if (confirm->Go() == 1) {
					BMessage cmd(MSG_ADMIN_FACTORY_RESET);
					cmd.AddData("pubkey", B_RAW_TYPE, fPublicKey, 32);
					if (fParent != NULL)
						fParent->PostMessage(&cmd);
				}
			}
			break;
		}

		case MSG_ADMIN_REMOVE_CONTACT:
		{
			BRow* row = fContactListView->CurrentSelection();
			if (row == NULL)
				break;

			BStringField* nameField = static_cast<BStringField*>(
				row->GetField(kContactNameCol));
			BStringField* keyField = static_cast<BStringField*>(
				row->GetField(kContactKeyCol));

			BString alertText;
			alertText.SetToFormat(
				"Remove contact '%s' from this device?",
				nameField != NULL ? nameField->String() : "unknown");

			BAlert* alert = new BAlert("Confirm Remove",
				alertText.String(),
				"Cancel", "Remove", NULL,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			if (alert->Go() == 1) {
				BMessage cmd(MSG_ADMIN_REMOVE_CONTACT);
				cmd.AddData("pubkey", B_RAW_TYPE, fPublicKey, 32);
				if (keyField != NULL)
					cmd.AddString("contact_key", keyField->String());
				if (fParent != NULL)
					fParent->PostMessage(&cmd);
			}
			break;
		}

		case MSG_ADMIN_RESET_PATH:
		{
			BRow* row = fContactListView->CurrentSelection();
			if (row == NULL)
				break;

			BStringField* keyField = static_cast<BStringField*>(
				row->GetField(kContactKeyCol));
			BMessage cmd(MSG_ADMIN_RESET_PATH);
			cmd.AddData("pubkey", B_RAW_TYPE, fPublicKey, 32);
			if (keyField != NULL)
				cmd.AddString("contact_key", keyField->String());
			if (fParent != NULL)
				fParent->PostMessage(&cmd);
			break;
		}

		case MSG_ADMIN_DISCONNECT:
		{
			SetSessionActive(false);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
RepeaterAdminWindow::SetBatteryInfo(uint16 battMv, uint32 usedKb,
	uint32 totalKb)
{
	fBattMv = battMv;
	fUsedKb = usedKb;
	fTotalKb = totalKb;
	_UpdateOverview();
}


void
RepeaterAdminWindow::SetStats(uint32 uptime, uint32 txPackets,
	uint32 rxPackets, int8 rssi, int8 snr, int8 noiseFloor)
{
	fUptime = uptime;
	fTxPkts = txPackets;
	fRxPkts = rxPackets;
	fRssi = rssi;
	fSnr = snr;
	fNoise = noiseFloor;
	_UpdateOverview();
}


void
RepeaterAdminWindow::UpdateContactList(
	const BObjectList<ContactInfo, true>* contacts)
{
	if (fContactListView == NULL || contacts == NULL)
		return;

	fContactListView->Clear();

	for (int32 i = 0; i < contacts->CountItems(); i++) {
		ContactInfo* contact = contacts->ItemAt(i);
		if (contact == NULL || !contact->isValid)
			continue;

		BRow* row = new BRow();

		// Name
		row->SetField(new BStringField(
			contact->name[0] != '\0' ? contact->name : "(unnamed)"),
			kContactNameCol);

		// Type
		const char* typeStr;
		switch (contact->type) {
			case 1: typeStr = "Companion"; break;
			case 2: typeStr = "Repeater"; break;
			case 3: typeStr = "Room"; break;
			default: typeStr = "Node"; break;
		}
		row->SetField(new BStringField(typeStr), kContactTypeCol);

		// Key prefix
		char keyHex[14];
		snprintf(keyHex, sizeof(keyHex), "%02X%02X%02X%02X%02X%02X",
			contact->publicKey[0], contact->publicKey[1],
			contact->publicKey[2], contact->publicKey[3],
			contact->publicKey[4], contact->publicKey[5]);
		row->SetField(new BStringField(keyHex), kContactKeyCol);

		// Path length
		char pathStr[16];
		if (contact->outPathLen == (int8)kPathLenDirect)
			snprintf(pathStr, sizeof(pathStr), "direct");
		else if (contact->outPathLen == 0)
			snprintf(pathStr, sizeof(pathStr), "unknown");
		else
			snprintf(pathStr, sizeof(pathStr), "%d hops",
				contact->outPathLen);
		row->SetField(new BStringField(pathStr), kContactPathCol);

		// Last seen
		char seenStr[32];
		if (contact->lastSeen > 0) {
			uint32 now = (uint32)real_time_clock();
			uint32 ago = now - contact->lastSeen;
			if (ago < 60)
				snprintf(seenStr, sizeof(seenStr), "%us ago", ago);
			else if (ago < 3600)
				snprintf(seenStr, sizeof(seenStr), "%um ago", ago / 60);
			else if (ago < 86400)
				snprintf(seenStr, sizeof(seenStr), "%uh ago", ago / 3600);
			else
				snprintf(seenStr, sizeof(seenStr), "%ud ago", ago / 86400);
		} else {
			snprintf(seenStr, sizeof(seenStr), "never");
		}
		row->SetField(new BStringField(seenStr), kContactLastSeenCol);

		fContactListView->AddRow(row);
	}
}


void
RepeaterAdminWindow::SetSessionActive(bool active)
{
	fSessionActive = active;

	if (fSessionView != NULL) {
		if (active) {
			fSessionView->SetHighColor(ActiveColor());
			fSessionView->SetText("Active");
		} else {
			fSessionView->SetHighColor(InactiveColor());
			fSessionView->SetText("Disconnected");
		}
	}

	// Disable action buttons when disconnected
	if (fRebootButton != NULL)
		fRebootButton->SetEnabled(active);
	if (fFactoryResetButton != NULL)
		fFactoryResetButton->SetEnabled(active);
	if (fRemoveContactButton != NULL)
		fRemoveContactButton->SetEnabled(active);
	if (fShareContactButton != NULL)
		fShareContactButton->SetEnabled(active);
	if (fResetPathButton != NULL)
		fResetPathButton->SetEnabled(active);
	if (fRefreshButton != NULL)
		fRefreshButton->SetEnabled(active);

	// Stop auto-refresh when disconnected
	if (!active) {
		delete fRefreshTimer;
		fRefreshTimer = NULL;
	}
}


void
RepeaterAdminWindow::_BuildUI()
{
	fTabView = new BTabView("tabview", B_WIDTH_FROM_WIDEST);

	fTabView->AddTab(_BuildOverviewTab(), new BTab());
	fTabView->TabAt(0)->SetLabel("Overview");

	fTabView->AddTab(_BuildContactsTab(), new BTab());
	fTabView->TabAt(1)->SetLabel("Contacts");

	fTabView->AddTab(_BuildActionsTab(), new BTab());
	fTabView->TabAt(2)->SetLabel("Actions");

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(0)
		.Add(fTabView)
	.End();
}


BView*
RepeaterAdminWindow::_BuildOverviewTab()
{
	BView* view = new BView("overview", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Section: Node Info
	BStringView* nodeHeader = new BStringView("nodeheader", "Node Information");
	BFont headerFont(be_bold_font);
	headerFont.SetSize(12);
	nodeHeader->SetFont(&headerFont);

	BView* nameRow = _CreateInfoRow("Name:", &fNameView, fNodeName.String());

	const char* typeLabel;
	switch (fNodeType) {
		case 1: typeLabel = "Companion"; break;
		case 2: typeLabel = "Repeater"; break;
		case 3: typeLabel = "Room"; break;
		default: typeLabel = "Node"; break;
	}
	BView* typeRow = _CreateInfoRow("Type:", &fTypeView, typeLabel);
	BView* sessionRow = _CreateInfoRow("Session:", &fSessionView, "Active");
	fSessionView->SetHighColor(ActiveColor());

	// Key
	char keyHex[14];
	snprintf(keyHex, sizeof(keyHex), "%02X%02X%02X%02X%02X%02X",
		fPublicKey[0], fPublicKey[1], fPublicKey[2],
		fPublicKey[3], fPublicKey[4], fPublicKey[5]);
	BStringView* keyLabelV = new BStringView("keylabel", "Key:");
	BFont boldFont(be_plain_font);
	boldFont.SetFace(B_BOLD_FACE);
	keyLabelV->SetFont(&boldFont);
	keyLabelV->SetHighColor(LabelColor());
	keyLabelV->SetExplicitMinSize(BSize(110, B_SIZE_UNSET));

	BStringView* keyValueV = new BStringView("keyvalue", keyHex);
	keyValueV->SetHighColor(ValueColor());

	BView* keyRow = new BView("keyrow", 0);
	keyRow->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(keyRow, B_HORIZONTAL, 4)
		.SetInsets(8, 2, 8, 2)
		.Add(keyLabelV)
		.Add(keyValueV)
		.AddGlue()
	.End();

	// Section: Device Status
	BStringView* statusHeader = new BStringView("statusheader",
		"Device Status");
	statusHeader->SetFont(&headerFont);

	BView* battRow = _CreateInfoRow("Battery:", &fBatteryView);
	BView* storageRow = _CreateInfoRow("Storage:", &fStorageView);
	BView* uptimeRow = _CreateInfoRow("Uptime:", &fUptimeView);

	// Section: Radio
	BStringView* radioHeader = new BStringView("radioheader",
		"Radio Statistics");
	radioHeader->SetFont(&headerFont);

	BView* txRow = _CreateInfoRow("TX Packets:", &fTxPacketsView);
	BView* rxRow = _CreateInfoRow("RX Packets:", &fRxPacketsView);
	BView* rssiRow = _CreateInfoRow("RSSI:", &fRssiView);
	BView* snrRow = _CreateInfoRow("SNR:", &fSnrView);
	BView* noiseRow = _CreateInfoRow("Noise Floor:", &fNoiseFloorView);

	BLayoutBuilder::Group<>(view, B_VERTICAL, 2)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(nodeHeader)
		.Add(nameRow)
		.Add(typeRow)
		.Add(sessionRow)
		.Add(keyRow)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(statusHeader)
		.Add(battRow)
		.Add(storageRow)
		.Add(uptimeRow)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(radioHeader)
		.Add(txRow)
		.Add(rxRow)
		.Add(rssiRow)
		.Add(snrRow)
		.Add(noiseRow)
		.AddGlue()
	.End();

	return view;
}


BView*
RepeaterAdminWindow::_BuildContactsTab()
{
	BView* view = new BView("contacts", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fContactListView = new BColumnListView("contactlist", 0,
		B_FANCY_BORDER);

	fContactListView->AddColumn(new BStringColumn("Name", 120, 60, 250,
		B_TRUNCATE_END), kContactNameCol);
	fContactListView->AddColumn(new BStringColumn("Type", 80, 50, 120,
		B_TRUNCATE_END), kContactTypeCol);
	fContactListView->AddColumn(new BStringColumn("Key", 100, 80, 150,
		B_TRUNCATE_END), kContactKeyCol);
	fContactListView->AddColumn(new BStringColumn("Path", 70, 50, 100,
		B_TRUNCATE_END), kContactPathCol);
	fContactListView->AddColumn(new BStringColumn("Last Seen", 80, 60, 120,
		B_TRUNCATE_END), kContactLastSeenCol);

	// Action buttons
	fRemoveContactButton = new BButton("remove", "Remove",
		new BMessage(MSG_ADMIN_REMOVE_CONTACT));
	fResetPathButton = new BButton("resetpath", "Reset Path",
		new BMessage(MSG_ADMIN_RESET_PATH));

	BLayoutBuilder::Group<>(view, B_VERTICAL, 4)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fContactListView, 1)
		.AddGroup(B_HORIZONTAL, 4)
			.Add(fRemoveContactButton)
			.Add(fResetPathButton)
			.AddGlue()
		.End()
	.End();

	return view;
}


BView*
RepeaterAdminWindow::_BuildActionsTab()
{
	BView* view = new BView("actions", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Refresh button
	fRefreshButton = new BButton("refresh", "Refresh Status",
		new BMessage(MSG_ADMIN_REFRESH));

	// Reboot
	BStringView* rebootLabel = new BStringView("rebootlabel",
		"Restart the device. The current session will be lost.");
	BFont smallFont(be_plain_font);
	smallFont.SetSize(10);
	rebootLabel->SetFont(&smallFont);
	rebootLabel->SetHighColor(LabelColor());

	fRebootButton = new BButton("reboot", "Reboot Device",
		new BMessage(MSG_ADMIN_REBOOT));

	// Factory reset
	BStringView* resetLabel = new BStringView("resetlabel",
		"Erase all settings and contacts. Cannot be undone!");
	resetLabel->SetFont(&smallFont);
	resetLabel->SetHighColor(InactiveColor());

	fFactoryResetButton = new BButton("factoryreset", "Factory Reset",
		new BMessage(MSG_ADMIN_FACTORY_RESET));

	BLayoutBuilder::Group<>(view, B_VERTICAL, 8)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fRefreshButton)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_VERTICAL, 2)
			.Add(fRebootButton)
			.Add(rebootLabel)
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_VERTICAL, 2)
			.Add(fFactoryResetButton)
			.Add(resetLabel)
		.End()
		.AddGlue()
	.End();

	return view;
}


void
RepeaterAdminWindow::_UpdateOverview()
{
	if (fBatteryView != NULL) {
		if (fBattMv > 0) {
			// Estimate percentage (3.0V = 0%, 4.2V = 100%)
			float pct = ((float)fBattMv - 3000.0f) / 1200.0f * 100.0f;
			if (pct < 0) pct = 0;
			if (pct > 100) pct = 100;
			BString battStr;
			battStr.SetToFormat("%.0f%% (%u mV)", pct, fBattMv);
			fBatteryView->SetText(battStr.String());
		}
	}

	if (fStorageView != NULL && fTotalKb > 0) {
		BString storStr;
		if (fTotalKb > 1024)
			storStr.SetToFormat("%.1f / %.1f MB",
				(float)fUsedKb / 1024.0f, (float)fTotalKb / 1024.0f);
		else
			storStr.SetToFormat("%u / %u KB", fUsedKb, fTotalKb);
		fStorageView->SetText(storStr.String());
	}

	if (fUptimeView != NULL && fUptime > 0) {
		BString uptStr;
		_FormatUptime(fUptime, uptStr);
		fUptimeView->SetText(uptStr.String());
	}

	if (fTxPacketsView != NULL) {
		BString txStr;
		txStr.SetToFormat("%u", fTxPkts);
		fTxPacketsView->SetText(txStr.String());
	}

	if (fRxPacketsView != NULL) {
		BString rxStr;
		rxStr.SetToFormat("%u", fRxPkts);
		fRxPacketsView->SetText(rxStr.String());
	}

	if (fRssiView != NULL) {
		BString rssiStr;
		rssiStr.SetToFormat("%d dBm", fRssi);
		fRssiView->SetText(rssiStr.String());
	}

	if (fSnrView != NULL) {
		BString snrStr;
		snrStr.SetToFormat("%d dB", fSnr);
		fSnrView->SetText(snrStr.String());
	}

	if (fNoiseFloorView != NULL) {
		BString nfStr;
		nfStr.SetToFormat("%d dBm", fNoise);
		fNoiseFloorView->SetText(nfStr.String());
	}
}


void
RepeaterAdminWindow::_FormatUptime(uint32 seconds, BString& output)
{
	uint32 days = seconds / 86400;
	uint32 hours = (seconds % 86400) / 3600;
	uint32 minutes = (seconds % 3600) / 60;

	if (days > 0)
		output.SetToFormat("%ud %uh %um", days, hours, minutes);
	else if (hours > 0)
		output.SetToFormat("%uh %um", hours, minutes);
	else
		output.SetToFormat("%um %us", minutes, seconds % 60);
}
