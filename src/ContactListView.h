/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactListView.h — List view for contacts
 */

#ifndef CONTACTLISTVIEW_H
#define CONTACTLISTVIEW_H

#include <ListView.h>
#include <ObjectList.h>

#include "Types.h"

class ContactItem;

class ContactListView : public BListView {
public:
							ContactListView(const char* name);
	virtual					~ContactListView();

	virtual void			SelectionChanged();
	virtual void			MessageReceived(BMessage* message);
	virtual void			MouseDown(BPoint where);

			void			AddContact(const Contact& contact);
			void			UpdateContact(const Contact& contact);
			void			RemoveContact(const uint8* publicKey);
			void			ClearContacts();

			Contact*		ContactAt(int32 index) const;
			Contact*		ContactByPubKeyPrefix(const uint8* prefix) const;
			int32			CountContacts() const;

private:
			ContactItem*	_FindByPublicKey(const uint8* publicKey) const;
			ContactItem*	_FindByPubKeyPrefix(const uint8* prefix) const;
};

#endif // CONTACTLISTVIEW_H
