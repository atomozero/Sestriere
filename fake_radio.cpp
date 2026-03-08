/*
 * fake_radio.cpp — MeshCore radio simulator for Sestriere testing
 *
 * Creates a PTY pair and simulates a MeshCore companion device.
 * Connect Sestriere to the printed device path.
 *
 * Usage: ./fake_radio
 *   Then in Sestriere: Connection > Connect > select the printed /dev/tt/XX path
 *
 * After handshake, sends a simulated incoming message every 10 seconds.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

static volatile bool sRunning = true;

static void _SignalHandler(int) { sRunning = false; }


// Frame protocol constants
static const uint8_t kMarkerIn  = 0x3C;  // '<' App -> Radio
static const uint8_t kMarkerOut = 0x3E;  // '>' Radio -> App


static void
SendFrame(int fd, const uint8_t* payload, int len)
{
	uint8_t header[3];
	header[0] = kMarkerOut;
	header[1] = len & 0xFF;
	header[2] = (len >> 8) & 0xFF;
	write(fd, header, 3);
	write(fd, payload, len);
	printf("  TX frame: code=0x%02X, %d bytes\n", payload[0], len);
}


// Read a complete frame from the app. Returns payload length, or -1.
static int
ReadFrame(int fd, uint8_t* payload, int maxLen)
{
	// Read until we get a marker
	while (sRunning) {
		uint8_t b;
		int n = read(fd, &b, 1);
		if (n <= 0) {
			usleep(10000);
			continue;
		}
		if (b == kMarkerIn)
			break;
	}
	if (!sRunning) return -1;

	// Read 2 length bytes
	uint8_t lenBytes[2];
	int got = 0;
	while (got < 2 && sRunning) {
		int n = read(fd, lenBytes + got, 2 - got);
		if (n > 0) got += n;
		else usleep(1000);
	}

	int payloadLen = lenBytes[0] | (lenBytes[1] << 8);
	if (payloadLen > maxLen) payloadLen = maxLen;

	// Read payload
	got = 0;
	while (got < payloadLen && sRunning) {
		int n = read(fd, payload + got, payloadLen - got);
		if (n > 0) got += n;
		else usleep(1000);
	}

	return payloadLen;
}


// Fake sender identity
static const uint8_t kFakePubKey[32] = {
	0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A
};

static const uint8_t kSelfPubKey[32] = {
	0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0x00, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
	0xA7, 0xA8, 0xA9, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
	0xB5, 0xB6
};


static void
SendAppStartAck(int fd)
{
	// RSP_ERR [0x01, 0x03] = V3 protocol ack
	uint8_t ack[2] = { 0x01, 0x03 };
	SendFrame(fd, ack, 2);
	printf("  -> APP_START ack sent (V3)\n");
}


static void
SendSelfInfo(int fd)
{
	uint8_t rsp[128];
	memset(rsp, 0, sizeof(rsp));

	rsp[0]  = 0x05;   // RSP_SELF_INFO
	rsp[1]  = 0x01;   // Type: CHAT
	rsp[2]  = 14;     // TX Power dBm
	rsp[3]  = 20;     // Max TX Power

	// Public key at [4-35]
	memcpy(rsp + 4, kSelfPubKey, 32);

	// GPS lat/lon at [36-43] — Rome: 41.9028, 12.4964
	int32_t lat = (int32_t)(41.9028 * 1e6);
	int32_t lon = (int32_t)(12.4964 * 1e6);
	memcpy(rsp + 36, &lat, 4);
	memcpy(rsp + 40, &lon, 4);

	// Flags [44-47]
	rsp[44] = 0;  // multi_acks
	rsp[45] = 0;  // advert_loc_policy
	rsp[46] = 0;  // telemetry_modes
	rsp[47] = 0;  // manual_add_contacts

	// Radio params
	uint32_t freq_hz = 869618000;  // EU 869.618 MHz
	uint32_t bw_hz   = 62500;     // 62.5 kHz
	memcpy(rsp + 48, &freq_hz, 4);
	memcpy(rsp + 52, &bw_hz, 4);
	rsp[56] = 8;   // SF8
	rsp[57] = 8;   // CR8

	// Device name at [58+]
	const char* name = "FakeRadio-01";
	strcpy((char*)(rsp + 58), name);

	int totalLen = 58 + strlen(name) + 1;
	SendFrame(fd, rsp, totalLen);
	printf("  -> RSP_SELF_INFO sent: '%s'\n", name);
}


static void
SendDeviceInfo(int fd)
{
	// RSP_DEVICE_INFO (0x0D): [code][boardType][maxContacts*2][maxChannels]
	uint8_t rsp[4];
	rsp[0] = 0x0D;  // RSP_DEVICE_INFO
	rsp[1] = 0x01;  // Board type
	rsp[2] = 16;    // maxContacts = 16*2 = 32
	rsp[3] = 8;     // maxChannels
	SendFrame(fd, rsp, 4);
	printf("  -> RSP_DEVICE_INFO sent\n");
}


static void
SendOk(int fd)
{
	uint8_t rsp[1] = { 0x00 };  // RSP_OK
	SendFrame(fd, rsp, 1);
}


static void
SendBattAndStorage(int fd)
{
	// RSP_BATT_AND_STORAGE (0x0C): [code][battMv_lo][battMv_hi][usedKb4][totalKb4]
	uint8_t rsp[11];
	memset(rsp, 0, sizeof(rsp));
	rsp[0] = 0x0C;

	uint16_t battMv = 3850;  // 3.85V
	memcpy(rsp + 1, &battMv, 2);

	uint32_t usedKb = 128;
	uint32_t totalKb = 4096;
	memcpy(rsp + 3, &usedKb, 4);
	memcpy(rsp + 7, &totalKb, 4);

	SendFrame(fd, rsp, 11);
	printf("  -> Battery: %d mV, Storage: %u/%u KB\n", battMv, usedKb, totalKb);
}


static void
SendStats(int fd, uint8_t subtype)
{
	// RSP_STATS (0x18)
	uint8_t rsp[16];
	memset(rsp, 0, sizeof(rsp));
	rsp[0] = 0x18;  // RSP_STATS
	rsp[1] = subtype;

	if (subtype == 0) {
		// Core stats: [2-3]=battMv, [4-7]=uptime
		uint16_t battMv = 3850;
		memcpy(rsp + 2, &battMv, 2);
		uint32_t uptime = 3600;  // 1 hour
		memcpy(rsp + 4, &uptime, 4);
		SendFrame(fd, rsp, 8);
	} else if (subtype == 1) {
		// Radio stats: [2-3]=noiseFloor(int16), [4]=rssi, [5]=snr
		int16_t noiseFloor = -110;
		memcpy(rsp + 2, &noiseFloor, 2);
		rsp[4] = (uint8_t)-65;   // RSSI: -65 dBm
		rsp[5] = (uint8_t)8;     // SNR: 8 dB
		SendFrame(fd, rsp, 6);
	} else if (subtype == 2) {
		// Packet stats: [2-5]=recvPkts, [6-9]=sentPkts
		uint32_t recv = 42;
		uint32_t sent = 17;
		memcpy(rsp + 2, &recv, 4);
		memcpy(rsp + 6, &sent, 4);
		SendFrame(fd, rsp, 10);
	}
	printf("  -> Stats subtype %d sent\n", subtype);
}


static void
SendFakeContact(int fd)
{
	// RSP_CONTACTS_START (0x02)
	uint8_t start[1] = { 0x02 };
	SendFrame(fd, start, 1);

	// RSP_CONTACT (0x03): 148-byte contact frame
	// [0]=code, [1-32]=pubkey, [33]=type, [34]=flags, [35]=outPathLen,
	// [100-131]=name, [132-135]=lastSeen, [136-139]=lat, [140-143]=lon
	uint8_t contact[148];
	memset(contact, 0, sizeof(contact));
	contact[0] = 0x03;  // RSP_CONTACT

	// Pubkey at [1-32]
	memcpy(contact + 1, kFakePubKey, 32);

	// Type at [33]: 1=CHAT
	contact[33] = 0x01;

	// Flags at [34]
	contact[34] = 0x00;

	// outPathLen at [35]: 1 = direct
	contact[35] = 1;

	// Name at [100-131]
	const char* contactName = "TestUser-Simulated";
	strncpy((char*)(contact + 100), contactName, 31);

	// lastSeen at [132-135]: now
	uint32_t now = (uint32_t)time(NULL);
	memcpy(contact + 132, &now, 4);

	// GPS at [136-143]: Milan 45.4642, 9.1900
	int32_t clat = (int32_t)(45.4642 * 1e6);
	int32_t clon = (int32_t)(9.1900 * 1e6);
	memcpy(contact + 136, &clat, 4);
	memcpy(contact + 140, &clon, 4);

	SendFrame(fd, contact, 148);
	printf("  -> Contact sent: '%s'\n", contactName);

	// RSP_END_OF_CONTACTS (0x04)
	uint8_t end[1] = { 0x04 };
	SendFrame(fd, end, 1);
	printf("  -> Contacts sync complete\n");
}


static void
SendIncomingMessage(int fd, const char* text, int8_t snr)
{
	// RSP_CONTACT_MSG_RECV_V3 (0x10)
	// [0]=0x10, [1]=snr, [2]=rssi, [3]=pathLen,
	// [4-35]=pubkey, [36-39]=timestamp, [40]=txtType, [41+]=text
	int textLen = strlen(text);
	int payloadLen = 41 + textLen + 1;

	uint8_t* rsp = (uint8_t*)malloc(payloadLen);
	memset(rsp, 0, payloadLen);

	rsp[0] = 0x10;             // RSP_CONTACT_MSG_RECV_V3
	rsp[1] = (uint8_t)snr;    // SNR
	rsp[2] = (uint8_t)-65;    // RSSI
	rsp[3] = 1;               // pathLen (direct)

	memcpy(rsp + 4, kFakePubKey, 32);  // sender pubkey

	uint32_t ts = (uint32_t)time(NULL);
	memcpy(rsp + 36, &ts, 4);         // timestamp

	rsp[40] = 0;  // txt_type: normal text

	memcpy(rsp + 41, text, textLen + 1);  // text + null

	SendFrame(fd, rsp, payloadLen);
	printf("  -> Incoming DM: \"%s\" (SNR=%d)\n", text, snr);

	free(rsp);
}


static void
SendChannelMessage(int fd, const char* text, int8_t snr)
{
	// RSP_CHANNEL_MSG_RECV_V3 (0x11)
	// [0]=0x11, [1]=snr, [2]=rssi, [3]=pathLen,
	// [4-35]=pubkey, [36]=channelIdx, [37-40]=timestamp, [41]=txtType, [42+]=text
	int textLen = strlen(text);
	int payloadLen = 42 + textLen + 1;

	uint8_t* rsp = (uint8_t*)malloc(payloadLen);
	memset(rsp, 0, payloadLen);

	rsp[0] = 0x11;             // RSP_CHANNEL_MSG_RECV_V3
	rsp[1] = (uint8_t)snr;
	rsp[2] = (uint8_t)-70;    // RSSI
	rsp[3] = 2;               // pathLen (2 hops)

	memcpy(rsp + 4, kFakePubKey, 32);

	rsp[36] = 0;  // channelIdx = 0 (public)

	uint32_t ts = (uint32_t)time(NULL);
	memcpy(rsp + 37, &ts, 4);

	rsp[41] = 0;  // txt_type: normal

	memcpy(rsp + 42, text, textLen + 1);

	SendFrame(fd, rsp, payloadLen);
	printf("  -> Channel msg: \"%s\" (SNR=%d)\n", text, snr);

	free(rsp);
}


static void
HandleCommand(int fd, const uint8_t* payload, int len)
{
	if (len < 1) return;

	uint8_t cmd = payload[0];
	printf("  RX cmd: 0x%02X (%d bytes)\n", cmd, len);

	switch (cmd) {
		case 0x01:  // CMD_APP_START
			printf("  <- APP_START received (version=%d)\n",
				len > 1 ? payload[1] : 0);
			SendAppStartAck(fd);
			usleep(50000);
			SendSelfInfo(fd);
			break;

		case 0x04:  // CMD_GET_CONTACTS
			printf("  <- GET_CONTACTS\n");
			SendFakeContact(fd);
			break;

		case 0x07:  // CMD_SEND_SELF_ADVERT
			printf("  <- SEND_ADVERT\n");
			SendOk(fd);
			break;

		case 0x11:  // CMD_EXPORT_CONTACT
			printf("  <- EXPORT_CONTACT (self key)\n");
			{
				// Send RSP_EXPORT_CONTACT (0x0B)
				uint8_t rsp[64];
				memset(rsp, 0, sizeof(rsp));
				rsp[0] = 0x0B;  // RSP_EXPORT_CONTACT
				rsp[1] = 0;
				rsp[2] = 0;
				memcpy(rsp + 3, kSelfPubKey, 32);
				SendFrame(fd, rsp, 35);
			}
			break;

		case 0x14:  // CMD_GET_BATT_AND_STORAGE
			printf("  <- GET_BATT\n");
			SendBattAndStorage(fd);
			break;

		case 0x16:  // CMD_DEVICE_QUERY
			printf("  <- DEVICE_QUERY\n");
			SendDeviceInfo(fd);
			break;

		case 0x38:  // CMD_GET_STATS
		{
			uint8_t subtype = len > 1 ? payload[1] : 0;
			printf("  <- GET_STATS (subtype=%d)\n", subtype);
			SendStats(fd, subtype);
			break;
		}

		case 0x02:  // CMD_SEND_TXT_MSG
			printf("  <- SEND_TXT_MSG: \"%s\"\n",
				len > 37 ? (const char*)(payload + 37) : "???");
			// Send RSP_SENT (0x06)
			{
				uint8_t sent[1] = { 0x06 };
				SendFrame(fd, sent, 1);
			}
			// Send PUSH_SEND_CONFIRMED (0x82) after 500ms
			usleep(500000);
			{
				uint8_t confirmed[1] = { 0x82 };
				SendFrame(fd, confirmed, 1);
				printf("  -> Delivery confirmed\n");
			}
			break;

		case 0x03:  // CMD_SEND_CHANNEL_TXT_MSG
			printf("  <- SEND_CHANNEL_MSG\n");
			{
				uint8_t sent[1] = { 0x06 };
				SendFrame(fd, sent, 1);
			}
			break;

		case 0x05:  // CMD_GET_DEVICE_TIME
			printf("  <- GET_DEVICE_TIME\n");
			SendOk(fd);
			break;

		case 0x06:  // CMD_SET_DEVICE_TIME
			printf("  <- SET_DEVICE_TIME\n");
			SendOk(fd);
			break;

		case 0x0A:  // CMD_SYNC_NEXT_MESSAGE
			printf("  <- SYNC_NEXT_MSG\n");
			SendOk(fd);
			break;

		default:
			printf("  <- Unknown cmd 0x%02X, sending OK\n", cmd);
			SendOk(fd);
			break;
	}
}


int main()
{
	setbuf(stdout, NULL);  // Unbuffered output
	printf("=== Sestriere Fake Radio Simulator ===\n\n");

	// Create PTY pair
	int master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0) {
		perror("posix_openpt");
		return 1;
	}

	if (grantpt(master) < 0 || unlockpt(master) < 0) {
		perror("grantpt/unlockpt");
		close(master);
		return 1;
	}

	const char* slavePath = ptsname(master);
	if (!slavePath) {
		perror("ptsname");
		close(master);
		return 1;
	}

	// Configure master as raw terminal
	struct termios tio;
	tcgetattr(master, &tio);
	cfmakeraw(&tio);
	cfsetispeed(&tio, B115200);
	cfsetospeed(&tio, B115200);
	tcsetattr(master, TCSANOW, &tio);

	// Non-blocking reads
	int flags = fcntl(master, F_GETFL);
	fcntl(master, F_SETFL, flags | O_NONBLOCK);

	printf("Fake radio ready!\n");
	printf("Connect Sestriere to: %s\n\n", slavePath);
	printf("Waiting for APP_START handshake...\n\n");

	signal(SIGINT, _SignalHandler);

	// Main loop: handle commands + send periodic messages
	time_t lastMsg = time(NULL);
	int msgCount = 0;
	bool handshakeDone = false;

	const char* messages[] = {
		"Ciao da FakeRadio!",
		"Test messaggio #2 - tutto ok?",
		"Prova SNR negativo...",
		"Messaggio multi-hop simulato",
		"LoRa mesh funziona! 73",
		"Ultimo test, poi ripeto"
	};
	int8_t snrValues[] = { 10, 5, -3, -8, 12, 0 };
	int numMessages = 6;

	while (sRunning) {
		// Try to read a frame from the app
		uint8_t payload[512];
		int len = ReadFrame(master, payload, sizeof(payload));

		if (len > 0) {
			HandleCommand(master, payload, len);
			if (payload[0] == 0x01)
				handshakeDone = true;
		}

		// After handshake, send a message every 10 seconds
		if (handshakeDone) {
			time_t now = time(NULL);
			if (now - lastMsg >= 10) {
				int idx = msgCount % numMessages;
				printf("\n--- Sending simulated message %d ---\n", msgCount + 1);

				if (msgCount % 3 == 2) {
					// Every 3rd message on channel
					SendChannelMessage(master, messages[idx], snrValues[idx]);
				} else {
					// Direct message
					SendIncomingMessage(master, messages[idx], snrValues[idx]);
				}

				msgCount++;
				lastMsg = now;
			}
		}

		usleep(10000);  // 10ms poll
	}

	printf("\nShutting down.\n");
	close(master);
	return 0;
}
