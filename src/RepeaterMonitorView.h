/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepeaterMonitorView.h — Structured repeater log viewer (embedded BView)
 */

#ifndef REPEATERMONITORVIEW_H
#define REPEATERMONITORVIEW_H

#include <String.h>
#include <View.h>

struct RepeaterPacket;

class BButton;
class BColumnListView;
class BHandler;
class BSplitView;
class BStringView;
class BTextControl;


// Data exported for network map visualization
struct RepeaterNodeInfo {
	char	name[8];		// Node hex ID (e.g. "FD")
	char	fullName[64];	// Resolved name (from contacts DB, or empty)
	int32	packetCount;
	int8	avgSnr;
	int16	avgRssi;
	bool	isDirect;		// Has packets with route='D'
	bool	isForwarded;	// Has packets with route='F'
	bool	isSelf;			// Auto-detected as repeater itself
	char	lastTime[12];
};

struct RepeaterLink {
	char	src[8];
	char	dst[8];
	int8	snr;			// Last SNR observed (0 if TX)
	int32	count;			// Packets on this link
};


class RepeaterMonitorView : public BView {
public:
						RepeaterMonitorView(BHandler* target);
	virtual				~RepeaterMonitorView();

	virtual void		AttachedToWindow();
	virtual void		MessageReceived(BMessage* message);

			void		ProcessLine(const char* line);
			void		SetFirmwareVersion(const char* ver);
			void		Clear();

			// Topology data access for NetworkMapWindow
			int32		GetNodeInfos(RepeaterNodeInfo* outArray,
							int32 maxCount) const;
			int32		GetLinks(RepeaterLink* outArray,
							int32 maxCount) const;

private:
			BHandler*		fTarget;

			// Packet table
			BColumnListView*	fPacketList;

			// Node stats table
			BColumnListView*	fNodeStatsList;

			// SNR/RSSI graph
			BView*			fSnrGraph;

			// Toolbar
			BStringView*	fFirmwareLabel;
			BButton*		fLoadLogButton;
			BButton*		fClearButton;

			// Status bar
			BStringView*	fTotalStatsLabel;

			// Filter
			BTextControl*	fSearchField;
			BString			fFilterNode;

			// Private methods
			void			_UpdateStats();
			void			_UpdateNodeStats(const RepeaterPacket& pkt);
			void			_UpdateLink(const char* src, const char* dst,
							int8 snr);
			void			_RebuildFilteredList();

			// Counters
			int32			fTotalTx;
			int32			fTotalRx;
			int32			fTotalFwd;
			int32			fTotalDirect;
			int32			fPacketCount;

			// Per-node tracking (indexed by node stats row order)
			static const int32 kMaxNodes = 64;
			bool			fNodeHasDirect[64];
			bool			fNodeHasForwarded[64];
			int32			fNodeTxAsSrc[64];
			int32			fNodeRxAsSrc[64];

			// Link tracking for topology
			static const int32 kMaxLinks = 128;
			RepeaterLink	fLinks[128];
			int32			fLinkCount;
};

#endif // REPEATERMONITORVIEW_H
