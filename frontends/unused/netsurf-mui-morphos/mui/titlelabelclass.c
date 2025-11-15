#include <proto/intuition.h>
#include <proto/utility.h>

#include "mui/applicationclass.h"
#include "mui/mui.h"

/******************************************************************
 * titlelabelclass
 *****************************************************************/

struct MUIP_Title_Close { ULONG MethodID; APTR dummy1; };
#define MUIA_Floating                       0x80429753 /* V20 isg BOOL              */

enum {
	POPMENU_NEW,
	POPMENU_RELOAD,
	POPMENU_RELOAD_ALL,
	POPMENU_CLOSE,
	POPMENU_CLOSE_ALL_OTHERS
};

struct Data
{
	Object *closebutton;
	Object *label;
	Object *transferanim;
	Object *cmenu;
};

STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
	struct TagItem *tag, *tstate;

	tstate = tags;

	while ((tag = NextTagItem(&tstate)) != NULL)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_Text_Contents:
				set(data->label, MUIA_Text_Contents, tag->ti_Data);
				break;

			case MA_TransferAnim_Animate:
				set(data->transferanim, MA_TransferAnim_Animate, tag->ti_Data);
				break;
		}
	}
}

DEFNEW
{
	#define DEFAULT_PAGE_NAME "NetSurf"
	#warning fixme
	Object *closebutton;
	Object *label;
	Object *transferanim;

	obj = (Object *) DoSuperNew(cl, obj,
		//InnerSpacing(2, 0),
        MUIA_Group_Horiz, TRUE,
		MUIA_ContextMenu, TRUE,
		Child, closebutton = ImageObject,
                        MUIA_Frame, MUIV_Frame_ImageButton,
						MUIA_CustomBackfill, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
						//MUIA_Floating, TRUE,
						#if defined(__MORPHOS2__)
						MUIA_Image_Spec, MUII_Close,
						#else
						#warning fixme...
						MUIA_Image_Spec, MUII_CheckMark,
						#endif
						End,
		Child, label = TextObject,
						MUIA_Text_SetMax, FALSE,
						MUIA_Text_SetMin, FALSE,
						MUIA_Text_PreParse, "\033-",
						MUIA_Text_Contents, DEFAULT_PAGE_NAME,
						End,
		Child, transferanim = (Object *) NewObject(gettransferanimclass(), NULL, TAG_DONE),
		TAG_MORE, INITTAGS
	);

	if (obj)
	{
		GETDATA;
		data->cmenu = NULL;
		data->closebutton = closebutton;
		data->label = label;
		data->transferanim = transferanim;

		DoMethod(data->closebutton, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_Title_Close);
	}
	return ((ULONG)obj);
}

DEFSET
{
	GETDATA;
	doset(obj, data, msg->ops_AttrList);
	return DOSUPER;
}

DEFDISP
{
	GETDATA;
	if (data->cmenu)
	{
		MUI_DisposeObject(data->cmenu);
	}
	return DOSUPER;
}

DEFMMETHOD(AskMinMax)
{
	DOSUPER;

	msg->MinMaxInfo->MinWidth  += 32;
//	  msg->MinMaxInfo->MaxWidth   256;

	return 0;
}

DEFMMETHOD(Title_Close)
{
#if 0
	Object *browser = (Object *) getv(obj, MUIA_UserData);

	if(browser)
	{
		DoMethod(application, MUIM_Application_PushMethod, application, 2, MM_OWBApp_RemoveBrowser, browser);
	}
#endif

	return 0;
}

DEFMMETHOD(ContextMenuBuild)
{
	GETDATA;

	if (data->cmenu)
	{
		MUI_DisposeObject(data->cmenu);
		data->cmenu = NULL;
	}

#if 0
	data->cmenu = MenustripObject,
			MUIA_Family_Child, MenuObjectT("Browser"),
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, "New Tab...",
				MUIA_UserData, POPMENU_NEW,
                End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, NM_BARLABEL,
				MUIA_UserData, NULL,
                End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, "Reload",
				MUIA_UserData, POPMENU_RELOAD,
                End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, "Reload All Tabs",
				MUIA_UserData, POPMENU_RELOAD_ALL,
                End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, NM_BARLABEL,
				MUIA_UserData, NULL,
                End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, "Close",
				MUIA_UserData, POPMENU_CLOSE,
				End,
			MUIA_Family_Child, MenuitemObject,
				MUIA_Menuitem_Title, "Close All Other Tabs",
				MUIA_UserData, POPMENU_CLOSE_ALL_OTHERS,
				End,
            End,
        End;
#endif

	return (ULONG)data->cmenu;
}

DEFMMETHOD(ContextMenuChoice)
{
#warning fixme
#if 0
	Object *browser = (Object *) muiUserData(obj);
	ULONG udata = muiUserData(msg->item);

	switch(udata)
	{
		case POPMENU_NEW:
		{
			DoMethod(_win(obj), MM_OWBWindow_MenuAction, MNA_NEW_PAGE);
		}
		break;

		case POPMENU_RELOAD:
		{
			DoMethod(_win(obj), MM_OWBWindow_Reload, browser);
		}
		break;

		case POPMENU_RELOAD_ALL:
		{
			APTR n;

			ITERATELIST(n, &view_list)
			{
				struct viewnode *vn = (struct viewnode *) n;
				DoMethod(_win(obj), MM_OWBWindow_Reload, vn->browser);
			}
		}
		break;

		case POPMENU_CLOSE:
		{
            DoMethod(app, MUIM_Application_PushMethod, app, 2, MM_OWBApp_RemoveBrowser, browser);
		}
		break;

		case POPMENU_CLOSE_ALL_OTHERS:
		{
			APTR n;
			ITERATELIST(n, &view_list)
			{
				struct viewnode *vn = (struct viewnode *) n;

				if(vn->browser != browser)
				{
					DoMethod(application, MUIM_Application_PushMethod, application, 2, MM_OWBApp_RemoveBrowser, vn->browser);
				}
			}
		}
		break;
	}
#endif

	return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECSET
DECMMETHOD(Title_Close)
DECMMETHOD(AskMinMax)
DECMMETHOD(ContextMenuBuild)
DECMMETHOD(ContextMenuChoice)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, titlelabelclass)
