/*
 * Test: Duplicate channel index rejection in _HandleChannelInfo.
 *
 * Verifies:
 * 1. First channel with given index is accepted
 * 2. Second channel with same index is rejected (duplicate)
 * 3. Different indices are accepted independently
 * 4. After re-enumeration (list cleared), same index can be added again
 */

#include <cstdio>
#include <cstring>
#include <cstdint>

struct ChannelInfo {
	uint8_t index;
	char name[32];
};

struct ChannelList {
	ChannelInfo items[32];
	int count;

	ChannelList() : count(0) {}

	void Clear() { count = 0; }

	// Simulate the fixed _HandleChannelInfo logic
	// Returns true if channel was added, false if duplicate
	bool TryAdd(uint8_t idx, const char* name) {
		if (name[0] == '\0')
			return false;

		// Check for duplicate
		for (int i = 0; i < count; i++) {
			if (items[i].index == idx)
				return false;  // duplicate
		}

		items[count].index = idx;
		strlcpy(items[count].name, name, sizeof(items[count].name));
		count++;
		return true;
	}
};

int main()
{
	int failures = 0;

	// Test 1: First add succeeds
	{
		ChannelList list;
		bool added = list.TryAdd(1, "alpha");
		if (!added || list.count != 1) {
			printf("FAIL: First add of idx 1 failed\n");
			failures++;
		} else {
			printf("PASS: First add of idx 1 accepted\n");
		}
	}

	// Test 2: Duplicate same index rejected
	{
		ChannelList list;
		list.TryAdd(1, "alpha");
		bool added = list.TryAdd(1, "alpha_dup");
		if (added || list.count != 1) {
			printf("FAIL: Duplicate idx 1 was accepted (count=%d)\n", list.count);
			failures++;
		} else {
			printf("PASS: Duplicate idx 1 rejected\n");
		}
	}

	// Test 3: Different indices accepted independently
	{
		ChannelList list;
		list.TryAdd(0, "zero");
		list.TryAdd(1, "one");
		list.TryAdd(2, "two");
		if (list.count != 3) {
			printf("FAIL: Different indices: count=%d (expected 3)\n", list.count);
			failures++;
		} else {
			printf("PASS: Three different indices all accepted\n");
		}
	}

	// Test 4: Same index rejected even with different name
	{
		ChannelList list;
		list.TryAdd(5, "first_name");
		bool added = list.TryAdd(5, "different_name");
		if (added) {
			printf("FAIL: Same index with different name was accepted\n");
			failures++;
		} else {
			printf("PASS: Same index rejected regardless of name\n");
		}
	}

	// Test 5: After clearing list, same index can be added again
	{
		ChannelList list;
		list.TryAdd(3, "channel_three");
		list.Clear();
		bool added = list.TryAdd(3, "channel_three_v2");
		if (!added || list.count != 1) {
			printf("FAIL: After clear, idx 3 was not accepted\n");
			failures++;
		} else {
			printf("PASS: After clear, same index can be re-added\n");
		}
	}

	// Test 6: Empty name is not added (regardless of duplicate check)
	{
		ChannelList list;
		bool added = list.TryAdd(0, "");
		if (added || list.count != 0) {
			printf("FAIL: Empty name was added\n");
			failures++;
		} else {
			printf("PASS: Empty name correctly skipped\n");
		}
	}

	// Test 7: Multiple duplicates all rejected
	{
		ChannelList list;
		list.TryAdd(7, "seven");
		bool a = list.TryAdd(7, "dup1");
		bool b = list.TryAdd(7, "dup2");
		bool c = list.TryAdd(7, "dup3");
		if (a || b || c || list.count != 1) {
			printf("FAIL: Multiple duplicates not all rejected (count=%d)\n",
				list.count);
			failures++;
		} else {
			printf("PASS: Multiple duplicates of same index all rejected\n");
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
