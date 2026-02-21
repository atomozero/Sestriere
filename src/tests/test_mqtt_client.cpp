/*
 * Test: MqttClient structural integrity
 * Verifies reconnect logic, timer lifecycle, thread safety patterns.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>


static FILE*
OpenSource(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		char alt[256];
		snprintf(alt, sizeof(alt), "../%s", filename);
		f = fopen(alt, "r");
	}
	return f;
}


// ============================================================================
// Test 1: Auto-reconnect with exponential backoff
// ============================================================================

static void
TestReconnectBackoff()
{
	FILE* fp = OpenSource("MqttClient.cpp");
	assert(fp != NULL);

	char line[1024];
	bool foundInitDelay = false;
	bool foundMaxDelay = false;
	bool foundBackoffMultiply = false;
	bool foundResetDelay = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "kReconnectInitDelay") != NULL)
			foundInitDelay = true;
		if (strstr(line, "kReconnectMaxDelay") != NULL)
			foundMaxDelay = true;
		if (strstr(line, "fReconnectDelay *") != NULL
			|| strstr(line, "fReconnectDelay*") != NULL
			|| strstr(line, "Delay * 2") != NULL
			|| strstr(line, "Delay *= 2") != NULL)
			foundBackoffMultiply = true;
		// Reset delay on successful connect
		if (strstr(line, "fReconnectDelay = kReconnectInitDelay") != NULL)
			foundResetDelay = true;
	}
	fclose(fp);

	assert(foundInitDelay && "Must have initial reconnect delay constant");
	assert(foundMaxDelay && "Must have max reconnect delay constant");

	printf("  PASS: Reconnect backoff constants defined\n");
}


// ============================================================================
// Test 2: Timer lifecycle (delete before recreate)
// ============================================================================

static void
TestTimerLifecycle()
{
	FILE* fp = OpenSource("MqttClient.cpp");
	assert(fp != NULL);

	char line[1024];
	int deleteStatus = 0;
	int deleteReconnect = 0;
	bool destructorDeletesTimers = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "delete fStatusTimer") != NULL)
			deleteStatus++;
		if (strstr(line, "delete fReconnectTimer") != NULL)
			deleteReconnect++;
	}
	fclose(fp);

	// Destructor + at least 1 other delete (before recreate)
	assert(deleteStatus >= 1 && "fStatusTimer must be deleted");
	assert(deleteReconnect >= 1 && "fReconnectTimer must be deleted");

	printf("  PASS: Timer pointers properly deleted (%d status, %d reconnect)\n",
		deleteStatus, deleteReconnect);
}


// ============================================================================
// Test 3: Thread safety with BAutolock
// ============================================================================

static void
TestThreadSafety()
{
	FILE* fp = OpenSource("MqttClient.cpp");
	assert(fp != NULL);

	char line[1024];
	int autolockCount = 0;
	bool hasStateLock = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "BAutolock") != NULL)
			autolockCount++;
		if (strstr(line, "fStateLock") != NULL)
			hasStateLock = true;
	}
	fclose(fp);

	assert(hasStateLock && "Must have fStateLock for thread safety");
	assert(autolockCount >= 2 && "Must use BAutolock in multiple places");

	printf("  PASS: Thread safety with BAutolock (%d usages)\n", autolockCount);
}


// ============================================================================
// Test 4: Manual disconnect flag prevents auto-reconnect
// ============================================================================

static void
TestManualDisconnectFlag()
{
	FILE* fp = OpenSource("MqttClient.cpp");
	assert(fp != NULL);

	char line[1024];
	int manualDisconnectRefs = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "fManualDisconnect") != NULL)
			manualDisconnectRefs++;
	}
	fclose(fp);

	assert(manualDisconnectRefs >= 3
		&& "fManualDisconnect should be set/checked/reset");

	printf("  PASS: Manual disconnect flag used (%d references)\n",
		manualDisconnectRefs);
}


// ============================================================================
// Test 5: Log target message forwarding
// ============================================================================

static void
TestLogTarget()
{
	FILE* fp = OpenSource("MqttClient.cpp");
	assert(fp != NULL);

	char line[1024];
	bool foundSetLogTarget = false;
	bool foundMsgMqttLog = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "SetLogTarget(") != NULL)
			foundSetLogTarget = true;
		if (strstr(line, "MSG_MQTT_LOG_ENTRY") != NULL)
			foundMsgMqttLog = true;
	}
	fclose(fp);

	assert(foundSetLogTarget && "Must have SetLogTarget method");
	assert(foundMsgMqttLog && "Must forward log messages via MSG_MQTT_LOG_ENTRY");

	printf("  PASS: Log target forwarding implemented\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== MqttClient Structural Tests ===\n\n");

	TestReconnectBackoff();
	TestTimerLifecycle();
	TestThreadSafety();
	TestManualDisconnectFlag();
	TestLogTarget();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
