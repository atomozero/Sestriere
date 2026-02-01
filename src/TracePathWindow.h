/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TracePathWindow.h — Trace path visualization window
 */

#ifndef TRACEPATHWINDOW_H
#define TRACEPATHWINDOW_H

#include <Window.h>
#include <ObjectList.h>

#include "Types.h"

class BButton;
class BListView;
class BStringView;

// Single hop in a trace path
struct TraceHop {
	uint8		pubKeyPrefix[kPubKeyPrefixSize];
	char		name[kMaxNameLen];
	int8		snr;		// SNR * 4
	uint32		timestamp;
};

class TracePathWindow : public BWindow {
public:
							TracePathWindow(BWindow* parent,
								const Contact* contact);
	virtual					~TracePathWindow();

	virtual void			MessageReceived(BMessage* message);

			void			AddTraceHop(const TraceHop& hop);
			void			SetTraceComplete(bool success);

private:
			void			_OnStartTrace();
			void			_UpdateHopList();

			BWindow*		fParent;
			Contact			fContact;

			BStringView*	fTargetLabel;
			BListView*		fHopList;
			BButton*		fTraceButton;
			BButton*		fCloseButton;
			BStringView*	fStatusLabel;

			BObjectList<TraceHop> fHops;
			bool			fTracing;
};

#endif // TRACEPATHWINDOW_H
