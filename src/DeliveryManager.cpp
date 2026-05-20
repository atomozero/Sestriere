/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DeliveryManager.cpp — Message delivery queue, retry, and timeout logic
 */

#include "DeliveryManager.h"

#include <Messenger.h>

#include "ContactManager.h"
#include "Constants.h"
#include "DatabaseManager.h"
#include "MainWindow.h"
#include "ProtocolHandler.h"

#include <stdio.h>
#include <string.h>


// Retry timeouts: 15s, 30s, 60s (exponential backoff)
static const bigtime_t kSendTimeouts[] = {
	15000000, 30000000, 60000000
};
static const int32 kMaxRetryAttempts = 3;
static const bigtime_t kConfirmCleanup = 120000000;  // 2 min
static const bigtime_t kLateAckGrace = 30000000;     // 30s grace
static const bigtime_t kDeliveryCheckInterval = 5000000;  // 5s


DeliveryManager::DeliveryManager(BHandler* owner,
	ProtocolHandler* protocol, ContactManager* contacts)
	:
	BHandler("DeliveryManager"),
	fOwner(owner),
	fProtocol(protocol),
	fContactManager(contacts),
	fPendingMessages(true),
	fDeliveryCheckTimer(NULL)
{
}


DeliveryManager::~DeliveryManager()
{
	delete fDeliveryCheckTimer;
}


void
DeliveryManager::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_DELIVERY_ENQUEUE:
		{
			// Extract PendingMessage data from BMessage
			const void* data = NULL;
			ssize_t size = 0;
			if (message->FindData("pending", B_RAW_TYPE, &data, &size) == B_OK
				&& size == sizeof(PendingMessage)) {
				PendingMessage* pm = new PendingMessage(
					*static_cast<const PendingMessage*>(data));
				fPendingMessages.AddItem(pm);
				_StartTimer();
			}
			break;
		}

		case MSG_DELIVERY_CHECK:
			_CheckTimeouts();
			break;

		case MSG_DELIVERY_DRAIN:
			_DrainOutbox();
			break;

		case MSG_DELIVERY_RSP_SENT:
		{
			uint32 ackCode = 0;
			message->FindUInt32("ack_code", &ackCode);
			// Match to most recent unacked pending message
			for (int32 i = fPendingMessages.CountItems() - 1; i >= 0; i--) {
				PendingMessage* pm = fPendingMessages.ItemAt(i);
				if (pm != NULL && !pm->gotRspSent) {
					pm->gotRspSent = true;
					pm->expectedAck = ackCode;
					break;
				}
			}
			break;
		}

		case MSG_DELIVERY_CONFIRMED:
		{
			uint32 ackCode = 0;
			uint32 rtt = 0;
			message->FindUInt32("ack_code", &ackCode);
			message->FindUInt32("rtt", &rtt);
			for (int32 i = fPendingMessages.CountItems() - 1; i >= 0; i--) {
				PendingMessage* pm = fPendingMessages.ItemAt(i);
				if (pm != NULL && pm->gotRspSent
					&& pm->expectedAck == ackCode) {
					_UpdateStatus(pm->contactKey, pm->chatViewIndex,
						DELIVERY_CONFIRMED, rtt);
					// Train predictor
					uint8 pathLen = 0;
					ContactInfo* c = fContactManager->FindByPrefix(
						pm->pubKey, kPubKeyPrefixSize);
					if (c != NULL)
						pathLen = c->outPathLen;
					fTimeoutPredictor.AddObservation(pathLen,
						(uint16)strlen(pm->text), rtt);
					delete fPendingMessages.RemoveItemAt(i);
					break;
				}
			}
			if (fPendingMessages.CountItems() == 0)
				_StopTimer();
			break;
		}

		case MSG_DELIVERY_STOP:
			_StopTimer();
			break;

		default:
			BHandler::MessageReceived(message);
			break;
	}
}


PendingMessage*
DeliveryManager::FindPending(const char* contactKey, uint32 timestamp)
{
	for (int32 i = 0; i < fPendingMessages.CountItems(); i++) {
		PendingMessage* pm = fPendingMessages.ItemAt(i);
		if (pm != NULL && pm->timestamp == timestamp
			&& strcmp(pm->contactKey, contactKey) == 0)
			return pm;
	}
	return NULL;
}


void
DeliveryManager::_StartTimer()
{
	if (fDeliveryCheckTimer != NULL)
		return;

	BMessage msg(MSG_DELIVERY_CHECK);
	fDeliveryCheckTimer = new BMessageRunner(
		BMessenger(this, Looper()), &msg, kDeliveryCheckInterval);
}


void
DeliveryManager::_StopTimer()
{
	delete fDeliveryCheckTimer;
	fDeliveryCheckTimer = NULL;
}


void
DeliveryManager::_CheckTimeouts()
{
	bigtime_t now = system_time();

	for (int32 i = fPendingMessages.CountItems() - 1; i >= 0; i--) {
		PendingMessage* pending = fPendingMessages.ItemAt(i);
		if (pending == NULL || pending->attemptCount == 0)
			continue;

		bigtime_t elapsed = now - pending->sentTime;

		bigtime_t timeout;
		if (fTimeoutPredictor.IsTrained()) {
			ContactInfo* c = fContactManager->FindByPrefix(
				pending->pubKey, kPubKeyPrefixSize);
			uint8 pathLen = (c != NULL) ? c->outPathLen : 0;
			uint16 msgBytes = (uint16)strlen(pending->text);
			timeout = fTimeoutPredictor.PredictTimeout(pathLen, msgBytes);
		} else {
			int32 idx = pending->attemptCount - 1;
			if (idx < 0) idx = 0;
			if (idx > 2) idx = 2;
			timeout = kSendTimeouts[idx];
		}

		if (pending->inGracePeriod) {
			if ((now - pending->graceStartTime) > kLateAckGrace) {
				_FailMessage(pending);
				delete fPendingMessages.RemoveItemAt(i);
			}
			continue;
		}

		if (!pending->gotRspSent && elapsed > timeout) {
			if (pending->attemptCount < kMaxRetryAttempts) {
				_RetryMessage(pending);
			} else {
				pending->inGracePeriod = true;
				pending->graceStartTime = now;
				_Log("INFO", "Max retries reached — waiting 30s for late ACK");
			}
		} else if (pending->gotRspSent && elapsed > kConfirmCleanup) {
			delete fPendingMessages.RemoveItemAt(i);
		}
	}

	if (fPendingMessages.CountItems() == 0)
		_StopTimer();
}


void
DeliveryManager::_DrainOutbox()
{
	if (fProtocol == NULL)
		return;

	int32 sent = 0;
	for (int32 i = 0; i < fPendingMessages.CountItems(); i++) {
		PendingMessage* pending = fPendingMessages.ItemAt(i);
		if (pending == NULL || pending->attemptCount > 0)
			continue;

		const char* sendText = pending->wireLen > 0
			? pending->wireText : pending->text;
		size_t sendLen = pending->wireLen > 0
			? pending->wireLen : strlen(pending->text);
		status_t result = fProtocol->SendDM(pending->pubKey,
			pending->txtType, pending->timestamp, sendText, sendLen);
		if (result == B_OK) {
			pending->attemptCount = 1;
			pending->sentTime = system_time();
			pending->gotRspSent = false;

			BMessage flash(MSG_DELIVERY_TX_FLASH);
			if (fOwner != NULL && fOwner->Looper() != NULL)
				BMessenger(fOwner, fOwner->Looper()).SendMessage(&flash);
			sent++;
		}
	}

	if (sent > 0) {
		_Log("INFO", BString().SetToFormat(
			"Outbox: sent %d queued message(s)", sent).String());
		_StartTimer();
	}
}


void
DeliveryManager::_RetryMessage(PendingMessage* pending)
{
	pending->attemptCount++;
	pending->sentTime = system_time();
	pending->gotRspSent = false;

	_Log("INFO", BString().SetToFormat(
		"Retrying message (attempt %d/%d)",
		(int)pending->attemptCount, kMaxRetryAttempts).String());

	_UpdateStatus(pending->contactKey, pending->chatViewIndex,
		DELIVERY_RETRYING, 0, pending->attemptCount);

	// Path diversity: reset path before retry
	if (pending->attemptCount > 1 && fProtocol != NULL)
		fProtocol->SendResetPath(pending->pubKey);

	const char* retryText = pending->wireLen > 0
		? pending->wireText : pending->text;
	size_t retryLen = pending->wireLen > 0
		? pending->wireLen : strlen(pending->text);
	uint8 wireAttempt = (pending->attemptCount > 1)
		? (uint8)(pending->attemptCount - 1) : 0;

	if (fProtocol == NULL
		|| fProtocol->SendDM(pending->pubKey, pending->txtType,
			pending->timestamp, retryText, retryLen,
			wireAttempt) != B_OK) {
		_Log("ERROR", "Retry send failed — not connected");
		_FailMessage(pending);
		int32 idx = fPendingMessages.IndexOf(pending);
		if (idx >= 0)
			delete fPendingMessages.RemoveItemAt(idx);
		if (fPendingMessages.CountItems() == 0)
			_StopTimer();
		return;
	}

	BMessage flash(MSG_DELIVERY_TX_FLASH);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&flash);
}


void
DeliveryManager::_FailMessage(PendingMessage* pending)
{
	_Log("ERROR", BString().SetToFormat(
		"Message delivery failed after %d attempt(s)",
		(int)pending->attemptCount).String());

	_UpdateStatus(pending->contactKey, pending->chatViewIndex,
		DELIVERY_FAILED);

	// Update in-memory ChatMessage
	ContactInfo* contact = fContactManager->FindByPrefix(
		pending->pubKey, kPubKeyPrefixSize);
	if (contact != NULL) {
		for (int32 i = contact->messages.CountItems() - 1; i >= 0; i--) {
			ChatMessage* msg = contact->messages.ItemAt(i);
			if (msg != NULL && msg->isOutgoing
				&& msg->timestamp == pending->timestamp
				&& msg->deliveryStatus != DELIVERY_CONFIRMED
				&& msg->deliveryStatus != DELIVERY_SENT) {
				msg->deliveryStatus = DELIVERY_FAILED;
				break;
			}
		}
	}

	DatabaseManager::Instance()->UpdateMessageDeliveryStatus(
		pending->contactKey, pending->timestamp, DELIVERY_FAILED, 0);
}


void
DeliveryManager::_Log(const char* prefix, const char* text)
{
	BMessage msg(MSG_DELIVERY_LOG);
	msg.AddString("prefix", prefix);
	msg.AddString("text", text);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&msg);
}


void
DeliveryManager::_UpdateStatus(const char* contactKey, int32 chatIdx,
	uint8 status, uint32 rtt, uint8 retryCount)
{
	BMessage msg(MSG_DELIVERY_UPDATE_STATUS);
	msg.AddString("contact_key", contactKey);
	msg.AddInt32("index", chatIdx);
	msg.AddUInt8("status", status);
	msg.AddUInt32("rtt", rtt);
	msg.AddUInt8("retry_count", retryCount);
	if (fOwner != NULL && fOwner->Looper() != NULL)
		BMessenger(fOwner, fOwner->Looper()).SendMessage(&msg);
}
