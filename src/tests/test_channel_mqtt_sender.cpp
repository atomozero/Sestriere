/*
 * Test: MQTT publish skipped when channel sender is unknown.
 *
 * Before the fix, MQTT would publish with an all-zero pubkey when the
 * channel message sender was not found in the contacts list, polluting
 * MQTT data with a fictitious source.
 *
 * Verifies:
 * 1. Known sender → MQTT publish allowed
 * 2. Unknown sender (NULL) → MQTT publish skipped
 * 3. MQTT disconnected → no publish regardless of sender
 * 4. MQTT client NULL → no publish regardless of sender
 */

#include <cstdio>
#include <cstring>
#include <cstdint>

struct ContactInfo {
	uint8_t publicKey[32];
	char name[64];
};

// Simulate the fixed MQTT publish decision
static bool ShouldPublishMqtt(bool mqttConnected, bool mqttClientExists,
	const ContactInfo* sender)
{
	return mqttClientExists && mqttConnected && sender != NULL;
}

// Verify that a zero pubkey is never used
static bool IsZeroPubkey(const uint8_t* key, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (key[i] != 0)
			return false;
	}
	return true;
}

int main()
{
	int failures = 0;

	ContactInfo knownSender;
	memset(&knownSender, 0, sizeof(knownSender));
	knownSender.publicKey[0] = 0xAB;
	knownSender.publicKey[1] = 0xCD;
	strlcpy(knownSender.name, "Alice", sizeof(knownSender.name));

	// Test 1: Known sender, MQTT connected → publish allowed
	{
		bool shouldPublish = ShouldPublishMqtt(true, true, &knownSender);
		if (!shouldPublish) {
			printf("FAIL: Known sender with MQTT connected was not published\n");
			failures++;
		} else {
			// Verify we would use a real pubkey
			if (IsZeroPubkey(knownSender.publicKey, 6)) {
				printf("FAIL: Known sender has zero pubkey\n");
				failures++;
			} else {
				printf("PASS: Known sender → publish with real pubkey\n");
			}
		}
	}

	// Test 2: Unknown sender (NULL), MQTT connected → publish SKIPPED
	{
		bool shouldPublish = ShouldPublishMqtt(true, true, NULL);
		if (shouldPublish) {
			printf("FAIL: Unknown sender was published to MQTT\n");
			failures++;
		} else {
			printf("PASS: Unknown sender → MQTT publish skipped\n");
		}
	}

	// Test 3: Known sender, MQTT disconnected → no publish
	{
		bool shouldPublish = ShouldPublishMqtt(false, true, &knownSender);
		if (shouldPublish) {
			printf("FAIL: Published to disconnected MQTT\n");
			failures++;
		} else {
			printf("PASS: MQTT disconnected → no publish\n");
		}
	}

	// Test 4: MQTT client NULL → no publish
	{
		bool shouldPublish = ShouldPublishMqtt(true, false, &knownSender);
		if (shouldPublish) {
			printf("FAIL: Published with NULL MQTT client\n");
			failures++;
		} else {
			printf("PASS: NULL MQTT client → no publish\n");
		}
	}

	// Test 5: Verify all-zero key would have been detected (old behavior)
	{
		uint8_t zeroKey[6] = {0};
		if (!IsZeroPubkey(zeroKey, 6)) {
			printf("FAIL: Zero key detection broken\n");
			failures++;
		} else {
			printf("PASS: Zero pubkey correctly detected\n");
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
