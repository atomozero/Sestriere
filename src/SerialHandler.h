/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SerialHandler.h — Serial communication handler using BLooper
 */

#ifndef SERIALHANDLER_H
#define SERIALHANDLER_H

#include <Handler.h>
#include <Looper.h>
#include <Locker.h>
#include <SerialPort.h>
#include <String.h>

#include "Types.h"

class SerialHandler : public BLooper {
public:
							SerialHandler(BHandler* target);
	virtual					~SerialHandler();

	virtual void			MessageReceived(BMessage* message);

			void			SetTarget(BHandler* target);

			status_t		Connect(const char* portName);
			void			Disconnect();
			bool			IsConnected() const;
			const char*		PortName() const { return fPortName.String(); }

			status_t		SendFrame(const uint8* payload, size_t length);

	static	status_t		ListPorts(BMessage* outPorts);

private:
			void			_ReadLoop();
	static	int32			_ReadThreadEntry(void* data);
			void			_ProcessBuffer();
			void			_HandleCompleteFrame(const uint8* data, size_t length);
			void			_NotifyFrameReceived(const uint8* data, size_t length);
			void			_NotifyError(status_t error, const char* message);
			void			_NotifyConnected();
			void			_NotifyDisconnected();

			BSerialPort		fSerialPort;
			BHandler*		fTarget;
			BString			fPortName;
			thread_id		fReadThread;
			volatile bool	fRunning;
			volatile bool	fConnected;

			uint8			fReadBuffer[4096];
			size_t			fBufferPos;
			size_t			fBufferLen;

			uint8			fFrameBuffer[kMaxFrameSize];
			size_t			fFramePos;
			bool			fInFrame;
			size_t			fExpectedFrameLen;

			BLocker			fLock;
			BLocker			fWriteLock;
};

#endif // SERIALHANDLER_H
