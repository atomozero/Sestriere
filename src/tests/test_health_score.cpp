/*
 * Test: Health score calculation (MissionControl)
 * Verifies the score formula with known inputs.
 */

#include <cstdio>
#include <cstdint>
#include <cassert>

// Include battery percent helper
#include "../Utils.h"


// Replicate health score logic from MissionControlWindow::_RecalcHealthScore()
static int32
CalcHealthScore(bool connected, uint16 battMv, uint8 battType,
	int8 rssi, int8 snr, int32 contactsOnline, int32 contactsTotal)
{
	int32 score = 0;

	if (connected) score += 25;

	score += BatteryPercent(battMv, (BatteryChemistry)battType) * 15 / 100;

	if (rssi != 0) {
		int32 rssiScore = (int32)((rssi + 120) * 20.0f / 80.0f);
		if (rssiScore < 0) rssiScore = 0;
		if (rssiScore > 20) rssiScore = 20;
		score += rssiScore;
	}

	if (snr != 0 || rssi != 0) {
		int32 snrScore = (int32)((snr + 20) * 20.0f / 35.0f);
		if (snrScore < 0) snrScore = 0;
		if (snrScore > 20) snrScore = 20;
		score += snrScore;
	}

	if (contactsTotal > 0)
		score += (int32)((contactsOnline * 20.0f) / contactsTotal);

	return score;
}


static void
TestDisconnected()
{
	printf("  TestDisconnected...");
	int32 score = CalcHealthScore(false, 3700, BATTERY_LIPO, -80, 5, 0, 0);
	// Not connected = no 25 bonus, but still has RSSI/SNR components
	assert(score < 50);
	assert(score >= 0);
	printf(" PASS (score=%d)\n", (int)score);
}


static void
TestPerfectConditions()
{
	printf("  TestPerfectConditions...");
	// Connected, full battery (4200mV LiPo), strong RSSI (-40), high SNR (15)
	// all contacts online
	int32 score = CalcHealthScore(true, 4200, BATTERY_LIPO, -40, 15, 5, 5);
	// Should be near max: 25 + 15 + 20 + 20 + 20 = 100
	assert(score >= 90);
	assert(score <= 100);
	printf(" PASS (score=%d)\n", (int)score);
}


static void
TestLowBattery()
{
	printf("  TestLowBattery...");
	// Connected, very low battery (3300mV LiPo), decent signal
	int32 score = CalcHealthScore(true, 3300, BATTERY_LIPO, -70, 5, 2, 4);
	// Battery ~5% → ~1pt, connected=25, RSSI~12, SNR~14, contacts~10
	assert(score >= 45);
	assert(score <= 75);
	printf(" PASS (score=%d)\n", (int)score);
}


static void
TestWeakSignal()
{
	printf("  TestWeakSignal...");
	// Connected, good battery, very weak signal
	int32 score = CalcHealthScore(true, 4000, BATTERY_LIPO, -115, -15, 1, 10);
	// RSSI near floor → ~1, SNR low → ~2, battery ~85% → ~12, contacts 10% → 2
	assert(score >= 30);
	assert(score <= 55);
	printf(" PASS (score=%d)\n", (int)score);
}


static void
TestScoreRange()
{
	printf("  TestScoreRange...");
	// Minimum possible (disconnected, dead battery, no signal)
	int32 minScore = CalcHealthScore(false, 3000, BATTERY_LIPO, 0, 0, 0, 0);
	assert(minScore >= 0);

	// Maximum possible
	int32 maxScore = CalcHealthScore(true, 4200, BATTERY_LIPO, -40, 15, 10, 10);
	assert(maxScore <= 100);

	printf(" PASS (min=%d, max=%d)\n", (int)minScore, (int)maxScore);
}


int
main()
{
	printf("=== test_health_score ===\n");

	TestDisconnected();
	TestPerfectConditions();
	TestLowBattery();
	TestWeakSignal();
	TestScoreRange();

	printf("All health score tests passed!\n");
	return 0;
}
