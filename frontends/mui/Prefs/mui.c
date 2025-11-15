#define MUI_OBSOLETE

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <proto/exec.h>
#include <proto/intuition.h>


#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#include "mui.h"


struct ObjApp * CreateApp(void)
{
	struct ObjApp * object;

	APTR	GROUP_ROOT_0, HorizontalBar;

	if (!(object = AllocVec(sizeof(struct ObjApp), MEMF_PUBLIC|MEMF_CLEAR)))
		return NULL;

	object->CY_searchContent[0] = "Google";
	object->CY_searchContent[1] = "YouTube";
	object->CY_searchContent[2] = "Aminet";
	object->CY_searchContent[3] = NULL;

	object->BT_left = SimpleButton("Left");

	object->BT_History = SimpleButton("History");

	object->BT_right = SimpleButton("Right");

	object->BT_refresh = SimpleButton("Refresh");

	object->BT_stop = SimpleButton("Stop");

	object->BT_book = SimpleButton("Bookmark");

	object->STR_url = StringObject,
		MUIA_Frame, MUIV_Frame_String,
		MUIA_HelpNode, "STR_url",
		MUIA_String_MaxLen, 200,
	End;

	object->CY_search = CycleObject,
		MUIA_HelpNode, "CY_search",
		MUIA_Frame, MUIV_Frame_Button,
		MUIA_Cycle_Entries, object->CY_searchContent,
	End;

	object->STR_search = StringObject,
		MUIA_Frame, MUIV_Frame_String,
		MUIA_HelpNode, "STR_search",
	End;

	HorizontalBar = GroupObject,
		MUIA_HelpNode, "HorizontalBar",
		MUIA_Group_Horiz, TRUE,
		Child, object->BT_left,
		Child, object->BT_History,
		Child, object->BT_right,
		Child, object->BT_refresh,
		Child, object->BT_stop,
		Child, object->BT_book,
		Child, object->STR_url,
		Child, object->CY_search,
		Child, object->STR_search,
	End;

	object->GR_grp_1 = VirtgroupObject,
		VirtualFrame,
	End;

	object->GR_grp_1 = ScrollgroupObject,
		MUIA_Scrollgroup_Contents, object->GR_grp_1,
	End;

	GROUP_ROOT_0 = GroupObject,
		MUIA_Group_Rows, 2,
		Child, HorizontalBar,
		Child, object->GR_grp_1,
	End;

	object->WI_label_0 = WindowObject,
		MUIA_Window_Title, "window_title",
		MUIA_Window_ID, MAKE_ID('0', 'W', 'I', 'N'),
		WindowContents, GROUP_ROOT_0,
	End;

	object->App = ApplicationObject,
		MUIA_Application_Author, "NONE",
		MUIA_Application_Base, "NONE",
		MUIA_Application_Title, "NONE",
		MUIA_Application_Version, "$VER: NONE XX.XX (XX.XX.XX)",
		MUIA_Application_Copyright, "NOBODY",
		MUIA_Application_Description, "NONE",
		SubWindow, object->WI_label_0,
	End;


	if (!object->App)
	{
		FreeVec(object);
		return NULL;
	}

	DoMethod(object->WI_label_0,
		MUIM_Window_SetCycleChain, object->BT_left,
		object->BT_History,
		object->BT_right,
		object->BT_refresh,
		object->BT_stop,
		object->BT_book,
		object->STR_url,
		object->CY_search,
		object->STR_search,
		0
		);

	set(object->WI_label_0,
		MUIA_Window_Open, TRUE
		);


	return object;
}

void DisposeApp(struct ObjApp * object)
{
	if (object)
	{
		MUI_DisposeObject(object->App);
		FreeVec(object);
		object = NULL;
	}
}
