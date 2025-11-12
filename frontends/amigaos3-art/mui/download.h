#include <exec/types.h>

struct ObjApp
{
	APTR	App;
	APTR	WI_download;
	APTR	GA_progress;
};


extern struct ObjApp * CreateApp(void);
extern void DisposeApp(struct ObjApp *);

char *fetching_title;
