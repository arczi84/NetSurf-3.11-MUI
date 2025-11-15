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

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"

struct Data
{
	struct MUI_EventHandlerNode ehnode;
};

struct MUIP_Title_Close { ULONG MethodID; APTR dummy1; };

DEFMMETHOD(Title_Close)
{
#if 0
	DOSUPER;

	if (getv(obj, MUIA_Group_ChildCount) == 0)
	{
		set(_win(obj), MUIA_Window_CloseRequest, TRUE);
	}

	return 0;
#else
	if (msg->dummy1)
	{
		struct gui_window *gw;

		gw = (APTR)getv(msg->dummy1, MUIA_UserData);

		netsurf_add_simple_job(gw->bw, JOB_DESTROY, 0, 0, 0);
	}

	return 0;
#endif
}


DEFMMETHOD(Hide)
{
	GETDATA;
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
	return DOSUPER;
}


DEFMMETHOD(Show)
{
	ULONG rc = DOSUPER;

	if (rc)
	{
		GETDATA;

		data->ehnode.ehn_Object = obj;
		data->ehnode.ehn_Class  = cl;
		data->ehnode.ehn_Priority = -1;
		data->ehnode.ehn_Flags  = MUI_EHF_GUIMODE;
		data->ehnode.ehn_Events = IDCMP_RAWKEY;

		DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
	}

	return rc;
}


DEFMMETHOD(HandleEvent)
{
	struct IntuiMessage *imsg = msg->imsg;
	ULONG rc = 0;

	if (imsg)
	{
		switch (imsg->Class)
		{
			case IDCMP_RAWKEY:
				switch (imsg->Code)
				{
					default:
						{
							struct InputEvent ie;
							TEXT buffer[4];

							ie.ie_Class        = IECLASS_RAWKEY;
							ie.ie_SubClass     = 0;
							ie.ie_Code         = imsg->Code;
							ie.ie_Qualifier    = 0;
							ie.ie_EventAddress = NULL;

							if (MapRawKey(&ie, buffer, 4, NULL) == 1)
							{
								LONG page = buffer[0] - '0';

								if (page >= 0 && page <= 9)
								{
									page--;

									if (page == -1)
										page += 10;

									if (imsg->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
										page += 10;

									if (imsg->Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT))
										page += 10;

									if (imsg->Qualifier & IEQUALIFIER_CONTROL)
										page += 10;

									if (imsg->Qualifier & (IEQUALIFIER_RCOMMAND | IEQUALIFIER_LCOMMAND))
									{
										set((APTR)getv(obj, MUIA_Parent), MUIA_Group_ActivePage, page);
										rc = MUI_EventHandlerRC_Eat;
									}
								}
							}
						}
						break;
				}
				break;
		}
	}

	return rc;
}


BEGINMTABLE
DECMMETHOD(HandleEvent)
DECMMETHOD(Hide)
DECMMETHOD(Show)
DECMMETHOD(Title_Close)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Title, titleclass)
