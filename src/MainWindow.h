/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MainWindow.h — Main application window
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Window.h>
#include <MessageRunner.h>

#include "Types.h"

class BBox;
class BButton;
class BMenuBar;
class BMenuItem;
class BSplitView;
class BStringView;
class BTextControl;

class ChatView;
class ContactListView;
class StatusBarView;

class MainWindow : public BWindow {
public:
							MainWindow(BRect frame);
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			SetConnected(bool connected);
			void			SetDeviceInfo(const DeviceInfo& info);
			void			SetSelfInfo(const SelfInfo& info);
			void			SetBatteryInfo(const BatteryAndStorage& info);

			void			AddContact(const Contact& contact);
			void			ClearContacts();
			void			UpdateContactCount(int32 count);

			void			AddMessage(const ReceivedMessage& message,
								bool outgoing = false);
			void			MessageSent(const char* text, uint32 timestamp);
			void			MessageConfirmed(uint32 ackCode, uint32 roundTripMs);

private:
			void			_BuildMenu();
			void			_BuildLayout();
			void			_UpdateConnectionUI();
			void			_SendCurrentMessage();
			void			_StartPolling();
			void			_StopPolling();

			void			_HandleFrameReceived(BMessage* message);
			void			_HandleDeviceInfo(BMessage* message);
			void			_HandleSelfInfo(BMessage* message);
			void			_HandleContactsStart(BMessage* message);
			void			_HandleContactReceived(BMessage* message);
			void			_HandleContactsEnd(BMessage* message);
			void			_HandleMessageReceived(BMessage* message);
			void			_HandleSendConfirmed(BMessage* message);
			void			_HandleBatteryReceived(BMessage* message);
			void			_HandlePushNotification(BMessage* message);

			BMenuBar*		fMenuBar;
			BMenuItem*		fConnectItem;
			BMenuItem*		fDisconnectItem;
			BMenuItem*		fRefreshContactsItem;
			BMenuItem*		fSendAdvertItem;
			BMenuItem*		fLoginItem;
			BMenuItem*		fTracePathItem;
			BMenuItem*		fExportContactItem;

			BSplitView*		fSplitView;
			StatusBarView*	fStatusBar;
			ContactListView* fContactList;
			ChatView*		fChatView;
			BTextControl*	fMessageInput;
			BButton*		fSendButton;
			BStringView*	fPlaceholderLabel;

			DeviceInfo		fDeviceInfo;
			SelfInfo		fSelfInfo;
			bool			fConnected;
			bool			fHandshakeComplete;
			bool			fSendingToChannel;
			int32			fExpectedContactCount;
			int32			fReceivedContactCount;

			BMessageRunner*	fBatteryTimer;
};

#endif // MAINWINDOW_H
