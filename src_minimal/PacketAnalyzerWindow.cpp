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
#include <TabView.h>
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
	MSG_PA_CONTACT_CLICKED = 'pacc',
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

static inline rgb_color SectionHeaderColor()
{
	return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
}


// #pragma mark - ColorStringField


ColorStringField::ColorStringField(const char* string, rgb_color color)
	:
	BStringField(string),
	fColor(color)
{
}


// #pragma mark - ColorStringColumn


ColorStringColumn::ColorStringColumn(const char* title, float width,
	float minWidth, float maxWidth, uint32 truncate, alignment align)
	:
	BStringColumn(title, width, minWidth, maxWidth, truncate, align)
{
}


void
ColorStringColumn::DrawField(BField* field, BRect rect, BView* parent)
{
	ColorStringField* colorField = dynamic_cast<ColorStringField*>(field);
	if (colorField != NULL) {
		rgb_color savedColor = parent->HighColor();
		parent->SetHighColor(colorField->Color());
		BStringColumn::DrawField(field, rect, parent);
		parent->SetHighColor(savedColor);
	} else {
		BStringColumn::DrawField(field, rect, parent);
	}
}


bool
ColorStringColumn::AcceptsField(const BField* field) const
{
	return dynamic_cast<const ColorStringField*>(field) != NULL
		|| dynamic_cast<const BStringField*>(field) != NULL;
}


// #pragma mark - SNRTrendView


static const int32 kMaxSNRPoints = 200;


class SNRTrendView : public BView {
public:
	SNRTrendView()
		:
		BView("snrtrend", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fPointCount(0)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 80));
		SetExplicitPreferredSize(BSize(B_SIZE_UNLIMITED, 80));
	}

	void AddPoint(int8 snr, bigtime_t timestamp)
	{
		if (fPointCount < kMaxSNRPoints) {
			fSNRValues[fPointCount] = snr;
			fTimestamps[fPointCount] = timestamp;
			fPointCount++;
		} else {
			// Shift left
			memmove(fSNRValues, fSNRValues + 1,
				(kMaxSNRPoints - 1) * sizeof(int8));
			memmove(fTimestamps, fTimestamps + 1,
				(kMaxSNRPoints - 1) * sizeof(bigtime_t));
			fSNRValues[kMaxSNRPoints - 1] = snr;
			fTimestamps[kMaxSNRPoints - 1] = timestamp;
		}
		Invalidate();
	}

	void Clear()
	{
		fPointCount = 0;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		float w = bounds.Width();
		float h = bounds.Height();

		// Background zones: green (>5), yellow (-5..5), red (<-10)
		float snrMax = 20.0f;
		float snrMin = -20.0f;
		float snrRange = snrMax - snrMin;

		// Zone boundaries in pixel Y (inverted: top = max)
		float yGoodTop = 0;
		float yGoodBot = h * (snrMax - 5.0f) / snrRange;
		float yFairBot = h * (snrMax - 0.0f) / snrRange;
		float yPoorBot = h * (snrMax - (-5.0f)) / snrRange;
		float yBadBot = h * (snrMax - (-10.0f)) / snrRange;

		// Draw background zones with very subtle tints
		rgb_color bgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
		bool isDark = bgColor.Brightness() < 128;
		float tintAmount = isDark ? 1.05f : 0.95f;

		rgb_color greenZone = tint_color(
			(rgb_color){200, 255, 200, 255}, tintAmount);
		rgb_color yellowZone = tint_color(
			(rgb_color){255, 255, 200, 255}, tintAmount);
		rgb_color redZone = tint_color(
			(rgb_color){255, 220, 220, 255}, tintAmount);

		if (isDark) {
			greenZone = tint_color(
				(rgb_color){20, 50, 20, 255}, 1.0f);
			yellowZone = tint_color(
				(rgb_color){50, 50, 10, 255}, 1.0f);
			redZone = tint_color(
				(rgb_color){50, 15, 15, 255}, 1.0f);
		}

		SetHighColor(greenZone);
		FillRect(BRect(0, yGoodTop, w, yGoodBot));
		SetHighColor(yellowZone);
		FillRect(BRect(0, yFairBot, w, yPoorBot));
		SetHighColor(redZone);
		FillRect(BRect(0, yBadBot, w, h));

		// Zero line
		float yZero = h * snrMax / snrRange;
		SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
			B_LIGHTEN_2_TINT));
		StrokeLine(BPoint(0, yZero), BPoint(w, yZero));

		// Draw 5 dB grid lines
		SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
			B_LIGHTEN_2_TINT));
		for (int snr = -15; snr <= 15; snr += 5) {
			if (snr == 0) continue;
			float y = h * (snrMax - snr) / snrRange;
			StrokeLine(BPoint(0, y), BPoint(w, y),
				B_MIXED_COLORS);
		}

		// Y-axis labels
		BFont labelFont(be_plain_font);
		labelFont.SetSize(9);
		SetFont(&labelFont);
		SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
			B_LIGHTEN_1_TINT));
		for (int snr = -15; snr <= 15; snr += 5) {
			float y = h * (snrMax - snr) / snrRange;
			char label[8];
			snprintf(label, sizeof(label), "%d", snr);
			DrawString(label, BPoint(2, y - 2));
		}

		if (fPointCount < 2) {
			// Draw placeholder text
			SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
				B_LIGHTEN_1_TINT));
			BFont font(be_plain_font);
			font.SetSize(10);
			SetFont(&font);
			DrawString("SNR Trend (waiting for data...)",
				BPoint(w / 2 - 80, h / 2));
			return;
		}

		// Draw line chart
		float xStep = w / (float)(kMaxSNRPoints - 1);

		SetPenSize(1.5f);
		for (int32 i = 1; i < fPointCount; i++) {
			float x1 = (i - 1 + (kMaxSNRPoints - fPointCount)) * xStep;
			float x2 = (i + (kMaxSNRPoints - fPointCount)) * xStep;
			float y1 = h * (snrMax - fSNRValues[i - 1]) / snrRange;
			float y2 = h * (snrMax - fSNRValues[i]) / snrRange;

			// Color segment by SNR quality
			int8 snr = fSNRValues[i];
			rgb_color lineColor;
			if (snr > 5)
				lineColor = (rgb_color){50, 205, 50, 255};
			else if (snr > 0)
				lineColor = (rgb_color){100, 200, 100, 255};
			else if (snr > -5)
				lineColor = (rgb_color){255, 193, 37, 255};
			else if (snr > -10)
				lineColor = (rgb_color){255, 140, 0, 255};
			else
				lineColor = (rgb_color){220, 20, 60, 255};

			SetHighColor(lineColor);
			StrokeLine(BPoint(x1, y1), BPoint(x2, y2));
		}

		// Draw dots at data points
		SetPenSize(1.0f);
		for (int32 i = 0; i < fPointCount; i++) {
			float x = (i + (kMaxSNRPoints - fPointCount)) * xStep;
			float y = h * (snrMax - fSNRValues[i]) / snrRange;

			int8 snr = fSNRValues[i];
			rgb_color dotColor;
			if (snr > 5)
				dotColor = (rgb_color){50, 205, 50, 255};
			else if (snr > 0)
				dotColor = (rgb_color){100, 200, 100, 255};
			else if (snr > -5)
				dotColor = (rgb_color){255, 193, 37, 255};
			else if (snr > -10)
				dotColor = (rgb_color){255, 140, 0, 255};
			else
				dotColor = (rgb_color){220, 20, 60, 255};

			SetHighColor(dotColor);
			FillEllipse(BPoint(x, y), 2.5f, 2.5f);
		}

		// Title with current SNR value
		SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
		BFont titleFont(be_plain_font);
		titleFont.SetSize(10);
		titleFont.SetFace(B_BOLD_FACE);
		SetFont(&titleFont);

		int8 lastSNR = fSNRValues[fPointCount - 1];
		char titleBuf[32];
		snprintf(titleBuf, sizeof(titleBuf), "SNR: %d dB", lastSNR);
		DrawString(titleBuf, BPoint(w - 70, 12));

		// Draw value label at last data point
		float lastX = (fPointCount - 1 + (kMaxSNRPoints - fPointCount))
			* xStep;
		float lastY = h * (snrMax - lastSNR) / snrRange;

		char valBuf[8];
		snprintf(valBuf, sizeof(valBuf), "%d", lastSNR);
		BFont valFont(be_plain_font);
		valFont.SetSize(9);
		SetFont(&valFont);

		// Use same color as the dot
		rgb_color valColor;
		if (lastSNR > 5)
			valColor = (rgb_color){50, 205, 50, 255};
		else if (lastSNR > 0)
			valColor = (rgb_color){100, 200, 100, 255};
		else if (lastSNR > -5)
			valColor = (rgb_color){255, 193, 37, 255};
		else if (lastSNR > -10)
			valColor = (rgb_color){255, 140, 0, 255};
		else
			valColor = (rgb_color){220, 20, 60, 255};

		SetHighColor(valColor);
		float valY = lastY > 12 ? lastY - 5 : lastY + 12;
		DrawString(valBuf, BPoint(lastX + 5, valY));
	}

private:
	int8		fSNRValues[kMaxSNRPoints];
	bigtime_t	fTimestamps[kMaxSNRPoints];
	int32		fPointCount;
};


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
	fSNRTrendView(NULL),
	fBottomTabView(NULL),
	fContactStatsList(NULL),
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
	fRateStartTime(system_time())
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
			fRateCount = 0;
			fRateStartTime = system_time();
			_UpdateStatusBar();
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

		case MSG_PA_CONTACT_CLICKED:
		{
			// Filter by selected contact
			BRow* row = fContactStatsList->CurrentSelection();
			if (row != NULL) {
				BStringField* nameField = static_cast<BStringField*>(
					row->GetField(0));
				if (nameField != NULL) {
					fSearchText = nameField->String();
					fSearchField->SetText(fSearchText.String());
					_RebuildFilteredList();
					// Switch to packet detail tab to see results
					fBottomTabView->Select(0);
				}
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

	// Calculate delta-t from previous packet
	bigtime_t deltaUs = -1;
	int32 prevIdx = fPackets.CountItems() - 1;
	if (prevIdx >= 0) {
		CapturedPacket* prev = fPackets.ItemAt(prevIdx);
		if (prev != NULL && stored->captureTime > 0 && prev->captureTime > 0)
			deltaUs = stored->captureTime - prev->captureTime;
	}

	fPackets.AddItem(stored);

	// Feed SNR trend chart
	if (stored->snr != 0 && fSNRTrendView != NULL)
		fSNRTrendView->AddPoint(stored->snr, stored->captureTime);

	// Update contact stats periodically (every 10 packets)
	if (fPackets.CountItems() % 10 == 0)
		_UpdateContactStats();

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

		char deltaStr[24];
		_FormatDelta(deltaUs, deltaStr, sizeof(deltaStr));

		char sizeStr[12];
		snprintf(sizeStr, sizeof(sizeStr), "%u", stored->payloadSize);

		char snrStr[12];
		if (stored->snr != 0)
			snprintf(snrStr, sizeof(snrStr), "%d", stored->snr);
		else
			snprintf(snrStr, sizeof(snrStr), "-");

		row->SetField(new BStringField(indexStr), kIndexColumn);
		row->SetField(new BStringField(timeStr), kTimeColumn);
		row->SetField(new ColorStringField(deltaStr,
			_DeltaColor(deltaUs)), kDeltaColumn);
		row->SetField(new ColorStringField(stored->typeStr,
			_PacketCategoryColor(stored->code)), kTypeColumn);
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

	// Clear detail view, SNR trend, and contact stats
	fDetailView->SetText("");
	if (fSNRTrendView != NULL)
		fSNRTrendView->Clear();
	if (fContactStatsList != NULL)
		fContactStatsList->Clear();

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
	fPacketList->AddColumn(new ColorStringColumn(B_UTF8_OPEN_QUOTE "t",
		65, 45, 100, B_TRUNCATE_END, B_ALIGN_RIGHT), kDeltaColumn);
	fPacketList->AddColumn(new ColorStringColumn("Type", 110, 80, 160,
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

	// SNR trend chart
	fSNRTrendView = new SNRTrendView();

	// Bottom pane: detail + SNR trend side by side (Tab 1)
	BSplitView* bottomSplit = new BSplitView(B_HORIZONTAL);
	BLayoutBuilder::Split<>(bottomSplit)
		.Add(detailScroll, 3)
		.Add(fSNRTrendView, 1)
	.End();

	// Contact stats list (Tab 2)
	fContactStatsList = new BColumnListView("contactstats", 0,
		B_FANCY_BORDER);
	fContactStatsList->SetSelectionMessage(
		new BMessage(MSG_PA_CONTACT_CLICKED));

	// Contact stats columns
	enum {
		kCSContactCol = 0,
		kCSPacketsCol,
		kCSAvgSNRCol,
		kCSMinSNRCol,
		kCSMaxSNRCol,
		kCSLastSeenCol
	};

	fContactStatsList->AddColumn(new BStringColumn("Contact", 130, 80, 200,
		B_TRUNCATE_END), kCSContactCol);
	fContactStatsList->AddColumn(new BStringColumn("Packets", 60, 40, 80,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kCSPacketsCol);
	fContactStatsList->AddColumn(new ColorStringColumn("Avg SNR", 65, 45, 80,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kCSAvgSNRCol);
	fContactStatsList->AddColumn(new BStringColumn("Min", 45, 35, 60,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kCSMinSNRCol);
	fContactStatsList->AddColumn(new BStringColumn("Max", 45, 35, 60,
		B_TRUNCATE_END, B_ALIGN_RIGHT), kCSMaxSNRCol);
	fContactStatsList->AddColumn(new BStringColumn("Last Seen", 80, 60, 120,
		B_TRUNCATE_END), kCSLastSeenCol);

	// Bottom tab view
	fBottomTabView = new BTabView("bottomtabs", B_WIDTH_FROM_LABEL);

	BView* detailTab = new BView("Packet Detail", B_WILL_DRAW);
	detailTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(detailTab, B_VERTICAL, 0)
		.Add(bottomSplit)
	.End();
	fBottomTabView->AddTab(detailTab);

	BView* contactTab = new BView("Contact Stats", B_WILL_DRAW);
	contactTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(contactTab, B_VERTICAL, 0)
		.Add(fContactStatsList)
	.End();
	fBottomTabView->AddTab(contactTab);

	// Split view: packet list on top, tab view on bottom
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
			.Add(fBottomTabView, 1)
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
				int keyOffset = isV3 ? 4 : 1;

				if (isV3 && rawLength > 3) {
					packet.snr = (int8)rawData[1];
					packet.rssi = (int8)rawData[2];
					packet.pathLen = rawData[3];
				}

				// Source = first 3 bytes of pubkey as hex
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
			if (isV3 && rawLength > 3) {
				packet.snr = (int8)rawData[1];
				packet.rssi = (int8)rawData[2];
				packet.pathLen = rawData[3];
			}

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
			// Try to extract name from advert payload
			// Advert structure: [code][pubkey6][type][name...]
			if (rawLength >= 40) {
				// Find first printable run as name (starts after pubkey+type)
				size_t nameOff = 8;
				size_t nameEnd = nameOff;
				for (size_t i = nameOff; i < rawLength; i++) {
					if (rawData[i] >= 0x20 && rawData[i] < 0x7F)
						nameEnd = i + 1;
					else if (nameEnd > nameOff)
						break;	// End of printable run
				}
				if (nameEnd > nameOff) {
					size_t nameLen = nameEnd - nameOff;
					if (nameLen > 63) nameLen = 63;
					snprintf(packet.summary, sizeof(packet.summary),
						"Advert: \"%.*s\"", (int)nameLen,
						(const char*)(rawData + nameOff));
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
		{
			if (rawLength >= 4) {
				snprintf(packet.summary, sizeof(packet.summary),
					"Device: max %u contacts, %u channels",
					rawData[2] * 2, rawData[3]);
			} else {
				strlcpy(packet.summary, "Device info",
					sizeof(packet.summary));
			}
			break;
		}

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
		{
			if (rawLength >= 2) {
				uint8 statType = rawData[1];
				if (statType == 0 && rawLength >= 8) {
					// Core stats: [0]=code,[1]=type,[2-3]=battMv,[4-7]=uptime
					uint16 battMv = rawData[2] | (rawData[3] << 8);
					uint32 uptime = rawData[4] | (rawData[5] << 8)
						| (rawData[6] << 16) | (rawData[7] << 24);
					snprintf(packet.summary, sizeof(packet.summary),
						"Stats/Core: uptime=%us, batt=%umV",
						uptime, battMv);
				} else if (statType == 1 && rawLength >= 6) {
					// Radio stats: [2-3]=noiseFloor,[4]=rssi,[5]=snr
					int16 noiseFloor = (int16)(rawData[2]
						| (rawData[3] << 8));
					packet.rssi = (int8)rawData[4];
					packet.snr = (int8)rawData[5];
					snprintf(packet.summary, sizeof(packet.summary),
						"Stats/Radio: noise=%ddBm, rssi=%ddBm, snr=%ddB",
						noiseFloor, packet.rssi, packet.snr);
				} else if (statType == 2 && rawLength >= 10) {
					// Packet stats: [2-5]=recvPkts,[6-9]=sentPkts
					uint32 recv = rawData[2] | (rawData[3] << 8)
						| (rawData[4] << 16) | (rawData[5] << 24);
					uint32 sent = rawData[6] | (rawData[7] << 8)
						| (rawData[8] << 16) | (rawData[9] << 24);
					snprintf(packet.summary, sizeof(packet.summary),
						"Stats/Packets: recv=%u, sent=%u", recv, sent);
				} else {
					snprintf(packet.summary, sizeof(packet.summary),
						"Stats (type=%u)", statType);
				}
			} else {
				strlcpy(packet.summary, "Statistics response",
					sizeof(packet.summary));
			}
			break;
		}

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


/* static */ rgb_color
PacketAnalyzerWindow::_PacketCategoryColor(uint8 code)
{
	rgb_color base;
	bool isDark = ui_color(B_PANEL_BACKGROUND_COLOR).Brightness() < 128;

	switch (code) {
		// Messages — steel blue
		case RSP_CONTACT_MSG_RECV:
		case RSP_CONTACT_MSG_RECV_V3:
		case RSP_CHANNEL_MSG_RECV:
		case RSP_CHANNEL_MSG_RECV_V3:
			base = (rgb_color){41, 128, 185, 255};
			if (isDark)
				base = tint_color(base, B_LIGHTEN_1_TINT);
			return base;

		// Adverts — emerald green
		case PUSH_ADVERT:
		case PUSH_NEW_ADVERT:
			base = (rgb_color){39, 174, 96, 255};
			if (isDark)
				base = tint_color(base, B_LIGHTEN_1_TINT);
			return base;

		// Alerts — amber
		case PUSH_MSG_WAITING:
		case PUSH_LOGIN_SUCCESS:
		case PUSH_LOGIN_FAIL:
		case PUSH_SEND_CONFIRMED:
		case PUSH_PATH_UPDATED:
			base = (rgb_color){211, 132, 0, 255};
			if (isDark)
				base = tint_color(base, B_LIGHTEN_1_TINT);
			return base;

		// Raw/Radio — purple
		case PUSH_RAW_RADIO_PACKET:
		case PUSH_TRACE_DATA:
		case PUSH_TELEMETRY_RESPONSE:
		case PUSH_RAW_DATA:
			base = (rgb_color){142, 68, 173, 255};
			if (isDark)
				base = tint_color(base, B_LIGHTEN_1_TINT);
			return base;

		// Sent — normal text color
		case RSP_SENT:
			return ui_color(B_LIST_ITEM_TEXT_COLOR);

		// System — dimmed text
		default:
			return tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
				B_LIGHTEN_1_TINT);
	}
}


/* static */ const char*
PacketAnalyzerWindow::_SignalQualityString(int8 snr)
{
	if (snr > 5)
		return "Excellent";
	if (snr > 0)
		return "Good";
	if (snr > -5)
		return "Fair";
	if (snr > -10)
		return "Poor";
	return "Bad";
}


/* static */ void
PacketAnalyzerWindow::_FormatDelta(bigtime_t deltaUs, char* buf, size_t bufSize)
{
	if (deltaUs < 0) {
		strlcpy(buf, "-", bufSize);
	} else if (deltaUs < 1000) {
		// Microseconds
		snprintf(buf, bufSize, "%ldus", (long)deltaUs);
	} else if (deltaUs < 1000000) {
		// Milliseconds
		snprintf(buf, bufSize, "%.1fms", deltaUs / 1000.0);
	} else if (deltaUs < 60000000LL) {
		// Seconds
		snprintf(buf, bufSize, "%.1fs", deltaUs / 1000000.0);
	} else {
		// Minutes
		snprintf(buf, bufSize, "%.0fm%02ds",
			deltaUs / 60000000.0,
			(int)((deltaUs / 1000000) % 60));
	}
}


/* static */ rgb_color
PacketAnalyzerWindow::_DeltaColor(bigtime_t deltaUs)
{
	bool isDark = ui_color(B_PANEL_BACKGROUND_COLOR).Brightness() < 128;

	if (deltaUs < 0) {
		// No delta (first packet)
		return tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
			B_LIGHTEN_1_TINT);
	}

	if (deltaUs < 100000) {
		// Burst: < 100ms — amber warning
		rgb_color c = (rgb_color){211, 132, 0, 255};
		if (isDark)
			c = tint_color(c, B_LIGHTEN_1_TINT);
		return c;
	}

	if (deltaUs > 10000000) {
		// Long gap: > 10s — red warning
		rgb_color c = (rgb_color){220, 20, 60, 255};
		if (isDark)
			c = tint_color(c, B_LIGHTEN_1_TINT);
		return c;
	}

	// Normal range — default text
	return ui_color(B_LIST_ITEM_TEXT_COLOR);
}


void
PacketAnalyzerWindow::_FormatDecodedSection(const CapturedPacket* packet,
	BString& output)
{
	output = "";

	switch (packet->code) {
		case RSP_CONTACT_MSG_RECV_V3:
		case RSP_CONTACT_MSG_RECV:
		{
			output << "\xe2\x9c\x89  Direct Message Received\n";
			output << "A private message was received from another "
				"node in the mesh.\n";

			if (packet->sourceStr[0] != '\0')
				output << "  From:     " << packet->sourceStr << "\n";

			// Extract text preview from summary
			if (packet->summary[0] != '\0') {
				const char* quote = strchr(packet->summary, '"');
				if (quote != NULL) {
					const char* end = strrchr(packet->summary, '"');
					if (end != NULL && end > quote) {
						BString msg;
						msg.Append(quote + 1, end - quote - 1);
						output << "  Message:  \"" << msg << "\"\n";
					}
				}
			}

			if (packet->snr != 0 || packet->rssi != 0) {
				output << "  Signal:   " << _SignalQualityString(packet->snr);
				char sigBuf[48];
				snprintf(sigBuf, sizeof(sigBuf),
					" (SNR %d dB, RSSI %d dBm)", packet->snr, packet->rssi);
				output << sigBuf << "\n";
			}

			if (packet->pathLen > 0) {
				char pathBuf[32];
				snprintf(pathBuf, sizeof(pathBuf), "%u", packet->pathLen);
				output << "  Path:     " << pathBuf
					<< " hop(s) through the mesh\n";
			}
			break;
		}

		case RSP_CHANNEL_MSG_RECV_V3:
		case RSP_CHANNEL_MSG_RECV:
		{
			output << "\xf0\x9f\x93\xa2  Channel Message Received\n";
			output << "A message was received on the public channel.\n";

			if (packet->summary[0] != '\0') {
				const char* quote = strchr(packet->summary, '"');
				if (quote != NULL) {
					const char* end = strrchr(packet->summary, '"');
					if (end != NULL && end > quote) {
						BString msg;
						msg.Append(quote + 1, end - quote - 1);
						output << "  Message:  \"" << msg << "\"\n";
					}
				}
			}

			if (packet->snr != 0 || packet->rssi != 0) {
				output << "  Signal:   " << _SignalQualityString(packet->snr);
				char sigBuf[48];
				snprintf(sigBuf, sizeof(sigBuf),
					" (SNR %d dB, RSSI %d dBm)", packet->snr, packet->rssi);
				output << sigBuf << "\n";
			}
			break;
		}

		case PUSH_ADVERT:
		case PUSH_NEW_ADVERT:
		{
			if (packet->code == PUSH_NEW_ADVERT)
				output << "\xf0\x9f\x86\x95  New Node Advertisement\n";
			else
				output << "\xf0\x9f\x93\xa1  Node Advertisement\n";

			if (packet->code == PUSH_NEW_ADVERT) {
				output << "A previously unknown node announced its "
					"presence on the mesh.\n";
			} else {
				output << "A known node announced its presence on "
					"the mesh.\n";
			}

			if (packet->sourceStr[0] != '\0')
				output << "  Node:     " << packet->sourceStr << "\n";
			break;
		}

		case RSP_BATT_AND_STORAGE:
		{
			output << "\xf0\x9f\x94\x8b  Battery & Storage Report\n";
			output << "The radio reported its battery and storage "
				"status.\n";

			if (packet->payloadSize >= 3) {
				uint16 battMv = packet->payload[1]
					| (packet->payload[2] << 8);
				int pct = ((int)battMv - 3000) * 100 / 1200;
				if (pct < 0) pct = 0;
				if (pct > 100) pct = 100;
				char buf[48];
				snprintf(buf, sizeof(buf), "  Battery:  %u mV (~%d%%)\n",
					battMv, pct);
				output << buf;
			}

			if (packet->payloadSize >= 11) {
				uint32 usedKb = packet->payload[3]
					| (packet->payload[4] << 8)
					| (packet->payload[5] << 16)
					| (packet->payload[6] << 24);
				uint32 totalKb = packet->payload[7]
					| (packet->payload[8] << 8)
					| (packet->payload[9] << 16)
					| (packet->payload[10] << 24);
				char buf[64];
				snprintf(buf, sizeof(buf),
					"  Storage:  %u / %u KB used\n", usedKb, totalKb);
				output << buf;
			}
			break;
		}

		case RSP_SELF_INFO:
		{
			output << "\xf0\x9f\x93\x8b  Device Identity\n";
			output << "The radio reported its identity and "
				"configuration.\n";

			if (packet->sourceStr[0] != '\0')
				output << "  Node ID:  " << packet->sourceStr << "\n";

			if (packet->payloadSize >= 37) {
				uint8 nodeType = packet->payload[36];
				const char* typeStr = "Unknown";
				if (nodeType == 1) typeStr = "Chat Node";
				else if (nodeType == 2) typeStr = "Repeater";
				else if (nodeType == 3) typeStr = "Room Server";
				output << "  Type:     " << typeStr << "\n";
			}
			break;
		}

		case RSP_SENT:
		{
			output << "\xe2\x9c\x85  Message Sent\n";
			output << "The radio successfully transmitted your "
				"message.\n";
			break;
		}

		case RSP_OK:
		{
			output << "\xe2\x9c\x93  Command Acknowledged\n";
			output << "The radio processed your command "
				"successfully.\n";
			break;
		}

		case RSP_ERR:
		{
			if (packet->payloadSize >= 2 && packet->payload[1] <= 3) {
				output << "\xf0\x9f\x94\x97  Connection Established\n";
				char buf[48];
				snprintf(buf, sizeof(buf),
					"APP_START acknowledged, protocol version %u.\n",
					packet->payload[1]);
				output << buf;
			} else {
				output << "\xe2\x9a\xa0  Error Response\n";
				output << "The radio returned an error.\n";
				if (packet->payloadSize >= 2) {
					char buf[32];
					snprintf(buf, sizeof(buf), "  Code:     %u\n",
						packet->payload[1]);
					output << buf;
				}
			}
			break;
		}

		case RSP_CONTACTS_START:
		{
			output << "\xf0\x9f\x93\x96  Contact Sync Started\n";
			output << "The radio is sending its contact list.\n";
			break;
		}

		case RSP_CONTACT:
		{
			output << "\xf0\x9f\x91\xa4  Contact Data\n";
			output << "Information about a node in the contact "
				"list.\n";

			if (packet->sourceStr[0] != '\0')
				output << "  Node:     " << packet->sourceStr << "\n";

			// Extract name from summary "Contact: name"
			if (strncmp(packet->summary, "Contact: ", 9) == 0)
				output << "  Name:     " << (packet->summary + 9) << "\n";

			if (packet->payloadSize >= 34) {
				uint8 nodeType = packet->payload[33];
				const char* typeStr = "Unknown";
				if (nodeType == 1) typeStr = "Chat Node";
				else if (nodeType == 2) typeStr = "Repeater";
				else if (nodeType == 3) typeStr = "Room Server";
				output << "  Type:     " << typeStr << "\n";
			}
			break;
		}

		case RSP_END_OF_CONTACTS:
		{
			output << "\xf0\x9f\x93\x96  Contact Sync Complete\n";
			output << "All contacts have been received from the "
				"radio.\n";
			break;
		}

		case RSP_DEVICE_INFO:
		{
			output << "\xe2\x84\xb9  Device Information\n";
			output << "Hardware and firmware details.\n";

			if (packet->payloadSize >= 4) {
				char buf[48];
				snprintf(buf, sizeof(buf),
					"  Max contacts:  %u\n", packet->payload[2] * 2);
				output << buf;
				snprintf(buf, sizeof(buf),
					"  Max channels:  %u\n", packet->payload[3]);
				output << buf;
			}
			break;
		}

		case RSP_STATS:
		{
			output << "\xf0\x9f\x93\x8a  Statistics Response\n";

			if (packet->payloadSize >= 2) {
				uint8 statType = packet->payload[1];
				if (statType == 0) {
					output << "Core statistics (uptime, battery).\n";
					if (packet->payloadSize >= 8) {
						uint32 uptime = packet->payload[4]
							| (packet->payload[5] << 8)
							| (packet->payload[6] << 16)
							| (packet->payload[7] << 24);
						char buf[48];
						snprintf(buf, sizeof(buf),
							"  Uptime:   %u seconds\n", uptime);
						output << buf;
					}
				} else if (statType == 1) {
					output << "Radio statistics (noise floor, "
						"RSSI, SNR).\n";
				} else if (statType == 2) {
					output << "Packet statistics (sent/received "
						"counts).\n";
				} else {
					output << "Statistics from the radio.\n";
				}
			} else {
				output << "Statistics from the radio.\n";
			}
			break;
		}

		case PUSH_SEND_CONFIRMED:
		{
			output << "\xe2\x9c\x93\xe2\x9c\x93  Delivery Confirmed\n";
			output << "The recipient acknowledged receiving your "
				"message.\n";
			break;
		}

		case PUSH_PATH_UPDATED:
		{
			output << "\xf0\x9f\x94\x80  Path Updated\n";
			output << "A new or better route was found to a node "
				"in the mesh.\n";
			break;
		}

		case PUSH_MSG_WAITING:
		{
			output << "\xf0\x9f\x93\xac  Messages Waiting\n";
			output << "There are queued messages on the radio "
				"device.\n";
			break;
		}

		case PUSH_LOGIN_SUCCESS:
		{
			output << "\xf0\x9f\x94\x93  Login Successful\n";
			output << "Successfully authenticated with a "
				"repeater or room.\n";
			break;
		}

		case PUSH_LOGIN_FAIL:
		{
			output << "\xf0\x9f\x94\x92  Login Failed\n";
			output << "Authentication was rejected by the "
				"repeater or room.\n";
			break;
		}

		case PUSH_RAW_RADIO_PACKET:
		{
			output << "\xf0\x9f\x93\xa1  Raw Radio Packet\n";
			output << "A low-level radio frame captured from the "
				"air.\n";

			if (packet->payloadSize >= 5) {
				char buf[64];
				uint8 seqLo = packet->payload[1];
				uint8 counter = packet->payload[2];
				uint8 flags = packet->payload[4];
				snprintf(buf, sizeof(buf),
					"  Sequence: %u\n  Flags:    0x%02X\n",
					seqLo | (counter << 8), flags);
				output << buf;
			}
			break;
		}

		case PUSH_TRACE_DATA:
		{
			output << "\xf0\x9f\x97\xba  Trace Route Data\n";
			output << "Path information showing how data travels "
				"through the mesh.\n";

			if (packet->payloadSize >= 12) {
				char buf[32];
				snprintf(buf, sizeof(buf), "  Hops:     %u\n",
					packet->payload[2]);
				output << buf;
			}
			break;
		}

		case PUSH_TELEMETRY_RESPONSE:
		{
			output << "\xf0\x9f\x8c\xa1  Telemetry Data\n";
			output << "Sensor readings received from a remote "
				"node.\n";

			if (packet->sourceStr[0] != '\0')
				output << "  From:     " << packet->sourceStr << "\n";
			break;
		}

		default:
		{
			if (packet->code >= 0x80) {
				output << "\xf0\x9f\x94\x94  Push Notification\n";
				output << "An unsolicited notification from the "
					"radio.\n";
			} else {
				output << "\xe2\x86\xa9  Response\n";
				output << "A response to a command sent to the "
					"radio.\n";
			}
			char buf[32];
			snprintf(buf, sizeof(buf), "  Code:     0x%02X\n",
				packet->code);
			output << buf;
			break;
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

	// === Section 1: Decoded (human-readable) ===
	BString decoded;
	_FormatDecodedSection(packet, decoded);

	// === Section 2: Technical Details ===
	BString technical;
	technical << "\n--- Technical Details ---\n";

	technical << "Type:      " << packet->typeStr;
	char codeBuf[8];
	snprintf(codeBuf, sizeof(codeBuf), "%02X", packet->code);
	technical << " (0x" << codeBuf << ")\n";

	char timeBuf[32];
	time_t t = (time_t)packet->timestamp;
	struct tm tm;
	if (localtime_r(&t, &tm) != NULL)
		strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);
	else
		snprintf(timeBuf, sizeof(timeBuf), "unknown");
	technical << "Time:      " << timeBuf << "\n";

	if (packet->sourceStr[0] != '\0')
		technical << "Source:    " << packet->sourceStr << "\n";
	if (packet->snr != 0) {
		char snrBuf[16];
		snprintf(snrBuf, sizeof(snrBuf), "%d dB", packet->snr);
		technical << "SNR:       " << snrBuf << "\n";
	}
	if (packet->rssi != 0) {
		char rssiBuf[16];
		snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", packet->rssi);
		technical << "RSSI:      " << rssiBuf << "\n";
	}
	if (packet->pathLen > 0) {
		char pathBuf[16];
		snprintf(pathBuf, sizeof(pathBuf), "%u", packet->pathLen);
		technical << "Path:      " << pathBuf << " hop(s)\n";
	}
	technical << "Size:      " << packet->payloadSize << " bytes\n";
	if (packet->summary[0] != '\0')
		technical << "Summary:   " << packet->summary << "\n";

	// === Section 3: Hex Dump ===
	BString hexHeader("\n--- Hex Dump ---\n");
	BString hexDump;
	_FormatHexDump(packet->payload, packet->payloadSize, hexDump);

	// Combine all sections
	BString detail;
	detail << decoded << technical << hexHeader << hexDump;

	fDetailView->SetText(detail.String());

	// === Apply fonts and colors ===
	BFont plainFont(be_plain_font);
	plainFont.SetSize(11);
	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11);

	rgb_color textColor = DetailTextColor();
	rgb_color headerColor = SectionHeaderColor();
	rgb_color categoryColor = _PacketCategoryColor(packet->code);

	int32 decodedLen = decoded.Length();
	int32 techStart = decodedLen;
	int32 techEnd = techStart + technical.Length();
	int32 hexHdrStart = techEnd;
	int32 totalLen = detail.Length();

	// Section 1: proportional font, normal text color
	if (decodedLen > 0) {
		fDetailView->SetFontAndColor(0, decodedLen, &plainFont,
			B_FONT_ALL, &textColor);

		// Color the first line (title) with category color
		int32 firstNewline = decoded.FindFirst('\n');
		if (firstNewline > 0) {
			BFont boldPlain(be_plain_font);
			boldPlain.SetSize(12);
			boldPlain.SetFace(B_BOLD_FACE);
			fDetailView->SetFontAndColor(0, firstNewline, &boldPlain,
				B_FONT_ALL, &categoryColor);
		}
	}

	// Section 2: monospace, normal text color
	if (techEnd > techStart) {
		fDetailView->SetFontAndColor(techStart, techEnd, &monoFont,
			B_FONT_ALL, &textColor);

		// Color the "--- Technical Details ---" header
		int32 hdrLine = detail.FindFirst("--- Technical Details ---");
		if (hdrLine >= 0) {
			fDetailView->SetFontAndColor(hdrLine,
				hdrLine + 24, &monoFont, B_FONT_ALL, &headerColor);
		}
	}

	// Section 3: monospace, normal text color
	if (totalLen > hexHdrStart) {
		fDetailView->SetFontAndColor(hexHdrStart, totalLen, &monoFont,
			B_FONT_ALL, &textColor);

		// Color the "--- Hex Dump ---" header
		int32 hexLabel = detail.FindFirst("--- Hex Dump ---");
		if (hexLabel >= 0) {
			fDetailView->SetFontAndColor(hexLabel,
				hexLabel + 16, &monoFont, B_FONT_ALL, &headerColor);
		}
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


void
PacketAnalyzerWindow::_UpdateContactStats()
{
	if (fContactStatsList == NULL)
		return;

	fContactStatsList->Clear();

	// Aggregate stats per source contact
	struct ContactStat {
		BString		name;
		int32		packetCount;
		int32		snrSum;
		int32		snrCount;
		int8		minSNR;
		int8		maxSNR;
		uint32		lastSeen;
	};

	// Simple array-based map (sufficient for typical contact counts)
	BObjectList<ContactStat, true> stats(20);

	for (int32 i = 0; i < fPackets.CountItems(); i++) {
		CapturedPacket* pkt = fPackets.ItemAt(i);
		if (pkt->sourceStr[0] == '\0')
			continue;

		// Find or create stat entry
		ContactStat* stat = NULL;
		for (int32 j = 0; j < stats.CountItems(); j++) {
			if (stats.ItemAt(j)->name == pkt->sourceStr) {
				stat = stats.ItemAt(j);
				break;
			}
		}
		if (stat == NULL) {
			stat = new ContactStat();
			stat->name = pkt->sourceStr;
			stat->packetCount = 0;
			stat->snrSum = 0;
			stat->snrCount = 0;
			stat->minSNR = 127;
			stat->maxSNR = -128;
			stat->lastSeen = 0;
			stats.AddItem(stat);
		}

		stat->packetCount++;
		if (pkt->snr != 0) {
			stat->snrSum += pkt->snr;
			stat->snrCount++;
			if (pkt->snr < stat->minSNR) stat->minSNR = pkt->snr;
			if (pkt->snr > stat->maxSNR) stat->maxSNR = pkt->snr;
		}
		if (pkt->timestamp > stat->lastSeen)
			stat->lastSeen = pkt->timestamp;
	}

	// Contact stats column indices (must match _BuildUI)
	enum {
		kCSContactCol = 0,
		kCSPacketsCol,
		kCSAvgSNRCol,
		kCSMinSNRCol,
		kCSMaxSNRCol,
		kCSLastSeenCol
	};

	// Build rows sorted by packet count (descending)
	for (int32 i = 0; i < stats.CountItems(); i++) {
		ContactStat* stat = stats.ItemAt(i);

		BRow* row = new BRow();

		char pktStr[12];
		snprintf(pktStr, sizeof(pktStr), "%d", (int)stat->packetCount);

		char avgStr[12];
		char minStr[12];
		char maxStr[12];
		int8 avgSNR = 0;

		if (stat->snrCount > 0) {
			avgSNR = (int8)(stat->snrSum / stat->snrCount);
			snprintf(avgStr, sizeof(avgStr), "%d dB", avgSNR);
			snprintf(minStr, sizeof(minStr), "%d", stat->minSNR);
			snprintf(maxStr, sizeof(maxStr), "%d", stat->maxSNR);
		} else {
			strlcpy(avgStr, "-", sizeof(avgStr));
			strlcpy(minStr, "-", sizeof(minStr));
			strlcpy(maxStr, "-", sizeof(maxStr));
		}

		char timeStr[16];
		if (stat->lastSeen > 0) {
			time_t t = (time_t)stat->lastSeen;
			struct tm tm;
			if (localtime_r(&t, &tm) != NULL)
				strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm);
			else
				strlcpy(timeStr, "?", sizeof(timeStr));
		} else {
			strlcpy(timeStr, "-", sizeof(timeStr));
		}

		// Color avg SNR
		rgb_color avgColor;
		if (stat->snrCount == 0)
			avgColor = tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
				B_LIGHTEN_1_TINT);
		else if (avgSNR > 5)
			avgColor = (rgb_color){50, 205, 50, 255};
		else if (avgSNR > 0)
			avgColor = (rgb_color){100, 200, 100, 255};
		else if (avgSNR > -5)
			avgColor = (rgb_color){255, 193, 37, 255};
		else if (avgSNR > -10)
			avgColor = (rgb_color){255, 140, 0, 255};
		else
			avgColor = (rgb_color){220, 20, 60, 255};

		row->SetField(new BStringField(stat->name.String()),
			kCSContactCol);
		row->SetField(new BStringField(pktStr), kCSPacketsCol);
		row->SetField(new ColorStringField(avgStr, avgColor),
			kCSAvgSNRCol);
		row->SetField(new BStringField(minStr), kCSMinSNRCol);
		row->SetField(new BStringField(maxStr), kCSMaxSNRCol);
		row->SetField(new BStringField(timeStr), kCSLastSeenCol);

		fContactStatsList->AddRow(row);
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

		// Calculate delta-t from previous packet in full list
		bigtime_t deltaUs = -1;
		if (i > 0) {
			CapturedPacket* prev = fPackets.ItemAt(i - 1);
			if (prev != NULL && packet->captureTime > 0
				&& prev->captureTime > 0) {
				deltaUs = packet->captureTime - prev->captureTime;
			}
		}

		char deltaStr[24];
		_FormatDelta(deltaUs, deltaStr, sizeof(deltaStr));

		char sizeStr[12];
		snprintf(sizeStr, sizeof(sizeStr), "%u", packet->payloadSize);

		char snrStr[12];
		if (packet->snr != 0)
			snprintf(snrStr, sizeof(snrStr), "%d", packet->snr);
		else
			snprintf(snrStr, sizeof(snrStr), "-");

		row->SetField(new BStringField(indexStr), kIndexColumn);
		row->SetField(new BStringField(timeStr), kTimeColumn);
		row->SetField(new ColorStringField(deltaStr,
			_DeltaColor(deltaUs)), kDeltaColumn);
		row->SetField(new ColorStringField(packet->typeStr,
			_PacketCategoryColor(packet->code)), kTypeColumn);
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
