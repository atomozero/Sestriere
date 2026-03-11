/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SerialHandler.cpp — Serial communication handler using POSIX APIs
 */

#include "SerialHandler.h"

#include <Autolock.h>
#include <Directory.h>
#include <Entry.h>
#include <OS.h>
#include <Path.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <cstdio>
#include <cstring>


SerialHandler::SerialHandler(BHandler* target)
	:
	BLooper("SerialHandler"),
	fSerialFd(-1),
	fTarget(target),
	fPortName(""),
	fReadThread(-1),
	fRunning(false),
	fConnected(false),
	fRawMode(false),
	fBufferPos(0),
	fBufferLen(0),
	fFramePos(0),
	fInFrame(false),
	fExpectedFrameLen(0),
	fLineLen(0),
	fLock("serial_lock"),
	fWriteLock("write_lock")
{
	memset(fReadBuffer, 0, sizeof(fReadBuffer));
	memset(fFrameBuffer, 0, sizeof(fFrameBuffer));
	memset(fLineBuffer, 0, sizeof(fLineBuffer));
}


SerialHandler::~SerialHandler()
{
	Disconnect();
}


void
SerialHandler::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_SERIAL_CONNECT:
		{
			const char* port;
			if (message->FindString(kFieldPort, &port) == B_OK)
				Connect(port);
			break;
		}

		case MSG_SERIAL_DISCONNECT:
			Disconnect();
			break;

		case MSG_SERIAL_SEND_RAW:
		{
			const char* text;
			if (message->FindString("text", &text) == B_OK)
				SendRawText(text);
			break;
		}

		default:
			BLooper::MessageReceived(message);
			break;
	}
}


void
SerialHandler::SetTarget(BHandler* target)
{
	BAutolock lock(fLock);
	fTarget = target;
}


status_t
SerialHandler::Connect(const char* portName)
{
	BAutolock lock(fLock);

	if (fConnected)
		Disconnect();

	// Open serial port using POSIX
	fSerialFd = open(portName, O_RDWR | O_NOCTTY);
	if (fSerialFd < 0) {
		status_t err = errno;
		fprintf(stderr, "SerialHandler: Failed to open %s: %s\n",
			portName, strerror(errno));
		_NotifyError(err, "Failed to open serial port");
		return B_ERROR;
	}

	// Configure port: 115200 8N1, raw mode
	struct termios tty;
	if (tcgetattr(fSerialFd, &tty) != 0) {
		close(fSerialFd);
		fSerialFd = -1;
		_NotifyError(errno, "Failed to get terminal attributes");
		return B_ERROR;
	}

	// Set baud rate
	cfsetispeed(&tty, B115200);
	cfsetospeed(&tty, B115200);

	// Raw mode (no echo, no signals, no canonical processing)
	cfmakeraw(&tty);

	// Enable receiver, ignore modem control lines
	tty.c_cflag |= CREAD | CLOCAL;

	// 8 data bits, no parity, 1 stop bit
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;

	// No hardware flow control
	tty.c_cflag &= ~CRTSCTS;

	// Non-blocking read with timeout
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1;  // 100ms timeout

	if (tcsetattr(fSerialFd, TCSANOW, &tty) != 0) {
		close(fSerialFd);
		fSerialFd = -1;
		_NotifyError(errno, "Failed to set terminal attributes");
		return B_ERROR;
	}

	// CRITICAL: Set DTR and RTS signals for ESP32-based devices
	int modemFlags;
	if (ioctl(fSerialFd, TIOCMGET, &modemFlags) == 0) {
		modemFlags |= TIOCM_DTR | TIOCM_RTS;
		ioctl(fSerialFd, TIOCMSET, &modemFlags);
	}

	// Flush any stale data
	tcflush(fSerialFd, TCIOFLUSH);

	// Small delay to allow device to stabilize
	snooze(100000);  // 100ms

	fPortName = portName;
	fConnected = true;
	fRunning = true;

	// Reset buffers
	fBufferPos = 0;
	fBufferLen = 0;
	fFramePos = 0;
	fInFrame = false;

	// Start read thread
	fReadThread = spawn_thread(_ReadThreadEntry, "serial_reader",
		B_NORMAL_PRIORITY, this);
	if (fReadThread < 0) {
		close(fSerialFd);
		fSerialFd = -1;
		fConnected = false;
		_NotifyError(fReadThread, "Failed to create read thread");
		return fReadThread;
	}

	resume_thread(fReadThread);
	_NotifyConnected();

	return B_OK;
}


void
SerialHandler::Disconnect()
{
	thread_id threadToWait = -1;

	{
		BAutolock lock(fLock);
		if (!fConnected)
			return;

		fRunning = false;
		threadToWait = fReadThread;

		// Close fd first to unblock the read thread
		if (fSerialFd >= 0) {
			close(fSerialFd);
			fSerialFd = -1;
		}
	}

	// Wait for read thread to finish (outside the lock)
	if (threadToWait >= 0) {
		status_t exitValue;
		wait_for_thread(threadToWait, &exitValue);
	}

	{
		BAutolock lock(fLock);
		fReadThread = -1;
		fConnected = false;
		fRawMode = false;
		fPortName = "";
	}

	_NotifyDisconnected();
}


bool
SerialHandler::IsConnected() const
{
	return fConnected;
}


status_t
SerialHandler::SendFrame(const uint8* payload, size_t length)
{
	BAutolock lock(fWriteLock);

	if (!fConnected || fSerialFd < 0)
		return B_NOT_INITIALIZED;

	if (length > kMaxFramePayload)
		return B_BAD_VALUE;

	// Build frame: '<' + len_lo + len_hi + payload
	uint8 frame[kMaxFrameSize];
	frame[0] = kFrameMarkerInbound;
	frame[1] = length & 0xFF;
	frame[2] = (length >> 8) & 0xFF;
	memcpy(frame + kFrameHeaderSize, payload, length);

	size_t totalLen = kFrameHeaderSize + length;
	ssize_t written = write(fSerialFd, frame, totalLen);

	if (written < 0)
		return B_IO_ERROR;

	if ((size_t)written != totalLen)
		return B_IO_ERROR;

	// Notify about sent frame (for logging)
	_NotifyFrameSent(payload, length);

	return B_OK;
}


status_t
SerialHandler::SendRawText(const char* text)
{
	BAutolock lock(fWriteLock);

	if (!fConnected || fSerialFd < 0)
		return B_NOT_INITIALIZED;

	if (text == NULL)
		return B_BAD_VALUE;

	size_t len = strlen(text);
	if (len > 0) {
		ssize_t written = write(fSerialFd, text, len);
		if (written < 0)
			return B_IO_ERROR;
	}

	// Send \r\n line terminator
	const char* crlf = "\r\n";
	ssize_t written = write(fSerialFd, crlf, 2);
	if (written < 0)
		return B_IO_ERROR;

	return B_OK;
}


status_t
SerialHandler::ListPorts(BMessage* outPorts)
{
	if (outPorts == NULL)
		return B_BAD_VALUE;

	// Scan /dev/ports for USB serial devices
	BDirectory dir("/dev/ports");
	if (dir.InitCheck() != B_OK)
		return dir.InitCheck();

	BEntry entry;
	while (dir.GetNextEntry(&entry) == B_OK) {
		BPath path;
		if (entry.GetPath(&path) == B_OK) {
			// Filter for USB ports (typically usb*)
			const char* name = path.Leaf();
			if (strncmp(name, "usb", 3) == 0) {
				outPorts->AddString(kFieldPort, path.Path());
			}
		}
	}

	// Also check /dev/serial for virtual serial ports
	BDirectory serialDir("/dev/serial");
	if (serialDir.InitCheck() == B_OK) {
		while (serialDir.GetNextEntry(&entry) == B_OK) {
			BPath path;
			if (entry.GetPath(&path) == B_OK) {
				outPorts->AddString(kFieldPort, path.Path());
			}
		}
	}

	// Scan /dev/tt/ for PTY devices (used by fake_radio_gui simulator)
	BDirectory ptyDir("/dev/tt");
	if (ptyDir.InitCheck() == B_OK) {
		while (ptyDir.GetNextEntry(&entry) == B_OK) {
			BPath path;
			if (entry.GetPath(&path) == B_OK) {
				const char* name = path.Leaf();
				// Only include p* PTYs (posix_openpt slave side)
				if (name[0] == 'p')
					outPorts->AddString(kFieldPort, path.Path());
			}
		}
	}

	return B_OK;
}


void
SerialHandler::SetRawMode(bool raw)
{
	BAutolock lock(fLock);
	fRawMode = raw;

	// If switching to raw mode, abort any in-progress frame
	if (raw) {
		fInFrame = false;
		fFramePos = 0;
	}
}


void
SerialHandler::_ReadLoop()
{
	while (fRunning) {
		if (fSerialFd < 0)
			break;

		// Read available data
		ssize_t bytesRead = read(fSerialFd, fReadBuffer + fBufferLen,
			sizeof(fReadBuffer) - fBufferLen);

		if (bytesRead > 0) {
			fBufferLen += bytesRead;
			_ProcessBuffer();
		} else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
			// Read error
			if (fRunning) {
				_NotifyError(errno, "Serial read error");
			}
			break;
		}

		// Small delay to prevent busy-waiting
		if (bytesRead <= 0)
			snooze(10000);  // 10ms
	}
}


int32
SerialHandler::_ReadThreadEntry(void* data)
{
	SerialHandler* handler = static_cast<SerialHandler*>(data);
	handler->_ReadLoop();
	return 0;
}


void
SerialHandler::_ProcessBuffer()
{
	while (fBufferPos < fBufferLen) {
		uint8 byte = fReadBuffer[fBufferPos];

		if (!fInFrame) {
			// Looking for frame start marker '>' (skip in raw mode)
			if (byte == kFrameMarkerOutbound && !fRawMode) {
				// Flush any accumulated raw text before switching to frame mode
				if (fLineLen > 0) {
					fLineBuffer[fLineLen] = '\0';
					_NotifyRawLine(fLineBuffer);
					fLineLen = 0;
				}
				fInFrame = true;
				fFramePos = 0;
				fExpectedFrameLen = 0;
			} else {
				// Non-frame byte — accumulate as raw text line
				if (byte == '\n' || byte == '\r') {
					if (fLineLen > 0) {
						fLineBuffer[fLineLen] = '\0';
						_NotifyRawLine(fLineBuffer);
						fLineLen = 0;
					}
				} else if (byte >= 0x20 && fLineLen < sizeof(fLineBuffer) - 1) {
					// Printable character
					fLineBuffer[fLineLen++] = (char)byte;
				}
			}
			fBufferPos++;
		} else {
			// In frame, collecting data
			// After marker, we read 2 bytes: len_lo, len_hi
			if (fFramePos < 2) {
				// Still reading length bytes
				fFrameBuffer[fFramePos++] = byte;
				fBufferPos++;

				if (fFramePos == 2) {
					// Length bytes complete, calculate expected payload length
					fExpectedFrameLen = fFrameBuffer[0] |
						(fFrameBuffer[1] << 8);

					if (fExpectedFrameLen > kMaxFramePayload) {
						// Invalid frame length, reset
						fprintf(stderr, "Invalid frame length: %zu\n",
							fExpectedFrameLen);
						fInFrame = false;
						fFramePos = 0;
					}
				}
			} else {
				// Reading payload (fFramePos >= 2, so payload starts at fFrameBuffer[2])
				size_t payloadPos = fFramePos - 2;
				if (payloadPos < fExpectedFrameLen) {
					fFrameBuffer[fFramePos++] = byte;
					fBufferPos++;

					if (payloadPos + 1 == fExpectedFrameLen) {
						// Frame complete - payload starts at fFrameBuffer[2]
						_HandleCompleteFrame(
							fFrameBuffer + 2,
							fExpectedFrameLen);
						fInFrame = false;
						fFramePos = 0;
					}
				} else {
					// Shouldn't happen, reset
					fInFrame = false;
					fFramePos = 0;
				}
			}
		}
	}

	// Compact buffer if we've consumed data
	if (fBufferPos > 0) {
		if (fBufferPos < fBufferLen) {
			memmove(fReadBuffer, fReadBuffer + fBufferPos,
				fBufferLen - fBufferPos);
			fBufferLen -= fBufferPos;
		} else {
			fBufferLen = 0;
		}
		fBufferPos = 0;
	}
}


void
SerialHandler::_HandleCompleteFrame(const uint8* data, size_t length)
{
	_NotifyFrameReceived(data, length);
}


void
SerialHandler::_NotifyFrameReceived(const uint8* data, size_t length)
{
	BAutolock lock(fLock);
	if (fTarget == NULL)
		return;

	BMessage msg(MSG_FRAME_RECEIVED);
	msg.AddData(kFieldData, B_RAW_TYPE, data, length);
	msg.AddInt32(kFieldSize, length);

	BLooper* looper = fTarget->Looper();
	if (looper != NULL)
		looper->PostMessage(&msg, fTarget);
}


void
SerialHandler::_NotifyFrameSent(const uint8* data, size_t length)
{
	BAutolock lock(fLock);
	if (fTarget == NULL)
		return;

	BMessage msg(MSG_FRAME_SENT);
	msg.AddData(kFieldData, B_RAW_TYPE, data, length);
	msg.AddInt32(kFieldSize, length);

	BLooper* looper = fTarget->Looper();
	if (looper != NULL)
		looper->PostMessage(&msg, fTarget);
}


void
SerialHandler::_NotifyRawLine(const char* line)
{
	BAutolock lock(fLock);
	if (fTarget == NULL)
		return;

	BMessage msg(MSG_RAW_SERIAL_DATA);
	msg.AddString("line", line);

	BLooper* looper = fTarget->Looper();
	if (looper != NULL)
		looper->PostMessage(&msg, fTarget);
}


void
SerialHandler::_NotifyError(status_t error, const char* message)
{
	BAutolock lock(fLock);
	if (fTarget == NULL)
		return;

	BMessage msg(MSG_SERIAL_ERROR);
	msg.AddInt32(kFieldErrorCode, error);
	msg.AddString(kFieldError, message);

	BLooper* looper = fTarget->Looper();
	if (looper != NULL)
		looper->PostMessage(&msg, fTarget);
}


void
SerialHandler::_NotifyConnected()
{
	BAutolock lock(fLock);
	if (fTarget == NULL)
		return;

	BMessage msg(MSG_SERIAL_CONNECTED);
	msg.AddString(kFieldPort, fPortName.String());

	BLooper* looper = fTarget->Looper();
	if (looper != NULL)
		looper->PostMessage(&msg, fTarget);
}


void
SerialHandler::_NotifyDisconnected()
{
	BAutolock lock(fLock);
	if (fTarget == NULL)
		return;

	BMessage msg(MSG_SERIAL_DISCONNECTED);

	BLooper* looper = fTarget->Looper();
	if (looper != NULL)
		looper->PostMessage(&msg, fTarget);
}
