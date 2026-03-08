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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


static const int kMaxMemoryTiles = 100;


TileCache::TileCache(const char* cacheDir)
	:
	BLooper("TileCache"),
	fCacheDir(cacheDir),
	fEnabled(false),
	fTiles(20),
	fLock("tile_cache_lock"),
	fMaxMemoryTiles(kMaxMemoryTiles),
	fDiskCacheSize(0),
	fDiskTileCount(0)
{
	// Ensure cache directory exists
	create_directory(fCacheDir.String(), 0755);

	// Scan disk cache to know current size
	_ScanDiskCache();
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

					// Download tile using fork/exec (no shell)
					bool downloaded = false;
					pid_t pid = fork();
					if (pid == 0) {
						// Child: redirect stderr to /dev/null
						int devnull = open("/dev/null", O_WRONLY);
						if (devnull >= 0) {
							dup2(devnull, STDERR_FILENO);
							close(devnull);
						}
						execlp("curl", "curl",
							"-s", "-m", "10",
							"-A", "Sestriere/1.0 (Haiku; MeshCore Client)",
							"-o", path.String(),
							url.String(),
							(char*)NULL);
						_exit(127);
					} else if (pid > 0) {
						int status;
						waitpid(pid, &status, 0);
						if (WIFEXITED(status)
							&& WEXITSTATUS(status) == 0) {
							downloaded = true;
						}
					}

					if (downloaded) {
						// Track new tile's disk size
						struct stat st;
						if (stat(path.String(), &st) == 0) {
							fDiskCacheSize += st.st_size;
							fDiskTileCount++;
						}

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
				if (fDiskCacheSize > kMaxDiskCacheBytes)
					_PruneDiskCache();
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


void
TileCache::_ScanDiskCache()
{
	fDiskCacheSize = 0;
	fDiskTileCount = 0;

	// Walk {cacheDir}/{z}/{x}/{y}.png
	BDirectory cacheDir(fCacheDir.String());
	if (cacheDir.InitCheck() != B_OK)
		return;

	BEntry zEntry;
	while (cacheDir.GetNextEntry(&zEntry) == B_OK) {
		if (!zEntry.IsDirectory())
			continue;

		BDirectory zDir(&zEntry);
		BEntry xEntry;
		while (zDir.GetNextEntry(&xEntry) == B_OK) {
			if (!xEntry.IsDirectory())
				continue;

			BDirectory xDir(&xEntry);
			BEntry yEntry;
			while (xDir.GetNextEntry(&yEntry) == B_OK) {
				struct stat st;
				if (yEntry.GetStat(&st) == 0 && S_ISREG(st.st_mode)) {
					fDiskCacheSize += st.st_size;
					fDiskTileCount++;
				}
			}
		}
	}
}


// Helper struct for disk pruning — collects file path + mtime
struct DiskTileInfo {
	BString		path;
	time_t		mtime;
	off_t		size;
};


static int
_CompareDiskTileByMtime(const DiskTileInfo* a, const DiskTileInfo* b)
{
	if (a->mtime < b->mtime)
		return -1;
	if (a->mtime > b->mtime)
		return 1;
	return 0;
}


void
TileCache::_PruneDiskCache()
{
	// Collect all tile files with metadata
	BObjectList<DiskTileInfo> files(256);

	BDirectory cacheDir(fCacheDir.String());
	if (cacheDir.InitCheck() != B_OK)
		return;

	BEntry zEntry;
	while (cacheDir.GetNextEntry(&zEntry) == B_OK) {
		if (!zEntry.IsDirectory())
			continue;

		BDirectory zDir(&zEntry);
		BEntry xEntry;
		while (zDir.GetNextEntry(&xEntry) == B_OK) {
			if (!xEntry.IsDirectory())
				continue;

			BDirectory xDir(&xEntry);
			BEntry yEntry;
			while (xDir.GetNextEntry(&yEntry) == B_OK) {
				struct stat st;
				if (yEntry.GetStat(&st) != 0 || !S_ISREG(st.st_mode))
					continue;

				BPath filePath;
				if (yEntry.GetPath(&filePath) != B_OK)
					continue;

				DiskTileInfo* info = new DiskTileInfo();
				info->path = filePath.Path();
				info->mtime = st.st_mtime;
				info->size = st.st_size;
				files.AddItem(info);
			}
		}
	}

	// Sort by modification time ascending (oldest first)
	files.SortItems(_CompareDiskTileByMtime);

	// Delete oldest tiles until under 90% of limit (hysteresis)
	off_t target = (off_t)(kMaxDiskCacheBytes * 0.9);

	for (int32 i = 0; i < files.CountItems() && fDiskCacheSize > target; i++) {
		DiskTileInfo* info = files.ItemAt(i);

		// Remove from memory cache if loaded
		// (file path encodes z/x/y but we just remove the file)
		if (remove(info->path.String()) == 0) {
			fDiskCacheSize -= info->size;
			fDiskTileCount--;
		}
	}

	// Cleanup DiskTileInfo objects
	for (int32 i = 0; i < files.CountItems(); i++)
		delete files.ItemAt(i);
}
