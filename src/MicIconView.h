/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MicIconView.h — Clickable microphone icon for voice recording
 */

#ifndef _MIC_ICON_VIEW_H
#define _MIC_ICON_VIEW_H

#include <View.h>

class MicIconView : public BView {
public:
							MicIconView(BMessage* clickMsg);
	virtual					~MicIconView();

	virtual void			Draw(BRect updateRect);
	virtual void			MouseDown(BPoint where);

			void			SetEnabled(bool enabled);
			void			SetRecording(bool recording);
			bool			IsEnabled() const { return fEnabled; }

private:
			BMessage*		fClickMsg;
			bool			fEnabled;
			bool			fRecording;
};

#endif // _MIC_ICON_VIEW_H
