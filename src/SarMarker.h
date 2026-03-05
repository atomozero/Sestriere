/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SarMarker.h — SAR (Search & Rescue) marker parsing and formatting
 *
 * Compatible with meshcore-sar text-based marker protocol:
 *   S:<emoji>:<colorIndex>:<lat>,<lon>:<note>   (new format)
 *   S:<emoji>:<lat>,<lon>:<note>                 (old format, no color)
 */

#ifndef SARMARKER_H
#define SARMARKER_H

#include <GraphicsDefs.h>
#include <SupportDefs.h>

#include <cstring>

enum SarMarkerType {
	SAR_PERSON = 0,
	SAR_FIRE,
	SAR_STAGING,
	SAR_OBJECT,
	SAR_UNKNOWN,
	SAR_TYPE_COUNT
};

struct SarMarker {
	SarMarkerType	type;
	char			emoji[8];
	int				colorIndex;
	double			lat;
	double			lon;
	char			notes[128];
	bool			isValid;

	SarMarker()
		: type(SAR_UNKNOWN), colorIndex(0),
		  lat(0.0), lon(0.0), isValid(false)
	{
		memset(emoji, 0, sizeof(emoji));
		memset(notes, 0, sizeof(notes));
	}
};

bool			ParseSarMarker(const char* text, SarMarker& out);
void			FormatSarMarker(char* buf, size_t size, const SarMarker& marker);
rgb_color		SarMarkerColor(int colorIndex);
const char*		SarMarkerTypeName(SarMarkerType type);
const char*		SarMarkerEmoji(SarMarkerType type);

#endif // SARMARKER_H
