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

#include "mui/bitmap.h"
#include "mui/font.h"
#include "mui/mui.h"

STATIC struct MinList schedule_list =
{
	(APTR)&schedule_list.mlh_Tail,
	NULL,
	(APTR)&schedule_list.mlh_Head
};

STATIC struct MsgPort *msgport;
STATIC struct timerequest tioreq;
STATIC UBYTE got_timer_device;

ULONG schedule_sig;

struct nscallback
{
	struct MinNode node;
	struct timerequest treq;
	void (*callback)(void *p);
	void *p;
};


static void remove_timer_event(struct nscallback *nscb)
{
	AbortIO((struct IORequest *)&nscb->treq);
	REMOVE(nscb);
	WaitIO((struct IORequest *)&nscb->treq);
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

void schedule(int t, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	nscb = AllocMem(sizeof(*nscb), MEMF_ANY);

	if (nscb) {
		t *= 10000;

		nscb->callback = callback;
		nscb->p = p;

		nscb->treq.tr_node.io_Message.mn_ReplyPort =
				tioreq.tr_node.io_Message.mn_ReplyPort;
		nscb->treq.tr_node.io_Device  = tioreq.tr_node.io_Device;
		nscb->treq.tr_node.io_Unit    = tioreq.tr_node.io_Unit;
		nscb->treq.tr_node.io_Command = TR_ADDREQUEST;
		nscb->treq.tr_time.tv_secs  = t / 1000000;
		nscb->treq.tr_time.tv_micro = t % 1000000;

  		SendIO((struct IORequest *)&nscb->treq);

		ADDTAIL(&schedule_list, nscb);
	}
}

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */

void schedule_remove(void (*callback)(void *p), void *p)
{
	struct nscallback *nscb, *next;

	ITERATELISTSAFE(nscb, next, &schedule_list) {
		if ((nscb->callback == callback) && (nscb->p == p))
			remove_timer_event(nscb);
	}
}

static void schedule_add_cache_timer(struct timerequest *req)
{
	req->tr_node.io_Command = TR_ADDREQUEST;
	req->tr_time.tv_secs  = 5 * 60;
	req->tr_time.tv_micro = 0;
	SendIO((struct IORequest *)req);
}

/**
 * Poll events
 *
 * Process events up to current time.
 */

VOID mui_schedule_poll(void)
{
	APTR msg;

	while ((msg = GetMsg(msgport))) {
		if (msg == &tioreq.tr_node.io_Message) {
			bitmap_cache_check();
			font_cache_check();
			schedule_add_cache_timer(msg);
		} else {
			struct nscallback *nscb = (APTR)((IPTR)msg -
					offsetof(struct nscallback, treq));
			nscb->callback(nscb->p);
			REMOVE(nscb);
			FreeMem(nscb, sizeof(*nscb));
		}
	}
}

/**
 * Initialise
 */

BOOL mui_schedule_init(void)
{
	BOOL rc;

	msgport = CreateMsgPort();
	rc = FALSE;

	if (msgport) {
		tioreq.tr_node.io_Message.mn_ReplyPort = msgport;

		/* UNIT_VBLANK is very cheap but has low resolution (1/50s)
		 *
		 * UNIT_MICROHZ has better accuracy but costs more
		 */

		if (OpenDevice("timer.device", UNIT_MICROHZ, &tioreq.tr_node, 0) == 0) {
			got_timer_device = 1;
			schedule_sig = 1 << msgport->mp_SigBit;
			rc = TRUE;

			schedule_add_cache_timer(&tioreq);
		}
	}

	return rc;
}

/**
 * Abort all events and quit
 */

void mui_schedule_finalise(void)
{
	if (got_timer_device) {
		struct nscallback *nscb, *next;

		ITERATELISTSAFE(nscb, next, &schedule_list) {
			remove_timer_event(nscb);
		}

		AbortIO((struct IORequest *)&tioreq);
		WaitIO((struct IORequest *)&tioreq);
		CloseDevice(&tioreq.tr_node);
	}

	DeleteMsgPort(msgport);
}
