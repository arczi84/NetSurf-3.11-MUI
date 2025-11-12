/*
 * Ambient - the ultimate desktop
 * ------------------------------
 * Copyright 2001-2005 by David Gerber <zapek@morphos.net>
 * Copyright 2006-2007 Ambient Open Source Team
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdlib.h>

#include <stdarg.h>
#include <exec/types.h>

#include <dos/dosextens.h>
#include <exec/execbase.h>
#include <proto/alib.h>
#include <proto/exec.h>

//#include "netsurf/netsurf.h"
#include "mui/mui.h"
#include "mui/netsurf.h"

STATIC struct SignalSemaphore semaphore;
STATIC struct MinList methodlist = { (APTR)&methodlist.mlh_Tail, NULL, (APTR)&methodlist.mlh_Head };

struct pushedmethod
{
	struct MinNode n;
	ULONG size;
	APTR obj;
	struct Message msg;
	ULONG sync;
	ULONG result;
	ULONG m[0];
};


void methodstack_init(void)
{
#ifdef MULTITHREADED
	InitSemaphore(&semaphore);
#endif
}


void methodstack_cleanup(void)
{
#ifdef MULTITHREADED
	struct pushedmethod *pm, *next;

	ITERATELISTSAFE(pm, next, &methodlist)
	{
		FreeMem(pm, pm->size);
	}
#endif
}

#ifdef MULTITHREADED
void methodstack_cleanup_guithread(void)
{
	struct pushedmethod *pm;

	ObtainSemaphore(&semaphore);

	while ((pm = REMHEAD(&methodlist)))
	{
		if (pm->sync)
		{
			pm->result = -1;
			ReplyMsg(&pm->msg);
		}
		else
		{
			FreeMem(pm, pm->size);
		}
	}

	ReleaseSemaphore(&semaphore);
}
#endif

#ifdef MULTITHREADED
void methodstack_check(void)
{
	ULONG not_empty = TRUE;

	do
	{
		struct pushedmethod *pm;

		ObtainSemaphore(&semaphore);
		pm = REMHEAD(&methodlist);
		not_empty = !ISLISTEMPTY(&methodlist);
		ReleaseSemaphore(&semaphore);

		if (!pm)
			break;

		if (pm->sync)
		{
			if (pm->obj)
			{
				pm->result = DoMethodA(pm->obj, (Msg)&pm->m[0]);
			}
			else
			{
				pm->result = -1;
			}

			ReplyMsg(&pm->msg);
		}
		else
		{
			DoMethodA(pm->obj, (Msg)&pm->m[0]);
			FreeMem(pm, pm->size);
		}
	}
	while (not_empty);
}
#endif

/*
 * Pushes a method asynchronously just like MUI, except
 * we have more control and there's no stupid static limit.
 */
#ifdef MULTITHREADED
static void methodstack_push_v(APTR obj, ULONG cnt, va_list va)
{
	struct pushedmethod *pm;
	ULONG size;

	size = sizeof(*pm) + cnt * sizeof(ULONG);

	if ((pm = AllocMem(size, MEMF_ANY)))
	{
		ULONG i = 0;
		pm->obj = obj;
		pm->size = size;
		pm->sync = 0;

		while (cnt--)
		{
			pm->m[i] = va_arg(va, ULONG);
			i++;
		}

		if (SysBase->ThisTask == (APTR)GUIThread)
		{
			methodstack_check();
			DoMethodA(obj, (Msg)&pm->m[0]);
			FreeMem(pm, size);
		}
		else
		{
			ObtainSemaphore(&semaphore);
			ADDTAIL(&methodlist, pm);
			ReleaseSemaphore(&semaphore);
		}
	}
}
#else
static ULONG methodstack_push_v(APTR obj, ULONG cnt, va_list va)
{
	ULONG size, *pm, rc;

	size = cnt * sizeof(ULONG);
	rc = 0;

	if ((pm = AllocMem(size, MEMF_ANY)))
	{
		ULONG i = 0;

		while (cnt--)
		{
			pm[i] = va_arg(va, ULONG);
			i++;
		}

		rc = DoMethodA(obj, (Msg)pm);
		FreeMem(pm, size);
	}

	return rc;
}

//ULONG methodstack_push_sync(APTR obj, ULONG cnt, ...) __attribute__((alias("methodstack_push")));
ULONG methodstack_push_sync(APTR obj, ULONG cnt, ...)
{
	//kprintf("methodstack_push_sync called, but not supported in single-threaded mode.\n");
	va_list va;
	ULONG res;

	va_start(va, cnt);
	res = methodstack_push_v(obj, cnt, va);
	va_end(va);

	return res;
}
#endif

void methodstack_push(APTR obj, ULONG cnt, ...)
{
	va_list va;
	va_start(va, cnt);
	methodstack_push_v(obj, cnt, va);
	va_end(va);
}

void methodstack_push_imm(APTR obj, ULONG cnt, ...)
{
	va_list va;
	va_start(va, cnt);
	methodstack_push_v(obj, cnt, va);
	va_end(va);
	#ifdef MULTITHREADED
	Signal(&GUIThread->pr_Task, SIGBREAKF_CTRL_E);
	#endif
}

/*
 * Pushes a method synchronously. Completes all the
 * waiting methods in the stack before ourself. Can
 * be used to synchronise previous pushmethods and
 * get greater efficiency by minimizing context switches.
 */
#ifdef MULTITHREADED
ULONG methodstack_push_sync(APTR obj, ULONG cnt, ...)
{
	struct pushedmethod *pm;
	ULONG res, size;
	va_list va;

	va_start(va, cnt);
	size = sizeof(*pm) + cnt * sizeof(ULONG);
	res = 0;

	if ((pm = AllocMem(size, MEMF_ANY)))
	{
		struct Process *thisproc = (APTR)SysBase->ThisTask;
		ULONG i = 0;

		while (cnt--)
		{
			pm->m[i] = va_arg(va, ULONG);
			i++;
		}

		if (thisproc == (APTR)GUIThread)
		{
			methodstack_check();
			res = DoMethodA(obj, (Msg)&pm->m[0]);

			FreeMem(pm, size);
			va_end(va);

			return res;
		}

		pm->size = size;
		pm->sync = 0;
		pm->obj = obj;
		pm->sync = TRUE;
		pm->msg.mn_ReplyPort = &thisproc->pr_MsgPort;

		ObtainSemaphore(&semaphore);
		ADDTAIL(&methodlist, pm);
		ReleaseSemaphore(&semaphore);

		Signal(&GUIThread->pr_Task, SIGBREAKF_CTRL_E);
		WaitPort(&thisproc->pr_MsgPort);
		GetMsg(&thisproc->pr_MsgPort);

		res = pm->result;

		FreeMem(pm, size);

		if (res == -1 && netsurf_quit)
			exit(0);
	}

	va_end(va);

	return res;
}
#endif

#ifndef MULTITHREADED
//extern void methodstack_push_sync(void);
//void methodstack_push(void) __attribute__((alias("methodstack_push_sync")));
#endif
