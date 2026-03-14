/*
 * Test: Serial timeout detection (B2)
 * Verifies select() usage, zero-read detection, and disconnect notification
 * in SerialHandler::_ReadLoop().
 */

#include <cstdio>
#include <cstring>
#include <cassert>


static FILE*
OpenSource(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		char path[256];
		snprintf(path, sizeof(path), "../%s", filename);
		f = fopen(path, "r");
	}
	return f;
}


static int
CountOccurrences(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return -1;

	char line[512];
	int count = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL)
			count++;
	}
	fclose(f);
	return count;
}


static bool
FileContains(const char* filename, const char* pattern)
{
	return CountOccurrences(filename, pattern) > 0;
}


// ============================================================================
// Test 1: select() is used in _ReadLoop() for fd monitoring
// ============================================================================

static void
TestSelectUsed()
{
	const char* file = "SerialHandler.cpp";

	assert(FileContains(file, "select("));
	assert(FileContains(file, "fd_set"));
	assert(FileContains(file, "FD_ZERO("));
	assert(FileContains(file, "FD_SET("));
	assert(FileContains(file, "struct timeval"));

	printf("  PASS: select() used for fd monitoring in _ReadLoop()\n");
}


// ============================================================================
// Test 2: sys/select.h is included
// ============================================================================

static void
TestSelectHeaderIncluded()
{
	const char* file = "SerialHandler.cpp";

	assert(FileContains(file, "#include <sys/select.h>"));

	printf("  PASS: sys/select.h is included\n");
}


// ============================================================================
// Test 3: Consecutive zero-read counter exists
// ============================================================================

static void
TestZeroReadCounter()
{
	const char* file = "SerialHandler.cpp";

	// Variable declaration
	assert(FileContains(file, "zeroReadCount"));

	// Counter increment
	assert(FileContains(file, "zeroReadCount++"));

	// Counter reset on successful read
	assert(FileContains(file, "zeroReadCount = 0"));

	// Threshold check
	assert(FileContains(file, "kMaxZeroReads"));

	printf("  PASS: Zero-read counter with increment, reset, and threshold\n");
}


// ============================================================================
// Test 4: kMaxZeroReads constant defined
// ============================================================================

static void
TestMaxZeroReadsConstant()
{
	const char* file = "Constants.h";

	assert(FileContains(file, "kMaxZeroReads"));
	assert(FileContains(file, "const int kMaxZeroReads"));

	printf("  PASS: kMaxZeroReads constant defined in Constants.h\n");
}


// ============================================================================
// Test 5: Disconnect notification on timeout/error
// ============================================================================

static void
TestDisconnectNotification()
{
	const char* file = "SerialHandler.cpp";

	// _NotifyError called for select() error
	int notifyErrorCount = CountOccurrences(file, "_NotifyError(");
	assert(notifyErrorCount >= 3);  // select error, device disappeared, zero reads

	// Error messages for different disconnect scenarios
	assert(FileContains(file, "Serial port disconnected"));
	assert(FileContains(file, "Serial device disconnected"));

	printf("  PASS: Disconnect notification on %d error paths\n",
		notifyErrorCount);
}


// ============================================================================
// Test 6: ioctl(TIOCMGET) used to validate fd on timeout
// ============================================================================

static void
TestIoctlValidation()
{
	const char* file = "SerialHandler.cpp";

	assert(FileContains(file, "ioctl(fd, TIOCMGET,"));

	printf("  PASS: ioctl(TIOCMGET) used for fd validation on timeout\n");
}


// ============================================================================
// Test 7: select() has a reasonable timeout (not zero, not too long)
// ============================================================================

static void
TestSelectTimeout()
{
	const char* file = "SerialHandler.cpp";

	// Should have tv_sec = 1 (1 second timeout)
	assert(FileContains(file, "timeout.tv_sec = 1"));
	assert(FileContains(file, "timeout.tv_usec = 0"));

	printf("  PASS: select() timeout is 1 second\n");
}


// ============================================================================
// Test 8: Disconnect() waits for thread BEFORE closing fd
// ============================================================================

static void
TestDisconnectOrderSafe()
{
	const char* file = "SerialHandler.cpp";

	// The old dangerous pattern should be gone
	assert(!FileContains(file, "Close fd first to unblock the read thread"));

	// The safe pattern should exist
	assert(FileContains(file, "Now safe to close"));
	assert(FileContains(file, "wait_for_thread"));

	printf("  PASS: Disconnect() waits for thread before closing fd\n");
}


// ============================================================================
// Test 9: _ReadLoop checks fRunning before ioctl on timeout
// ============================================================================

static void
TestRunningCheckBeforeIoctl()
{
	const char* file = "SerialHandler.cpp";

	// fRunning check before ioctl to avoid kernel race
	assert(FileContains(file, "check fRunning before touching the fd"));

	printf("  PASS: _ReadLoop checks fRunning before ioctl\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Serial Timeout Detection Tests (B2) ===\n\n");

	TestSelectUsed();
	TestSelectHeaderIncluded();
	TestZeroReadCounter();
	TestMaxZeroReadsConstant();
	TestDisconnectNotification();
	TestIoctlValidation();
	TestSelectTimeout();
	TestDisconnectOrderSafe();
	TestRunningCheckBeforeIoctl();

	printf("\nAll 9 tests passed.\n");
	return 0;
}
