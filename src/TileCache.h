/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TileCache.h — Asynchronous OSM tile fetcher and cache
 */

#ifndef TILE_CACHE_H
#define TILE_CACHE_H

#include <Bitmap.h>
#include <Handler.h>
#include <Locker.h>
#include <Looper.h>
#include <String.h>

#include <ObjectList.h>

#include <sys/types.h>


// Messages
enum {
	MSG_FETCH_TILES		= 'ftch',
	MSG_TILES_READY		= 'tlrd',
};

static const off_t kMaxDiskCacheBytes = 50 * 1024 * 1024;	// 50 MB


struct TileEntry {
	int				z;
	int				x;
	int				y;
	BBitmap*		bitmap;		// owned
	bigtime_t		lastUsed;

	TileEntry()
		: z(0), x(0), y(0), bitmap(NULL), lastUsed(0)
	{
	}

	~TileEntry()
	{
		delete bitmap;
	}
};


class TileCache : public BLooper {
public:
							TileCache(const char* cacheDir);
	virtual					~TileCache();

	virtual void			MessageReceived(BMessage* msg);

	// Request tiles for visible area — async
	void					RequestTiles(int z, int minX, int minY,
								int maxX, int maxY, BHandler* target);

	// Get a tile from memory cache (returns NULL if not loaded)
	BBitmap*				GetCachedTile(int z, int x, int y);

	void					SetEnabled(bool enabled);
	bool					IsEnabled() const { return fEnabled; }

	// Disk cache stats
	off_t					DiskCacheSize() const { return fDiskCacheSize; }
	int32					DiskTileCount() const { return fDiskTileCount; }

private:
	void					_FetchTile(int z, int x, int y,
								BHandler* target);
	BString					_DiskPath(int z, int x, int y) const;
	BBitmap*				_LoadFromDisk(int z, int x, int y);
	void					_PruneMemoryCache();
	void					_ScanDiskCache();
	void					_PruneDiskCache();
	TileEntry*				_FindEntry(int z, int x, int y) const;

	BString					fCacheDir;
	bool					fEnabled;
	BObjectList<TileEntry>	fTiles;		// in-memory cache
	mutable BLocker			fLock;
	int						fMaxMemoryTiles;
	off_t					fDiskCacheSize;
	int32					fDiskTileCount;
};


#endif // TILE_CACHE_H
