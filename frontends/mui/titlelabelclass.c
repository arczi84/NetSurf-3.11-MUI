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
#include <libraries/mui.h>
#include "mui/applicationclass.h"
#include "mui/mui.h"
#include "utils/log.h"

enum {
	POPMENU_NEW,
	POPMENU_RELOAD,
	POPMENU_RELOAD_ALL,
	POPMENU_CLOSE,
	POPMENU_CLOSE_ALL_OTHERS
};

struct Data
{
	Object *closebutton;
	Object *label;
	Object *transferanim;
	Object *cmenu;
};

STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
	struct TagItem *tag, *tstate;

	tstate = tags;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_Text_Contents:
				set(data->label, MUIA_Text_Contents, tag->ti_Data);
				break;

			case MA_TransferAnim_Animate:
				set(data->transferanim, MA_TransferAnim_Animate, tag->ti_Data);
				break;
		}
	}
}

DEFNEW
{
	#define DEFAULT_PAGE_NAME "NetSurf"
	Object *closebutton = NULL;
	Object *label = NULL;

	LOG(("Entering TitleLabelClass NEW method"));

	LOG(("Creating close button"));
	closebutton = ImageObject,
		//MUIA_Image_Spec, MUII_CheckMark,
		MUIA_Image_Spec, MUII_Close,
		End;
	if (!closebutton) {
		LOG(("Failed to create closebutton"));
		return 0;
	}

	LOG(("Creating label"));
	label = TextObject,
		MUIA_Text_Contents, DEFAULT_PAGE_NAME,
		End;
	if (!label) {
		LOG(("Failed to create label"));
		MUI_DisposeObject(closebutton);
		return 0;
	}

	LOG(("Creating TitleLabelClass group"));
	obj = (Object *) DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		Child, closebutton,
		Child, label,
		TAG_MORE, INITTAGS
	);

	if (!obj) {
		LOG(("DoSuperNew failed for TitleLabelClass"));
		MUI_DisposeObject(closebutton);
		MUI_DisposeObject(label);
		return 0;
	}

	GETDATA;
	data->cmenu = NULL;
	data->closebutton = closebutton;
	data->label = label;

	LOG(("Setting close button notification"));
	DoMethod(data->closebutton, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_Title_Close);

	LOG(("TitleLabelClass created successfully, obj=%p", obj));
	return ((ULONG)obj);
}

DEFDISP
{
	GETDATA;
	LOG(("Disposing TitleLabelClass"));
	if (data->cmenu) {
		LOG(("Disposing cmenu=%p", data->cmenu));
		MUI_DisposeObject(data->cmenu);
		data->cmenu = NULL;
	}
	return DOSUPER;
}

DEFMMETHOD(AskMinMax)
{
	DOSUPER;
	LOG(("AskMinMax: Setting MinWidth"));
	msg->MinMaxInfo->MinWidth += 32;
	return 0;
}

DEFMMETHOD(Title_Close)
{
#if 0
	Object *browser = (Object *) getv(obj, MUIA_UserData);
	if (browser) {
		DoMethod(application, MUIM_Application_PushMethod, application, 2, MM_OWBApp_RemoveBrowser, browser);
	}
#endif
	LOG(("Title_Close called"));
	return 0;
}

DEFMMETHOD(ContextMenuBuild)
{
	GETDATA;
	LOG(("ContextMenuBuild called"));
	if (data->cmenu) {
		MUI_DisposeObject(data->cmenu);
		data->cmenu = NULL;
	}
	return (ULONG)data->cmenu;
}

DEFMMETHOD(ContextMenuChoice)
{
	LOG(("ContextMenuChoice called"));
	return 0;
}

DEFSET
{
	GETDATA;
	LOG(("Setting attributes"));
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

BEGINMTABLE
DECNEW
DECDISP
DECSET
DECMMETHOD(Title_Close)
DECMMETHOD(AskMinMax)
DECMMETHOD(ContextMenuBuild)
DECMMETHOD(ContextMenuChoice)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, titlelabelclass)
