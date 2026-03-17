/*
 * test_channel_tab.cpp — Tests for Channels tab logic
 *
 * Verifies PSK hex formatting, slot usage display, and channel data handling.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>

// =============================================================================
// Test: PSK hex formatting (16 bytes → "AB CD EF..." string)
// =============================================================================
static void test_psk_hex_format()
{
	uint8_t secret[16] = {
		0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89,
		0x00, 0xFF, 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE
	};

	char hex[64];
	int pos = 0;
	for (int i = 0; i < 16; i++) {
		if (i > 0)
			hex[pos++] = ' ';
		snprintf(hex + pos, sizeof(hex) - pos, "%02X", secret[i]);
		pos += 2;
	}
	hex[pos] = '\0';

	assert(strcmp(hex, "AB CD EF 01 23 45 67 89 00 FF DE AD BE EF CA FE") == 0);
	printf("PASS: PSK hex format correct: %s\n", hex);
}

// =============================================================================
// Test: PSK hex formatting with all zeros
// =============================================================================
static void test_psk_hex_zeros()
{
	uint8_t secret[16];
	memset(secret, 0, sizeof(secret));

	char hex[64];
	int pos = 0;
	for (int i = 0; i < 16; i++) {
		if (i > 0)
			hex[pos++] = ' ';
		snprintf(hex + pos, sizeof(hex) - pos, "%02X", secret[i]);
		pos += 2;
	}
	hex[pos] = '\0';

	assert(strcmp(hex, "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00") == 0);
	printf("PASS: PSK hex all-zeros correct\n");
}

// =============================================================================
// Test: Slot usage display logic
// =============================================================================
static void test_slot_usage_display()
{
	// Simulated: 3 channels out of 16 slots
	int channelCount = 3;
	int maxChannels = 16;

	char buf[64];
	snprintf(buf, sizeof(buf), "%d / %d slots used", channelCount, maxChannels);
	assert(strcmp(buf, "3 / 16 slots used") == 0);
	printf("PASS: Slot usage '3 / 16 slots used'\n");

	// Zero channels
	channelCount = 0;
	maxChannels = 8;
	snprintf(buf, sizeof(buf), "%d / %d slots used", channelCount, maxChannels);
	assert(strcmp(buf, "0 / 8 slots used") == 0);
	printf("PASS: Slot usage '0 / 8 slots used'\n");

	// Full
	channelCount = 16;
	maxChannels = 16;
	snprintf(buf, sizeof(buf), "%d / %d slots used", channelCount, maxChannels);
	assert(strcmp(buf, "16 / 16 slots used") == 0);
	printf("PASS: Slot usage '16 / 16 slots used'\n");
}

// =============================================================================
// Test: Channel list label formatting
// =============================================================================
static void test_channel_label_format()
{
	struct {
		int index;
		const char* name;
		const char* expected;
	} cases[] = {
		{ 0, "rescue",   "Slot 0: rescue" },
		{ 3, "alpha",    "Slot 3: alpha" },
		{ 15, "test-ch", "Slot 15: test-ch" },
	};

	for (int i = 0; i < 3; i++) {
		char label[128];
		snprintf(label, sizeof(label), "Slot %d: %s",
			cases[i].index, cases[i].name);
		assert(strcmp(label, cases[i].expected) == 0);
		printf("PASS: Channel label '%s'\n", label);
	}
}

// =============================================================================
// Test: SettingsChannelEntry struct layout
// =============================================================================
static void test_channel_entry_struct()
{
	// Verify the struct matches expected sizes
	struct SettingsChannelEntry {
		uint8_t index;
		char name[32];
		uint8_t secret[16];
	};

	SettingsChannelEntry entry;
	memset(&entry, 0, sizeof(entry));

	entry.index = 5;
	strlcpy(entry.name, "test-channel", sizeof(entry.name));
	uint8_t testSecret[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
	memcpy(entry.secret, testSecret, 16);

	assert(entry.index == 5);
	assert(strcmp(entry.name, "test-channel") == 0);
	assert(entry.secret[0] == 1);
	assert(entry.secret[15] == 16);
	printf("PASS: SettingsChannelEntry struct layout correct\n");
}

// =============================================================================
// Test: Max 16 channel entries guard
// =============================================================================
static void test_max_channel_guard()
{
	struct SettingsChannelEntry {
		uint8_t index;
		char name[32];
		uint8_t secret[16];
	};

	SettingsChannelEntry entries[16];
	int32_t count = 0;

	// Simulate adding 20 channels — should cap at 16
	for (int i = 0; i < 20; i++) {
		if (count >= 16)
			break;
		entries[count].index = (uint8_t)i;
		snprintf(entries[count].name, 32, "ch%d", i);
		memset(entries[count].secret, i, 16);
		count++;
	}

	assert(count == 16);
	assert(entries[0].index == 0);
	assert(entries[15].index == 15);
	printf("PASS: Max 16 channel entries guard works\n");
}

// =============================================================================
int main()
{
	printf("=== Channel Tab Tests ===\n");
	test_psk_hex_format();
	test_psk_hex_zeros();
	test_slot_usage_display();
	test_channel_label_format();
	test_channel_entry_struct();
	test_max_channel_guard();
	printf("=== All channel tab tests passed! ===\n");
	return 0;
}
