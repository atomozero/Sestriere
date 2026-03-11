/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ElevationService.h — Open-Meteo elevation API client
 */

#ifndef ELEVATIONSERVICE_H
#define ELEVATIONSERVICE_H

#include <SupportDefs.h>

#include "LoSAnalysis.h"


class ElevationService {
public:
	// Fetch elevations for an array of coordinates
	// Returns B_OK on success, fills outElevations[]
	// Max 100 coords per call; batches automatically if count > 100
	static	status_t	FetchElevations(const double* lats,
							const double* lons, int32 count,
							double* outElevations);

	// Build a terrain profile between two points
	// Allocates numSamples TerrainPoints, fills distances and elevations
	// outPoints must have space for numSamples entries
	static	status_t	BuildTerrainProfile(double lat1, double lon1,
							double lat2, double lon2, int32 numSamples,
							TerrainPoint* outPoints, int32* outCount);
};


#endif // ELEVATIONSERVICE_H
