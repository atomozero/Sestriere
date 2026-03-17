/*
 * Test: GIF download only triggers when the channel is currently displayed.
 *
 * Simulates the isCurrentChannel check that guards GIF download logic.
 * Before the fix, GIF download would check the last item in ChatView
 * even when the message's channel was NOT the one being viewed.
 *
 * Verifies:
 * 1. GIF on currently viewed channel → triggers download
 * 2. GIF on different channel → does NOT trigger download
 * 3. GIF on public channel while viewing private → no trigger
 * 4. GIF on private channel while viewing public → no trigger
 * 5. Not viewing any channel (viewing DM) → no trigger
 */

#include <cstdio>
#include <cstdint>

// Simulate the isCurrentChannel logic from _HandleChannelMsgRecv
static bool IsCurrentChannel(bool sendingToChannel, int32_t selectedChannelIdx,
	uint8_t msgChannelIdx)
{
	if (!sendingToChannel)
		return false;
	if (msgChannelIdx == 0 && selectedChannelIdx < 0)
		return true;   // Viewing public, message is public
	if (msgChannelIdx > 0 && selectedChannelIdx == (int32_t)msgChannelIdx)
		return true;   // Viewing this private channel
	return false;
}

// Simulate the fixed GIF guard: only process if isCurrentChannel
static bool ShouldProcessGif(bool isCurrentChannel, bool isGifMessage)
{
	return isCurrentChannel && isGifMessage;
}

int main()
{
	int failures = 0;

	// Test 1: GIF on public channel, viewing public → trigger
	{
		bool isCurrent = IsCurrentChannel(true, -1, 0);
		bool shouldProcess = ShouldProcessGif(isCurrent, true);
		if (!shouldProcess) {
			printf("FAIL: GIF on public channel while viewing public not triggered\n");
			failures++;
		} else {
			printf("PASS: GIF on public channel while viewing public → triggers\n");
		}
	}

	// Test 2: GIF on private channel 3, viewing channel 3 → trigger
	{
		bool isCurrent = IsCurrentChannel(true, 3, 3);
		bool shouldProcess = ShouldProcessGif(isCurrent, true);
		if (!shouldProcess) {
			printf("FAIL: GIF on channel 3 while viewing 3 not triggered\n");
			failures++;
		} else {
			printf("PASS: GIF on channel 3 while viewing 3 → triggers\n");
		}
	}

	// Test 3: GIF on public channel, viewing private channel 2 → NO trigger
	{
		bool isCurrent = IsCurrentChannel(true, 2, 0);
		bool shouldProcess = ShouldProcessGif(isCurrent, true);
		if (shouldProcess) {
			printf("FAIL: GIF on public triggered while viewing channel 2\n");
			failures++;
		} else {
			printf("PASS: GIF on public while viewing channel 2 → no trigger\n");
		}
	}

	// Test 4: GIF on private channel 5, viewing public → NO trigger
	{
		bool isCurrent = IsCurrentChannel(true, -1, 5);
		bool shouldProcess = ShouldProcessGif(isCurrent, true);
		if (shouldProcess) {
			printf("FAIL: GIF on channel 5 triggered while viewing public\n");
			failures++;
		} else {
			printf("PASS: GIF on channel 5 while viewing public → no trigger\n");
		}
	}

	// Test 5: GIF on channel 1, viewing channel 2 → NO trigger
	{
		bool isCurrent = IsCurrentChannel(true, 2, 1);
		bool shouldProcess = ShouldProcessGif(isCurrent, true);
		if (shouldProcess) {
			printf("FAIL: GIF on channel 1 triggered while viewing channel 2\n");
			failures++;
		} else {
			printf("PASS: GIF on channel 1 while viewing channel 2 → no trigger\n");
		}
	}

	// Test 6: Not viewing any channel (viewing DM) → NO trigger
	{
		bool isCurrent = IsCurrentChannel(false, -1, 0);
		bool shouldProcess = ShouldProcessGif(isCurrent, true);
		if (shouldProcess) {
			printf("FAIL: GIF triggered while viewing DM\n");
			failures++;
		} else {
			printf("PASS: GIF while viewing DM → no trigger\n");
		}
	}

	// Test 7: Non-GIF message on current channel → no trigger
	{
		bool isCurrent = IsCurrentChannel(true, -1, 0);
		bool shouldProcess = ShouldProcessGif(isCurrent, false);
		if (shouldProcess) {
			printf("FAIL: Non-GIF message triggered GIF download\n");
			failures++;
		} else {
			printf("PASS: Non-GIF message on current channel → no trigger\n");
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
