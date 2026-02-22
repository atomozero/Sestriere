/*
 * Test: Unread message badge logic
 * Verifies unread counter increment, clear, and badge formatting
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

// Redefine Haiku types for standalone test
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;


// ============================================================================
// Minimal UnreadCounter for standalone testing
// ============================================================================

struct UnreadCounter {
	int32 count;

	UnreadCounter() : count(0) {}

	void Increment() { count++; }
	void Clear() { count = 0; }
	void Set(int32 c) { count = c; }

	void FormatBadge(char* buf, size_t size) const {
		if (count > 99)
			snprintf(buf, size, "99+");
		else
			snprintf(buf, size, "%u", (unsigned)count);
	}
};


// ============================================================================
// Test 1: Basic increment and clear
// ============================================================================

static void
TestIncrementAndClear()
{
	printf("Test 1: Increment and clear... ");

	UnreadCounter uc;
	assert(uc.count == 0);

	uc.Increment();
	assert(uc.count == 1);

	uc.Increment();
	uc.Increment();
	assert(uc.count == 3);

	uc.Clear();
	assert(uc.count == 0);

	printf("PASS\n");
}


// ============================================================================
// Test 2: Badge text formatting
// ============================================================================

static void
TestBadgeFormatting()
{
	printf("Test 2: Badge formatting... ");

	UnreadCounter uc;
	char buf[8];

	uc.Set(1);
	uc.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "1") == 0);

	uc.Set(42);
	uc.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "42") == 0);

	uc.Set(99);
	uc.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "99") == 0);

	uc.Set(100);
	uc.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "99+") == 0);

	uc.Set(999);
	uc.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "99+") == 0);

	printf("PASS\n");
}


// ============================================================================
// Test 3: Unread only when not viewing contact
// ============================================================================

static void
TestUnreadOnlyWhenNotViewing()
{
	printf("Test 3: Unread only when not viewing contact... ");

	uint8 senderKey[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	uint8 viewingKey[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

	UnreadCounter uc;

	// Message from sender while viewing different contact
	if (memcmp(senderKey, viewingKey, 6) != 0) {
		uc.Increment();  // Should increment
	}
	assert(uc.count == 1);

	// Message from sender while viewing same contact
	uint8 sameKey[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	if (memcmp(senderKey, sameKey, 6) != 0) {
		uc.Increment();  // Should NOT increment
	}
	assert(uc.count == 1);  // Still 1, not incremented

	printf("PASS\n");
}


// ============================================================================
// Test 4: Multiple contacts independent counters
// ============================================================================

static void
TestMultipleContacts()
{
	printf("Test 4: Multiple independent counters... ");

	UnreadCounter contacts[3];

	contacts[0].Increment();
	contacts[0].Increment();
	contacts[0].Increment();

	contacts[1].Increment();

	// contacts[2] has no messages

	assert(contacts[0].count == 3);
	assert(contacts[1].count == 1);
	assert(contacts[2].count == 0);

	// Clear one doesn't affect others
	contacts[0].Clear();
	assert(contacts[0].count == 0);
	assert(contacts[1].count == 1);

	printf("PASS\n");
}


// ============================================================================
// Test 5: Channel unread badge
// ============================================================================

static void
TestChannelUnread()
{
	printf("Test 5: Channel unread badge... ");

	UnreadCounter channel;

	// Multiple channel messages increment
	for (int i = 0; i < 50; i++)
		channel.Increment();

	assert(channel.count == 50);

	char buf[8];
	channel.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "50") == 0);

	// Over 99
	for (int i = 0; i < 60; i++)
		channel.Increment();

	assert(channel.count == 110);
	channel.FormatBadge(buf, sizeof(buf));
	assert(strcmp(buf, "99+") == 0);

	printf("PASS\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Unread Message Badge Tests ===\n");

	TestIncrementAndClear();
	TestBadgeFormatting();
	TestUnreadOnlyWhenNotViewing();
	TestMultipleContacts();
	TestChannelUnread();

	printf("\nAll unread badge tests passed!\n");
	return 0;
}
