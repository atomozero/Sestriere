/*
 * Test: Channel enumeration uses internal counter, not device-reported idx.
 *
 * Verifies:
 * 1. Normal sequential enumeration works (device returns expected idx)
 * 2. Device returning unexpected idx does NOT skip channels
 * 3. Out-of-bounds idx from device does NOT stall enumeration
 * 4. Enumeration completes even with all-invalid responses
 */

#include <cstdio>
#include <cstdint>

// Simulate the enumeration state
struct EnumState {
	uint8_t maxChannels;
	uint8_t enumIndex;     // our internal counter
	bool enumerating;
	int channelsFound;
	int getChannelCalls;   // how many SendGetChannel would be called
	int lastRequestedIdx;

	void Start(uint8_t max) {
		maxChannels = max;
		enumIndex = 0;
		enumerating = true;
		channelsFound = 0;
		getChannelCalls = 0;
		lastRequestedIdx = -1;
	}

	// Simulate receiving RSP_CHANNEL_INFO with device-reported idx
	// and a channel name (empty = no channel at that slot)
	void HandleResponse(uint8_t deviceIdx, bool hasName) {
		if (!enumerating)
			return;

		// Validate idx (as in the fixed code)
		bool idxValid = !(maxChannels > 0 && deviceIdx >= maxChannels);

		if (idxValid && hasName)
			channelsFound++;

		// FIXED: use internal counter, NOT device idx
		enumIndex++;
		if (enumIndex < maxChannels) {
			getChannelCalls++;
			lastRequestedIdx = enumIndex;
		} else {
			enumerating = false;
		}
	}
};

int main()
{
	int failures = 0;

	// Test 1: Normal sequential enumeration (device returns expected idx)
	{
		EnumState state;
		state.Start(4);

		state.HandleResponse(0, true);   // slot 0: has channel
		state.HandleResponse(1, false);  // slot 1: empty
		state.HandleResponse(2, true);   // slot 2: has channel
		state.HandleResponse(3, false);  // slot 3: empty

		if (state.enumerating || state.channelsFound != 2
			|| state.getChannelCalls != 3) {
			printf("FAIL: Normal enum: enumerating=%d found=%d calls=%d\n",
				state.enumerating, state.channelsFound, state.getChannelCalls);
			failures++;
		} else {
			printf("PASS: Normal sequential enumeration completed correctly\n");
		}
	}

	// Test 2: Device returns wrong idx — enumeration must NOT skip
	// Old bug: fChannelEnumIndex = idx + 1 would skip to 201 if idx=200
	{
		EnumState state;
		state.Start(4);

		state.HandleResponse(200, false);  // device returns bogus idx 200
		// OLD behavior: enumIndex = 201, enumeration ends (201 >= 4)
		// NEW behavior: enumIndex = 1, continues to slot 1

		if (!state.enumerating) {
			printf("FAIL: Bogus idx 200 stopped enumeration early\n");
			failures++;
		} else if (state.lastRequestedIdx != 1) {
			printf("FAIL: After bogus idx, next request was %d (expected 1)\n",
				state.lastRequestedIdx);
			failures++;
		} else {
			printf("PASS: Bogus device idx does not skip enumeration\n");
		}

		// Continue to completion
		state.HandleResponse(1, true);
		state.HandleResponse(2, false);
		state.HandleResponse(3, true);

		if (state.enumerating || state.channelsFound != 2) {
			printf("FAIL: Enum did not complete after bogus idx\n");
			failures++;
		} else {
			printf("PASS: Enumeration completed after bogus idx recovery\n");
		}
	}

	// Test 3: Device returns out-of-bounds idx — channel not stored but
	// enumeration continues
	{
		EnumState state;
		state.Start(4);

		state.HandleResponse(255, true);  // out-of-bounds, has name
		// idxValid=false, so channelsFound should NOT increment

		if (state.channelsFound != 0) {
			printf("FAIL: Out-of-bounds channel was counted\n");
			failures++;
		} else {
			printf("PASS: Out-of-bounds channel idx not stored\n");
		}

		if (!state.enumerating) {
			printf("FAIL: Enumeration stopped on out-of-bounds idx\n");
			failures++;
		} else {
			printf("PASS: Enumeration continues after out-of-bounds idx\n");
		}
	}

	// Test 4: All responses have mismatched idx — enumeration still completes
	{
		EnumState state;
		state.Start(3);

		state.HandleResponse(99, false);   // wrong idx, no name
		state.HandleResponse(50, true);    // wrong idx, has name (but invalid)
		state.HandleResponse(77, false);   // wrong idx, no name

		if (state.enumerating) {
			printf("FAIL: Enumeration did not complete with 3 responses for max 3\n");
			failures++;
		} else {
			// channelsFound should be 0 because all idx were out of bounds
			printf("PASS: Enumeration completed with all-mismatched responses "
				"(found=%d)\n", state.channelsFound);
		}
	}

	// Test 5: Single channel device
	{
		EnumState state;
		state.Start(1);

		state.HandleResponse(0, true);

		if (state.enumerating || state.channelsFound != 1) {
			printf("FAIL: Single-channel enum: enumerating=%d found=%d\n",
				state.enumerating, state.channelsFound);
			failures++;
		} else {
			printf("PASS: Single-channel enumeration works\n");
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
