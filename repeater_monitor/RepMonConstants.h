/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepMonConstants.h — Constants for standalone Repeater Monitor
 */

#ifndef REPMONCONSTANTS_H
#define REPMONCONSTANTS_H

#include <GraphicsDefs.h>
#include <SupportDefs.h>

// Serial frame constants
const uint8 kFrameMarkerInbound = '<';   // App -> Device
const uint8 kFrameMarkerOutbound = '>';  // Device -> App
const size_t kFrameHeaderSize = 3;       // marker + len_lo + len_hi
const size_t kMaxFramePayload = 512;
const size_t kMaxFrameSize = kFrameHeaderSize + kMaxFramePayload;

// Signal quality level colors
const rgb_color kColorGood = {80, 180, 80, 255};
const rgb_color kColorFair = {200, 170, 50, 255};
const rgb_color kColorPoor = {210, 120, 50, 255};
const rgb_color kColorBad = {200, 60, 60, 255};

// Message field names
const char* const kFieldPort = "port";
const char* const kFieldData = "data";
const char* const kFieldSize = "size";
const char* const kFieldError = "error";
const char* const kFieldErrorCode = "error_code";

// Application messages
enum {
	// Serial connection
	MSG_SERIAL_CONNECT = 'scon',
	MSG_SERIAL_DISCONNECT = 'sdis',
	MSG_SERIAL_CONNECTED = 'scok',
	MSG_SERIAL_DISCONNECTED = 'scdc',
	MSG_SERIAL_ERROR = 'serr',
	MSG_FRAME_RECEIVED = 'frec',
	MSG_FRAME_SENT = 'fsnt',

	// Raw serial data
	MSG_RAW_SERIAL_DATA = 'raws',

	// Serial Monitor
	MSG_SERIAL_SEND_RAW = 'srnd',
	MSG_SHOW_SERIAL_MONITOR = 'srmn',

	// Repeater packet flow (used internally by RepeaterMonitorView)
	MSG_REPEATER_PACKET_FLOW = 'rpfl',
};

#endif // REPMONCONSTANTS_H
