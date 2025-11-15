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

#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/utility.h>

#include "mui/extrasrc.h"
#include "mui/loadpng.h"
#include "mui/mui.h"
#include "mui/netsurf.h"

struct Data
{
	UWORD imagenum;
	UBYTE animate;
	UBYTE is_shown;
	UBYTE added;
	struct MUI_InputHandlerNode ihnode;
};

#define MAX_THROBBER_IMAGES 16
STATIC struct nsimage *throbber[MAX_THROBBER_IMAGES];
STATIC UWORD throbber_width;
STATIC UWORD throbber_height;
STATIC UWORD throbber_images;

void transferanimclass_load(void)
{
	TEXT buffer[64];
	ULONG i, width, height, images;

	width = 1;
	height = 1;
	images = 0;

	for (i = 0; i < MAX_THROBBER_IMAGES; i++)
	{
		struct nsimage *image;

		NewRawDoFmt("PROGDIR:Resources/Throbber/throbber%d.png", NULL, buffer, i);
		image = png_load(buffer);

		if (!image)
			break;

		throbber[i] = image;
		width = MAX(width, image->width);
		height = MAX(height, image->height);
		images++;
	}

	throbber_width = width;
	throbber_height = height;
	throbber_images = images;
}

void transferanimclass_unload(void)
{
	ULONG i;

	for (i = 0; i < MAX_THROBBER_IMAGES; i++)
	{
		FreeVec(throbber[i]);
	}
}

STATIC VOID doset(struct Data *data, APTR obj, struct TagItem *taglist)
{
	struct TagItem *tag, *tstate;

	tstate = taglist;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		ULONG tag_data = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case MA_TransferAnim_Animate:
				if (tag_data && data->animate == 0)
				{
					data->animate = 1;

					if (data->is_shown && !data->added)
					{
						data->added = TRUE;
						DoMethod(application, MUIM_Application_AddInputHandler, (ULONG)&data->ihnode);
					}
				}
				else if (!tag_data && data->animate)
				{
					data->animate = 0;

					if (data->is_shown && data->added)
					{
						DoMethod(application, MUIM_Application_RemInputHandler, (ULONG)&data->ihnode);
						data->imagenum = 0;
						data->added = FALSE;
						MUI_Redraw(obj, MADF_DRAWUPDATE);
					}
				}
				break;
		}
	}
}

DEFNEW
{
	obj = DoSuperNew(cl, obj,
		MUIA_FillArea, FALSE,
		MUIA_Frame   , MUIV_Frame_ImageButton,
		MUIA_Weight  , 0,
		TAG_MORE     , (IPTR)msg->ops_AttrList);

	if (obj)
	{
		GETDATA;
		struct TagItem *tag;

		data->ihnode.ihn_Object = obj;
		data->ihnode.ihn_Flags = MUIIHNF_TIMER;
		data->ihnode.ihn_Millis = 50;
		data->ihnode.ihn_Method = MM_TransferAnim_Run;

		doset(data, obj, msg->ops_AttrList);
	}

	return (IPTR)obj;
}

DEFMMETHOD(AskMinMax)
{
	GETDATA;

	DOSUPER;

	msg->MinMaxInfo->MinWidth  += throbber_width;
	msg->MinMaxInfo->MinHeight += throbber_height;
	msg->MinMaxInfo->MaxWidth  += MUI_MAXMAX;
	msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
	msg->MinMaxInfo->DefWidth  += throbber_width;
	msg->MinMaxInfo->DefHeight += throbber_height;

	return 0;
}

DEFMMETHOD(Show)
{
	BOOL rc = DOSUPER;

	if (rc)
	{
		GETDATA;

		data->is_shown = 1;

		if (data->animate && !data->added)
		{
			data->added = TRUE;
			DoMethod(_app(obj), MUIM_Application_AddInputHandler, (IPTR)&data->ihnode);
		}
	}

	return rc;
}

DEFMMETHOD(Hide)
{
	GETDATA;

	data->is_shown = 0;

	if (data->animate && data->added)
	{
		data->added = FALSE;
		DoMethod(_app(obj), MUIM_Application_RemInputHandler, (IPTR)&data->ihnode);
	}

	return DOSUPER;
}

DEFSET
{
	GETDATA;
	doset(data, obj, msg->ops_AttrList);
	return DOSUPER;
}

DEFTMETHOD(TransferAnim_Run)
{
	if (throbber_images > 1)
	{
		GETDATA;

		data->imagenum++;

		if (data->imagenum >= throbber_images)
			data->imagenum = 0;

		MUI_Redraw(obj, MADF_DRAWUPDATE);
	}

	return 0;
}

DEFMMETHOD(Draw)
{
	DOSUPER;

	if (msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))
	{
		GETDATA;
		ULONG mleft, mtop, mwidth, mheight, w;
		struct nsimage *image;

		mleft   = _mleft(obj);
		mtop    = _mtop(obj);
		mwidth  = _mwidth(obj);
		mheight = _mheight(obj);

		DoMethod(obj, MUIM_DrawBackground, mleft, mtop, mwidth, mheight, 0, 0, 0);

		image = throbber[data->imagenum];

		WritePixelArrayAlpha(image->data, 0, 0, image->modulo * sizeof(ULONG), _rp(obj), mleft + (mwidth - image->width) / 2, mtop + (mheight - image->height) / 2, image->width, image->height, 0xffffffff);	//data->animate ? 0xffffffff : 0x4fffffff);
		//WritePixelArray((APTR)&Throbber, data->imagenum * w, 0, THROBBER_WIDTH * 3, _rp(obj), mleft + (mwidth - w) / 2, mtop + (mheight - THROBBER_HEIGHT) / 2, w, THROBBER_HEIGHT, RECTFMT_RGB);
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECSET
DECMMETHOD(AskMinMax)
DECMMETHOD(Draw)
DECMMETHOD(Hide)
DECMMETHOD(Show)
DECSMETHOD(TransferAnim_Run)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Area, transferanimclass)
