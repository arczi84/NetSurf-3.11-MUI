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
#include <stdbool.h>

#include <dos/dostags.h>
#include <exec/execbase.h>
#include <hardware/atomic.h>
#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "utils/errors.h"
#include "utils/nsoption.h"
#include "content/urldb.h"
#include "netsurf/browser.h"
#include "desktop/gui_internal.h"
#include "netsurf/netsurf.h"
#include "desktop/save_text.h"
#include "mui/applicationclass.h"
#include "mui/gui.h"
#include "mui/methodstack.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/save_complete.h"
#include "mui/save_pdf.h"
#include "mui/search.h"
#include "utils/log.h"
/*********************************************************************/
#include <exec/types.h>
#include <dos/dos.h>
/*********************************************************************/

CONST TEXT version[] __TEXT_SEGMENT__ = "\0$VER: NetSurf 2.0 (18.1.09)";

#ifdef MULTITHREADED
STATIC struct MsgPort GUIThreadMsgPort =
{
	{ NULL, NULL, NT_MSGPORT, 0, NULL },
	PA_SIGNAL, 0x00, NULL,
	{
		(APTR)&GUIThreadMsgPort.mp_MsgList.lh_Tail,
		NULL,
		(APTR)&GUIThreadMsgPort.mp_MsgList.lh_Head
	}
};

STATIC struct MinList worklist =
{
	(APTR)&worklist.mlh_Tail,
	NULL,
	(APTR)&worklist
};

STATIC struct SignalSemaphore work_semaphore;
STATIC LONG thread_quit;
STATIC BYTE startup_signal = -1;
#endif

STATIC BYTE startup_signal = -1; //was missing here arczi
STATIC struct DiskObject *diskobj;

/*********************************************************************/

#ifdef MULTITHREADED
struct Process * VOLATILE GUIThread;
#endif

struct MsgPort StartupMsgPort =
{
	{ NULL, NULL, NT_MSGPORT, 0, NULL },
	PA_SIGNAL, 0x00, NULL,
	{
		(APTR)&StartupMsgPort.mp_MsgList.lh_Tail,
		NULL,
		(APTR)&StartupMsgPort.mp_MsgList.lh_Head
	}
};

APTR application;
LONG thread_count;

/*********************************************************************/

#ifdef MULTITHREADED
STATIC VOID netsurf_delete_worklist(void)
{
	struct worknode *next, *node;

	ITERATELISTSAFE(node, next, &worklist)
	{
		FreeMem(node, node->size);
	}
}
#endif

STATIC VOID netsurf_guicleanup(void)
{
	MUI_DisposeObject(application);

	if (diskobj)
		FreeDiskObject(diskobj);

#ifdef MULTITHREADED
	netsurf_quit = true;

	Signal(StartupMsgPort.mp_SigTask, SIGBREAKF_CTRL_C);

	while (thread_quit == 0) {
		Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E);
		methodstack_cleanup_guithread();
	}

	Forbid();
	GUIThread = NULL;
#endif
}

STATIC APTR netsurf_guisetup(void)
{
	STATIC CONST CONST_STRPTR classlist[] = { NULL };
	APTR prefs_save, prefs_use, prefs_cancel, app;
	
	LOG(("DEBUG: Tworzę applicationclass...\n"));
	diskobj = GetDiskObject("PROGDIR:NetSurf");
	LOG(("DEBUG: Pobieram DiskObject 'PROGDIR:NetSurf'...\n"));
	if (!diskobj) {
    LOG(("ERROR: Brak DiskObject 'PROGDIR:NetSurf'\n"));
		exit(1);
	}
    LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	
	app = NewObject(getapplicationclass(), NULL,
		MUIA_Application_DiskObject, diskobj,
		MUIA_Application_Version, &version[1], /* skip leading \0 */
		MUIA_Application_Title, "NetSurf",
		MUIA_Application_Copyright,
		"NetSurf 3.11 @ 2023 Ilkka Lehtoranta, Artur Jarosik and the NetSurf development team",
		MUIA_Application_Author, "Ilkka Lehtoranta",
		MUIA_Application_Base, "NETSURF",
		/*MUIA_Application_SingleTask, TRUE,*/
		MUIA_Application_Description, "NetSurf",
		MUIA_Application_UsedClasses, classlist,
	TAG_DONE);
    LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	
	if (app) {
		LOG(("DEBUG: Tworze obiekt aplikacji...\n"));
		application = app;
		DoMethod(app, MUIM_Application_Load, APPLICATION_ENVARC_PREFS);
	}
	else
	{
		LOG(("****ERROR:***** Nie mogę utworzyć obiektu aplikacji!\n"));
		exit(1);
		return NULL;
	}
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	return app;
}

ULONG netsurf_check_events(ULONG poll, ULONG sigmask)
{
	STATIC ULONG signals = 0;
	ULONG ret, startmask;
	APTR app;

	app = application;
	startmask = 1 << startup_signal;

    if (!app) {
        LOG(("ERROR: application is NULL!\n"));
		exit(1);
        return 0;
    }

	while (((ret = DoMethod(app, MUIM_Application_NewInput, (IPTR)&signals))
			!= MUIV_Application_ReturnID_Quit)) {
		if (signals) {
			if (poll)
				break;

			signals = Wait(signals | SIGBREAKF_CTRL_C | sigmask |
					SIGBREAKF_CTRL_E | startmask);
					
			if (startmask) {
				struct Message *msg;

				msg = GetMsg(&StartupMsgPort);
				if (msg) {
					ATOMIC_SUB(&thread_count, 1);
					FreeMem(msg, sizeof(*msg));
				}
			}

			if (signals & SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E)
				break;

			if (signals & sigmask)
				break;
		}

		if (poll)
			break;
	}

	if (ret == MUIV_Application_ReturnID_Quit || signals & SIGBREAKF_CTRL_C)
		gui_quit();

	ULONG result = signals;
	signals = 0;
	return result;
}


LONG netsurf_setup(void)
{
	LONG rc;

	startup_signal = AllocSignal(-1);
	rc = FALSE;

	if (startup_signal >= 0) {
#ifdef MULTITHREADED
		STATIC struct Message msg;

		StartupMsgPort.mp_SigBit = startup_signal;
		StartupMsgPort.mp_SigTask = SysBase->ThisTask;
		GUIThreadMsgPort.mp_SigBit = startup_signal;
		GUIThreadMsgPort.mp_SigTask = SysBase->ThisTask;

		msg.mn_ReplyPort = &GUIThreadMsgPort;

		InitSemaphore(&work_semaphore);
		ObtainSemaphore(&work_semaphore);

		GUIThread = CreateNewProcTags(
			#if defined(__MORPHOS__)
			NP_CodeType, CODETYPE_PPC,
			NP_StartupMsg, &msg,
			#endif
			NP_Entry, &netsurf_guithread,
			NP_Name, "NetSurf User Interface",
			NP_Priority, 1,
			TAG_DONE
		);

		if (GUIThread)
		{
			#if !defined(__MORPHOS__)
			PutMsg(&GUIThread->pr_MsgPort, (APTR)&msg);
			#endif

			rc = TRUE;
		}
#else
		StartupMsgPort.mp_SigBit = startup_signal;
		StartupMsgPort.mp_SigTask = SysBase->ThisTask;

		if (netsurf_guisetup())
			rc = TRUE;
#endif
	}

	return rc;
}

/* After this call nobody should make call to methodstack. Maybe we should build
 * some protection there?
 */

void netsurf_cleanup(void)
{
#ifdef MULTITHREADED
	thread_quit = 1;

	Forbid();

	if (GUIThread) {
		Signal(&GUIThread->pr_Task, SIGBREAKF_CTRL_C);
		WaitPort(&GUIThreadMsgPort);
	}

	Permit();

	netsurf_delete_worklist();

	while (thread_count > 0) {
		struct Message *msg;

		WaitPort(&StartupMsgPort);

		msg = GetMsg(&StartupMsgPort);

		if (msg) {
			thread_count--;
			FreeMem(msg, sizeof(*msg));
		}
	}

#else
	netsurf_guicleanup();
#endif

	FreeSignal(startup_signal);
}