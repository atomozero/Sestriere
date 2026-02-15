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
class BTextControl;
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
			void		SetChannelStats(int32 contactCount,
							int32 onlineCount);
			void		Clear();
			void		RefreshSNRChart();

			// Admin session (repeater/room management)
			void		SetAdminSession(bool active);
			void		SetBatteryInfo(uint16 battMv,
							uint32 usedKb, uint32 totalKb);
			void		SetRadioStats(uint32 uptime,
							uint32 txPkts, uint32 rxPkts,
							int8 rssi, int8 snr, int8 noise);
			bool		IsAdminSession() const { return fAdminActive; }

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
			void		_DrawSectionHeader(float& y,
							const char* title);
			void		_DrawAdminSections(float& y);
			void		_PositionButtons(float& y);

			const ContactInfo*	fContact;
			bool		fIsChannel;
			int32		fChannelContactCount;
			int32		fChannelOnlineCount;
			SNRChartView*	fSNRChart;

			// Admin state
			bool		fAdminActive;
			uint16		fBattMv;
			uint32		fUsedKb;
			uint32		fTotalKb;
			uint32		fAdminUptime;
			uint32		fAdminTxPkts;
			uint32		fAdminRxPkts;
			int8		fAdminRssi;
			int8		fAdminSnr;
			int8		fAdminNoise;

			// Admin buttons
			BButton*	fRebootButton;
			BButton*	fFactoryResetButton;

			// CLI command buttons
			BButton*	fVersionButton;
			BButton*	fNeighborsButton;
			BButton*	fClockButton;
			BButton*	fClearStatsButton;

			// CLI input fields + buttons
			BTextControl*	fSetNameField;
			BButton*	fSetNameButton;
};


#endif // CONTACTINFOPANEL_H
