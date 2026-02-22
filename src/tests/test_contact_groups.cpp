/*
 * Test: Contact group management via DatabaseManager
 * Verifies creation, membership, deletion, and constraints.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdlib>

// Haiku headers
#include <Application.h>
#include <String.h>

#include "../DatabaseManager.h"
#include "../Compat.h"
#include "../Constants.h"


// ============================================================================
// Test 1: Create and load groups
// ============================================================================

static void
TestCreateAndLoadGroups()
{
	DatabaseManager* db = DatabaseManager::Instance();

	assert(db->CreateGroup("Alpha"));
	assert(db->CreateGroup("Bravo"));
	assert(db->CreateGroup("Charlie"));

	OwningObjectList<BString> names;
	int32 count = db->LoadGroups(names);
	assert(count == 3);

	// Should be sorted alphabetically
	assert(*names.ItemAt(0) == "Alpha");
	assert(*names.ItemAt(1) == "Bravo");
	assert(*names.ItemAt(2) == "Charlie");

	printf("  PASS: Create and load groups\n");
}


// ============================================================================
// Test 2: Duplicate group names are ignored
// ============================================================================

static void
TestDuplicateGroupNames()
{
	DatabaseManager* db = DatabaseManager::Instance();

	// Creating same group again should succeed (INSERT OR IGNORE)
	assert(db->CreateGroup("Alpha"));

	OwningObjectList<BString> names;
	int32 count = db->LoadGroups(names);
	assert(count == 3);  // Still 3, not 4

	printf("  PASS: Duplicate group names are ignored\n");
}


// ============================================================================
// Test 3: Add contact to group
// ============================================================================

static void
TestAddContactToGroup()
{
	DatabaseManager* db = DatabaseManager::Instance();

	assert(db->AddContactToGroup("Alpha", "aabbccddeeff"));
	assert(db->AddContactToGroup("Alpha", "112233445566"));

	OwningObjectList<BString> members;
	int32 count = db->LoadGroupMembers("Alpha", members);
	assert(count == 2);

	printf("  PASS: Add contact to group\n");
}


// ============================================================================
// Test 4: GetContactGroup returns correct group
// ============================================================================

static void
TestGetContactGroup()
{
	DatabaseManager* db = DatabaseManager::Instance();

	BString group = db->GetContactGroup("aabbccddeeff");
	assert(group == "Alpha");

	// Contact not in any group returns empty
	BString noGroup = db->GetContactGroup("deadbeef0000");
	assert(noGroup.Length() == 0);

	printf("  PASS: GetContactGroup returns correct group\n");
}


// ============================================================================
// Test 5: One group per contact — adding to new group removes from old
// ============================================================================

static void
TestOneGroupPerContact()
{
	DatabaseManager* db = DatabaseManager::Instance();

	// Contact "aabbccddeeff" is in Alpha. Move to Bravo.
	assert(db->AddContactToGroup("Bravo", "aabbccddeeff"));

	BString group = db->GetContactGroup("aabbccddeeff");
	assert(group == "Bravo");

	// Alpha should now have only 1 member
	OwningObjectList<BString> alphaMembers;
	int32 alphaCount = db->LoadGroupMembers("Alpha", alphaMembers);
	assert(alphaCount == 1);
	assert(*alphaMembers.ItemAt(0) == "112233445566");

	// Bravo should have 1 member
	OwningObjectList<BString> bravoMembers;
	int32 bravoCount = db->LoadGroupMembers("Bravo", bravoMembers);
	assert(bravoCount == 1);
	assert(*bravoMembers.ItemAt(0) == "aabbccddeeff");

	printf("  PASS: One group per contact constraint\n");
}


// ============================================================================
// Test 6: Remove contact from group
// ============================================================================

static void
TestRemoveContactFromGroup()
{
	DatabaseManager* db = DatabaseManager::Instance();

	assert(db->RemoveContactFromGroup("Bravo", "aabbccddeeff"));

	OwningObjectList<BString> members;
	int32 count = db->LoadGroupMembers("Bravo", members);
	assert(count == 0);

	BString group = db->GetContactGroup("aabbccddeeff");
	assert(group.Length() == 0);

	printf("  PASS: Remove contact from group\n");
}


// ============================================================================
// Test 7: Delete group cascades to membership
// ============================================================================

static void
TestDeleteGroupCascade()
{
	DatabaseManager* db = DatabaseManager::Instance();

	// Alpha still has "112233445566"
	assert(db->DeleteGroup("Alpha"));

	// Group should be gone
	OwningObjectList<BString> names;
	int32 count = db->LoadGroups(names);
	assert(count == 2);  // Bravo and Charlie remain

	// Member should have no group
	BString group = db->GetContactGroup("112233445566");
	assert(group.Length() == 0);

	printf("  PASS: Delete group cascades to membership\n");
}


// ============================================================================
// Test 8: Message constants exist
// ============================================================================

static void
TestGroupMessageConstants()
{
	assert(MSG_GROUP_ADD_CONTACT == 'gadd');
	assert(MSG_GROUP_REMOVE_CONTACT == 'grmv');
	assert(MSG_GROUP_CREATE == 'gcrt');
	assert(MSG_GROUP_DELETE == 'gdel');

	printf("  PASS: Group message constants\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	BApplication app("application/x-vnd.Sestriere-Test-Groups");

	printf("=== Contact Groups Tests ===\n\n");

	// Open test database in /tmp
	char tmpDir[] = "/tmp/sestriere_test_groups_XXXXXX";
	char* dir = mkdtemp(tmpDir);
	assert(dir != NULL);

	DatabaseManager* db = DatabaseManager::Instance();
	assert(db->Open(dir));

	TestCreateAndLoadGroups();
	TestDuplicateGroupNames();
	TestAddContactToGroup();
	TestGetContactGroup();
	TestOneGroupPerContact();
	TestRemoveContactFromGroup();
	TestDeleteGroupCascade();
	TestGroupMessageConstants();

	db->Close();
	DatabaseManager::Destroy();

	// Cleanup temp dir
	BString rmCmd;
	rmCmd.SetToFormat("rm -rf %s", dir);
	system(rmCmd.String());

	printf("\nAll 8 tests passed.\n");
	return 0;
}
