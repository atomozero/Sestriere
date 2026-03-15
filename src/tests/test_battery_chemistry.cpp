/*
 * Test: Battery chemistry types and percentage calculation
 * Verifies BatteryPercent() for LiPo, LiFePO4, and NMC chemistries
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

// Redefine Haiku types for standalone test
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

#include "../Constants.h"
#include "../Utils.h"


// ============================================================================
// Test 1: LiPo chemistry (default, 3.0V-4.2V)
// ============================================================================

static void
TestLiPoChemistry()
{
	printf("Test 1: LiPo chemistry... ");

	// At minimum voltage: 0%
	assert(BatteryPercent(3000, BATTERY_LIPO) == 0);
	assert(BatteryPercent(2800, BATTERY_LIPO) == 0);  // Below min

	// At maximum voltage: 100%
	assert(BatteryPercent(4200, BATTERY_LIPO) == 100);
	assert(BatteryPercent(4500, BATTERY_LIPO) == 100);  // Above max

	// Mid-range: 50%
	assert(BatteryPercent(3600, BATTERY_LIPO) == 50);

	// Quarter points
	assert(BatteryPercent(3300, BATTERY_LIPO) == 25);
	assert(BatteryPercent(3900, BATTERY_LIPO) == 75);

	// Default chemistry is LiPo
	assert(BatteryPercent(3600) == 50);

	printf("PASS\n");
}


// ============================================================================
// Test 2: LiFePO4 chemistry (2.5V-3.65V)
// ============================================================================

static void
TestLiFePO4Chemistry()
{
	printf("Test 2: LiFePO4 chemistry... ");

	assert(BatteryPercent(2500, BATTERY_LIFEPO4) == 0);
	assert(BatteryPercent(2000, BATTERY_LIFEPO4) == 0);  // Below min

	assert(BatteryPercent(3650, BATTERY_LIFEPO4) == 100);
	assert(BatteryPercent(4000, BATTERY_LIFEPO4) == 100);  // Above max

	// LiFePO4 has different range (1150mV)
	// 50% = 2500 + 575 = 3075 mV
	int pct50 = BatteryPercent(3075, BATTERY_LIFEPO4);
	assert(pct50 == 50);

	// At 3.3V — typical operating voltage for LiFePO4
	int pct33 = BatteryPercent(3300, BATTERY_LIFEPO4);
	assert(pct33 > 60 && pct33 < 75);  // ~69%

	printf("PASS\n");
}


// ============================================================================
// Test 3: NMC chemistry (2.5V-4.2V)
// ============================================================================

static void
TestNMCChemistry()
{
	printf("Test 3: NMC chemistry... ");

	assert(BatteryPercent(2500, BATTERY_NMC) == 0);
	assert(BatteryPercent(4200, BATTERY_NMC) == 100);

	// NMC range is 1700mV, wider than LiPo
	// At 3.35V: (3350-2500)*100/1700 = 50%
	assert(BatteryPercent(3350, BATTERY_NMC) == 50);

	// Same voltage, different results per chemistry
	int lipoAt3600 = BatteryPercent(3600, BATTERY_LIPO);
	int nmcAt3600 = BatteryPercent(3600, BATTERY_NMC);
	int lifeAt3600 = BatteryPercent(3600, BATTERY_LIFEPO4);

	// LiPo: 50%, NMC: ~64%, LiFePO4: ~95%
	assert(lipoAt3600 == 50);
	assert(nmcAt3600 > 60 && nmcAt3600 < 70);
	assert(lifeAt3600 > 90);

	printf("PASS\n");
}


// ============================================================================
// Test 4: Edge cases
// ============================================================================

static void
TestEdgeCases()
{
	printf("Test 4: Edge cases... ");

	// Zero voltage
	assert(BatteryPercent(0, BATTERY_LIPO) == 0);
	assert(BatteryPercent(0, BATTERY_LIFEPO4) == 0);
	assert(BatteryPercent(0, BATTERY_NMC) == 0);

	// Invalid chemistry falls back to LiPo
	assert(BatteryPercent(3600, (BatteryChemistry)99) == 50);

	// Exact boundary values
	assert(BatteryPercent(3000, BATTERY_LIPO) == 0);    // Exact min
	assert(BatteryPercent(4200, BATTERY_LIPO) == 100);  // Exact max
	assert(BatteryPercent(3012, BATTERY_LIPO) == 1);     // Just above 0%
	assert(BatteryPercent(4188, BATTERY_LIPO) == 99);   // Just below 100%

	printf("PASS\n");
}


// ============================================================================
// Test 5: Chemistry constants
// ============================================================================

static void
TestChemistryConstants()
{
	printf("Test 5: Chemistry constants... ");

	assert(BATTERY_LIPO == 0);
	assert(BATTERY_LIFEPO4 == 1);
	assert(BATTERY_NMC == 2);
	assert(BATTERY_USB == 3);
	assert(BATTERY_CHEMISTRY_COUNT == 4);

	// Verify ranges
	assert(kBatteryRanges[BATTERY_LIPO].minMv == 3000);
	assert(kBatteryRanges[BATTERY_LIPO].maxMv == 4200);
	assert(kBatteryRanges[BATTERY_LIFEPO4].minMv == 2500);
	assert(kBatteryRanges[BATTERY_LIFEPO4].maxMv == 3650);
	assert(kBatteryRanges[BATTERY_NMC].minMv == 2500);
	assert(kBatteryRanges[BATTERY_NMC].maxMv == 4200);

	// Verify USB chemistry
	assert(BatteryPercent(0, BATTERY_USB) == 0);
	assert(BatteryPercent(5000, BATTERY_USB) == 100);

	// Verify names
	assert(strcmp(kBatteryChemistryNames[BATTERY_LIPO], "LiPo") == 0);
	assert(strcmp(kBatteryChemistryNames[BATTERY_LIFEPO4], "LiFePO4") == 0);
	assert(strcmp(kBatteryChemistryNames[BATTERY_NMC], "NMC") == 0);
	assert(strcmp(kBatteryChemistryNames[BATTERY_USB], "USB") == 0);

	printf("PASS\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Battery Chemistry Tests ===\n");

	TestLiPoChemistry();
	TestLiFePO4Chemistry();
	TestNMCChemistry();
	TestEdgeCases();
	TestChemistryConstants();

	printf("\nAll battery chemistry tests passed!\n");
	return 0;
}
