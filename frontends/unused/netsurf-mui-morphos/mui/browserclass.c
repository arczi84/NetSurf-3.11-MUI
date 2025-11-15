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

#include <devices/rawkeycodes.h>
#include <exec/execbase.h>
#include <graphics/rpattr.h>
#include <intuition/pointerclass.h>
#include <proto/codesets.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/ttengine.h>
#include <proto/utility.h>

#if defined(__MORPHOS__)
#include <intuition/intuimessageclass.h>
#endif

#include "desktop/browser.h"
#include "desktop/history_core.h"
#include "desktop/textinput.h"
#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/plotters.h"
#include "mui/print.h"
#include "mui/search.h"
#include "mui/utils.h"

#define USE_VMEM 0

#if USE_VMEM
#define BITMAP_FLAGS BMF_DISPLAYABLE | BMF_MINPLANES
#else
#define BITMAP_FLAGS BMF_MINPLANES
#endif

#if !defined(__MORPHOS__)
LONG global_pen_a = -1;
LONG global_pen_b = -1;
#endif

struct Data
{
	struct MUI_EventHandlerNode  ehnode;
	struct gui_window           *context;
	struct browser_window       *browser;

	WORD mouse_state, key_state;
	WORD mwidth, mheight;

	ULONG content_width, content_height;

	ULONG pointertype;
	ULONG mouse_inside;

	BYTE back_available;
	BYTE forward_available;
	BYTE stop_available;
	BYTE reload_available;
	WORD redraw_pending, redraw;
	LONG vleft_old, vtop_old;
	LONG changed;

	// states
	LONG loading;
	STRPTR status_text;
	STRPTR url;
	STRPTR title;

	// rendering
	struct RastPort *RastPort;
	struct BitMap   *BitMap;
	struct Layer    *Layer;
	APTR LayerInfo;
	LONG bm_width, bm_height;

	APTR title_obj;
};

#define RENDER_MULTIPLIER 1

MUI_HOOK(layoutfunc, APTR grp, struct MUI_LayoutMsg *lm)
{
	ULONG rc = MUILM_UNKNOWN;

	switch (lm->lm_Type) {
	case MUILM_MINMAX:
		lm->lm_MinMax.MinWidth = 1;
		lm->lm_MinMax.MinHeight = 1;
		lm->lm_MinMax.DefWidth = 800;
		lm->lm_MinMax.DefHeight = 600;
		lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
		lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
		rc = 0;
		break;

	case MUILM_LAYOUT:
	{
		struct Data *data = INST_DATA(OCLASS(grp), grp);

		lm->lm_Layout.Width  = data->content_width;
		lm->lm_Layout.Height = data->content_height;
		rc = TRUE;
	}
		break;
	}

	return rc;
}

STATIC VOID delete_offscreen_bitmap(struct Data *data)
{
	if (data->Layer) {
		TT_DoneRastPort(data->RastPort);
		DeleteLayer(NULL, data->Layer);
	}

	FreeBitMap(data->BitMap);

	if (data->LayerInfo)
		DisposeLayerInfo(data->LayerInfo);

	data->changed = 0;
	data->redraw_pending = 1;
	data->context->redraw = 1;

	data->RastPort = NULL;
	data->BitMap = NULL;
	data->Layer  = NULL;
	data->LayerInfo = NULL;

	data->context->RastPort = NULL;
	data->context->Layer = NULL;
	data->context->LayerInfo = NULL;
}

//STATIC APTR create_offscreen_bitmap(APTR obj, struct Data *data, ULONG width, ULONG height)

DEFSMETHOD(Browser_GetBitMap)
{
	GETDATA;
	struct RastPort *rp;

	if (data->changed)
		delete_offscreen_bitmap(data);

	rp = data->RastPort;

	if (rp == NULL && muiRenderInfo(obj)) {
		//data->context->redraw = 1;

		if (!data->LayerInfo)
			data->LayerInfo = NewLayerInfo();

		if (data->LayerInfo)
		{
			struct BitMap *bm;
			ULONG mwidth, mheight;

			bm = _screen(obj)->RastPort.BitMap;
			mwidth  = msg->width;	//data->mwidth;
			mheight = msg->height;	//data->mheight;

			data->bm_width = mwidth;
			data->bm_height = mheight;

			if ((data->BitMap = AllocBitMap(mwidth, mheight * RENDER_MULTIPLIER,
					GetBitMapAttr(bm, BMA_DEPTH), BITMAP_FLAGS,
					bm))) {
				if ((data->Layer = CreateUpfrontHookLayer(data->LayerInfo,
						data->BitMap, 0, 0, mwidth - 1,
						mheight * RENDER_MULTIPLIER - 1,
						LAYERSIMPLE, LAYERS_NOBACKFILL, NULL))) {
					data->RastPort = rp = data->Layer->rp;
					//data->render_top = -1;

					data->context->RastPort = rp;
					data->context->BitMap = data->BitMap;
					data->context->Layer = data->Layer;
				} else {
					FreeBitMap(data->BitMap);
					data->BitMap = NULL;
				}
			}
		}
	}

	return (IPTR)rp;
}

STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
	struct TagItem *tag, *tstate;
	STRPTR p;

	tstate = tags;

	while ((tag = NextTagItem(&tstate)) != NULL) {
		IPTR tdata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case MA_Browser_Loading:
				data->loading = tdata;
				break;

			case MA_Browser_Pointer:
				if (data->pointertype != tdata)
				{
					data->pointertype = tdata;

					#if defined(__MORPHOS2__)
					if (muiRenderInfo(obj))
						set(_window(obj), WA_PointerType, tdata);
					#endif
				}
				break;

			case MA_Browser_StatusText:
				data->status_text = (APTR)tdata;
				break;

			case MA_Browser_Title:
				if (data->title)
					free(data->title);

				p = (APTR)tdata;

				if (p)
				{
					p = DupStr(p);
				}

				data->title = p;
				break;

			case MA_Browser_URL:
				if (data->url)
					free(data->url);

				p = (APTR)tdata;

				if (p)
				{
					p = DupStr(p);
				}

				data->url = p;
				break;
		}
	}
}

DEFNEW
{
	obj = DoSuperNew(cl, obj,
			MUIA_Group_LayoutHook, (IPTR)&layoutfunc_hook,
			MUIA_FillArea, FALSE,
			//InputListFrame,
			MUIA_CustomBackfill, TRUE,
			InnerSpacing(0,0),
			TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->ehnode.ehn_Object   = obj;
		data->ehnode.ehn_Class    = cl;
		data->ehnode.ehn_Priority = 1;
		data->ehnode.ehn_Flags    = MUI_EHF_GUIMODE;

		data->mwidth = 1920;
		data->mheight = 1200;
		data->pointertype = POINTERTYPE_NORMAL;

		data->context = (APTR)FindTagItem(MA_Browser_Context, msg->ops_AttrList)->ti_Data;
		data->browser = data->context->bw;
		data->title_obj = (APTR)FindTagItem(MA_Browser_TitleObj, msg->ops_AttrList)->ti_Data;
	}

	return (IPTR)obj;
}

DEFDISP
{
	GETDATA;

#if 0
	if (data->browser)
	{
		data->context->obj = NULL;
		data->context->win = NULL;
		#if MULTITHREADED
		netsurf_add_simple_job(data->browser, JOB_DESTROY, 0, 0, 0);
		#else
		browser_window_destroy(data->browser);
		#endif
	}
#endif

	return DOSUPER;
}

DEFMMETHOD(Setup)
{
	GETDATA;
	IPTR rc;

	rc = DOSUPER;

	#if !defined(__MORPHOS__)
	if (rc)
	{
		if (global_pen_a < 0)
			global_pen_a = ObtainPen(_screen(obj)->ViewPort.ColorMap, -1, 0, 0, 0, PEN_EXCLUSIVE);

		if (global_pen_b < 0)
			global_pen_b = ObtainPen(_screen(obj)->ViewPort.ColorMap, -1, 0, 0, 0, PEN_EXCLUSIVE);
	}
	#endif

	return rc;
}

DEFMMETHOD(Cleanup)
{
	GETDATA;

	#if !defined(__MORPHOS__)
	ReleasePen(_screen(obj)->ViewPort.ColorMap, global_pen_a);
	ReleasePen(_screen(obj)->ViewPort.ColorMap, global_pen_b);

	global_pen_a = -1;
	global_pen_b = -1;
	#endif

	data->changed = 1;

	return DOSUPER;
}

DEFMMETHOD(Show)
{
	ULONG rc;
	GETDATA;

	if ((rc = DOSUPER))
	{
		ULONG mwidth, mheight;

		mwidth = _mwidth(obj);
		mheight = _mheight(obj);

		if (data->mwidth != mwidth || data->mheight != mheight)
		{
			data->mwidth = mwidth;
			data->mheight = mheight;
			data->changed = 1;
			#if MULTITHREADED
			netsurf_add_simple_job(data->browser, JOB_REFORMAT, mwidth, mheight, 0);
			#else
			browser_window_reformat(data->browser, mwidth, mheight);
			#endif
		}

		data->ehnode.ehn_Events = IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
		DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
	}

	return rc;
}

DEFMMETHOD(Hide)
{
	GETDATA;

	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode );
	return DOSUPER;
}

DEFTMETHOD(Browser_Print)
{
	GETDATA;

	if (data->RastPort) {
		print_doc(data->RastPort, data->bm_width, data->bm_height);
	} else {
		/* XXX: take care of this on separate thread */
		#warning fixme
	}

	return 0;
}

STATIC VOID update_buttons(APTR obj, struct Data *data)
{
	data->back_available = history_back_available(data->browser->history);
	data->forward_available = history_forward_available(data->browser->history);

	SetAttrs(obj,
		MA_Browser_BackAvailable, data->back_available,
		MA_Browser_ForwardAvailable, data->forward_available,
		TAG_DONE);
}

DEFSMETHOD(Browser_SetContentSize)
{
	GETDATA;

	data->content_width = msg->width;
	data->content_height = msg->height;

	data->vleft_old = 0;
	data->vtop_old = 0;

	if (data->mwidth != data->content_width || data->mheight != data->content_height)
	{
		APTR parent = (APTR)getv(obj, MUIA_Parent);

		data->changed = 1;

		if (parent)
		{
			//delete_offscreen_bitmap(data);
			DoMethod(parent, MUIM_Group_InitChange);
			DoMethod(parent, MUIM_Group_ExitChange);
		}
	}

	update_buttons(obj, data);

	return SetAttrs(obj, MA_Browser_ReloadAvailable, TRUE, MA_Browser_StopAvailable, TRUE, TAG_DONE);
}

DEFSMETHOD(Browser_SetContentType)
{
	GETDATA;

	if (msg->type <= CONTENT_CSS)
	{
		#warning enable stuff for textual content
	}
	else
	{
		#warning enable stuff for gfx content
	}

	return 0;
}

DEFTMETHOD(Browser_Back)
{
	GETDATA;
	#if MULTITHREADED
	netsurf_add_simple_job(data->browser, JOB_HISTORY_BACK, (IPTR)data->browser->history, 0, 0);
	#else
	history_back(data->browser, data->browser->history);
	update_buttons(obj, data);
	#endif
	return 0;
}

DEFTMETHOD(Browser_Forward)
{
	GETDATA;
	#if MULTITHREADED
	netsurf_add_simple_job(data->browser, JOB_HISTORY_FORWARD, (IPTR)data->browser->history, 0, 0);
	#else
	history_forward(data->browser, data->browser->history);
	update_buttons(obj, data);
	#endif
	return 0;
}

DEFTMETHOD(Browser_Reload)
{
	GETDATA;
	#if MULTITHREADED
	netsurf_add_simple_job(data->browser, JOB_RELOAD, data->key_state & BROWSER_MOUSE_MOD_1 ? true : false, 0, 0);
	#else
	browser_window_reload(data->browser, data->key_state & BROWSER_MOUSE_MOD_1 ? true : false);
	#endif
	return 0;
}

DEFTMETHOD(Browser_Stop)
{
	GETDATA;
	#if MULTITHREADED
	netsurf_add_simple_job(data->browser, JOB_STOP, 0, 0, 0);
	#else
	browser_window_stop(data->browser);
	#endif
	return 0;
}

DEFSMETHOD(Browser_Find)
{
#ifdef MULTITHREADED
	struct worknode *node;
	ULONG length;

	length = strlen(msg->string) + 1;
	node = AllocMem(sizeof(*node) + length, MEMF_ANY);

	if (node)
	{
		GETDATA;

		node->param1 = msg->flags & MF_Browser_Find_Previous ? false : true;
		node->param2 = msg->flags & MF_Browser_Find_CaseSensitive ? false : true;
		node->param3 = (IPTR)data->context;
		node->size = sizeof(*node) + length;
		bcopy(msg->string, node->data, length);
		netsurf_add_job(data->browser, JOB_FIND, node);
	}
#else
	GETDATA;
	start_search(msg->flags & MF_Browser_Find_Previous ? false : true, msg->flags & MF_Browser_Find_CaseSensitive ? true : false, msg->string);
#endif

	return 0;
}

DEFSMETHOD(Browser_Go)
{
#ifdef MULTITHREADED
	struct worknode *node;
	ULONG length;

	length = strlen(msg->url) + 1;

	node = AllocMem(sizeof(*node) + length, MEMF_ANY);

	if (node)
	{
		GETDATA;

		node->size = sizeof(*node) + length;
		bcopy(msg->url, node->data, length);
		netsurf_add_job(data->browser, JOB_GO, node);
	}
#else
	GETDATA;
	browser_window_go(data->browser, msg->url, NULL, true);	
	Signal(SysBase->ThisTask, SIGBREAKF_CTRL_E);
#endif

	return 0;
}

DEFGET
{
	GETDATA;

	switch (msg->opg_AttrID)
	{
		case MA_Browser_BackAvailable:
			*msg->opg_Storage = data->back_available;
			return TRUE;

		case MA_Browser_Box:
		{
			struct IBox *box = (APTR)msg->opg_Storage;
			box->Left = getv(obj, MUIA_Virtgroup_Left);
			box->Top = getv(obj, MUIA_Virtgroup_Top);
			box->Width = data->mwidth;
			box->Height = data->mheight;
			return FALSE;
		}

		case MA_Browser_Browser:
			*msg->opg_Storage = (IPTR)data->browser;
			return FALSE;

		case MA_Browser_ForwardAvailable:
			*msg->opg_Storage = data->forward_available;
			return TRUE;

		case MA_Browser_Loading:
			*msg->opg_Storage = data->loading;
			return TRUE;

		case MA_Browser_ReloadAvailable:
			*msg->opg_Storage = (IPTR)data->reload_available;
			return TRUE;

		case MA_Browser_StatusText:
			*msg->opg_Storage = (IPTR)data->status_text;
			return TRUE;

		case MA_Browser_StopAvailable:
			*msg->opg_Storage = (IPTR)data->stop_available;
			return TRUE;

		case MA_Browser_Title:
			*msg->opg_Storage = (IPTR)data->title;
			return TRUE;

		case MA_Browser_TitleObj:
			*msg->opg_Storage = (IPTR)data->title_obj;
			return FALSE;

		case MA_Browser_URL:
			*msg->opg_Storage = (IPTR)data->url;
			return TRUE;
	}

	return DOSUPER;
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

DEFMMETHOD(HandleEvent)
{
	struct IntuiMessage *imsg;
	ULONG rc;

	imsg = msg->imsg;
	rc = 0;

	if (imsg)
	{
		GETDATA;
		ULONG mouse_inside;
		LONG MouseX, MouseY;

		MouseX = imsg->MouseX - _mleft(obj);	// + virtx
		MouseY = imsg->MouseY - _mtop(obj);	// + virty

		if (imsg->Class == IDCMP_RAWKEY)
		{
			mouse_inside = data->mouse_inside;

			switch (imsg->Code)
			{
				case RAWKEY_UP    : browser_window_key_press(data->browser, KEY_UP); break;
				case RAWKEY_DOWN  : browser_window_key_press(data->browser, KEY_DOWN); break;
				case RAWKEY_LEFT  : browser_window_key_press(data->browser, KEY_LEFT); break;
				case RAWKEY_RIGHT : browser_window_key_press(data->browser, KEY_RIGHT); break;
				case RAWKEY_ESCAPE: browser_window_key_press(data->browser, 27); break;

				case RAWKEY_LSHIFT        : data->key_state = BROWSER_MOUSE_MOD_1; break;
				case RAWKEY_LSHIFT  + 0x80: data->key_state = 0; break;
				case RAWKEY_CONTROL       : data->key_state = BROWSER_MOUSE_MOD_2; break;
				case RAWKEY_CONTROL + 0x80: data->key_state = 0; break;

				default:
					if (imsg->Code < 0x80)
					{
						ULONG ucs4;

						#if defined(__MORPHOS2__)
						ucs4 = getv(imsg, IMSGA_UCS4);
						#else
						struct InputEvent ie;
						TEXT buffer[4];

						ie.ie_Class				= IECLASS_RAWKEY;
						ie.ie_SubClass			= 0;
						ie.ie_Code				= imsg->Code;
						ie.ie_Qualifier		= imsg->Qualifier;
						ie.ie_EventAddress	= NULL;

						ucs4 = 0;

						if ((MapRawKey(&ie, (STRPTR)&buffer, sizeof(buffer), NULL)) == 1)
						{
							ucs4 = buffer[0];

							if (CodesetsBase == NULL)
							{
								CodesetsBase = OpenLibrary("codesets.library", 0);
							}

							if (CodesetsBase)
							{
								ULONG *dst;
								APTR cset;

								cset = CodesetsFindA("UTF-32", NULL);	// codesets does not know about UCS-4

								if (cset)
								{
									dst = (APTR)CodesetsConvertStr(CSA_Source, &buffer, CSA_SourceLen, 1, CSA_DestCodeset, cset, TAG_DONE);
									ucs4 = *dst;
									CodesetsFreeA(dst, NULL);
								}
							}
						}
						#endif

						#if MULTITHREADED
						netsurf_add_simple_job(data->browser, JOB_KEY_PRESS, ucs4, 0, 0);
						#else
						browser_window_key_press(data->browser, ucs4);
						#endif
					}
					break;
			}

			rc = MUI_EventHandlerRC_Eat;
		}
		else if (imsg->Class == IDCMP_MOUSEBUTTONS)
		{
			mouse_inside = 0;

			if (MouseX >= 0 && MouseY >= 0 && MouseX < data->mwidth && MouseY < data->mheight)
			{
				LONG button, click;

				button = imsg->Code & IECODE_LBUTTON ? BROWSER_MOUSE_PRESS_1 : BROWSER_MOUSE_PRESS_2;
				click = imsg->Code & IECODE_LBUTTON ? BROWSER_MOUSE_CLICK_1 : BROWSER_MOUSE_CLICK_2;
				mouse_inside = 1;
				MouseX += getv(obj, MUIA_Virtgroup_Left);	//data->vleft_old;
				MouseY += getv(obj, MUIA_Virtgroup_Top);	//data->vtop_old;

				switch (imsg->Code)
				{
#if MULTITHREADED
					case SELECTDOWN:
					case MIDDLEDOWN:
						netsurf_add_simple_job(data->browser, JOB_MOUSECLICK, button | data->key_state, MouseX, MouseY);
						data->mouse_state = button;
						rc = MUI_EventHandlerRC_Eat;
						break;

					case SELECTUP:
					case MIDDLEUP:
						if (data->mouse_state & button)
							netsurf_add_simple_job(data->browser, JOB_MOUSECLICK, click | data->key_state, MouseX, MouseY);
						else
							browser_window_mouse_drag_end(data->browser, 0, MouseX, MouseY);

						data->mouse_state = 0;
						rc = MUI_EventHandlerRC_Eat;
						break;
#else
					case SELECTDOWN:
					case MIDDLEDOWN:
						browser_window_mouse_click(data->browser, button | data->key_state, MouseX, MouseY);
						data->mouse_state = button;
						rc = MUI_EventHandlerRC_Eat;
						break;

					case SELECTUP:
					case MIDDLEUP:
						if (data->mouse_state & button)
							browser_window_mouse_click(data->browser, click | data->key_state, MouseX, MouseY);
						else
							browser_window_mouse_drag_end(data->browser, 0, MouseX, MouseY);

						data->mouse_state = 0;
						rc = MUI_EventHandlerRC_Eat;
						break;
#endif
				}

#if !MULTITHREADED
				if (rc == MUI_EventHandlerRC_Eat)
					Signal(SysBase->ThisTask, SIGBREAKF_CTRL_E);
#endif
			}
		}
		else /* if (imsg->Class == IDCMP_MOUSEMOVE) */
		{
			mouse_inside = 0;

			if (MouseX >= 0 && MouseY >= 0 && MouseX < data->mwidth && MouseY < data->mheight)
			{
				#warning might need fix for multithreaded build
				rc = MUI_EventHandlerRC_Eat;
				MouseX += getv(obj, MUIA_Virtgroup_Left);	//data->vleft_old;
				MouseY += getv(obj, MUIA_Virtgroup_Top);	//data->vtop_old;
				mouse_inside = 1;

				if (data->mouse_state & BROWSER_MOUSE_PRESS_1)
				{
					browser_window_mouse_track(data->browser, BROWSER_MOUSE_DRAG_1 | data->key_state, MouseX,  MouseY);
					data->mouse_state = BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
				}
				else if (data->mouse_state & BROWSER_MOUSE_PRESS_2)
				{
					browser_window_mouse_track(data->browser, BROWSER_MOUSE_DRAG_2 | data->key_state, MouseX,  MouseY);
					data->mouse_state = BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
				}
				else
				{
					browser_window_mouse_track(data->browser, data->mouse_state | data->key_state, MouseX,  MouseY);
				}

				//Signal(SysBase->ThisTask, SIGBREAKF_CTRL_E);
			}
		}

		#if defined(__MORPHOS2__)
		if (data->mouse_inside != mouse_inside)
		{
			data->mouse_inside = mouse_inside;
			set(_window(obj), WA_PointerType, mouse_inside ? data->pointertype : POINTERTYPE_NORMAL);
		}
		#endif
	}

	return rc;

}

DEFMMETHOD(Backfill)
{
#if 0
	GETDATA;
	struct content *c;

		c = data->browser->current_content;

		if (c)
		{
			ULONG redraw, vleft, vtop;

			vleft = getv(obj, MUIA_Virtgroup_Left);
			vtop  = getv(obj, MUIA_Virtgroup_Top);

			redraw = data->redraw || data->RastPort == NULL || data->redraw || vleft != data->vleft_old || vtop != data->vtop_old;

			if (redraw && c->locked)
			{
				DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 1 | MUIV_PushMethod_Delay(1) | MUIF_PUSHMETHOD_SINGLE, MM_Browser_Redraw);
			}
			else
			{
				data->redraw = 0;

				if (create_offscreen_bitmap(obj, data))
				{
					ULONG mleft, mtop, width, height;

					mleft = msg->left;	//_mleft(obj);
					mtop = msg->top;	//_mtop(obj);
					width = msg->right - mleft + 1;
					height = msg->bottom - mtop + 1;

					//if (redraw)
					{
						LONG offx, offy;

						offx = _mleft(obj) - mleft;
						offy = _mtop(obj) - mtop;

						data->vleft_old = vleft;
						data->vtop_old = vtop;

						current_redraw_browser = data->browser;
						plot = muiplot;

						#if !defined(__MORPHOS__)
						renderinfo.screen = _screen(obj);
						#endif

						renderinfo.rp = data->RastPort;
						renderinfo.left = offx;
						renderinfo.top = offy;
						renderinfo.width = data->mwidth;
						renderinfo.height = data->mheight;

						content_redraw(c, -(vleft - offx), -(vtop - offy), width, height, 0, 0, c->width, c->height, data->browser->scale, 0xfffff);
						//content_redraw(c, -(vleft), -(vtop), width, height, 0, 0, c->width, c->height, data->browser->scale, 0xfffff);

						current_redraw_browser = NULL;
					}

					//BltBitMapRastPort(data->BitMap, 0, 0, _rp(obj), mleft, mtop, width, height, 0xc0);
					BltBitMapRastPort(data->BitMap, 0, 0, _rp(obj), _mleft(obj), _mtop(obj), data->mwidth, data->mheight, 0xc0);
				}
			}
		}
#endif
	return 0;
}

DEFTMETHOD(Browser_Redraw)
{
	struct content *c;
	GETDATA;

	data->redraw_pending = 0;

#if 0
	c = data->browser->current_content;
	data->redraw = 1;

	if (c && !c->locked)
		MUI_Redraw(obj, MADF_DRAWUPDATE);
#else
	MUI_Redraw(obj, MADF_DRAWUPDATE);
#endif

	if (data->reload_available == 0) {
		data->reload_available = 1;
		set(obj, MA_Browser_ReloadAvailable, TRUE);
	}

	update_buttons(obj, data);
}

DEFMMETHOD(Draw)
{
	DOSUPER;

	if (msg->flags & MADF_DRAWOBJECT|MADF_DRAWUPDATE) {
		GETDATA;
#if 1
		if (data->changed || !data->BitMap || data->redraw_pending) {
			if (data->redraw_pending == 0) {
				data->redraw_pending = 1;

				netsurf_add_simple_job(NULL, JOB_REDRAW, (IPTR)data->context, 0, 0);

				if (!data->BitMap)
					FillPixelArray(_rp(obj), _mleft(obj), _mtop(obj), data->mwidth, data->mheight, 0xffffff);
			}
		} else /* if (data->BitMap) */ {
			LONG vleft, vtop;

			vleft = getv(obj, MUIA_Virtgroup_Left);
			vtop  = getv(obj, MUIA_Virtgroup_Top);

			BltBitMapRastPort(data->BitMap, vleft, vtop, _rp(obj), _mleft(obj),
					_mtop(obj), MIN(data->mwidth, data->bm_width), MIN(data->mheight, data->bm_height), 0xc0);

			if (data->bm_width < data->mwidth)
				FillPixelArray(_rp(obj), _mleft(obj) + data->bm_width, _mtop(obj), data->mwidth - data->bm_width, data->mheight, 0xffffff);

			if (data->bm_height < data->mheight)
				FillPixelArray(_rp(obj), _mleft(obj), _mtop(obj) + data->bm_height, data->mwidth, data->mheight - data->bm_height, 0xffffff);
		}

#else
		struct content *c;

		c = data->browser->current_content;

		if (c)
		{
			LONG redraw, pos_change, vleft, vtop;

			vleft = getv(obj, MUIA_Virtgroup_Left);
			vtop  = getv(obj, MUIA_Virtgroup_Top);

			redraw = data->redraw || data->RastPort == NULL;
			pos_change = vleft != data->vleft_old || vtop != data->vtop_old;

			if (c->locked)
			{
#if 1
				if (redraw || pos_change)
					DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 1 | MUIV_PushMethod_Delay(1) | MUIF_PUSHMETHOD_SINGLE, MM_Browser_Redraw);
#endif
			}
			else
			{
				data->redraw = 0;

				if (create_offscreen_bitmap(obj, data, data->mwidth, data->mheight))
				{
					ULONG mleft, mtop, width, height;

					mleft = _mleft(obj);
					mtop = _mtop(obj);
					width = _mwidth(obj);
					height = _mheight(obj);

					if (redraw)
					{
						LONG offy = 0;

						if (vtop < data->render_top)
						{
							offy = height;

							if (vtop - offy < 0)
								offy = vtop;
						}

						data->vleft_old = vleft;
						data->vtop_old = vtop;
						data->render_top = vtop - offy;

						current_redraw_browser = data->browser;
						plot = muiplot;

						renderinfo.window = _window(obj);
						renderinfo.rp = data->RastPort;
						renderinfo.left = 0;
						renderinfo.top = 0;
						renderinfo.width = c->width;
						renderinfo.height = c->height;	// * RENDER_MULTIPLIER;
						renderinfo.maxwidth = _screen(obj)->Width;
						renderinfo.maxheight = _screen(obj)->Height;

						//content_redraw(c, -(vleft), -(vtop - offy), 1, 1, 0, 0, c->width, c->height, data->browser->scale, 0xfffff);
						content_redraw(c, 0, 0, 1, 1, 0, 0, c->width, c->height, data->browser->scale, 0xfffff);

						current_redraw_browser = NULL;
					}

					//BltBitMapRastPort(data->BitMap, 0, vtop - data->render_top, _rp(obj), mleft, mtop, width, height, 0xc0);
					BltBitMapRastPort(data->BitMap, vleft, vtop, _rp(obj), mleft, mtop, width, height, 0xc0);
				}
			}
		}
#endif
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECGET
DECSET
DECMMETHOD(Backfill)
DECMMETHOD(Cleanup)
DECMMETHOD(Draw)
DECMMETHOD(Hide)
DECMMETHOD(Setup)
DECMMETHOD(Show)
DECMMETHOD(HandleEvent)
DECSMETHOD(Browser_Back)
DECSMETHOD(Browser_Find)
DECSMETHOD(Browser_Forward)
DECSMETHOD(Browser_GetBitMap)
DECSMETHOD(Browser_Go)
DECSMETHOD(Browser_Print)
DECSMETHOD(Browser_Redraw)
DECSMETHOD(Browser_Reload)
DECSMETHOD(Browser_SetContentSize)
DECSMETHOD(Browser_SetContentType)
DECSMETHOD(Browser_Stop)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Virtgroup, browserclass)
