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

#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "desktop/browser.h"
#include "mui/applicationclass.h"
#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"

struct Data
{
	APTR windownode;

	APTR active_browser;

	APTR navbar;
	APTR addressbar;
	APTR bgroup;
	APTR findbar;
	APTR statusbar;

	APTR pagetitles;

	APTR lonely_title;
	APTR lonely_friend;

	APTR windowtitle;
};

DEFNEW
{
	APTR navbar, bgroup, statusbar, addressbar, searchbar, findbar;

	obj = DoSuperNew(cl, obj,
			MUIA_Window_Width , MUIV_Window_Width_Visible(75),
			MUIA_Window_Height , MUIV_Window_Height_Visible(90),
			MUIA_Window_TopEdge , MUIV_Window_TopEdge_Centered,
			MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
			MUIA_Window_AppWindow, TRUE,
			WindowContents, VGroup,
				Child, HGroup,
					Child, navbar = NewObject(getnavigationbargroupclass(), NULL, TAG_DONE),
					Child, searchbar = NewObject(getsearchbargroupclass(), NULL, TAG_DONE),
				End,
				Child, addressbar = NewObject(getaddressbargroupclass(), NULL, TAG_DONE),
				Child, bgroup = VGroup, TAG_DONE),
				Child, findbar = NewObject(getfindtextclass(), NULL, MUIA_ShowMe, FALSE, TAG_DONE),
				Child, statusbar = TextObject, TAG_DONE),
			End,

			TAG_MORE, msg->ops_AttrList);

	if (obj) {
		APTR node;
		GETDATA;

		data->navbar = navbar;
		data->addressbar = addressbar;
		data->bgroup = bgroup;
		data->findbar = findbar;
		data->statusbar = statusbar;

		data->windownode = node = (APTR)GetTagData(MA_Window_Node, NULL, msg->ops_AttrList);

		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 2, MM_Application_CloseWindow, node);
		DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, MUIV_EveryTime, obj, 3, MM_Window_MenuAction, MUIV_TriggerValue);
	}

	return (IPTR)obj;
}

DEFDISP
{
	GETDATA;

	if (data->windowtitle)
		FreeVecTaskPooled(data->windowtitle);

	MUI_DisposeObject(data->lonely_title);

	return DOSUPER;
}

DEFGET
{
	GETDATA;

	switch (msg->opg_AttrID) {
	case MA_Browser_Browser:
	#warning we should not need this... some unidentified bug somewhere
	case MA_Browser_Loading:
	case MA_Browser_URL:
		return DoMethodA(data->active_browser, msg);
	}

	return DOSUPER;
}

DEFTMETHOD(Window_InsertBookmark)
{
	GETDATA;
	STRPTR url, title;

	url = (STRPTR)getv(data->active_browser, MA_Browser_URL);
	title = (STRPTR)getv(data->active_browser, MA_Browser_Title);

	return DoMethod(_app(obj), MM_HotlistWindow_Insert, title, url);
}

DEFSMETHOD(Window_MenuAction)
{
	GETDATA;
	APTR app = _app(obj) ? _app(obj) : application;

	switch (msg->action) {
	//case MNA_NEW_WINDOW      : DoMethod(app, MM_Application_NewWindow); break;
	case MNA_RELOAD          : DoMethod(obj, MM_Window_Navigate, NAV_RELOAD); break;

	case MNA_SAVE_AS_TEXT    : DoMethod(app, MM_Application_SaveDocument, SAVEDOC_HTML); break;
	case MNA_SAVE_AS_SOURCE  : DoMethod(app, MM_Application_SaveDocument, SAVEDOC_SOURCE); break;
	case MNA_SAVE_AS_PDF     : DoMethod(app, MM_Application_SaveDocument, SAVEDOC_PDF); break;
	case MNA_PRINT           : DoMethod(data->active_browser, MM_Browser_Print); break;

	case MNA_ABOUT           : DoMethod(app, MM_Application_About); break;
	case MNA_FIND            : set(data->findbar, MUIA_ShowMe, TRUE); break;
	case MNA_DOWNLOADS_WINDOW: DoMethod(app, MM_Application_OpenWindow, WINDOW_DOWNLOADS); break;
	case MNA_HOTLIST_WINDOW  : DoMethod(app, MM_Application_OpenWindow, WINDOW_HOTLIST); break;
	case MNA_MUI_SETTINGS    : DoMethod(app, MUIM_Application_OpenConfigWindow, 0, NULL); break;
	case MNA_NETSURF_SETTINGS: DoMethod(app, MM_Application_OpenWindow, WINDOW_SETTINGS); break;

	case MNA_NEW_PAGE:
	{
		#if MULTITHREADED
		struct worknode *node;
		STRPTR page;
		ULONG length;

		page = (STRPTR)getv(app, MA_Application_Homepage);
		length = strlen(page) + 1;

		node = AllocMem(sizeof(*node) + length, MEMF_ANY);

		if (node) {
			node->size = sizeof(*node) + length;
			bcopy(page, node->data, length);

			netsurf_add_job(NULL, JOB_NEW_PAGE, node);
		}
		#else
		browser_window_create((char *)getv(app, MA_Application_Homepage), 0, 0, true, false);
		#endif
	}
		break;

	case MNA_CLOSE_PAGE:
	{
		APTR bw = (APTR)getv(data->active_browser, MA_Browser_Browser);
		netsurf_add_simple_job(bw, JOB_DESTROY, 0, 0, 0);
	}
		break;
		#if 0
		if (data->pagetitles) {
			DoMethod(data->pagetitles, MUIM_Title_Close, NULL);
		} else {
			DoMethod(app, MUIM_Application_PushMethod, app, 2, MM_Application_CloseWindow, data->windownode);
		}
		break;
		#endif

	case MNA_CLOSE_WINDOW:
		if (data->pagetitles) {
			ULONG count = getv(data->pagetitles, MUIA_Group_ChildCount);

			if (count > 1) {
				if (!MUI_RequestA(_app(obj), obj, 0, "NetSurf", "_Yes|*_No", "Do you want to close all pages?", NULL))
					break;
			}
		}

		DoMethod(app, MUIM_Application_PushMethod, app, 2, MM_Application_CloseWindow, data->windownode);
		break;
	}

	return 0;
}

DEFSMETHOD(Window_RemovePage)
{
	GETDATA;

	if (data->pagetitles == NULL || getv(data->pagetitles, MUIA_Group_ChildCount) == 1) {
		DoMethod(application, MUIM_Application_PushMethod, application,
				2, MM_Application_CloseWindow, data->windownode);
	} else {
		APTR title = (APTR)getv(msg->browser, MA_Browser_TitleObj);

		DoMethod(data->bgroup, MUIM_Group_InitChange);
		DoMethod(data->bgroup, OM_REMMEMBER, msg->browser);
		DoMethod(data->pagetitles, MUIM_Group_InitChange);
		DoMethod(data->pagetitles, OM_REMMEMBER, title);
		DoMethod(data->pagetitles, MUIM_Group_ExitChange);
		DoMethod(data->bgroup, MUIM_Group_ExitChange);

		MUI_DisposeObject(title);
		MUI_DisposeObject(msg->browser);
	}

	return 0;
}

STATIC VOID setup_browser_notifications(APTR obj, struct Data *data, APTR bw)
{
	DoMethod(bw, MUIM_Notify, MA_Browser_Loading, MUIV_EveryTime,
			data->navbar, 3, MUIM_Set, MA_TransferAnim_Animate,
			MUIV_NotTriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_StatusText, MUIV_EveryTime,
			data->statusbar, 3, MUIM_Set, MUIA_Text_Contents,
			MUIV_TriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_URL, MUIV_EveryTime,
			data->addressbar, 3, MUIM_Set, MUIA_String_Contents,
			MUIV_TriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_Title, MUIV_EveryTime, obj, 2,
			MM_Window_SetTitle, MUIV_TriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_BackAvailable, MUIV_EveryTime,
			data->navbar, 3, MUIM_Set, MA_Navigation_BackEnabled,
			MUIV_TriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_ForwardAvailable, MUIV_EveryTime,
			data->navbar, 3, MUIM_Set,
			MA_Navigation_ForwardEnabled, MUIV_TriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_ReloadAvailable, MUIV_EveryTime,
			data->navbar, 3, MUIM_Set, MA_Navigation_ReloadEnabled,
			MUIV_TriggerValue);
	DoMethod(bw, MUIM_Notify, MA_Browser_StopAvailable, MUIV_EveryTime,
			data->navbar, 3, MUIM_Set, MA_Navigation_StopEnabled,
			MUIV_TriggerValue);
}

DEFSMETHOD(Window_AddPage)
{
	GETDATA;
	APTR bwgroup, bw, title;
	LONG ok;

	title = TextObject,
			MUIA_Text_Contents, "NetSurf",
			MUIA_UserData, msg->context,
		End;

	bwgroup = ScrollgroupObject,
			MUIA_Scrollgroup_AutoBars, TRUE,
			MUIA_FillArea, FALSE,
			MUIA_Scrollgroup_Contents, bw = NewObject(getbrowserclass(), NULL,
				MA_Browser_Context, msg->context,
				MA_Browser_TitleObj, title,
			End,
		TAG_DONE);

	ok = FALSE;

	if (bwgroup && title) {
		APTR maingroup = data->bgroup;

		DoMethod(maingroup, MUIM_Group_InitChange);

		if (data->pagetitles) {
			DoMethod(data->pagetitles, MUIM_Group_InitChange);
			DoMethod(data->pagetitles, OM_ADDMEMBER, title);
			DoMethod(data->bgroup, OM_ADDMEMBER, bwgroup);
			DoMethod(data->pagetitles, MUIM_Group_ExitChange);
			ok = TRUE;
		} else if (data->lonely_title == NULL) {
			data->lonely_title = title;
			data->lonely_friend = bwgroup;
			DoMethod(maingroup, OM_ADDMEMBER, bwgroup);
			ok = TRUE;
		} else {
			APTR group, titles;

			group = VGroup,
					MUIA_Background, MUII_RegisterBack,
					MUIA_Frame, MUIV_Frame_Register,
					MUIA_Group_PageMode, TRUE,
					Child, titles = NewObject(gettitleclass(), NULL,
						MUIA_CycleChain, 1,
						MUIA_Title_Closable, TRUE,
					End,
				TAG_DONE);

			if (group) {
				data->bgroup = group;
				data->pagetitles = titles;

				if (data->lonely_title) {
					DoMethod(group, MUIM_Notify,
							MUIA_Group_ActivePage,
							MUIV_EveryTime, obj, 2,
							MM_Window_ActivePage,
							MUIV_TriggerValue);

					DoMethod(maingroup, OM_REMMEMBER, data->lonely_friend);
					DoMethod(titles, OM_ADDMEMBER, data->lonely_title);
					DoMethod(group, OM_ADDMEMBER, data->lonely_friend);
					DoMethod(maingroup, OM_ADDMEMBER, group);
					data->lonely_title = NULL;
				}

				DoMethod(titles, OM_ADDMEMBER, title);
				DoMethod(group, OM_ADDMEMBER, bwgroup);
				ok = TRUE;
			}
		}

		DoMethod(maingroup, MUIM_Group_ExitChange);

		//set(group, MUIA_Window_Title, APPLICATION_NAME);
	}

	if (ok) {
		data->active_browser = bw;

		if (data->bgroup)
			set(data->bgroup, MUIA_Group_ActivePage,
					MUIV_Group_ActivePage_Last);

		DoMethod(bw, MUIM_Notify, MA_Browser_Title, MUIV_EveryTime,
				title, 3, MUIM_Set, MUIA_Text_Contents,
				MUIV_TriggerValue);

		setup_browser_notifications(obj, data, bw);
	} else {
		MUI_DisposeObject(bwgroup);
		MUI_DisposeObject(title);
		bw = NULL;
	}

	return (IPTR)bw;
}

DEFSMETHOD(Window_ActivePage)
{
	GETDATA;
	ULONG idx;

	idx = 0;

	FORCHILD(data->bgroup, MUIA_Group_ChildList) {
		if (data->pagetitles == child)
			continue;

		if (msg->pagenum == idx) {
			APTR old = data->active_browser;

			if (old) {
				DoMethod(old, MUIM_KillNotify, MA_Browser_Loading);
				DoMethod(old, MUIM_KillNotify, MA_Browser_StatusText);
				DoMethod(old, MUIM_KillNotify, MA_Browser_URL);
				DoMethod(old, MUIM_KillNotifyObj, MA_Browser_Title, obj);
				DoMethod(old, MUIM_KillNotify, MA_Browser_BackAvailable);
				DoMethod(old, MUIM_KillNotify, MA_Browser_ForwardAvailable);
				DoMethod(old, MUIM_KillNotify, MA_Browser_ReloadAvailable);
				DoMethod(old, MUIM_KillNotify, MA_Browser_StopAvailable);
			}

			setup_browser_notifications(obj, data, child);

			data->active_browser = child;

			DoMethod(data->addressbar, MUIM_NoNotifySet, MUIA_String_Contents, getv(child, MA_Browser_URL));
			DoMethod(data->statusbar, MUIM_NoNotifySet, MUIA_String_Contents, getv(child, MA_Browser_StatusText));
			DoMethod(obj, MM_Window_SetTitle, getv(child, MA_Browser_Title));

			SetAttrs(data->navbar,
				MA_TransferAnim_Animate, getv(child, MA_Browser_Loading),
				MA_Navigation_BackEnabled, getv(child, MA_Browser_BackAvailable),
				MA_Navigation_ForwardEnabled, getv(child, MA_Browser_ForwardAvailable),
				MA_Navigation_ReloadEnabled, getv(child, MA_Browser_ReloadAvailable),
				MA_Navigation_StopEnabled, getv(child, MA_Browser_StopAvailable),
			TAG_DONE);

			break;
		}

		idx++;
	}
	NEXTCHILD

	return 0;
}

DEFSMETHOD(Window_SetTitle)
{
	GETDATA;
	STRPTR newtitle, title;
	ULONG length;

	length = sizeof("NetSurf: %s") + strlen(msg->title);
	newtitle = AllocVecTaskPooled(length);
	title = data->windowtitle;

	if (newtitle) {
		NewRawDoFmt("NetSurf: %s", NULL, newtitle, msg->title);
		set(obj, MUIA_Window_Title, newtitle);

		if (title)
			FreeVecTaskPooled(title);

		data->windowtitle = newtitle;
	}

	return 0;
}

DEFSMETHOD(Window_Navigate)
{
	GETDATA;

	switch (msg->Navigate) {
	case NAV_BACK   : return DoMethod(data->active_browser, MM_Browser_Back);
	case NAV_FORWARD: return DoMethod(data->active_browser, MM_Browser_Forward);
	case NAV_RELOAD : return DoMethod(data->active_browser, MM_Browser_Reload);
	case NAV_STOP   : return DoMethod(data->active_browser, MM_Browser_Stop);
	}

	return 0;
}

/* Browser dispatchers */
DEFSMETHOD(Browser_Go)
{
	GETDATA;
	return DoMethodA(data->active_browser, msg);
}

DEFSMETHOD(Browser_Find)
{
	GETDATA;
	return DoMethodA(data->active_browser, msg);
}

DEFSMETHOD(FindText_DisableButtons)
{
	GETDATA;
	return DoMethodA(data->findbar, msg);
}

BEGINMTABLE
DECNEW
DECDISP
DECGET
DECSMETHOD(Browser_Find)
DECSMETHOD(Browser_Go)
DECSMETHOD(FindText_DisableButtons)
DECSMETHOD(Window_ActivePage)
DECSMETHOD(Window_AddPage)
DECSMETHOD(Window_InsertBookmark)
DECSMETHOD(Window_Navigate)
DECSMETHOD(Window_MenuAction)
DECSMETHOD(Window_RemovePage)
DECSMETHOD(Window_SetTitle)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, windowclass)
