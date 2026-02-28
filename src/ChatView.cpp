/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatView.cpp — Chat message display view implementation
 */

#include "ChatView.h"

#include <Clipboard.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <Window.h>

#include <cstring>

#include "Constants.h"
#include "MessageView.h"


static const uint32 kMsgCopyText = 'cpyt';


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
ChatView::FrameResized(float newWidth, float newHeight)
{
	// Recalculate all item heights for the new width —
	// text wrapping depends on view width, so heights change on resize
	BFont font;
	GetFont(&font);

	for (int32 i = 0; i < CountItems(); i++)
		ItemAt(i)->Update(this, &font);

	BListView::FrameResized(newWidth, newHeight);
	Invalidate();
}


void
ChatView::MouseDown(BPoint where)
{
	BMessage* current = Window()->CurrentMessage();
	int32 buttons = 0;
	if (current != NULL)
		current->FindInt32("buttons", &buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		int32 index = IndexOf(where);
		if (index < 0) return;

		MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
		if (item == NULL) return;

		Select(index);

		BPopUpMenu* menu = new BPopUpMenu("context", false, false);

		BMessage* copyMsg = new BMessage(kMsgCopyText);
		copyMsg->AddInt32("index", index);
		menu->AddItem(new BMenuItem("Copy", copyMsg));

		menu->SetTargetForItems(this);

		ConvertToScreen(&where);
		menu->Go(where, true, true, true);
		return;
	}

	// Check for left-click on "X hops" link
	if (buttons & B_PRIMARY_MOUSE_BUTTON) {
		int32 index = IndexOf(where);
		if (index >= 0) {
			MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
			if (item != NULL && item->HasClickableHops()
				&& item->HopsClickRect().Contains(where)) {
				// Post trace path request to parent window
				BMessage msg(MSG_TRACE_PATH);
				msg.AddData("pubkey", B_RAW_TYPE,
					item->PubKeyPrefix(), kPubKeyPrefixSize);
				Window()->PostMessage(&msg);
				return;
			}
		}
	}

	BListView::MouseDown(where);
}


void
ChatView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgCopyText:
		{
			int32 index;
			if (message->FindInt32("index", &index) != B_OK)
				break;

			MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
			if (item == NULL)
				break;

			const char* text = item->Text();
			if (text == NULL || text[0] == '\0')
				break;

			if (be_clipboard->Lock()) {
				be_clipboard->Clear();
				BMessage* clip = be_clipboard->Data();
				clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
				be_clipboard->Commit();
				be_clipboard->Unlock();
			}
			break;
		}

		default:
			BListView::MessageReceived(message);
			break;
	}
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
ChatView::UpdateDeliveryStatus(int32 index, uint8 status, uint32 rtt)
{
	if (index < 0 || index >= CountItems())
		return;

	MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
	if (item == NULL)
		return;

	item->SetDeliveryStatus(status, rtt);
	InvalidateItem(index);
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


OwningObjectList<ChatMessage>*
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
