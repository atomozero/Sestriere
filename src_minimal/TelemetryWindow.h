/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TelemetryWindow.h — Sensor telemetry dashboard window
 */

#ifndef TELEMETRYWINDOW_H
#define TELEMETRYWINDOW_H

#include <Button.h>
#include <MessageRunner.h>
#include <ObjectList.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>

#include <cfloat>


// Message codes
enum {
	MSG_TELEMETRY_REFRESH		= 'tlrf',
	MSG_TELEMETRY_TIMER			= 'tltm',
	MSG_TELEMETRY_SELECT_SENSOR	= 'tlss',
	MSG_TELEMETRY_EXPORT		= 'tlex',
	MSG_TELEMETRY_CLEAR_HISTORY	= 'tlch'
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


// Sensor info
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
	BObjectList<TelemetryDataPoint, true>	history;

	SensorInfo()
		: type(SENSOR_CUSTOM),
		  nodeId(0),
		  currentValue(0),
		  minValue(FLT_MAX),
		  maxValue(-FLT_MAX),
		  avgValue(0)
	{
		color = (rgb_color){100, 150, 255, 255};
	}
};


// Telemetry graph view
class TelemetryGraphView : public BView {
public:
						TelemetryGraphView(BRect frame);
	virtual				~TelemetryGraphView();

	virtual void		Draw(BRect updateRect);
	virtual void		MouseDown(BPoint where);
	virtual void		MouseMoved(BPoint where, uint32 transit,
							const BMessage* dragMessage);

	void				SetSensor(SensorInfo* sensor);
	void				SetTimeRange(bigtime_t range);

private:
	void				_DrawGrid();
	void				_DrawGraph();
	void				_DrawCursor();
	void				_DrawLegend();

	float				_ValueToY(float value);

	SensorInfo*			fSensor;
	bigtime_t			fTimeRange;
	BPoint				fCursorPos;
	bool				fShowCursor;

	BRect				fGraphRect;
	float				fMarginLeft;
	float				fMarginRight;
	float				fMarginTop;
	float				fMarginBottom;
};


// Sensor list item view
class TelemetrySensorView : public BView {
public:
						TelemetrySensorView(BRect frame, SensorInfo* sensor);
	virtual				~TelemetrySensorView();

	virtual void		Draw(BRect updateRect);
	virtual void		MouseDown(BPoint where);

	void				SetSelected(bool selected);
	void				UpdateValue();

private:
	SensorInfo*			fSensor;
	bool				fSelected;
};


// Main telemetry window
class TelemetryWindow : public BWindow {
public:
						TelemetryWindow(BWindow* parent);
	virtual				~TelemetryWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

	void				AddTelemetryData(uint32 nodeId,
							const BString& sensorName,
							SensorType type, float value,
							const BString& unit,
							const char* contactName = NULL);
	void				LoadHistoryFromDB();
	void				ClearAllData();

private:
	void				_BuildLayout();
	void				_UpdateSensorList();
	void				_SelectSensor(int32 index);
	void				_UpdateStats();
	void				_ExportData();
	SensorInfo*			_FindOrCreateSensor(uint32 nodeId,
							const BString& name, SensorType type,
							const BString& unit);
	rgb_color			_ColorForSensorType(SensorType type);

	BWindow*			fParent;
	BMessageRunner*		fRefreshRunner;

	// UI components
	BView*				fSensorListView;
	BScrollView*		fSensorScrollView;
	TelemetryGraphView*	fGraphView;

	// Stats display
	BStringView*		fCurrentValueView;
	BStringView*		fMinValueView;
	BStringView*		fMaxValueView;
	BStringView*		fAvgValueView;
	BStringView*		fNodeIdView;

	// Buttons
	BButton*			fRange1MinButton;
	BButton*			fRange5MinButton;
	BButton*			fRange15MinButton;
	BButton*			fRange1HourButton;
	BButton*			fRange6HourButton;
	BButton*			fRange24HourButton;
	BButton*			fRange7DayButton;
	BButton*			fExportButton;
	BButton*			fClearButton;
	BButton*			fLoadHistoryButton;
	BButton*			fRequestAllButton;

	// Data
	int32				fSelectedSensor;
	bigtime_t			fCurrentTimeRange;
	BObjectList<SensorInfo, true>	fSensors;
};


#endif // TELEMETRYWINDOW_H
