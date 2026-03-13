/*
 * test_los_divzero.cpp — Verify divide-by-zero guards in LoS analysis
 * when start and end points are identical (totalDist == 0).
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "../LoSAnalysis.h"

static int gPassed = 0;
static int gFailed = 0;

static void Check(bool cond, const char* name) {
	if (cond) {
		printf("  PASS: %s\n", name);
		gPassed++;
	} else {
		printf("  FAIL: %s\n", name);
		gFailed++;
	}
}

static bool FileContains(const char* path, const char* needle) {
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);
	bool found = strstr(buf, needle) != NULL;
	free(buf);
	return found;
}

static bool FileContainsBetween(const char* path,
	const char* startMarker, const char* endMarker, const char* needle)
{
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	const char* start = strstr(buf, startMarker);
	if (!start) { free(buf); return false; }
	const char* end = strstr(start + strlen(startMarker), endMarker);
	if (!end) { free(buf); return false; }

	size_t regionLen = end - start;
	char* region = (char*)malloc(regionLen + 1);
	memcpy(region, start, regionLen);
	region[regionLen] = 0;
	bool found = strstr(region, needle) != NULL;
	free(region);
	free(buf);
	return found;
}


int main() {
	printf("=== LoS Divide-by-Zero Guard Test ===\n\n");

	// --- Code pattern tests ---
	printf("--- Code guards ---\n");

	Check(FileContainsBetween(
		"../LoSAnalysis.h",
		"AnalyzeLineOfSight",
		"for (int32_t i = 1",
		"if (totalDist <= 0)"),
		"AnalyzeLineOfSight has totalDist <= 0 guard before loop");

	Check(FileContainsBetween(
		"../LoSWindow.cpp",
		"_DrawFresnelZone",
		"for (int32 i = 1",
		"if (totalDist <= 0)"),
		"_DrawFresnelZone has totalDist <= 0 guard before loop");

	// --- Runtime tests: AnalyzeLineOfSight with zero distance ---
	printf("\n--- Runtime: zero-distance terrain ---\n");

	// All points at same location → totalDist = 0
	TerrainPoint samePoints[5];
	for (int i = 0; i < 5; i++) {
		samePoints[i].distance = 0;
		samePoints[i].elevation = 100.0;
		samePoints[i].latitude = 45.0;
		samePoints[i].longitude = 7.0;
	}

	LoSResult zeroResult = AnalyzeLineOfSight(samePoints, 5, 2.0, 2.0, 868e6);
	Check(zeroResult.totalDistance == 0,
		"Zero-distance: totalDistance is 0");
	Check(zeroResult.hasLineOfSight == true,
		"Zero-distance: returns default hasLineOfSight (true)");
	Check(zeroResult.worstPointIndex == -1,
		"Zero-distance: no worst point analyzed (early return)");
	Check(!std::isnan(zeroResult.worstFresnelRatio) && !std::isinf(zeroResult.worstFresnelRatio),
		"Zero-distance: worstFresnelRatio is not NaN/Inf");

	// --- Runtime tests: normal terrain still works ---
	printf("\n--- Runtime: normal terrain ---\n");

	TerrainPoint normalPoints[3];
	normalPoints[0].distance = 0;
	normalPoints[0].elevation = 100.0;
	normalPoints[1].distance = 5000;
	normalPoints[1].elevation = 200.0;
	normalPoints[2].distance = 10000;
	normalPoints[2].elevation = 150.0;

	LoSResult normalResult = AnalyzeLineOfSight(normalPoints, 3, 2.0, 2.0, 868e6);
	Check(normalResult.totalDistance == 10000,
		"Normal: totalDistance is 10000");
	Check(normalResult.worstPointIndex >= 0,
		"Normal: worst point was analyzed");
	Check(!std::isnan(normalResult.worstFresnelRatio),
		"Normal: worstFresnelRatio is not NaN");

	// --- Runtime: single-point terrain (count < 2) ---
	printf("\n--- Runtime: degenerate cases ---\n");

	TerrainPoint onePoint;
	onePoint.distance = 0;
	onePoint.elevation = 50.0;
	LoSResult oneResult = AnalyzeLineOfSight(&onePoint, 1, 2.0, 2.0, 868e6);
	Check(oneResult.totalDistance == 0,
		"Single point: returns default result");

	// Two points, zero distance
	TerrainPoint twoPoints[2];
	twoPoints[0].distance = 0;
	twoPoints[0].elevation = 100.0;
	twoPoints[1].distance = 0;
	twoPoints[1].elevation = 100.0;
	LoSResult twoResult = AnalyzeLineOfSight(twoPoints, 2, 2.0, 2.0, 868e6);
	Check(twoResult.totalDistance == 0,
		"Two points zero-distance: totalDistance is 0");
	Check(twoResult.hasLineOfSight == true,
		"Two points zero-distance: returns default hasLineOfSight");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
