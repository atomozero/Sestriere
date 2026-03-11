/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SerialHandler.h — Serial communication handler using POSIX APIs
 */

#ifndef SERIALHANDLER_H
#define SERIALHANDLER_H

#include <Handler.h>
#include <Locker.h>
#include <Looper.h>
#include <String.h>

#include "RepMonConstants.h"

class BMessage;

class SerialHandler : public BLooper {
public:
							SerialHandler(BHandler* target);
	virtual					~SerialHandler();

	virtual void			MessageReceived(BMessage* message);

			void			SetTarget(BHandler* target);

			status_t		Connect(const char* portName);
			void			Disconnect();
			bool			IsConnected() const;

			status_t		SendFrame(const uint8* payload, size_t length);
			status_t		SendRawText(const char* text);
	static	status_t		ListPorts(BMessage* outPorts);

			void			SetRawMode(bool raw);
			bool			IsRawMode() const { return fRawMode; }

private:
			void			_ReadLoop();
	static	int32			_ReadThreadEntry(void* data);

			void			_ProcessBuffer();
			void			_HandleCompleteFrame(const uint8* data, size_t length);

			void			_NotifyFrameReceived(const uint8* data, size_t length);
			void			_NotifyFrameSent(const uint8* data, size_t length);
			void			_NotifyRawLine(const char* line);
			void			_NotifyError(status_t error, const char* message);
			void			_NotifyConnected();
			void			_NotifyDisconnected();

			int				fSerialFd;
			BHandler*		fTarget;
			BString			fPortName;

			thread_id		fReadThread;
			volatile bool	fRunning;
			volatile bool	fConnected;
			volatile bool	fRawMode;

			// Read buffer
			uint8			fReadBuffer[4096];
			size_t			fBufferPos;
			size_t			fBufferLen;

			// Frame assembly
			uint8			fFrameBuffer[kMaxFrameSize];
			size_t			fFramePos;
			bool			fInFrame;
			size_t			fExpectedFrameLen;

			// Raw text line buffer (for non-protocol serial output)
			char			fLineBuffer[512];
			size_t			fLineLen;

			BLocker			fLock;
			BLocker			fWriteLock;
};

#endif // SERIALHANDLER_H
