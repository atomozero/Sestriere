/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactExportWindow.h — Contact import/export dialog
 */

#ifndef CONTACTEXPORTWINDOW_H
#define CONTACTEXPORTWINDOW_H

#include <Window.h>

#include "Types.h"

class BButton;
class BTextView;
class BStringView;

class ContactExportWindow : public BWindow {
public:
	// Mode: true = export, false = import
							ContactExportWindow(BWindow* parent, bool exportMode,
								const Contact* contact = NULL);
	virtual					~ContactExportWindow();

	virtual void			MessageReceived(BMessage* message);

			void			SetExportData(const uint8* data, size_t length);

private:
			void			_OnCopyToClipboard();
			void			_OnImport();

			BWindow*		fParent;
			bool			fExportMode;
			Contact			fContact;

			BStringView*	fTitleLabel;
			BTextView*		fDataView;
			BButton*		fActionButton;
			BButton*		fCloseButton;
			BStringView*	fStatusLabel;
};

#endif // CONTACTEXPORTWINDOW_H
