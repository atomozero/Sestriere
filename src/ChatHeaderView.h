/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatHeaderView.h — Chat area header showing contact info
 */

#ifndef CHATHEADERVIEW_H
#define CHATHEADERVIEW_H

#include <View.h>
#include <String.h>

#include "Types.h"

class ChatHeaderView : public BView {
public:
							ChatHeaderView(const char* name);
	virtual					~ChatHeaderView();

	virtual void			Draw(BRect updateRect);
	virtual void			AttachedToWindow();
	virtual BSize			MinSize();
	virtual BSize			PreferredSize();

			void			SetContact(const ContactInfo* contact);
			void			SetChannel(bool isChannel);
			void			SetChannelName(const char* name);
			void			SetStatus(const char* status);
			void			SetConnectionInfo(int8 pathLen, int8 snr);
			void			SetConsoleMode(bool console);

private:
			void			_DrawAvatar(BRect rect);
			rgb_color		_AvatarColor() const;

			const ContactInfo*	fContact;
			BString			fDisplayName;
			BString			fStatus;
			bool			fIsChannel;
			bool			fConsoleMode;
			int8			fPathLen;
			int8			fSnr;
};

#endif // CHATHEADERVIEW_H
