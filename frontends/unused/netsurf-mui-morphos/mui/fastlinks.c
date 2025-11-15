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
};

STATIC VOID CreateFastLinks(void)
{
	APTR group;

	group = ColGroup(8)
			Child, HGroup
		End;

	if (group)
	{
		for (i = 0; i < 8; i++)
		{
			Child, MakeButton()
		}
	}

}

DEFNEW
{
	APTR back, forward, reload, stop;

	obj = DoSuperNew(cl, obj,
			MUIA_Group_Horiz, TRUE,
			Child, HGroup,
				Child, RectangleObject, End,
				Child, back = create_toolbutton("Back", NULL),
				Child, forward = create_toolbutton("Forward", NULL),
				//Child, MakeVBar(),
				Child, reload = create_toolbutton("Reload", NULL),
				Child, stop = create_toolbutton("Stop", NULL),
				Child, RectangleObject, End,
			End,
			Child, NewObject(gettransferanimclass(), NULL, End,
			TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->back = back;
		data->forward = forward;
		data->reload = reload;
		data->stop = stop;

		DoMethod(obj, MUIM_MultiSet, MUIA_Disabled, TRUE, back, forward, reload, stop, NULL);

		DoMethod(back, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 2, MM_Window_Navigate, NAV_BACK);
		DoMethod(forward, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 2, MM_Window_Navigate, NAV_FORWARD);
		DoMethod(reload, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 2, MM_Window_Navigate, NAV_RELOAD);
		DoMethod(stop, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 2, MM_Window_Navigate, NAV_STOP);
	}

	return (IPTR)obj;
}

BEGINMTABLE
DECNEW
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, fastlinksclass)
