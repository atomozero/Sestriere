/*
 * test_vla_removal.cpp — Verify VLA replaced with heap allocation
 * in SNRChartView::_DrawLine() to prevent stack overflow.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

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
	printf("=== VLA Removal Test ===\n\n");

	// Verify no VLA remains in SNRChartView
	Check(!FileContains(
		"../SNRChartView.cpp",
		"BPoint points[fPointCount"),
		"No VLA 'BPoint points[fPointCount...]' in SNRChartView.cpp");

	// Verify heap allocation used instead
	Check(FileContainsBetween(
		"../SNRChartView.cpp",
		"::_DrawLine",
		"FillPolygon",
		"new BPoint[fPointCount + 2]"),
		"Uses heap allocation 'new BPoint[fPointCount + 2]'");

	// Verify delete[] is called after FillPolygon
	Check(FileContainsBetween(
		"../SNRChartView.cpp",
		"FillPolygon(points",
		"SetDrawingMode",
		"delete[] points"),
		"Heap-allocated points freed with delete[] after FillPolygon");

	// Verify FillPolygon still uses the same count
	Check(FileContainsBetween(
		"../SNRChartView.cpp",
		"::_DrawLine",
		"delete[] points",
		"FillPolygon(points, fPointCount + 2)"),
		"FillPolygon still receives fPointCount + 2 elements");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
