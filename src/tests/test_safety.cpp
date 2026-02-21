/*
 * Test: String safety and integer overflow fixes
 * Verifies strlcpy usage and overflow-safe percentage calculation
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

// Haiku type aliases for standalone test
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;


// ============================================================================
// Test 1: Contact name extraction via strlcpy (safe null termination)
// ============================================================================

static void
TestContactNameSafety()
{
	// Simulate protocol data: 32-byte name field, no null terminator
	uint8 nameField[32];
	memset(nameField, 'A', sizeof(nameField));  // All 'A', no null

	// Old unsafe pattern: memcpy + truncate at [31]
	char unsafeName[64];
	memcpy(unsafeName, nameField, 32);
	unsafeName[31] = '\0';  // Truncates at 31 chars

	// New safe pattern: copy to temp buffer with null, then strlcpy
	char nameBuf[33];
	memcpy(nameBuf, nameField, 32);
	nameBuf[32] = '\0';  // Null-terminate at full 32 chars

	char safeName[64];
	strlcpy(safeName, nameBuf, sizeof(safeName));

	// Safe version preserves all 32 characters
	assert(strlen(safeName) == 32);
	assert(strlen(unsafeName) == 31);  // Old version lost 1 char

	printf("  PASS: strlcpy preserves full 32-char name safely\n");
}


// ============================================================================
// Test 2: Contact name with embedded null
// ============================================================================

static void
TestContactNameEmbeddedNull()
{
	// Simulate name with null in middle: "TestNode\0\0\0..."
	uint8 nameField[32];
	memset(nameField, 0, sizeof(nameField));
	memcpy(nameField, "TestNode", 8);

	char nameBuf[33];
	memcpy(nameBuf, nameField, 32);
	nameBuf[32] = '\0';

	char name[64];
	strlcpy(name, nameBuf, sizeof(name));

	assert(strcmp(name, "TestNode") == 0);
	assert(strlen(name) == 8);

	printf("  PASS: strlcpy handles embedded null correctly\n");
}


// ============================================================================
// Test 3: Storage percentage overflow prevention
// ============================================================================

static void
TestStoragePercentageOverflow()
{
	// Old pattern: (usedKb * 100) / totalKb
	// This overflows uint32 when usedKb > 42949672 (~42 MB)

	uint32 usedKb = 100000000;   // 100 GB used
	uint32 totalKb = 200000000;  // 200 GB total

	// Old (overflow): usedKb * 100 = 10000000000 > UINT32_MAX (4294967295)
	uint32 oldResult = (usedKb * 100) / totalKb;
	// New (safe): cast to uint64 first
	int8 newResult = (int8)(((uint64)usedKb * 100) / totalKb);

	// Old result is wrong due to overflow
	// New result should be exactly 50%
	assert(newResult == 50);

	// Test edge case: full storage
	usedKb = totalKb;
	newResult = (int8)(((uint64)usedKb * 100) / totalKb);
	assert(newResult == 100);

	// Test zero usage
	usedKb = 0;
	newResult = (int8)(((uint64)usedKb * 100) / totalKb);
	assert(newResult == 0);

	// Test small values (no overflow even without fix)
	usedKb = 1024;
	totalKb = 4096;
	newResult = (int8)(((uint64)usedKb * 100) / totalKb);
	assert(newResult == 25);

	printf("  PASS: uint64 cast prevents storage percentage overflow\n");
}


// ============================================================================
// Test 4: Device name extraction safety
// ============================================================================

static void
TestDeviceNameExtraction()
{
	// Simulate protocol data with device name at offset 58
	uint8 data[128];
	memset(data, 0, sizeof(data));
	memcpy(data + 58, "MyDevice", 8);

	size_t length = 70;  // Frame length

	// New safe extraction pattern
	char tempName[64];
	size_t maxLen = length - 58;
	if (maxLen > sizeof(tempName) - 1)
		maxLen = sizeof(tempName) - 1;
	memcpy(tempName, data + 58, maxLen);
	tempName[maxLen] = '\0';

	char fDeviceName[64];
	strlcpy(fDeviceName, tempName, sizeof(fDeviceName));

	assert(strcmp(fDeviceName, "MyDevice") == 0);

	// Test with very long name (should truncate safely)
	memset(data + 58, 'X', 70);  // 70 bytes of 'X'
	length = 128;

	maxLen = length - 58;
	if (maxLen > sizeof(tempName) - 1)
		maxLen = sizeof(tempName) - 1;
	memcpy(tempName, data + 58, maxLen);
	tempName[maxLen] = '\0';
	strlcpy(fDeviceName, tempName, sizeof(fDeviceName));

	assert(strlen(fDeviceName) == 63);  // Truncated to fit

	printf("  PASS: Device name extraction handles long names safely\n");
}


// ============================================================================
// Test 5: strlcpy vs memcpy boundary behavior
// ============================================================================

static void
TestStrlcpyBoundary()
{
	// Verify strlcpy always null-terminates
	char dest[8];
	memset(dest, 'X', sizeof(dest));

	strlcpy(dest, "Hello World!", sizeof(dest));
	assert(strlen(dest) == 7);  // Truncated to 7 + null
	assert(dest[7] == '\0');

	// Empty string
	strlcpy(dest, "", sizeof(dest));
	assert(strlen(dest) == 0);
	assert(dest[0] == '\0');

	printf("  PASS: strlcpy boundary behavior correct\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== String Safety & Integer Overflow Tests ===\n\n");

	TestContactNameSafety();
	TestContactNameEmbeddedNull();
	TestStoragePercentageOverflow();
	TestDeviceNameExtraction();
	TestStrlcpyBoundary();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
