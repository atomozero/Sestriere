/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactListView.cpp — List view for contacts implementation
 */

#include "ContactListView.h"

#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#include <cstring>

#include "Constants.h"
#include "ContactItem.h"


ContactListView::ContactListView(const char* name)
	:
	BListView(name, B_SINGLE_SELECTION_LIST)
{
}


ContactListView::~ContactListView()
{
	ClearContacts();
}


void
ContactListView::SelectionChanged()
{
	BListView::SelectionChanged();

	int32 selection = CurrentSelection();

	BMessage msg(MSG_CONTACT_SELECTED);
	msg.AddInt32("index", selection);

	if (selection >= 0) {
		Contact* contact = ContactAt(selection);
		if (contact != NULL) {
			msg.AddData(kFieldContact, B_RAW_TYPE, contact, sizeof(Contact));
			msg.AddString(kFieldName, contact->advName);
		}
	}

	if (Window() != NULL)
		Window()->PostMessage(&msg);
}


void
ContactListView::MessageReceived(BMessage* message)
{
	BListView::MessageReceived(message);
}


void
ContactListView::MouseDown(BPoint where)
{
	// Check for right-click
	BMessage* currentMsg = Window()->CurrentMessage();
	int32 buttons = 0;
	if (currentMsg != NULL)
		currentMsg->FindInt32("buttons", &buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		// Find which item was clicked
		int32 index = IndexOf(where);
		if (index >= 0) {
			// Select the item
			Select(index);

			// Show context menu
			BPopUpMenu* menu = new BPopUpMenu("ContactMenu", false, false);

			menu->AddItem(new BMenuItem("Send Message",
				new BMessage(MSG_CONTACT_SELECTED)));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Login to Repeater/Room",
				new BMessage(MSG_SHOW_LOGIN)));
			menu->AddItem(new BMenuItem("Trace Path",
				new BMessage(MSG_SHOW_TRACE_PATH)));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Export Contact",
				new BMessage(MSG_EXPORT_CONTACT)));

			// Convert to screen coordinates
			ConvertToScreen(&where);
			menu->SetTargetForItems(Window());

			BMenuItem* selected = menu->Go(where, false, true);
			if (selected != NULL) {
				// Item was selected, message already sent
			}

			delete menu;
		}
		return;
	}

	BListView::MouseDown(where);
}


void
ContactListView::AddContact(const Contact& contact)
{
	// Check if contact already exists
	ContactItem* existing = _FindByPublicKey(contact.publicKey);
	if (existing != NULL) {
		// Update existing contact
		existing->SetContact(contact);
		Invalidate();
		return;
	}

	// Add new contact
	ContactItem* item = new ContactItem(contact);
	AddItem(item);
}


void
ContactListView::UpdateContact(const Contact& contact)
{
	ContactItem* item = _FindByPublicKey(contact.publicKey);
	if (item != NULL) {
		item->SetContact(contact);
		Invalidate();
	}
}


void
ContactListView::RemoveContact(const uint8* publicKey)
{
	ContactItem* item = _FindByPublicKey(publicKey);
	if (item != NULL) {
		RemoveItem(item);
		delete item;
	}
}


void
ContactListView::ClearContacts()
{
	// Delete all items
	for (int32 i = CountItems() - 1; i >= 0; i--) {
		ContactItem* item = dynamic_cast<ContactItem*>(RemoveItem(i));
		delete item;
	}
}


Contact*
ContactListView::ContactAt(int32 index) const
{
	ContactItem* item = dynamic_cast<ContactItem*>(ItemAt(index));
	if (item == NULL)
		return NULL;

	return &item->GetContact();
}


Contact*
ContactListView::ContactByPubKeyPrefix(const uint8* prefix) const
{
	ContactItem* item = _FindByPubKeyPrefix(prefix);
	if (item == NULL)
		return NULL;

	return &item->GetContact();
}


int32
ContactListView::CountContacts() const
{
	return CountItems();
}


ContactItem*
ContactListView::_FindByPublicKey(const uint8* publicKey) const
{
	for (int32 i = 0; i < CountItems(); i++) {
		ContactItem* item = dynamic_cast<ContactItem*>(ItemAt(i));
		if (item == NULL)
			continue;

		if (memcmp(item->GetContact().publicKey, publicKey,
				kPublicKeySize) == 0)
			return item;
	}

	return NULL;
}


ContactItem*
ContactListView::_FindByPubKeyPrefix(const uint8* prefix) const
{
	for (int32 i = 0; i < CountItems(); i++) {
		ContactItem* item = dynamic_cast<ContactItem*>(ItemAt(i));
		if (item == NULL)
			continue;

		if (memcmp(item->GetContact().publicKey, prefix,
				kPubKeyPrefixSize) == 0)
			return item;
	}

	return NULL;
}
