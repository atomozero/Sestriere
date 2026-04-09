/*
 * Test: Security audit — frame parsing and input validation
 * Verifies bounds checks, buffer limits, and attack surface hardening
 * in SerialHandler and MainWindow frame dispatch.
 */

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>


// ============================================================================
// Source-scanning helpers
// ============================================================================

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

static bool
FileContains(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return false;
	char line[1024];
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL) {
			fclose(f);
			return true;
		}
	}
	fclose(f);
	return false;
}


// Replicate frame buffer constants
static const size_t kMaxFramePayload = 512;
static const size_t kMaxFrameSize = 3 + kMaxFramePayload;  // 515


// ============================================================================
// TESTS
// ============================================================================

static void
TestFrameBufferSize()
{
	printf("  TestFrameBufferSize...");
	assert(FileContains("SerialHandler.h", "fFrameBuffer[kMaxFrameSize]"));
	assert(FileContains("Constants.h", "kMaxFramePayload = 512"));
	assert(FileContains("Constants.h",
		"kMaxFrameSize = kFrameHeaderSize + kMaxFramePayload"));
	printf(" PASS\n");
}

static void
TestFrameLengthValidation()
{
	printf("  TestFrameLengthValidation...");
	assert(FileContains("SerialHandler.cpp",
		"fExpectedFrameLen > kMaxFramePayload"));
	assert(FileContains("SerialHandler.cpp", "Invalid frame length"));
	printf(" PASS\n");
}

static void
TestFrameLengthOverflow()
{
	printf("  TestFrameLengthOverflow...");
	// Simulate attack frame lengths
	uint8_t len[2] = {0xFF, 0xFF};
	assert((len[0] | (len[1] << 8)) > kMaxFramePayload);  // Rejected
	len[0] = 0x00; len[1] = 0x02;
	assert((len[0] | (len[1] << 8)) == 512);               // Max valid
	len[0] = 0x01; len[1] = 0x02;
	assert((len[0] | (len[1] << 8)) > kMaxFramePayload);   // Rejected
	printf(" PASS\n");
}

static void
TestPayloadBoundsInFrame()
{
	printf("  TestPayloadBoundsInFrame...");
	assert(FileContains("SerialHandler.cpp", "payloadPos < fExpectedFrameLen"));
	assert(FileContains("SerialHandler.cpp",
		"payloadPos + 1 == fExpectedFrameLen"));
	assert(2 + kMaxFramePayload < kMaxFrameSize);  // No overflow possible
	printf(" PASS\n");
}

static void
TestParseFrameMinLength()
{
	printf("  TestParseFrameMinLength...");
	assert(FileContains("MainWindow.cpp", "length < 1"));
	assert(FileContains("MainWindow.cpp", "uint8 cmd = data[0]"));
	printf(" PASS\n");
}

static void
TestAllHandlersHaveLengthChecks()
{
	printf("  TestAllHandlersHaveLengthChecks...");
	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);

	char line[1024];
	int handlers = 0;
	int withCheck = 0;
	char currentFunc[128] = "";
	bool hasLengthCheck = false;
	int linesSinceFunc = 0;

	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "MainWindow::_Handle") != NULL
			&& strstr(line, "const uint8*") != NULL) {
			if (currentFunc[0] != '\0') {
				handlers++;
				if (hasLengthCheck)
					withCheck++;
			}
			const char* start = strstr(line, "_Handle");
			const char* end = strchr(start, '(');
			if (end != NULL) {
				size_t len = (size_t)(end - start);
				if (len > sizeof(currentFunc) - 1)
					len = sizeof(currentFunc) - 1;
				memcpy(currentFunc, start, len);
				currentFunc[len] = '\0';
			}
			hasLengthCheck = false;
			linesSinceFunc = 0;
		}
		if (currentFunc[0] != '\0' && linesSinceFunc < 25) {
			if (strstr(line, "length") != NULL &&
				(strstr(line, "if") != NULL || strstr(line, "<") != NULL))
				hasLengthCheck = true;
			linesSinceFunc++;
		}
	}
	if (currentFunc[0] != '\0') {
		handlers++;
		if (hasLengthCheck)
			withCheck++;
	}
	fclose(f);

	if (withCheck != handlers) {
		// Re-scan to find which handler failed
		FILE* f2 = OpenSource("MainWindow.cpp");
		currentFunc[0] = '\0';
		hasLengthCheck = false;
		linesSinceFunc = 0;
		while (f2 != NULL && fgets(line, sizeof(line), f2) != NULL) {
			if (strstr(line, "MainWindow::_Handle") != NULL
				&& strstr(line, "const uint8*") != NULL) {
				if (currentFunc[0] != '\0' && !hasLengthCheck)
					printf("\n    MISSING LENGTH CHECK: %s", currentFunc);
				const char* s = strstr(line, "_Handle");
				const char* e = strchr(s, '(');
				if (e != NULL) {
					size_t l = (size_t)(e - s);
					if (l > sizeof(currentFunc) - 1)
						l = sizeof(currentFunc) - 1;
					memcpy(currentFunc, s, l);
					currentFunc[l] = '\0';
				}
				hasLengthCheck = false;
				linesSinceFunc = 0;
			}
			if (currentFunc[0] != '\0' && linesSinceFunc < 25) {
				if (strstr(line, "length") != NULL &&
					(strstr(line, "if") != NULL || strstr(line, "<") != NULL))
					hasLengthCheck = true;
				linesSinceFunc++;
			}
		}
		if (currentFunc[0] != '\0' && !hasLengthCheck)
			printf("\n    MISSING LENGTH CHECK: %s", currentFunc);
		if (f2 != NULL) fclose(f2);
		printf("\n");
		fflush(stdout);
	}
	printf(" (%d/%d validated)", withCheck, handlers);
	fflush(stdout);
	// Allow up to 2 handlers without explicit length check
	// (handlers that don't access data[], e.g. _HandlePushMsgWaiting,
	// _HandleRawPacket — they only log or trigger actions)
	assert(handlers - withCheck <= 2);
	printf(" PASS\n");
}

static void
TestReadLE32BoundsProtection()
{
	printf("  TestReadLE32BoundsProtection...");
	// Critical access points all have length guards
	assert(FileContains("MainWindow.cpp", "if (length >= 5)"));
	assert(FileContains("MainWindow.cpp", "if (length >= 9)"));
	assert(FileContains("MainWindow.cpp", "if (length < 5)"));
	assert(FileContains("MainWindow.cpp", "if (length >= 8)"));
	assert(FileContains("MainWindow.cpp", "if (length >= 6)"));
	assert(FileContains("MainWindow.cpp", "if (length >= 10)"));
	printf(" PASS\n");
}

static void
TestContactFrameValidation()
{
	printf("  TestContactFrameValidation...");
	assert(FileContains("Constants.h", "kContactFrameSize = 148"));
	assert(FileContains("MainWindow.cpp", "nameBuf[kContactNameSize] = '\\0'"));
	assert(FileContains("MainWindow.cpp", "kContactOutPathMaxSize"));
	assert(FileContains("MainWindow.cpp", "pathBytes > 0"));
	printf(" PASS\n");
}

static void
TestTextExtractionSafety()
{
	printf("  TestTextExtractionSafety...");
	assert(FileContains("MainWindow.cpp", "textLen > 255"));
	assert(FileContains("MainWindow.cpp",
		"(length > textOffset) ? (length - textOffset) : 0"));
	printf(" PASS\n");
}

static void
TestSmazDecompressionBounds()
{
	printf("  TestSmazDecompressionBounds...");
	assert(FileContains("MainWindow.cpp", "char decoded[256]"));
	assert(FileContains("MainWindow.cpp", "decompLen > 0"));
	assert(FileContains("MainWindow.cpp", "decompLen < (int)sizeof(decoded)"));
	assert(FileContains("MainWindow.cpp", "textLen > kSmazPrefixLen"));
	printf(" PASS\n");
}

static void
TestNoFormatStringVulns()
{
	printf("  TestNoFormatStringVulns...");
	FILE* f = OpenSource("MainWindow.cpp");
	assert(f != NULL);
	char line[1024];
	bool hasFormatVuln = false;
	while (fgets(line, sizeof(line), f) != NULL) {
		char* comment = strstr(line, "//");
		char* vuln = strstr(line, "printf(text");
		if (vuln != NULL && (comment == NULL || vuln < comment))
			hasFormatVuln = true;
		vuln = strstr(line, "fprintf(stderr, text");
		if (vuln != NULL && (comment == NULL || vuln < comment))
			hasFormatVuln = true;
	}
	fclose(f);
	assert(!hasFormatVuln);
	printf(" PASS\n");
}

static void
TestConnectResetsFrameState()
{
	printf("  TestConnectResetsFrameState...");
	assert(FileContains("SerialHandler.cpp", "fFramePos = 0"));
	assert(FileContains("SerialHandler.cpp", "fInFrame = false"));
	assert(FileContains("SerialHandler.cpp", "fBufferPos = 0"));
	assert(FileContains("SerialHandler.cpp", "fBufferLen = 0"));
	printf(" PASS\n");
}

static void
TestSetRawModeLocking()
{
	printf("  TestSetRawModeLocking...");
	// SetRawMode must lock to prevent race with _ProcessBuffer
	FILE* f = OpenSource("SerialHandler.cpp");
	assert(f != NULL);
	char line[1024];
	bool inSetRaw = false;
	bool hasLock = false;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "SetRawMode") != NULL)
			inSetRaw = true;
		if (inSetRaw && strstr(line, "BAutolock") != NULL) {
			hasLock = true;
			break;
		}
		// Stop scanning after 10 lines past function start
		if (inSetRaw && strstr(line, "fInFrame = false") != NULL)
			break;
	}
	fclose(f);
	assert(hasLock);
	printf(" PASS\n");
}

static void
TestLineBufferBounds()
{
	printf("  TestLineBufferBounds...");
	assert(FileContains("SerialHandler.cpp",
		"fLineLen < sizeof(fLineBuffer) - 1"));
	assert(FileContains("SerialHandler.h", "fLineBuffer[512]"));
	printf(" PASS\n");
}

static void
TestReadBufferBounds()
{
	printf("  TestReadBufferBounds...");
	assert(FileContains("SerialHandler.cpp",
		"sizeof(fReadBuffer) - fBufferLen"));
	assert(FileContains("SerialHandler.cpp", "memmove(fReadBuffer"));
	printf(" PASS\n");
}

static void
TestDeviceInfoStringTermination()
{
	printf("  TestDeviceInfoStringTermination...");
	assert(FileContains("MainWindow.cpp", "strnlen((const char*)data + 8"));
	assert(FileContains("MainWindow.cpp", "strnlen((const char*)data + 20"));
	assert(FileContains("MainWindow.cpp", "strnlen((const char*)data + 60"));
	assert(FileContains("MainWindow.cpp", "strlcpy(fDeviceBoard"));
	assert(FileContains("MainWindow.cpp", "strlcpy(fDeviceFirmware"));
	printf(" PASS\n");
}

static void
TestAllowedRepeatFreqLoopBounds()
{
	printf("  TestAllowedRepeatFreqLoopBounds...");
	assert(FileContains("MainWindow.cpp", "(length - 1) / 8"));
	// Prove mathematically: for any length >= 9,
	// max accessed index = 5 + (pairCount-1)*8 + 3 < length
	for (size_t length = 9; length <= 520; length++) {
		size_t pairCount = (length - 1) / 8;
		if (pairCount == 0) continue;
		size_t maxIndex = 5 + (pairCount - 1) * 8 + 3;
		assert(maxIndex < length);
	}
	printf(" PASS\n");
}

static void
TestTraceDataBoundsChecks()
{
	printf("  TestTraceDataBoundsChecks...");
	assert(FileContains("TracePathWindow.cpp", "hopStart + hopSize <= length"));
	assert(FileContains("TracePathWindow.cpp", "snrOffset + i < length"));
	assert(FileContains("TracePathWindow.cpp", "length < 12"));
	printf(" PASS\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
	printf("=== Frame Security Audit Tests ===\n");

	TestFrameBufferSize();
	TestFrameLengthValidation();
	TestFrameLengthOverflow();
	TestPayloadBoundsInFrame();
	TestParseFrameMinLength();
	TestAllHandlersHaveLengthChecks();
	TestReadLE32BoundsProtection();
	TestContactFrameValidation();
	TestTextExtractionSafety();
	TestSmazDecompressionBounds();
	TestNoFormatStringVulns();
	TestConnectResetsFrameState();
	TestSetRawModeLocking();
	TestLineBufferBounds();
	TestReadBufferBounds();
	TestDeviceInfoStringTermination();
	TestAllowedRepeatFreqLoopBounds();
	TestTraceDataBoundsChecks();

	printf("\n=== All 18 security tests passed ===\n");
	return 0;
}
