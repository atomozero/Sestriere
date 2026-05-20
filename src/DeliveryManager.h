/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * DeliveryManager.h — Message delivery queue, retry, and timeout logic
 */

#ifndef _DELIVERY_MANAGER_H
#define _DELIVERY_MANAGER_H

#include <Handler.h>
#include <MessageRunner.h>

#include "TimeoutPredictor.h"
#include "Types.h"

class ContactManager;
class ProtocolHandler;
struct PendingMessage;


// Messages from DeliveryManager → MainWindow (UI updates)
enum {
	MSG_DELIVERY_LOG			= 'dlg0',	// "prefix" + "text"
	MSG_DELIVERY_UPDATE_STATUS	= 'dlg1',	// "contact_key" + "index" + "status"
	MSG_DELIVERY_TX_FLASH		= 'dlg2',	// flash TX LED
	MSG_DELIVERY_QUEUE_EMPTY	= 'dlg3',	// queue drained
};

// Messages MainWindow → DeliveryManager
enum {
	MSG_DELIVERY_ENQUEUE		= 'dlen',	// add PendingMessage to queue
	MSG_DELIVERY_CHECK			= 'dlck',	// periodic timeout check
	MSG_DELIVERY_DRAIN			= 'dldr',	// flush offline queue
	MSG_DELIVERY_RSP_SENT		= 'dlrs',	// RSP_SENT received
	MSG_DELIVERY_CONFIRMED		= 'dlcf',	// PUSH_SEND_CONFIRMED received
	MSG_DELIVERY_STOP			= 'dlst',	// stop timer (disconnect)
};


class DeliveryManager : public BHandler {
public:
							DeliveryManager(BHandler* owner,
								ProtocolHandler* protocol,
								ContactManager* contacts);
	virtual					~DeliveryManager();

	virtual void			MessageReceived(BMessage* message);

	// Direct access for MainWindow queries
	int32					PendingCount() const
								{ return fPendingMessages.CountItems(); }
	PendingMessage*			FindPending(const char* contactKey,
								uint32 timestamp);

	void					SetProtocol(ProtocolHandler* p)
								{ fProtocol = p; }

private:
			void			_StartTimer();
			void			_StopTimer();
			void			_CheckTimeouts();
			void			_DrainOutbox();
			void			_RetryMessage(PendingMessage* pending);
			void			_FailMessage(PendingMessage* pending);
			void			_Log(const char* prefix, const char* text);
			void			_UpdateStatus(const char* contactKey,
								int32 chatIdx, uint8 status,
								uint32 rtt = 0, uint8 retryCount = 0);

			BHandler*		fOwner;
			ProtocolHandler* fProtocol;
			ContactManager*	fContactManager;

			OwningObjectList<PendingMessage>	fPendingMessages;
			BMessageRunner*	fDeliveryCheckTimer;
			TimeoutPredictor fTimeoutPredictor;
};


#endif // _DELIVERY_MANAGER_H
