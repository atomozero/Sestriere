/*
 * Sestriere - MeshCore Client for Haiku OS
 * TelemetryWindow.h - Sensor telemetry dashboard window
 */

#ifndef TELEMETRY_WINDOW_H
#define TELEMETRY_WINDOW_H

#include <Window.h>
#include <View.h>
#include <ListView.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <Button.h>
#include <TabView.h>
#include <MessageRunner.h>
#include <ObjectList.h>

#include <cfloat>

#include "Types.h"

// Forward declarations
class TelemetryGraphView;
class TelemetrySensorView;

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
		color = {100, 150, 255, 255};
	}

	~SensorInfo()
	{
		// BObjectList will delete items
	}
};


// Telemetry graph view - displays sensor history as a line graph
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
	bigtime_t			_XToTime(float x);
	float				_TimeToX(bigtime_t time);

	SensorInfo*			fSensor;
	bigtime_t			fTimeRange;		// in microseconds
	BPoint				fCursorPos;
	bool				fShowCursor;

	BRect				fGraphRect;		// actual graph area
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
	bool				IsSelected() const { return fSelected; }
	SensorInfo*			Sensor() const { return fSensor; }
	void				UpdateValue();

private:
	SensorInfo*			fSensor;
	bool				fSelected;
};


// Main telemetry window
class TelemetryWindow : public BWindow {
public:
						TelemetryWindow(BRect frame, BMessenger target);
	virtual				~TelemetryWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

	void				AddTelemetryData(uint32 nodeId, const BString& sensorName,
							SensorType type, float value, const BString& unit);
	void				ClearAllData();

private:
	void				_BuildLayout();
	void				_UpdateSensorList();
	void				_SelectSensor(int32 index);
	void				_UpdateStats();
	void				_ExportData();
	SensorInfo*			_FindOrCreateSensor(uint32 nodeId, const BString& name,
							SensorType type, const BString& unit);
	rgb_color			_ColorForSensorType(SensorType type);
	const char*			_IconForSensorType(SensorType type);

	BMessenger			fTarget;
	BMessageRunner*		fRefreshRunner;

	// UI components
	BView*				fRootView;
	BView*				fSensorListView;
	BScrollView*		fSensorScrollView;
	TelemetryGraphView*	fGraphView;

	// Stats display
	BStringView*		fCurrentValueView;
	BStringView*		fMinValueView;
	BStringView*		fMaxValueView;
	BStringView*		fAvgValueView;
	BStringView*		fNodeIdView;

	// Time range buttons
	BButton*			fRange1MinButton;
	BButton*			fRange5MinButton;
	BButton*			fRange15MinButton;
	BButton*			fRange1HourButton;

	// Control buttons
	BButton*			fExportButton;
	BButton*			fClearButton;

	// Data
	int32				fSelectedSensor;
	bigtime_t			fCurrentTimeRange;
	BObjectList<SensorInfo, true>	fSensors;
};


#endif // TELEMETRY_WINDOW_H
