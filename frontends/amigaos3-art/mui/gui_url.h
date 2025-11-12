#include <exec/types.h>

struct ObjApp
{
	APTR	App;
	APTR	WI_Url;
	APTR	STR_Url;
	APTR	BT_go;
};

//APTR	 STR_Url;

extern struct ObjApp * CreateUrlApp(void);
extern void DisposeApp(struct ObjApp *);
