/*
 * Copyright 2001-2005 by David Gerber <zapek@morphos.net>
 * Copyright 2005-2007 Ambient Open Source Team
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

#include <devices/rawkeycodes.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/mui.h"
#include "utils/messages.h"

static ULONG DoSuperNew1(struct IClass *cl, APTR obj, ULONG tag1, ...)
{
return ((APTR)DoSuperMethod(cl, obj, OM_NEW, &tag1, NULL));
}
struct Data
{
	APTR str;
	APTR pop;

	APTR lv_entries;

	struct MUI_EventHandlerNode ehnode;
};

MUI_HOOK(popclose, APTR list, APTR str)
{
	STRPTR s;

	DoMethod(list, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &s);
	nnset(str, MUIA_String_Contents, s);
	set(_win((Object *) str), MUIA_Window_ActiveObject, (Object *) str);

	return 0;
}

MUI_HOOK(popopen, APTR pop, APTR win)
{
	APTR list = (APTR)getv(pop , MUIA_Listview_List);

	SetAttrs(win, MUIA_Window_DefaultObject, list, MUIA_Window_ActiveObject, list, TAG_DONE);
	set(list, MUIA_List_Active, 0);

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
	APTR str, bt_prev, bt_next;
	APTR pop, bt_pop, lv_entries;

	obj = DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		Child, pop = PopobjectObject,
			MUIA_Popstring_String, str = StringObject,
				StringFrame,
				//MUIA_Textinput_RemainActive, TRUE,
				//MUIA_Textinput_Format, MUIV_Textinput_Format_Center,
				MUIA_Textinput_ResetMarkOnCursor, TRUE,
				MUIA_CycleChain, 1,
				MUIA_String_MaxLen, 2048,
			End,
			MUIA_Popstring_Button, bt_pop = PopButton(MUII_PopUp),
			MUIA_Popobject_Object, lv_entries = ListviewObject,
				MUIA_Listview_List, ListObject,
					InputListFrame,
					MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
					MUIA_List_DestructHook, MUIV_List_DestructHook_String,
					End,
				End,
			MUIA_Popobject_ObjStrHook, &popclose_hook,
			MUIA_Popobject_WindowHook, &popopen_hook,
		End,
		TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->str = str;
		data->pop = pop;
		data->lv_entries = lv_entries;

		data->ehnode.ehn_Object   = obj;
		data->ehnode.ehn_Class    = cl;
		data->ehnode.ehn_Events   = IDCMP_RAWKEY;
		data->ehnode.ehn_Priority = 1;
		data->ehnode.ehn_Flags    = MUI_EHF_GUIMODE;

		set(bt_pop, MUIA_CycleChain, 1);

		DoMethod(data->lv_entries, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, obj, 2, MUIM_Popstring_Close, TRUE);
		//DoMethod(str, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 2, MM_PopString_Insert, MUIV_TriggerValue);
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
		DoMethod( _win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
	}

	return rc;
}

DEFMMETHOD(Hide)
{
	GETDATA;
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode );
	return DOSUPER;
}

DEFMMETHOD(HandleEvent)
{
	struct IntuiMessage *imsg;

	if ((imsg = msg->imsg))
	{
		if (imsg->Class == IDCMP_RAWKEY)
		{
			GETDATA;
			APTR active;

			switch (imsg->Code)
			{
				case RAWKEY_DOWN + 0x80:
				case RAWKEY_NM_WHEEL_DOWN + 0x80:
					active = (APTR)getv(_win(obj) , MUIA_Window_ActiveObject);

					if	(active == data->str)
					{
						if (getv(data->lv_entries, MUIA_List_Entries))
						{
							DoMethod(data->pop, MUIM_Popstring_Open);
							return MUI_EventHandlerRC_Eat;
						}
					}
					break;
			}
		}
	}

	return 0;

}

DEFSMETHOD(PopString_Insert)
{
	STRPTR s, x;
	ULONG i;
	GETDATA;

	s = msg->txt;

	/*
	 * Find if the current entry is already there and remove it
	 * if so.
	 */

	for (i = 0; ; i++)
	{
		DoMethod(data->lv_entries, MUIM_List_GetEntry, i, &x);
						
		if (!x)
			break;

		if (!(stricmp(s, x))) /* XXX: we should strip spaces too.. */
		{
			DoMethod(data->lv_entries, MUIM_List_Remove, i);
			break;
		}
	}

#if 0
	while (getv(data->lv_entries, MUIA_List_Entries) >= data->max_items)
	{
		DoMethod(data->lv_entries, MUIM_List_Remove, MUIV_List_Remove_Last);
	}
#endif

	return DoMethod(data->lv_entries, MUIM_List_InsertSingle, s, MUIV_List_Insert_Top);
}

BEGINMTABLE
DECNEW
DECGET
DECMMETHOD(Show)
DECMMETHOD(Hide)
DECMMETHOD(HandleEvent)
DECSMETHOD(PopString_Insert)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, popstringclass)
