/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * RepeaterAdminWindow.h — Remote repeater administration window
 */

#ifndef REPEATERADMINWINDOW_H
#define REPEATERADMINWINDOW_H

#include <Window.h>
#include <String.h>

#include "Types.h"

class BButton;
class BColumnListView;
class BMessageRunner;
class BStringView;
class BTabView;

// Remote admin message codes
enum {
	MSG_ADMIN_REQUEST_STATUS	= 'arst',
	MSG_ADMIN_REBOOT			= 'arbt',
	MSG_ADMIN_FACTORY_RESET		= 'arfr',
	MSG_ADMIN_REMOVE_CONTACT	= 'arrc',
	MSG_ADMIN_SHARE_CONTACT		= 'arsc',
	MSG_ADMIN_RESET_PATH		= 'arrp',
	MSG_ADMIN_SET_TX_POWER		= 'astp',
	MSG_ADMIN_REFRESH			= 'arrf',
	MSG_ADMIN_REFRESH_TIMER		= 'artm',
	MSG_ADMIN_DISCONNECT		= 'ards',
};


class RepeaterAdminWindow : public BWindow {
public:
							RepeaterAdminWindow(BWindow* parent,
								const uint8* pubkey,
								const char* name, uint8 type);
	virtual					~RepeaterAdminWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			// Data updates from MainWindow
			void			SetBatteryInfo(uint16 battMv,
								uint32 usedKb, uint32 totalKb);
			void			SetStats(uint32 uptime, uint32 txPackets,
								uint32 rxPackets, int8 rssi, int8 snr,
								int8 noiseFloor);
			void			UpdateContactList(
								const BObjectList<ContactInfo, true>* contacts);
			void			SetSessionActive(bool active);

			const uint8*	PublicKey() const { return fPublicKey; }

private:
			void			_BuildUI();
			BView*			_BuildOverviewTab();
			BView*			_BuildContactsTab();
			BView*			_BuildActionsTab();

			void			_UpdateOverview();
			void			_FormatUptime(uint32 seconds, BString& output);

			BWindow*		fParent;
			uint8			fPublicKey[32];
			BString			fNodeName;
			uint8			fNodeType;

			BTabView*		fTabView;

			// Overview tab
			BStringView*	fNameView;
			BStringView*	fTypeView;
			BStringView*	fSessionView;
			BStringView*	fBatteryView;
			BStringView*	fStorageView;
			BStringView*	fUptimeView;
			BStringView*	fTxPacketsView;
			BStringView*	fRxPacketsView;
			BStringView*	fRssiView;
			BStringView*	fSnrView;
			BStringView*	fNoiseFloorView;

			// Contacts tab
			BColumnListView*	fContactListView;
			BButton*		fRemoveContactButton;
			BButton*		fShareContactButton;
			BButton*		fResetPathButton;

			// Actions tab
			BButton*		fRebootButton;
			BButton*		fFactoryResetButton;
			BButton*		fRefreshButton;

			// State
			bool			fSessionActive;
			uint16			fBattMv;
			uint32			fUsedKb;
			uint32			fTotalKb;
			uint32			fUptime;
			uint32			fTxPkts;
			uint32			fRxPkts;
			int8			fRssi;
			int8			fSnr;
			int8			fNoise;

			BMessageRunner*	fRefreshTimer;
};


#endif // REPEATERADMINWINDOW_H
