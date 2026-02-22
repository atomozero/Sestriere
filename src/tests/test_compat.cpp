/*
 * Test: Compat.h - OwningObjectList compatibility layer
 * Verifies that OwningObjectList<T> works correctly as an owning list,
 * and that plain BObjectList<T> works as non-owning, on any Haiku version.
 */

#include <cstdio>
#include <cstring>
#include <cassert>

#include "../Compat.h"
#include <String.h>


// Simple test struct with a flag to detect destruction
struct TestItem {
	int value;
	bool* destroyed;

	TestItem(int v, bool* flag)
		: value(v), destroyed(flag) {}

	~TestItem()
	{
		if (destroyed)
			*destroyed = true;
	}
};


// ============================================================================
// Test 1: OwningObjectList deletes items when cleared
// ============================================================================

static void
TestOwningListDeletesOnClear()
{
	bool destroyed1 = false;
	bool destroyed2 = false;
	bool destroyed3 = false;

	{
		OwningObjectList<TestItem> list;
		list.AddItem(new TestItem(1, &destroyed1));
		list.AddItem(new TestItem(2, &destroyed2));
		list.AddItem(new TestItem(3, &destroyed3));

		assert(list.CountItems() == 3);
		assert(!destroyed1 && !destroyed2 && !destroyed3);

		list.MakeEmpty(true);
	}

	assert(destroyed1);
	assert(destroyed2);
	assert(destroyed3);

	printf("  PASS: OwningObjectList deletes items on MakeEmpty(true)\n");
}


// ============================================================================
// Test 2: OwningObjectList deletes items on destruction
// ============================================================================

static void
TestOwningListDeletesOnDestroy()
{
	bool destroyed1 = false;
	bool destroyed2 = false;

	{
		OwningObjectList<TestItem> list;
		list.AddItem(new TestItem(10, &destroyed1));
		list.AddItem(new TestItem(20, &destroyed2));

		assert(list.CountItems() == 2);
		assert(!destroyed1 && !destroyed2);
		// list goes out of scope here
	}

	assert(destroyed1);
	assert(destroyed2);

	printf("  PASS: OwningObjectList deletes items on destruction\n");
}


// ============================================================================
// Test 3: OwningObjectList item access works correctly
// ============================================================================

static void
TestOwningListAccess()
{
	OwningObjectList<TestItem> list;
	list.AddItem(new TestItem(100, NULL));
	list.AddItem(new TestItem(200, NULL));
	list.AddItem(new TestItem(300, NULL));

	assert(list.ItemAt(0)->value == 100);
	assert(list.ItemAt(1)->value == 200);
	assert(list.ItemAt(2)->value == 300);
	assert(list.CountItems() == 3);

	printf("  PASS: OwningObjectList item access works correctly\n");
}


// ============================================================================
// Test 4: OwningObjectList RemoveItem deletes the item
// ============================================================================

static void
TestOwningListRemoveDeletes()
{
	bool destroyed = false;

	OwningObjectList<TestItem> list;
	TestItem* item = new TestItem(42, &destroyed);
	list.AddItem(item);

	assert(list.CountItems() == 1);
	assert(!destroyed);

	list.RemoveItem(item, true);

	assert(list.CountItems() == 0);
	assert(destroyed);

	printf("  PASS: OwningObjectList RemoveItem(item, true) deletes item\n");
}


// ============================================================================
// Test 5: Non-owning BObjectList does NOT delete items
// ============================================================================

static void
TestNonOwningListNoDelete()
{
	bool destroyed1 = false;
	bool destroyed2 = false;
	TestItem item1(1, &destroyed1);
	TestItem item2(2, &destroyed2);

	{
		BObjectList<TestItem> list;
		list.AddItem(&item1);
		list.AddItem(&item2);

		assert(list.CountItems() == 2);
		assert(!destroyed1 && !destroyed2);
		// list goes out of scope — should NOT delete items
	}

	assert(!destroyed1);
	assert(!destroyed2);

	printf("  PASS: Non-owning BObjectList<T> does not delete items\n");
}


// ============================================================================
// Test 6: OwningObjectList with BString (real-world type)
// ============================================================================

static void
TestOwningListWithBString()
{
	OwningObjectList<BString> list;
	list.AddItem(new BString("hello"));
	list.AddItem(new BString("meshcore"));
	list.AddItem(new BString("sestriere"));

	assert(list.CountItems() == 3);
	assert(*list.ItemAt(0) == "hello");
	assert(*list.ItemAt(1) == "meshcore");
	assert(*list.ItemAt(2) == "sestriere");

	list.MakeEmpty(true);
	assert(list.CountItems() == 0);

	printf("  PASS: OwningObjectList<BString> works correctly\n");
}


// ============================================================================
// Test 7: OwningObjectList with custom itemsPerBlock
// ============================================================================

static void
TestOwningListCustomBlockSize()
{
	OwningObjectList<TestItem> list(4);

	for (int i = 0; i < 20; i++)
		list.AddItem(new TestItem(i, NULL));

	assert(list.CountItems() == 20);
	assert(list.ItemAt(0)->value == 0);
	assert(list.ItemAt(19)->value == 19);

	printf("  PASS: OwningObjectList with custom block size works\n");
}


// ============================================================================
// Test 8: OwningObjectList empty list operations
// ============================================================================

static void
TestOwningListEmpty()
{
	OwningObjectList<TestItem> list;

	assert(list.CountItems() == 0);
	assert(list.ItemAt(0) == NULL);
	assert(list.IsEmpty());

	list.MakeEmpty(true);  // should not crash
	assert(list.CountItems() == 0);

	printf("  PASS: OwningObjectList handles empty list correctly\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Compat.h / OwningObjectList Tests ===\n\n");

	TestOwningListDeletesOnClear();
	TestOwningListDeletesOnDestroy();
	TestOwningListAccess();
	TestOwningListRemoveDeletes();
	TestNonOwningListNoDelete();
	TestOwningListWithBString();
	TestOwningListCustomBlockSize();
	TestOwningListEmpty();

	printf("\nAll 8 tests passed.\n");
	return 0;
}
