/*
 * test_los_math.cpp — Unit tests for LoSAnalysis.h math functions
 *
 * Build: g++ -o test_los_math test_los_math.cpp -I../ -lm
 */

#include "../LoSAnalysis.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>


static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  %-50s ", name); \
	fflush(stdout);

#define PASS() \
	do { printf("[PASS]\n"); sTestsPassed++; } while (0)

#define FAIL(msg) \
	do { printf("[FAIL] %s\n", msg); sTestsFailed++; } while (0)

#define ASSERT_NEAR(a, b, tol) \
	do { \
		double _a = (a), _b = (b), _t = (tol); \
		if (fabs(_a - _b) > _t) { \
			char _buf[128]; \
			snprintf(_buf, sizeof(_buf), \
				"expected %.6f, got %.6f (tol %.6f)", _b, _a, _t); \
			FAIL(_buf); \
			return; \
		} \
	} while (0)


// ============================================================================
// Haversine distance tests
// ============================================================================

static void
test_haversine_same_point()
{
	TEST("Haversine: same point = 0");
	double d = HaversineDistance(45.0, 7.0, 45.0, 7.0);
	ASSERT_NEAR(d, 0, 0.001);
	PASS();
}

static void
test_haversine_known_distance()
{
	TEST("Haversine: Turin-Milan ~126 km");
	// Turin (45.0703, 7.6869) to Milan (45.4642, 9.1900)
	double d = HaversineDistance(45.0703, 7.6869, 45.4642, 9.1900);
	// Expected ~126 km
	assert(d > 120000 && d < 135000);
	PASS();
}

static void
test_haversine_antipodal()
{
	TEST("Haversine: antipodal ~20015 km");
	// North pole to south pole
	double d = HaversineDistance(90.0, 0.0, -90.0, 0.0);
	ASSERT_NEAR(d / 1000.0, 20015.0, 100.0);
	PASS();
}


// ============================================================================
// Interpolation tests
// ============================================================================

static void
test_interpolate_endpoints()
{
	TEST("Interpolate: endpoints match");
	double lats[3], lons[3];
	InterpolatePoints(45.0, 7.0, 46.0, 8.0, lats, lons, 3);

	ASSERT_NEAR(lats[0], 45.0, 0.001);
	ASSERT_NEAR(lons[0], 7.0, 0.001);
	ASSERT_NEAR(lats[2], 46.0, 0.001);
	ASSERT_NEAR(lons[2], 8.0, 0.001);
	PASS();
}

static void
test_interpolate_midpoint()
{
	TEST("Interpolate: midpoint reasonable");
	double lats[3], lons[3];
	InterpolatePoints(45.0, 7.0, 45.0, 9.0, lats, lons, 3);

	// Midpoint should be ~45.0, 8.0
	ASSERT_NEAR(lats[1], 45.0, 0.1);
	ASSERT_NEAR(lons[1], 8.0, 0.01);
	PASS();
}


// ============================================================================
// Earth curvature tests
// ============================================================================

static void
test_curvature_at_endpoints()
{
	TEST("Curvature: zero at endpoints");
	// d1=0 or d2=0 should give 0
	double h = EarthCurvatureBulge(0, 10000);
	ASSERT_NEAR(h, 0, 0.001);
	h = EarthCurvatureBulge(10000, 0);
	ASSERT_NEAR(h, 0, 0.001);
	PASS();
}

static void
test_curvature_midpoint()
{
	TEST("Curvature: ~0.78m at 10km midpoint");
	// At midpoint of a 10km path: d1=d2=5000m
	// h = 5000*5000/(2*8495333) = 1.47m (with 4/3 Earth)
	double h = EarthCurvatureBulge(5000, 5000);
	assert(h > 1.0 && h < 2.0);
	PASS();
}

static void
test_curvature_30km()
{
	TEST("Curvature: significant at 30km");
	// 30km path midpoint: d1=d2=15000m
	// h = 15000*15000/(2*8495333) = ~13.2m
	double h = EarthCurvatureBulge(15000, 15000);
	assert(h > 10.0 && h < 20.0);
	PASS();
}


// ============================================================================
// Fresnel zone tests
// ============================================================================

static void
test_fresnel_868mhz()
{
	TEST("Fresnel: R1 at 868MHz, 10km");
	// Lambda = c/f = 299792458/868000000 = 0.3454m
	// At midpoint of 10km: d1=d2=5000m
	// R1 = sqrt(0.3454 * 5000 * 5000 / 10000) = sqrt(863.5) = ~29.4m
	double r = FresnelRadius(5000, 5000, 868e6);
	assert(r > 25 && r < 35);
	PASS();
}

static void
test_fresnel_zero_freq()
{
	TEST("Fresnel: zero for zero freq");
	double r = FresnelRadius(5000, 5000, 0);
	ASSERT_NEAR(r, 0, 0.001);
	PASS();
}

static void
test_fresnel_at_endpoint()
{
	TEST("Fresnel: zero at endpoint");
	double r = FresnelRadius(0, 10000, 868e6);
	ASSERT_NEAR(r, 0, 0.001);
	PASS();
}


// ============================================================================
// Full LoS analysis tests
// ============================================================================

static void
test_los_flat_terrain()
{
	TEST("LoS: clear on flat terrain (500m)");
	TerrainPoint points[5];
	for (int i = 0; i < 5; i++) {
		points[i].distance = i * 125.0;  // 500m total — tiny Fresnel zone
		points[i].elevation = 100.0;  // Flat at 100m
		points[i].latitude = 45.0;
		points[i].longitude = 7.0 + i * 0.001;
	}

	// 500m at 868MHz, 10m antennas — Fresnel R1 at midpoint ~5m, clearance 10m
	LoSResult result = AnalyzeLineOfSight(points, 5, 10.0, 10.0, 868e6);
	assert(result.hasLineOfSight == true);
	ASSERT_NEAR(result.totalDistance, 500, 1);
	PASS();
}

static void
test_los_mountain_obstruction()
{
	TEST("LoS: blocked by mountain");
	TerrainPoint points[5];
	// Start: 100m, End: 100m, Mountain in middle: 500m
	double elevations[] = {100, 200, 500, 200, 100};
	for (int i = 0; i < 5; i++) {
		points[i].distance = i * 2500.0;
		points[i].elevation = elevations[i];
		points[i].latitude = 45.0;
		points[i].longitude = 7.0 + i * 0.01;
	}

	LoSResult result = AnalyzeLineOfSight(points, 5, 2.0, 2.0, 868e6);
	assert(result.hasLineOfSight == false);
	assert(result.maxObstruction > 100);
	assert(result.worstPointIndex == 2);
	PASS();
}

static void
test_los_hilltop_to_hilltop()
{
	TEST("LoS: hilltop-to-hilltop clear");
	TerrainPoint points[7];
	// Two hills with a valley in between
	double elevations[] = {500, 450, 300, 200, 300, 450, 500};
	for (int i = 0; i < 7; i++) {
		points[i].distance = i * 1000.0;
		points[i].elevation = elevations[i];
		points[i].latitude = 45.0;
		points[i].longitude = 7.0 + i * 0.005;
	}

	LoSResult result = AnalyzeLineOfSight(points, 7, 2.0, 2.0, 868e6);
	assert(result.hasLineOfSight == true);
	PASS();
}

static void
test_los_result_init()
{
	TEST("LoSResult: default init");
	LoSResult r;
	assert(r.hasLineOfSight == true);
	assert(r.totalDistance == 0);
	assert(r.maxObstruction == 0);
	assert(r.worstPointIndex == -1);
	PASS();
}

static void
test_los_two_points()
{
	TEST("LoS: two points (min valid)");
	TerrainPoint points[2];
	points[0].distance = 0;
	points[0].elevation = 100;
	points[1].distance = 1000;
	points[1].elevation = 100;

	LoSResult result = AnalyzeLineOfSight(points, 2, 2.0, 2.0, 868e6);
	assert(result.hasLineOfSight == true);
	ASSERT_NEAR(result.totalDistance, 1000, 1);
	PASS();
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== LoS Analysis Math Tests ===\n\n");

	// Haversine
	test_haversine_same_point();
	test_haversine_known_distance();
	test_haversine_antipodal();

	// Interpolation
	test_interpolate_endpoints();
	test_interpolate_midpoint();

	// Earth curvature
	test_curvature_at_endpoints();
	test_curvature_midpoint();
	test_curvature_30km();

	// Fresnel
	test_fresnel_868mhz();
	test_fresnel_zero_freq();
	test_fresnel_at_endpoint();

	// Full LoS analysis
	test_los_flat_terrain();
	test_los_mountain_obstruction();
	test_los_hilltop_to_hilltop();
	test_los_result_init();
	test_los_two_points();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
