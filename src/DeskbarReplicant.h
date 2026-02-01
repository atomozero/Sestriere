/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DeskbarReplicant.h — Deskbar tray icon replicant
 */

#ifndef DESKBARREPLICANT_H
#define DESKBARREPLICANT_H

#include <View.h>

class BBitmap;

class DeskbarReplicant : public BView {
public:
							DeskbarReplicant(BRect frame, int32 resizingMode);
							DeskbarReplicant(BMessage* archive);
	virtual					~DeskbarReplicant();

	static	DeskbarReplicant* Instantiate(BMessage* archive);
	virtual	status_t		Archive(BMessage* archive, bool deep = true) const;

	virtual void			AttachedToWindow();
	virtual void			DetachedFromWindow();
	virtual void			Draw(BRect updateRect);
	virtual void			MouseDown(BPoint where);
	virtual void			MessageReceived(BMessage* message);

			void			SetConnected(bool connected);
			void			SetBatteryLevel(uint8 level);
			void			SetUnreadCount(int32 count);

private:
			void			_Init();
			void			_DrawIcon(BRect bounds);
			void			_ShowPopupMenu(BPoint where);
			BBitmap*		_CreateIcon(bool connected);

			bool			fConnected;
			uint8			fBatteryLevel;
			int32			fUnreadCount;
			BBitmap*		fIconConnected;
			BBitmap*		fIconDisconnected;
};

#endif // DESKBARREPLICANT_H
