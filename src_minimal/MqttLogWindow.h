/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttLogWindow.h — Rich MQTT debug log window
 */

#ifndef MQTTLOGWINDOW_H
#define MQTTLOGWINDOW_H

#include <ObjectList.h>
#include <String.h>
#include <Window.h>

#include <ctime>

class BButton;
class BCheckBox;
class BMenuField;
class BPopUpMenu;
class BScrollView;
class BStringView;
class BTextControl;
class BTextView;


struct MqttLogEntry {
	int32		type;
	time_t		timestamp;
	BString		text;

	MqttLogEntry(int32 t, time_t ts, const char* txt)
		: type(t), timestamp(ts), text(txt) {}
};


class MqttLogWindow : public BWindow {
public:
						MqttLogWindow();
	virtual				~MqttLogWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();
	virtual void		Show();

			void		AddLogEntry(int32 type, const char* text);
			void		SetMqttStatus(bool connected);

private:
			void		_AppendStyledEntry(MqttLogEntry* entry);
			void		_RebuildLog();
			bool		_MatchesFilter(MqttLogEntry* entry);
			void		_UpdateStatusBar();
			rgb_color	_ColorForType(int32 type);
			const char*	_TagForType(int32 type);
			void		_PruneEntries();

			BTextView*		fLogView;
			BScrollView*	fScrollView;
			BPopUpMenu*		fFilterMenu;
			BMenuField*		fFilterField;
			BTextControl*	fSearchField;
			BCheckBox*		fAutoScroll;
			BStringView*	fStatusView;
			BStringView*	fMsgCountView;
			BStringView*	fErrCountView;
			BStringView*	fUptimeView;
			BButton*		fClearButton;

			BObjectList<MqttLogEntry, true>	fEntries;
			int32			fCurrentFilter;
			int32			fMsgCount;
			int32			fErrCount;
			time_t			fConnectTime;
			bool			fIsConnected;

	static const int32	kMaxEntries = 2000;
};

#endif // MQTTLOGWINDOW_H
