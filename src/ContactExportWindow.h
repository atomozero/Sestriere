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
class BStringView;
class BTextView;


class ContactExportWindow : public BWindow {
public:
	// exportMode: true = export, false = import
							ContactExportWindow(BWindow* parent,
								bool exportMode,
								const ContactInfo* contact = NULL);
	virtual					~ContactExportWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

			void			SetExportData(const uint8* data, size_t length);

private:
			void			_OnCopyToClipboard();
			void			_OnImport();

			BWindow*		fParent;
			bool			fExportMode;
			uint8			fPublicKey[32];
			char			fContactName[64];

			BStringView*	fTitleLabel;
			BTextView*		fDataView;
			BButton*		fActionButton;
			BButton*		fCloseButton;
			BStringView*	fStatusLabel;
};

#endif // CONTACTEXPORTWINDOW_H
