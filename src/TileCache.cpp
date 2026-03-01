/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TileCache.cpp — Asynchronous OSM tile fetcher and cache
 */

#include "TileCache.h"

#include <Autolock.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Messenger.h>
#include <Path.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <OS.h>


static const int kMaxMemoryTiles = 100;


TileCache::TileCache(const char* cacheDir)
	:
	BLooper("TileCache"),
	fCacheDir(cacheDir),
	fEnabled(false),
	fTiles(20),
	fLock("tile_cache_lock"),
	fMaxMemoryTiles(kMaxMemoryTiles)
{
	// Ensure cache directory exists
	create_directory(fCacheDir.String(), 0755);
}


TileCache::~TileCache()
{
	BAutolock lock(fLock);
	for (int32 i = 0; i < fTiles.CountItems(); i++)
		delete fTiles.ItemAt(i);
	fTiles.MakeEmpty(false);
}


void
TileCache::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case MSG_FETCH_TILES:
		{
			int32 z, minX, minY, maxX, maxY;
			BMessenger target;
			if (msg->FindInt32("z", &z) != B_OK
				|| msg->FindInt32("minX", &minX) != B_OK
				|| msg->FindInt32("minY", &minY) != B_OK
				|| msg->FindInt32("maxX", &maxX) != B_OK
				|| msg->FindInt32("maxY", &maxY) != B_OK
				|| msg->FindMessenger("target", &target) != B_OK) {
				break;
			}

			if (!fEnabled)
				break;

			// Limit tiles per request to avoid runaway fetches
			int count = 0;
			bool anyFetched = false;
			for (int tx = minX; tx <= maxX && count < 50; tx++) {
				for (int ty = minY; ty <= maxY && count < 50; ty++) {
					count++;

					// Already in memory?
					if (_FindEntry(z, tx, ty) != NULL)
						continue;

					// Try disk cache
					BBitmap* bitmap = _LoadFromDisk(z, tx, ty);
					if (bitmap != NULL) {
						BAutolock lock(fLock);
						TileEntry* entry = new TileEntry();
						entry->z = z;
						entry->x = tx;
						entry->y = ty;
						entry->bitmap = bitmap;
						entry->lastUsed = system_time();
						fTiles.AddItem(entry);
						anyFetched = true;
						continue;
					}

					// Download from OSM
					BString path = _DiskPath(z, tx, ty);

					// Ensure directory exists
					BString dir;
					dir.SetToFormat("%s/%d/%d", fCacheDir.String(), z, tx);
					create_directory(dir.String(), 0755);

					BString url;
					url.SetToFormat(
						"https://tile.openstreetmap.org/%d/%d/%d.png",
						(int)z, (int)tx, (int)ty);

					BString cmd;
					cmd.SetToFormat(
						"curl -s -m 10 -A 'Sestriere/1.0 (Haiku; MeshCore Client)' "
						"-o '%s' '%s' 2>/dev/null",
						path.String(), url.String());

					int result = system(cmd.String());
					if (result == 0) {
						bitmap = _LoadFromDisk(z, tx, ty);
						if (bitmap != NULL) {
							BAutolock lock(fLock);
							TileEntry* entry = new TileEntry();
							entry->z = z;
							entry->x = tx;
							entry->y = ty;
							entry->bitmap = bitmap;
							entry->lastUsed = system_time();
							fTiles.AddItem(entry);
							anyFetched = true;
						}
					}
				}
			}

			if (anyFetched) {
				_PruneMemoryCache();
				BMessage ready(MSG_TILES_READY);
				target.SendMessage(&ready);
			}
			break;
		}

		default:
			BLooper::MessageReceived(msg);
			break;
	}
}


void
TileCache::RequestTiles(int z, int minX, int minY, int maxX, int maxY,
	BHandler* target)
{
	if (!fEnabled || target == NULL)
		return;

	BMessage msg(MSG_FETCH_TILES);
	msg.AddInt32("z", z);
	msg.AddInt32("minX", minX);
	msg.AddInt32("minY", minY);
	msg.AddInt32("maxX", maxX);
	msg.AddInt32("maxY", maxY);
	msg.AddMessenger("target", BMessenger(target));
	PostMessage(&msg);
}


BBitmap*
TileCache::GetCachedTile(int z, int x, int y)
{
	BAutolock lock(fLock);
	TileEntry* entry = _FindEntry(z, x, y);
	if (entry != NULL) {
		entry->lastUsed = system_time();
		return entry->bitmap;
	}
	return NULL;
}


void
TileCache::SetEnabled(bool enabled)
{
	fEnabled = enabled;
}


BString
TileCache::_DiskPath(int z, int x, int y) const
{
	BString path;
	path.SetToFormat("%s/%d/%d/%d.png", fCacheDir.String(), z, x, y);
	return path;
}


BBitmap*
TileCache::_LoadFromDisk(int z, int x, int y)
{
	BString path = _DiskPath(z, x, y);

	BEntry entry(path.String());
	if (!entry.Exists())
		return NULL;

	BBitmap* bitmap = BTranslationUtils::GetBitmap(path.String());
	return bitmap;
}


void
TileCache::_PruneMemoryCache()
{
	BAutolock lock(fLock);

	while (fTiles.CountItems() > fMaxMemoryTiles) {
		// Find oldest entry
		bigtime_t oldest = LLONG_MAX;
		int32 oldestIndex = 0;

		for (int32 i = 0; i < fTiles.CountItems(); i++) {
			TileEntry* entry = fTiles.ItemAt(i);
			if (entry->lastUsed < oldest) {
				oldest = entry->lastUsed;
				oldestIndex = i;
			}
		}

		TileEntry* entry = fTiles.RemoveItemAt(oldestIndex);
		delete entry;
	}
}


TileEntry*
TileCache::_FindEntry(int z, int x, int y) const
{
	BAutolock lock(fLock);
	for (int32 i = 0; i < fTiles.CountItems(); i++) {
		TileEntry* entry = fTiles.ItemAt(i);
		if (entry->z == z && entry->x == x && entry->y == y)
			return entry;
	}
	return NULL;
}
