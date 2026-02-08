/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * SNRChartView.h — Historical SNR line chart widget
 */

#ifndef SNRCHARTVIEW_H
#define SNRCHARTVIEW_H

#include <ObjectList.h>
#include <View.h>

#include "DatabaseManager.h"

class SNRChartView : public BView {
public:
						SNRChartView(const char* name);
	virtual				~SNRChartView();

	virtual void		Draw(BRect updateRect);
	virtual void		AttachedToWindow();
	virtual BSize		MinSize();
	virtual BSize		MaxSize();

			void		SetData(const BObjectList<SNRDataPoint, true>& points);
			void		ClearData();

private:
			void		_DrawGrid(BRect chartRect);
			void		_DrawLine(BRect chartRect);
			void		_DrawAxisLabels(BRect chartRect);
			void		_DrawNoDataMessage(BRect chartRect);
			float		_MapY(BRect chartRect, int8 snr) const;
			float		_MapX(BRect chartRect, uint32 timestamp) const;

			struct DataPoint {
				uint32	timestamp;
				int8	snr;
			};

			DataPoint*		fPoints;
			int32			fPointCount;
			int8			fMinSNR;
			int8			fMaxSNR;
			uint32			fMinTime;
			uint32			fMaxTime;
};

#endif // SNRCHARTVIEW_H
