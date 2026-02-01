/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * PortSelectionWindow.h — Serial port selection dialog
 */

#ifndef PORTSELECTIONWINDOW_H
#define PORTSELECTIONWINDOW_H

#include <Window.h>

class BButton;
class BListView;
class BStringView;

class PortSelectionWindow : public BWindow {
public:
							PortSelectionWindow(BWindow* parent);
	virtual					~PortSelectionWindow();

	virtual void			MessageReceived(BMessage* message);

private:
			void			_PopulatePortList();
			void			_OnConnect();
			void			_OnRefresh();

			BWindow*		fParent;
			BListView*		fPortList;
			BButton*		fConnectButton;
			BButton*		fRefreshButton;
			BStringView*	fStatusLabel;
};

#endif // PORTSELECTIONWINDOW_H
