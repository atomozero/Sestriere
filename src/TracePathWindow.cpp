/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TracePathWindow.cpp — Trace path visualization window implementation
 */

#include "TracePathWindow.h"

#include <Application.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <Region.h>
#include <ScrollView.h>
#include <StringView.h>
#include <View.h>

#include <cstring>
#include <cstdio>

#include "Constants.h"


static const uint32 MSG_START_TRACE = 'sttr';
static const uint32 MSG_TRACE_TIMEOUT = 'trto';
static const bigtime_t kTraceTimeoutUs = 30000000LL;  // 30 seconds

// Layout constants for TracePathView
static const float kNodeCardH = 40.0f;
static const float kArrowH = 28.0f;
static const float kAvatarSize = 24.0f;
static const float kLeftMargin = 12.0f;
static const float kSnrPillH = 16.0f;
static const float kSnrPillRadius = 4.0f;
static const float kTopPadding = 8.0f;


// =============================================================================
// TracePathView — Custom BView that draws node cards and arrows
// =============================================================================

class TracePathView : public BView {
public:
							TracePathView();

	virtual void			Draw(BRect updateRect);
	virtual BSize			MinSize();

			void			SetHops(OwningObjectList<TraceHop>* hops,
								const char* selfName, const char* destName,
								uint8 destType);

private:
			void			_DrawNodeCard(float y, const char* name,
								const char* subtitle, bool isDest);
			void			_DrawArrow(float y, int8 snrRaw, bool hasSnr);
			rgb_color		_SnrColor(float snrDb);
			rgb_color		_AvatarColor(const char* name);
			void			_DrawAvatar(BPoint center, const char* name);

			OwningObjectList<TraceHop>*	fHops;
			char			fSelfName[64];
			char			fDestName[64];
			uint8			fDestType;
};


TracePathView::TracePathView()
	:
	BView("trace_path_view", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fHops(NULL),
	fDestType(0)
{
	fSelfName[0] = '\0';
	fDestName[0] = '\0';
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


void
TracePathView::SetHops(OwningObjectList<TraceHop>* hops, const char* selfName,
	const char* destName, uint8 destType)
{
	fHops = hops;
	strlcpy(fSelfName, selfName ? selfName : "Self", sizeof(fSelfName));
	strlcpy(fDestName, destName ? destName : "Unknown", sizeof(fDestName));
	fDestType = destType;

	// Calculate total height: self card + (arrow + hop card) * N + arrow + dest card
	int32 hopCount = fHops ? fHops->CountItems() : 0;
	float totalH = kTopPadding + kNodeCardH;  // Self card
	if (hopCount > 0) {
		// Each hop: arrow + card
		totalH += hopCount * (kArrowH + kNodeCardH);
		// Final arrow + destination card
		totalH += kArrowH + kNodeCardH;
	}
	totalH += kTopPadding;

	SetExplicitMinSize(BSize(200, totalH));
	SetExplicitPreferredSize(BSize(B_SIZE_UNLIMITED, totalH));
}


BSize
TracePathView::MinSize()
{
	return ExplicitMinSize();
}


void
TracePathView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	rgb_color bgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetHighColor(bgColor);
	FillRect(bounds);

	if (fHops == NULL || fHops->CountItems() == 0)
		return;

	float y = kTopPadding;

	// Draw Self node card
	_DrawNodeCard(y, fSelfName, "Start", false);

	// Draw hops
	int32 count = fHops->CountItems();

	// Determine if hops have a final "Destination" entry
	// (ParseTraceData adds it), detect by checking if last hop has zeroed prefix
	bool hasDestHop = false;
	int32 lastIdx = count - 1;
	if (lastIdx >= 0) {
		TraceHop* lastHop = fHops->ItemAt(lastIdx);
		if (lastHop != NULL && lastHop->pubKeyPrefix[0] == 0)
			hasDestHop = true;
	}

	int32 intermediateCount = hasDestHop ? count - 1 : count;

	for (int32 i = 0; i < intermediateCount; i++) {
		TraceHop* hop = fHops->ItemAt(i);
		if (hop == NULL)
			continue;

		// Arrow from previous card
		float arrowY = y + kNodeCardH;
		_DrawArrow(arrowY, hop->snr, hop->hasSnr);
		y = arrowY + kArrowH;

		// Hop subtitle
		char subtitle[32];
		// Determine type string from name if it contains type info
		snprintf(subtitle, sizeof(subtitle), "Hop %d", (int)(i + 1));

		_DrawNodeCard(y, hop->name, subtitle, false);
	}

	// Draw final arrow + destination card
	if (hasDestHop) {
		TraceHop* destHop = fHops->ItemAt(lastIdx);
		float arrowY = y + kNodeCardH;
		_DrawArrow(arrowY, destHop ? destHop->snr : 0,
			destHop ? destHop->hasSnr : false);
		y = arrowY + kArrowH;
	} else {
		float arrowY = y + kNodeCardH;
		_DrawArrow(arrowY, 0, false);
		y = arrowY + kArrowH;
	}

	_DrawNodeCard(y, fDestName, "Destination", true);
}


void
TracePathView::_DrawNodeCard(float y, const char* name, const char* subtitle,
	bool isDest)
{
	float avatarCenterX = kLeftMargin + kAvatarSize / 2.0f;
	float avatarCenterY = y + kNodeCardH / 2.0f;

	// Draw avatar circle
	_DrawAvatar(BPoint(avatarCenterX, avatarCenterY), name);

	// Draw name (bold)
	float textX = kLeftMargin + kAvatarSize + 8.0f;
	float nameY = y + 16.0f;

	BFont boldFont(be_plain_font);
	boldFont.SetFace(B_BOLD_FACE);
	boldFont.SetSize(12.0f);
	SetFont(&boldFont);

	rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
	SetHighColor(textColor);

	// Extract clean name (strip hex prefix part if present)
	char cleanName[64];
	strlcpy(cleanName, name, sizeof(cleanName));
	// If name looks like "NodeName (AABBCCDD)" keep the full thing
	// If it looks like "Hop #N (0xAB)" or "Hop #N (AABBCCDD)", use just the name
	DrawString(cleanName, BPoint(textX, nameY));

	// Draw subtitle (dimmed)
	BFont smallFont(be_plain_font);
	smallFont.SetSize(10.0f);
	SetFont(&smallFont);

	rgb_color dimColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_2_TINT);
	SetHighColor(dimColor);

	float subtitleY = y + 30.0f;
	DrawString(subtitle, BPoint(textX, subtitleY));
}


void
TracePathView::_DrawAvatar(BPoint center, const char* name)
{
	rgb_color avatarBg = _AvatarColor(name);
	SetHighColor(avatarBg);

	float radius = kAvatarSize / 2.0f;
	FillEllipse(center, radius, radius);

	// Draw initials (up to 2 chars) in white
	char initials[3] = {0};
	if (name != NULL && name[0] != '\0') {
		// Skip "Hop #N " prefix if present
		const char* displayName = name;
		if (strncmp(name, "Hop #", 5) == 0) {
			// Look for first '(' to find hex prefix
			const char* paren = strchr(name, '(');
			if (paren != NULL)
				displayName = paren + 1;
		}

		initials[0] = (displayName[0] >= 'a' && displayName[0] <= 'z')
			? displayName[0] - 32 : displayName[0];

		// Find second initial (after space)
		const char* space = strchr(displayName, ' ');
		if (space != NULL && space[1] != '\0') {
			initials[1] = (space[1] >= 'a' && space[1] <= 'z')
				? space[1] - 32 : space[1];
		}
	}

	if (initials[0] != '\0') {
		BFont initFont(be_bold_font);
		initFont.SetSize(10.0f);
		SetFont(&initFont);

		float strWidth = StringWidth(initials);
		font_height fh;
		initFont.GetHeight(&fh);

		SetHighColor(255, 255, 255);
		DrawString(initials,
			BPoint(center.x - strWidth / 2.0f,
				center.y + fh.ascent / 2.0f - 1.0f));
	}
}


void
TracePathView::_DrawArrow(float y, int8 snrRaw, bool hasSnr)
{
	float centerX = kLeftMargin + kAvatarSize / 2.0f;
	float lineTop = y + 2.0f;
	float lineBottom = y + kArrowH - 2.0f;

	rgb_color lineColor;
	if (hasSnr) {
		float snrDb = snrRaw / 4.0f;
		lineColor = _SnrColor(snrDb);
	} else {
		lineColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT);
	}

	// Draw vertical line
	SetHighColor(lineColor);
	SetPenSize(2.0f);
	StrokeLine(BPoint(centerX, lineTop), BPoint(centerX, lineBottom));
	SetPenSize(1.0f);

	// Draw arrowhead
	float arrowSize = 4.0f;
	BPoint tip(centerX, lineBottom);
	BPoint left(centerX - arrowSize, lineBottom - arrowSize);
	BPoint right(centerX + arrowSize, lineBottom - arrowSize);
	FillTriangle(tip, left, right);

	// Draw SNR pill if SNR data available
	if (hasSnr) {
		float snrDb = snrRaw / 4.0f;
		char snrText[24];
		snprintf(snrText, sizeof(snrText), "SNR: %.1f dB", (double)snrDb);

		BFont pillFont(be_plain_font);
		pillFont.SetSize(9.0f);
		SetFont(&pillFont);

		float textWidth = StringWidth(snrText);
		float pillW = textWidth + 10.0f;
		float pillX = centerX + kAvatarSize / 2.0f + 8.0f;
		float pillY = y + (kArrowH - kSnrPillH) / 2.0f;

		BRect pillRect(pillX, pillY, pillX + pillW, pillY + kSnrPillH);

		rgb_color pillColor = _SnrColor(snrDb);
		SetHighColor(pillColor);
		FillRoundRect(pillRect, kSnrPillRadius, kSnrPillRadius);

		// White text on colored pill
		SetHighColor(255, 255, 255);
		font_height fh;
		pillFont.GetHeight(&fh);
		float textY = pillY + (kSnrPillH + fh.ascent - fh.descent) / 2.0f;
		DrawString(snrText, BPoint(pillX + 5.0f, textY));
	}
}


rgb_color
TracePathView::_SnrColor(float snrDb)
{
	if (snrDb >= (float)kSnrExcellent)
		return kColorGood;
	else if (snrDb >= (float)kSnrGood)
		return kColorFair;
	else if (snrDb >= (float)kSnrFair)
		return kColorPoor;
	else
		return kColorBad;
}


rgb_color
TracePathView::_AvatarColor(const char* name)
{
	if (name == NULL || name[0] == '\0')
		return kAvatarPalette[0];

	// Simple hash on name
	uint32 hash = 0;
	for (const char* p = name; *p != '\0'; p++)
		hash = hash * 31 + (uint8)*p;

	return kAvatarPalette[hash % kAvatarPaletteCount];
}


// =============================================================================
// TracePathWindow
// =============================================================================

TracePathWindow::TracePathWindow(BWindow* parent, const ContactInfo* contact)
	:
	BWindow(BRect(0, 0, 380, 320), "Trace Path",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fParent(parent),
	fTargetLabel(NULL),
	fPathView(NULL),
	fTraceButton(NULL),
	fCloseButton(NULL),
	fStatusLabel(NULL),
	fHops(10),
	fTracing(false),
	fTimeoutRunner(NULL)
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

	fPathView = new TracePathView();

	BScrollView* scrollView = new BScrollView("path_scroll", fPathView,
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
	delete fTimeoutRunner;
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

		case MSG_TRACE_TIMEOUT:
			if (fTracing)
				SetTraceComplete(false);
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
	_UpdatePathView();
}


void
TracePathWindow::SetTraceComplete(bool success)
{
	// Cancel timeout timer
	delete fTimeoutRunner;
	fTimeoutRunner = NULL;

	fTracing = false;
	fTraceButton->SetEnabled(true);
	fTraceButton->SetLabel("Retry Trace");

	if (success) {
		BString status;
		status.SetToFormat("Live trace: %d hop(s)", (int)fHops.CountItems());
		fStatusLabel->SetText(status.String());
	} else {
		BString status;
		status.SetToFormat("No live response — showing known path (%d hop(s))",
			(int)fHops.CountItems());
		fStatusLabel->SetText(status.String());
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
	// pathLen>=4: N hops x 4-byte pubkey prefix (numHops = pathLen / 4)

	if (length < 12) {
		// Direct path or too short
		SetTraceComplete(false);
		return;
	}

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

		if (snrOffset + i < length) {
			hop.snr = (int8)data[snrOffset + i];
			hop.hasSnr = true;
		}

		hop.timestamp = (uint32)real_time_clock();
		AddTraceHop(hop);
	}

	// Last SNR entry is the destination's SNR
	if (numHops > 0 && snrOffset + numHops < length) {
		TraceHop destHop;
		memset(destHop.pubKeyPrefix, 0, sizeof(destHop.pubKeyPrefix));
		destHop.snr = (int8)data[snrOffset + numHops];
		destHop.hasSnr = true;
		snprintf(destHop.name, sizeof(destHop.name), "Destination");
		destHop.timestamp = (uint32)real_time_clock();
		AddTraceHop(destHop);
	}

	// Mark trace as complete
	if (numHops == 0) {
		fStatusLabel->SetText("Direct path — no intermediate hops");
		fTracing = false;
		fTraceButton->SetEnabled(true);
		fTraceButton->SetLabel("Retry Trace");
		delete fTimeoutRunner;
		fTimeoutRunner = NULL;
	} else {
		SetTraceComplete(true);
	}
}


void
TracePathWindow::SetContact(const ContactInfo* contact)
{
	if (contact == NULL)
		return;

	// Cancel any in-progress trace
	delete fTimeoutRunner;
	fTimeoutRunner = NULL;
	fTracing = false;

	fContact = *contact;
	BString titleStr;
	titleStr.SetToFormat("Trace path to: %s",
		fContact.name[0] ? fContact.name : "Unknown");
	fTargetLabel->SetText(titleStr.String());

	// Clear previous results
	fHops.MakeEmpty();
	_UpdatePathView();

	fTraceButton->SetEnabled(true);
	fTraceButton->SetLabel("Start Trace");
	fStatusLabel->SetText("Press Start to begin trace");
}


void
TracePathWindow::StartExternalTrace(const ContactInfo* contact)
{
	// Cancel any in-progress trace (timeout, state)
	delete fTimeoutRunner;
	fTimeoutRunner = NULL;

	// Update contact info (window may be reused for a different contact)
	if (contact != NULL) {
		fContact = *contact;
		BString titleStr;
		titleStr.SetToFormat("Trace path to: %s",
			fContact.name[0] ? fContact.name : "Unknown");
		fTargetLabel->SetText(titleStr.String());
	}

	// Clear previous results
	fHops.MakeEmpty();
	_UpdatePathView();

	// Direct path — no hops to trace
	if (fContact.outPathLen <= 0) {
		fTracing = false;
		fTraceButton->SetEnabled(true);
		fTraceButton->SetLabel("Start Trace");
		fStatusLabel->SetText("Direct path — no intermediate hops");
		return;
	}

	// Show known path immediately from stored outPath data
	_PopulateKnownPath();

	// Set tracing state — trace command already sent by MainWindow
	fTracing = true;
	fTraceButton->SetEnabled(false);
	fTraceButton->SetLabel("Tracing...");

	BString statusStr;
	statusStr.SetToFormat("Known path: %d hop(s) — waiting for live trace...",
		(int)fContact.outPathLen);
	fStatusLabel->SetText(statusStr.String());

	// Start timeout timer
	BMessage timeoutMsg(MSG_TRACE_TIMEOUT);
	fTimeoutRunner = new BMessageRunner(this, &timeoutMsg,
		kTraceTimeoutUs, 1);
}


void
TracePathWindow::ResolveHopNames(const OwningObjectList<ContactInfo>* contacts)
{
	if (contacts == NULL)
		return;

	for (int32 i = 0; i < fHops.CountItems(); i++) {
		TraceHop* hop = fHops.ItemAt(i);
		if (hop == NULL)
			continue;

		// Skip if prefix is all zeros (e.g. "Destination" hop)
		if (hop->pubKeyPrefix[0] == 0)
			continue;

		// Determine match length: 4-byte prefix (from live trace)
		// vs 1-byte hash (from known path where bytes 1-3 are 0)
		bool is4byte = (hop->pubKeyPrefix[1] != 0
			|| hop->pubKeyPrefix[2] != 0 || hop->pubKeyPrefix[3] != 0);
		size_t matchLen = is4byte ? 4 : 1;

		for (int32 c = 0; c < contacts->CountItems(); c++) {
			ContactInfo* contact = contacts->ItemAt(c);
			if (contact == NULL || !contact->isValid)
				continue;

			if (memcmp(contact->publicKey, hop->pubKeyPrefix, matchLen) == 0) {
				if (is4byte) {
					snprintf(hop->name, sizeof(hop->name),
						"%.50s (%02X%02X%02X%02X)",
						contact->name,
						hop->pubKeyPrefix[0], hop->pubKeyPrefix[1],
						hop->pubKeyPrefix[2], hop->pubKeyPrefix[3]);
				} else {
					snprintf(hop->name, sizeof(hop->name),
						"%.50s (0x%02X)",
						contact->name, hop->pubKeyPrefix[0]);
				}
				break;
			}
		}
	}

	_UpdatePathView();
}


void
TracePathWindow::_OnStartTrace()
{
	if (fTracing)
		return;

	// Send trace path request to parent window
	if (fParent != NULL) {
		BMessage msg(MSG_TRACE_PATH);
		msg.AddData("pubkey", B_RAW_TYPE, fContact.publicKey, kPubKeyPrefixSize);
		fParent->PostMessage(&msg);
	}
}


void
TracePathWindow::_UpdatePathView()
{
	// Determine self name from the window title context
	const char* selfName = "You";
	const char* destName = fContact.name[0] ? fContact.name : "Unknown";

	fPathView->SetHops(&fHops, selfName, destName, fContact.type);
	fPathView->Invalidate();
}


void
TracePathWindow::_PopulateKnownPath()
{
	fHops.MakeEmpty();

	int numHops = fContact.outPathLen;
	if (numHops <= 0 || numHops > 16)
		return;

	for (int i = 0; i < numHops; i++) {
		TraceHop hop;
		memset(hop.pubKeyPrefix, 0, sizeof(hop.pubKeyPrefix));
		// outPath stores 1-byte hashes (publicKey[0] of each hop)
		hop.pubKeyPrefix[0] = fContact.outPath[i];
		hop.snr = 0;
		hop.hasSnr = false;  // Known path has no SNR data
		hop.timestamp = (uint32)real_time_clock();
		snprintf(hop.name, sizeof(hop.name),
			"Hop #%d (0x%02X)", i + 1, fContact.outPath[i]);
		fHops.AddItem(new TraceHop(hop));
	}

	_UpdatePathView();
}
