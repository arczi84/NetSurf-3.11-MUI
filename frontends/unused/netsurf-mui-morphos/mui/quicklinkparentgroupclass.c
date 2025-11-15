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
 *

 * $Id: bookmarkgroupclass.c,v 0.1 2009/01/23 16:00:00 polymere Exp $
 */

#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/mui.h"
//#include "bookmarkgroupclass.h"

//#define D(x)

/* private */

struct Data {
	ULONG hided;
	Object *qlgroup;
};

extern Object *app;

#define D(x)
#define LOC(a,b) (b)

static ULONG LayoutFonc( struct Hook *hook, Object *obj, struct MUI_LayoutMsg *lm )
{
	//struct Data *data=NULL;
	
	switch (lm->lm_Type)
	{
		case MUILM_MINMAX:
		{
			ULONG min_w=0,min_h=0,mode=0,buts=0,max_w=0,max_h=0;
			Object *cstate = (Object *)lm->lm_Children->mlh_Head;
			Object *child;
			if ( (child=(Object *)NextObject(&cstate)) )
			{
				if (get(child, MA_QuickLinkGroup_Mode, &mode))
				{
					get(child, MA_QuickLinkGroup_Mode, &mode);
					get(child, MA_QuickLinkGroup_MinW, &min_w);
					get(child, MA_QuickLinkGroup_MinH, &min_h);
					get(child, MA_QuickLinkGroup_MaxW, &max_w);
					get(child, MA_QuickLinkGroup_MaxH, &max_h);
					D(kprintf("QL Parent Group: mode=%ld buts=%ld min_w=%ld min_h=%ld\n",mode,buts,min_w,min_h));
					if (mode & MV_QuickLinkGroup_Mode_Vert)
					{
						lm->lm_MinMax.MinWidth  = max_w;
						lm->lm_MinMax.MinHeight = min_h;
						lm->lm_MinMax.DefWidth  = max_w;
						lm->lm_MinMax.DefHeight = max_h;
						lm->lm_MinMax.MaxWidth  = max_w;
						lm->lm_MinMax.MaxHeight = max_h;
					}
					else
					{
						lm->lm_MinMax.MinWidth  = min_w;
						lm->lm_MinMax.MinHeight = max_h;
						lm->lm_MinMax.DefWidth  = max_w;
						lm->lm_MinMax.DefHeight = max_h;
						lm->lm_MinMax.MaxWidth  = max_w;
						lm->lm_MinMax.MaxHeight = max_h;
					}
				}
				else
				{
					D(kprintf("QL Parent Group: HIDED\n"));
					if (_maxheight(child)==1)
					{
						// HSpace()
						lm->lm_MinMax.MinWidth  = 1;
						lm->lm_MinMax.MinHeight = 1;
						lm->lm_MinMax.DefWidth  = MUI_MAXMAX;
						lm->lm_MinMax.DefHeight = 1;
						lm->lm_MinMax.MaxWidth  = MUI_MAXMAX;
						lm->lm_MinMax.MaxHeight = 1;
					}
					else
					{
						// VSpace()
						lm->lm_MinMax.MinWidth  = 1;
						lm->lm_MinMax.MinHeight = 1;
						lm->lm_MinMax.DefWidth  = 1;
						lm->lm_MinMax.DefHeight = MUI_MAXMAX;
						lm->lm_MinMax.MaxWidth  = 1;
						lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
					}
				}
			}
			return(0);
		}

		case MUILM_LAYOUT:
		{
			APTR cstate = (APTR)lm->lm_Children->mlh_Head;
			Object *child;
			ULONG w,h,mode;
			if ( (child=(Object *)NextObject(&cstate)) )
			{
				if (get(child, MA_QuickLinkGroup_Mode, &mode))
				{
					w=_maxwidth(child);
					h=_maxheight(child);
					if (w>lm->lm_Layout.Width) w=lm->lm_Layout.Width;
					if (h>lm->lm_Layout.Height) h=lm->lm_Layout.Height;
					D(kprintf("QL Parent Group: w=%ld h=%ld\n",w,h));
					if (!MUI_Layout(child,0,0,w,h,0))
					{
						return(FALSE);
					}
				}
				else
				{
					// Space
					if (_maxheight(obj)==1)
					{
						// Layout HSpace()
						if (!MUI_Layout(child,0,0,lm->lm_Layout.Width,1,0))
						{
							return(FALSE);
						}
					}
					else
					{
						// Layout VSpace()
						if (!MUI_Layout(child,0,0,1,lm->lm_Layout.Height,0))
						{
							return(FALSE);
						}
					}
				}
			}
			return (TRUE);
		}
	}
	return((ULONG)MUILM_UNKNOWN);
}

static struct Hook LayoutFonc_hook = {
    {NULL, NULL},
	(HOOKFUNC) HookEntry,
	(HOOKFUNC) LayoutFonc,
	NULL
};

static void doset(Object *obj, struct Data *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MA_QuickLinkParentGroup_QLGroup:      //Init only
			if (!data->qlgroup)
			{
				ULONG mode=0;
				Object *child;
				data->qlgroup=(Object *)tag->ti_Data;
				get(data->qlgroup, MA_QuickLinkGroup_Mode, &mode);
				if (mode & MV_QuickLinkGroup_Mode_Vert)
				{
					// Vertical mode add 1 pixel band
					child=(Object *)HSpace(1);
					if(child) DoMethod(obj, MUIM_Group_AddTail, child);
				}
				else
				{
					// Horiz mode add 1 pixel band
					child=(Object *)VSpace(1);
					if(child) DoMethod(obj, MUIM_Group_AddTail, child);
				}
				data->hided=TRUE;  
			}
			break;
		case MA_QuickLinkParentGroup_Hide:
			if (data->hided!=tag->ti_Data)
			{
				if (data->hided)
				{
					// Hided
					// Remove space then add group
					Object *button;
					struct MinList *l;
					APTR cstate;
					D(kprintf("QuickLinkParentGroup_Hide FALSE\n"));
					get(obj, MUIA_Group_ChildList, &l);
					cstate=l->mlh_Head;
					button=(Object *)NextObject(&cstate);
					//DoMethod(obj, MUIM_Group_InitChange);
					if (button)
					{
						D(kprintf("QuickLinkParentGroup_Hide Remove space\n"));
						DoMethod(obj, OM_REMMEMBER, button);
						DoMethod(button, OM_DISPOSE);
					}
					D(kprintf("QuickLinkParentGroup_Hide Add QLGroup %08lx\n", data->qlgroup));
					DoMethod(obj, OM_ADDMEMBER, data->qlgroup);
					//DoMethod(obj, MUIM_Group_ExitChange);
					data->hided=FALSE;
				}
				else
				{
					// Group showed, remove it and add little space
					ULONG mode=0;
					APTR child;
					struct MinList *l;
					APTR cstate;

					D(kprintf("QuickLinkParentGroup_Hide TRUE\n"));
					get(obj, MUIA_Group_ChildList, &l);
					cstate=l->mlh_Head;
					data->qlgroup=(Object *)NextObject(&cstate);
					//DoMethod(obj, MUIM_Group_InitChange);
					D(kprintf("QuickLinkParentGroup_Hide Remove QLGroup %08lx\n", data->qlgroup));
					if (data->qlgroup)
					{
						DoMethod(obj, OM_REMMEMBER, data->qlgroup);
					}

					get(data->qlgroup, MA_QuickLinkGroup_Mode, &mode);
					if (mode & MV_QuickLinkGroup_Mode_Vert)
					{
						// Vertical mode add 1 pixel band
						D(kprintf("QuickLinkParentGroup_Hide Add HSpace\n"));
						child=(Object *)HSpace(1);
						if(child) DoMethod(obj, MUIM_Group_AddTail, child);
					}
					else
					{
						// Horiz mode add 1 pixel band
						D(kprintf("QuickLinkParentGroup_Hide Add VSpace\n"));
						child=(Object *)VSpace(1);
						if(child) DoMethod(obj, MUIM_Group_AddTail, child);
					}
					//DoMethod(obj, MUIM_Group_ExitChange);
					data->hided=TRUE;
				}
			}
			break;
	}
	NEXTTAG
}

DEFNEW
{
	obj = (Object *) DoSuperNew(cl, obj,
		MUIA_Group_LayoutHook, &LayoutFonc_hook,
		//MUIA_Weight, 800,
		TAG_MORE, INITTAGS,
		End;
	if (obj)
	{
		GETDATA;
		data->qlgroup=NULL;
		data->hided=FALSE;
		doset(obj, data, msg->ops_AttrList);
	}
	return ((ULONG)obj);
}

DEFDISP
{
	GETDATA;
	D(kprintf("QuickLinkParentGroup: disposing\n"));
	if (data->hided)
	{
		D(kprintf("Hided mode dispose QLGroup"));
		DoMethod(data->qlgroup, OM_DISPOSE);
	}
	return DOSUPER;
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

BEGINMTABLE
DECNEW
DECDISP
DECSET
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, quicklinkparentgroupclass)
