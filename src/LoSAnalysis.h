/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * LoSAnalysis.h — Line-of-Sight math library (header-only, no Haiku deps)
 */

#ifndef LOSANALYSIS_H
#define LOSANALYSIS_H

#include <cmath>
#include <cstdint>


// Effective Earth radius for 4/3 refraction model (meters)
static const double kEffectiveEarthRadius = 6371000.0 * 4.0 / 3.0;


struct TerrainPoint {
	double	distance;	// Distance from start (meters)
	double	elevation;	// Elevation above sea level (meters)
	double	latitude;	// Decimal degrees
	double	longitude;	// Decimal degrees
};


struct LoSResult {
	bool	hasLineOfSight;		// True if Fresnel zone 60%+ clear
	double	totalDistance;		// Total path distance (meters)
	double	maxObstruction;		// Max obstruction into LoS line (meters, >0 = blocked)
	double	worstFresnelRatio;	// Worst Fresnel clearance (0=touching, <0=blocked, >1=clear)
	int32_t	worstPointIndex;	// Index of worst obstruction point

	LoSResult()
		: hasLineOfSight(true), totalDistance(0), maxObstruction(0),
		  worstFresnelRatio(1e9), worstPointIndex(-1) {}
};


// Haversine distance between two points in meters
inline double
HaversineDistance(double lat1, double lon1, double lat2, double lon2)
{
	static const double kEarthRadius = 6371000.0;
	static const double kDegToRad = M_PI / 180.0;

	double dLat = (lat2 - lat1) * kDegToRad;
	double dLon = (lon2 - lon1) * kDegToRad;

	double a = sin(dLat / 2) * sin(dLat / 2)
		+ cos(lat1 * kDegToRad) * cos(lat2 * kDegToRad)
		* sin(dLon / 2) * sin(dLon / 2);

	double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	return kEarthRadius * c;
}


// Interpolate count equidistant points along a great circle
inline void
InterpolatePoints(double lat1, double lon1, double lat2, double lon2,
	double* latOut, double* lonOut, int32_t count)
{
	static const double kDegToRad = M_PI / 180.0;
	static const double kRadToDeg = 180.0 / M_PI;

	double phi1 = lat1 * kDegToRad;
	double lambda1 = lon1 * kDegToRad;
	double phi2 = lat2 * kDegToRad;
	double lambda2 = lon2 * kDegToRad;

	double dPhi = phi2 - phi1;
	double dLambda = lambda2 - lambda1;
	double a = sin(dPhi / 2) * sin(dPhi / 2)
		+ cos(phi1) * cos(phi2) * sin(dLambda / 2) * sin(dLambda / 2);
	double delta = 2 * atan2(sqrt(a), sqrt(1 - a));

	if (delta < 1e-12) {
		// Points are essentially the same
		for (int32_t i = 0; i < count; i++) {
			latOut[i] = lat1;
			lonOut[i] = lon1;
		}
		return;
	}

	for (int32_t i = 0; i < count; i++) {
		double f = (double)i / (double)(count - 1);
		double A = sin((1 - f) * delta) / sin(delta);
		double B = sin(f * delta) / sin(delta);

		double x = A * cos(phi1) * cos(lambda1) + B * cos(phi2) * cos(lambda2);
		double y = A * cos(phi1) * sin(lambda1) + B * cos(phi2) * sin(lambda2);
		double z = A * sin(phi1) + B * sin(phi2);

		latOut[i] = atan2(z, sqrt(x * x + y * y)) * kRadToDeg;
		lonOut[i] = atan2(y, x) * kRadToDeg;
	}
}


// Earth curvature bulge at a point between two endpoints
// d1 = distance from start to point, d2 = distance from point to end
// Returns height drop due to curvature (meters)
inline double
EarthCurvatureBulge(double d1, double d2)
{
	return (d1 * d2) / (2.0 * kEffectiveEarthRadius);
}


// First Fresnel zone radius at a point between transmitter and receiver
// d1 = distance from TX to point (meters), d2 = distance from point to RX (meters)
// freqHz = frequency in Hz
// Returns radius R1 in meters
inline double
FresnelRadius(double d1, double d2, double freqHz)
{
	if (freqHz <= 0 || d1 + d2 <= 0)
		return 0;

	double lambda = 299792458.0 / freqHz;  // Speed of light / frequency
	return sqrt(lambda * d1 * d2 / (d1 + d2));
}


// Analyze Line-of-Sight between two antennas over terrain
// points[] = terrain profile (distance + elevation)
// count = number of points (must be >= 2)
// startHeight = antenna height above ground at start (meters)
// endHeight = antenna height above ground at end (meters)
// freqHz = radio frequency in Hz
inline LoSResult
AnalyzeLineOfSight(const TerrainPoint* points, int32_t count,
	double startHeight, double endHeight, double freqHz)
{
	LoSResult result;

	if (count < 2)
		return result;

	double totalDist = points[count - 1].distance;
	result.totalDistance = totalDist;

	// Antenna elevations (ground + mast height)
	double txElev = points[0].elevation + startHeight;
	double rxElev = points[count - 1].elevation + endHeight;

	for (int32_t i = 1; i < count - 1; i++) {
		double d1 = points[i].distance;
		double d2 = totalDist - d1;

		// Line-of-sight elevation at this distance (linear interpolation)
		double fraction = d1 / totalDist;
		double losElev = txElev + fraction * (rxElev - txElev);

		// Subtract earth curvature bulge (terrain appears higher)
		double curvature = EarthCurvatureBulge(d1, d2);
		double effectiveTerrainElev = points[i].elevation + curvature;

		// Clearance = LoS line height - effective terrain height
		double clearance = losElev - effectiveTerrainElev;

		// Fresnel zone radius at this point
		double fresnel = FresnelRadius(d1, d2, freqHz);

		// Fresnel ratio: clearance / fresnel radius
		// >1 = fully clear, 0 = touching, <0 = obstructed
		double fresnelRatio = (fresnel > 0) ? clearance / fresnel : 1e9;

		// Track worst point
		if (fresnelRatio < result.worstFresnelRatio) {
			result.worstFresnelRatio = fresnelRatio;
			result.worstPointIndex = i;
		}

		// Track max obstruction (negative clearance = obstruction)
		if (-clearance > result.maxObstruction)
			result.maxObstruction = -clearance;
	}

	// LoS is considered viable if 60% of first Fresnel zone is clear
	result.hasLineOfSight = (result.worstFresnelRatio >= 0.6);

	// If max obstruction is negative (all clear), set to 0
	if (result.maxObstruction < 0)
		result.maxObstruction = 0;

	return result;
}


#endif // LOSANALYSIS_H
