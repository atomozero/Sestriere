/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ContactInfoPanel.h — Right-side panel showing selected contact details
 */

#ifndef CONTACTINFOPANEL_H
#define CONTACTINFOPANEL_H

#include <View.h>
#include <String.h>

#include "Types.h"

class BButton;
class SNRChartView;

class ContactInfoPanel : public BView {
public:
						ContactInfoPanel(const char* name);
	virtual				~ContactInfoPanel();

	virtual void		Draw(BRect updateRect);
	virtual void		AttachedToWindow();
	virtual void		FrameResized(float newWidth, float newHeight);
	virtual void		MessageReceived(BMessage* message);
	virtual BSize		MinSize();
	virtual BSize		PreferredSize();

			void		SetContact(const ContactInfo* contact);
	const ContactInfo*	GetContact() const { return fContact; }
			void		SetChannel(bool isChannel);
			void		Clear();
			void		RefreshSNRChart();

private:
			void		_DrawAvatar(BRect rect);
			void		_DrawInfoRow(float& y, const char* label,
							const char* value, rgb_color valueColor);
			rgb_color	_AvatarColor() const;
			const char*	_TypeName() const;
			void		_FormatLastSeen(char* buffer, size_t size) const;
			void		_FormatPubKey(char* buffer, size_t size) const;
			void		_UpdateSNRChart();
			void		_PositionChart(float y);

			const ContactInfo*	fContact;
			bool		fIsChannel;
			SNRChartView*	fSNRChart;
};

#endif // CONTACTINFOPANEL_H
