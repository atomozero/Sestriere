/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * PacketAnalyzerWindow.h — Real-time MeshCore packet analyzer window
 */

#ifndef PACKETANALYZERWINDOW_H
#define PACKETANALYZERWINDOW_H

#include <Window.h>
#include <ObjectList.h>
#include <String.h>

#include "Types.h"

class BButton;
class BCheckBox;
class BColumnListView;
class BFilePanel;
class BMenuBar;
class BMenuField;
class BPopUpMenu;
class BRow;
class BSplitView;
class BStringView;
class BTextControl;
class BTextView;

// Column indices for the packet list
enum {
	kIndexColumn = 0,
	kTimeColumn,
	kTypeColumn,
	kSourceColumn,
	kSNRColumn,
	kSizeColumn,
	kSummaryColumn,
	kColumnCount
};


class PacketAnalyzerWindow : public BWindow {
public:
							PacketAnalyzerWindow(BWindow* parent);
	virtual					~PacketAnalyzerWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			AddPacket(const CapturedPacket& packet);
			void			Clear();

			bool			IsCapturing() const { return fCapturing; }

			// Packet decoding (public for MainWindow access)
	static	void			_DecodePacket(CapturedPacket& packet,
								const uint8* rawData, size_t rawLength);
	static	const char*		_PacketTypeName(uint8 code);

private:
			void			_BuildMenuBar();
			void			_BuildUI();
			void			_BuildToolBar(BView* parent);

			// Display helpers
			void			_UpdatePacketDetail(int32 index);
			void			_FormatHexDump(const uint8* data, size_t length,
								BString& output);
			void			_UpdateStatusBar();

			// Filtering
			bool			_MatchesFilter(const CapturedPacket& packet);
			void			_RebuildFilteredList();

			// Export
			void			_ExportCSV(const char* path);

			BWindow*		fParent;
			BMenuBar*		fMenuBar;

			// Toolbar controls
			BButton*		fStartStopButton;
			BButton*		fClearButton;
			BPopUpMenu*		fFilterMenu;
			BMenuField*		fFilterField;
			BTextControl*	fSearchField;
			BCheckBox*		fAutoScrollCheck;

			// Main views
			BColumnListView*	fPacketList;
			BTextView*		fDetailView;
			BSplitView*		fSplitView;

			// Status bar
			BStringView*	fStatusTotal;
			BStringView*	fStatusFiltered;
			BStringView*	fStatusRate;

			// Data
			BObjectList<CapturedPacket, true>	fPackets;
			bool			fCapturing;
			uint32			fPacketIndex;
			int32			fFilterType;	// -1 = all, or specific code
			BString			fSearchText;

			// Export
			BFilePanel*		fSavePanel;

			// Rate calculation
			uint32			fRateCount;
			bigtime_t		fRateStartTime;
};


#endif // PACKETANALYZERWINDOW_H
