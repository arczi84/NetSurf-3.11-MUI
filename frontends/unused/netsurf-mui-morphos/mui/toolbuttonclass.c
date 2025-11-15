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
#include <graphics/rpattr.h>
#include <proto/alib.h>
#include <proto/cybergraphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui/mui.h"

struct Data
{
	CONST_STRPTR Text;
	APTR  Image;
	UWORD Width, Height;
	UWORD Disabled;
	UWORD IsToggled;
};

static ULONG doset(struct Data *data, APTR obj, struct TagItem *taglist)
{
	struct TagItem *tag, *tstate;
	ULONG update;

	tstate = taglist;
	update = 0;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		ULONG tdata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case MA_ToolButton_Text:
				data->Text = (CONST_STRPTR)tdata;
				break;

			case MA_ToolButton_Image:
				data->Image = (APTR)tdata;
				break;

			case MUIA_Disabled:
				if (data->Disabled != tdata)
				{
					data->Disabled = tdata;
					//SetAttrs(obj, MUIA_FrameDynamic, !tdata, MUIA_FrameVisible, FALSE, TAG_DONE);
					//SetAttrs(obj, MUIA_Frame, tdata ? MUIV_Frame_None : MUIV_Frame_ImageButton, TAG_DONE);
					update = 1;
				}
				break;
		}
	}

	return update;
}

DEFNEW
{
	obj = DoSuperNew(cl, obj,
		MUIA_CycleChain     , 1,
		MUIA_FillArea       , FALSE,
     	MUIA_Font           , MUIV_Font_Tiny,
		MUIA_Frame          , MUIV_Frame_ImageButton,
		//MUIA_FrameDynamic   , TRUE,
		//MUIA_FrameVisible   , FALSE,
		MUIA_InputMode      , MUIV_InputMode_RelVerify,
		TAG_MORE            , (IPTR)msg->ops_AttrList);

	if (obj)
	{
		GETDATA;
		struct TagItem *tag;

		doset(data, obj, msg->ops_AttrList);

		data->Width  = 64;
		data->Height = 64;
	}

	return (IPTR)obj;
}

STATIC CONST UBYTE ToolButtonMode = TBMODE_TEXT;	// will be in prefs later
STATIC CONST UBYTE ScaledImage = FALSE;
STATIC CONST UBYTE ToolbarSmall = FALSE;

DEFSET
{
	GETDATA;

	if (doset(data, obj, msg->ops_AttrList))
		MUI_Redraw(obj, MADF_DRAWUPDATE);

	return DOSUPER;
}

DEFMMETHOD(AskMinMax)
{
	GETDATA;
	ULONG	w, h;

	DOSUPER;

	w = data->Width * 2;
	h = data->Height;

	switch (ToolButtonMode)
	{
		case TBMODE_ICON_WITH_TEXT: h += _font(obj)->tf_YSize; break;
		case TBMODE_ICON          : break;
		case TBMODE_TEXT          : h = _font(obj)->tf_YSize; break;
	}

	msg->MinMaxInfo->MinWidth  += w;
	msg->MinMaxInfo->MinHeight += h;
	msg->MinMaxInfo->MaxWidth  += w;
	msg->MinMaxInfo->MaxHeight += h;
	msg->MinMaxInfo->DefWidth  += w;
	msg->MinMaxInfo->DefHeight += h;

	return 0;
}

DEFMMETHOD(Draw)
{
	DOSUPER;

	if (msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))
	{
		GETDATA;
		struct RastPort *rp;
		CONST_STRPTR text;
		ULONG *chunky, mleft, mtop, mwidth, mheight;
		ULONG w, h;

		mleft   = _mleft(obj);
		mtop    = _mtop(obj);
		mwidth  = _mwidth(obj);
		mheight = _mheight(obj);

		DoMethod(obj, MUIM_DrawBackground, mleft, mtop, mwidth, mheight, 0, 0, 0);

		rp    = _rp(obj);

		if (ScaledImage)
		{
			//chunky = ScaledImage; w = data->ScaledWidth;
			//h = ToolbarMode == TBMODE_TEXT ? 0 : data->ScaledHeight;
		}
		else
		{
			chunky = data->Image;
			w = data->Width;
			h = ToolButtonMode == TBMODE_TEXT ? 0 : data->Height;
		}

		if (ToolButtonMode != TBMODE_TEXT)
		{
			WritePixelArrayAlpha(chunky, 0, 0, w * sizeof(ULONG), rp, mleft + (ToolbarSmall ? 0 : w/2), mtop, w, h, data->Disabled ? 0x4fffffff : 0xffffffff);
		}

		if (ToolButtonMode != TBMODE_ICON)
		{
			ULONG pixlen, doppp, was_space;
			LONG textlen;
			LONG textdim;
			STRPTR preparse = data->Disabled ? "\033g" : NULL;
			ULONG pplen = 0;

			text    = (CONST_STRPTR)data->Text;
			textlen = strlen(text);
			textdim = DoMethod(obj, MUIM_TextDim, text, textlen, preparse, 0);
			pixlen  = textdim & 0xffff;

			was_space = FALSE;

			if ((doppp = (pixlen > mwidth)))
			{
				textdim = DoMethod(obj, MUIM_TextDim, "...", ToolbarSmall ? 1 : 3, preparse, 0);
				pplen = textdim & 0xffff;

				do
				{
					textlen--;
					was_space = FALSE;

					if (textlen > 0 && text[textlen-1] == ' ')
					{
						was_space = TRUE;
						textlen--;
					}

					if (textlen <= 0)
						break;

					textdim = DoMethod(obj, MUIM_TextDim, text, textlen, preparse, 0);
					pixlen = (textdim & 0xffff) + pplen;
				}
				while (pixlen > mwidth);
			}

			if (textlen > 0)
			{
				DoMethod(obj, MUIM_Text, mleft + (mwidth - pixlen) / 2, mtop + h, pixlen, mheight - h + 1, text, textlen, preparse, 0);
				
				if (doppp && !(was_space && ToolbarSmall))
					DoMethod(obj, MUIM_Text, mleft + (mwidth - pixlen) / 2 + pixlen - pplen, mtop + h, pixlen, mheight - h + 1, "...", ToolbarSmall ? 1 : 3, preparse, 0);
			}
		}
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECMMETHOD(AskMinMax)
DECMMETHOD(Draw)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Area, toolbuttonclass)

APTR create_toolbutton(CONST_STRPTR text, CONST_APTR image)
{
	return NewObject(gettoolbuttonclass(), NULL, MA_ToolButton_Text, text, MA_ToolButton_Image, image, MUIA_CycleChain, 1, TAG_DONE);
}
