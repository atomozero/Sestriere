/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GifPickerWindow.h — GIPHY GIF search and selection window
 */

#ifndef GIFPICKERWINDOW_H
#define GIFPICKERWINDOW_H

#include <Window.h>

#include "GiphyClient.h"

class BButton;
class BTextControl;
class BView;


class GifPickerWindow : public BWindow {
public:
							GifPickerWindow(BWindow* target);
	virtual					~GifPickerWindow();

	virtual void			MessageReceived(BMessage* message);
	virtual bool			QuitRequested();

private:
			void			_Search(const char* query);
			void			_LoadTrending();
	static	int32			_SearchThread(void* data);
	static	int32			_DownloadThumbnail(void* data);

			BTextControl*	fSearchField;
			BView*			fGridView;
			BButton*		fSendButton;
			BWindow*		fTarget;
			thread_id		fSearchThread;
			GiphyResult		fResults[20];
			int32			fResultCount;
};

#endif // GIFPICKERWINDOW_H
