/*
 * fake_radio.cpp — MeshCore Radio Simulator with native Haiku GUI
 *
 * Standalone BApplication that creates a PTY pair and simulates a MeshCore
 * V3 companion device. Connect Sestriere to the displayed PTY path.
 *
 * Build: g++ -o fake_radio fake_radio.cpp -lbe -ltranslation -ltracker -Wall -O2
 */

#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <Button.h>
#include <CheckBox.h>
#include <Clipboard.h>
#include <File.h>
#include <FilePanel.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Looper.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <Slider.h>
#include <SplitView.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <View.h>
#include <Window.h>
#include <OS.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>


// ============================================================================
// Constants
// ============================================================================

static const char* kAppSignature = "application/x-vnd.FakeRadioSim";

enum {
	MSG_PTY_FRAME_RX		= 'pfRX',
	MSG_PTY_FRAME_TX		= 'pfTX',
	MSG_INJECT_DM			= 'injD',
	MSG_INJECT_CHANNEL		= 'injC',
	MSG_AUTO_MSG_TICK		= 'amtk',
	MSG_AUTO_MSG_TOGGLE		= 'amtg',
	MSG_ADD_CONTACT			= 'addc',
	MSG_REMOVE_CONTACT		= 'remc',
	MSG_COPY_PTY_PATH		= 'cpty',
	MSG_STATUS_TICK			= 'sttk',
	MSG_LED_FLASH_OFF		= 'ldof',
	MSG_BATTERY_CHANGED		= 'batc',
	MSG_SNR_CHANGED			= 'snrc',
	MSG_INTERVAL_CHANGED	= 'intc',
	MSG_DEVICE_TYPE_SELECTED = 'dtsl',
	MSG_CONTACT_TYPE_SELECTED = 'ctsl',
	MSG_SEND_CONFIRMED_TICK	= 'scft',
	MSG_WRITE_FRAME			= 'wrfr',
	MSG_BROWSE_IMAGE		= 'bImg',
	MSG_BROWSE_AUDIO		= 'bAud',
	MSG_IMAGE_FILE_SELECTED	= 'iSel',
	MSG_AUDIO_FILE_SELECTED	= 'aSel',
	MSG_SEND_IMAGE_DM		= 'sImg',
	MSG_SEND_AUDIO_DM		= 'sAud',
	MSG_MEDIA_FRAGMENT_TICK	= 'mFrg',
};

// Frame protocol
static const uint8_t kMarkerIn  = 0x3C;  // '<' App -> Radio
static const uint8_t kMarkerOut = 0x3E;  // '>' Radio -> App

// Auto-message pool
static const char* kAutoMessages[] = {
	"Ciao da FakeRadio!",
	"Test messaggio #2",
	"Prova SNR negativo...",
	"Messaggio multi-hop simulato",
	"LoRa mesh funziona! 73",
	"QSO dalla montagna",
	"Batteria al 80%, tutto ok",
	"Segnale forte oggi!",
	"Prova canale pubblico",
	"Test di raggiungibilità",
	"Meteo sereno, SNR ottimo",
	"Fine test, 73 de FakeRadio"
};
static const int kNumAutoMessages = 12;

// LED flash duration
static const bigtime_t kLedFlashDuration = 300000;  // 300ms

// Media injection constants
static const size_t kMaxFragmentPayload = 152;
static const size_t kFragmentHeaderSize = 8;
static const uint8_t kImageMagic = 0x49;   // 'I'
static const uint8_t kVoiceMagic = 0x56;   // 'V'
static const uint8_t kImageFormatJPEG = 1;
static const uint8_t kVoiceMode1300 = 3;   // VOICE_MODE_1300


// ============================================================================
// MediaSendState — tracks ongoing fragment transmission
// ============================================================================

struct MediaSendState {
	uint8_t*	data;
	size_t		dataSize;
	uint32_t	sessionId;
	uint8_t		totalFragments;
	uint8_t		currentIndex;
	uint8_t		magic;         // 0x49 for image, 0x56 for voice
	uint8_t		formatOrMode;  // 1=JPEG or 3=VOICE_MODE_1300
	bool		active;

	MediaSendState()
		:
		data(NULL),
		dataSize(0),
		sessionId(0),
		totalFragments(0),
		currentIndex(0),
		magic(0),
		formatOrMode(0),
		active(false)
	{
	}

	~MediaSendState()
	{
		free(data);
	}

	void Reset()
	{
		free(data);
		data = NULL;
		dataSize = 0;
		sessionId = 0;
		totalFragments = 0;
		currentIndex = 0;
		magic = 0;
		formatOrMode = 0;
		active = false;
	}
};


// ============================================================================
// SimulatedContact
// ============================================================================

struct SimulatedContact {
	uint8	pubKey[32];
	char	name[64];
	uint8	type;		// 1=CHAT, 2=REPEATER, 3=ROOM
	int32	latitude;	// 1e-6 degrees
	int32	longitude;
};

// Default contacts
static SimulatedContact sDefaultContacts[] = {
	{ {0xDE,0xAD,0xBE,0xEF,0xCA,0xFE, 0x01,0x02,0x03,0x04,0x05,0x06,
	   0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,
	   0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A},
	  "Marco-Roma", 1, (int32)(41.9028 * 1e6), (int32)(12.4964 * 1e6) },

	{ {0xAA,0x11,0x22,0x33,0x44,0x55, 0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,
	   0xA7,0xA8,0xA9,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,
	   0xB7,0xB8,0xB9,0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6},
	  "Luca-Milano", 1, (int32)(45.4642 * 1e6), (int32)(9.1900 * 1e6) },

	{ {0xBB,0x22,0x33,0x44,0x55,0x66, 0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,
	   0xD7,0xD8,0xD9,0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,
	   0xE7,0xE8,0xE9,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6},
	  "Repeater-Torino", 2, (int32)(45.0703 * 1e6), (int32)(7.6869 * 1e6) },

	{ {0xCC,0x33,0x44,0x55,0x66,0x77, 0x01,0x12,0x23,0x34,0x45,0x56,
	   0x67,0x78,0x89,0x9A,0xAB,0xBC,0xCD,0xDE,0xEF,0xF0,
	   0x10,0x21,0x32,0x43,0x54,0x65,0x76,0x87,0x98,0xA9},
	  "Room-Firenze", 3, (int32)(43.7696 * 1e6), (int32)(11.2558 * 1e6) },

	{ {0xDD,0x44,0x55,0x66,0x77,0x88, 0x11,0x22,0x33,0x44,0x55,0x66,
	   0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00,
	   0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA},
	  "Anna-Napoli", 1, (int32)(40.8518 * 1e6), (int32)(14.2681 * 1e6) },

	{ {0xEE,0x55,0x66,0x77,0x88,0x99, 0x21,0x32,0x43,0x54,0x65,0x76,
	   0x87,0x98,0xA9,0xBA,0xCB,0xDC,0xED,0xFE,0x0F,0x10,
	   0x21,0x32,0x43,0x54,0x65,0x76,0x87,0x98,0xA9,0xBA},
	  "Sala-Venezia", 3, (int32)(45.4408 * 1e6), (int32)(12.3155 * 1e6) },
};
static const int kNumDefaultContacts = 6;


// ============================================================================
// Device config — modified from GUI, read by protocol engine
// ============================================================================

struct DeviceConfig {
	char	nodeName[64];
	uint8	deviceType;		// 1=CHAT, 2=REPEATER, 3=ROOM
	uint16	batteryMv;
	int32	latitude;
	int32	longitude;
	uint32	freqKhz;		// in kHz for display (e.g. 869618)
	uint32	bwHz;			// in Hz
	uint8	sf;
	uint8	cr;
	uint32	uptime;			// seconds since start
};

static const uint8_t kSelfPubKey[32] = {
	0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0x00, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
	0xA7, 0xA8, 0xA9, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
	0xB5, 0xB6
};


// ============================================================================
// Protocol engine (static functions)
// ============================================================================

static void
SendFrame(int fd, const uint8_t* payload, int len)
{
	uint8_t header[3];
	header[0] = kMarkerOut;
	header[1] = len & 0xFF;
	header[2] = (len >> 8) & 0xFF;
	write(fd, header, 3);
	write(fd, payload, len);
}


static void
SendAppStartAck(int fd)
{
	uint8_t ack[2] = { 0x01, 0x03 };
	SendFrame(fd, ack, 2);
}


static void
SendOk(int fd)
{
	uint8_t rsp[1] = { 0x00 };
	SendFrame(fd, rsp, 1);
}


static void
SendSelfInfo(int fd, const DeviceConfig& cfg)
{
	uint8_t rsp[128];
	memset(rsp, 0, sizeof(rsp));

	rsp[0] = 0x05;			// RSP_SELF_INFO
	rsp[1] = cfg.deviceType;
	rsp[2] = 14;			// TX Power
	rsp[3] = 20;			// Max TX Power

	memcpy(rsp + 4, kSelfPubKey, 32);

	memcpy(rsp + 36, &cfg.latitude, 4);
	memcpy(rsp + 40, &cfg.longitude, 4);

	rsp[44] = 0; rsp[45] = 0; rsp[46] = 0; rsp[47] = 0;

	uint32_t freq_hz = cfg.freqKhz * 1000;
	memcpy(rsp + 48, &freq_hz, 4);
	memcpy(rsp + 52, &cfg.bwHz, 4);
	rsp[56] = cfg.sf;
	rsp[57] = cfg.cr;

	int nameLen = strlen(cfg.nodeName);
	memcpy(rsp + 58, cfg.nodeName, nameLen + 1);

	SendFrame(fd, rsp, 58 + nameLen + 1);
}


static void
SendDeviceInfo(int fd)
{
	uint8_t rsp[4];
	rsp[0] = 0x0D;
	rsp[1] = 0x01;
	rsp[2] = 16;	// maxContacts = 32
	rsp[3] = 8;	// maxChannels
	SendFrame(fd, rsp, 4);
}


static void
SendBattAndStorage(int fd, uint16_t battMv)
{
	uint8_t rsp[11];
	memset(rsp, 0, sizeof(rsp));
	rsp[0] = 0x0C;
	memcpy(rsp + 1, &battMv, 2);
	uint32_t usedKb = 128;
	uint32_t totalKb = 4096;
	memcpy(rsp + 3, &usedKb, 4);
	memcpy(rsp + 7, &totalKb, 4);
	SendFrame(fd, rsp, 11);
}


static void
SendStats(int fd, uint8_t subtype, uint16_t battMv, uint32_t uptime)
{
	uint8_t rsp[16];
	memset(rsp, 0, sizeof(rsp));
	rsp[0] = 0x18;
	rsp[1] = subtype;

	if (subtype == 0) {
		memcpy(rsp + 2, &battMv, 2);
		memcpy(rsp + 4, &uptime, 4);
		SendFrame(fd, rsp, 8);
	} else if (subtype == 1) {
		int16_t noiseFloor = -110;
		memcpy(rsp + 2, &noiseFloor, 2);
		rsp[4] = (uint8_t)-65;
		rsp[5] = (uint8_t)8;
		SendFrame(fd, rsp, 6);
	} else if (subtype == 2) {
		uint32_t recv = 42;
		uint32_t sent = 17;
		memcpy(rsp + 2, &recv, 4);
		memcpy(rsp + 6, &sent, 4);
		SendFrame(fd, rsp, 10);
	}
}


static void
SendContacts(int fd, SimulatedContact contacts[], int count)
{
	uint8_t start[1] = { 0x02 };
	SendFrame(fd, start, 1);

	for (int i = 0; i < count; i++) {
		uint8_t frame[148];
		memset(frame, 0, sizeof(frame));
		frame[0] = 0x03;	// RSP_CONTACT
		memcpy(frame + 1, contacts[i].pubKey, 32);
		frame[33] = contacts[i].type;
		frame[34] = 0;		// flags
		frame[35] = 1;		// outPathLen (direct)
		strncpy((char*)(frame + 100), contacts[i].name, 31);
		uint32_t now = (uint32_t)time(NULL);
		memcpy(frame + 132, &now, 4);
		memcpy(frame + 136, &contacts[i].latitude, 4);
		memcpy(frame + 140, &contacts[i].longitude, 4);
		SendFrame(fd, frame, 148);
	}

	uint8_t end[1] = { 0x04 };
	SendFrame(fd, end, 1);
}


static void
SendIncomingDM(int fd, const SimulatedContact& contact, const char* text,
	int8_t snr)
{
	// V3 DM format (matches Sestriere Constants.h):
	// [0]=0x10, [1]=snr, [2]=rssi, [3]=pathLen,
	// [4-9]=pubkey prefix (6 bytes), [10]=pathLen, [11]=txtType,
	// [12-15]=timestamp (uint32 LE), [16+]=text
	int textLen = strlen(text);
	int payloadLen = 16 + textLen + 1;
	uint8_t* rsp = (uint8_t*)malloc(payloadLen);
	memset(rsp, 0, payloadLen);

	rsp[0] = 0x10;
	rsp[1] = (uint8_t)snr;
	rsp[2] = (uint8_t)-65;          // RSSI
	rsp[3] = 1;                     // pathLen
	memcpy(rsp + 4, contact.pubKey, 6);  // 6-byte pubkey prefix only
	rsp[10] = 1;                    // pathLen (duplicate)
	rsp[11] = 0;                    // txtType (plain text)
	uint32_t ts = (uint32_t)time(NULL);
	memcpy(rsp + 12, &ts, 4);       // timestamp at [12-15]
	memcpy(rsp + 16, text, textLen + 1);  // text at [16+]

	SendFrame(fd, rsp, payloadLen);
	free(rsp);
}


static void
SendChannelMsg(int fd, const SimulatedContact& contact, const char* text,
	int8_t snr)
{
	// V3 Channel format (matches Sestriere Constants.h):
	// [0]=0x11, [1]=snr, [2-3]=reserved,
	// [4]=channelIdx, [5]=pathLen, [6]=txtType,
	// [7-10]=timestamp (uint32 LE), [11+]=text
	(void)contact;  // channel messages don't include sender pubkey
	int textLen = strlen(text);
	int payloadLen = 11 + textLen + 1;
	uint8_t* rsp = (uint8_t*)malloc(payloadLen);
	memset(rsp, 0, payloadLen);

	rsp[0] = 0x11;
	rsp[1] = (uint8_t)snr;
	rsp[2] = 0;                     // reserved
	rsp[3] = 0;                     // reserved
	rsp[4] = 0;                     // channelIdx (0 = public channel)
	rsp[5] = 1;                     // pathLen
	rsp[6] = 0;                     // txtType (plain text)
	uint32_t ts = (uint32_t)time(NULL);
	memcpy(rsp + 7, &ts, 4);        // timestamp at [7-10]
	memcpy(rsp + 11, text, textLen + 1);  // text at [11+]

	SendFrame(fd, rsp, payloadLen);
	free(rsp);
}


static void
SendPushRawData(int fd, const uint8_t* payload, size_t payloadLen, int8_t snr)
{
	size_t frameLen = 4 + payloadLen;
	uint8_t* frame = (uint8_t*)malloc(frameLen);
	frame[0] = 0x84;              // PUSH_RAW_DATA
	frame[1] = (uint8_t)(snr * 4); // SNR quarter-dB
	frame[2] = (uint8_t)-65;       // RSSI
	frame[3] = 0xFF;               // reserved
	memcpy(frame + 4, payload, payloadLen);
	SendFrame(fd, frame, frameLen);
	free(frame);
}


static char*
ToBase36(uint32_t val, char* buf, size_t bufSize)
{
	static const char kDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	if (val == 0) {
		strlcpy(buf, "0", bufSize);
		return buf;
	}

	char tmp[16];
	int pos = sizeof(tmp) - 1;
	tmp[pos] = '\0';

	while (val > 0 && pos > 0) {
		tmp[--pos] = kDigits[val % 36];
		val /= 36;
	}

	strlcpy(buf, tmp + pos, bufSize);
	return buf;
}


// Compress an image file to JPEG at max 128px using Translation Kit.
// Returns malloc'd buffer in *outData. Caller must free().
// Follows the same approach as Sestriere's ImageCodec::CompressImageFile.
static bool
CompressToJpeg(const char* path, uint8_t** outData, size_t* outSize,
	int32* outW, int32* outH)
{
	// Load image via Translation Kit (supports JPEG, PNG, BMP, etc.)
	BBitmap* source = BTranslationUtils::GetBitmap(path);
	if (source == NULL)
		return false;

	BRect srcBounds = source->Bounds();
	int32 srcW = (int32)(srcBounds.Width() + 1);
	int32 srcH = (int32)(srcBounds.Height() + 1);

	// Scale down to max 128px on longest side
	int32 maxDim = 128;
	int32 dstW, dstH;
	if (srcW >= srcH) {
		dstW = (srcW > maxDim) ? maxDim : srcW;
		dstH = (int32)((float)srcH * dstW / srcW);
		if (dstH < 1) dstH = 1;
	} else {
		dstH = (srcH > maxDim) ? maxDim : srcH;
		dstW = (int32)((float)srcW * dstH / srcH);
		if (dstW < 1) dstW = 1;
	}

	// Create scaled bitmap in B_RGB32 (NOT B_RGBA32, NOT acceptsViews)
	BBitmap* scaled = new BBitmap(BRect(0, 0, dstW - 1, dstH - 1), B_RGB32);
	if (scaled->InitCheck() != B_OK) {
		delete source;
		delete scaled;
		return false;
	}

	// Ensure source is B_RGB32 or B_RGBA32 for 4-byte pixel access
	if (source->ColorSpace() != B_RGB32
		&& source->ColorSpace() != B_RGBA32) {
		BBitmap* converted = new BBitmap(srcBounds, B_RGB32);
		if (converted->InitCheck() != B_OK) {
			delete source;
			delete scaled;
			delete converted;
			return false;
		}
		converted->ImportBits(source);
		delete source;
		source = converted;
	}

	// Nearest-neighbor scaling (keep colors, set alpha=255)
	uint8* srcBits = (uint8*)source->Bits();
	uint8* dstBits = (uint8*)scaled->Bits();
	int32 srcBPR = source->BytesPerRow();
	int32 dstBPR = scaled->BytesPerRow();

	for (int32 y = 0; y < dstH; y++) {
		int32 srcY = y * srcH / dstH;
		if (srcY >= srcH) srcY = srcH - 1;
		uint8* srcRow = srcBits + srcY * srcBPR;
		uint8* dstRow = dstBits + y * dstBPR;

		for (int32 x = 0; x < dstW; x++) {
			int32 srcX = x * srcW / dstW;
			if (srcX >= srcW) srcX = srcW - 1;

			uint8* sp = srcRow + srcX * 4;  // B_RGB32: B,G,R,A
			uint8* dp = dstRow + x * 4;
			dp[0] = sp[0];  // B
			dp[1] = sp[1];  // G
			dp[2] = sp[2];  // R
			dp[3] = 255;    // A = opaque
		}
	}

	delete source;

	// Compress to JPEG using Translation Kit
	BBitmapStream stream(scaled);  // stream takes ownership of scaled

	BTranslatorRoster* roster = BTranslatorRoster::Default();
	BMallocIO jpegOutput;

	status_t err = roster->Translate(&stream, NULL, NULL, &jpegOutput,
		B_JPEG_FORMAT);

	// Detach bitmap from stream (stream destructor would delete it)
	BBitmap* detached = NULL;
	stream.DetachBitmap(&detached);
	delete detached;

	if (err != B_OK)
		return false;

	size_t jpegSize = jpegOutput.BufferLength();
	if (jpegSize < 2)
		return false;

	// Verify JPEG SOI marker
	const uint8_t* jpegBuf = (const uint8_t*)jpegOutput.Buffer();
	if (jpegBuf[0] != 0xFF || jpegBuf[1] != 0xD8)
		return false;

	*outData = (uint8_t*)malloc(jpegSize);
	memcpy(*outData, jpegBuf, jpegSize);
	*outSize = jpegSize;
	*outW = dstW;
	*outH = dstH;
	return true;
}


// Read raw PCM or WAV audio file.
// Returns malloc'd buffer in *outData. Caller must free().
static bool
ReadAudioFile(const char* path, uint8_t** outData, size_t* outSize,
	uint32_t* outDurSec)
{
	struct stat st;
	if (stat(path, &st) != 0 || st.st_size < 16)
		return false;

	FILE* f = fopen(path, "rb");
	if (f == NULL)
		return false;

	size_t fileSize = st.st_size;
	size_t dataOffset = 0;
	size_t dataLen = fileSize;

	// Check for WAV header
	uint8_t header[44];
	if (fread(header, 1, 44, f) == 44
		&& header[0] == 'R' && header[1] == 'I'
		&& header[2] == 'F' && header[3] == 'F') {
		// WAV file — skip 44-byte header
		dataOffset = 44;
		dataLen = (fileSize > 44) ? (fileSize - 44) : 0;
	} else {
		// Raw PCM — rewind
		fseek(f, 0, SEEK_SET);
	}

	if (dataLen == 0) {
		fclose(f);
		return false;
	}

	*outData = (uint8_t*)malloc(dataLen);
	if (dataOffset > 0)
		fseek(f, dataOffset, SEEK_SET);
	size_t got = fread(*outData, 1, dataLen, f);
	fclose(f);

	if (got == 0) {
		free(*outData);
		*outData = NULL;
		return false;
	}

	*outSize = got;
	// 8kHz 16-bit mono = 16000 bytes/sec
	*outDurSec = (uint32_t)(got / 16000);
	if (*outDurSec == 0)
		*outDurSec = 1;

	return true;
}


// Returns command name for logging
static const char*
CommandName(uint8_t cmd)
{
	switch (cmd) {
		case 0x01: return "APP_START";
		case 0x02: return "SEND_TXT_MSG";
		case 0x03: return "SEND_CHANNEL_MSG";
		case 0x04: return "GET_CONTACTS";
		case 0x05: return "GET_DEVICE_TIME";
		case 0x06: return "SET_DEVICE_TIME";
		case 0x07: return "SEND_ADVERT";
		case 0x0A: return "SYNC_NEXT_MSG";
		case 0x0B: return "RESET_PATH";
		case 0x0F: return "REMOVE_CONTACT";
		case 0x11: return "EXPORT_CONTACT";
		case 0x14: return "GET_BATT";
		case 0x16: return "DEVICE_QUERY";
		case 0x19: return "SEND_RAW_DATA";
		case 0x32: return "SEND_BINARY_REQ";
		case 0x38: return "GET_STATS";
		default:   return "UNKNOWN";
	}
}


static const char*
ResponseName(uint8_t code) __attribute__((unused));

static const char*
ResponseName(uint8_t code)
{
	switch (code) {
		case 0x00: return "RSP_OK";
		case 0x01: return "RSP_ERR/V3_ACK";
		case 0x02: return "RSP_CONTACTS_START";
		case 0x03: return "RSP_CONTACT";
		case 0x04: return "RSP_END_CONTACTS";
		case 0x05: return "RSP_SELF_INFO";
		case 0x06: return "RSP_SENT";
		case 0x0B: return "RSP_EXPORT_CONTACT";
		case 0x0C: return "RSP_BATT_STORAGE";
		case 0x0D: return "RSP_DEVICE_INFO";
		case 0x10: return "PUSH_DM_V3";
		case 0x11: return "PUSH_CHANNEL_V3";
		case 0x18: return "RSP_STATS";
		case 0x82: return "PUSH_SEND_CONFIRMED";
		case 0x84: return "PUSH_RAW_DATA";
		default:   return "RSP_???";
	}
}


// Handle incoming command and write responses. Returns log info.
static void
HandleCommand(int fd, const uint8_t* payload, int len,
	DeviceConfig& cfg, SimulatedContact contacts[], int contactCount,
	char* logBuf, int logBufSize)
{
	if (len < 1) return;
	uint8_t cmd = payload[0];

	snprintf(logBuf, logBufSize, "%s (%d bytes)", CommandName(cmd), len);

	switch (cmd) {
		case 0x01:	// APP_START
			SendAppStartAck(fd);
			usleep(50000);
			SendSelfInfo(fd, cfg);
			break;

		case 0x02:	// SEND_TXT_MSG
		{
			uint8_t sent[1] = { 0x06 };
			SendFrame(fd, sent, 1);
			// Confirmed will be sent by timer in the window
			if (len > 37)
				snprintf(logBuf, logBufSize, "SEND_TXT_MSG \"%s\"",
					(const char*)(payload + 37));
			break;
		}

		case 0x03:	// SEND_CHANNEL_MSG
		{
			uint8_t sent[1] = { 0x06 };
			SendFrame(fd, sent, 1);
			snprintf(logBuf, logBufSize, "SEND_CHANNEL_MSG (%d bytes)", len);
			break;
		}

		case 0x04:	// GET_CONTACTS
			SendContacts(fd, contacts, contactCount);
			snprintf(logBuf, logBufSize, "GET_CONTACTS -> %d contacts",
				contactCount);
			break;

		case 0x05:	// GET_DEVICE_TIME
		case 0x06:	// SET_DEVICE_TIME
		case 0x07:	// SEND_ADVERT
		case 0x0A:	// SYNC_NEXT_MSG
		case 0x0B:	// RESET_PATH
		case 0x0F:	// REMOVE_CONTACT
			SendOk(fd);
			break;

		case 0x11:	// EXPORT_CONTACT
		{
			uint8_t rsp[64];
			memset(rsp, 0, sizeof(rsp));
			rsp[0] = 0x0B;
			memcpy(rsp + 3, kSelfPubKey, 32);
			SendFrame(fd, rsp, 35);
			break;
		}

		case 0x14:	// GET_BATT
			SendBattAndStorage(fd, cfg.batteryMv);
			snprintf(logBuf, logBufSize, "GET_BATT -> %d mV", cfg.batteryMv);
			break;

		case 0x16:	// DEVICE_QUERY
			SendDeviceInfo(fd);
			break;

		case 0x19:	// CMD_SEND_RAW_DATA (image/voice fragment from app)
		{
			const char* fragType = "unknown";
			if (len >= 5) {
				uint8_t magic = payload[1];
				if (magic == 0x49)
					fragType = "image";
				else if (magic == 0x56)
					fragType = "voice";
			}
			SendOk(fd);
			snprintf(logBuf, logBufSize, "SEND_RAW_DATA (%s fragment, %d bytes)",
				fragType, len);
			break;
		}

		case 0x32:	// CMD_SEND_BINARY_REQ (fetch request from app)
		{
			SendOk(fd);
			snprintf(logBuf, logBufSize, "SEND_BINARY_REQ (fetch request, %d bytes)",
				len);
			break;
		}

		case 0x38:	// GET_STATS
		{
			uint8_t subtype = len > 1 ? payload[1] : 0;
			SendStats(fd, subtype, cfg.batteryMv, cfg.uptime);
			snprintf(logBuf, logBufSize, "GET_STATS sub=%d", subtype);
			break;
		}

		default:
			SendOk(fd);
			snprintf(logBuf, logBufSize, "UNKNOWN 0x%02X -> OK", cmd);
			break;
	}
}


// ============================================================================
// ContactListItem
// ============================================================================

class ContactListItem : public BStringItem {
public:
	ContactListItem(SimulatedContact* contact)
		:
		BStringItem(contact->name),
		fContact(contact)
	{
	}

	SimulatedContact*	Contact() const { return fContact; }

private:
	SimulatedContact*	fContact;
};


// ============================================================================
// RadioVizView — animated radio visualization
// ============================================================================

class RadioVizView : public BView {
public:
	RadioVizView()
		:
		BView("radio_viz", B_WILL_DRAW | B_PULSE_NEEDED),
		fTxPulse(0.0f),
		fTxActive(false)
	{
		SetExplicitMinSize(BSize(80, 80));
		SetExplicitMaxSize(BSize(80, 80));
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}

	void TriggerTx()
	{
		fTxPulse = 1.0f;
		fTxActive = true;
	}

	void AttachedToWindow()
	{
		BView::AttachedToWindow();
		Window()->SetPulseRate(100000);	// 100ms
	}

	void Pulse()
	{
		if (fTxActive) {
			fTxPulse -= 0.05f;
			if (fTxPulse <= 0.0f) {
				fTxPulse = 0.0f;
				fTxActive = false;
			}
			Invalidate();
		}
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		float cx = bounds.Width() / 2.0f;
		float cy = bounds.Height() / 2.0f;
		BPoint center(cx, cy);

		rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
		SetHighColor(bg);
		FillRect(bounds);

		// Draw concentric arcs that pulse outward on TX
		rgb_color arcColor = tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_LIGHTEN_2_TINT);

		for (int i = 0; i < 3; i++) {
			float baseR = 12.0f + i * 10.0f;
			float r = baseR;
			uint8 alpha = 180;

			if (fTxActive) {
				float phase = fTxPulse - i * 0.15f;
				if (phase < 0) phase = 0;
				r = baseR + phase * 8.0f;
				alpha = (uint8)(phase * 200);
			}

			rgb_color c = arcColor;
			c.alpha = alpha;
			SetHighColor(c);
			SetPenSize(2.0f);
			// Draw arcs as partial ellipses (upper half)
			StrokeArc(center, r, r * 0.7f, 200, 140);
		}

		// Antenna icon at center
		rgb_color antennaColor = ui_color(B_PANEL_TEXT_COLOR);
		SetHighColor(antennaColor);
		SetPenSize(2.0f);

		// Vertical line
		StrokeLine(BPoint(cx, cy + 12), BPoint(cx, cy - 4));
		// Triangle top
		StrokeLine(BPoint(cx - 5, cy - 2), BPoint(cx, cy - 10));
		StrokeLine(BPoint(cx, cy - 10), BPoint(cx + 5, cy - 2));
		StrokeLine(BPoint(cx - 5, cy - 2), BPoint(cx + 5, cy - 2));
		// Small dot at tip
		FillEllipse(BPoint(cx, cy - 12), 2, 2);

		SetPenSize(1.0f);
	}

private:
	float	fTxPulse;
	bool	fTxActive;
};


// ============================================================================
// StatusView — top status bar with PTY path, connection dot, LEDs
// ============================================================================

class StatusView : public BView {
public:
	StatusView()
		:
		BView("status_bar", B_WILL_DRAW | B_PULSE_NEEDED
			| B_FULL_UPDATE_ON_RESIZE),
		fConnected(false),
		fTxCount(0),
		fRxCount(0),
		fTxFlashTime(0),
		fRxFlashTime(0),
		fPulsePhase(0.0f),
		fStartTime(system_time())
	{
		SetViewUIColor(B_MENU_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(400, 36));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 36));
		memset(fPtyPath, 0, sizeof(fPtyPath));
	}

	void SetPtyPath(const char* path)
	{
		strlcpy(fPtyPath, path, sizeof(fPtyPath));
		Invalidate();
	}

	void SetConnected(bool c)
	{
		fConnected = c;
		Invalidate();
	}

	void FlashTx()
	{
		fTxCount++;
		fTxFlashTime = system_time();
		Invalidate();
	}

	void FlashRx()
	{
		fRxCount++;
		fRxFlashTime = system_time();
		Invalidate();
	}

	int32 TxCount() const { return fTxCount; }
	int32 RxCount() const { return fRxCount; }

	void AttachedToWindow()
	{
		BView::AttachedToWindow();
		Window()->SetPulseRate(200000);	// 200ms
	}

	void Pulse()
	{
		fPulsePhase += 0.15f;
		if (fPulsePhase > 2.0f * M_PI)
			fPulsePhase -= 2.0f * M_PI;
		Invalidate();
	}

	void MouseDown(BPoint where)
	{
		// Check if click is on the PTY path text area (first 250px)
		if (where.x < 260.0f) {
			BMessage msg(MSG_COPY_PTY_PATH);
			Window()->PostMessage(&msg);
		}
	}

	void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		bigtime_t now = system_time();

		rgb_color bg = ui_color(B_MENU_BACKGROUND_COLOR);
		rgb_color text = ui_color(B_MENU_ITEM_TEXT_COLOR);
		SetHighColor(bg);
		FillRect(bounds);

		BFont monoFont(be_fixed_font);
		monoFont.SetSize(11);
		SetFont(&monoFont);

		float y = bounds.Height() / 2.0f + 4.0f;
		float x = 8.0f;

		// PTY path
		SetHighColor(tint_color(text, B_LIGHTEN_1_TINT));
		DrawString(fPtyPath, BPoint(x, y));
		x += monoFont.StringWidth(fPtyPath) + 12.0f;

		// Connection dot — pulsing green or static red
		float dotR = 5.0f;
		float dotY = bounds.Height() / 2.0f;
		if (fConnected) {
			float pulse = 0.6f + 0.4f * sinf(fPulsePhase);
			rgb_color green = {0, (uint8)(200 * pulse), 0, 255};
			SetHighColor(green);
		} else {
			rgb_color red = {180, 40, 40, 255};
			SetHighColor(red);
		}
		FillEllipse(BPoint(x + dotR, dotY), dotR, dotR);
		x += dotR * 2 + 8.0f;

		// Status text
		SetHighColor(text);
		DrawString(fConnected ? "Connected" : "Waiting...", BPoint(x, y));
		x += monoFont.StringWidth("Connected  ") + 8.0f;

		// TX LED
		bool txOn = (now - fTxFlashTime) < kLedFlashDuration;
		rgb_color txColor = txOn
			? (rgb_color){100, 180, 255, 255}
			: tint_color(bg, B_DARKEN_2_TINT);
		SetHighColor(txColor);
		FillEllipse(BPoint(x + 4, dotY), 4, 4);
		x += 12.0f;
		SetHighColor(text);
		DrawString("TX", BPoint(x, y));
		x += monoFont.StringWidth("TX") + 10.0f;

		// RX LED
		bool rxOn = (now - fRxFlashTime) < kLedFlashDuration;
		rgb_color rxColor = rxOn
			? (rgb_color){255, 160, 40, 255}
			: tint_color(bg, B_DARKEN_2_TINT);
		SetHighColor(rxColor);
		FillEllipse(BPoint(x + 4, dotY), 4, 4);
		x += 12.0f;
		SetHighColor(text);
		DrawString("RX", BPoint(x, y));
		x += monoFont.StringWidth("RX") + 16.0f;

		// Uptime
		int32 uptimeSec = (int32)((now - fStartTime) / 1000000);
		int32 hours = uptimeSec / 3600;
		int32 mins = (uptimeSec % 3600) / 60;
		int32 secs = uptimeSec % 60;
		char uptimeStr[32];
		snprintf(uptimeStr, sizeof(uptimeStr), "Up %d:%02d:%02d",
			(int)hours, (int)mins, (int)secs);
		DrawString(uptimeStr, BPoint(x, y));
		x += monoFont.StringWidth(uptimeStr) + 16.0f;

		// Frame counters
		char cntStr[48];
		snprintf(cntStr, sizeof(cntStr), "TX:%d RX:%d",
			(int)fTxCount, (int)fRxCount);
		DrawString(cntStr, BPoint(x, y));

		// Bottom separator line
		rgb_color sep = tint_color(bg, B_DARKEN_2_TINT);
		SetHighColor(sep);
		StrokeLine(BPoint(0, bounds.bottom), BPoint(bounds.right, bounds.bottom));
	}

private:
	bool		fConnected;
	int32		fTxCount;
	int32		fRxCount;
	bigtime_t	fTxFlashTime;
	bigtime_t	fRxFlashTime;
	float		fPulsePhase;
	bigtime_t	fStartTime;
	char		fPtyPath[128];
};


// ============================================================================
// PTYWorker — reads frames from PTY in a thread, posts to window
// ============================================================================

class PTYWorker : public BLooper {
public:
	PTYWorker(int masterFd, BHandler* target)
		:
		BLooper("PTYWorker"),
		fMasterFd(masterFd),
		fTarget(target),
		fReadThread(-1),
		fRunning(false)
	{
	}

	~PTYWorker()
	{
		StopReading();
	}

	void StartReading()
	{
		fRunning = true;
		fReadThread = spawn_thread(_ReadThread, "pty_reader",
			B_NORMAL_PRIORITY, this);
		if (fReadThread >= 0)
			resume_thread(fReadThread);
	}

	void StopReading()
	{
		fRunning = false;
		if (fReadThread >= 0) {
			status_t result;
			wait_for_thread(fReadThread, &result);
			fReadThread = -1;
		}
	}

	void MessageReceived(BMessage* msg)
	{
		switch (msg->what) {
			case MSG_WRITE_FRAME:
			{
				const void* data;
				ssize_t size;
				if (msg->FindData("payload", B_RAW_TYPE, &data, &size) == B_OK
					&& size > 0) {
					uint8_t header[3];
					header[0] = kMarkerOut;
					header[1] = size & 0xFF;
					header[2] = (size >> 8) & 0xFF;
					write(fMasterFd, header, 3);
					write(fMasterFd, data, size);
				}
				break;
			}
			default:
				BLooper::MessageReceived(msg);
				break;
		}
	}

	int MasterFd() const { return fMasterFd; }

private:
	static int32 _ReadThread(void* data)
	{
		((PTYWorker*)data)->_ReadLoop();
		return 0;
	}

	void _ReadLoop()
	{
		uint8_t payload[512];

		while (fRunning) {
			// Wait for marker byte
			uint8_t b;
			int n = read(fMasterFd, &b, 1);
			if (n <= 0) {
				usleep(5000);
				continue;
			}
			if (b != kMarkerIn)
				continue;

			// Read 2 length bytes
			uint8_t lenBytes[2];
			int got = 0;
			while (got < 2 && fRunning) {
				n = read(fMasterFd, lenBytes + got, 2 - got);
				if (n > 0) got += n;
				else usleep(1000);
			}
			if (!fRunning) break;

			int payloadLen = lenBytes[0] | (lenBytes[1] << 8);
			if (payloadLen > (int)sizeof(payload))
				payloadLen = sizeof(payload);

			// Read payload
			got = 0;
			while (got < payloadLen && fRunning) {
				n = read(fMasterFd, payload + got, payloadLen - got);
				if (n > 0) got += n;
				else usleep(1000);
			}
			if (!fRunning) break;

			// Post to window
			BMessage msg(MSG_PTY_FRAME_RX);
			msg.AddData("payload", B_RAW_TYPE, payload, payloadLen);
			BMessenger(fTarget).SendMessage(&msg);
		}
	}

	int				fMasterFd;
	BHandler*		fTarget;
	thread_id		fReadThread;
	volatile bool	fRunning;
};


// ============================================================================
// FakeRadioWindow
// ============================================================================

class FakeRadioWindow : public BWindow {
public:
	FakeRadioWindow(int masterFd, const char* ptyPath);
	~FakeRadioWindow();

	void MessageReceived(BMessage* msg);
	bool QuitRequested();

private:
	void			_BuildDeviceTab(BView* parent);
	void			_BuildContactsTab(BView* parent);
	void			_BuildInjectTab(BView* parent);

	void			_HandleIncomingFrame(BMessage* msg);
	void			_InjectDM();
	void			_InjectChannel();
	void			_DoAutoMessage();
	void			_SendConfirmed();
	void			_CopyPtyPath();

	void			_SendImageDM();
	void			_SendAudioDM();
	void			_SendNextMediaFragment();

	void			_LogEntry(const char* tag, rgb_color color,
						const char* fmt, ...);
	void			_AppendStyledText(const char* text, rgb_color color);
	void			_PruneLog();

	// UI elements
	StatusView*		fStatusView;
	RadioVizView*	fRadioViz;
	BTextView*		fLogView;
	BScrollView*	fLogScroll;

	// Device tab
	BTextControl*	fNodeNameControl;
	BMenuField*		fDeviceTypeField;
	BSlider*		fBatterySlider;
	BTextControl*	fLatControl;
	BTextControl*	fLonControl;
	BTextControl*	fFreqControl;
	BTextControl*	fBwControl;
	BTextControl*	fSfControl;
	BTextControl*	fCrControl;

	// Contacts tab
	BListView*		fContactList;
	BTextControl*	fContactNameControl;
	BMenuField*		fContactTypeField;
	BTextControl*	fContactLatControl;
	BTextControl*	fContactLonControl;

	// Inject tab
	BTextControl*	fInjectTextControl;
	BSlider*		fSnrSlider;
	BCheckBox*		fAutoMsgCheck;
	BSlider*		fIntervalSlider;
	BStringView*	fAutoMsgStatus;

	// Media inject controls
	BTextControl*	fImagePathControl;
	BTextControl*	fAudioPathControl;
	BButton*		fImageBrowseBtn;
	BButton*		fAudioBrowseBtn;
	BButton*		fSendImageBtn;
	BButton*		fSendAudioBtn;
	BStringView*	fMediaStatus;
	BFilePanel*		fImageFilePanel;
	BFilePanel*		fAudioFilePanel;

	// Status bar
	BStringView*	fFrameCountLabel;
	BStringView*	fMsgCountLabel;
	BStringView*	fLastActivityLabel;

	// State
	int				fMasterFd;
	char			fPtyPath[128];
	PTYWorker*		fWorker;
	DeviceConfig	fConfig;
	SimulatedContact fContacts[32];
	int				fContactCount;
	bool			fHandshakeDone;
	BMessageRunner*	fStatusTimer;
	BMessageRunner*	fAutoMsgTimer;
	BMessageRunner*	fMediaFragmentTimer;
	MediaSendState	fMediaSend;
	int32			fDmCount;
	int32			fChCount;
	bigtime_t		fLastActivity;
	bigtime_t		fStartTime;
};


FakeRadioWindow::FakeRadioWindow(int masterFd, const char* ptyPath)
	:
	BWindow(BRect(80, 80, 880, 640), "MeshCore Radio Simulator",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fStatusView(NULL),
	fRadioViz(NULL),
	fLogView(NULL),
	fLogScroll(NULL),
	fNodeNameControl(NULL),
	fDeviceTypeField(NULL),
	fBatterySlider(NULL),
	fLatControl(NULL),
	fLonControl(NULL),
	fFreqControl(NULL),
	fBwControl(NULL),
	fSfControl(NULL),
	fCrControl(NULL),
	fContactList(NULL),
	fContactNameControl(NULL),
	fContactTypeField(NULL),
	fContactLatControl(NULL),
	fContactLonControl(NULL),
	fInjectTextControl(NULL),
	fSnrSlider(NULL),
	fAutoMsgCheck(NULL),
	fIntervalSlider(NULL),
	fAutoMsgStatus(NULL),
	fImagePathControl(NULL),
	fAudioPathControl(NULL),
	fImageBrowseBtn(NULL),
	fAudioBrowseBtn(NULL),
	fSendImageBtn(NULL),
	fSendAudioBtn(NULL),
	fMediaStatus(NULL),
	fImageFilePanel(NULL),
	fAudioFilePanel(NULL),
	fFrameCountLabel(NULL),
	fMsgCountLabel(NULL),
	fLastActivityLabel(NULL),
	fMasterFd(masterFd),
	fWorker(NULL),
	fContactCount(0),
	fHandshakeDone(false),
	fStatusTimer(NULL),
	fAutoMsgTimer(NULL),
	fMediaFragmentTimer(NULL),
	fDmCount(0),
	fChCount(0),
	fLastActivity(0),
	fStartTime(system_time())
{
	strncpy(fPtyPath, ptyPath, sizeof(fPtyPath) - 1);
	fPtyPath[sizeof(fPtyPath) - 1] = '\0';

	// Init device config
	strcpy(fConfig.nodeName, "FakeRadio-01");
	fConfig.deviceType = 1;
	fConfig.batteryMv = 3850;
	fConfig.latitude = (int32)(41.9028 * 1e6);
	fConfig.longitude = (int32)(12.4964 * 1e6);
	fConfig.freqKhz = 869618;
	fConfig.bwHz = 62500;
	fConfig.sf = 8;
	fConfig.cr = 8;
	fConfig.uptime = 0;

	// Copy default contacts
	fContactCount = kNumDefaultContacts;
	memcpy(fContacts, sDefaultContacts, sizeof(SimulatedContact) * fContactCount);

	// -- Build UI --

	// Status bar (top)
	fStatusView = new StatusView();
	fStatusView->SetPtyPath(fPtyPath);

	// Tab view (left side)
	BTabView* tabView = new BTabView("tabs", B_WIDTH_FROM_WIDEST);

	// Device tab
	BView* deviceTab = new BView("Device", 0);
	deviceTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildDeviceTab(deviceTab);
	tabView->AddTab(deviceTab, new BTab());
	tabView->TabAt(0)->SetLabel("Device");

	// Contacts tab
	BView* contactsTab = new BView("Contacts", 0);
	contactsTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildContactsTab(contactsTab);
	tabView->AddTab(contactsTab, new BTab());
	tabView->TabAt(1)->SetLabel("Contacts");

	// Inject tab
	BView* injectTab = new BView("Inject", 0);
	injectTab->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_BuildInjectTab(injectTab);
	tabView->AddTab(injectTab, new BTab());
	tabView->TabAt(2)->SetLabel("Inject");

	// Protocol log (right side)
	BRect logRect(0, 0, 300, 200);
	fLogView = new BTextView(logRect, "log_view", logRect,
		B_FOLLOW_ALL, B_WILL_DRAW);
	fLogView->MakeEditable(false);
	fLogView->MakeSelectable(true);
	fLogView->SetStylable(true);
	fLogView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(10);
	rgb_color defaultTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);
	fLogView->SetFontAndColor(&monoFont, B_FONT_ALL, &defaultTextColor);

	fLogScroll = new BScrollView("log_scroll", fLogView,
		B_FOLLOW_ALL, 0, false, true);

	// Split view: tabs | log
	BSplitView* splitView = new BSplitView(B_HORIZONTAL);

	// Status bar (bottom)
	fFrameCountLabel = new BStringView("frame_count", "Frames: TX:0 RX:0");
	fMsgCountLabel = new BStringView("msg_count", "Messages: DM:0 CH:0");
	fLastActivityLabel = new BStringView("last_activity", "Last: --");

	BFont smallFont(be_plain_font);
	smallFont.SetSize(10);
	fFrameCountLabel->SetFont(&smallFont);
	fMsgCountLabel->SetFont(&smallFont);
	fLastActivityLabel->SetFont(&smallFont);

	// Main layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fStatusView)
		.AddSplit(splitView, 0.4f)
			.Add(tabView, 0.35f)
			.Add(fLogScroll, 0.65f)
		.End()
		.AddGroup(B_HORIZONTAL)
			.SetInsets(8, 4, 8, 4)
			.Add(fFrameCountLabel)
			.AddGlue()
			.Add(fMsgCountLabel)
			.AddGlue()
			.Add(fLastActivityLabel)
		.End()
	;

	// Start PTY worker
	fWorker = new PTYWorker(masterFd, this);
	fWorker->Run();
	fWorker->StartReading();

	// Status update timer (1 second)
	fStatusTimer = new BMessageRunner(this,
		new BMessage(MSG_STATUS_TICK), 1000000);

	_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
		"Simulator started. PTY: %s", fPtyPath);
	_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
		"Waiting for Sestriere connection...");
}


FakeRadioWindow::~FakeRadioWindow()
{
	delete fStatusTimer;
	delete fAutoMsgTimer;
	delete fMediaFragmentTimer;
	delete fImageFilePanel;
	delete fAudioFilePanel;

	if (fWorker != NULL) {
		fWorker->StopReading();
		fWorker->Lock();
		fWorker->Quit();
	}
}


void
FakeRadioWindow::_BuildDeviceTab(BView* parent)
{
	fNodeNameControl = new BTextControl("node_name", "Node Name:",
		fConfig.nodeName, NULL);

	// Device type popup
	BPopUpMenu* typeMenu = new BPopUpMenu("type_menu");
	BMessage* chatMsg = new BMessage(MSG_DEVICE_TYPE_SELECTED);
	chatMsg->AddInt32("type", 1);
	typeMenu->AddItem(new BMenuItem("Chat Node", chatMsg));
	BMessage* repMsg = new BMessage(MSG_DEVICE_TYPE_SELECTED);
	repMsg->AddInt32("type", 2);
	typeMenu->AddItem(new BMenuItem("Repeater", repMsg));
	BMessage* roomMsg = new BMessage(MSG_DEVICE_TYPE_SELECTED);
	roomMsg->AddInt32("type", 3);
	typeMenu->AddItem(new BMenuItem("Room Server", roomMsg));
	typeMenu->ItemAt(0)->SetMarked(true);
	fDeviceTypeField = new BMenuField("type_field", "Type:", typeMenu);

	// Battery slider
	fBatterySlider = new BSlider("battery", "Battery (mV):",
		new BMessage(MSG_BATTERY_CHANGED), 3000, 4200, B_HORIZONTAL);
	fBatterySlider->SetValue(fConfig.batteryMv);
	fBatterySlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fBatterySlider->SetHashMarkCount(7);
	fBatterySlider->SetLimitLabels("3000", "4200");
	fBatterySlider->SetModificationMessage(new BMessage(MSG_BATTERY_CHANGED));

	// GPS
	char latStr[32], lonStr[32];
	snprintf(latStr, sizeof(latStr), "%.6f", fConfig.latitude / 1e6);
	snprintf(lonStr, sizeof(lonStr), "%.6f", fConfig.longitude / 1e6);
	fLatControl = new BTextControl("lat", "Latitude:", latStr, NULL);
	fLonControl = new BTextControl("lon", "Longitude:", lonStr, NULL);

	// Radio params
	char freqStr[16], bwStr[16], sfStr[8], crStr[8];
	snprintf(freqStr, sizeof(freqStr), "%.3f", fConfig.freqKhz / 1000.0f);
	snprintf(bwStr, sizeof(bwStr), "%.1f", fConfig.bwHz / 1000.0f);
	snprintf(sfStr, sizeof(sfStr), "%d", fConfig.sf);
	snprintf(crStr, sizeof(crStr), "%d", fConfig.cr);
	fFreqControl = new BTextControl("freq", "Freq (MHz):", freqStr, NULL);
	fBwControl = new BTextControl("bw", "BW (kHz):", bwStr, NULL);
	fSfControl = new BTextControl("sf", "SF:", sfStr, NULL);
	fCrControl = new BTextControl("cr", "CR:", crStr, NULL);

	// Radio viz
	fRadioViz = new RadioVizView();

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fRadioViz)
			.AddGroup(B_VERTICAL, 2)
				.Add(fNodeNameControl)
				.Add(fDeviceTypeField)
			.End()
		.End()
		.Add(fBatterySlider)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fLatControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fLatControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fLonControl->CreateLabelLayoutItem(), 2, 0)
			.Add(fLonControl->CreateTextViewLayoutItem(), 3, 0)
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fFreqControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fFreqControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fBwControl->CreateLabelLayoutItem(), 2, 0)
			.Add(fBwControl->CreateTextViewLayoutItem(), 3, 0)
			.Add(fSfControl->CreateLabelLayoutItem(), 0, 1)
			.Add(fSfControl->CreateTextViewLayoutItem(), 1, 1)
			.Add(fCrControl->CreateLabelLayoutItem(), 2, 1)
			.Add(fCrControl->CreateTextViewLayoutItem(), 3, 1)
		.End()
		.AddGlue()
	;
}


void
FakeRadioWindow::_BuildContactsTab(BView* parent)
{
	fContactList = new BListView("contact_list", B_SINGLE_SELECTION_LIST);
	BScrollView* listScroll = new BScrollView("contact_scroll",
		fContactList, 0, false, true);

	// Populate initial contacts
	for (int i = 0; i < fContactCount; i++)
		fContactList->AddItem(new ContactListItem(&fContacts[i]));

	fContactNameControl = new BTextControl("c_name", "Name:", "NewNode", NULL);

	BPopUpMenu* ctypeMenu = new BPopUpMenu("ctype_menu");
	BMessage* ct1 = new BMessage(MSG_CONTACT_TYPE_SELECTED);
	ct1->AddInt32("type", 1);
	ctypeMenu->AddItem(new BMenuItem("Chat", ct1));
	BMessage* ct2 = new BMessage(MSG_CONTACT_TYPE_SELECTED);
	ct2->AddInt32("type", 2);
	ctypeMenu->AddItem(new BMenuItem("Repeater", ct2));
	BMessage* ct3 = new BMessage(MSG_CONTACT_TYPE_SELECTED);
	ct3->AddInt32("type", 3);
	ctypeMenu->AddItem(new BMenuItem("Room", ct3));
	ctypeMenu->ItemAt(0)->SetMarked(true);
	fContactTypeField = new BMenuField("ctype_field", "Type:", ctypeMenu);

	fContactLatControl = new BTextControl("c_lat", "Lat:", "45.0", NULL);
	fContactLonControl = new BTextControl("c_lon", "Lon:", "7.0", NULL);

	BButton* addBtn = new BButton("add_btn", "Add Contact",
		new BMessage(MSG_ADD_CONTACT));
	BButton* removeBtn = new BButton("remove_btn", "Remove",
		new BMessage(MSG_REMOVE_CONTACT));

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(listScroll, 1.0f)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(fContactNameControl)
		.Add(fContactTypeField)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fContactLatControl->CreateLabelLayoutItem(), 0, 0)
			.Add(fContactLatControl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fContactLonControl->CreateLabelLayoutItem(), 2, 0)
			.Add(fContactLonControl->CreateTextViewLayoutItem(), 3, 0)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(addBtn)
			.Add(removeBtn)
		.End()
	;
}


void
FakeRadioWindow::_BuildInjectTab(BView* parent)
{
	fInjectTextControl = new BTextControl("inject_text", "Message:",
		"Ciao da FakeRadio!", NULL);

	fSnrSlider = new BSlider("snr", "SNR (dB):",
		new BMessage(MSG_SNR_CHANGED), -20, 15, B_HORIZONTAL);
	fSnrSlider->SetValue(8);
	fSnrSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fSnrSlider->SetHashMarkCount(8);
	fSnrSlider->SetLimitLabels("-20", "+15");

	BButton* dmBtn = new BButton("dm_btn", "Send DM",
		new BMessage(MSG_INJECT_DM));
	BButton* chBtn = new BButton("ch_btn", "Send Channel",
		new BMessage(MSG_INJECT_CHANNEL));

	fAutoMsgCheck = new BCheckBox("auto_msg", "Auto-messages",
		new BMessage(MSG_AUTO_MSG_TOGGLE));

	fIntervalSlider = new BSlider("interval", "Interval (sec):",
		new BMessage(MSG_INTERVAL_CHANGED), 5, 60, B_HORIZONTAL);
	fIntervalSlider->SetValue(10);
	fIntervalSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fIntervalSlider->SetHashMarkCount(12);
	fIntervalSlider->SetLimitLabels("5s", "60s");

	fAutoMsgStatus = new BStringView("auto_status", "Auto: Off");
	BFont smallFont(be_plain_font);
	smallFont.SetSize(10);
	fAutoMsgStatus->SetFont(&smallFont);

	// Media injection controls
	fImagePathControl = new BTextControl("img_path", "Image:", "", NULL);
	fImagePathControl->SetEnabled(false);
	fAudioPathControl = new BTextControl("aud_path", "Audio:", "", NULL);
	fAudioPathControl->SetEnabled(false);

	fImageBrowseBtn = new BButton("img_browse", "Browse",
		new BMessage(MSG_BROWSE_IMAGE));
	fAudioBrowseBtn = new BButton("aud_browse", "Browse",
		new BMessage(MSG_BROWSE_AUDIO));

	fSendImageBtn = new BButton("send_img", "Send Image DM",
		new BMessage(MSG_SEND_IMAGE_DM));
	fSendAudioBtn = new BButton("send_aud", "Send Audio DM",
		new BMessage(MSG_SEND_AUDIO_DM));

	fMediaStatus = new BStringView("media_status", "Ready");
	fMediaStatus->SetFont(&smallFont);

	// Create file panels (lazy, shown on browse click)
	BMessage imgSelMsg(MSG_IMAGE_FILE_SELECTED);
	fImageFilePanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL,
		B_FILE_NODE, false, &imgSelMsg);
	fImageFilePanel->SetTarget(this);

	BMessage audSelMsg(MSG_AUDIO_FILE_SELECTED);
	fAudioFilePanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL,
		B_FILE_NODE, false, &audSelMsg);
	fAudioFilePanel->SetTarget(this);

	BLayoutBuilder::Group<>(parent, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fInjectTextControl)
		.Add(fSnrSlider)
		.AddGroup(B_HORIZONTAL)
			.Add(dmBtn)
			.Add(chBtn)
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(fAutoMsgCheck)
		.Add(fIntervalSlider)
		.Add(fAutoMsgStatus)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL)
			.Add(fImagePathControl)
			.Add(fImageBrowseBtn)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fAudioPathControl)
			.Add(fAudioBrowseBtn)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fSendImageBtn)
			.Add(fSendAudioBtn)
		.End()
		.Add(fMediaStatus)
		.AddGlue()
	;
}


void
FakeRadioWindow::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case MSG_PTY_FRAME_RX:
			_HandleIncomingFrame(msg);
			break;

		case MSG_INJECT_DM:
			_InjectDM();
			break;

		case MSG_INJECT_CHANNEL:
			_InjectChannel();
			break;

		case MSG_AUTO_MSG_TICK:
			_DoAutoMessage();
			break;

		case MSG_AUTO_MSG_TOGGLE:
		{
			bool on = fAutoMsgCheck->Value() == B_CONTROL_ON;
			if (on) {
				int32 interval = fIntervalSlider->Value();
				bigtime_t delay = interval * 1000000LL;
				delete fAutoMsgTimer;
				fAutoMsgTimer = new BMessageRunner(this,
					new BMessage(MSG_AUTO_MSG_TICK), delay);
				char status[64];
				snprintf(status, sizeof(status), "Auto: ON (every %ds)",
					(int)interval);
				fAutoMsgStatus->SetText(status);
				_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
					"Auto-messages enabled, interval %ds", (int)interval);
			} else {
				delete fAutoMsgTimer;
				fAutoMsgTimer = NULL;
				fAutoMsgStatus->SetText("Auto: Off");
				_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
					"Auto-messages disabled");
			}
			break;
		}

		case MSG_ADD_CONTACT:
		{
			if (fContactCount >= 32)
				break;

			SimulatedContact& c = fContacts[fContactCount];
			memset(&c, 0, sizeof(SimulatedContact));

			strncpy(c.name, fContactNameControl->Text(), 63);
			c.type = 1;  // default

			// Get type from menu
			BMenuItem* item = fContactTypeField->Menu()->FindMarked();
			if (item != NULL) {
				BMessage* itemMsg = item->Message();
				if (itemMsg != NULL)
					c.type = itemMsg->FindInt32("type");
			}

			c.latitude = (int32)(atof(fContactLatControl->Text()) * 1e6);
			c.longitude = (int32)(atof(fContactLonControl->Text()) * 1e6);

			// Generate random pubkey
			srand(time(NULL) + fContactCount);
			for (int i = 0; i < 32; i++)
				c.pubKey[i] = rand() & 0xFF;

			fContactList->AddItem(new ContactListItem(&c));
			fContactCount++;

			_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
				"Contact added: %s (type=%d)", c.name, c.type);
			break;
		}

		case MSG_REMOVE_CONTACT:
		{
			int32 sel = fContactList->CurrentSelection();
			if (sel < 0 || sel >= fContactCount)
				break;

			char removedName[64];
			strncpy(removedName, fContacts[sel].name, 63);
			removedName[63] = '\0';

			// Remove from list view
			BListItem* item = fContactList->RemoveItem(sel);
			delete item;

			// Shift contacts array
			for (int i = sel; i < fContactCount - 1; i++)
				fContacts[i] = fContacts[i + 1];
			fContactCount--;

			// Update pointers in list items
			for (int32 i = 0; i < fContactList->CountItems(); i++) {
				ContactListItem* cli = (ContactListItem*)fContactList->ItemAt(i);
				// Re-create items with correct pointers
				cli->SetText(fContacts[i].name);
			}

			_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
				"Contact removed: %s", removedName);
			break;
		}

		case MSG_COPY_PTY_PATH:
			_CopyPtyPath();
			break;

		case MSG_BATTERY_CHANGED:
			fConfig.batteryMv = fBatterySlider->Value();
			break;

		case MSG_SNR_CHANGED:
			// Just updates the slider value, read on inject
			break;

		case MSG_INTERVAL_CHANGED:
		{
			if (fAutoMsgCheck->Value() == B_CONTROL_ON) {
				int32 interval = fIntervalSlider->Value();
				bigtime_t delay = interval * 1000000LL;
				delete fAutoMsgTimer;
				fAutoMsgTimer = new BMessageRunner(this,
					new BMessage(MSG_AUTO_MSG_TICK), delay);
				char status[64];
				snprintf(status, sizeof(status), "Auto: ON (every %ds)",
					(int)interval);
				fAutoMsgStatus->SetText(status);
			}
			break;
		}

		case MSG_DEVICE_TYPE_SELECTED:
		{
			int32 type;
			if (msg->FindInt32("type", &type) == B_OK)
				fConfig.deviceType = type;
			break;
		}

		case MSG_CONTACT_TYPE_SELECTED:
			// Just marks the menu item
			break;

		case MSG_STATUS_TICK:
		{
			// Update uptime in config
			fConfig.uptime = (uint32)((system_time() - fStartTime) / 1000000);

			// Read node name from control
			strncpy(fConfig.nodeName, fNodeNameControl->Text(), 63);
			fConfig.nodeName[63] = '\0';

			// Read GPS
			fConfig.latitude = (int32)(atof(fLatControl->Text()) * 1e6);
			fConfig.longitude = (int32)(atof(fLonControl->Text()) * 1e6);

			// Read radio params
			fConfig.freqKhz = (uint32)(atof(fFreqControl->Text()) * 1000.0f);
			fConfig.bwHz = (uint32)(atof(fBwControl->Text()) * 1000.0f);
			fConfig.sf = atoi(fSfControl->Text());
			fConfig.cr = atoi(fCrControl->Text());

			// Update status bar labels
			char frameStr[64];
			snprintf(frameStr, sizeof(frameStr), "Frames: TX:%d RX:%d",
				(int)fStatusView->TxCount(),
				(int)fStatusView->RxCount());
			fFrameCountLabel->SetText(frameStr);

			char msgStr[64];
			snprintf(msgStr, sizeof(msgStr), "Messages: DM:%d CH:%d",
				(int)fDmCount, (int)fChCount);
			fMsgCountLabel->SetText(msgStr);

			if (fLastActivity > 0) {
				int32 ago = (int32)((system_time() - fLastActivity) / 1000000);
				char lastStr[32];
				snprintf(lastStr, sizeof(lastStr), "Last: %ds ago", (int)ago);
				fLastActivityLabel->SetText(lastStr);
			}
			break;
		}

		case MSG_SEND_CONFIRMED_TICK:
		{
			uint8_t confirmed[1] = { 0x82 };
			SendFrame(fMasterFd, confirmed, 1);
			fStatusView->FlashTx();
			_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
				"PUSH_SEND_CONFIRMED (1 bytes)");
			break;
		}

		case MSG_BROWSE_IMAGE:
			fImageFilePanel->Show();
			break;

		case MSG_BROWSE_AUDIO:
			fAudioFilePanel->Show();
			break;

		case MSG_IMAGE_FILE_SELECTED:
		{
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				fImagePathControl->SetText(path.Path());
				_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
					"Image selected: %s", path.Path());
			}
			break;
		}

		case MSG_AUDIO_FILE_SELECTED:
		{
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				fAudioPathControl->SetText(path.Path());
				_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
					"Audio selected: %s", path.Path());
			}
			break;
		}

		case MSG_SEND_IMAGE_DM:
			_SendImageDM();
			break;

		case MSG_SEND_AUDIO_DM:
			_SendAudioDM();
			break;

		case MSG_MEDIA_FRAGMENT_TICK:
			_SendNextMediaFragment();
			// If timer failed, schedule next fragment via one-shot runner
			if (fMediaSend.active && fMediaFragmentTimer != NULL
				&& fMediaFragmentTimer->InitCheck() != B_OK) {
				BMessageRunner::StartSending(BMessenger(this),
					new BMessage(MSG_MEDIA_FRAGMENT_TICK), 150000LL, 1);
			}
			break;

		default:
			BWindow::MessageReceived(msg);
			break;
	}
}


bool
FakeRadioWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
FakeRadioWindow::_HandleIncomingFrame(BMessage* msg)
{
	const void* data;
	ssize_t size;
	if (msg->FindData("payload", B_RAW_TYPE, &data, &size) != B_OK || size < 1)
		return;

	const uint8_t* payload = (const uint8_t*)data;
	int len = (int)size;

	fStatusView->FlashRx();
	fLastActivity = system_time();

	if (!fHandshakeDone && payload[0] == 0x01) {
		fHandshakeDone = true;
		fStatusView->SetConnected(true);
	}

	// Log the received command
	_LogEntry("RX<<<", (rgb_color){255, 160, 40, 255},
		"%s (%d bytes)", CommandName(payload[0]), len);

	// Handle the command
	char logBuf[256];
	HandleCommand(fMasterFd, payload, len, fConfig, fContacts, fContactCount,
		logBuf, sizeof(logBuf));

	// Log what we sent back
	_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255}, "%s", logBuf);
	fStatusView->FlashTx();
	fRadioViz->TriggerTx();

	// Schedule PUSH_SEND_CONFIRMED for DM sends
	if (payload[0] == 0x02) {
		BMessageRunner::StartSending(this,
			new BMessage(MSG_SEND_CONFIRMED_TICK), 500000, 1);
	}
}


void
FakeRadioWindow::_InjectDM()
{
	if (fContactCount <= 0) return;

	const char* text = fInjectTextControl->Text();
	if (text == NULL || text[0] == '\0') return;

	int8_t snr = (int8_t)fSnrSlider->Value();

	// Use first contact or selected one
	int32 sel = fContactList->CurrentSelection();
	if (sel < 0) sel = 0;
	if (sel >= fContactCount) sel = 0;

	SendIncomingDM(fMasterFd, fContacts[sel], text, snr);
	fStatusView->FlashTx();
	fRadioViz->TriggerTx();
	fDmCount++;
	fLastActivity = system_time();

	_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
		"PUSH_DM_V3 from %s: \"%s\" (SNR=%d)",
		fContacts[sel].name, text, snr);
}


void
FakeRadioWindow::_InjectChannel()
{
	if (fContactCount <= 0) return;

	const char* text = fInjectTextControl->Text();
	if (text == NULL || text[0] == '\0') return;

	int8_t snr = (int8_t)fSnrSlider->Value();

	int32 sel = fContactList->CurrentSelection();
	if (sel < 0) sel = 0;
	if (sel >= fContactCount) sel = 0;

	SendChannelMsg(fMasterFd, fContacts[sel], text, snr);
	fStatusView->FlashTx();
	fRadioViz->TriggerTx();
	fChCount++;
	fLastActivity = system_time();

	_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
		"PUSH_CHANNEL_V3 from %s: \"%s\" (SNR=%d)",
		fContacts[sel].name, text, snr);
}


void
FakeRadioWindow::_DoAutoMessage()
{
	if (fContactCount <= 0) return;

	// Random contact
	int idx = rand() % fContactCount;
	// Random message
	int msgIdx = rand() % kNumAutoMessages;
	// Random SNR between -15 and +12
	int8_t snr = (int8_t)((rand() % 28) - 15);

	const char* text = kAutoMessages[msgIdx];

	// Alternate DM and channel
	if (rand() % 3 == 0) {
		SendChannelMsg(fMasterFd, fContacts[idx], text, snr);
		fChCount++;
		_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
			"[AUTO] PUSH_CHANNEL_V3 from %s: \"%s\" (SNR=%d)",
			fContacts[idx].name, text, snr);
	} else {
		SendIncomingDM(fMasterFd, fContacts[idx], text, snr);
		fDmCount++;
		_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
			"[AUTO] PUSH_DM_V3 from %s: \"%s\" (SNR=%d)",
			fContacts[idx].name, text, snr);
	}

	fStatusView->FlashTx();
	fRadioViz->TriggerTx();
	fLastActivity = system_time();
}


void
FakeRadioWindow::_SendImageDM()
{
	if (fMediaSend.active) {
		fMediaStatus->SetText("Send in progress, please wait...");
		return;
	}
	if (fContactCount <= 0) {
		fMediaStatus->SetText("No contacts available");
		return;
	}

	const char* path = fImagePathControl->Text();
	if (path == NULL || path[0] == '\0') {
		fMediaStatus->SetText("Select an image file first");
		return;
	}

	// Compress to JPEG
	uint8_t* jpegData = NULL;
	size_t jpegSize = 0;
	int32 imgW = 0, imgH = 0;

	fMediaStatus->SetText("Compressing image...");

	if (!CompressToJpeg(path, &jpegData, &jpegSize, &imgW, &imgH)) {
		fMediaStatus->SetText("Failed to compress image");
		_LogEntry("ERR  ", (rgb_color){255, 80, 80, 255},
			"Image compression failed: %s", path);
		return;
	}

	// Determine sender contact
	int32 sel = fContactList->CurrentSelection();
	if (sel < 0) sel = 0;
	if (sel >= fContactCount) sel = 0;
	SimulatedContact& contact = fContacts[sel];

	int8_t snr = (int8_t)fSnrSlider->Value();

	// Setup media send state
	fMediaSend.data = jpegData;
	fMediaSend.dataSize = jpegSize;
	fMediaSend.sessionId = (uint32_t)(rand() & 0x7FFFFFFF);
	if (fMediaSend.sessionId == 0) fMediaSend.sessionId = 1;
	fMediaSend.totalFragments = (uint8_t)((jpegSize + kMaxFragmentPayload - 1)
		/ kMaxFragmentPayload);
	fMediaSend.currentIndex = 0;
	fMediaSend.magic = kImageMagic;
	fMediaSend.formatOrMode = kImageFormatJPEG;
	fMediaSend.active = true;

	// Build IE2 envelope text
	char keyHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(keyHex + i * 2, 3, "%02x", contact.pubKey[i]);

	uint32_t ts = (uint32_t)time(NULL);
	char envelope[256];
	snprintf(envelope, sizeof(envelope),
		"IE2:%08x:%d:%d:%ld:%ld:%lu:%s:%lu",
		fMediaSend.sessionId,
		(int)kImageFormatJPEG,
		(int)fMediaSend.totalFragments,
		(long)imgW, (long)imgH,
		(unsigned long)jpegSize, keyHex,
		(unsigned long)ts);

	// Send envelope as DM
	SendIncomingDM(fMasterFd, contact, envelope, snr);
	fStatusView->FlashTx();
	fRadioViz->TriggerTx();
	fDmCount++;
	fLastActivity = system_time();

	_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
		"PUSH_DM_V3 IE2 envelope from %s (%dx%d, %lu bytes, %d frags)",
		contact.name, (int)imgW, (int)imgH,
		(unsigned long)jpegSize, (int)fMediaSend.totalFragments);

	// Send first fragment immediately (don't wait for timer)
	_SendNextMediaFragment();

	// Start fragment timer (150ms) for remaining fragments (if any left)
	if (fMediaSend.active) {
		delete fMediaFragmentTimer;
		fMediaFragmentTimer = new BMessageRunner(BMessenger(this),
			new BMessage(MSG_MEDIA_FRAGMENT_TICK), 150000LL);
		if (fMediaFragmentTimer->InitCheck() != B_OK) {
			fprintf(stderr, "ERROR: BMessageRunner InitCheck failed: %s\n",
				strerror(fMediaFragmentTimer->InitCheck()));
			// Fallback: self-post to continue
			PostMessage(MSG_MEDIA_FRAGMENT_TICK);
		}

		char status[64];
		snprintf(status, sizeof(status), "Sending image 1/%d...",
			(int)fMediaSend.totalFragments);
		fMediaStatus->SetText(status);
	}
}


void
FakeRadioWindow::_SendAudioDM()
{
	if (fMediaSend.active) {
		fMediaStatus->SetText("Send in progress, please wait...");
		return;
	}
	if (fContactCount <= 0) {
		fMediaStatus->SetText("No contacts available");
		return;
	}

	const char* path = fAudioPathControl->Text();
	if (path == NULL || path[0] == '\0') {
		fMediaStatus->SetText("Select an audio file first");
		return;
	}

	// Read audio file
	uint8_t* audioData = NULL;
	size_t audioSize = 0;
	uint32_t durSec = 0;

	fMediaStatus->SetText("Reading audio file...");

	if (!ReadAudioFile(path, &audioData, &audioSize, &durSec)) {
		fMediaStatus->SetText("Failed to read audio file");
		_LogEntry("ERR  ", (rgb_color){255, 80, 80, 255},
			"Audio read failed: %s", path);
		return;
	}

	// Determine sender contact
	int32 sel = fContactList->CurrentSelection();
	if (sel < 0) sel = 0;
	if (sel >= fContactCount) sel = 0;
	SimulatedContact& contact = fContacts[sel];

	int8_t snr = (int8_t)fSnrSlider->Value();

	// Setup media send state
	fMediaSend.data = audioData;
	fMediaSend.dataSize = audioSize;
	fMediaSend.sessionId = (uint32_t)(rand() & 0x7FFFFFFF);
	if (fMediaSend.sessionId == 0) fMediaSend.sessionId = 1;
	fMediaSend.totalFragments = (uint8_t)((audioSize + kMaxFragmentPayload - 1)
		/ kMaxFragmentPayload);
	fMediaSend.currentIndex = 0;
	fMediaSend.magic = kVoiceMagic;
	fMediaSend.formatOrMode = kVoiceMode1300;
	fMediaSend.active = true;

	// Build VE2 envelope text (all numerics in base-36)
	char keyHex[13];
	for (int i = 0; i < 6; i++)
		snprintf(keyHex + i * 2, 3, "%02x", contact.pubKey[i]);

	uint32_t ts = (uint32_t)time(NULL);
	char sidB36[16], modeB36[16], totalB36[16], durB36[16], tsB36[16];
	ToBase36(fMediaSend.sessionId, sidB36, sizeof(sidB36));
	ToBase36((uint32_t)kVoiceMode1300, modeB36, sizeof(modeB36));
	ToBase36((uint32_t)fMediaSend.totalFragments, totalB36, sizeof(totalB36));
	ToBase36(durSec, durB36, sizeof(durB36));
	ToBase36(ts, tsB36, sizeof(tsB36));

	char envelope[256];
	snprintf(envelope, sizeof(envelope), "VE2:%s:%s:%s:%s:%s:%s",
		sidB36, modeB36, totalB36, durB36, keyHex, tsB36);

	// Send envelope as DM
	SendIncomingDM(fMasterFd, contact, envelope, snr);
	fStatusView->FlashTx();
	fRadioViz->TriggerTx();
	fDmCount++;
	fLastActivity = system_time();

	_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
		"PUSH_DM_V3 VE2 envelope from %s (%lu bytes, %d frags, %ds)",
		contact.name, (unsigned long)audioSize,
		(int)fMediaSend.totalFragments, (int)durSec);

	// Send first fragment immediately (don't wait for timer)
	_SendNextMediaFragment();

	// Start fragment timer (150ms) for remaining fragments (if any left)
	if (fMediaSend.active) {
		delete fMediaFragmentTimer;
		fMediaFragmentTimer = new BMessageRunner(BMessenger(this),
			new BMessage(MSG_MEDIA_FRAGMENT_TICK), 150000LL);
		if (fMediaFragmentTimer->InitCheck() != B_OK) {
			fprintf(stderr, "ERROR: BMessageRunner InitCheck failed: %s\n",
				strerror(fMediaFragmentTimer->InitCheck()));
			PostMessage(MSG_MEDIA_FRAGMENT_TICK);
		}

		char status[64];
		snprintf(status, sizeof(status), "Sending audio 1/%d...",
			(int)fMediaSend.totalFragments);
		fMediaStatus->SetText(status);
	}
}


void
FakeRadioWindow::_SendNextMediaFragment()
{
	if (!fMediaSend.active)
		return;

	uint8_t idx = fMediaSend.currentIndex;
	uint8_t total = fMediaSend.totalFragments;
	int8_t snr = (int8_t)fSnrSlider->Value();

	fprintf(stderr, "[fake_radio] _SendNextMediaFragment: idx=%d/%d sid=%08x magic=%02x\n",
		(int)idx, (int)total, fMediaSend.sessionId, fMediaSend.magic);

	// Calculate fragment data range
	size_t offset = (size_t)idx * kMaxFragmentPayload;
	size_t remaining = (fMediaSend.dataSize > offset)
		? (fMediaSend.dataSize - offset) : 0;
	size_t fragLen = (remaining > kMaxFragmentPayload)
		? kMaxFragmentPayload : remaining;

	// Build fragment packet: [magic][sid_4LE][fmt/mode][idx][total][data...]
	size_t packetLen = kFragmentHeaderSize + fragLen;
	uint8_t packet[kFragmentHeaderSize + kMaxFragmentPayload];
	packet[0] = fMediaSend.magic;
	memcpy(packet + 1, &fMediaSend.sessionId, 4);  // LE
	packet[5] = fMediaSend.formatOrMode;
	packet[6] = idx;
	packet[7] = total;
	if (fragLen > 0)
		memcpy(packet + kFragmentHeaderSize, fMediaSend.data + offset, fragLen);

	// Wrap in PUSH_RAW_DATA and send
	SendPushRawData(fMasterFd, packet, packetLen, snr);
	fStatusView->FlashTx();
	fRadioViz->TriggerTx();
	fLastActivity = system_time();

	fprintf(stderr, "[fake_radio] Sent PUSH_RAW_DATA: %zu bytes (hdr=%zu + frag=%zu)\n",
		packetLen + 4, kFragmentHeaderSize + 4, fragLen);

	const char* typeStr = (fMediaSend.magic == kImageMagic) ? "IMG" : "AUD";
	_LogEntry("TX>>>", (rgb_color){100, 180, 255, 255},
		"PUSH_RAW_DATA %s frag %d/%d (%lu bytes)",
		typeStr, (int)(idx + 1), (int)total, (unsigned long)fragLen);

	fMediaSend.currentIndex++;

	// Update status
	if (fMediaSend.currentIndex >= total) {
		// Done!
		delete fMediaFragmentTimer;
		fMediaFragmentTimer = NULL;

		char status[128];
		if (fMediaSend.magic == kImageMagic) {
			snprintf(status, sizeof(status),
				"Image sent! (%.1f KB, %d fragments)",
				fMediaSend.dataSize / 1024.0f, (int)total);
		} else {
			uint32_t durSec = (uint32_t)(fMediaSend.dataSize / 16000);
			if (durSec == 0) durSec = 1;
			snprintf(status, sizeof(status),
				"Audio sent! (%.1f KB, %d fragments, %ds)",
				fMediaSend.dataSize / 1024.0f, (int)total, (int)durSec);
		}
		fMediaStatus->SetText(status);

		_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
			"Media transfer complete: %s", status);

		fMediaSend.Reset();
	} else {
		char status[64];
		snprintf(status, sizeof(status), "Sending %s %d/%d...",
			(fMediaSend.magic == kImageMagic) ? "image" : "audio",
			(int)(fMediaSend.currentIndex), (int)total);
		fMediaStatus->SetText(status);
	}
}


void
FakeRadioWindow::_CopyPtyPath()
{
	if (be_clipboard->Lock()) {
		be_clipboard->Clear();
		BMessage* clip = be_clipboard->Data();
		clip->AddData("text/plain", B_MIME_TYPE, fPtyPath, strlen(fPtyPath));
		be_clipboard->Commit();
		be_clipboard->Unlock();

		_LogEntry("SYS  ", (rgb_color){80, 180, 80, 255},
			"PTY path copied to clipboard: %s", fPtyPath);
	}
}


void
FakeRadioWindow::_LogEntry(const char* tag, rgb_color color,
	const char* fmt, ...)
{
	// Format timestamp
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm tmBuf;
	localtime_r(&tv.tv_sec, &tmBuf);

	char timestamp[32];
	snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d.%03d] ",
		tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
		(int)(tv.tv_usec / 1000));

	// Format message
	char msgBuf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
	va_end(args);

	// Build full line
	BString line;
	line << timestamp << tag << "  " << msgBuf << "\n";

	// Build text_run_array for colored output
	struct {
		uint32		count;
		text_run	runs[3];
	} runBuffer;

	int32 tagStart = strlen(timestamp);
	int32 tagLen = strlen(tag);
	int32 textStart = tagStart + tagLen + 2;

	rgb_color defaultColor = ui_color(B_DOCUMENT_TEXT_COLOR);
	BFont monoFont(be_fixed_font);
	monoFont.SetSize(10);

	runBuffer.count = 3;

	// Timestamp
	runBuffer.runs[0].offset = 0;
	runBuffer.runs[0].font = monoFont;
	runBuffer.runs[0].color = tint_color(defaultColor, B_LIGHTEN_1_TINT);

	// Tag (colored)
	runBuffer.runs[1].offset = tagStart;
	runBuffer.runs[1].font = monoFont;
	runBuffer.runs[1].color = color;

	// Message text
	runBuffer.runs[2].offset = textStart;
	runBuffer.runs[2].font = monoFont;
	runBuffer.runs[2].color = defaultColor;

	int32 insertOffset = fLogView->TextLength();
	fLogView->Insert(insertOffset, line.String(), line.Length(),
		(text_run_array*)&runBuffer);

	// Auto-scroll
	fLogView->ScrollToOffset(fLogView->TextLength());

	// Prune
	_PruneLog();
}


void
FakeRadioWindow::_PruneLog()
{
	// Count lines, prune if > 500
	const char* text = fLogView->Text();
	int lineCount = 0;
	for (int i = 0; text[i]; i++) {
		if (text[i] == '\n')
			lineCount++;
	}

	if (lineCount > 500) {
		// Find offset of 100th newline to remove first 100 lines
		int nlCount = 0;
		int offset = 0;
		for (int i = 0; text[i]; i++) {
			if (text[i] == '\n') {
				nlCount++;
				if (nlCount >= 100) {
					offset = i + 1;
					break;
				}
			}
		}
		if (offset > 0)
			fLogView->Delete(0, offset);
	}
}


// ============================================================================
// FakeRadioApp
// ============================================================================

class FakeRadioApp : public BApplication {
public:
	FakeRadioApp();
	void ReadyToRun();
	bool QuitRequested();

private:
	int				fMasterFd;
	char			fSlavePath[128];
	FakeRadioWindow* fWindow;
};


FakeRadioApp::FakeRadioApp()
	:
	BApplication(kAppSignature),
	fMasterFd(-1),
	fWindow(NULL)
{
	memset(fSlavePath, 0, sizeof(fSlavePath));

	// Create PTY pair
	fMasterFd = posix_openpt(O_RDWR | O_NOCTTY);
	if (fMasterFd < 0) {
		fprintf(stderr, "posix_openpt failed\n");
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	if (grantpt(fMasterFd) < 0 || unlockpt(fMasterFd) < 0) {
		fprintf(stderr, "grantpt/unlockpt failed\n");
		close(fMasterFd);
		fMasterFd = -1;
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	const char* slavePath = ptsname(fMasterFd);
	if (slavePath == NULL) {
		fprintf(stderr, "ptsname failed\n");
		close(fMasterFd);
		fMasterFd = -1;
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	strncpy(fSlavePath, slavePath, sizeof(fSlavePath) - 1);

	// Configure as raw terminal
	struct termios tio;
	tcgetattr(fMasterFd, &tio);
	cfmakeraw(&tio);
	cfsetispeed(&tio, B115200);
	cfsetospeed(&tio, B115200);
	tcsetattr(fMasterFd, TCSANOW, &tio);

	// Non-blocking reads
	int flags = fcntl(fMasterFd, F_GETFL);
	fcntl(fMasterFd, F_SETFL, flags | O_NONBLOCK);

	printf("=== MeshCore Radio Simulator (GUI) ===\n");
	printf("PTY: %s\n\n", fSlavePath);
}


void
FakeRadioApp::ReadyToRun()
{
	if (fMasterFd < 0) {
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	fWindow = new FakeRadioWindow(fMasterFd, fSlavePath);
	fWindow->Show();
}


bool
FakeRadioApp::QuitRequested()
{
	if (fMasterFd >= 0)
		close(fMasterFd);
	return true;
}


// ============================================================================
// main
// ============================================================================

int
main()
{
	setbuf(stdout, NULL);
	srand(time(NULL));

	FakeRadioApp app;
	app.Run();
	return 0;
}
