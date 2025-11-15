#ifndef MUI_MUI_H
#define MUI_MUI_H
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
 */

#ifndef SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef LIBRARIES_MUI_H
#include <libraries/mui.h>
#endif

#ifndef PROTO_ALIB_H
#include <proto/alib.h>
#endif

#ifndef PROTO_MUIMASTER_H
#include <proto/muimaster.h>
#endif

#ifndef VAPOR_MACROS_H
#include "macros/vapor.h"
#endif

#ifndef MUI_EXTRASRC_H
#include "extrasrc.h"
#endif

#define DEFCLASS(s) ULONG create_##s##class(void); \
	struct IClass *get##s##class(void); \
	APTR get##s##classroot(void); \
	void delete_##s##class(void)

ULONG classes_init(void);
void classes_cleanup(void);

extern APTR application;

/************************************************************/

APTR create_toolbutton(CONST_STRPTR text, CONST_APTR image);

/************************************************************/

/* Classes */

DEFCLASS(application);
DEFCLASS(window);
DEFCLASS(navigationbargroup);
DEFCLASS(transferanim);
DEFCLASS(searchbargroup);
DEFCLASS(toolbutton);
DEFCLASS(title);
DEFCLASS(titlelabel);
DEFCLASS(browser);
DEFCLASS(popstring);
DEFCLASS(historylist);
DEFCLASS(historypopstring);
DEFCLASS(addressbargroup);
DEFCLASS(findtext);
DEFCLASS(pdf);
DEFCLASS(hotlistwindow);
DEFCLASS(downloadwindow);
DEFCLASS(loginwindow);
DEFCLASS(quicklinkgroup);
DEFCLASS(quicklinkbuttongroup);
DEFCLASS(quicklinkparentgroup);

/************************************************************/

enum
{
	MA_AddressBarGroup_Active = 0xfece0200,
	MA_AddressBarGroup_PopString,

	MM_Application_About,
	MM_Application_AddBrowser,
	MM_Application_AppMessage,
	MM_Application_AskPasswordPDF,
	MM_Application_CloseWindow,
	MM_Application_Download,
	MM_Application_DownloadDone,
	MM_Application_DownloadError,
	MM_Application_DownloadUpdate,
	MM_Application_KillWindow,
	MM_Application_Login,
	MM_Application_NewWindow,
	MM_Application_OpenWindow,
	MM_Application_PrefsSave,
	MM_Application_SaveDocument,
	MM_Application_SetPasswordPDF,
	MA_Application_DownloadsInProgress,
	MA_Application_Homepage,

	MM_Browser_AutoComplete,
	MM_Browser_Back,
	MM_Browser_Forward,
	MM_Browser_GetBitMap,
	MM_Browser_Go,
	MM_Browser_Find,
	MM_Browser_Login,					/* private */
	MM_Browser_Print,
	MM_Browser_Redraw,				/* private */
	MM_Browser_Reload,
	MM_Browser_SetContentSize,		/* private */
	MM_Browser_SetContentType,		/* private */
	MM_Browser_Stop,
	MA_Browser_BackAvailable,		/* ..G [..G] */
	MA_Browser_Box,					/* ..G private struct IBox * */
	MA_Browser_Browser,           /* ..G private */
	MA_Browser_Context,           /* I.G private */
	MA_Browser_ForwardAvailable,	/* ..G [..G] */
	MA_Browser_Loading,				/* .SG [..G] */
	MA_Browser_Pointer,				/* .S. private */
	MA_Browser_ReloadAvailable,	/* .SG [..G] */
	MA_Browser_StatusText,			/* .SG private */
	MA_Browser_StopAvailable,		/* .SG [..G] */
	MA_Browser_Title,					/* .SG [..G] */
	MA_Browser_TitleObj,          /* I.G private */
	MA_Browser_URL,					/* .SG [..G] */

	/* Download window */
	MM_Download_Cancel,
	MM_Download_Cancelled,
	MM_Download_Done,
	MM_Download_Error,
	MM_Download_RemoveEntry,

	MM_FindText_DisableButtons,

	/* HistoryList */
	MA_HistoryList_Opened,
	MM_HistoryList_SelectChange,

	/* HistoryPopString */
	MM_HistoryPopString_Insert,
	MM_HistoryPopString_Remove,

	MM_HotlistWindow_Activate,
	MM_HotlistWindow_Insert,
	MM_HotlistWindow_Modify,

	MM_LoginWindow_Login,
	MA_LoginWindow_Browser,			/* I.. */
	MA_LoginWindow_Host,				/* I.. */
	MA_LoginWindow_Realm,			/* I.. */
	MA_LoginWindow_URL,				/* I.. */

	MA_Navigation_BackEnabled,
	MA_Navigation_ForwardEnabled,
	MA_Navigation_ReloadEnabled,
	MA_Navigation_StopEnabled,

	MM_PDF_SetPassword,
	MA_PDF_Reference,

	MM_PopString_Insert,
	MA_PopString_ActivateString,

	/* QuickLinkGroup */
	MA_QuickLinkGroup_Data,
	MA_QuickLinkGroup_Row,
	MA_QuickLinkGroup_Mode,
	MA_QuickLinkGroup_Buttons,
	MA_QuickLinkGroup_MinW,
	MA_QuickLinkGroup_MinH,
	MA_QuickLinkGroup_MaxW,
	MA_QuickLinkGroup_MaxH,
	MA_QuickLinkGroup_Parent,
	MM_QuickLinkGroup_Add,
	MM_QuickLinkGroup_Remove,
	MM_QuickLinkGroup_Update,
	MM_QuickLinkGroup_InitChange,
	MM_QuickLinkGroup_ExitChange,

	/* QuickLinkButtonGroup */
	MA_QuickLinkButtonGroup_Node,
	MM_QuickLinkButtonGroup_Update,

	/* QuickLinkParentGroup */
	MA_QuickLinkParentGroup_Hide,
	MA_QuickLinkParentGroup_QLGroup,

	MA_ToolButton_Image,
	MA_ToolButton_Text,

	MM_TransferAnim_Run,
	MA_TransferAnim_Animate,	/* ISG */

	MM_Window_ActivePage,
	MM_Window_AddPage,			/* Adds new browser with browser instance */
	MM_Window_InsertBookmark,
	MM_Window_MenuAction,
	MM_Window_Navigate,
	MM_Window_RemovePage,
	MM_Window_SetTitle,
	MA_Window_Node,
};

enum
{
	NAV_BACK,
	NAV_FORWARD,
	NAV_RELOAD,
	NAV_STOP,
};

enum
{
	TBMODE_TEXT,
	TBMODE_ICON,
	TBMODE_ICON_WITH_TEXT,
};

enum
{
	BM_ADD_LINK,
	BM_ADD_GROUP,
	BM_REMOVE,
};

enum
{
	SAVEDOC_HTML,
	SAVEDOC_SOURCE,
	SAVEDOC_PDF,
};

struct MP_Application_AddBrowser     { ULONG MethodID; APTR context; };
struct MP_Application_AskPasswordPDF { ULONG MethodID; STRPTR *owner; STRPTR *user; APTR ref; };
struct MP_Application_CloseWindow    { ULONG MethodID; APTR windownode; };
struct MP_Application_Download       { ULONG MethodID; STRPTR url; STRPTR filename; };
struct MP_Application_DownloadDone   { ULONG MethodID; APTR entry; };
struct MP_Application_DownloadError  { ULONG MethodID; APTR entry; };
struct MP_Application_KillWindow     { ULONG MethodID; APTR window; };
struct MP_Application_Login          { ULONG MethodID; APTR browser; STRPTR url; STRPTR host; STRPTR realm; };
struct MP_Application_NewWindow      { ULONG MethodID; APTR browser; };
struct MP_Application_OpenWindow     { ULONG MethodID; ULONG window_id; };
struct MP_Application_PrefsSave      { ULONG MethodID; ULONG SaveENVARC; };
struct MP_Application_SaveDocument   { ULONG MethodID; ULONG type; };
struct MP_Application_SetPasswordPDF { ULONG MethodID; STRPTR owner; STRPTR user; APTR ref; APTR window; };
struct MP_Browser_AutoComplete       { ULONG MethodID; LONG len; LONG prevlen; };
struct MP_Browser_Find               { ULONG MethodID; STRPTR string; ULONG flags; };
struct MP_Browser_GetBitMap          { ULONG MethodID; ULONG width; ULONG height; };
struct MP_Browser_Go                 { ULONG MethodID; STRPTR url; };
struct MP_Browser_SetContentSize     { ULONG MethodID; ULONG width; ULONG height; };
struct MP_Browser_SetContentType     { ULONG MethodID; ULONG type; };

struct MP_Download_Cancel            { ULONG MethodID; LONG all; };
struct MP_Download_Cancelled         { ULONG MethodID; APTR entry; };
struct MP_Download_Done              { ULONG MethodID; APTR entry; };
struct MP_Download_Error             { ULONG MethodID; APTR entry; };
struct MP_Download_RemoveEntry       { ULONG MethodID; APTR listview; LONG all; };

struct MP_FindText_DisableButtons    { ULONG MethodID; LONG prev; LONG next; };
struct MP_HistoryPopString_Insert    { ULONG MethodID; APTR item; };
struct MP_HistoryPopString_Remove    { ULONG MethodID; APTR item; };
struct MP_HotlistWindow_Activate     { ULONG MethodID; APTR node; };
struct MP_HotlistWindow_Insert       { ULONG MethodID; STRPTR title ; STRPTR url; };
struct MP_HotlistWindow_Modify       { ULONG MethodID; ULONG operation; };
struct MP_PDF_SetPassword            { ULONG MethodID; LONG set_pass; };
struct MP_PopString_Insert           { ULONG MethodID; STRPTR txt; };

/* QuickLinkGroup */
struct MP_QuickLinkGroup_Add{
	LONG MethodID;
	struct treedata *td;
};

struct MP_QuickLinkGroup_Remove {
	LONG MethodID;
    struct treedata *td;
};

struct MP_QuickLinkGroup_Update {
	LONG MethodID;
    struct treedata *td;
};

struct MP_QuickLinkGroup_InitChange {
	LONG MethodID;
	ULONG mode;
};

struct MP_QuickLinkGroup_ExitChange
{
	LONG MethodID;
	ULONG mode;
};

struct MP_Window_ActivePage          { ULONG MethodID; LONG pagenum; };
struct MP_Window_AddPage             { ULONG MethodID; APTR context; };
struct MP_Window_MenuAction          { ULONG MethodID; ULONG action; };
struct MP_Window_Navigate            { ULONG MethodID; ULONG Navigate; };
struct MP_Window_RemovePage          { ULONG MethodID; APTR browser; };
struct MP_Window_SetTitle            { ULONG MethodID; STRPTR title; };

#define MF_Browser_Find_CaseSensitive (1 << 0)
#define MF_Browser_Find_Previous      (1 << 1)

#define MV_QuickLinkGroup_Mode_Button 1
#define MV_QuickLinkGroup_Mode_Prop   2
#define MV_QuickLinkGroup_Mode_Horiz  0
#define MV_QuickLinkGroup_Mode_Vert   4
#define MV_QuickLinkGroup_Mode_Col    8

#define MV_QuickLinkGroup_Change_Add 1
#define MV_QuickLinkGroup_Change_Remove 2
#define MV_QuickLinkGroup_Change_Redraw 4

#define FORCHILD(_o, _a) \
	{ \
		APTR child, _cstate = (APTR)((struct MinList *)getv(_o, _a))->mlh_Head; \
		while ((child = NextObject(&_cstate)))

#define NEXTCHILD }

#ifndef MUIM_Title_Close
#define MUIM_Title_Close                    0x8042303a /* private */ /* V20 */
#endif

#ifndef MUIA_Group_ChildCount
#define MUIA_Group_ChildCount               0x80420322 /* V20 isg LONG              */ /* private */
#endif

#ifndef MUIM_Text
#define MUIM_Text                           0x8042ee70 /* private */ /* V20 */
#define MUIM_TextDim                        0x80422ad7 /* private */ /* V20 */

struct  MUIP_Text                           { ULONG MethodID; LONG left;LONG top;LONG width;LONG height;STRPTR text;LONG len;STRPTR preparse;ULONG flags; }; /* private */
struct  MUIP_TextDim                        { ULONG MethodID; STRPTR text;LONG len;STRPTR preparse;ULONG flags; }; /* private */
#endif

#ifndef MUIF_PUSHMETHOD_SINGLE
#define MUIF_PUSHMETHOD_SINGLE       (1<<28UL)
#endif

#ifndef MUIA_CustomBackfill
#define MUIA_CustomBackfill 0x80420a63
#endif

#ifndef MUIM_Backfill
#define MUIM_Backfill 0x80428d73
struct  MUIP_Backfill { ULONG MethodID; LONG left; LONG top; LONG right; LONG bottom; LONG xoffset; LONG yoffset; LONG brightness; };
#endif

APTR MakeCheck(CONST_STRPTR str, ULONG checked);
APTR MakeRect(void);
APTR MakeButton(CONST_STRPTR msg);
APTR MakeVBar(void);
APTR MakeHBar(void);
APTR MakeLabel(CONST_STRPTR msg);
APTR MakeNewString(CONST_STRPTR str, ULONG maxlen);
APTR MakeString(CONST_STRPTR def, ULONG secret);
APTR MakePrefsString(CONST_STRPTR str, CONST_STRPTR def, ULONG maxlen, ULONG id);
APTR MakeDirString(CONST_STRPTR str, CONST_STRPTR def, ULONG id);
APTR MakeCycle(CONST_STRPTR label, const CONST_STRPTR *entries, ULONG id);
ULONG getv(APTR obj, ULONG attr);

#define _isinobject(_x,_y) (_between(_left(obj),(_x),_right(obj)) && _between(_top(obj),(_y),_bottom(obj)))
#define _isinobject2(_o,_x,_y) (_between(_left(_o),(_x),_right(_o)) && _between(_top(_o),(_y),_bottom(_o)))
#define _between(_a,_x,_b) ((_x)>=(_a) && (_x)<=(_b))
#define _isinwinborder(_x,_y) \
	((_between(0, (_x), _window(obj)->Width) && _between(0, (_y), _window(obj)->BorderTop)) || \
	 (_between(_window(obj)->Width - _window(obj)->BorderRight, (_x), _window(obj)->Width) && _between(0, (_y), _window(obj)->Height)) || \
	 (_between(0, (_x), _window(obj)->Width) && _between(_window(obj)->Height - _window(obj)->BorderBottom, (_y), _window(obj)->Height)) || \
	 (_between(0, (_x), _window(obj)->BorderLeft) && _between(0, (_y), _window(obj)->Height)))

/************************************************************/

#define INITTAGS (((struct opSet *)msg)->ops_AttrList)

#define FORTAG(_tagp) \
	{ \
		struct TagItem *tag, *_tags = (struct TagItem *)(_tagp); \
		while ((tag = NextTagItem(&_tags))) switch ((int)tag->ti_Tag)
#define NEXTTAG }

#define FORCHILD(_o, _a) \
	{ \
		APTR child, _cstate = (APTR)((struct MinList *)getv(_o, _a))->mlh_Head; \
		while ((child = NextObject(&_cstate)))

#define NEXTCHILD }

/************************************************************/

#define CY_SEARCH_ID MAKE_ID('C','S','R','H')

#endif /* MUI_MUI_H */
