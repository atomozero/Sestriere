/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepeaterMonitorView.cpp — Structured repeater log viewer (embedded BView)
 */

#include "RepeaterMonitorView.h"

#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "RepMonConstants.h"


// Private message codes
enum {
	MSG_RM_LOAD_LOG   = 'rmll',
	MSG_RM_CLEAR      = 'rmcl',
	MSG_RM_SEARCH     = 'rmsr',
	MSG_RM_ROW_SELECT = 'rmrs',
	MSG_RM_NODE_CLICK = 'rmnc',
};


// #pragma mark - RepeaterPacket


struct RepeaterPacket {
	char	time[12];
	char	date[16];
	bool	isTx;
	int32	len;
	int32	type;
	char	route;		// 'D' or 'F'
	int32	payloadLen;
	int8	snr;
	int16	rssi;
	int32	score;		// -1 if absent
	char	src[8];
	char	dst[8];
};


// #pragma mark - ColorStringField (local duplicate)


class RMColorStringField : public BStringField {
public:
	RMColorStringField(const char* string, rgb_color color)
		:
		BStringField(string),
		fColor(color)
	{
	}

	rgb_color Color() const { return fColor; }

private:
	rgb_color fColor;
};


class RMColorStringColumn : public BStringColumn {
public:
	RMColorStringColumn(const char* title, float width, float minWidth,
		float maxWidth, uint32 truncate, alignment align = B_ALIGN_LEFT)
		:
		BStringColumn(title, width, minWidth, maxWidth, truncate, align)
	{
	}

	virtual void DrawField(BField* field, BRect rect, BView* parent)
	{
		RMColorStringField* colorField
			= dynamic_cast<RMColorStringField*>(field);
		if (colorField != NULL) {
			rgb_color savedColor = parent->HighColor();
			parent->SetHighColor(colorField->Color());
			BStringColumn::DrawField(field, rect, parent);
			parent->SetHighColor(savedColor);
		} else {
			BStringColumn::DrawField(field, rect, parent);
		}
	}

	virtual bool AcceptsField(const BField* field) const
	{
		return dynamic_cast<const RMColorStringField*>(field) != NULL
			|| dynamic_cast<const BStringField*>(field) != NULL;
	}
};


// #pragma mark - SNRGraphView


static const int32 kMaxGraphPoints = 200;


class SNRGraphView : public BView {
public:
	SNRGraphView()
		:
		BView("snrgraph", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fPointCount(0)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 80));
		SetExplicitPreferredSize(BSize(B_SIZE_UNLIMITED, 120));
	}

	void AddPoint(int8 snr, int16 rssi)
	{
		if (fPointCount < kMaxGraphPoints) {
			fSNRValues[fPointCount] = snr;
			fRSSIValues[fPointCount] = rssi;
			fPointCount++;
		} else {
			memmove(fSNRValues, fSNRValues + 1,
				(kMaxGraphPoints - 1) * sizeof(int8));
			memmove(fRSSIValues, fRSSIValues + 1,
				(kMaxGraphPoints - 1) * sizeof(int16));
			fSNRValues[kMaxGraphPoints - 1] = snr;
			fRSSIValues[kMaxGraphPoints - 1] = rssi;
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

		// SNR range: -20..+20
		float snrMax = 20.0f;
		float snrMin = -20.0f;
		float snrRange = snrMax - snrMin;

		// Background zones
		rgb_color bgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
		bool isDark = bgColor.Brightness() < 128;

		rgb_color greenZone, yellowZone, redZone;
		if (isDark) {
			greenZone = (rgb_color){20, 50, 20, 255};
			yellowZone = (rgb_color){50, 50, 10, 255};
			redZone = (rgb_color){50, 15, 15, 255};
		} else {
			greenZone = (rgb_color){200, 255, 200, 255};
			yellowZone = (rgb_color){255, 255, 200, 255};
			redZone = (rgb_color){255, 220, 220, 255};
		}

		float yGoodBot = h * (snrMax - 5.0f) / snrRange;
		float yPoorBot = h * (snrMax - (-5.0f)) / snrRange;
		float yBadBot = h * (snrMax - (-10.0f)) / snrRange;

		SetHighColor(greenZone);
		FillRect(BRect(0, 0, w, yGoodBot));
		SetHighColor(yellowZone);
		FillRect(BRect(0, yGoodBot, w, yPoorBot));
		SetHighColor(redZone);
		FillRect(BRect(0, yBadBot, w, h));

		// Zero line
		float yZero = h * snrMax / snrRange;
		SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
			B_LIGHTEN_2_TINT));
		StrokeLine(BPoint(0, yZero), BPoint(w, yZero));

		// 5 dB grid lines
		for (int snr = -15; snr <= 15; snr += 5) {
			if (snr == 0)
				continue;
			float y = h * (snrMax - snr) / snrRange;
			StrokeLine(BPoint(0, y), BPoint(w, y), B_MIXED_COLORS);
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
			SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
				B_LIGHTEN_2_TINT));
			DrawString("Waiting for RX data\xe2\x80\xa6",
				BPoint(w / 2 - 50, h / 2));
			return;
		}

		float leftMargin = 25;
		float plotW = w - leftMargin - 5;

		// Draw RSSI overlay (gray dashed) — range -120..-40 dBm
		float rssiMax = -40.0f;
		float rssiMin = -120.0f;
		float rssiRange = rssiMax - rssiMin;

		SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
			B_LIGHTEN_1_TINT));
		SetPenSize(1.0f);
		for (int32 i = 1; i < fPointCount; i++) {
			if (fRSSIValues[i] == 0 || fRSSIValues[i - 1] == 0)
				continue;
			float x1 = leftMargin
				+ plotW * (float)(i - 1) / (fPointCount - 1);
			float x2 = leftMargin
				+ plotW * (float)i / (fPointCount - 1);
			float y1 = h * (rssiMax - fRSSIValues[i - 1]) / rssiRange;
			float y2 = h * (rssiMax - fRSSIValues[i]) / rssiRange;
			// Dashed effect: draw every other segment
			if (i % 2 == 0)
				StrokeLine(BPoint(x1, y1), BPoint(x2, y2));
		}

		// Draw SNR line (colored by quality)
		SetPenSize(2.0f);
		for (int32 i = 1; i < fPointCount; i++) {
			float x1 = leftMargin
				+ plotW * (float)(i - 1) / (fPointCount - 1);
			float x2 = leftMargin
				+ plotW * (float)i / (fPointCount - 1);
			float y1 = h * (snrMax - fSNRValues[i - 1]) / snrRange;
			float y2 = h * (snrMax - fSNRValues[i]) / snrRange;

			// Color by SNR quality
			int8 snrVal = fSNRValues[i];
			if (snrVal > 5)
				SetHighColor(kColorGood);
			else if (snrVal > 0)
				SetHighColor((rgb_color){120, 200, 120, 255});
			else if (snrVal > -5)
				SetHighColor(kColorFair);
			else if (snrVal > -10)
				SetHighColor(kColorPoor);
			else
				SetHighColor(kColorBad);

			StrokeLine(BPoint(x1, y1), BPoint(x2, y2));
		}

		// Dot markers at data points
		SetPenSize(1.0f);
		for (int32 i = 0; i < fPointCount; i++) {
			float x = leftMargin
				+ plotW * (float)i / (fPointCount > 1
					? fPointCount - 1 : 1);
			float y = h * (snrMax - fSNRValues[i]) / snrRange;

			int8 snrVal = fSNRValues[i];
			if (snrVal > 5)
				SetHighColor(kColorGood);
			else if (snrVal > 0)
				SetHighColor((rgb_color){120, 200, 120, 255});
			else if (snrVal > -5)
				SetHighColor(kColorFair);
			else
				SetHighColor(kColorBad);

			FillEllipse(BPoint(x, y), 2, 2);
		}

		// Legend
		BFont legendFont(be_plain_font);
		legendFont.SetSize(9);
		SetFont(&legendFont);
		SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
		DrawString("SNR", BPoint(w - 70, 12));
		SetHighColor(tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
			B_LIGHTEN_1_TINT));
		DrawString("RSSI", BPoint(w - 35, 12));

		SetPenSize(1.0f);
	}

private:
	int8	fSNRValues[kMaxGraphPoints];
	int16	fRSSIValues[kMaxGraphPoints];
	int32	fPointCount;
};


// #pragma mark - NodeStats (helper)


struct NodeStats {
	char	node[8];
	int32	packetCount;
	int64	snrSum;
	int32	snrCount;
	int64	rssiSum;
	int32	rssiCount;
	char	lastTime[12];
	char	lastDate[16];
};

static const int32 kMaxNodes = 64;


// #pragma mark - Parsing helpers


static rgb_color
_TypeColor(int32 type)
{
	switch (type) {
		case 2:  return (rgb_color){80, 140, 220, 255};    // messages — blue
		case 4:  return (rgb_color){80, 180, 80, 255};     // contacts — green
		case 9:  return (rgb_color){140, 140, 140, 255};   // ping — gray
		case 7:  return (rgb_color){210, 170, 50, 255};    // handshake — amber
		case 5:  return (rgb_color){160, 100, 200, 255};   // trace — purple
		default: return ui_color(B_LIST_ITEM_TEXT_COLOR);
	}
}


static rgb_color
_SnrColor(int8 snr)
{
	if (snr > 5)
		return kColorGood;
	if (snr > 0)
		return (rgb_color){120, 200, 120, 255};
	if (snr > -5)
		return kColorFair;
	if (snr > -10)
		return kColorPoor;
	return kColorBad;
}


static bool
_ParseLogLine(const char* line, RepeaterPacket* out)
{
	// Format:
	// "HH:MM:SS - DD/M/YYYY U: TX, len=38 (type=2, route=D, payload_len=36) [FD -> D5]"
	// "HH:MM:SS - DD/M/YYYY U: RX, len=38 (type=2, route=D, payload_len=36) SNR=11 RSSI=-62 score=174 [FD -> D5]"

	memset(out, 0, sizeof(RepeaterPacket));
	out->score = -1;

	// Parse time
	if (strlen(line) < 10)
		return false;

	// Time: HH:MM:SS
	int hour, min, sec;
	if (sscanf(line, "%d:%d:%d", &hour, &min, &sec) != 3)
		return false;
	snprintf(out->time, sizeof(out->time), "%02d:%02d:%02d", hour, min, sec);

	// Skip to date: find " - "
	const char* dateSep = strstr(line, " - ");
	if (dateSep == NULL)
		return false;
	const char* dateStart = dateSep + 3;

	// Date: DD/M/YYYY (variable width month)
	int day, month, year;
	int dateChars = 0;
	if (sscanf(dateStart, "%d/%d/%d%n", &day, &month, &year, &dateChars) != 3)
		return false;
	snprintf(out->date, sizeof(out->date), "%d/%d/%d", day, month, year);

	// Skip past date to " U: "
	const char* uPos = strstr(dateStart + dateChars, "U: ");
	if (uPos == NULL)
		return false;
	const char* dirStart = uPos + 3;

	// Direction: TX or RX
	if (strncmp(dirStart, "TX", 2) == 0)
		out->isTx = true;
	else if (strncmp(dirStart, "RX", 2) == 0)
		out->isTx = false;
	else
		return false;

	// len=N
	const char* lenPos = strstr(dirStart, "len=");
	if (lenPos == NULL)
		return false;
	out->len = atoi(lenPos + 4);

	// (type=T, route=R, payload_len=P)
	const char* typePos = strstr(dirStart, "type=");
	if (typePos != NULL)
		out->type = atoi(typePos + 5);

	const char* routePos = strstr(dirStart, "route=");
	if (routePos != NULL)
		out->route = routePos[6];

	const char* plPos = strstr(dirStart, "payload_len=");
	if (plPos != NULL)
		out->payloadLen = atoi(plPos + 12);

	// Optional: SNR=, RSSI=, score=
	const char* snrPos = strstr(dirStart, "SNR=");
	if (snrPos != NULL)
		out->snr = (int8)atoi(snrPos + 4);

	const char* rssiPos = strstr(dirStart, "RSSI=");
	if (rssiPos != NULL)
		out->rssi = (int16)atoi(rssiPos + 5);

	const char* scorePos = strstr(dirStart, "score=");
	if (scorePos != NULL)
		out->score = atoi(scorePos + 6);

	// [SRC -> DST]
	const char* bracket = strstr(dirStart, "[");
	if (bracket != NULL) {
		const char* arrow = strstr(bracket, " -> ");
		const char* closeBracket = strstr(bracket, "]");
		if (arrow != NULL && closeBracket != NULL) {
			// Extract src (between [ and ->)
			int srcLen = arrow - (bracket + 1);
			if (srcLen > 0 && srcLen < (int)sizeof(out->src)) {
				memcpy(out->src, bracket + 1, srcLen);
				out->src[srcLen] = '\0';
			}
			// Extract dst (between -> and ])
			const char* dstStart = arrow + 4;
			int dstLen = closeBracket - dstStart;
			if (dstLen > 0 && dstLen < (int)sizeof(out->dst)) {
				memcpy(out->dst, dstStart, dstLen);
				out->dst[dstLen] = '\0';
			}
		}
	}

	return true;
}


// Packet list column indices
enum {
	kPktColIndex = 0,
	kPktColTime,
	kPktColDate,
	kPktColDir,
	kPktColType,
	kPktColRoute,
	kPktColLen,
	kPktColSNR,
	kPktColRSSI,
	kPktColScore,
	kPktColNodes,
};

// Node stats column indices
enum {
	kNodeColName = 0,
	kNodeColPkts,
	kNodeColTx,
	kNodeColRx,
	kNodeColAvgSNR,
	kNodeColAvgRSSI,
	kNodeColRole,
	kNodeColLastSeen,
};


// #pragma mark - RepeaterMonitorView


RepeaterMonitorView::RepeaterMonitorView(BHandler* target)
	:
	BView("repeater_monitor", B_WILL_DRAW),
	fTarget(target),
	fPacketList(NULL),
	fNodeStatsList(NULL),
	fSnrGraph(NULL),
	fFirmwareLabel(NULL),
	fLoadLogButton(NULL),
	fClearButton(NULL),
	fTotalStatsLabel(NULL),
	fSearchField(NULL),
	fTotalTx(0),
	fTotalRx(0),
	fTotalFwd(0),
	fTotalDirect(0),
	fPacketCount(0),
	fLinkCount(0)
{
	memset(fNodeHasDirect, 0, sizeof(fNodeHasDirect));
	memset(fNodeHasForwarded, 0, sizeof(fNodeHasForwarded));
	memset(fNodeTxAsSrc, 0, sizeof(fNodeTxAsSrc));
	memset(fNodeRxAsSrc, 0, sizeof(fNodeRxAsSrc));
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// --- Toolbar ---
	fFirmwareLabel = new BStringView("firmware", "Firmware: \xe2\x80\x94");
	BFont boldFont(be_bold_font);
	boldFont.SetSize(11);
	fFirmwareLabel->SetFont(&boldFont);

	fLoadLogButton = new BButton("loadlog", "Load Log",
		new BMessage(MSG_RM_LOAD_LOG));
	fClearButton = new BButton("clear", "Clear",
		new BMessage(MSG_RM_CLEAR));

	fSearchField = new BTextControl("search", "Filter:", "",
		new BMessage(MSG_RM_SEARCH));
	fSearchField->SetModificationMessage(new BMessage(MSG_RM_SEARCH));

	// --- Packet list ---
	fPacketList = new BColumnListView("packets", 0, B_PLAIN_BORDER);
	fPacketList->SetSelectionMessage(new BMessage(MSG_RM_ROW_SELECT));

	fPacketList->AddColumn(
		new BIntegerColumn("#", 40, 30, 60, B_ALIGN_RIGHT), kPktColIndex);
	fPacketList->AddColumn(
		new BStringColumn("Time", 70, 50, 100, B_TRUNCATE_END), kPktColTime);
	fPacketList->AddColumn(
		new BStringColumn("Date", 80, 60, 120, B_TRUNCATE_END), kPktColDate);
	fPacketList->AddColumn(
		new RMColorStringColumn("Dir", 40, 30, 60, B_TRUNCATE_END),
		kPktColDir);
	fPacketList->AddColumn(
		new RMColorStringColumn("Type", 50, 35, 80, B_TRUNCATE_END),
		kPktColType);
	fPacketList->AddColumn(
		new BStringColumn("Route", 40, 30, 60, B_TRUNCATE_END),
		kPktColRoute);
	fPacketList->AddColumn(
		new BIntegerColumn("Len", 45, 35, 70, B_ALIGN_RIGHT), kPktColLen);
	fPacketList->AddColumn(
		new RMColorStringColumn("SNR", 50, 35, 80, B_TRUNCATE_END),
		kPktColSNR);
	fPacketList->AddColumn(
		new BIntegerColumn("RSSI", 50, 35, 80, B_ALIGN_RIGHT), kPktColRSSI);
	fPacketList->AddColumn(
		new BIntegerColumn("Score", 50, 35, 80, B_ALIGN_RIGHT),
		kPktColScore);
	fPacketList->AddColumn(
		new BStringColumn("Nodes", 90, 60, 150, B_TRUNCATE_END),
		kPktColNodes);

	// --- Node stats list ---
	fNodeStatsList = new BColumnListView("nodestats", 0, B_PLAIN_BORDER);
	fNodeStatsList->SetSelectionMessage(new BMessage(MSG_RM_NODE_CLICK));

	fNodeStatsList->AddColumn(
		new BStringColumn("Node", 50, 35, 80, B_TRUNCATE_END),
		kNodeColName);
	fNodeStatsList->AddColumn(
		new BIntegerColumn("Pkts", 45, 35, 70, B_ALIGN_RIGHT),
		kNodeColPkts);
	fNodeStatsList->AddColumn(
		new BIntegerColumn("TX", 40, 30, 60, B_ALIGN_RIGHT),
		kNodeColTx);
	fNodeStatsList->AddColumn(
		new BIntegerColumn("RX", 40, 30, 60, B_ALIGN_RIGHT),
		kNodeColRx);
	fNodeStatsList->AddColumn(
		new RMColorStringColumn("SNR", 50, 35, 80, B_TRUNCATE_END),
		kNodeColAvgSNR);
	fNodeStatsList->AddColumn(
		new BIntegerColumn("RSSI", 50, 35, 80, B_ALIGN_RIGHT),
		kNodeColAvgRSSI);
	fNodeStatsList->AddColumn(
		new RMColorStringColumn("Role", 60, 40, 100, B_TRUNCATE_END),
		kNodeColRole);
	fNodeStatsList->AddColumn(
		new BStringColumn("Last Seen", 100, 70, 160, B_TRUNCATE_END),
		kNodeColLastSeen);

	// --- SNR Graph ---
	fSnrGraph = new SNRGraphView();

	// --- Status bar ---
	fTotalStatsLabel = new BStringView("stats",
		"TX: 0 | RX: 0 | Fwd: 0 | Direct: 0");
	BFont statusFont(be_plain_font);
	statusFont.SetSize(11);
	fTotalStatsLabel->SetFont(&statusFont);

	// --- Bottom split: node stats + SNR graph ---
	BSplitView* bottomSplit = new BSplitView(B_HORIZONTAL);
	BLayoutBuilder::Split<>(bottomSplit)
		.Add(fNodeStatsList, 1)
		.Add(fSnrGraph, 2)
		.SetInsets(0)
	;

	// --- Main layout ---
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		// Toolbar
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.SetInsets(B_USE_HALF_ITEM_INSETS)
			.Add(fFirmwareLabel)
			.AddGlue()
			.Add(fSearchField)
			.Add(fLoadLogButton)
			.Add(fClearButton)
		.End()
		// Main split: packet list (top) + bottom panel
		.AddSplit(B_VERTICAL, 0, 1)
			.Add(fPacketList, 3)
			.Add(bottomSplit, 1)
		.End()
		// Status bar
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(B_USE_HALF_ITEM_INSETS, 2, B_USE_HALF_ITEM_INSETS, 2)
			.Add(fTotalStatsLabel)
			.AddGlue()
		.End()
	;
}


RepeaterMonitorView::~RepeaterMonitorView()
{
}


void
RepeaterMonitorView::AttachedToWindow()
{
	BView::AttachedToWindow();

	fLoadLogButton->SetTarget(this);
	fClearButton->SetTarget(this);
	fSearchField->SetTarget(this);
	fPacketList->SetTarget(this);
	fNodeStatsList->SetTarget(this);
}


void
RepeaterMonitorView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_RM_LOAD_LOG:
		{
			// Send "log\n" via serial
			BMessage rawMsg(MSG_SERIAL_SEND_RAW);
			rawMsg.AddString("text", "log");
			BMessenger(fTarget).SendMessage(&rawMsg);
			break;
		}

		case MSG_RM_CLEAR:
		{
			Clear();
			break;
		}

		case MSG_RM_SEARCH:
		{
			fFilterNode = "";
			break;
		}

		case MSG_RM_NODE_CLICK:
		{
			BRow* row = fNodeStatsList->CurrentSelection();
			if (row != NULL) {
				BStringField* nameField
					= static_cast<BStringField*>(row->GetField(kNodeColName));
				if (nameField != NULL) {
					fFilterNode = nameField->String();
					fSearchField->SetText(fFilterNode.String());
					_RebuildFilteredList();
				}
			}
			break;
		}

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
RepeaterMonitorView::ProcessLine(const char* line)
{
	// Check for firmware version response: "-> v1.x.x..."
	if (strncmp(line, "-> v", 4) == 0 || strncmp(line, "->v", 3) == 0) {
		const char* ver = line + 3;
		if (*ver == ' ')
			ver++;
		SetFirmwareVersion(ver);
		return;
	}

	// Try to parse as log line
	RepeaterPacket pkt;
	if (!_ParseLogLine(line, &pkt))
		return;

	fPacketCount++;

	// Update counters
	if (pkt.isTx)
		fTotalTx++;
	else
		fTotalRx++;

	if (pkt.route == 'F')
		fTotalFwd++;
	else if (pkt.route == 'D')
		fTotalDirect++;

	// Check if this packet matches filter
	bool matchesFilter = true;
	if (fFilterNode.Length() > 0) {
		if (fFilterNode != pkt.src && fFilterNode != pkt.dst)
			matchesFilter = false;
	}

	// Add row to packet list
	if (matchesFilter) {
		BRow* row = new BRow();

		row->SetField(new BIntegerField(fPacketCount), kPktColIndex);
		row->SetField(new BStringField(pkt.time), kPktColTime);
		row->SetField(new BStringField(pkt.date), kPktColDate);

		// Dir: colored
		rgb_color dirColor = pkt.isTx
			? (rgb_color){80, 180, 80, 255}	// green for TX
			: (rgb_color){80, 140, 220, 255};	// blue for RX
		row->SetField(
			new RMColorStringField(pkt.isTx ? "TX" : "RX", dirColor),
			kPktColDir);

		// Type: colored by category
		char typeStr[8];
		snprintf(typeStr, sizeof(typeStr), "%d", (int)pkt.type);
		row->SetField(
			new RMColorStringField(typeStr, _TypeColor(pkt.type)),
			kPktColType);

		char routeStr[4];
		snprintf(routeStr, sizeof(routeStr), "%c", pkt.route);
		row->SetField(new BStringField(routeStr), kPktColRoute);

		row->SetField(new BIntegerField(pkt.len), kPktColLen);

		// SNR: colored (only for RX)
		if (!pkt.isTx && pkt.snr != 0) {
			char snrStr[8];
			snprintf(snrStr, sizeof(snrStr), "%d", (int)pkt.snr);
			row->SetField(
				new RMColorStringField(snrStr, _SnrColor(pkt.snr)),
				kPktColSNR);
		} else {
			row->SetField(new BStringField(""), kPktColSNR);
		}

		row->SetField(
			new BIntegerField(pkt.rssi != 0 ? pkt.rssi : 0), kPktColRSSI);
		row->SetField(
			new BIntegerField(pkt.score >= 0 ? pkt.score : 0), kPktColScore);

		// Nodes: "SRC -> DST"
		char nodesStr[24];
		snprintf(nodesStr, sizeof(nodesStr), "%s \xe2\x86\x92 %s",
			pkt.src, pkt.dst);
		row->SetField(new BStringField(nodesStr), kPktColNodes);

		fPacketList->AddRow(row);

		// Auto-scroll to bottom
		fPacketList->ScrollTo(row);
	}

	// Update SNR graph for RX packets with SNR
	if (!pkt.isTx && pkt.snr != 0)
		static_cast<SNRGraphView*>(fSnrGraph)->AddPoint(pkt.snr, pkt.rssi);

	// Update node stats
	_UpdateNodeStats(pkt);

	// Send packet flow animation to NetworkMapWindow via MainWindow
	if (pkt.src[0] != '\0' && pkt.dst[0] != '\0') {
		BMessage flowMsg(MSG_REPEATER_PACKET_FLOW);
		flowMsg.AddString("src", pkt.src);
		flowMsg.AddString("dst", pkt.dst);
		flowMsg.AddInt8("snr", pkt.snr);
		flowMsg.AddInt32("type", pkt.type);
		BMessenger(fTarget).SendMessage(&flowMsg);
	}

	// Update status bar
	_UpdateStats();
}


void
RepeaterMonitorView::SetFirmwareVersion(const char* ver)
{
	BString label("Firmware: ");
	label << ver;
	fFirmwareLabel->SetText(label.String());
}


void
RepeaterMonitorView::Clear()
{
	fPacketList->Clear();
	fNodeStatsList->Clear();
	static_cast<SNRGraphView*>(fSnrGraph)->Clear();
	fTotalTx = 0;
	fTotalRx = 0;
	fTotalFwd = 0;
	fTotalDirect = 0;
	fPacketCount = 0;
	fFilterNode = "";
	fSearchField->SetText("");
	fTotalStatsLabel->SetText(
		"TX: 0 | RX: 0 | Fwd: 0 | Direct: 0");
	memset(fNodeHasDirect, 0, sizeof(fNodeHasDirect));
	memset(fNodeHasForwarded, 0, sizeof(fNodeHasForwarded));
	memset(fNodeTxAsSrc, 0, sizeof(fNodeTxAsSrc));
	memset(fNodeRxAsSrc, 0, sizeof(fNodeRxAsSrc));
	fLinkCount = 0;
}


// #pragma mark - Private


void
RepeaterMonitorView::_UpdateStats()
{
	char buf[128];
	snprintf(buf, sizeof(buf),
		"TX: %d | RX: %d | Fwd: %d | Direct: %d | Total: %d",
		(int)fTotalTx, (int)fTotalRx, (int)fTotalFwd, (int)fTotalDirect,
		(int)fPacketCount);
	fTotalStatsLabel->SetText(buf);
}


void
RepeaterMonitorView::_UpdateNodeStats(const RepeaterPacket& pkt)
{
	// We track stats for src nodes
	if (pkt.src[0] == '\0')
		return;

	// Search existing rows
	BRow* foundRow = NULL;
	int32 rowIndex = -1;
	for (int32 i = 0; i < fNodeStatsList->CountRows(); i++) {
		BRow* row = fNodeStatsList->RowAt(i);
		BStringField* nameField
			= static_cast<BStringField*>(row->GetField(kNodeColName));
		if (nameField != NULL && strcmp(nameField->String(), pkt.src) == 0) {
			foundRow = row;
			rowIndex = i;
			break;
		}
	}

	if (foundRow != NULL) {
		// Update existing row
		BIntegerField* pktsField
			= static_cast<BIntegerField*>(foundRow->GetField(kNodeColPkts));
		int32 newCount = pktsField ? (int32)pktsField->Value() + 1 : 1;
		foundRow->SetField(new BIntegerField(newCount), kNodeColPkts);

		// Track TX/RX counts
		if (rowIndex >= 0 && rowIndex < kMaxNodes) {
			if (pkt.isTx)
				fNodeTxAsSrc[rowIndex]++;
			else
				fNodeRxAsSrc[rowIndex]++;
			foundRow->SetField(
				new BIntegerField(fNodeTxAsSrc[rowIndex]), kNodeColTx);
			foundRow->SetField(
				new BIntegerField(fNodeRxAsSrc[rowIndex]), kNodeColRx);

			if (pkt.route == 'D')
				fNodeHasDirect[rowIndex] = true;
			else if (pkt.route == 'F')
				fNodeHasForwarded[rowIndex] = true;
		}

		// Update SNR average
		if (!pkt.isTx && pkt.snr != 0) {
			RMColorStringField* snrField = dynamic_cast<RMColorStringField*>(
				foundRow->GetField(kNodeColAvgSNR));
			int oldAvg = 0;
			if (snrField != NULL)
				oldAvg = atoi(snrField->String());
			int newAvg = (oldAvg * (newCount - 1) + pkt.snr) / newCount;
			char snrStr[8];
			snprintf(snrStr, sizeof(snrStr), "%d", newAvg);
			foundRow->SetField(
				new RMColorStringField(snrStr, _SnrColor((int8)newAvg)),
				kNodeColAvgSNR);
		}

		if (!pkt.isTx && pkt.rssi != 0) {
			BIntegerField* rssiField = static_cast<BIntegerField*>(
				foundRow->GetField(kNodeColAvgRSSI));
			int oldAvg = rssiField ? (int)rssiField->Value() : 0;
			int newAvg = (oldAvg * (newCount - 1) + pkt.rssi) / newCount;
			foundRow->SetField(new BIntegerField(newAvg), kNodeColAvgRSSI);
		}

		// Auto-detect role from TX/RX pattern
		if (rowIndex >= 0 && rowIndex < kMaxNodes) {
			int32 tx = fNodeTxAsSrc[rowIndex];
			int32 rx = fNodeRxAsSrc[rowIndex];
			const char* role;
			rgb_color roleColor;
			if (rx == 0 && tx > 0) {
				role = "Self";
				roleColor = (rgb_color){255, 180, 50, 255};  // amber
			} else if (tx > rx * 2 && tx > 3) {
				role = "Self?";
				roleColor = (rgb_color){255, 180, 50, 255};
			} else {
				role = "Remote";
				roleColor = (rgb_color){100, 180, 255, 255};  // blue
			}
			foundRow->SetField(
				new RMColorStringField(role, roleColor), kNodeColRole);
		}

		// Last seen
		char lastSeen[32];
		snprintf(lastSeen, sizeof(lastSeen), "%s %s", pkt.time, pkt.date);
		foundRow->SetField(new BStringField(lastSeen), kNodeColLastSeen);

		fNodeStatsList->UpdateRow(foundRow);
	} else {
		// New node
		int32 newIndex = fNodeStatsList->CountRows();
		BRow* row = new BRow();
		row->SetField(new BStringField(pkt.src), kNodeColName);
		row->SetField(new BIntegerField(1), kNodeColPkts);

		// Initial TX/RX count
		if (newIndex < kMaxNodes) {
			if (pkt.isTx)
				fNodeTxAsSrc[newIndex] = 1;
			else
				fNodeRxAsSrc[newIndex] = 1;
			row->SetField(
				new BIntegerField(fNodeTxAsSrc[newIndex]), kNodeColTx);
			row->SetField(
				new BIntegerField(fNodeRxAsSrc[newIndex]), kNodeColRx);

			if (pkt.route == 'D')
				fNodeHasDirect[newIndex] = true;
			else if (pkt.route == 'F')
				fNodeHasForwarded[newIndex] = true;

			// Initial role
			const char* role = pkt.isTx ? "Self?" : "Remote";
			rgb_color roleColor = pkt.isTx
				? (rgb_color){255, 180, 50, 255}
				: (rgb_color){100, 180, 255, 255};
			row->SetField(
				new RMColorStringField(role, roleColor), kNodeColRole);
		}

		if (!pkt.isTx && pkt.snr != 0) {
			char snrStr[8];
			snprintf(snrStr, sizeof(snrStr), "%d", (int)pkt.snr);
			row->SetField(
				new RMColorStringField(snrStr, _SnrColor(pkt.snr)),
				kNodeColAvgSNR);
		} else {
			row->SetField(new BStringField("\xe2\x80\x94"), kNodeColAvgSNR);
		}

		row->SetField(
			new BIntegerField(pkt.rssi != 0 ? pkt.rssi : 0),
			kNodeColAvgRSSI);

		char lastSeen[32];
		snprintf(lastSeen, sizeof(lastSeen), "%s %s", pkt.time, pkt.date);
		row->SetField(new BStringField(lastSeen), kNodeColLastSeen);

		fNodeStatsList->AddRow(row);
	}

	// Track src→dst link
	if (pkt.src[0] != '\0' && pkt.dst[0] != '\0')
		_UpdateLink(pkt.src, pkt.dst, pkt.isTx ? 0 : pkt.snr);

	// Also ensure destination node exists in the node list
	// (so nodes that only appear as destinations are still visible on the map)
	if (pkt.dst[0] != '\0' && strcmp(pkt.dst, pkt.src) != 0) {
		bool dstExists = false;
		for (int32 i = 0; i < fNodeStatsList->CountRows(); i++) {
			BRow* row = fNodeStatsList->RowAt(i);
			BStringField* nameField
				= static_cast<BStringField*>(row->GetField(kNodeColName));
			if (nameField != NULL
				&& strcmp(nameField->String(), pkt.dst) == 0) {
				dstExists = true;
				break;
			}
		}
		if (!dstExists && fNodeStatsList->CountRows() < kMaxNodes) {
			int32 newIndex = fNodeStatsList->CountRows();
			BRow* row = new BRow();
			row->SetField(new BStringField(pkt.dst), kNodeColName);
			row->SetField(new BIntegerField(0), kNodeColPkts);
			if (newIndex < kMaxNodes) {
				fNodeTxAsSrc[newIndex] = 0;
				fNodeRxAsSrc[newIndex] = 0;
				row->SetField(new BIntegerField(0), kNodeColTx);
				row->SetField(new BIntegerField(0), kNodeColRx);
				row->SetField(
					new RMColorStringField("Remote",
						(rgb_color){100, 180, 255, 255}),
					kNodeColRole);
			}
			row->SetField(
				new BStringField("\xe2\x80\x94"), kNodeColAvgSNR);
			row->SetField(new BIntegerField(0), kNodeColAvgRSSI);
			row->SetField(new BStringField(""), kNodeColLastSeen);
			fNodeStatsList->AddRow(row);
		}
	}
}


int32
RepeaterMonitorView::GetNodeInfos(RepeaterNodeInfo* outArray,
	int32 maxCount) const
{
	int32 count = 0;
	for (int32 i = 0; i < fNodeStatsList->CountRows() && count < maxCount;
			i++) {
		BRow* row = fNodeStatsList->RowAt(i);
		if (row == NULL)
			continue;

		RepeaterNodeInfo& info = outArray[count];
		memset(&info, 0, sizeof(RepeaterNodeInfo));

		BStringField* nameField
			= static_cast<BStringField*>(row->GetField(kNodeColName));
		if (nameField != NULL)
			strlcpy(info.name, nameField->String(), sizeof(info.name));

		BIntegerField* pktsField
			= static_cast<BIntegerField*>(row->GetField(kNodeColPkts));
		if (pktsField != NULL)
			info.packetCount = (int32)pktsField->Value();

		RMColorStringField* snrField = dynamic_cast<RMColorStringField*>(
			row->GetField(kNodeColAvgSNR));
		if (snrField != NULL)
			info.avgSnr = (int8)atoi(snrField->String());

		BIntegerField* rssiField
			= static_cast<BIntegerField*>(row->GetField(kNodeColAvgRSSI));
		if (rssiField != NULL)
			info.avgRssi = (int16)rssiField->Value();

		BStringField* lastField
			= static_cast<BStringField*>(row->GetField(kNodeColLastSeen));
		if (lastField != NULL)
			strlcpy(info.lastTime, lastField->String(), sizeof(info.lastTime));

		// Read per-node flags (index matches row order)
		if (i < kMaxNodes) {
			info.isDirect = fNodeHasDirect[i];
			info.isForwarded = fNodeHasForwarded[i];

			// Self-detection: TX only with no RX, or TX >> RX
			int32 tx = fNodeTxAsSrc[i];
			int32 rx = fNodeRxAsSrc[i];
			info.isSelf = (rx == 0 && tx > 0)
				|| (tx > rx * 2 && tx > 3);
		}

		count++;
	}
	return count;
}


int32
RepeaterMonitorView::GetLinks(RepeaterLink* outArray, int32 maxCount) const
{
	int32 count = fLinkCount < maxCount ? fLinkCount : maxCount;
	memcpy(outArray, fLinks, count * sizeof(RepeaterLink));
	return count;
}


void
RepeaterMonitorView::_UpdateLink(const char* src, const char* dst, int8 snr)
{
	// Search for existing link src→dst
	for (int32 i = 0; i < fLinkCount; i++) {
		if (strcmp(fLinks[i].src, src) == 0
			&& strcmp(fLinks[i].dst, dst) == 0) {
			fLinks[i].count++;
			if (snr != 0)
				fLinks[i].snr = snr;
			return;
		}
	}

	// Add new link
	if (fLinkCount >= kMaxLinks)
		return;

	RepeaterLink& link = fLinks[fLinkCount];
	memset(&link, 0, sizeof(RepeaterLink));
	strlcpy(link.src, src, sizeof(link.src));
	strlcpy(link.dst, dst, sizeof(link.dst));
	link.snr = snr;
	link.count = 1;
	fLinkCount++;
}


void
RepeaterMonitorView::_RebuildFilteredList()
{
	// This is called when node filter changes.
	// Since we don't store all packets separately, we just clear the
	// filter for now. A full rebuild would require storing all packets.
	if (fFilterNode.Length() == 0) {
		// No filter — user will need to reload
		// The filtering is applied on incoming packets via ProcessLine
	}
	// Note: filtering is applied live as packets arrive.
	// Click on a node sets the filter; new packets are filtered.
	// To see all packets again, clear the search field and reload.
}
