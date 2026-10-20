/*
 * Copyright 2026, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TcpConnectWindow.h — Dialog asking for the TCP address of a device
 */

#ifndef TCPCONNECTWINDOW_H
#define TCPCONNECTWINDOW_H

#include <Window.h>

class BButton;
class BStringView;
class BTextControl;


class TcpConnectWindow : public BWindow {
public:
							TcpConnectWindow(BWindow* parent);

	virtual void			MessageReceived(BMessage* message);

private:
			void			_OnConnect();

			BWindow*		fParent;

			BTextControl*	fAddressControl;
			BButton*		fConnectButton;
			BButton*		fCancelButton;
			BStringView*	fStatusLabel;
};

#endif // TCPCONNECTWINDOW_H
