/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TelemetryWindow.h — Sensor telemetry dashboard window (card-based redesign)
 */

#ifndef TELEMETRYWINDOW_H
#define TELEMETRYWINDOW_H

#include <Button.h>
#include <MessageRunner.h>
#include "Compat.h"
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>

#include <cfloat>


// Message codes
enum {
	MSG_TELEMETRY_TIMER			= 'tltm',
	MSG_TELEMETRY_EXPORT		= 'tlex',
	MSG_TELEMETRY_CLEAR_HISTORY	= 'tlch',
	MSG_TELEMETRY_CARD_CLICKED	= 'tlcc',
	MSG_TELEMETRY_TIME_RANGE	= 'tltr'
};


// Telemetry data point
struct TelemetryDataPoint {
	bigtime_t	timestamp;
	float		value;
};


// Sensor types
enum SensorType {
	SENSOR_TEMPERATURE	= 0,
	SENSOR_HUMIDITY		= 1,
	SENSOR_PRESSURE		= 2,
	SENSOR_BATTERY		= 3,
	SENSOR_ALTITUDE		= 4,
	SENSOR_LIGHT		= 5,
	SENSOR_CO2			= 6,
	SENSOR_CUSTOM		= 7
};


// Forward declarations for local views
class SensorCardView;
class TelemetryChartView;


// Sensor info — owns history data and back-pointer to card view
struct SensorInfo {
	BString		name;
	BString		unit;
	BString		displayName;
	SensorType	type;
	uint32		nodeId;
	float		currentValue;
	float		minValue;
	float		maxValue;
	float		avgValue;
	rgb_color	color;
	SensorCardView*	cardView;

	// Sparkline data (last 20 points, pre-normalized 0..1)
	float		sparkline[20];
	int32		sparklineCount;

	OwningObjectList<TelemetryDataPoint>	history;

	SensorInfo()
		: type(SENSOR_CUSTOM),
		  nodeId(0),
		  currentValue(0),
		  minValue(FLT_MAX),
		  maxValue(-FLT_MAX),
		  avgValue(0),
		  cardView(NULL),
		  sparklineCount(0)
	{
		color = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		memset(sparkline, 0, sizeof(sparkline));
	}
};


// Main telemetry window — card-based dashboard
class TelemetryWindow : public BWindow {
public:
						TelemetryWindow(BWindow* parent);
	virtual				~TelemetryWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

	// Public API (preserved)
	void				AddTelemetryData(uint32 nodeId,
							const BString& sensorName,
							SensorType type, float value,
							const BString& unit,
							const char* contactName = NULL);
	void				LoadHistoryFromDB();
	void				ClearAllData();

private:
	void				_BuildLayout();
	void				_RebuildContent();
	void				_SelectSensor(SensorInfo* sensor);
	void				_DeselectSensor();
	void				_UpdateSparkline(SensorInfo* sensor);
	void				_UpdateFooter();
	void				_ExportData();
	SensorInfo*			_FindOrCreateSensor(uint32 nodeId,
							const BString& name, SensorType type,
							const BString& unit);
	rgb_color			_ColorForSensorType(SensorType type);

	BWindow*			fParent;
	BMessageRunner*		fRefreshRunner;

	// UI components
	BView*				fContentView;
	BScrollView*		fContentScroll;
	TelemetryChartView*	fChartView;
	BStringView*		fFooterLabel;
	BStringView*		fSummaryLabel;

	// Time range toolbar
	BButton*			fTimeRangeButtons[7];
	int32				fActiveTimeRange;
	BButton*			fExportButton;
	BButton*			fClearButton;
	BButton*			fLoadHistoryButton;
	BButton*			fRequestAllButton;

	// Data
	SensorInfo*			fSelectedSensor;
	bigtime_t			fCurrentTimeRange;
	bigtime_t			fLastDataTime;
	OwningObjectList<SensorInfo>	fSensors;
	bool				fNeedsRebuild;
};


#endif // TELEMETRYWINDOW_H
