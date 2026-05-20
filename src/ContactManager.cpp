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

	// Use O(1) hashmap for standard 6-byte prefix lookups
	if (prefixLen == kPubKeyPrefixSize && !fPrefixIndex.empty()) {
		char hex[13];
		PubKeyToHex(prefix, hex, sizeof(hex));
		auto it = fPrefixIndex.find(std::string(hex, 12));
		if (it != fPrefixIndex.end())
			return it->second;
		return NULL;
	}

	// Fallback to linear scan for non-standard prefix lengths
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
	// Rebuild O(1) lookup index
	RebuildIndex();
}


void
ContactManager::AddItem(ContactInfo* contact)
{
	BAutolock lock(fLock);
	fContacts.AddItem(contact);
}


ContactInfo*
ContactManager::RemoveItemAt(int32 index)
{
	BAutolock lock(fLock);
	return fContacts.RemoveItemAt(index);
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
	size_t maxBytes = (hexLen > 0) ? (hexLen - 1) / 2 : 0;
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


void
ContactManager::RebuildIndex()
{
	fPrefixIndex.clear();
	for (int32 i = 0; i < fContacts.CountItems(); i++) {
		ContactInfo* c = fContacts.ItemAt(i);
		if (c != NULL) {
			char hex[13];
			PubKeyToHex(c->publicKey, hex, sizeof(hex));
			fPrefixIndex[std::string(hex, 12)] = c;
		}
	}
}
