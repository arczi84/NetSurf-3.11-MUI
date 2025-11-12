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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "utils/errors.h"
#include "content/urldb.h"
#include "netsurf/browser.h"
#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/utils.h"

struct Data
{
	APTR browser;
	APTR username;
	APTR password;
	APTR realm;
	STRPTR url;
};

DEFNEW
{
	APTR tx_realm, username, password, login, cancel;
	STRPTR host, realm, url;

	host = (STRPTR)GetTagData(MA_LoginWindow_Host, NULL, msg->ops_AttrList);
	realm = (STRPTR)GetTagData(MA_LoginWindow_Realm, NULL, msg->ops_AttrList);
	url = DupStr((STRPTR)GetTagData(MA_LoginWindow_URL, NULL, msg->ops_AttrList));

	obj = DoSuperNew(cl, obj,
			//MUIA_Window_Width , MUIV_Window_Width_Visible(25),
			//MUIA_Window_Height , MUIV_Window_Height_Visible(25),
			MUIA_Window_TopEdge , MUIV_Window_TopEdge_Centered,
			MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
			MUIA_Window_Title, url,
			MUIA_Window_NoMenus, TRUE,
			WindowContents, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel("Host"),
					Child, TextObject, MUIA_Text_Contents, host, End,
					Child, MakeLabel("Realm"),
					Child, tx_realm = TextObject, MUIA_Text_Contents, realm, End,
				End,
				Child, ColGroup(2),
					Child, MakeLabel("Username"),
					Child, username = MakeString(NULL, FALSE),
					Child, MakeLabel("Password"),
					Child, password = MakeString(NULL, TRUE),
				End,
				Child, MakeHBar(),
				Child, HGroup,
					Child, login = MakeButton("Login"),
					Child, RectangleObject, MUIA_Weight, 10000, End,
					Child, cancel = MakeButton("Cancel"),
				End,
			End,
			TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->realm = tx_realm;
		data->url = url;

		data->browser = (APTR)GetTagData(MA_LoginWindow_Browser, NULL, msg->ops_AttrList);

		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 2, MM_Application_KillWindow, obj);
		DoMethod(cancel, MUIM_Notify, MUIA_Pressed, FALSE,  MUIV_Notify_Application, 2, MM_Application_KillWindow, obj);
		DoMethod(login, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MM_LoginWindow_Login);
	}

	return (IPTR)obj;
}

DEFDISP
{
	GETDATA;

	if (data->url)
		free(data->url);

	return DOSUPER;
}

DEFTMETHOD(LoginWindow_Login)
{
	GETDATA;

#if MULTITHREADED
	struct worknode *node;
#else
	STRPTR userpass;
#endif
	STRPTR username, password;
	ULONG length;

	DoMethod(_app(obj), MUIM_Application_PushMethod, _app(obj), 2, MM_Application_KillWindow, obj);

	username = (STRPTR)getv(data->username, MUIA_String_Contents);
	password = (STRPTR)getv(data->password, MUIA_String_Contents);

	length = strlen(username) + strlen(password) + 2;

#if MULTITHREADED
	node = AllocMem(sizeof(*node) + length, MEMF_ANY);
	userpass = malloc(length);

	if (node)
	{
		NewRawDoFmt("%s:%s", NULL, node->data, username, password);

		node->param1 = (IPTR)strdup(data->url);
		node->param2 = (IPTR)strdup((char *)getv(data->realm, MUIA_Text_Contents));
		node->size = sizeof(*node) + length;
		netsurf_add_job(data->browser, JOB_LOGIN, node);
	}
#else
	userpass = malloc(length);

	if (userpass)
	{
		NewRawDoFmt("%s:%s", NULL, userpass, username, password);

		urldb_set_auth_details(data->url, (char *)getv(data->realm, MUIA_Text_Contents), userpass);
		FreeMem(userpass, length);
		//browser_window_go(data->browser, data->url, 0, true); //disabled
	}
#endif

	return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECSMETHOD(LoginWindow_Login)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, loginwindowclass)
