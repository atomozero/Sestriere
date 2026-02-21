/*
 * Test: Utils.h helper functions
 * Verifies ReadLE16/ReadLE32 and FormatHex helpers produce correct results
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>


// Include the header under test
#include "../Utils.h"


// ============================================================================
// Test 1: ReadLE16 reads 2-byte little-endian unsigned
// ============================================================================

static void
TestReadLE16()
{
	uint8_t data[] = {0x34, 0x12};
	uint16_t result = ReadLE16(data);
	assert(result == 0x1234);

	// Zero
	uint8_t zero[] = {0x00, 0x00};
	assert(ReadLE16(zero) == 0);

	// Max
	uint8_t max[] = {0xFF, 0xFF};
	assert(ReadLE16(max) == 0xFFFF);

	// Battery voltage example: 3700 mV = 0x0E74
	uint8_t batt[] = {0x74, 0x0E};
	assert(ReadLE16(batt) == 3700);

	printf("  PASS: ReadLE16 correctly reads little-endian uint16\n");
}


// ============================================================================
// Test 2: ReadLE16Signed reads 2-byte little-endian signed
// ============================================================================

static void
TestReadLE16Signed()
{
	// Positive
	uint8_t pos[] = {0x34, 0x12};
	assert(ReadLE16Signed(pos) == 0x1234);

	// Negative: -100 = 0xFF9C
	uint8_t neg[] = {0x9C, 0xFF};
	assert(ReadLE16Signed(neg) == -100);

	// Noise floor example: -95 dBm = 0xFFA1
	uint8_t noise[] = {0xA1, 0xFF};
	assert(ReadLE16Signed(noise) == -95);

	printf("  PASS: ReadLE16Signed correctly reads signed int16\n");
}


// ============================================================================
// Test 3: ReadLE32 reads 4-byte little-endian unsigned
// ============================================================================

static void
TestReadLE32()
{
	uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
	uint32_t result = ReadLE32(data);
	assert(result == 0x12345678);

	// Zero
	uint8_t zero[] = {0x00, 0x00, 0x00, 0x00};
	assert(ReadLE32(zero) == 0);

	// Max
	uint8_t max[] = {0xFF, 0xFF, 0xFF, 0xFF};
	assert(ReadLE32(max) == 0xFFFFFFFF);

	// Frequency example: 906875 kHz = 0x000DD67B
	uint8_t freq[] = {0x7B, 0xD6, 0x0D, 0x00};
	assert(ReadLE32(freq) == 906875);

	// Uptime example: 86400 seconds (1 day) = 0x00015180
	uint8_t uptime[] = {0x80, 0x51, 0x01, 0x00};
	assert(ReadLE32(uptime) == 86400);

	printf("  PASS: ReadLE32 correctly reads little-endian uint32\n");
}


// ============================================================================
// Test 4: ReadLE32Signed reads 4-byte little-endian signed
// ============================================================================

static void
TestReadLE32Signed()
{
	// Positive latitude: 45440800 microdegrees (45.4408°)
	uint8_t lat[] = {0x20, 0x5F, 0xB5, 0x02};
	assert(ReadLE32Signed(lat) == 45440800);

	// Negative longitude: -122675200 microdegrees (-122.6752°)
	int32_t expected = -122675200;
	uint8_t lon[4];
	uint32_t uval = (uint32_t)expected;
	lon[0] = uval & 0xFF;
	lon[1] = (uval >> 8) & 0xFF;
	lon[2] = (uval >> 16) & 0xFF;
	lon[3] = (uval >> 24) & 0xFF;
	assert(ReadLE32Signed(lon) == expected);

	printf("  PASS: ReadLE32Signed correctly reads signed int32\n");
}


// ============================================================================
// Test 5: ReadLE functions work at arbitrary offsets
// ============================================================================

static void
TestReadAtOffset()
{
	// Simulate a protocol frame: [code][batt_lo][batt_hi][uptime_0..3]
	uint8_t frame[] = {0x42, 0x74, 0x0E, 0x80, 0x51, 0x01, 0x00};

	assert(ReadLE16(frame + 1) == 3700);   // Battery at offset 1
	assert(ReadLE32(frame + 3) == 86400);  // Uptime at offset 3

	printf("  PASS: ReadLE functions work correctly at arbitrary offsets\n");
}


// ============================================================================
// Test 6: FormatHexBytes formats correctly
// ============================================================================

static void
TestFormatHexBytes()
{
	uint8_t data[] = {0xAA, 0xBB, 0xCC, 0x01, 0x02, 0xFF};
	char buf[16];
	memset(buf, 0, sizeof(buf));

	FormatHexBytes(buf, data, 6);
	assert(strcmp(buf, "AABBCC0102FF") == 0);

	// Single byte
	memset(buf, 0, sizeof(buf));
	FormatHexBytes(buf, data, 1);
	assert(buf[0] == 'A' && buf[1] == 'A');

	printf("  PASS: FormatHexBytes produces correct uppercase hex\n");
}


// ============================================================================
// Test 7: FormatPubKeyPrefix formats 6-byte prefix
// ============================================================================

static void
TestFormatPubKeyPrefix()
{
	uint8_t prefix[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23};
	char buf[16];
	memset(buf, 0, sizeof(buf));

	FormatPubKeyPrefix(buf, prefix);
	assert(strcmp(buf, "DEADBEEF0123") == 0);
	assert(strlen(buf) == 12);
	assert(buf[12] == '\0');

	printf("  PASS: FormatPubKeyPrefix produces 12-char hex string\n");
}


// ============================================================================
// Test 8: FormatPubKeyFull formats 32-byte key
// ============================================================================

static void
TestFormatPubKeyFull()
{
	uint8_t key[32];
	for (int i = 0; i < 32; i++)
		key[i] = (uint8_t)i;

	char buf[68];
	memset(buf, 0, sizeof(buf));

	FormatPubKeyFull(buf, key);
	assert(strlen(buf) == 64);
	assert(buf[64] == '\0');

	// Verify first few bytes
	assert(buf[0] == '0' && buf[1] == '0');  // 0x00
	assert(buf[2] == '0' && buf[3] == '1');  // 0x01
	assert(buf[4] == '0' && buf[5] == '2');  // 0x02

	// Verify last byte: 0x1F = "1F"
	assert(buf[62] == '1' && buf[63] == 'F');

	printf("  PASS: FormatPubKeyFull produces 64-char hex string\n");
}


// ============================================================================
// Test 9: ReadLE32 matches manual extraction
// ============================================================================

static void
TestReadLE32MatchesManual()
{
	// Verify ReadLE32 produces identical results to the old inline pattern
	uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

	// Old manual pattern for offset 0
	uint32_t manual0 = (uint32_t)data[0] | ((uint32_t)data[1] << 8)
		| ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
	assert(ReadLE32(data + 0) == manual0);

	// Old manual pattern for offset 4
	uint32_t manual4 = (uint32_t)data[4] | ((uint32_t)data[5] << 8)
		| ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
	assert(ReadLE32(data + 4) == manual4);

	// Old manual pattern for offset 2
	uint32_t manual2 = data[2] | (data[3] << 8)
		| (data[4] << 16) | (data[5] << 24);
	assert(ReadLE32(data + 2) == manual2);

	printf("  PASS: ReadLE32 matches manual inline extraction\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Utils.h Helper Function Tests ===\n\n");

	TestReadLE16();
	TestReadLE16Signed();
	TestReadLE32();
	TestReadLE32Signed();
	TestReadAtOffset();
	TestFormatHexBytes();
	TestFormatPubKeyPrefix();
	TestFormatPubKeyFull();
	TestReadLE32MatchesManual();

	printf("\nAll 9 tests passed.\n");
	return 0;
}
