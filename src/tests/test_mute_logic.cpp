/*
 * Test: Mute logic for contacts and channels
 * Verifies mute key storage, retrieval, channel key format,
 * and interaction with DatabaseManager.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdlib>

// Haiku headers
#include <Application.h>
#include <Message.h>
#include <String.h>

#include "../DatabaseManager.h"
#include "../Constants.h"
#include "../Utils.h"


// ============================================================================
// Test 1: Mute key storage and retrieval
// ============================================================================

static void
TestMuteStorageRetrieval()
{
	DatabaseManager* db = DatabaseManager::Instance();

	// Initially not muted
	assert(!db->IsMuted("aabbccddeeff"));
	assert(!db->IsMuted("112233445566"));

	// Mute a contact
	assert(db->SetMuted("aabbccddeeff", true));
	assert(db->IsMuted("aabbccddeeff"));
	assert(!db->IsMuted("112233445566"));

	// Mute another
	assert(db->SetMuted("112233445566", true));
	assert(db->IsMuted("112233445566"));

	// Unmute the first
	assert(db->SetMuted("aabbccddeeff", false));
	assert(!db->IsMuted("aabbccddeeff"));
	assert(db->IsMuted("112233445566"));

	// Cleanup
	db->SetMuted("112233445566", false);

	printf("  PASS: Mute key storage and retrieval\n");
}


// ============================================================================
// Test 2: LoadAllMuted populates BMessage correctly
// ============================================================================

static void
TestLoadAllMuted()
{
	DatabaseManager* db = DatabaseManager::Instance();

	// Set up some muted keys
	db->SetMuted("aabbccddeeff", true);
	db->SetMuted("ch_public", true);
	db->SetMuted("ch_1", true);
	db->SetMuted("unmuted_key", false);

	BMessage msg;
	db->LoadAllMuted(&msg);

	// Count muted keys
	int32 count = 0;
	const char* key;
	bool foundContact = false, foundChPublic = false, foundCh1 = false;
	while (msg.FindString("muted_key", count, &key) == B_OK) {
		if (strcmp(key, "aabbccddeeff") == 0) foundContact = true;
		if (strcmp(key, "ch_public") == 0) foundChPublic = true;
		if (strcmp(key, "ch_1") == 0) foundCh1 = true;
		count++;
	}

	assert(count >= 3);
	assert(foundContact);
	assert(foundChPublic);
	assert(foundCh1);

	// Cleanup
	db->SetMuted("aabbccddeeff", false);
	db->SetMuted("ch_public", false);
	db->SetMuted("ch_1", false);

	printf("  PASS: LoadAllMuted populates BMessage correctly\n");
}


// ============================================================================
// Test 3: Channel key format
// ============================================================================

static void
TestChannelKeyFormat()
{
	// Public channel should use "ch_public"
	char key[16];
	uint8 channelIdx = 0;
	if (channelIdx == 0)
		strlcpy(key, "ch_public", sizeof(key));
	else
		snprintf(key, sizeof(key), "ch_%d", (int)channelIdx);
	assert(strcmp(key, "ch_public") == 0);

	// Private channel 1
	channelIdx = 1;
	if (channelIdx == 0)
		strlcpy(key, "ch_public", sizeof(key));
	else
		snprintf(key, sizeof(key), "ch_%d", (int)channelIdx);
	assert(strcmp(key, "ch_1") == 0);

	// Private channel 5
	channelIdx = 5;
	snprintf(key, sizeof(key), "ch_%d", (int)channelIdx);
	assert(strcmp(key, "ch_5") == 0);

	printf("  PASS: Channel key format\n");
}


// ============================================================================
// Test 4: Mute/unmute toggle idempotency
// ============================================================================

static void
TestMuteToggleIdempotency()
{
	DatabaseManager* db = DatabaseManager::Instance();

	// Mute twice should be fine
	assert(db->SetMuted("test_toggle", true));
	assert(db->SetMuted("test_toggle", true));
	assert(db->IsMuted("test_toggle"));

	// Unmute twice should be fine
	assert(db->SetMuted("test_toggle", false));
	assert(db->SetMuted("test_toggle", false));
	assert(!db->IsMuted("test_toggle"));

	printf("  PASS: Mute/unmute toggle idempotency\n");
}


// ============================================================================
// Test 5: Contact key format via FormatContactKey
// ============================================================================

static void
TestContactKeyFormat()
{
	uint8 prefix[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	char hex[kContactHexSize];
	FormatContactKey(hex, prefix);
	assert(strcmp(hex, "aabbccddeeff") == 0);

	uint8 prefix2[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
	FormatContactKey(hex, prefix2);
	assert(strcmp(hex, "010203040506") == 0);

	printf("  PASS: Contact key format via FormatContactKey\n");
}


// ============================================================================
// Test 6: MSG_CONTACT_MUTE_TOGGLE constant
// ============================================================================

static void
TestMuteMessageConstant()
{
	assert(MSG_CONTACT_MUTE_TOGGLE == 'cmut');
	printf("  PASS: MSG_CONTACT_MUTE_TOGGLE constant\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	// BApplication required for BMessage
	BApplication app("application/x-vnd.Sestriere-Test-Mute");

	printf("=== Mute Logic Tests ===\n\n");

	// Open test database in /tmp
	char tmpDir[] = "/tmp/sestriere_test_mute_XXXXXX";
	char* dir = mkdtemp(tmpDir);
	assert(dir != NULL);

	DatabaseManager* db = DatabaseManager::Instance();
	assert(db->Open(dir));

	TestMuteStorageRetrieval();
	TestLoadAllMuted();
	TestChannelKeyFormat();
	TestMuteToggleIdempotency();
	TestContactKeyFormat();
	TestMuteMessageConstant();

	db->Close();
	DatabaseManager::Destroy();

	// Cleanup temp dir
	BString rmCmd;
	rmCmd.SetToFormat("rm -rf %s", dir);
	system(rmCmd.String());

	printf("\nAll 6 tests passed.\n");
	return 0;
}
