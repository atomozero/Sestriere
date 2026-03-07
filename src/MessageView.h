/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MessageView.h — Individual chat message bubble display
 */

#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <Bitmap.h>
#include <ListItem.h>
#include <Rect.h>
#include <String.h>

#include <vector>

#include "ImageSession.h"
#include "SarMarker.h"
#include "Types.h"
#include "VoiceSession.h"

class MessageView : public BListItem {
public:
							MessageView(const ChatMessage& message,
								const char* senderName = NULL);
	virtual					~MessageView();

	virtual void			DrawItem(BView* owner, BRect frame,
								bool complete = false);
	virtual void			Update(BView* owner, const BFont* font);

			bool			IsOutgoing() const { return fOutgoing; }
			bool			IsChannel() const { return fIsChannel; }
			uint32			Timestamp() const { return fTimestamp; }
			const char*		Text() const { return fText.String(); }
			const char*		SenderName() const { return fSenderName.String(); }
			uint8			PathLen() const { return fPathLen; }
			const uint8*	PubKeyPrefix() const { return fPubKeyPrefix; }
			BRect			HopsClickRect() const { return fHopsClickRect; }
			bool			HasClickableHops() const
								{ return !fOutgoing && fPathLen > 0
									&& fPathLen != kPathLenDirect; }

			void			SetDeliveryStatus(uint8 status, uint32 rtt = 0,
								uint8 retryCount = 0);
			uint8			DeliveryStatus() const { return fDeliveryStatus; }

			// Image message support
			bool			IsImageMessage() const { return fIsImageMsg; }
			uint32			ImageSessionId() const { return fImageSessionId; }
			ImageSessionState ImageState() const { return fImageState; }
			void			SetImageState(ImageSessionState state,
								uint8 receivedCount);
			void			SetImageBitmap(BBitmap* bitmap);

			// Voice message support
			bool			IsVoiceMessage() const { return fIsVoiceMsg; }
			uint32			VoiceSessionId() const { return fVoiceSessionId; }
			uint32			VoiceDuration() const { return fVoiceDuration; }
			VoiceSessionState VoiceState() const { return fVoiceState; }
			void			SetVoiceState(VoiceSessionState state,
								uint8 receivedCount);
			void			SetVoicePlaying(bool playing);
			bool			IsVoicePlaying() const
								{ return fVoicePlaying; }
			BRect			PlayClickRect() const { return fPlayClickRect; }

			// GIF message support
			bool			IsGifMessage() const { return fIsGifMsg; }
			const char*		GifId() const { return fGifId; }
			uint8			GifLoadState() const { return fGifLoadState; }
			void			SetGifLoadState(uint8 state);
			void			SetGifFrames(BBitmap** frames,
								uint32* durations, int32 count);
			void			AdvanceGifFrame();
			int32			GifFrameCount() const { return fGifFrameCount; }
			int32			CurrentGifFrame() const
								{ return fGifCurrentFrame; }
			uint32			CurrentFrameDuration() const;

private:
			void			_FormatTimestamp(char* buffer, size_t size) const;
			void			_WrapText(BView* owner, const BString& text,
								float maxWidth,
								std::vector<BString>& outLines,
								float emojiSize = 0) const;
			void			_DrawImageBubble(BView* owner, BRect frame);
			void			_DrawVoiceBubble(BView* owner, BRect frame);
			void			_DrawGifBubble(BView* owner, BRect frame);

			BString			fText;
			BString			fSenderName;
			uint32			fTimestamp;
			bool			fOutgoing;
			bool			fIsChannel;
			uint8			fPathLen;
			int8			fSnr;
			uint8			fDeliveryStatus;
			uint32			fRoundTripMs;
			uint8			fTxtType;
			uint8			fRetryCount;
			uint8			fPubKeyPrefix[6];
			BRect			fHopsClickRect;

			bool			fIsSarMarker;
			SarMarker		fSarMarker;

			// Voice message fields
			bool			fIsVoiceMsg;
			uint32			fVoiceSessionId;
			uint32			fVoiceDuration;     // seconds
			VoiceSessionState fVoiceState;
			uint8			fVoiceTotalFragments;
			uint8			fVoiceReceivedFragments;
			bool			fVoicePlaying;
			BRect			fPlayClickRect;

			// Image message fields
			bool			fIsImageMsg;
			uint32			fImageSessionId;
			int32			fImageWidth;
			int32			fImageHeight;
			uint8			fImageTotalFragments;
			uint8			fImageReceivedFragments;
			ImageSessionState fImageState;
			BBitmap*		fImageBitmap;

			// GIF message fields
			bool			fIsGifMsg;
			char			fGifId[64];
			BBitmap**		fGifFrames;
			uint32*			fGifDurations;
			int32			fGifFrameCount;
			int32			fGifCurrentFrame;
			bigtime_t		fGifLastAdvance;
			uint8			fGifLoadState;

			float			fBaselineOffset;
};

#endif // MESSAGEVIEW_H
