/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TracePathWindow.h — Trace path visualization window
 */

#ifndef TRACEPATHWINDOW_H
#define TRACEPATHWINDOW_H

#include <Window.h>
#include "Compat.h"

#include "Types.h"

class BButton;
class BListView;
class BStringView;

// Single hop in a trace path
struct TraceHop {
	uint8		pubKeyPrefix[kPubKeyPrefixSize];
	char		name[64];
	int8		snr;		// SNR * 4
	uint32		timestamp;

	TraceHop() : snr(0), timestamp(0) {
		memset(pubKeyPrefix, 0, sizeof(pubKeyPrefix));
		memset(name, 0, sizeof(name));
	}
};

class TracePathWindow : public BWindow {
public:
							TracePathWindow(BWindow* parent,
								const ContactInfo* contact);
	virtual					~TracePathWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			AddTraceHop(const TraceHop& hop);
			void			SetTraceComplete(bool success);

			// Parse trace data from device
			void			ParseTraceData(const uint8* data, size_t length);

private:
			void			_OnStartTrace();
			void			_UpdateHopList();
			void			_FormatPubKeyPrefix(const uint8* prefix, char* out,
								size_t outSize);

			BWindow*		fParent;
			ContactInfo		fContact;

			BStringView*	fTargetLabel;
			BListView*		fHopList;
			BButton*		fTraceButton;
			BButton*		fCloseButton;
			BStringView*	fStatusLabel;

			OwningObjectList<TraceHop>	fHops;
			bool			fTracing;
};

#endif // TRACEPATHWINDOW_H
