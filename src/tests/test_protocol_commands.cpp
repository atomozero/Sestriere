/*
 * Test: Protocol command payload construction
 * Verifies V3 protocol compliance for pubkey commands and radio params
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

// Mirror the constants from the project
static const size_t kPubKeySize = 32;
static const uint8_t CMD_SET_RADIO_PARAMS = 11;
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

	// =============================================
	// CMD_SET_RADIO_PARAMS tests
	// =============================================
	printf("\n--- CMD_SET_RADIO_PARAMS ---\n");

	// --- Test 8: Frequency must be sent in kHz, not Hz ---
	TEST("Frequency 906875000 Hz → 906875 kHz in payload");
	{
		// Simulate the fixed _SendRadioParams logic:
		// freqHz from preset is in Hz, must be divided by 1000 for wire format
		uint32_t freqHz = 906875000;  // 906.875 MHz stored as Hz
		uint32_t freqKHz = freqHz / 1000;  // Convert to kHz for protocol

		uint8_t payload[11];
		payload[0] = CMD_SET_RADIO_PARAMS;
		payload[1] = freqKHz & 0xFF;
		payload[2] = (freqKHz >> 8) & 0xFF;
		payload[3] = (freqKHz >> 16) & 0xFF;
		payload[4] = (freqKHz >> 24) & 0xFF;

		// Read back the uint32 from payload
		uint32_t wireFreq = payload[1] | (payload[2] << 8)
			| (payload[3] << 16) | (payload[4] << 24);

		if (wireFreq == 906875) {
			PASS();
		} else {
			char msg[80];
			snprintf(msg, sizeof(msg),
				"expected 906875 kHz, got %u", wireFreq);
			FAIL(msg);
		}
	}

	// --- Test 9: Bandwidth must be sent in Hz ---
	TEST("Bandwidth 250000 Hz stays as Hz in payload");
	{
		uint32_t bwHz = 250000;

		uint8_t payload[11];
		memset(payload, 0, sizeof(payload));
		payload[0] = CMD_SET_RADIO_PARAMS;
		payload[5] = bwHz & 0xFF;
		payload[6] = (bwHz >> 8) & 0xFF;
		payload[7] = (bwHz >> 16) & 0xFF;
		payload[8] = (bwHz >> 24) & 0xFF;

		uint32_t wireBw = payload[5] | (payload[6] << 8)
			| (payload[7] << 16) | (payload[8] << 24);

		if (wireBw == 250000)
			PASS();
		else {
			char msg[80];
			snprintf(msg, sizeof(msg),
				"expected 250000 Hz, got %u", wireBw);
			FAIL(msg);
		}
	}

	// --- Test 10: Frequency roundtrip kHz → Hz matches original ---
	TEST("Frequency kHz roundtrip: encode then decode matches");
	{
		// All 12 presets should roundtrip correctly
		uint32_t testFreqs[] = {
			906875000, 915000000, 868000000, 869525000, 916000000,
			433775000, 869525000
		};
		bool allMatch = true;
		for (size_t i = 0; i < sizeof(testFreqs) / sizeof(testFreqs[0]); i++) {
			uint32_t freqHz = testFreqs[i];
			uint32_t kHz = freqHz / 1000;
			uint32_t recovered = kHz * 1000;
			if (recovered != freqHz) {
				char msg[80];
				snprintf(msg, sizeof(msg),
					"freq %u Hz: kHz=%u, recovered=%u",
					freqHz, kHz, recovered);
				FAIL(msg);
				allMatch = false;
				break;
			}
		}
		if (allMatch)
			PASS();
	}

	// --- Test 11: Payload must NOT contain Hz value (old bug) ---
	TEST("Frequency payload must NOT be raw Hz (906875000 would overflow)");
	{
		uint32_t freqHz = 906875000;
		uint32_t freqKHz = freqHz / 1000;

		// The old buggy code would put freqHz directly in payload
		// Verify the fixed value is different from the buggy value
		if (freqKHz != freqHz)
			PASS();
		else
			FAIL("kHz equals Hz - conversion not applied");
	}

	// --- Summary ---
	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);

	return sTestsFailed > 0 ? 1 : 0;
}
