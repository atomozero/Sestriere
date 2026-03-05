/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SarMarker.cpp — SAR marker parsing and formatting implementation
 */

#include "SarMarker.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>


// 8 SAR colors matching meshcore-sar palette
static const rgb_color kSarColors[8] = {
	{230,  70,  70, 255},	// 0: Red
	{240, 160,  40, 255},	// 1: Orange
	{240, 220,  60, 255},	// 2: Yellow
	{ 80, 190,  80, 255},	// 3: Green
	{ 70, 150, 230, 255},	// 4: Blue
	{150,  90, 220, 255},	// 5: Purple
	{200, 200, 200, 255},	// 6: White/Light gray
	{ 60,  60,  60, 255},	// 7: Black/Dark gray
};


static SarMarkerType
_EmojiToType(const char* emoji)
{
	// Compare UTF-8 emoji sequences
	// Person: U+1F9D1 = F0 9F A7 91
	if (strncmp(emoji, "\xF0\x9F\xA7\x91", 4) == 0)
		return SAR_PERSON;
	// Fire: U+1F525 = F0 9F 94 A5
	if (strncmp(emoji, "\xF0\x9F\x94\xA5", 4) == 0)
		return SAR_FIRE;
	// Staging/Tent: U+1F3D5 = F0 9F 8F 95 (+ optional VS16 EF B8 8F)
	if (strncmp(emoji, "\xF0\x9F\x8F\x95", 4) == 0)
		return SAR_STAGING;
	// Object/Package: U+1F4E6 = F0 9F 93 A6
	if (strncmp(emoji, "\xF0\x9F\x93\xA6", 4) == 0)
		return SAR_OBJECT;
	// Unknown/Question: U+2753 = E2 9D 93
	if (strncmp(emoji, "\xE2\x9D\x93", 3) == 0)
		return SAR_UNKNOWN;
	return SAR_UNKNOWN;
}


bool
ParseSarMarker(const char* text, SarMarker& out)
{
	out.isValid = false;

	if (text == NULL || text[0] != 'S' || text[1] != ':')
		return false;

	const char* p = text + 2;	// skip "S:"

	// Extract emoji (UTF-8 multibyte, up to next ':')
	const char* emojiStart = p;
	const char* colonAfterEmoji = strchr(p, ':');
	if (colonAfterEmoji == NULL)
		return false;

	size_t emojiLen = colonAfterEmoji - emojiStart;
	if (emojiLen == 0 || emojiLen >= sizeof(out.emoji))
		return false;

	memcpy(out.emoji, emojiStart, emojiLen);
	out.emoji[emojiLen] = '\0';
	out.type = _EmojiToType(out.emoji);

	p = colonAfterEmoji + 1;	// skip ':'

	// Determine format: new (with colorIndex) vs old (coordinates directly)
	// New format: <colorIndex>:<lat>,<lon>:<note>
	// Old format: <lat>,<lon>:<note>
	// Heuristic: if first field is a single digit and followed by ':', it's colorIndex
	bool hasColor = false;
	if (p[0] >= '0' && p[0] <= '9' && p[1] == ':') {
		hasColor = true;
	}

	if (hasColor) {
		out.colorIndex = p[0] - '0';
		if (out.colorIndex < 0 || out.colorIndex > 7)
			out.colorIndex = 0;
		p += 2;	// skip "N:"
	} else {
		out.colorIndex = 0;	// default
	}

	// Parse lat,lon
	char* endLat = NULL;
	out.lat = strtod(p, &endLat);
	if (endLat == NULL || *endLat != ',')
		return false;

	char* endLon = NULL;
	out.lon = strtod(endLat + 1, &endLon);
	if (endLon == NULL)
		return false;

	// Validate ranges
	if (out.lat < -90.0 || out.lat > 90.0)
		return false;
	if (out.lon < -180.0 || out.lon > 180.0)
		return false;

	// Optional note after ':'
	if (*endLon == ':' && *(endLon + 1) != '\0') {
		strlcpy(out.notes, endLon + 1, sizeof(out.notes));
	} else {
		out.notes[0] = '\0';
	}

	out.isValid = true;
	return true;
}


void
FormatSarMarker(char* buf, size_t size, const SarMarker& marker)
{
	if (marker.notes[0] != '\0') {
		snprintf(buf, size, "S:%s:%d:%.6f,%.6f:%s",
			marker.emoji, marker.colorIndex,
			marker.lat, marker.lon, marker.notes);
	} else {
		snprintf(buf, size, "S:%s:%d:%.6f,%.6f:",
			marker.emoji, marker.colorIndex,
			marker.lat, marker.lon);
	}
}


rgb_color
SarMarkerColor(int colorIndex)
{
	if (colorIndex < 0 || colorIndex >= 8)
		return kSarColors[0];
	return kSarColors[colorIndex];
}


const char*
SarMarkerTypeName(SarMarkerType type)
{
	switch (type) {
		case SAR_PERSON:	return "Found Person";
		case SAR_FIRE:		return "Fire";
		case SAR_STAGING:	return "Staging Area";
		case SAR_OBJECT:	return "Object Found";
		case SAR_UNKNOWN:	return "Unknown";
		default:			return "Marker";
	}
}


const char*
SarMarkerEmoji(SarMarkerType type)
{
	switch (type) {
		case SAR_PERSON:	return "\xF0\x9F\xA7\x91";			// 🧑
		case SAR_FIRE:		return "\xF0\x9F\x94\xA5";			// 🔥
		case SAR_STAGING:	return "\xF0\x9F\x8F\x95\xEF\xB8\x8F"; // 🏕️
		case SAR_OBJECT:	return "\xF0\x9F\x93\xA6";			// 📦
		case SAR_UNKNOWN:	return "\xE2\x9D\x93";				// ❓
		default:			return "\xE2\x9D\x93";
	}
}
