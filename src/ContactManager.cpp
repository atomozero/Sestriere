/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactManager.cpp — Contact storage, lookup, and sync logic
 */

#include "ContactManager.h"

#include <Autolock.h>

#include <stdio.h>
#include <string.h>


ContactManager::ContactManager()
	:
	fLock("ContactManager"),
	fContacts(true),
	fOldContacts(true),
	fSyncing(false)
{
}


ContactManager::~ContactManager()
{
}


int32
ContactManager::CountContacts() const
{
	BAutolock lock(fLock);
	return fContacts.CountItems();
}


ContactInfo*
ContactManager::ContactAt(int32 index) const
{
	BAutolock lock(fLock);
	return fContacts.ItemAt(index);
}


ContactInfo*
ContactManager::FindByPrefix(const uint8* prefix, size_t prefixLen) const
{
	BAutolock lock(fLock);
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* c = fContacts.ItemAt(i);
		if (memcmp(c->publicKey, prefix, prefixLen) == 0)
			return c;
	}
	return NULL;
}


ContactInfo*
ContactManager::FindByKeyHex(const char* keyHex) const
{
	if (keyHex == NULL || strlen(keyHex) < 12)
		return NULL;

	uint8 prefix[6];
	for (int i = 0; i < 6; i++) {
		unsigned int byte;
		if (sscanf(keyHex + i * 2, "%2x", &byte) != 1)
			return NULL;
		prefix[i] = (uint8)byte;
	}
	return FindByPrefix(prefix, 6);
}


void
ContactManager::BeginSync()
{
	BAutolock lock(fLock);
	fSyncing = true;
	// Move current contacts to old list for comparison after sync
	while (fContacts.CountItems() > 0) {
		ContactInfo* c = fContacts.RemoveItemAt(0);
		fOldContacts.AddItem(c);
	}
}


void
ContactManager::AddContact(const ContactInfo& info)
{
	BAutolock lock(fLock);
	ContactInfo* copy = new ContactInfo(info);
	fContacts.AddItem(copy);
}


void
ContactManager::EndSync()
{
	BAutolock lock(fLock);
	fSyncing = false;
	// Clear old contacts — they've been replaced
	fOldContacts.MakeEmpty(true);
}


void
ContactManager::RemoveContact(const uint8* pubKey)
{
	BAutolock lock(fLock);
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* c = fContacts.ItemAt(i);
		if (memcmp(c->publicKey, pubKey, 32) == 0) {
			fContacts.RemoveItemAt(i);
			delete c;
			return;
		}
	}
}


void
ContactManager::Clear()
{
	BAutolock lock(fLock);
	fContacts.MakeEmpty(true);
	fOldContacts.MakeEmpty(true);
	fSyncing = false;
}


// static
void
ContactManager::PubKeyToHex(const uint8* pubKey, char* outHex, size_t hexLen)
{
	size_t maxBytes = (hexLen - 1) / 2;
	if (maxBytes > 32)
		maxBytes = 32;
	for (size_t i = 0; i < maxBytes; i++)
		snprintf(outHex + i * 2, 3, "%02x", pubKey[i]);
	outHex[maxBytes * 2] = '\0';
}


// static
BString
ContactManager::PubKeyPrefix(const uint8* pubKey)
{
	char hex[13];
	PubKeyToHex(pubKey, hex, sizeof(hex));
	return BString(hex);
}
