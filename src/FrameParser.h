/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * FrameParser.h — MeshCore protocol frame decoder (BHandler)
 *
 * Receives raw serial frames from SerialHandler and posts decoded,
 * structured BMessages to its target (MainWindow). This separates
 * binary protocol parsing from application logic.
 */

#ifndef _FRAME_PARSER_H
#define _FRAME_PARSER_H

#include <Handler.h>
#include <SupportDefs.h>

class BHandler;


// Parsed frame message codes (FrameParser -> MainWindow)
enum {
	MSG_FRAME_OK				= 'fp00',
	MSG_FRAME_ERR				= 'fp01',
	MSG_FRAME_CONTACTS_START	= 'fp02',
	MSG_FRAME_CONTACT			= 'fp03',
	MSG_FRAME_CONTACTS_END		= 'fp04',
	MSG_FRAME_SELF_INFO			= 'fp05',
	MSG_FRAME_MSG_SENT			= 'fp06',
	MSG_FRAME_CONTACT_MSG		= 'fp07',
	MSG_FRAME_CHANNEL_MSG		= 'fp08',
	MSG_FRAME_CURR_TIME			= 'fp09',
	MSG_FRAME_NO_MORE_MSGS		= 'fp0a',
	MSG_FRAME_EXPORT_CONTACT	= 'fp0b',
	MSG_FRAME_BATT_STORAGE		= 'fp0c',
	MSG_FRAME_DEVICE_INFO		= 'fp0d',
	MSG_FRAME_STATS				= 'fp0e',
	MSG_FRAME_CHANNEL_INFO		= 'fp0f',
	MSG_FRAME_SEND_CONFIRMED	= 'fp10',
	MSG_FRAME_PATH_UPDATED		= 'fp11',
	MSG_FRAME_TRACE_DATA		= 'fp12',
	MSG_FRAME_TELEMETRY			= 'fp13',
	MSG_FRAME_LOGIN_RESULT		= 'fp14',
	MSG_FRAME_RAW_PACKET		= 'fp15',
	MSG_FRAME_STATUS_RESPONSE	= 'fp16',
	MSG_FRAME_ADVERT			= 'fp17',
	MSG_FRAME_MSG_WAITING		= 'fp18',
	MSG_FRAME_RAW_DATA			= 'fp19',
	MSG_FRAME_CONTROL_DATA		= 'fp1a',
	MSG_FRAME_PATH_DISCOVERY	= 'fp1b',
	MSG_FRAME_CONTACT_DELETED	= 'fp1c',
	MSG_FRAME_CONTACTS_FULL		= 'fp1d',
	MSG_FRAME_DISABLED			= 'fp1e',
	MSG_FRAME_PRIVATE_KEY		= 'fp1f',
	MSG_FRAME_SIGN_RESPONSE		= 'fp20',
	MSG_FRAME_CUSTOM_VARS		= 'fp21',
	MSG_FRAME_ADVERT_PATH		= 'fp22',
	MSG_FRAME_TUNING_PARAMS		= 'fp23',
	MSG_FRAME_AUTO_ADD_CONFIG	= 'fp24',
	MSG_FRAME_ALLOWED_REPEAT	= 'fp25',
	MSG_FRAME_UNKNOWN			= 'fp99'
};


class FrameParser : public BHandler {
public:
							FrameParser(BHandler* target);
	virtual					~FrameParser();

	virtual void			MessageReceived(BMessage* message);

			// Main entry point: decode a raw frame and post result to target
			void			ParseFrame(const uint8* data, size_t length);

private:
			void			_ParseSelfInfo(const uint8* data, size_t length);
			void			_ParseContact(const uint8* data, size_t length);
			void			_ParseContactMsg(const uint8* data, size_t length,
								bool isV3);
			void			_ParseChannelMsg(const uint8* data, size_t length,
								bool isV3);
			void			_ParseDeviceInfo(const uint8* data, size_t length);
			void			_ParseExportContact(const uint8* data,
								size_t length);
			void			_ParseBattAndStorage(const uint8* data,
								size_t length);
			void			_ParseStats(const uint8* data, size_t length);
			void			_ParseChannelInfo(const uint8* data,
								size_t length);
			void			_ParseAdvert(const uint8* data, size_t length);
			void			_ParseTraceData(const uint8* data, size_t length);
			void			_ParseTelemetry(const uint8* data, size_t length);
			void			_ParseSendConfirmed(const uint8* data,
								size_t length);
			void			_ParseMsgSent(const uint8* data, size_t length);
			void			_ParseRawData(const uint8* data, size_t length);
			void			_ParseControlData(const uint8* data,
								size_t length);
			void			_ParseLoginResult(const uint8* data,
								size_t length, bool success);
			void			_ParseStatusResponse(const uint8* data,
								size_t length);
			void			_ParseRawPacket(const uint8* data, size_t length);
			void			_ParsePathDiscovery(const uint8* data,
								size_t length);
			void			_ParseContactDeleted(const uint8* data,
								size_t length);
			void			_ParseSignResponse(const uint8* data,
								size_t length, bool isStart);
			void			_ParseCustomVars(const uint8* data, size_t length);
			void			_ParseAdvertPath(const uint8* data, size_t length);
			void			_ParseTuningParams(const uint8* data,
								size_t length);
			void			_ParseAllowedRepeatFreq(const uint8* data,
								size_t length);
			void			_ParseCurrTime(const uint8* data, size_t length);
			void			_ParseCmdErr(const uint8* data, size_t length);

			void			_PostToTarget(BMessage* message,
								const uint8* rawData = NULL,
								size_t rawLength = 0);

			BHandler*		fTarget;
};


#endif // _FRAME_PARSER_H
