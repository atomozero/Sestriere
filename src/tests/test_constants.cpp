/*
 * Test: Protocol frame offset constants
 * Verifies that named constants match the documented MeshCore V3 protocol
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
// Test 1: Contact frame constants match protocol spec
// ============================================================================

static void
TestContactFrameOffsets()
{
	assert(kContactFrameSize == 148);
	assert(kContactPubKeyOffset == 1);
	assert(kContactTypeOffset == 33);    // 1 + 32
	assert(kContactFlagsOffset == 34);
	assert(kContactPathLenOffset == 35);
	assert(kContactNameOffset == 100);
	assert(kContactNameSize == 32);
	assert(kContactLastSeenOffset == 132);
	assert(kContactLatOffset == 136);
	assert(kContactLonOffset == 140);

	// Simulate a contact frame and verify extraction
	uint8 frame[148];
	memset(frame, 0, sizeof(frame));
	frame[0] = 3;  // RSP_CONTACT code

	// Set pubkey
	for (int i = 0; i < 32; i++)
		frame[kContactPubKeyOffset + i] = (uint8)(0xA0 + i);

	frame[kContactTypeOffset] = 1;      // CHAT
	frame[kContactFlagsOffset] = 0x42;
	frame[kContactPathLenOffset] = 3;    // 3 hops

	// Set name at offset 100
	const char* name = "TestNode";
	memcpy(frame + kContactNameOffset, name, strlen(name));

	// Set lastSeen at offset 132
	uint32 lastSeen = 1700000000;
	frame[kContactLastSeenOffset] = lastSeen & 0xFF;
	frame[kContactLastSeenOffset + 1] = (lastSeen >> 8) & 0xFF;
	frame[kContactLastSeenOffset + 2] = (lastSeen >> 16) & 0xFF;
	frame[kContactLastSeenOffset + 3] = (lastSeen >> 24) & 0xFF;

	// Verify extraction using constants
	assert(frame[kContactTypeOffset] == 1);
	assert(frame[kContactFlagsOffset] == 0x42);
	assert(frame[kContactPathLenOffset] == 3);
	assert(memcmp(frame + kContactNameOffset, "TestNode", 8) == 0);
	assert(ReadLE32(frame + kContactLastSeenOffset) == 1700000000);

	printf("  PASS: Contact frame offsets match protocol spec\n");
}


// ============================================================================
// Test 2: V3 DM message constants
// ============================================================================

static void
TestV3DmOffsets()
{
	assert(kV3DmSnrOffset == 1);
	assert(kV3DmSenderOffset == 4);
	assert(kV3DmPathLenOffset == 10);
	assert(kV3DmTxtTypeOffset == 11);
	assert(kV3DmTimestampOffset == 12);
	assert(kV3DmTextOffset == 16);
	assert(kV3DmMinLength == 16);

	// Simulate V3 DM: [0]=code [1]=snr [2]=rssi [3]=pathLen [4-9]=sender
	// [10]=pathLen [11]=txtType [12-15]=timestamp [16+]=text
	uint8 frame[32];
	memset(frame, 0, sizeof(frame));
	frame[0] = 0x10;  // RSP_CONTACT_MSG_RECV_V3
	frame[kV3DmSnrOffset] = (uint8)(int8)-5;  // SNR = -5
	frame[kV3DmSenderOffset] = 0xAA;          // sender prefix[0]
	frame[kV3DmPathLenOffset] = 2;             // 2 hops
	frame[kV3DmTxtTypeOffset] = 0;             // plain text

	uint32 ts = 1700000000;
	frame[kV3DmTimestampOffset] = ts & 0xFF;
	frame[kV3DmTimestampOffset + 1] = (ts >> 8) & 0xFF;
	frame[kV3DmTimestampOffset + 2] = (ts >> 16) & 0xFF;
	frame[kV3DmTimestampOffset + 3] = (ts >> 24) & 0xFF;

	memcpy(frame + kV3DmTextOffset, "Hello", 5);

	assert((int8)frame[kV3DmSnrOffset] == -5);
	assert(frame[kV3DmSenderOffset] == 0xAA);
	assert(frame[kV3DmPathLenOffset] == 2);
	assert(ReadLE32(frame + kV3DmTimestampOffset) == 1700000000);
	assert(memcmp(frame + kV3DmTextOffset, "Hello", 5) == 0);

	printf("  PASS: V3 DM offsets match protocol spec\n");
}


// ============================================================================
// Test 3: V3 Channel message constants
// ============================================================================

static void
TestV3ChannelOffsets()
{
	assert(kV3ChSnrOffset == 1);
	assert(kV3ChChannelOffset == 4);
	assert(kV3ChPathLenOffset == 5);
	assert(kV3ChTimestampOffset == 7);
	assert(kV3ChTextOffset == 11);
	assert(kV3ChMinLength == 11);

	printf("  PASS: V3 channel offsets match protocol spec\n");
}


// ============================================================================
// Test 4: V2 backward compatibility offsets
// ============================================================================

static void
TestV2Offsets()
{
	// V2 DM
	assert(kV2DmSenderOffset == 1);
	assert(kV2DmPathLenOffset == 7);
	assert(kV2DmTxtTypeOffset == 8);
	assert(kV2DmTimestampOffset == 9);
	assert(kV2DmTextOffset == 13);
	assert(kV2DmMinLength == 13);

	// V2 Channel
	assert(kV2ChChannelOffset == 1);
	assert(kV2ChPathLenOffset == 2);
	assert(kV2ChTimestampOffset == 4);
	assert(kV2ChTextOffset == 8);
	assert(kV2ChMinLength == 9);

	printf("  PASS: V2 backward compatibility offsets correct\n");
}


// ============================================================================
// Test 5: Battery/storage frame offsets
// ============================================================================

static void
TestBattStorageOffsets()
{
	assert(kBattMvOffset == 1);
	assert(kStorageUsedOffset == 3);
	assert(kStorageTotalOffset == 7);

	// Simulate RSP_BATT_AND_STORAGE
	uint8 frame[11];
	frame[0] = 12;  // RSP_BATT_AND_STORAGE

	// Battery: 3700 mV
	uint16 battMv = 3700;
	frame[kBattMvOffset] = battMv & 0xFF;
	frame[kBattMvOffset + 1] = (battMv >> 8) & 0xFF;

	// Used: 1024 KB
	uint32 usedKb = 1024;
	frame[kStorageUsedOffset] = usedKb & 0xFF;
	frame[kStorageUsedOffset + 1] = (usedKb >> 8) & 0xFF;
	frame[kStorageUsedOffset + 2] = (usedKb >> 16) & 0xFF;
	frame[kStorageUsedOffset + 3] = (usedKb >> 24) & 0xFF;

	// Total: 4096 KB
	uint32 totalKb = 4096;
	frame[kStorageTotalOffset] = totalKb & 0xFF;
	frame[kStorageTotalOffset + 1] = (totalKb >> 8) & 0xFF;
	frame[kStorageTotalOffset + 2] = (totalKb >> 16) & 0xFF;
	frame[kStorageTotalOffset + 3] = (totalKb >> 24) & 0xFF;

	assert(ReadLE16(frame + kBattMvOffset) == 3700);
	assert(ReadLE32(frame + kStorageUsedOffset) == 1024);
	assert(ReadLE32(frame + kStorageTotalOffset) == 4096);

	printf("  PASS: Battery/storage offsets match protocol spec\n");
}


// ============================================================================
// Test 6: Stats subtypes and offsets
// ============================================================================

static void
TestStatsOffsets()
{
	assert(kStatsSubtypeCore == 0);
	assert(kStatsSubtypeRadio == 1);
	assert(kStatsSubtypePackets == 2);

	assert(kStatsCoreSubtypeOffset == 1);
	assert(kStatsCoreBattOffset == 2);
	assert(kStatsCoreUptimeOffset == 4);

	assert(kStatsRadioNoiseOffset == 2);
	assert(kStatsRadioRssiOffset == 4);
	assert(kStatsRadioSnrOffset == 5);

	assert(kStatsPacketsRxOffset == 2);
	assert(kStatsPacketsTxOffset == 6);

	// Simulate RSP_STATS core frame
	uint8 frame[12];
	memset(frame, 0, sizeof(frame));
	frame[0] = 24;  // RSP_STATS
	frame[kStatsCoreSubtypeOffset] = kStatsSubtypeCore;

	// Battery: 3800 mV
	uint16 battMv = 3800;
	frame[kStatsCoreBattOffset] = battMv & 0xFF;
	frame[kStatsCoreBattOffset + 1] = (battMv >> 8) & 0xFF;

	// Uptime: 86400 seconds
	uint32 uptime = 86400;
	frame[kStatsCoreUptimeOffset] = uptime & 0xFF;
	frame[kStatsCoreUptimeOffset + 1] = (uptime >> 8) & 0xFF;
	frame[kStatsCoreUptimeOffset + 2] = (uptime >> 16) & 0xFF;
	frame[kStatsCoreUptimeOffset + 3] = (uptime >> 24) & 0xFF;

	assert(frame[kStatsCoreSubtypeOffset] == 0);
	assert(ReadLE16(frame + kStatsCoreBattOffset) == 3800);
	assert(ReadLE32(frame + kStatsCoreUptimeOffset) == 86400);

	printf("  PASS: Stats subtypes and offsets match protocol spec\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Protocol Frame Offset Constants Tests ===\n\n");

	TestContactFrameOffsets();
	TestV3DmOffsets();
	TestV3ChannelOffsets();
	TestV2Offsets();
	TestBattStorageOffsets();
	TestStatsOffsets();

	printf("\nAll 6 tests passed.\n");
	return 0;
}
