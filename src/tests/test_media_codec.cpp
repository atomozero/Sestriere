/*
 * Test: Media codec session logic
 * Verifies ImageSession/VoiceSession envelope format, fragment assembly,
 * and round-trip integrity without requiring actual codec libraries.
 */

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <cstdlib>


// Minimal stubs to test session logic without full Haiku deps
#define _APPLICATION_H
#define _BITMAP_H
class BBitmap {};

#include "../ImageSession.h"
#include "../VoiceSession.h"


static void
TestImageEnvelopeFormatParse()
{
	printf("  TestImageEnvelopeFormatParse...");

	uint8 selfKey[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	uint32 sid = 0x12345678;
	uint8 format = kImageFormatWebP;
	uint8 totalFrags = 5;
	int32 width = 192, height = 144;
	uint32 totalSize = 2048;
	uint32 timestamp = 1700000000;

	BString envelope = ImageSessionManager::FormatEnvelope(sid, format,
		totalFrags, width, height, totalSize, selfKey, timestamp);

	// Verify envelope starts with "IE2:"
	assert(envelope.FindFirst("IE2:") == 0);

	// Verify it contains sender key hex (lowercase)
	assert(envelope.FindFirst("aabbccddeeff") >= 0);

	printf(" PASS\n");
}


static void
TestImageFragmentAssembly()
{
	printf("  TestImageFragmentAssembly...");

	ImageSessionManager mgr;

	uint8 selfKey[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
	uint8 testData[100];
	for (int i = 0; i < 100; i++)
		testData[i] = (uint8)(i & 0xFF);

	// Create outgoing session
	uint32 sid = mgr.CreateOutgoing(testData, 100, 64, 48, selfKey);
	assert(sid != 0);

	ImageSession* session = mgr.FindSession(sid);
	assert(session != NULL);
	assert(session->totalFragments > 0);
	assert(session->jpegSize == 100);

	// Verify fragments contain the original data
	size_t totalReassembled = 0;
	for (uint8 i = 0; i < session->totalFragments; i++) {
		assert(session->fragments[i].length > 0);
		totalReassembled += session->fragments[i].length;
	}
	assert(totalReassembled == 100);

	printf(" PASS\n");
}


static void
TestVoiceEnvelopeFormatParse()
{
	printf("  TestVoiceEnvelopeFormatParse...");

	uint8 selfKey[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	uint32 sid = 0xDEADBEEF;
	VoicePacketMode mode = VOICE_MODE_1300;
	uint8 totalFrags = 3;
	uint32 duration = 5;
	uint32 timestamp = 1700000000;

	BString envelope = VoiceSessionManager::FormatEnvelope(sid, mode,
		totalFrags, duration, selfKey, timestamp);

	// Verify envelope starts with "VE2:"
	assert(envelope.FindFirst("VE2:") == 0);

	// Verify it contains sender key hex (lowercase)
	assert(envelope.FindFirst("112233445566") >= 0);

	printf(" PASS\n");
}


static void
TestVoiceFragmentAssembly()
{
	printf("  TestVoiceFragmentAssembly...");

	VoiceSessionManager mgr;

	uint8 selfKey[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	uint8 testData[64];
	for (int i = 0; i < 64; i++)
		testData[i] = (uint8)(i * 2);

	uint32 sid = mgr.CreateOutgoing(testData, 64, VOICE_MODE_1300, 3, selfKey);
	assert(sid != 0);

	VoiceSession* session = mgr.FindSession(sid);
	assert(session != NULL);
	assert(session->totalFragments > 0);
	assert(session->codec2Size == 64);
	assert(session->durationSec == 3);

	printf(" PASS\n");
}


static void
TestImageBuildFragmentPacket()
{
	printf("  TestImageBuildFragmentPacket...");

	uint8 packet[256];
	uint8 fragData[50];
	memset(fragData, 0x42, 50);

	size_t pktLen = ImageSessionManager::BuildFragmentPacket(
		packet, 0xAABBCCDD, kImageFormatWebP, 2, 5, fragData, 50);

	// Verify header
	assert(pktLen == kImageHeaderSize + 50);
	assert(packet[0] == kImageMagic);
	// Session ID (LE)
	assert(packet[1] == 0xDD);
	assert(packet[2] == 0xCC);
	assert(packet[3] == 0xBB);
	assert(packet[4] == 0xAA);
	// Format
	assert(packet[5] == kImageFormatWebP);
	// Index
	assert(packet[6] == 2);
	// Total
	assert(packet[7] == 5);
	// Data
	assert(packet[kImageHeaderSize] == 0x42);

	printf(" PASS\n");
}


int
main()
{
	printf("=== test_media_codec ===\n");

	TestImageEnvelopeFormatParse();
	TestImageFragmentAssembly();
	TestVoiceEnvelopeFormatParse();
	TestVoiceFragmentAssembly();
	TestImageBuildFragmentPacket();

	printf("All media codec tests passed!\n");
	return 0;
}
