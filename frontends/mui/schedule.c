/*
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>

#include <proto/exec.h>
#include <stdint.h>

#include "mui/bitmap.h"
#include "mui/font.h"
#include "mui/mui.h"

// Dodaj na początku pliku
#include "utils/errors.h"
#include "utils/log0.h"
#include "mui/schedule.h"

#define SCH_LOG(fmt, ...) NSLOG(netsurf, INFO, "mui_schedule: " fmt, ##__VA_ARGS__)
#define SCH_LOG2(fmt, ...) NSLOG(netsurf, DEBUG, "mui_schedule: " fmt, ##__VA_ARGS__)

STATIC struct MinList schedule_list =
{
	(APTR)&schedule_list.mlh_Tail,
	NULL,
	(APTR)&schedule_list.mlh_Head
};

STATIC struct MsgPort *msgport;
STATIC struct timerequest tioreq;
STATIC UBYTE got_timer_device;

unsigned long schedule_sig;

struct nscallback
{
	struct MinNode node;
	struct timerequest treq;
	void (*callback)(void *p);
	void *p;
};

static bool schedule_remove(void (*callback)(void *p), void *p);

bool mui_schedule_has_tasks(void)
{
	return !ISLISTEMPTY(&schedule_list);
}

static void remove_timer_event(struct nscallback *nscb)
{
	if (nscb == NULL) {
		return;
	}

	SCH_LOG2("remove_timer_event: nscb=%p callback=%p ctx=%p", nscb,
			nscb->callback, nscb->p);

	if (CheckIO((struct IORequest *)&nscb->treq) == 0) {
		SCH_LOG2("remove_timer_event: abort outstanding IO");
		AbortIO((struct IORequest *)&nscb->treq);
		WaitIO((struct IORequest *)&nscb->treq);
	} else {
		SCH_LOG2("remove_timer_event: IO already complete");
		WaitIO((struct IORequest *)&nscb->treq);
	}

	REMOVE(nscb);
	FreeMem(nscb, sizeof(*nscb));
}

/**
 * Schedule a callback.
 *
 * \param  t         interval before the callback should be made / cs
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * The callback function will be called as soon as possible after t cs have
 * passed.
 */

nserror mui_schedule(int t, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;
	uint64_t ticks64;
	uint32_t ticks;
	
	SCH_LOG2("mui_schedule: request delay_ms=%d callback=%p ctx=%p", t, callback, p);
	write_to_log("mui_schedule: request delay_ms=%d callback=%p ctx=%p\n", t, callback, p);

	if (callback == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	if ((msgport == NULL) || (got_timer_device == 0)) {
		return NSERROR_INVALID;
	}

	if (t < 0) {
		/* cancel any existing callback */
		bool removed = schedule_remove(callback, p);
		SCH_LOG2("mui_schedule: cancel request callback=%p ctx=%p removed=%d",
			callback, p, removed);
		write_to_log("mui_schedule: cancel callback=%p ctx=%p removed=%d\n",
		    callback, p, removed);
		return NSERROR_OK;
	}

	if (t == 0) {
		/* make sure we wait at least one tick */
		t = 1;
	}

	/* reset existing timer for this callback/context */
	bool rescheduled = schedule_remove(callback, p);
	if (rescheduled) {
		SCH_LOG2("mui_schedule: rescheduling existing callback=%p ctx=%p", callback, p);
		write_to_log("mui_schedule: reschedule callback=%p ctx=%p\n", callback, p);
	}

	nscb = AllocMem(sizeof(*nscb), MEMF_ANY);
	if (nscb == NULL) {
		return NSERROR_NOMEM;
	}

	nscb->callback = callback;
	nscb->p = p;

	/* convert milliseconds to UNIT_VBLANK ticks (20ms granularity) */
	ticks64 = ((uint64_t)t + 19ULL) / 20ULL;
	if (ticks64 == 0) {
		ticks64 = 1;
	} else if (ticks64 > UINT32_MAX) {
		ticks64 = UINT32_MAX;
	}

	ticks = (uint32_t)ticks64;

	nscb->treq.tr_node.io_Message.mn_ReplyPort =
			tioreq.tr_node.io_Message.mn_ReplyPort;
	nscb->treq.tr_node.io_Device  = tioreq.tr_node.io_Device;
	nscb->treq.tr_node.io_Unit    = tioreq.tr_node.io_Unit;
	nscb->treq.tr_node.io_Command = TR_ADDREQUEST;
	nscb->treq.tr_time.tv_secs  = ticks / 50;
	nscb->treq.tr_time.tv_micro = (ticks % 50) * 20000;

	SendIO((struct IORequest *)&nscb->treq);
	ADDTAIL(&schedule_list, nscb);

	SCH_LOG2("mui_schedule: scheduled callback=%p ctx=%p ticks=%u", callback, p, ticks);
	write_to_log("mui_schedule: scheduled callback=%p ctx=%p ticks=%u\n", callback, p, ticks);

	return NSERROR_OK;
}

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

static bool schedule_remove(void (*callback)(void *p), void *p)
{
	struct nscallback *nscb, *next;
 	bool removed = false;

	SCH_LOG2("schedule_remove: START - callback=%p, p=%p", callback, p);

	ITERATELISTSAFE(nscb, next, &schedule_list) {
		SCH_LOG2("schedule_remove: Checking nscb=%p (callback=%p, p=%p)",
		     nscb, nscb->callback, nscb->p);
		     
		if ((nscb->callback == callback) && (nscb->p == p)) {
			SCH_LOG2("schedule_remove: Found matching callback - removing");
			remove_timer_event(nscb);
			removed = true;
		}
	}
	
	SCH_LOG2("schedule_remove: END removed=%d", removed);
	return removed;
}

static void schedule_add_cache_timer(struct timerequest *req)
{
	SCH_LOG2("schedule_add_cache_timer: START");

	if (!req) {
		SCH_LOG2("schedule_add_cache_timer: ERROR - req is NULL");
		return;
	}

	req->tr_node.io_Command = TR_ADDREQUEST;
	req->tr_time.tv_secs  = 5 * 60;
	req->tr_time.tv_micro = 0;

	SCH_LOG2("schedule_add_cache_timer: Calling SendIO");
	SendIO((struct IORequest *)req);

	SCH_LOG2("schedule_add_cache_timer: END");
}

/**
 * Poll events
 *
 * Process events up to current time.
 */

void mui_schedule_poll(void)
{
	APTR msg;

	SCH_LOG2("mui_schedule_poll: START");

	if (!msgport) {
		SCH_LOG2("mui_schedule_poll: ERROR - msgport is NULL");
		return;
	}

	while ((msg = GetMsg(msgport))) {
		SCH_LOG2("mui_schedule_poll: Got message %p", msg);
		write_to_log("mui_schedule_poll: got message=%p\n", msg);
		
		if (msg == &tioreq.tr_node.io_Message) {
			SCH_LOG2("mui_schedule_poll: Cache timer message");
			
			SCH_LOG2("mui_schedule_poll: Calling bitmap_cache_check");
			bitmap_cache_check();
			
			SCH_LOG2("mui_schedule_poll: Calling font_cache_check");
			font_cache_check();
			
			SCH_LOG2("mui_schedule_poll: Rescheduling cache timer");
			schedule_add_cache_timer(msg);
		} else {
			struct nscallback *nscb = (APTR)((IPTR)msg -
				offsetof(struct nscallback, treq));
			void (*callback)(void *) = nscb->callback;
			void *ctx = nscb->p;

			SCH_LOG2("mui_schedule_poll: Callback message - nscb=%p, callback=%p, p=%p",
	     nscb, callback, ctx);
			write_to_log("mui_schedule_poll: callback msg nscb=%p cb=%p ctx=%p\n",
		    nscb, callback, ctx);

			SCH_LOG2("mui_schedule_poll: Removing from list before callback");
			REMOVE(nscb);

			if (callback) {
				SCH_LOG2("mui_schedule_poll: Calling callback");
				callback(ctx);
				SCH_LOG2("mui_schedule_poll: Callback returned");
				write_to_log("mui_schedule_poll: callback %p returned\n", callback);
			} else {
				SCH_LOG2("mui_schedule_poll: ERROR - callback is NULL");
			}

			SCH_LOG2("mui_schedule_poll: Freeing memory");
			FreeMem(nscb, sizeof(*nscb));
		}
	}
	
	SCH_LOG2("mui_schedule_poll: END");
	write_to_log("mui_schedule_poll: end\n");
}

/**
 * Initialise
 */

bool mui_schedule_init(void)
{
	bool rc;

	SCH_LOG2("mui_schedule_init: START");
	SCH_LOG2("mui_schedule_init: addrs mui_schedule=%p schedule_remove=%p remove_timer_event=%p poll=%p",
		mui_schedule,
		schedule_remove,
		remove_timer_event,
		mui_schedule_poll);

	msgport = CreateMsgPort();
	rc = false;

	if (msgport) {
		SCH_LOG2("mui_schedule_init: MsgPort created successfully");
		
		tioreq.tr_node.io_Message.mn_ReplyPort = msgport;

		/* UNIT_VBLANK is very cheap but has low resolution (1/50s)
		 *
		 * UNIT_MICROHZ has better accuracy but costs more
		 */

		SCH_LOG2("mui_schedule_init: Opening timer.device with UNIT_VBLANK");
		if (OpenDevice("timer.device", UNIT_VBLANK, &tioreq.tr_node, 0) == 0) {
			SCH_LOG2("mui_schedule_init: Timer device opened successfully");
			
			got_timer_device = 1;
			schedule_sig = 1 << msgport->mp_SigBit;
			rc = true;

			SCH_LOG2("mui_schedule_init: Adding initial cache timer");
			schedule_add_cache_timer(&tioreq);
		} else {
			SCH_LOG2("mui_schedule_init: ERROR - Failed to open timer.device");
		}
	} else {
		SCH_LOG2("mui_schedule_init: ERROR - Failed to create MsgPort");
	}

	SCH_LOG2("mui_schedule_init: END - returning %s", rc ? "TRUE" : "FALSE");
	return rc;
}

/**
 * Abort all events and quit
 */

void mui_schedule_finalise(void)
{
	SCH_LOG2("mui_schedule_finalise: START");

	if (got_timer_device) {
		struct nscallback *nscb, *next;

		SCH_LOG2("mui_schedule_finalise: Removing all scheduled events");
		ITERATELISTSAFE(nscb, next, &schedule_list) {
			SCH_LOG2("mui_schedule_finalise: Removing event nscb=%p", nscb);
			remove_timer_event(nscb);
		}

		SCH_LOG2("mui_schedule_finalise: Aborting main timer");
		AbortIO((struct IORequest *)&tioreq);
		
		SCH_LOG2("mui_schedule_finalise: Waiting for main timer");
		WaitIO((struct IORequest *)&tioreq);
		
		SCH_LOG2("mui_schedule_finalise: Closing timer device");
		CloseDevice(&tioreq.tr_node);
	}

	if (msgport) {
		SCH_LOG2("mui_schedule_finalise: Deleting MsgPort");
		DeleteMsgPort(msgport);
	}
	
	SCH_LOG2("mui_schedule_finalise: END");
}
