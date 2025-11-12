/*
 * Copyright 2001-2005 by David Gerber <zapek@morphos.net>
 * Copyright 2005-2007 Ambient Open Source Team
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 * Copyright 2009 Fabien Coeurjoly
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

#include <devices/rawkeycodes.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/mui.h"

struct Data
{
	Object *str;
	Object *pop;
	Object *lv_entries;

	ULONG added;
	struct MUI_EventHandlerNode ehnode;
};

MUI_HOOK(history_popclose, APTR list, APTR str)
{
#if 0
	WebHistoryItem *s;

	DoMethod((Object *) list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &s);
	if(s)
	{
		nnset((Object *) str, MUIA_String_Contents, s->URLString());
		set(_win((Object *) str), MUIA_Window_ActiveObject, (Object *) str);
		//DoMethod(_win((Object *) str), MM_OWBWindow_LoadURL, s->URLString());
	}
#endif

	return 0;
}

MUI_HOOK(history_popopen, APTR pop, APTR win)
{
	Object *list = (Object *)getv((Object *)pop, MUIA_Listview_List);

	SetAttrs((Object *) win, MUIA_Window_DefaultObject, list, MUIA_Window_ActiveObject, list, TAG_DONE);
	//set(list, MUIA_List_Active, 0);

	return (TRUE);
}

STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
	struct TagItem *tag, *tstate;

	tstate = tags;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		switch (tag->ti_Tag)
		{
			case MA_PopString_ActivateString:
				if(tag->ti_Data)
				{
					set((Object *) _win(obj), MUIA_Window_ActiveObject, data->str);
				}
				break;
		}
	}
}

DEFNEW
{
	Object *str, *bt_prev, *bt_next;
	Object *pop, *bt_pop, *lv_entries;

	obj = (Object *) DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		Child, pop = PopobjectObject,
			MUIA_Popstring_String, str = StringObject,
				StringFrame,
				MUIA_Textinput_ResetMarkOnCursor, TRUE,
				MUIA_CycleChain, 1,
				MUIA_String_MaxLen, 2048,
			End,
			MUIA_Popstring_Button, bt_pop = PopButton(MUII_PopUp),
			MUIA_Popobject_Object, lv_entries =  (Object *) NewObject(gethistorylistclass(), NULL, TAG_DONE),
			MUIA_Popobject_ObjStrHook, &history_popclose_hook,
			MUIA_Popobject_WindowHook, &history_popopen_hook,
		End,
		TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->added = FALSE;

		data->str = str;
		data->pop = pop;
		data->lv_entries = lv_entries;

		data->ehnode.ehn_Object   = obj;
		data->ehnode.ehn_Class    = cl;
		data->ehnode.ehn_Events   = IDCMP_RAWKEY;
		data->ehnode.ehn_Priority = 5;
		data->ehnode.ehn_Flags    = MUI_EHF_GUIMODE;

		set(bt_pop, MUIA_CycleChain, 1);

		set(lv_entries, MUIA_Popstring_String, str);

		DoMethod(data->lv_entries, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, data->lv_entries, 1, MM_HistoryList_SelectChange);
		DoMethod(data->lv_entries, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, obj, 2, MUIM_Popstring_Close, TRUE);
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
		case MUIA_Popobject_Object:
            *msg->opg_Storage = (ULONG) data->lv_entries;
            return TRUE;

		case MUIA_Popstring_String:
			*msg->opg_Storage = (ULONG) data->str;
			return TRUE;

		case MUIA_String_Contents:
			return get(data->str, MUIA_String_Contents, msg->opg_Storage);
	}

	return DOSUPER;
}

DEFMMETHOD(Show)
{
	ULONG rc;
	GETDATA;

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

	if(data->added)
	{
		DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode );
		data->added = FALSE;
	}
	return DOSUPER;
}

DEFMMETHOD(HandleEvent)
{
	ULONG rc = 0;
	struct IntuiMessage *imsg;

	if((imsg = msg->imsg))
	{
		if(imsg->Class == IDCMP_RAWKEY)
		{
			GETDATA;
			Object *active = (Object *) getv(_win(obj), MUIA_Window_ActiveObject);

			if(active == data->str && !(imsg->Code & IECODE_UP_PREFIX))
			{
				switch(imsg->Code & ~IECODE_UP_PREFIX)
				{
					case RAWKEY_ESCAPE:
						DoMethod(obj, MUIM_Popstring_Close, FALSE);
	                    rc = MUI_EventHandlerRC_Eat;
						break;

					case RAWKEY_RETURN:
						DoMethod(obj, MUIM_Popstring_Close, TRUE);
	                    rc = MUI_EventHandlerRC_Eat;
						break;

					case RAWKEY_UP:
					case RAWKEY_NM_WHEEL_UP:
						set(data->lv_entries, MUIA_List_Active, MUIV_List_Active_Up);
						rc = MUI_EventHandlerRC_Eat;
						break;

					case RAWKEY_DOWN:
					case RAWKEY_NM_WHEEL_DOWN:
						DoMethod(obj, MUIM_Popstring_Open);
						set(_win(obj), MUIA_Window_Activate, TRUE);
						set(_win(obj), MUIA_Window_ActiveObject, data->str);
						set(data->lv_entries, MUIA_List_Active, MUIV_List_Active_Down);
	                    rc = MUI_EventHandlerRC_Eat;
						break;
				}
			}
		}
	}

	return rc;

}

DEFSMETHOD(HistoryPopString_Insert)
{
	GETDATA;
#if 0
	WebHistoryItem *s = (WebHistoryItem *) msg->item;

	// DoMethod(data->lv_entries, MM_HistoryPopString_Remove, s);
	return DoMethod(data->lv_entries, MUIM_List_InsertSingle, s, MUIV_List_Insert_Top);
#else
	return 0;
#endif
}

DEFSMETHOD(HistoryPopString_Remove)
{
	GETDATA;
#warning fixme
#if 0
	WebHistoryItem *s, *x;
	ULONG i;

	s = (WebHistoryItem *) msg->item;

	for (i = 0; ; i++)
	{
		DoMethod(data->lv_entries, MUIM_List_GetEntry, i, &x);

		if (!x)
			break;

		if (s == x)
		{
			DoMethod(data->lv_entries, MUIM_List_Remove, i);
			break;
		}
	}
#endif
	return 0;
}

DEFMMETHOD(Popstring_Open)
{
	GETDATA;

	// Avoid closing popup when it's told to be opened again. :)
	if(!getv(data->lv_entries, MA_HistoryList_Opened))
	{
		return DOSUPER;
	}
	else
	{	
		return 0;
	}
}

DEFMMETHOD(Popstring_Close)
{
	return DOSUPER;
}

BEGINMTABLE
DECNEW
DECGET
DECSET
DECMMETHOD(Show)
DECMMETHOD(Hide)
DECMMETHOD(HandleEvent)
DECSMETHOD(HistoryPopString_Insert)
DECSMETHOD(HistoryPopString_Remove)
DECMMETHOD(Popstring_Open)
DECMMETHOD(Popstring_Close)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, historypopstringclass)
