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

#ifndef MORPHOS_NETSURF_H
#define MORPHOS_NETSURF_H

#define APPLICATION_ENV_PREFS "ENV:MUI/NetSurf.config"
#define APPLICATION_ENVARC_PREFS "ENVARC:MUI/NetSurf.config"
#define APPLICATION_HOTLIST_FILE "PROGDIR:Resources/Hotlist"
#define APPLICATION_COOKIES_FILE "PROGDIR:Resources/Cookies"
#define APPLICATION_RECENT_FILE  "PROGDIR:Resources/Recent"

#if defined(__MORPHOS__) || defined(__PPC__)
#define __TEXT_SEGMENT__ __attribute__((section(".text")))
#else
#define __TEXT_SEGMENT__
#endif

struct worknode
{
	struct MinNode node;
	APTR browser;
	ULONG job;
	ULONG param1;
	ULONG param2;
	ULONG param3;
	ULONG size;
	UBYTE data[0];
};

enum
{
	JOB_GO,
	JOB_MOUSECLICK,
	JOB_FIND,
	JOB_NEW_PAGE,
	JOB_DESTROY,
	JOB_KEY_PRESS,
	JOB_LOGIN,
	JOB_HISTORY_BACK,
	JOB_HISTORY_FORWARD,
	JOB_RELOAD,
	JOB_STOP,
	JOB_SAVE_DOC,
	JOB_REDRAW,
	JOB_REFORMAT,
};

extern struct Process * VOLATILE GUIThread;
extern struct MsgPort StartupMsgPort;
extern APTR application;
extern LONG thread_count;

struct worknode;

LONG netsurf_setup(void);
void netsurf_cleanup(void);
VOID netsurf_add_job(APTR browser, ULONG job_id, struct worknode *node);
VOID netsurf_add_simple_job(APTR browser, ULONG job_id, ULONG param1, ULONG param2, ULONG param3);
ULONG netsurf_check_events(ULONG poll, ULONG sigmask);

#endif /* MORPHOS_NETSURF_H */
