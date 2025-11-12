#define MUI_OBSOLETE

#include <clib/intuition_protos.h>
#include "amigaos3-art/mui/mui.h"

#include <SDI_hook.h>
#include <sys/types.h>
#include <math.h>

#include "gui_urlExtern.h"
#include "gui_url.h"

struct Library *MUIMasterBase;

char *EditUrl(char *url)
{
	InitApp();
	ULONG sigs = 0;
	struct ObjApp * obj = CreateUrlApp();
	STRPTR var;
	char *newurl;
	
	if (obj)
	{
		//set(STR_Url, MUIA_Width, strlen(url));		
		set(obj->STR_Url, MUIA_String_Contents, (ULONG*)url);
		
		while (DoMethod(obj->App, MUIM_Application_NewInput, (IPTR)&sigs)
			!= MUIV_Application_ReturnID_Quit)
		{
			if (sigs)
			{
				sigs = Wait(sigs | SIGBREAKF_CTRL_C);
				if (sigs & SIGBREAKF_CTRL_C)
					break;
			}
		}
		
		get(obj->STR_Url, MUIA_String_Contents, &var);
		newurl = strdup((STRPTR)var);
		
		DisposeApp(obj);
	}
	else
	{
		CleanExit("Can't create application\n");
	}
	CleanExit(NULL);
	
	
	return newurl;
}
