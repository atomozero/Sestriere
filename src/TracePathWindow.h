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
class BMessageRunner;
class BStringView;
class TracePathView;

// Single hop in a trace path
struct TraceHop {
	uint8		pubKeyPrefix[kPubKeyPrefixSize];
	char		name[64];
	int8		snr;		// SNR * 4
	uint32		timestamp;
	bool		hasSnr;		// true if SNR data available (live trace)

	TraceHop() : snr(0), timestamp(0), hasSnr(false) {
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

			// Start trace externally (from ChatView click) — sends immediately
			void			StartExternalTrace(const ContactInfo* contact);

			// Update target contact without starting trace (from menu)
			void			SetContact(const ContactInfo* contact);

			// Resolve hop names from contact list
			void			ResolveHopNames(
								const OwningObjectList<ContactInfo>* contacts);

private:
			void			_OnStartTrace();
			void			_UpdatePathView();
			void			_PopulateKnownPath();

			BWindow*		fParent;
			ContactInfo		fContact;

			BStringView*	fTargetLabel;
			TracePathView*	fPathView;
			BButton*		fTraceButton;
			BButton*		fCloseButton;
			BStringView*	fStatusLabel;

			OwningObjectList<TraceHop>	fHops;
			bool			fTracing;
			BMessageRunner*	fTimeoutRunner;
};

#endif // TRACEPATHWINDOW_H
