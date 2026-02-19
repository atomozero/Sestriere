/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GrowingTextView.h — Auto-growing text input that sends a message on Enter
 */

#ifndef GROWINGTEXTVIEW_H
#define GROWINGTEXTVIEW_H

#include <TextView.h>

class GrowingTextView : public BTextView {
public:
						GrowingTextView(const char* name,
							BMessage* enterMessage);
	virtual				~GrowingTextView();

	virtual void		KeyDown(const char* bytes, int32 numBytes);
	virtual void		InsertText(const char* text, int32 length,
							int32 offset, const text_run_array* runs);
	virtual void		DeleteText(int32 fromOffset, int32 toOffset);

	virtual BSize		MinSize();
	virtual BSize		MaxSize();
	virtual BSize		PreferredSize();

			void		SetSendMessage(BMessage* message);
			void		SetEnabled(bool enabled);
			bool		IsEnabled() const { return fEnabled; }
			void		SetModificationMessage(BMessage* message);

private:
			void		_RecalcHeight();
			void		_NotifyModification();

			BMessage*	fEnterMessage;
			BMessage*	fModificationMessage;
			bool		fEnabled;
			float		fMinHeight;
			float		fMaxHeight;
};

#endif // GROWINGTEXTVIEW_H
