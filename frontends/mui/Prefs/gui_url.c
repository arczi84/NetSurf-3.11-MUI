#define MUI_OBSOLETE

//#undef NO_INLINE_STDARG
//#include <inline/intuition.h>
#include <clib/intuition_protos.h>
//#define NO_INLINE_STDARG
#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <proto/exec.h>

#include <stddef.h> //gcc6

#include <libraries/mui.h>

#include "utils/messages.h"
#include "amiga/utf8.h"

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#include "gui_urlExtern.h"
#include "gui_url.h"


struct ObjApp * CreateUrlApp(void)
{
	struct ObjApp * object;

	APTR	GROUP_ROOT_0, obj_aux0, obj_aux1;

	if (!(object = AllocVec(sizeof(struct ObjApp), MEMF_PUBLIC|MEMF_CLEAR)))
		return NULL;

	object->STR_Url = StringObject,
		MUIA_Frame, MUIV_Frame_String,
		MUIA_HelpNode, "STR_Url",
		MUIA_String_Contents, "http://www.netsurf-browser.org/welcome/",
		MUIA_String_MaxLen, 1000,
		MUIA_FixWidth, 400,
	End;

	obj_aux1 = Label2("Url:");

	obj_aux0 = GroupObject,
		MUIA_Group_Columns, 2,
		Child, obj_aux1,
		Child, object->STR_Url,
	End;

	object->BT_go = TextObject,
		ButtonFrame,
		MUIA_Weight, 5,
		MUIA_Background, MUII_ButtonBack,
		MUIA_Text_Contents, ami_utf8_easy(messages_get("Navigate")),
		MUIA_Text_PreParse, "\033c",
		MUIA_HelpNode, "BT_go",
		MUIA_InputMode, MUIV_InputMode_RelVerify,
	End;

	GROUP_ROOT_0 = GroupObject,
		MUIA_Group_Rows, 1,
		Child, obj_aux0,
		Child, object->BT_go,
	End;

	object->WI_Url = WindowObject,
		MUIA_Window_Title, messages_get("EditLink"),
		MUIA_Window_ID, MAKE_ID('0', 'W', 'I', 'N'),
		MUIA_Window_NoMenus, TRUE,
		WindowContents, GROUP_ROOT_0,
	End;

	object->App = ApplicationObject,
		MUIA_Application_Author, "Artur Jarosik",
		MUIA_Application_Base, "MUI GUI",
		MUIA_Application_Title, "NONE",
	//	MUIA_Application_Version, "$VER: NONE XX.XX (XX.XX.XX)",
		MUIA_Application_Copyright, "Artur Jarosik",
		MUIA_Application_Description, "Url GUI",
		SubWindow, object->WI_Url,
	End;


	if (!object->App)
	{
		FreeVec(object);
		return NULL;
	}
	
	DoMethod(object->STR_Url,
		MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
		object->App,
		3,
		MUIM_Set, MUIA_String_Contents, MUIV_TriggerValue
		);
	
	DoMethod(object->STR_Url,
		MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
		object->App,
		2,
		MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit
		);
		
	DoMethod(object->WI_Url,
		MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		object->App,
		2,
		MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit
		);

	DoMethod(object->BT_go,
		MUIM_Notify, MUIA_Pressed, TRUE,
		object->App,
		2,
		MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit
		);

	DoMethod(object->WI_Url,
		MUIM_Window_SetCycleChain, object->STR_Url,
		object->BT_go,
		0
		);

	set(object->WI_Url,
		MUIA_Window_Open, TRUE
		);


	return object;
}
