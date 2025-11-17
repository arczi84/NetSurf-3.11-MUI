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

#include "mui/mui.h"

struct Data
{
	APTR str_search;
	APTR cy_search;
};

STATIC CONST CONST_STRPTR entries[] =
{
	"Google",
	"Aminet",
	NULL
};

STATIC CONST CONST_STRPTR template[] =
{
	"https://www.google.com/search?q=%s",
	"http://www.aminet.net/search.php?query=%s",
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
	APTR str_search, bt_search, cy_search;

	obj = DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		MUIA_Weight, 25,
		Child, MakeVBar(),
		Child, cy_search = MakeCycle(NULL, entries, CY_SEARCH_ID),
		Child, str_search = StringObject,
			StringFrame,
			//MUIA_Textinput_RemainActive, TRUE,
			//MUIA_Textinput_Format, MUIV_Textinput_Format_Center,
			MUIA_CycleChain, 1,
		End,
		//Child, bt_search = MakeButton("_Search"),
		TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->str_search = str_search;
		//data->bt_search = bt_search;
		data->cy_search = cy_search;

		DoMethod(obj, MUIM_MultiSet, MUIA_Weight, 0, cy_search, /* bt_search,*/ NULL);

		DoMethod(str_search, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 2,  MM_Browser_Go, NULL);
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
			return get(data->str_search, MUIA_String_Contents, msg->opg_Storage);
	}

	return DOSUPER;
}

DEFSMETHOD(Browser_Go)
{
	GETDATA;
	STRPTR str, fmt, p;
	ULONG idx, len;

	str = (APTR)getv(data->str_search, MUIA_String_Contents);
	idx = getv(data->cy_search, MUIA_Cycle_Active);
	fmt = (STRPTR)template[idx];
	len = strlen(fmt) + strlen(str) + 1;

	p = AllocMem(len, MEMF_ANY);

	if (p)
	{
		msg->url = p;
		NewRawDoFmt(fmt, NULL, p, str);
		DoMethodA(_win(obj), msg);
		FreeMem(p, len);
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECGET
DECSMETHOD(Browser_Go)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, searchbargroupclass)
