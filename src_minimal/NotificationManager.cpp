/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * NotificationManager.cpp — Desktop notification handler implementation
 */

#include "NotificationManager.h"

#include <Notification.h>

#include <cstdio>

#include "Constants.h"


NotificationManager* NotificationManager::sInstance = NULL;


NotificationManager*
NotificationManager::Instance()
{
	if (sInstance == NULL)
		sInstance = new NotificationManager();
	return sInstance;
}


void
NotificationManager::Destroy()
{
	delete sInstance;
	sInstance = NULL;
}


NotificationManager::NotificationManager()
	:
	fEnabled(true),
	fNotificationCount(0)
{
}


NotificationManager::~NotificationManager()
{
}


void
NotificationManager::NotifyNewMessage(const char* senderName,
	const char* messageText, bool isChannel)
{
	if (!fEnabled)
		return;

	BString title;
	if (isChannel) {
		title.SetToFormat("Channel message from %s", senderName);
	} else {
		title.SetToFormat("Message from %s", senderName);
	}

	// Truncate message if too long
	BString content(messageText);
	if (content.Length() > 100) {
		content.Truncate(97);
		content.Append("...");
	}

	_SendNotification(title.String(), content.String(), "message");
}


void
NotificationManager::NotifyConnectionStatus(bool connected, const char* portName)
{
	if (!fEnabled)
		return;

	BString title;
	BString content;

	if (connected) {
		title = "Device connected";
		if (portName != NULL)
			content.SetToFormat("Connected to %s", portName);
		else
			content = "Serial connection established";
	} else {
		title = "Device disconnected";
		content = "Serial connection lost";
	}

	_SendNotification(title.String(), content.String(), "connection");
}


void
NotificationManager::_SendNotification(const char* title, const char* content,
	const char* messageId)
{
	BNotification notification(B_INFORMATION_NOTIFICATION);

	notification.SetGroup(APP_NAME);
	notification.SetTitle(title);
	notification.SetContent(content);

	// Generate unique message ID
	BString msgId;
	if (messageId != NULL)
		msgId.SetToFormat("%s-%s-%d", APP_SIGNATURE, messageId,
			(int)++fNotificationCount);
	else
		msgId.SetToFormat("%s-%d", APP_SIGNATURE, (int)++fNotificationCount);

	notification.SetMessageID(msgId.String());

	// Send notification
	status_t status = notification.Send();
	if (status != B_OK) {
		fprintf(stderr, "Failed to send notification: %s\n", strerror(status));
	}
}
