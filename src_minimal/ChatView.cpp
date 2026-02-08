/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatView.cpp — Chat message display view implementation
 */

#include "ChatView.h"

#include <ScrollView.h>
#include <Window.h>

#include <cstring>

#include "MessageView.h"


ChatView::ChatView(const char* name)
	:
	BListView(name, B_SINGLE_SELECTION_LIST),
	fCurrentContact(NULL),
	fCurrentContactName(""),
	fCurrentMessages(20)
{
}


ChatView::~ChatView()
{
	ClearMessages();
}


void
ChatView::AttachedToWindow()
{
	BListView::AttachedToWindow();

	// Light gray background for chat area
	SetViewColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
}


void
ChatView::MessageReceived(BMessage* message)
{
	BListView::MessageReceived(message);
}


void
ChatView::AddMessage(const ChatMessage& message, const char* senderName)
{
	// Store message in history
	ChatMessage* stored = new ChatMessage(message);
	fCurrentMessages.AddItem(stored);

	// Create and add visual item
	MessageView* item = new MessageView(message, senderName);
	AddItem(item);

	_ScrollToBottom();
}


void
ChatView::ClearMessages()
{
	// Remove visual items
	for (int32 i = CountItems() - 1; i >= 0; i--) {
		MessageView* item = dynamic_cast<MessageView*>(RemoveItem(i));
		delete item;
	}

	// Clear stored messages
	fCurrentMessages.MakeEmpty();
}


void
ChatView::SetCurrentContact(ContactInfo* contact)
{
	fCurrentContact = contact;
	if (contact != NULL)
		fCurrentContactName = contact->name;
	else
		fCurrentContactName = "";

	// Clear current view and load contact's message history
	ClearMessages();

	if (contact != NULL) {
		// Load messages from contact's history
		for (int32 i = 0; i < contact->messages.CountItems(); i++) {
			ChatMessage* msg = contact->messages.ItemAt(i);
			if (msg != NULL) {
				const char* senderName = msg->isOutgoing ? "Me" : contact->name;
				MessageView* item = new MessageView(*msg, senderName);
				AddItem(item);
			}
		}
		_ScrollToBottom();
	}
}


BObjectList<ChatMessage, true>*
ChatView::GetMessageHistory()
{
	return &fCurrentMessages;
}


void
ChatView::_ScrollToBottom()
{
	if (CountItems() == 0)
		return;

	// Scroll to show last item
	int32 lastIndex = CountItems() - 1;
	ScrollToSelection();
	Select(lastIndex);
	Deselect(lastIndex);

	// Also try to scroll the parent scroll view
	BScrollView* scrollView = dynamic_cast<BScrollView*>(Parent());
	if (scrollView != NULL) {
		BScrollBar* vScroll = scrollView->ScrollBar(B_VERTICAL);
		if (vScroll != NULL) {
			float min, max;
			vScroll->GetRange(&min, &max);
			vScroll->SetValue(max);
		}
	}
}
