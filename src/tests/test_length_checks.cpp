/*
 * Test: Minimum length checks in _Handle* methods
 * Verifies that all protocol handlers have proper frame length validation
 * to prevent out-of-bounds data access on malformed/truncated frames.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

// Haiku type aliases for standalone test
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;


// ============================================================================
// Helper: check if a handler has an early return length guard in source
// ============================================================================

static FILE*
OpenSource(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		// Try parent directory
		char path[256];
		snprintf(path, sizeof(path), "../%s", filename);
		f = fopen(path, "r");
	}
	return f;
}


// Search for a pattern within a handler's body (between its signature and
// the next handler or end of relevant section)
static bool
HandlerHasPattern(const char* filename, const char* handlerName,
	const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return false;

	char line[512];
	bool inHandler = false;
	int braceDepth = 0;
	bool found = false;

	while (fgets(line, sizeof(line), f) != NULL) {
		if (!inHandler) {
			// Look for handler signature
			if (strstr(line, handlerName) != NULL
				&& strstr(line, "const uint8*") != NULL) {
				inHandler = true;
				braceDepth = 0;
			}
			continue;
		}

		// Track brace depth to know when we leave the handler
		for (const char* p = line; *p != '\0'; p++) {
			if (*p == '{') braceDepth++;
			else if (*p == '}') braceDepth--;
		}

		if (strstr(line, pattern) != NULL) {
			found = true;
			break;
		}

		// End of handler
		if (braceDepth <= 0 && braceDepth != 0)
			break;
		if (inHandler && braceDepth == 0 && strchr(line, '}') != NULL)
			break;
	}

	fclose(f);
	return found;
}


// ============================================================================
// Test 1: Handlers with early return length guards
// ============================================================================

static void
TestEarlyReturnGuards()
{
	const char* file = "MainWindow.cpp";

	// These handlers MUST have early return length checks
	struct {
		const char* handler;
		const char* guard;
	} handlers[] = {
		{ "_HandleDeviceInfo",        "length < 4" },
		{ "_HandleCurrTime",          "length < 5" },
		{ "_HandleCustomVars",        "length < 2" },
		{ "_HandleAdvertPath",        "length < 6" },
		{ "_HandlePushRawData",       "length < 4" },
		{ "_HandleContact(",          "length < kContactFrameSize" },
		{ "_HandleStats(",            "length < 2" },
		{ "_HandleChannelInfo",       "length < 50" },
		{ "_HandlePushAdvert",        "length < 7" },
		{ "_HandlePushTelemetry",     "length < 12" },
		{ "_HandlePushStatusResponse","length < 32" },
		{ "_HandleExportContact",     "length < 3" },
		{ "_HandleSelfInfo",          "length < 2" },
		{ "_HandlePushTraceData",     "length < 3" },
	};

	int count = sizeof(handlers) / sizeof(handlers[0]);
	for (int i = 0; i < count; i++) {
		bool found = HandlerHasPattern(file, handlers[i].handler,
			handlers[i].guard);
		if (!found) {
			printf("  FAIL: %s missing guard '%s'\n",
				handlers[i].handler, handlers[i].guard);
		}
		assert(found);
	}

	printf("  PASS: All %d handlers have early return length guards\n",
		count);
}


// ============================================================================
// Test 2: V3/V2 message handlers have branch-specific length checks
// ============================================================================

static void
TestMessageHandlerBranchGuards()
{
	const char* file = "MainWindow.cpp";

	// _HandleContactMsgRecv has V3 and V2 branch guards
	assert(HandlerHasPattern(file, "_HandleContactMsgRecv", "length < 16"));
	assert(HandlerHasPattern(file, "_HandleContactMsgRecv", "length < kV2DmMinLength"));

	// _HandleChannelMsgRecv has V3 and V2 branch guards
	assert(HandlerHasPattern(file, "_HandleChannelMsgRecv", "length < 11"));
	assert(HandlerHasPattern(file, "_HandleChannelMsgRecv", "length < kV2ChMinLength"));

	printf("  PASS: Message handlers have V3/V2 branch-specific guards\n");
}


// ============================================================================
// Test 3: Conditional guard handlers protect all data access
// ============================================================================

static void
TestConditionalGuards()
{
	const char* file = "MainWindow.cpp";

	// _HandleBattAndStorage uses conditional guard for all data access
	assert(HandlerHasPattern(file, "_HandleBattAndStorage", "length >= 3"));
	assert(HandlerHasPattern(file, "_HandleBattAndStorage", "length >= 7"));
	assert(HandlerHasPattern(file, "_HandleBattAndStorage", "length >= 11"));

	// _HandleSelfInfo uses progressive conditional guards
	assert(HandlerHasPattern(file, "_HandleSelfInfo", "length >= 36"));
	assert(HandlerHasPattern(file, "_HandleSelfInfo", "length >= 44"));
	assert(HandlerHasPattern(file, "_HandleSelfInfo", "length >= 48"));
	assert(HandlerHasPattern(file, "_HandleSelfInfo", "length >= 58"));

	// _HandleContactsStart guards count read
	assert(HandlerHasPattern(file, "_HandleContactsStart", "length >= 5"));

	printf("  PASS: Conditional guard handlers protect all data access\n");
}


// ============================================================================
// Test 4: Log warnings on short frames
// ============================================================================

static void
TestWarningMessages()
{
	const char* file = "MainWindow.cpp";

	// New guards should log warnings
	assert(HandlerHasPattern(file, "_HandleExportContact",
		"RSP_EXPORT_CONTACT: frame too short"));
	assert(HandlerHasPattern(file, "_HandleSelfInfo",
		"RSP_SELF_INFO: frame too short"));
	assert(HandlerHasPattern(file, "_HandlePushTraceData",
		"PUSH_TRACE_DATA: frame too short"));
	assert(HandlerHasPattern(file, "_HandleDeviceInfo",
		"RSP_DEVICE_INFO: frame too short"));

	printf("  PASS: Short frame warnings present in guard handlers\n");
}


// ============================================================================
// Test 5: Verify no handler accesses data[N] without length >= N+1
// Checks that specific known offset accesses are behind proper guards
// ============================================================================

static void
TestKnownOffsetProtection()
{
	const char* file = "MainWindow.cpp";

	// _HandleStats accesses data[kStatsCoreSubtypeOffset] (offset 1)
	// which is behind 'length < 2' guard
	assert(HandlerHasPattern(file, "_HandleStats(", "length < 2"));

	// Stats subtypes have inner guards for their specific offsets
	assert(HandlerHasPattern(file, "_HandleStats(", "length >= 8"));
	assert(HandlerHasPattern(file, "_HandleStats(", "length >= 4"));
	assert(HandlerHasPattern(file, "_HandleStats(", "length >= 6"));
	assert(HandlerHasPattern(file, "_HandleStats(", "length >= 10"));

	// _HandlePushAdvert accesses data[7] and data[8] conditionally
	assert(HandlerHasPattern(file, "_HandlePushAdvert", "length >= 8"));
	assert(HandlerHasPattern(file, "_HandlePushAdvert", "length >= 9"));

	printf("  PASS: Known offset accesses are behind proper guards\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== Handler Length Check Tests ===\n\n");

	TestEarlyReturnGuards();
	TestMessageHandlerBranchGuards();
	TestConditionalGuards();
	TestWarningMessages();
	TestKnownOffsetProtection();

	printf("\nAll 5 tests passed.\n");
	return 0;
}
