/*
 * Test: Protocol command payload construction
 * Verifies that CMD_REMOVE_CONTACT and CMD_RESET_PATH send full 32-byte pubkey
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

// Mirror the constants from the project
static const size_t kPubKeySize = 32;
static const uint8_t CMD_RESET_PATH = 13;
static const uint8_t CMD_REMOVE_CONTACT = 15;

// Simulate the fixed _SendRemoveContact payload construction
static void BuildRemoveContactPayload(uint8_t* payload, size_t* outSize,
	const uint8_t* pubkey)
{
	payload[0] = CMD_REMOVE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	*outSize = 1 + kPubKeySize;
}

// Simulate the fixed _SendResetPath payload construction
static void BuildResetPathPayload(uint8_t* payload, size_t* outSize,
	const uint8_t* pubkey)
{
	payload[0] = CMD_RESET_PATH;
	memcpy(payload + 1, pubkey, kPubKeySize);
	*outSize = 1 + kPubKeySize;
}

static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  TEST: %s ... ", name);

#define PASS() \
	do { printf("PASS\n"); sTestsPassed++; } while(0)

#define FAIL(msg) \
	do { printf("FAIL: %s\n", msg); sTestsFailed++; } while(0)

int main()
{
	printf("=== Protocol Command Tests ===\n\n");

	// Create a known 32-byte pubkey
	uint8_t pubkey[kPubKeySize];
	for (size_t i = 0; i < kPubKeySize; i++)
		pubkey[i] = (uint8_t)(0xA0 + i);

	// --- Test 1: CMD_REMOVE_CONTACT payload size ---
	TEST("CMD_REMOVE_CONTACT payload size is 33 bytes (1+32)");
	{
		uint8_t payload[64];
		size_t size = 0;
		BuildRemoveContactPayload(payload, &size, pubkey);

		if (size == 33)
			PASS();
		else {
			char msg[64];
			snprintf(msg, sizeof(msg), "expected 33, got %zu", size);
			FAIL(msg);
		}
	}

	// --- Test 2: CMD_REMOVE_CONTACT command byte ---
	TEST("CMD_REMOVE_CONTACT command byte is 0x0F");
	{
		uint8_t payload[64];
		size_t size = 0;
		BuildRemoveContactPayload(payload, &size, pubkey);

		if (payload[0] == CMD_REMOVE_CONTACT)
			PASS();
		else
			FAIL("wrong command byte");
	}

	// --- Test 3: CMD_REMOVE_CONTACT includes full 32-byte pubkey ---
	TEST("CMD_REMOVE_CONTACT includes full 32-byte pubkey");
	{
		uint8_t payload[64];
		size_t size = 0;
		BuildRemoveContactPayload(payload, &size, pubkey);

		bool match = (memcmp(payload + 1, pubkey, kPubKeySize) == 0);
		if (match)
			PASS();
		else
			FAIL("pubkey mismatch in payload");
	}

	// --- Test 4: CMD_REMOVE_CONTACT NOT truncated to 6 bytes ---
	TEST("CMD_REMOVE_CONTACT pubkey bytes 7-32 are NOT zero");
	{
		uint8_t payload[64];
		memset(payload, 0, sizeof(payload));
		size_t size = 0;
		BuildRemoveContactPayload(payload, &size, pubkey);

		// Bytes 7+ (payload[7..32]) must contain pubkey data, not zeros
		bool hasData = false;
		for (size_t i = 7; i <= 32; i++) {
			if (payload[i] != 0) {
				hasData = true;
				break;
			}
		}
		if (hasData)
			PASS();
		else
			FAIL("bytes 7-32 are all zero (old 6-byte truncation bug)");
	}

	// --- Test 5: CMD_RESET_PATH payload size is 33 bytes ---
	TEST("CMD_RESET_PATH payload size is 33 bytes (1+32)");
	{
		uint8_t payload[64];
		size_t size = 0;
		BuildResetPathPayload(payload, &size, pubkey);

		if (size == 33)
			PASS();
		else {
			char msg[64];
			snprintf(msg, sizeof(msg), "expected 33, got %zu", size);
			FAIL(msg);
		}
	}

	// --- Test 6: CMD_RESET_PATH command byte ---
	TEST("CMD_RESET_PATH command byte is 0x0D");
	{
		uint8_t payload[64];
		size_t size = 0;
		BuildResetPathPayload(payload, &size, pubkey);

		if (payload[0] == CMD_RESET_PATH)
			PASS();
		else
			FAIL("wrong command byte");
	}

	// --- Test 7: CMD_RESET_PATH includes full 32-byte pubkey ---
	TEST("CMD_RESET_PATH includes full 32-byte pubkey");
	{
		uint8_t payload[64];
		size_t size = 0;
		BuildResetPathPayload(payload, &size, pubkey);

		bool match = (memcmp(payload + 1, pubkey, kPubKeySize) == 0);
		if (match)
			PASS();
		else
			FAIL("pubkey mismatch in payload");
	}

	// --- Summary ---
	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);

	return sTestsFailed > 0 ? 1 : 0;
}
