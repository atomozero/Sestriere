/*
 * Test: Channel index bounds validation.
 *
 * Simulates the channelIdx validation logic in _HandleChannelMsgRecv
 * and _HandleChannelInfo to verify out-of-bounds indices are rejected.
 */

#include <cstdio>
#include <cstdint>

// Simulate _HandleChannelMsgRecv validation
// Returns true if channelIdx would be accepted, false if rejected
static bool ValidateChannelMsgIdx(uint8_t channelIdx, uint8_t maxChannels)
{
	// Public channel (idx 0) is always valid
	if (channelIdx > 0 && maxChannels > 0 && channelIdx >= maxChannels)
		return false;
	return true;
}

// Simulate _HandleChannelInfo validation
// Returns true if idx would be accepted, false if rejected
static bool ValidateChannelInfoIdx(uint8_t idx, uint8_t maxChannels)
{
	if (maxChannels > 0 && idx >= maxChannels)
		return false;
	return true;
}

int main()
{
	int failures = 0;
	const uint8_t maxChannels = 16;  // typical device value

	// --- _HandleChannelMsgRecv tests ---

	// Test 1: Public channel (idx 0) always valid
	{
		if (!ValidateChannelMsgIdx(0, maxChannels)) {
			printf("FAIL: MsgRecv public channel (0) rejected\n");
			failures++;
		} else {
			printf("PASS: MsgRecv public channel (0) accepted\n");
		}
	}

	// Test 2: Valid private channel (idx 1)
	{
		if (!ValidateChannelMsgIdx(1, maxChannels)) {
			printf("FAIL: MsgRecv valid channel 1 rejected\n");
			failures++;
		} else {
			printf("PASS: MsgRecv valid channel 1 accepted\n");
		}
	}

	// Test 3: Max valid channel (idx 15 with max 16)
	{
		if (!ValidateChannelMsgIdx(15, maxChannels)) {
			printf("FAIL: MsgRecv valid channel 15 rejected\n");
			failures++;
		} else {
			printf("PASS: MsgRecv valid channel 15 accepted\n");
		}
	}

	// Test 4: Out of bounds (idx 16 with max 16)
	{
		if (ValidateChannelMsgIdx(16, maxChannels)) {
			printf("FAIL: MsgRecv out-of-bounds channel 16 accepted\n");
			failures++;
		} else {
			printf("PASS: MsgRecv out-of-bounds channel 16 rejected\n");
		}
	}

	// Test 5: Way out of bounds (idx 255)
	{
		if (ValidateChannelMsgIdx(255, maxChannels)) {
			printf("FAIL: MsgRecv channel 255 accepted\n");
			failures++;
		} else {
			printf("PASS: MsgRecv channel 255 rejected\n");
		}
	}

	// Test 6: maxChannels=0 (device has no channel support), private idx
	// should still pass (we don't know the real limit)
	{
		if (!ValidateChannelMsgIdx(5, 0)) {
			printf("FAIL: MsgRecv channel 5 rejected with maxChannels=0\n");
			failures++;
		} else {
			printf("PASS: MsgRecv channel 5 passes when maxChannels unknown\n");
		}
	}

	// --- _HandleChannelInfo tests ---

	// Test 7: Valid info idx
	{
		if (!ValidateChannelInfoIdx(0, maxChannels)) {
			printf("FAIL: ChannelInfo idx 0 rejected\n");
			failures++;
		} else {
			printf("PASS: ChannelInfo idx 0 accepted\n");
		}
	}

	// Test 8: Out of bounds info idx
	{
		if (ValidateChannelInfoIdx(16, maxChannels)) {
			printf("FAIL: ChannelInfo idx 16 accepted\n");
			failures++;
		} else {
			printf("PASS: ChannelInfo idx 16 rejected\n");
		}
	}

	// Test 9: Max valid info idx
	{
		if (!ValidateChannelInfoIdx(15, maxChannels)) {
			printf("FAIL: ChannelInfo idx 15 rejected\n");
			failures++;
		} else {
			printf("PASS: ChannelInfo idx 15 accepted\n");
		}
	}

	// Test 10: Info idx with maxChannels=0 (unknown)
	{
		if (!ValidateChannelInfoIdx(5, 0)) {
			printf("FAIL: ChannelInfo idx 5 rejected with maxChannels=0\n");
			failures++;
		} else {
			printf("PASS: ChannelInfo idx 5 passes when maxChannels unknown\n");
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
