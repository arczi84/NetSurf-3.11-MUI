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

#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/mui.h"
#include "mui/utils.h"

struct Data
{
	APTR owner_password;
	APTR user_password;
	APTR reference;
};

DEFNEW
{
	APTR owner_password, user_password, set_password, no_password;

	obj = DoSuperNew(cl, obj,
			//MUIA_Window_Width , MUIV_Window_Width_Visible(25),
			//MUIA_Window_Height , MUIV_Window_Height_Visible(25),
			MUIA_Window_TopEdge , MUIV_Window_TopEdge_Centered,
			MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
			MUIA_Window_Title, "Set PDF password",
			MUIA_Window_NoMenus, TRUE,
			WindowContents, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel("_Owner password"),
					Child, owner_password = MakeString("_Owner password", FALSE),
					Child, MakeLabel("_User password"),
					Child, user_password = MakeString("_User password", FALSE),
				End,
				Child, MakeHBar(),
				Child, HGroup,
					Child, set_password = MakeButton("Set Password"),
					Child, RectangleObject, MUIA_Weight, 10000, End,
					Child, no_password = MakeButton("Do not set password"),
				End,
			End,
			TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->owner_password = owner_password;
		data->user_password = user_password;

		data->reference = (APTR)GetTagData(MA_PDF_Reference, NULL, msg->ops_AttrList);

		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, obj, 2, MM_PDF_SetPassword, 0);
		DoMethod(set_password, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_PDF_SetPassword, 1);
		DoMethod(no_password, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_PDF_SetPassword, 0);
	}

	return (IPTR)obj;
}

DEFSMETHOD(PDF_SetPassword)
{
	GETDATA;
	STRPTR owner, user;

	owner = NULL;
	user = NULL;

	if (msg->set_pass)
	{

		owner = (STRPTR)getv(data->owner_password, MUIA_String_Contents);
		user = (STRPTR)getv(data->user_password, MUIA_String_Contents);

		if (*owner == '\0')
		{
			MUI_RequestA(_app(obj), obj, 0, "Set PDF password", "*_OK", "Owner password must be at least 1 character long.", NULL);
			return 0;
		}
		else if (!strcmp(owner, user))
		{
			MUI_RequestA(_app(obj), obj, 0, "Set PDF password", "*_OK", "User and owner password must be different.", NULL);
			return 0;
		}

		if ((owner = strdup(owner)) == NULL)
			return 0;

		if ((user = strdup(user)) == NULL)
		{
			free(user);
			return 0;
		}
	}

	DoMethod(_app(obj), MUIM_Application_PushMethod, _app(obj), 5, MM_Application_SetPasswordPDF, owner, user, data->reference, obj);
	return set(obj, MUIA_Window_Open, FALSE);
}

BEGINMTABLE
DECNEW
DECSMETHOD(PDF_SetPassword)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, pdfclass)
