/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GrowingTextView.cpp — Auto-growing text input that sends a message on Enter
 */

#include "GrowingTextView.h"

#include <Window.h>

#include <algorithm>


static const float kMaxLines = 4.0f;


GrowingTextView::GrowingTextView(const char* name, BMessage* enterMessage)
	:
	BTextView(name, B_WILL_DRAW | B_PULSE_NEEDED),
	fEnterMessage(enterMessage),
	fModificationMessage(NULL),
	fEnabled(true),
	fMinHeight(0),
	fMaxHeight(0)
{
	SetWordWrap(true);
	SetStylable(false);

	// Calculate line heights
	BFont font;
	GetFont(&font);
	font_height fh;
	font.GetHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	fMinHeight = lineHeight + 8;  // 1 line + padding
	fMaxHeight = lineHeight * kMaxLines + 8;

	SetExplicitMinSize(BSize(100, fMinHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, fMaxHeight));

	// Insets for text
	SetInsets(4, 4, 4, 4);
}


GrowingTextView::~GrowingTextView()
{
	delete fEnterMessage;
	delete fModificationMessage;
}


void
GrowingTextView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes == 1 && bytes[0] == B_RETURN) {
		// Check for Shift+Enter (insert newline)
		int32 modifiers = 0;
		BMessage* msg = Window()->CurrentMessage();
		if (msg != NULL)
			msg->FindInt32("modifiers", &modifiers);

		if (modifiers & B_SHIFT_KEY) {
			// Shift+Enter: insert newline
			BTextView::KeyDown(bytes, numBytes);
			return;
		}

		// Enter alone: send message
		if (fEnterMessage != NULL && fEnabled && TextLength() > 0) {
			BMessage copy(*fEnterMessage);
			Window()->PostMessage(&copy);
		}
		return;
	}

	if (!fEnabled)
		return;

	BTextView::KeyDown(bytes, numBytes);
}


void
GrowingTextView::InsertText(const char* text, int32 length, int32 offset,
	const text_run_array* runs)
{
	BTextView::InsertText(text, length, offset, runs);
	_RecalcHeight();
	_NotifyModification();
}


void
GrowingTextView::DeleteText(int32 fromOffset, int32 toOffset)
{
	BTextView::DeleteText(fromOffset, toOffset);
	_RecalcHeight();
	_NotifyModification();
}


BSize
GrowingTextView::MinSize()
{
	return BSize(100, fMinHeight);
}


BSize
GrowingTextView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, fMaxHeight);
}


BSize
GrowingTextView::PreferredSize()
{
	float textHeight = TextHeight(0, TextLength());
	float height = std::max(fMinHeight, std::min(textHeight + 8, fMaxHeight));
	return BSize(B_SIZE_UNLIMITED, height);
}


void
GrowingTextView::SetSendMessage(BMessage* message)
{
	delete fEnterMessage;
	fEnterMessage = message;
}


void
GrowingTextView::SetModificationMessage(BMessage* message)
{
	delete fModificationMessage;
	fModificationMessage = message;
}


void
GrowingTextView::SetEnabled(bool enabled)
{
	fEnabled = enabled;
	MakeEditable(enabled);

	if (enabled) {
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
	} else {
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetHighUIColor(B_PANEL_TEXT_COLOR);
	}

	Invalidate();
}


void
GrowingTextView::_NotifyModification()
{
	if (fModificationMessage != NULL && Window() != NULL) {
		BMessage copy(*fModificationMessage);
		Window()->PostMessage(&copy);
	}
}


void
GrowingTextView::_RecalcHeight()
{
	float textHeight = TextHeight(0, TextLength());
	float desired = std::max(fMinHeight, std::min(textHeight + 8, fMaxHeight));

	BSize current = ExplicitPreferredSize();

	if (current.height != desired) {
		SetExplicitPreferredSize(BSize(B_SIZE_UNLIMITED, desired));
		if (Parent() != NULL)
			Parent()->InvalidateLayout();
	}
}
