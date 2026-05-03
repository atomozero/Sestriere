/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MediaHandler.h — Handles image/voice/GIF media transfer logic
 */

#ifndef _MEDIA_HANDLER_H
#define _MEDIA_HANDLER_H

#include <Handler.h>
#include <MessageRunner.h>
#include <String.h>

#include "ImageSession.h"
#include "VoiceSession.h"

class AudioEngine;
class ChatView;
class ProtocolHandler;


// Messages posted by MediaHandler back to MainWindow
enum {
	MSG_MEDIA_LOG			= 'mdlg',	// "prefix" + "text"
	MSG_MEDIA_SEND_TEXT		= 'mdst',	// "text" + "channel" (bool)
	MSG_MEDIA_UPDATE_IMAGE	= 'mdui',	// "session_id" (uint32)
	MSG_MEDIA_UPDATE_VOICE	= 'mduv',	// "session_id" (uint32)
};

// Messages that MainWindow forwards to MediaHandler
enum {
	MSG_MEDIA_IMAGE_SELECTED	= 'mhis',
	MSG_MEDIA_GIF_SELECTED		= 'mhgs',
	MSG_MEDIA_IMAGE_FRAGMENT	= 'mhif',
	MSG_MEDIA_IMAGE_FETCH_REQ	= 'mhfr',
	MSG_MEDIA_START_IMG_FETCH	= 'mhsf',
	MSG_MEDIA_SEND_NEXT_IMG		= 'mhni',
	MSG_MEDIA_VOICE_RECORD_START = 'mhvs',
	MSG_MEDIA_VOICE_RECORD_STOP	= 'mhve',
	MSG_MEDIA_VOICE_FRAGMENT	= 'mhvf',
	MSG_MEDIA_VOICE_FETCH_REQ	= 'mhvr',
	MSG_MEDIA_START_VOICE_FETCH	= 'mhsv',
	MSG_MEDIA_SEND_NEXT_VOICE	= 'mhnv',
	MSG_MEDIA_VOICE_PLAY		= 'mhvp',
	MSG_MEDIA_VOICE_TIMEOUT		= 'mhvt',
	MSG_MEDIA_ENVELOPE_CONFIRMED = 'mhec',
};


class MediaHandler : public BHandler {
public:
							MediaHandler(BHandler* owner,
								ProtocolHandler* protocol,
								AudioEngine* audio);
	virtual					~MediaHandler();

	virtual void			MessageReceived(BMessage* message);

	// Access to session managers (MainWindow still needs these for UI)
	ImageSessionManager*	ImageSessions() const { return fImageSessions; }
	VoiceSessionManager*	VoiceSessions() const { return fVoiceSessions; }

	// State queries
	bool					IsRecording() const { return fRecordingVoice; }
	uint32					CurrentImageSendSession() const
								{ return fCurrentSendSession; }
	uint32					ImageEnvelopeSession() const
								{ return fImageEnvelopeSession; }
	uint32					VoiceEnvelopeSession() const
								{ return fVoiceEnvelopeSession; }

	// Called by MainWindow when connection state changes
	void					SetConnected(bool connected)
								{ fConnected = connected; }
	void					SetProtocol(ProtocolHandler* p)
								{ fProtocol = p; }

	// Called by MainWindow to provide context for sends
	void					SetSendContext(bool toChannel,
								const uint8* recipientKey,
								const char* publicKeyHex);

	// Voice playback PCM (owned by MediaHandler)
	int16*					VoicePlayPcm() const { return fVoicePlayPcm; }
	size_t					VoicePlayPcmSize() const
								{ return fVoicePlayPcmSize; }

private:
			void			_HandleImageSelected(BMessage* message);
			void			_HandleGifSelected(BMessage* message);
			void			_SendNextImageFragment();
			void			_HandleIncomingImageFragment(
								const uint8* payload, size_t length);
			void			_HandleIncomingFetchRequest(
								const uint8* payload, size_t length);
			void			_StartImageFetch(uint32 sessionId);

			void			_StartVoiceRecord();
			void			_StopVoiceRecord();
			void			_SendNextVoiceFragment();
			void			_HandleIncomingVoiceFragment(
								const uint8* payload, size_t length);
			void			_HandleIncomingVoiceFetch(
								const uint8* payload, size_t length);
			void			_StartVoiceFetch(uint32 sessionId);
			void			_HandleVoicePlayRequest(uint32 sessionId);

			void			_Log(const char* prefix, const char* text);
			void			_SendText(const char* text);

			BHandler*		fOwner;
			ProtocolHandler* fProtocol;
			AudioEngine*	fAudioEngine;

			// State
			bool			fConnected;
			bool			fSendingToChannel;
			uint8			fRecipientKey[32];
			char			fPublicKey[65];

			// Image sessions
			ImageSessionManager* fImageSessions;
			BMessageRunner*	fImageFragmentTimer;
			uint32			fCurrentSendSession;
			uint8			fCurrentSendIndex;
			uint32			fImageEnvelopeSession;

			// Voice sessions
			VoiceSessionManager* fVoiceSessions;
			BMessageRunner*	fVoiceFragmentTimer;
			BMessageRunner*	fVoiceRecordTimer;
			uint32			fCurrentVoiceSendSession;
			uint8			fCurrentVoiceSendIndex;
			uint32			fVoiceEnvelopeSession;
			bool			fRecordingVoice;
			int16*			fVoicePlayPcm;
			size_t			fVoicePlayPcmSize;
};


#endif // _MEDIA_HANDLER_H
