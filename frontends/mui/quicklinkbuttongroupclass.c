#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/bookmarkgroupclass.h"
#include "mui/mui.h"

#define D(x) x

#define LOC(a,b) b

struct Data
{
	struct treedata *node;
	STRPTR title;
	APTR   button;
	APTR   icon;
	ULONG  mode;
	APTR   cMenu; 
	char   cMenu_Title[32];
};

static void build_content(struct Data *data)
{
	//kprintf("build_content %02lx Title '%s'\n", data->mode & MV_QuickLinkGroup_Mode_Button, data->title);
	if (data->mode & MV_QuickLinkGroup_Mode_Button)
	{
		data->button=TextObject,
			MUIA_Font, MUIV_Font_Tiny,
			MUIA_Text_Contents, data->title,
			MUIA_Background,    MUII_ButtonBack,
			MUIA_Frame,         MUIV_Frame_Button,
			MUIA_Text_PreParse, "\033c",
			End;
		data->icon=NULL;
		//kprintf("Build QL Button %08lx\n", data->button);
	}
	else
	{
		data->icon=ImageObject,
			NoFrame,
			MUIA_Image_Spec,     MUII_Network,
			MUIA_Image_FreeVert, TRUE,
			MUIA_ShowSelState,   TRUE,
			End;
		//kprintf("Build QL Icon %08lx\n",data->icon);
		data->button=TextObject,
			MUIA_Font, MUIV_Font_Normal,
			MUIA_Text_Contents, data->title,
			End;
		//kprintf("Build QL Link %08lx\n", data->button);
	}
}

static void doset(Object *obj, struct Data *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MA_QuickLinkGroup_Mode:
			if ((data->mode & MV_QuickLinkGroup_Mode_Button)!=(tag->ti_Data & MV_QuickLinkGroup_Mode_Button))
			{
				// Type change
				Object *OldIcon=(Object *)data->icon;
				Object *OldButton=(Object *)data->button;
				data->mode=tag->ti_Data;   
				build_content(data);
				if (data->button)
				{
					// Remove old objects from group
					DoMethod(obj, MUIM_Group_InitChange);
					DoMethod(obj, MUIM_Group_Remove, OldButton);
					DoMethod(OldButton, OM_DISPOSE);
					if (OldIcon)
					{
						DoMethod(obj, MUIM_Group_Remove, OldIcon);
						DoMethod(OldIcon, OM_DISPOSE);
					}
					
					if (data->icon) DoMethod(obj, MUIM_Group_AddTail, data->icon);
					DoMethod(obj, MUIM_Group_AddTail, data->button);
					DoMethod(obj, MUIM_Group_ExitChange);
				}
				else
				{
					// Something fail so restore old one's
					data->icon=OldIcon;
					data->button=OldButton;
				}
				
			}
			data->mode=tag->ti_Data;   
			break;
	}
	NEXTTAG
}

DEFNEW
{
	struct Data tmp={NULL,NULL,NULL,NULL,0,NULL,""}, *data;
	struct TagItem *tags,*tag;
	STRPTR title;

	for (tags=((struct opSet *)msg)->ops_AttrList;(tag=NextTagItem(&tags));)
    {
		//kprintf("QLBG: Tag=%08lx Data=%08lx\n",tag->ti_Tag, tag->ti_Data);
        switch (tag->ti_Tag)
        {
			case MA_QuickLinkGroup_Mode:
				tmp.mode=tag->ti_Data;
				break;
			case MA_QuickLinkButtonGroup_Node:
				tmp.node=(struct treedata *)tag->ti_Data;
				break;
        }
	}

	if (!tmp.node) return (NULL);

	/*kprintf("QuickLinkButtonGroup New: N:%08lx F:%03lx T:%08lx A:%08lx Ad:%08lx B:%08lx O:%08lx\n",
		tmp.node,
		tmp.node->flags,
		tmp.node->title,
		tmp.node->alias,
		tmp.node->address,
		tmp.node->buffer1,
		tmp.node->ql_order);*/

	// Copy string
	if ((tmp.node->alias) && (*tmp.node->alias!='\0')) title=tmp.node->alias;
	else title="(no name)";
	tmp.title=(STRPTR)malloc(strlen(title)+1);
	if (!tmp.title) return (NULL);
	strcpy(tmp.title,title);

	build_content(&tmp);

	obj = (Object *) DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		MUIA_CycleChain, 1,
		MUIA_InputMode, MUIV_InputMode_RelVerify,  
		MUIA_UserData, tmp.node,
		MUIA_ShortHelp, TRUE,
        MUIA_ContextMenu, TRUE,
		(tmp.icon) ? Child : TAG_IGNORE, tmp.icon,
		Child, tmp.button,
		End;

	if(obj)
	{
		data = (struct Data *) INST_DATA(cl, obj);
		data->node=tmp.node;
		data->title=tmp.title;
		data->button=tmp.button;
		data->icon=tmp.icon;
		data->cMenu=tmp.cMenu;
		data->mode=tmp.mode;
		data->cMenu_Title[0]='\0';
		//kprintf("QuickLinkButtonGroup End\n");

		DoMethod(obj, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 2, MM_Browser_Go, data->node->address);
	}
	return ((ULONG)obj);
}


DEFDISP
{
	GETDATA;
	free(data->title);
	// Free menu
	if (data->cMenu) MUI_DisposeObject((Object *)data->cMenu);
	return (DOSUPER);
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

DEFTMETHOD(QuickLinkButtonGroup_Update)
{
	GETDATA;
	STRPTR title;
	if ((data->node->alias) && (*data->node->alias!='\0')) title=data->node->alias;
	else title="(no name)";
	free(data->title);
	data->title=(STRPTR)malloc(strlen(title)+1);
	if (data->title)
	{
		set(data->button, MUIA_Text_Contents, title);
	}
	return 0;
}

DEFMMETHOD(ContextMenuBuild)
{
	GETDATA;

#if 0
	//kprintf("QLButtonGroup Context menu build\n");

	if (data->cMenu)
	{
		MUI_DisposeObject((Object *)data->cMenu);
		data->cMenu = NULL;
	}

	snprintf(data->cMenu_Title, sizeof(data->cMenu_Title)-1, "%s", data->node->title);

	if ( strlen(data->node->title)>(sizeof(data->cMenu_Title)-1) )
	{
		ULONG a=sizeof(data->cMenu_Title)-2;
		data->cMenu_Title[a--]='.';
		data->cMenu_Title[a--]='.';
		data->cMenu_Title[a--]='.';
	}

	data->cMenu=MenustripObject,
		MUIA_Family_Child, MenuObjectT( data->cMenu_Title ),
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, LOC(-1,"\033bOpen"),
				MUIA_UserData, POPMENU_OPEN_CURRENT,
				End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, LOC(-1,"Open Quicklink in New Tab..."),
				MUIA_UserData, POPMENU_OPEN_NEWTAB,
				End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, LOC(-1,"Open Quicklink in New Window..."),
				MUIA_UserData, POPMENU_OPEN_NEWWIN,
				End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title,  NM_BARLABEL,
				End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, LOC(-1,"Remove Quicklink"),
				MUIA_UserData, POPMENU_REMOVE,
				End,
			End,
		End;
#endif
	
	return (ULONG)data->cMenu;
}

DEFMMETHOD(ContextMenuChoice)
{
	GETDATA;
#if 0
	ULONG udata = muiUserData(msg->item);

	//kprintf("QLButtonGroup menu trig: %lx\n",udata);
	switch (udata)
    {
		case POPMENU_OPEN_CURRENT:
			//kprintf("Open link in current view\n");
			//kprintf("URL: %s\n", (data->node->address) ? data->node->address : (STRPTR)"(NULL)" );
			Object *window = (Object *) getv(app, MA_OWBApp_ActiveWindow);
			if(window && data->node->address)
			{
				DoMethod(window, MM_OWBWindow_LoadURL, data->node->address);
			}

			break;
		case POPMENU_OPEN_NEWTAB:
			//kprintf("Open link in new tab\n");
			//kprintf("URL: %s\n", (data->node->address) ? data->node->address : (STRPTR)"(NULL)" );
			if(data->node->address)
			{
				DoMethod(app, MM_OWBApp_AddBrowser, NULL, data->node->address, FALSE, NULL, FALSE);
			}
			break;
		case POPMENU_OPEN_NEWWIN:
			//kprintf("Open link in new window\n");
			//kprintf("URL: %s\n", (data->node->address) ? data->node->address : (STRPTR)"(NULL)" );
			if(data->node->address)
			{
				DoMethod(app, MM_OWBApp_AddWindow, data->node->address, FALSE, NULL);
			}
			break;
		case POPMENU_REMOVE:
			//kprintf("Remove button\n");
			DoMethod(app, MM_Bookmarkgroup_RemoveQuickLink, data->node);
			break;
		default:
			;//kprintf("Bad Context menu return\n");
    }
#endif
    return (ULONG)NULL;
}

DEFMMETHOD(CreateShortHelp)
{
	GETDATA;  
	STRPTR help;
	ULONG title_len=(data->node->address) ? strlen(data->node->title) : 0;
	ULONG address_len=(data->node->address) ? strlen(data->node->address) : 0;

	if ((address_len) && (title_len))
	{
		help=(STRPTR)malloc(address_len+title_len+1);
		if (help)
		{
			sprintf(help, "%s\n%s", data->node->title, data->node->address);
			return ((ULONG)help);
		}
	}
	return (0);
}

DEFMMETHOD(DeleteShortHelp)
{
	free(msg->help);
	return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECSET
DECTMETHOD(QuickLinkButtonGroup_Update)
DECMMETHOD(ContextMenuBuild)
DECMMETHOD(ContextMenuChoice)
DECMMETHOD(CreateShortHelp)
DECMMETHOD(DeleteShortHelp)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, quicklinkbuttongroupclass)
