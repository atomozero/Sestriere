/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Community.h — Community model with HMAC-SHA256 PSK derivation
 *
 * Compatible with meshcore-open community sharing format.
 * QR/JSON payload: {"v":1,"type":"meshcore_community","name":"...","k":"base64url"}
 */

#ifndef COMMUNITY_H
#define COMMUNITY_H

#include <String.h>
#include <SupportDefs.h>

#include <string.h>


struct CommunityInfo {
	char	name[64];
	uint8	secret[32];			// Shared secret K (32 bytes)
	char	communityId[65];	// SHA256("community:v1" || K) hex

	CommunityInfo()
	{
		memset(name, 0, sizeof(name));
		memset(secret, 0, sizeof(secret));
		memset(communityId, 0, sizeof(communityId));
	}
};


// Derive Community Public Channel PSK:
// PSK = HMAC-SHA256(K, "channel:v1:__public__")[:16]
void DeriveCommunityPublicPsk(const uint8* secret, uint8* outPsk);

// Derive Community Hashtag Channel PSK:
// PSK = HMAC-SHA256(K, "channel:v1:<normalized>")[:16]
// normalized = lowercase, strip leading #, trim
void DeriveCommunityHashtagPsk(const uint8* secret,
	const char* hashtag, uint8* outPsk);

// Compute Community ID: SHA256("community:v1" || K) → 64-char hex
void ComputeCommunityId(const uint8* secret, char* outHex);

// Parse community from JSON sharing format
// Returns true if valid, fills outInfo
bool ParseCommunityJson(const char* json, CommunityInfo* outInfo);

// Format community as JSON sharing payload
// outJson must be at least 256 bytes
bool FormatCommunityJson(const CommunityInfo* info, char* outJson,
	size_t outSize);


#endif	// COMMUNITY_H
