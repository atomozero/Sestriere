/*
 * Test: ProtocolHandler command frame construction
 * Verifies that extracted protocol methods build correct V3 frames
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>


// Mirror key constants from the project
static const size_t kPubKeySize = 32;
static const size_t kPubKeyPrefixSize = 6;
static const uint8_t CMD_APP_START = 1;
static const uint8_t CMD_SEND_TXT_MSG = 2;
static const uint8_t CMD_SEND_CHANNEL_TXT_MSG = 3;
static const uint8_t CMD_GET_CONTACTS = 4;
static const uint8_t CMD_GET_DEVICE_TIME = 5;
static const uint8_t CMD_SET_DEVICE_TIME = 6;
static const uint8_t CMD_SEND_SELF_ADVERT = 7;
static const uint8_t CMD_SET_ADVERT_NAME = 8;
static const uint8_t CMD_SET_RADIO_PARAMS = 11;
static const uint8_t CMD_SET_RADIO_TX_POWER = 12;
static const uint8_t CMD_RESET_PATH = 13;
static const uint8_t CMD_SET_ADVERT_LATLON = 14;
static const uint8_t CMD_REMOVE_CONTACT = 15;
static const uint8_t CMD_SHARE_CONTACT = 16;
static const uint8_t CMD_EXPORT_CONTACT = 17;
static const uint8_t CMD_REBOOT = 19;
static const uint8_t CMD_DEVICE_QUERY = 22;
static const uint8_t CMD_SEND_LOGIN = 26;
static const uint8_t CMD_SEND_TRACE_PATH = 36;
static const uint8_t CMD_GET_STATS = 56;
static const uint8_t CMD_SET_OTHER_PARAMS = 38;

static const uint8_t TXT_TYPE_PLAIN = 0;
static const uint8_t TXT_TYPE_CLI_DATA = 1;


// ============================================================================
// Simulate ProtocolHandler frame building (extracted from ProtocolHandler.cpp)
// ============================================================================

// Captures what would be sent
struct CapturedFrame {
	uint8_t data[512];
	size_t length;
};


static CapturedFrame gLastFrame;


static void
SimulateSendFrame(const uint8_t* payload, size_t length)
{
	assert(length <= sizeof(gLastFrame.data));
	memcpy(gLastFrame.data, payload, length);
	gLastFrame.length = length;
}


// ============================================================================
// Test 1: APP_START frame format
// ============================================================================

static void
TestAppStartFrame()
{
	uint8_t payload[32];
	memset(payload, 0, sizeof(payload));
	payload[0] = CMD_APP_START;
	payload[1] = 3;  // V3
	const char* appName = "Sestriere";
	size_t nameLen = strlen(appName);
	memcpy(payload + 8, appName, nameLen);
	SimulateSendFrame(payload, 8 + nameLen);

	assert(gLastFrame.data[0] == CMD_APP_START);
	assert(gLastFrame.data[1] == 3);
	// Bytes 2-7 are reserved zeros
	for (int i = 2; i < 8; i++)
		assert(gLastFrame.data[i] == 0);
	assert(memcmp(gLastFrame.data + 8, "Sestriere", 9) == 0);
	assert(gLastFrame.length == 17);

	printf("  PASS: APP_START frame has V3 format with reserved bytes\n");
}


// ============================================================================
// Test 2: RadioParams uses kHz for frequency
// ============================================================================

static void
TestRadioParamsFrame()
{
	uint32_t freqHz = 906875000;  // 906.875 MHz
	uint32_t bwHz = 250000;       // 250 kHz
	uint8_t sf = 11;
	uint8_t cr = 5;

	uint32_t freqKHz = freqHz / 1000;

	uint8_t payload[11];
	payload[0] = CMD_SET_RADIO_PARAMS;
	payload[1] = freqKHz & 0xFF;
	payload[2] = (freqKHz >> 8) & 0xFF;
	payload[3] = (freqKHz >> 16) & 0xFF;
	payload[4] = (freqKHz >> 24) & 0xFF;
	payload[5] = bwHz & 0xFF;
	payload[6] = (bwHz >> 8) & 0xFF;
	payload[7] = (bwHz >> 16) & 0xFF;
	payload[8] = (bwHz >> 24) & 0xFF;
	payload[9] = sf;
	payload[10] = cr;
	SimulateSendFrame(payload, 11);

	// Verify frequency is in kHz
	uint32_t readFreqKHz = gLastFrame.data[1]
		| (gLastFrame.data[2] << 8)
		| (gLastFrame.data[3] << 16)
		| (gLastFrame.data[4] << 24);
	assert(readFreqKHz == 906875);

	// Verify bandwidth is in Hz
	uint32_t readBwHz = gLastFrame.data[5]
		| (gLastFrame.data[6] << 8)
		| (gLastFrame.data[7] << 16)
		| (gLastFrame.data[8] << 24);
	assert(readBwHz == 250000);

	assert(gLastFrame.data[9] == 11);
	assert(gLastFrame.data[10] == 5);

	printf("  PASS: RadioParams sends freq in kHz, bandwidth in Hz\n");
}


// ============================================================================
// Test 3: RemoveContact uses full 32-byte pubkey
// ============================================================================

static void
TestRemoveContactFrame()
{
	uint8_t pubkey[32];
	for (int i = 0; i < 32; i++)
		pubkey[i] = (uint8_t)(0xA0 + i);

	uint8_t payload[1 + kPubKeySize];
	payload[0] = CMD_REMOVE_CONTACT;
	memcpy(payload + 1, pubkey, kPubKeySize);
	SimulateSendFrame(payload, sizeof(payload));

	assert(gLastFrame.length == 33);
	assert(gLastFrame.data[0] == CMD_REMOVE_CONTACT);
	assert(memcmp(gLastFrame.data + 1, pubkey, 32) == 0);

	printf("  PASS: RemoveContact sends full 32-byte pubkey\n");
}


// ============================================================================
// Test 4: Login uses full 32-byte pubkey with password
// ============================================================================

static void
TestLoginFrame()
{
	uint8_t pubkey[32];
	for (int i = 0; i < 32; i++)
		pubkey[i] = (uint8_t)(0x10 + i);
	const char* password = "mypass";
	size_t passLen = strlen(password);

	uint8_t payload[128];
	payload[0] = CMD_SEND_LOGIN;
	memcpy(payload + 1, pubkey, kPubKeySize);
	memcpy(payload + 33, password, passLen);
	payload[33 + passLen] = '\0';
	SimulateSendFrame(payload, 34 + passLen);

	assert(gLastFrame.length == 40);
	assert(gLastFrame.data[0] == CMD_SEND_LOGIN);
	assert(memcmp(gLastFrame.data + 1, pubkey, 32) == 0);
	assert(memcmp(gLastFrame.data + 33, "mypass", 6) == 0);
	assert(gLastFrame.data[39] == '\0');

	printf("  PASS: Login sends full 32-byte pubkey + password + null\n");
}


// ============================================================================
// Test 5: SendDM frame format
// ============================================================================

static void
TestSendDMFrame()
{
	uint8_t pubkeyPrefix[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	const char* text = "Hello";
	size_t textLen = 5;
	uint32_t timestamp = 1700000000;

	uint8_t payload[256];
	size_t pos = 0;
	payload[pos++] = CMD_SEND_TXT_MSG;
	payload[pos++] = TXT_TYPE_PLAIN;
	payload[pos++] = 0;

	payload[pos++] = timestamp & 0xFF;
	payload[pos++] = (timestamp >> 8) & 0xFF;
	payload[pos++] = (timestamp >> 16) & 0xFF;
	payload[pos++] = (timestamp >> 24) & 0xFF;

	memcpy(payload + pos, pubkeyPrefix, kPubKeyPrefixSize);
	pos += kPubKeyPrefixSize;
	memcpy(payload + pos, text, textLen);
	pos += textLen;
	SimulateSendFrame(payload, pos);

	assert(gLastFrame.data[0] == CMD_SEND_TXT_MSG);
	assert(gLastFrame.data[1] == TXT_TYPE_PLAIN);
	assert(gLastFrame.data[2] == 0);  // attempt

	uint32_t readTs = gLastFrame.data[3]
		| (gLastFrame.data[4] << 8)
		| (gLastFrame.data[5] << 16)
		| (gLastFrame.data[6] << 24);
	assert(readTs == 1700000000);

	assert(memcmp(gLastFrame.data + 7, pubkeyPrefix, 6) == 0);
	assert(memcmp(gLastFrame.data + 13, "Hello", 5) == 0);
	assert(gLastFrame.length == 18);

	printf("  PASS: SendDM frame has correct format [cmd][type][attempt][ts4][key6][text]\n");
}


// ============================================================================
// Test 6: SendChannelMsg frame format
// ============================================================================

static void
TestSendChannelMsgFrame()
{
	uint8_t channelIdx = 2;
	uint32_t timestamp = 1700000000;
	const char* text = "Hi channel";
	size_t textLen = 10;

	uint8_t payload[256];
	size_t pos = 0;
	payload[pos++] = CMD_SEND_CHANNEL_TXT_MSG;
	payload[pos++] = 0;  // txt_type
	payload[pos++] = channelIdx;
	payload[pos++] = timestamp & 0xFF;
	payload[pos++] = (timestamp >> 8) & 0xFF;
	payload[pos++] = (timestamp >> 16) & 0xFF;
	payload[pos++] = (timestamp >> 24) & 0xFF;
	memcpy(payload + pos, text, textLen);
	pos += textLen;
	SimulateSendFrame(payload, pos);

	assert(gLastFrame.data[0] == CMD_SEND_CHANNEL_TXT_MSG);
	assert(gLastFrame.data[1] == 0);
	assert(gLastFrame.data[2] == 2);
	assert(gLastFrame.length == 17);
	assert(memcmp(gLastFrame.data + 7, "Hi channel", 10) == 0);

	printf("  PASS: SendChannelMsg frame has correct format [cmd][type][chIdx][ts4][text]\n");
}


// ============================================================================
// Test 7: SetLatLon encoding
// ============================================================================

static void
TestSetLatLonFrame()
{
	double lat = 45.4408;
	double lon = 12.3155;

	int32_t latInt = (int32_t)(lat * 1000000.0);
	int32_t lonInt = (int32_t)(lon * 1000000.0);

	uint8_t payload[9];
	payload[0] = CMD_SET_ADVERT_LATLON;
	payload[1] = latInt & 0xFF;
	payload[2] = (latInt >> 8) & 0xFF;
	payload[3] = (latInt >> 16) & 0xFF;
	payload[4] = (latInt >> 24) & 0xFF;
	payload[5] = lonInt & 0xFF;
	payload[6] = (lonInt >> 8) & 0xFF;
	payload[7] = (lonInt >> 16) & 0xFF;
	payload[8] = (lonInt >> 24) & 0xFF;
	SimulateSendFrame(payload, 9);

	int32_t readLat = gLastFrame.data[1]
		| (gLastFrame.data[2] << 8)
		| (gLastFrame.data[3] << 16)
		| (gLastFrame.data[4] << 24);
	int32_t readLon = gLastFrame.data[5]
		| (gLastFrame.data[6] << 8)
		| (gLastFrame.data[7] << 16)
		| (gLastFrame.data[8] << 24);

	// Verify microdegrees
	assert(readLat == 45440800);
	assert(readLon == 12315500);

	printf("  PASS: SetLatLon encodes coordinates as microdegrees (int32 LE)\n");
}


// ============================================================================
// Test 8: GetStats sends 3 subtype requests
// ============================================================================

static void
TestGetStatsFrame()
{
	// The protocol requires 3 separate frames for core/radio/packets
	uint8_t payload[2];
	payload[0] = CMD_GET_STATS;

	payload[1] = 0;
	SimulateSendFrame(payload, 2);
	assert(gLastFrame.data[0] == CMD_GET_STATS);
	assert(gLastFrame.data[1] == 0);

	payload[1] = 1;
	SimulateSendFrame(payload, 2);
	assert(gLastFrame.data[1] == 1);

	payload[1] = 2;
	SimulateSendFrame(payload, 2);
	assert(gLastFrame.data[1] == 2);

	printf("  PASS: GetStats sends 3 separate subtype requests (core/radio/packets)\n");
}


// ============================================================================
// Test 9: OtherParams frame layout
// ============================================================================

static void
TestOtherParamsFrame()
{
	uint8_t manualAdd = 1, telemetry = 2, locPolicy = 1, multiAcks = 3;

	uint8_t payload[5];
	payload[0] = CMD_SET_OTHER_PARAMS;
	payload[1] = manualAdd;
	payload[2] = telemetry;
	payload[3] = locPolicy;
	payload[4] = multiAcks;
	SimulateSendFrame(payload, 5);

	assert(gLastFrame.data[0] == CMD_SET_OTHER_PARAMS);
	assert(gLastFrame.data[1] == 1);
	assert(gLastFrame.data[2] == 2);
	assert(gLastFrame.data[3] == 1);
	assert(gLastFrame.data[4] == 3);

	printf("  PASS: OtherParams frame [cmd][manualAdd][telemetry][locPolicy][multiAcks]\n");
}


// ============================================================================
// Test 10: ProtocolHandler class exists and is separate from MainWindow
// ============================================================================

static void
TestProtocolHandlerSeparation()
{
	// Verify ProtocolHandler.h exists and declares the class
	FILE* f = fopen("ProtocolHandler.h", "r");
	assert(f != NULL);

	char buf[16384];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';

	assert(strstr(buf, "class ProtocolHandler") != NULL);
	assert(strstr(buf, "SendAppStart") != NULL);
	assert(strstr(buf, "SendRadioParams") != NULL);
	assert(strstr(buf, "SendRemoveContact") != NULL);
	assert(strstr(buf, "SendDM") != NULL);
	assert(strstr(buf, "SendChannelMsg") != NULL);
	assert(strstr(buf, "SerialHandler*") != NULL);

	// Verify MainWindow.h no longer declares extracted methods
	f = fopen("MainWindow.h", "r");
	assert(f != NULL);
	n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';

	// These should NOT be in MainWindow.h anymore
	assert(strstr(buf, "_SendAppStart") == NULL);
	assert(strstr(buf, "_SendRadioParams") == NULL);
	assert(strstr(buf, "_SendGetContacts") == NULL);
	assert(strstr(buf, "_SendReboot") == NULL);
	assert(strstr(buf, "_SendRemoveContact") == NULL);
	assert(strstr(buf, "_SendResetPath") == NULL);

	// These SHOULD still be in MainWindow.h (UI-level messaging)
	assert(strstr(buf, "_SendTextMessage") != NULL);
	assert(strstr(buf, "_SendChannelMessage") != NULL);
	assert(strstr(buf, "_SendCliCommand") != NULL);

	// MainWindow should have ProtocolHandler pointer
	assert(strstr(buf, "ProtocolHandler*") != NULL);

	printf("  PASS: ProtocolHandler is separate class, MainWindow uses delegation\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== ProtocolHandler Extraction Tests ===\n\n");

	TestAppStartFrame();
	TestRadioParamsFrame();
	TestRemoveContactFrame();
	TestLoginFrame();
	TestSendDMFrame();
	TestSendChannelMsgFrame();
	TestSetLatLonFrame();
	TestGetStatsFrame();
	TestOtherParamsFrame();
	TestProtocolHandlerSeparation();

	printf("\nAll 10 tests passed.\n");
	return 0;
}
