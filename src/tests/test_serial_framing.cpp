/*
 * Test: Serial frame encoding/decoding
 * Verifies the frame format [marker][len_lo][len_hi][payload...]
 * without requiring actual serial I/O.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>


// Frame marker byte
static const uint8_t kFrameMarker = 0xC0;


// Simulate frame encoding (same logic as SerialHandler::SendFrame)
static size_t
EncodeFrame(const uint8_t* payload, size_t payloadLen,
	uint8_t* outBuffer, size_t bufferSize)
{
	if (payloadLen > 0xFFFF || bufferSize < payloadLen + 3)
		return 0;

	outBuffer[0] = kFrameMarker;
	outBuffer[1] = (uint8_t)(payloadLen & 0xFF);        // len_lo
	outBuffer[2] = (uint8_t)((payloadLen >> 8) & 0xFF); // len_hi
	memcpy(outBuffer + 3, payload, payloadLen);
	return payloadLen + 3;
}


// Simulate frame decoding (same logic as SerialHandler read loop)
static bool
DecodeFrame(const uint8_t* buffer, size_t bufferLen,
	const uint8_t** outPayload, size_t* outPayloadLen)
{
	if (bufferLen < 3)
		return false;

	if (buffer[0] != kFrameMarker)
		return false;

	uint16_t len = (uint16_t)buffer[1] | ((uint16_t)buffer[2] << 8);
	if (bufferLen < (size_t)(3 + len))
		return false;

	*outPayload = buffer + 3;
	*outPayloadLen = len;
	return true;
}


static void
TestEmptyPayload()
{
	printf("  TestEmptyPayload...");
	uint8_t buf[16];
	size_t frameLen = EncodeFrame(NULL, 0, buf, sizeof(buf));
	assert(frameLen == 3);
	assert(buf[0] == kFrameMarker);
	assert(buf[1] == 0);
	assert(buf[2] == 0);

	const uint8_t* payload;
	size_t payloadLen;
	assert(DecodeFrame(buf, frameLen, &payload, &payloadLen));
	assert(payloadLen == 0);
	printf(" PASS\n");
}


static void
TestSmallPayload()
{
	printf("  TestSmallPayload...");
	uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	uint8_t buf[64];
	size_t frameLen = EncodeFrame(data, 5, buf, sizeof(buf));
	assert(frameLen == 8);
	assert(buf[1] == 5);  // len_lo
	assert(buf[2] == 0);  // len_hi

	const uint8_t* payload;
	size_t payloadLen;
	assert(DecodeFrame(buf, frameLen, &payload, &payloadLen));
	assert(payloadLen == 5);
	assert(memcmp(payload, data, 5) == 0);
	printf(" PASS\n");
}


static void
TestLargePayload()
{
	printf("  TestLargePayload...");
	// 300 bytes — tests 16-bit length encoding
	uint8_t data[300];
	for (int i = 0; i < 300; i++)
		data[i] = (uint8_t)(i & 0xFF);

	uint8_t buf[512];
	size_t frameLen = EncodeFrame(data, 300, buf, sizeof(buf));
	assert(frameLen == 303);
	assert(buf[1] == (300 & 0xFF));     // 0x2C
	assert(buf[2] == ((300 >> 8) & 0xFF)); // 0x01

	const uint8_t* payload;
	size_t payloadLen;
	assert(DecodeFrame(buf, frameLen, &payload, &payloadLen));
	assert(payloadLen == 300);
	assert(memcmp(payload, data, 300) == 0);
	printf(" PASS\n");
}


static void
TestIncompleteFrame()
{
	printf("  TestIncompleteFrame...");
	uint8_t buf[] = {kFrameMarker, 0x05, 0x00, 0x01, 0x02};
	// Says 5 bytes payload but only 2 available
	const uint8_t* payload;
	size_t payloadLen;
	assert(!DecodeFrame(buf, sizeof(buf), &payload, &payloadLen));
	printf(" PASS\n");
}


static void
TestInvalidMarker()
{
	printf("  TestInvalidMarker...");
	uint8_t buf[] = {0x00, 0x02, 0x00, 0xAA, 0xBB};
	const uint8_t* payload;
	size_t payloadLen;
	assert(!DecodeFrame(buf, sizeof(buf), &payload, &payloadLen));
	printf(" PASS\n");
}


static void
TestMaxPayload()
{
	printf("  TestMaxPayload...");
	// 65535 bytes — maximum
	size_t maxLen = 65535;
	uint8_t* data = new uint8_t[maxLen];
	memset(data, 0x42, maxLen);

	uint8_t* buf = new uint8_t[maxLen + 3];
	size_t frameLen = EncodeFrame(data, maxLen, buf, maxLen + 3);
	assert(frameLen == maxLen + 3);
	assert(buf[1] == 0xFF);
	assert(buf[2] == 0xFF);

	const uint8_t* payload;
	size_t payloadLen;
	assert(DecodeFrame(buf, frameLen, &payload, &payloadLen));
	assert(payloadLen == maxLen);

	delete[] data;
	delete[] buf;
	printf(" PASS\n");
}


int
main()
{
	printf("=== test_serial_framing ===\n");

	TestEmptyPayload();
	TestSmallPayload();
	TestLargePayload();
	TestIncompleteFrame();
	TestInvalidMarker();
	TestMaxPayload();

	printf("All serial framing tests passed!\n");
	return 0;
}
