/*
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

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/extrasrc.h"
#include "mui/gui.h"
#include "mui/mui.h"

struct Data
{
	APTR lv_downloads;
	APTR lv_finished;
	APTR lv_failed;
};

STATIC CONST CONST_STRPTR dltitles[] =
{
	"In progress",
	"Finished",
	"Failed",
	NULL
};

MUI_HOOK(download_display, STRPTR *array, struct download *dl)
{
	if (dl)
	{
		STATIC TEXT buf1[16];
		STATIC TEXT buf2[16];
		STATIC TEXT buf3[16];

		NewRawDoFmt("%lld", NULL, buf1, dl->size);
		NewRawDoFmt("%lld", NULL, buf2, dl->done);
		NewRawDoFmt("%ld%%", NULL, buf3, (ULONG)((float)dl->done / (float)dl->size * 100.f));

		array[0] = dl->filename;
		array[1] = buf1;
		array[2] = buf2;
		array[3] = buf3;
		array[4] = dl->path;
	}
	else
	{
		array[0] = "Name";
		array[1] = "Size";
		array[2] = "Downloaded";
		array[3] = "Progress";
		array[4] = "Path";
	}

	return 0;
}

MUI_HOOK(download_finished, STRPTR *array, struct download *dl)
{
	if (dl)
	{
		array[0] = dl->filename;
		array[1] = dl->path;
	}
	else
	{
		array[0] = "Name";
		array[1] = "Path";
	}

	return 0;
}

MUI_HOOK(download_failed, STRPTR *array, struct download *dl)
{
	if (dl)
	{
		array[0] = dl->filename;
		array[1] = dl->path;
	}
	else
	{
		array[0] = "Name";
		array[1] = "Path";
	}

	return 0;
}

DEFNEW
{
	APTR lv_downloads, lv_finished, lv_failed;
	APTR rem_finished, rem_finished_all;
	APTR rem_failed, rem_failed_all;

	obj = DoSuperNew(cl, obj,
			MUIA_Window_ID, MAKE_ID('W','D','L','S'),
			MUIA_Window_Title, "Downloads",
			MUIA_Window_NoMenus, TRUE,
			WindowContents, VGroup,
				Child, RegisterObject,
					MUIA_Register_Titles, dltitles,
					Child, VGroup,
						Child, lv_downloads = ListviewObject,
							MUIA_Listview_List, ListObject,
								InputListFrame,
								MUIA_List_DisplayHook, &download_display_hook,
								MUIA_List_Title, TRUE,
								MUIA_List_Format, "BAR,BAR,BAR,BAR,",
							End,
						End,
#if 0
						Child, HGroup,
							Child, canecl = MakeButton("Abort"),
							Child, cancel_all = MakeButton("Abort All"),
						End,
#endif
					End,

					Child, VGroup,
						Child, lv_finished = ListviewObject,
							MUIA_Listview_List, ListObject,
								InputListFrame,
								MUIA_List_DisplayHook, &download_finished_hook,
								MUIA_List_Title, TRUE,
								MUIA_List_Format, "BAR,",
							End,
						End,
						Child, HGroup,
							Child, rem_finished = MakeButton("_Remove"),
							Child, rem_finished_all = MakeButton("Remove _All"),
						End,
					End,

					Child, VGroup,
						Child, lv_failed = ListviewObject,
							MUIA_Listview_List, ListObject,
								InputListFrame,
								MUIA_List_DisplayHook, &download_failed_hook,
								MUIA_List_Title, TRUE,
								MUIA_List_Format, "BAR,",
							End,
						End,
						Child, HGroup,
							Child, rem_failed = MakeButton("_Remove"),
							Child, rem_failed_all = MakeButton("Remove _All"),
						End,
					End,
				End,
			End,
			TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->lv_downloads = lv_downloads;
		data->lv_finished = lv_finished;
		data->lv_failed = lv_failed;

		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, obj, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(rem_finished, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Download_RemoveEntry, lv_finished, 0);
		DoMethod(rem_finished_all, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Download_RemoveEntry, lv_finished, 1);
		DoMethod(rem_failed, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Download_RemoveEntry, lv_failed, 0);
		DoMethod(rem_failed_all, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Download_RemoveEntry, lv_failed, 1);
		//DoMethod(cancel,           MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Download_Cancel, 0);
		//DoMethod(cancel_all,       MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MM_Download_Cancel, 1);
	}

	return (IPTR)obj;
}

#if 0
DEFGET
{
	switch (msg->opg_AttrID)
	{
		case MA_OWB_WindowType:
		{
			*msg->opg_Storage = (ULONG) MV_OWB_Window_Downloads;
		}
		return TRUE;
	}

	return DOSUPER;
}
#endif

DEFMMETHOD(List_InsertSingle)
{
	GETDATA;
	return DoMethodA(data->lv_downloads, msg);
}

DEFMMETHOD(List_Redraw)
{
	GETDATA;
	return DoMethodA(data->lv_downloads, msg);
}

DEFSMETHOD(Download_Done)
{
	GETDATA;
	struct download *dl;
	ULONG i = 0;

	do
	{
		DoMethod(data->lv_downloads, MUIM_List_GetEntry, i, &dl);

		if (dl == msg->entry)
		{
			APTR app = application;

			DoMethod(data->lv_downloads, MUIM_List_Remove, i);
			DoMethod(data->lv_finished, MUIM_List_InsertSingle, msg->entry, MUIV_List_Insert_Bottom);

			set(app, MA_Application_DownloadsInProgress, getv(app, MA_Application_DownloadsInProgress) - 1);

			if (getv(app, MA_Application_DownloadsInProgress) == 0)
			{
				//if (getv(app, MA_OWBApp_DownloadAutoClose))
					set(obj, MUIA_Window_Open, FALSE);
			}

			break;
		}

		i++;
	}
	while (dl);

	return 0;
}

DEFSMETHOD(Download_Cancelled)
{
	GETDATA;
	struct download *dl;
	ULONG i = 0;

	do
	{
		DoMethod(data->lv_downloads, MUIM_List_GetEntry, i, (struct download *) &dl);

		if ((APTR) dl == msg->entry)
		{
			DoMethod(data->lv_downloads, MUIM_List_Remove, i);
			set(application, MA_Application_DownloadsInProgress, getv(application, MA_Application_DownloadsInProgress) - 1);
			//free(dl); // This one must be freed at that point
			break;
		}

		i++;
	}
	while (dl);

	return 0;
}

DEFSMETHOD(Download_Error)
{
	GETDATA;

	struct download *dl;
	ULONG i = 0;

	do
	{
		DoMethod(data->lv_downloads, MUIM_List_GetEntry, i, &dl);

		if (dl == msg->entry)
		{
			DoMethod(data->lv_downloads, MUIM_List_Remove, i);
			DoMethod(data->lv_failed, MUIM_List_InsertSingle, msg->entry, MUIV_List_Insert_Bottom);

			set(application, MA_Application_DownloadsInProgress, getv(application, MA_Application_DownloadsInProgress) - 1);

			break;
		}

		i++;
	}
	while (dl);

	return 0;
}

DEFSMETHOD(Download_RemoveEntry)
{
	if (msg->all)
	{
		DoMethod(msg->listview, MUIM_List_Clear);
	}
	else
	{
		struct download *dl;

		DoMethod(msg->listview, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &dl);

		if (dl)
		{
			DoMethod(msg->listview, MUIM_List_Remove, MUIV_List_Remove_Active);
		}
	}

	return 0;
}

DEFSMETHOD(Download_Cancel)
{
	GETDATA;

	if (msg->all)
	{
		struct download *dl;

		do
		{
			DoMethod(data->lv_downloads, MUIM_List_GetEntry, 0, (struct download *) &dl);

			if (dl)
			{
#warning fixme
#if 0
                WebDownload *webdownload = (WebDownload *)dl->webdownload;
				webdownload->cancel();
				dl->webdownload = NULL; // just in case
#endif

				DoMethod(data->lv_downloads, MUIM_List_Remove, MUIV_List_Remove_First);
				DoMethod(data->lv_failed, MUIM_List_InsertSingle, dl, MUIV_List_Insert_Bottom);
			}
		}
		while (dl);

		set(application, MA_Application_DownloadsInProgress, 0);
	}
	else
	{
		struct download *dl;
		DoMethod(data->lv_downloads, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, (struct download *) &dl);

		if(dl)
		{
#if 0
			WebDownload *webdownload = (WebDownload *)dl->webdownload;
			webdownload->cancel();
			dl->webdownload = NULL; // just in case
#endif

			DoMethod(data->lv_downloads, MUIM_List_Remove, MUIV_List_Remove_Active);
			DoMethod(data->lv_failed, MUIM_List_InsertSingle, dl, MUIV_List_Insert_Bottom);

			set(application, MA_Application_DownloadsInProgress, getv(application, MA_Application_DownloadsInProgress) - 1);
		}
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECSMETHOD(Download_Cancel)
DECSMETHOD(Download_Cancelled)
DECSMETHOD(Download_Done)
DECSMETHOD(Download_Error)
DECSMETHOD(Download_RemoveEntry)
DECMMETHOD(List_InsertSingle)
DECMMETHOD(List_Redraw)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, downloadwindowclass)
