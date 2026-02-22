/*
 * Test: Single Hop Ping functionality
 * Verifies ping timing, RTT formatting, and protocol frame construction
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
typedef int64_t bigtime_t;

#include "../Constants.h"
#include "../Utils.h"


// ============================================================================
// Test 1: Ping frame construction
// ============================================================================

static void
TestPingFrameConstruction()
{
	printf("Test 1: Ping frame construction... ");

	uint8 pubkey[32];
	for (int i = 0; i < 32; i++)
		pubkey[i] = (uint8)(0xA0 + i);

	// Ping uses CMD_SEND_TRACE_PATH with 6-byte prefix
	uint8 payload[7];
	payload[0] = CMD_SEND_TRACE_PATH;
	memcpy(payload + 1, pubkey, 6);

	assert(payload[0] == 36);  // CMD_SEND_TRACE_PATH = 36
	assert(payload[1] == 0xA0);
	assert(payload[2] == 0xA1);
	assert(payload[3] == 0xA2);
	assert(payload[4] == 0xA3);
	assert(payload[5] == 0xA4);
	assert(payload[6] == 0xA5);

	printf("PASS\n");
}


// ============================================================================
// Test 2: RTT formatting
// ============================================================================

static void
TestRTTFormatting()
{
	printf("Test 2: RTT formatting... ");

	// Test microseconds range
	{
		bigtime_t rtt = 500;  // 500 us
		char buf[128];
		if (rtt < 1000)
			snprintf(buf, sizeof(buf), "%lld us", (long long)rtt);
		else if (rtt < 1000000)
			snprintf(buf, sizeof(buf), "%lld ms", (long long)(rtt / 1000));
		else
			snprintf(buf, sizeof(buf), "%.1f s", rtt / 1000000.0);
		assert(strstr(buf, "500 us") != NULL);
	}

	// Test milliseconds range
	{
		bigtime_t rtt = 150000;  // 150 ms
		char buf[128];
		if (rtt < 1000)
			snprintf(buf, sizeof(buf), "%lld us", (long long)rtt);
		else if (rtt < 1000000)
			snprintf(buf, sizeof(buf), "%lld ms", (long long)(rtt / 1000));
		else
			snprintf(buf, sizeof(buf), "%.1f s", rtt / 1000000.0);
		assert(strstr(buf, "150 ms") != NULL);
	}

	// Test seconds range
	{
		bigtime_t rtt = 2500000;  // 2.5 s
		char buf[128];
		if (rtt < 1000)
			snprintf(buf, sizeof(buf), "%lld us", (long long)rtt);
		else if (rtt < 1000000)
			snprintf(buf, sizeof(buf), "%lld ms", (long long)(rtt / 1000));
		else
			snprintf(buf, sizeof(buf), "%.1f s", rtt / 1000000.0);
		assert(strstr(buf, "2.5 s") != NULL);
	}

	printf("PASS\n");
}


// ============================================================================
// Test 3: Ping state management
// ============================================================================

static void
TestPingState()
{
	printf("Test 3: Ping state management... ");

	// Simulate ping state
	bool pingPending = false;
	bigtime_t pingStartTime = 0;
	uint8 pingTargetKey[6];
	memset(pingTargetKey, 0, sizeof(pingTargetKey));

	// Start ping
	pingPending = true;
	pingStartTime = 1000000;  // Simulated system_time()
	uint8 target[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	memcpy(pingTargetKey, target, 6);

	assert(pingPending);
	assert(pingStartTime == 1000000);
	assert(memcmp(pingTargetKey, target, 6) == 0);

	// Receive response
	bigtime_t responseTime = 1150000;  // 150ms later
	bigtime_t rtt = responseTime - pingStartTime;
	pingPending = false;

	assert(!pingPending);
	assert(rtt == 150000);  // 150ms in microseconds

	printf("PASS\n");
}


// ============================================================================
// Test 4: Hop count extraction from PUSH_TRACE_DATA
// ============================================================================

static void
TestHopCountExtraction()
{
	printf("Test 4: Hop count from trace data... ");

	// PUSH_TRACE_DATA format: [0]=code, [1]=?, [2]=pathLen
	uint8 traceData[16];
	memset(traceData, 0, sizeof(traceData));
	traceData[0] = PUSH_TRACE_DATA;
	traceData[2] = 3;  // 3 hops

	uint8 hops = traceData[2];
	assert(hops == 3);

	// Direct path (0 hops)
	traceData[2] = 0;
	hops = traceData[2];
	assert(hops == 0);

	printf("PASS\n");
}


// ============================================================================
// Test 5: Ping target key matching
// ============================================================================

static void
TestPingTargetMatching()
{
	printf("Test 5: Ping target key matching... ");

	uint8 pingTargetKey[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

	// Matching contact
	uint8 contactKey1[32];
	memset(contactKey1, 0, sizeof(contactKey1));
	contactKey1[0] = 0xAA;
	contactKey1[1] = 0xBB;
	contactKey1[2] = 0xCC;
	contactKey1[3] = 0xDD;
	contactKey1[4] = 0xEE;
	contactKey1[5] = 0xFF;
	assert(memcmp(contactKey1, pingTargetKey, 6) == 0);

	// Non-matching contact
	uint8 contactKey2[32];
	memset(contactKey2, 0x11, sizeof(contactKey2));
	assert(memcmp(contactKey2, pingTargetKey, 6) != 0);

	printf("PASS\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Single Hop Ping Tests ===\n");

	TestPingFrameConstruction();
	TestRTTFormatting();
	TestPingState();
	TestHopCountExtraction();
	TestPingTargetMatching();

	printf("\nAll ping tests passed!\n");
	return 0;
}
