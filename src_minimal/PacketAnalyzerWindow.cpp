/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * PacketAnalyzerWindow.cpp — Real-time MeshCore packet analyzer window
 */

#include "PacketAnalyzerWindow.h"

#include <Button.h>
#include <CheckBox.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Entry.h>
#include <FilePanel.h>
#include <File.h>
#include <Messenger.h>
#include <Path.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SeparatorItem.h>
#include <SplitView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <OS.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "Constants.h"


// Private message codes
enum {
	MSG_PA_START_STOP = 'pass',
	MSG_PA_CLEAR = 'pacl',
	MSG_PA_FILTER_ALL = 'pfal',
	MSG_PA_FILTER_MSG = 'pfms',
	MSG_PA_FILTER_ADVERT = 'pfad',
	MSG_PA_FILTER_PUSH = 'pfpu',
	MSG_PA_FILTER_RSP = 'pfrs',
	MSG_PA_SEARCH = 'pasr',
	MSG_PA_ROW_SELECTED = 'pars',
	MSG_PA_EXPORT = 'paex',
	MSG_PA_SAVE_DONE = 'pasd',
};

// Theme-aware colors
static inline rgb_color DetailBgColor()
{
	return tint_color(ui_color(B_DOCUMENT_BACKGROUND_COLOR), B_NO_TINT);
}

static inline rgb_color DetailTextColor()
{
	return ui_color(B_DOCUMENT_TEXT_COLOR);
}

static inline rgb_color HexOffsetColor()
{
	return tint_color(ui_color(B_DOCUMENT_TEXT_COLOR), B_LIGHTEN_1_TINT);
}

static inline rgb_color HexDataColor()
{
	return ui_color(B_DOCUMENT_TEXT_COLOR);
}

static inline rgb_color HexAsciiColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}

static inline rgb_color StatusBgColor()
{
	return tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
}


PacketAnalyzerWindow::PacketAnalyzerWindow(BWindow* parent)
	:
	BWindow(BRect(100, 100, 820, 620), "Packet Analyzer",
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fMenuBar(NULL),
	fStartStopButton(NULL),
	fClearButton(NULL),
	fFilterMenu(NULL),
	fFilterField(NULL),
	fSearchField(NULL),
	fAutoScrollCheck(NULL),
	fPacketList(NULL),
	fDetailView(NULL),
	fSplitView(NULL),
	fStatusTotal(NULL),
	fStatusFiltered(NULL),
	fStatusRate(NULL),
	fPackets(20),
	fCapturing(true),
	fPacketIndex(0),
	fFilterType(-1),
	fSavePanel(NULL),
	fRateCount(0),
	fRateStartTime(0)
{
	_BuildMenuBar();
	_BuildUI();
	_UpdateStatusBar();
}


PacketAnalyzerWindow::~PacketAnalyzerWindow()
{
	delete fSavePanel;
}


bool
PacketAnalyzerWindow::QuitRequested()
{
	Hide();
	return false;
}


void
PacketAnalyzerWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PA_START_STOP:
		{
			fCapturing = !fCapturing;
			fStartStopButton->SetLabel(fCapturing ? "Stop" : "Start");
			if (fCapturing) {
				fRateCount = 0;
				fRateStartTime = system_time();
			}
			break;
		}

		case MSG_PA_CLEAR:
			Clear();
			break;

		case MSG_PA_FILTER_ALL:
			fFilterType = -1;
			_RebuildFilteredList();
			break;

		case MSG_PA_FILTER_MSG:
			fFilterType = 0x10;	// Messages (V3 DM + channel)
			_RebuildFilteredList();
			break;

		case MSG_PA_FILTER_ADVERT:
			fFilterType = 0x80;	// Adverts and push notifications
			_RebuildFilteredList();
			break;

		case MSG_PA_FILTER_PUSH:
			fFilterType = 0x88;	// Raw radio packets only
			_RebuildFilteredList();
			break;

		case MSG_PA_FILTER_RSP:
			fFilterType = 0x00;	// Responses (RSP_*)
			_RebuildFilteredList();
			break;

		case MSG_PA_SEARCH:
		{
			fSearchText = fSearchField->Text();
			_RebuildFilteredList();
			break;
		}

		case MSG_PA_ROW_SELECTED:
		{
			BRow* row = fPacketList->CurrentSelection();
			if (row != NULL) {
				int32 index = fPacketList->IndexOf(row);
				_UpdatePacketDetail(index);
			}
			break;
		}

		case MSG_PA_EXPORT:
		{
			if (fSavePanel == NULL) {
				BMessage saveMsg(MSG_PA_SAVE_DONE);
				fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this),
					NULL, 0, false, &saveMsg);
				fSavePanel->SetSaveText("packets.csv");
			}
			fSavePanel->Show();
			break;
		}

		case MSG_PA_SAVE_DONE:
		{
			entry_ref dirRef;
			BString name;
			if (message->FindRef("directory", &dirRef) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BPath path(&dirRef);
				path.Append(name.String());
				_ExportCSV(path.Path());
			}
			break;
		}

		case MSG_PACKET_CAPTURED:
		{
			if (!fCapturing)
				break;

			const void* data;
			ssize_t size;
			if (message->FindData("packet", B_RAW_TYPE, &data, &size) == B_OK
				&& size == sizeof(CapturedPacket)) {
				AddPacket(*reinterpret_cast<const CapturedPacket*>(data));
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
PacketAnalyzerWindow::AddPacket(const CapturedPacket& packet)
{
	CapturedPacket* stored = new CapturedPacket(packet);
	fPackets.AddItem(stored);

	// Rate tracking
	fRateCount++;

	if (_MatchesFilter(*stored)) {
		// Add row to visible list
		BRow* row = new BRow();

		char indexStr[12];
		snprintf(indexStr, sizeof(indexStr), "%u", stored->index);

		char timeStr[16];
		time_t t = (time_t)stored->timestamp;
		struct tm tm;
		if (localtime_r(&t, &tm) != NULL)
			strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm);
		else
			snprintf(timeStr, sizeof(timeStr), "--:--:--");

		char sizeStr[12];
		snprintf(sizeStr, sizeof(sizeStr), "%u", stored->payloadSize);

		char snrStr[12];
		if (stored->snr != 0)
			snprintf(snrStr, sizeof(snrStr), "%d", stored->snr);
		else
			snprintf(snrStr, sizeof(snrStr), "-");

		row->SetField(new BStringField(indexStr), kIndexColumn);
		row->SetField(new BStringField(timeStr), kTimeColumn);
		row->SetField(new BStringField(stored->typeStr), kTypeColumn);
		row->SetField(new BStringField(stored->sourceStr), kSourceColumn);
		row->SetField(new BStringField(snrStr), kSNRColumn);
		row->SetField(new BStringField(sizeStr), kSizeColumn);
		row->SetField(new BStringField(stored->summary), kSummaryColumn);

		fPacketList->AddRow(row);

		// Auto-scroll if enabled
		if (fAutoScrollCheck != NULL
			&& fAutoScrollCheck->Value() == B_CONTROL_ON) {
			fPacketList->ScrollTo(row);
		}
	}

	_UpdateStatusBar();
}


void
PacketAnalyzerWindow::Clear()
{
	fPacketList->Clear();
	fPackets.MakeEmpty();
	fPacketIndex = 0;
	fRateCount = 0;
	fRateStartTime = system_time();

	// Clear detail view
	fDetailView->SetText("");

	_UpdateStatusBar();
}


void
PacketAnalyzerWindow::_BuildMenuBar()
{
	fMenuBar = new BMenuBar("menubar");

	// File menu
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("Export CSV" B_UTF8_ELLIPSIS,
		new BMessage(MSG_PA_EXPORT), 'E'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED),
		'W'));
	fMenuBar->AddItem(fileMenu);

	// Capture menu
	BMenu* captureMenu = new BMenu("Capture");
	captureMenu->AddItem(new BMenuItem("Start/Stop",
		new BMessage(MSG_PA_START_STOP), 'R'));
	captureMenu->AddItem(new BMenuItem("Clear",
		new BMessage(MSG_PA_CLEAR), 'K'));
	fMenuBar->AddItem(captureMenu);
}


void
PacketAnalyzerWindow::_BuildUI()
{
	// Toolbar
	BView* toolBar = new BView("toolbar", B_WILL_DRAW);
	toolBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	toolBar->SetExplicitMinSize(BSize(B_SIZE_UNSET, 32));
	toolBar->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 32));

	fStartStopButton = new BButton("startstop", "Stop",
		new BMessage(MSG_PA_START_STOP));
	fStartStopButton->SetExplicitMinSize(BSize(60, 24));

	fClearButton = new BButton("clear", "Clear",
		new BMessage(MSG_PA_CLEAR));
	fClearButton->SetExplicitMinSize(BSize(60, 24));

	// Filter dropdown
	fFilterMenu = new BPopUpMenu("All Packets");
	fFilterMenu->AddItem(new BMenuItem("All Packets",
		new BMessage(MSG_PA_FILTER_ALL)));
	fFilterMenu->AddSeparatorItem();
	fFilterMenu->AddItem(new BMenuItem("Messages",
		new BMessage(MSG_PA_FILTER_MSG)));
	fFilterMenu->AddItem(new BMenuItem("Adverts",
		new BMessage(MSG_PA_FILTER_ADVERT)));
	fFilterMenu->AddItem(new BMenuItem("Raw Radio",
		new BMessage(MSG_PA_FILTER_PUSH)));
	fFilterMenu->AddItem(new BMenuItem("Responses",
		new BMessage(MSG_PA_FILTER_RSP)));
	fFilterMenu->SetTargetForItems(this);
	fFilterMenu->ItemAt(0)->SetMarked(true);

	fFilterField = new BMenuField("filter", "Filter:", fFilterMenu);
	fFilterField->SetExplicitMinSize(BSize(140, 24));

	// Search field
	fSearchField = new BTextControl("search", "Find:", "",
		new BMessage(MSG_PA_SEARCH));
	fSearchField->SetExplicitMinSize(BSize(120, 24));

	// Auto-scroll checkbox
	fAutoScrollCheck = new BCheckBox("autoscroll", "Auto-scroll", NULL);
	fAutoScrollCheck->SetValue(B_CONTROL_ON);

	BLayoutBuilder::Group<>(toolBar, B_HORIZONTAL, 4)
		.SetInsets(4, 2, 4, 2)
		.Add(fStartStopButton)
		.Add(fClearButton)
		.Add(fFilterField)
		.Add(fSearchField)
		.AddGlue()
		.Add(fAutoScrollCheck)
	.End();

	// Packet list (BColumnListView)
	fPacketList = new BColumnListView("packetlist", 0, B_FANCY_BORDER);
	fPacketList->SetSelectionMessage(new BMessage(MSG_PA_ROW_SELECTED));
	fPacketList->SetInvocationMessage(new BMessage(MSG_PA_ROW_SELECTED));

	// Add columns
	fPacketList->AddColumn(new BStringColumn("#", 50, 30, 80,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kIndexColumn);
	fPacketList->AddColumn(new BStringColumn("Time", 70, 60, 100,
		B_TRUNCATE_END), kTimeColumn);
	fPacketList->AddColumn(new BStringColumn("Type", 110, 80, 160,
		B_TRUNCATE_END), kTypeColumn);
	fPacketList->AddColumn(new BStringColumn("Source", 90, 60, 130,
		B_TRUNCATE_END), kSourceColumn);
	fPacketList->AddColumn(new BStringColumn("SNR", 45, 35, 60,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kSNRColumn);
	fPacketList->AddColumn(new BStringColumn("Size", 45, 35, 60,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kSizeColumn);
	fPacketList->AddColumn(new BStringColumn("Summary", 250, 100, 600,
		B_TRUNCATE_END), kSummaryColumn);

	// Detail view (hex dump + decoded fields)
	fDetailView = new BTextView("detail");
	fDetailView->SetViewColor(DetailBgColor());
	fDetailView->MakeEditable(false);
	fDetailView->SetStylable(true);
	fDetailView->SetWordWrap(false);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);
	rgb_color detailColor = DetailTextColor();
	fDetailView->SetFontAndColor(&monoFont, B_FONT_ALL, &detailColor);

	BScrollView* detailScroll = new BScrollView("detailscroll", fDetailView,
		0, true, true);

	// Split view: packet list on top, detail on bottom
	fSplitView = new BSplitView(B_VERTICAL);

	// Status bar
	BView* statusBar = new BView("statusbar", B_WILL_DRAW);
	statusBar->SetViewColor(StatusBgColor());
	statusBar->SetExplicitMinSize(BSize(B_SIZE_UNSET, 18));
	statusBar->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 18));

	fStatusTotal = new BStringView("total", "Total: 0");
	fStatusFiltered = new BStringView("filtered", "Shown: 0");
	fStatusRate = new BStringView("rate", "Rate: 0 pkt/s");

	BFont smallFont(be_plain_font);
	smallFont.SetSize(10);
	fStatusTotal->SetFont(&smallFont);
	fStatusFiltered->SetFont(&smallFont);
	fStatusRate->SetFont(&smallFont);

	BLayoutBuilder::Group<>(statusBar, B_HORIZONTAL, 8)
		.SetInsets(6, 1, 6, 1)
		.Add(fStatusTotal)
		.Add(fStatusFiltered)
		.AddGlue()
		.Add(fStatusRate)
	.End();

	// Main layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(toolBar)
		.AddSplit(fSplitView, B_VERTICAL)
			.Add(fPacketList, 3)
			.Add(detailScroll, 1)
		.End()
		.Add(statusBar)
	.End();

	fSplitView->SetItemWeight(0, 3, true);
	fSplitView->SetItemWeight(1, 1, true);
}


/* static */ void
PacketAnalyzerWindow::_DecodePacket(CapturedPacket& packet,
	const uint8* rawData, size_t rawLength)
{
	if (rawLength < 1)
		return;

	uint8 code = rawData[0];
	packet.code = code;
	packet.payloadSize = (uint16)rawLength;
	if (rawLength <= sizeof(packet.payload))
		memcpy(packet.payload, rawData, rawLength);
	else
		memcpy(packet.payload, rawData, sizeof(packet.payload));

	// Set type name
	strlcpy(packet.typeStr, _PacketTypeName(code), sizeof(packet.typeStr));

	// Decode based on packet type
	switch (code) {
		case RSP_CONTACT_MSG_RECV_V3:
		case RSP_CONTACT_MSG_RECV:
		{
			// DM received
			bool isV3 = (code == RSP_CONTACT_MSG_RECV_V3);
			size_t minLen = isV3 ? 12 : 9;
			if (rawLength >= minLen) {
				int snrOffset = isV3 ? 1 : -1;
				int keyOffset = isV3 ? 4 : 1;

				if (isV3 && rawLength > 1)
					packet.snr = (int8)rawData[snrOffset];

				// Source = first 6 bytes of pubkey
				if (rawLength > (size_t)(keyOffset + 5)) {
					snprintf(packet.sourceStr, sizeof(packet.sourceStr),
						"%02X%02X%02X",
						rawData[keyOffset], rawData[keyOffset + 1],
						rawData[keyOffset + 2]);
				}

				// Extract text preview
				size_t textOff = isV3 ? 12 : 9;
				uint8 txtType = isV3 ? rawData[10] : rawData[8];
				if (textOff < rawLength) {
					size_t textLen = rawLength - textOff;
					if (textLen > sizeof(packet.summary) - 20)
						textLen = sizeof(packet.summary) - 20;
					if (txtType == 0) {
						snprintf(packet.summary, sizeof(packet.summary),
							"DM: \"%.*s\"", (int)textLen,
							(const char*)(rawData + textOff));
					} else {
						snprintf(packet.summary, sizeof(packet.summary),
							"DM (type=%u): %zu bytes", txtType, textLen);
					}
				}
			}
			break;
		}

		case RSP_CHANNEL_MSG_RECV_V3:
		case RSP_CHANNEL_MSG_RECV:
		{
			// Channel message
			bool isV3 = (code == RSP_CHANNEL_MSG_RECV_V3);
			if (isV3 && rawLength > 1)
				packet.snr = (int8)rawData[1];

			size_t textOff = isV3 ? 12 : 9;
			if (textOff < rawLength) {
				size_t textLen = rawLength - textOff;
				if (textLen > sizeof(packet.summary) - 20)
					textLen = sizeof(packet.summary) - 20;
				snprintf(packet.summary, sizeof(packet.summary),
					"CH: \"%.*s\"", (int)textLen,
					(const char*)(rawData + textOff));
			}
			break;
		}

		case PUSH_ADVERT:
		case PUSH_NEW_ADVERT:
		{
			strlcpy(packet.typeStr,
				code == PUSH_NEW_ADVERT ? "NEW_ADVERT" : "ADVERT",
				sizeof(packet.typeStr));
			// Advert: pubkey at [1-6], name further in
			if (rawLength >= 7) {
				snprintf(packet.sourceStr, sizeof(packet.sourceStr),
					"%02X%02X%02X",
					rawData[1], rawData[2], rawData[3]);
			}
			// Try to extract name from advert
			if (rawLength >= 40) {
				size_t nameOff = 7;	// After pubkey prefix + type
				size_t nameLen = rawLength - nameOff;
				if (nameLen > sizeof(packet.summary) - 10)
					nameLen = sizeof(packet.summary) - 10;
				// Look for printable name
				bool hasPrintable = false;
				for (size_t i = nameOff; i < nameOff + nameLen && i < rawLength; i++) {
					if (rawData[i] >= 0x20 && rawData[i] < 0x7F) {
						hasPrintable = true;
						break;
					}
				}
				if (hasPrintable) {
					snprintf(packet.summary, sizeof(packet.summary),
						"Advert from node");
				}
			}
			if (packet.summary[0] == '\0')
				strlcpy(packet.summary, "Node advertisement",
					sizeof(packet.summary));
			break;
		}

		case PUSH_RAW_RADIO_PACKET:
		{
			if (rawLength >= 5) {
				uint8 seqLo = rawData[1];
				uint8 counter = rawData[2];
				uint8 payloadLen = rawData[3];
				uint8 flags = rawData[4];
				snprintf(packet.summary, sizeof(packet.summary),
					"Raw: seq=%u len=%u flags=0x%02X",
					seqLo | (counter << 8), payloadLen, flags);
			} else {
				strlcpy(packet.summary, "Raw radio packet",
					sizeof(packet.summary));
			}
			break;
		}

		case PUSH_TRACE_DATA:
		{
			if (rawLength >= 12) {
				uint8 pathLen = rawData[2];
				snprintf(packet.summary, sizeof(packet.summary),
					"Trace: %u hops", pathLen);
			} else {
				strlcpy(packet.summary, "Trace path data",
					sizeof(packet.summary));
			}
			break;
		}

		case PUSH_TELEMETRY_RESPONSE:
		{
			strlcpy(packet.summary, "Telemetry response",
				sizeof(packet.summary));
			if (rawLength >= 7) {
				snprintf(packet.sourceStr, sizeof(packet.sourceStr),
					"%02X%02X%02X",
					rawData[1], rawData[2], rawData[3]);
			}
			break;
		}

		case PUSH_SEND_CONFIRMED:
			strlcpy(packet.summary, "Delivery confirmed",
				sizeof(packet.summary));
			break;

		case PUSH_PATH_UPDATED:
			strlcpy(packet.summary, "Path updated", sizeof(packet.summary));
			break;

		case PUSH_MSG_WAITING:
			strlcpy(packet.summary, "Messages waiting on device",
				sizeof(packet.summary));
			break;

		case PUSH_LOGIN_SUCCESS:
			strlcpy(packet.summary, "Login successful",
				sizeof(packet.summary));
			break;

		case PUSH_LOGIN_FAIL:
			strlcpy(packet.summary, "Login failed", sizeof(packet.summary));
			break;

		case RSP_SELF_INFO:
		{
			strlcpy(packet.summary, "Self info (pubkey + radio params)",
				sizeof(packet.summary));
			if (rawLength >= 36) {
				snprintf(packet.sourceStr, sizeof(packet.sourceStr),
					"%02X%02X%02X",
					rawData[4], rawData[5], rawData[6]);
			}
			break;
		}

		case RSP_CONTACTS_START:
			strlcpy(packet.summary, "Contact sync started",
				sizeof(packet.summary));
			break;

		case RSP_END_OF_CONTACTS:
			strlcpy(packet.summary, "Contact sync complete",
				sizeof(packet.summary));
			break;

		case RSP_CONTACT:
		{
			if (rawLength >= 33) {
				snprintf(packet.sourceStr, sizeof(packet.sourceStr),
					"%02X%02X%02X",
					rawData[1], rawData[2], rawData[3]);
			}
			// Try to extract contact name (offset 100-131)
			if (rawLength >= 132) {
				char name[33];
				memset(name, 0, sizeof(name));
				memcpy(name, rawData + 100, 32);
				name[32] = '\0';
				if (name[0] >= 0x20 && name[0] < 0x7F) {
					snprintf(packet.summary, sizeof(packet.summary),
						"Contact: %s", name);
				} else {
					strlcpy(packet.summary, "Contact data",
						sizeof(packet.summary));
				}
			} else {
				strlcpy(packet.summary, "Contact data",
					sizeof(packet.summary));
			}
			break;
		}

		case RSP_DEVICE_INFO:
			strlcpy(packet.summary, "Device info", sizeof(packet.summary));
			break;

		case RSP_BATT_AND_STORAGE:
		{
			if (rawLength >= 3) {
				uint16 battMv = rawData[1] | (rawData[2] << 8);
				snprintf(packet.summary, sizeof(packet.summary),
					"Battery: %u mV", battMv);
			} else {
				strlcpy(packet.summary, "Battery & storage info",
					sizeof(packet.summary));
			}
			break;
		}

		case RSP_STATS:
			strlcpy(packet.summary, "Statistics response",
				sizeof(packet.summary));
			break;

		case RSP_SENT:
			strlcpy(packet.summary, "Message sent OK", sizeof(packet.summary));
			break;

		case RSP_OK:
			strlcpy(packet.summary, "OK", sizeof(packet.summary));
			break;

		case RSP_ERR:
		{
			if (rawLength >= 2) {
				if (rawData[1] <= 3) {
					snprintf(packet.summary, sizeof(packet.summary),
						"APP_START ACK (proto v%u)", rawData[1]);
				} else {
					snprintf(packet.summary, sizeof(packet.summary),
						"Error: code %u", rawData[1]);
				}
			} else {
				strlcpy(packet.summary, "Error", sizeof(packet.summary));
			}
			break;
		}

		default:
		{
			if (code >= 0x80) {
				snprintf(packet.summary, sizeof(packet.summary),
					"Push notification (0x%02X): %zu bytes", code, rawLength);
			} else {
				snprintf(packet.summary, sizeof(packet.summary),
					"Response (0x%02X): %zu bytes", code, rawLength);
			}
			break;
		}
	}
}


/* static */ const char*
PacketAnalyzerWindow::_PacketTypeName(uint8 code)
{
	switch (code) {
		case RSP_OK:					return "OK";
		case RSP_ERR:					return "ERR/APP_ACK";
		case RSP_CONTACTS_START:		return "CONTACTS_START";
		case RSP_CONTACT:				return "CONTACT";
		case RSP_END_OF_CONTACTS:		return "END_CONTACTS";
		case RSP_SELF_INFO:				return "SELF_INFO";
		case RSP_SENT:					return "SENT";
		case RSP_CONTACT_MSG_RECV:		return "DM (V2)";
		case RSP_CHANNEL_MSG_RECV:		return "CHANNEL (V2)";
		case RSP_CONTACT_MSG_RECV_V3:	return "DM (V3)";
		case RSP_CHANNEL_MSG_RECV_V3:	return "CHANNEL (V3)";
		case RSP_DEVICE_INFO:			return "DEVICE_INFO";
		case RSP_EXPORT_CONTACT:		return "EXPORT_CONTACT";
		case RSP_BATT_AND_STORAGE:		return "BATT_STORAGE";
		case RSP_STATS:					return "STATS";
		case RSP_CUSTOM_VARS:			return "CUSTOM_VARS";
		case RSP_ADVERT_PATH:			return "ADVERT_PATH";

		case PUSH_ADVERT:				return "ADVERT";
		case PUSH_PATH_UPDATED:			return "PATH_UPDATE";
		case PUSH_SEND_CONFIRMED:		return "CONFIRMED";
		case PUSH_MSG_WAITING:			return "MSG_WAITING";
		case PUSH_RAW_DATA:				return "RAW_DATA";
		case PUSH_LOGIN_SUCCESS:		return "LOGIN_OK";
		case PUSH_LOGIN_FAIL:			return "LOGIN_FAIL";
		case PUSH_STATUS_RESPONSE:		return "STATUS_RSP";
		case PUSH_RAW_RADIO_PACKET:		return "RAW_RADIO";
		case PUSH_TRACE_DATA:			return "TRACE_DATA";
		case PUSH_NEW_ADVERT:			return "NEW_ADVERT";
		case PUSH_TELEMETRY_RESPONSE:	return "TELEMETRY";
		case PUSH_BINARY_RESPONSE:		return "BINARY_RSP";
		case PUSH_CONTROL_DATA:			return "CONTROL_DATA";

		default:
		{
			static char buf[16];
			snprintf(buf, sizeof(buf), "0x%02X", code);
			return buf;
		}
	}
}


void
PacketAnalyzerWindow::_UpdatePacketDetail(int32 index)
{
	// Find the actual packet data for this row
	// We need to map the filtered row index back to fPackets
	BRow* row = fPacketList->RowAt(index);
	if (row == NULL)
		return;

	// Get the packet index from column 0 ("#")
	BStringField* indexField = static_cast<BStringField*>(
		row->GetField(kIndexColumn));
	if (indexField == NULL)
		return;

	uint32 pktIdx = atoi(indexField->String());

	// Find packet with matching index
	CapturedPacket* packet = NULL;
	for (int32 i = 0; i < fPackets.CountItems(); i++) {
		if (fPackets.ItemAt(i)->index == pktIdx) {
			packet = fPackets.ItemAt(i);
			break;
		}
	}

	if (packet == NULL)
		return;

	// Build detail text
	BString detail;

	// Decoded fields
	detail << "--- Packet #" << packet->index << " ---\n";
	detail << "Type:      " << packet->typeStr;
	detail << " (0x" ;
	char codeBuf[8];
	snprintf(codeBuf, sizeof(codeBuf), "%02X", packet->code);
	detail << codeBuf << ")\n";

	char timeBuf[32];
	time_t t = (time_t)packet->timestamp;
	struct tm tm;
	if (localtime_r(&t, &tm) != NULL)
		strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);
	else
		snprintf(timeBuf, sizeof(timeBuf), "unknown");
	detail << "Time:      " << timeBuf << "\n";

	if (packet->sourceStr[0] != '\0')
		detail << "Source:    " << packet->sourceStr << "\n";
	if (packet->snr != 0) {
		char snrBuf[16];
		snprintf(snrBuf, sizeof(snrBuf), "%d dB", packet->snr);
		detail << "SNR:       " << snrBuf << "\n";
	}
	detail << "Size:      " << packet->payloadSize << " bytes\n";
	if (packet->summary[0] != '\0')
		detail << "Summary:   " << packet->summary << "\n";

	detail << "\n--- Hex Dump ---\n";

	BString hexDump;
	_FormatHexDump(packet->payload, packet->payloadSize, hexDump);
	detail << hexDump;

	fDetailView->SetText(detail.String());

	// Apply colors to the detail view
	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);

	// Header section in normal text color
	rgb_color textColor = DetailTextColor();
	fDetailView->SetFontAndColor(0, detail.Length(), &monoFont,
		B_FONT_ALL, &textColor);

	// Find hex dump section and color it
	int32 hexStart = detail.FindFirst("--- Hex Dump ---");
	if (hexStart >= 0) {
		rgb_color headerColor = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		fDetailView->SetFontAndColor(hexStart, hexStart + 16, &monoFont,
			B_FONT_ALL, &headerColor);
	}
}


void
PacketAnalyzerWindow::_FormatHexDump(const uint8* data, size_t length,
	BString& output)
{
	output = "";

	for (size_t offset = 0; offset < length; offset += 16) {
		// Offset
		char line[128];
		snprintf(line, sizeof(line), "%04zX  ", offset);
		output << line;

		// Hex bytes
		for (size_t i = 0; i < 16; i++) {
			if (offset + i < length) {
				snprintf(line, sizeof(line), "%02X ", data[offset + i]);
				output << line;
			} else {
				output << "   ";
			}
			if (i == 7)
				output << " ";
		}

		output << " |";

		// ASCII representation
		for (size_t i = 0; i < 16 && (offset + i) < length; i++) {
			uint8 c = data[offset + i];
			if (c >= 0x20 && c < 0x7F)
				output << (char)c;
			else
				output << '.';
		}

		output << "|\n";
	}
}


void
PacketAnalyzerWindow::_UpdateStatusBar()
{
	BString totalStr;
	totalStr.SetToFormat("Total: %d", (int)fPackets.CountItems());
	fStatusTotal->SetText(totalStr.String());

	BString filteredStr;
	filteredStr.SetToFormat("Shown: %d",
		(int)fPacketList->CountRows());
	fStatusFiltered->SetText(filteredStr.String());

	// Calculate rate
	if (fRateStartTime > 0) {
		double elapsed = (double)(system_time() - fRateStartTime) / 1000000.0;
		if (elapsed > 0.5) {
			double rate = (double)fRateCount / elapsed;
			BString rateStr;
			rateStr.SetToFormat("Rate: %.1f pkt/s", rate);
			fStatusRate->SetText(rateStr.String());
		}
	}
}


bool
PacketAnalyzerWindow::_MatchesFilter(const CapturedPacket& packet)
{
	// Type filter
	if (fFilterType >= 0) {
		switch (fFilterType) {
			case 0x10:	// Messages
				if (packet.code != RSP_CONTACT_MSG_RECV
					&& packet.code != RSP_CONTACT_MSG_RECV_V3
					&& packet.code != RSP_CHANNEL_MSG_RECV
					&& packet.code != RSP_CHANNEL_MSG_RECV_V3)
					return false;
				break;
			case 0x80:	// Adverts
				if (packet.code != PUSH_ADVERT
					&& packet.code != PUSH_NEW_ADVERT)
					return false;
				break;
			case 0x88:	// Raw radio
				if (packet.code != PUSH_RAW_RADIO_PACKET)
					return false;
				break;
			case 0x00:	// Responses
				if (packet.code >= 0x80)
					return false;	// Push notifications excluded
				if (packet.code == RSP_CONTACT_MSG_RECV
					|| packet.code == RSP_CONTACT_MSG_RECV_V3
					|| packet.code == RSP_CHANNEL_MSG_RECV
					|| packet.code == RSP_CHANNEL_MSG_RECV_V3)
					return false;	// Messages excluded
				break;
		}
	}

	// Text search
	if (fSearchText.Length() > 0) {
		BString typeStr(packet.typeStr);
		BString srcStr(packet.sourceStr);
		BString sumStr(packet.summary);

		// Case-insensitive search
		typeStr.ToLower();
		srcStr.ToLower();
		sumStr.ToLower();

		BString needle(fSearchText);
		needle.ToLower();

		if (typeStr.FindFirst(needle) < 0
			&& srcStr.FindFirst(needle) < 0
			&& sumStr.FindFirst(needle) < 0)
			return false;
	}

	return true;
}


void
PacketAnalyzerWindow::_RebuildFilteredList()
{
	fPacketList->Clear();

	for (int32 i = 0; i < fPackets.CountItems(); i++) {
		CapturedPacket* packet = fPackets.ItemAt(i);
		if (!_MatchesFilter(*packet))
			continue;

		BRow* row = new BRow();

		char indexStr[12];
		snprintf(indexStr, sizeof(indexStr), "%u", packet->index);

		char timeStr[16];
		time_t t = (time_t)packet->timestamp;
		struct tm tm;
		if (localtime_r(&t, &tm) != NULL)
			strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm);
		else
			snprintf(timeStr, sizeof(timeStr), "--:--:--");

		char sizeStr[12];
		snprintf(sizeStr, sizeof(sizeStr), "%u", packet->payloadSize);

		char snrStr[12];
		if (packet->snr != 0)
			snprintf(snrStr, sizeof(snrStr), "%d", packet->snr);
		else
			snprintf(snrStr, sizeof(snrStr), "-");

		row->SetField(new BStringField(indexStr), kIndexColumn);
		row->SetField(new BStringField(timeStr), kTimeColumn);
		row->SetField(new BStringField(packet->typeStr), kTypeColumn);
		row->SetField(new BStringField(packet->sourceStr), kSourceColumn);
		row->SetField(new BStringField(snrStr), kSNRColumn);
		row->SetField(new BStringField(sizeStr), kSizeColumn);
		row->SetField(new BStringField(packet->summary), kSummaryColumn);

		fPacketList->AddRow(row);
	}

	_UpdateStatusBar();
}


void
PacketAnalyzerWindow::_ExportCSV(const char* path)
{
	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK) {
		fprintf(stderr, "PacketAnalyzer: Failed to create %s\n", path);
		return;
	}

	// CSV header
	BString header("Index,Timestamp,Type,Code,Source,SNR,Size,Summary\n");
	file.Write(header.String(), header.Length());

	for (int32 i = 0; i < fPackets.CountItems(); i++) {
		CapturedPacket* pkt = fPackets.ItemAt(i);

		char timeBuf[32];
		time_t t = (time_t)pkt->timestamp;
		struct tm tm;
		if (localtime_r(&t, &tm) != NULL)
			strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);
		else
			snprintf(timeBuf, sizeof(timeBuf), "unknown");

		// Escape summary for CSV (double quotes)
		BString escapedSummary(pkt->summary);
		escapedSummary.ReplaceAll("\"", "\"\"");

		BString line;
		line.SetToFormat("%u,%s,%s,0x%02X,%s,%d,%u,\"%s\"\n",
			pkt->index, timeBuf, pkt->typeStr, pkt->code,
			pkt->sourceStr, pkt->snr, pkt->payloadSize,
			escapedSummary.String());
		file.Write(line.String(), line.Length());
	}

	fprintf(stderr, "PacketAnalyzer: Exported %d packets to %s\n",
		(int)fPackets.CountItems(), path);
}
