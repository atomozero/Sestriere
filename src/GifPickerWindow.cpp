/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * GifPickerWindow.cpp — GIPHY GIF search and selection window (grid layout)
 */

#include "GifPickerWindow.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <Button.h>
#include <DataIO.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslatorRoster.h>

#include <cstring>

#include "Constants.h"
#include "GiphyClient.h"


static const uint32 kMsgSearchGif = 'gsrc';
static const uint32 kMsgSearchDone = 'gsdn';
static const uint32 kMsgThumbnailReady = 'gthm';
static const uint32 kMsgSendGif = 'gsnd';
static const uint32 kMsgGridSelect = 'gsel';
static const uint32 kMsgGridInvoke = 'ginv';

static const float kCellPadding = 4.0f;
static const int32 kMaxCells = 20;


// --- GifGridView: custom BView showing thumbnails in a grid ---

class GifGridView : public BView {
public:
	GifGridView()
		:
		BView("gif_grid", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE
			| B_FRAME_EVENTS),
		fCount(0),
		fSelected(-1)
	{
		memset(fCells, 0, sizeof(fCells));
		SetViewColor(ui_color(B_LIST_BACKGROUND_COLOR));
	}

	virtual ~GifGridView()
	{
		for (int32 i = 0; i < fCount; i++)
			delete fCells[i].bitmap;
	}

	virtual void Draw(BRect updateRect)
	{
		rgb_color bg = ui_color(B_LIST_BACKGROUND_COLOR);
		SetLowColor(bg);

		for (int32 i = 0; i < fCount; i++) {
			BRect cell = _CellRect(i);
			if (!cell.Intersects(updateRect))
				continue;

			if (i == fSelected) {
				// Selected highlight
				rgb_color sel = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
				SetHighColor(sel);
				BRect highlight = cell;
				highlight.InsetBy(-2, -2);
				FillRoundRect(highlight, 4, 4);
			}

			if (fCells[i].bitmap != NULL) {
				// Draw thumbnail fitted in cell, preserving aspect ratio
				BRect src = fCells[i].bitmap->Bounds();
				float srcW = src.Width() + 1;
				float srcH = src.Height() + 1;
				float cellW = cell.Width() + 1;
				float cellH = cell.Height() + 1;

				float scale = std::min(cellW / srcW, cellH / srcH);
				float dstW = srcW * scale;
				float dstH = srcH * scale;
				float ox = cell.left + (cellW - dstW) / 2;
				float oy = cell.top + (cellH - dstH) / 2;

				BRect dst(ox, oy, ox + dstW - 1, oy + dstH - 1);
				SetDrawingMode(B_OP_ALPHA);
				SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
				DrawBitmap(fCells[i].bitmap, src, dst);
				SetDrawingMode(B_OP_COPY);
			} else {
				// Gray placeholder
				SetHighColor(tint_color(bg, B_DARKEN_2_TINT));
				FillRoundRect(cell, 3, 3);
			}
		}
	}

	virtual void MouseDown(BPoint where)
	{
		int32 index = _IndexAt(where);
		if (index >= 0 && index < fCount) {
			fSelected = index;
			Invalidate();

			// Check for double-click
			BMessage* msg = Window()->CurrentMessage();
			int32 clicks = 0;
			if (msg != NULL)
				msg->FindInt32("clicks", &clicks);

			if (clicks >= 2)
				Window()->PostMessage(kMsgGridInvoke);
		}
	}

	virtual void FrameResized(float newWidth, float newHeight)
	{
		(void)newHeight;
		(void)newWidth;
		_UpdateSize();
		Invalidate();
	}

	void Clear()
	{
		for (int32 i = 0; i < fCount; i++) {
			delete fCells[i].bitmap;
			fCells[i].bitmap = NULL;
		}
		fCount = 0;
		fSelected = -1;
		_UpdateSize();
		Invalidate();
	}

	int32 AddCell(const char* id)
	{
		if (fCount >= kMaxCells)
			return -1;
		int32 idx = fCount;
		strlcpy(fCells[idx].id, id, sizeof(fCells[idx].id));
		fCells[idx].bitmap = NULL;
		fCount++;
		_UpdateSize();
		return idx;
	}

	void SetThumbnail(int32 index, BBitmap* bitmap)
	{
		if (index < 0 || index >= fCount)
			return;
		delete fCells[index].bitmap;
		fCells[index].bitmap = bitmap;
		Invalidate(_CellRect(index));
	}

	int32 Selection() const { return fSelected; }

	const char* SelectedId() const
	{
		if (fSelected < 0 || fSelected >= fCount)
			return NULL;
		return fCells[fSelected].id;
	}

private:
	struct GridCell {
		char		id[64];
		BBitmap*	bitmap;
	};

	GridCell		fCells[kMaxCells];
	int32			fCount;
	int32			fSelected;

	int32 _Columns() const
	{
		float w = Bounds().Width() + 1;
		float cellSize = _CellSize();
		int32 cols = (int32)((w + kCellPadding) / (cellSize + kCellPadding));
		return (cols < 1) ? 1 : cols;
	}

	float _CellSize() const
	{
		// Target ~120px cells (20% larger than 100px GIPHY stills)
		return 120.0f;
	}

	BRect _CellRect(int32 index) const
	{
		int32 cols = _Columns();
		float cellSize = _CellSize();
		// Center the grid horizontally
		float totalW = cols * cellSize + (cols - 1) * kCellPadding;
		float offsetX = (Bounds().Width() + 1 - totalW) / 2;
		if (offsetX < kCellPadding)
			offsetX = kCellPadding;

		int32 col = index % cols;
		int32 row = index / cols;

		float x = offsetX + col * (cellSize + kCellPadding);
		float y = kCellPadding + row * (cellSize + kCellPadding);

		return BRect(x, y, x + cellSize - 1, y + cellSize - 1);
	}

	int32 _IndexAt(BPoint point) const
	{
		for (int32 i = 0; i < fCount; i++) {
			if (_CellRect(i).Contains(point))
				return i;
		}
		return -1;
	}

	void _UpdateSize()
	{
		if (fCount == 0) {
			SetExplicitMinSize(BSize(B_SIZE_UNSET, 0));
			return;
		}

		int32 cols = _Columns();
		int32 rows = (fCount + cols - 1) / cols;
		float cellSize = _CellSize();
		float totalH = rows * (cellSize + kCellPadding) + kCellPadding;

		// Resize the view to fit content so scroll bars work
		BRect bounds = Bounds();
		ResizeTo(bounds.Width(), totalH);

		// Update scroll bar range
		BScrollBar* vbar = ScrollBar(B_VERTICAL);
		if (vbar != NULL) {
			BRect parent = Parent()->Bounds();
			float dataHeight = totalH;
			float viewHeight = parent.Height();
			if (dataHeight > viewHeight)
				vbar->SetRange(0, dataHeight - viewHeight);
			else
				vbar->SetRange(0, 0);
			vbar->SetProportion(viewHeight / dataHeight);
			vbar->SetSteps(cellSize / 4, cellSize + kCellPadding);
		}
	}
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
	BWindow(BRect(200, 200, 600, 650), "GIPHY",
		B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fSearchThread(-1),
	fResultCount(0)
{
	memset(fResults, 0, sizeof(fResults));

	fSearchField = new BTextControl("search", "Search:", "",
		new BMessage(kMsgSearchGif));

	fGridView = new GifGridView();
	BScrollView* scrollView = new BScrollView("gif_scroll", fGridView,
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

	// Load trending on open
	_LoadTrending();
}


GifPickerWindow::~GifPickerWindow()
{
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

			GifGridView* grid = dynamic_cast<GifGridView*>(fGridView);
			if (grid == NULL)
				break;

			grid->Clear();

			for (int32 i = 0; i < count; i++) {
				grid->AddCell(fResults[i].id);

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

			GifGridView* grid = dynamic_cast<GifGridView*>(fGridView);
			if (grid != NULL)
				grid->SetThumbnail(index, bitmap);
			else
				delete bitmap;
			break;
		}

		case kMsgGridInvoke:
		case kMsgSendGif:
		{
			GifGridView* grid = dynamic_cast<GifGridView*>(fGridView);
			if (grid == NULL)
				break;

			const char* id = grid->SelectedId();
			if (id == NULL)
				break;

			BMessage msg(MSG_GIF_SELECTED);
			msg.AddString("gif_id", id);
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
	if (fSearchThread >= 0)
		return;

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

	fSearchThread = -1;
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
	(void)data;
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
		}
	}

	free(imgData);
	delete ctx;
	return 0;
}
