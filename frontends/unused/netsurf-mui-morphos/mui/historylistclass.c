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
 *
 * $Id: bookmarkkisttreeclass.c,v 0.0 2009/01/29 11:30:00 polymere Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <devices/rawkeycodes.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/mui.h"

enum
{
	POPMENU_OPEN_URL,
	POPMENU_OPEN_URL_IN_NEW_TAB,
	POPMENU_OPEN_URL_IN_NEW_WINDOW
};

struct Data
{
	Object *str;
	Object *cmenu;

	ULONG opened;
	ULONG added;
	struct MUI_EventHandlerNode ehnode;
};

static void doset(Object *obj, struct Data *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		/*
		case MUIA_List_DoubleClick:
		{
			WebHistoryItem *item = NULL;
			DoMethod(obj, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &item);
			if (item)
			{
			}
		}
		break;
		*/
		case MUIA_Popstring_String:
			data->str = (Object *) tag->ti_Data;
			break;
	}
	NEXTTAG
}

DEFNEW
{
	obj = (Object *) DoSuperNew(cl, obj,
		InputListFrame,
		//MUIA_List_Format, "MIW=-1 MAW=-2",
		MUIA_List_Title, FALSE,
		MUIA_ContextMenu, TRUE,
		TAG_MORE, INITTAGS
	);

	if (obj)
	{
		GETDATA;
		data->cmenu = NULL;

		data->added = FALSE;

		data->ehnode.ehn_Object   = obj;
		data->ehnode.ehn_Class    = cl;
		data->ehnode.ehn_Events   = IDCMP_RAWKEY;
		data->ehnode.ehn_Priority = 1;
		data->ehnode.ehn_Flags    = MUI_EHF_GUIMODE;

		doset(obj, data, msg->ops_AttrList);
	}
	return ((ULONG)obj);
}

DEFDISP
{
	GETDATA;
	if (data->cmenu)
	{
		MUI_DisposeObject(data->cmenu);
	}
	return DOSUPER;
}

DEFGET
{
	GETDATA;

	switch (msg->opg_AttrID)
	{
		case MA_HistoryList_Opened:
			*msg->opg_Storage = (ULONG) data->opened;
            return TRUE;
	}

	return DOSUPER;
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

DEFMMETHOD(Show)
{
	ULONG rc;
	GETDATA;

	data->opened = TRUE;

	if ((rc = DOSUPER))
	{
		if(!data->added)
		{
			DoMethod( _win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
			data->added = TRUE;
		}
	}

	return rc;
}

DEFMMETHOD(Hide)
{
	GETDATA;

	data->opened = FALSE;

	if(data->added)
	{
		DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode );
		data->added = FALSE;
	}
	return DOSUPER;
}

DEFMMETHOD(HandleEvent)
{
	struct IntuiMessage *imsg;

	if((imsg = msg->imsg))
	{
		if(imsg->Class == IDCMP_RAWKEY)
		{
			GETDATA;

			switch(imsg->Code & ~IECODE_UP_PREFIX)
			{
				case RAWKEY_UP:
				case RAWKEY_DOWN:
				case RAWKEY_NM_WHEEL_UP:
				case RAWKEY_NM_WHEEL_DOWN:
					//return MUI_EventHandlerRC_Eat;
					break;
				default:
					set(_win(data->str), MUIA_Window_Activate, TRUE);
					set(_win(data->str), MUIA_Window_ActiveObject, (Object *) data->str);
					break;
			}
		}
	}

	return 0;

}

DEFMMETHOD(List_Construct)
{
	return (ULONG)msg->entry;
}

DEFMMETHOD(List_Destruct)
{
	return TRUE;
}

DEFMMETHOD(List_Display)
{
#if 0
	WebHistoryItem *item = (WebHistoryItem *) msg->entry;

	if(item)
	{
		msg->array[0] = (char *) item->URLString();

		if( (ULONG)msg->array[-1] % 2 )
		{
			msg->array[-9] = (STRPTR) 10;
		}
	}
	else
	{
		//msg->array[0] = "URL";
	}
#endif
	return TRUE;
}

DEFTMETHOD(HistoryList_SelectChange)
{
	GETDATA;
#if 0
	WebHistoryItem *item = NULL;

	DoMethod(obj, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &item);
	if (item)
	{
		//ULONG len = strlen(item->URLString());
		nnset(data->str, MUIA_String_Contents, item->URLString());
		/*
		nnset(data->str, MUIA_Textinput_MarkStart, 0);
		nnset(data->str, MUIA_Textinput_MarkEnd, len - 1);
		*/
	}
#endif
	return 0;
}

DEFMMETHOD(ContextMenuBuild)
{
	GETDATA;

#if 0
	struct MUI_List_TestPos_Result res;
	WebHistoryItem *item;

	if (data->cmenu)
	{
		MUI_DisposeObject(data->cmenu);
		data->cmenu = NULL;
	}

	if (DoMethod(obj, MUIM_List_TestPos, msg->mx, msg->my, &res) && (res.entry != -1))
	{
		DoMethod(obj, MUIM_List_GetEntry, res.entry, (ULONG *)&item);

		if(item)
		{
			data->cmenu = MenustripObject,
					MUIA_Family_Child, MenuObjectT("OWB History"),
					MUIA_Family_Child, MenuitemObject,
						MUIA_Menuitem_Title, "\033bOpen URL",
						MUIA_UserData, POPMENU_OPEN_URL,
	                    End,
					MUIA_Family_Child, MenuitemObject,
						MUIA_Menuitem_Title, "Open URL in New Tab...",
						MUIA_UserData, POPMENU_OPEN_URL_IN_NEW_TAB,
						End,
					MUIA_Family_Child, MenuitemObject,
						MUIA_Menuitem_Title, "Open URL in New Window...",
						MUIA_UserData, POPMENU_OPEN_URL_IN_NEW_WINDOW,
						End,
	                End,
	            End;
		}
	}
#endif

	return (ULONG)data->cmenu;
}

DEFMMETHOD(ContextMenuChoice)
{
#if 0
	WebHistoryItem *item;

	DoMethod(obj, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, (ULONG *)&item);

	if(item)
	{
		ULONG udata = muiUserData(msg->item);

		switch(udata)
		{
			case POPMENU_OPEN_URL:
			{
                Object *window = (Object *) getv(app, MA_OWBApp_ActiveWindow);
				DoMethod(window, MM_OWBWindow_LoadURL, item->URLString());
			}
			break;

			case POPMENU_OPEN_URL_IN_NEW_TAB:
			{
                DoMethod(app, MM_OWBApp_AddBrowser, NULL, item->URLString(), FALSE, NULL, FALSE);
			}
			break;

			case POPMENU_OPEN_URL_IN_NEW_WINDOW:
			{
                DoMethod(app, MM_OWBApp_AddWindow, item->URLString(), FALSE, NULL);
			}
			break;
		}		 
	}
#endif

	return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECSET
DECGET
DECMMETHOD(List_Construct)
DECMMETHOD(List_Destruct)
DECMMETHOD(List_Display)
DECTMETHOD(HistoryList_SelectChange)
DECMMETHOD(ContextMenuBuild)
DECMMETHOD(ContextMenuChoice)
DECMMETHOD(Show)
DECMMETHOD(Hide)
DECMMETHOD(HandleEvent)
ENDMTABLE

DECSUBCLASS_NC(MUIC_List, historylistclass)
