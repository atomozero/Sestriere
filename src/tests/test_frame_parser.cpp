/*
 * Test: FrameParser decode verification
 * Feeds known binary frames and verifies BMessage fields produced.
 * Uses a mock BLooper+BHandler to capture posted messages.
 */

#include <Application.h>
#include <Handler.h>
#include <Looper.h>
#include <Message.h>
#include <OS.h>

#include <cstdio>
#include <cstring>
#include <cassert>

#include "../FrameParser.h"
#include "../Constants.h"


// Mock handler that stores the last received message
class MockHandler : public BHandler {
public:
	MockHandler() : BHandler("mock"), fLastMsg((uint32)0) {}

	void MessageReceived(BMessage* message) override
	{
		fLastMsg = *message;
		fReceived = true;
	}

	BMessage	fLastMsg;
	bool		fReceived = false;

	void Reset() { fReceived = false; fLastMsg.what = 0; }
};


static BApplication* sApp = NULL;
static BLooper* sLooper = NULL;
static MockHandler* sMock = NULL;
static FrameParser* sParser = NULL;


static void
Setup()
{
	sApp = new BApplication("application/x-vnd.test-frame-parser");
	sLooper = new BLooper("TestLooper");
	sMock = new MockHandler();
	sLooper->AddHandler(sMock);
	sParser = new FrameParser(sMock);
	sLooper->AddHandler(sParser);
	sLooper->Run();
	snooze(50000);  // Let looper start
}


static void
Teardown()
{
	sLooper->Lock();
	sLooper->Quit();
	delete sApp;
}


// Helper: parse a frame and wait for the message to arrive
static BMessage
ParseAndWait(const uint8* data, size_t length)
{
	sMock->Reset();
	sParser->ParseFrame(data, length);
	// Message is posted async — wait for delivery
	for (int i = 0; i < 50 && !sMock->fReceived; i++)
		snooze(10000);
	return sMock->fLastMsg;
}


// ============================================================================
// Tests
// ============================================================================

static void
TestRspOk()
{
	printf("  TestRspOk...");
	uint8 frame[] = {RSP_OK};
	BMessage msg = ParseAndWait(frame, 1);
	assert(msg.what == MSG_FRAME_OK);
	printf(" PASS\n");
}


static void
TestRspErr()
{
	printf("  TestRspErr...");
	uint8 frame[] = {RSP_ERR, 0x03};  // error code 3 = TABLE_FULL
	BMessage msg = ParseAndWait(frame, 2);
	assert(msg.what == MSG_FRAME_ERR);
	uint8 code = 0;
	assert(msg.FindUInt8("error_code", &code) == B_OK);
	assert(code == 3);
	printf(" PASS\n");
}


static void
TestDeviceInfo()
{
	printf("  TestDeviceInfo...");
	// RSP_DEVICE_INFO: [0]=13, [1]=boardType, [2]=maxContacts/2, [3]=maxChannels
	uint8 frame[] = {RSP_DEVICE_INFO, 10, 25, 8};
	BMessage msg = ParseAndWait(frame, 4);
	assert(msg.what == MSG_FRAME_DEVICE_INFO);

	uint8 boardType = 0;
	msg.FindUInt8("board_type", &boardType);
	assert(boardType == 10);
	uint8 maxCh = 0;
	msg.FindUInt8("max_channels", &maxCh);
	assert(maxCh == 8);

	printf(" PASS\n");
}


static void
TestContactsStart()
{
	printf("  TestContactsStart...");
	uint8 frame[] = {RSP_CONTACTS_START};
	BMessage msg = ParseAndWait(frame, 1);
	assert(msg.what == MSG_FRAME_CONTACTS_START);
	printf(" PASS\n");
}


static void
TestContactsEnd()
{
	printf("  TestContactsEnd...");
	uint8 frame[] = {RSP_END_OF_CONTACTS};
	BMessage msg = ParseAndWait(frame, 1);
	assert(msg.what == MSG_FRAME_CONTACTS_END);
	printf(" PASS\n");
}


static void
TestBattAndStorage()
{
	printf("  TestBattAndStorage...");
	// RSP_BATT_AND_STORAGE: [0]=12, [1-2]=battMv(LE), [3-6]=usedKb, [7-10]=totalKb
	uint8 frame[11] = {RSP_BATT_AND_STORAGE,
		0x64, 0x0F,              // 3940 mV (0x0F64)
		0x00, 0x04, 0x00, 0x00, // 1024 KB used
		0x00, 0x10, 0x00, 0x00  // 4096 KB total
	};
	BMessage msg = ParseAndWait(frame, 11);
	assert(msg.what == MSG_FRAME_BATT_STORAGE);

	uint16 battMv = 0;
	msg.FindUInt16("batt_mv", &battMv);
	assert(battMv == 3940);

	printf(" PASS\n");
}


static void
TestPushAdvert()
{
	printf("  TestPushAdvert...");
	// PUSH_ADVERT: [0]=0x80, [1]=advType, [2-7]=pubkey prefix,
	// [8-11]=lastSeen, [12-43]=pubkey full, [44+]=name
	uint8 frame[50];
	memset(frame, 0, sizeof(frame));
	frame[0] = PUSH_ADVERT;
	frame[1] = 1;  // advType
	// pubkey prefix at [2-7]
	frame[2] = 0xAA; frame[3] = 0xBB; frame[4] = 0xCC;
	frame[5] = 0xDD; frame[6] = 0xEE; frame[7] = 0xFF;

	BMessage msg = ParseAndWait(frame, 50);
	assert(msg.what == MSG_FRAME_ADVERT);
	printf(" PASS\n");
}


static void
TestMsgWaiting()
{
	printf("  TestMsgWaiting...");
	uint8 frame[] = {PUSH_MSG_WAITING};
	BMessage msg = ParseAndWait(frame, 1);
	assert(msg.what == MSG_FRAME_MSG_WAITING);
	printf(" PASS\n");
}


static void
TestPathUpdated()
{
	printf("  TestPathUpdated...");
	uint8 frame[] = {PUSH_PATH_UPDATED};
	BMessage msg = ParseAndWait(frame, 1);
	assert(msg.what == MSG_FRAME_PATH_UPDATED);
	printf(" PASS\n");
}


static void
TestUnknownFrame()
{
	printf("  TestUnknownFrame...");
	uint8 frame[] = {0xFE, 0x01, 0x02, 0x03};
	BMessage msg = ParseAndWait(frame, 4);
	assert(msg.what == MSG_FRAME_UNKNOWN);
	uint8 code = 0;
	msg.FindUInt8("code", &code);
	assert(code == 0xFE);
	printf(" PASS\n");
}


static void
TestAutoAddConfig()
{
	printf("  TestAutoAddConfig...");
	uint8 frame[] = {RSP_AUTO_ADD_CONFIG, 0x0F};  // all flags set
	BMessage msg = ParseAndWait(frame, 2);
	assert(msg.what == MSG_FRAME_AUTO_ADD_CONFIG);
	uint8 flags = 0;
	msg.FindUInt8("flags", &flags);
	assert(flags == 0x0F);
	printf(" PASS\n");
}


static void
TestCurrTime()
{
	printf("  TestCurrTime...");
	// RSP_CURR_TIME: [0]=9, [1-4]=unix timestamp LE
	uint8 frame[] = {RSP_CURR_TIME, 0x40, 0x42, 0x0F, 0x00};  // 1000000
	BMessage msg = ParseAndWait(frame, 5);
	assert(msg.what == MSG_FRAME_CURR_TIME);
	printf(" PASS\n");
}


static void
TestRawData()
{
	printf("  TestRawData...");
	// PUSH_RAW_DATA: [0]=0x84, [1]=snr, [2]=rssi, [3]=?, [4+]=payload
	uint8 frame[] = {0x84, 20, (uint8)-80, 0x00, 0xDE, 0xAD};
	BMessage msg = ParseAndWait(frame, 6);
	assert(msg.what == MSG_FRAME_RAW_DATA);
	printf(" PASS\n");
}


int
main()
{
	printf("=== test_frame_parser ===\n");

	Setup();

	TestRspOk();
	TestRspErr();
	TestDeviceInfo();
	TestContactsStart();
	TestContactsEnd();
	TestBattAndStorage();
	TestPushAdvert();
	TestMsgWaiting();
	TestPathUpdated();
	TestUnknownFrame();
	TestAutoAddConfig();
	TestCurrTime();
	TestRawData();

	Teardown();

	printf("All FrameParser tests passed!\n");
	return 0;
}
