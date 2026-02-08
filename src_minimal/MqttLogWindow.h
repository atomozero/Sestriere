/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * MqttLogWindow.h — MQTT message log window
 */

#ifndef MQTTLOGWINDOW_H
#define MQTTLOGWINDOW_H

#include <Window.h>

class BButton;
class BScrollView;
class BTextView;

class MqttLogWindow : public BWindow {
public:
						MqttLogWindow();
	virtual				~MqttLogWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

			void		AddLogEntry(const char* entry);
			void		SetMqttStatus(bool connected);

private:
			void		_PruneLines();

			BTextView*	fLogView;
			BButton*	fClearButton;

	static const int32	kMaxLines = 500;
};

#endif // MQTTLOGWINDOW_H
