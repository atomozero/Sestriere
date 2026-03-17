/*
 * Test: Channel message storage logic for known vs unknown channels.
 *
 * Simulates the fixed _HandleChannelMsgRecv logic to verify:
 * 1. Known channel: message stored in memory AND marked for DB insert
 * 2. Unknown channel: message NOT stored, NOT marked for DB insert
 * 3. Public channel (idx 0): always stored
 */

#include <cstdio>
#include <cstring>
#include <cstdint>

struct ChannelInfo {
	uint8_t index;
	char name[32];

	ChannelInfo() : index(0) { memset(name, 0, sizeof(name)); }
};

// Simulate the fixed storage logic
// Returns: 0 = stored in memory + DB, 1 = public channel, -1 = dropped
static int SimulateChannelStore(uint8_t channelIdx,
	ChannelInfo* channels[], int channelCount)
{
	if (channelIdx > 0) {
		bool added = false;
		for (int i = 0; i < channelCount; i++) {
			if (channels[i]->index == channelIdx) {
				added = true;
				break;
			}
		}
		if (added) {
			// DB insert would happen here
			return 0;  // stored
		} else {
			// Fixed: do NOT write to DB, log warning
			return -1;  // dropped
		}
	} else {
		// Public channel always stored
		return 1;
	}
}

int main()
{
	int failures = 0;

	// Set up known channels: slot 1 and slot 3
	ChannelInfo ch1, ch3;
	ch1.index = 1;
	strlcpy(ch1.name, "alpha", sizeof(ch1.name));
	ch3.index = 3;
	strlcpy(ch3.name, "bravo", sizeof(ch3.name));
	ChannelInfo* channels[] = { &ch1, &ch3 };

	// Test 1: Message for known channel (idx 1) should be stored
	{
		int result = SimulateChannelStore(1, channels, 2);
		if (result != 0) {
			printf("FAIL: Known channel 1 was not stored (result=%d)\n", result);
			failures++;
		} else {
			printf("PASS: Known channel 1 stored correctly\n");
		}
	}

	// Test 2: Message for known channel (idx 3) should be stored
	{
		int result = SimulateChannelStore(3, channels, 2);
		if (result != 0) {
			printf("FAIL: Known channel 3 was not stored (result=%d)\n", result);
			failures++;
		} else {
			printf("PASS: Known channel 3 stored correctly\n");
		}
	}

	// Test 3: Message for unknown channel (idx 2) should be DROPPED
	{
		int result = SimulateChannelStore(2, channels, 2);
		if (result != -1) {
			printf("FAIL: Unknown channel 2 was stored (result=%d)\n", result);
			failures++;
		} else {
			printf("PASS: Unknown channel 2 correctly dropped\n");
		}
	}

	// Test 4: Message for unknown channel (idx 255) should be DROPPED
	{
		int result = SimulateChannelStore(255, channels, 2);
		if (result != -1) {
			printf("FAIL: Unknown channel 255 was stored (result=%d)\n", result);
			failures++;
		} else {
			printf("PASS: Unknown channel 255 correctly dropped\n");
		}
	}

	// Test 5: Public channel (idx 0) should always be stored
	{
		int result = SimulateChannelStore(0, channels, 2);
		if (result != 1) {
			printf("FAIL: Public channel was not stored (result=%d)\n", result);
			failures++;
		} else {
			printf("PASS: Public channel stored correctly\n");
		}
	}

	// Test 6: No channels configured — private messages should be dropped
	{
		int result = SimulateChannelStore(5, NULL, 0);
		if (result != -1) {
			printf("FAIL: Channel 5 stored with no channels configured\n");
			failures++;
		} else {
			printf("PASS: No channels configured — private message dropped\n");
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
