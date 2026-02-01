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

#include "Constants.h"
#include "MessageStore.h"
#include "MessageView.h"


ChatView::ChatView(const char* name)
	:
	BListView(name, B_SINGLE_SELECTION_LIST),
	fCurrentContact(NULL),
	fCurrentContactName("")
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

	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}


void
ChatView::MessageReceived(BMessage* message)
{
	BListView::MessageReceived(message);
}


void
ChatView::AddMessage(const ReceivedMessage& message, bool outgoing)
{
	const char* senderName = NULL;

	if (!outgoing) {
		// Try to find sender name from pub key prefix
		senderName = _FindContactName(message.pubKeyPrefix);
		if (senderName == NULL)
			senderName = "Unknown";
	}

	MessageView* item = new MessageView(message, outgoing, senderName);
	AddItem(item);

	// Save message to persistent storage
	if (fCurrentContact != NULL) {
		MessageStore::Instance()->SaveMessage(
			fCurrentContact->publicKey, message, outgoing, senderName);
	}

	_ScrollToBottom();
}


void
ChatView::ClearMessages()
{
	for (int32 i = CountItems() - 1; i >= 0; i--) {
		MessageView* item = dynamic_cast<MessageView*>(RemoveItem(i));
		delete item;
	}
}


void
ChatView::SetCurrentContact(const Contact* contact)
{
	fCurrentContact = contact;
	if (contact != NULL)
		fCurrentContactName = contact->advName;
	else
		fCurrentContactName = "";

	ClearMessages();

	// Load message history for this contact
	if (contact != NULL) {
		BObjectList<StoredMessage> messages(true);
		if (MessageStore::Instance()->LoadMessages(contact->publicKey, messages) == B_OK) {
			for (int32 i = 0; i < messages.CountItems(); i++) {
				StoredMessage* stored = messages.ItemAt(i);
				MessageView* item = new MessageView(
					stored->message,
					stored->outgoing,
					stored->senderName.String());
				AddItem(item);
			}
		}

		// Mark as read when viewing
		MessageStore::Instance()->MarkAsRead(contact->publicKey);

		_ScrollToBottom();
	}
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


const char*
ChatView::_FindContactName(const uint8* pubKeyPrefix) const
{
	// TODO: Look up contact name from ContactListView
	// For now, return NULL and use "Unknown"

	if (fCurrentContact != NULL) {
		if (memcmp(fCurrentContact->publicKey, pubKeyPrefix,
				kPubKeyPrefixSize) == 0)
			return fCurrentContact->advName;
	}

	return NULL;
}
