/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * LoSWindow.h — Line-of-Sight terrain profile analysis window
 */

#ifndef LOSWINDOW_H
#define LOSWINDOW_H

#include <Window.h>
#include "Compat.h"

#include "LoSAnalysis.h"

class BStringView;
class BView;


class LoSWindow : public BWindow {
public:
						LoSWindow(BWindow* parent);
	virtual				~LoSWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

			void		SetEndpoints(double startLat, double startLon,
							const char* startName,
							double endLat, double endLon,
							const char* endName);
			void		SetFrequency(double freqHz);
			void		StartAnalysis();

private:
	static	int32		_FetchThread(void* data);

			BWindow*	fParent;
			BView*		fProfileView;		// Custom drawing view (ProfileView)
			BStringView* fTitleLabel;
			BStringView* fStatusLabel;

			// Endpoints
			double		fStartLat;
			double		fStartLon;
			double		fEndLat;
			double		fEndLon;
			char		fStartName[64];
			char		fEndName[64];
			double		fFreqHz;

			// Terrain data (owned)
			TerrainPoint*	fPoints;
			int32		fPointCount;
			LoSResult	fResult;
			bool		fHasData;
			bool		fFetching;
			thread_id	fFetchThreadId;
};


#endif // LOSWINDOW_H
