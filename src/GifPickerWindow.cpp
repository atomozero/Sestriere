/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GifPickerWindow.cpp — GIPHY GIF search and selection window
 */

#include "GifPickerWindow.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <Button.h>
#include <DataIO.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <cstring>

#include "Constants.h"
#include "GiphyClient.h"


static const uint32 kMsgSearchGif = 'gsrc';
static const uint32 kMsgSearchDone = 'gsdn';
static const uint32 kMsgThumbnailReady = 'gthm';
static const uint32 kMsgSendGif = 'gsnd';
static const uint32 kMsgGifDoubleClick = 'gdbl';


// --- GifResultItem: BListItem subclass showing title + thumbnail ---

class GifResultItem : public BListItem {
public:
	GifResultItem(const char* id, const char* title, int32 index)
		:
		BListItem(),
		fThumbnail(NULL),
		fIndex(index)
	{
		strlcpy(fId, id, sizeof(fId));
		strlcpy(fTitle, title, sizeof(fTitle));
		// Truncate long titles
		if (strlen(fTitle) > 40) {
			fTitle[37] = '.';
			fTitle[38] = '.';
			fTitle[39] = '.';
			fTitle[40] = '\0';
		}
	}

	virtual ~GifResultItem()
	{
		delete fThumbnail;
	}

	virtual void DrawItem(BView* owner, BRect frame, bool complete = false)
	{
		rgb_color bgColor;
		if (IsSelected())
			bgColor = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
		else
			bgColor = ui_color(B_LIST_BACKGROUND_COLOR);

		owner->SetHighColor(bgColor);
		owner->SetLowColor(bgColor);
		owner->FillRect(frame);

		float thumbSize = frame.Height() - 4;
		float thumbLeft = frame.left + 4;
		float thumbTop = frame.top + 2;

		if (fThumbnail != NULL) {
			BRect src = fThumbnail->Bounds();
			BRect dst(thumbLeft, thumbTop,
				thumbLeft + thumbSize - 1, thumbTop + thumbSize - 1);
			owner->DrawBitmap(fThumbnail, src, dst);
		} else {
			// Placeholder
			BRect dst(thumbLeft, thumbTop,
				thumbLeft + thumbSize - 1, thumbTop + thumbSize - 1);
			owner->SetHighColor(tint_color(bgColor, B_DARKEN_2_TINT));
			owner->FillRect(dst);
		}

		// Title text
		float textLeft = thumbLeft + thumbSize + 8;
		font_height fh;
		owner->GetFontHeight(&fh);

		if (IsSelected())
			owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
		else
			owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));

		float textY = frame.top + frame.Height() / 2 + fh.ascent / 2;
		owner->DrawString(fTitle, BPoint(textLeft, textY));
	}

	virtual void Update(BView* owner, const BFont* font)
	{
		BListItem::Update(owner, font);
		SetHeight(52);
	}

	void SetThumbnail(BBitmap* bitmap)
	{
		delete fThumbnail;
		fThumbnail = bitmap;
	}

	const char* Id() const { return fId; }
	int32 Index() const { return fIndex; }

private:
	char		fId[64];
	char		fTitle[128];
	BBitmap*	fThumbnail;
	int32		fIndex;
};


// --- Thumbnail download context ---

struct ThumbnailContext {
	char		url[256];
	int32		index;
	BWindow*	window;
};


// --- GifPickerWindow ---

GifPickerWindow::GifPickerWindow(BWindow* target)
	:
	BWindow(BRect(200, 200, 550, 600), "GIPHY — Search GIFs",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fSearchThread(-1),
	fResultCount(0)
{
	memset(fResults, 0, sizeof(fResults));

	fSearchField = new BTextControl("search", "Search:", "",
		new BMessage(kMsgSearchGif));

	fResultList = new BListView("gif_results");
	fResultList->SetInvocationMessage(new BMessage(kMsgGifDoubleClick));
	BScrollView* scrollView = new BScrollView("gif_scroll", fResultList,
		0, false, true);

	fSendButton = new BButton("send", "Send", new BMessage(kMsgSendGif));

	BStringView* powered = new BStringView("powered", "Powered by GIPHY");
	BFont smallFont(be_plain_font);
	smallFont.SetSize(smallFont.Size() * 0.8f);
	powered->SetFont(&smallFont);
	powered->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
		B_LIGHTEN_1_TINT));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(fSearchField)
		.Add(scrollView, 1.0)
		.AddGroup(B_HORIZONTAL)
			.Add(powered)
			.AddGlue()
			.Add(fSendButton)
		.End()
	.End();

	fSearchField->SetTarget(this);
	fResultList->SetTarget(this);

	// Load trending on open
	_LoadTrending();
}


GifPickerWindow::~GifPickerWindow()
{
	// Wait for search thread to finish
	if (fSearchThread >= 0) {
		status_t result;
		wait_for_thread(fSearchThread, &result);
	}
}


bool
GifPickerWindow::QuitRequested()
{
	Hide();
	return false;
}


void
GifPickerWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSearchGif:
		{
			const char* query = fSearchField->Text();
			if (query != NULL && query[0] != '\0')
				_Search(query);
			else
				_LoadTrending();
			break;
		}

		case kMsgSearchDone:
		{
			int32 count = 0;
			if (message->FindInt32("count", &count) != B_OK)
				break;

			// Clear old results
			while (fResultList->CountItems() > 0) {
				BListItem* item = fResultList->RemoveItem((int32)0);
				delete item;
			}

			// Add new result items
			for (int32 i = 0; i < count; i++) {
				GifResultItem* item = new GifResultItem(
					fResults[i].id, fResults[i].title, i);
				fResultList->AddItem(item);

				// Start thumbnail download
				if (fResults[i].previewUrl[0] != '\0') {
					ThumbnailContext* ctx = new ThumbnailContext;
					strlcpy(ctx->url, fResults[i].previewUrl,
						sizeof(ctx->url));
					ctx->index = i;
					ctx->window = this;
					thread_id tid = spawn_thread(_DownloadThumbnail,
						"gif_thumb", B_LOW_PRIORITY, ctx);
					if (tid >= 0)
						resume_thread(tid);
					else
						delete ctx;
				}
			}
			break;
		}

		case kMsgThumbnailReady:
		{
			int32 index;
			if (message->FindInt32("index", &index) != B_OK)
				break;

			void* dataPtr = NULL;
			if (message->FindPointer("bitmap", &dataPtr) != B_OK)
				break;

			BBitmap* bitmap = (BBitmap*)dataPtr;

			// Find the matching item
			for (int32 i = 0; i < fResultList->CountItems(); i++) {
				GifResultItem* item = dynamic_cast<GifResultItem*>(
					fResultList->ItemAt(i));
				if (item != NULL && item->Index() == index) {
					item->SetThumbnail(bitmap);
					fResultList->InvalidateItem(i);
					bitmap = NULL;  // ownership transferred
					break;
				}
			}
			delete bitmap;  // delete if not used
			break;
		}

		case kMsgGifDoubleClick:
		case kMsgSendGif:
		{
			int32 sel = fResultList->CurrentSelection();
			if (sel < 0)
				break;

			GifResultItem* item = dynamic_cast<GifResultItem*>(
				fResultList->ItemAt(sel));
			if (item == NULL)
				break;

			// Send GIF selection to target window
			BMessage msg(MSG_GIF_SELECTED);
			msg.AddString("gif_id", item->Id());
			BMessenger(fTarget).SendMessage(&msg);

			Hide();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
GifPickerWindow::_Search(const char* query)
{
	if (fSearchThread >= 0) {
		// Previous search still running — skip
		return;
	}

	struct SearchContext {
		char		query[256];
		GiphyResult* results;
		int32*		count;
		BWindow*	window;
	};

	SearchContext* ctx = new SearchContext;
	strlcpy(ctx->query, query, sizeof(ctx->query));
	ctx->results = fResults;
	ctx->count = &fResultCount;
	ctx->window = this;

	fSearchThread = spawn_thread([](void* data) -> int32 {
		SearchContext* ctx = (SearchContext*)data;
		int32 count = GiphyClient::Search(ctx->query, ctx->results, 20);
		*ctx->count = count;

		BMessage msg(kMsgSearchDone);
		msg.AddInt32("count", count);
		BMessenger(ctx->window).SendMessage(&msg);

		delete ctx;
		return 0;
	}, "gif_search", B_NORMAL_PRIORITY, ctx);

	if (fSearchThread >= 0)
		resume_thread(fSearchThread);
	else
		delete ctx;

	fSearchThread = -1;  // Don't track — fire and forget
}


void
GifPickerWindow::_LoadTrending()
{
	struct TrendingContext {
		GiphyResult* results;
		int32*		count;
		BWindow*	window;
	};

	TrendingContext* ctx = new TrendingContext;
	ctx->results = fResults;
	ctx->count = &fResultCount;
	ctx->window = this;

	thread_id tid = spawn_thread([](void* data) -> int32 {
		TrendingContext* ctx = (TrendingContext*)data;
		int32 count = GiphyClient::Trending(ctx->results, 20);
		*ctx->count = count;

		BMessage msg(kMsgSearchDone);
		msg.AddInt32("count", count);
		BMessenger(ctx->window).SendMessage(&msg);

		delete ctx;
		return 0;
	}, "gif_trending", B_NORMAL_PRIORITY, ctx);

	if (tid >= 0)
		resume_thread(tid);
	else
		delete ctx;
}


int32
GifPickerWindow::_SearchThread(void* data)
{
	// Not used — inline lambdas above
	return 0;
}


int32
GifPickerWindow::_DownloadThumbnail(void* data)
{
	ThumbnailContext* ctx = (ThumbnailContext*)data;

	uint8* imgData = NULL;
	size_t imgSize = 0;
	status_t status = GiphyClient::DownloadData(ctx->url, &imgData, &imgSize);

	if (status == B_OK && imgData != NULL && imgSize > 0) {
		// Decode image data using Translation Kit
		BMemoryIO input(imgData, imgSize);
		BBitmap* bitmap = NULL;

		BTranslatorRoster* roster = BTranslatorRoster::Default();
		BBitmapStream output;
		status_t result = roster->Translate(&input, NULL, NULL, &output,
			B_TRANSLATOR_BITMAP);
		if (result == B_OK)
			output.DetachBitmap(&bitmap);

		if (bitmap != NULL) {
			BMessage msg(kMsgThumbnailReady);
			msg.AddInt32("index", ctx->index);
			msg.AddPointer("bitmap", bitmap);
			BMessenger(ctx->window).SendMessage(&msg);
			// Note: bitmap ownership transferred via message
		}
	}

	free(imgData);
	delete ctx;
	return 0;
}
