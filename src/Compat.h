/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Compat.h — Compatibility layer for Haiku API differences
 *
 * Handles BObjectList template signature change between beta5 and pre-beta6:
 *   Beta5:      template<class T> class BObjectList (ownership via constructor)
 *   Pre-beta6+: template<class T, bool Owning = false> class BObjectList
 */
#ifndef COMPAT_H
#define COMPAT_H

#include <BeBuild.h>
#include <ObjectList.h>

#if B_HAIKU_VERSION > B_HAIKU_VERSION_1_BETA_5

// New API: ownership is a template parameter
template<class T>
using OwningObjectList = BObjectList<T, true>;

#else

// Old API: ownership is a constructor parameter
template<class T>
class OwningObjectList : public BObjectList<T> {
public:
	OwningObjectList(int32 itemsPerBlock = 20)
		: BObjectList<T>(itemsPerBlock, true) {}
};

#endif

#endif // COMPAT_H
