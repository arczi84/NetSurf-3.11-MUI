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

#include <cybergraphx/cybergraphics.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#if defined(__MORPHOS__)
#include <mui/Aboutbox_mcc.h>
#endif

#include "desktop/version.h"
#include "netsurf/netsurf.h"
#include "mui/applicationclass.h"
#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/utils.h"

#include "utils/log2.h"

struct windownode
{
	struct MinNode node;
	ULONG num;
	APTR  winobj;
};

struct Data
{
	struct MinList windowlist;	// window list
	struct windownode *active_window;

	STRPTR screentitle;			// screen title (NetSurf 2.0)
	APTR hotlistwin;
	APTR dlwin;
	APTR prefswin;
	APTR aboutwin;

	// prefs
	APTR str_homepage;
	APTR str_dldir;

	STRPTR homepage;
	STRPTR download_dir;
};

#define TICK (CHECKIT|MENUTOGGLE)
#define DIS  NM_ITEMDISABLED

static struct NewMenu mainmenu[] =
{
	{ NM_TITLE, "Project"              , 0, 0, 0, NULL },
//	{ NM_ITEM , "W\0New window..."     , 0, 0, 0, (APTR)MNA_NEW_WINDOW },
	{ NM_ITEM , "N\0New page"          , 0, 0, 0, (APTR)MNA_NEW_PAGE },
//	{ NM_ITEM , "O\0Open local file...", 0, 0, 0, (APTR)MNA_OPEN_LOCAL_FILE },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
	{ NM_ITEM , "R\0Reload"            , 0, 0, 0, (APTR)MNA_RELOAD },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
	{ NM_ITEM , "Save as..."           , 0, 0, 0, NULL },
	{ NM_SUB  , "Text..."              , 0, 0, 0, (APTR)MNA_SAVE_AS_TEXT },
	{ NM_SUB  , "Source..."            , 0, 0, 0, (APTR)MNA_SAVE_AS_SOURCE },
	{ NM_SUB  , "PDF..."               , 0, 0, 0, (APTR)MNA_SAVE_AS_PDF },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
	{ NM_ITEM , "P\0Print"             , 0, 0, 0, (APTR)MNA_PRINT },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
	{ NM_ITEM , "About..."             , 0, 0, 0, (APTR)MNA_ABOUT },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
	{ NM_ITEM , "Close page"           , 0, 0, 0, (APTR)MNA_CLOSE_PAGE },
//	{ NM_ITEM , "Close window"         , 0, 0, 0, (APTR)MNA_CLOSE_WINDOW },
	{ NM_TITLE, "Edit"                 , 0, 0, 0, NULL },
#if 0
	{ NM_ITEM , "C\0Copy"              , 0, DIS , 0, (APTR)MNA_COPY },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
	{ NM_ITEM , "A\0Select all"        , 0, DIS , 0, (APTR)MNA_SELECT_ALL },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },
#endif
	{ NM_ITEM , "Find..."              , 0, 0, 0, (APTR)MNA_FIND },
	{ NM_TITLE, "Windows"              , 0, 0, 0, NULL },
	{ NM_ITEM , "Downloads..."         , 0, 0, 0, (APTR)MNA_DOWNLOADS_WINDOW },
	{ NM_ITEM , "Hotlist..."           , 0, 0, 0, (APTR)MNA_HOTLIST_WINDOW },
	{ NM_ITEM , NM_BARLABEL            , 0, 0, 0, NULL },	
	{ NM_ITEM , "Settings..."      , 0, 0, 0, (APTR)MNA_NETSURF_SETTINGS },
	{ NM_ITEM , "MUI..."           , 0, 0, 0, (APTR)MNA_MUI_SETTINGS },
	{ NM_END  , NULL               , 0, 0, 0, NULL }
};

enum
{
	REXX_PRINT,
	REXX_ABOUT,
	REXX_COPY,
	REXX_CLEARSELECTED,
	REXX_SNAPSHOT,
	REXX_WINDOWTOFRONT,
	REXX_WINDOWTOBACK,
	REXX_ACTIVATEWINDOW,
	REXX_SCREENTOFRONT,
	REXX_SCREENTOBACK,
	REXX_OPEN,
	REXX_RELOAD,
	REXX_VERSION,
	REXX_GETTITLE,
	REXX_GETURL,
	REXX_SAVE,
};

STATIC LONG Rexx(struct Hook *h, APTR obj, IPTR *params);
#define REXXHOOK(name, param)	static const struct Hook name = { { NULL, NULL }, (HOOKFUNC)&HookEntry, (HOOKFUNC)&Rexx, (APTR)(param) }

REXXHOOK(RexxHookA, REXX_PRINT);
REXXHOOK(RexxHookB, REXX_ABOUT);
REXXHOOK(RexxHookC, REXX_COPY);
REXXHOOK(RexxHookD, REXX_CLEARSELECTED);
REXXHOOK(RexxHookE, REXX_SNAPSHOT);
REXXHOOK(RexxHookI, REXX_WINDOWTOFRONT);
REXXHOOK(RexxHookJ, REXX_WINDOWTOBACK);
REXXHOOK(RexxHookK, REXX_ACTIVATEWINDOW);
REXXHOOK(RexxHookL, REXX_SCREENTOFRONT);
REXXHOOK(RexxHookM, REXX_SCREENTOBACK);
REXXHOOK(RexxHookO, REXX_OPEN);
REXXHOOK(RexxHookP, REXX_RELOAD);
REXXHOOK(RexxHookQ, REXX_VERSION);
REXXHOOK(RexxHookR, REXX_GETTITLE);
REXXHOOK(RexxHookS, REXX_GETURL);
REXXHOOK(RexxHookT, REXX_SAVE);

STATIC CONST struct MUI_Command rexxcommands[] =
{
	{ "PRINT"         , NULL    , 0, (struct Hook *)&RexxHookA },
	{ "ABOUT"         , NULL    , 0, (struct Hook *)&RexxHookB },
	{ "COPY"          , NULL    , 0, (struct Hook *)&RexxHookC },
	{ "CLEARSELECTED" , NULL    , 0, (struct Hook *)&RexxHookD },
	{ "SNAPSHOT"      , NULL    , 0, (struct Hook *)&RexxHookE },
	{ "WINDOWTOFRONT" , NULL    , 0, (struct Hook *)&RexxHookI },
	{ "WINDOWTOBACK"  , NULL    , 0, (struct Hook *)&RexxHookJ },
	{ "ACTIVATEWINDOW", NULL    , 0, (struct Hook *)&RexxHookK },
	{ "SCREENTOFRONT" , NULL    , 0, (struct Hook *)&RexxHookL },
	{ "SCREENTOBACK"  , NULL    , 0, (struct Hook *)&RexxHookM },
	{ "OPEN"          , "NAME/K,NEWPAGE/S", 2, (struct Hook *)&RexxHookO },
	{ "RELOAD"        , NULL              , 0, (struct Hook *)&RexxHookP },
	{ "VERSION"       , NULL              , 0, (struct Hook *)&RexxHookQ },
	{ "GETTITLE"      , NULL              , 0, (struct Hook *)&RexxHookR },
	{ "GETURL"        , NULL              , 0, (struct Hook *)&RexxHookS },
	{ "SAVE"          , "NAME/A"          , 0, (struct Hook *)&RexxHookT },
	{ NULL            , NULL    , 0, NULL }
};
//OPEN URL/A,NEW=NEWWINDOW/S,SAVEAS/K

STATIC LONG Rexx(struct Hook *h, APTR obj, IPTR *params)
{
		LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	struct windownode *node;
	struct Data *data;

	data = INST_DATA(OCLASS(obj), obj);
	node = data->active_window;

	if ((ULONG)h->h_Data == REXX_ABOUT)
	{
		DoMethod(obj, MM_Application_About);
	}
	else if (node)
	{
		ULONG tags = TAG_DONE;

		switch ((ULONG)h->h_Data)
		{
			case REXX_PRINT:
				//
				break;

			case REXX_COPY:
				//
				break;

			case REXX_CLEARSELECTED:
				//
				break;

			case REXX_SNAPSHOT:
				DoMethod(node->winobj, MUIM_Window_Snapshot);
				break;

			case REXX_WINDOWTOFRONT:
				DoMethod(node->winobj, MUIM_Window_ToFront);
				break;

			case REXX_WINDOWTOBACK:
				DoMethod(node->winobj, MUIM_Window_ToBack);
				break;

			case REXX_ACTIVATEWINDOW:
				set(node->winobj, MUIA_Window_Activate, TRUE);
				break;

			case REXX_SCREENTOFRONT:
				DoMethod(node->winobj, MUIM_Window_ScreenToFront);
				break;

			case REXX_SCREENTOBACK:
				DoMethod(node->winobj, MUIM_Window_ScreenToBack);
				break;

			case REXX_OPEN:
				{
					CONST_STRPTR url = (APTR)*params;
					ULONG newpage = params[1];
					APTR app = application; //arczi was underclared 
					if (newpage)
					{
						#if MULTITHREADED
						struct worknode *node;
						ULONG length;

						length = strlen(url) + 1;

						node = AllocMem(sizeof(*node) + length, MEMF_ANY);

						if (node) {
							node->size = sizeof(*node) + length;
							bcopy(url, node->data, length);

							netsurf_add_job(NULL, JOB_NEW_PAGE, node);
						}
						#else
						LOG(("DEBUG: REXX_OPEN: url=%s, app=%p\n", url, app));
						browser_window_create((char *)getv(app, MA_Application_Homepage), 0, 0, true, false);
						#endif
					}
					else
					{
						DoMethod(node->winobj, MM_Browser_Go, url);
					}
				}
				break;

			case REXX_RELOAD:
				DoMethod(node->winobj, MM_Window_Navigate, NAV_RELOAD);
				break;

			case REXX_VERSION:
				break;

			case REXX_GETTITLE:
				break;

			case REXX_GETURL:
				break;

			case REXX_SAVE:
#if 0
				#if MULTITHREADED
				struct worknode *node;
				ULONG length;

				if (msg->type != SAVEDOC_HTML)
					filename = filereq->fr_File;

				length = strlen(filename) + strlen(filereq->fr_Drawer) + 4;
				node = AllocMem(sizeof(*node) + length, MEMF_ANY);

				if (node)
				{
					node->size = length + sizeof(*node);
					node->param1 = msg->type;
					strcpy(node->data, filereq->fr_Drawer);
					AddPart(node->data, filename, length);

					netsurf_add_job((APTR)getv(data->active_window->winobj, MA_Browser_Browser), JOB_SAVE_DOC, node);
				}
				#else
				#warning MM_Application_SaveDocument not implemented
				#endif
#endif
				break;
		}
	}

	return 0;
}

STATIC VOID update_prefs(struct Data *data)
{
	STRPTR p;

	if (data->homepage)
		free(data->homepage);

	if (data->download_dir)
		free(data->download_dir);

	data->homepage = DupStr((STRPTR)getv(data->str_homepage, MUIA_String_Contents));
	data->download_dir = DupStr((STRPTR)getv(data->str_dldir, MUIA_String_Contents));
}

static void insert_sorted(struct MinList *list, struct windownode *new)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	struct windownode *node = (APTR)GetHead(list);
	LOG(("DEBUG: insert_sorted: new->num=%lu\n", new->num));
	while (node)
	{
		if (node->num > new->num)
		{
			Insert((struct List *)list, (APTR)new, GetPred(node));
			return;
		}

		node = (APTR)GetSucc(node);
	}
	LOG(("DEBUG: insert_sorted: appending new node to end of list\n"));
	ADDTAIL(list, new);
}

STATIC CONST CONST_STRPTR prefslist[] =
{
	"General",
	"Downloads",
	//"Fastlinks",
	NULL
};

#if 1
// W applicationclass.c, w DEFNEW dodaj debug na początku:

DEFNEW
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));

	APTR prefswin, prefs_save, prefs_use, prefs_cancel, lv_prefs, gp_prefs;
	APTR menustrip;
	APTR lv_fastlinks;
	APTR str_homepage, str_dldir;
	APTR dlwin, hotlistwin;

	obj = DoSuperNew(cl, obj,
		SubWindow, hotlistwin = NewObject(gethotlistwindowclass(), NULL, TAG_DONE),
		SubWindow, dlwin =  NewObject(getdownloadwindowclass(), NULL, TAG_DONE),

		SubWindow, prefswin = WindowObject,
			MUIA_Window_ID, MAKE_ID('W','P','R','F'),
			MUIA_Window_Title, "Preferences",
			MUIA_Window_NoMenus, TRUE,
			WindowContents, HGroup,
				Child, lv_prefs = ListviewObject,
					MUIA_Listview_List, ListObject,
						InputListFrame,
						MUIA_List_AdjustWidth, TRUE,
						MUIA_List_SourceArray, prefslist,
						MUIA_List_Active, 0,
					End,
				End,
				Child, VGroup,
					Child, gp_prefs = VGroup,
						MUIA_Group_PageMode, TRUE,
						Child, VGroup,
							Child, HGroup,
								Child, MakeLabel("_Homepage:"),
								Child, str_homepage = MakePrefsString("_Homepage:", NETSURF_HOMEPAGE, 1024, MAKE_ID('S','H','P','U')),
							End,
						End,
						Child, VGroup,
							Child, HGroup,
								Child, MakeLabel("_Downloads:"),
								Child, str_dldir = MakeDirString("_Downloads:", "RAM:", MAKE_ID('S', 'D','L','S')),
							End,
						End,
					#if 0
						Child, VGroup,
							Child, lv_fastlinks = ListviewObject,
								MUIA_Listview_List, ListObject,
									InputListFrame,
								End,
							End,
						End,
					#endif
					End,
					Child, HGroup,
						Child, prefs_save = MakeButton("_Save"),
						Child, RectangleObject, End,
						Child, prefs_use = MakeButton("_Use"),
						Child, RectangleObject, End,
						Child, prefs_cancel = MakeButton("_Cancel"),
					End,
				End,
			End,
		End,
		MUIA_Application_Commands, &rexxcommands,
		MUIA_Application_Menustrip, menustrip = MUI_MakeObject(MUIO_MenustripNM, mainmenu, MUIO_MenustripNM_CommandKeyCheck),
		TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;
		ULONG len;

		data->hotlistwin = hotlistwin;
		data->dlwin = dlwin;
		data->prefswin = prefswin;

		data->str_homepage = str_homepage;
		data->str_dldir = str_dldir;

		NEWLIST(&data->windowlist);
		set(obj, MA_Application_DownloadsInProgress, 0);

		len = strlen("NetSurf ") + strlen(netsurf_version) + 1;
		data->screentitle = AllocTaskPooled(len);

		if (data->screentitle)
			snprintf(data->screentitle, len, "NetSurf %s", netsurf_version);
		else
			data->screentitle = "NetSurf";
		LOG(("#### DEBUG: Allocated screentitle: %s\n", data->screentitle));
		DoMethod(lv_prefs, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, gp_prefs, 3, MUIM_Set, MUIA_Group_ActivePage, MUIV_TriggerValue);
		DoMethod(prefswin, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, obj, 2, MUIM_Application_Load, APPLICATION_ENV_PREFS);
		DoMethod(prefs_save, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_Application_PrefsSave, TRUE);
		DoMethod(prefs_use, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_Application_PrefsSave, FALSE);
		DoMethod(prefs_cancel, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MUIM_Application_Load, APPLICATION_ENV_PREFS);

#if 0
			data->menustrip = menustrip;
			data->mn_reload     = (APTR)DoMethod(menustrip, MUIM_FindUData, MNA_RELOAD);
#endif
	}

	return (IPTR)obj;
}
#else
DEFNEW
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));

	APTR prefswin, prefs_save, prefs_use, prefs_cancel, lv_prefs, gp_prefs;
	APTR menustrip;
	APTR lv_fastlinks;
	APTR str_homepage, str_dldir;
	APTR dlwin, hotlistwin;
	#warning "Disabled hotlistwin and dlwin creation for now, uncomment if needed"
#if 0
	LOG(("DEBUG: Before creating hotlistwin\n"));
	hotlistwin = NewObject(gethotlistwindowclass(), NULL, TAG_DONE);
	LOG(("DEBUG: hotlistwin created: %p\n", hotlistwin));

	LOG(("DEBUG: Before creating dlwin\n"));
	dlwin = NewObject(getdownloadwindowclass(), NULL, TAG_DONE);
	LOG(("DEBUG: dlwin created: %p\n", dlwin));

	LOG(("DEBUG: Before creating prefswin\n"));

	obj = DoSuperNew(cl, obj,
		SubWindow, hotlistwin,
		SubWindow, dlwin,
		SubWindow, prefswin = WindowObject,
			MUIA_Window_ID, MAKE_ID('W','P','R','F'),
			MUIA_Window_Title, "Preferences",
			MUIA_Window_NoMenus, TRUE,
			WindowContents, HGroup,
				Child, lv_prefs = ListviewObject,
					MUIA_Listview_List, ListObject,
						InputListFrame,
						MUIA_List_AdjustWidth, TRUE,
						MUIA_List_SourceArray, prefslist,
						MUIA_List_Active, 0,
					End,
				End,
				Child, VGroup,
					Child, gp_prefs = VGroup,
						MUIA_Group_PageMode, TRUE,
						Child, VGroup,
							Child, HGroup,
								Child, MakeLabel("_Homepage:"),
								Child, str_homepage = MakePrefsString("_Homepage:", NETSURF_HOMEPAGE, 1024, MAKE_ID('S','H','P','U')),
							End,
						End,
						Child, VGroup,
							Child, HGroup,
								Child, MakeLabel("_Downloads:"),
								Child, str_dldir = MakeDirString("_Downloads:", "RAM:", MAKE_ID('S', 'D','L','S')),
							End,
						End,
					End,
					Child, HGroup,
						Child, prefs_save = MakeButton("_Save"),
						Child, RectangleObject, End,
						Child, prefs_use = MakeButton("_Use"),
						Child, RectangleObject, End,
						Child, prefs_cancel = MakeButton("_Cancel"),
					End,
				End,
			End,
		End,
		MUIA_Application_Commands, &rexxcommands,
		MUIA_Application_Menustrip, menustrip = MUI_MakeObject(MUIO_MenustripNM, mainmenu, MUIO_MenustripNM_CommandKeyCheck),
		TAG_MORE, msg->ops_AttrList);
#else
	obj = DoSuperNew(cl, obj,
		MUIA_Application_Commands, &rexxcommands,
		MUIA_Application_Menustrip, MUI_MakeObject(MUIO_MenustripNM, mainmenu, MUIO_MenustripNM_CommandKeyCheck),
		TAG_MORE, msg->ops_AttrList);
#endif
		LOG(("DEBUG: prefswin and main object created: %p\n", obj));
	if (obj)
	{
		GETDATA;
		ULONG len;

		//data->hotlistwin = hotlistwin;
		//data->dlwin = dlwin;
		//data->prefswin = prefswin;

		//data->str_homepage = str_homepage;
		//data->str_dldir = str_dldir;

		NEWLIST(&data->windowlist);

		len = strlen(netsurf_version) + sizeof("NetSurf %s");

		data->screentitle = AllocTaskPooled(len);

		if (data->screentitle)
			NewRawDoFmt("NetSurf %s", NULL, data->screentitle, netsurf_version);
		else
			data->screentitle = "NetSurf";
		LOG(("#### DEBUG: Allocated screentitle: %s\n", data->screentitle));
		LOG(("DEBUG: Completed object setup\n"));

		//DoMethod(lv_prefs, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, gp_prefs, 3, MUIM_Set, MUIA_Group_ActivePage, MUIV_TriggerValue);
		//DoMethod(prefswin, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, obj, 2, MUIM_Application_Load, APPLICATION_ENV_PREFS);
		//DoMethod(prefs_save, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_Application_PrefsSave, TRUE);
		//DoMethod(prefs_use, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_Application_PrefsSave, FALSE);
		//DoMethod(prefs_cancel, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MUIM_Application_Load, APPLICATION_ENV_PREFS);
	}

	return (IPTR)obj;
}

#endif

DEFSMETHOD(HotlistWindow_Insert)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	return DoMethodA(data->hotlistwin, (Msg)msg);
}
DEFGET
{
    LOG(("DEBUG: FUNCTION=%s START", __FUNCTION__));
    LOG(("DEBUG: msg=%p", msg));
    LOG(("DEBUG: msg->opg_AttrID=0x%08x", msg->opg_AttrID));
    LOG(("DEBUG: MA_Application_Homepage=0x%08x", MA_Application_Homepage));
    
    GETDATA;
    LOG(("DEBUG: GETDATA completed, data=%p", data));
    
    switch (msg->opg_AttrID)
    {
        case MA_Application_Homepage:
            LOG(("DEBUG: Handling MA_Application_Homepage"));
            LOG(("DEBUG: data->homepage=%p", data->homepage));
            
            // SPRAWDŹ CZY msg->opg_Storage JEST PRAWIDŁOWE
            LOG(("DEBUG: msg->opg_Storage=%p", msg->opg_Storage));
            if (!msg->opg_Storage) {
                LOG(("ERROR: msg->opg_Storage is NULL!"));
                return DOSUPER;
            }
            
            LOG(("DEBUG: About to set storage"));
			LOG(("DEBUG: Returning homepage: %s", data->homepage));
            *msg->opg_Storage = (IPTR)"https://www.example.com";//(IPTR)data->homepage;
            LOG(("DEBUG: Set storage to 0x%08x", *msg->opg_Storage));
            
            // ZMIEŃ NA TRUE - może MUI oczekuje TRUE dla obsłużonych atrybutów
            LOG(("DEBUG: FUNCTION=%s END - returning TRUE", __FUNCTION__));
            return FALSE;//TRUE;  // <- ZMIANA Z FALSE NA TRUE
            
        default:
            LOG(("DEBUG: Unknown attribute 0x%08x - calling DOSUPER", msg->opg_AttrID));
			
            break;
    }
    
    LOG(("DEBUG: FUNCTION=%s END - calling DOSUPER", __FUNCTION__));
    ULONG result = DOSUPER;
    LOG(("DEBUG: DOSUPER returned %lu", result));
    return result;
}
#if 0

DEFGET
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
//return 0;
	GETDATA;

	switch (msg->opg_AttrID)
	{
		case MA_Application_Homepage:
			if (data->homepage)
			{
				LOG(("DEBUG: Returning homepage: %s", data->homepage));
				IPTR len = strlen(data->homepage) + 1;
				msg->opg_Storage = AllocTaskPooled(len);
				if (msg->opg_Storage)
				{
					strcpy((STRPTR)msg->opg_Storage, data->homepage);
					return len;
				}
			}
			else
			{
				msg->opg_Storage = NULL;
				LOG(("DEBUG: No homepage set, returning NULL"));
				return 0;
			}
			*msg->opg_Storage = (IPTR)data->homepage;
			return FALSE;
	}
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	//return false;
	return DOSUPER;
}
#endif
DEFSMETHOD(Application_Download)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	struct FileRequester *filereq;
	struct download *dl;
	CONST_STRPTR default_file;
	CONST_STRPTR initial_drawer;

	filereq = (struct FileRequester *)MUI_AllocAslRequest(ASL_FileRequest, NULL);
	dl = NULL;
	default_file = (msg->filename && msg->filename[0]) ? msg->filename : FilePart(msg->url);
	if (!default_file || !default_file[0])
		default_file = "download";
	initial_drawer = data->download_dir ? data->download_dir : "RAM:";

	if (filereq)
	{
		if (MUI_AslRequestTags(filereq,
			ASLFR_TitleText, "NetSurf",
			ASLFR_DoSaveMode,TRUE,
			ASLFR_InitialFile, default_file,
			ASLFR_InitialDrawer, initial_drawer,
			TAG_DONE))
		{
			ULONG size, filenamelen;

			filenamelen = strlen(filereq->fr_File) + 1;
			size = strlen(filereq->fr_Drawer) + 1 + sizeof(*dl) + filenamelen;
			dl = AllocMem(size, MEMF_ANY);

			if (dl)
			{
				dl->filename = dl->data;
				dl->path = &dl->data[filenamelen];
				dl->done = 0;

				strcpy(dl->filename, filereq->fr_File);
				strcpy(dl->path, filereq->fr_Drawer);

				DoMethod(data->dlwin, MUIM_List_InsertSingle, dl, MUIV_List_Insert_Bottom);
				set(data->dlwin, MUIA_Window_Open, TRUE);
				IPTR in_progress = getv(obj, MA_Application_DownloadsInProgress);
				set(obj, MA_Application_DownloadsInProgress, in_progress + 1);
			}
		}

		MUI_FreeAslRequest(filereq);
	}

	return (IPTR)dl;
}

DEFTMETHOD(Application_DownloadUpdate)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	return DoMethod(data->dlwin, MUIM_List_Redraw, MUIV_List_Redraw_All);
}

DEFSMETHOD(Application_DownloadDone)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	msg->MethodID = MM_Download_Done;
	return DoMethodA(data->dlwin, (Msg)msg);
}

DEFSMETHOD(Application_DownloadError)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	msg->MethodID = MM_Download_Error;
	return DoMethodA(data->dlwin, (Msg)msg);
}

DEFMMETHOD(Application_Load)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;

	set(data->prefswin, MUIA_Window_Open, FALSE);
	DOSUPER;
	update_prefs(data);
	return 0;
}

DEFSMETHOD(Application_KillWindow)
{
	DoMethod(obj, OM_REMMEMBER, msg->window);
	MUI_DisposeObject(msg->window);
	return 0;
}

DEFSMETHOD(Application_PrefsSave)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;

	if (msg->SaveENVARC)
		DoMethod(obj, MUIM_Application_Save, APPLICATION_ENVARC_PREFS);

	DoMethod(obj, MUIM_Application_Save, APPLICATION_ENV_PREFS);

	update_prefs(data);

	return set(data->prefswin, MUIA_Window_Open, FALSE);
}

DEFSMETHOD(Application_Login)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	APTR loginwin;

	loginwin = NewObject(getloginwindowclass(), NULL,
		MA_LoginWindow_Host, msg->host,
		MA_LoginWindow_Realm, msg->realm,
		MA_LoginWindow_URL, msg->url,
		MA_LoginWindow_Browser, msg->browser,
		TAG_DONE);

	if (loginwin)
	{
		DoMethod(obj, OM_ADDMEMBER, loginwin);
		set(loginwin, MUIA_Window_Open, TRUE);
	}

	return 0;
}

DEFSMETHOD(Application_CloseWindow)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	struct windownode *node = msg->windownode;

	REMOVE(node);
	DoMethod(obj, OM_REMMEMBER, node->winobj);
	MUI_DisposeObject(node->winobj);
	FreeMem(node, sizeof(*node));

	if (ISLISTEMPTY(&data->windowlist))
	{
		DoMethod(obj, MUIM_Application_PushMethod, obj, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	}

	return 0;
}

DEFTMETHOD(Application_About)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;

	if (!data->aboutwin)
	{
		#if defined(__MORPHOS__)
		STATIC CONST_STRPTR credits = 
#if 0
			"\033b%P\033n"               /* add. programming */
			"\n\tJohn Doe"  
			"\n"       
			"\n\033b%l\033n"             /* translations (abc order) */
			"\n\tJohn Doe"
			"\n"
			"\n\033b%L\033n"             /* proofreading (abc order) */
			"\n\tJohn Doe"
#else
			""
#endif
			;

		data->aboutwin = AboutboxObject,
			//MUIA_Aboutbox_Credits, credits,
		End;
		#else
		#warning implement proper about window for OS3 builds
		data->aboutwin = WindowObject,
			MUIA_Window_Title, "About",
			MUIA_Window_NoMenus, TRUE,
			WindowContents, HGroup,
				Child, RectangleObject, End,
				End,
			TAG_DONE);
		#endif

		if (data->aboutwin)
		{
			DoMethod(obj, OM_ADDMEMBER, data->aboutwin);
		}
	}

	if (data->aboutwin)
	{
		set(data->aboutwin, MUIA_Window_Open, TRUE);
	}

	return 0;
}

DEFSMETHOD(Application_OpenWindow)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	APTR win = win;

	switch (msg->window_id)
	{
		case WINDOW_SETTINGS:
			win = data->prefswin;
			break;

		case WINDOW_DOWNLOADS:
			win = data->dlwin;
			break;

		case WINDOW_HOTLIST:
			win = data->hotlistwin;
			break;
	}

	return set(win, MUIA_Window_Open, TRUE);
}

DEFSMETHOD(Application_NewWindow)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	struct windownode *node;
	ULONG num, rc;

	num = 0;

	ITERATELIST(node, &data->windowlist)
	{
		if (node->num > num)
			break;

		num++;
	}

	node = AllocMem(sizeof(*node), MEMF_ANY);
	rc = FALSE;

	if (node)
	{
		ULONG win_id;
		APTR win;

		node->num = num;

		/* Build a unique 32-bit identifier using 'M' as the high byte. */
		win_id = ((ULONG)'M' << 24) | (num & 0x00ffffff);

		LOG(("DEBUG: Creating window with ID 0x%08lx (index %lu)", win_id, num));
		win = NewObject(getwindowclass(), NULL,
			MUIA_Window_ID, win_id,
			MUIA_Window_ScreenTitle, data->screentitle,
			MUIA_Window_Title, "NetSurf",
			MA_Window_Node, node,
		End;

		if (win)
		{
			node->winobj = win;

			data->active_window = node;

			insert_sorted(&data->windowlist, node);
			LOG(("DEBUG: About to call OM_ADDMEMBER"));
			DoMethod(obj, OM_ADDMEMBER, win);

			LOG(("DEBUG: OM_ADDMEMBER completed, scheduling window open"));
			DoMethod(obj, MUIM_Application_PushMethod, win, 3, MUIM_Set, MUIA_Window_Open, TRUE);
			rc = TRUE;
			LOG(("DEBUG: MUIM_Application_PushMethod queued window open"));
		}
		else
		{
			FreeMem(node, sizeof(*node));
		}
	}

	return rc;
}

DEFSMETHOD(Application_AddBrowser)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	APTR o;

	if (!data->active_window)
		DoMethod(obj, MM_Application_NewWindow);

	o = NULL;

	if (data->active_window)
	{
		struct gui_window *g;

		g = msg->context;
		g->win = data->active_window->winobj;

		/* MM_Window_AddPage returns browser object */
		o = (APTR)DoMethod(data->active_window->winobj, MM_Window_AddPage, g);
	}

	return (IPTR)o;
}

DEFSMETHOD(Application_SaveDocument)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	struct FileRequester *filereq;

	filereq = (struct FileRequester *)MUI_AllocAslRequest(ASL_FileRequest, NULL);

	if (filereq)
	{
		CONST_STRPTR title;
		STRPTR filename, buf;
		ULONG length;

		filename = (STRPTR)getv(data->active_window->winobj, MA_Browser_URL);

		length = strlen(filename) + 1;

		if (msg->type == SAVEDOC_PDF)
			length += 4;

		buf = AllocMem(length, MEMF_ANY);

		if (buf)
		{
			bcopy(filename, buf, length);

			for (;;)
			{
				filename = strrchr(buf, '/');

				if (filename)
				{
					if (filename[1])
					{
						filename++;
						break;
					}

					*filename = '\0';
				}
			}

			if (!filename)
			{
				filename = buf;
				strcpy(buf, "index");
			}

			if (msg->type == SAVEDOC_PDF)
			{
				char *p = strrchr(filename, '.');

				if (!p || stricmp(p, "pdf"))
					strcat(buf, ".pdf");
			}

			switch (msg->type)
			{
				default:
				case SAVEDOC_HTML:
					title = "Save page to...";
					break;

				case SAVEDOC_SOURCE:
					title = "Save source";
					break;

				case SAVEDOC_PDF:
					title = "Save as PDF";
					break;
			}

			if (MUI_AslRequestTags(filereq,
				ASLFR_TitleText, title,
				ASLFR_DoSaveMode,TRUE,
				ASLFR_InitialFile, filename,
				ASLFR_InitialDrawer, data->download_dir,
				ASLFR_DrawersOnly, msg->type == SAVEDOC_HTML ? TRUE : FALSE,
				TAG_DONE))
			{
				#if MULTITHREADED
				struct worknode *node;
				ULONG length;

				if (msg->type != SAVEDOC_HTML)
					filename = filereq->fr_File;

				length = strlen(filename) + strlen(filereq->fr_Drawer) + 4;
				node = AllocMem(sizeof(*node) + length, MEMF_ANY);

				if (node)
				{
					node->size = length + sizeof(*node);
					node->param1 = msg->type;
					strcpy(node->data, filereq->fr_Drawer);
					AddPart(node->data, filename, length);

					netsurf_add_job((APTR)getv(data->active_window->winobj, MA_Browser_Browser), JOB_SAVE_DOC, node);
				}
				#else
				#warning MM_Application_SaveDocument not implemented
				#endif
			}

			FreeMem(buf, length);
		}

		MUI_FreeAslRequest(filereq);
	}

	return 0;
}

DEFSMETHOD(Application_AskPasswordPDF)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;
	APTR win;

	win = NewObject(getpdfclass(), NULL, TAG_DONE);

	if (win)
	{
		DoMethod(obj, OM_ADDMEMBER, win);
		set(win, MUIA_Window_Open, TRUE);
	}

	return 0;
}

DEFSMETHOD(Application_SetPasswordPDF)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;

	return DoMethod(obj, MM_Application_KillWindow, msg->window);
}

DEFSMETHOD(Window_MenuAction)
{
	LOG(("DEBUG: FUNCTION=%s", __FUNCTION__));
	GETDATA;

	if (!data->active_window)
		DoMethod(obj, MM_Application_NewWindow);

	if (data->active_window)
		DoMethodA(data->active_window->winobj, (Msg)msg);

	return 0;
}

DEFSMETHOD(Browser_Go)
{
	LOG(("DEBUG: ApplicationClass Browser_Go\n"));
	GETDATA;
	return DoMethodA(data->active_window->winobj, (Msg)msg);
}

BEGINMTABLE
DECNEW
DECGET
DECMMETHOD(Application_Load)
DECSMETHOD(Application_About)
DECSMETHOD(Application_AddBrowser)
DECSMETHOD(Application_AskPasswordPDF)
DECSMETHOD(Application_CloseWindow)
DECSMETHOD(Application_Download)
DECSMETHOD(Application_DownloadDone)
DECSMETHOD(Application_DownloadError)
DECSMETHOD(Application_DownloadUpdate)
DECSMETHOD(Application_KillWindow)
DECSMETHOD(Application_Login)
DECSMETHOD(Application_NewWindow)
DECSMETHOD(Application_OpenWindow)
DECSMETHOD(Application_PrefsSave)
DECSMETHOD(Application_SaveDocument)
DECSMETHOD(Application_SetPasswordPDF)
DECSMETHOD(Browser_Go)
DECSMETHOD(HotlistWindow_Insert)
DECSMETHOD(Window_MenuAction)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Application, applicationclass)
