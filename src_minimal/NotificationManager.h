/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * NotificationManager.h — Desktop notification handler
 */

#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <String.h>

class NotificationManager {
public:
	static	NotificationManager* Instance();
	static	void			Destroy();

			void			SetEnabled(bool enabled) { fEnabled = enabled; }
			bool			IsEnabled() const { return fEnabled; }

			void			NotifyNewMessage(const char* senderName,
								const char* messageText, bool isChannel);
			void			NotifyConnectionStatus(bool connected,
								const char* portName = NULL);

private:
							NotificationManager();
							~NotificationManager();

			void			_SendNotification(const char* title,
								const char* content,
								const char* messageId = NULL);

	static	NotificationManager* sInstance;

			bool			fEnabled;
			int32			fNotificationCount;
};

#endif // NOTIFICATIONMANAGER_H
