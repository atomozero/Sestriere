/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactManager.h — Contact storage, lookup, and sync logic
 */

#ifndef _CONTACT_MANAGER_H
#define _CONTACT_MANAGER_H

#include <Locker.h>
#include <String.h>

#include "Types.h"


class ContactManager {
public:
							ContactManager();
							~ContactManager();

	// Contact list access
	int32					CountContacts() const;
	ContactInfo*			ContactAt(int32 index) const;
	ContactInfo*			FindByPrefix(const uint8* prefix,
								size_t prefixLen) const;
	ContactInfo*			FindByKeyHex(const char* keyHex) const;

	// Sync lifecycle
	void					BeginSync();
	void					AddContact(const ContactInfo& info);
	void					EndSync();
	bool					IsSyncing() const { return fSyncing; }

	// Direct manipulation
	void					AddItem(ContactInfo* contact);
	ContactInfo*			RemoveItemAt(int32 index);
	void					RemoveContact(const uint8* pubKey);
	void					Clear();

	// Direct list access (for iteration patterns in MainWindow)
	OwningObjectList<ContactInfo>&		Contacts() { return fContacts; }
	OwningObjectList<ContactInfo>&		OldContacts() { return fOldContacts; }

	// Key helpers
	static void				PubKeyToHex(const uint8* pubKey,
								char* outHex, size_t hexLen);
	static BString			PubKeyPrefix(const uint8* pubKey);

	// Thread safety (for callers that do multiple operations)
	bool					Lock() { return fLock.Lock(); }
	void					Unlock() { fLock.Unlock(); }

private:
	mutable BLocker			fLock;
	OwningObjectList<ContactInfo>	fContacts;
	OwningObjectList<ContactInfo>	fOldContacts;
	bool					fSyncing;
};


#endif // _CONTACT_MANAGER_H
