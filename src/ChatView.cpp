/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * ChatView.cpp — Chat message display view implementation
 */

#include "ChatView.h"

#include <Clipboard.h>
#include <Entry.h>
#include <File.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <Window.h>

#include <cstring>

#include "Constants.h"
#include "DatabaseManager.h"
#include "EmojiRenderer.h"
#include "GiphyClient.h"
#include "ImageCodec.h"
#include "ImageSession.h"
#include "MessageView.h"


static const uint32 kMsgCopyText = 'cpyt';


ChatView::ChatView(const char* name)
	:
	BListView(name, B_SINGLE_SELECTION_LIST),
	fCurrentContact(NULL),
	fCurrentContactName(""),
	fGifAnimateRunner(NULL),
	fCurrentMessages(20)
{
}


ChatView::~ChatView()
{
	ClearMessages();
}


void
ChatView::AttachedToWindow()
{
	BListView::AttachedToWindow();

	// Light gray background for chat area
	SetViewColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
}


void
ChatView::FrameResized(float newWidth, float newHeight)
{
	// Recalculate all item heights for the new width —
	// text wrapping depends on view width, so heights change on resize
	BFont font;
	GetFont(&font);

	for (int32 i = 0; i < CountItems(); i++)
		ItemAt(i)->Update(this, &font);

	BListView::FrameResized(newWidth, newHeight);
	Invalidate();
}


void
ChatView::MouseDown(BPoint where)
{
	BMessage* current = Window()->CurrentMessage();
	int32 buttons = 0;
	if (current != NULL)
		current->FindInt32("buttons", &buttons);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		int32 index = IndexOf(where);
		if (index < 0) return;

		MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
		if (item == NULL) return;

		Select(index);

		BPopUpMenu* menu = new BPopUpMenu("context", false, false);

		BMessage* copyMsg = new BMessage(kMsgCopyText);
		copyMsg->AddInt32("index", index);
		menu->AddItem(new BMenuItem("Copy", copyMsg));

		menu->SetTargetForItems(this);

		ConvertToScreen(&where);
		menu->Go(where, true, true, true);
		return;
	}

	// Check for double-click on incoming message → quote reply
	if (buttons & B_PRIMARY_MOUSE_BUTTON) {
		int32 clicks = 1;
		if (current != NULL)
			current->FindInt32("clicks", &clicks);

		if (clicks == 2) {
			int32 index = IndexOf(where);
			if (index >= 0) {
				MessageView* item = dynamic_cast<MessageView*>(
					ItemAt(index));
				if (item != NULL && !item->IsOutgoing()
					&& item->SenderName()[0] != '\0') {
					BMessage msg(MSG_QUOTE_REPLY);
					msg.AddString("sender", item->SenderName());
					Window()->PostMessage(&msg);
					return;
				}
			}
		}
	}

	// Check for left-click on "X hops" link or image download
	if (buttons & B_PRIMARY_MOUSE_BUTTON) {
		int32 index = IndexOf(where);
		if (index >= 0) {
			MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
			if (item != NULL) {
				// Click on hops link
				if (item->HasClickableHops()
					&& item->HopsClickRect().Contains(where)) {
					BMessage msg(MSG_TRACE_PATH);
					msg.AddData("pubkey", B_RAW_TYPE,
						item->PubKeyPrefix(), kPubKeyPrefixSize);
					Window()->PostMessage(&msg);
					return;
				}
				// Click on pending image to download
				if (item->IsImageMessage()
					&& item->ImageState() == IMAGE_PENDING) {
					BMessage msg(MSG_IMAGE_FETCH_REQ);
					msg.AddUInt32("session_id", item->ImageSessionId());
					Window()->PostMessage(&msg);
					return;
				}
				// Click on voice play button
				if (item->IsVoiceMessage()
					&& item->PlayClickRect().Contains(where)) {
					BMessage msg(MSG_VOICE_PLAY_REQ);
					msg.AddUInt32("session_id",
						item->VoiceSessionId());
					Window()->PostMessage(&msg);
					return;
				}
			}
		}
	}

	BListView::MouseDown(where);
}


void
ChatView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_GIF_ANIMATE:
		{
			bool hasGifs = false;
			for (int32 i = 0; i < CountItems(); i++) {
				MessageView* mv = dynamic_cast<MessageView*>(ItemAt(i));
				if (mv != NULL && mv->IsGifMessage()
					&& mv->GifFrameCount() > 1) {
					hasGifs = true;
					int32 oldFrame = mv->CurrentGifFrame();
					mv->AdvanceGifFrame();
					if (mv->CurrentGifFrame() != oldFrame)
						InvalidateItem(i);
				}
			}
			if (!hasGifs) {
				delete fGifAnimateRunner;
				fGifAnimateRunner = NULL;
			}
			break;
		}

		case MSG_EMOJI_LOADED:
		{
			// An emoji bitmap was downloaded — repaint all items
			Invalidate();
			break;
		}

		case kMsgCopyText:
		{
			int32 index;
			if (message->FindInt32("index", &index) != B_OK)
				break;

			MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
			if (item == NULL)
				break;

			const char* text = item->Text();
			if (text == NULL || text[0] == '\0')
				break;

			if (be_clipboard->Lock()) {
				be_clipboard->Clear();
				BMessage* clip = be_clipboard->Data();
				clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
				be_clipboard->Commit();
				be_clipboard->Unlock();
			}
			break;
		}

		default:
			BListView::MessageReceived(message);
			break;
	}
}


void
ChatView::AddMessage(const ChatMessage& message, const char* senderName)
{
	// Store message in history
	ChatMessage* stored = new ChatMessage(message);
	fCurrentMessages.AddItem(stored);

	// Create and add visual item
	MessageView* item = new MessageView(message, senderName);

	// Detect @[selfName] mention in incoming messages
	if (!message.isOutgoing && fSelfName.Length() > 0) {
		BString mention;
		mention.SetToFormat("@[%s]", fSelfName.String());
		if (BString(message.text).IFindFirst(mention) >= 0)
			item->SetMention(true);
	}

	AddItem(item);

	// Request emoji downloads for any emoji in the message text
	if (!item->IsGifMessage() && !item->IsImageMessage())
		EmojiRenderer::RequestEmoji(message.text, this);

	_ScrollToBottom();
}


void
ChatView::SetSelfName(const char* name)
{
	fSelfName = (name != NULL) ? name : "";
}


void
ChatView::UpdateDeliveryStatus(int32 index, uint8 status, uint32 rtt,
	uint8 retryCount)
{
	if (index < 0 || index >= CountItems())
		return;

	MessageView* item = dynamic_cast<MessageView*>(ItemAt(index));
	if (item == NULL)
		return;

	item->SetDeliveryStatus(status, rtt, retryCount);
	InvalidateItem(index);
}


void
ChatView::ClearMessages()
{
	// Stop GIF animation
	delete fGifAnimateRunner;
	fGifAnimateRunner = NULL;

	// Remove visual items
	for (int32 i = CountItems() - 1; i >= 0; i--) {
		MessageView* item = dynamic_cast<MessageView*>(RemoveItem(i));
		delete item;
	}

	// Clear stored messages
	fCurrentMessages.MakeEmpty();
}


void
ChatView::SetCurrentContact(ContactInfo* contact)
{
	fCurrentContact = contact;
	if (contact != NULL)
		fCurrentContactName = contact->name;
	else
		fCurrentContactName = "";

	// Clear current view and load contact's message history
	ClearMessages();

	if (contact != NULL) {
		// Load messages from contact's history
		for (int32 i = 0; i < contact->messages.CountItems(); i++) {
			ChatMessage* msg = contact->messages.ItemAt(i);
			if (msg != NULL) {
				const char* senderName = msg->isOutgoing ? "Me" : contact->name;
				MessageView* item = new MessageView(*msg, senderName);

				// Detect @[selfName] mention
				if (!msg->isOutgoing && fSelfName.Length() > 0) {
					BString mention;
					mention.SetToFormat("@[%s]", fSelfName.String());
					if (BString(msg->text).IFindFirst(mention) >= 0)
						item->SetMention(true);
				}

				// Load saved image from DB if this is an IE2 message
				if (item->IsImageMessage()) {
					uint8* jpegData = NULL;
					size_t jpegSize = 0;
					int32 w = 0, h = 0;
					if (DatabaseManager::Instance()->LoadImage(
						item->ImageSessionId(), &jpegData, &jpegSize,
						&w, &h)) {
						BBitmap* bitmap = ImageCodec::DecompressImageData(
							jpegData, jpegSize);
						if (bitmap != NULL)
							item->SetImageBitmap(bitmap);
						free(jpegData);
					}
				}

				// Load cached GIF if this is a GIF message
				if (item->IsGifMessage()) {
					BString cachePath;
					cachePath.SetToFormat(
						"/boot/home/config/settings/Sestriere/"
						"gif_cache/%s.gif", item->GifId());
					BEntry entry(cachePath.String());
					if (entry.Exists()) {
						off_t fileSize;
						entry.GetSize(&fileSize);
						if (fileSize > 0) {
							uint8* gifData = (uint8*)malloc(fileSize);
							if (gifData != NULL) {
								BFile file(cachePath.String(),
									B_READ_ONLY);
								if (file.Read(gifData, fileSize)
									== fileSize) {
									BBitmap** frames = NULL;
									uint32* durations = NULL;
									int32 frameCount = 0;
									if (ImageCodec::DecompressGifFrames(
										gifData, fileSize, &frames,
										&durations, &frameCount)
										== B_OK) {
										item->SetGifFrames(frames,
											durations, frameCount);
									}
								}
								free(gifData);
							}
						}
					}
				}

				AddItem(item);

				// Trigger download for GIF messages not found in cache
				if (item->IsGifMessage() && item->GifLoadState() == 0
					&& Window() != NULL) {
					item->SetGifLoadState(1);  // loading
					BMessage dlMsg(MSG_GIF_DOWNLOAD_REQ);
					dlMsg.AddString("gif_id", item->GifId());
					dlMsg.AddInt32("item_index",
						CountItems() - 1);
					Window()->PostMessage(&dlMsg);
				}

				// Request emoji downloads for regular text messages
				if (!item->IsGifMessage() && !item->IsImageMessage())
					EmojiRenderer::RequestEmoji(msg->text, this);
			}
		}
		_ScrollToBottom();

		// Start GIF animation if any loaded GIFs have multiple frames
		for (int32 i = 0; i < CountItems(); i++) {
			MessageView* mv = dynamic_cast<MessageView*>(ItemAt(i));
			if (mv != NULL && mv->IsGifMessage()
				&& mv->GifFrameCount() > 1) {
				StartGifAnimation();
				break;
			}
		}
	}
}


OwningObjectList<ChatMessage>*
ChatView::GetMessageHistory()
{
	return &fCurrentMessages;
}


void
ChatView::StartGifAnimation()
{
	if (fGifAnimateRunner != NULL)
		return;

	BMessage msg(MSG_GIF_ANIMATE);
	fGifAnimateRunner = new BMessageRunner(BMessenger(this), &msg,
		50000);  // 50ms = 20fps
}


void
ChatView::_ScrollToBottom()
{
	if (CountItems() == 0)
		return;

	// Scroll to show last item
	int32 lastIndex = CountItems() - 1;
	ScrollToSelection();
	Select(lastIndex);
	Deselect(lastIndex);

	// Also try to scroll the parent scroll view
	BScrollView* scrollView = dynamic_cast<BScrollView*>(Parent());
	if (scrollView != NULL) {
		BScrollBar* vScroll = scrollView->ScrollBar(B_VERTICAL);
		if (vScroll != NULL) {
			float min, max;
			vScroll->GetRange(&min, &max);
			vScroll->SetValue(max);
		}
	}
}
