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

#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/mui.h"

struct Data
{
	APTR pop_search;
	APTR bt_prev;
	APTR bt_next;
	APTR ch_case;
};

#if 0
STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
	struct TagItem *tag, *tstate;

	tstate = tags;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_Text_Contents:
				#warning is it needed?
				break;
		}
	}
}
#endif

DEFNEW
{
	APTR bt_close, bt_prev, bt_next;
	APTR pop_search, ch_case;

	obj = DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		Child, bt_close = ImageObject,
			MUIA_InputMode, MUIV_InputMode_RelVerify,
#if MUIMASTER_VLATEST < 20
			MUIA_Image_Spec, MUII_CheckMark,
#else
			MUIA_Image_Spec, MUII_Close,
#endif
			MUIA_CycleChain, 1,
		End,
		Child, pop_search = NewObject(getpopstringclass(), NULL, TAG_DONE),
		Child, bt_prev = MakeButton("Prev"),
		Child, bt_next = MakeButton("Next"),
		Child, ch_case = MakeCheck("Case Sensitive", FALSE),
		Child, MakeLabel("Case Sensitive"),
		Child, MakeRect(),
		TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->pop_search = pop_search;
		data->bt_prev = bt_prev;
		data->bt_next = bt_next;
		data->ch_case = ch_case;

		set(bt_prev, MUIA_Weight, 0);
		set(bt_next, MUIA_Weight, 0);

		DoMethod(pop_search, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 3, MM_Browser_Find, 0, NULL);
		DoMethod(bt_prev, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Browser_Find, MF_Browser_Find_Previous, NULL);
		DoMethod(bt_next, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Browser_Find, 0, NULL);
		DoMethod(bt_close, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MUIM_Set, MUIA_ShowMe, FALSE);
	}

	return (IPTR)obj;
}

#if 0
DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}
#endif

DEFGET
{
	GETDATA;

	switch (msg->opg_AttrID)
	{
		case MUIA_String_Contents:
			return DoMethodA(data->pop_search, (Msg)msg);
	}

	return DOSUPER;
}

DEFSMETHOD(FindText_DisableButtons)
{
	GETDATA;
	set(data->bt_prev, MUIA_Disabled, msg->prev);
	return set(data->bt_next, MUIA_Disabled, msg->next);
}

DEFSMETHOD(Browser_Find)
{
	GETDATA;

	msg->flags |= getv(data->ch_case, MUIA_Selected) ? MF_Browser_Find_CaseSensitive : 0;
	msg->string = (APTR)getv(data->pop_search, MUIA_String_Contents);
	DoMethod(data->pop_search, MM_PopString_Insert, msg->string);

	return DoMethodA(_win(obj), (Msg)msg);
}

BEGINMTABLE
DECNEW
DECGET
DECSMETHOD(FindText_DisableButtons)
DECSMETHOD(Browser_Find)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, findtextclass)
