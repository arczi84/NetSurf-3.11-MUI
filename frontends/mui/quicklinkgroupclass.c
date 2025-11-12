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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mui/applicationclass.h"
#include "mui/bookmarkgroupclass.h"
#include "mui/mui.h"

#define D(x)

/* private */

struct Data {
	ULONG count;
	ULONG button_w;
	ULONG button_h;
	ULONG hspace;
	ULONG vspace; 
	ULONG row;
	ULONG mode;
	ULONG truerow;
	ULONG re_layout_count;
	ULONG num_but;
	ULONG max_w;
	ULONG max_h;
	Object *parent;
};

#define LOC(a,b) (b)

static void doset(Object *obj, struct Data *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MA_QuickLinkGroup_Row:
			data->row=data->truerow=tag->ti_Data;
			break;
		case MA_QuickLinkGroup_Mode:
			if (data->mode!=tag->ti_Data)
			{
				data->mode=tag->ti_Data;   
				if (data->num_but)
				{
					// Refresh all buttons
					struct MinList *l;
					APTR cstate;
					Object *child;
					
					D(kprintf("QuickLinkGroup Set Mode\n"));
					
					get(obj,MUIA_Group_ChildList,&l);
					cstate=l->mlh_Head;
					
					DoMethod(obj, MM_QuickLinkGroup_InitChange, MV_QuickLinkGroup_Change_Redraw);

					while ((child=(Object *)NextObject(&cstate)))
					{
						set(child, MA_QuickLinkGroup_Mode, data->mode);
					}

					DoMethod(obj, MM_QuickLinkGroup_ExitChange, MV_QuickLinkGroup_Change_Redraw);
				}
			}
			break;
		case MA_QuickLinkGroup_Buttons:
			data->num_but=tag->ti_Data;
			break;
		case MA_QuickLinkGroup_Parent:
			data->parent=(Object *)tag->ti_Data;
			break;
	}
	NEXTTAG
}

static ULONG LayoutFonc( struct Hook *hook, Object *obj, struct MUI_LayoutMsg *lm )
{
	struct Data *data=NULL;
	
	switch (lm->lm_Type)
	{
		case MUILM_MINMAX:
		{
			/*
			** MinMax calculation function. When this is called,
			** the children of your group have already been asked
			** about their min/max dimension so you can use their
			** dimensions to calculate yours.
			*/

			Object *cstate = (Object *)lm->lm_Children->mlh_Head;
			Object *child;

			LONG minwidth=0;
			LONG minheight=0;
			LONG maxheight=0;
			
			get(obj, MA_QuickLinkGroup_Data, &data);
	
			D(kprintf("### QuickLinkGroup MinMax Data: %08lx:\n",(ULONG)data));
			if (!data) return(FALSE);    

			//get(obj, MUIA_Group_HorizSpacing, &data->hspace);
			//get(obj, MUIA_Group_VertSpacing,  &data->vspace);
			data->hspace=(data->mode & MV_QuickLinkGroup_Mode_Button) ? 2 : 4;
			data->vspace=(data->mode & MV_QuickLinkGroup_Mode_Button) ? 2 : 4;

			/* find out biggest widths & heights of our children */
			data->count=0;
			//data->truerow=data->row;

			//kprintf("Start calculate hspace=%ld vspace=%ld\n",data->hspace,data->vspace);
			cstate = (Object *)lm->lm_Children->mlh_Head;
			while ( (child=(Object *)NextObject(&cstate)) )
			{
				data->count++;
				if (_minwidth(child)>minwidth)  minwidth=_minwidth(child);
				if (_minheight(child)>minheight) minheight=_minheight(child);
				D(kprintf("mw=%ld dw=%ld\n",_minwidth(child),_defwidth(child));)
			}

			// Store size mini of a button like a grid
			data->button_w=minwidth;
			data->button_h=minheight;

			// Define minimum size

			
			if (data->mode & MV_QuickLinkGroup_Mode_Vert)
			{
				if (data->count>1) minheight = minheight*2+data->vspace;
				// minwidth=(minwidth*data->row)+(data->hspace*(data->row-1)); Vert has only one column
			}
			else
			{
				if (data->count>1) minwidth = minwidth*2 + data->hspace;
				//minheight=(minheight*data->row)+(data->vspace*(data->row-1));
				maxheight=(minheight*data->truerow)+(data->vspace*(data->truerow-1));
			}

			//D(kprintf("Group: min size is w=%ld h=%ld\n",minwidth,minheight));

			/* set the result fields in the message */
			if (data->mode & MV_QuickLinkGroup_Mode_Vert)
			{
				lm->lm_MinMax.MinWidth  = minwidth;
				lm->lm_MinMax.MinHeight = minheight;
				lm->lm_MinMax.DefWidth  = minwidth,
				lm->lm_MinMax.DefHeight = MUI_MAXMAX;
				lm->lm_MinMax.MaxWidth  = minwidth;
				lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
			}
			else
			{
				lm->lm_MinMax.MinWidth  = minwidth;
				lm->lm_MinMax.MinHeight = minheight;
				lm->lm_MinMax.DefWidth  = MUI_MAXMAX;
				lm->lm_MinMax.DefHeight = minheight;
				lm->lm_MinMax.MaxWidth  = MUI_MAXMAX;
				lm->lm_MinMax.MaxHeight = maxheight;
			}
			
			data->max_h=lm->lm_MinMax.MaxHeight;
			data->max_w=lm->lm_MinMax.MaxWidth;
 
			D(kprintf("Layout MinMax END.\n"));
			return(0);
		}

		case MUILM_LAYOUT:
		{
			/*
			** Layout function. Here, we have to call MUI_Layout() for each
			** our children. MUI wants us to place them in a rectangle
			** defined by (0,0,lm->lm_Layout.Width-1,lm->lm_Layout.Height-1)
			** You are free to put the children anywhere in this rectangle.
			**
			** If you are a virtual group, you may also extend
			** the given dimensions and place your children anywhere. Be sure
			** to return the dimensions you need in lm->lm_Layout.Width and
			** lm->lm_Layout.Height in this case.
			**
			** Return TRUE if everything went ok, FALSE on error.
			** Note: Errors during layout are not easy to handle for MUI.
			**       Better avoid them!
			*/

			APTR cstate = (APTR)lm->lm_Children->mlh_Head;
			Object *child;
			ULONG xpos,ypos,cols=0,col_count,row_count=0,count=0,max_row=0;
			ULONG array_x,array_y,temp_w;
			struct treedata *node; 	
			
			get(obj, MA_QuickLinkGroup_Data, &data);
			
			D(kprintf("### QuickLinkGroup Layout Data: %08lx:\n",(ULONG)data));
			if (!data) return(FALSE);    
			//D(kprintf("Layout Width=%ld Button_w=%ld Hspace=%ld\n", lm->lm_Layout.Width, data->button_w, data->hspace));

			if (data->mode & MV_QuickLinkGroup_Mode_Vert)
			{
				cols= (lm->lm_Layout.Height) / (data->button_h + data->vspace); // get max button per colum
				//if (cols>data->count) cols=data->count;
				array_y= data->button_h;
				array_x= data->button_w;
			}
			else
			{
				if (data->mode & MV_QuickLinkGroup_Mode_Col)
				{
					ULONG last_col=(lm->lm_Layout.Width) / (data->button_w + data->hspace);  
					cols=data->count/data->row;  // Want n cols per line   	
					if (data->count%data->row>0) cols++;   
					D(kprintf("Layout can display %ld and want %ld\n",last_col,cols);)
					
					if (last_col>cols)
					{
						// enouth space
						array_x= (lm->lm_Layout.Width) / cols;
						if ((array_x*cols)+(data->hspace*(cols-1)>lm->lm_Layout.Width)) array_x--;   
						if ((array_x*cols)+(data->hspace*(cols-1)>lm->lm_Layout.Width)) array_x--;
						D(kprintf("Col Mode 1: cols=%ld array_x=%ld\n",cols,array_x);)
					}
					else
					{
						array_x= (lm->lm_Layout.Width) / last_col; 	 
						if ((array_x*last_col)+(data->hspace*(last_col-1)>lm->lm_Layout.Width)) array_x--;
						if ((array_x*last_col)+(data->hspace*(last_col-1)>lm->lm_Layout.Width)) array_x--;    
						D(kprintf("Col Mode 2: last_col=%ld array_x=%ld\n",last_col,array_x);)
					}
					/*if (lm->lm_Layout.Width/cols > array_x)
					{
						  
						array_x= (lm->lm_Layout.Width) / last_col;
						//if ((array_x*cols)+(data->hspace*(cols-1)>lm->lm_Layout.Width)) array_x--;
						//if ((array_x*cols)+(data->hspace*(cols-1)>lm->lm_Layout.Width)) array_x--;
					}*/
					array_y= data->button_h;
				}
				else
				{
					cols= (lm->lm_Layout.Width) / (data->button_w + data->hspace); // get max button per line
					if (cols>data->count) cols=data->count;
					array_x= (lm->lm_Layout.Width) / cols;
					array_y= data->button_h;
					if ((array_x*cols)+(data->hspace*(cols-1)>lm->lm_Layout.Width)) array_x--;
					if ((array_x*cols)+(data->hspace*(cols-1)>lm->lm_Layout.Width)) array_x--;
				}
			}
			//D(kprintf("Layout cols=%ld array_x=%ld array_y=%ld\n", cols, array_x, array_y));
			xpos=0;
			ypos=0;
			col_count=1;

			if (data->mode & MV_QuickLinkGroup_Mode_Prop)
			{
				for(count=1;(count<=data->count);count++)
				{
					cstate = (APTR)lm->lm_Children->mlh_Head;  
					while ( (child =(Object *)NextObject(&cstate)) )
					{
						node=NULL;
						get(child, MUIA_UserData, &node);
						if ((node) && node->ql_order==count )
						{
							// This one now
							temp_w=_minwidth(child)+10;
							if (temp_w>data->button_w) temp_w=data->button_w;

							if (data->mode & MV_QuickLinkGroup_Mode_Vert)
							{
								// Do nothing
							}
							else
							{
								if (xpos+temp_w>lm->lm_Layout.Width)
								{
									// Not enouth space for this button
									xpos=0;
									ypos+=data->vspace+array_y;
									row_count++;
								}
							}
							if (!MUI_Layout(child,xpos,ypos,temp_w,_minheight(child),0))
							{
								D(kprintf("Layout fail for: child=%08lx x=%ld y=%ld w=%ld h=%ld\n",child,xpos,ypos,array_x,array_y));
								return(FALSE);
							}
							if (data->mode & MV_QuickLinkGroup_Mode_Vert) ypos+=data->vspace+_minheight(child);
							else xpos+=data->hspace+temp_w; //_minwidth(child);
							continue;
						}
					}
				}
				max_row=row_count;
				D(kprintf("Prop MaxRow is %ld Truerow is %ld\n", max_row, data->truerow)); 	
			}
			else
			{
				while ( (child =(Object *)NextObject(&cstate)) )
				{
					node=NULL;
					get(child, MUIA_UserData, &node);
					if (node)
					{
						ULONG pos=node->ql_order;
						pos--;
						//D(kprintf("Order %ld xpos=%ld ypos=%ld\n", pos, pos%cols, pos/cols));
						if (data->mode & MV_QuickLinkGroup_Mode_Vert)  
						{
							row_count=pos%cols;
							col_count=pos/cols;
							xpos=0; //col_count*(array_x+data->hspace);
							ypos=pos*(array_y+data->vspace);
						}
						else
						{
							if (data->mode & MV_QuickLinkGroup_Mode_Col)
							{
								// Mode for move button by column when no space
								col_count=pos%cols;
								row_count=pos/cols;
								xpos=col_count*(array_x+data->hspace);
								ypos=row_count*(array_y+data->vspace);
								D(kprintf("Want %ld x %ld\n",col_count, row_count);)
								if (xpos+array_x>lm->lm_Layout.Width)
								{
									// no more space for this button add it under  	
									ULONG last_col=(lm->lm_Layout.Width) / (array_x + data->hspace);
									ULONG warp_col=col_count-last_col;
									col_count=(warp_col%last_col);
									row_count+=data->row+data->row*(warp_col/last_col);
									D(kprintf("-> Recalc %ld x %ld\n",col_count, row_count);)
									xpos=col_count*(array_x+data->hspace);
									ypos=row_count*(array_y+data->vspace);
								}
								if (row_count>max_row) max_row=row_count;  	
							}
							else
							{
								// Mode for move buttons at end of line in the next
								col_count=pos%cols;
								row_count=pos/cols;
								xpos=col_count*(array_x+data->hspace);
								ypos=row_count*(array_y+data->vspace);
								if (row_count>max_row) max_row=row_count;  	
							}
						}
						//D(kprintf("Layout: x=%ld y=%ld x+=%ld y+=%ld\n", xpos, ypos, xpos+array_x, ypos+array_y ));
						if (array_x<data->button_w)
						{
							D(kprintf("################# BUG ##############\n");)
							D(kprintf("Array X < minbut delta is %ld (space avail %ld)\n",data->button_w-array_x,data->hspace);)
							if (data->button_w-array_x<=data->hspace) array_x+=data->button_w-array_x;
						}
						if (!MUI_Layout(child,xpos,ypos,array_x,array_y,0))
						{
							D(kprintf("Layout fail for: child=%08lx x=%ld y=%ld w=%ld h=%ld\n",child,xpos,ypos,array_x,array_y));
							return(FALSE);
						}
					}
				}
			}
			if (data->mode & MV_QuickLinkGroup_Mode_Vert)
			{
				if (data->count>cols) lm->lm_Layout.Height=(array_y+data->vspace)*data->count;
			}
			else
			{
				lm->lm_Layout.Height=((max_row+1)*array_y)+(data->vspace*max_row);
			}
			D(kprintf("Layout END.\n"));
			max_row++;
			D(kprintf("Relayout: max_row=%ld truerow=%ld initial=%ld\n", max_row, data->truerow, data->row));
			if (data->re_layout_count<2)
			{
				APTR app = application;

				if (max_row>data->truerow && data->truerow<data->row )
				{
					D(kprintf("Relayout: Need more max_row=%ld truerow=%ld initial=%ld ", max_row, data->truerow, data->row));
					if (max_row>data->row)
					{
						data->truerow=data->row;
						//if (max_row>1 && data->row==1) data->truerow++;
					}
					else data->truerow=max_row;
					
					D(kprintf("-> truerow=%ld\n", data->truerow));
					data->re_layout_count++;
					/*
					DoMethod(app, MUIM_Application_PushMethod,
						_parent(obj), 1,
						MUIM_Group_InitChange);
					DoMethod(app, MUIM_Application_PushMethod,
						obj, 1,
						MUIM_Group_InitChange);
					DoMethod(app, MUIM_Application_PushMethod,
						obj, 1,
						MUIM_Group_ExitChange);
					DoMethod(app, MUIM_Application_PushMethod,
						_parent(obj), 1,
						MUIM_Group_ExitChange);*/
					DoMethod(app, MUIM_Application_PushMethod,
						obj, 2,
						MM_QuickLinkGroup_InitChange, MV_QuickLinkGroup_Change_Redraw);
					DoMethod(app, MUIM_Application_PushMethod,
						obj, 2,
						MM_QuickLinkGroup_ExitChange, MV_QuickLinkGroup_Change_Redraw);

					return(TRUE);
				}
				if (max_row<data->truerow && data->truerow>1)
				{
					D(kprintf("Relayout: Need less max_row=%ld truerow=%ld initial=%ld ", max_row, data->truerow, data->row));
					if (max_row==2 && data->row==1) data->truerow=2;
					else data->truerow=max_row;
					D(kprintf("-> truerow=%ld\n", data->truerow));
					data->re_layout_count++;
					/*DoMethod(app, MUIM_Application_PushMethod,
						obj, 1,
						MUIM_Group_InitChange);
					DoMethod(app, MUIM_Application_PushMethod,
						obj, 1,
						MUIM_Group_ExitChange);*/

					DoMethod(app, MUIM_Application_PushMethod,
						obj, 2,
						MM_QuickLinkGroup_InitChange, MV_QuickLinkGroup_Change_Redraw);
					DoMethod(app, MUIM_Application_PushMethod,
						obj, 2,
						MM_QuickLinkGroup_ExitChange, MV_QuickLinkGroup_Change_Redraw);
 
					return(TRUE);
				}
			}
			else
			{
				D(kprintf("############## Relayout bug try fix\n"));
			}
			data->re_layout_count=0;
			return(TRUE);
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

DEFNEW
{
	obj = (Object *) DoSuperNew(cl, obj,
		MUIA_Group_LayoutHook, &LayoutFonc_hook,
		TAG_MORE, INITTAGS,
		End;

	if (obj)
	{
		GETDATA;
		data->count=0;
		data->button_w=0;
		data->button_h=0;
		data->hspace=0;
		data->vspace=0;
		data->row=1;
		data->truerow=1;
		data->re_layout_count=0;
		data->parent=NULL;
		data->num_but=0;
		doset(obj, data, msg->ops_AttrList);
	}
	return ((ULONG)obj);
}

DEFDISP
{
	GETDATA;
	D(kprintf("QuickLinkGroup: disposing\n"));
	return DOSUPER;
}

DEFGET
{
	GETDATA;  
	switch (msg->opg_AttrID)
	{
		case MA_QuickLinkGroup_Data:
		{
			*msg->opg_Storage = (ULONG)data;
			return (TRUE);
		}
		case MA_QuickLinkGroup_Mode:
		{
			D(kprintf("QLGroup Get Mode=%08lx\n",data->mode));
			*msg->opg_Storage = data->mode;
			return (TRUE);
		}
		case MA_QuickLinkGroup_Buttons:
		{
			*msg->opg_Storage = data->num_but;
			return (TRUE);
		}
		case MA_QuickLinkGroup_MinW:
			*msg->opg_Storage = data->button_w+data->hspace;
			return (TRUE);
		case MA_QuickLinkGroup_MinH:
			*msg->opg_Storage = data->button_h;
			return (TRUE);
		case MA_QuickLinkGroup_MaxW:
			*msg->opg_Storage = data->max_w;
			return (TRUE);
		case MA_QuickLinkGroup_MaxH:
			*msg->opg_Storage = data->max_h;
			return (TRUE);
 
	}
	return (DOSUPER);
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return (DOSUPER);
}

DEFSMETHOD(QuickLinkGroup_InitChange)
{
	GETDATA;

	if (msg->mode & MV_QuickLinkGroup_Change_Add)
	{
		// InitChange for add button
		DoMethod(data->parent, MUIM_Group_InitChange); 	
		if (data->num_but==0)
		{
			// Group hidded
			set(data->parent, MA_QuickLinkParentGroup_Hide, FALSE);
		}
		DoMethod(obj, MUIM_Group_InitChange);  	
		return (0);
	}
	if (msg->mode & MV_QuickLinkGroup_Change_Remove)
	{
		// InitChange for remove button
		DoMethod(data->parent, MUIM_Group_InitChange);
		DoMethod(obj, MUIM_Group_InitChange);
		return (0);
	}
	if (msg->mode & MV_QuickLinkGroup_Change_Redraw)
	{
		// InitChange for redraw buttons
		DoMethod(data->parent, MUIM_Group_InitChange); 	
		if (data->num_but) DoMethod(obj, MUIM_Group_InitChange);
	}
	return (0);
}

DEFSMETHOD(QuickLinkGroup_ExitChange)
{
	GETDATA;
	
	if (msg->mode & MV_QuickLinkGroup_Change_Add)
	{
		// ExitChange after add button
		DoMethod(obj, MUIM_Group_ExitChange);
		if (data->num_but==0)
		{
			// Group showed
			set(data->parent, MA_QuickLinkParentGroup_Hide, TRUE);
		}
		DoMethod(data->parent, MUIM_Group_ExitChange);
		return (0);
	}
	if (msg->mode & MV_QuickLinkGroup_Change_Remove)
	{
		// ExitChange after remove button
		DoMethod(obj, MUIM_Group_ExitChange);
		if (data->num_but==0)
		{
			// Group showed
			DoMethod(data->parent, MUIM_Group_InitChange);   
			set(data->parent, MA_QuickLinkParentGroup_Hide, TRUE);
			DoMethod(data->parent, MUIM_Group_ExitChange); 	
		}
		DoMethod(data->parent, MUIM_Group_ExitChange);
		return (0);
	}
	if (msg->mode & MV_QuickLinkGroup_Change_Redraw)
	{
		// ExitChange after redraw buttons
		if (data->num_but) DoMethod(obj, MUIM_Group_ExitChange);
		DoMethod(data->parent, MUIM_Group_ExitChange);
	}
	return (0);
}

DEFSMETHOD(QuickLinkGroup_Add)
{
	GETDATA;
	Object *button;

	button=(Object *)NewObject(getquicklinkbuttongroupclass(), NULL,
		MA_QuickLinkGroup_Mode, data->mode,
		MA_QuickLinkButtonGroup_Node, (ULONG)msg->td,
		End;

	if (button) DoMethod(obj, MUIM_Group_AddTail, button);
	data->num_but++;
	return (0);
}

DEFSMETHOD(QuickLinkGroup_Remove)
{
	GETDATA;
	struct treedata *node;
	struct MinList *l;
	APTR           cstate;
	APTR child;
	
	D(kprintf("QuickLinkGroup_Remove\n"));

	get(obj,MUIA_Group_ChildList,&l);
	cstate=l->mlh_Head;
	
	while ((child=(APTR)NextObject(&cstate)))
	{
		node=NULL;
		get(child, MUIA_UserData, &node);
		//kprintf("Button %08lx %08lx\n", msg->td, node);
		if (node==msg->td)
		{
			D(kprintf("Found %s\n",node->alias));
			DoMethod(obj, OM_REMMEMBER, child);
			// add break here;
			DoMethod((Object *)child, OM_DISPOSE);
		}
	}
	
	data->num_but--;
	return (0);
}

DEFSMETHOD(QuickLinkGroup_Update)
{
	//GETDATA;
	struct treedata *node;
	struct MinList *l;
	APTR cstate;
	Object *child;
	
	D(kprintf("QuickLinkGroup_Update\n"));
	
	get(obj,MUIA_Group_ChildList,&l);
	cstate=l->mlh_Head;
	
	while ((child=(Object *)NextObject(&cstate)))
	{
		node=NULL;
		get(child, MUIA_UserData, &node);
		if (node==msg->td)
		{
			D(kprintf("Found %s\n",node->alias));
			DoMethod(obj,   MUIM_Group_InitChange);
			DoMethod(child, MM_QuickLinkButtonGroup_Update);
			DoMethod(obj,   MUIM_Group_ExitChange);
			// add break here
		}
	}
	return (0);
}

BEGINMTABLE
DECNEW
DECDISP
DECGET
DECSET
//DECMMETHOD(Setup)
//DECMMETHOD(Cleanup)
DECSMETHOD(QuickLinkGroup_Add)
DECSMETHOD(QuickLinkGroup_Remove)
DECSMETHOD(QuickLinkGroup_Update)
DECSMETHOD(QuickLinkGroup_InitChange)
DECSMETHOD(QuickLinkGroup_ExitChange)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Virtgroup, quicklinkgroupclass)
