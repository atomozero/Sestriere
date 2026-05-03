/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MediaHandler.cpp — Image/voice/GIF media transfer logic
 */

#include "MediaHandler.h"

#include <Messenger.h>
#include <String.h>

#include "AudioEngine.h"
#include "Constants.h"
#include "DatabaseManager.h"
#include "ImageCodec.h"
#include "ProtocolHandler.h"
#include "VoiceCodec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


MediaHandler::MediaHandler(BHandler* owner, ProtocolHandler* protocol,
	AudioEngine* audio)
	:
	BHandler("MediaHandler"),
	fOwner(owner),
	fProtocol(protocol),
	fAudioEngine(audio),
	fConnected(false),
	fSendingToChannel(false),
	fImageSessions(new ImageSessionManager()),
	fImageFragmentTimer(NULL),
	fCurrentSendSession(0),
	fCurrentSendIndex(0),
	fImageEnvelopeSession(0),
	fVoiceSessions(new VoiceSessionManager()),
	fVoiceFragmentTimer(NULL),
	fVoiceRecordTimer(NULL),
	fCurrentVoiceSendSession(0),
	fCurrentVoiceSendIndex(0),
	fVoiceEnvelopeSession(0),
	fRecordingVoice(false),
	fVoicePlayPcm(NULL),
	fVoicePlayPcmSize(0)
{
	memset(fRecipientKey, 0, sizeof(fRecipientKey));
	memset(fPublicKey, 0, sizeof(fPublicKey));
}


MediaHandler::~MediaHandler()
{
	delete fImageSessions;
	delete fVoiceSessions;
	delete fImageFragmentTimer;
	delete fVoiceFragmentTimer;
	delete fVoiceRecordTimer;
	delete[] fVoicePlayPcm;
}


void
MediaHandler::SetSendContext(bool toChannel, const uint8* recipientKey,
	const char* publicKeyHex)
{
	fSendingToChannel = toChannel;
	if (recipientKey != NULL)
		memcpy(fRecipientKey, recipientKey, 32);
	else
		memset(fRecipientKey, 0, 32);
	if (publicKeyHex != NULL)
		strlcpy(fPublicKey, publicKeyHex, sizeof(fPublicKey));
}


void
MediaHandler::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_MEDIA_IMAGE_SELECTED:
			_HandleImageSelected(message);
			break;

		case MSG_MEDIA_GIF_SELECTED:
			_HandleGifSelected(message);
			break;

		case MSG_MEDIA_SEND_NEXT_IMG:
			_SendNextImageFragment();
			break;

		case MSG_MEDIA_IMAGE_FRAGMENT:
		{
			const void* data = NULL;
			ssize_t size = 0;
			if (message->FindData("payload", B_RAW_TYPE, &data, &size) == B_OK)
				_HandleIncomingImageFragment(
					static_cast<const uint8*>(data), (size_t)size);
			break;
		}

		case MSG_MEDIA_IMAGE_FETCH_REQ:
		{
			const void* data = NULL;
			ssize_t size = 0;
			if (message->FindData("payload", B_RAW_TYPE, &data, &size) == B_OK)
				_HandleIncomingFetchRequest(
					static_cast<const uint8*>(data), (size_t)size);
			break;
		}

		case MSG_MEDIA_START_IMG_FETCH:
		{
			uint32 sid = 0;
			if (message->FindUInt32("session_id", &sid) == B_OK)
				_StartImageFetch(sid);
			break;
		}

		case MSG_MEDIA_VOICE_RECORD_START:
			_StartVoiceRecord();
			break;

		case MSG_MEDIA_VOICE_RECORD_STOP:
			_StopVoiceRecord();
			break;

		case MSG_MEDIA_SEND_NEXT_VOICE:
			_SendNextVoiceFragment();
			break;

		case MSG_MEDIA_VOICE_FRAGMENT:
		{
			const void* data = NULL;
			ssize_t size = 0;
			if (message->FindData("payload", B_RAW_TYPE, &data, &size) == B_OK)
				_HandleIncomingVoiceFragment(
					static_cast<const uint8*>(data), (size_t)size);
			break;
		}

		case MSG_MEDIA_VOICE_FETCH_REQ:
		{
			const void* data = NULL;
			ssize_t size = 0;
			if (message->FindData("payload", B_RAW_TYPE, &data, &size) == B_OK)
				_HandleIncomingVoiceFetch(
					static_cast<const uint8*>(data), (size_t)size);
			break;
		}

		case MSG_MEDIA_START_VOICE_FETCH:
		{
			uint32 sid = 0;
			if (message->FindUInt32("session_id", &sid) == B_OK)
				_StartVoiceFetch(sid);
			break;
		}

		case MSG_MEDIA_VOICE_PLAY:
		{
			uint32 sid = 0;
			if (message->FindUInt32("session_id", &sid) == B_OK)
				_HandleVoicePlayRequest(sid);
			break;
		}

		case MSG_MEDIA_VOICE_TIMEOUT:
			_StopVoiceRecord();
			break;

		case MSG_MEDIA_ENVELOPE_CONFIRMED:
		{
			// Envelope delivery confirmed — start sending fragments
			if (fImageEnvelopeSession != 0) {
				fImageEnvelopeSession = 0;
				delete fImageFragmentTimer;
				BMessage fragMsg(MSG_MEDIA_SEND_NEXT_IMG);
				fImageFragmentTimer = new BMessageRunner(
					BMessenger(this, Looper()), &fragMsg, 4000000, -1);
				_Log("IMG", "Delivery confirmed — starting fragments");
			}
			if (fVoiceEnvelopeSession != 0) {
				fVoiceEnvelopeSession = 0;
				delete fVoiceFragmentTimer;
				BMessage fragMsg(MSG_MEDIA_SEND_NEXT_VOICE);
				fVoiceFragmentTimer = new BMessageRunner(
					BMessenger(this, Looper()), &fragMsg, 4000000, -1);
				_Log("VOICE", "Delivery confirmed — starting voice fragments");
			}
			break;
		}

		default:
			BHandler::MessageReceived(message);
			break;
	}
}


// =============================================================================
// Helper: post log message to owner
// =============================================================================

void
MediaHandler::_Log(const char* prefix, const char* text)
{
	BMessage msg(MSG_MEDIA_LOG);
	msg.AddString("prefix", prefix);
	msg.AddString("text", text);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&msg);
}


void
MediaHandler::_SendText(const char* text)
{
	BMessage msg(MSG_MEDIA_SEND_TEXT);
	msg.AddString("text", text);
	msg.AddBool("channel", fSendingToChannel);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&msg);
}


// =============================================================================
// GIF
// =============================================================================

void
MediaHandler::_HandleGifSelected(BMessage* msg)
{
	const char* gifId = NULL;
	if (msg->FindString("gif_id", &gifId) != B_OK || gifId == NULL)
		return;

	BString gifText;
	gifText.SetToFormat("g:%s", gifId);
	_SendText(gifText.String());
}


// =============================================================================
// Image sending
// =============================================================================

void
MediaHandler::_HandleImageSelected(BMessage* message)
{
	const char* path = NULL;
	if (message->FindString("path", &path) != B_OK)
		return;

	if (!fConnected) {
		_Log("WARN", "Cannot send image: not connected");
		return;
	}

	// Compress image
	uint8* jpegData = NULL;
	size_t jpegSize = 0;
	int32 w = 0, h = 0;
	status_t status = ImageCodec::CompressImageFile(path,
		&jpegData, &jpegSize, &w, &h);
	if (status != B_OK) {
		_Log("ERROR", BString().SetToFormat(
			"Failed to compress image: %s", strerror(status)).String());
		return;
	}

	_Log("IMG", BString().SetToFormat(
		"Compressed → %zd bytes (%ldx%ld color WebP)",
		jpegSize, (long)w, (long)h).String());

	// Get self key prefix for envelope
	uint8 selfKey[6];
	for (int i = 0; i < 6 && fPublicKey[i * 2] != '\0'; i++) {
		unsigned int byte;
		if (sscanf(fPublicKey + i * 2, "%2x", &byte) == 1)
			selfKey[i] = (uint8)byte;
	}

	// Create outgoing session
	uint32 sid = fImageSessions->CreateOutgoing(jpegData, jpegSize,
		w, h, selfKey);
	free(jpegData);

	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL) {
		_Log("ERROR", "Failed to create image session");
		return;
	}

	// Store recipient pubkey
	if (!fSendingToChannel)
		memcpy(session->recipientKey, fRecipientKey, 32);

	// Format and send IE2 envelope as text message
	BString envelope = ImageSessionManager::FormatEnvelope(sid,
		session->format, session->totalFragments, w, h,
		(uint32)session->jpegSize, selfKey, session->timestamp);
	_SendText(envelope.String());

	// Notify owner to update UI with session id
	BMessage notify(MSG_MEDIA_UPDATE_IMAGE);
	notify.AddUInt32("session_id", sid);
	notify.AddBool("new_outgoing", true);
	notify.AddInt32("width", w);
	notify.AddInt32("height", h);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	// Set session to LOADING so sender bubble shows progress
	session->state = IMAGE_LOADING;
	session->receivedCount = 0;

	fCurrentSendSession = sid;
	fCurrentSendIndex = 0;
	fImageEnvelopeSession = sid;

	// Fallback: start fragments after 10s
	delete fImageFragmentTimer;
	BMessage timerMsg(MSG_MEDIA_SEND_NEXT_IMG);
	fImageFragmentTimer = new BMessageRunner(
		BMessenger(this, Looper()), &timerMsg, 10000000, 1);

	_Log("IMG", BString().SetToFormat(
		"Waiting for envelope delivery before sending %d fragments "
		"(session %08x)", (int)session->totalFragments, sid).String());
}


void
MediaHandler::_SendNextImageFragment()
{
	ImageSession* session = fImageSessions->FindSession(fCurrentSendSession);
	if (session == NULL || fCurrentSendIndex >= session->totalFragments) {
		delete fImageFragmentTimer;
		fImageFragmentTimer = NULL;
		if (session != NULL) {
			session->state = IMAGE_COMPLETE;
			_Log("IMG", BString().SetToFormat(
				"All %d fragments sent for session %08x",
				(int)session->totalFragments, fCurrentSendSession).String());

			BMessage notify(MSG_MEDIA_UPDATE_IMAGE);
			notify.AddUInt32("session_id", fCurrentSendSession);
			if (fOwner != NULL && fOwner->Looper() != NULL)
				BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);
		}
		return;
	}

	uint8 packet[kImageHeaderSize + kMaxFragmentPayload];
	ImageFragment& frag = session->fragments[fCurrentSendIndex];
	size_t pktLen = ImageSessionManager::BuildFragmentPacket(packet,
		session->sessionId, session->format, fCurrentSendIndex,
		session->totalFragments, frag.data, frag.length);

	if (fProtocol == NULL || fProtocol->SendRawData(packet, pktLen) != B_OK) {
		_Log("IMG", "Send failed — aborting image transfer");
		session->state = IMAGE_FAILED;
		BMessage notify(MSG_MEDIA_UPDATE_IMAGE);
		notify.AddUInt32("session_id", fCurrentSendSession);
		if (fOwner != NULL && fOwner->Looper() != NULL)
			BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);
		delete fImageFragmentTimer;
		fImageFragmentTimer = NULL;
		return;
	}

	fCurrentSendIndex++;
	session->receivedCount = fCurrentSendIndex;
	session->state = IMAGE_LOADING;

	BMessage notify(MSG_MEDIA_UPDATE_IMAGE);
	notify.AddUInt32("session_id", fCurrentSendSession);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	// If called from fallback timer, switch to repeating
	if (fImageEnvelopeSession != 0) {
		fImageEnvelopeSession = 0;
		_Log("IMG", "Fallback: starting fragments without delivery confirmation");
		delete fImageFragmentTimer;
		BMessage fragMsg(MSG_MEDIA_SEND_NEXT_IMG);
		fImageFragmentTimer = new BMessageRunner(
			BMessenger(this, Looper()), &fragMsg, 4000000, -1);
	}
}


// =============================================================================
// Incoming image fragments
// =============================================================================

void
MediaHandler::_HandleIncomingImageFragment(const uint8* payload, size_t length)
{
	if (length < kImageHeaderSize)
		return;

	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 idx = payload[6];
	uint8 total = payload[7];

	const uint8* fragData = payload + kImageHeaderSize;
	uint16 fragLen = (uint16)(length - kImageHeaderSize);

	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL) {
		_Log("IMG", BString().SetToFormat(
			"Fragment %d/%d for unknown session %08x (waiting for envelope)",
			idx + 1, total, sid).String());
		return;
	}

	bool complete = fImageSessions->AddFragment(sid, idx, fragData, fragLen);

	_Log("IMG", BString().SetToFormat(
		"Fragment %d/%d for session %08x (%d/%d received)",
		idx + 1, total, sid,
		(int)session->receivedCount, (int)session->totalFragments).String());

	// Notify owner to update UI
	BMessage notify(MSG_MEDIA_UPDATE_IMAGE);
	notify.AddUInt32("session_id", sid);
	notify.AddBool("complete", complete);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	if (complete) {
		session->state = IMAGE_COMPLETE;
		session->Reassemble();
		_Log("IMG", BString().SetToFormat(
			"Session %08x complete, %zu bytes",
			sid, session->jpegSize).String());
	}
}


void
MediaHandler::_HandleIncomingFetchRequest(const uint8* payload, size_t length)
{
	if (length < 6)
		return;

	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 count = payload[5];

	ImageSession* session = fImageSessions->FindSession(sid);
	if (session == NULL || session->state != IMAGE_SENDING) {
		_Log("IMG", BString().SetToFormat(
			"Fetch request for unknown/non-outgoing session %08x", sid).String());
		return;
	}

	_Log("IMG", BString().SetToFormat(
		"Fetch request: %d fragments for session %08x",
		(int)count, sid).String());

	for (uint8 i = 0; i < count && (size_t)(6 + i) < length; i++) {
		uint8 idx = payload[6 + i];
		if (idx >= session->totalFragments)
			continue;

		uint8 packet[kImageHeaderSize + kMaxFragmentPayload];
		ImageFragment& frag = session->fragments[idx];
		size_t pktLen = ImageSessionManager::BuildFragmentPacket(packet,
			sid, session->format, idx, session->totalFragments,
			frag.data, frag.length);

		if (fProtocol == NULL
			|| fProtocol->SendRawData(packet, pktLen) != B_OK) {
			_Log("IMG", "Resend failed — not connected");
			break;
		}
	}
}


void
MediaHandler::_StartImageFetch(uint32 sessionId)
{
	ImageSession* session = fImageSessions->FindSession(sessionId);
	if (session == NULL)
		return;

	uint8 missing[255];
	uint8 count = 0;
	for (uint8 i = 0; i < session->totalFragments && count < 255; i++) {
		if (!session->fragments[i].received)
			missing[count++] = i;
	}

	if (count == 0) {
		_Log("IMG", "No missing fragments to fetch");
		return;
	}

	uint8 packet[6 + 255];
	size_t pktLen = ImageSessionManager::BuildFetchRequest(packet,
		sessionId, missing, count);

	if (fProtocol == NULL
		|| fProtocol->SendRawData(packet, pktLen) != B_OK) {
		_Log("IMG", "Cannot fetch: not connected");
		return;
	}

	session->state = IMAGE_LOADING;

	BMessage notify(MSG_MEDIA_UPDATE_IMAGE);
	notify.AddUInt32("session_id", sessionId);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	_Log("IMG", BString().SetToFormat(
		"Sent fetch request for %d fragments of session %08x",
		(int)count, sessionId).String());
}


// =============================================================================
// Voice recording
// =============================================================================

void
MediaHandler::_StartVoiceRecord()
{
	if (fRecordingVoice || fAudioEngine == NULL)
		return;

	status_t err = fAudioEngine->StartRecording();
	if (err != B_OK) {
		_Log("VOICE", BString().SetToFormat(
			"Failed to start recording: %s", strerror(err)).String());
		return;
	}

	fRecordingVoice = true;

	// Safety timer: auto-stop after 30 seconds
	delete fVoiceRecordTimer;
	BMessage timerMsg(MSG_MEDIA_VOICE_TIMEOUT);
	fVoiceRecordTimer = new BMessageRunner(
		BMessenger(this, Looper()), &timerMsg, 30000000, 1);

	_Log("VOICE", "Recording started");
}


void
MediaHandler::_StopVoiceRecord()
{
	if (!fRecordingVoice || fAudioEngine == NULL)
		return;

	delete fVoiceRecordTimer;
	fVoiceRecordTimer = NULL;

	int16* pcm = NULL;
	size_t sampleCount = 0;
	fAudioEngine->StopRecording(&pcm, &sampleCount);
	fRecordingVoice = false;

	if (pcm == NULL || sampleCount == 0) {
		_Log("VOICE", "No audio recorded");
		delete[] pcm;
		return;
	}

	_Log("VOICE", BString().SetToFormat(
		"Recorded %zu samples (%.1f seconds)",
		sampleCount, sampleCount / 8000.0f).String());

	// Encode to Codec2
	uint8* codec2Data = NULL;
	size_t codec2Size = 0;
	status_t err = VoiceCodec::Encode(pcm, sampleCount, VOICE_MODE_1300,
		&codec2Data, &codec2Size);
	delete[] pcm;

	if (err != B_OK || codec2Data == NULL) {
		_Log("VOICE", "Failed to encode audio");
		return;
	}

	uint32 durationSec = VoiceCodec::DurationSec(codec2Size, VOICE_MODE_1300);
	if (durationSec == 0)
		durationSec = 1;

	// Get self key prefix
	uint8 selfKey[6];
	for (int i = 0; i < 6 && fPublicKey[i * 2] != '\0'; i++) {
		unsigned int byte;
		if (sscanf(fPublicKey + i * 2, "%2x", &byte) == 1)
			selfKey[i] = (uint8)byte;
	}

	// Create outgoing session
	uint32 sid = fVoiceSessions->CreateOutgoing(codec2Data, codec2Size,
		VOICE_MODE_1300, durationSec, selfKey);
	free(codec2Data);

	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL) {
		_Log("ERROR", "Failed to create voice session");
		return;
	}

	// Store recipient pubkey
	if (!fSendingToChannel)
		memcpy(session->recipientKey, fRecipientKey, 32);

	// Format and send VE2 envelope as text
	BString envelope = VoiceSessionManager::FormatEnvelope(sid,
		VOICE_MODE_1300, session->totalFragments, durationSec,
		selfKey, session->timestamp);
	_SendText(envelope.String());

	// Notify owner to update voice UI
	BMessage notify(MSG_MEDIA_UPDATE_VOICE);
	notify.AddUInt32("session_id", sid);
	notify.AddBool("new_outgoing", true);
	notify.AddUInt32("duration", durationSec);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	fCurrentVoiceSendSession = sid;
	fCurrentVoiceSendIndex = 0;
	fVoiceEnvelopeSession = sid;

	// Fallback: start fragments after 10s
	delete fVoiceFragmentTimer;
	BMessage timerMsg(MSG_MEDIA_SEND_NEXT_VOICE);
	fVoiceFragmentTimer = new BMessageRunner(
		BMessenger(this, Looper()), &timerMsg, 10000000, 1);

	_Log("VOICE", BString().SetToFormat(
		"Waiting for envelope delivery before sending %d voice fragments "
		"(session %08x)", (int)session->totalFragments, sid).String());
}


void
MediaHandler::_SendNextVoiceFragment()
{
	VoiceSession* session = fVoiceSessions->FindSession(
		fCurrentVoiceSendSession);
	if (session == NULL
		|| fCurrentVoiceSendIndex >= session->totalFragments) {
		delete fVoiceFragmentTimer;
		fVoiceFragmentTimer = NULL;
		if (session != NULL) {
			session->state = VOICE_COMPLETE;
			_Log("VOICE", BString().SetToFormat(
				"All %d fragments sent for voice session %08x",
				(int)session->totalFragments,
				fCurrentVoiceSendSession).String());
		}
		return;
	}

	uint8 packet[kVoiceHeaderSize + kMaxVoiceFragmentPayload];
	VoiceFragment& frag = session->fragments[fCurrentVoiceSendIndex];
	size_t pktLen = VoiceSessionManager::BuildFragmentPacket(packet,
		session->sessionId, session->mode, fCurrentVoiceSendIndex,
		session->totalFragments, frag.data, frag.length);

	if (fProtocol == NULL || fProtocol->SendRawData(packet, pktLen) != B_OK) {
		_Log("VOICE", "Send failed — aborting voice transfer");
		session->state = VOICE_FAILED;
		BMessage notify(MSG_MEDIA_UPDATE_VOICE);
		notify.AddUInt32("session_id", fCurrentVoiceSendSession);
		if (fOwner != NULL && fOwner->Looper() != NULL)
			BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);
		delete fVoiceFragmentTimer;
		fVoiceFragmentTimer = NULL;
		return;
	}

	fCurrentVoiceSendIndex++;

	// If called from fallback timer, switch to repeating
	if (fVoiceEnvelopeSession != 0) {
		fVoiceEnvelopeSession = 0;
		_Log("VOICE", "Fallback: starting fragments without delivery "
			"confirmation");
		delete fVoiceFragmentTimer;
		BMessage fragMsg(MSG_MEDIA_SEND_NEXT_VOICE);
		fVoiceFragmentTimer = new BMessageRunner(
			BMessenger(this, Looper()), &fragMsg, 4000000, -1);
	}
}


// =============================================================================
// Incoming voice fragments
// =============================================================================

void
MediaHandler::_HandleIncomingVoiceFragment(const uint8* payload, size_t length)
{
	if (length < kVoiceHeaderSize)
		return;

	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 idx = payload[6];
	uint8 total = payload[7];

	const uint8* fragData = payload + kVoiceHeaderSize;
	uint16 fragLen = (uint16)(length - kVoiceHeaderSize);

	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL) {
		_Log("VOICE", BString().SetToFormat(
			"Fragment %d/%d for unknown voice session %08x",
			idx + 1, total, sid).String());
		return;
	}

	bool complete = fVoiceSessions->AddFragment(sid, idx, fragData, fragLen);

	_Log("VOICE", BString().SetToFormat(
		"Voice fragment %d/%d for session %08x (%d/%d received)",
		idx + 1, total, sid,
		(int)session->receivedCount, (int)session->totalFragments).String());

	BMessage notify(MSG_MEDIA_UPDATE_VOICE);
	notify.AddUInt32("session_id", sid);
	notify.AddBool("complete", complete);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	if (complete) {
		session->state = VOICE_COMPLETE;
		session->Reassemble();
		_Log("VOICE", BString().SetToFormat(
			"Voice session %08x complete, %zu bytes Codec2",
			sid, session->codec2Size).String());
	}
}


void
MediaHandler::_HandleIncomingVoiceFetch(const uint8* payload, size_t length)
{
	if (length < 17)
		return;

	uint32 sid = (uint32)payload[1]
		| ((uint32)payload[2] << 8)
		| ((uint32)payload[3] << 16)
		| ((uint32)payload[4] << 24);
	uint8 count = payload[16];

	VoiceSession* session = fVoiceSessions->FindSession(sid);
	if (session == NULL || session->state != VOICE_SENDING) {
		_Log("VOICE", BString().SetToFormat(
			"Voice fetch for unknown/non-outgoing session %08x", sid).String());
		return;
	}

	_Log("VOICE", BString().SetToFormat(
		"Voice fetch request: %d fragments for session %08x",
		(int)count, sid).String());

	for (uint8 i = 0; i < count && (size_t)(17 + i) < length; i++) {
		uint8 idx = payload[17 + i];
		if (idx >= session->totalFragments)
			continue;

		uint8 packet[kVoiceHeaderSize + kMaxVoiceFragmentPayload];
		VoiceFragment& frag = session->fragments[idx];
		size_t pktLen = VoiceSessionManager::BuildFragmentPacket(packet,
			sid, session->mode, idx, session->totalFragments,
			frag.data, frag.length);

		if (fProtocol == NULL
			|| fProtocol->SendRawData(packet, pktLen) != B_OK) {
			_Log("VOICE", "Resend failed — not connected");
			break;
		}
	}
}


void
MediaHandler::_StartVoiceFetch(uint32 sessionId)
{
	VoiceSession* session = fVoiceSessions->FindSession(sessionId);
	if (session == NULL)
		return;

	uint8 missing[255];
	uint8 count = 0;
	for (uint8 i = 0; i < session->totalFragments && count < 255; i++) {
		if (!session->fragments[i].received)
			missing[count++] = i;
	}

	if (count == 0) {
		_Log("VOICE", "No missing voice fragments to fetch");
		return;
	}

	uint8 packet[17 + 255];
	size_t pktLen = VoiceSessionManager::BuildFetchRequest(packet,
		sessionId, 1, session->senderKey, session->timestamp,
		missing, count);

	if (fProtocol == NULL
		|| fProtocol->SendRawData(packet, pktLen) != B_OK) {
		_Log("VOICE", "Cannot fetch: not connected");
		return;
	}

	session->state = VOICE_LOADING;

	BMessage notify(MSG_MEDIA_UPDATE_VOICE);
	notify.AddUInt32("session_id", sessionId);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&notify);

	_Log("VOICE", BString().SetToFormat(
		"Sent voice fetch request for %d fragments of session %08x",
		(int)count, sessionId).String());
}


// =============================================================================
// Voice playback
// =============================================================================

void
MediaHandler::_HandleVoicePlayRequest(uint32 sessionId)
{
	VoiceSession* session = fVoiceSessions->FindSession(sessionId);

	if (session == NULL || session->state != VOICE_COMPLETE) {
		if (session != NULL && session->state == VOICE_PENDING) {
			_StartVoiceFetch(sessionId);
			return;
		}

		// Try loading from database
		uint8* c2Data = NULL;
		size_t c2Size = 0;
		uint32 dur = 0;
		uint8 mode = 0;
		if (DatabaseManager::Instance()->LoadVoiceClip(sessionId,
			&c2Data, &c2Size, &dur, &mode)) {
			int16* pcm = NULL;
			size_t pcmSamples = 0;
			if (VoiceCodec::Decode(c2Data, c2Size,
				(VoicePacketMode)mode, &pcm, &pcmSamples) == B_OK) {
				delete[] fVoicePlayPcm;
				fVoicePlayPcm = pcm;
				fVoicePlayPcmSize = pcmSamples;
				fAudioEngine->Play(fVoicePlayPcm, fVoicePlayPcmSize,
					fOwner);
			}
			free(c2Data);
			return;
		}

		_Log("VOICE", "Voice clip not available");
		return;
	}

	// Session is complete — decode and play
	if (session->codec2Data == NULL || session->codec2Size == 0) {
		if (!session->Reassemble()) {
			_Log("VOICE", "Failed to reassemble voice data");
			return;
		}
	}

	int16* pcm = NULL;
	size_t pcmSamples = 0;
	if (VoiceCodec::Decode(session->codec2Data, session->codec2Size,
		session->mode, &pcm, &pcmSamples) != B_OK) {
		_Log("VOICE", "Failed to decode Codec2 data");
		return;
	}

	delete[] fVoicePlayPcm;
	fVoicePlayPcm = pcm;
	fVoicePlayPcmSize = pcmSamples;

	fAudioEngine->Play(fVoicePlayPcm, fVoicePlayPcmSize, fOwner);
	_Log("VOICE", BString().SetToFormat(
		"Playing voice session %08x (%zu samples)",
		sessionId, pcmSamples).String());
}
