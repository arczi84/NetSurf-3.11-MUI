#define MUI_OBSOLETE
#include <stddef.h>

#include <clib/intuition_protos.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <proto/exec.h>
#include "utils/messages.h"

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#include "downloadExtern.h"
#include "download.h"


struct ObjApp * CreateApp(void)
{
	struct ObjApp * object;

	APTR	GROUP_ROOT_0;


	if (!(object = AllocVec(sizeof(struct ObjApp), MEMF_PUBLIC|MEMF_CLEAR)))
		return NULL;

	object->GA_progress = GaugeObject,
		GaugeFrame,
		MUIA_HelpNode, "GA_progress",
		MUIA_FixWidth, 250,
		MUIA_FixHeight, 120,
		MUIA_Gauge_Horiz, TRUE,
		MUIA_Gauge_InfoText, "%ld %%",
		MUIA_Gauge_Max, 100,
	End;

	GROUP_ROOT_0 = GroupObject,
		Child, object->GA_progress,
	End;

	object->WI_download = WindowObject,
		MUIA_Window_Title, fetching_title,
		MUIA_Window_ID, MAKE_ID('0', 'W', 'I', 'N'),
		MUIA_Window_DepthGadget, FALSE,
		MUIA_Window_SizeGadget, FALSE,
		MUIA_Window_NoMenus, TRUE,
		
		WindowContents, GROUP_ROOT_0,
	End;

	object->App = ApplicationObject,
		MUIA_Application_Author, "NONE",
		MUIA_Application_Base, "Download",
		MUIA_Application_Title, "Download",
		MUIA_Application_Copyright, "NOBODY",
		MUIA_Application_Description, "Download",
		SubWindow, object->WI_download,
	End;


	if (!object->App)
	{
		FreeVec(object);
		return NULL;
	}

	DoMethod(object->WI_download,
		MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		object->App,
		2,
		MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit
		);

	DoMethod(object->WI_download,
		MUIM_Window_SetCycleChain, 0
		);

	set(object->WI_download,
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
