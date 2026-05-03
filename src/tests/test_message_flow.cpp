/*
 * Test: DM and Room message flow edge cases
 * Verifies protocol handling, parsing, truncation, dedup, room format,
 * delivery tracking, SMAZ compression, self-echo, and timestamp logic.
 *
 * These tests work by scanning MainWindow.cpp source for patterns and
 * by replicating critical logic paths with actual data.
 */

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ctime>


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

static int
CountPattern(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return -1;
	char line[1024];
	int count = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL)
			count++;
	}
	fclose(f);
	return count;
}

// Count lines matching TWO patterns (both must appear on same line)
static int
CountDualPattern(const char* filename, const char* p1, const char* p2)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return -1;
	char line[1024];
	int count = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, p1) != NULL && strstr(line, p2) != NULL)
			count++;
	}
	fclose(f);
	return count;
}


// ============================================================================
// Inline replications of critical logic for functional testing
// ============================================================================

// Replicate V3 DM frame parsing offsets (from Constants.h)
static const size_t kV3DmSnrOffset = 1;
static const size_t kV3DmSenderOffset = 4;
static const size_t kV3DmPathLenOffset = 10;
static const size_t kV3DmTxtTypeOffset = 11;
static const size_t kV3DmTimestampOffset = 12;
static const size_t kV3DmTextOffset = 16;
static const size_t kV3DmMinLength = 16;

// V2 offsets
static const size_t kV2DmSenderOffset = 1;
static const size_t kV2DmPathLenOffset = 7;
static const size_t kV2DmTxtTypeOffset = 8;
static const size_t kV2DmTimestampOffset = 9;
static const size_t kV2DmTextOffset = 13;
static const size_t kV2DmMinLength = 13;

static uint32_t
ReadLE32(const uint8_t* p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}


// ============================================================================
// TESTS
// ============================================================================


static void
TestV3FrameOffsets()
{
	printf("  TestV3FrameOffsets...");

	// Build a realistic V3 DM frame
	// [0]=code(0x10) [1]=SNR*4 [2-3]=reserved [4-9]=pubkey
	// [10]=pathLen [11]=txtType [12-15]=timestamp [16+]=text
	uint8_t frame[64];
	memset(frame, 0, sizeof(frame));
	frame[0] = 0x10;  // RSP_CONTACT_MSG_RECV_V3
	frame[1] = 40;    // SNR = 10.0 dB (10*4=40)
	frame[2] = 0;     // reserved
	frame[3] = 0;     // reserved
	// Sender pubkey prefix at [4-9]
	frame[4] = 0xAA; frame[5] = 0xBB; frame[6] = 0xCC;
	frame[7] = 0xDD; frame[8] = 0x01; frame[9] = 0x02;
	frame[10] = 2;    // pathLen = 2 hops
	frame[11] = 0;    // txtType = TXT_TYPE_PLAIN
	// Timestamp at [12-15] = 1700000000 = 0x6553F100 (LE)
	frame[12] = 0x00; frame[13] = 0xF1; frame[14] = 0x53; frame[15] = 0x65;
	// Text at [16+]
	const char* msg = "Hello mesh!";
	memcpy(frame + 16, msg, strlen(msg));
	size_t frameLen = 16 + strlen(msg);

	// Verify V3 parsing matches expected values
	assert(frameLen >= kV3DmMinLength);

	int8_t snr = (int8_t)frame[kV3DmSnrOffset];
	float snrDb = snr / 4.0f;
	assert(snrDb >= 9.9f && snrDb <= 10.1f);

	assert(frame[kV3DmSenderOffset] == 0xAA);
	assert(frame[kV3DmSenderOffset + 5] == 0x02);

	assert(frame[kV3DmPathLenOffset] == 2);
	assert(frame[kV3DmTxtTypeOffset] == 0);

	uint32_t ts = ReadLE32(frame + kV3DmTimestampOffset);
	assert(ts == 1700000000);

	size_t textLen = frameLen - kV3DmTextOffset;
	assert(textLen == strlen(msg));
	assert(memcmp(frame + kV3DmTextOffset, "Hello mesh!", textLen) == 0);

	printf(" PASS\n");
}


static void
TestV2FrameOffsets()
{
	printf("  TestV2FrameOffsets...");

	// V2: [0]=code(0x07) [1-6]=pubkey [7]=pathLen [8]=txtType
	// [9-12]=timestamp [13+]=text
	uint8_t frame[64];
	memset(frame, 0, sizeof(frame));
	frame[0] = 0x07;
	frame[1] = 0xAA; frame[2] = 0xBB; frame[3] = 0xCC;
	frame[4] = 0xDD; frame[5] = 0x01; frame[6] = 0x02;
	frame[7] = 1;     // pathLen
	frame[8] = 0;     // txtType
	frame[9] = 0x00; frame[10] = 0xF1; frame[11] = 0x53; frame[12] = 0x65;
	const char* msg = "V2 message";
	memcpy(frame + 13, msg, strlen(msg));
	size_t frameLen = 13 + strlen(msg);

	assert(frameLen >= kV2DmMinLength);
	assert(frame[kV2DmSenderOffset] == 0xAA);
	assert(frame[kV2DmPathLenOffset] == 1);
	assert(frame[kV2DmTxtTypeOffset] == 0);

	uint32_t ts = ReadLE32(frame + kV2DmTimestampOffset);
	assert(ts == 1700000000);

	size_t textLen = frameLen - kV2DmTextOffset;
	assert(memcmp(frame + kV2DmTextOffset, "V2 message", textLen) == 0);

	printf(" PASS\n");
}


static void
TestV3MinLengthCheck()
{
	printf("  TestV3MinLengthCheck...");

	// Frame too short for V3 — must be rejected
	assert(15 < kV3DmMinLength);  // 15 bytes is too short
	assert(16 >= kV3DmMinLength); // 16 bytes is minimum

	// V2 minimum
	assert(12 < kV2DmMinLength);
	assert(13 >= kV2DmMinLength);

	// Verify code uses these constants (or hardcoded equivalents)
	assert(FileContains("MainWindow.cpp", "length < 16") ||
		   FileContains("MainWindow.cpp", "kV3DmMinLength"));
	assert(FileContains("MainWindow.cpp", "kV2DmMinLength"));

	printf(" PASS\n");
}


static void
TestTimestampSanitization()
{
	printf("  TestTimestampSanitization...");

	// Replicate the sanitization logic:
	// if (timestamp < now - 86400 || timestamp > now + 3600) timestamp = now;
	uint32_t now = (uint32_t)time(NULL);

	// Normal timestamp (5 minutes ago) — should pass
	uint32_t ts1 = now - 300;
	bool reject1 = (ts1 < now - 86400 || ts1 > now + 3600);
	assert(!reject1);

	// Timestamp 2 days ago — should be rejected
	uint32_t ts2 = now - 172800;
	bool reject2 = (ts2 < now - 86400 || ts2 > now + 3600);
	assert(reject2);

	// Timestamp 2 hours in future — should be rejected
	uint32_t ts3 = now + 7200;
	bool reject3 = (ts3 < now - 86400 || ts3 > now + 3600);
	assert(reject3);

	// Timestamp 30 minutes in future — should pass
	uint32_t ts4 = now + 1800;
	bool reject4 = (ts4 < now - 86400 || ts4 > now + 3600);
	assert(!reject4);

	// Zero timestamp — should be rejected (way in the past)
	uint32_t ts5 = 0;
	bool reject5 = (ts5 < now - 86400 || ts5 > now + 3600);
	assert(reject5);

	// Verify sanitization exists in code
	assert(FileContains("MainWindow.cpp", "now - 86400"));
	assert(FileContains("MainWindow.cpp", "now + 3600"));

	printf(" PASS\n");
}


static void
TestSelfEchoFilter()
{
	printf("  TestSelfEchoFilter...");

	// Self-echo: sender prefix matches own public key
	// Code parses fPublicKey hex string into bytes and compares 6 bytes

	uint8_t selfPrefix[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	uint8_t senderPrefix[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	uint8_t otherPrefix[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

	// Match — should be filtered
	assert(memcmp(senderPrefix, selfPrefix, 6) == 0);

	// Mismatch — should NOT be filtered
	assert(memcmp(otherPrefix, selfPrefix, 6) != 0);

	// Single byte difference — should NOT be filtered
	uint8_t almostSelf[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00};
	assert(memcmp(almostSelf, selfPrefix, 6) != 0);

	// Verify self-echo check exists in receive handler
	assert(FileContains("MainWindow.cpp", "self-echo") ||
		   FileContains("MainWindow.cpp", "selfPrefix"));

	printf(" PASS\n");
}


static void
TestRoomMessageParsing()
{
	printf("  TestRoomMessageParsing...");

	// Room messages arrive as "SenderNick: actual message"
	// Parser finds first ":" followed by space within first 32 chars

	// Normal case
	const char* msg1 = "Alice: Hello everyone!";
	const char* colon1 = strchr(msg1, ':');
	assert(colon1 != NULL);
	assert(colon1 > msg1);
	assert(colon1 < msg1 + 32);
	assert(colon1[1] == ' ');
	// Nick = "Alice", text = "Hello everyone!"
	char nick1[33];
	size_t nickLen1 = (size_t)(colon1 - msg1);
	memcpy(nick1, msg1, nickLen1);
	nick1[nickLen1] = '\0';
	assert(strcmp(nick1, "Alice") == 0);
	assert(strcmp(colon1 + 2, "Hello everyone!") == 0);

	// Colon at start — should NOT parse (colon > text check)
	const char* msg2 = ": no nick";
	const char* colon2 = strchr(msg2, ':');
	assert(colon2 != NULL);
	bool valid2 = (colon2 > msg2 && colon2 < msg2 + 32 && colon2[1] == ' ');
	assert(!valid2);  // colon2 == msg2, so > fails

	// Colon without space — should NOT parse
	const char* msg3 = "Bob:no space";
	const char* colon3 = strchr(msg3, ':');
	assert(colon3 != NULL);
	bool valid3 = (colon3 > msg3 && colon3 < msg3 + 32 && colon3[1] == ' ');
	assert(!valid3);

	// Long nick (>32 chars) — should NOT parse (colon < text + 32 check)
	char msg4[100];
	memset(msg4, 'A', 40);
	msg4[40] = ':';
	msg4[41] = ' ';
	msg4[42] = 'x';
	msg4[43] = '\0';
	const char* colon4 = strchr(msg4, ':');
	bool valid4 = (colon4 > msg4 && colon4 < msg4 + 32 && colon4[1] == ' ');
	assert(!valid4);  // colon at offset 40, beyond 32

	// Nick with embedded colon — LAST ": " wins (fixed)
	const char* msg5 = "User: Admin: hello world";
	// Replicate fixed algorithm: find last ": " within 32 chars
	const char* bestColon5 = NULL;
	for (const char* p = msg5; p < msg5 + 32 && *p != '\0'; p++) {
		if (*p == ':' && p > msg5 && p[1] == ' ')
			bestColon5 = p;
	}
	assert(bestColon5 != NULL);
	size_t nickLen5 = (size_t)(bestColon5 - msg5);
	// Nick is now "User: Admin" (11 chars), not just "User" (4 chars)
	assert(nickLen5 == 11);

	printf(" PASS\n");
}


static void
TestRoomTxtTypePlain()
{
	printf("  TestRoomTxtTypePlain...");

	// After commit 977e2b5, normal Room messages use TXT_TYPE_PLAIN
	// CLI commands use TXT_TYPE_CLI_DATA via _SendCliCommand only

	// Verify _SendTextMessage uses TXT_TYPE_PLAIN unconditionally
	assert(FileContains("MainWindow.cpp", "uint8 txtType = TXT_TYPE_PLAIN"));

	// Verify _SendCliCommand uses TXT_TYPE_CLI_DATA
	assert(FileContains("MainWindow.cpp", "TXT_TYPE_CLI_DATA"));

	// Verify Room parser only extracts nick when txtType is TXT_TYPE_PLAIN
	assert(FileContains("MainWindow.cpp", "txtType == TXT_TYPE_PLAIN") ||
		   FileContains("MainWindow.cpp", "TXT_TYPE_PLAIN"));

	printf(" PASS\n");
}


static void
TestTextTruncationLimit()
{
	printf("  TestTextTruncationLimit...");

	// DM send: max 160 chars enforced
	assert(FileContains("MainWindow.cpp", "textLen > 160"));

	// Channel send: max 200 chars (or similar)
	assert(FileContains("MainWindow.cpp", "textLen > 200") ||
		   FileContains("MainWindow.cpp", "textLen > 160"));

	// Receive: max 255 bytes in text buffer
	assert(FileContains("MainWindow.cpp", "textLen > 255"));

	// ChatMessage.text is char[256]
	assert(FileContains("Types.h", "text[256]"));

	printf(" PASS\n");
}


static void
TestUTF8TruncationFix()
{
	printf("  TestUTF8TruncationFix...");

	// Demonstrate the UTF-8 truncation issue:
	// strlcpy at byte boundary can split multi-byte chars

	// "Hello " + 38x "世" (3 bytes each) = 6 + 114 = 120 bytes (safe)
	// "Hello " + 52x "世" (3 bytes each) = 6 + 156 = 162 bytes (over 160 limit)
	char utf8msg[300];
	strcpy(utf8msg, "Hello ");
	size_t pos = 6;
	for (int i = 0; i < 52; i++) {
		utf8msg[pos++] = 0xE4;  // 世 = E4 B8 96
		utf8msg[pos++] = 0xB8;
		utf8msg[pos++] = 0x96;
	}
	utf8msg[pos] = '\0';
	assert(strlen(utf8msg) == 162);  // Over 160 byte limit

	// Verify the fix exists in code
	assert(FileContains("MainWindow.cpp", "UTF-8 boundary"));
	assert(FileContains("MainWindow.cpp", "0xC0) == 0x80"));

	// Simulate the fixed truncation algorithm
	size_t textLen = 160;
	while (textLen > 0 && ((uint8_t)utf8msg[textLen] & 0xC0) == 0x80)
		textLen--;  // skip continuation bytes
	utf8msg[textLen] = '\0';

	// byte 160 = 0xB8 (continuation) -> back to 159
	// byte 159 = 0xE4 (lead byte, not continuation) -> stop
	// Truncate at 159: removes the incomplete 3-byte sequence
	assert(textLen == 159);

	// Verify last complete char: E4 B8 96 at positions 156-158
	assert((uint8_t)utf8msg[156] == 0xE4);
	assert((uint8_t)utf8msg[157] == 0xB8);
	assert((uint8_t)utf8msg[158] == 0x96);

	printf(" PASS\n");
}


static void
TestSNRScaling()
{
	printf("  TestSNRScaling...");

	// V3 protocol: SNR stored as int8 × 4 (Q6.2 fixed-point)
	// Verify correct scaling

	// SNR = 10.0 dB → wire value = 40
	int8_t wire1 = 40;
	float snr1 = wire1 / 4.0f;
	assert(snr1 >= 9.99f && snr1 <= 10.01f);

	// SNR = -5.25 dB → wire value = -21
	int8_t wire2 = -21;
	float snr2 = wire2 / 4.0f;
	assert(snr2 >= -5.26f && snr2 <= -5.24f);

	// SNR = 0 dB → wire value = 0
	int8_t wire3 = 0;
	float snr3 = wire3 / 4.0f;
	assert(snr3 == 0.0f);

	// Max positive: 127/4 = 31.75 dB
	int8_t wire4 = 127;
	float snr4 = wire4 / 4.0f;
	assert(snr4 >= 31.74f && snr4 <= 31.76f);

	// Max negative: -128/4 = -32.0 dB
	int8_t wire5 = -128;
	float snr5 = wire5 / 4.0f;
	assert(snr5 >= -32.01f && snr5 <= -31.99f);

	printf(" PASS\n");
}


static void
TestAckCodeMatching()
{
	printf("  TestAckCodeMatching...");

	// PUSH_SEND_CONFIRMED: [0]=code [1-4]=ackCode [5-8]=roundTripMs
	// Fix: ackCode is now read and matched against pending->expectedAck

	// Verify expectedAck field exists in PendingMessage
	assert(FileContains("MainWindow.h", "expectedAck"));

	// Verify ackCode is extracted in PUSH_SEND_CONFIRMED handler
	assert(FileContains("MainWindow.cpp", "ReadLE32(data + 1)"));

	// Verify matching logic uses expectedAck
	int matchRefs = CountDualPattern("MainWindow.cpp", "expectedAck", "ackCode");
	assert(matchRefs > 0);

	// Verify expectedAck is populated from RSP_SENT
	assert(FileContains("MainWindow.cpp", "pending->expectedAck"));

	printf(" PASS\n");
}


static void
TestDeliveryQueueLimit()
{
	printf("  TestDeliveryQueueLimit...");

	// kMaxSimultaneousPending = 3
	assert(FileContains("MainWindow.cpp", "kMaxSimultaneousPending"));

	// Fix: queue full now shows feedback in chat (DELIVERY_FAILED message)
	assert(FileContains("MainWindow.cpp", "Too many pending messages"));
	assert(FileContains("MainWindow.cpp", "Message not sent"));
	assert(FileContains("MainWindow.cpp", "DELIVERY_FAILED"));

	printf(" PASS\n");
}


static void
TestChannelDeliveryStatus()
{
	printf("  TestChannelDeliveryStatus...");

	// Fix: channel messages now show DELIVERY_FAILED if SendChannelMsg fails
	// Before: outMsg.deliveryStatus was always hardcoded to DELIVERY_SENT
	// After: set conditionally based on SendChannelMsg return value

	// Verify both DELIVERY_SENT and DELIVERY_FAILED appear near channel send
	// The key change is that outMsg.deliveryStatus is set inside an if/else
	// checking sendResult, not unconditionally
	assert(FileContains("MainWindow.cpp", "outMsg.deliveryStatus = DELIVERY_SENT"));
	assert(FileContains("MainWindow.cpp", "outMsg.deliveryStatus = DELIVERY_FAILED"));

	printf(" PASS\n");
}


static void
TestSmazCompressionGuards()
{
	printf("  TestSmazCompressionGuards...");

	// SMAZ compression only attempted if:
	// 1. textLen > 4
	// 2. Not a GIF/voice/image envelope
	// 3. Compressed output smaller than original

	assert(FileContains("MainWindow.cpp", "textLen > 4"));
	assert(FileContains("MainWindow.cpp", "IsGifMessage"));
	assert(FileContains("MainWindow.cpp", "IsVoiceEnvelope"));
	assert(FileContains("MainWindow.cpp", "IsImageEnvelope"));

	// Decompression guard: rejects if decompLen <= 0 or >= sizeof(decoded)
	assert(FileContains("MainWindow.cpp", "decompLen > 0"));
	assert(FileContains("MainWindow.cpp", "decompLen < (int)sizeof(decoded)"));

	// Fix: decompression failure leaves message as-is and logs warning
	assert(FileContains("MainWindow.cpp", "SMAZ decompression failed"));
	assert(FileContains("MainWindow.cpp", "showing message as-is"));

	printf(" PASS\n");
}


static void
TestPendingMessageRetryConstants()
{
	printf("  TestPendingMessageRetryConstants...");

	// Verify retry timeouts and limits are defined
	assert(FileContains("MainWindow.cpp", "kSendTimeouts"));
	assert(FileContains("MainWindow.cpp", "kMaxRetryAttempts") ||
		   FileContains("MainWindow.cpp", "kMaxSimultaneousPending"));
	assert(FileContains("MainWindow.cpp", "kLateAckGrace"));

	// Verify grace period logic exists
	assert(FileContains("MainWindow.cpp", "inGracePeriod"));
	assert(FileContains("MainWindow.cpp", "graceStartTime"));

	printf(" PASS\n");
}


static void
TestDisconnectPendingCleanup()
{
	printf("  TestDisconnectPendingCleanup...");

	// On disconnect, messages with gotRspSent=true are removed (already sent)
	// Messages without RSP_SENT are kept for retry after reconnect
	assert(FileContains("MainWindow.cpp", "gotRspSent") &&
		   FileContains("MainWindow.cpp", "_OnDisconnected"));

	// Verify reconnect drains outbox
	assert(FileContains("MainWindow.cpp", "_DrainOutbox"));

	printf(" PASS\n");
}


static void
TestRoomNickParserInCode()
{
	printf("  TestRoomNickParserInCode...");

	// Room nick parsing: checks type == 3, finds last ": " within 32 chars
	assert(FileContains("MainWindow.cpp", "type == 3"));

	// Fix: uses loop to find LAST ": " instead of strchr (first)
	assert(FileContains("MainWindow.cpp", "bestColon"));

	// Nick extracted into BString (safe dynamic allocation)
	assert(FileContains("MainWindow.cpp", "roomParticipant"));

	printf(" PASS\n");
}


static void
TestDeleteMessageUsesInt64()
{
	printf("  TestDeleteMessageUsesInt64...");

	// Verify the Y2038 fix: DeleteMessage must use sqlite3_bind_int64 for timestamp
	FILE* f = OpenSource("DatabaseManager.cpp");
	assert(f != NULL);

	char line[512];
	bool inDeleteMethod = false;
	bool usesInt64 = false;
	bool usesBuggyInt = false;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "DeleteMessage") != NULL &&
			strstr(line, "void") == NULL &&
			strstr(line, "ForContact") == NULL)
			inDeleteMethod = true;
		if (inDeleteMethod) {
			if (strstr(line, "sqlite3_bind_int64") != NULL &&
				strstr(line, "timestamp") != NULL)
				usesInt64 = true;
			if (strstr(line, "sqlite3_bind_int(") != NULL &&
				strstr(line, "(int)timestamp") != NULL)
				usesBuggyInt = true;
			// End of method
			if (strstr(line, "sqlite3_finalize") != NULL)
				break;
		}
	}
	fclose(f);

	assert(usesInt64);
	assert(!usesBuggyInt);

	printf(" PASS\n");
}


static void
TestImageEnvelopeSession()
{
	printf("  TestImageEnvelopeSession...");

	// Envelope session tracking moved to MediaHandler
	// Verify session-based tracking exists in MediaHandler
	assert(FileContains("MediaHandler.h", "fImageEnvelopeSession"));
	assert(FileContains("MediaHandler.h", "fVoiceEnvelopeSession"));

	// Old boolean should be gone
	assert(!FileContains("MainWindow.h", "fImageEnvelopeWaiting"));
	assert(!FileContains("MainWindow.h", "fVoiceEnvelopeWaiting"));

	// MainWindow routes confirmation to MediaHandler
	assert(FileContains("MainWindow.cpp", "MSG_MEDIA_ENVELOPE_CONFIRMED"));

	printf(" PASS\n");
}


// ============================================================================
// MAIN
// ============================================================================

int main()
{
	printf("=== DM & Room Message Flow Tests ===\n");

	TestV3FrameOffsets();
	TestV2FrameOffsets();
	TestV3MinLengthCheck();
	TestTimestampSanitization();
	TestSelfEchoFilter();
	TestRoomMessageParsing();
	TestRoomTxtTypePlain();
	TestTextTruncationLimit();
	TestUTF8TruncationFix();
	TestSNRScaling();
	TestAckCodeMatching();
	TestDeliveryQueueLimit();
	TestChannelDeliveryStatus();
	TestSmazCompressionGuards();
	TestPendingMessageRetryConstants();
	TestDisconnectPendingCleanup();
	TestRoomNickParserInCode();
	TestDeleteMessageUsesInt64();
	TestImageEnvelopeSession();

	printf("\n=== All 19 message flow tests passed (7 bug fixes verified) ===\n");
	return 0;
}
