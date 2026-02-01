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
NotificationManager::NotifyNewContact(const char* contactName)
{
	if (!fEnabled)
		return;

	BString title("New contact discovered");
	BString content;
	content.SetToFormat("%s is now visible on the mesh", contactName);

	_SendNotification(title.String(), content.String(), "contact");
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
NotificationManager::NotifyLoginResult(bool success, const char* targetName)
{
	if (!fEnabled)
		return;

	BString title;
	BString content;

	if (success) {
		title = "Login successful";
		content.SetToFormat("Logged in to %s", targetName);
	} else {
		title = "Login failed";
		content.SetToFormat("Could not log in to %s", targetName);
	}

	_SendNotification(title.String(), content.String(), "login");
}


void
NotificationManager::NotifyMessageDelivered(const char* recipientName)
{
	if (!fEnabled)
		return;

	BString title("Message delivered");
	BString content;
	content.SetToFormat("Your message to %s was received", recipientName);

	_SendNotification(title.String(), content.String(), "delivered");
}


void
NotificationManager::_SendNotification(const char* title, const char* content,
	const char* messageId)
{
	BNotification notification(B_INFORMATION_NOTIFICATION);

	notification.SetGroup(kAppName);
	notification.SetTitle(title);
	notification.SetContent(content);

	// Generate unique message ID
	BString msgId;
	if (messageId != NULL)
		msgId.SetToFormat("%s-%s-%d", kAppSignature, messageId,
			(int)++fNotificationCount);
	else
		msgId.SetToFormat("%s-%d", kAppSignature, (int)++fNotificationCount);

	notification.SetMessageID(msgId.String());

	// Send notification
	status_t status = notification.Send();
	if (status != B_OK) {
		fprintf(stderr, "Failed to send notification: %s\n", strerror(status));
	}
}
