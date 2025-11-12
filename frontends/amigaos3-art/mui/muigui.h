#include <exec/types.h>

struct ObjApp
{
	APTR	App;
	APTR	WI_label_0;
	APTR	BT_left;
	APTR	BT_History;
	APTR	BT_right;
	APTR	BT_refresh;
	APTR	BT_stop;
	APTR	BT_book;
	APTR	STR_url;
	APTR	CY_search;
	APTR	STR_search;
	APTR	GR_grp_1;
	CONST_STRPTR	CY_searchContent[4];
};


extern struct ObjApp * CreateApp(void);
extern void DisposeApp(struct ObjApp *);
