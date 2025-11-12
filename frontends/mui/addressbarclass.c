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

#include <proto/intuition.h>
#include <proto/utility.h>

#include <clib/intuition_protos.h> // SetAttrs
#undef NO_INLINE_STDARG
#include <inline/intuition.h>

#include "mui/applicationclass.h"
#include "mui/mui.h"

struct Data
{
	APTR pop_path;
	LONG prevlen;
};

#include <stdarg.h>
#include <exec/types.h>
#include <exec/memory.h>

#include "utils/log.h"

static void loadhistory(struct Data *data)
{
#if 0
	WebHistory* history = WebHistory::sharedHistory();
	std::vector<WebHistoryItem *> historyList = *(history->historyList());
	APTR popobj;

	popobj = getv(data->pop_path, MUIA_Popobject_Object);

	set(popobj, MUIA_List_Quiet, TRUE);

	DoMethod((Object *) getv(data->pop_path, MUIA_Popobject_Object), MUIM_List_Clear);

	for(unsigned int i = 0; i < historyList.size(); i++)
	{
		WebHistoryItem *webHistoryItem = historyList[i];

		if(webHistoryItem)
		{
			DoMethod(data->pop_path, MM_HistoryPopString_Insert, webHistoryItem);
		}
	}

	set(popobj, MUIA_List_Quiet, FALSE);
#endif
}

STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
	struct TagItem *tag, *tstate;

	tstate = tags;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_Text_Contents:
				nnset(data->pop_path, MUIA_String_Contents, (char *) tag->ti_Data);
				break;

			case MA_AddressBarGroup_Active:
				if(tag->ti_Data)
				{
					set((Object *) _win(obj), MUIA_Window_ActiveObject, (Object *) getv(data->pop_path, MUIA_Popstring_String));
				}
				break;
		}
	}
}

DEFNEW
{
	APTR str_path, lv_entries, pop_path, bt_pop;
	APTR bt_hotlist, bt_add;

	obj = DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		Child, pop_path = NewObject(gethistorypopstringclass(), NULL, TAG_DONE),
		Child, MakeVBar(),
		Child, bt_hotlist = PopButton(MUII_PopUp),
		Child, bt_add = MakeButton("_Add"),
		TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->pop_path = pop_path;
		data->prevlen = -1;

		set(bt_hotlist, MUIA_CycleChain, 1);
		set(bt_add, MUIA_Weight, 0);

		loadhistory(data);

		DoMethod(pop_path, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 2,  MM_Browser_Go, NULL);
		DoMethod(bt_hotlist, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MM_Application_OpenWindow, WINDOW_HOTLIST);
		DoMethod(bt_add, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 1, MM_Window_InsertBookmark);

		DoMethod((APTR)getv(pop_path, MUIA_Popstring_String), MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, obj, 1, MM_Browser_AutoComplete, 0, 0);
	}

	return (IPTR)obj;
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

DEFGET
{
	GETDATA;

	switch (msg->opg_AttrID)
	{
		case MUIA_String_Contents:
			return DoMethodA(data->pop_path, (Msg)msg);

		case MA_AddressBarGroup_PopString:
			*msg->opg_Storage = (ULONG) data->pop_path;
			return TRUE;

		case MUIA_Popstring_String:
			*msg->opg_Storage = getv(data->pop_path, MUIA_Popstring_String);
			return TRUE;

		case MUIA_Popobject_Object:
			*msg->opg_Storage = getv(data->pop_path, MUIA_Popobject_Object);
			return TRUE;
	}

	return DOSUPER;
}

DEFMMETHOD(List_InsertSingle)
{
	GETDATA;
	return DoMethodA(data->pop_path, (Msg)msg);
}

DEFSMETHOD(Browser_Go)
{
		LOG(("DEBUG: BRowser_Go called in %s\n",__FUNCTION__));
	if (muiRenderInfo(obj))
	{
			LOG(("DEBUG: Found render info, proceeding with Browser_Go\n"));
		GETDATA;
		msg->url = (APTR)getv(data->pop_path, MUIA_String_Contents);
		DoMethod(data->pop_path, MM_PopString_Insert, msg->url);
		DoMethodA(_win(obj), (Msg)msg);
	}
	LOG(("DEBUG: WArNING: Browser_Go called without render info\n"));
	return 0;
}

DEFSMETHOD(HistoryPopString_Insert)
{
	GETDATA;
	return DoMethod(data->pop_path, MM_HistoryPopString_Insert, msg->item);
}

DEFSMETHOD(HistoryPopString_Remove)
{
	GETDATA;
	return DoMethod(data->pop_path, MM_HistoryPopString_Remove, msg->item);
}

DEFSMETHOD(Browser_AutoComplete)
{
	//if(getv(app, MA_OWBApp_URLCompletionType) != MV_OWBApp_URLCompletionType_None)
	{
		if (muiRenderInfo(obj))
		{
			GETDATA;
			STRPTR text = (STRPTR) getv(data->pop_path, MUIA_String_Contents);
			
			if(text)
			{
				LONG len = strlen(text);

				DoMethod(_win(obj), MM_Browser_AutoComplete, len, data->prevlen);

	         text = (STRPTR) getv(data->pop_path, MUIA_String_Contents);
				data->prevlen = strlen(text);
			}
		}
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECGET
DECSET
DECMMETHOD(List_InsertSingle)
DECSMETHOD(Browser_AutoComplete)
DECSMETHOD(Browser_Go)
DECSMETHOD(HistoryPopString_Insert)
DECSMETHOD(HistoryPopString_Remove)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, addressbargroupclass)
