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

#include <dos/dostags.h>
#include <exec/execbase.h>
#include <hardware/atomic.h>
#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/save_text.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include "mui/applicationclass.h"
#include "mui/gui.h"
#include "mui/methodstack.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/save_complete.h"
#include "mui/save_pdf.h"
#include "mui/search.h"

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
	STATIC CONST CONST_STRPTR classlist = { NULL };
	APTR prefs_save, prefs_use, prefs_cancel, app;

	diskobj = GetDiskObject("PROGDIR:NetSurf");

	app = NewObject(getapplicationclass(), NULL,
		MUIA_Application_DiskObject, diskobj,
		MUIA_Application_Version, &version[1], /* skip leading \0 */
		MUIA_Application_Title, "NetSurf",
		MUIA_Application_Copyright,
	"NetSurf 2.0 @ 2009 Ilkka Lehtoranta and the NetSurf development team",
		MUIA_Application_Author, "Ilkka Lehtoranta",
		MUIA_Application_Base, "NETSURF",
		/*MUIA_Application_SingleTask, TRUE,*/
		MUIA_Application_Description, "NetSurf",
		MUIA_Application_UsedClasses, classlist,
	TAG_DONE);

	if (app) {
		application = app;
		DoMethod(app, MUIM_Application_Load, APPLICATION_ENVARC_PREFS);
	}

	return app;
}

#ifdef MULTITHREADED
STATIC VOID netsurf_guithread(void)
{
	APTR app;

	#if !defined(__MORPHOS__)
	struct Process *proc = SysBase->ThisTask;
	APTR startupmsg;

	WaitPort(&proc->pr_MsgPort);
	startupmsg = GetMsg(&proc->pr_MsgPort);
	#endif

	if ((app = netsurf_guisetup())) {
		ULONG signals, ret;

		signals = 0;

		while (((ret = DoMethod(app, MUIM_Application_NewInput,
			(IPTR)&signals)) != MUIV_Application_ReturnID_Quit)) {
			if (signals) {
				signals = Wait(signals | SIGBREAKF_CTRL_C |
						SIGBREAKF_CTRL_E);

				if (signals & SIGBREAKF_CTRL_C)
					break;

				if (signals & SIGBREAKF_CTRL_E)
					methodstack_check();
			}
		}
	}

	netsurf_guicleanup();

	#if !defined(__MORPHOS__)
	ReplyMsg(startupmsg);
	#endif
}

STATIC VOID netsurf_execute_job(struct worknode *node)
{
	struct browser_window *browser = node->browser;

	switch (node->job) {
	case JOB_GO:
		browser_window_go(browser, node->data, NULL, true);	
		break;

	case JOB_MOUSECLICK:
		browser_window_mouse_click(browser, node->param1, node->param2,
				node->param3);
		break;

	case JOB_FIND:
		search_start(node->param1, node->param2, node->data,
				(APTR)node->param3);
		break;

	case JOB_NEW_PAGE:
		browser_window_create(node->size > sizeof(*node) && *((char *)node->data) ? (char *)node->data : "",
			0, 0, true, false);
		break;

	case JOB_DESTROY:
		browser_window_destroy(browser);
		break;

	case JOB_KEY_PRESS:
		browser_window_key_press(browser, node->param1);
		break;

	case JOB_LOGIN:
		urldb_set_auth_details((char *)node->param1,
			(char *)node->param2, node->data);
		browser_window_go(browser, (char *)node->param1, 0, true);
		free((void *)node->param1);
		free((void *)node->param2);
		break;

	case JOB_HISTORY_BACK:
		history_back(browser, (APTR)node->param1);
		break;

	case JOB_HISTORY_FORWARD:
		history_forward(browser, (APTR)node->param1);
		break;

	case JOB_RELOAD:
		browser_window_reload(browser, (APTR)node->param1);
		break;

	case JOB_STOP:
		browser_window_stop(browser);
		break;

	case JOB_SAVE_DOC:
		switch (node->param1) {
		case SAVEDOC_HTML:
		{
			BPTR lock;

			lock = CreateDir(node->data);

			if (lock == 0 && IoErr() == ERROR_OBJECT_EXISTS) {
				BPTR fh = Open(node->data, MODE_OLDFILE);

				if (fh) {
					Close(fh);
					/* XXX: deleting without user
					 * intervention
					 */
					DeleteFile(node->data);
					lock = CreateDir(node->data);
				} else {
					lock = Lock(node->data, ACCESS_READ);
				}
			}

			if (lock) {
				lock = CurrentDir(lock);
				save_complete(browser->current_content, node->data);
				UnLock(CurrentDir(lock));
			}
		}
		break;

		case SAVEDOC_SOURCE:
			save_as_text(browser->current_content, node->data);
			break;

		case SAVEDOC_PDF:
			#if defined(__MORPHOS__)
			save_as_pdf(browser->current_content, node->data);
			#endif
			break;
		}

		SetComment(node->data, browser->current_content->url);
		break;

	case JOB_REDRAW:
		gui_window_redraw_window((APTR)node->param1);
		break;

	case JOB_REFORMAT:
		browser_window_reformat(browser, node->param1, node->param2);
		break;
	}

	FreeMem(node, node->size);
}

VOID netsurf_add_job(APTR browser, ULONG job_id, struct worknode *node)
{
	node->browser = browser;
	node->job = job_id;

#if 0
	/* We could do that but then we run into troubles with exit() */

	if (AttemptSemaphore(&work_semaphore))
	{
		netsurf_execute_job(node);
		ReleaseSemaphore(&work_semaphore);
	}
	else
#endif
	{
		Forbid();
		ADDTAIL(&worklist, node);
		Permit();
	}

	Signal(StartupMsgPort.mp_SigTask, SIGBREAKF_CTRL_E);
}

VOID netsurf_add_simple_job(APTR browser, ULONG job_id, ULONG param1,
	ULONG param2, ULONG param3)
{
	struct worknode *node;

	node = AllocMem(sizeof(*node), MEMF_ANY);

	if (node) {
		node->param1 = param1;
		node->param2 = param2;
		node->param3 = param3;
		node->size = sizeof(*node);
		netsurf_add_job(browser, job_id, node);
	}
}

STATIC VOID netsurf_check_worklist(void)
{
	for (;;) {
		struct worknode *node;

		Forbid();
		node = REMHEAD(&worklist);
		Permit();

		if (!node)
			break;

		netsurf_execute_job(node);
	}
}

ULONG netsurf_check_events(ULONG poll, ULONG sigmask)
{
	ULONG signals, startmask;

	startmask = 1 << startup_signal;

	if (poll)
		signals = SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E |
				sigmask | startmask);
	else
		signals = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E | sigmask |
				startmask);

	if (signals & SIGBREAKF_CTRL_C)
		netsurf_quit = true;

	if (signals & SIGBREAKF_CTRL_E)
		netsurf_check_worklist();

	if (startmask) {
		struct Message *msg;

		msg = GetMsg(&StartupMsgPort);

		if (msg) {
			ATOMIC_SUB(&thread_count, 1);
			FreeMem(msg, sizeof(*msg));
		}
	}

	return signals;
}
#else
ULONG netsurf_check_events(ULONG poll, ULONG sigmask)
{
	STATIC ULONG signals = 0;
	ULONG ret, startmask;
	APTR app;

	app = application;
	startmask = 1 << startup_signal;

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
	}

	if (ret == MUIV_Application_ReturnID_Quit || signals & SIGBREAKF_CTRL_C)
		netsurf_quit = true;

	if (signals & SIGBREAKF_CTRL_E)
		netsurf_check_worklist();

	return signals;
}
#endif

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
