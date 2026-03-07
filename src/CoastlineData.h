/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * CoastlineData.h — Simplified world coastline polylines for offline map
 *
 * Data derived from Natural Earth 110m simplified coastlines.
 * Format: {lat, lon, lat, lon, ..., 999, 999} — 999 = polyline break
 * Major landmasses: Europe, Africa, Asia, Americas, Australia, etc.
 *
 * Data defined in CoastlineData.cpp to avoid bloating every translation unit.
 */

#ifndef COASTLINE_DATA_H
#define COASTLINE_DATA_H


extern const float kCoastlineData[];
extern const int kCoastlinePointCount;


#endif // COASTLINE_DATA_H
