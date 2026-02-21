/*
 * Test: DatabaseManager singleton thread safety
 * Verifies that concurrent Instance() calls return the same pointer
 * and that the double-checked locking pattern works correctly.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <pthread.h>

// We test the singleton pattern in isolation without linking the full app.
// Simulate the thread-safe singleton pattern used in DatabaseManager.

#include <Locker.h>
#include <Autolock.h>

class MockSingleton {
public:
	static MockSingleton* Instance()
	{
		if (sInstance == NULL) {
			BAutolock lock(sLock);
			if (sInstance == NULL)
				sInstance = new MockSingleton();
		}
		return sInstance;
	}

	static void Destroy()
	{
		BAutolock lock(sLock);
		delete sInstance;
		sInstance = NULL;
	}

	int GetId() const { return fId; }

private:
	MockSingleton() : fId(42) {}
	~MockSingleton() {}

	static MockSingleton*	sInstance;
	static BLocker			sLock;
	int						fId;
};

MockSingleton* MockSingleton::sInstance = NULL;
BLocker MockSingleton::sLock("MockSingleton");

// Thread results
static const int kNumThreads = 8;
static MockSingleton* sResults[kNumThreads];

static void* ThreadFunc(void* arg)
{
	int idx = (int)(intptr_t)arg;
	// Each thread calls Instance() and stores the result
	sResults[idx] = MockSingleton::Instance();
	return NULL;
}


static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  TEST: %s ... ", name);

#define PASS() \
	do { printf("PASS\n"); sTestsPassed++; } while(0)

#define FAIL(msg) \
	do { printf("FAIL: %s\n", msg); sTestsFailed++; } while(0)


int main()
{
	printf("=== DatabaseManager Thread Safety Tests ===\n\n");

	// --- Test 1: Singleton returns non-null ---
	TEST("Instance() returns non-null");
	{
		MockSingleton* inst = MockSingleton::Instance();
		if (inst != NULL)
			PASS();
		else
			FAIL("returned NULL");
	}

	// --- Test 2: Multiple calls return same pointer ---
	TEST("Multiple Instance() calls return same pointer");
	{
		MockSingleton* a = MockSingleton::Instance();
		MockSingleton* b = MockSingleton::Instance();
		MockSingleton* c = MockSingleton::Instance();
		if (a == b && b == c)
			PASS();
		else
			FAIL("different pointers returned");
	}

	// --- Test 3: Concurrent access from multiple threads ---
	TEST("Concurrent Instance() from 8 threads returns same pointer");
	{
		// Reset singleton
		MockSingleton::Destroy();
		memset(sResults, 0, sizeof(sResults));

		pthread_t threads[kNumThreads];
		for (int i = 0; i < kNumThreads; i++)
			pthread_create(&threads[i], NULL, ThreadFunc, (void*)(intptr_t)i);

		for (int i = 0; i < kNumThreads; i++)
			pthread_join(threads[i], NULL);

		// All threads should have gotten the same pointer
		bool allSame = true;
		for (int i = 1; i < kNumThreads; i++) {
			if (sResults[i] != sResults[0]) {
				allSame = false;
				break;
			}
		}

		if (allSame && sResults[0] != NULL)
			PASS();
		else
			FAIL("threads got different singleton instances");
	}

	// --- Test 4: Singleton data is consistent ---
	TEST("Singleton data is consistent after concurrent access");
	{
		MockSingleton* inst = MockSingleton::Instance();
		if (inst->GetId() == 42)
			PASS();
		else
			FAIL("data corrupted");
	}

	// --- Test 5: Destroy and recreate works ---
	TEST("Destroy() then Instance() creates new valid instance");
	{
		MockSingleton* old = MockSingleton::Instance();
		MockSingleton::Destroy();
		MockSingleton* fresh = MockSingleton::Instance();
		if (fresh != NULL && fresh->GetId() == 42)
			PASS();
		else
			FAIL("re-creation failed");
	}

	// Cleanup
	MockSingleton::Destroy();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);

	return sTestsFailed > 0 ? 1 : 0;
}
